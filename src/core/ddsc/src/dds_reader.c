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
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/ddsi/ddsi_thread.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsi/ddsi_statistics.h"
#include "dds/ddsi/ddsi_endpoint_match.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsc/dds_rhc.h"
#include "dds/ddsc/dds_internal_api.h"
#include "dds__participant.h"
#include "dds__subscriber.h"
#include "dds__reader.h"
#include "dds__listener.h"
#include "dds__init.h"
#include "dds__rhc_default.h"
#include "dds__topic.h"
#include "dds__get_status.h"
#include "dds__qos.h"
#include "dds__builtin.h"
#include "dds__statistics.h"
#include "dds__psmx.h"
#include "dds__guid.h"

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

  ddsi_thread_state_awake (ddsi_lookup_thread_state (), &e->m_domain->gv);
  (void) ddsi_delete_reader (&e->m_domain->gv, &e->m_guid);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());

  ddsrt_mutex_lock (&e->m_mutex);
  while (rd->m_rd != NULL)
    ddsrt_cond_wait (&e->m_cond, &e->m_mutex);
  ddsrt_mutex_unlock (&e->m_mutex);
}

ddsrt_nonnull_all
static dds_return_t dds_reader_delete (dds_entity *e)
{
  dds_return_t ret = DDS_RETCODE_OK;
  dds_reader * const rd = (dds_reader *) e;

  ddsi_thread_state_awake (ddsi_lookup_thread_state (), &e->m_domain->gv);
  dds_rhc_free (rd->m_rhc);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());

  dds_loan_pool_free (rd->m_heap_loan_cache);
  dds_loan_pool_free (rd->m_loans);
  dds_endpoint_remove_psmx_endpoints (&rd->m_endpoint);

  dds_entity_drop_ref (&rd->m_topic->m_entity);
  return ret;
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

static void data_avail_cb_invoke_dor (dds_entity *sub, const struct dds_listener *lst, bool async)
{
  // assumes sub->m_observers_lock held on entry
  // unlocks and relocks sub->m_observers_lock
  if (async) data_avail_cb_enter_listener_exclusive_access (sub);
  ddsrt_mutex_unlock (&sub->m_observers_lock);
  lst->on_data_on_readers (sub->m_hdllink.hdl, lst->on_data_on_readers_arg);
  ddsrt_mutex_lock (&sub->m_observers_lock);
  if (async) data_avail_cb_leave_listener_exclusive_access (sub);
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
      dds_entity_observers_signal (sub);
    ddsrt_mutex_unlock (&sub->m_observers_lock);
  }
  if (signal & DDS_DATA_AVAILABLE_STATUS)
  {
    const uint32_t sm = ddsrt_atomic_ld32 (&rd->m_status.m_status_and_mask);
    if ((sm & (sm >> SAM_ENABLED_SHIFT)) & DDS_DATA_AVAILABLE_STATUS)
      dds_entity_observers_signal (rd);
  }
}

static uint32_t da_or_dor_cb_invoke(struct dds_reader *rd, struct dds_listener const * const lst, uint32_t status_and_mask, bool async)
{
  uint32_t signal = 0;

  if (lst->on_data_on_readers)
  {
    dds_entity * const sub = rd->m_entity.m_parent;
    ddsrt_mutex_unlock (&rd->m_entity.m_observers_lock);
    ddsrt_mutex_lock (&sub->m_observers_lock);
    if (!(lst->reset_on_invoke & DDS_DATA_ON_READERS_STATUS))
      signal = data_avail_cb_set_status (&rd->m_entity, status_and_mask);
    data_avail_cb_invoke_dor (sub, lst, async);
    ddsrt_mutex_unlock (&sub->m_observers_lock);
    ddsrt_mutex_lock (&rd->m_entity.m_observers_lock);
  }
  else if(rd->m_entity.m_listener.on_data_available)
  {
    if (!(lst->reset_on_invoke & DDS_DATA_AVAILABLE_STATUS))
      signal = data_avail_cb_set_status (&rd->m_entity, status_and_mask);
    ddsrt_mutex_unlock (&rd->m_entity.m_observers_lock);
    lst->on_data_available (rd->m_entity.m_hdllink.hdl, lst->on_data_available_arg);
    ddsrt_mutex_lock (&rd->m_entity.m_observers_lock);
  }
  return signal;
}

