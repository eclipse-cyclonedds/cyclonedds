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
#include "ddsi__portmapping.h"
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
  bool discovered; // true iff we discovered the existence of this locator through discovery
  ddsrt_mtime_t tprune; // pruning only occurs for "aging" locators; set to 0 when discovering a new address
};

struct spdp_loc_aging {
  struct spdp_loc_common c;
  ddsrt_mtime_t tsched; // time at which to ping this locator again
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

struct handle_locators_xevent_arg {
  struct spdp_admin *adm;
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

static dds_return_t add_peer_address_xlocator (struct spdp_admin *adm, const ddsi_xlocator_t *xloc, dds_duration_t prune_delay)
{
  // Used for initial addresses only.  These are all inserted as "aging" so there cannot
  // be any live addresses yet.
  assert (ddsrt_avl_is_empty (&adm->live));
  dds_return_t ret = DDS_RETCODE_OK;
  const ddsrt_mtime_t tnow = ddsrt_time_monotonic ();
  const ddsrt_mtime_t tprune = ddsrt_mtime_add_duration (tnow, prune_delay);
  union spdp_loc_union *n;
  ddsrt_mutex_lock (&adm->lock);
  union {
    ddsrt_avl_ipath_t ip;
    ddsrt_avl_dpath_t dp;
  } avlpath;
  if ((n = ddsrt_avl_lookup_ipath (&spdp_loc_td, &adm->aging, xloc, &avlpath.ip)) != NULL)
  {
    // duplicate: take the maximum prune_delay, that seems a reasonable-enough approach
    if (tprune.v > n->c.tprune.v)
      n->c.tprune = tprune;
  }
  else if ((n = ddsrt_malloc_s (sizeof (*n))) != NULL)
  {
    n->c.xloc = *xloc;
    n->c.tprune = tprune;
    n->c.discovered = false;
    // FIXME: initial schedule, should be "NEVER" in the absence of participants (but that isn't going to happen)
    n->aging.tsched = ddsrt_mtime_add_duration (tnow, DDS_MSECS (100));
    ddsrt_avl_insert_ipath (&spdp_loc_td, &adm->aging, n, &avlpath.ip);
  }
  else
  {
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
  }
  ddsrt_mutex_unlock (&adm->lock);
  return ret;
}

static dds_return_t add_peer_address_ports_interface (struct spdp_admin *adm, const ddsi_locator_t *loc, dds_duration_t prune_delay)
{
  struct ddsi_domaingv const * const gv = adm->gv;
  dds_return_t rc = DDS_RETCODE_OK;
  if (ddsi_is_unspec_locator (loc))
    return rc;
  if (ddsi_is_mcaddr (gv, loc))
  {
    // multicast: use all transmit connections
    for (int i = 0; i < gv->n_interfaces && rc == DDS_RETCODE_OK; i++)
    {
      if (ddsi_factory_supports (gv->xmit_conns[i]->m_factory, loc->kind))
        rc = add_peer_address_xlocator (adm, &(const ddsi_xlocator_t) {
          .conn = gv->xmit_conns[i],
          .c = *loc }, prune_delay);
    }
  }
  else
  {
    // unicast: assume the kernel knows how to route it from any connection
    // if it doesn't match a local interface
    int interf_idx = -1, fallback_interf_idx = -1;
    for (int i = 0; i < gv->n_interfaces && interf_idx < 0; i++)
    {
      if (!ddsi_factory_supports (gv->xmit_conns[i]->m_factory, loc->kind))
        continue;
      switch (ddsi_is_nearby_address (gv, loc, (size_t) gv->n_interfaces, gv->interfaces, NULL))
      {
        case DNAR_SELF:
        case DNAR_LOCAL:
          interf_idx = i;
          break;
        case DNAR_DISTANT:
          if (fallback_interf_idx < 0)
            fallback_interf_idx = i;
          break;
        case DNAR_UNREACHABLE:
          break;
      }
    }
    if (interf_idx >= 0 || fallback_interf_idx >= 0)
    {
      const int i = (interf_idx >= 0) ? interf_idx : fallback_interf_idx;
      rc = add_peer_address_xlocator (adm, &(const ddsi_xlocator_t) {
        .conn = gv->xmit_conns[i],
        .c = *loc }, prune_delay);
    }
  }
  return rc;
}

static dds_return_t add_peer_address_ports (struct spdp_admin *adm, ddsi_locator_t *loc, dds_duration_t prune_delay)
{
  struct ddsi_domaingv const * const gv = adm->gv;
  struct ddsi_tran_factory * const tran = ddsi_factory_find_supported_kind (gv, loc->kind);
  assert (tran != NULL);
  char buf[DDSI_LOCSTRLEN];
  int32_t maxidx;
  dds_return_t rc = DDS_RETCODE_OK;

  // check whether port number, address type and mode make sense, and prepare the
  // locator by patching the first port number to use if none is given
  if (ddsi_tran_get_locator_port (tran, loc) != DDSI_LOCATOR_PORT_INVALID)
  {
    maxidx = 0;
  }
  else if (ddsi_is_mcaddr (gv, loc))
  {
    ddsi_tran_set_locator_port (tran, loc, ddsi_get_port (&gv->config, DDSI_PORT_MULTI_DISC, 0));
    maxidx = 0;
  }
  else
  {
    ddsi_tran_set_locator_port (tran, loc, ddsi_get_port (&gv->config, DDSI_PORT_UNI_DISC, 0));
    maxidx = gv->config.maxAutoParticipantIndex;
  }

  GVLOG (DDS_LC_CONFIG, "add_peer_address: add %s", ddsi_locator_to_string (buf, sizeof (buf), loc));
  rc = add_peer_address_ports_interface (adm, loc, prune_delay);
  for (int32_t i = 1; i <= maxidx && rc == DDS_RETCODE_OK; i++)
  {
    ddsi_tran_set_locator_port (tran, loc, ddsi_get_port (&gv->config, DDSI_PORT_UNI_DISC, i));
    if (i == maxidx)
      GVLOG (DDS_LC_CONFIG, " ... :%"PRIu32, loc->port);
    rc = add_peer_address_ports_interface (adm, loc, prune_delay);
  }
  GVLOG (DDS_LC_CONFIG, " (prune delay %"PRId64")\n", prune_delay);
  return rc;
}

static dds_return_t add_peer_address (struct spdp_admin *adm, const char *addrs, dds_duration_t prune_delay)
{
  DDSRT_WARNING_MSVC_OFF(4996);
  struct ddsi_domaingv const * const gv = adm->gv;
  char *addrs_copy, *cursor, *a;
  dds_return_t rc = DDS_RETCODE_ERROR;
  addrs_copy = ddsrt_strdup (addrs);
  cursor = addrs_copy;
  while ((a = ddsrt_strsep (&cursor, ",")) != NULL)
  {
    ddsi_locator_t loc;
    switch (ddsi_locator_from_string (gv, &loc, a, gv->m_factory))
    {
      case AFSR_OK:
        break;
      case AFSR_INVALID:
        GVERROR ("add_peer_address: %s: not a valid address\n", a);
        goto error;
      case AFSR_UNKNOWN:
        GVERROR ("add_peer_address: %s: unknown address\n", a);
        goto error;
      case AFSR_MISMATCH:
        GVERROR ("add_peer_address: %s: address family mismatch\n", a);
        goto error;
    }
    if ((rc = add_peer_address_ports (adm, &loc, prune_delay)) < 0)
    {
      goto error;
    }
  }
  rc = DDS_RETCODE_OK;
 error:
  ddsrt_free (addrs_copy);
  return rc;
  DDSRT_WARNING_MSVC_ON(4996);
}

static dds_return_t add_peer_addresses (struct spdp_admin *adm, const struct ddsi_config_peer_listelem *list)
{
  dds_return_t rc = DDS_RETCODE_OK;
  while (list && rc == DDS_RETCODE_OK)
  {
    const dds_duration_t prune_delay = list->prune_delay.isdefault ? adm->gv->config.spdp_prune_delay_initial : list->prune_delay.value;
    rc = add_peer_address (adm, list->peer, prune_delay);
    list = list->next;
  }
  return rc;
}

static dds_return_t populate_initial_addresses (struct spdp_admin *adm, bool add_localhost)
{
  struct ddsi_domaingv const * const gv = adm->gv;
  dds_return_t rc = DDS_RETCODE_OK;

  // There is no difference in the resulting configuration if the config.peers gets added
  // first or the automatic localhost is: because it takes the maximum of the prune delay.
  if (rc == DDS_RETCODE_OK && gv->config.peers)
  {
    rc = add_peer_addresses (adm, gv->config.peers);
  }
  if (rc == DDS_RETCODE_OK && add_localhost)
  {
    struct ddsi_config_peer_listelem peer_local;
    char local_addr[DDSI_LOCSTRLEN];
    ddsi_locator_to_string_no_port (local_addr, sizeof (local_addr), &gv->interfaces[0].loc);
    GVLOG (DDS_LC_CONFIG, "adding self (%s)\n", local_addr);
    peer_local.next = NULL;
    peer_local.peer = local_addr;
    peer_local.prune_delay.isdefault = true;
    rc = add_peer_addresses (adm, &peer_local);
  }

  // Add default multicast addresses for interfaces on which multicast SPDP is enabled only
  // once all initial addresses have been added: that way, "add_peer_addresses" can assert
  // that the live tree is still empty and trivially guarantee the invariant that the set of
  // live ones is disjoint from the set of aging ones.
  //
  // ddsi_spdp_ref_locator takes care to move addresses over from the aging ones to the live
  // ones, and so we need not worry about someone adding the multicast as a peer.
  for (int i = 0; rc == DDS_RETCODE_OK && i < gv->n_interfaces; i++)
  {
    if ((gv->interfaces[i].allow_multicast & DDSI_AMC_SPDP) &&
        ddsi_factory_supports (gv->xmit_conns[i]->m_factory, gv->loc_spdp_mc.kind))
    {
      const ddsi_xlocator_t xloc = { .conn = gv->xmit_conns[i], .c = gv->loc_spdp_mc };
      // multicast discovery addresses never expire
      char buf[DDSI_LOCSTRLEN];
      GVLOG (DDS_LC_CONFIG, "interface %s has spdp multicast enabled, adding %s (never expiring)\n",
             gv->interfaces[i].name, ddsi_xlocator_to_string (buf, sizeof (buf), &xloc));
      rc = ddsi_spdp_ref_locator (adm, &xloc, false);
    }
  }
  return rc;
}

struct spdp_admin *ddsi_spdp_scheduler_new (struct ddsi_domaingv *gv, bool add_localhost)
{
  struct spdp_admin *adm;
  if ((adm = ddsrt_malloc_s (sizeof (*adm))) == NULL)
    return NULL;
  ddsrt_mutex_init (&adm->lock);
  adm->gv = gv;
  ddsrt_avl_init (&spdp_loc_td, &adm->aging);
  ddsrt_avl_init (&spdp_loc_td, &adm->live);
  ddsrt_avl_init (&spdp_pp_td, &adm->pp);

