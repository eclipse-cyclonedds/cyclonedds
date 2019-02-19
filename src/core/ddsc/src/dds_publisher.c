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
#include "dds/ddsrt/misc.h"
#include "dds__listener.h"
#include "dds__publisher.h"
#include "dds__qos.h"
#include "dds__err.h"
#include "dds/ddsi/q_entity.h"
#include "dds/version.h"

DECL_ENTITY_LOCK_UNLOCK(extern inline, dds_publisher)

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
    const dds_qos_t *qos,
    bool enabled)
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
            DDS_ERROR(DDS_PROJECT_NAME" does not support changing QoS policies yet\n");
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

dds_entity_t
dds_create_publisher(
    dds_entity_t participant,
    const dds_qos_t *qos,
    const dds_listener_t *listener)
{
    dds_entity * par;
    dds_publisher * pub;
    dds_entity_t hdl;
    dds_qos_t * new_qos = NULL;
    dds_return_t ret;
    dds_retcode_t rc;

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

DDS_EXPORT dds_return_t
dds_suspend(
    dds_entity_t publisher)
{
  return dds_generic_unimplemented_operation (publisher, DDS_KIND_PUBLISHER);
}

dds_return_t
dds_resume(
    dds_entity_t publisher)
{
  return dds_generic_unimplemented_operation (publisher, DDS_KIND_PUBLISHER);
}

dds_return_t
dds_wait_for_acks(
    dds_entity_t publisher_or_writer,
    dds_duration_t timeout)
{
  if (timeout < 0)
    return DDS_ERRNO (DDS_RETCODE_BAD_PARAMETER);
  static const dds_entity_kind_t kinds[] = { DDS_KIND_WRITER, DDS_KIND_PUBLISHER };
  return dds_generic_unimplemented_operation_manykinds (publisher_or_writer, sizeof (kinds) / sizeof (kinds[0]), kinds);
}

dds_return_t
dds_publisher_begin_coherent(
    dds_entity_t e)
{
    /* TODO: CHAM-124 Currently unsupported. */
    (void)e;
    DDS_ERROR("Using coherency to get a coherent data set is not being supported yet\n");
    return DDS_ERRNO(DDS_RETCODE_UNSUPPORTED);
}

dds_return_t
dds_publisher_end_coherent(
    dds_entity_t e)
{
    /* TODO: CHAM-124 Currently unsupported. */
    (void)e;
    DDS_ERROR("Using coherency to get a coherent data set is not being supported yet\n");
    return DDS_ERRNO(DDS_RETCODE_UNSUPPORTED);
}

