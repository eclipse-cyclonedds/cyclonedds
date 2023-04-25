// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <string.h>
#include <stddef.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_builtin_topic_if.h"
#include "ddsi__entity.h"
#include "ddsi__endpoint_match.h"
#include "ddsi__participant.h"
#include "ddsi__entity_index.h"
#include "ddsi__security_omg.h"
#include "ddsi__lease.h"
#include "ddsi__addrset.h"
#include "ddsi__endpoint.h"
#include "ddsi__gc.h"
#include "ddsi__plist.h"
#include "ddsi__proxy_endpoint.h"
#include "ddsi__proxy_participant.h"
#include "ddsi__protocol.h"
#include "ddsi__topic.h"
#include "ddsi__tran.h"
#include "ddsi__vendor.h"
#include "ddsi__addrset.h"

typedef struct proxy_purge_data {
  struct ddsi_proxy_participant *proxypp;
  const ddsi_xlocator_t *loc;
  ddsrt_wctime_t timestamp;
} *proxy_purge_data_t;

#ifdef DDS_HAS_TOPIC_DISCOVERY
const ddsrt_avl_treedef_t ddsi_proxypp_proxytp_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct ddsi_proxy_topic, avlnode), offsetof (struct ddsi_proxy_topic, entityid), ddsi_compare_entityid, 0);
#endif

static void proxy_participant_replace_minl (struct ddsi_proxy_participant *proxypp, bool manbypp, struct ddsi_lease *lnew)
{
  /* By loading/storing the pointer atomically, we ensure we always
     read a valid (or once valid) lease. By delaying freeing the lease
     through the garbage collector, we ensure whatever lease update
     occurs in parallel completes before the memory is released. */
  struct ddsi_gcreq *gcreq = ddsi_gcreq_new (proxypp->e.gv->gcreq_queue, ddsi_gc_participant_lease);
  struct ddsi_lease *lease_old = ddsrt_atomic_ldvoidp (manbypp ? &proxypp->minl_man : &proxypp->minl_auto);
  ddsi_lease_unregister (lease_old); /* ensures lease will not expire while it is replaced */
  gcreq->arg = lease_old;
  ddsi_gcreq_enqueue (gcreq);
  ddsrt_atomic_stvoidp (manbypp ? &proxypp->minl_man : &proxypp->minl_auto, lnew);
}

void ddsi_proxy_participant_reassign_lease (struct ddsi_proxy_participant *proxypp, struct ddsi_lease *newlease)
{
  ddsrt_mutex_lock (&proxypp->e.lock);
  if (proxypp->owns_lease)
  {
    struct ddsi_lease *minl = ddsrt_fibheap_min (&ddsi_lease_fhdef_pp, &proxypp->leaseheap_auto);
    ddsrt_fibheap_delete (&ddsi_lease_fhdef_pp, &proxypp->leaseheap_auto, proxypp->lease);
    if (minl == proxypp->lease)
    {
      if ((minl = ddsrt_fibheap_min (&ddsi_lease_fhdef_pp, &proxypp->leaseheap_auto)) != NULL)
      {
        dds_duration_t trem = minl->tdur - proxypp->lease->tdur;
        assert (trem >= 0);
        ddsrt_etime_t texp = ddsrt_etime_add_duration (ddsrt_time_elapsed(), trem);
        struct ddsi_lease *lnew = ddsi_lease_new (texp, minl->tdur, minl->entity);
        proxy_participant_replace_minl (proxypp, false, lnew);
        ddsi_lease_register (lnew);
      }
      else
      {
        proxy_participant_replace_minl (proxypp, false, NULL);
      }
    }

    /* Lease renewal is done by the receive thread without locking the
      proxy participant (and I'd like to keep it that way), but that
      means we must guarantee that the lease pointer remains valid once
      loaded.

      By loading/storing the pointer atomically, we ensure we always
      read a valid (or once valid) value, by delaying the freeing
      through the garbage collector, we ensure whatever lease update
      occurs in parallel completes before the memory is released.

      The ddsi_lease_unregister call ensures the lease will never expire
      while we are messing with it. */
    struct ddsi_gcreq *gcreq = ddsi_gcreq_new (proxypp->e.gv->gcreq_queue, ddsi_gc_participant_lease);
    ddsi_lease_unregister (proxypp->lease);
    gcreq->arg = proxypp->lease;
    ddsi_gcreq_enqueue (gcreq);
    proxypp->owns_lease = 0;
  }
  proxypp->lease = newlease;

  ddsrt_mutex_unlock (&proxypp->e.lock);
}

static void create_proxy_builtin_endpoint_impl (struct ddsi_domaingv *gv, ddsrt_wctime_t timestamp, const struct ddsi_guid *ppguid,
    struct ddsi_proxy_participant *proxypp, const struct ddsi_guid *ep_guid, ddsi_plist_t *plist, const char *topic_name)
{
  if ((plist->qos.present & DDSI_QP_TOPIC_NAME) == DDSI_QP_TOPIC_NAME)
    ddsi_plist_fini_mask (plist, 0, DDSI_QP_TOPIC_NAME);
  plist->qos.topic_name = dds_string_dup (topic_name);
  plist->qos.present |= DDSI_QP_TOPIC_NAME;
  if (ddsi_is_writer_entityid (ep_guid->entityid))
    ddsi_new_proxy_writer (gv, ppguid, ep_guid, proxypp->as_meta, plist, gv->builtins_dqueue, gv->xevents, timestamp, 0);
  else
  {
#ifdef DDS_HAS_SSM
    const int ssm = ddsi_addrset_contains_ssm (gv, proxypp->as_meta);
    ddsi_new_proxy_reader (gv, ppguid, ep_guid, proxypp->as_meta, plist, timestamp, 0, ssm);
#else
    ddsi_new_proxy_reader (gv, ppguid, ep_guid, proxypp->as_meta, plist, timestamp, 0);
#endif
  }
}

