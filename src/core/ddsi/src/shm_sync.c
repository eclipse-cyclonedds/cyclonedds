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
#include "dds/ddsi/shm_sync.h"

static ddsrt_mutex_t shm_mutex;
static int shm_mutex_initialized = 0;

int shm_mutex_init()
{
  if (shm_mutex_initialized)
    return 1;

  ddsrt_mutex_init(&shm_mutex);
  shm_mutex_initialized = 1;

  return 0;
}

void shm_mutex_lock(void)
{
  ddsrt_mutex_lock(&shm_mutex);
}

void shm_mutex_unlock(void)
{
  ddsrt_mutex_unlock(&shm_mutex);
}

void iox_sub_storage_extension_init(iox_sub_storage_extension_t *storage)
{
  storage->monitor = NULL;
  storage->parent_reader = NULL;
  ddsrt_mutex_init(&storage->mutex);
}

void iox_sub_storage_extension_fini(iox_sub_storage_extension_t* storage)
{
  ddsrt_mutex_destroy(&storage->mutex);
}

void shm_lock_iox_sub(iox_sub_t sub)
{
    iox_sub_storage_extension_t* storage = (iox_sub_storage_extension_t*) sub;
    ddsrt_mutex_lock(&storage->mutex);
}

void shm_unlock_iox_sub(iox_sub_t sub)
{
    iox_sub_storage_extension_t* storage = (iox_sub_storage_extension_t*) sub;
    ddsrt_mutex_unlock(&storage->mutex);
}