void dds_reader_data_available_cb (struct dds_reader *rd)
{
  /* DATA_AVAILABLE is special in two ways: firstly, it should first try
     DATA_ON_READERS on the line of ancestors, and if not consumed set the
     status on the subscriber; secondly it is the only one for which
     overhead really matters.  Otherwise, it is pretty much like
     dds_reader_status_cb. */
  uint32_t signal;
  struct dds_listener const * const lst = &rd->m_entity.m_listener;

  ddsrt_mutex_lock (&rd->m_entity.m_observers_lock);
  const uint32_t status_and_mask = ddsrt_atomic_ld32 (&rd->m_entity.m_status.m_status_and_mask);
  if (lst->on_data_on_readers == NULL && lst->on_data_available == NULL)
    signal = data_avail_cb_set_status (&rd->m_entity, status_and_mask);
  else
  {
    // "lock" listener object so we can look at "lst" without holding m_observers_lock
    data_avail_cb_enter_listener_exclusive_access (&rd->m_entity);
    signal = da_or_dor_cb_invoke(rd, lst, status_and_mask, true);
    data_avail_cb_leave_listener_exclusive_access (&rd->m_entity);
  }
  data_avail_cb_trigger_waitsets (&rd->m_entity, signal);
  ddsrt_mutex_unlock (&rd->m_entity.m_observers_lock);
}

static void update_requested_deadline_missed (struct dds_requested_deadline_missed_status *st, const ddsi_status_cb_data_t *data)
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

static void update_requested_incompatible_qos (struct dds_requested_incompatible_qos_status *st, const ddsi_status_cb_data_t *data)
{
  st->last_policy_id = data->extra;
  st->total_count++;
  st->total_count_change++;
}

static void update_sample_lost (struct dds_sample_lost_status *st, const ddsi_status_cb_data_t *data)
{
  (void) data;
  st->total_count++;
  st->total_count_change++;
}

static void update_sample_rejected (struct dds_sample_rejected_status *st, const ddsi_status_cb_data_t *data)
{
  st->last_reason = data->extra;
  st->last_instance_handle = data->handle;
  st->total_count++;
  st->total_count_change++;
}

static void update_liveliness_changed (struct dds_liveliness_changed_status *st, const ddsi_status_cb_data_t *data)
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

static void update_subscription_matched (struct dds_subscription_matched_status *st, const ddsi_status_cb_data_t *data)
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

void dds_reader_status_cb (void *ventity, const struct ddsi_status_cb_data *data)
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

void dds_reader_invoke_cbs_for_pending_events(struct dds_entity *e, uint32_t status)
{
  dds_reader * const rdr = (dds_reader *) e;
  struct dds_listener const * const lst =  &e->m_listener;

  if (lst->on_requested_deadline_missed && (status & DDS_REQUESTED_DEADLINE_MISSED_STATUS)) {
    status_cb_requested_deadline_missed_invoke(rdr);
  }
  if (lst->on_requested_incompatible_qos && (status & DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS)) {
    status_cb_requested_incompatible_qos_invoke(rdr);
  }
  if (lst->on_sample_lost && (status & DDS_SAMPLE_LOST_STATUS)) {
    status_cb_sample_lost_invoke(rdr);
  }
  if (lst->on_sample_rejected && (status & DDS_SAMPLE_REJECTED_STATUS)) {
    status_cb_sample_rejected_invoke(rdr);
  }
  if (lst->on_liveliness_changed && (status & DDS_LIVELINESS_CHANGED_STATUS)) {
    status_cb_liveliness_changed_invoke(rdr);
  }
  if (lst->on_subscription_matched && (status & DDS_SUBSCRIPTION_MATCHED_STATUS)) {
    status_cb_subscription_matched_invoke(rdr);
  }
  if ((status & DDS_DATA_AVAILABLE_STATUS)) {
    const uint32_t status_and_mask = ddsrt_atomic_ld32 (&e->m_status.m_status_and_mask);
    (void) da_or_dor_cb_invoke(rdr, lst, status_and_mask, false);
  }
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
  .refresh_statistics = dds_reader_refresh_statistics,
  .invoke_cbs_for_pending_events = dds_reader_invoke_cbs_for_pending_events
};

