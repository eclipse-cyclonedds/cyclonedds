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
#include "ddsc/ddsc_project.h"

#define DDS_PUBLISHER_STATUS_MASK   0u

static dds_return_t
dds_publisher_instance_hdl(
        dds_entity *e,
        dds_instance_handle_t *i)
{
    (void)e;
    (void)i;
    /* TODO: Get/generate proper handle. */
    DDS_ERROR("Getting publisher instance handle is not supported\n");
    return DDS_ERRNO(DDS_RETCODE_UNSUPPORTED);
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
        DDS_ERROR("Group data policy is inconsistent and caused an error\n");
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY);
    }
    if((qos->present & QP_PRESENTATION) && (validate_presentation_qospolicy(&qos->presentation) != 0)){
        DDS_ERROR("Presentation policy is inconsistent and caused an error\n");
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY);
    }
    if((qos->present & QP_PARTITION) && !validate_stringseq(&qos->partition)){
        DDS_ERROR("Partition policy is inconsistent and caused an error\n");
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY);
    }
    if((qos->present & QP_PRISMTECH_ENTITY_FACTORY) && !validate_entityfactory_qospolicy(&qos->entity_factory)){
        DDS_ERROR("Prismtech entity factory policy is inconsistent and caused an error\n");
        ret = DDS_ERRNO(DDS_RETCODE_INCONSISTENT_POLICY);
    }
    if(ret == DDS_RETCODE_OK && enabled && (qos->present & QP_PRESENTATION)){
        /* TODO: Improve/check immutable check. */
        DDS_ERROR("Presentation policy is immutable\n");
        ret = DDS_ERRNO(DDS_RETCODE_IMMUTABLE_POLICY);
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

static dds_return_t dds_publisher_status_validate (uint32_t mask)
{
    dds_return_t ret = DDS_RETCODE_OK;

    if (mask & ~(DDS_PUBLISHER_STATUS_MASK)) {
        DDS_ERROR("Invalid status mask\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
    }

    return ret;
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

    rc = dds_entity_lock(participant, DDS_KIND_PARTICIPANT, &par);
    if (rc != DDS_RETCODE_OK) {
        DDS_ERROR("Error occurred on locking participant\n");
        hdl = DDS_ERRNO(rc);
        goto lock_err;
    }

    /* Validate qos */
    if (qos) {
        ret = dds_publisher_qos_validate(qos, false);
        if (ret != DDS_RETCODE_OK) {
            hdl = ret;
            goto qos_err;
        }
        new_qos = dds_create_qos ();
        /* Only returns failure when one of the qos args is NULL, which
         * is not the case here. */
        (void)dds_copy_qos(new_qos, qos);
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
    return hdl;
}


_Pre_satisfies_((publisher & DDS_ENTITY_KIND_MASK) == DDS_KIND_PUBLISHER)
DDS_EXPORT dds_return_t
dds_suspend(
        _In_ dds_entity_t publisher)
{
    dds_return_t ret;

    if(dds_entity_kind_from_handle(publisher) != DDS_KIND_PUBLISHER) {
        DDS_ERROR("Provided entity is not a publisher kind\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        goto err;
    }
    /* TODO: CHAM-123 Currently unsupported. */
    DDS_ERROR("Suspend publication operation does not being supported yet\n");
    ret = DDS_ERRNO(DDS_RETCODE_UNSUPPORTED);
err:
    return ret;
}


_Pre_satisfies_((publisher & DDS_ENTITY_KIND_MASK) == DDS_KIND_PUBLISHER)
dds_return_t
dds_resume(
        _In_ dds_entity_t publisher)
{
    dds_return_t ret = DDS_RETCODE_OK;

    if(dds_entity_kind_from_handle(publisher) != DDS_KIND_PUBLISHER) {
        DDS_ERROR("Provided entity is not a publisher kind\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
        goto err;
    }
    /* TODO: CHAM-123 Currently unsupported. */
    DDS_ERROR("Suspend publication operation does not being supported yet\n");
    ret = DDS_ERRNO(DDS_RETCODE_UNSUPPORTED);
err:
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

    /* TODO: CHAM-125 Currently unsupported. */
    OS_UNUSED_ARG(timeout);

    switch(dds_entity_kind_from_handle(publisher_or_writer)) {
        case DDS_KIND_WRITER:
            DDS_ERROR("Wait for acknowledgments on a writer is not being supported yet\n");
            ret = DDS_ERRNO(DDS_RETCODE_UNSUPPORTED);
            break;
        case DDS_KIND_PUBLISHER:
            DDS_ERROR("Wait for acknowledgments on a publisher is not being supported yet\n");
            ret = DDS_ERRNO(DDS_RETCODE_UNSUPPORTED);
            break;
        default:
            DDS_ERROR("Provided entity is not a publisher nor a writer\n");
            ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
            break;
    }

    return ret;
}

dds_return_t
dds_publisher_begin_coherent(
        _In_ dds_entity_t e)
{
    /* TODO: CHAM-124 Currently unsupported. */
    (void)e;
    DDS_ERROR("Using coherency to get a coherent data set is not being supported yet\n");
    return DDS_ERRNO(DDS_RETCODE_UNSUPPORTED);
}

dds_return_t
dds_publisher_end_coherent(
        _In_ dds_entity_t e)
{
    /* TODO: CHAM-124 Currently unsupported. */
    (void)e;
    DDS_ERROR("Using coherency to get a coherent data set is not being supported yet\n");
    return DDS_ERRNO(DDS_RETCODE_UNSUPPORTED);
}

