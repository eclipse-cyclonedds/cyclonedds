/*
 * Copyright(c) 2006 to 2022 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <assert.h>
#include <string.h>
#include <stddef.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_entity_match.h"
#include "dds/ddsi/ddsi_proxy_participant.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/ddsi/ddsi_proxy_endpoint.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_builtin_topic_if.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsi/q_addrset.h"
#include "dds/ddsi/q_whc.h"
#include "dds/ddsi/q_xevent.h"
#include "dds/ddsi/q_radmin.h"

const ddsrt_avl_treedef_t ddsi_pwr_readers_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct ddsi_pwr_rd_match, avlnode), offsetof (struct ddsi_pwr_rd_match, rd_guid), ddsi_compare_guid, 0);
const ddsrt_avl_treedef_t ddsi_prd_writers_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct ddsi_prd_wr_match, avlnode), offsetof (struct ddsi_prd_wr_match, wr_guid), ddsi_compare_guid, 0);

#ifdef DDS_HAS_SHM
struct has_iceoryx_address_helper_arg {
  const ddsi_locator_t *loc_iceoryx_addr;
  bool has_iceoryx_address;
};
#endif /* DDS_HAS_SHM */

static void proxy_writer_get_alive_state_locked (struct ddsi_proxy_writer *pwr, struct ddsi_alive_state *st)
{
  st->alive = pwr->alive;
  st->vclock = pwr->alive_vclock;
}

void ddsi_proxy_writer_get_alive_state (struct ddsi_proxy_writer *pwr, struct ddsi_alive_state *st)
{
  ddsrt_mutex_lock (&pwr->e.lock);
  proxy_writer_get_alive_state_locked (pwr, st);
  ddsrt_mutex_unlock (&pwr->e.lock);
}

static int proxy_endpoint_common_init (struct ddsi_entity_common *e, struct ddsi_proxy_endpoint_common *c, enum ddsi_entity_kind kind, const struct ddsi_guid *guid, ddsrt_wctime_t tcreate, seqno_t seq, struct ddsi_proxy_participant *proxypp, struct addrset *as, const ddsi_plist_t *plist)
{
  int ret;

  if (ddsi_is_builtin_entityid (guid->entityid, proxypp->vendor))
    assert ((plist->qos.present & QP_TYPE_NAME) == 0);
  else
    assert ((plist->qos.present & (QP_TOPIC_NAME | QP_TYPE_NAME)) == (QP_TOPIC_NAME | QP_TYPE_NAME));

  ddsi_entity_common_init (e, proxypp->e.gv, guid, kind, tcreate, proxypp->vendor, false);
  c->xqos = ddsi_xqos_dup (&plist->qos);
  c->as = ref_addrset (as);
  c->vendor = proxypp->vendor;
  c->seq = seq;
#ifdef DDS_HAS_TYPE_DISCOVERY
  if (plist->qos.present & QP_TYPE_INFORMATION)
  {
    if ((c->type_pair = ddsrt_calloc (1, sizeof (*c->type_pair))) == NULL)
    {
      ret = DDS_RETCODE_OUT_OF_RESOURCES;
      goto err;
    }
    if ((ret = ddsi_type_ref_proxy (proxypp->e.gv, &c->type_pair->minimal, plist->qos.type_information, DDSI_TYPEID_KIND_MINIMAL, guid))
        || (ret = ddsi_type_ref_proxy (proxypp->e.gv, &c->type_pair->complete, plist->qos.type_information, DDSI_TYPEID_KIND_COMPLETE, guid)))
      goto err;
  }
  else
  {
    c->type_pair = NULL;
  }
#endif

  if (plist->present & PP_GROUP_GUID)
    c->group_guid = plist->group_guid;
  else
    memset (&c->group_guid, 0, sizeof (c->group_guid));

#ifdef DDS_HAS_SECURITY
  q_omg_get_proxy_endpoint_security_info(e, &proxypp->security_info, plist, &c->security_info);
#endif

  ret = ddsi_ref_proxy_participant (proxypp, c);

#ifdef DDS_HAS_TYPE_DISCOVERY
err:
#endif
  if (ret != DDS_RETCODE_OK)
  {
#ifdef DDS_HAS_TYPE_DISCOVERY
    if (c->type_pair != NULL)
    {
      if (c->type_pair->minimal)
      {
        ddsi_type_unreg_proxy (proxypp->e.gv, c->type_pair->minimal, guid);
        ddsi_type_unref (proxypp->e.gv, c->type_pair->minimal);
      }
      if (c->type_pair->complete)
      {
        ddsi_type_unreg_proxy (proxypp->e.gv, c->type_pair->complete, guid);
        ddsi_type_unref (proxypp->e.gv, c->type_pair->complete);
      }
      ddsrt_free (c->type_pair);
    }
#endif
    ddsi_xqos_fini (c->xqos);
    ddsrt_free (c->xqos);
    unref_addrset (c->as);
    ddsi_entity_common_fini (e);
  }

  return ret;
}

