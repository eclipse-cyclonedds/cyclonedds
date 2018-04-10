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
#include "dds__listener.h"
#include "dds__qos.h"
#include "dds__err.h"
#include "ddsi/q_entity.h"
#include "dds__report.h"
#include "ddsc/ddsc_project.h"

#define DDS_PUBLISHER_STATUS_MASK   0

static dds_return_t
dds_publisher_instance_hdl(
        dds_entity *e,
        dds_instance_handle_t *i)
{
    assert(e);
    assert(i);
    /* TODO: Get/generate proper handle. */
    return DDS_ERRNO(DDS_RETCODE_UNSUPPORTED, "Getting publisher instance handle is not supported");
}

static dds_return_t
dds_publisher_qos_validate(
        _In_ const dds_qos_t *qos,
        _In_ bool enabled)
{
    dds_return_t ret = DDS_RETCODE_OK;
    assert(qos);

    /* Check consistency. */
    if((qos->present & QP_GROUP_DATA) && !validate_octetseq(&qos->group_data)){
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY, "Group data policy is inconsistent and caused an error");
    }
    if((qos->present & QP_PRESENTATION) && (validate_presentation_qospolicy(&qos->presentation) != 0)){
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY, "Presentation policy is inconsistent and caused an error");
    }
    if((qos->present & QP_PARTITION) && !validate_stringseq(&qos->partition)){
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY, "Partition policy is inconsistent and caused an error");
    }
    if((qos->present & QP_PRISMTECH_ENTITY_FACTORY) && !validate_entityfactory_qospolicy(&qos->entity_factory)){
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY, "Prismtech entity factory policy is inconsistent and caused an error");
    }
    if(ret == DDS_RETCODE_OK && enabled && (qos->present & QP_PRESENTATION)){
        /* TODO: Improve/check immutable check. */
        ret = DDS_ERRNO(DDS_RETCODE_IMMUTABLE_POLICY, "Presentation policy is immutable");
    }
    return ret;
}

static dds_return_t
dds_publisher_qos_set(
        dds_entity *e,
        const dds_qos_t *qos,
        bool enabled)
{
    dds_return_t ret = dds_publisher_qos_validate(qos, enabled);
    if (ret == DDS_RETCODE_OK) {
        if (enabled) {
            /* TODO: CHAM-95: DDSI does not support changing QoS policies. */
            ret = DDS_ERRNO(DDS_RETCODE_UNSUPPORTED, DDSC_PROJECT_NAME" does not support changing QoS policies yet");
        }
    }
    return ret;
}

static dds_return_t dds_publisher_status_validate (uint32_t mask)
{
    return (mask & ~(DDS_PUBLISHER_STATUS_MASK)) ?
                     DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Invalid status mask") :
                     DDS_RETCODE_OK;
}

_Pre_satisfies_((participant & DDS_ENTITY_KIND_MASK) == DDS_KIND_PARTICIPANT)
_Must_inspect_result_ dds_entity_t
dds_create_publisher(
        _In_     dds_entity_t participant,
        _In_opt_ const dds_qos_t *qos,
        _In_opt_ const dds_listener_t *listener)
{
    dds_entity * par;
    dds_publisher * pub;
    dds_entity_t hdl;
    dds_qos_t * new_qos = NULL;
    dds_return_t ret;
    dds__retcode_t rc;

    DDS_REPORT_STACK();

    rc = dds_entity_lock(participant, DDS_KIND_PARTICIPANT, &par);
    if (rc != DDS_RETCODE_OK) {
        hdl = DDS_ERRNO(rc, "Error occurred on locking participant");
        goto lock_err;
    }

    /* Validate qos */
    if (qos) {
        ret = dds_publisher_qos_validate(qos, false);
        if (ret != DDS_RETCODE_OK) {
            hdl = ret;
            goto qos_err;
        }
        new_qos = dds_qos_create ();
        /* Only returns failure when one of the qos args is NULL, which
         * is not the case here. */
        (void)dds_qos_copy(new_qos, qos);
    }

    /* Create publisher */
    pub = dds_alloc (sizeof (*pub));
    hdl = dds_entity_init (&pub->m_entity, par, DDS_KIND_PUBLISHER, new_qos, listener, DDS_PUBLISHER_STATUS_MASK);
    pub->m_entity.m_deriver.set_qos = dds_publisher_qos_set;
    pub->m_entity.m_deriver.get_instance_hdl = dds_publisher_instance_hdl;
    pub->m_entity.m_deriver.validate_status = dds_publisher_status_validate;

qos_err:
    dds_entity_unlock(par);
lock_err:
    DDS_REPORT_FLUSH(hdl <= 0);
    return hdl;
}


_Pre_satisfies_((publisher & DDS_ENTITY_KIND_MASK) == DDS_KIND_PUBLISHER)
DDS_EXPORT dds_return_t
dds_suspend(
        _In_ dds_entity_t publisher)
{
    dds_return_t ret;

    DDS_REPORT_STACK();

    if(dds_entity_kind(publisher) != DDS_KIND_PUBLISHER) {
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Provided entity is not a publisher kind");
        goto err;
    }
    /* TODO: CHAM-123 Currently unsupported. */
    ret = DDS_ERRNO(DDS_RETCODE_UNSUPPORTED, "Suspend publication operation does not being supported yet");
err:
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}


_Pre_satisfies_((publisher & DDS_ENTITY_KIND_MASK) == DDS_KIND_PUBLISHER)
dds_return_t
dds_resume(
        _In_ dds_entity_t publisher)
{
    dds_return_t ret = DDS_RETCODE_OK;

    DDS_REPORT_STACK();

    if(dds_entity_kind(publisher) != DDS_KIND_PUBLISHER) {
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER,"Provided entity is not a publisher kind");
        goto err;
    }
    /* TODO: CHAM-123 Currently unsupported. */
    ret = DDS_ERRNO(DDS_RETCODE_UNSUPPORTED, "Suspend publication operation does not being supported yet");
err:
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}


_Pre_satisfies_(((publisher_or_writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER   ) ||\
                ((publisher_or_writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_PUBLISHER) )
dds_return_t
dds_wait_for_acks(
        _In_ dds_entity_t publisher_or_writer,
        _In_ dds_duration_t timeout)
{
    dds_return_t ret;
    DDS_REPORT_STACK();

    /* TODO: CHAM-125 Currently unsupported. */
    OS_UNUSED_ARG(timeout);

    switch(dds_entity_kind(publisher_or_writer)) {
        case DDS_KIND_WRITER:
            ret = DDS_ERRNO(DDS_RETCODE_UNSUPPORTED, "Wait for acknowledgments on a writer is not being supported yet");
            break;
        case DDS_KIND_PUBLISHER:
            ret = DDS_ERRNO(DDS_RETCODE_UNSUPPORTED, "Wait for acknowledgments on a publisher is not being supported yet");
            break;
        default:
            ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Provided entity is not a publisher nor a writer");
            break;
    }
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}

dds_return_t
dds_publisher_begin_coherent(
        _In_ dds_entity_t e)
{
    /* TODO: CHAM-124 Currently unsupported. */
    return DDS_ERRNO(DDS_RETCODE_UNSUPPORTED, "Using coherency to get a coherent data set is not being supported yet");
}

dds_return_t
dds_publisher_end_coherent(
        _In_ dds_entity_t e)
{
    /* TODO: CHAM-124 Currently unsupported. */
    return DDS_ERRNO(DDS_RETCODE_UNSUPPORTED, "Using coherency to get a coherent data set is not being supported yet");
}

