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
#include "ddsc/dds.h"
#include "dds__subscriber.h"
#include "dds__reader.h"
#include "dds__listener.h"
#include "dds__qos.h"
#include "dds__init.h"
#include "dds__rhc.h"
#include "dds__err.h"
#include "ddsi/q_entity.h"
#include "ddsi/q_thread.h"
#include "dds__report.h"
#include "dds__builtin.h"
#include "ddsc/ddsc_project.h"

#include <string.h>
#include "os/os.h"


#define DDS_READER_STATUS_MASK                                   \
                        DDS_SAMPLE_REJECTED_STATUS              |\
                        DDS_LIVELINESS_CHANGED_STATUS           |\
                        DDS_REQUESTED_DEADLINE_MISSED_STATUS    |\
                        DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS   |\
                        DDS_DATA_AVAILABLE_STATUS               |\
                        DDS_SAMPLE_LOST_STATUS                  |\
                        DDS_SUBSCRIPTION_MATCHED_STATUS

static dds_return_t
dds_reader_instance_hdl(
        dds_entity *e,
        dds_instance_handle_t *i)
{
    assert(e);
    assert(i);
    *i = (dds_instance_handle_t)reader_instance_id(&e->m_guid);
    return DDS_RETCODE_OK;
}

static dds_return_t
dds_reader_close(
        dds_entity *e)
{
    dds__retcode_t rc;
    dds_return_t ret = DDS_RETCODE_OK;
    struct thread_state1 * const thr = lookup_thread_state();
    const bool asleep = !vtime_awake_p(thr->vtime);

    assert(e);
    assert(thr);

    if (asleep) {
      thread_state_awake(thr);
    }
    if (delete_reader(&e->m_guid) != 0) {
        rc = DDS_RETCODE_ERROR;
        ret = DDS_ERRNO(rc, "Internal error");
    }
    if (asleep) {
      thread_state_asleep(thr);
    }
    return ret;
}

static dds_return_t
dds_reader_delete(
        dds_entity *e)
{
    dds_reader *rd = (dds_reader*)e;
    dds_return_t ret;
    assert(e);
    ret = dds_delete(rd->m_topic->m_entity.m_hdl);
    if(ret == DDS_RETCODE_OK){
        ret = dds_delete_impl(e->m_parent->m_hdl, true);
        if(dds_err_nr(ret) == DDS_RETCODE_ALREADY_DELETED){
            ret = DDS_RETCODE_OK;
        }
    }
    dds_free(rd->m_loan);
    return ret;
}

static dds_return_t
dds_reader_qos_validate(
        const dds_qos_t *qos,
        bool enabled)
{
    dds_return_t ret = DDS_RETCODE_OK;

    assert(qos);

    /* Check consistency. */
    if(!dds_qos_validate_common(qos)) {
        ret = DDS_ERRNO(DDS_RETCODE_ERROR, "Argument Qos is not valid");
    }
    if((qos->present & QP_USER_DATA) && !(validate_octetseq (&qos->user_data))) {
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY, "User data policy is inconsistent and caused an error");
    }
    if((qos->present & QP_PRISMTECH_READER_DATA_LIFECYCLE) && (validate_reader_data_lifecycle (&qos->reader_data_lifecycle) != 0)) {
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY, "Prismtech reader data lifecycle policy is inconsistent and caused an error");
    }
    if((qos->present & QP_TIME_BASED_FILTER) && (validate_duration (&qos->time_based_filter.minimum_separation) != 0)) {
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY, "Time based filter policy is inconsistent and caused an error");
    }
    if((qos->present & QP_HISTORY) && (qos->present & QP_RESOURCE_LIMITS) && (validate_history_and_resource_limits (&qos->history, &qos->resource_limits) != 0)) {
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY, "History and resource limits policy is inconsistent and caused an error");
    }
    if((qos->present & QP_TIME_BASED_FILTER) && (qos->present & QP_DEADLINE) && !(validate_deadline_and_timebased_filter (qos->deadline.deadline, qos->time_based_filter.minimum_separation))) {
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY, "Time based filter and deadline policy is inconsistent and caused an error");
    }
    if(ret == DDS_RETCODE_OK && enabled) {
        ret = dds_qos_validate_mutable_common(qos);
    }

    return ret;
}

