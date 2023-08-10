// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "dds/version.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__discovery_spdp.h"
#include "ddsi__discovery_addrset.h"
#include "ddsi__discovery_endpoint.h"
#include "ddsi__spdp_schedule.h"
#include "ddsi__addrset.h"
#include "ddsi__serdata_plist.h"
#include "ddsi__entity_index.h"
#include "ddsi__entity.h"
#include "ddsi__security_omg.h"
#include "ddsi__endpoint.h"
#include "ddsi__plist.h"
#include "ddsi__proxy_participant.h"
#include "ddsi__topic.h"
#include "ddsi__vendor.h"
#include "ddsi__xevent.h"
#include "ddsi__transmit.h"
#include "ddsi__lease.h"
#include "ddsi__xqos.h"

/*
X

 "aging" locators can be skipped every now and then
 - L has T_aging_start
 - f(T_now, T_next, T_aging_start) -> Bool that says whether locator with given T_aging_start
   should be sent an SPDP now for the participant that plans to send its next SPDP at T_next
 actually: "aging" happens when there are no proxy participants known at the address
 so it is purely for discovery purposes and we don't need to worry about a per-participant SPDP interval

 P_i needs to send SPDP no later than T_i
 Most of the time, all P have same interval
 Most of the time, there aren't all that many P's
 Arguably, it is ok to loop over them without worrying
 Basically, as long as there is a P, there are SPDPs to be sent
   not strictly true, but the case where this isn't true is rare indeed, so let's accept some unnecessary work
   there is almost always a P

 Want to send them ordered by locator

 Could do table (T, P, L) ordered in some sane way but that means a lot of duplication ...

 Indexing on locator address, makes the most sense

 spdp_locator_ref (const struct ddsi_xlocator *loc)
   L = find loc somewhere
   if !L.is_fixed && L.proxypp_refc++ == 0
     delete L from L_aging
     insert L in L_live

 spdp_locator_unref (const struct ddsi_xlocator *loc)
   L = find loc L_live
   if !L.is_fixed && --L.proxypp_refc == 0
     delete L from L_live
     L.interval[0] = 1
     L.interval[1] = 0
     insert L in L_aging

 foreach L_aging:
   assert L.proxypp_refc == 0
   if T_now > L.T_sched
     foreach P:
       resend_spdp(P, L)
     n = L.interval[0] + L.interval[1]
     if (n > max_n)
       delete L
     else
       L.interval[1] = L.interval[0]
       L.interval[0] = n
       L.T_sched = T_now + function-that-maps-F_n-to-SPDP-interval
 resched at min(L.T_sched for L in L_aging)

 foreach L_live:
   assert L.is_fixed || L.proxypp_refc > 0
   foreach P:
     if (P.T_sched <= T_now + a bit)
       resend_spdp(P, L)
 foreach P:
   if (P.T_sched <= T_now + a bit)
     P.T_sched = T_now + intv(P)
 resched at min(P.T_sched)

X
*/

struct spdp_loc_common {
  ddsrt_avl_node_t avlnode; // indexed on address
  ddsi_xlocator_t xloc; // the address
};

struct spdp_loc_aging {
  struct spdp_loc_common c;
  ddsrt_mtime_t tsched; // time at which to ping this locator again
  uint32_t age; // decremented, entry is deleted when it reaches 0
};

struct spdp_loc_live {
  struct spdp_loc_common c;
  uint32_t proxypp_refc; // number of proxy participants known at this address
  bool lease_expiry_occurred; // if refc goes to 0 and not lease expiry, then no need to age
};

union spdp_loc_union {
  struct spdp_loc_common c;
  struct spdp_loc_live live;
  struct spdp_loc_aging aging;
};

struct spdp_pp {
  ddsrt_avl_node_t avlnode; // indexed on pp's GUID but needs a struct spdp_pp in lookup
  const struct ddsi_participant *pp;
  ddsrt_mtime_t tsched;
};