static void proxy_endpoint_common_fini (struct ddsi_entity_common *e, struct ddsi_proxy_endpoint_common *c)
{
  ddsi_unref_proxy_participant (c->proxypp, c);
  ddsi_xqos_fini (c->xqos);
  ddsrt_free (c->xqos);
  unref_addrset (c->as);
  ddsi_entity_common_fini (e);
}

#ifdef DDS_HAS_SHM
static void has_iceoryx_address_helper (const ddsi_xlocator_t *n, void *varg)
{
  struct has_iceoryx_address_helper_arg *arg = varg;
  if (n->c.kind == NN_LOCATOR_KIND_SHEM && memcmp (arg->loc_iceoryx_addr->address, n->c.address, sizeof (arg->loc_iceoryx_addr->address)) == 0)
    arg->has_iceoryx_address = true;
}

static bool has_iceoryx_address (struct ddsi_domaingv * const gv, struct addrset * const as)
{
  if (!gv->config.enable_shm)
    return false;
  else
  {
    struct has_iceoryx_address_helper_arg arg = {
      .loc_iceoryx_addr = &gv->loc_iceoryx_addr,
      .has_iceoryx_address = false
    };
    addrset_forall (as, has_iceoryx_address_helper, &arg);
    return arg.has_iceoryx_address;
  }
}
#endif /* DDS_HAS_SHM */

#ifdef DDS_HAS_TYPE_DISCOVERY
bool ddsi_is_proxy_endpoint (const struct ddsi_entity_common *e)
{
  return e->kind == DDSI_EK_PROXY_READER || e->kind == DDSI_EK_PROXY_WRITER;
}
#endif /* DDS_HAS_TYPE_DISCOVERY */

/* PROXY-WRITER ----------------------------------------------------- */

static enum nn_reorder_mode get_proxy_writer_reorder_mode(const ddsi_entityid_t pwr_entityid, int isreliable)
{
  if (isreliable)
  {
    return NN_REORDER_MODE_NORMAL;
  }
  if (pwr_entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER)
  {
    return NN_REORDER_MODE_ALWAYS_DELIVER;
  }
  return NN_REORDER_MODE_MONOTONICALLY_INCREASING;
}

