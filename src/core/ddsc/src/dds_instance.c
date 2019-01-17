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
#include "dds__entity.h"
#include "dds__write.h"
#include "dds__writer.h"
#include "dds__rhc.h"
#include "dds__err.h"
#include "ddsi/ddsi_tkmap.h"
#include "ddsi/ddsi_serdata.h"
#include "ddsi/q_entity.h"
#include "ddsi/q_thread.h"


_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
dds_return_t
dds_writedispose(
       _In_ dds_entity_t writer,
       _In_ const void *data)
{
    return dds_writedispose_ts(writer, data, dds_time());
}

_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
dds_return_t
dds_dispose(
       _In_ dds_entity_t writer,
       _In_ const void *data)
{
    return dds_dispose_ts(writer, data, dds_time());
}

_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
dds_return_t
dds_dispose_ih(
       _In_ dds_entity_t writer,
       _In_ dds_instance_handle_t handle)
{
    return dds_dispose_ih_ts(writer, handle, dds_time());
}

static struct ddsi_tkmap_instance*
dds_instance_find(
        _In_ const dds_topic *topic,
        _In_ const void *data,
        _In_ const bool create)
{
    struct ddsi_serdata *sd = ddsi_serdata_from_sample (topic->m_stopic, SDK_KEY, data);
    struct ddsi_tkmap_instance * inst = ddsi_tkmap_find (sd, false, create);
    ddsi_serdata_unref (sd);
    return inst;
}

static void
dds_instance_remove(
        _In_     const dds_topic *topic,
        _In_opt_ const void *data,
        _In_     dds_instance_handle_t handle)
{
    struct ddsi_tkmap_instance * inst;

    if (handle != DDS_HANDLE_NIL) {
        inst = ddsi_tkmap_find_by_id (gv.m_tkmap, handle);
    } else {
        assert (data);
        inst = dds_instance_find (topic, data, false);
    }
    if (inst) {
        ddsi_tkmap_instance_unref (inst);
    }
}

static const dds_topic *dds_instance_info (dds_entity *e)
{
  const dds_topic *topic;
  switch (dds_entity_kind (e))
  {
    case DDS_KIND_READER:
      topic = ((dds_reader*) e)->m_topic;
      break;
    case DDS_KIND_WRITER:
      topic = ((dds_writer*) e)->m_topic;
      break;
    default:
      assert (0);
      topic = NULL;
  }
  return topic;
}

static const dds_topic * dds_instance_info_by_hdl (dds_entity_t e)
{
    const dds_topic * topic = NULL;
    dds__retcode_t rc;
    dds_entity *w_or_r;

    rc = dds_entity_lock(e, DDS_KIND_WRITER, &w_or_r);
    if (rc == DDS_RETCODE_ILLEGAL_OPERATION) {
        rc = dds_entity_lock(e, DDS_KIND_READER, &w_or_r);
    }
    if (rc == DDS_RETCODE_OK) {
        topic = dds_instance_info(w_or_r);
        dds_entity_unlock(w_or_r);
    } else {
        DDS_ERROR("Error occurred on locking entity");
    }
    return topic;
}

