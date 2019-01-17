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
#include <string.h>
#include "dds__listener.h"
#include "dds__qos.h"
#include "dds__err.h"
#include "ddsi/q_entity.h"
#include "ddsc/ddsc_project.h"

#define DDS_SUBSCRIBER_STATUS_MASK                               \
                        DDS_DATA_ON_READERS_STATUS

static dds_return_t
dds_subscriber_instance_hdl(
        dds_entity *e,
        dds_instance_handle_t *i)
{
    (void)e;
    (void)i;
    /* TODO: Get/generate proper handle. */
    DDS_ERROR("Generating subscriber instance handle is not supported");
    return DDS_ERRNO(DDS_RETCODE_UNSUPPORTED);
}

static dds_return_t
dds__subscriber_qos_validate(
        _In_ const dds_qos_t *qos,
        _In_ bool enabled)
{
    dds_return_t ret = DDS_RETCODE_OK;

    assert(qos);

    if((qos->present & QP_GROUP_DATA) && !validate_octetseq(&qos->group_data)) {
        DDS_ERROR("Group data policy is inconsistent and caused an error\n");
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY);
    }
    if((qos->present & QP_PARTITION) && !validate_stringseq(&qos->partition)) {
        DDS_ERROR("Partition policy is inconsistent and caused an error\n");
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY);
    }
    if((qos->present & QP_PRESENTATION) && validate_presentation_qospolicy(&qos->presentation)) {
        DDS_ERROR("Presentation policy is inconsistent and caused an error\n");
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY);
    }
    if((qos->present & QP_PRISMTECH_ENTITY_FACTORY) && !validate_entityfactory_qospolicy(&qos->entity_factory)) {
        DDS_ERROR("Prismtech entity factory policy is inconsistent and caused an error\n");
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY);
    }
    if(ret == DDS_RETCODE_OK && enabled && (qos->present & QP_PRESENTATION)) {
        /* TODO: Improve/check immutable check. */
        DDS_ERROR("Presentation QoS policy is immutable\n");
        ret = DDS_ERRNO(DDS_RETCODE_IMMUTABLE_POLICY);
    }

    return ret;
}

static dds_return_t
dds_subscriber_qos_set(
        dds_entity *e,
        const dds_qos_t *qos,
        bool enabled)
{
    dds_return_t ret = dds__subscriber_qos_validate(qos, enabled);
    (void)e;
    if (ret == DDS_RETCODE_OK) {
        if (enabled) {
            /* TODO: CHAM-95: DDSI does not support changing QoS policies. */
            DDS_ERROR(DDSC_PROJECT_NAME" does not support changing QoS policies yet\n");
            ret = DDS_ERRNO(DDS_RETCODE_UNSUPPORTED);
        }
    }
    return ret;
}

static dds_return_t
dds_subscriber_status_validate(
        uint32_t mask)
{
    dds_return_t ret = DDS_RETCODE_OK;

    if (mask & ~(DDS_SUBSCRIBER_STATUS_MASK)) {
        DDS_ERROR("Invalid status mask\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
    }

    return ret;
}

_Requires_exclusive_lock_held_(participant)
_Check_return_ dds_entity_t
dds__create_subscriber_l(
        _Inout_  dds_entity *participant, /* entity-lock must be held */
        _In_opt_ const dds_qos_t *qos,
        _In_opt_ const dds_listener_t *listener)
{
    dds_subscriber * sub;
    dds_entity_t subscriber;
    dds_return_t ret;
    dds_qos_t * new_qos;

    /* Validate qos */
    if (qos) {
        if ((ret = dds__subscriber_qos_validate(qos, false)) != DDS_RETCODE_OK) {
            goto err_param;
        }
        new_qos = dds_create_qos();
        /* Only returns failure when one of the qos args is NULL, which
         * is not the case here. */
        (void)dds_copy_qos(new_qos, qos);
    } else {
        new_qos = NULL;
    }

    /* Create subscriber */
    sub = dds_alloc(sizeof(*sub));
    subscriber = dds_entity_init(&sub->m_entity, participant, DDS_KIND_SUBSCRIBER, new_qos, listener, DDS_SUBSCRIBER_STATUS_MASK);
    sub->m_entity.m_deriver.set_qos = dds_subscriber_qos_set;
    sub->m_entity.m_deriver.validate_status = dds_subscriber_status_validate;
    sub->m_entity.m_deriver.get_instance_hdl = dds_subscriber_instance_hdl;

    return subscriber;

    /* Error handling */
err_param:
    return ret;
}

_Pre_satisfies_((participant & DDS_ENTITY_KIND_MASK) == DDS_KIND_PARTICIPANT)
_Must_inspect_result_ dds_entity_t
dds_create_subscriber(
        _In_     dds_entity_t participant,
        _In_opt_ const dds_qos_t *qos,
        _In_opt_ const dds_listener_t *listener)
{
    dds_entity * par;
    dds_entity_t hdl;
    dds__retcode_t errnr;

    errnr = dds_entity_lock(participant, DDS_KIND_PARTICIPANT, &par);
    if (errnr != DDS_RETCODE_OK) {
        DDS_ERROR("Error occurred on locking participant\n");
        hdl = DDS_ERRNO(errnr);
        return hdl;
    }

    hdl = dds__create_subscriber_l(par, qos, listener);
    dds_entity_unlock(par);

    return hdl;
}

_Pre_satisfies_((subscriber & DDS_ENTITY_KIND_MASK) == DDS_KIND_SUBSCRIBER)
dds_return_t
dds_notify_readers(
        _In_ dds_entity_t subscriber)
{
    dds_entity *iter;
    dds_entity *sub;
    dds__retcode_t errnr;
    dds_return_t ret;

    errnr = dds_entity_lock(subscriber, DDS_KIND_SUBSCRIBER, &sub);
    if (errnr == DDS_RETCODE_OK) {
        errnr = DDS_RETCODE_UNSUPPORTED;
        DDS_ERROR("Unsupported operation\n");
        ret = DDS_ERRNO(errnr);
        iter = sub->m_children;
        while (iter) {
            os_mutexLock(&iter->m_mutex);
            // TODO: check if reader has data available, call listener
            os_mutexUnlock(&iter->m_mutex);
            iter = iter->m_next;
        }
        dds_entity_unlock(sub);
    } else {
        DDS_ERROR("Error occurred on locking subscriber\n");
        ret = DDS_ERRNO(errnr);
    }

    return ret;
}

dds_return_t
dds_subscriber_begin_coherent(
        _In_ dds_entity_t e)
{
    /* TODO: CHAM-124 Currently unsupported. */
    (void)e;
    DDS_ERROR("Using coherency to get a coherent data set is not currently being supported\n");
    return DDS_ERRNO(DDS_RETCODE_UNSUPPORTED);
}

dds_return_t
dds_subscriber_end_coherent(
        _In_ dds_entity_t e)
{
    /* TODO: CHAM-124 Currently unsupported. */
    (void)e;
    DDS_ERROR("Using coherency to get a coherent data set is not currently being supported\n");
    return DDS_ERRNO(DDS_RETCODE_UNSUPPORTED);
}

