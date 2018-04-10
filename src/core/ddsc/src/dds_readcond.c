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
#include "dds__reader.h"
#include "dds__readcond.h"
#include "dds__rhc.h"
#include "dds__entity.h"
#include "dds__err.h"
#include "ddsi/q_ephash.h"
#include "ddsi/q_entity.h"
#include "ddsi/q_thread.h"
#include "dds__report.h"

static dds_return_t
dds_readcond_delete(
        dds_entity *e)
{
    dds_rhc_remove_readcondition((dds_readcond*)e);
    return DDS_RETCODE_OK;
}

_Must_inspect_result_ dds_readcond*
dds_create_readcond(
        _In_ dds_reader *rd,
        _In_ dds_entity_kind_t kind,
        _In_ uint32_t mask)
{
    dds_readcond * cond = dds_alloc(sizeof(*cond));
    assert(kind == DDS_KIND_COND_READ || kind == DDS_KIND_COND_QUERY);
    cond->m_entity.m_hdl = dds_entity_init(&cond->m_entity, (dds_entity*)rd, kind, NULL, NULL, 0);
    cond->m_entity.m_deriver.delete = dds_readcond_delete;
    cond->m_rhc = rd->m_rd->rhc;
    cond->m_sample_states = mask & DDS_ANY_SAMPLE_STATE;
    cond->m_view_states = mask & DDS_ANY_VIEW_STATE;
    cond->m_instance_states = mask & DDS_ANY_INSTANCE_STATE;
    cond->m_rd_guid = ((dds_entity*)rd)->m_guid;
    dds_rhc_add_readcondition (cond);
    return cond;
}

_Pre_satisfies_((reader & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER)
_Must_inspect_result_ dds_entity_t
dds_create_readcondition(
        _In_ dds_entity_t reader,
        _In_ uint32_t mask)
{
    dds_entity_t hdl;
    dds_reader * rd;
    dds__retcode_t rc;

    DDS_REPORT_STACK();

    rc = dds_reader_lock(reader, &rd);
    if (rc == DDS_RETCODE_OK) {
        dds_readcond *cond = dds_create_readcond(rd, DDS_KIND_COND_READ, mask);
        assert(cond);
        hdl = cond->m_entity.m_hdl;
        dds_reader_unlock(rd);
    } else {
        hdl = DDS_ERRNO(rc, "Error occurred on locking reader");
    }
    DDS_REPORT_FLUSH(hdl <= 0);
    return hdl;
}

_Pre_satisfies_(((condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY) )
dds_entity_t
dds_get_datareader(
        _In_ dds_entity_t condition)
{
    dds_entity_t hdl;

    DDS_REPORT_STACK();
    if (dds_entity_kind(condition) == DDS_KIND_COND_READ) {
        hdl = dds_get_parent(condition);
    } else if (dds_entity_kind(condition) == DDS_KIND_COND_QUERY) {
        hdl = dds_get_parent(condition);
    } else {
        hdl = DDS_ERRNO(dds_valid_hdl(condition, DDS_KIND_COND_READ), "Argument condition is not valid");
    }
    DDS_REPORT_FLUSH(hdl <= 0);
    return hdl;
}


_Pre_satisfies_(((condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY) )
_Check_return_ dds_return_t
dds_get_mask(
        _In_ dds_entity_t condition,
        _Out_ uint32_t   *mask)
{
    dds_return_t ret;
    dds_readcond *cond;
    dds__retcode_t rc;

    DDS_REPORT_STACK();

    if (mask != NULL) {
        *mask = 0;
        if ((dds_entity_kind(condition) == DDS_KIND_COND_READ ) ||
            (dds_entity_kind(condition) == DDS_KIND_COND_QUERY) ){
            rc = dds_entity_lock(condition, DDS_KIND_DONTCARE, (dds_entity**)&cond);
            if (rc == DDS_RETCODE_OK) {
                *mask = (cond->m_sample_states | cond->m_view_states | cond->m_instance_states);
                dds_entity_unlock((dds_entity*)cond);
                ret = DDS_RETCODE_OK;
            } else{
                ret = DDS_ERRNO(rc, "Error occurred on locking condition");
            }
        } else {
            ret = DDS_ERRNO(dds_valid_hdl(condition, DDS_KIND_COND_READ), "Argument condition is not valid");
        }
    } else {
        ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER, "Argument mask is NULL");
    }
    DDS_REPORT_FLUSH(ret != DDS_RETCODE_OK);
    return ret;
}
