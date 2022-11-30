/*
 * Copyright(c) 2022 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSRT_SORT_H
#define DDSRT_SORT_H

#include <stdint.h>
#include <stddef.h>

#include "dds/export.h"
#include "dds/ddsrt/retcode.h"


#if defined(__cplusplus)
extern "C" {
#endif

typedef int (*ddsrt_sort_cmp_t)(const void *, const void *);
typedef int (*ddsrt_sort_ctx_cmp_t)(const void *, const void *, void *);


/**
 * @brief Sort an array in-place like qsort.
 *
 * This is a generic sorting function. The sorting algorithm is not
 * stable but runs in constant memory and runs worst-case in O(N^2).
 *
 * @param[in,out]   ptr     pointer to the array to sort
 * @param[in]       count   number of elements in the array
 * @param[in]       size	size of each element in the array in bytes
 * @param[in]       cmp     comparison function which returns ​a negative integer value if the first argument
 *                          is less than the second, a positive integer value if the first argument is greater
 *                          than the second and zero if the arguments are equivalent.
 */
DDS_EXPORT void
ddsrt_sort(void * ptr, size_t count, size_t size, ddsrt_sort_cmp_t cmp);

/**
 * @brief Sort an array in-place like qsort_r.
 *
 * This is a generic sorting function. The sorting algorithm is not
 * stable but runs in constant memory and runs worst-case in O(N^2).
 *
 * @param[in,out]   ptr     pointer to the array to sort
 * @param[in]       count   number of elements in the array
 * @param[in]       size	size of each element in the array in bytes
 * @param[in]       cmp     comparison function which returns ​a negative integer value if the first argument
 *                          is less than the second, a positive integer value if the first argument is greater
 *                          than the second and zero if the arguments are equivalent.
 *
 * @param[in]       context additional information (e.g., collating sequence), passed to comp as the third argument
 */
DDS_EXPORT void
ddsrt_sort_with_context(void * ptr, size_t count, size_t size, ddsrt_sort_ctx_cmp_t cmp, void* context);

#if defined(__cplusplus)
}
#endif

#endif /* DDSRT_SORT_H */
