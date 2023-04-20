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
#include "dds__participant.h"
#include "dds__subscriber.h"
#include "dds__reader.h"
#include "dds__listener.h"
#include "dds__init.h"
#include "dds/ddsc/dds_rhc.h"
#include "dds__rhc_default.h"
#include "dds__topic.h"
#include "dds__get_status.h"
#include "dds__qos.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/ddsi/ddsi_thread.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds__builtin.h"
#include "dds__statistics.h"
#include "dds__data_allocator.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsi/ddsi_statistics.h"
#include "dds/ddsi/ddsi_endpoint_match.h"

#ifdef DDS_HAS_SHM
#include "dds/ddsi/ddsi_shm_transport.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/threads.h"
#include "iceoryx_binding_c/wait_set.h"
#include "dds__shm_monitor.h"
#include "dds__shm_qos.h"
#endif

DECL_ENTITY_LOCK_UNLOCK (dds_reader)

#define DDS_READER_STATUS_MASK                                   \
                        (DDS_SAMPLE_REJECTED_STATUS              |\
                         DDS_LIVELINESS_CHANGED_STATUS           |\
                         DDS_REQUESTED_DEADLINE_MISSED_STATUS    |\
                         DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS   |\
                         DDS_DATA_AVAILABLE_STATUS               |\
                         DDS_SAMPLE_LOST_STATUS                  |\
                         DDS_SUBSCRIPTION_MATCHED_STATUS)

static void dds_reader_close (dds_entity *e) ddsrt_nonnull_all;

static void dds_reader_close (dds_entity *e)
{
  struct dds_reader * const rd = (struct dds_reader *) e;
  assert (rd->m_rd != NULL);

#ifdef DDS_HAS_SHM
  if (rd->m_iox_sub)
  {
  //will wait for any runing callback using the iceoryx subscriber of this reader
    dds_shm_monitor_detach_reader(&rd->m_entity.m_domain->m_shm_monitor, rd);
  //from now on no callbacks on this reader will run
  }
#endif

  ddsi_thread_state_awake (ddsi_lookup_thread_state (), &e->m_domain->gv);
  (void) ddsi_delete_reader (&e->m_domain->gv, &e->m_guid);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());

  ddsrt_mutex_lock (&e->m_mutex);
  while (rd->m_rd != NULL)
    ddsrt_cond_wait (&e->m_cond, &e->m_mutex);
  ddsrt_mutex_unlock (&e->m_mutex);
}

static dds_return_t dds_reader_delete (dds_entity *e) ddsrt_nonnull_all;

static dds_return_t dds_reader_delete (dds_entity *e)
{
  dds_reader * const rd = (dds_reader *) e;

  if (rd->m_loan)
  {
    void **ptrs = ddsrt_malloc (rd->m_loan_size * sizeof (*ptrs));
    ddsi_sertype_realloc_samples (ptrs, rd->m_topic->m_stype, rd->m_loan, rd->m_loan_size, rd->m_loan_size);
    ddsi_sertype_free_samples (rd->m_topic->m_stype, ptrs, rd->m_loan_size, DDS_FREE_ALL);
    ddsrt_free (ptrs);
  }

  ddsi_thread_state_awake (ddsi_lookup_thread_state (), &e->m_domain->gv);
  dds_rhc_free (rd->m_rhc);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());

#ifdef DDS_HAS_SHM
  if (rd->m_iox_sub)
  {
    // deletion must happen at the very end after the reader cache is not used anymore
    // since the mutex is needed and the data needs to be released using the iceoryx subscriber
    DDS_CLOG (DDS_LC_SHM, &e->m_domain->gv.logconfig, "Release iceoryx's subscriber\n");
    iox_sub_deinit(rd->m_iox_sub);
    iox_sub_context_fini(&rd->m_iox_sub_context);
  }
#endif

  dds_entity_drop_ref (&rd->m_topic->m_entity);
  return DDS_RETCODE_OK;
}

static dds_return_t validate_reader_qos (const dds_qos_t *rqos)
{
#ifndef DDS_HAS_DEADLINE_MISSED
  if (rqos != NULL && (rqos->present & DDSI_QP_DEADLINE) && rqos->deadline.deadline != DDS_INFINITY)
    return DDS_RETCODE_BAD_PARAMETER;
#else
  DDSRT_UNUSED_ARG (rqos);
#endif
  return DDS_RETCODE_OK;
}

static dds_return_t dds_reader_qos_set (dds_entity *e, const dds_qos_t *qos, bool enabled)
{
  /* note: e->m_qos is still the old one to allow for failure here */
  dds_return_t ret;
  if ((ret = validate_reader_qos(qos)) != DDS_RETCODE_OK)
    return ret;
  if (enabled)
  {
    struct ddsi_reader *rd;
    ddsi_thread_state_awake (ddsi_lookup_thread_state (), &e->m_domain->gv);
    if ((rd = ddsi_entidx_lookup_reader_guid (e->m_domain->gv.entity_index, &e->m_guid)) != NULL)
      ddsi_update_reader_qos (rd, qos);
    ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  }
  return DDS_RETCODE_OK;
}

