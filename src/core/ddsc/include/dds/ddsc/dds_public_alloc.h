// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

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

DDS_EXPORT extern const struct dds_cdrstream_allocator dds_cdrstream_default_allocator;

/**
 * @anchor DDS_FREE_KEY_BIT
 * @ingroup alloc
 * @brief Instruction to free all keyfields in sample
 */
#define DDS_FREE_KEY_BIT 0x01

/**
 * @anchor DDS_FREE_CONTENTS_BIT
 * @ingroup alloc
 * @brief Instruction to free all non-keyfields in sample
 */
#define DDS_FREE_CONTENTS_BIT 0x02

/**
 * @anchor DDS_FREE_ALL_BIT
 * @ingroup alloc
 * @brief Instruction to free outer sample
 */
#define DDS_FREE_ALL_BIT 0x04

/**
 * @brief Freeing operation type
 * @ingroup alloc
 *
 * Selects which parts of a sample are freed by @ref dds_sample_free and by the
 * generated type-specific free macros.  The selected fields are freed according
 * to the C binding ownership model.  In particular, ownership of IDL sequence
 * buffers is controlled by @ref dds_sequence_t and its `_release` flag.
 */
typedef enum
{
  DDS_FREE_ALL = DDS_FREE_KEY_BIT | DDS_FREE_CONTENTS_BIT | DDS_FREE_ALL_BIT, /**< Free owned contents and the sample object itself. */
  DDS_FREE_CONTENTS = DDS_FREE_KEY_BIT | DDS_FREE_CONTENTS_BIT, /**< Free owned key and non-key contents, but leave the sample object intact. */
  DDS_FREE_KEY = DDS_FREE_KEY_BIT /**< Free only owned key fields in the sample. */
}
dds_free_op_t;

/**
 * @brief DDS Allocator
 * @ingroup alloc
 * C-Style allocator API
 */
typedef struct dds_allocator
{
  void * (*malloc) (size_t size); /**< behave like C malloc */
  void * (*realloc) (void *ptr, size_t size); /**< behave like C realloc, may be null */
  void (*free) (void *ptr); /**< behave like C free */
}
dds_allocator_t;

/**
 * @brief Perform an alloc() with the default allocator.
 * @component memory_alloc
 *
 * @param[in] size number of bytes
 * @returns new pointer or NULL if out of memory
 */
DDS_EXPORT void * dds_alloc (size_t size);

/**
 * @brief Perform a realloc() with the default allocator.
 * @component memory_alloc
 *
 * @param[in] ptr previously alloc()'ed pointer
 * @param[in] size new size
 * @return new pointer or NULL if out of memory
 */
DDS_EXPORT void * dds_realloc (void * ptr, size_t size);

/**
 * @brief Perform a realloc() with the default allocator. Zero out memory.
 * @component memory_alloc
 *
 * @param[in] ptr previously alloc()'ed pointer
 * @param[in] size new size
 * @return new pointer or NULL if out of memory
 */
DDS_EXPORT void * dds_realloc_zero (void * ptr, size_t size);

/**
 * @brief Perform a free() on a memory fragment allocated with the default allocator.
 * @component memory_alloc
 *
 * @param[in] ptr previously alloc()'ed pointer
 */
DDS_EXPORT void dds_free (void * ptr);

#ifndef DOXYGEN_SHOULD_SKIP_THIS
typedef void * (*dds_alloc_fn_t) (size_t p);
typedef void * (*dds_realloc_fn_t) (void * a, size_t b);
typedef void (*dds_free_fn_t) (void *p);
#endif // DOXYGEN_SHOULD_SKIP_THIS

/**
 * @brief Allocated a string with size, accounting for the null terminator.
 * @component memory_alloc
 *
 * @param[in] size number of characters
 * @returns newly allocated string or NULL if out of memory
 */
DDS_EXPORT char * dds_string_alloc (size_t size);

/**
 * @brief Duplicate a null-terminated string
 * @component memory_alloc
 *
 * @param[in] str string to duplicate
 * @returns newly allocated duplicate string, or NULL if out of memory
 */
DDS_EXPORT char * dds_string_dup (const char * str);

/**
 * @brief Free a string, equivalent to dds_free
 * @component memory_alloc
 *
 * @param[in] str string to free
 */
DDS_EXPORT void dds_string_free (char * str);

/**
 * @brief Free (parts of) a sample according to the \ref dds_free_op_t
 * @component memory_alloc
 *
 * Frees the parts of `sample` selected by `op`.  The IDL compiler generates a
 * type-specific `<type>_free(d,o)` macro for each topic type; these generated
 * macros call `dds_sample_free(d, &<type>_desc, o)` and follow the same rules.
 *
 * The selected fields are freed according to the generated C binding ownership
 * model.  Non-null strings, external members, optional members and recursively
 * contained complex members are treated as sample-owned and freed as described
 * by the type descriptor.  For IDL sequence fields, ownership of the sequence
 * buffer is controlled by the `_release` flag of @ref dds_sequence_t: if it is
 * true, the sequence buffer is freed; if it is false, the buffer is left for
 * the application to manage.
 *
 * For `sequence<string>` and `sequence<wstring>`, `_release` also controls the
 * strings stored in the sequence buffer.  If `_release` is false, both the
 * buffer and the pointed-to strings are left untouched.  For sequences of
 * complex elements, the elements are stored in the sequence buffer, but their
 * owned contents are freed recursively even if the containing sequence buffer
 * itself is application-owned.
 *
 * Memory released by this function must have been allocated with the Cyclone
 * DDS allocator, for example by a read operation, by `dds_alloc` or by one of
 * the generated allocation helpers.
 *
 * @param[in] sample sample to free
 * @param[in] desc topic descriptor of the type this sample was created from.
 * @param[in] op Which parts of the sample to free.
 */
DDS_EXPORT void dds_sample_free (void * sample, const struct dds_topic_descriptor * desc, dds_free_op_t op);

#if defined (__cplusplus)
}
#endif
#endif
