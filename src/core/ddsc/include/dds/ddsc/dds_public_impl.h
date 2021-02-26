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
/* TODO: do we really need to expose all of this as an API? maybe some, but all? */

/** @file
 *
 * @brief DDS C Implementation API
 *
 * This header file defines the public API for all kinds of things in the
 * Eclipse Cyclone DDS C language binding.
 */
#ifndef DDS_IMPL_H
#define DDS_IMPL_H

#include <stdint.h>
#include <stdbool.h>
#include "dds/export.h"
#include "dds/ddsc/dds_public_alloc.h"
#include "dds/ddsc/dds_opcodes.h"

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct dds_sequence
{
  uint32_t _maximum;
  uint32_t _length;
  uint8_t * _buffer;
  bool _release;
}
dds_sequence_t;

typedef struct dds_key_descriptor
{
  const char * m_name;
  uint32_t m_index;
}
dds_key_descriptor_t;

/*
  Topic definitions are output by a preprocessor and have an
  implementation-private definition. The only thing exposed on the
  API is a pointer to the "topic_descriptor_t" struct type.
*/

typedef struct dds_topic_descriptor
{
  const uint32_t m_size;               /* Size of topic type */
  const uint32_t m_align;              /* Alignment of topic type */
  const uint32_t m_flagset;            /* Flags */
  const uint32_t m_nkeys;              /* Number of keys (can be 0) */
  const char * m_typename;             /* Type name */
  const dds_key_descriptor_t * m_keys; /* Key descriptors (NULL iff m_nkeys 0) */
  const uint32_t m_nops;               /* Number of ops in m_ops */
  const uint32_t * m_ops;              /* Marshalling meta data */
  const char * m_meta;                 /* XML topic description meta data */
}
dds_topic_descriptor_t;

/*
  Masks for read condition, read, take: there is only one mask here,
  which combines the sample, view and instance states.
*/

#define DDS_READ_SAMPLE_STATE 1u
#define DDS_NOT_READ_SAMPLE_STATE 2u
#define DDS_ANY_SAMPLE_STATE (1u | 2u)

#define DDS_NEW_VIEW_STATE 4u
#define DDS_NOT_NEW_VIEW_STATE 8u
#define DDS_ANY_VIEW_STATE (4u | 8u)

#define DDS_ALIVE_INSTANCE_STATE 16u
#define DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE 32u
#define DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE 64u
#define DDS_ANY_INSTANCE_STATE (16u | 32u | 64u)

#define DDS_ANY_STATE (DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE)

#define DDS_DOMAIN_DEFAULT ((uint32_t) 0xffffffffu)
#define DDS_HANDLE_NIL 0
#define DDS_ENTITY_NIL 0

typedef enum dds_entity_kind
{
  DDS_KIND_DONTCARE,
  DDS_KIND_TOPIC,
  DDS_KIND_PARTICIPANT,
  DDS_KIND_READER,
  DDS_KIND_WRITER,
  DDS_KIND_SUBSCRIBER,
  DDS_KIND_PUBLISHER,
  DDS_KIND_COND_READ,
  DDS_KIND_COND_QUERY,
  DDS_KIND_COND_GUARD,
  DDS_KIND_WAITSET,
  DDS_KIND_DOMAIN,
  DDS_KIND_CYCLONEDDS
} dds_entity_kind_t;
#define DDS_KIND_MAX DDS_KIND_CYCLONEDDS

/* Handles are opaque pointers to implementation types */
typedef uint64_t dds_instance_handle_t;
typedef uint32_t dds_domainid_t;

/**
 * Description : Enable or disable write batching. Overrides default configuration
 * setting for write batching (Internal/WriteBatch).
 *
 * Arguments :
 *   -# enable Enables or disables write batching for all writers.
 */
DDS_EXPORT void dds_write_set_batch (bool enable);

#if defined (__cplusplus)
}
#endif
#endif
