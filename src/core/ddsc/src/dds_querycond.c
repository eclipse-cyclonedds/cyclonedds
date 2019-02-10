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
#include "dds__entity.h"
#include "dds__reader.h"
#include "dds__topic.h"
#include "dds__querycond.h"
#include "dds__readcond.h"
#include "dds__err.h"
#include "ddsi/ddsi_serdata.h"
#include "ddsi/ddsi_sertopic.h"

_Pre_satisfies_((reader & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER)
DDS_EXPORT dds_entity_t
dds_create_querycondition(
        _In_ dds_entity_t reader,
        _In_ uint32_t mask,
        _In_ dds_querycondition_filter_fn filter)
{
    dds_entity_t hdl;
    dds__retcode_t rc;
    dds_reader *r;

    rc = dds_reader_lock(reader, &r);
    if (rc == DDS_RETCODE_OK) {
        dds_readcond *cond = dds_create_readcond(r, DDS_KIND_COND_QUERY, mask, filter);
        assert(cond);
        const bool success = (cond->m_entity.m_deriver.delete != 0);
        dds_reader_unlock(r);
        if (success) {
            hdl = cond->m_entity.m_hdl;
        } else {
            dds_delete (cond->m_entity.m_hdl);
            hdl = DDS_ERRNO(DDS_RETCODE_OUT_OF_RESOURCES);
        }
    } else {
        DDS_ERROR("Error occurred on locking reader\n");
        hdl = DDS_ERRNO(rc);
    }

    return hdl;
}