int ddsi_new_proxy_writer (struct ddsi_domaingv *gv, const struct ddsi_guid *ppguid, const struct ddsi_guid *guid, struct addrset *as, const ddsi_plist_t *plist, struct nn_dqueue *dqueue, struct xeventq *evq, ddsrt_wctime_t timestamp, seqno_t seq)
{
  struct ddsi_proxy_participant *proxypp;
  struct ddsi_proxy_writer *pwr;
  int isreliable;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();
  enum nn_reorder_mode reorder_mode;
  int ret;

  assert (ddsi_is_writer_entityid (guid->entityid));
  assert (entidx_lookup_proxy_writer_guid (gv->entity_index, guid) == NULL);

  if ((proxypp = entidx_lookup_proxy_participant_guid (gv->entity_index, ppguid)) == NULL)
  {
    GVWARNING ("ddsi_new_proxy_writer("PGUIDFMT"): proxy participant unknown\n", PGUID (*guid));
    return DDS_RETCODE_BAD_PARAMETER;
  }

  pwr = ddsrt_malloc (sizeof (*pwr));
  if ((ret = proxy_endpoint_common_init (&pwr->e, &pwr->c, DDSI_EK_PROXY_WRITER, guid, timestamp, seq, proxypp, as, plist)) != DDS_RETCODE_OK)
  {
    ddsrt_free (pwr);
    return ret;
  }

  ddsrt_avl_init (&ddsi_pwr_readers_treedef, &pwr->readers);
  pwr->n_reliable_readers = 0;
  pwr->n_readers_out_of_sync = 0;
  pwr->last_seq = 0;
  pwr->last_fragnum = UINT32_MAX;
  pwr->nackfragcount = 1;
  pwr->alive = 1;
  pwr->alive_vclock = 0;
  pwr->filtered = 0;
  ddsrt_atomic_st32 (&pwr->next_deliv_seq_lowword, 1);
  if (ddsi_is_builtin_entityid (pwr->e.guid.entityid, pwr->c.vendor)) {
    /* The DDSI built-in proxy writers always deliver
       asynchronously */
    pwr->deliver_synchronously = 0;
  } else if (pwr->c.xqos->latency_budget.duration <= gv->config.synchronous_delivery_latency_bound &&
             pwr->c.xqos->transport_priority.value >= gv->config.synchronous_delivery_priority_threshold) {
    /* Regular proxy-writers with a sufficiently low latency_budget
       and a sufficiently high transport_priority deliver
       synchronously */
    pwr->deliver_synchronously = 1;
  } else {
    pwr->deliver_synchronously = 0;
  }
  /* Pretend we have seen a heartbeat if the proxy writer is a best-effort one */
  isreliable = (pwr->c.xqos->reliability.kind != DDS_RELIABILITY_BEST_EFFORT);
  pwr->have_seen_heartbeat = !isreliable;
  pwr->local_matching_inprogress = 1;
#ifdef DDS_HAS_SSM
  pwr->supports_ssm = (addrset_contains_ssm (gv, as) && gv->config.allowMulticast & DDSI_AMC_SSM) ? 1 : 0;
#endif
#ifdef DDS_HAS_SHM
  pwr->is_iceoryx = has_iceoryx_address (gv, as) ? 1 : 0;
#endif
  if (plist->present & PP_CYCLONE_REDUNDANT_NETWORKING)
    pwr->redundant_networking = (plist->cyclone_redundant_networking != 0);
  else
    pwr->redundant_networking = proxypp->redundant_networking;

  assert (pwr->c.xqos->present & QP_LIVELINESS);
  if (pwr->c.xqos->liveliness.lease_duration != DDS_INFINITY)
  {
    ddsrt_etime_t texpire = ddsrt_etime_add_duration (ddsrt_time_elapsed (), pwr->c.xqos->liveliness.lease_duration);
    pwr->lease = lease_new (texpire, pwr->c.xqos->liveliness.lease_duration, &pwr->e);
    if (pwr->c.xqos->liveliness.kind != DDS_LIVELINESS_MANUAL_BY_TOPIC)
    {
      ddsrt_mutex_lock (&proxypp->e.lock);
      ddsi_proxy_participant_add_pwr_lease_locked (proxypp, pwr);
      ddsrt_mutex_unlock (&proxypp->e.lock);
    }
    else
    {
      lease_register (pwr->lease);
    }
  }
  else
  {
    pwr->lease = NULL;
  }

  if (isreliable)
  {
    pwr->defrag = nn_defrag_new (&gv->logconfig, NN_DEFRAG_DROP_LATEST, gv->config.defrag_reliable_maxsamples);
  }
  else
  {
    pwr->defrag = nn_defrag_new (&gv->logconfig, NN_DEFRAG_DROP_OLDEST, gv->config.defrag_unreliable_maxsamples);
  }
  reorder_mode = get_proxy_writer_reorder_mode(pwr->e.guid.entityid, isreliable);
  pwr->reorder = nn_reorder_new (&gv->logconfig, reorder_mode, gv->config.primary_reorder_maxsamples, gv->config.late_ack_mode);

  if (pwr->e.guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER)
  {
    /* for the builtin_volatile_secure proxy writer which uses a content filter set the next expected
     * sequence number of the reorder administration to the maximum sequence number to ensure that effectively
     * the reorder administration of the builtin_volatile_secure proxy writer is not used and because the corresponding
     * reader is always considered out of sync the reorder administration of the corresponding reader will be used
     * instead.
     */
    nn_reorder_set_next_seq(pwr->reorder, MAX_SEQ_NUMBER);
    pwr->filtered = 1;
  }

  pwr->dqueue = dqueue;
  pwr->evq = evq;
  pwr->ddsi2direct_cb = 0;
  pwr->ddsi2direct_cbarg = 0;

  local_reader_ary_init (&pwr->rdary);

  /* locking the entity prevents matching while the built-in topic hasn't been published yet */
  ddsrt_mutex_lock (&pwr->e.lock);
  entidx_insert_proxy_writer_guid (gv->entity_index, pwr);
  builtintopic_write_endpoint (gv->builtin_topic_interface, &pwr->e, timestamp, true);
  ddsrt_mutex_unlock (&pwr->e.lock);

  match_proxy_writer_with_readers (pwr, tnow);

  ddsrt_mutex_lock (&pwr->e.lock);
  pwr->local_matching_inprogress = 0;
  ddsrt_mutex_unlock (&pwr->e.lock);

  return 0;
}

