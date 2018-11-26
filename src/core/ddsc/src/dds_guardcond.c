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
#include "dds__reader.h"
#include "dds__guardcond.h"
#include "dds__entity.h"
#include "dds__err.h"
#include "ddsi/q_ephash.h"
#include "ddsi/q_entity.h"
#include "ddsi/q_thread.h"

_Must_inspect_result_ dds_guardcond*
dds_create_guardcond(
        _In_ dds_participant *pp)
{
    dds_guardcond * gcond = dds_alloc(sizeof(*gcond));
    gcond->m_entity.m_hdl = dds_entity_init(&gcond->m_entity, (dds_entity*)pp, DDS_KIND_COND_GUARD, NULL, NULL, 0);
    return gcond;
}

_Pre_satisfies_((reader & DDS_ENTITY_KIND_MASK) == DDS_KIND_PARTICIPANT)
_Must_inspect_result_ dds_entity_t
dds_create_guardcondition(
        _In_ dds_entity_t participant)
{
    dds_entity_t hdl;
    dds_entity * pp;
    dds__retcode_t rc;

    rc = dds_entity_lock(participant, DDS_KIND_PARTICIPANT, &pp);
    if (rc == DDS_RETCODE_OK) {
        dds_guardcond *cond = dds_create_guardcond((dds_participant *)pp);
        assert(cond);
        hdl = cond->m_entity.m_hdl;
        dds_entity_unlock(pp);
    } else {
        DDS_ERROR("Error occurred on locking reader\n");
        hdl = DDS_ERRNO(rc);
    }

    return hdl;
}

_Pre_satisfies_(((condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_GUARD) )
dds_return_t
dds_set_guardcondition(
        _In_ dds_entity_t condition,
        _In_ bool triggered)
{
    dds_return_t ret;
    dds_guardcond *gcond;
    dds__retcode_t rc;

    rc = dds_entity_lock(condition, DDS_KIND_COND_GUARD, (dds_entity**)&gcond);
    if (rc == DDS_RETCODE_OK) {
        if (triggered) {
            dds_entity_status_set(gcond, DDS_WAITSET_TRIGGER_STATUS);
            dds_entity_status_signal(&gcond->m_entity);
        } else {
            dds_entity_status_reset(gcond, DDS_WAITSET_TRIGGER_STATUS);
        }
        dds_entity_unlock(&gcond->m_entity);
        ret = DDS_RETCODE_OK;
    } else {
        DDS_ERROR("Argument condition is not valid\n");
        ret = DDS_ERRNO(dds_valid_hdl(condition, DDS_KIND_COND_GUARD));
    }

    return ret;
}

_Pre_satisfies_(((condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_GUARD) )
dds_return_t
dds_read_guardcondition(
        _In_ dds_entity_t condition,
        _Out_ bool *triggered)
{
    dds_return_t ret;
    dds_guardcond *gcond;
    dds__retcode_t rc;

    if (triggered != NULL) {
        *triggered = false;
        rc = dds_entity_lock(condition, DDS_KIND_COND_GUARD, (dds_entity**)&gcond);
        if (rc == DDS_RETCODE_OK) {
            *triggered = dds_entity_status_match(gcond, DDS_WAITSET_TRIGGER_STATUS);
            dds_entity_unlock((dds_entity*)gcond);
            ret = DDS_RETCODE_OK;
        } else {
            DDS_ERROR("Argument condition is not valid\n");
            ret = DDS_ERRNO(dds_valid_hdl(condition, DDS_KIND_COND_GUARD));
        }
    } else {
        DDS_ERROR("Argument triggered is NULL\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
    }

    return ret;
}

_Pre_satisfies_(((condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_GUARD) )
dds_return_t
dds_take_guardcondition(
        _In_ dds_entity_t condition,
        _Out_ bool *triggered)
{
    dds_return_t ret;
    dds_guardcond *gcond;
    dds__retcode_t rc;

    if (triggered != NULL) {
        *triggered = false;
        rc = dds_entity_lock(condition, DDS_KIND_COND_GUARD, (dds_entity**)&gcond);
        if (rc == DDS_RETCODE_OK) {
            *triggered = dds_entity_status_match(gcond, DDS_WAITSET_TRIGGER_STATUS);
            dds_entity_status_reset(gcond, DDS_WAITSET_TRIGGER_STATUS);
            dds_entity_unlock((dds_entity*)gcond);
            ret = DDS_RETCODE_OK;
        } else {
            DDS_ERROR("Argument condition is not valid\n");
            ret = DDS_ERRNO(dds_valid_hdl(condition, DDS_KIND_COND_GUARD));
        }
    } else {
        DDS_ERROR("Argument triggered is NULL\n");
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
    }

    return ret;
}
