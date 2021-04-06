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
#include "dds/dds.h"
#include "dds/ddsrt/heap.h"
#include "dds__data_allocator.h"
#include "dds__entity.h"

dds_return_t dds_data_allocator_init (dds_entity_t entity, dds_data_allocator_t *data_allocator)
{
  dds_entity *e;
  dds_return_t ret;

  if (data_allocator == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_entity_pin (entity, &e)) != DDS_RETCODE_OK)
    return ret;
  switch (dds_entity_kind (e))
  {
    case DDS_KIND_READER:
      ret = dds__reader_data_allocator_init ((struct dds_reader *) e, data_allocator);
      break;
    case DDS_KIND_WRITER:
      ret = dds__writer_data_allocator_init ((struct dds_writer *) e, data_allocator);
      break;
    default:
      ret = DDS_RETCODE_ILLEGAL_OPERATION;
      break;
  }
  if (ret == DDS_RETCODE_OK)
    data_allocator->entity = entity;
  dds_entity_unpin (e);
  return ret;
}

dds_return_t dds_data_allocator_fini (dds_data_allocator_t *data_allocator)
{
  dds_entity *e;
  dds_return_t ret;

  if (data_allocator == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_entity_pin (data_allocator->entity, &e)) != DDS_RETCODE_OK)
    return ret;
  switch (dds_entity_kind (e))
  {
    case DDS_KIND_READER:
      ret = dds__reader_data_allocator_fini ((struct dds_reader *) e, data_allocator);
      break;
    case DDS_KIND_WRITER:
      ret = dds__writer_data_allocator_fini ((struct dds_writer *) e, data_allocator);
      break;
    default:
      ret = DDS_RETCODE_ILLEGAL_OPERATION;
      break;
  }
  if (ret == DDS_RETCODE_OK)
    data_allocator->entity = 0;
  dds_entity_unpin(e);
  return ret;
}

void *dds_data_allocator_alloc (dds_data_allocator_t *data_allocator, size_t size)
{
#if DDS_HAS_SHM
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
      else
      {
        enum iox_AllocationResult alloc_result;
        void *ptr;
        alloc_result = iox_pub_loan_chunk (d->ref.pub, &ptr, (uint32_t) size);
        return (alloc_result == AllocationResult_SUCCESS) ? ptr : NULL;
      }
    default:
      return NULL;
  }
#else
  (void) data_allocator;
  return ddsrt_malloc (size);
#endif
}

void dds_data_allocator_free (dds_data_allocator_t *data_allocator, void *ptr)
{
#if DDS_HAS_SHM
  dds_iox_allocator_t *d = (dds_iox_allocator_t *) data_allocator->opaque.bytes;
  switch (d->kind)
  {
    case DDS_IOX_ALLOCATOR_KIND_FINI:
      break;
    case DDS_IOX_ALLOCATOR_KIND_NONE:
      ddsrt_free (ptr);
      break;
    case DDS_IOX_ALLOCATOR_KIND_SUBSCRIBER:
      if (ptr != NULL)
        iox_sub_release_chunk (d->ref.sub, ptr);
      break;
    case DDS_IOX_ALLOCATOR_KIND_PUBLISHER:
      if (ptr != NULL)
        iox_pub_release_chunk (d->ref.pub, ptr);
      break;
  }
#else
  (void) data_allocator;
  ddsrt_free (ptr);
#endif
}