static dds_return_t dds_reader_status_validate (uint32_t mask)
{
  return (mask & ~DDS_READER_STATUS_MASK) ? DDS_RETCODE_BAD_PARAMETER : DDS_RETCODE_OK;
}

static void data_avail_cb_enter_listener_exclusive_access (dds_entity *e)
{
  // assumes e->m_observers_lock held on entry
  // possibly unlocks and relocks e->m_observers_lock
  // afterward e->m_listener is stable
  e->m_cb_pending_count++;
  while (e->m_cb_count > 0)
    ddsrt_cond_wait (&e->m_observers_cond, &e->m_observers_lock);
  e->m_cb_count++;
}

static void data_avail_cb_leave_listener_exclusive_access (dds_entity *e)
{
  // assumes e->m_observers_lock held on entry
  e->m_cb_count--;
  e->m_cb_pending_count--;
  ddsrt_cond_broadcast (&e->m_observers_cond);
}

static void data_avail_cb_invoke_dor (dds_entity *sub, const struct dds_listener *lst)
{
  // assumes sub->m_observers_lock held on entry
  // unlocks and relocks sub->m_observers_lock
  data_avail_cb_enter_listener_exclusive_access (sub);
  ddsrt_mutex_unlock (&sub->m_observers_lock);
  lst->on_data_on_readers (sub->m_hdllink.hdl, lst->on_data_on_readers_arg);
  ddsrt_mutex_lock (&sub->m_observers_lock);
  data_avail_cb_leave_listener_exclusive_access (sub);
}

static uint32_t data_avail_cb_set_status (dds_entity *rd, uint32_t status_and_mask)
{
  uint32_t ret = 0;
  if (dds_entity_status_set (rd, DDS_DATA_AVAILABLE_STATUS))
    ret |= DDS_DATA_AVAILABLE_STATUS;
  if (status_and_mask & (DDS_DATA_ON_READERS_STATUS << SAM_ENABLED_SHIFT))
  {
    if (dds_entity_status_set (rd->m_parent, DDS_DATA_ON_READERS_STATUS))
      ret |= DDS_DATA_ON_READERS_STATUS;
  }
  return ret;
}

static void data_avail_cb_trigger_waitsets (dds_entity *rd, uint32_t signal)
{
  if (signal == 0)
    return;

  if (signal & DDS_DATA_ON_READERS_STATUS)
  {
    dds_entity * const sub = rd->m_parent;
    ddsrt_mutex_lock (&sub->m_observers_lock);
    const uint32_t sm = ddsrt_atomic_ld32 (&sub->m_status.m_status_and_mask);
    if ((sm & (sm >> SAM_ENABLED_SHIFT)) & DDS_DATA_ON_READERS_STATUS)
      dds_entity_observers_signal (sub, DDS_DATA_ON_READERS_STATUS);
    ddsrt_mutex_unlock (&sub->m_observers_lock);
  }
  if (signal & DDS_DATA_AVAILABLE_STATUS)
  {
    const uint32_t sm = ddsrt_atomic_ld32 (&rd->m_status.m_status_and_mask);
    if ((sm & (sm >> SAM_ENABLED_SHIFT)) & DDS_DATA_AVAILABLE_STATUS)
      dds_entity_observers_signal (rd, DDS_DATA_AVAILABLE_STATUS);
  }
}

void dds_reader_data_available_cb (struct dds_reader *rd)
{
  /* DATA_AVAILABLE is special in two ways: firstly, it should first try
     DATA_ON_READERS on the line of ancestors, and if not consumed set the
     status on the subscriber; secondly it is the only one for which
     overhead really matters.  Otherwise, it is pretty much like
     dds_reader_status_cb. */
  struct dds_listener const * const lst = &rd->m_entity.m_listener;
  uint32_t signal = 0;

  ddsrt_mutex_lock (&rd->m_entity.m_observers_lock);
  const uint32_t status_and_mask = ddsrt_atomic_ld32 (&rd->m_entity.m_status.m_status_and_mask);
  if (lst->on_data_on_readers == 0 && lst->on_data_available == 0)
    signal = data_avail_cb_set_status (&rd->m_entity, status_and_mask);
  else
  {
    // "lock" listener object so we can look at "lst" without holding m_observers_lock
    data_avail_cb_enter_listener_exclusive_access (&rd->m_entity);
    if (lst->on_data_on_readers)
    {
      dds_entity * const sub = rd->m_entity.m_parent;
      ddsrt_mutex_unlock (&rd->m_entity.m_observers_lock);
      ddsrt_mutex_lock (&sub->m_observers_lock);
      if (!(lst->reset_on_invoke & DDS_DATA_ON_READERS_STATUS))
        signal = data_avail_cb_set_status (&rd->m_entity, status_and_mask);
      data_avail_cb_invoke_dor (sub, lst);
      ddsrt_mutex_unlock (&sub->m_observers_lock);
      ddsrt_mutex_lock (&rd->m_entity.m_observers_lock);
    }
    else
    {
      assert (rd->m_entity.m_listener.on_data_available);
      if (!(lst->reset_on_invoke & DDS_DATA_AVAILABLE_STATUS))
        signal = data_avail_cb_set_status (&rd->m_entity, status_and_mask);
      ddsrt_mutex_unlock (&rd->m_entity.m_observers_lock);
      lst->on_data_available (rd->m_entity.m_hdllink.hdl, lst->on_data_available_arg);
      ddsrt_mutex_lock (&rd->m_entity.m_observers_lock);
    }
    data_avail_cb_leave_listener_exclusive_access (&rd->m_entity);
  }
  data_avail_cb_trigger_waitsets (&rd->m_entity, signal);
  ddsrt_mutex_unlock (&rd->m_entity.m_observers_lock);
}

