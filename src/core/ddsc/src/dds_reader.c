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
#include "dds__subscriber.h"
#include "dds__reader.h"
#include "dds__listener.h"
#include "dds__init.h"
#include "dds__rhc.h"
#include "dds__rhc_default.h"
#include "dds__topic.h"
#include "dds__get_status.h"
#include "dds__qos.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/q_globals.h"
#include "dds__builtin.h"
#include "dds/ddsi/ddsi_sertopic.h"

DECL_ENTITY_LOCK_UNLOCK (extern inline, dds_reader)

#define DDS_READER_STATUS_MASK                                   \
                        (DDS_SAMPLE_REJECTED_STATUS              |\
                         DDS_LIVELINESS_CHANGED_STATUS           |\
                         DDS_REQUESTED_DEADLINE_MISSED_STATUS    |\
                         DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS   |\
                         DDS_DATA_AVAILABLE_STATUS               |\
                         DDS_SAMPLE_LOST_STATUS                  |\
                         DDS_SUBSCRIPTION_MATCHED_STATUS)

static dds_return_t dds_reader_close (dds_entity *e) ddsrt_nonnull_all;

static dds_return_t dds_reader_close (dds_entity *e)
{
  dds_return_t ret = DDS_RETCODE_OK;
  thread_state_awake (lookup_thread_state (), &e->m_domain->gv);
  if (delete_reader (&e->m_domain->gv, &e->m_guid) != 0)
    ret = DDS_RETCODE_ERROR;
  thread_state_asleep (lookup_thread_state ());
  return ret;
}

static dds_return_t dds_reader_delete (dds_entity *e) ddsrt_nonnull_all;

static dds_return_t dds_reader_delete (dds_entity *e)
{
  dds_reader * const rd = (dds_reader *) e;
  dds_return_t ret;
  if ((ret = dds_delete (rd->m_topic->m_entity.m_hdllink.hdl)) == DDS_RETCODE_OK)
  {
    /* Delete an implicitly created parent; for normal ones, this is expected
       to fail with BAD_PARAMETER - FIXME: there must be a cleaner way */
    ret = dds_delete_impl (e->m_parent->m_hdllink.hdl, true);
    if (ret == DDS_RETCODE_BAD_PARAMETER)
      ret = DDS_RETCODE_OK;
  }
  dds_free (rd->m_loan);
  return ret;
}