static void create_proxy_builtin_endpoints (struct ddsi_domaingv *gv, const struct ddsi_bestab *bestab, int nbes, const struct ddsi_guid *ppguid, struct ddsi_proxy_participant *proxypp, ddsrt_wctime_t timestamp, dds_qos_t *xqos_wr, dds_qos_t *xqos_rd)
{
  ddsi_plist_t plist_rd, plist_wr;
  /* Note: no entity name or group GUID supplied, but that shouldn't
   * matter, as these are internal to DDSI and don't use group coherency */
  ddsi_plist_init_empty (&plist_wr);
  ddsi_plist_init_empty (&plist_rd);
  ddsi_xqos_copy (&plist_wr.qos, xqos_wr);
  ddsi_xqos_copy (&plist_rd.qos, xqos_rd);
  for (int i = 0; i < nbes; i++)
  {
    const struct ddsi_bestab *te = &bestab[i];
    if (proxypp->bes & te->besflag)
    {
      ddsi_guid_t ep_guid = { .prefix = proxypp->e.guid.prefix, .entityid.u = te->entityid };
      assert (ddsi_is_builtin_entityid (ep_guid.entityid, proxypp->vendor));
      create_proxy_builtin_endpoint_impl (gv, timestamp, ppguid, proxypp, &ep_guid, ddsi_is_writer_entityid (ep_guid.entityid) ? &plist_wr : &plist_rd, te->topic_name);
    }
  }
  ddsi_plist_fini (&plist_wr);
  ddsi_plist_fini (&plist_rd);
}