struct spdp_admin {
  struct ddsi_domaingv *gv;
  struct ddsi_xevent *live_xev;
  struct ddsi_xevent *aging_xev;
  ddsrt_mutex_t lock;
  // Locators to send SPDP messages to: those where there are live proxy participants
  // and those that we're aging because there aren't any
  ddsrt_avl_tree_t live;
  ddsrt_avl_tree_t aging;
  // use global table of participants and:
  // - embed schedule info in them
  // - worry about race conditions
  // or: have a separate table here ... what to do ...
  //
  // I like keeping the schedule info out of the participant.  That settles it.
  ddsrt_avl_tree_t pp;
};

static int compare_xlocators_vwrap (const void *va, const void *vb) {
  return ddsi_compare_xlocators (va, vb);
}

static int compare_spdp_pp (const void *va, const void *vb) {
  const struct spdp_pp *a = va;
  const struct spdp_pp *b = vb;
  return ddsi_compare_guid (&a->pp->e.guid, &b->pp->e.guid);
}

static const ddsrt_avl_treedef_t spdp_loc_td = DDSRT_AVL_TREEDEF_INITIALIZER(offsetof (union spdp_loc_union, c.avlnode), offsetof (union spdp_loc_union, c.xloc), compare_xlocators_vwrap, NULL);
static const ddsrt_avl_treedef_t spdp_pp_td = DDSRT_AVL_TREEDEF_INITIALIZER(offsetof (struct spdp_pp, avlnode), 0, compare_spdp_pp, NULL);

struct add_as_disc_helper_arg {
  struct spdp_admin *adm;
  bool all_ok;
};

static void add_as_disc_helper (const ddsi_xlocator_t *loc, void *varg)
{
  struct add_as_disc_helper_arg * const arg = varg;
  // FIXME: some but not all initial locators need to go into aging
  if (arg->all_ok && ddsi_spdp_ref_locator (arg->adm, loc) != DDS_RETCODE_OK)
    arg->all_ok = false;
}

static void remove_as_disc_helper (const ddsi_xlocator_t *loc, void *varg)
{
  struct spdp_admin * const adm = varg;
  ddsi_spdp_unref_locator (adm, loc, false);
}

struct spdp_admin *ddsi_spdp_scheduler_new (struct ddsi_domaingv *gv)
{
  struct spdp_admin *adm;
  if ((adm = ddsrt_malloc_s (sizeof (*adm))) == NULL)
    return NULL;
  ddsrt_mutex_init (&adm->lock);
  adm->gv = gv;
  ddsrt_avl_init (&spdp_loc_td, &adm->aging);
  ddsrt_avl_init (&spdp_loc_td, &adm->live);
  ddsrt_avl_init (&spdp_pp_td, &adm->pp);
  
  struct add_as_disc_helper_arg arg = { .adm = adm, .all_ok = true };
  ddsi_addrset_forall (gv->as_disc, add_as_disc_helper, &arg);
  if (!arg.all_ok)
  {
    ddsrt_avl_free (&spdp_loc_td, &adm->live, ddsrt_free);
    // FIXME: need to do aging of initial locators, too
    ddsrt_avl_free (&spdp_loc_td, &adm->aging, ddsrt_free);
    ddsrt_mutex_destroy (&adm->lock);
    ddsrt_free (adm);
    return NULL;
  }
  
  // from here on we potentially have multiple threads messing with `adm`
  const ddsrt_mtime_t t_sched = ddsrt_mtime_add_duration (ddsrt_time_monotonic (), DDS_MSECS (0));
  adm->aging_xev = ddsi_qxev_callback (gv->xevents, t_sched, ddsi_spdp_handle_aging_locators_xevent_cb, NULL, 0, true);
  adm->live_xev = ddsi_qxev_callback (gv->xevents, t_sched, ddsi_spdp_handle_live_locators_xevent_cb, NULL, 0, true);
  return adm;
}

void ddsi_spdp_scheduler_delete (struct spdp_admin *adm)
{
  // FIXME: Initial addresses may still be around, probably should invert it, check refc=1, check present in as_disc, then free
  ddsi_addrset_forall (adm->gv->as_disc, remove_as_disc_helper, adm);
  assert (ddsrt_avl_is_empty (&adm->live));
  assert (ddsrt_avl_is_empty (&adm->pp));
  ddsi_delete_xevent (adm->aging_xev);
  ddsi_delete_xevent (adm->live_xev);
  // intrusive data structures, can simply free everything
  ddsrt_avl_free (&spdp_loc_td, &adm->aging, ddsrt_free);
  ddsrt_mutex_destroy (&adm->lock);
  ddsrt_free (adm);
}