_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
dds_return_t
dds_register_instance(
        _In_ dds_entity_t writer,
        _Out_ dds_instance_handle_t *handle,
        _In_ const void *data)
{
    struct thread_state1 * const thr = lookup_thread_state();
    const bool asleep = !vtime_awake_p(thr->vtime);
    struct ddsi_tkmap_instance * inst;
    dds_writer *wr;
    dds_return_t ret;
    dds__retcode_t rc;

    if(data == NULL){
        DDS_ERROR("Argument data is NULL\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        goto err;
    }
    if(handle == NULL){
        DDS_ERROR("Argument handle is NULL\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        goto err;
    }
    rc = dds_writer_lock(writer, &wr);
    if (rc != DDS_RETCODE_OK) {
        DDS_ERROR("Error occurred on locking writer\n");
        ret = DDS_ERRNO(rc);
        goto err;
    }
    if (asleep) {
        thread_state_awake(thr);
    }
    inst = dds_instance_find (wr->m_topic, data, true);
    if(inst != NULL){
        *handle = inst->m_iid;
        ret = DDS_RETCODE_OK;
    } else {
        DDS_ERROR("Unable to create instance\n");
        ret = DDS_ERRNO(DDS_RETCODE_ERROR);
    }
    if (asleep) {
        thread_state_asleep(thr);
    }
    dds_writer_unlock(wr);
err:
    return ret;
}

_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
dds_return_t
dds_unregister_instance(
        _In_ dds_entity_t writer,
        _In_opt_ const void *data)
{
    return dds_unregister_instance_ts (writer, data, dds_time());
}

_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
dds_return_t
dds_unregister_instance_ih(
       _In_ dds_entity_t writer,
       _In_opt_ dds_instance_handle_t handle)
{
    return dds_unregister_instance_ih_ts(writer, handle, dds_time());
}

_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
dds_return_t
dds_unregister_instance_ts(
       _In_ dds_entity_t writer,
       _In_opt_ const void *data,
       _In_ dds_time_t timestamp)
{
    struct thread_state1 * const thr = lookup_thread_state();
    const bool asleep = !vtime_awake_p(thr->vtime);
    dds_return_t ret = DDS_RETCODE_OK;
    dds__retcode_t rc;
    bool autodispose = true;
    dds_write_action action = DDS_WR_ACTION_UNREGISTER;
    dds_writer *wr;

    if (data == NULL){
        DDS_ERROR("Argument data is NULL\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        goto err;
    }
    if(timestamp < 0){
        DDS_ERROR("Argument timestamp has negative value\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        goto err;
    }
    rc = dds_writer_lock(writer, &wr);
    if (rc != DDS_RETCODE_OK) {
        DDS_ERROR("Error occurred on locking writer\n");
        ret =  DDS_ERRNO(rc);
        goto err;
    }

    if (wr->m_entity.m_qos) {
        dds_qget_writer_data_lifecycle (wr->m_entity.m_qos, &autodispose);
    }
    if (asleep) {
        thread_state_awake(thr);
    }
    if (autodispose) {
        dds_instance_remove (wr->m_topic, data, DDS_HANDLE_NIL);
        action |= DDS_WR_DISPOSE_BIT;
    }
    ret = dds_write_impl (wr, data, timestamp, action);
    if (asleep) {
        thread_state_asleep(thr);
    }
    dds_writer_unlock(wr);
err:
    return ret;
}

_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
dds_return_t
dds_unregister_instance_ih_ts(
       _In_ dds_entity_t writer,
       _In_opt_ dds_instance_handle_t handle,
       _In_ dds_time_t timestamp)
{
    struct thread_state1 * const thr = lookup_thread_state();
    const bool asleep = !vtime_awake_p(thr->vtime);
    dds_return_t ret = DDS_RETCODE_OK;
    dds__retcode_t rc;
    bool autodispose = true;
    dds_write_action action = DDS_WR_ACTION_UNREGISTER;
    dds_writer *wr;
    struct ddsi_tkmap_instance *tk;

    rc = dds_writer_lock(writer, &wr);
    if (rc != DDS_RETCODE_OK) {
        DDS_ERROR("Error occurred on locking writer\n");
        ret = DDS_ERRNO(rc);
        goto err;
    }

    if (wr->m_entity.m_qos) {
        dds_qget_writer_data_lifecycle (wr->m_entity.m_qos, &autodispose);
    }
    if (autodispose) {
        dds_instance_remove (wr->m_topic, NULL, handle);
        action |= DDS_WR_DISPOSE_BIT;
    }

    if (asleep) {
        thread_state_awake(thr);
    }
    tk = ddsi_tkmap_find_by_id (gv.m_tkmap, handle);
    if (tk) {
        struct ddsi_sertopic *tp = wr->m_topic->m_stopic;
        void *sample = ddsi_sertopic_alloc_sample (tp);
        ddsi_serdata_topicless_to_sample (tp, tk->m_sample, sample, NULL, NULL);
        ddsi_tkmap_instance_unref (tk);
        ret = dds_write_impl (wr, sample, timestamp, action);
        ddsi_sertopic_free_sample (tp, sample, DDS_FREE_ALL);
    } else {
        DDS_ERROR("No instance related with the provided handle is found\n");
        ret = DDS_ERRNO(DDS_RETCODE_PRECONDITION_NOT_MET);
    }
    if (asleep) {
        thread_state_asleep(thr);
    }
    dds_writer_unlock(wr);
err:
    return ret;
}

_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
dds_return_t
dds_writedispose_ts(
       _In_ dds_entity_t writer,
       _In_ const void *data,
       _In_ dds_time_t timestamp)
{
    dds_return_t ret;
    dds__retcode_t rc;
    dds_writer *wr;

    rc = dds_writer_lock(writer, &wr);
    if (rc == DDS_RETCODE_OK) {
        struct thread_state1 * const thr = lookup_thread_state();
        const bool asleep = !vtime_awake_p(thr->vtime);
        if (asleep) {
            thread_state_awake(thr);
        }
        ret = dds_write_impl (wr, data, timestamp, DDS_WR_ACTION_WRITE_DISPOSE);
        if (ret == DDS_RETCODE_OK) {
            dds_instance_remove (wr->m_topic, data, DDS_HANDLE_NIL);
        }
        if (asleep) {
            thread_state_asleep(thr);
        }
        dds_writer_unlock(wr);
    } else {
        DDS_ERROR("Error occurred on locking writer\n");
        ret = DDS_ERRNO(rc);
    }

    return ret;
}

static dds_return_t
dds_dispose_impl(
       _In_ dds_writer *wr,
       _In_ const void *data,
       _In_ dds_instance_handle_t handle,
       _In_ dds_time_t timestamp)
{
    dds_return_t ret;
    assert(vtime_awake_p(lookup_thread_state()->vtime));
    assert(wr);
    ret = dds_write_impl(wr, data, timestamp, DDS_WR_ACTION_DISPOSE);
    if (ret == DDS_RETCODE_OK) {
        dds_instance_remove (wr->m_topic, data, handle);
    }
    return ret;
}

_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
dds_return_t
dds_dispose_ts(
       _In_ dds_entity_t writer,
       _In_ const void *data,
       _In_ dds_time_t timestamp)
{
    dds_return_t ret;
    dds__retcode_t rc;
    dds_writer *wr;

    rc = dds_writer_lock(writer, &wr);
    if (rc == DDS_RETCODE_OK) {
        struct thread_state1 * const thr = lookup_thread_state();
        const bool asleep = !vtime_awake_p(thr->vtime);
        if (asleep) {
            thread_state_awake(thr);
        }
        ret = dds_dispose_impl(wr, data, DDS_HANDLE_NIL, timestamp);
        if (asleep) {
            thread_state_asleep(thr);
        }
        dds_writer_unlock(wr);
    } else {
        DDS_ERROR("Error occurred on locking writer\n");
        ret = DDS_ERRNO(rc);
    }

    return ret;
}

_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
dds_return_t
dds_dispose_ih_ts(
       _In_ dds_entity_t writer,
       _In_ dds_instance_handle_t handle,
       _In_ dds_time_t timestamp)
{
    dds_return_t ret;
    dds__retcode_t rc;
    dds_writer *wr;

    rc = dds_writer_lock(writer, &wr);
    if (rc == DDS_RETCODE_OK) {
        struct thread_state1 * const thr = lookup_thread_state();
        const bool asleep = !vtime_awake_p(thr->vtime);
        struct ddsi_tkmap_instance *tk;
        if (asleep) {
            thread_state_awake(thr);
        }
        if ((tk = ddsi_tkmap_find_by_id (gv.m_tkmap, handle)) != NULL) {
            struct ddsi_sertopic *tp = wr->m_topic->m_stopic;
            void *sample = ddsi_sertopic_alloc_sample (tp);
            ddsi_serdata_topicless_to_sample (tp, tk->m_sample, sample, NULL, NULL);
            ddsi_tkmap_instance_unref (tk);
            ret = dds_dispose_impl (wr, sample, handle, timestamp);
            ddsi_sertopic_free_sample (tp, sample, DDS_FREE_ALL);
        } else {
            DDS_ERROR("No instance related with the provided handle is found\n");
            ret = DDS_ERRNO(DDS_RETCODE_PRECONDITION_NOT_MET);
        }
        if (asleep) {
            thread_state_asleep(thr);
        }
        dds_writer_unlock(wr);
    } else {
        DDS_ERROR("Error occurred on locking writer\n");
        ret = DDS_ERRNO(rc);
    }

    return ret;
}

_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
dds_instance_handle_t
dds_lookup_instance(
        dds_entity_t entity,
        const void *data)
{
    dds_instance_handle_t ih = DDS_HANDLE_NIL;
    const dds_topic * topic;
    struct ddsi_tkmap * map = gv.m_tkmap;
    struct ddsi_serdata *sd;

    if(data == NULL){
        DDS_ERROR("Argument data is NULL\n");
        goto err;
    }

    topic = dds_instance_info_by_hdl (entity);
    if (topic) {
        struct thread_state1 * const thr = lookup_thread_state();
        const bool asleep = !vtime_awake_p(thr->vtime);
        if (asleep) {
            thread_state_awake(thr);
        }
        sd = ddsi_serdata_from_sample (topic->m_stopic, SDK_KEY, data);
        ih = ddsi_tkmap_lookup (map, sd);
        ddsi_serdata_unref (sd);
        if (asleep) {
            thread_state_asleep(thr);
        }
    } else {
        DDS_ERROR("Acquired topic is NULL\n");
    }
err:
    return ih;
}

_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
dds_instance_handle_t
dds_instance_lookup (
        dds_entity_t entity,
        const void *data)
{
    return dds_lookup_instance(entity, data);
}

_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
dds_return_t
dds_instance_get_key(
        dds_entity_t entity,
        dds_instance_handle_t ih,
        void *data)
{
    struct thread_state1 * const thr = lookup_thread_state();
    const bool asleep = !vtime_awake_p(thr->vtime);
    dds_return_t ret;
    const dds_topic * topic;
    struct ddsi_tkmap_instance * tk;

    if(data == NULL){
        DDS_ERROR("Argument data is NULL\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        goto err;
    }

    topic = dds_instance_info_by_hdl (entity);
    if(topic == NULL){
        DDS_ERROR("Could not find topic related to the given entity\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        goto err;
    }
    if (asleep) {
        thread_state_awake(thr);
    }
    if ((tk = ddsi_tkmap_find_by_id(gv.m_tkmap, ih)) != NULL) {
        ddsi_sertopic_zero_sample (topic->m_stopic, data);
        ddsi_serdata_topicless_to_sample (topic->m_stopic, tk->m_sample, data, NULL, NULL);
        ddsi_tkmap_instance_unref (tk);
        ret = DDS_RETCODE_OK;
    } else {
        DDS_ERROR("No instance related with the provided entity is found\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
    }
    if (asleep) {
        thread_state_asleep(thr);
    }
err:
    return ret;
}
