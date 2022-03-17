/*
 * Copyright(c) 2022 ZettaScale Technology
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
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

#include "dds/dds.h"
#include "dds/version.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsi/ddsi_config_impl.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsi/ddsi_cdrstream.h"
#include "dds__writer.h"
#include "dds__listener.h"
#include "dds__init.h"
#include "dds__publisher.h"
#include "dds__topic.h"
#include "dds__get_status.h"
#include "dds__qos.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds__whc.h"
#include "dds__statistics.h"
#include "dds__data_allocator.h"
#include "dds/ddsi/ddsi_statistics.h"

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

static void update_offered_deadline_missed (struct dds_offered_deadline_missed_status * __restrict st, const status_cb_data_t *data)
{
  st->last_instance_handle = data->handle;
  st->total_count++;
  // always incrementing st->total_count_change, then copying into *lst is
  // a bit more than minimal work, but this guarantees the correct value
  // also when enabling a listeners after some events have occurred
  //
  // (same line of reasoning for all of them)
  st->total_count_change++;
}

static void update_offered_incompatible_qos (struct dds_offered_incompatible_qos_status * __restrict st, const status_cb_data_t *data)
{
  st->last_policy_id = data->extra;
  st->total_count++;
  st->total_count_change++;
}

static void update_liveliness_lost (struct dds_liveliness_lost_status * __restrict st, const status_cb_data_t *data)
{
  (void) data;
  st->total_count++;
  st->total_count_change++;
}

static void update_publication_matched (struct dds_publication_matched_status * __restrict st, const status_cb_data_t *data)
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

void dds_writer_status_cb (void *entity, const struct status_cb_data *data)
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

static uint32_t get_bandwidth_limit (dds_transport_priority_qospolicy_t transport_priority)
{
#ifdef DDS_HAS_NETWORK_CHANNELS
  struct ddsi_config_channel_listelem *channel = find_channel (&config, transport_priority);
  return channel->data_bandwidth_limit;
#else
  (void) transport_priority;
  return 0;
#endif
}

static void dds_writer_interrupt (dds_entity *e) ddsrt_nonnull_all;

static void dds_writer_interrupt (dds_entity *e)
{
  struct ddsi_domaingv * const gv = &e->m_domain->gv;
  thread_state_awake (lookup_thread_state (), gv);
  unblock_throttled_writer (gv, &e->m_guid);
  thread_state_asleep (lookup_thread_state ());
}

static void dds_writer_close (dds_entity *e) ddsrt_nonnull_all;

static void dds_writer_close (dds_entity *e)
{
  struct dds_writer * const wr = (struct dds_writer *) e;
  struct ddsi_domaingv * const gv = &e->m_domain->gv;
  struct thread_state1 * const ts1 = lookup_thread_state ();
  thread_state_awake (ts1, gv);
  nn_xpack_send (wr->m_xp, false);
  (void) delete_writer (gv, &e->m_guid);
  thread_state_asleep (ts1);

  ddsrt_mutex_lock (&e->m_mutex);
  while (wr->m_wr != NULL)
    ddsrt_cond_wait (&e->m_cond, &e->m_mutex);
  ddsrt_mutex_unlock (&e->m_mutex);
}

static dds_return_t dds_writer_delete (dds_entity *e) ddsrt_nonnull_all;

static dds_return_t dds_writer_delete (dds_entity *e)
{
  dds_writer * const wr = (dds_writer *) e;
#ifdef DDS_HAS_SHM
  if (wr->m_iox_pub)
  {
    DDS_CLOG(DDS_LC_SHM, &e->m_domain->gv.logconfig, "Release iceoryx's publisher\n");
    iox_pub_stop_offer(wr->m_iox_pub);
    iox_pub_deinit(wr->m_iox_pub);
  }
#endif
  /* FIXME: not freeing WHC here because it is owned by the DDSI entity */
  thread_state_awake (lookup_thread_state (), &e->m_domain->gv);
  nn_xpack_free (wr->m_xp);
  thread_state_asleep (lookup_thread_state ());
  dds_entity_drop_ref (&wr->m_topic->m_entity);
  return DDS_RETCODE_OK;
}

static dds_return_t validate_writer_qos (const dds_qos_t *wqos)
{
#ifndef DDS_HAS_LIFESPAN
  if (wqos != NULL && (wqos->present & QP_LIFESPAN) && wqos->lifespan.duration != DDS_INFINITY)
    return DDS_RETCODE_BAD_PARAMETER;
#endif
#ifndef DDS_HAS_DEADLINE_MISSED
  if (wqos != NULL && (wqos->present & QP_DEADLINE) && wqos->deadline.deadline != DDS_INFINITY)
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
    struct writer *wr;
    thread_state_awake (lookup_thread_state (), &e->m_domain->gv);
    if ((wr = entidx_lookup_writer_guid (e->m_domain->gv.entity_index, &e->m_guid)) != NULL)
      update_writer_qos (wr, qos);
    thread_state_asleep (lookup_thread_state ());
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
  .refresh_statistics = dds_writer_refresh_statistics
};

