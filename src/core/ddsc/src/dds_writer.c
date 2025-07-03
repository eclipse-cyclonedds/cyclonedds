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

#include "dds/dds.h"
#include "dds/version.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/ddsi/ddsi_thread.h"
#include "dds/ddsi/ddsi_xmsg.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_statistics.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/cdr/dds_cdrstream.h"
#include "dds/ddsc/dds_internal_api.h"
#include "dds__writer.h"
#include "dds__listener.h"
#include "dds__init.h"
#include "dds__publisher.h"
#include "dds__topic.h"
#include "dds__get_status.h"
#include "dds__qos.h"
#include "dds__whc.h"
#include "dds__statistics.h"
#include "dds__psmx.h"
#include "dds__heap_loan.h"
#include "dds__guid.h"

DECL_ENTITY_LOCK_UNLOCK (dds_writer)

#define DDS_WRITER_STATUS_MASK                                   \
                        (DDS_LIVELINESS_LOST_STATUS              |\
                         DDS_OFFERED_DEADLINE_MISSED_STATUS      |\
                         DDS_OFFERED_INCOMPATIBLE_QOS_STATUS     |\
                         DDS_PUBLICATION_MATCHED_STATUS)

static dds_return_t dds_writer_status_validate (uint32_t mask)
{
  return (mask & ~DDS_WRITER_STATUS_MASK) ? DDS_RETCODE_BAD_PARAMETER : DDS_RETCODE_OK;
}

static void update_offered_deadline_missed (struct dds_offered_deadline_missed_status *st, const ddsi_status_cb_data_t *data)
{
  st->last_instance_handle = data->handle;
  uint64_t tmp = (uint64_t)data->extra + (uint64_t)st->total_count;
  st->total_count = tmp > UINT32_MAX ? UINT32_MAX : (uint32_t)tmp;
  // always incrementing st->total_count_change, then copying into *lst is
  // a bit more than minimal work, but this guarantees the correct value
  // also when enabling a listeners after some events have occurred
  //
  // (same line of reasoning for all of them)
  int64_t tmp2 = (int64_t)data->extra + (int64_t)st->total_count_change;
  st->total_count_change = tmp2 > INT32_MAX ? INT32_MAX : tmp2 < INT32_MIN ? INT32_MIN : (int32_t)tmp2;
}

static void update_offered_incompatible_qos (struct dds_offered_incompatible_qos_status *st, const ddsi_status_cb_data_t *data)
{
  st->last_policy_id = data->extra;
  st->total_count++;
  st->total_count_change++;
}

static void update_liveliness_lost (struct dds_liveliness_lost_status *st, const ddsi_status_cb_data_t *data)
{
  (void) data;
  st->total_count++;
  st->total_count_change++;
}

static void update_publication_matched (struct dds_publication_matched_status *st, const ddsi_status_cb_data_t *data)
{
  st->last_subscription_handle = data->handle;
  if (data->add) {
    st->total_count++;
    st->current_count++;
    st->total_count_change++;
    st->current_count_change++;
  } else {
    st->current_count--;
    st->current_count_change--;
  }
}

DDS_GET_STATUS(writer, publication_matched, PUBLICATION_MATCHED, total_count_change, current_count_change)
DDS_GET_STATUS(writer, liveliness_lost, LIVELINESS_LOST, total_count_change)
DDS_GET_STATUS(writer, offered_deadline_missed, OFFERED_DEADLINE_MISSED, total_count_change)
DDS_GET_STATUS(writer, offered_incompatible_qos, OFFERED_INCOMPATIBLE_QOS, total_count_change)

STATUS_CB_IMPL(writer, publication_matched, PUBLICATION_MATCHED, total_count_change, current_count_change)
STATUS_CB_IMPL(writer, liveliness_lost, LIVELINESS_LOST, total_count_change)
STATUS_CB_IMPL(writer, offered_deadline_missed, OFFERED_DEADLINE_MISSED, total_count_change)
STATUS_CB_IMPL(writer, offered_incompatible_qos, OFFERED_INCOMPATIBLE_QOS, total_count_change)

