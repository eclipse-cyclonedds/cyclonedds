/*
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
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds__builtin.h"
#include "dds__statistics.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsi/ddsi_statistics.h"

DECL_ENTITY_LOCK_UNLOCK (extern inline, dds_reader)

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

  thread_state_awake (lookup_thread_state (), &e->m_domain->gv);
  (void) delete_reader (&e->m_domain->gv, &e->m_guid);
  thread_state_asleep (lookup_thread_state ());

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

  thread_state_awake (lookup_thread_state (), &e->m_domain->gv);
  dds_rhc_free (rd->m_rhc);
  thread_state_asleep (lookup_thread_state ());
  dds_entity_drop_ref (&rd->m_topic->m_entity);
  return DDS_RETCODE_OK;
}

static dds_return_t validate_reader_qos (const dds_qos_t *rqos)
{
#ifndef DDS_HAS_DEADLINE_MISSED
  if (rqos != NULL && (rqos->present & QP_DEADLINE) && rqos->deadline.deadline != DDS_INFINITY)
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
    struct reader *rd;
    thread_state_awake (lookup_thread_state (), &e->m_domain->gv);
    if ((rd = entidx_lookup_reader_guid (e->m_domain->gv.entity_index, &e->m_guid)) != NULL)
      update_reader_qos (rd, qos);
    thread_state_asleep (lookup_thread_state ());
  }
  return DDS_RETCODE_OK;
}

static dds_return_t dds_reader_status_validate (uint32_t mask)
{
  return (mask & ~DDS_READER_STATUS_MASK) ? DDS_RETCODE_BAD_PARAMETER : DDS_RETCODE_OK;
}

void dds_reader_data_available_cb (struct dds_reader *rd)
{
  /* DATA_AVAILABLE is special in two ways: firstly, it should first try
     DATA_ON_READERS on the line of ancestors, and if not consumed set the
     status on the subscriber; secondly it is the only one for which
     overhead really matters.  Otherwise, it is pretty much like
     dds_reader_status_cb. */

  const uint32_t data_av_enabled = (ddsrt_atomic_ld32 (&rd->m_entity.m_status.m_status_and_mask) & (DDS_DATA_AVAILABLE_STATUS << SAM_ENABLED_SHIFT));
  if (data_av_enabled == 0)
    return;

  ddsrt_mutex_lock (&rd->m_entity.m_observers_lock);
  rd->m_entity.m_cb_pending_count++;

  /* FIXME: why wait if no listener is set? */
  while (rd->m_entity.m_cb_count > 0)
    ddsrt_cond_wait (&rd->m_entity.m_observers_cond, &rd->m_entity.m_observers_lock);
  rd->m_entity.m_cb_count++;

  struct dds_listener const * const lst = &rd->m_entity.m_listener;
  dds_entity * const sub = rd->m_entity.m_parent;
  if (lst->on_data_on_readers)
  {
    ddsrt_mutex_unlock (&rd->m_entity.m_observers_lock);
    ddsrt_mutex_lock (&sub->m_observers_lock);
    const uint32_t data_on_rds_enabled = (ddsrt_atomic_ld32 (&sub->m_status.m_status_and_mask) & (DDS_DATA_ON_READERS_STATUS << SAM_ENABLED_SHIFT));
    if (data_on_rds_enabled)
    {
      sub->m_cb_pending_count++;
      while (sub->m_cb_count > 0)
        ddsrt_cond_wait (&sub->m_observers_cond, &sub->m_observers_lock);
      sub->m_cb_count++;
      ddsrt_mutex_unlock (&sub->m_observers_lock);

      lst->on_data_on_readers (sub->m_hdllink.hdl, lst->on_data_on_readers_arg);

      ddsrt_mutex_lock (&sub->m_observers_lock);
      sub->m_cb_count--;
      sub->m_cb_pending_count--;
      ddsrt_cond_broadcast (&sub->m_observers_cond);
    }
    ddsrt_mutex_unlock (&sub->m_observers_lock);
    ddsrt_mutex_lock (&rd->m_entity.m_observers_lock);
  }
  else if (rd->m_entity.m_listener.on_data_available)
  {
    ddsrt_mutex_unlock (&rd->m_entity.m_observers_lock);
    lst->on_data_available (rd->m_entity.m_hdllink.hdl, lst->on_data_available_arg);
    ddsrt_mutex_lock (&rd->m_entity.m_observers_lock);
  }
  else
  {
    dds_entity_status_set (&rd->m_entity, DDS_DATA_AVAILABLE_STATUS);
    ddsrt_mutex_lock (&sub->m_observers_lock);
    dds_entity_status_set (sub, DDS_DATA_ON_READERS_STATUS);
    ddsrt_mutex_unlock (&sub->m_observers_lock);
  }

  rd->m_entity.m_cb_count--;
  rd->m_entity.m_cb_pending_count--;

  ddsrt_cond_broadcast (&rd->m_entity.m_observers_cond);
  ddsrt_mutex_unlock (&rd->m_entity.m_observers_lock);
}

