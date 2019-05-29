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
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_globals.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds__writer.h"
#include "dds__listener.h"
#include "dds__qos.h"
#include "dds__init.h"
#include "dds__publisher.h"
#include "dds__topic.h"
#include "dds__get_status.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds__whc.h"

DECL_ENTITY_LOCK_UNLOCK (extern inline, dds_writer)

#define DDS_WRITER_STATUS_MASK                                   \
                        (DDS_LIVELINESS_LOST_STATUS              |\
                         DDS_OFFERED_DEADLINE_MISSED_STATUS      |\
                         DDS_OFFERED_INCOMPATIBLE_QOS_STATUS     |\
                         DDS_PUBLICATION_MATCHED_STATUS)

static dds_return_t dds_writer_instance_hdl (dds_entity *e, dds_instance_handle_t *i) ddsrt_nonnull_all;

static dds_return_t dds_writer_instance_hdl (dds_entity *e, dds_instance_handle_t *i)
{
  *i = writer_instance_id(&e->m_guid);
  return DDS_RETCODE_OK;
}

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
  struct dds_entity * const entity = ventity;

  /* When data is NULL, it means that the writer is deleted. */
  if (data == NULL)
  {
    /* Release the initial claim that was done during the create. This
     * will indicate that further API deletion is now possible. */
    dds_handle_release (&entity->m_hdllink);
    return;
  }

  struct dds_listener const * const lst = &entity->m_listener;
  enum dds_status_id status_id = (enum dds_status_id) data->raw_status_id;
  bool invoke = false;
  void *vst = NULL;
  int32_t *reset[2] = { NULL, NULL };

  ddsrt_mutex_lock (&entity->m_observers_lock);
  while (entity->m_cb_count > 0)
    ddsrt_cond_wait (&entity->m_observers_cond, &entity->m_observers_lock);
  entity->m_cb_count++;

  /* Reset the status for possible Listener call.
   * When a listener is not called, the status will be set (again). */
  dds_entity_status_reset (entity, 1u << status_id);

  /* Update status metrics. */
  dds_writer * const wr = (dds_writer *) entity;
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

  if (invoke)
  {
    ddsrt_mutex_unlock (&entity->m_observers_lock);
    dds_entity_invoke_listener(entity, status_id, vst);
    ddsrt_mutex_lock (&entity->m_observers_lock);
    *reset[0] = 0;
    if (reset[1])
      *reset[1] = 0;
  }
  else
  {
    dds_entity_status_set (entity, 1u << status_id);
  }

  entity->m_cb_count--;
  ddsrt_cond_broadcast (&entity->m_observers_cond);
  ddsrt_mutex_unlock (&entity->m_observers_lock);
}

static uint32_t get_bandwidth_limit (dds_transport_priority_qospolicy_t transport_priority)
{
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  struct config_channel_listelem *channel = find_channel (transport_priority);
  return channel->data_bandwidth_limit;
#else
  (void) transport_priority;
  return 0;
#endif
}

static dds_return_t dds_writer_close (dds_entity *e) ddsrt_nonnull_all;

static dds_return_t dds_writer_close (dds_entity *e)
{
  dds_writer * const wr = (dds_writer *) e;
  dds_return_t ret;
  thread_state_awake (lookup_thread_state ());
  nn_xpack_send (wr->m_xp, false);
  if ((ret = delete_writer (&e->m_guid)) < 0)
    ret = DDS_RETCODE_ERROR;
  thread_state_asleep (lookup_thread_state ());
  return ret;
}

static dds_return_t dds_writer_delete (dds_entity *e) ddsrt_nonnull_all;

static dds_return_t dds_writer_delete (dds_entity *e)
{
  dds_writer * const wr = (dds_writer *) e;
  dds_return_t ret;
  /* FIXME: not freeing WHC here because it is owned by the DDSI entity */
  thread_state_awake (lookup_thread_state ());
  nn_xpack_free (wr->m_xp);
  thread_state_asleep (lookup_thread_state ());
  if ((ret = dds_delete (wr->m_topic->m_entity.m_hdllink.hdl)) == DDS_RETCODE_OK)
  {
    ret = dds_delete_impl (e->m_parent->m_hdllink.hdl, true);
    if (ret == DDS_RETCODE_BAD_PARAMETER)
      ret = DDS_RETCODE_OK;
  }
  return ret;
}