static dds_return_t
dds_reader_qos_set(
        dds_entity *e,
        const dds_qos_t *qos,
        bool enabled)
{
    dds_return_t ret = dds_reader_qos_validate(qos, enabled);
    if (ret == DDS_RETCODE_OK) {
        if (enabled) {
            /* TODO: CHAM-95: DDSI does not support changing QoS policies. */
            ret = DDS_ERRNO(DDS_RETCODE_UNSUPPORTED, DDSC_PROJECT_NAME" does not support changing QoS policies");
        }
    }
    return ret;
}

static dds_return_t
dds_reader_status_validate(
        uint32_t mask)
{
    return (mask & ~(DDS_READER_STATUS_MASK)) ?
                     DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Invalid status mask") :
                     DDS_RETCODE_OK;
}

void
dds_reader_status_cb(
        void *entity,
        const status_cb_data_t *data)
{
    dds_reader *rd;
    dds__retcode_t rc;
    void *metrics = NULL;

    DDS_REPORT_STACK();

    /* When data is NULL, it means that the DDSI reader is deleted. */
    if (data == NULL) {
        /* Release the initial claim that was done during the create. This
         * will indicate that further API deletion is now possible. */
        ut_handle_release(((dds_entity*)entity)->m_hdl, ((dds_entity*)entity)->m_hdllink);
        return;
    }

    if (dds_reader_lock(((dds_entity*)entity)->m_hdl, &rd) != DDS_RETCODE_OK) {
        /* There's a deletion or closing going on. */
        DDS_REPORT_FLUSH(false);
        return;
    }
    assert(rd == entity);

    /* Reset the status for possible Listener call.
     * When a listener is not called, the status will be set (again). */
    dds_entity_status_reset(entity, data->status);

    /* Update status metrics. */
    switch (data->status) {
        case DDS_REQUESTED_DEADLINE_MISSED_STATUS: {
            rd->m_requested_deadline_missed_status.total_count++;
            rd->m_requested_deadline_missed_status.total_count_change++;
            rd->m_requested_deadline_missed_status.last_instance_handle = data->handle;
            metrics = (void*)&(rd->m_requested_deadline_missed_status);
            break;
        }
        case DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS: {
            rd->m_requested_incompatible_qos_status.total_count++;
            rd->m_requested_incompatible_qos_status.total_count_change++;
            rd->m_requested_incompatible_qos_status.last_policy_id = data->extra;
            metrics = (void*)&(rd->m_requested_incompatible_qos_status);
            break;
        }
        case DDS_SAMPLE_LOST_STATUS: {
            rd->m_sample_lost_status.total_count++;
            rd->m_sample_lost_status.total_count_change++;
            metrics = (void*)&(rd->m_sample_lost_status);
            break;
        }
        case DDS_SAMPLE_REJECTED_STATUS: {
            rd->m_sample_rejected_status.total_count++;
            rd->m_sample_rejected_status.total_count_change++;
            rd->m_sample_rejected_status.last_reason = data->extra;
            rd->m_sample_rejected_status.last_instance_handle = data->handle;
            metrics = (void*)&(rd->m_sample_rejected_status);
            break;
        }
        case DDS_DATA_AVAILABLE_STATUS: {
            metrics = NULL;
            break;
        }
        case DDS_LIVELINESS_CHANGED_STATUS: {
            if (data->add) {
                rd->m_liveliness_changed_status.alive_count++;
                rd->m_liveliness_changed_status.alive_count_change++;
                if (rd->m_liveliness_changed_status.not_alive_count > 0) {
                    rd->m_liveliness_changed_status.not_alive_count--;
                }
            } else {
                rd->m_liveliness_changed_status.alive_count--;
                rd->m_liveliness_changed_status.not_alive_count++;
                rd->m_liveliness_changed_status.not_alive_count_change++;
            }
            rd->m_liveliness_changed_status.last_publication_handle = data->handle;
            metrics = (void*)&(rd->m_liveliness_changed_status);
            break;
        }
        case DDS_SUBSCRIPTION_MATCHED_STATUS: {
            if (data->add) {
                rd->m_subscription_matched_status.total_count++;
                rd->m_subscription_matched_status.total_count_change++;
                rd->m_subscription_matched_status.current_count++;
                rd->m_subscription_matched_status.current_count_change++;
            } else {
                rd->m_subscription_matched_status.current_count--;
                rd->m_subscription_matched_status.current_count_change--;
            }
            rd->m_subscription_matched_status.last_publication_handle = data->handle;
            metrics = (void*)&(rd->m_subscription_matched_status);
            break;
        }
        default: assert (0);
    }

    /* The reader needs to be unlocked when propagating the (possible) listener
     * call because the application should be able to call this reader within
     * the callback function. */
    dds_reader_unlock(rd);

    /* DATA_AVAILABLE is handled differently to normal status changes. */
    if (data->status == DDS_DATA_AVAILABLE_STATUS) {
        dds_entity *parent = rd->m_entity.m_parent;
        /* First, try to ship it off to its parent(s) DDS_DATA_ON_READERS_STATUS. */
        rc = dds_entity_listener_propagation(parent, parent, DDS_DATA_ON_READERS_STATUS, NULL, true);

        if (rc == DDS_RETCODE_NO_DATA) {
            /* No parent was interested (NO_DATA == NO_CALL).
             * What about myself with DDS_DATA_AVAILABLE_STATUS? */
            rc = dds_entity_listener_propagation(entity, entity, DDS_DATA_AVAILABLE_STATUS, NULL, false);
        }

        if ( rc == DDS_RETCODE_NO_DATA ) {
            /* Nobody was interested (NO_DATA == NO_CALL). Set the status on the subscriber. */
            dds_entity_status_set(parent, DDS_DATA_ON_READERS_STATUS);
            /* Notify possible interested observers of the subscriber. */
            dds_entity_status_signal(parent);
        }
    } else {
        /* Is anybody interested within the entity hierarchy through listeners? */
        rc = dds_entity_listener_propagation(entity, entity, data->status, metrics, true);
    }

    if (rc == DDS_RETCODE_OK) {
        /* Event was eaten by a listener. */
        if (dds_reader_lock(((dds_entity*)entity)->m_hdl, &rd) == DDS_RETCODE_OK) {
            assert(rd == entity);

            /* Reset the change counts of the metrics. */
            switch (data->status) {
                case DDS_REQUESTED_DEADLINE_MISSED_STATUS: {
                    rd->m_requested_deadline_missed_status.total_count_change = 0;
                    break;
                }
                case DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS: {
                    rd->m_requested_incompatible_qos_status.total_count_change = 0;
                    break;
                }
                case DDS_SAMPLE_LOST_STATUS: {
                    rd->m_sample_lost_status.total_count_change = 0;
                    break;
                }
                case DDS_SAMPLE_REJECTED_STATUS: {
                    rd->m_sample_rejected_status.total_count_change = 0;
                    break;
                }
                case DDS_DATA_AVAILABLE_STATUS: {
                    /* Nothing to reset. */;
                    break;
                }
                case DDS_LIVELINESS_CHANGED_STATUS: {
                    rd->m_liveliness_changed_status.alive_count_change = 0;
                    rd->m_liveliness_changed_status.not_alive_count_change = 0;
                    break;
                }
                case DDS_SUBSCRIPTION_MATCHED_STATUS: {
                    rd->m_subscription_matched_status.total_count_change = 0;
                    rd->m_subscription_matched_status.current_count_change = 0;
                    break;
                }
                default: assert (0);
            }
            dds_reader_unlock(rd);
        } else {
            /* There's a deletion or closing going on. */
        }
    } else if (rc == DDS_RETCODE_NO_DATA) {
        /* Nobody was interested through a listener (NO_DATA == NO_CALL): set the status. */
        dds_entity_status_set(entity, data->status);
        /* Notify possible interested observers. */
        dds_entity_status_signal(entity);
        rc = DDS_RETCODE_OK;
    } else if (rc == DDS_RETCODE_ALREADY_DELETED) {
        /* An entity up the hierarchy is being deleted. */
        rc = DDS_RETCODE_OK;
    } else {
        /* Something went wrong up the hierarchy. */
    }

    DDS_REPORT_FLUSH(rc != DDS_RETCODE_OK);
}