static void add_proxy_builtin_endpoints (struct ddsi_domaingv *gv, const struct ddsi_guid *ppguid, struct ddsi_proxy_participant *proxypp, ddsrt_wctime_t timestamp)
{
  /* Add proxy endpoints based on the advertised (& possibly augmented
     ...) built-in endpoint set. */
#define TE(ap_, a_, bp_, b_, c_) { DDSI_##ap_##BUILTIN_ENDPOINT_##a_, DDSI_ENTITYID_##bp_##_BUILTIN_##b_, DDS_BUILTIN_TOPIC_##c_##_NAME }
#define LTE(a_, bp_, b_, c_) { DDSI_##BUILTIN_ENDPOINT_##a_, DDSI_ENTITYID_##bp_##_BUILTIN_##b_, DDS_BUILTIN_TOPIC_##c_##_NAME }

  /* 'Default' proxy endpoints. */
  static const struct ddsi_bestab bestab_default[] = {
#if 0
    /* SPDP gets special treatment => no need for proxy
       writers/readers */
    TE (DISC_, PARTICIPANT_ANNOUNCER, SPDP, PARTICIPANT_WRITER, PARTICIPANT),
#endif
    TE (DISC_, PARTICIPANT_DETECTOR, SPDP, PARTICIPANT_READER, PARTICIPANT),
    TE (DISC_, PUBLICATION_ANNOUNCER, SEDP, PUBLICATIONS_WRITER, PUBLICATION),
    TE (DISC_, PUBLICATION_DETECTOR, SEDP, PUBLICATIONS_READER, PUBLICATION),
    TE (DISC_, SUBSCRIPTION_ANNOUNCER, SEDP, SUBSCRIPTIONS_WRITER, SUBSCRIPTION),
    TE (DISC_, SUBSCRIPTION_DETECTOR, SEDP, SUBSCRIPTIONS_READER, SUBSCRIPTION),
    LTE (PARTICIPANT_MESSAGE_DATA_WRITER, P2P, PARTICIPANT_MESSAGE_WRITER, PARTICIPANT_MESSAGE),
    LTE (PARTICIPANT_MESSAGE_DATA_READER, P2P, PARTICIPANT_MESSAGE_READER, PARTICIPANT_MESSAGE),
#ifdef DDS_HAS_TOPIC_DISCOVERY
    TE (DISC_, TOPICS_ANNOUNCER, SEDP, TOPIC_WRITER, TOPIC),
    TE (DISC_, TOPICS_DETECTOR, SEDP, TOPIC_READER, TOPIC),
#endif
  };
  create_proxy_builtin_endpoints(gv, bestab_default,
    (int)(sizeof (bestab_default) / sizeof (*bestab_default)),
    ppguid, proxypp, timestamp, &gv->builtin_endpoint_xqos_wr, &gv->builtin_endpoint_xqos_rd);

#ifdef DDS_HAS_TYPE_DISCOVERY
  /* Volatile proxy endpoints. */
  static const struct ddsi_bestab bestab_volatile[] = {
    LTE (TL_SVC_REQUEST_DATA_WRITER, TL_SVC, REQUEST_WRITER, TYPELOOKUP_REQUEST),
    LTE (TL_SVC_REQUEST_DATA_READER, TL_SVC, REQUEST_READER, TYPELOOKUP_REQUEST),
    LTE (TL_SVC_REPLY_DATA_WRITER, TL_SVC, REPLY_WRITER, TYPELOOKUP_REPLY),
    LTE (TL_SVC_REPLY_DATA_READER, TL_SVC, REPLY_READER, TYPELOOKUP_REPLY),
  };
  create_proxy_builtin_endpoints(gv, bestab_volatile,
    (int)(sizeof (bestab_volatile) / sizeof (*bestab_volatile)),
    ppguid, proxypp, timestamp, &gv->builtin_volatile_xqos_wr, &gv->builtin_volatile_xqos_rd);
#endif

#ifdef DDS_HAS_SECURITY
  /* Security 'default' proxy endpoints. */
  static const struct ddsi_bestab bestab_security[] = {
    LTE (PUBLICATION_MESSAGE_SECURE_ANNOUNCER, SEDP, PUBLICATIONS_SECURE_WRITER, PUBLICATION_SECURE),
    LTE (PUBLICATION_MESSAGE_SECURE_DETECTOR, SEDP, PUBLICATIONS_SECURE_READER, PUBLICATION_SECURE),
    LTE (SUBSCRIPTION_MESSAGE_SECURE_ANNOUNCER, SEDP, SUBSCRIPTIONS_SECURE_WRITER, SUBSCRIPTION_SECURE),
    LTE (SUBSCRIPTION_MESSAGE_SECURE_DETECTOR, SEDP, SUBSCRIPTIONS_SECURE_READER, SUBSCRIPTION_SECURE),
    LTE (PARTICIPANT_MESSAGE_SECURE_ANNOUNCER, P2P, PARTICIPANT_MESSAGE_SECURE_WRITER, PARTICIPANT_MESSAGE_SECURE),
    LTE (PARTICIPANT_MESSAGE_SECURE_DETECTOR, P2P, PARTICIPANT_MESSAGE_SECURE_READER, PARTICIPANT_MESSAGE_SECURE),
    TE (DISC_, PARTICIPANT_SECURE_ANNOUNCER, SPDP_RELIABLE, PARTICIPANT_SECURE_WRITER, PARTICIPANT_SECURE),
    TE (DISC_, PARTICIPANT_SECURE_DETECTOR, SPDP_RELIABLE, PARTICIPANT_SECURE_READER, PARTICIPANT_SECURE)
  };
  create_proxy_builtin_endpoints(gv, bestab_security,
    (int)(sizeof (bestab_security) / sizeof (*bestab_security)),
    ppguid, proxypp, timestamp, &gv->builtin_endpoint_xqos_wr, &gv->builtin_endpoint_xqos_rd);

  /* Security 'volatile' proxy endpoints. */
  static const struct ddsi_bestab bestab_security_volatile[] = {
    LTE (PARTICIPANT_VOLATILE_SECURE_ANNOUNCER, P2P, PARTICIPANT_VOLATILE_SECURE_WRITER, PARTICIPANT_VOLATILE_MESSAGE_SECURE),
    LTE (PARTICIPANT_VOLATILE_SECURE_DETECTOR, P2P, PARTICIPANT_VOLATILE_SECURE_READER, PARTICIPANT_VOLATILE_MESSAGE_SECURE)
  };
  create_proxy_builtin_endpoints(gv, bestab_security_volatile,
    (int)(sizeof (bestab_security_volatile) / sizeof (*bestab_security_volatile)),
    ppguid, proxypp, timestamp, &gv->builtin_secure_volatile_xqos_wr, &gv->builtin_secure_volatile_xqos_rd);

  /* Security 'stateless' proxy endpoints. */
  static const struct ddsi_bestab bestab_security_stateless[] = {
    LTE (PARTICIPANT_STATELESS_MESSAGE_ANNOUNCER, P2P, PARTICIPANT_STATELESS_MESSAGE_WRITER, PARTICIPANT_STATELESS_MESSAGE),
    LTE (PARTICIPANT_STATELESS_MESSAGE_DETECTOR, P2P, PARTICIPANT_STATELESS_MESSAGE_READER, PARTICIPANT_STATELESS_MESSAGE)
  };
  create_proxy_builtin_endpoints(gv, bestab_security_stateless,
    (int)(sizeof (bestab_security_stateless) / sizeof (*bestab_security_stateless)),
    ppguid, proxypp, timestamp, &gv->builtin_stateless_xqos_wr, &gv->builtin_stateless_xqos_rd);
#endif

#undef TE
#undef LTE
}

