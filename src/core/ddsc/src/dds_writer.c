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

#include "dds/dds.h"
#include "dds/version.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_globals.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds__writer.h"
#include "dds__listener.h"
#include "dds__init.h"
#include "dds__publisher.h"
#include "dds__topic.h"
#include "dds__get_status.h"
#include "dds__qos.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds__whc.h"

DECL_ENTITY_LOCK_UNLOCK (extern inline, dds_writer)

#define DDS_WRITER_STATUS_MASK                                   \
                        (DDS_LIVELINESS_LOST_STATUS              |\
                         DDS_OFFERED_DEADLINE_MISSED_STATUS      |\
                         DDS_OFFERED_INCOMPATIBLE_QOS_STATUS     |\
                         DDS_PUBLICATION_MATCHED_STATUS)

static dds_return_t dds_writer_status_validate (uint32_t mask)
{
  return (mask & ~DDS_WRITER_STATUS_MASK) ? DDS_RETCODE_BAD_PARAMETER : DDS_RETCODE_OK;
}

/*
  Handler function for all write related status callbacks. May trigger status
  condition or call listener on writer. Each entity has a mask of
  supported status types. According to DDS specification, if listener is called
  then status conditions is not triggered.
*/

static void dds_writer_status_cb (void *ventity, const status_cb_data_t *data)
{
  dds_writer * const wr = ventity;

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

  struct dds_listener const * const lst = &wr->m_entity.m_listener;
  enum dds_status_id status_id = (enum dds_status_id) data->raw_status_id;
  bool invoke = false;
  void *vst = NULL;
  int32_t *reset[2] = { NULL, NULL };

  /* FIXME: why wait if no listener is set? */
  ddsrt_mutex_lock (&wr->m_entity.m_observers_lock);
  while (wr->m_entity.m_cb_count > 0)
    ddsrt_cond_wait (&wr->m_entity.m_observers_cond, &wr->m_entity.m_observers_lock);

  /* Reset the status for possible Listener call.
   * When a listener is not called, the status will be set (again). */
  dds_entity_status_reset (&wr->m_entity, (status_mask_t) (1u << status_id));

  /* Update status metrics. */
  switch (status_id)
  {
    case DDS_OFFERED_DEADLINE_MISSED_STATUS_ID: {
      struct dds_offered_deadline_missed_status * const st = vst = &wr->m_offered_deadline_missed_status;
      st->total_count++;
      st->total_count_change++;
      st->last_instance_handle = data->handle;
      invoke = (lst->on_offered_deadline_missed != 0);
      reset[0] = &st->total_count_change;
      break;
    }
    case DDS_LIVELINESS_LOST_STATUS_ID: {
      struct dds_liveliness_lost_status * const st = vst = &wr->m_liveliness_lost_status;
      st->total_count++;
      st->total_count_change++;
      invoke = (lst->on_liveliness_lost != 0);
      reset[0] = &st->total_count_change;
      break;
    }
    case DDS_OFFERED_INCOMPATIBLE_QOS_STATUS_ID: {
      struct dds_offered_incompatible_qos_status * const st = vst = &wr->m_offered_incompatible_qos_status;
      st->total_count++;
      st->total_count_change++;
      st->last_policy_id = data->extra;
      invoke = (lst->on_offered_incompatible_qos != 0);
      reset[0] = &st->total_count_change;
      break;
    }
    case DDS_PUBLICATION_MATCHED_STATUS_ID: {
      struct dds_publication_matched_status * const st = vst = &wr->m_publication_matched_status;
      if (data->add) {
        st->total_count++;
        st->total_count_change++;
        st->current_count++;
        st->current_count_change++;
      } else {
        st->current_count--;
        st->current_count_change--;
      }
      wr->m_publication_matched_status.last_subscription_handle = data->handle;
      invoke = (lst->on_publication_matched != 0);
      reset[0] = &st->total_count_change;
      reset[1] = &st->current_count_change;
      break;
    }
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

  const uint32_t enabled = (ddsrt_atomic_ld32 (&wr->m_entity.m_status.m_status_and_mask) & ((1u << status_id) << SAM_ENABLED_SHIFT));
  if (enabled == 0)
  {
    /* Don't invoke listeners or set status flag if masked */
  }
  else if (invoke)
  {
    wr->m_entity.m_cb_pending_count++;
    wr->m_entity.m_cb_count++;
    ddsrt_mutex_unlock (&wr->m_entity.m_observers_lock);
    dds_entity_invoke_listener (&wr->m_entity, status_id, vst);
    ddsrt_mutex_lock (&wr->m_entity.m_observers_lock);
    wr->m_entity.m_cb_count--;
    wr->m_entity.m_cb_pending_count--;
    *reset[0] = 0;
    if (reset[1])
      *reset[1] = 0;
  }
  else
  {
    dds_entity_status_set (&wr->m_entity, (status_mask_t) (1u << status_id));
  }

  ddsrt_cond_broadcast (&wr->m_entity.m_observers_cond);
  ddsrt_mutex_unlock (&wr->m_entity.m_observers_lock);
}

static uint32_t get_bandwidth_limit (dds_transport_priority_qospolicy_t transport_priority)
{
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  struct config_channel_listelem *channel = find_channel (&config, transport_priority);
  return channel->data_bandwidth_limit;
#else
  (void) transport_priority;
  return 0;
#endif
}

static void dds_writer_interrupt (dds_entity *e) ddsrt_nonnull_all;

static void dds_writer_interrupt (dds_entity *e)
{
  struct q_globals * const gv = &e->m_domain->gv;
  thread_state_awake (lookup_thread_state (), gv);
  unblock_throttled_writer (gv, &e->m_guid);
  thread_state_asleep (lookup_thread_state ());
}

static void dds_writer_close (dds_entity *e) ddsrt_nonnull_all;

static void dds_writer_close (dds_entity *e)
{
  struct dds_writer * const wr = (struct dds_writer *) e;
  struct q_globals * const gv = &e->m_domain->gv;
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
  /* FIXME: not freeing WHC here because it is owned by the DDSI entity */
  thread_state_awake (lookup_thread_state (), &e->m_domain->gv);
  nn_xpack_free (wr->m_xp);
  thread_state_asleep (lookup_thread_state ());
  dds_entity_drop_ref (&wr->m_topic->m_entity);
  return DDS_RETCODE_OK;
}

static dds_return_t validate_writer_qos (const dds_qos_t *wqos)
{
#ifndef DDSI_INCLUDE_LIFESPAN
  if (wqos != NULL && (wqos->present & QP_LIFESPAN) && wqos->lifespan.duration != DDS_INFINITY)
    return DDS_RETCODE_BAD_PARAMETER;
#else
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

static struct whc *make_whc (struct dds_domain *dom, const dds_qos_t *qos)
{
  bool handle_as_transient_local;
  uint32_t hdepth, tldepth;
  /* Construct WHC -- if aggressive_keep_last1 is set, the WHC will
     drop all samples for which a later update is available.  This
     forces it to maintain a tlidx.  */
  handle_as_transient_local = (qos->durability.kind == DDS_DURABILITY_TRANSIENT_LOCAL);
  if (qos->history.kind == DDS_HISTORY_KEEP_ALL)
    hdepth = 0;
  else
    hdepth = (unsigned) qos->history.depth;
  if (!handle_as_transient_local)
    tldepth = 0;
  else
  {
    if (qos->durability_service.history.kind == DDS_HISTORY_KEEP_ALL)
      tldepth = 0;
    else
      tldepth = (unsigned) qos->durability_service.history.depth;
  }
  return whc_new (&dom->gv, handle_as_transient_local, hdepth, tldepth);
}

const struct dds_entity_deriver dds_entity_deriver_writer = {
  .interrupt = dds_writer_interrupt,
  .close = dds_writer_close,
  .delete = dds_writer_delete,
  .set_qos = dds_writer_qos_set,
  .validate_status = dds_writer_status_validate
};

dds_entity_t dds_create_writer (dds_entity_t participant_or_publisher, dds_entity_t topic, const dds_qos_t *qos, const dds_listener_t *listener)
{
  dds_return_t rc;
  dds_qos_t *wqos;
  dds_writer *wr;
  dds_entity_t writer;
  dds_publisher *pub = NULL;
  dds_participant *pp;
  dds_topic *tp;
  dds_entity_t publisher;

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
        break;
      default:
        dds_entity_unlock (p_or_p);
        return DDS_RETCODE_ILLEGAL_OPERATION;
    }
  }

  ddsi_tran_conn_t conn = pub->m_entity.m_domain->gv.data_conn_uc;

  if ((rc = dds_topic_lock (topic, &tp)) != DDS_RETCODE_OK)
    goto err_tp_lock;
  assert (tp->m_stopic);

  pp = dds_entity_participant (&pub->m_entity);
  if (pp != dds_entity_participant (&tp->m_entity))
  {
    rc = DDS_RETCODE_BAD_PARAMETER;
    goto err_pp_mismatch;
  }

  /* Merge Topic & Publisher qos */
  wqos = dds_create_qos ();
  if (qos)
    nn_xqos_mergein_missing (wqos, qos, DDS_WRITER_QOS_MASK);
  if (pub->m_entity.m_qos)
    nn_xqos_mergein_missing (wqos, pub->m_entity.m_qos, ~(uint64_t)0);
  if (tp->m_entity.m_qos)
    nn_xqos_mergein_missing (wqos, tp->m_entity.m_qos, ~(uint64_t)0);
  nn_xqos_mergein_missing (wqos, &pub->m_entity.m_domain->gv.default_xqos_wr, ~(uint64_t)0);

  if ((rc = nn_xqos_valid (&pub->m_entity.m_domain->gv.logconfig, wqos)) < 0 ||
      (rc = validate_writer_qos(wqos)) != DDS_RETCODE_OK)
  {
    dds_delete_qos(wqos);
    goto err_bad_qos;
  }

  /* Create writer */
  wr = dds_alloc (sizeof (*wr));
  writer = dds_entity_init (&wr->m_entity, &pub->m_entity, DDS_KIND_WRITER, false, wqos, listener, DDS_WRITER_STATUS_MASK);

  wr->m_topic = tp;
  dds_entity_add_ref_locked (&tp->m_entity);
  wr->m_xp = nn_xpack_new (conn, get_bandwidth_limit (wqos->transport_priority), pub->m_entity.m_domain->gv.config.xpack_send_async);
  wr->m_whc = make_whc (pub->m_entity.m_domain, wqos);
  wr->whc_batch = pub->m_entity.m_domain->gv.config.whc_batch;

  thread_state_awake (lookup_thread_state (), &pub->m_entity.m_domain->gv);
  rc = new_writer (&wr->m_wr, &wr->m_entity.m_domain->gv, &wr->m_entity.m_guid, NULL, &pp->m_entity.m_guid, tp->m_stopic, wqos, wr->m_whc, dds_writer_status_cb, wr);
  assert(rc == DDS_RETCODE_OK);
  thread_state_asleep (lookup_thread_state ());

  wr->m_entity.m_iid = get_entity_instance_id (&wr->m_entity.m_domain->gv, &wr->m_entity.m_guid);
  dds_entity_register_child (&pub->m_entity, &wr->m_entity);

  dds_entity_init_complete (&wr->m_entity);
  dds_topic_unlock (tp);
  dds_publisher_unlock (pub);
  return writer;

err_bad_qos:
err_pp_mismatch:
  dds_topic_unlock (tp);
err_tp_lock:
  dds_publisher_unlock (pub);
  if ((pub->m_entity.m_flags & DDS_ENTITY_IMPLICIT) != 0)
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

DDS_GET_STATUS(writer, publication_matched, PUBLICATION_MATCHED, total_count_change, current_count_change)
DDS_GET_STATUS(writer, liveliness_lost, LIVELINESS_LOST, total_count_change)
DDS_GET_STATUS(writer, offered_deadline_missed, OFFERED_DEADLINE_MISSED, total_count_change)
DDS_GET_STATUS(writer, offered_incompatible_qos, OFFERED_INCOMPATIBLE_QOS, total_count_change)