void dds_writer_status_cb (void *entity, const struct ddsi_status_cb_data *data)
{
  dds_writer * const wr = entity;

  /* When data is NULL, it means that the DDSI reader is deleted. */
  if (data == NULL)
  {
    /* Release the initial claim that was done during the create. This
     * will indicate that further API deletion is now possible. */
    ddsrt_mutex_lock (&wr->m_entity.m_mutex);
    wr->m_wr = NULL;
    ddsrt_cond_broadcast (&wr->m_entity.m_cond);
    ddsrt_mutex_unlock (&wr->m_entity.m_mutex);
    return;
  }

  /* FIXME: why wait if no listener is set? */
  ddsrt_mutex_lock (&wr->m_entity.m_observers_lock);
  wr->m_entity.m_cb_pending_count++;
  while (wr->m_entity.m_cb_count > 0)
    ddsrt_cond_wait (&wr->m_entity.m_observers_cond, &wr->m_entity.m_observers_lock);
  wr->m_entity.m_cb_count++;

  const enum dds_status_id status_id = (enum dds_status_id) data->raw_status_id;
  switch (status_id)
  {
    case DDS_OFFERED_DEADLINE_MISSED_STATUS_ID:
      status_cb_offered_deadline_missed (wr, data);
      break;
    case DDS_LIVELINESS_LOST_STATUS_ID:
      status_cb_liveliness_lost (wr, data);
      break;
    case DDS_OFFERED_INCOMPATIBLE_QOS_STATUS_ID:
      status_cb_offered_incompatible_qos (wr, data);
      break;
    case DDS_PUBLICATION_MATCHED_STATUS_ID:
      status_cb_publication_matched (wr, data);
      break;
    case DDS_DATA_AVAILABLE_STATUS_ID:
    case DDS_INCONSISTENT_TOPIC_STATUS_ID:
    case DDS_SAMPLE_LOST_STATUS_ID:
    case DDS_DATA_ON_READERS_STATUS_ID:
    case DDS_SAMPLE_REJECTED_STATUS_ID:
    case DDS_LIVELINESS_CHANGED_STATUS_ID:
    case DDS_SUBSCRIPTION_MATCHED_STATUS_ID:
    case DDS_REQUESTED_DEADLINE_MISSED_STATUS_ID:
    case DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS_ID:
      assert (0);
  }

  wr->m_entity.m_cb_count--;
  wr->m_entity.m_cb_pending_count--;
  ddsrt_cond_broadcast (&wr->m_entity.m_observers_cond);
  ddsrt_mutex_unlock (&wr->m_entity.m_observers_lock);
}

void dds_writer_invoke_cbs_for_pending_events(struct dds_entity *e, uint32_t status)
{
  dds_writer * const wr = (dds_writer *) e;
  struct dds_listener const * const lst =  &e->m_listener;

  if (lst->on_publication_matched && (status & DDS_PUBLICATION_MATCHED_STATUS)) {
    status_cb_publication_matched_invoke(wr);
  }
  if (lst->on_liveliness_lost && (status & DDS_LIVELINESS_LOST_STATUS)) {
    status_cb_liveliness_lost_invoke(wr);
  }
  if (lst->on_offered_incompatible_qos && (status & DDS_OFFERED_INCOMPATIBLE_QOS_STATUS)) {
    status_cb_offered_incompatible_qos_invoke(wr);
  }
  if (lst->on_offered_deadline_missed && (status & DDS_OFFERED_DEADLINE_MISSED_STATUS)) {
    status_cb_offered_deadline_missed_invoke(wr);
  }
}

static void dds_writer_interrupt (dds_entity *e) ddsrt_nonnull_all;

static void dds_writer_interrupt (dds_entity *e)
{
  struct ddsi_domaingv * const gv = &e->m_domain->gv;
  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv);
  ddsi_unblock_throttled_writer (gv, &e->m_guid);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
}

static void dds_writer_close (dds_entity *e) ddsrt_nonnull_all;

static void dds_writer_close (dds_entity *e)
{
  struct dds_writer * const wr = (struct dds_writer *) e;
  struct ddsi_domaingv * const gv = &e->m_domain->gv;
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_thread_state_awake (thrst, gv);
  ddsi_xpack_send (wr->m_xp, false);
  (void) ddsi_delete_writer (gv, &e->m_guid);
  ddsi_thread_state_asleep (thrst);

  ddsrt_mutex_lock (&e->m_mutex);
  while (wr->m_wr != NULL)
    ddsrt_cond_wait (&e->m_cond, &e->m_mutex);
  ddsrt_mutex_unlock (&e->m_mutex);
}

