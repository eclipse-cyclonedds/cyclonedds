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
#include "dds__err.h"
#include "dds__init.h"
#include "dds__publisher.h"
#include "dds__topic.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds__whc.h"

DECL_ENTITY_LOCK_UNLOCK(extern inline, dds_writer)

#define DDS_WRITER_STATUS_MASK                                   \
                        DDS_LIVELINESS_LOST_STATUS              |\
                        DDS_OFFERED_DEADLINE_MISSED_STATUS      |\
                        DDS_OFFERED_INCOMPATIBLE_QOS_STATUS     |\
                        DDS_PUBLICATION_MATCHED_STATUS

static dds_return_t
dds_writer_instance_hdl(
        dds_entity *e,
        dds_instance_handle_t *i)
{
    assert(e);
    assert(i);
    *i = (dds_instance_handle_t)writer_instance_id(&e->m_guid);
    return DDS_RETCODE_OK;
}

static dds_return_t
dds_writer_status_validate(
        uint32_t mask)
{
    dds_return_t ret = DDS_RETCODE_OK;

    if (mask & ~(DDS_WRITER_STATUS_MASK)) {
        DDS_ERROR("Invalid status mask\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
    }

    return ret;
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

static uint32_t
get_bandwidth_limit(
        nn_transport_priority_qospolicy_t transport_priority)
{
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  struct config_channel_listelem *channel = find_channel (transport_priority);
  return channel->data_bandwidth_limit;
#else
  (void)transport_priority;
  return 0;
#endif
}

static dds_return_t
dds_writer_close(
        dds_entity *e)
{
    dds_return_t ret = DDS_RETCODE_OK;
    dds_writer *wr = (dds_writer*)e;

    assert(e);

    thread_state_awake (lookup_thread_state ());
    nn_xpack_send (wr->m_xp, false);
    if (delete_writer (&e->m_guid) != 0) {
        DDS_ERROR("Internal error");
        ret = DDS_ERRNO(DDS_RETCODE_ERROR);
    }
    thread_state_asleep (lookup_thread_state ());
    return ret;
}

static dds_return_t
dds_writer_delete(
        dds_entity *e)
{
    dds_writer *wr = (dds_writer*)e;
    dds_return_t ret;
    /* FIXME: not freeing WHC here because it is owned by the DDSI entity */
    thread_state_awake (lookup_thread_state ());
    nn_xpack_free(wr->m_xp);
    thread_state_asleep (lookup_thread_state ());
    ret = dds_delete(wr->m_topic->m_entity.m_hdllink.hdl);
    if(ret == DDS_RETCODE_OK){
        ret = dds_delete_impl(e->m_parent->m_hdllink.hdl, true);
        if(dds_err_nr(ret) == DDS_RETCODE_BAD_PARAMETER){
            ret = DDS_RETCODE_OK;
        }
    }
    return ret;
}


static dds_return_t
dds_writer_qos_validate(
        const dds_qos_t *qos,
        bool enabled)
{
    dds_return_t ret = DDS_RETCODE_OK;

    assert(qos);

    /* Check consistency. */
    if(dds_qos_validate_common(qos) != true){
        DDS_ERROR("Provided inconsistent QoS policy\n");
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY);
    }
    if((qos->present & QP_USER_DATA) && validate_octetseq(&qos->user_data) != true){
        DDS_ERROR("User Data QoS policy is inconsistent and caused an error\n");
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY);
    }
    if ((qos->present & QP_DURABILITY_SERVICE) && validate_durability_service_qospolicy(&qos->durability_service) != 0){
        DDS_ERROR("Durability service QoS policy is inconsistent and caused an error\n");
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY);
    }
    if ((qos->present & QP_LIFESPAN) && validate_duration(&qos->lifespan.duration) != 0){
        DDS_ERROR("Lifespan QoS policy is inconsistent and caused an error\n");
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY);
    }
    if ((qos->present & QP_HISTORY) && (qos->present & QP_RESOURCE_LIMITS) && (validate_history_and_resource_limits(&qos->history, &qos->resource_limits) != 0)){
        DDS_ERROR("Resource limits QoS policy is inconsistent and caused an error\n");
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY);
    }
    if(ret == DDS_RETCODE_OK && enabled) {
        ret = dds_qos_validate_mutable_common(qos);
    }
    return ret;
}