static dds_entity_t dds_create_reader_int (dds_entity_t participant_or_subscriber, dds_entity_t topic, dds_guid_t *guid, const dds_qos_t *qos, const dds_listener_t *listener, struct dds_rhc *rhc)
{
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
  dds_qos_t *rqos = dds_create_qos ();
  bool own_rqos = true;
  if (qos)
    ddsi_xqos_mergein_missing (rqos, qos, DDS_READER_QOS_MASK);
  if (sub->m_entity.m_qos)
    ddsi_xqos_mergein_missing (rqos, sub->m_entity.m_qos, ~DDSI_QP_ENTITY_NAME);
  if (tp->m_ktopic->qos)
    ddsi_xqos_mergein_missing (rqos, tp->m_ktopic->qos, (DDS_READER_QOS_MASK | DDSI_QP_TOPIC_DATA) & ~DDSI_QP_ENTITY_NAME);
  ddsi_xqos_mergein_missing (rqos, &ddsi_default_qos_reader, ~DDSI_QP_DATA_REPRESENTATION);
  dds_apply_entity_naming(rqos, sub->m_entity.m_qos, gv);

  if ((rc = dds_ensure_valid_data_representation (rqos, tp->m_stype->allowed_data_representation, tp->m_stype->data_type_props, DDS_KIND_READER)) != DDS_RETCODE_OK)
    goto err_data_repr;
  if ((rc = dds_ensure_valid_psmx_instances (rqos, DDS_PSMX_ENDPOINT_TYPE_READER, tp->m_stype, &sub->m_entity.m_domain->psmx_instances)) != DDS_RETCODE_OK)
    goto err_psmx;

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
      goto err_not_allowed;
    }
  }
#endif

  /* Create reader and associated read cache (if not provided by caller) */
  struct dds_reader * const rd = dds_alloc (sizeof (*rd));
  const dds_entity_t reader = dds_entity_init (&rd->m_entity, &sub->m_entity, DDS_KIND_READER, false, true, rqos, listener, DDS_READER_STATUS_MASK);

  // Ownership of rqos is transferred to reader entity
  own_rqos = false;

  // assume DATA_ON_READERS is materialized in the subscriber:
  // - changes to it won't be propagated to this reader until after it has been added to the subscriber's children
  // - data can arrive once `new_reader` is called, requiring raising DATA_ON_READERS if materialized
  // - setting DATA_ON_READERS on subscriber if it is not actually materialized is no problem
  ddsrt_atomic_or32 (&rd->m_entity.m_status.m_status_and_mask, DDS_DATA_ON_READERS_STATUS << SAM_ENABLED_SHIFT);
  rd->m_sample_rejected_status.last_reason = DDS_NOT_REJECTED;
  rd->m_topic = tp;
  rd->m_rhc = rhc ? rhc : dds_rhc_default_new (rd, tp->m_stype);
  rc = dds_loan_pool_create (&rd->m_loans, 0);
  assert (rc == DDS_RETCODE_OK); // FIXME: can be out of resources
  rc = dds_loan_pool_create (&rd->m_heap_loan_cache, 0);
  assert (rc == DDS_RETCODE_OK); // FIXME: can be out of resources
  if (dds_rhc_associate (rd->m_rhc, rd, tp->m_stype, rd->m_entity.m_domain->gv.m_tkmap) < 0)
  {
    /* FIXME: see also create_querycond, need to be able to undo entity_init */
    abort ();
  }
  dds_entity_add_ref_locked (&tp->m_entity);

  if ((rc = dds_endpoint_add_psmx_endpoint (&rd->m_endpoint, rqos, &tp->m_ktopic->psmx_topics, DDS_PSMX_ENDPOINT_TYPE_READER)) != DDS_RETCODE_OK)
  {
    ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
    goto err_create_endpoint;
  }

  /* FIXME: listeners can come too soon ... should set mask based on listeners
     then atomically set the listeners, save the mask to a pending set and clear
     it; and then invoke those listeners that are in the pending set */
  dds_entity_init_complete (&rd->m_entity);

  if (guid)
    rd->m_entity.m_guid = dds_guid_to_ddsi_guid (*guid);
  else
  {
    rc = ddsi_generate_reader_guid (&rd->m_entity.m_guid, pp, tp->m_stype);
    if (rc != DDS_RETCODE_OK)
    {
      ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
      goto err_rd_guid;
    }
  }

  struct ddsi_psmx_locators_set *vl_set = dds_get_psmx_locators_set (rqos, &rd->m_entity.m_domain->psmx_instances);

  /* Reader gets the sertype from the topic, as the serdata functions the reader uses are
     not specific for a data representation (the representation can be retrieved from the cdr header) */
  rc = ddsi_new_reader (&rd->m_rd, &rd->m_entity.m_guid, NULL, pp, tp->m_name, tp->m_stype, rqos, &rd->m_rhc->common.rhc, dds_reader_status_cb, rd, vl_set);
  if (rc != DDS_RETCODE_OK)
  {
    /* FIXME: can be out-of-resources at the very least; would leak allocated entity id */
    abort ();
  }
  dds_psmx_locators_set_free (vl_set);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());

  rd->m_entity.m_iid = ddsi_get_entity_instanceid (&rd->m_entity.m_domain->gv, &rd->m_entity.m_guid);
  dds_entity_register_child (&sub->m_entity, &rd->m_entity);

  for (uint32_t i = 0; i < rd->m_endpoint.psmx_endpoints.length; i++)
  {
    struct dds_psmx_endpoint_int *psmx_endpoint = rd->m_endpoint.psmx_endpoints.endpoints[i];
    if (psmx_endpoint->ops.on_data_available && (rc = psmx_endpoint->ops.on_data_available (psmx_endpoint->ext, reader)) != DDS_RETCODE_OK)
      goto err_psmx_endpoint_setcb;
  }

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