#ifndef NDEBUG
static bool spdp_message_exists (const struct ddsi_participant *pp)
{
  struct ddsi_writer *spdp_wr;
  dds_return_t ret = ddsi_get_builtin_writer (pp, DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER, &spdp_wr);
  if (ret != DDS_RETCODE_OK || spdp_wr == NULL)
    return true;
  else
  {
    // Construct a key for looking up the SPDP sample in the WHC ...
    ddsi_plist_t ps;
    ddsi_plist_init_empty (&ps);
    ps.present |= PP_PARTICIPANT_GUID;
    ps.participant_guid = pp->e.guid;
    struct ddsi_serdata * const sdkey = ddsi_serdata_from_sample (pp->e.gv->spdp_type, SDK_KEY, &ps);
    ddsi_plist_fini (&ps);
    // ... then borrow it, surreptitiously increment its refcount before returning it so we can unlock earlier ...
    struct ddsi_whc_borrowed_sample sample;
    ddsrt_mutex_lock (&spdp_wr->e.lock);
    const bool exists = ddsi_whc_borrow_sample_key (spdp_wr->whc, sdkey, &sample);
    if (exists)
      ddsi_whc_return_sample (spdp_wr->whc, &sample, false);
    ddsrt_mutex_unlock (&spdp_wr->e.lock);
    return exists;
  }
}
#endif

