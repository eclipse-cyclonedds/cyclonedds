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
/* TODO: do we really need to expose this as an API? */

/** @file
 *
 * @brief DDS C Allocation API
 *
 * This header file defines the public API of allocation convenience functions
 * in the Eclipse Cyclone DDS C language binding.
 */
#ifndef DDS_ALLOC_H
#define DDS_ALLOC_H

#include <stddef.h>

#include "dds/export.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct dds_topic_descriptor;
struct dds_sequence;

#define DDS_FREE_KEY_BIT 0x01
#define DDS_FREE_CONTENTS_BIT 0x02
#define DDS_FREE_ALL_BIT 0x04

typedef enum
{
  DDS_FREE_ALL = DDS_FREE_KEY_BIT | DDS_FREE_CONTENTS_BIT | DDS_FREE_ALL_BIT,
  DDS_FREE_CONTENTS = DDS_FREE_KEY_BIT | DDS_FREE_CONTENTS_BIT,
  DDS_FREE_KEY = DDS_FREE_KEY_BIT
}
dds_free_op_t;

typedef struct dds_allocator
{
  /* Behaviour as C library malloc, realloc and free */

  void * (*malloc) (size_t size);
  void * (*realloc) (void *ptr, size_t size); /* if needed */
  void (*free) (void *ptr);
}
dds_allocator_t;

DDS_EXPORT void * dds_alloc (size_t size);
DDS_EXPORT void * dds_realloc (void * ptr, size_t size);
DDS_EXPORT void * dds_realloc_zero (void * ptr, size_t size);
DDS_EXPORT void dds_free (void * ptr);

typedef void * (*dds_alloc_fn_t) (size_t);
typedef void * (*dds_realloc_fn_t) (void *, size_t);
typedef void (*dds_free_fn_t) (void *);

DDS_EXPORT char * dds_string_alloc (size_t size);
DDS_EXPORT char * dds_string_dup (const char * str);
DDS_EXPORT void dds_string_free (char * str);
DDS_EXPORT void dds_sample_free (void * sample, const struct dds_topic_descriptor * desc, dds_free_op_t op);

#if defined (__cplusplus)
}
#endif
#endif