void ddsi_proxy_participant_add_pwr_lease_locked (struct ddsi_proxy_participant * proxypp, const struct ddsi_proxy_writer * pwr)
{
  struct ddsi_lease *minl_prev;
  struct ddsi_lease *minl_new;
  ddsrt_fibheap_t *lh;
  bool manbypp;

  assert (pwr->lease != NULL);
  manbypp = (pwr->c.xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_PARTICIPANT);
  lh = manbypp ? &proxypp->leaseheap_man : &proxypp->leaseheap_auto;
  minl_prev = ddsrt_fibheap_min (&ddsi_lease_fhdef_pp, lh);
  ddsrt_fibheap_insert (&ddsi_lease_fhdef_pp, lh, pwr->lease);
  minl_new = ddsrt_fibheap_min (&ddsi_lease_fhdef_pp, lh);
  /* ensure proxypp->minl_man/minl_auto is equivalent to min(leaseheap_man/auto) */
  if (proxypp->owns_lease && minl_prev != minl_new)
  {
    ddsrt_etime_t texp = ddsrt_etime_add_duration (ddsrt_time_elapsed (), minl_new->tdur);
    struct ddsi_lease *lnew = ddsi_lease_new (texp, minl_new->tdur, minl_new->entity);
    if (minl_prev == NULL)
    {
      assert (manbypp);
      assert (ddsrt_atomic_ldvoidp (&proxypp->minl_man) == NULL);
      ddsrt_atomic_stvoidp (&proxypp->minl_man, lnew);
    }
    else
    {
      proxy_participant_replace_minl (proxypp, manbypp, lnew);
    }
    ddsi_lease_register (lnew);
  }
}

void ddsi_proxy_participant_remove_pwr_lease_locked (struct ddsi_proxy_participant * proxypp, struct ddsi_proxy_writer * pwr)
{
  struct ddsi_lease *minl_prev;
  struct ddsi_lease *minl_new;
  bool manbypp;
  ddsrt_fibheap_t *lh;

  assert (pwr->lease != NULL);
  manbypp = (pwr->c.xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_PARTICIPANT);
  lh = manbypp ? &proxypp->leaseheap_man : &proxypp->leaseheap_auto;
  minl_prev = ddsrt_fibheap_min (&ddsi_lease_fhdef_pp, lh);
  ddsrt_fibheap_delete (&ddsi_lease_fhdef_pp, lh, pwr->lease);
  minl_new = ddsrt_fibheap_min (&ddsi_lease_fhdef_pp, lh);
  /* ensure proxypp->minl_man/minl_auto is equivalent to min(leaseheap_man/auto) */
  if (proxypp->owns_lease && minl_prev != minl_new)
  {
    if (minl_new != NULL)
    {
      dds_duration_t trem = minl_new->tdur - minl_prev->tdur;
      assert (trem >= 0);
      ddsrt_etime_t texp = ddsrt_etime_add_duration (ddsrt_time_elapsed(), trem);
      struct ddsi_lease *lnew = ddsi_lease_new (texp, minl_new->tdur, minl_new->entity);
      proxy_participant_replace_minl (proxypp, manbypp, lnew);
      ddsi_lease_register (lnew);
    }
    else
    {
      proxy_participant_replace_minl (proxypp, manbypp, NULL);
    }
  }
}

static void free_proxy_participant (struct ddsi_proxy_participant *proxypp)
{
  if (proxypp->owns_lease)
  {
    struct ddsi_lease * minl_auto = ddsrt_atomic_ldvoidp (&proxypp->minl_auto);
    ddsrt_fibheap_delete (&ddsi_lease_fhdef_pp, &proxypp->leaseheap_auto, proxypp->lease);
    assert (ddsrt_fibheap_min (&ddsi_lease_fhdef_pp, &proxypp->leaseheap_auto) == NULL);
    assert (ddsrt_fibheap_min (&ddsi_lease_fhdef_pp, &proxypp->leaseheap_man) == NULL);
    assert (ddsrt_atomic_ldvoidp (&proxypp->minl_man) == NULL);
    assert (!ddsi_compare_guid (&minl_auto->entity->guid, &proxypp->e.guid));
    /* if the lease hasn't been registered yet (which is the case when
       new_proxy_participant calls this, it is marked as such and calling
       ddsi_lease_unregister is ok */
    ddsi_lease_unregister (minl_auto);
    ddsi_lease_free (minl_auto);
    ddsi_lease_free (proxypp->lease);
  }
#ifdef DDS_HAS_SECURITY
  ddsi_disconnect_proxy_participant_secure(proxypp);
  ddsi_omg_security_deregister_remote_participant (proxypp);
#endif
  ddsi_unref_addrset (proxypp->as_default);
  ddsi_unref_addrset (proxypp->as_meta);
  ddsi_plist_fini (proxypp->plist);
  ddsrt_free (proxypp->plist);
  ddsi_entity_common_fini (&proxypp->e);
  ddsrt_free (proxypp);
}