static dds_return_t dds_writer_qos_validate (const dds_qos_t *qos, bool enabled)
{
  dds_return_t ret;
  if ((ret = nn_xqos_valid (qos)) < 0)
    return ret;
  return enabled ? dds_qos_validate_mutable_common (qos) : DDS_RETCODE_OK;
}

static dds_return_t dds_writer_qos_set (dds_entity *e, const dds_qos_t *qos, bool enabled)
{
  /* FIXME: QoS changes */
  dds_return_t ret;

  if ((ret = dds_writer_qos_validate (qos, enabled)) != DDS_RETCODE_OK)
    return ret;

  /* Sort-of support updating ownership strength */
  if ((qos->present & QP_OWNERSHIP_STRENGTH) && (qos->present & ~QP_OWNERSHIP_STRENGTH) == 0)
  {
    dds_ownership_kind_t kind;
    dds_qget_ownership (e->m_qos, &kind);

    if (kind != DDS_OWNERSHIP_EXCLUSIVE)
      return DDS_RETCODE_ERROR;

    struct writer *ddsi_wr = ((dds_writer *) e)->m_wr;
    dds_qset_ownership_strength (e->m_qos, qos->ownership_strength.value);

    thread_state_awake (lookup_thread_state ());

    /* FIXME: with QoS changes being unsupported by the underlying stack I wonder what will happen; locking the underlying DDSI writer is of doubtful value as well */
    ddsrt_mutex_lock (&ddsi_wr->e.lock);
    ddsi_wr->xqos->ownership_strength.value = qos->ownership_strength.value;
    ddsrt_mutex_unlock (&ddsi_wr->e.lock);
    thread_state_asleep (lookup_thread_state ());
  }
  else
  {
    if (enabled)
      ret = DDS_RETCODE_UNSUPPORTED;
  }
  return ret;
}

static struct whc *make_whc (const dds_qos_t *qos)
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
  return whc_new (handle_as_transient_local, hdepth, tldepth);
}

