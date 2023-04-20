// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/ddsi/ddsi_shm_transport.h"
#include "iceoryx_binding_c/log.h"

void iox_sub_context_init(iox_sub_context_t *context)
{
  context->monitor = NULL;
  context->parent_reader = NULL;
  ddsrt_mutex_init(&context->mutex);
}

void iox_sub_context_fini(iox_sub_context_t* context)
{
  ddsrt_mutex_destroy(&context->mutex);
}

iox_sub_context_t **iox_sub_context_ptr(iox_sub_t sub) {
  // we know that 8 bytes in front of sub there is a pointer to the context
  char* p = (char*) sub - sizeof(void *);
  return (iox_sub_context_t **)p;
}

void shm_lock_iox_sub(iox_sub_t sub)
{
  iox_sub_context_t **context = iox_sub_context_ptr(sub);
  ddsrt_mutex_lock(&(*context)->mutex);
}

void shm_unlock_iox_sub(iox_sub_t sub)
{
  iox_sub_context_t **context = iox_sub_context_ptr(sub);
  ddsrt_mutex_unlock(&(*context)->mutex);
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

// TODO: further consolidation of shared memory allocation logic
void *shm_create_chunk(iox_pub_t iox_pub, size_t size) {
  iceoryx_header_t *ice_hdr;
  void *iox_chunk;

  // TODO: use a proper timeout to control the time it is allowed to take to
  // obtain a chunk more accurately but for now only try a limited number of
  // times (hence non-blocking). Otherwise we could block here forever and this
  // also leads to problems with thread progress monitoring.

  int32_t number_of_tries =
      10; // try 10 times over at least 10ms, considering the wait time below

  while (true) {
    enum iox_AllocationResult alloc_result =
        iox_pub_loan_aligned_chunk_with_user_header(
            iox_pub, &iox_chunk, (uint32_t)size,
            IOX_C_CHUNK_DEFAULT_USER_PAYLOAD_ALIGNMENT,
            sizeof(iceoryx_header_t), 8);

    if (AllocationResult_SUCCESS == alloc_result)
      break;

    if (--number_of_tries <= 0) {
      return NULL;
    }

    dds_sleepfor(DDS_MSECS(1));
  }

  iox_chunk_header_t *iox_chunk_header =
      iox_chunk_header_from_user_payload(iox_chunk);
  ice_hdr = iox_chunk_header_to_user_header(iox_chunk_header);
  ice_hdr->data_size = (uint32_t)size;
  ice_hdr->shm_data_state = IOX_CHUNK_UNINITIALIZED;
  return iox_chunk;
}

void shm_set_data_state(void *iox_chunk, iox_shm_data_state_t data_state) {
  iceoryx_header_t *iox_hdr = iceoryx_header_from_chunk(iox_chunk);
  iox_hdr->shm_data_state = data_state;
}

iox_shm_data_state_t shm_get_data_state(void *iox_chunk) {
  iceoryx_header_t *iox_hdr = iceoryx_header_from_chunk(iox_chunk);
  return iox_hdr->shm_data_state;
}