bool ddsi_new_proxy_participant (struct ddsi_domaingv *gv, const struct ddsi_guid *ppguid, uint32_t bes, const struct ddsi_guid *privileged_pp_guid, struct ddsi_addrset *as_default, struct ddsi_addrset *as_meta, const ddsi_plist_t *plist, dds_duration_t tlease_dur, ddsi_vendorid_t vendor, unsigned custom_flags, ddsrt_wctime_t timestamp, ddsi_seqno_t seq)
{
  /* No locking => iff all participants use unique guids, and sedp
     runs on a single thread, it can't go wrong. FIXME, maybe? The
     same holds for the other functions for creating entities. */
  struct ddsi_proxy_participant *proxypp;
  const bool is_secure = ((bes & DDSI_DISC_BUILTIN_ENDPOINT_PARTICIPANT_SECURE_ANNOUNCER) != 0);
  assert (!is_secure || (plist->present & PP_IDENTITY_TOKEN));
  assert (is_secure || (bes & ~DDSI_BES_MASK_NON_SECURITY) == 0);
  (void) is_secure;

  assert (ppguid->entityid.u == DDSI_ENTITYID_PARTICIPANT);
  assert (ddsi_entidx_lookup_proxy_participant_guid (gv->entity_index, ppguid) == NULL);
  assert (privileged_pp_guid == NULL || privileged_pp_guid->entityid.u == DDSI_ENTITYID_PARTICIPANT);

  ddsi_prune_deleted_participant_guids (gv->deleted_participants, ddsrt_time_monotonic ());

  proxypp = ddsrt_malloc (sizeof (*proxypp));

  ddsi_entity_common_init (&proxypp->e, gv, ppguid, DDSI_EK_PROXY_PARTICIPANT, timestamp, vendor, false);
  proxypp->refc = 1;
  proxypp->lease_expired = 0;
  proxypp->deleting = 0;
  proxypp->vendor = vendor;
  proxypp->bes = bes;
  proxypp->seq = seq;
  if (privileged_pp_guid) {
    proxypp->privileged_pp_guid = *privileged_pp_guid;
  } else {
    memset (&proxypp->privileged_pp_guid.prefix, 0, sizeof (proxypp->privileged_pp_guid.prefix));
    proxypp->privileged_pp_guid.entityid.u = DDSI_ENTITYID_PARTICIPANT;
  }
  if ((plist->present & PP_ADLINK_PARTICIPANT_VERSION_INFO) &&
      (plist->adlink_participant_version_info.flags & DDSI_ADLINK_FL_DDSI2_PARTICIPANT_FLAG) &&
      (plist->adlink_participant_version_info.flags & DDSI_ADLINK_FL_PARTICIPANT_IS_DDSI2))
    proxypp->is_ddsi2_pp = 1;
  else
    proxypp->is_ddsi2_pp = 0;
  if ((plist->present & PP_ADLINK_PARTICIPANT_VERSION_INFO) &&
      (plist->adlink_participant_version_info.flags & DDSI_ADLINK_FL_MINIMAL_BES_MODE))
    proxypp->minimal_bes_mode = 1;
  else
    proxypp->minimal_bes_mode = 0;
  proxypp->implicitly_created = ((custom_flags & DDSI_CF_IMPLICITLY_CREATED_PROXYPP) != 0);
  proxypp->proxypp_have_spdp = ((custom_flags & DDSI_CF_PROXYPP_NO_SPDP) == 0);
  if (plist->present & PP_CYCLONE_RECEIVE_BUFFER_SIZE)
    proxypp->receive_buffer_size = plist->cyclone_receive_buffer_size;
  else /* default to what we use */
    proxypp->receive_buffer_size = ddsi_receive_buffer_size (gv->m_factory);
  if (proxypp->receive_buffer_size < 131072)
  {
    /* if we don't know anything, or if it is implausibly tiny, use 128kB */
    proxypp->receive_buffer_size = 131072;
  }
  if (plist->present & PP_CYCLONE_REDUNDANT_NETWORKING)
    proxypp->redundant_networking = (plist->cyclone_redundant_networking != 0);
  else
    proxypp->redundant_networking = 0;

  {
    struct ddsi_proxy_participant *privpp;
    privpp = ddsi_entidx_lookup_proxy_participant_guid (gv->entity_index, &proxypp->privileged_pp_guid);

    ddsrt_fibheap_init (&ddsi_lease_fhdef_pp, &proxypp->leaseheap_auto);
    ddsrt_fibheap_init (&ddsi_lease_fhdef_pp, &proxypp->leaseheap_man);
    ddsrt_atomic_stvoidp (&proxypp->minl_man, NULL);

    if (privpp != NULL && privpp->is_ddsi2_pp)
    {
      proxypp->lease = privpp->lease;
      proxypp->owns_lease = 0;
      ddsrt_atomic_stvoidp (&proxypp->minl_auto, NULL);
    }
    else
    {
      /* Lease duration is meaningless when the lease never expires, but when proxy participants are
        created implicitly because of endpoint discovery from a cloud service, we do want the lease to expire
        eventually when the cloud discovery service disappears and never reappears. The normal data path renews
        the lease, so if the lease expiry is changed after the DS disappears but data continues to flow (even if
        it is only a single sample) the proxy participant would immediately go back to a non-expiring lease with
        no further triggers for deleting it. Instead, we take tlease_dur == NEVER as a special value meaning a
        lease that doesn't expire now and that has a "reasonable" lease duration. That way the lease renewal in
        the data path is fine, and we only need to do something special in SEDP handling. */
      ddsrt_etime_t texp = ddsrt_etime_add_duration (ddsrt_time_elapsed(), tlease_dur);
      dds_duration_t dur = (tlease_dur == DDS_INFINITY) ? gv->config.lease_duration : tlease_dur;
      proxypp->lease = ddsi_lease_new (texp, dur, &proxypp->e);
      proxypp->owns_lease = 1;

      /* Add the proxypp lease to heap so that monitoring liveliness will include this lease
         and uses the shortest duration for proxypp and all its pwr's (with automatic liveliness) */
      ddsrt_fibheap_insert (&ddsi_lease_fhdef_pp, &proxypp->leaseheap_auto, proxypp->lease);

      /* Set the shortest lease for auto liveliness: clone proxypp's lease and store the clone in
         proxypp->minl_auto. As there are no pwr's at this point, the proxy pp's lease is the
         shortest lease. When a pwr with a shorter is added, the lease in minl_auto is replaced
         by the lease from the proxy writer in ddsi_proxy_participant_add_pwr_lease_locked. This old shortest
         lease is freed, so that's why we need a clone and not the proxypp's lease in the heap.  */
      ddsrt_atomic_stvoidp (&proxypp->minl_auto, (void *) ddsi_lease_clone (proxypp->lease));
    }
  }

  proxypp->as_default = as_default;
  proxypp->as_meta = as_meta;
  proxypp->endpoints = NULL;

#ifdef DDS_HAS_TOPIC_DISCOVERY
  ddsrt_avl_init (&ddsi_proxypp_proxytp_treedef, &proxypp->topics);
#endif
  proxypp->plist = ddsi_plist_dup (plist);
  ddsi_xqos_mergein_missing (&proxypp->plist->qos, &ddsi_default_qos_participant, ~(uint64_t)0);

#ifdef DDS_HAS_SECURITY
  proxypp->sec_attr = NULL;
  ddsi_set_proxy_participant_security_info (proxypp, plist);
  if (is_secure)
  {
    ddsi_omg_security_init_remote_participant (proxypp);
    /* check if the proxy participant has a match with a local participant */
    if (!ddsi_proxy_participant_has_pp_match (gv, proxypp))
    {
      GVWARNING ("Remote secure participant "PGUIDFMT" not allowed\n", PGUID (*ppguid));
      free_proxy_participant (proxypp);
      return false;
    }
  }
#endif

  /* Proxy participant must be in the hash tables for new_proxy_{writer,reader} to work */
  ddsi_entidx_insert_proxy_participant_guid (gv->entity_index, proxypp);
  add_proxy_builtin_endpoints(gv, ppguid, proxypp, timestamp);

  /* write DCPSParticipant topic before the lease can expire */
  ddsi_builtintopic_write_endpoint (gv->builtin_topic_interface, &proxypp->e, timestamp, true);

  /* Register lease for auto liveliness, but be careful not to accidentally re-register
     DDSI's lease, as we may have become dependent on DDSI any time after
     ddsi_entidx_insert_proxy_participant_guid even if privileged_pp_guid was NULL originally */
  ddsrt_mutex_lock (&proxypp->e.lock);
  if (proxypp->owns_lease)
    ddsi_lease_register (ddsrt_atomic_ldvoidp (&proxypp->minl_auto));
  ddsrt_mutex_unlock (&proxypp->e.lock);

#ifdef DDS_HAS_SECURITY
  if (is_secure)
  {
    ddsi_proxy_participant_create_handshakes (gv, proxypp);
  }
#endif
  return true;
}