static void update_requested_deadline_missed (struct dds_requested_deadline_missed_status * __restrict st, struct dds_requested_deadline_missed_status * __restrict lst, const status_cb_data_t *data)
{
  st->last_instance_handle = data->handle;
  st->total_count++;
  // always incrementing st->total_count_change, then copying into *lst is
  // a bit more than minimal work, but this guarantees the correct value
  // also when enabling a listeners after some events have occurred
  //
  // (same line of reasoning for all of them)
  st->total_count_change++;
  if (lst != NULL)
  {
    *lst = *st;
    st->total_count_change = 0;
  }
}

static void update_requested_incompatible_qos (struct dds_requested_incompatible_qos_status * __restrict st, struct dds_requested_incompatible_qos_status * __restrict lst, const status_cb_data_t *data)
{
  st->last_policy_id = data->extra;
  st->total_count++;
  st->total_count_change++;
  if (lst != NULL)
  {
    *lst = *st;
    st->total_count_change = 0;
  }
}

static void update_sample_lost (struct dds_sample_lost_status * __restrict st, struct dds_sample_lost_status * __restrict lst, const status_cb_data_t *data)
{
  (void) data;
  st->total_count++;
  st->total_count_change++;
  if (lst != NULL)
  {
    *lst = *st;
    st->total_count_change = 0;
  }
}

static void update_sample_rejected (struct dds_sample_rejected_status * __restrict st, struct dds_sample_rejected_status * __restrict lst, const status_cb_data_t *data)
{
  st->last_reason = data->extra;
  st->last_instance_handle = data->handle;
  st->total_count++;
  st->total_count_change++;
  if (lst != NULL)
  {
    *lst = *st;
    st->total_count_change = 0;
  }
}

static void update_liveliness_changed (struct dds_liveliness_changed_status * __restrict st, struct dds_liveliness_changed_status * __restrict lst, const status_cb_data_t *data)
{
  DDSRT_STATIC_ASSERT ((uint32_t) LIVELINESS_CHANGED_ADD_ALIVE == 0 &&
                       LIVELINESS_CHANGED_ADD_ALIVE < LIVELINESS_CHANGED_ADD_NOT_ALIVE &&
                       LIVELINESS_CHANGED_ADD_NOT_ALIVE < LIVELINESS_CHANGED_REMOVE_NOT_ALIVE &&
                       LIVELINESS_CHANGED_REMOVE_NOT_ALIVE < LIVELINESS_CHANGED_REMOVE_ALIVE &&
                       LIVELINESS_CHANGED_REMOVE_ALIVE < LIVELINESS_CHANGED_ALIVE_TO_NOT_ALIVE &&
                       LIVELINESS_CHANGED_ALIVE_TO_NOT_ALIVE < LIVELINESS_CHANGED_NOT_ALIVE_TO_ALIVE &&
                       (uint32_t) LIVELINESS_CHANGED_NOT_ALIVE_TO_ALIVE < UINT32_MAX);
  assert (data->extra <= (uint32_t) LIVELINESS_CHANGED_NOT_ALIVE_TO_ALIVE);
  st->last_publication_handle = data->handle;
  switch ((enum liveliness_changed_data_extra) data->extra)
  {
    case LIVELINESS_CHANGED_ADD_ALIVE:
      st->alive_count++;
      st->alive_count_change++;
      break;
    case LIVELINESS_CHANGED_ADD_NOT_ALIVE:
      st->not_alive_count++;
      st->not_alive_count_change++;
      break;
    case LIVELINESS_CHANGED_REMOVE_NOT_ALIVE:
      st->not_alive_count--;
      st->not_alive_count_change--;
      break;
    case LIVELINESS_CHANGED_REMOVE_ALIVE:
      st->alive_count--;
      st->alive_count_change--;
      break;
    case LIVELINESS_CHANGED_ALIVE_TO_NOT_ALIVE:
      st->alive_count--;
      st->alive_count_change--;
      st->not_alive_count++;
      st->not_alive_count_change++;
      break;
    case LIVELINESS_CHANGED_NOT_ALIVE_TO_ALIVE:
      st->not_alive_count--;
      st->not_alive_count_change--;
      st->alive_count++;
      st->alive_count_change++;
      break;
  }
  if (lst != NULL)
  {
    *lst = *st;
    st->alive_count_change = 0;
    st->not_alive_count_change = 0;
  }
}