static void update_requested_deadline_missed (struct dds_requested_deadline_missed_status * __restrict st, const ddsi_status_cb_data_t *data)
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

static void update_requested_incompatible_qos (struct dds_requested_incompatible_qos_status * __restrict st, const ddsi_status_cb_data_t *data)
{
  st->last_policy_id = data->extra;
  st->total_count++;
  st->total_count_change++;
}

static void update_sample_lost (struct dds_sample_lost_status * __restrict st, const ddsi_status_cb_data_t *data)
{
  (void) data;
  st->total_count++;
  st->total_count_change++;
}

static void update_sample_rejected (struct dds_sample_rejected_status * __restrict st, const ddsi_status_cb_data_t *data)
{
  st->last_reason = data->extra;
  st->last_instance_handle = data->handle;
  st->total_count++;
  st->total_count_change++;
}

static void update_liveliness_changed (struct dds_liveliness_changed_status * __restrict st, const ddsi_status_cb_data_t *data)
{
  DDSRT_STATIC_ASSERT ((uint32_t) DDSI_LIVELINESS_CHANGED_ADD_ALIVE == 0 &&
                       DDSI_LIVELINESS_CHANGED_ADD_ALIVE < DDSI_LIVELINESS_CHANGED_ADD_NOT_ALIVE &&
                       DDSI_LIVELINESS_CHANGED_ADD_NOT_ALIVE < DDSI_LIVELINESS_CHANGED_REMOVE_NOT_ALIVE &&
                       DDSI_LIVELINESS_CHANGED_REMOVE_NOT_ALIVE < DDSI_LIVELINESS_CHANGED_REMOVE_ALIVE &&
                       DDSI_LIVELINESS_CHANGED_REMOVE_ALIVE < DDSI_LIVELINESS_CHANGED_ALIVE_TO_NOT_ALIVE &&
                       DDSI_LIVELINESS_CHANGED_ALIVE_TO_NOT_ALIVE < DDSI_LIVELINESS_CHANGED_NOT_ALIVE_TO_ALIVE &&
                       (uint32_t) DDSI_LIVELINESS_CHANGED_NOT_ALIVE_TO_ALIVE < UINT32_MAX);
  assert (data->extra <= (uint32_t) DDSI_LIVELINESS_CHANGED_NOT_ALIVE_TO_ALIVE);
  st->last_publication_handle = data->handle;
  switch ((enum ddsi_liveliness_changed_data_extra) data->extra)
  {
    case DDSI_LIVELINESS_CHANGED_ADD_ALIVE:
      st->alive_count++;
      st->alive_count_change++;
      break;
    case DDSI_LIVELINESS_CHANGED_ADD_NOT_ALIVE:
      st->not_alive_count++;
      st->not_alive_count_change++;
      break;
    case DDSI_LIVELINESS_CHANGED_REMOVE_NOT_ALIVE:
      st->not_alive_count--;
      st->not_alive_count_change--;
      break;
    case DDSI_LIVELINESS_CHANGED_REMOVE_ALIVE:
      st->alive_count--;
      st->alive_count_change--;
      break;
    case DDSI_LIVELINESS_CHANGED_ALIVE_TO_NOT_ALIVE:
      st->alive_count--;
      st->alive_count_change--;
      st->not_alive_count++;
      st->not_alive_count_change++;
      break;
    case DDSI_LIVELINESS_CHANGED_NOT_ALIVE_TO_ALIVE:
      st->not_alive_count--;
      st->not_alive_count_change--;
      st->alive_count++;
      st->alive_count_change++;
      break;
  }
}