static dds_return_t
dds_writer_qos_set(
        dds_entity *e,
        const dds_qos_t *qos,
        bool enabled)
{
    dds_return_t ret = dds_writer_qos_validate(qos, enabled);
    if (ret == DDS_RETCODE_OK) {
        /*
         * TODO: CHAM-95: DDSI does not support changing QoS policies.
         *
         * Only Ownership is required for the minimum viable product. This seems
         * to be the only QoS policy that DDSI supports changes on.
         */
        if (qos->present & QP_OWNERSHIP_STRENGTH) {
            dds_ownership_kind_t kind;
            /* check for ownership before updating, ownership strength is applicable only if
             * writer is exclusive */
            dds_qget_ownership (e->m_qos, &kind);

            if (kind == DDS_OWNERSHIP_EXCLUSIVE) {
                struct writer * ddsi_wr = ((dds_writer*)e)->m_wr;

                dds_qset_ownership_strength (e->m_qos, qos->ownership_strength.value);

                thread_state_awake (lookup_thread_state ());

                /* FIXME: with QoS changes being unsupported by the underlying stack I wonder what will happen; locking the underlying DDSI writer is of doubtful value as well */
                ddsrt_mutex_lock (&ddsi_wr->e.lock);
                if (qos->ownership_strength.value != ddsi_wr->xqos->ownership_strength.value) {
                    ddsi_wr->xqos->ownership_strength.value = qos->ownership_strength.value;
                }
                ddsrt_mutex_unlock (&ddsi_wr->e.lock);
                thread_state_asleep (lookup_thread_state ());
            } else {
                DDS_ERROR("Setting ownership strength doesn't make sense when the ownership is shared\n");
                ret = DDS_ERRNO(DDS_RETCODE_ERROR);
            }
        } else {
            if (enabled) {
                DDS_ERROR(DDS_PROJECT_NAME" does not support changing QoS policies yet\n");
                ret = DDS_ERRNO(DDS_RETCODE_UNSUPPORTED);
            }
        }
    }
    return ret;
}

static struct whc *make_whc(const dds_qos_t *qos)
{
  bool startup_mode;
  bool handle_as_transient_local;
  unsigned hdepth, tldepth;
  /* Startup mode causes the writer to treat data in its WHC as if
   transient-local, for the first few seconds after startup of the
   DDSI service. It is done for volatile reliable writers only
   (which automatically excludes all builtin writers) or for all
   writers except volatile best-effort & transient-local ones.

   Which one to use depends on whether merge policies are in effect
   in durability. If yes, then durability will take care of all
   transient & persistent data; if no, DDSI discovery usually takes
   too long and this'll save you.

   Note: may still be cleared, if it turns out we are not maintaining
   an index at all (e.g., volatile KEEP_ALL) */
  if (config.startup_mode_full) {
    startup_mode = gv.startup_mode &&
      (qos->durability.kind >= NN_TRANSIENT_DURABILITY_QOS ||
       (qos->durability.kind == NN_VOLATILE_DURABILITY_QOS &&
        qos->reliability.kind != NN_BEST_EFFORT_RELIABILITY_QOS));
  } else {
    startup_mode = gv.startup_mode &&
      (qos->durability.kind == NN_VOLATILE_DURABILITY_QOS &&
       qos->reliability.kind != NN_BEST_EFFORT_RELIABILITY_QOS);
  }

  /* Construct WHC -- if aggressive_keep_last1 is set, the WHC will
    drop all samples for which a later update is available.  This
    forces it to maintain a tlidx.  */
  handle_as_transient_local = (qos->durability.kind == NN_TRANSIENT_LOCAL_DURABILITY_QOS);
  if (!config.aggressive_keep_last_whc || qos->history.kind == NN_KEEP_ALL_HISTORY_QOS)
    hdepth = 0;
  else
    hdepth = (unsigned)qos->history.depth;
  if (handle_as_transient_local) {
    if (qos->durability_service.history.kind == NN_KEEP_ALL_HISTORY_QOS)
      tldepth = 0;
    else
      tldepth = (unsigned)qos->durability_service.history.depth;
  } else if (startup_mode) {
    tldepth = (hdepth == 0) ? 1 : hdepth;
  } else {
    tldepth = 0;
  }
  return whc_new (handle_as_transient_local, hdepth, tldepth);
}


