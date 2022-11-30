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

#include "dds/ddsrt/sort.h"
#include <string.h>

#define DDSRT_SORT_SWAP_SIZE 1024

static void do_swap(void * ptr1, void * ptr2, size_t size) {
    char tmp[DDSRT_SORT_SWAP_SIZE];

    if (size <= DDSRT_SORT_SWAP_SIZE) {
        memmove(tmp, ptr1, size);
        memmove(ptr1, ptr2, size);
        memmove(ptr2, tmp, size);
    } else {
        size_t subsize;
        for(subsize = 0; subsize < size - DDSRT_SORT_SWAP_SIZE; subsize += DDSRT_SORT_SWAP_SIZE) {
            memmove((char*) tmp, (char*) ptr1 + subsize, DDSRT_SORT_SWAP_SIZE);
            memmove((char*) ptr1 + subsize, (char*) ptr2 + subsize, DDSRT_SORT_SWAP_SIZE);
            memmove((char*) ptr2 + subsize, (char*) tmp, DDSRT_SORT_SWAP_SIZE);
        }
        memmove((char*) tmp, (char*) ptr1 + subsize, size - subsize);
        memmove((char*) ptr1 + subsize, (char*) ptr2 + subsize, size - subsize);
        memmove((char*) ptr2 + subsize, (char*) tmp, size - subsize);
    }
}

#undef DDSRT_SORT_SWAP_SIZE

inline static void sift_down_ctx(void * ptr, size_t count, size_t size, ddsrt_sort_ctx_cmp_t cmp, void* context, size_t start) {
    size_t j = start, k = start;
    while(1) {
        if ((j << 1) + 1 < count &&
            cmp(
                (void*) ((char*) ptr + ((j << 1) + 1) * size),
                (void*) ((char*) ptr + k * size),
                context
            ) > 0)
            k = (j << 1) + 1;
        if ((j << 1) + 2 < count &&
            cmp(
                (void*) ((char*) ptr + ((j << 1) + 2) * size),
                (void*) ((char*) ptr + k * size),
                context
            ) > 0)
            k = (j << 1) + 2;
        if (k != j) {
            do_swap((void*) ((char*) ptr + (j * size)), (void*) ((char*) ptr + (k * size)), size);
            j = k;
            continue;
        }
        break;
    };
}

inline static void sift_down_noctx(void * ptr, size_t count, size_t size, ddsrt_sort_cmp_t cmp, size_t start) {
    size_t j = start, k = start;
    while(1) {
        if ((j << 1) + 1 < count &&
            cmp(
                (void*) ((char*) ptr + ((j << 1) + 1) * size),
                (void*) ((char*) ptr + k * size)
            ) > 0)
            k = (j << 1) + 1;
        if ((j << 1) + 2 < count &&
            cmp(
                (void*) ((char*) ptr + ((j << 1) + 2) * size),
                (void*) ((char*) ptr + k * size)
            ) > 0)
            k = (j << 1) + 2;
        if (k != j) {
            do_swap((void*) ((char*) ptr + (j * size)), (void*) ((char*) ptr + (k * size)), size);
            j = k;
            continue;
        }
        break;
    };
}

void ddsrt_sort_with_context(void * ptr, size_t count, size_t size, ddsrt_sort_ctx_cmp_t cmp, void* context)
{
    // This is a fairly naive straightforward heapsort implementation.
    // The amount of times we need to _sort_ in DDS is limited so
    // no attempt is made to make this as efficient as it can be.

    if (count < 2) {
        // An array of 0 or 1 elements is sorted by definition.
        return;
    }

    for (size_t j = ((count - 1) >> 1) + 1; j > 0; --j) {
        sift_down_ctx(ptr, count, size, cmp, context, j - 1);
    }
    for (size_t i = count - 1; i > 0; --i) {
        do_swap((void*) ((char *) ptr + i * size), ptr, size);
        sift_down_ctx(ptr, i, size, cmp, context, 0);
    }
}

void ddsrt_sort(void * ptr, size_t count, size_t size, ddsrt_sort_cmp_t cmp)
{
    // This is a fairly naive straightforward heapsort implementation.
    // The amount of times we need to _sort_ in DDS is limited so
    // no attempt is made to make this as efficient as it can be.

    if (count < 2) {
        // An array of 0 or 1 elements is sorted by definition.
        return;
    }

    for (size_t j = ((count - 1) >> 1) + 1; j > 0; --j) {
        sift_down_noctx(ptr, count, size, cmp, j - 1);
    }
    for (size_t i = count - 1; i > 0; --i) {
        do_swap((void*) ((char *) ptr + i * size), ptr, size);
        sift_down_noctx(ptr, i, size, cmp, 0);
    }
}