ddsrt_nonnull_all
static dds_return_t dds_writer_delete (dds_entity *e)
{
  dds_return_t ret = DDS_RETCODE_OK;
  dds_writer * const wr = (dds_writer *) e;

  // Freeing the loans requires the PSMX endpoints, so must be done before cleaning
  // up the endpoints. And m_loans is not used anymore from this point, so can also
  // be freed safely.
  dds_loan_pool_free (wr->m_loans);
  dds_endpoint_remove_psmx_endpoints (&wr->m_endpoint);

  /* FIXME: not freeing WHC here because it is owned by the DDSI entity */
  ddsi_thread_state_awake (ddsi_lookup_thread_state (), &e->m_domain->gv);
  ddsi_xpack_free (wr->m_xp);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  dds_entity_drop_ref (&wr->m_topic->m_entity);
  return ret;
}

static dds_return_t validate_writer_qos (const dds_qos_t *wqos)
{
#ifndef DDS_HAS_LIFESPAN
  if (wqos != NULL && (wqos->present & DDSI_QP_LIFESPAN) && wqos->lifespan.duration != DDS_INFINITY)
    return DDS_RETCODE_BAD_PARAMETER;
#endif
#ifndef DDS_HAS_DEADLINE_MISSED
  if (wqos != NULL && (wqos->present & DDSI_QP_DEADLINE) && wqos->deadline.deadline != DDS_INFINITY)
    return DDS_RETCODE_BAD_PARAMETER;
#endif
#if defined(DDS_HAS_LIFESPAN) && defined(DDS_HAS_DEADLINE_MISSED)
  DDSRT_UNUSED_ARG (wqos);
#endif
  return DDS_RETCODE_OK;
}

static dds_return_t dds_writer_qos_set (dds_entity *e, const dds_qos_t *qos, bool enabled)
{
  /* note: e->m_qos is still the old one to allow for failure here */
  dds_return_t ret;
  if ((ret = validate_writer_qos(qos)) != DDS_RETCODE_OK)
    return ret;
  if (enabled)
  {
    struct ddsi_writer *wr;
    ddsi_thread_state_awake (ddsi_lookup_thread_state (), &e->m_domain->gv);
    if ((wr = ddsi_entidx_lookup_writer_guid (e->m_domain->gv.entity_index, &e->m_guid)) != NULL)
      ddsi_update_writer_qos (wr, qos);
    ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  }
  return DDS_RETCODE_OK;
}

static const struct dds_stat_keyvalue_descriptor dds_writer_statistics_kv[] = {
  { "rexmit_bytes", DDS_STAT_KIND_UINT64 },
  { "throttle_count", DDS_STAT_KIND_UINT32 },
  { "time_throttle", DDS_STAT_KIND_UINT64 },
  { "time_rexmit", DDS_STAT_KIND_UINT64 }
};

static const struct dds_stat_descriptor dds_writer_statistics_desc = {
  .count = sizeof (dds_writer_statistics_kv) / sizeof (dds_writer_statistics_kv[0]),
  .kv = dds_writer_statistics_kv
};

static struct dds_statistics *dds_writer_create_statistics (const struct dds_entity *entity)
{
  return dds_alloc_statistics (entity, &dds_writer_statistics_desc);
}

static void dds_writer_refresh_statistics (const struct dds_entity *entity, struct dds_statistics *stat)
{
  const struct dds_writer *wr = (const struct dds_writer *) entity;
  if (wr->m_wr)
    ddsi_get_writer_stats (wr->m_wr, &stat->kv[0].u.u64, &stat->kv[1].u.u32, &stat->kv[2].u.u64, &stat->kv[3].u.u64);
}

const struct dds_entity_deriver dds_entity_deriver_writer = {
  .interrupt = dds_writer_interrupt,
  .close = dds_writer_close,
  .delete = dds_writer_delete,
  .set_qos = dds_writer_qos_set,
  .validate_status = dds_writer_status_validate,
  .create_statistics = dds_writer_create_statistics,
  .refresh_statistics = dds_writer_refresh_statistics,
  .invoke_cbs_for_pending_events = dds_writer_invoke_cbs_for_pending_events
};