static void update_subscription_matched (struct dds_subscription_matched_status * __restrict st, const ddsi_status_cb_data_t *data)
{
  st->last_publication_handle = data->handle;
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

/* Reset sets everything (type) 0, including the reason field, verify that 0 is correct */
DDSRT_STATIC_ASSERT ((int) DDS_NOT_REJECTED == 0);

DDS_GET_STATUS (reader, subscription_matched,       SUBSCRIPTION_MATCHED,       total_count_change, current_count_change)
DDS_GET_STATUS (reader, liveliness_changed,         LIVELINESS_CHANGED,         alive_count_change, not_alive_count_change)
DDS_GET_STATUS (reader, sample_rejected,            SAMPLE_REJECTED,            total_count_change)
DDS_GET_STATUS (reader, sample_lost,                SAMPLE_LOST,                total_count_change)
DDS_GET_STATUS (reader, requested_deadline_missed,  REQUESTED_DEADLINE_MISSED,  total_count_change)
DDS_GET_STATUS (reader, requested_incompatible_qos, REQUESTED_INCOMPATIBLE_QOS, total_count_change)

STATUS_CB_IMPL (reader, subscription_matched,       SUBSCRIPTION_MATCHED,       total_count_change, current_count_change)
STATUS_CB_IMPL (reader, liveliness_changed,         LIVELINESS_CHANGED,         alive_count_change, not_alive_count_change)
STATUS_CB_IMPL (reader, sample_rejected,            SAMPLE_REJECTED,            total_count_change)
STATUS_CB_IMPL (reader, sample_lost,                SAMPLE_LOST,                total_count_change)
STATUS_CB_IMPL (reader, requested_deadline_missed,  REQUESTED_DEADLINE_MISSED,  total_count_change)
STATUS_CB_IMPL (reader, requested_incompatible_qos, REQUESTED_INCOMPATIBLE_QOS, total_count_change)

void dds_reader_status_cb (void *ventity, const ddsi_status_cb_data_t *data)
{
  dds_reader * const rd = ventity;

  /* When data is NULL, it means that the DDSI reader is deleted. */
  if (data == NULL)
  {
    /* Release the initial claim that was done during the create. This
     * will indicate that further API deletion is now possible. */
    ddsrt_mutex_lock (&rd->m_entity.m_mutex);
    rd->m_rd = NULL;
    ddsrt_cond_broadcast (&rd->m_entity.m_cond);
    ddsrt_mutex_unlock (&rd->m_entity.m_mutex);
    return;
  }

  /* Serialize listener invocations -- it is somewhat sad to do this,
     but then it may also be unreasonable to expect the application to
     handle concurrent invocations of a single listener.  The benefit
     here is that it means the counters and "change" counters
     can safely be incremented and/or reset while releasing
     m_observers_lock for the duration of the listener call itself,
     and that similarly the listener function and argument pointers
     are stable */
  /* FIXME: why do this if no listener is set? */
  ddsrt_mutex_lock (&rd->m_entity.m_observers_lock);
  rd->m_entity.m_cb_pending_count++;
  while (rd->m_entity.m_cb_count > 0)
    ddsrt_cond_wait (&rd->m_entity.m_observers_cond, &rd->m_entity.m_observers_lock);
  rd->m_entity.m_cb_count++;

  const enum dds_status_id status_id = (enum dds_status_id) data->raw_status_id;
  switch (status_id)
  {
    case DDS_REQUESTED_DEADLINE_MISSED_STATUS_ID:
      status_cb_requested_deadline_missed (rd, data);
      break;
    case DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS_ID:
      status_cb_requested_incompatible_qos (rd, data);
      break;
    case DDS_SAMPLE_LOST_STATUS_ID:
      status_cb_sample_lost (rd, data);
      break;
    case DDS_SAMPLE_REJECTED_STATUS_ID:
      status_cb_sample_rejected (rd, data);
      break;
    case DDS_LIVELINESS_CHANGED_STATUS_ID:
      status_cb_liveliness_changed (rd, data);
      break;
    case DDS_SUBSCRIPTION_MATCHED_STATUS_ID:
      status_cb_subscription_matched (rd, data);
      break;
    case DDS_DATA_ON_READERS_STATUS_ID:
    case DDS_DATA_AVAILABLE_STATUS_ID:
    case DDS_INCONSISTENT_TOPIC_STATUS_ID:
    case DDS_LIVELINESS_LOST_STATUS_ID:
    case DDS_PUBLICATION_MATCHED_STATUS_ID:
    case DDS_OFFERED_DEADLINE_MISSED_STATUS_ID:
    case DDS_OFFERED_INCOMPATIBLE_QOS_STATUS_ID:
      assert (0);
  }

  rd->m_entity.m_cb_count--;
  rd->m_entity.m_cb_pending_count--;
  ddsrt_cond_broadcast (&rd->m_entity.m_observers_cond);
  ddsrt_mutex_unlock (&rd->m_entity.m_observers_lock);
}

static const struct dds_stat_keyvalue_descriptor dds_reader_statistics_kv[] = {
  { "discarded_bytes", DDS_STAT_KIND_UINT64 }
};

static const struct dds_stat_descriptor dds_reader_statistics_desc = {
  .count = sizeof (dds_reader_statistics_kv) / sizeof (dds_reader_statistics_kv[0]),
  .kv = dds_reader_statistics_kv
};

static struct dds_statistics *dds_reader_create_statistics (const struct dds_entity *entity)
{
  return dds_alloc_statistics (entity, &dds_reader_statistics_desc);
}

static void dds_reader_refresh_statistics (const struct dds_entity *entity, struct dds_statistics *stat)
{
  const struct dds_reader *rd = (const struct dds_reader *) entity;
  if (rd->m_rd)
    ddsi_get_reader_stats (rd->m_rd, &stat->kv[0].u.u64);
}

const struct dds_entity_deriver dds_entity_deriver_reader = {
  .interrupt = dds_entity_deriver_dummy_interrupt,
  .close = dds_reader_close,
  .delete = dds_reader_delete,
  .set_qos = dds_reader_qos_set,
  .validate_status = dds_reader_status_validate,
  .create_statistics = dds_reader_create_statistics,
  .refresh_statistics = dds_reader_refresh_statistics
};

#ifdef DDS_HAS_SHM
static iox_sub_options_t create_iox_sub_options(const dds_qos_t* qos) {

  iox_sub_options_t opts;
  iox_sub_options_init(&opts);

  const uint32_t max_sub_queue_capacity = iox_cfg_max_subscriber_queue_capacity();

  // NB: We may lose data after history.depth many samples are received (if we
  // are not taking them fast enough from the iceoryx queue and move them in
  // the reader history cache), but this is valid behavior for volatile.
  // It may still lead to undesired behavior as the queues are filled very
  // fast if data is published as fast as possible.
  // NB: If the history depth is larger than the queue capacity, we still use
  // shared memory but limit the queueCapacity accordingly (otherwise iceoryx
  // emits a warning and limits it itself)

  if ((uint32_t) qos->history.depth <= max_sub_queue_capacity) {
    opts.queueCapacity = (uint64_t)qos->history.depth;
  } else {
    opts.queueCapacity = max_sub_queue_capacity;
  }

  // with BEST EFFORT DDS requires that no historical
  // data is received (regardless of durability)
  if(qos->reliability.kind == DDS_RELIABILITY_BEST_EFFORT ||
     qos->durability.kind == DDS_DURABILITY_VOLATILE) {
    opts.historyRequest = 0;
  } else {
    // TRANSIENT LOCAL and stronger
    opts.historyRequest = (uint64_t) qos->history.depth;
    // if the publisher does not support historicial data
    // it will not be connected by iceoryx
    opts.requirePublisherHistorySupport = true;
  }

  return opts;
}
#endif

static dds_entity_t dds_create_reader_int (dds_entity_t participant_or_subscriber, dds_entity_t topic, const dds_qos_t *qos, const dds_listener_t *listener, struct dds_rhc *rhc)
{
  dds_qos_t *rqos;
  dds_subscriber *sub = NULL;
  dds_entity_t subscriber;
  dds_topic *tp;
  dds_return_t rc;
  dds_entity_t pseudo_topic = 0;
  bool created_implicit_sub = false;

  switch (topic)
  {
    case DDS_BUILTIN_TOPIC_DCPSTOPIC:
#ifndef DDS_HAS_TOPIC_DISCOVERY
      return DDS_RETCODE_UNSUPPORTED;
#endif
    case DDS_BUILTIN_TOPIC_DCPSPARTICIPANT:
    case DDS_BUILTIN_TOPIC_DCPSPUBLICATION:
    case DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION:
      /* translate provided pseudo-topic to a real one */
      pseudo_topic = topic;
      if ((subscriber = dds__get_builtin_subscriber (participant_or_subscriber)) < 0)
        return subscriber;
      if ((rc = dds_subscriber_lock (subscriber, &sub)) != DDS_RETCODE_OK)
        return rc;
      topic = dds__get_builtin_topic (subscriber, topic);
      break;

    default: {
      dds_entity *p_or_s;
      if ((rc = dds_entity_lock (participant_or_subscriber, DDS_KIND_DONTCARE, &p_or_s)) != DDS_RETCODE_OK)
        return rc;
      switch (dds_entity_kind (p_or_s))
      {
        case DDS_KIND_SUBSCRIBER:
          subscriber = participant_or_subscriber;
          sub = (dds_subscriber *) p_or_s;
          break;
        case DDS_KIND_PARTICIPANT:
          created_implicit_sub = true;
          subscriber = dds__create_subscriber_l ((dds_participant *) p_or_s, true, qos, NULL);
          dds_entity_unlock (p_or_s);
          if ((rc = dds_subscriber_lock (subscriber, &sub)) < 0)
            return rc;
          break;
        default:
          dds_entity_unlock (p_or_s);
          return DDS_RETCODE_ILLEGAL_OPERATION;
      }
      break;
    }
  }

  /* If pseudo_topic != 0, topic didn't didn't originate from the application and we allow pinning
     it despite it being marked as NO_USER_ACCESS */
  if ((rc = dds_topic_pin_with_origin (topic, pseudo_topic ? false : true, &tp)) < 0)
    goto err_pin_topic;
  assert (tp->m_stype);
  if (dds_entity_participant (&sub->m_entity) != dds_entity_participant (&tp->m_entity))
  {
    rc = DDS_RETCODE_BAD_PARAMETER;
    goto err_pp_mismatch;
  }

  /* Prevent set_qos on the topic until reader has been created and registered: we can't
     allow a TOPIC_DATA change to ccur before the reader has been created because that
     change would then not be published in the discovery/built-in topics.

     Don't keep the participant (which protects the topic's QoS) locked because that
     can cause deadlocks for applications creating a reader/writer from within a
     subscription matched listener (whether the restrictions on what one can do in
     listeners are reasonable or not, it used to work so it can be broken arbitrarily). */
  dds_topic_defer_set_qos (tp);

  /* Merge qos from topic and subscriber, dds_copy_qos only fails when it is passed a null
     argument, but that isn't the case here */
  struct ddsi_domaingv *gv = &sub->m_entity.m_domain->gv;
  rqos = dds_create_qos ();
  if (qos)
    ddsi_xqos_mergein_missing (rqos, qos, DDS_READER_QOS_MASK);
  if (sub->m_entity.m_qos)
    ddsi_xqos_mergein_missing (rqos, sub->m_entity.m_qos, ~DDSI_QP_ENTITY_NAME);
  if (tp->m_ktopic->qos)
    ddsi_xqos_mergein_missing (rqos, tp->m_ktopic->qos, (DDS_READER_QOS_MASK | DDSI_QP_TOPIC_DATA) & ~DDSI_QP_ENTITY_NAME);
  ddsi_xqos_mergein_missing (rqos, &ddsi_default_qos_reader, ~DDSI_QP_DATA_REPRESENTATION);
  dds_apply_entity_naming(rqos, sub->m_entity.m_qos, gv);

  if ((rc = dds_ensure_valid_data_representation (rqos, tp->m_stype->allowed_data_representation, false)) != 0)
    goto err_data_repr;

  if ((rc = ddsi_xqos_valid (&gv->logconfig, rqos)) < 0 || (rc = validate_reader_qos(rqos)) != DDS_RETCODE_OK)
    goto err_bad_qos;

  /* Additional checks required for built-in topics: we don't want to
     run into a resource limit on a built-in topic, it is a needless
     complication */
  if (pseudo_topic && !dds__validate_builtin_reader_qos (tp->m_entity.m_domain, pseudo_topic, rqos))
  {
    rc = DDS_RETCODE_INCONSISTENT_POLICY;
    goto err_bad_qos;
  }

  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv);
  const struct ddsi_guid * ppguid = dds_entity_participant_guid (&sub->m_entity);
  struct ddsi_participant * pp = ddsi_entidx_lookup_participant_guid (gv->entity_index, ppguid);

  /* When deleting a participant, the child handles (that include the subscriber)
     are removed before removing the DDSI participant. So at this point, within
     the subscriber lock, we can assert that the participant exists. */
  assert (pp != NULL);

#ifdef DDS_HAS_SECURITY
  /* Check if DDS Security is enabled */
  if (ddsi_omg_participant_is_secure (pp))
  {
    /* ask to access control security plugin for create reader permissions */
    if (!ddsi_omg_security_check_create_reader (pp, gv->config.domainId, tp->m_name, rqos))
    {
      rc = DDS_RETCODE_NOT_ALLOWED_BY_SECURITY;
      ddsi_thread_state_asleep(ddsi_lookup_thread_state());
      goto err_bad_qos;
    }
  }
#endif

  /* Create reader and associated read cache (if not provided by caller) */
  struct dds_reader * const rd = dds_alloc (sizeof (*rd));
  const dds_entity_t reader = dds_entity_init (&rd->m_entity, &sub->m_entity, DDS_KIND_READER, false, true, rqos, listener, DDS_READER_STATUS_MASK);
  // assume DATA_ON_READERS is materialized in the subscriber:
  // - changes to it won't be propagated to this reader until after it has been added to the subscriber's children
  // - data can arrive once `new_reader` is called, requiring raising DATA_ON_READERS if materialized
  // - setting DATA_ON_READERS on subscriber if it is not actually materialized is no problem
  ddsrt_atomic_or32 (&rd->m_entity.m_status.m_status_and_mask, DDS_DATA_ON_READERS_STATUS << SAM_ENABLED_SHIFT);
  rd->m_sample_rejected_status.last_reason = DDS_NOT_REJECTED;
  rd->m_topic = tp;
  rd->m_rhc = rhc ? rhc : dds_rhc_default_new (rd, tp->m_stype);
  if (dds_rhc_associate (rd->m_rhc, rd, tp->m_stype, rd->m_entity.m_domain->gv.m_tkmap) < 0)
  {
    /* FIXME: see also create_querycond, need to be able to undo entity_init */
    abort ();
  }
  dds_entity_add_ref_locked (&tp->m_entity);

  /* FIXME: listeners can come too soon ... should set mask based on listeners
     then atomically set the listeners, save the mask to a pending set and clear
     it; and then invoke those listeners that are in the pending set */
  dds_entity_init_complete (&rd->m_entity);

#ifdef DDS_HAS_SHM
  assert(rqos->present & DDSI_QP_LOCATOR_MASK);
  if (!(gv->config.enable_shm && dds_shm_compatible_qos_and_topic (rqos, tp, true)))
    rqos->ignore_locator_type |= DDSI_LOCATOR_KIND_SHEM;
#endif

  /* Reader gets the sertype from the topic, as the serdata functions the reader uses are
     not specific for a data representation (the representation can be retrieved from the cdr header) */
  rc = ddsi_new_reader (&rd->m_rd, &rd->m_entity.m_guid, NULL, pp, tp->m_name, tp->m_stype, rqos, &rd->m_rhc->common.rhc, dds_reader_status_cb, rd);
  assert (rc == DDS_RETCODE_OK); /* FIXME: can be out-of-resources at the very least */
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());

