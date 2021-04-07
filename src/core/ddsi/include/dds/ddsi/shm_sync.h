/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDS_SHM__SYNC_H
#define DDS_SHM__SYNC_H

#include "dds/export.h"
#include "iceoryx_binding_c/subscriber.h"
#include "dds/ddsrt/sync.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct dds_reader;
struct shm_monitor;

typedef struct {
    iox_sub_storage_t storage;
    //we use a mutex per subscriber to handle concurrent take and release of the data
    ddsrt_mutex_t mutex;
    struct shm_monitor* monitor;
    struct dds_reader* parent_reader;
} iox_sub_storage_extension_t;

//lock and unlock for individual subscribers
DDS_EXPORT void iox_sub_storage_extension_init(iox_sub_storage_extension_t* storage);
DDS_EXPORT void iox_sub_storage_extension_fini(iox_sub_storage_extension_t* storage);
DDS_EXPORT void shm_lock_iox_sub(iox_sub_t sub);
DDS_EXPORT void shm_unlock_iox_sub(iox_sub_t sub);

// TODO: lock and unlock for individual publishers

#if defined (__cplusplus)
}
#endif

#endif //DDS_SHM__SYNC_H