static dds_entity_t dds_create_writer_int (dds_entity_t participant_or_publisher, dds_guid_t *guid, dds_entity_t topic, const dds_qos_t *qos, const dds_listener_t *listener)
{
  dds_return_t rc;
  dds_publisher *pub = NULL;
  dds_topic *tp;
  dds_entity_t publisher;
  struct whc_writer_info *wrinfo;
  bool created_implicit_pub = false;

  {
    dds_entity *p_or_p;
    if ((rc = dds_entity_lock (participant_or_publisher, DDS_KIND_DONTCARE, &p_or_p)) != DDS_RETCODE_OK)
      return rc;
    switch (dds_entity_kind (p_or_p))
    {
      case DDS_KIND_PUBLISHER:
        publisher = participant_or_publisher;
        pub = (dds_publisher *) p_or_p;
        break;
      case DDS_KIND_PARTICIPANT:
        publisher = dds__create_publisher_l ((dds_participant *) p_or_p, true, qos, NULL);
        dds_entity_unlock (p_or_p);
        if ((rc = dds_publisher_lock (publisher, &pub)) < 0)
          return rc;
        created_implicit_pub = true;
        break;
      default:
        dds_entity_unlock (p_or_p);
        return DDS_RETCODE_ILLEGAL_OPERATION;
    }
  }

  if ((rc = dds_topic_pin (topic, &tp)) != DDS_RETCODE_OK)
    goto err_pin_topic;
  assert (tp->m_stype);
  if (dds_entity_participant (&pub->m_entity) != dds_entity_participant (&tp->m_entity))
  {
    rc = DDS_RETCODE_BAD_PARAMETER;
    goto err_pp_mismatch;
  }

  /* Prevent set_qos on the topic until writer has been created and registered: we can't
     allow a TOPIC_DATA change to ccur before the writer has been created because that
     change would then not be published in the discovery/built-in topics.

     Don't keep the participant (which protects the topic's QoS) locked because that
     can cause deadlocks for applications creating a reader/writer from within a
     publication matched listener (whether the restrictions on what one can do in
     listeners are reasonable or not, it used to work so it can be broken arbitrarily). */
  dds_topic_defer_set_qos (tp);

  /* Merge Topic & Publisher qos */
  struct ddsi_domaingv *gv = &pub->m_entity.m_domain->gv;
  dds_qos_t *wqos = dds_create_qos ();
  bool own_wqos = true;
  if (qos)
    ddsi_xqos_mergein_missing (wqos, qos, DDS_WRITER_QOS_MASK);
  if (pub->m_entity.m_qos)
    ddsi_xqos_mergein_missing (wqos, pub->m_entity.m_qos, ~DDSI_QP_ENTITY_NAME);
  if (tp->m_ktopic->qos)
    ddsi_xqos_mergein_missing (wqos, tp->m_ktopic->qos, (DDS_WRITER_QOS_MASK | DDSI_QP_TOPIC_DATA) & ~DDSI_QP_ENTITY_NAME);
  ddsi_xqos_mergein_missing (wqos, &ddsi_default_qos_writer, ~DDSI_QP_DATA_REPRESENTATION);
  dds_apply_entity_naming(wqos, pub->m_entity.m_qos, gv);

  if ((rc = dds_ensure_valid_data_representation (wqos, tp->m_stype->allowed_data_representation, tp->m_stype->data_type_props, DDS_KIND_WRITER)) != DDS_RETCODE_OK)
    goto err_data_repr;
  if ((rc = dds_ensure_valid_psmx_instances (wqos, DDS_PSMX_ENDPOINT_TYPE_WRITER, tp->m_stype, &pub->m_entity.m_domain->psmx_instances)) != DDS_RETCODE_OK)
    goto err_psmx;

  if ((rc = ddsi_xqos_valid (&gv->logconfig, wqos)) < 0 || (rc = validate_writer_qos(wqos)) != DDS_RETCODE_OK)
    goto err_bad_qos;

  assert (wqos->present & DDSI_QP_DATA_REPRESENTATION && wqos->data_representation.value.n > 0);
  dds_data_representation_id_t data_representation = wqos->data_representation.value.ids[0];

  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv);
  const struct ddsi_guid *ppguid = dds_entity_participant_guid (&pub->m_entity);
  struct ddsi_participant *pp = ddsi_entidx_lookup_participant_guid (gv->entity_index, ppguid);
  /* When deleting a participant, the child handles (that include the publisher)
     are removed before removing the DDSI participant. So at this point, within
     the publisher lock, we can assert that the participant exists. */
  assert (pp != NULL);