void ddsi_update_proxy_writer (struct ddsi_proxy_writer *pwr, seqno_t seq, struct addrset *as, const struct dds_qos *xqos, ddsrt_wctime_t timestamp)
{
  struct ddsi_reader * rd;
  struct ddsi_pwr_rd_match * m;
  ddsrt_avl_iter_t iter;

  /* Update proxy writer endpoints (from SEDP alive) */

  ddsrt_mutex_lock (&pwr->e.lock);
  if (seq > pwr->c.seq)
  {
    pwr->c.seq = seq;
    if (! addrset_eq_onesidederr (pwr->c.as, as))
    {
#ifdef DDS_HAS_SSM
      pwr->supports_ssm = (addrset_contains_ssm (pwr->e.gv, as) && pwr->e.gv->config.allowMulticast & DDSI_AMC_SSM) ? 1 : 0;
#endif
      unref_addrset (pwr->c.as);
      ref_addrset (as);
      pwr->c.as = as;
      m = ddsrt_avl_iter_first (&ddsi_pwr_readers_treedef, &pwr->readers, &iter);
      while (m)
      {
        rd = entidx_lookup_reader_guid (pwr->e.gv->entity_index, &m->rd_guid);
        if (rd)
        {
          qxev_pwr_entityid (pwr, &rd->e.guid);
        }
        m = ddsrt_avl_iter_next (&iter);
      }
    }

    (void) ddsi_update_qos_locked (&pwr->e, pwr->c.xqos, xqos, timestamp);
  }
  ddsrt_mutex_unlock (&pwr->e.lock);
}

static void gc_delete_proxy_writer (struct gcreq *gcreq)
{
  struct ddsi_proxy_writer *pwr = gcreq->arg;
  ELOGDISC (pwr, "gc_delete_proxy_writer (%p, "PGUIDFMT")\n", (void *) gcreq, PGUID (pwr->e.guid));
  gcreq_free (gcreq);

#ifdef DDS_HAS_TYPE_DISCOVERY
  if (pwr->c.type_pair != NULL)
  {
    ddsi_type_unref (pwr->e.gv, pwr->c.type_pair->minimal);
    ddsi_type_unref (pwr->e.gv, pwr->c.type_pair->complete);
    ddsrt_free (pwr->c.type_pair);
  }
#endif

  while (!ddsrt_avl_is_empty (&pwr->readers))
  {
    struct ddsi_pwr_rd_match *m = ddsrt_avl_root_non_empty (&ddsi_pwr_readers_treedef, &pwr->readers);
    ddsrt_avl_delete (&ddsi_pwr_readers_treedef, &pwr->readers, m);
    reader_drop_connection (&m->rd_guid, pwr);
    ddsi_update_reader_init_acknack_count (&pwr->e.gv->logconfig, pwr->e.gv->entity_index, &m->rd_guid, m->count);
    free_pwr_rd_match (m);
  }
  local_reader_ary_fini (&pwr->rdary);
  if (pwr->c.xqos->liveliness.lease_duration != DDS_INFINITY)
    lease_free (pwr->lease);
#ifdef DDS_HAS_SECURITY
  q_omg_security_deregister_remote_writer(pwr);
#endif
  proxy_endpoint_common_fini (&pwr->e, &pwr->c);
  nn_defrag_free (pwr->defrag);
  nn_reorder_free (pwr->reorder);
  ddsrt_free (pwr);
}

static void gc_delete_proxy_writer_dqueue_bubble_cb (struct gcreq *gcreq)
{
  /* delete proxy_writer, phase 3 */
  struct ddsi_proxy_writer *pwr = gcreq->arg;
  ELOGDISC (pwr, "gc_delete_proxy_writer_dqueue_bubble(%p, "PGUIDFMT")\n", (void *) gcreq, PGUID (pwr->e.guid));
  gcreq_requeue (gcreq, gc_delete_proxy_writer);
}

static void gc_delete_proxy_writer_dqueue (struct gcreq *gcreq)
{
  /* delete proxy_writer, phase 2 */
  struct ddsi_proxy_writer *pwr = gcreq->arg;
  struct nn_dqueue *dqueue = pwr->dqueue;
  ELOGDISC (pwr, "gc_delete_proxy_writer_dqueue(%p, "PGUIDFMT")\n", (void *) gcreq, PGUID (pwr->e.guid));
  nn_dqueue_enqueue_callback (dqueue, (void (*) (void *)) gc_delete_proxy_writer_dqueue_bubble_cb, gcreq);
}