static dds_return_t dds_reader_qos_set (dds_entity *e, const dds_qos_t *qos, bool enabled)
{
  /* note: e->m_qos is still the old one to allow for failure here */
  if (enabled)
  {
    struct reader *rd;
    thread_state_awake (lookup_thread_state (), &e->m_domain->gv);
    if ((rd = ephash_lookup_reader_guid (e->m_domain->gv.guid_hash, &e->m_guid)) != NULL)
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

  const bool data_av_enabled = (ddsrt_atomic_ld32 (&rd->m_entity.m_status.m_status_and_mask) & (DDS_DATA_AVAILABLE_STATUS << SAM_ENABLED_SHIFT));
  if (!data_av_enabled)
    return;

  ddsrt_mutex_lock (&rd->m_entity.m_observers_lock);
  while (rd->m_entity.m_cb_count > 0)
    ddsrt_cond_wait (&rd->m_entity.m_observers_cond, &rd->m_entity.m_observers_lock);
  rd->m_entity.m_cb_count++;

  struct dds_listener const * const lst = &rd->m_entity.m_listener;
  dds_entity * const sub = rd->m_entity.m_parent;
  if (lst->on_data_on_readers)
  {
    ddsrt_mutex_unlock (&rd->m_entity.m_observers_lock);

    ddsrt_mutex_lock (&sub->m_observers_lock);
    while (sub->m_cb_count > 0)
      ddsrt_cond_wait (&sub->m_observers_cond, &sub->m_observers_lock);
    sub->m_cb_count++;
    ddsrt_mutex_unlock (&sub->m_observers_lock);

    lst->on_data_on_readers (sub->m_hdllink.hdl, lst->on_data_on_readers_arg);

    ddsrt_mutex_lock (&rd->m_entity.m_observers_lock);
    ddsrt_mutex_lock (&sub->m_observers_lock);
    sub->m_cb_count--;
    ddsrt_cond_broadcast (&sub->m_observers_cond);
    ddsrt_mutex_unlock (&sub->m_observers_lock);
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
  ddsrt_cond_broadcast (&rd->m_entity.m_observers_cond);
  ddsrt_mutex_unlock (&rd->m_entity.m_observers_lock);
}

void dds_reader_status_cb (void *ventity, const status_cb_data_t *data)
{
  struct dds_entity * const entity = ventity;

  /* When data is NULL, it means that the DDSI reader is deleted. */
  if (data == NULL)
  {
    /* Release the initial claim that was done during the create. This
     * will indicate that further API deletion is now possible. */
    dds_handle_unpin (&entity->m_hdllink);
    return;
  }

  struct dds_listener const * const lst = &entity->m_listener;
  enum dds_status_id status_id = (enum dds_status_id) data->raw_status_id;
  bool invoke = false;
  void *vst = NULL;
  int32_t *reset[2] = { NULL, NULL };

  /* DATA_AVAILABLE is handled by dds_reader_data_available_cb */
  assert (status_id != DDS_DATA_AVAILABLE_STATUS_ID);

  /* Serialize listener invocations -- it is somewhat sad to do this,
     but then it may also be unreasonable to expect the application to
     handle concurrent invocations of a single listener.  The benefit
     here is that it means the counters and "change" counters
     can safely be incremented and/or reset while releasing
     m_observers_lock for the duration of the listener call itself,
     and that similarly the listener function and argument pointers
     are stable */
  ddsrt_mutex_lock (&entity->m_observers_lock);
  while (entity->m_cb_count > 0)
    ddsrt_cond_wait (&entity->m_observers_cond, &entity->m_observers_lock);
  entity->m_cb_count++;

  /* Update status metrics. */
  dds_reader * const rd = (dds_reader *) entity;
  switch (status_id) {
    case DDS_REQUESTED_DEADLINE_MISSED_STATUS_ID: {
      struct dds_requested_deadline_missed_status * const st = vst = &rd->m_requested_deadline_missed_status;
      st->last_instance_handle = data->handle;
      st->total_count++;
      st->total_count_change++;
      invoke = (lst->on_requested_deadline_missed != 0);
      reset[0] = &st->total_count_change;
      break;
    }
    case DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS_ID: {
      struct dds_requested_incompatible_qos_status * const st = vst = &rd->m_requested_incompatible_qos_status;
      st->total_count++;
      st->total_count_change++;
      st->last_policy_id = data->extra;
      invoke = (lst->on_requested_incompatible_qos != 0);
      reset[0] = &st->total_count_change;
      break;
    }
    case DDS_SAMPLE_LOST_STATUS_ID: {
      struct dds_sample_lost_status * const st = vst = &rd->m_sample_lost_status;
      st->total_count++;
      st->total_count_change++;
      invoke = (lst->on_sample_lost != 0);
      reset[0] = &st->total_count_change;
      break;
    }
    case DDS_SAMPLE_REJECTED_STATUS_ID: {
      struct dds_sample_rejected_status * const st = vst = &rd->m_sample_rejected_status;
      st->total_count++;
      st->total_count_change++;
      st->last_reason = data->extra;
      st->last_instance_handle = data->handle;
      invoke = (lst->on_sample_rejected != 0);
      reset[0] = &st->total_count_change;
      break;
    }
    case DDS_LIVELINESS_CHANGED_STATUS_ID: {
      struct dds_liveliness_changed_status * const st = vst = &rd->m_liveliness_changed_status;
      if (data->add) {
        st->alive_count++;
        st->alive_count_change++;
        if (st->not_alive_count > 0) {
          st->not_alive_count--;
        }
      } else {
        st->alive_count--;
        st->not_alive_count++;
        st->not_alive_count_change++;
      }
      st->last_publication_handle = data->handle;
      invoke = (lst->on_liveliness_changed != 0);
      reset[0] = &st->alive_count_change;
      reset[1] = &st->not_alive_count_change;
      break;
    }
    case DDS_SUBSCRIPTION_MATCHED_STATUS_ID: {
      struct dds_subscription_matched_status * const st = vst = &rd->m_subscription_matched_status;
      if (data->add) {
        st->total_count++;
        st->total_count_change++;
        st->current_count++;
        st->current_count_change++;
      } else {
        st->current_count--;
        st->current_count_change--;
      }
      st->last_publication_handle = data->handle;
      invoke = (lst->on_subscription_matched != 0);
      reset[0] = &st->total_count_change;
      reset[1] = &st->current_count_change;
      break;
    }
    case DDS_DATA_ON_READERS_STATUS_ID:
    case DDS_DATA_AVAILABLE_STATUS_ID:
    case DDS_INCONSISTENT_TOPIC_STATUS_ID:
    case DDS_LIVELINESS_LOST_STATUS_ID:
    case DDS_PUBLICATION_MATCHED_STATUS_ID:
    case DDS_OFFERED_DEADLINE_MISSED_STATUS_ID:
    case DDS_OFFERED_INCOMPATIBLE_QOS_STATUS_ID:
      assert (0);
  }

  if (invoke)
  {
    ddsrt_mutex_unlock (&entity->m_observers_lock);
    dds_entity_invoke_listener (entity, status_id, vst);
    ddsrt_mutex_lock (&entity->m_observers_lock);
    *reset[0] = 0;
    if (reset[1])
      *reset[1] = 0;
  }
  else
  {
    dds_entity_status_set (entity, (status_mask_t) (1u << status_id));
  }

  entity->m_cb_count--;
  ddsrt_cond_broadcast (&entity->m_observers_cond);
  ddsrt_mutex_unlock (&entity->m_observers_lock);
}

const struct dds_entity_deriver dds_entity_deriver_reader = {
  .close = dds_reader_close,
  .delete = dds_reader_delete,
  .set_qos = dds_reader_qos_set,
  .validate_status = dds_reader_status_validate
};

dds_entity_t dds_create_reader (dds_entity_t participant_or_subscriber, dds_entity_t topic, const dds_qos_t *qos, const dds_listener_t *listener)
{
  dds_qos_t *rqos;
  dds_subscriber *sub = NULL;
  dds_entity_t subscriber;
  dds_reader *rd;
  dds_topic *tp;
  dds_entity_t reader;
  dds_entity_t t;
  dds_return_t ret = DDS_RETCODE_OK;
  bool internal_topic;

  switch (topic)
  {
    case DDS_BUILTIN_TOPIC_DCPSPARTICIPANT:
    case DDS_BUILTIN_TOPIC_DCPSTOPIC:
    case DDS_BUILTIN_TOPIC_DCPSPUBLICATION:
    case DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION:
      internal_topic = true;
      subscriber = dds__get_builtin_subscriber (participant_or_subscriber);
      t = dds__get_builtin_topic (subscriber, topic);
      break;

    default: {
      dds_entity *p_or_s;
      if ((ret = dds_entity_pin (participant_or_subscriber, &p_or_s)) != DDS_RETCODE_OK)
        return ret;
      if (dds_entity_kind (p_or_s) == DDS_KIND_PARTICIPANT)
        subscriber = dds_create_subscriber (participant_or_subscriber, qos, NULL);
      else
        subscriber = participant_or_subscriber;
      dds_entity_unpin (p_or_s);
      internal_topic = false;
      t = topic;
      break;
    }
  }

  if ((ret = dds_subscriber_lock (subscriber, &sub)) != DDS_RETCODE_OK)
  {
    reader = ret;
    goto err_sub_lock;
  }

  if (subscriber != participant_or_subscriber && !internal_topic)
  {
    /* Delete implicit subscriber if reader creation fails */
    sub->m_entity.m_flags |= DDS_ENTITY_IMPLICIT;
  }

  if ((ret = dds_topic_lock (t, &tp)) != DDS_RETCODE_OK)
  {
    reader = ret;
    goto err_tp_lock;
  }
  assert (tp->m_stopic);
  /* FIXME: domain check */
  assert (sub->m_entity.m_domain == tp->m_entity.m_domain);

  /* Merge qos from topic and subscriber, dds_copy_qos only fails when it is passed a null
     argument, but that isn't the case here */
  rqos = dds_create_qos ();
  if (qos)
    nn_xqos_mergein_missing (rqos, qos, DDS_READER_QOS_MASK);
  if (sub->m_entity.m_qos)
    nn_xqos_mergein_missing (rqos, sub->m_entity.m_qos, ~(uint64_t)0);
  if (tp->m_entity.m_qos)
    nn_xqos_mergein_missing (rqos, tp->m_entity.m_qos, ~(uint64_t)0);
  nn_xqos_mergein_missing (rqos, &sub->m_entity.m_domain->gv.default_xqos_rd, ~(uint64_t)0);

  if ((ret = nn_xqos_valid (&sub->m_entity.m_domain->gv.logconfig, rqos)) != DDS_RETCODE_OK)
  {
    dds_delete_qos (rqos);
    reader = ret;
    goto err_bad_qos;
  }

  /* Additional checks required for built-in topics: we don't want to
     run into a resource limit on a built-in topic, it is a needless
     complication */
  if (internal_topic && !dds__validate_builtin_reader_qos (tp->m_entity.m_domain, topic, rqos))
  {
    dds_delete_qos (rqos);
    reader = DDS_RETCODE_INCONSISTENT_POLICY;
    goto err_bad_qos;
  }

  /* Create reader and associated read cache */
  rd = dds_alloc (sizeof (*rd));
  reader = dds_entity_init (&rd->m_entity, &sub->m_entity, DDS_KIND_READER, rqos, listener, DDS_READER_STATUS_MASK);
  rd->m_sample_rejected_status.last_reason = DDS_NOT_REJECTED;
  rd->m_topic = tp;
  rd->m_rhc = dds_rhc_default_new (rd, tp->m_stopic);
  dds_entity_add_ref_locked (&tp->m_entity);

  /* Extra claim of this reader to make sure that the delete waits until DDSI
     has deleted its reader as well. This can be known through the callback. */
  dds_handle_repin (&rd->m_entity.m_hdllink);

  ddsrt_mutex_unlock (&tp->m_entity.m_mutex);
  ddsrt_mutex_unlock (&sub->m_entity.m_mutex);

  thread_state_awake (lookup_thread_state (), &sub->m_entity.m_domain->gv);
  ret = new_reader (&rd->m_rd, &rd->m_entity.m_domain->gv, &rd->m_entity.m_guid, NULL, &sub->m_entity.m_participant->m_guid, tp->m_stopic, rqos, &rd->m_rhc->common.rhc, dds_reader_status_cb, rd);
  ddsrt_mutex_lock (&sub->m_entity.m_mutex);
  ddsrt_mutex_lock (&tp->m_entity.m_mutex);
  assert (ret == DDS_RETCODE_OK); /* FIXME: can be out-of-resources at the very least */
  thread_state_asleep (lookup_thread_state ());
  
  rd->m_entity.m_iid = get_entity_instance_id (&rd->m_entity.m_domain->gv, &rd->m_entity.m_guid);
  dds_entity_register_child (&sub->m_entity, &rd->m_entity);

  dds_topic_unlock (tp);
  dds_subscriber_unlock (sub);

  if (internal_topic)
  {
    /* If topic is builtin, then the topic entity is local and should be deleted because the application won't. */
    dds_delete (t);
  }
  return reader;

err_bad_qos:
  dds_topic_unlock (tp);
err_tp_lock:
  dds_subscriber_unlock (sub);
  if ((sub->m_entity.m_flags & DDS_ENTITY_IMPLICIT) != 0)
    (void) dds_delete (subscriber);
err_sub_lock:
  if (internal_topic)
    dds_delete (t);
  return reader;
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
  nn_guid_t pwrguid;
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
    nn_guid_t pwrguid_next;
    pwrguid = m->pwr_guid;
    if ((m_next = ddsrt_avl_find_succ (&rd_writers_treedef, &rd->writers, m)) != NULL)
      pwrguid_next = m_next->pwr_guid;
    else
    {
      memset (&pwrguid_next, 0xff, sizeof (pwrguid_next));
      pwrguid_next.entityid.u = (pwrguid_next.entityid.u & ~(uint32_t)0xff) | NN_ENTITYID_KIND_WRITER_NO_KEY;
    }
    ddsrt_mutex_unlock (&rd->e.lock);
    if ((pwr = ephash_lookup_proxy_writer_guid (dds_entity->m_domain->gv.guid_hash, &pwrguid)) != NULL)
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