int ddsi_update_proxy_participant_plist_locked (struct ddsi_proxy_participant *proxypp, ddsi_seqno_t seq, const struct ddsi_plist *datap, ddsrt_wctime_t timestamp)
{
  if (seq > proxypp->seq)
  {
    proxypp->seq = seq;

    const uint64_t pmask = 0;
    const uint64_t qmask = DDSI_QP_USER_DATA;
    ddsi_plist_t *new_plist = ddsrt_malloc (sizeof (*new_plist));
    ddsi_plist_init_empty (new_plist);
    ddsi_plist_mergein_missing (new_plist, datap, pmask, qmask);
    ddsi_xqos_mergein_missing(&new_plist->qos, &ddsi_default_qos_participant,~(uint64_t)0);
    (void) ddsi_update_qos_locked (&proxypp->e, &proxypp->plist->qos, &new_plist->qos, timestamp);
    ddsi_plist_fini (new_plist);
    ddsrt_free (new_plist);
    proxypp->proxypp_have_spdp = 1;
  }
  return 0;
}

int ddsi_ref_proxy_participant (struct ddsi_proxy_participant *proxypp, struct ddsi_proxy_endpoint_common *c)
{
  ddsrt_mutex_lock (&proxypp->e.lock);
  if (proxypp->deleting)
  {
    ddsrt_mutex_unlock (&proxypp->e.lock);
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  }

  proxypp->refc++;
  if (c != NULL)
  {
    c->proxypp = proxypp;
    c->next_ep = proxypp->endpoints;
    c->prev_ep = NULL;
    if (c->next_ep)
      c->next_ep->prev_ep = c;
    proxypp->endpoints = c;
  }
  ddsrt_mutex_unlock (&proxypp->e.lock);

  return DDS_RETCODE_OK;
}

