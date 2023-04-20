// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/dds.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsrt/heap.h"
#include "dds__data_allocator.h"
#include "dds__entity.h"

#ifdef DDS_HAS_SHM
#include "dds/ddsi/ddsi_shm_transport.h"
#endif

#include "dds/ddsc/dds_loan_api.h"

dds_return_t dds_data_allocator_init_heap (dds_data_allocator_t *data_allocator)
{
  // Use special entity handle to allocate on heap
  return dds_data_allocator_init(DDS_DATA_ALLOCATOR_ALLOC_ON_HEAP, data_allocator);
}

dds_return_t dds_data_allocator_init (dds_entity_t entity, dds_data_allocator_t *data_allocator)
{
  dds_entity *e;
  dds_return_t ret;

  if (data_allocator == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  // special case, the allocator treats this entity as an allocation on the heap
  if (entity == DDS_DATA_ALLOCATOR_ALLOC_ON_HEAP) {
    ret = DDS_RETCODE_OK;
  } else {
    if ((ret = dds_entity_pin(entity, &e)) != DDS_RETCODE_OK)
      return ret;
    switch (dds_entity_kind(e)) {
      case DDS_KIND_READER:
        ret = dds__reader_data_allocator_init((struct dds_reader *)e, data_allocator);
        break;
      case DDS_KIND_WRITER:
        ret = dds__writer_data_allocator_init((struct dds_writer *)e, data_allocator);
        break;
      default:
        ret = DDS_RETCODE_ILLEGAL_OPERATION;
        break;
    }
    dds_entity_unpin (e);
  }
  if (ret == DDS_RETCODE_OK)
    data_allocator->entity = entity;
  return ret;
}

dds_return_t dds_data_allocator_fini (dds_data_allocator_t *data_allocator)
{
  dds_entity *e;
  dds_return_t ret;

  if (data_allocator == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  // special case, the allocator treats this entity as an allocation on the heap
  if (data_allocator->entity == DDS_DATA_ALLOCATOR_ALLOC_ON_HEAP) {
    ret = DDS_RETCODE_OK;
  } else {
    if ((ret = dds_entity_pin(data_allocator->entity, &e)) != DDS_RETCODE_OK)
      return ret;
    switch (dds_entity_kind(e)) {
      case DDS_KIND_READER:
        ret = dds__reader_data_allocator_fini((struct dds_reader *)e, data_allocator);
        break;
      case DDS_KIND_WRITER:
        ret = dds__writer_data_allocator_fini((struct dds_writer *)e, data_allocator);
        break;
      default:
        ret = DDS_RETCODE_ILLEGAL_OPERATION;
        break;
    }
    dds_entity_unpin(e);
  }
  if (ret == DDS_RETCODE_OK)
    data_allocator->entity = 0;
  return ret;
}

void *dds_data_allocator_alloc (dds_data_allocator_t *data_allocator, size_t size)
{
#if DDS_HAS_SHM
  if (data_allocator == NULL)
    return NULL;

  if(data_allocator->entity == DDS_DATA_ALLOCATOR_ALLOC_ON_HEAP)
    return ddsrt_malloc (size);

  dds_iox_allocator_t *d = (dds_iox_allocator_t *) data_allocator->opaque.bytes;
  switch (d->kind)
  {
    case DDS_IOX_ALLOCATOR_KIND_FINI:
      return NULL;
    case DDS_IOX_ALLOCATOR_KIND_NONE:
      return ddsrt_malloc (size);
    case DDS_IOX_ALLOCATOR_KIND_SUBSCRIBER:
      return NULL;
    case DDS_IOX_ALLOCATOR_KIND_PUBLISHER:
      if (size > UINT32_MAX)
        return NULL;
      else {
        ddsrt_mutex_lock(&d->mutex);
        // NB: This creates an iceoryx header in addition to the allocation.
        //     This may be undesirable, especially for small allocations...
        // The header contains the size of the allocation and other information,
        // e.g. whether the memory is uninitialized or contains data.
        void *chunk = shm_create_chunk(d->ref.pub, (uint32_t)size);
        ddsrt_mutex_unlock(&d->mutex);
        return chunk;
      }
    default:
      return NULL;
  }
#else
  (void) data_allocator;
  return ddsrt_malloc (size);
#endif
}

dds_return_t dds_data_allocator_free (dds_data_allocator_t *data_allocator, void *ptr)
{
  dds_return_t ret = DDS_RETCODE_OK;
#if DDS_HAS_SHM
  if (data_allocator == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  if(data_allocator->entity == DDS_DATA_ALLOCATOR_ALLOC_ON_HEAP) {
    ddsrt_free(ptr);
  } else {
    dds_iox_allocator_t *d = (dds_iox_allocator_t *)data_allocator->opaque.bytes;
    switch (d->kind) {
      case DDS_IOX_ALLOCATOR_KIND_FINI:
        ret = DDS_RETCODE_PRECONDITION_NOT_MET;
        break;
      case DDS_IOX_ALLOCATOR_KIND_NONE:
        ddsrt_free(ptr);
        break;
      case DDS_IOX_ALLOCATOR_KIND_SUBSCRIBER:
        if (ptr != NULL) {
          ddsrt_mutex_lock(&d->mutex);
          shm_lock_iox_sub(d->ref.sub);
          iox_sub_release_chunk(d->ref.sub, ptr);
          shm_unlock_iox_sub(d->ref.sub);
          ddsrt_mutex_unlock(&d->mutex);
        }
        break;
      case DDS_IOX_ALLOCATOR_KIND_PUBLISHER:
        if (ptr != NULL) {
          ddsrt_mutex_lock(&d->mutex);
          iox_pub_release_chunk(d->ref.pub, ptr);
          ddsrt_mutex_unlock(&d->mutex);
        }
        break;
      default:
        ret = DDS_RETCODE_BAD_PARAMETER;
    }
  }
#else
  (void) data_allocator;
  ddsrt_free (ptr);
#endif
  return ret;
}