#ifdef DDS_HAS_SHM
#define DDS_WRITER_QOS_CHECK_FIELDS (QP_LIVELINESS|QP_DEADLINE|QP_RELIABILITY|QP_DURABILITY|QP_HISTORY)
static bool dds_writer_support_shm(const struct ddsi_config* cfg, const dds_qos_t* qos, const struct dds_topic *tp)
{
  if (NULL == cfg ||
      false == cfg->enable_shm)
    return false;

  // check necessary condition: fixed size data type OR serializing into shared
  // memory is available
  if (!tp->m_stype->fixed_size && (!tp->m_stype->ops->get_serialized_size ||
                                   !tp->m_stype->ops->serialize_into)) {
    return false;
  }

  // only VOLATILE or TRANSIENT LOCAL
  if(!(qos->durability.kind == DDS_DURABILITY_VOLATILE ||
    qos->durability.kind == DDS_DURABILITY_TRANSIENT_LOCAL)) {
    return false;
  }

  // only KEEP LAST
  if(qos->history.kind != DDS_HISTORY_KEEP_LAST) {
    return false;
  }
  // we cannot support the required history with iceoryx
  if (qos->durability.kind == DDS_DURABILITY_TRANSIENT_LOCAL &&
      qos->durability_service.history.kind == DDS_HISTORY_KEEP_LAST &&
      qos->durability_service.history.depth >
          (int32_t)iox_cfg_max_publisher_history()) {
    return false;
  }

  return (DDS_WRITER_QOS_CHECK_FIELDS == (qos->present & DDS_WRITER_QOS_CHECK_FIELDS) &&
          DDS_LIVELINESS_AUTOMATIC == qos->liveliness.kind &&
          DDS_INFINITY == qos->deadline.deadline);
}

static iox_pub_options_t create_iox_pub_options(const dds_qos_t* qos) {

  iox_pub_options_t opts;
  iox_pub_options_init(&opts);

  if(qos->durability.kind == DDS_DURABILITY_VOLATILE) {
    opts.historyCapacity = 0;
  } else {
    // Transient Local and stronger
    if (qos->durability_service.history.kind == DDS_HISTORY_KEEP_LAST) {
      opts.historyCapacity = (uint64_t)qos->durability_service.history.depth;
    } else {
      opts.historyCapacity = 0;
    }
  }

  return opts;
}
#endif