#ifdef DDS_HAS_SECURITY
  /* Check if DDS Security is enabled */
  if (ddsi_omg_participant_is_secure (pp))
  {
    /* ask to access control security plugin for create writer permissions */
    if (!ddsi_omg_security_check_create_writer (pp, gv->config.domainId, tp->m_name, wqos))
    {
      rc = DDS_RETCODE_NOT_ALLOWED_BY_SECURITY;
      goto err_not_allowed;
    }
  }
#endif

  // configure async mode
  bool async_mode = (wqos->latency_budget.duration > 0);

  /* Create writer */
  struct dds_writer * const wr = dds_alloc (sizeof (*wr));
  const dds_entity_t writer = dds_entity_init (&wr->m_entity, &pub->m_entity, DDS_KIND_WRITER, false, true, wqos, listener, DDS_WRITER_STATUS_MASK);

  // Ownership of rqos is transferred to reader entity
  own_wqos = false;

  wr->m_topic = tp;
  dds_entity_add_ref_locked (&tp->m_entity);
  wr->m_xp = ddsi_xpack_new (gv, async_mode);
  wrinfo = dds_whc_make_wrinfo (wr, wqos);
  wr->m_whc = dds_whc_new (gv, wrinfo);
  rc = dds_loan_pool_create (&wr->m_loans, 0);
  assert(rc == DDS_RETCODE_OK); // FIXME: can be out of resources
  dds_whc_free_wrinfo (wrinfo);
  // We now have the QoS which defaults to "false", but it used to be controlled by a global setting
  // (that most people were sensible enough to leave at false and that this deprecated now).  Or'ing
  // the two together is perhaps a bit simplistic because it doesn't allow you to enable it globally
  // and then disable it for a specific writer.  Should somebody runs into a problem because of this
  // we can have another look.
  wr->whc_batch = wqos->writer_batching.batch_updates || gv->config.whc_batch;
  wr->protocol_version = gv->config.protocol_version;

  if ((rc = dds_endpoint_add_psmx_endpoint (&wr->m_endpoint, wqos, &tp->m_ktopic->psmx_topics, DDS_PSMX_ENDPOINT_TYPE_WRITER)) != DDS_RETCODE_OK)
    goto err_pipe_open;

  struct ddsi_sertype *sertype = ddsi_sertype_derive_sertype (tp->m_stype, data_representation,
    wqos->present & DDSI_QP_TYPE_CONSISTENCY_ENFORCEMENT ? wqos->type_consistency : ddsi_default_qos_topic.type_consistency);
  if (!sertype)
    sertype = tp->m_stype;

  if (guid)
    wr->m_entity.m_guid = dds_guid_to_ddsi_guid (*guid);
  else
  {
    rc = ddsi_generate_writer_guid (&wr->m_entity.m_guid, pp, sertype);
    if (rc != DDS_RETCODE_OK)
      goto err_wr_guid;
  }
  struct ddsi_psmx_locators_set *vl_set = dds_get_psmx_locators_set (wqos, &wr->m_entity.m_domain->psmx_instances);
  rc = ddsi_new_writer (&wr->m_wr, &wr->m_entity.m_guid, NULL, pp, tp->m_name, sertype, wqos, wr->m_whc, dds_writer_status_cb, wr, vl_set);
  if (rc != DDS_RETCODE_OK)
  {
    /* FIXME: can be out-of-resources at the very least; would leak allocated entity id */
    abort ();
  }
  dds_psmx_locators_set_free (vl_set);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());

  wr->m_entity.m_iid = ddsi_get_entity_instanceid (&wr->m_entity.m_domain->gv, &wr->m_entity.m_guid);
  dds_entity_register_child (&pub->m_entity, &wr->m_entity);

  dds_entity_init_complete (&wr->m_entity);

  dds_topic_allow_set_qos (tp);
  dds_topic_unpin (tp);
  dds_publisher_unlock (pub);

  // start async thread if not already started and the latency budget is non zero
  ddsrt_mutex_lock (&gv->sendq_running_lock);
  if (async_mode && !gv->sendq_running) {
    ddsi_xpack_sendq_init(gv);
    ddsi_xpack_sendq_start(gv);
  }

  ddsrt_mutex_unlock (&gv->sendq_running_lock);
  return writer;

