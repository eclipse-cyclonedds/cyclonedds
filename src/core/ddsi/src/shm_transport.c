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
#include "dds/ddsi/shm_transport.h"
#include "iceoryx_binding_c/log.h"

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

iceoryx_header_t *iceoryx_header_from_chunk(const void *iox_chunk) {
  iox_chunk_header_t *chunk_header =
      iox_chunk_header_from_user_payload((void*) iox_chunk);
  return iox_chunk_header_to_user_header(chunk_header);
}

static enum iox_LogLevel to_iox_loglevel(enum ddsi_shm_loglevel level) {
    switch(level) {
        case DDSI_SHM_OFF : return Iceoryx_LogLevel_Off;
        case DDSI_SHM_FATAL : return Iceoryx_LogLevel_Fatal;
        case DDSI_SHM_ERROR : return Iceoryx_LogLevel_Error;
        case DDSI_SHM_WARN : return Iceoryx_LogLevel_Warn;
        case DDSI_SHM_INFO : return Iceoryx_LogLevel_Info;
        case DDSI_SHM_DEBUG : return Iceoryx_LogLevel_Debug;
        case DDSI_SHM_VERBOSE : return Iceoryx_LogLevel_Verbose;
    }
    return Iceoryx_LogLevel_Off;
}

void shm_set_loglevel(enum ddsi_shm_loglevel level) {
    iox_set_loglevel(to_iox_loglevel(level));
}

void free_iox_chunk(iox_sub_t *iox_sub, void **iox_chunk) {
  if (*iox_chunk)
  {   
    // assume *iox_chunk is only set to NULL while holding this lock
    // (we could also use an atomic exchange)
    // actually all reads on *iox_chunk must be atomic ...
    shm_lock_iox_sub(*iox_sub);
    void* chunk = *iox_chunk;
    if (chunk)
    {
      iox_sub_release_chunk(*iox_sub, chunk);
      *iox_chunk = NULL;
    }
    shm_unlock_iox_sub(*iox_sub);
  }
}