_Pre_satisfies_(((participant_or_subscriber & DDS_ENTITY_KIND_MASK) == DDS_KIND_SUBSCRIBER ) ||\
                ((participant_or_subscriber & DDS_ENTITY_KIND_MASK) == DDS_KIND_PARTICIPANT) )
_Pre_satisfies_(((topic & DDS_ENTITY_KIND_MASK) == DDS_KIND_TOPIC   ) ||\
                ((topic & DDS_ENTITY_KIND_MASK) == DDS_KIND_INTERNAL) )
dds_entity_t
dds_create_reader(
        _In_ dds_entity_t participant_or_subscriber,
        _In_ dds_entity_t topic,
        _In_opt_ const dds_qos_t *qos,
        _In_opt_ const dds_listener_t *listener)
{
    dds_qos_t * rqos;
    dds__retcode_t rc;
    dds_entity * sub = NULL;
    dds_entity_t subscriber;
    dds_reader * rd;
    struct rhc * rhc;
    dds_entity * tp;
    dds_entity_t reader;
    dds_entity_t t;
    struct thread_state1 * const thr = lookup_thread_state ();
    const bool asleep = !vtime_awake_p (thr->vtime);
    dds_return_t ret = DDS_RETCODE_OK;

    DDS_REPORT_STACK();

    if (dds_entity_kind(topic) != DDS_KIND_INTERNAL) {
        /* Try claiming a participant. If that's not working, then it could be a subscriber. */
        if (dds_entity_kind(participant_or_subscriber) == DDS_KIND_PARTICIPANT) {
            subscriber = dds_create_subscriber(participant_or_subscriber, qos, NULL);
        } else {
            subscriber = participant_or_subscriber;
        }
        t = topic;
    } else {
        /* TODO If qos is provided, we need to compare with writer qos to determine compatibility */
        subscriber = dds__get_builtin_subscriber(participant_or_subscriber);
        t = dds__get_builtin_topic(subscriber, topic);
    }

    rc = dds_entity_lock(subscriber, DDS_KIND_SUBSCRIBER, &sub);
    if (rc != DDS_RETCODE_OK) {
        reader = DDS_ERRNO(rc, "Error occurred on locking subscriber");
        goto err_sub_lock;
    }

    if ((subscriber != participant_or_subscriber) &&
        (dds_entity_kind(topic) != DDS_KIND_INTERNAL)) {
        /* Delete implicit subscriber if reader creation fails */
        sub->m_flags |= DDS_ENTITY_IMPLICIT;
    }

    rc = dds_entity_lock(t, DDS_KIND_TOPIC, &tp);
    if (rc != DDS_RETCODE_OK) {
        reader = DDS_ERRNO(rc, "Error occurred on locking topic");
        goto err_tp_lock;
    }
    assert (((dds_topic*)tp)->m_stopic);
    assert (sub->m_domain == tp->m_domain);

    /* Merge qos from topic and subscriber */
    rqos = dds_qos_create ();
    if (qos) {
        /* Only returns failure when one of the qos args is NULL, which
         * is not the case here. */
        (void)dds_qos_copy(rqos, qos);
    }

    if(sub->m_qos){
        dds_qos_merge (rqos, sub->m_qos);
    }

    if (tp->m_qos) {
        dds_qos_merge (rqos, tp->m_qos);

        /* reset the following qos policies if set during topic qos merge as they aren't applicable for reader */
        rqos->present &= ~(QP_DURABILITY_SERVICE | QP_TRANSPORT_PRIORITY | QP_LIFESPAN);
    }
    nn_xqos_mergein_missing (rqos, &gv.default_xqos_rd);

    ret = dds_reader_qos_validate (rqos, false);
    if (ret != 0) {
        dds_qos_delete(rqos);
        reader = ret;
        goto err_bad_qos;
    }

    /* Create reader and associated read cache */
    rd = dds_alloc (sizeof (*rd));
    reader = dds_entity_init (&rd->m_entity, sub, DDS_KIND_READER, rqos, listener, DDS_READER_STATUS_MASK);
    rd->m_sample_rejected_status.last_reason = DDS_NOT_REJECTED;
    rd->m_topic = (dds_topic*)tp;
    rhc = dds_rhc_new (rd, ((dds_topic*)tp)->m_stopic);
    dds_entity_add_ref_nolock (tp);
    rd->m_entity.m_deriver.close = dds_reader_close;
    rd->m_entity.m_deriver.delete = dds_reader_delete;
    rd->m_entity.m_deriver.set_qos = dds_reader_qos_set;
    rd->m_entity.m_deriver.validate_status = dds_reader_status_validate;
    rd->m_entity.m_deriver.get_instance_hdl = dds_reader_instance_hdl;

    /* Extra claim of this reader to make sure that the delete waits until DDSI
     * has deleted its reader as well. This can be known through the callback. */
    if (ut_handle_claim(rd->m_entity.m_hdl, rd->m_entity.m_hdllink, DDS_KIND_READER, NULL) != UT_HANDLE_OK) {
        assert(0);
    }

    os_mutexUnlock(&tp->m_mutex);
    os_mutexUnlock(&sub->m_mutex);

    if (asleep) {
        thread_state_awake (thr);
    }
    rd->m_rd = new_reader(&rd->m_entity.m_guid, NULL, &sub->m_participant->m_guid, ((dds_topic*)tp)->m_stopic,
                          rqos, rhc, dds_reader_status_cb, rd);
    os_mutexLock(&sub->m_mutex);
    os_mutexLock(&tp->m_mutex);
    assert (rd->m_rd);
    if (asleep) {
        thread_state_asleep (thr);
    }

    /* For persistent data register reader with durability */
    if (dds_global.m_dur_reader && (rd->m_entity.m_qos->durability.kind > NN_TRANSIENT_LOCAL_DURABILITY_QOS)) {
        (dds_global.m_dur_reader) (rd, rhc);
    }
    dds_entity_unlock(tp);
    dds_entity_unlock(sub);

    if (dds_entity_kind(topic) == DDS_KIND_INTERNAL) {
        /* If topic is builtin, then the topic entity is local and should
         * be deleted because the application won't. */
        dds_delete(t);
    }

    DDS_REPORT_FLUSH(reader <= 0);
    return reader;

err_bad_qos:
    dds_entity_unlock(tp);
err_tp_lock:
    dds_entity_unlock(sub);
    if((sub->m_flags & DDS_ENTITY_IMPLICIT) != 0){
        (void)dds_delete(subscriber);
    }
err_sub_lock:
    DDS_REPORT_FLUSH(reader <= 0);
    return reader;
}