dds_entity_t dds_create_writer (dds_entity_t participant_or_publisher, dds_entity_t topic, const dds_qos_t *qos, const dds_listener_t *listener)
{
  dds_return_t rc;
  dds_qos_t *wqos;
  dds_writer *wr;
  dds_entity_t writer;
  dds_publisher *pub = NULL;
  dds_topic *tp;
  dds_entity_t publisher;
  ddsi_tran_conn_t conn = gv.data_conn_uc;

  {
    dds_entity *p_or_p;
    if ((rc = dds_entity_claim (participant_or_publisher, &p_or_p)) != DDS_RETCODE_OK)
      return rc;
    if (dds_entity_kind (p_or_p) == DDS_KIND_PARTICIPANT)
      publisher = dds_create_publisher(participant_or_publisher, qos, NULL);
    else
      publisher = participant_or_publisher;
    dds_entity_release (p_or_p);
  }

  if ((rc = dds_publisher_lock (publisher, &pub)) != DDS_RETCODE_OK)
    return rc;

  if (publisher != participant_or_publisher)
    pub->m_entity.m_flags |= DDS_ENTITY_IMPLICIT;

  if ((rc = dds_topic_lock (topic, &tp)) != DDS_RETCODE_OK)
    goto err_tp_lock;

  assert (tp->m_stopic);
  assert (pub->m_entity.m_domain == tp->m_entity.m_domain);

  /* Merge Topic & Publisher qos */
#define DDS_QOSMASK_WRITER (QP_USER_DATA | QP_DURABILITY | QP_DURABILITY_SERVICE | QP_DEADLINE | QP_LATENCY_BUDGET | QP_OWNERSHIP | QP_OWNERSHIP_STRENGTH | QP_LIVELINESS | QP_RELIABILITY | QP_TRANSPORT_PRIORITY | QP_LIFESPAN | QP_DESTINATION_ORDER | QP_HISTORY | QP_RESOURCE_LIMITS | QP_PRISMTECH_WRITER_DATA_LIFECYCLE | QP_CYCLONE_IGNORELOCAL)
  wqos = dds_create_qos ();
  if (qos)
    nn_xqos_mergein_missing (wqos, qos, DDS_QOSMASK_WRITER);
  if (pub->m_entity.m_qos)
    nn_xqos_mergein_missing (wqos, pub->m_entity.m_qos, ~(uint64_t)0);
  if (tp->m_entity.m_qos)
    nn_xqos_mergein_missing (wqos, tp->m_entity.m_qos, ~(uint64_t)0);
  nn_xqos_mergein_missing (wqos, &gv.default_xqos_wr, ~(uint64_t)0);

  if ((rc = dds_writer_qos_validate (wqos, false)) != DDS_RETCODE_OK)
  {
    dds_delete_qos(wqos);
    goto err_bad_qos;
  }

  /* Create writer */
  wr = dds_alloc (sizeof (*wr));
  writer = dds_entity_init (&wr->m_entity, &pub->m_entity, DDS_KIND_WRITER, wqos, listener, DDS_WRITER_STATUS_MASK);

  wr->m_topic = tp;
  dds_entity_add_ref_nolock (&tp->m_entity);
  wr->m_xp = nn_xpack_new (conn, get_bandwidth_limit (wqos->transport_priority), config.xpack_send_async);
  wr->m_entity.m_deriver.close = dds_writer_close;
  wr->m_entity.m_deriver.delete = dds_writer_delete;
  wr->m_entity.m_deriver.set_qos = dds_writer_qos_set;
  wr->m_entity.m_deriver.validate_status = dds_writer_status_validate;
  wr->m_entity.m_deriver.get_instance_hdl = dds_writer_instance_hdl;
  wr->m_whc = make_whc (wqos);

  /* Extra claim of this writer to make sure that the delete waits until DDSI
   * has deleted its writer as well. This can be known through the callback. */
  dds_handle_claim_inc (&wr->m_entity.m_hdllink);

  ddsrt_mutex_unlock (&tp->m_entity.m_mutex);
  ddsrt_mutex_unlock (&pub->m_entity.m_mutex);

  thread_state_awake (lookup_thread_state ());
  rc = new_writer (&wr->m_wr, &wr->m_entity.m_guid, NULL, &pub->m_entity.m_participant->m_guid, tp->m_stopic, wqos, wr->m_whc, dds_writer_status_cb, wr);
  ddsrt_mutex_lock (&pub->m_entity.m_mutex);
  ddsrt_mutex_lock (&tp->m_entity.m_mutex);
  assert(rc == DDS_RETCODE_OK);
  thread_state_asleep (lookup_thread_state ());
  dds_topic_unlock (tp);
  dds_publisher_unlock (pub);
  return writer;

err_bad_qos:
  dds_topic_unlock (tp);
err_tp_lock:
  dds_publisher_unlock (pub);
  if ((pub->m_entity.m_flags & DDS_ENTITY_IMPLICIT) != 0){
    (void )dds_delete (publisher);
  }
  return rc;
}

dds_entity_t dds_get_publisher (dds_entity_t writer)
{
  dds_entity *e;
  dds_return_t rc;
  if ((rc = dds_entity_claim (writer, &e)) != DDS_RETCODE_OK)
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
    dds_entity_release (e);
    return pubh;
  }
}

DDS_GET_STATUS(writer, publication_matched, PUBLICATION_MATCHED, total_count_change, current_count_change)
DDS_GET_STATUS(writer, liveliness_lost, LIVELINESS_LOST, total_count_change)
DDS_GET_STATUS(writer, offered_deadline_missed, OFFERED_DEADLINE_MISSED, total_count_change)
DDS_GET_STATUS(writer, offered_incompatible_qos, OFFERED_INCOMPATIBLE_QOS, total_count_change)