dds_entity_t dds_create_writer (dds_entity_t participant_or_publisher, dds_entity_t topic, const dds_qos_t *qos, const dds_listener_t *listener)
{
  dds_return_t rc;
  dds_qos_t *wqos;
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
  wqos = dds_create_qos ();
  if (qos)
    ddsi_xqos_mergein_missing (wqos, qos, DDS_WRITER_QOS_MASK);
  if (pub->m_entity.m_qos)
    ddsi_xqos_mergein_missing (wqos, pub->m_entity.m_qos, ~(uint64_t)0);
  if (tp->m_ktopic->qos)
    ddsi_xqos_mergein_missing (wqos, tp->m_ktopic->qos, ~(uint64_t)0);
  ddsi_xqos_mergein_missing (wqos, &ddsi_default_qos_writer, ~QP_DATA_REPRESENTATION);
  if ((rc = dds_ensure_valid_data_representation (wqos, tp->m_stype->allowed_data_representation, false)) != 0)
    goto err_data_repr;

  if ((rc = ddsi_xqos_valid (&gv->logconfig, wqos)) < 0 || (rc = validate_writer_qos(wqos)) != DDS_RETCODE_OK)
    goto err_bad_qos;

  assert (wqos->present & QP_DATA_REPRESENTATION && wqos->data_representation.value.n > 0);
  dds_data_representation_id_t data_representation = wqos->data_representation.value.ids[0];

  thread_state_awake (lookup_thread_state (), gv);
  const struct ddsi_guid *ppguid = dds_entity_participant_guid (&pub->m_entity);
  struct participant *pp = entidx_lookup_participant_guid (gv->entity_index, ppguid);
  /* When deleting a participant, the child handles (that include the publisher)
     are removed before removing the DDSI participant. So at this point, within
     the publisher lock, we can assert that the participant exists. */
  assert (pp != NULL);

#ifdef DDS_HAS_SECURITY
  /* Check if DDS Security is enabled */
  if (q_omg_participant_is_secure (pp))
  {
    /* ask to access control security plugin for create writer permissions */
    if (!q_omg_security_check_create_writer (pp, gv->config.domainId, tp->m_name, wqos))
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
  wr->m_topic = tp;
  dds_entity_add_ref_locked (&tp->m_entity);
  wr->m_xp = nn_xpack_new (gv, get_bandwidth_limit (wqos->transport_priority), async_mode);
  wrinfo = whc_make_wrinfo (wr, wqos);
  wr->m_whc = whc_new (gv, wrinfo);
  whc_free_wrinfo (wrinfo);
  wr->whc_batch = gv->config.whc_batch;

#ifdef DDS_HAS_SHM
  assert(wqos->present & QP_LOCATOR_MASK);
  if (!dds_writer_support_shm(&gv->config, wqos, tp))
    wqos->ignore_locator_type |= NN_LOCATOR_KIND_SHEM;
#endif

  struct ddsi_sertype *sertype = ddsi_sertype_derive_sertype (tp->m_stype, data_representation,
    wqos->present & QP_TYPE_CONSISTENCY_ENFORCEMENT ? wqos->type_consistency : ddsi_default_qos_topic.type_consistency);
  if (!sertype)
    sertype = tp->m_stype;

  rc = new_writer (&wr->m_wr, &wr->m_entity.m_guid, NULL, pp, tp->m_name, sertype, wqos, wr->m_whc, dds_writer_status_cb, wr);
  assert(rc == DDS_RETCODE_OK);
  thread_state_asleep (lookup_thread_state ());

#ifdef DDS_HAS_SHM
  if (0x0 == (wqos->ignore_locator_type & NN_LOCATOR_KIND_SHEM))
  {
    DDS_CLOG (DDS_LC_SHM, &wr->m_entity.m_domain->gv.logconfig, "Writer's topic name will be DDS:Cyclone:%s\n", wr->m_topic->m_name);
    iox_pub_options_t opts = create_iox_pub_options(wqos);

    // NB: This may fail due to icoeryx being out of internal resources for publishers
    //     In this case terminate is called by iox_pub_init.
    //     it is currently (iceoryx 2.0 and lower) not possible to change this to
    //     e.g. return a nullptr and handle the error here.   
    wr->m_iox_pub = iox_pub_init(&(iox_pub_storage_t){0}, gv->config.iceoryx_service, wr->m_topic->m_stype->type_name, wr->m_topic->m_name, &opts);
    memset(wr->m_iox_pub_loans, 0, sizeof(wr->m_iox_pub_loans));
    dds_sleepfor(DDS_MSECS(10));
  }
#endif

  wr->m_entity.m_iid = get_entity_instance_id (&wr->m_entity.m_domain->gv, &wr->m_entity.m_guid);
  dds_entity_register_child (&pub->m_entity, &wr->m_entity);

  dds_entity_init_complete (&wr->m_entity);

  dds_topic_allow_set_qos (tp);
  dds_topic_unpin (tp);
  dds_publisher_unlock (pub);

  // start async thread if not already started and the latency budget is non zero
  ddsrt_mutex_lock (&gv->sendq_running_lock);
  if (async_mode && !gv->sendq_running) {
    nn_xpack_sendq_init(gv);
    nn_xpack_sendq_start(gv);
  }
  ddsrt_mutex_unlock (&gv->sendq_running_lock);
  return writer;

#ifdef DDS_HAS_SECURITY
err_not_allowed:
  thread_state_asleep (lookup_thread_state ());
#endif
err_bad_qos:
err_data_repr:
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

dds_return_t dds__writer_data_allocator_init (const dds_writer *wr, dds_data_allocator_t *data_allocator)
{
#ifdef DDS_HAS_SHM
  dds_iox_allocator_t *d = (dds_iox_allocator_t *) data_allocator->opaque.bytes;
  ddsrt_mutex_init(&d->mutex);
  if (NULL != wr->m_iox_pub)
  {
    d->kind = DDS_IOX_ALLOCATOR_KIND_PUBLISHER;
    d->ref.pub = wr->m_iox_pub;
  }
  else
  {
    d->kind = DDS_IOX_ALLOCATOR_KIND_NONE;
  }
  return DDS_RETCODE_OK;
#else
  (void) wr;
  (void) data_allocator;
  return DDS_RETCODE_OK;
#endif
}

dds_return_t dds__writer_data_allocator_fini (const dds_writer *wr, dds_data_allocator_t *data_allocator)
{
#ifdef DDS_HAS_SHM
  dds_iox_allocator_t *d = (dds_iox_allocator_t *) data_allocator->opaque.bytes;
  ddsrt_mutex_destroy(&d->mutex);
  d->kind = DDS_IOX_ALLOCATOR_KIND_FINI;
#else
  (void) data_allocator;
#endif
  (void) wr;
  return DDS_RETCODE_OK;
}

dds_return_t dds__writer_wait_for_acks (struct dds_writer *wr, ddsi_guid_t *rdguid, dds_time_t abstimeout)
{
  /* during lifetime of the writer m_wr is constant, it is only during deletion that it
     gets erased at some point */
  if (wr->m_wr == NULL)
    return DDS_RETCODE_OK;
  else
    return writer_wait_for_acks (wr->m_wr, rdguid, abstimeout);
}