#ifdef DDS_HAS_SHM
  if (rd->m_rd->has_iceoryx)
  {
    DDS_CLOG (DDS_LC_SHM, &rd->m_entity.m_domain->gv.logconfig, "Reader's topic name will be DDS:Cyclone:%s\n", rd->m_topic->m_name);

    iox_sub_context_init(&rd->m_iox_sub_context);

    iox_sub_options_t opts = create_iox_sub_options(rqos);

    // quick hack to make partitions work; use a * mark to separate partition name and topic name
    // because we already know the partition can't contain a * anymore
    char *part_topic = dds_shm_partition_topic (rqos, rd->m_topic);
    assert (part_topic != NULL);
    rd->m_iox_sub = iox_sub_init(&(iox_sub_storage_t){0}, gv->config.iceoryx_service, rd->m_topic->m_stype->type_name, part_topic, &opts);
    ddsrt_free (part_topic);

    // NB: Due to some storage paradigm change of iceoryx structs
    // we now have a pointer 8 bytes before m_iox_sub
    // We use this address to store a pointer to the context.
    iox_sub_context_t **context = iox_sub_context_ptr(rd->m_iox_sub);
    *context = &rd->m_iox_sub_context;

    rc = dds_shm_monitor_attach_reader(&rd->m_entity.m_domain->m_shm_monitor, rd);

    if (rc != DDS_RETCODE_OK) {
      // we fail if we cannot attach to the listener (as we would get no data)
      iox_sub_deinit(rd->m_iox_sub);
      rd->m_iox_sub = NULL;
      DDS_CLOG(DDS_LC_WARNING | DDS_LC_SHM,
               &rd->m_entity.m_domain->gv.logconfig,
               "Failed to attach iox subscriber to iox listener\n");
      // FIXME: We need to clean up everything created up to now.
      //        Currently there is only partial cleanup, we need to extend it.
      goto err_bad_qos;
    }

    // those are set once and never changed
    // they are used to access reader and monitor from the callback when data is received
    rd->m_iox_sub_context.monitor = &rd->m_entity.m_domain->m_shm_monitor;
    rd->m_iox_sub_context.parent_reader = rd;
  }