dds_entity_t
dds_create_writer(
    dds_entity_t participant_or_publisher,
    dds_entity_t topic,
    const dds_qos_t *qos,
    const dds_listener_t *listener)
{
    dds_retcode_t rc;
    dds_qos_t * wqos;
    dds_writer * wr;
    dds_entity_t writer;
    dds_publisher * pub = NULL;
    dds_topic * tp;
    dds_entity_t publisher;
    ddsi_tran_conn_t conn = gv.data_conn_uc;
    dds_return_t ret;

    {
        dds_entity *p_or_p;
        if ((rc = dds_entity_claim (participant_or_publisher, &p_or_p)) != DDS_RETCODE_OK) {
            return DDS_ERRNO (rc);
        }
        if (dds_entity_kind (p_or_p) == DDS_KIND_PARTICIPANT) {
            publisher = dds_create_publisher(participant_or_publisher, qos, NULL);
        } else {
            publisher = participant_or_publisher;
        }
        dds_entity_release (p_or_p);
    }

    if ((rc = dds_publisher_lock(publisher, &pub)) != DDS_RETCODE_OK) {
        writer = DDS_ERRNO(rc);
        goto err_pub_lock;
    }

    if (publisher != participant_or_publisher) {
        pub->m_entity.m_flags |= DDS_ENTITY_IMPLICIT;
    }

    rc = dds_topic_lock(topic, &tp);
    if (rc != DDS_RETCODE_OK) {
        DDS_ERROR("Error occurred on locking topic\n");
        writer = DDS_ERRNO(rc);
        goto err_tp_lock;
    }
    assert(tp->m_stopic);
    assert(pub->m_entity.m_domain == tp->m_entity.m_domain);

    /* Merge Topic & Publisher qos */
    wqos = dds_create_qos();
    if (qos) {
        /* Only returns failure when one of the qos args is NULL, which
         * is not the case here. */
        (void)dds_copy_qos(wqos, qos);
    }

    if (pub->m_entity.m_qos) {
        dds_merge_qos(wqos, pub->m_entity.m_qos);
    }

    if (tp->m_entity.m_qos) {
        /* merge topic qos data to writer qos */
        dds_merge_qos(wqos, tp->m_entity.m_qos);
    }
    nn_xqos_mergein_missing(wqos, &gv.default_xqos_wr);

    ret = dds_writer_qos_validate(wqos, false);
    if (ret != 0) {
        dds_delete_qos(wqos);
        writer = ret;
        goto err_bad_qos;
    }

    /* Create writer */
    wr = dds_alloc(sizeof (*wr));
    writer = dds_entity_init(&wr->m_entity, &pub->m_entity, DDS_KIND_WRITER, wqos, listener, DDS_WRITER_STATUS_MASK);

    wr->m_topic = tp;
    dds_entity_add_ref_nolock(&tp->m_entity);
    wr->m_xp = nn_xpack_new(conn, get_bandwidth_limit(wqos->transport_priority), config.xpack_send_async);
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
    ret = new_writer(&wr->m_wr, &wr->m_entity.m_guid, NULL, &pub->m_entity.m_participant->m_guid, tp->m_stopic, wqos, wr->m_whc, dds_writer_status_cb, wr);
    ddsrt_mutex_lock (&pub->m_entity.m_mutex);
    ddsrt_mutex_lock (&tp->m_entity.m_mutex);
    assert(ret == DDS_RETCODE_OK);
    thread_state_asleep (lookup_thread_state ());
    dds_topic_unlock(tp);
    dds_publisher_unlock(pub);
    return writer;

err_bad_qos:
    dds_topic_unlock(tp);
err_tp_lock:
    dds_publisher_unlock(pub);
    if((pub->m_entity.m_flags & DDS_ENTITY_IMPLICIT) != 0){
        (void)dds_delete(publisher);
    }
err_pub_lock:
    return writer;
}