static void update_subscription_matched (struct dds_subscription_matched_status * __restrict st, struct dds_subscription_matched_status * __restrict lst, const status_cb_data_t *data)
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
  if (lst != NULL)
  {
    *lst = *st;
    st->total_count_change = 0;
    st->current_count_change = 0;
  }
}

STATUS_CB_IMPL (reader, requested_deadline_missed, REQUESTED_DEADLINE_MISSED)
STATUS_CB_IMPL (reader, requested_incompatible_qos, REQUESTED_INCOMPATIBLE_QOS)
STATUS_CB_IMPL (reader, sample_lost, SAMPLE_LOST)
STATUS_CB_IMPL (reader, sample_rejected, SAMPLE_REJECTED)
STATUS_CB_IMPL (reader, liveliness_changed, LIVELINESS_CHANGED)
STATUS_CB_IMPL (reader, subscription_matched, SUBSCRIPTION_MATCHED)

void dds_reader_status_cb (void *ventity, const status_cb_data_t *data)
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
  while (rd->m_entity.m_cb_count > 0)
    ddsrt_cond_wait (&rd->m_entity.m_observers_cond, &rd->m_entity.m_observers_lock);

  const enum dds_status_id status_id = (enum dds_status_id) data->raw_status_id;
  const bool enabled = (ddsrt_atomic_ld32 (&rd->m_entity.m_status.m_status_and_mask) & ((1u << status_id) << SAM_ENABLED_SHIFT)) != 0;
  switch (status_id)
  {
    case DDS_REQUESTED_DEADLINE_MISSED_STATUS_ID:
      status_cb_requested_deadline_missed (rd, data, enabled);
      break;
    case DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS_ID:
      status_cb_requested_incompatible_qos (rd, data, enabled);
      break;
    case DDS_SAMPLE_LOST_STATUS_ID:
      status_cb_sample_lost (rd, data, enabled);
      break;
    case DDS_SAMPLE_REJECTED_STATUS_ID:
      status_cb_sample_rejected (rd, data, enabled);
      break;
    case DDS_LIVELINESS_CHANGED_STATUS_ID:
      status_cb_liveliness_changed (rd, data, enabled);
      break;
    case DDS_SUBSCRIPTION_MATCHED_STATUS_ID:
      status_cb_subscription_matched (rd, data, enabled);
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

  if ((rc = dds_topic_pin (topic, &tp)) < 0)
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
    ddsi_xqos_mergein_missing (rqos, sub->m_entity.m_qos, ~(uint64_t)0);
  if (tp->m_ktopic->qos)
    ddsi_xqos_mergein_missing (rqos, tp->m_ktopic->qos, ~(uint64_t)0);
  ddsi_xqos_mergein_missing (rqos, &gv->default_xqos_rd, ~(uint64_t)0);

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

  thread_state_awake (lookup_thread_state (), gv);
  const struct ddsi_guid * ppguid = dds_entity_participant_guid (&sub->m_entity);
  struct participant * pp = entidx_lookup_participant_guid (gv->entity_index, ppguid);

  /* When deleting a participant, the child handles (that include the subscriber)
     are removed before removing the DDSI participant. So at this point, within
     the subscriber lock, we can assert that the participant exists. */
  assert (pp != NULL);

#ifdef DDS_HAS_SECURITY
  /* Check if DDS Security is enabled */
  if (q_omg_participant_is_secure (pp))
  {
    /* ask to access control security plugin for create reader permissions */
    if (!q_omg_security_check_create_reader (pp, gv->config.domainId, tp->m_name, rqos))
    {
      rc = DDS_RETCODE_NOT_ALLOWED_BY_SECURITY;
      goto err_not_allowed;
    }
  }
#endif

  /* Create reader and associated read cache (if not provided by caller) */
  struct dds_reader * const rd = dds_alloc (sizeof (*rd));
  const dds_entity_t reader = dds_entity_init (&rd->m_entity, &sub->m_entity, DDS_KIND_READER, false, rqos, listener, DDS_READER_STATUS_MASK);
  rd->m_sample_rejected_status.last_reason = DDS_NOT_REJECTED;
  rd->m_topic = tp;
  rd->m_wrapped_sertopic = (tp->m_stype->wrapped_sertopic != NULL) ? 1 : 0;
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

  rc = new_reader (&rd->m_rd, &rd->m_entity.m_guid, NULL, pp, tp->m_name, tp->m_stype, rqos, &rd->m_rhc->common.rhc, dds_reader_status_cb, rd);
  assert (rc == DDS_RETCODE_OK); /* FIXME: can be out-of-resources at the very least */
  thread_state_asleep (lookup_thread_state ());

  rd->m_entity.m_iid = get_entity_instance_id (&rd->m_entity.m_domain->gv, &rd->m_entity.m_guid);
  dds_entity_register_child (&sub->m_entity, &rd->m_entity);

  dds_topic_allow_set_qos (tp);
  dds_topic_unpin (tp);
  dds_subscriber_unlock (sub);
  return reader;

#ifdef DDS_HAS_SECURITY
err_not_allowed:
  thread_state_asleep (lookup_thread_state ());
#endif
err_bad_qos:
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

void dds_reader_ddsi2direct (dds_entity_t entity, ddsi2direct_directread_cb_t cb, void *cbarg)
{
  dds_entity *dds_entity;
  if (dds_entity_pin (entity, &dds_entity) != DDS_RETCODE_OK)
    return;
  if (dds_entity_kind (dds_entity) != DDS_KIND_READER)
  {
    dds_entity_unpin (dds_entity);
    return;
  }

  dds_reader *dds_rd = (dds_reader *) dds_entity;
  struct reader *rd = dds_rd->m_rd;
  ddsi_guid_t pwrguid;
  struct proxy_writer *pwr;
  struct rd_pwr_match *m;
  memset (&pwrguid, 0, sizeof (pwrguid));
  ddsrt_mutex_lock (&rd->e.lock);

  rd->ddsi2direct_cb = cb;
  rd->ddsi2direct_cbarg = cbarg;
  while ((m = ddsrt_avl_lookup_succ_eq (&rd_writers_treedef, &rd->writers, &pwrguid)) != NULL)
  {
    /* have to be careful walking the tree -- pretty is different, but
       I want to check this before I write a lookup_succ function. */
    struct rd_pwr_match *m_next;
    ddsi_guid_t pwrguid_next;
    pwrguid = m->pwr_guid;
    if ((m_next = ddsrt_avl_find_succ (&rd_writers_treedef, &rd->writers, m)) != NULL)
      pwrguid_next = m_next->pwr_guid;
    else
    {
      memset (&pwrguid_next, 0xff, sizeof (pwrguid_next));
      pwrguid_next.entityid.u = (pwrguid_next.entityid.u & ~(uint32_t)0xff) | NN_ENTITYID_KIND_WRITER_NO_KEY;
    }
    ddsrt_mutex_unlock (&rd->e.lock);
    if ((pwr = entidx_lookup_proxy_writer_guid (dds_entity->m_domain->gv.entity_index, &pwrguid)) != NULL)
    {
      ddsrt_mutex_lock (&pwr->e.lock);
      pwr->ddsi2direct_cb = cb;
      pwr->ddsi2direct_cbarg = cbarg;
      ddsrt_mutex_unlock (&pwr->e.lock);
    }
    pwrguid = pwrguid_next;
    ddsrt_mutex_lock (&rd->e.lock);
  }
  ddsrt_mutex_unlock (&rd->e.lock);
  dds_entity_unpin (dds_entity);
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

/* Reset sets everything (type) 0, including the reason field, verify that 0 is correct */
DDSRT_STATIC_ASSERT ((int) DDS_NOT_REJECTED == 0);

DDS_GET_STATUS (reader, subscription_matched,       SUBSCRIPTION_MATCHED,       total_count_change, current_count_change)
DDS_GET_STATUS (reader, liveliness_changed,         LIVELINESS_CHANGED,         alive_count_change, not_alive_count_change)
DDS_GET_STATUS (reader, sample_rejected,            SAMPLE_REJECTED,            total_count_change)
DDS_GET_STATUS (reader, sample_lost,                SAMPLE_LOST,                total_count_change)
DDS_GET_STATUS (reader, requested_deadline_missed,  REQUESTED_DEADLINE_MISSED,  total_count_change)
DDS_GET_STATUS (reader, requested_incompatible_qos, REQUESTED_INCOMPATIBLE_QOS, total_count_change)