void
dds_reader_ddsi2direct(
        dds_entity_t entity,
        ddsi2direct_directread_cb_t cb,
        void *cbarg)
{
  dds_reader *dds_rd;

  if (ut_handle_claim(entity, NULL, DDS_KIND_READER, (void**)&dds_rd) == UT_HANDLE_OK)
  {
    struct reader *rd = dds_rd->m_rd;
    nn_guid_t pwrguid;
    struct proxy_writer *pwr;
    struct rd_pwr_match *m;
    memset (&pwrguid, 0, sizeof (pwrguid));
    os_mutexLock (&rd->e.lock);

    rd->ddsi2direct_cb = cb;
    rd->ddsi2direct_cbarg = cbarg;
    while ((m = ut_avlLookupSuccEq (&rd_writers_treedef, &rd->writers, &pwrguid)) != NULL)
    {
      /* have to be careful walking the tree -- pretty is different, but
       I want to check this before I write a lookup_succ function. */
      struct rd_pwr_match *m_next;
      nn_guid_t pwrguid_next;
      pwrguid = m->pwr_guid;
      if ((m_next = ut_avlFindSucc (&rd_writers_treedef, &rd->writers, m)) != NULL)
        pwrguid_next = m_next->pwr_guid;
      else
      {
        memset (&pwrguid_next, 0xff, sizeof (pwrguid_next));
        pwrguid_next.entityid.u = (pwrguid_next.entityid.u & ~0xff) | NN_ENTITYID_KIND_WRITER_NO_KEY;
      }
      os_mutexUnlock (&rd->e.lock);
      if ((pwr = ephash_lookup_proxy_writer_guid (&pwrguid)) != NULL)
      {
        os_mutexLock (&pwr->e.lock);
        pwr->ddsi2direct_cb = cb;
        pwr->ddsi2direct_cbarg = cbarg;
        os_mutexUnlock (&pwr->e.lock);
      }
      pwrguid = pwrguid_next;
      os_mutexLock (&rd->e.lock);
    }
    os_mutexUnlock (&rd->e.lock);
    ut_handle_release(entity, ((dds_entity*)rd)->m_hdllink);
  }
}