dds_entity_t
dds_get_publisher(
    dds_entity_t writer)
{
  dds_entity *e;
  dds_retcode_t rc;
  if ((rc = dds_entity_claim (writer, &e)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);
  else
  {
    dds_entity_t pubh;
    if (dds_entity_kind (e) != DDS_KIND_WRITER)
      pubh = DDS_ERRNO (DDS_RETCODE_ILLEGAL_OPERATION);
    else
    {
      assert (dds_entity_kind (e->m_parent) == DDS_KIND_PUBLISHER);
      pubh = e->m_parent->m_hdllink.hdl;
    }
    dds_entity_release (e);
    return pubh;
  }
}

dds_return_t
dds_get_publication_matched_status (
    dds_entity_t writer,
    dds_publication_matched_status_t * status)
{
    dds_retcode_t rc;
    dds_writer *wr;
    dds_return_t ret = DDS_RETCODE_OK;

    rc = dds_writer_lock(writer, &wr);
    if (rc != DDS_RETCODE_OK) {
        DDS_ERROR("Error occurred on locking writer\n");
        ret = DDS_ERRNO(rc);
        goto fail;
    }
    /* status = NULL, application do not need the status, but reset the counter & triggered bit */
    if (status) {
        *status = wr->m_publication_matched_status;
    }
    ddsrt_mutex_lock (&wr->m_entity.m_observers_lock);
    if (wr->m_entity.m_status_enable & DDS_PUBLICATION_MATCHED_STATUS) {
        wr->m_publication_matched_status.total_count_change = 0;
        wr->m_publication_matched_status.current_count_change = 0;
        dds_entity_status_reset(&wr->m_entity, DDS_PUBLICATION_MATCHED_STATUS);
    }
    ddsrt_mutex_unlock (&wr->m_entity.m_observers_lock);
    dds_writer_unlock(wr);
fail:
    return ret;
}

dds_return_t
dds_get_liveliness_lost_status (
    dds_entity_t writer,
    dds_liveliness_lost_status_t * status)
{
    dds_retcode_t rc;
    dds_writer *wr;
    dds_return_t ret = DDS_RETCODE_OK;

    rc = dds_writer_lock(writer, &wr);
    if (rc != DDS_RETCODE_OK) {
        DDS_ERROR("Error occurred on locking writer\n");
        ret = DDS_ERRNO(rc);
        goto fail;
    }
    /* status = NULL, application do not need the status, but reset the counter & triggered bit */
    if (status) {
      *status = wr->m_liveliness_lost_status;
    }
    ddsrt_mutex_lock (&wr->m_entity.m_observers_lock);
    if (wr->m_entity.m_status_enable & DDS_LIVELINESS_LOST_STATUS) {
      wr->m_liveliness_lost_status.total_count_change = 0;
      dds_entity_status_reset(&wr->m_entity, DDS_LIVELINESS_LOST_STATUS);
    }
    ddsrt_mutex_unlock (&wr->m_entity.m_observers_lock);
    dds_writer_unlock(wr);
fail:
    return ret;
}

dds_return_t
dds_get_offered_deadline_missed_status(
    dds_entity_t writer,
    dds_offered_deadline_missed_status_t *status)
{
    dds_retcode_t rc;
    dds_writer *wr;
    dds_return_t ret = DDS_RETCODE_OK;

    rc = dds_writer_lock(writer, &wr);
    if (rc != DDS_RETCODE_OK) {
        DDS_ERROR("Error occurred on locking writer\n");
        ret = DDS_ERRNO(rc);
        goto fail;
    }
    /* status = NULL, application do not need the status, but reset the counter & triggered bit */
    if (status) {
      *status = wr->m_offered_deadline_missed_status;
    }
    ddsrt_mutex_lock (&wr->m_entity.m_observers_lock);
    if (wr->m_entity.m_status_enable & DDS_OFFERED_DEADLINE_MISSED_STATUS) {
      wr->m_offered_deadline_missed_status.total_count_change = 0;
      dds_entity_status_reset(&wr->m_entity, DDS_OFFERED_DEADLINE_MISSED_STATUS);
    }
    ddsrt_mutex_unlock (&wr->m_entity.m_observers_lock);
    dds_writer_unlock(wr);
fail:
    return ret;
}

dds_return_t
dds_get_offered_incompatible_qos_status (
    dds_entity_t writer,
    dds_offered_incompatible_qos_status_t * status)
{
    dds_retcode_t rc;
    dds_writer *wr;
    dds_return_t ret = DDS_RETCODE_OK;

    rc = dds_writer_lock(writer, &wr);
    if (rc != DDS_RETCODE_OK) {
        DDS_ERROR("Error occurred on locking writer\n");
        ret = DDS_ERRNO(rc);
        goto fail;
    }
    /* status = NULL, application do not need the status, but reset the counter & triggered bit */
    if (status) {
      *status = wr->m_offered_incompatible_qos_status;
    }
    ddsrt_mutex_lock (&wr->m_entity.m_observers_lock);
    if (wr->m_entity.m_status_enable & DDS_OFFERED_INCOMPATIBLE_QOS_STATUS) {
      wr->m_offered_incompatible_qos_status.total_count_change = 0;
      dds_entity_status_reset(&wr->m_entity, DDS_OFFERED_INCOMPATIBLE_QOS_STATUS);
    }
    ddsrt_mutex_unlock (&wr->m_entity.m_observers_lock);
    dds_writer_unlock(wr);
fail:
    return ret;
}