err_psmx_endpoint_setcb:
err_rd_guid:
  dds_endpoint_remove_psmx_endpoints (&rd->m_endpoint);
err_create_endpoint:
err_bad_qos:
#ifdef DDS_HAS_SECURITY
err_not_allowed:
#endif
err_data_repr:
err_psmx:
  if (own_rqos)
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

struct writer_metadata
{
  int32_t ownership_strength;
  bool autodispose_unregistered_instances;
  dds_duration_t lifespan_duration;
};

static dds_return_t get_writer_info (struct ddsi_domaingv *gv, const ddsi_guid_t *guid, uint32_t statusinfo, const struct writer_metadata *writer_md, struct ddsi_writer_info *wi)
{
  dds_return_t ret = DDS_RETCODE_OK;
  struct dds_qos *xqos = NULL;

  if (writer_md == NULL)
  {
    struct ddsi_entity_common *ec = ddsi_entidx_lookup_guid_untyped (gv->entity_index, guid);
    if (ec == NULL || (ec->kind != DDSI_EK_PROXY_WRITER && ec->kind != DDSI_EK_WRITER))
    {
      ret = DDS_RETCODE_NOT_FOUND;
      goto err;
    }
    else if (ec->kind == DDSI_EK_PROXY_WRITER)
      xqos = ((struct ddsi_proxy_writer *) ec)->c.xqos;
    else
      xqos = ((struct ddsi_writer *) ec)->xqos;

    ddsi_make_writer_info (wi, ec, xqos, statusinfo);
  }
  else
  {
    uint64_t iid = ((uint64_t) ddsrt_toBE4u (guid->prefix.u[2]) << 32llu) + ddsrt_toBE4u (guid->entityid.u);
    ddsi_make_writer_info_params (wi, guid, writer_md->ownership_strength, writer_md->autodispose_unregistered_instances, iid, statusinfo, writer_md->lifespan_duration);
  }

err:
  return ret;
}