void ddsi_unref_proxy_participant (struct ddsi_proxy_participant *proxypp, struct ddsi_proxy_endpoint_common *c)
{
  uint32_t refc;
  const ddsrt_wctime_t tnow = ddsrt_time_wallclock();

  ddsrt_mutex_lock (&proxypp->e.lock);
  refc = --proxypp->refc;

  if (c != NULL)
  {
    if (c->next_ep)
      c->next_ep->prev_ep = c->prev_ep;
    if (c->prev_ep)
      c->prev_ep->next_ep = c->next_ep;
    else
      proxypp->endpoints = c->next_ep;
  }

  if (refc == 0)
  {
    struct ddsi_domaingv * const gv = proxypp->e.gv;
    const ddsi_guid_t pp_guid = proxypp->e.guid;
    assert (proxypp->endpoints == NULL);
#ifdef DDS_HAS_TOPIC_DISCOVERY
    /* Last unref is called from gc_delete_proxy_participant, which is added to the gc queue after all proxy
       topic gc tasks. So we can safely assert that the proxy topic list is empty at this point */
    assert (ddsrt_avl_is_empty (&proxypp->topics));
    ddsrt_avl_free (&ddsi_proxypp_proxytp_treedef, &proxypp->topics, 0);
#endif
    ddsrt_mutex_unlock (&proxypp->e.lock);
    ELOGDISC (proxypp, "ddsi_unref_proxy_participant("PGUIDFMT"): refc=0, freeing\n", PGUID (proxypp->e.guid));
    free_proxy_participant (proxypp);
    ddsi_remove_deleted_participant_guid (gv->deleted_participants, &pp_guid, DDSI_DELETED_PPGUID_LOCAL | DDSI_DELETED_PPGUID_REMOTE);
  }
  else if (
    proxypp->endpoints == NULL
#ifdef DDS_HAS_TOPIC_DISCOVERY
    && ddsrt_avl_is_empty (&proxypp->topics)
#endif
    && proxypp->implicitly_created)
  {
    assert (refc == 1);
    ddsrt_mutex_unlock (&proxypp->e.lock);
    ELOGDISC (proxypp, "ddsi_unref_proxy_participant("PGUIDFMT"): refc=%u, no endpoints, implicitly created, deleting\n",
              PGUID (proxypp->e.guid), (unsigned) refc);
    ddsi_delete_proxy_participant_by_guid(proxypp->e.gv, &proxypp->e.guid, tnow, 1);
    /* Deletion is still (and has to be) asynchronous. A parallel endpoint creation may or may not
       succeed, and if it succeeds it will be deleted along with the proxy participant. So "your
       mileage may vary". Also, the proxy participant may be blacklisted for a little ... */
  }
  else
  {
    ddsrt_mutex_unlock (&proxypp->e.lock);
    ELOGDISC (proxypp, "ddsi_unref_proxy_participant("PGUIDFMT"): refc=%u\n", PGUID (proxypp->e.guid), (unsigned) refc);
  }
}

static void gc_delete_proxy_participant (struct ddsi_gcreq *gcreq)
{
  struct ddsi_proxy_participant *proxypp = gcreq->arg;
  ELOGDISC (proxypp, "gc_delete_proxy_participant(%p, "PGUIDFMT")\n", (void *) gcreq, PGUID (proxypp->e.guid));
  ddsi_gcreq_free (gcreq);
  ddsi_unref_proxy_participant (proxypp, NULL);
}

static int gcreq_proxy_participant (struct ddsi_proxy_participant *proxypp)
{
  struct ddsi_gcreq *gcreq = ddsi_gcreq_new (proxypp->e.gv->gcreq_queue, gc_delete_proxy_participant);
  gcreq->arg = proxypp;
  ddsi_gcreq_enqueue (gcreq);
  return 0;
}

static void delete_or_detach_dependent_pp (struct ddsi_proxy_participant *p, struct ddsi_proxy_participant *proxypp, ddsrt_wctime_t timestamp, int isimplicit)
{
  ddsrt_mutex_lock (&p->e.lock);
  if (memcmp (&p->privileged_pp_guid, &proxypp->e.guid, sizeof (proxypp->e.guid)) != 0)
  {
    /* p not dependent on proxypp */
    ddsrt_mutex_unlock (&p->e.lock);
    return;
  }
  else if (!(ddsi_vendor_is_cloud (p->vendor) && p->implicitly_created))
  {
    /* DDSI minimal participant mode -- but really, anything not discovered via Cloud gets deleted */
    ddsrt_mutex_unlock (&p->e.lock);
    (void) ddsi_delete_proxy_participant_by_guid (p->e.gv, &p->e.guid, timestamp, isimplicit);
  }
  else
  {
    ddsrt_etime_t texp = ddsrt_etime_add_duration (ddsrt_time_elapsed(), p->e.gv->config.ds_grace_period);
    /* Clear dependency (but don't touch entity id, which must be 0x1c1) and set the lease ticking */
    ELOGDISC (p, PGUIDFMT" detach-from-DS "PGUIDFMT"\n", PGUID(p->e.guid), PGUID(proxypp->e.guid));
    memset (&p->privileged_pp_guid.prefix, 0, sizeof (p->privileged_pp_guid.prefix));
    ddsi_lease_set_expiry (p->lease, texp);
    /* FIXME: replace in p->leaseheap_auto and get new minl_auto */
    ddsrt_mutex_unlock (&p->e.lock);
  }
}

