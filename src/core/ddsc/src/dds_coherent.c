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

#include "ddsc/dds.h"
#include "dds__entity.h"
#include "dds__subscriber.h"
#include "dds__publisher.h"
#include "dds__err.h"

_Pre_satisfies_(((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER    ) || \
                ((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_SUBSCRIBER) || \
                ((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER    ) || \
                ((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_SUBSCRIBER) )
dds_return_t
dds_begin_coherent(
        _In_ dds_entity_t entity)
{
    dds_return_t ret;

    switch(dds_entity_kind_from_handle(entity)) {
        case DDS_KIND_READER:
        case DDS_KIND_WRITER:
            /* Invoking on a writer/reader behaves as if invoked on
             * its parent publisher/subscriber. */
            ret = dds_begin_coherent(dds_get_parent(entity));
            break;
        case DDS_KIND_PUBLISHER:
            ret = dds_publisher_begin_coherent(entity);
            break;
        case DDS_KIND_SUBSCRIBER:
            ret = dds_subscriber_begin_coherent(entity);
            break;
        default:
            DDS_ERROR("Given entity can not control coherency\n");
            ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
            break;
    }
    return ret;
}

_Pre_satisfies_(((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER    ) || \
                ((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_SUBSCRIBER) || \
                ((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER    ) || \
                ((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_SUBSCRIBER) )
dds_return_t
dds_end_coherent(
        _In_ dds_entity_t entity)
{
    dds_return_t ret;

    switch(dds_entity_kind_from_handle(entity)) {
        case DDS_KIND_READER:
        case DDS_KIND_WRITER:
            /* Invoking on a writer/reader behaves as if invoked on
             * its parent publisher/subscriber. */
            ret = dds_end_coherent(dds_get_parent(entity));
            break;
        case DDS_KIND_PUBLISHER:
            ret = dds_publisher_end_coherent(entity);
            break;
        case DDS_KIND_SUBSCRIBER:
            ret = dds_subscriber_end_coherent(entity);
            break;
        default:
            DDS_ERROR("Given entity can not control coherency\n");
            ret = DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
            break;
    }
    return ret;
}