static dds_return_t dds_reader_store_loaned_sample_impl (dds_entity_t reader, dds_loaned_sample_t *data, const struct writer_metadata *writer_md)
{
  dds_return_t ret = DDS_RETCODE_OK;
  dds_entity * e;
  if ((ret = dds_entity_pin (reader, &e)) < 0)
    return ret;
  else if (dds_entity_kind (e) != DDS_KIND_READER)
  {
    dds_entity_unpin (e);
    return DDS_RETCODE_ILLEGAL_OPERATION;
  }

  dds_reader *dds_rd = (dds_reader *) e;
  struct ddsi_reader *rd = dds_rd->m_rd;
  struct ddsi_domaingv *gv = rd->e.gv;

  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv);
  ddsrt_mutex_lock (&rd->e.lock);

  // FIXME: what if the sample is overwritten?
  // if the sample is not matched to this reader, return ownership to the PSMX?

  //samples incoming from local writers should be dropped
  const ddsi_guid_t ddsi_guid = dds_guid_to_ddsi_guid (data->metadata->guid);
  struct ddsi_entity_common *lookup_entity = NULL;
  if ((lookup_entity = ddsi_entidx_lookup_guid_untyped (gv->entity_index, &ddsi_guid)) != NULL &&
      lookup_entity->kind == DDSI_EK_WRITER)
    goto drop_local;

  struct ddsi_serdata * sd = ddsi_serdata_from_psmx (rd->type, data);
  if (sd == NULL)
  {
    ret = DDS_RETCODE_ERROR;
    goto fail_serdata;
  }

  struct ddsi_writer_info wi;
  if ((ret = get_writer_info (gv, &ddsi_guid, sd->statusinfo, writer_md, &wi)) != DDS_RETCODE_OK)
    goto fail_get_writer_info;

  struct ddsi_tkmap_instance * tk = ddsi_tkmap_lookup_instance_ref (gv->m_tkmap, sd);
  if (tk == NULL)
  {
    ret = DDS_RETCODE_BAD_PARAMETER;
    goto fail_get_writer_info;
  }

  if (!dds_rhc_store (dds_rd->m_rhc, &wi, sd, tk))
  {
    ret = DDS_RETCODE_ERROR;
    goto fail_rhc_store;
  }

fail_rhc_store:
  ddsi_tkmap_instance_unref (gv->m_tkmap, tk);
fail_get_writer_info:
  ddsi_serdata_unref (sd);
drop_local:
fail_serdata:
  ddsrt_mutex_unlock (&rd->e.lock);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  dds_entity_unpin (e);
  return ret;
}

dds_return_t dds_reader_store_loaned_sample (dds_entity_t reader, dds_loaned_sample_t *data)
{
  return dds_reader_store_loaned_sample_impl (reader, data, NULL);
}

dds_return_t dds_reader_store_loaned_sample_wr_metadata (dds_entity_t reader, dds_loaned_sample_t *data, int32_t ownership_strength, bool autodispose_unregistered_instances, dds_duration_t lifespan_duration)
{
  struct writer_metadata writer_md = { .ownership_strength = ownership_strength, .autodispose_unregistered_instances = autodispose_unregistered_instances, .lifespan_duration = lifespan_duration };
  return dds_reader_store_loaned_sample_impl (reader, data, &writer_md);
}

dds_entity_t dds_create_reader (dds_entity_t participant_or_subscriber, dds_entity_t topic, const dds_qos_t *qos, const dds_listener_t *listener)
{
  return dds_create_reader_int (participant_or_subscriber, topic, NULL, qos, listener, NULL);
}

dds_entity_t dds_create_reader_guid (dds_entity_t participant_or_subscriber, dds_entity_t topic, const dds_qos_t *qos, const dds_listener_t *listener, dds_guid_t *guid)
{
  return dds_create_reader_int (participant_or_subscriber, topic, guid, qos, listener, NULL);
}

dds_entity_t dds_create_reader_rhc (dds_entity_t participant_or_subscriber, dds_entity_t topic, const dds_qos_t *qos, const dds_listener_t *listener, struct dds_rhc *rhc)
{
  if (rhc == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  return dds_create_reader_int (participant_or_subscriber, topic, NULL, qos, listener, rhc);
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