uint32_t
dds_reader_lock_samples(
        dds_entity_t reader)
{
    uint32_t ret = 0;
    dds_reader *rd;

    ret = dds_reader_lock(reader, &rd);
    if (ret == DDS_RETCODE_OK) {
        ret = dds_rhc_lock_samples(rd->m_rd->rhc);
        dds_reader_unlock(rd);
    } else {
        ret = 0;
    }
    return ret;
}

_Pre_satisfies_((reader & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER)
int
dds_reader_wait_for_historical_data(
        dds_entity_t reader,
        dds_duration_t max_wait)
{
    int ret;
    dds_reader *rd;

    DDS_REPORT_STACK();
    assert (reader);

    ret = dds_reader_lock(reader, &rd);
    if (ret == DDS_RETCODE_OK) {
        if (((dds_entity*)rd)->m_qos->durability.kind > NN_TRANSIENT_LOCAL_DURABILITY_QOS) {
            ret = (dds_global.m_dur_wait) (rd, max_wait);
        } else {
            ret = DDS_ERRNO(DDS_RETCODE_ERROR, "Can not wait for historical data on a reader with volatile durability");
        }
        dds_reader_unlock(rd);
    } else {
        ret = DDS_ERRNO(ret, "Error occurred on locking reader");
    }
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}

_Pre_satisfies_(((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER    ) || \
                ((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY) )
dds_entity_t
dds_get_subscriber(
        _In_ dds_entity_t entity)
{
    dds_entity_t hdl;

    DDS_REPORT_STACK();

    if (dds_entity_kind(entity) == DDS_KIND_READER) {
        hdl = dds_get_parent(entity);
    } else if (dds_entity_kind(entity) == DDS_KIND_COND_READ || dds_entity_kind(entity) == DDS_KIND_COND_QUERY) {
        hdl = dds_get_parent(entity);
        if(hdl > 0){
            hdl = dds_get_subscriber(hdl);
        } else {
            DDS_ERROR(hdl, "Reader of this condition is already deleted");
        }
    } else {
        hdl = DDS_ERRNO(dds_valid_hdl(entity, DDS_KIND_READER), "Provided entity is not a reader nor a condition");
    }
    DDS_REPORT_FLUSH(hdl <= 0);
    return hdl;
}

_Pre_satisfies_((reader & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER)
dds_return_t
dds_get_subscription_matched_status (
        _In_ dds_entity_t reader,
        _Out_opt_ dds_subscription_matched_status_t * status)
{
    dds__retcode_t rc;
    dds_reader *rd;
    dds_return_t ret = DDS_RETCODE_OK;

    DDS_REPORT_STACK();

    rc = dds_reader_lock(reader, &rd);
    if (rc != DDS_RETCODE_OK) {
        ret = DDS_ERRNO(rc, "Error occurred on locking reader");
        goto fail;
    }
    /* status = NULL, application do not need the status, but reset the counter & triggered bit */
    if (status) {
        *status = rd->m_subscription_matched_status;
    }
    if (((dds_entity*)rd)->m_status_enable & DDS_SUBSCRIPTION_MATCHED_STATUS) {
        rd->m_subscription_matched_status.total_count_change = 0;
        rd->m_subscription_matched_status.current_count_change = 0;
        dds_entity_status_reset(rd, DDS_SUBSCRIPTION_MATCHED_STATUS);
    }
    dds_reader_unlock(rd);
fail:
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}

_Pre_satisfies_((reader & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER)
dds_return_t
dds_get_liveliness_changed_status (
        _In_ dds_entity_t reader,
        _Out_opt_ dds_liveliness_changed_status_t * status)
{
    dds__retcode_t rc;
    dds_reader *rd;
    dds_return_t ret = DDS_RETCODE_OK;

    DDS_REPORT_STACK();

    rc = dds_reader_lock(reader, &rd);
    if (rc != DDS_RETCODE_OK) {
        ret = DDS_ERRNO(rc, "Error occurred on locking reader");
        goto fail;
    }
    /* status = NULL, application do not need the status, but reset the counter & triggered bit */
    if (status) {
        *status = rd->m_liveliness_changed_status;
    }
    if (((dds_entity*)rd)->m_status_enable & DDS_LIVELINESS_CHANGED_STATUS) {
        rd->m_liveliness_changed_status.alive_count_change = 0;
        rd->m_liveliness_changed_status.not_alive_count_change = 0;
        dds_entity_status_reset(rd, DDS_LIVELINESS_CHANGED_STATUS);
    }
    dds_reader_unlock(rd);
fail:
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}

_Pre_satisfies_((reader & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER)
dds_return_t dds_get_sample_rejected_status (
        _In_ dds_entity_t reader,
        _Out_opt_ dds_sample_rejected_status_t * status)
{
    dds__retcode_t rc;
    dds_reader *rd;
    dds_return_t ret = DDS_RETCODE_OK;

    DDS_REPORT_STACK();

    rc = dds_reader_lock(reader, &rd);
    if (rc != DDS_RETCODE_OK) {
        ret = DDS_ERRNO(rc, "Error occurred on locking reader");
        goto fail;
    }
    /* status = NULL, application do not need the status, but reset the counter & triggered bit */
    if (status) {
        *status = rd->m_sample_rejected_status;
    }
    if (((dds_entity*)rd)->m_status_enable & DDS_SAMPLE_REJECTED_STATUS) {
        rd->m_sample_rejected_status.total_count_change = 0;
        rd->m_sample_rejected_status.last_reason = DDS_NOT_REJECTED;
        dds_entity_status_reset(rd, DDS_SAMPLE_REJECTED_STATUS);
    }
    dds_reader_unlock(rd);
fail:
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}

_Pre_satisfies_((reader & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER)
dds_return_t dds_get_sample_lost_status (
        _In_ dds_entity_t reader,
        _Out_opt_ dds_sample_lost_status_t * status)
{
    dds__retcode_t rc;
    dds_reader *rd;
    dds_return_t ret = DDS_RETCODE_OK;

    DDS_REPORT_STACK();

    rc = dds_reader_lock(reader, &rd);
    if (rc != DDS_RETCODE_OK) {
        ret = DDS_ERRNO(rc, "Error occurred on locking reader");
        goto fail;
    }
    /* status = NULL, application do not need the status, but reset the counter & triggered bit */
    if (status) {
        *status = rd->m_sample_lost_status;
    }
    if (((dds_entity*)rd)->m_status_enable & DDS_SAMPLE_LOST_STATUS) {
        rd->m_sample_lost_status.total_count_change = 0;
        dds_entity_status_reset(rd, DDS_SAMPLE_LOST_STATUS);
    }
    dds_reader_unlock(rd);
fail:
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}

_Pre_satisfies_((reader & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER)
dds_return_t dds_get_requested_deadline_missed_status (
        _In_ dds_entity_t reader,
        _Out_opt_ dds_requested_deadline_missed_status_t * status)
{
    dds__retcode_t rc;
    dds_reader *rd;
    dds_return_t ret = DDS_RETCODE_OK;

    DDS_REPORT_STACK();
    rc = dds_reader_lock(reader, &rd);
    if (rc != DDS_RETCODE_OK) {
        ret = DDS_ERRNO(rc, "Error occurred on locking reader");
        goto fail;
    }
    /* status = NULL, application do not need the status, but reset the counter & triggered bit */
    if (status) {
        *status = rd->m_requested_deadline_missed_status;
    }
    if (((dds_entity*)rd)->m_status_enable & DDS_REQUESTED_DEADLINE_MISSED_STATUS) {
        rd->m_requested_deadline_missed_status.total_count_change = 0;
        dds_entity_status_reset(rd, DDS_REQUESTED_DEADLINE_MISSED_STATUS);
    }
    dds_reader_unlock(rd);
fail:
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}

_Pre_satisfies_((reader & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER)
dds_return_t dds_get_requested_incompatible_qos_status (
        _In_ dds_entity_t reader,
        _Out_opt_ dds_requested_incompatible_qos_status_t * status)
{
    dds__retcode_t rc;
    dds_reader *rd;
    dds_return_t ret = DDS_RETCODE_OK;

    DDS_REPORT_STACK();

    rc = dds_reader_lock(reader, &rd);
    if (rc != DDS_RETCODE_OK) {
        ret = DDS_ERRNO(rc, "Error occurred on locking reader");
        goto fail;
    }
    /* status = NULL, application do not need the status, but reset the counter & triggered bit */
    if (status) {
        *status = rd->m_requested_incompatible_qos_status;
    }
    if (((dds_entity*)rd)->m_status_enable & DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS) {
        rd->m_requested_incompatible_qos_status.total_count_change = 0;
        dds_entity_status_reset(rd, DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS);
    }
    dds_reader_unlock(rd);
fail:
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}