#endif

  rd->m_entity.m_iid = ddsi_get_entity_instanceid (&rd->m_entity.m_domain->gv, &rd->m_entity.m_guid);
  dds_entity_register_child (&sub->m_entity, &rd->m_entity);

  // After including the reader amongst the subscriber's children, the subscriber will start
  // propagating whether data_on_readers is materialised or not.  That doesn't cater for the cases
  // where pessimistically set it to materialized here, nor for the race where the it actually was
  // materialized but no longer so prior to `dds_entity_register_child`.
  ddsrt_mutex_lock (&rd->m_entity.m_observers_lock);
  ddsrt_mutex_lock (&sub->m_entity.m_observers_lock);
  if (sub->materialize_data_on_readers == 0)
    ddsrt_atomic_and32 (&rd->m_entity.m_status.m_status_and_mask, ~(uint32_t)(DDS_DATA_ON_READERS_STATUS << SAM_ENABLED_SHIFT));
  ddsrt_mutex_unlock (&sub->m_entity.m_observers_lock);
  ddsrt_mutex_unlock (&rd->m_entity.m_observers_lock);

  dds_topic_allow_set_qos (tp);
  dds_topic_unpin (tp);
  dds_subscriber_unlock (sub);
  return reader;

err_bad_qos:
err_data_repr:
  dds_delete_qos (rqos);
  dds_topic_allow_set_qos (tp);