err_wr_guid:
err_pipe_open:
#ifdef DDS_HAS_SECURITY
err_not_allowed:
#endif
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
err_bad_qos:
err_data_repr:
err_psmx:
  if (own_wqos)
    dds_delete_qos(wqos);
  dds_topic_allow_set_qos (tp);
err_pp_mismatch:
  dds_topic_unpin (tp);
err_pin_topic:
  dds_publisher_unlock (pub);
  if (created_implicit_pub)
    (void) dds_delete (publisher);
  return rc;
}

dds_entity_t dds_create_writer (dds_entity_t participant_or_publisher, dds_entity_t topic, const dds_qos_t *qos, const dds_listener_t *listener)
{
  return dds_create_writer_int (participant_or_publisher, NULL, topic, qos, listener);
}

dds_entity_t dds_create_writer_guid (dds_entity_t participant_or_publisher, dds_entity_t topic, const dds_qos_t *qos, const dds_listener_t *listener, dds_guid_t *guid)
{
  return dds_create_writer_int (participant_or_publisher, guid, topic, qos, listener);
}

dds_entity_t dds_get_publisher (dds_entity_t writer)
{
  dds_entity *e;
  dds_return_t rc;
  if ((rc = dds_entity_pin (writer, &e)) != DDS_RETCODE_OK)
    return rc;
  else
  {
    dds_entity_t pubh;
    if (dds_entity_kind (e) != DDS_KIND_WRITER)
      pubh = DDS_RETCODE_ILLEGAL_OPERATION;
    else
    {
      assert (dds_entity_kind (e->m_parent) == DDS_KIND_PUBLISHER);
      pubh = e->m_parent->m_hdllink.hdl;
    }
    dds_entity_unpin (e);
    return pubh;
  }
}

dds_return_t dds__ddsi_writer_wait_for_acks (struct dds_writer *wr, ddsi_guid_t *rdguid, dds_time_t abstimeout)
{
  /* during lifetime of the writer m_wr is constant, it is only during deletion that it
     gets erased at some point */
  if (wr->m_wr == NULL)
    return DDS_RETCODE_OK;
  else
    return ddsi_writer_wait_for_acks (wr->m_wr, rdguid, abstimeout);
}

dds_loaned_sample_t *dds_writer_request_psmx_loan(const dds_writer *wr, uint32_t size)
{
  // return the loan from the first endpoint that returns one
  dds_loaned_sample_t *loan = NULL;
  for (uint32_t e = 0; e < wr->m_endpoint.psmx_endpoints.length && loan == NULL; e++)
  {
    struct dds_psmx_endpoint_int const * const ep = wr->m_endpoint.psmx_endpoints.endpoints[e];
    loan = ep->ops.request_loan (ep, size);
  }
  return loan;
}

dds_return_t dds_request_writer_loan (dds_writer *wr, enum dds_writer_loan_type loan_type, uint32_t sz, void **sample)
{
  dds_return_t ret = DDS_RETCODE_ERROR;

  ddsrt_mutex_lock (&wr->m_entity.m_mutex);
  // We don't bother the PSMX interface with types that contain pointers, but we do
  // support the programming model of borrowing memory first via the "heap" loans.
  //
  // One should expect the latter performance to be worse than the a plain write.

  dds_loaned_sample_t *loan = NULL;
  switch (loan_type)
  {
    case DDS_WRITER_LOAN_RAW:
      if (wr->m_endpoint.psmx_endpoints.length > 0)
      {
        if ((loan = dds_writer_request_psmx_loan (wr, sz)) != NULL)
          ret = DDS_RETCODE_OK;
      }
      break;

    case DDS_WRITER_LOAN_REGULAR:
      if (wr->m_endpoint.psmx_endpoints.length > 0 && wr->m_topic->m_stype->is_memcpy_safe)
      {
        if ((loan = dds_writer_request_psmx_loan (wr, wr->m_topic->m_stype->sizeof_type)) != NULL)
          ret = DDS_RETCODE_OK;
      }
      else
        ret = dds_heap_loan (wr->m_topic->m_stype, DDS_LOANED_SAMPLE_STATE_UNITIALIZED, &loan);
      break;
  }

  if (ret == DDS_RETCODE_OK)
  {
    assert (loan != NULL);
    if ((ret = dds_loan_pool_add_loan (wr->m_loans, loan)) != DDS_RETCODE_OK)
      dds_loaned_sample_unref (loan);
    else
      *sample = loan->sample_ptr;
  }
  ddsrt_mutex_unlock (&wr->m_entity.m_mutex);
  return ret;
}