dds_return_t ddsi_spdp_register_participant (struct spdp_admin *adm, const struct ddsi_participant *pp)
{
  /* SPDP periodic broadcast uses the retransmit path, so the initial
     publication must be done differently. Must be later than making
     the participant globally visible, or the SPDP processing won't
     recognise the participant as a local one. */
  if (ddsi_spdp_write ((struct ddsi_participant *) pp) < 0)
    return DDS_RETCODE_OK; // FIXME: Need to check why it can fail; this is what it used to do

  ddsi_spdp_force_republish (adm, pp);

  // FIXME: let's just cache the serdata in the participant and do all the publishing in this file

  // FIXME: what about the secure writer? It overwrites its as with as_disc on creation, but it is reliable and therefore the matching code will recompute it, so I don't think there is any need to do anything for that one (except possibly making sure it does get updated on a QoS change)
  
  // FIXME: why would I register a participant if its SPDP writer cannot be found? Maybe it can change ...
  assert (spdp_message_exists (pp));
  ddsrt_mutex_lock (&adm->lock);
  ddsrt_avl_ipath_t ip;
  const struct spdp_pp template = { .pp = pp };
  struct spdp_pp *ppn;
  ppn = ddsrt_avl_lookup_ipath (&spdp_pp_td, &adm->pp, &template, &ip);
  assert (ppn == NULL);
  (void) ppn;
  if ((ppn = ddsrt_malloc_s (sizeof (*ppn))) == NULL)
  {
    ddsrt_mutex_unlock (&adm->lock);
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  else
  {
    const ddsrt_mtime_t tsched = ddsrt_mtime_add_duration (ddsrt_time_monotonic(), DDS_MSECS (100)); // FIXME: initial schedule ...
    ppn->pp = pp;
    ppn->tsched = tsched;
    ddsrt_avl_insert_ipath (&spdp_pp_td, &adm->pp, ppn, &ip);
    ddsrt_mutex_unlock (&adm->lock);
    ddsi_resched_xevent_if_earlier (adm->live_xev, tsched);
    return DDS_RETCODE_OK;
  }
}

static struct ddsi_addrset *make_spdp_addrset (struct spdp_admin *adm)
{
  struct ddsi_addrset *as = ddsi_new_addrset ();
  ddsrt_mutex_lock (&adm->lock);
  ddsrt_avl_iter_t loc_it;
  for (struct spdp_loc_live *n = ddsrt_avl_iter_first (&spdp_loc_td, &adm->live, &loc_it); n; n = ddsrt_avl_iter_next (&loc_it))
    ddsi_add_xlocator_to_addrset (adm->gv, as, &n->c.xloc);
  for (struct spdp_loc_aging *n = ddsrt_avl_iter_first (&spdp_loc_td, &adm->aging, &loc_it); n; n = ddsrt_avl_iter_next (&loc_it))
    ddsi_add_xlocator_to_addrset (adm->gv, as, &n->c.xloc);
  ddsrt_mutex_unlock (&adm->lock);
  return as;
}

void ddsi_spdp_unregister_participant (struct spdp_admin *adm, const struct ddsi_participant *pp)
{
  ddsrt_mutex_lock (&adm->lock);
  ddsrt_avl_dpath_t dp;
  const struct spdp_pp template = { .pp = pp };
  struct spdp_pp *ppn;
  // FIXME: do I really have to allow for ppn == NULL?
  if ((ppn = ddsrt_avl_lookup_dpath (&spdp_pp_td, &adm->pp, &template, &dp)) != NULL)
  {
    assert (ppn->pp == pp);
    ddsrt_avl_delete_dpath (&spdp_pp_td, &adm->pp, ppn, &dp);
    ddsrt_free (ppn);
  }
  ddsrt_mutex_unlock (&adm->lock);
  
  /* SPDP relies on the WHC, but dispose-unregister will empty
     it. The event handler verifies the event has already been
     scheduled for deletion when it runs into an empty WHC */
  // FIXME: store serdata in pp and this hack can go away
  // HACK HACK HACK
  struct ddsi_addrset *as = make_spdp_addrset (adm);
  const ddsi_guid_t subguid = { .prefix = pp->e.guid.prefix, .entityid = { .u = DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER } };
  struct ddsi_writer *wr = ddsi_entidx_lookup_writer_guid (pp->e.gv->entity_index, &subguid);
  assert (wr != NULL);
  ddsrt_mutex_lock (&wr->e.lock);
  ddsi_unref_addrset (wr->as);
  wr->as = as;
  ddsrt_mutex_unlock (&wr->e.lock);
  // HACK HACK HACK
  // FIXME: this currently also handles the secure SPDP message, but that may change
  ddsi_spdp_dispose_unregister ((struct ddsi_participant *) pp);
  // HACK HACK HACK
  ddsrt_mutex_lock (&wr->e.lock);
  ddsi_unref_addrset (wr->as);
  wr->as = ddsi_new_addrset ();
  ddsrt_mutex_unlock (&wr->e.lock);
  // HACK HACK HACK
}

dds_return_t ddsi_spdp_ref_locator (struct spdp_admin *adm, const ddsi_xlocator_t *xloc)
{
  dds_return_t ret = DDS_RETCODE_OK;
  union spdp_loc_union *n;
  ddsrt_mutex_lock (&adm->lock);
  union {
    ddsrt_avl_ipath_t ip;
    ddsrt_avl_dpath_t dp;
  } avlpath;
  if ((n = ddsrt_avl_lookup_dpath (&spdp_loc_td, &adm->aging, xloc, &avlpath.dp)) != NULL)
  {
    ddsrt_avl_delete_dpath (&spdp_loc_td, &adm->aging, n, &avlpath.dp);
    n->live.proxypp_refc = 1;
    n->live.lease_expiry_occurred = false;
    ddsrt_avl_insert (&spdp_loc_td, &adm->live, n);
  }
  else if ((n = ddsrt_avl_lookup_ipath (&spdp_loc_td, &adm->live, xloc, &avlpath.ip)) != NULL)
  {
    n->live.proxypp_refc++;
  }
  else if ((n = ddsrt_malloc_s (sizeof (*n))) != NULL)
  {
    n->c.xloc = *xloc;
    n->live.proxypp_refc = 1;
    n->live.lease_expiry_occurred = false;
    ddsrt_avl_insert_ipath (&spdp_loc_td, &adm->live, n, &avlpath.ip);
  }
  else
  {
    // fatal: we could kinda continue even in the absence of memory, but must prevent the
    // proxy from attempting to decrement the refcount when it gets deleted
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
  }
  ddsrt_mutex_unlock (&adm->lock);
  return ret;
}

void ddsi_spdp_unref_locator (struct spdp_admin *adm, const ddsi_xlocator_t *xloc, bool on_lease_expiry)
{
  union spdp_loc_union *n;
  ddsrt_mutex_lock (&adm->lock);
  ddsrt_avl_dpath_t dp;
  n = ddsrt_avl_lookup_dpath (&spdp_loc_td, &adm->live, xloc, &dp);
  assert (n != NULL);
  assert (n->live.proxypp_refc > 0);
  if (on_lease_expiry)
    n->live.lease_expiry_occurred = true;
  if (--n->live.proxypp_refc == 0)
  {
    ddsrt_avl_delete_dpath (&spdp_loc_td, &adm->live, n, &dp);
    assert (ddsrt_avl_lookup (&spdp_loc_td, &adm->aging, xloc) == NULL);
    // If all proxy participants informed us they were being deleted, then we don't need to
    // start aging it
    //
    // What if it is shortly after startup and we'd still be pinging it if there hadn't been
    // a proxy participant at this address?  It is unicast, so if there are others it is
    // reasonable to assume they would all have discovered us at the same time (true for
    // Cyclone anyway) and so there won't be anything at this locator until a new one is
    // created.  For that case, we can reasonably rely on that new one.
    if (!n->live.lease_expiry_occurred)
      ddsrt_free (n);
    else
    {
      // FIXME: Discovery/Peers: user needs to set timeout for each locator, that should be used here for those in the initial set
      // FIXME: for those learnt along the way, an appropriate configuration setting needs to be added (here 10 times/10 minutes)
      // the idea is to ping at least several (10) times and keep trying for at least several (10) minutes
      const dds_duration_t base_intv = adm->gv->config.spdp_interval.isdefault ? DDS_SECS (30) : adm->gv->config.spdp_interval.value;
      n->aging.age = (base_intv < 1 || base_intv >= DDS_SECS (60)) ? 10 : (uint32_t) (DDS_SECS (600) / base_intv);
      n->aging.tsched = ddsrt_mtime_add_duration (ddsrt_time_monotonic (), base_intv);
      ddsrt_avl_insert (&spdp_loc_td, &adm->aging, n);
    }
  }
  ddsrt_mutex_unlock (&adm->lock);
}

ddsrt_nonnull ((2, 3))
static void resend_spdp (struct ddsi_xpack *xp, const struct ddsi_participant *pp, const ddsi_xlocator_t *xloc)
{
  struct ddsi_writer *spdp_wr;
  dds_return_t ret = ddsi_get_builtin_writer (pp, DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER, &spdp_wr);
  if (ret != DDS_RETCODE_OK || spdp_wr == NULL)
    return;
  // Construct a key for looking up the SPDP sample in the WHC ...
  // FIXME: this is just way too inefficient (and we do it over and over ...)
  // FIXME: why not cache a pointer to it in pp?
  ddsi_plist_t ps;
  ddsi_plist_init_empty (&ps);
  ps.present |= PP_PARTICIPANT_GUID;
  ps.participant_guid = pp->e.guid;
  struct ddsi_serdata * const sdkey = ddsi_serdata_from_sample (pp->e.gv->spdp_type, SDK_KEY, &ps);
  ddsi_plist_fini (&ps);
  // ... then borrow it, surreptitiously increment its refcount before returning it so we can unlock earlier ...
  struct ddsi_whc_borrowed_sample sample;
  ddsrt_mutex_lock (&spdp_wr->e.lock);
  if (!ddsi_whc_borrow_sample_key (spdp_wr->whc, sdkey, &sample))
    sample.serdata = NULL;
  else
  {
    (void) ddsi_serdata_ref (sample.serdata);
    ddsi_whc_return_sample (spdp_wr->whc, &sample, false);
  }
  ddsrt_mutex_unlock (&spdp_wr->e.lock);
  ddsi_serdata_unref (sdkey);
  // ... and send it
  if (sample.serdata)
  {
    static const ddsi_guid_prefix_t nullguidprefix;
    struct ddsi_xmsg *msg;
    if (ddsi_create_fragment_message (spdp_wr, sample.seq, sample.serdata, 0, UINT16_MAX, NULL, &msg, 1, UINT32_MAX) >= 0)
    {
      // FIXME: ddsi_create_fragment_message set the wrong destination, maybe refactor that?
      ddsi_xmsg_setdst1_generic (pp->e.gv, msg, &nullguidprefix, xloc);
      if (xp)
        ddsi_xpack_addmsg (xp, msg, 0);
      else
        ddsi_qxev_msg (pp->e.gv->xevents, msg);
    }
    ddsi_serdata_unref (sample.serdata);
  }
}

ddsrt_nonnull_all
static ddsrt_mtime_t spdp_do_aging_locators (struct spdp_admin *adm, struct ddsi_xpack *xp, ddsrt_mtime_t tnow)
{
  const dds_duration_t t_coalesce = DDS_SECS (1); // aging, so low rate
  const ddsrt_mtime_t t_cutoff = ddsrt_mtime_add_duration (tnow, t_coalesce);
  ddsrt_mtime_t t_sched = DDSRT_MTIME_NEVER;
  ddsrt_mutex_lock (&adm->lock);
  struct spdp_loc_aging *n = ddsrt_avl_find_max (&spdp_loc_td, &adm->aging);
  while (n != NULL)
  {
    struct spdp_loc_aging * const nextn = ddsrt_avl_find_succ (&spdp_loc_td, &adm->aging, n);
    if (t_cutoff.v < n->tsched.v)
    {
      if (n->tsched.v < t_sched.v)
        t_sched = n->tsched;
    }
    else
    {
      ddsrt_avl_iter_t it;
      for (struct spdp_pp *ppn = ddsrt_avl_iter_first (&spdp_pp_td, &adm->pp, &it); ppn; ppn = ddsrt_avl_iter_next (&it))
        resend_spdp (xp, ppn->pp, &n->c.xloc);
      // Why do we keep trying an address where there used to be one if there's no one anymore? Well,
      // there might be someone else in the same situation with the cable cut ...
      //
      // That of course is interesting only if the disappearance of that node was detected by a lease
      // expiration.  If all participants behind the locator tell us they will be gone, it is a
      // different situation: then we can (possibly) delete it immedialy.
      //
      // There is also the case of the initial set of addresses, there we (probably) want to drop the
      // rate over time, but let's not do so now.
      //
      // So the strategy is to resend for a long time with a decreasing frequency
      // default interval is 30s FIXME: tweak configurability
      // exponential back-off would be the classic trick but is very rapid
      // in this case, arguably, just pinging at the default frequency for
      // a limited amount of time seems to make more sense
      if (--n->age == 0)
      {
        ddsrt_avl_delete (&spdp_loc_td, &adm->aging, n);
        ddsrt_free (n);
      }
      else
      {
        // FIXME: this base_intv thing needs to be done smarter, or else combined with other places
        const dds_duration_t base_intv = adm->gv->config.spdp_interval.isdefault ? DDS_SECS (30) : adm->gv->config.spdp_interval.value;
        n->tsched = ddsrt_mtime_add_duration (tnow, base_intv);
        if (n->tsched.v < t_sched.v)
          t_sched = n->tsched;
      }
    }
    n = nextn;
  }
  ddsrt_mutex_unlock (&adm->lock);
  return t_sched;
}

// unlocked access, but that's ok because the lease duration can't be changed
// and therefore it could even be marked pure
ddsrt_nonnull_all
static dds_duration_t spdp_intv (const struct ddsi_participant *pp)
{
  // FIXME: can easily cache this in pp
  if (!pp->e.gv->config.spdp_interval.isdefault)
    return pp->e.gv->config.spdp_interval.value;
  else
  {
    // Default interval is 80% of the lease duration with a bit of fiddling around the
    // edges (similar to PMD), and with an upper limit
    const dds_duration_t mindelta = DDS_MSECS (10);
    const dds_duration_t ldur = pp->plist->qos.liveliness.lease_duration;
    dds_duration_t intv;
    if (ldur < 5 * mindelta / 4)
      intv = mindelta;
    else if (ldur < DDS_SECS (10))
      intv = 4 * ldur / 5;
    else
      intv = ldur - DDS_SECS (2);
    // Historical maximum interval is 30s, stick to that
    if (intv > DDS_SECS (30))
      intv = DDS_SECS (30);
    return intv;
  }
}

ddsrt_nonnull_all
static ddsrt_mtime_t spdp_do_live_locators (struct spdp_admin *adm, struct ddsi_xpack *xp, ddsrt_mtime_t tnow)
{
  const dds_duration_t t_coalesce = DDS_MSECS (100); // let's be a bit more precise than for aging
  const ddsrt_mtime_t t_cutoff = ddsrt_mtime_add_duration (tnow, t_coalesce);
  // Send SPDP messages first, then sort out the new times for sending SPDP messages
  // this is because we really want to order them by destination address, because that
  // way we can combine the SPDP messages into larger RTPS messages
  ddsrt_mutex_lock (&adm->lock);
  ddsrt_avl_iter_t loc_it, pp_it;
  for (struct spdp_loc_live *n = ddsrt_avl_iter_first (&spdp_loc_td, &adm->live, &loc_it); n; n = ddsrt_avl_iter_next (&loc_it))
    for (struct spdp_pp *ppn = ddsrt_avl_iter_first (&spdp_pp_td, &adm->pp, &pp_it); ppn; ppn = ddsrt_avl_iter_next (&pp_it))
      if (t_cutoff.v >= ppn->tsched.v)
        resend_spdp (xp, ppn->pp, &n->c.xloc);
  // Update schedule
  ddsrt_mtime_t t_sched = DDSRT_MTIME_NEVER;
  for (struct spdp_pp *ppn = ddsrt_avl_iter_first (&spdp_pp_td, &adm->pp, &pp_it); ppn; ppn = ddsrt_avl_iter_next (&pp_it))
  {
    if (t_cutoff.v >= ppn->tsched.v)
      ppn->tsched = ddsrt_mtime_add_duration (tnow, spdp_intv (ppn->pp));
    if (ppn->tsched.v < t_sched.v)
      t_sched = ppn->tsched;
  }
  ddsrt_mutex_unlock (&adm->lock);
  return t_sched;
}

void ddsi_spdp_handle_aging_locators_xevent_cb (struct ddsi_domaingv *gv, struct ddsi_xevent *xev, struct ddsi_xpack *xp, void *varg, ddsrt_mtime_t tnow)
{
  (void) varg;
  const ddsrt_mtime_t t_sched = spdp_do_aging_locators (gv->spdp_schedule, xp, tnow);
  ddsi_resched_xevent_if_earlier (xev, t_sched);
}

void ddsi_spdp_handle_live_locators_xevent_cb (struct ddsi_domaingv *gv, struct ddsi_xevent *xev, struct ddsi_xpack *xp, void *varg, ddsrt_mtime_t tnow)
{
  (void) varg;
  const ddsrt_mtime_t t_sched = spdp_do_live_locators (gv->spdp_schedule, xp, tnow);
  ddsi_resched_xevent_if_earlier (xev, t_sched);
}

void ddsi_spdp_force_republish (struct spdp_admin *adm, const struct ddsi_participant *pp)
{
  // Used for: initial publication, QoS update, dispose+unregister, faster rediscovery in
  // oneliner in implementation of "hearing!"
  //
  // It seems there's no need to update the scheduled next publication for any of these cases.
  //
  // This gets called from various threads and not all of them have a message packer at hand.
  // Passing a null pointer for "xpack" hands it off to the tev thread for publication.
  ddsrt_mutex_lock (&adm->lock);
  ddsrt_avl_iter_t loc_it;
  for (struct spdp_loc_live *n = ddsrt_avl_iter_first (&spdp_loc_td, &adm->live, &loc_it); n; n = ddsrt_avl_iter_next (&loc_it))
    resend_spdp (NULL, pp, &n->c.xloc);
  for (struct spdp_loc_aging *n = ddsrt_avl_iter_first (&spdp_loc_td, &adm->aging, &loc_it); n; n = ddsrt_avl_iter_next (&loc_it))
    resend_spdp (NULL, pp, &n->c.xloc);
  ddsrt_mutex_unlock (&adm->lock);
}