err_pp_mismatch:
  dds_topic_unpin (tp);
err_pin_topic:
  dds_subscriber_unlock (sub);
  if (created_implicit_sub)
    (void) dds_delete (subscriber);
  return rc;
}

dds_entity_t dds_create_reader (dds_entity_t participant_or_subscriber, dds_entity_t topic, const dds_qos_t *qos, const dds_listener_t *listener)
{
  return dds_create_reader_int (participant_or_subscriber, topic, qos, listener, NULL);
}

dds_entity_t dds_create_reader_rhc (dds_entity_t participant_or_subscriber, dds_entity_t topic, const dds_qos_t *qos, const dds_listener_t *listener, struct dds_rhc *rhc)
{
  if (rhc == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  return dds_create_reader_int (participant_or_subscriber, topic, qos, listener, rhc);
}

uint32_t dds_reader_lock_samples (dds_entity_t reader)
{
  dds_reader *rd;
  uint32_t n;
  if (dds_reader_lock (reader, &rd) != DDS_RETCODE_OK)
    return 0;
  n = dds_rhc_lock_samples (rd->m_rhc);
  dds_reader_unlock (rd);
  return n;
}

dds_return_t dds_reader_wait_for_historical_data (dds_entity_t reader, dds_duration_t max_wait)
{
  dds_reader *rd;
  dds_return_t ret;
  (void) max_wait;
  if ((ret = dds_reader_lock (reader, &rd)) != DDS_RETCODE_OK)
    return ret;
  switch (rd->m_entity.m_qos->durability.kind)
  {
    case DDS_DURABILITY_VOLATILE:
      ret = DDS_RETCODE_OK;
      break;
    case DDS_DURABILITY_TRANSIENT_LOCAL:
      break;
    case DDS_DURABILITY_TRANSIENT:
    case DDS_DURABILITY_PERSISTENT:
      break;
  }
  dds_reader_unlock(rd);
  return ret;
}

dds_entity_t dds_get_subscriber (dds_entity_t entity)
{
  dds_entity *e;
  dds_return_t ret;
  if ((ret = dds_entity_pin (entity, &e)) != DDS_RETCODE_OK)
    return ret;
  else
  {
    dds_entity_t subh;
    switch (dds_entity_kind (e))
    {
      case DDS_KIND_READER:
        assert (dds_entity_kind (e->m_parent) == DDS_KIND_SUBSCRIBER);
        subh = e->m_parent->m_hdllink.hdl;
        break;
      case DDS_KIND_COND_READ:
      case DDS_KIND_COND_QUERY:
        assert (dds_entity_kind (e->m_parent) == DDS_KIND_READER);
        assert (dds_entity_kind (e->m_parent->m_parent) == DDS_KIND_SUBSCRIBER);
        subh = e->m_parent->m_parent->m_hdllink.hdl;
        break;
      default:
        subh = DDS_RETCODE_ILLEGAL_OPERATION;
        break;
    }
    dds_entity_unpin (e);
    return subh;
  }
}

dds_return_t dds__reader_data_allocator_init (const dds_reader *rd, dds_data_allocator_t *data_allocator)
{
#ifdef DDS_HAS_SHM
  dds_iox_allocator_t *d = (dds_iox_allocator_t *) data_allocator->opaque.bytes;
  ddsrt_mutex_init(&d->mutex);
  if (NULL != rd->m_iox_sub)
  {
    d->kind = DDS_IOX_ALLOCATOR_KIND_SUBSCRIBER;
    d->ref.sub = rd->m_iox_sub;
  }
  else
  {
    d->kind = DDS_IOX_ALLOCATOR_KIND_NONE;
  }
  return DDS_RETCODE_OK;
#else
  (void) rd;
  (void) data_allocator;
  return DDS_RETCODE_OK;
#endif
}

dds_return_t dds__reader_data_allocator_fini (const dds_reader *rd, dds_data_allocator_t *data_allocator)
{
#ifdef DDS_HAS_SHM
  dds_iox_allocator_t *d = (dds_iox_allocator_t *) data_allocator->opaque.bytes;
  ddsrt_mutex_destroy(&d->mutex);
  d->kind = DDS_IOX_ALLOCATOR_KIND_FINI;
#else
  (void) data_allocator;
#endif
  (void) rd;
  return DDS_RETCODE_OK;
}
