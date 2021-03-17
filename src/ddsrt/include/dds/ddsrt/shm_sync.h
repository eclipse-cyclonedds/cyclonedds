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
#ifndef DDSRT_SHM_SYNC_H
#define DDSRT_SHM_SYNC_H

#include "dds/export.h"

#if defined (__cplusplus)
extern "C" {
#endif

DDS_EXPORT int shm_mutex_init(void);
DDS_EXPORT void shm_mutex_lock(void);
DDS_EXPORT void shm_mutex_unlock(void);

#if defined (__cplusplus)
}
#endif

#endif //DDSRT_SHM_SYNC_H
