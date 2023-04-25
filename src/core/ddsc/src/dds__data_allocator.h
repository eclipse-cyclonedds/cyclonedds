// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__DATA_ALLOCATOR_H
#define DDS__DATA_ALLOCATOR_H

#include "dds/ddsc/dds_data_allocator.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsrt/sync.h"

#if defined (__cplusplus)
extern "C" {
#endif

#ifdef DDS_HAS_SHM

#include "iceoryx_binding_c/publisher.h"
#include "iceoryx_binding_c/subscriber.h"

typedef enum dds_iox_allocator_kind {
  DDS_IOX_ALLOCATOR_KIND_FINI,
  DDS_IOX_ALLOCATOR_KIND_NONE, /* use heap */
  DDS_IOX_ALLOCATOR_KIND_PUBLISHER,
  DDS_IOX_ALLOCATOR_KIND_SUBSCRIBER
} dds_iox_allocator_kind_t;

typedef struct dds_iox_allocator {
  enum dds_iox_allocator_kind kind;
  union {
    iox_pub_t pub;
    iox_sub_t sub;
  } ref;
  ddsrt_mutex_t mutex;
} dds_iox_allocator_t;

DDSRT_STATIC_ASSERT(sizeof (dds_iox_allocator_t) <= sizeof (dds_data_allocator_t));

#endif // DDS_HAS_SHM

struct dds_writer;
struct dds_reader;

/** @component data_alloc */
dds_return_t dds__writer_data_allocator_init (const struct dds_writer *wr, dds_data_allocator_t *data_allocator)
  ddsrt_nonnull_all;

/** @component data_alloc */
dds_return_t dds__writer_data_allocator_fini (const struct dds_writer *wr, dds_data_allocator_t *data_allocator)
  ddsrt_nonnull_all;

/** @component data_alloc */
dds_return_t dds__reader_data_allocator_init (const struct dds_reader *wr, dds_data_allocator_t *data_allocator)
  ddsrt_nonnull_all;

/** @component data_alloc */
dds_return_t dds__reader_data_allocator_fini (const struct dds_reader *wr, dds_data_allocator_t *data_allocator)
  ddsrt_nonnull_all;

#if defined (__cplusplus)
}
#endif

#endif /* DDS__DATA_ALLOCATOR_H */