  if (populate_initial_addresses (adm, add_localhost) != DDS_RETCODE_OK)
  {
    ddsrt_avl_free (&spdp_loc_td, &adm->live, ddsrt_free);
    ddsrt_avl_free (&spdp_loc_td, &adm->aging, ddsrt_free);
    ddsrt_mutex_destroy (&adm->lock);
    ddsrt_free (adm);
    return NULL;
  }
  else
  {
    // from here on we potentially have multiple threads messing with `adm`
    const ddsrt_mtime_t t_sched = ddsrt_mtime_add_duration (ddsrt_time_monotonic (), DDS_MSECS (0));
    struct handle_locators_xevent_arg arg = { .adm = adm };
    adm->aging_xev = ddsi_qxev_callback (adm->gv->xevents, t_sched, ddsi_spdp_handle_aging_locators_xevent_cb, &arg, sizeof (arg), true);
    adm->live_xev = ddsi_qxev_callback (adm->gv->xevents, t_sched, ddsi_spdp_handle_live_locators_xevent_cb, &arg, sizeof (arg), true);
    return adm;
  }
}

void ddsi_spdp_scheduler_delete (struct spdp_admin *adm)
{
  assert (ddsrt_avl_is_empty (&adm->pp));
#ifndef NDEBUG
  {
    ddsrt_avl_iter_t it;
    for (struct spdp_loc_live *n = ddsrt_avl_iter_first (&spdp_loc_td, &adm->live, &it); n; n = ddsrt_avl_iter_next (&it))
      assert (n->proxypp_refc == 1);
  }
#endif
  ddsi_delete_xevent (adm->aging_xev);
  ddsi_delete_xevent (adm->live_xev);
  // intrusive data structures, can simply free everything
  ddsrt_avl_free (&spdp_loc_td, &adm->live, ddsrt_free);
  ddsrt_avl_free (&spdp_loc_td, &adm->aging, ddsrt_free);
  ddsrt_mutex_destroy (&adm->lock);
  ddsrt_free (adm);
}

dds_return_t ddsi_spdp_register_participant (struct spdp_admin *adm, const struct ddsi_participant *pp)
{
#ifndef NDEBUG
  ddsrt_mutex_lock ((ddsrt_mutex_t *) &pp->e.lock);
  assert (pp->spdp_seqno == 1);
  assert (pp->spdp_serdata);
  assert (pp->spdp_serdata->statusinfo == 0);
  ddsrt_mutex_unlock ((ddsrt_mutex_t *) &pp->e.lock);
#endif
  ddsi_spdp_force_republish (adm, pp, NULL);

  // FIXME: let's just cache the serdata in the participant and do all the publishing in this file

  // FIXME: what about the secure writer? It overwrites its as with as_disc on creation, but it is reliable and therefore the matching code will recompute it, so I don't think there is any need to do anything for that one (except possibly making sure it does get updated on a QoS change)

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

void ddsi_spdp_unregister_participant (struct spdp_admin *adm, const struct ddsi_participant *pp)
{
#ifndef NDEBUG
  ddsrt_mutex_lock ((ddsrt_mutex_t *) &pp->e.lock);
  assert (pp->spdp_seqno > 1);
  assert (pp->spdp_serdata);
  assert (pp->spdp_serdata->statusinfo == (DDSI_STATUSINFO_DISPOSE | DDSI_STATUSINFO_UNREGISTER));
  ddsrt_mutex_unlock ((ddsrt_mutex_t *) &pp->e.lock);
#endif
  ddsi_spdp_force_republish (adm, pp, NULL);

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
}

dds_return_t ddsi_spdp_ref_locator (struct spdp_admin *adm, const ddsi_xlocator_t *xloc, bool discovered)
{
  dds_return_t ret = DDS_RETCODE_OK;
  union spdp_loc_union *n;
  char locstr[DDSI_LOCSTRLEN];
  struct ddsi_domaingv * const gv = adm->gv;
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
    GVTRACE ("spdp: ref aging loc %s, now live (refc = %"PRIu32", tprune = %"PRId64")\n", ddsi_xlocator_to_string (locstr, sizeof (locstr), xloc), n->live.proxypp_refc, n->c.tprune.v);
  }
  else if ((n = ddsrt_avl_lookup_ipath (&spdp_loc_td, &adm->live, xloc, &avlpath.ip)) != NULL)
  {
    n->live.proxypp_refc++;
    GVTRACE ("spdp: ref live loc %s (refc = %"PRIu32", tprune = %"PRId64")\n", ddsi_xlocator_to_string (locstr, sizeof (locstr), xloc), n->live.proxypp_refc, n->c.tprune.v);
  }
  else if ((n = ddsrt_malloc_s (sizeof (*n))) != NULL)
  {
    n->c.xloc = *xloc;
    n->c.tprune.v = 0;
    n->c.discovered = discovered;
    n->live.proxypp_refc = 1;
    n->live.lease_expiry_occurred = false;
    ddsrt_avl_insert_ipath (&spdp_loc_td, &adm->live, n, &avlpath.ip);
    GVTRACE ("spdp: new live loc %s (refc = %"PRIu32", tprune = %"PRId64")\n", ddsi_xlocator_to_string (locstr, sizeof (locstr), xloc), n->live.proxypp_refc, n->c.tprune.v);
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
  char locstr[DDSI_LOCSTRLEN];
  struct ddsi_domaingv * const gv = adm->gv;
  n = ddsrt_avl_lookup_dpath (&spdp_loc_td, &adm->live, xloc, &dp);
  assert (n != NULL);
  assert (n->live.proxypp_refc > 0);
  if (on_lease_expiry)
    n->live.lease_expiry_occurred = true;
  if (--n->live.proxypp_refc > 0)
  {
    GVTRACE ("spdp: unref live loc %s (refc = %"PRIu32", tprune = %"PRId64")\n", ddsi_xlocator_to_string (locstr, sizeof (locstr), xloc), n->live.proxypp_refc, n->c.tprune.v);
  }
  else
  {
    ddsrt_avl_delete_dpath (&spdp_loc_td, &adm->live, n, &dp);
    assert (ddsrt_avl_lookup (&spdp_loc_td, &adm->aging, xloc) == NULL);
    const ddsrt_mtime_t tnow = ddsrt_time_monotonic ();
    if (!n->live.lease_expiry_occurred && n->c.discovered)
    {
      // If all proxy participants at the locator informed us they were being deleted,
      // and the address is one we discovered, drop it immediately (on the assumption
      // that next time it is used, we'll discover it again)
      GVTRACE ("spdp: drop live loc %s: delete (explicit, discovered)\n", ddsi_xlocator_to_string (locstr, sizeof (locstr), xloc));
      ddsrt_free (n);
    }
    else if (!n->live.lease_expiry_occurred && n->c.tprune.v <= tnow.v)
    {
      // What if it is shortly after startup and we'd still be pinging it if there hadn't been
      // a proxy participant at this address?  It is unicast, so if there are others it is
      // reasonable to assume they would all have discovered us at the same time (true for
      // Cyclone anyway) and so there won't be anything at this locator until a new one is
      // created.  For that case, we can reasonably rely on that new one.

      // FIXME: do I want really to drop it immediately even if the address was configured and wouldn't have expired yet? no, right?
      GVTRACE ("spdp: drop live loc %s: delete (%s, tprune = %"PRId64")\n", ddsi_xlocator_to_string (locstr, sizeof (locstr), xloc), n->live.lease_expiry_occurred ? "implicit" : "explicit", n->c.tprune.v);
      ddsrt_free (n);
    }
    else
    {
      // FIXME: Discovery/Peers: user needs to set timeout for each locator, that should be used here for those in the initial set
      // FIXME: for those learnt along the way, an appropriate configuration setting needs to be added (here 10 times/10 minutes)
      // the idea is to ping at least several (10) times and keep trying for at least several (10) minutes
      const dds_duration_t base_intv = adm->gv->config.spdp_interval.isdefault ? DDS_SECS (30) : adm->gv->config.spdp_interval.value;
      ddsrt_mtime_t tprune = ddsrt_mtime_add_duration (tnow, gv->config.spdp_prune_delay_discovered);
      if (tprune.v > n->c.tprune.v)
        n->c.tprune = tprune;
      n->aging.tsched = ddsrt_mtime_add_duration (tnow, base_intv);
      ddsrt_avl_insert (&spdp_loc_td, &adm->aging, n);
      GVTRACE ("spdp: drop live loc %s: now aging (tprune = %"PRId64")\n", ddsi_xlocator_to_string (locstr, sizeof (locstr), xloc), n->c.tprune.v);
      ddsi_resched_xevent_if_earlier (adm->aging_xev, (n->aging.tsched.v < n->c.tprune.v) ? n->aging.tsched : n->c.tprune);
    }
  }
  ddsrt_mutex_unlock (&adm->lock);
}

enum resend_spdp_dst_kind {
  RSDK_LOCATOR,
  RSDK_PROXY_READER
};

struct resend_spdp_dst {
  enum resend_spdp_dst_kind kind;
  union {
    const ddsi_xlocator_t *xloc;
    const struct ddsi_proxy_reader *prd;
  } u;
};

ddsrt_nonnull ((2, 3))
static void resend_spdp (struct ddsi_xpack *xp, const struct ddsi_participant *pp, const struct resend_spdp_dst *dst)
{
  // SPDP writer serves no purpose other than providing some things to ddsi_create_fragment_message, and for
  // SPDP most of that information is in the uninteresting state (because it is best-effort and can't be
  // fragmented) but passing all that info in a different way is also unpleasant.
  struct ddsi_writer *spdp_wr;
  dds_return_t ret = ddsi_get_builtin_writer (pp, DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER, &spdp_wr);
  if (ret != DDS_RETCODE_OK || spdp_wr == NULL)
  {
    ETRACE (pp, "ddsi_spdp_write("PGUIDFMT") - builtin participant writer not found\n", PGUID (pp->e.guid));
    return;
  }

  const ddsi_xlocator_t *xloc = NULL;
  const struct ddsi_proxy_reader *prd = NULL;
  switch (dst->kind)
  {
    case RSDK_LOCATOR:
      xloc = dst->u.xloc;
      prd = NULL;
      break;
    case RSDK_PROXY_READER:
      xloc = NULL;
      prd = dst->u.prd;
      break;
  }
  assert ((xloc != NULL) != (prd != NULL));

  ddsrt_mutex_lock ((ddsrt_mutex_t *) &pp->e.lock);
  ddsi_seqno_t seqno = pp->spdp_seqno;
  struct ddsi_serdata *serdata = ddsi_serdata_ref (pp->spdp_serdata);
  ddsrt_mutex_unlock ((ddsrt_mutex_t *) &pp->e.lock);

  static const ddsi_guid_prefix_t nullguidprefix;
  struct ddsi_xmsg *msg;
  // FIXME: need a spdp_wr for this, but we don't need one for any other reason (can't fragment them anyway, so its trivial)
  if (ddsi_create_fragment_message (spdp_wr, seqno, serdata, 0, UINT16_MAX, prd, &msg, 1, UINT32_MAX) >= 0)
  {
    // FIXME: ddsi_create_fragment_message set the wrong destination so we have to patch it. Maybe refactor that?
    if (xloc)
      ddsi_xmsg_setdst1_generic (pp->e.gv, msg, &nullguidprefix, xloc);

    if (xp)
      ddsi_xpack_addmsg (xp, msg, 0);
    else
      ddsi_qxev_msg (pp->e.gv->xevents, msg);
  }
  ddsi_serdata_unref (serdata);
}

ddsrt_nonnull_all
static ddsrt_mtime_t spdp_do_aging_locators (struct spdp_admin *adm, struct ddsi_xpack *xp, ddsrt_mtime_t tnow)
{
  struct ddsi_domaingv * const gv = adm->gv;
  const dds_duration_t t_coalesce = DDS_SECS (1); // aging, so low rate
  const ddsrt_mtime_t t_cutoff = ddsrt_mtime_add_duration (tnow, t_coalesce);
  ddsrt_mtime_t t_sched = DDSRT_MTIME_NEVER;
  ddsrt_mutex_lock (&adm->lock);
  struct spdp_loc_aging *n = ddsrt_avl_find_min (&spdp_loc_td, &adm->aging);
  while (n != NULL)
  {
    struct spdp_loc_aging * const nextn = ddsrt_avl_find_succ (&spdp_loc_td, &adm->aging, n);
    if (n->c.tprune.v <= tnow.v)
    {
      char buf[DDSI_LOCSTRLEN];
      GVLOGDISC("spdp: prune loc %s\n", ddsi_xlocator_to_string (buf, sizeof (buf), &n->c.xloc));
      ddsrt_avl_delete (&spdp_loc_td, &adm->aging, n);
      ddsrt_free (n);
    }
    else
    {
      if (n->tsched.v <= t_cutoff.v)
      {
        ddsrt_avl_iter_t it;
        for (struct spdp_pp *ppn = ddsrt_avl_iter_first (&spdp_pp_td, &adm->pp, &it); ppn; ppn = ddsrt_avl_iter_next (&it))
          resend_spdp (xp, ppn->pp, &(struct resend_spdp_dst){ .kind = RSDK_LOCATOR, .u = { .xloc = &n->c.xloc }});
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
        // FIXME: this base_intv thing needs to be done smarter, or else combined with other places
        const dds_duration_t base_intv = gv->config.spdp_interval.isdefault ? DDS_SECS (30) : gv->config.spdp_interval.value;
        n->tsched = ddsrt_mtime_add_duration (tnow, base_intv);
      }
      // Next time to look at the aging locators again: the first to be scheduled or pruned
      if (n->tsched.v < t_sched.v)
        t_sched = n->tsched;
      if (n->c.tprune.v <= t_sched.v)
        t_sched = n->c.tprune;
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
        resend_spdp (xp, ppn->pp, &(struct resend_spdp_dst){ .kind = RSDK_LOCATOR, .u = { .xloc = &n->c.xloc }});
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
  struct handle_locators_xevent_arg * const arg = varg;
  (void) gv;
  const ddsrt_mtime_t t_sched = spdp_do_aging_locators (arg->adm, xp, tnow);
  ddsi_resched_xevent_if_earlier (xev, t_sched);
}

void ddsi_spdp_handle_live_locators_xevent_cb (struct ddsi_domaingv *gv, struct ddsi_xevent *xev, struct ddsi_xpack *xp, void *varg, ddsrt_mtime_t tnow)
{
  struct handle_locators_xevent_arg * const arg = varg;
  (void) gv;
  const ddsrt_mtime_t t_sched = spdp_do_live_locators (arg->adm, xp, tnow);
  ddsi_resched_xevent_if_earlier (xev, t_sched);
}

bool ddsi_spdp_force_republish (struct spdp_admin *adm, const struct ddsi_participant *pp, const struct ddsi_proxy_reader *prd)
{
  // Used for: initial publication, QoS update, dispose+unregister, faster rediscovery in
  // oneliner in implementation of "hearing!"
  //
  // It seems there's no need to update the scheduled next publication for any of these cases.
  //
  // This gets called from various threads and not all of them have a message packer at hand.
  // Passing a null pointer for "xpack" hands it off to the tev thread for publication.
  if (prd == NULL)
  {
#ifndef NDEBUG
    ddsrt_mutex_lock ((ddsrt_mutex_t *) &pp->e.lock);
    assert (pp->spdp_serdata);
    ddsrt_mutex_unlock ((ddsrt_mutex_t *) &pp->e.lock);
#endif

    ddsrt_mutex_lock (&adm->lock);
    ddsrt_avl_iter_t loc_it;
    for (struct spdp_loc_live *n = ddsrt_avl_iter_first (&spdp_loc_td, &adm->live, &loc_it); n; n = ddsrt_avl_iter_next (&loc_it))
      resend_spdp (NULL, pp, &(struct resend_spdp_dst){ .kind = RSDK_LOCATOR, .u = { .xloc = &n->c.xloc }});
    for (struct spdp_loc_aging *n = ddsrt_avl_iter_first (&spdp_loc_td, &adm->aging, &loc_it); n; n = ddsrt_avl_iter_next (&loc_it))
      resend_spdp (NULL, pp, &(struct resend_spdp_dst){ .kind = RSDK_LOCATOR, .u = { .xloc = &n->c.xloc }});
    ddsrt_mutex_unlock (&adm->lock);
    return true;
  }
  else
  {
    ddsrt_mutex_lock ((ddsrt_mutex_t *) &pp->e.lock);
    if (pp->spdp_serdata != NULL)
    {
      ddsrt_mutex_unlock ((ddsrt_mutex_t *) &pp->e.lock);
      resend_spdp (NULL, pp, &(struct resend_spdp_dst){ .kind = RSDK_PROXY_READER, .u = { .prd = prd }});
      return true;
    }
    else
    {
      ddsrt_mutex_unlock ((ddsrt_mutex_t *) &pp->e.lock);
      return false;
    }
  }
}