static int gcreq_proxy_writer (struct ddsi_proxy_writer *pwr)
{
  struct gcreq *gcreq = gcreq_new (pwr->e.gv->gcreq_queue, gc_delete_proxy_writer_dqueue);
  gcreq->arg = pwr;
  gcreq_enqueue (gcreq);
  return 0;
}

/* First stage in deleting the proxy writer. In this function the pwr and its member pointers
   will remain valid. The real cleaning-up is done async in gc_delete_proxy_writer. */
int ddsi_delete_proxy_writer (struct ddsi_domaingv *gv, const struct ddsi_guid *guid, ddsrt_wctime_t timestamp, int isimplicit)
{
  struct ddsi_proxy_writer *pwr;
  DDSRT_UNUSED_ARG (isimplicit);
  GVLOGDISC ("ddsi_delete_proxy_writer ("PGUIDFMT") ", PGUID (*guid));

  ddsrt_mutex_lock (&gv->lock);
  if ((pwr = entidx_lookup_proxy_writer_guid (gv->entity_index, guid)) == NULL)
  {
    ddsrt_mutex_unlock (&gv->lock);
    GVLOGDISC ("- unknown\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }

  /* Set "deleting" flag in particular for Lite, to signal to the receive path it can't
     trust rdary[] anymore, which is because removing the proxy writer from the hash
     table will prevent the readers from looking up the proxy writer, and consequently
     from removing themselves from the proxy writer's rdary[]. */
  local_reader_ary_setinvalid (&pwr->rdary);
  GVLOGDISC ("- deleting\n");
  builtintopic_write_endpoint (gv->builtin_topic_interface, &pwr->e, timestamp, false);
#ifdef DDS_HAS_TYPE_DISCOVERY
  /* Unregister from type before removing from entity index, because a tl_lookup_reply
     could be pending and will trigger an update of the endpoint matching for all
     endpoints that are registered for the type. This call removes this proxy writer
     from the type's endpoint list. */
  if (pwr->c.type_pair != NULL)
  {
    ddsi_type_unreg_proxy (gv, pwr->c.type_pair->minimal, &pwr->e.guid);
    ddsi_type_unreg_proxy (gv, pwr->c.type_pair->complete, &pwr->e.guid);
  }
#endif
  entidx_remove_proxy_writer_guid (gv->entity_index, pwr);
  ddsrt_mutex_unlock (&gv->lock);
  if (pwr->c.xqos->liveliness.lease_duration != DDS_INFINITY && pwr->c.xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_TOPIC)
    lease_unregister (pwr->lease);
  if (ddsi_proxy_writer_set_notalive (pwr, false) != DDS_RETCODE_OK)
    GVLOGDISC ("ddsi_proxy_writer_set_notalive failed for "PGUIDFMT"\n", PGUID(*guid));
  gcreq_proxy_writer (pwr);
  return DDS_RETCODE_OK;
}

static void proxy_writer_notify_liveliness_change_may_unlock (struct ddsi_proxy_writer *pwr)
{
  struct ddsi_alive_state alive_state;
  proxy_writer_get_alive_state_locked (pwr, &alive_state);

  struct ddsi_guid rdguid;
  struct ddsi_pwr_rd_match *m;
  memset (&rdguid, 0, sizeof (rdguid));
  while (pwr->alive_vclock == alive_state.vclock &&
         (m = ddsrt_avl_lookup_succ (&ddsi_pwr_readers_treedef, &pwr->readers, &rdguid)) != NULL)
  {
    rdguid = m->rd_guid;
    ddsrt_mutex_unlock (&pwr->e.lock);
    /* unlocking pwr means alive state may have changed already; we break out of the loop once we
       detect this but there for the reader in the current iteration, anything is possible */
    ddsi_reader_update_notify_pwr_alive_state_guid (&rdguid, pwr, &alive_state);
    ddsrt_mutex_lock (&pwr->e.lock);
  }
}

void ddsi_proxy_writer_set_alive_may_unlock (struct ddsi_proxy_writer *pwr, bool notify)
{
  /* Caller has pwr->e.lock, so we can safely read pwr->alive.  Updating pwr->alive requires
     also taking pwr->c.proxypp->e.lock because pwr->alive <=> (pwr->lease in proxypp's lease
     heap). */
  assert (!pwr->alive);

  /* check that proxy writer still exists (when deleting it is removed from guid hash) */
  if (entidx_lookup_proxy_writer_guid (pwr->e.gv->entity_index, &pwr->e.guid) == NULL)
  {
    ELOGDISC (pwr, "ddsi_proxy_writer_set_alive_may_unlock("PGUIDFMT") - not in entity index, pwr deleting\n", PGUID (pwr->e.guid));
    return;
  }

  ddsrt_mutex_lock (&pwr->c.proxypp->e.lock);
  pwr->alive = true;
  pwr->alive_vclock++;
  if (pwr->c.xqos->liveliness.lease_duration != DDS_INFINITY)
  {
    if (pwr->c.xqos->liveliness.kind != DDS_LIVELINESS_MANUAL_BY_TOPIC)
      ddsi_proxy_participant_add_pwr_lease_locked (pwr->c.proxypp, pwr);
    else
      lease_set_expiry (pwr->lease, ddsrt_etime_add_duration (ddsrt_time_elapsed (), pwr->lease->tdur));
  }
  ddsrt_mutex_unlock (&pwr->c.proxypp->e.lock);

  if (notify)
    proxy_writer_notify_liveliness_change_may_unlock (pwr);
}

int ddsi_proxy_writer_set_notalive (struct ddsi_proxy_writer *pwr, bool notify)
{
  /* Caller should not have taken pwr->e.lock and pwr->c.proxypp->e.lock;
   * this function takes both locks to update pwr->alive value */
  ddsrt_mutex_lock (&pwr->e.lock);
  if (!pwr->alive)
  {
    ddsrt_mutex_unlock (&pwr->e.lock);
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  }

  ddsrt_mutex_lock (&pwr->c.proxypp->e.lock);
  pwr->alive = false;
  pwr->alive_vclock++;
  if (pwr->c.xqos->liveliness.lease_duration != DDS_INFINITY && pwr->c.xqos->liveliness.kind != DDS_LIVELINESS_MANUAL_BY_TOPIC)
    ddsi_proxy_participant_remove_pwr_lease_locked (pwr->c.proxypp, pwr);
  ddsrt_mutex_unlock (&pwr->c.proxypp->e.lock);

  if (notify)
    proxy_writer_notify_liveliness_change_may_unlock (pwr);
  ddsrt_mutex_unlock (&pwr->e.lock);
  return DDS_RETCODE_OK;
}

/* PROXY-READER ----------------------------------------------------- */

int ddsi_new_proxy_reader (struct ddsi_domaingv *gv, const struct ddsi_guid *ppguid, const struct ddsi_guid *guid, struct addrset *as, const ddsi_plist_t *plist, ddsrt_wctime_t timestamp, seqno_t seq
#ifdef DDS_HAS_SSM
, int favours_ssm
#endif
)
{
  struct ddsi_proxy_participant *proxypp;
  struct ddsi_proxy_reader *prd;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();
  int ret;

  assert (!ddsi_is_writer_entityid (guid->entityid));
  assert (entidx_lookup_proxy_reader_guid (gv->entity_index, guid) == NULL);

  if ((proxypp = entidx_lookup_proxy_participant_guid (gv->entity_index, ppguid)) == NULL)
  {
    GVWARNING ("ddsi_new_proxy_reader("PGUIDFMT"): proxy participant unknown\n", PGUID (*guid));
    return DDS_RETCODE_BAD_PARAMETER;
  }

  prd = ddsrt_malloc (sizeof (*prd));
  if ((ret = proxy_endpoint_common_init (&prd->e, &prd->c, DDSI_EK_PROXY_READER, guid, timestamp, seq, proxypp, as, plist)) != DDS_RETCODE_OK)
  {
    ddsrt_free (prd);
    return ret;
  }

  prd->deleting = 0;
#ifdef DDS_HAS_SSM
  prd->favours_ssm = (favours_ssm && gv->config.allowMulticast & DDSI_AMC_SSM) ? 1 : 0;
#endif
#ifdef DDS_HAS_SHM
  prd->is_iceoryx = has_iceoryx_address (gv, as) ? 1 : 0;
#endif
  prd->is_fict_trans_reader = 0;
  prd->receive_buffer_size = proxypp->receive_buffer_size;
  prd->requests_keyhash = (plist->present & PP_CYCLONE_REQUESTS_KEYHASH) && plist->cyclone_requests_keyhash;
  if (plist->present & PP_CYCLONE_REDUNDANT_NETWORKING)
    prd->redundant_networking = (plist->cyclone_redundant_networking != 0);
  else
    prd->redundant_networking = proxypp->redundant_networking;

  ddsrt_avl_init (&ddsi_prd_writers_treedef, &prd->writers);

#ifdef DDS_HAS_SECURITY
  if (prd->e.guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER)
    prd->filter = volatile_secure_data_filter;
  else
    prd->filter = NULL;
#else
  prd->filter = NULL;
#endif

  /* locking the entity prevents matching while the built-in topic hasn't been published yet */
  ddsrt_mutex_lock (&prd->e.lock);
  entidx_insert_proxy_reader_guid (gv->entity_index, prd);
  builtintopic_write_endpoint (gv->builtin_topic_interface, &prd->e, timestamp, true);
  ddsrt_mutex_unlock (&prd->e.lock);

  match_proxy_reader_with_writers (prd, tnow);
  return DDS_RETCODE_OK;
}

void ddsi_update_proxy_reader (struct ddsi_proxy_reader *prd, seqno_t seq, struct addrset *as, const struct dds_qos *xqos, ddsrt_wctime_t timestamp)
{
  struct ddsi_prd_wr_match * m;
  ddsi_guid_t wrguid;

  memset (&wrguid, 0, sizeof (wrguid));

  ddsrt_mutex_lock (&prd->e.lock);
  if (seq > prd->c.seq)
  {
    prd->c.seq = seq;
    if (! addrset_eq_onesidederr (prd->c.as, as))
    {
      /* Update proxy reader endpoints (from SEDP alive) */

      unref_addrset (prd->c.as);
      ref_addrset (as);
      prd->c.as = as;

      /* Rebuild writer endpoints */

      while ((m = ddsrt_avl_lookup_succ_eq (&ddsi_prd_writers_treedef, &prd->writers, &wrguid)) != NULL)
      {
        struct ddsi_prd_wr_match *next;
        ddsi_guid_t guid_next;
        struct ddsi_writer * wr;

        wrguid = m->wr_guid;
        next = ddsrt_avl_find_succ (&ddsi_prd_writers_treedef, &prd->writers, m);
        if (next)
        {
          guid_next = next->wr_guid;
        }
        else
        {
          memset (&guid_next, 0xff, sizeof (guid_next));
          guid_next.entityid.u = (guid_next.entityid.u & ~(unsigned)0xff) | NN_ENTITYID_KIND_WRITER_NO_KEY;
        }

        ddsrt_mutex_unlock (&prd->e.lock);
        wr = entidx_lookup_writer_guid (prd->e.gv->entity_index, &wrguid);
        if (wr)
        {
          ddsrt_mutex_lock (&wr->e.lock);
          ddsi_rebuild_writer_addrset (wr);
          ddsrt_mutex_unlock (&wr->e.lock);
          qxev_prd_entityid (prd, &wr->e.guid);
        }
        wrguid = guid_next;
        ddsrt_mutex_lock (&prd->e.lock);
      }
    }

    (void) ddsi_update_qos_locked (&prd->e, prd->c.xqos, xqos, timestamp);
  }
  ddsrt_mutex_unlock (&prd->e.lock);
}

static void proxy_reader_set_delete_and_ack_all_messages (struct ddsi_proxy_reader *prd)
{
  ddsi_guid_t wrguid;
  struct ddsi_writer *wr;
  struct ddsi_prd_wr_match *m;

  memset (&wrguid, 0, sizeof (wrguid));
  ddsrt_mutex_lock (&prd->e.lock);
  prd->deleting = 1;
  while ((m = ddsrt_avl_lookup_succ_eq (&ddsi_prd_writers_treedef, &prd->writers, &wrguid)) != NULL)
  {
    /* have to be careful walking the tree -- pretty is different, but
       I want to check this before I write a lookup_succ function. */
    struct ddsi_prd_wr_match *m_a_next;
    ddsi_guid_t wrguid_next;
    wrguid = m->wr_guid;
    if ((m_a_next = ddsrt_avl_find_succ (&ddsi_prd_writers_treedef, &prd->writers, m)) != NULL)
      wrguid_next = m_a_next->wr_guid;
    else
    {
      memset (&wrguid_next, 0xff, sizeof (wrguid_next));
      wrguid_next.entityid.u = (wrguid_next.entityid.u & ~(unsigned)0xff) | NN_ENTITYID_KIND_WRITER_NO_KEY;
    }

    ddsrt_mutex_unlock (&prd->e.lock);
    if ((wr = entidx_lookup_writer_guid (prd->e.gv->entity_index, &wrguid)) != NULL)
    {
      struct whc_node *deferred_free_list = NULL;
      struct ddsi_wr_prd_match *m_wr;
      ddsrt_mutex_lock (&wr->e.lock);
      if ((m_wr = ddsrt_avl_lookup (&ddsi_wr_readers_treedef, &wr->readers, &prd->e.guid)) != NULL)
      {
        struct whc_state whcst;
        m_wr->seq = MAX_SEQ_NUMBER;
        ddsrt_avl_augment_update (&ddsi_wr_readers_treedef, m_wr);
        (void)ddsi_remove_acked_messages (wr, &whcst, &deferred_free_list);
        ddsi_writer_clear_retransmitting (wr);
      }
      ddsrt_mutex_unlock (&wr->e.lock);
      whc_free_deferred_free_list (wr->whc, deferred_free_list);
    }

    wrguid = wrguid_next;
    ddsrt_mutex_lock (&prd->e.lock);
  }
  ddsrt_mutex_unlock (&prd->e.lock);
}

static void gc_delete_proxy_reader (struct gcreq *gcreq)
{
  struct ddsi_proxy_reader *prd = gcreq->arg;
  ELOGDISC (prd, "gc_delete_proxy_reader (%p, "PGUIDFMT")\n", (void *) gcreq, PGUID (prd->e.guid));
  gcreq_free (gcreq);

#ifdef DDS_HAS_TYPE_DISCOVERY
  if (prd->c.type_pair != NULL)
  {
    ddsi_type_unref (prd->e.gv, prd->c.type_pair->minimal);
    ddsi_type_unref (prd->e.gv, prd->c.type_pair->complete);
    ddsrt_free (prd->c.type_pair);
  }
#endif

  while (!ddsrt_avl_is_empty (&prd->writers))
  {
    struct ddsi_prd_wr_match *m = ddsrt_avl_root_non_empty (&ddsi_prd_writers_treedef, &prd->writers);
    ddsrt_avl_delete (&ddsi_prd_writers_treedef, &prd->writers, m);
    writer_drop_connection (&m->wr_guid, prd);
    free_prd_wr_match (m);
  }
#ifdef DDS_HAS_SECURITY
  q_omg_security_deregister_remote_reader(prd);
#endif
  proxy_endpoint_common_fini (&prd->e, &prd->c);
  ddsrt_free (prd);
}

static int gcreq_proxy_reader (struct ddsi_proxy_reader *prd)
{
  struct gcreq *gcreq = gcreq_new (prd->e.gv->gcreq_queue, gc_delete_proxy_reader);
  gcreq->arg = prd;
  gcreq_enqueue (gcreq);
  return 0;
}

int ddsi_delete_proxy_reader (struct ddsi_domaingv *gv, const struct ddsi_guid *guid, ddsrt_wctime_t timestamp, int isimplicit)
{
  struct ddsi_proxy_reader *prd;
  (void)isimplicit;
  GVLOGDISC ("ddsi_delete_proxy_reader ("PGUIDFMT") ", PGUID (*guid));

  ddsrt_mutex_lock (&gv->lock);
  if ((prd = entidx_lookup_proxy_reader_guid (gv->entity_index, guid)) == NULL)
  {
    ddsrt_mutex_unlock (&gv->lock);
    GVLOGDISC ("- unknown\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  builtintopic_write_endpoint (gv->builtin_topic_interface, &prd->e, timestamp, false);
#ifdef DDS_HAS_TYPE_DISCOVERY
  /* Unregister the proxy guid with the ddsi_type before removing from
     entity index, because a tl_lookup_reply could be pending and will
     trigger an update of the endpoint matching for all endpoints that
     are registered for the type. This removes this proxy writer from
     the endpoint list for the type. */
  if (prd->c.type_pair != NULL)
  {
    ddsi_type_unreg_proxy (gv, prd->c.type_pair->minimal, &prd->e.guid);
    ddsi_type_unreg_proxy (gv, prd->c.type_pair->complete, &prd->e.guid);
  }
#endif
  entidx_remove_proxy_reader_guid (gv->entity_index, prd);
  ddsrt_mutex_unlock (&gv->lock);
  GVLOGDISC ("- deleting\n");

  /* If the proxy reader is reliable, pretend it has just acked all
     messages: this allows a throttled writer to once again make
     progress, which in turn is necessary for the garbage collector to
     do its work. */
  proxy_reader_set_delete_and_ack_all_messages (prd);

  gcreq_proxy_reader (prd);
  return 0;
}

struct ddsi_entity_common *ddsi_entity_common_from_proxy_endpoint_common (const struct ddsi_proxy_endpoint_common *c)
{
  assert (offsetof (struct ddsi_proxy_writer, e) == 0);
  assert (offsetof (struct ddsi_proxy_reader, e) == offsetof (struct ddsi_proxy_writer, e));
  assert (offsetof (struct ddsi_proxy_reader, c) == offsetof (struct ddsi_proxy_writer, c));
  assert (c != NULL);
  return (struct ddsi_entity_common *) ((char *) c - offsetof (struct ddsi_proxy_writer, c));
}