dds_return_t dds_return_writer_loan (dds_writer *wr, void **samples_ptr, int32_t n_samples)
{
  dds_return_t ret = DDS_RETCODE_OK;
  ddsrt_mutex_lock (&wr->m_entity.m_mutex);
  for (int32_t i = 0; i < n_samples && samples_ptr[i] != NULL; i++)
  {
    dds_loaned_sample_t * const loan = dds_loan_pool_find_and_remove_loan (wr->m_loans, samples_ptr[i]);
    if (loan != NULL)
    {
      dds_loaned_sample_unref (loan);
      samples_ptr[i] = NULL;
    }
    else if (i == 0)
    {
      // match reader version of the loan: if first entry is bogus, abort with
      // precondition not met ...
      ret = DDS_RETCODE_PRECONDITION_NOT_MET;
      break;
    }
    else
    {
      // ... if any other entry is bogus, continue releasing loans and return
      // bad parameter
      ret = DDS_RETCODE_BAD_PARAMETER;
    }
  }
  ddsrt_mutex_unlock (&wr->m_entity.m_mutex);
  return ret;
}

struct dds_loaned_sample * dds_writer_psmx_loan_raw (const struct dds_writer *wr, const void *data, enum ddsi_serdata_kind sdkind, dds_time_t timestamp, uint32_t statusinfo)
{
  struct ddsi_sertype const * const sertype = wr->m_wr->type;
  assert (sertype->is_memcpy_safe);
  struct dds_loaned_sample * const loan = dds_writer_request_psmx_loan (wr, sertype->sizeof_type);
  if (loan == NULL)
    return NULL;
  struct dds_psmx_metadata * const md = loan->metadata;
  md->sample_state = (sdkind == SDK_KEY) ? DDS_LOANED_SAMPLE_STATE_RAW_KEY : DDS_LOANED_SAMPLE_STATE_RAW_DATA;
  md->cdr_identifier = DDSI_RTPS_SAMPLE_NATIVE;
  md->cdr_options = 0;
  if (sdkind == SDK_DATA || sertype->has_key)
    memcpy (loan->sample_ptr, data, sertype->sizeof_type);
  dds_psmx_set_loan_writeinfo (loan, &wr->m_entity.m_guid, timestamp, statusinfo);
  return loan;
}

struct dds_loaned_sample * dds_writer_psmx_loan_from_serdata (const struct dds_writer *wr, const struct ddsi_serdata *sd)
{
  assert (ddsi_serdata_size (sd) >= 4);
  const uint32_t loan_size = ddsi_serdata_size (sd) - 4;
  struct dds_loaned_sample * const loan = dds_writer_request_psmx_loan (wr, loan_size);
  if (loan == NULL)
    return NULL;
  struct dds_psmx_metadata * const md = loan->metadata;
  md->sample_state = (sd->kind == SDK_KEY) ? DDS_LOANED_SAMPLE_STATE_SERIALIZED_KEY : DDS_LOANED_SAMPLE_STATE_SERIALIZED_DATA;
  struct { uint16_t identifier, options; } header;
  ddsi_serdata_to_ser (sd, 0, 4, &header);
  md->cdr_identifier = header.identifier;
  md->cdr_options = header.options;
  if (loan_size > 0)
    ddsi_serdata_to_ser (sd, 4, loan_size, loan->sample_ptr);
  dds_psmx_set_loan_writeinfo (loan, &wr->m_entity.m_guid, sd->timestamp.v, sd->statusinfo);
  return loan;
}