static void delete_ppt (struct ddsi_proxy_participant *proxypp, ddsrt_wctime_t timestamp, int isimplicit)
{
  ddsi_entityid_t *child_entities;
  uint32_t n_child_entities = 0;

  /* if any proxy participants depend on this participant, delete them */
  ELOGDISC (proxypp, "delete_ppt("PGUIDFMT") - deleting dependent proxy participants\n", PGUID (proxypp->e.guid));
  {
    struct ddsi_entity_enum_proxy_participant est;
    struct ddsi_proxy_participant *p;
    ddsi_entidx_enum_proxy_participant_init (&est, proxypp->e.gv->entity_index);
    while ((p = ddsi_entidx_enum_proxy_participant_next (&est)) != NULL)
      delete_or_detach_dependent_pp(p, proxypp, timestamp, isimplicit);
    ddsi_entidx_enum_proxy_participant_fini (&est);
  }

  ddsrt_mutex_lock (&proxypp->e.lock);
  proxypp->deleting = 1;
  if (isimplicit)
    proxypp->lease_expired = 1;

#ifdef DDS_HAS_TOPIC_DISCOVERY
  ddsrt_avl_iter_t it;
  for (struct ddsi_proxy_topic *proxytp = ddsrt_avl_iter_first (&ddsi_proxypp_proxytp_treedef, &proxypp->topics, &it); proxytp != NULL; proxytp = ddsrt_avl_iter_next (&it))
    if (!proxytp->deleted)
      (void) ddsi_delete_proxy_topic_locked (proxypp, proxytp, timestamp);
#endif

  /* Get snapshot of endpoints and topics so that we can release proxypp->e.lock
     Pwrs/prds may be deleted during the iteration over the entities,
     but resolving the guid will fail for these entities and the
     call to delete_proxy_writer/reader returns. */
  {
    child_entities = ddsrt_malloc (proxypp->refc * sizeof(ddsi_entityid_t));
    struct ddsi_proxy_endpoint_common *cep = proxypp->endpoints;
    while (cep)
    {
      const struct ddsi_entity_common *entc = ddsi_entity_common_from_proxy_endpoint_common (cep);
      child_entities[n_child_entities++] = entc->guid.entityid;
      cep = cep->next_ep;
    }
  }
  ddsrt_mutex_unlock (&proxypp->e.lock);

  ELOGDISC (proxypp, "delete_ppt("PGUIDFMT") - deleting endpoints\n", PGUID (proxypp->e.guid));
  ddsi_guid_t ep_guid = { .prefix = proxypp->e.guid.prefix, .entityid = { 0 } };
  for (uint32_t n = 0; n < n_child_entities; n++)
  {
    ep_guid.entityid = child_entities[n];
    if (ddsi_is_writer_entityid (ep_guid.entityid))
      ddsi_delete_proxy_writer (proxypp->e.gv, &ep_guid, timestamp, isimplicit);
    else if (ddsi_is_reader_entityid (ep_guid.entityid))
      ddsi_delete_proxy_reader (proxypp->e.gv, &ep_guid, timestamp, isimplicit);
  }
  ddsrt_free (child_entities);

  gcreq_proxy_participant (proxypp);
}

static void purge_helper (const ddsi_xlocator_t *n, void * varg)
{
  proxy_purge_data_t data = (proxy_purge_data_t) varg;
  if (ddsi_compare_xlocators (n, data->loc) == 0)
    ddsi_delete_proxy_participant_by_guid (data->proxypp->e.gv, &data->proxypp->e.guid, data->timestamp, 1);
}

void ddsi_purge_proxy_participants (struct ddsi_domaingv *gv, const ddsi_xlocator_t *loc, bool delete_from_as_disc)
{
  /* FIXME: check whether addr:port can't be reused for a new connection by the time we get here. */
  /* NOTE: This function exists for the sole purpose of cleaning up after closing a TCP connection in ddsi_tcp_close_conn and the state of the calling thread could be anything at this point. Because of that we do the unspeakable and toggle the thread state conditionally. We can't afford to have it in "asleep", as that causes a race with the garbage collector. */
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  struct ddsi_entity_enum_proxy_participant est;
  struct proxy_purge_data data;

  ddsi_thread_state_awake (thrst, gv);
  data.loc = loc;
  data.timestamp = ddsrt_time_wallclock();
  ddsi_entidx_enum_proxy_participant_init (&est, gv->entity_index);
  while ((data.proxypp = ddsi_entidx_enum_proxy_participant_next (&est)) != NULL)
    ddsi_addrset_forall (data.proxypp->as_meta, purge_helper, &data);
  ddsi_entidx_enum_proxy_participant_fini (&est);

  /* Shouldn't try to keep pinging clients once they're gone */
  if (delete_from_as_disc)
    ddsi_remove_from_addrset (gv, gv->as_disc, loc);

  ddsi_thread_state_asleep (thrst);
}

int ddsi_delete_proxy_participant_by_guid (struct ddsi_domaingv *gv, const struct ddsi_guid *guid, ddsrt_wctime_t timestamp, int isimplicit)
{
  struct ddsi_proxy_participant *ppt;

  GVLOGDISC ("ddsi_delete_proxy_participant_by_guid("PGUIDFMT") ", PGUID (*guid));
  ddsrt_mutex_lock (&gv->lock);
  ppt = ddsi_entidx_lookup_proxy_participant_guid (gv->entity_index, guid);
  if (ppt == NULL)
  {
    ddsrt_mutex_unlock (&gv->lock);
    GVLOGDISC ("- unknown\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  GVLOGDISC ("- deleting\n");
  ddsi_builtintopic_write_endpoint (gv->builtin_topic_interface, &ppt->e, timestamp, false);
  ddsi_remember_deleted_participant_guid (gv->deleted_participants, &ppt->e.guid);
  ddsi_entidx_remove_proxy_participant_guid (gv->entity_index, ppt);
  ddsrt_mutex_unlock (&gv->lock);
  delete_ppt (ppt, timestamp, isimplicit);

  return 0;
}
