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
#ifndef _DDS_WRITER_H_
#define _DDS_WRITER_H_

#include "dds__entity.h"

#if defined (__cplusplus)
extern "C" {
#endif

DEFINE_ENTITY_LOCK_UNLOCK(dds_writer, DDS_KIND_WRITER)

struct status_cb_data;

void dds_writer_status_cb (void *entity, const struct status_cb_data * data);

DDS_EXPORT dds_return_t dds_return_writer_loan(dds_writer *writer, void **buf,
                                               int32_t bufsz) ddsrt_nonnull_all;

DDS_EXPORT dds_return_t dds__writer_wait_for_acks (struct dds_writer *wr, ddsi_guid_t *rdguid, dds_time_t abstimeout);

#if defined (__cplusplus)
}
#endif
#endif
