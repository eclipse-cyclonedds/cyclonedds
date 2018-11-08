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
#include "CUnit/Test.h"
#include "os/os.h"


CU_Init(os_heap)
{
    int result = 0;
    os_osInit();
    printf("Run os_heap_Initialize\n");

    return result;
}

CU_Clean(os_heap)
{
    int result = 0;
    os_osExit();
    printf("Run os_heap_Cleanup\n");

    return result;
}

static const size_t allocsizes[] = {0, 1, 2, 3, 4, 5, 10, 20, 257, 1024};
static const size_t nof_allocsizes = sizeof allocsizes / sizeof *allocsizes;

CU_Test(os_heap, os_malloc)
{
    for(size_t i = 0; i < nof_allocsizes; i++) {
        for(size_t j = 0; j < nof_allocsizes; j++) {
            size_t s = allocsizes[i] * allocsizes[j]; /* Allocates up to 1MB */
            void *ptr = os_malloc(s);
            CU_ASSERT_PTR_NOT_EQUAL(ptr, NULL); /* os_malloc is supposed to abort on failure */
            memset(ptr, 0, s); /* This potentially segfaults if the actual allocated block is too small */
            os_free(ptr);
        }
    }
    CU_PASS("os_malloc");
}

CU_Test(os_heap, os_malloc_0)
{
    for(size_t i = 0; i < nof_allocsizes; i++) {
        for(size_t j = 0; j < nof_allocsizes; j++) {
            size_t s = allocsizes[i] * allocsizes[j]; /* Allocates up to 1MB */
            char *ptr = os_malloc_0(s);
            CU_ASSERT_PTR_NOT_EQUAL(ptr, NULL); /* os_malloc_0 is supposed to abort on failure */
            if(s) {
                CU_ASSERT (ptr[0] == 0 && !memcmp(ptr, ptr + 1, s - 1)); /* malloc_0 should memset properly */
            }
            os_free(ptr);
        }
    }
    CU_PASS("os_malloc_0");
}

CU_Test(os_heap, os_calloc)
{
    for(size_t i = 0; i < nof_allocsizes; i++) {
        for(size_t j = 0; j < nof_allocsizes; j++) {
            char *ptr = os_calloc(allocsizes[i], allocsizes[j]);
            CU_ASSERT_PTR_NOT_EQUAL(ptr, NULL); /* os_calloc is supposed to abort on failure */
            if(allocsizes[i] * allocsizes[j] > 0) {
                CU_ASSERT (ptr[0] == 0 && !memcmp(ptr, ptr + 1, (allocsizes[i] * allocsizes[j]) - 1)); /* os_calloc should memset properly */
            }
            os_free(ptr);
        }
    }
    CU_PASS("os_calloc");
}

CU_Test(os_heap, os_realloc)
{
    char *ptr = NULL;
    size_t unchanged, s, prevs = 0;

    for(size_t i = 0; i < nof_allocsizes; i++) {
        for(size_t j = 0; j < nof_allocsizes; j++) {
            s = allocsizes[i] * allocsizes[j]; /* Allocates up to 1MB */
            printf("os_realloc(%p) %zu -> %zu\n", ptr, prevs, s);
            ptr = os_realloc(ptr, s);
            CU_ASSERT_PTR_NOT_EQUAL(ptr, NULL); /* os_realloc is supposed to abort on failure */
            unchanged = (prevs < s) ? prevs : s;
            if(unchanged) {
                CU_ASSERT (ptr[0] == 1 && !memcmp(ptr, ptr + 1, unchanged - 1)); /* os_realloc shouldn't change memory */
            }
            memset(ptr, 1, s); /* This potentially segfaults if the actual allocated block is too small */
            prevs = s;
        }
    }
    os_free(ptr);
    CU_PASS("os_realloc");
}

static const size_t allocsizes_s[] = {0, 1, 2, 3, 4, 5, 10, 20, 257, 1024, 8192};
static const size_t nof_allocsizes_s = sizeof allocsizes_s / sizeof *allocsizes_s;

CU_Test(os_heap, os_malloc_s)
{
    for(size_t i = 0; i < nof_allocsizes_s; i++) {
        for(size_t j = 0; j < nof_allocsizes_s; j++) {
            size_t s = allocsizes_s[i] * allocsizes_s[j]; /* Allocates up to 8MB */
            void *ptr = os_malloc_s(s); /* If s == 0, os_malloc_s should still return a pointer */
            if(ptr) {
                memset(ptr, 0, s); /* This potentially segfaults if the actual allocated block is too small */
            } else if (s <= 16) {
                /* Failure to allocate can't be considered a test fault really,
                 * except that a malloc(<=16) would fail is unlikely. */
                CU_FAIL("os_malloc_0_s(<=16) returned NULL");
            }
            os_free(ptr);
        }
    }
    CU_PASS("os_malloc_s");
}

CU_Test(os_heap, os_malloc_0_s)
{
    for(size_t i = 0; i < nof_allocsizes_s; i++) {
        for(size_t j = 0; j < nof_allocsizes_s; j++) {
            size_t s = allocsizes_s[i] * allocsizes_s[j]; /* Allocates up to 8MB */
            char *ptr = os_malloc_0_s(s); /* If s == 0, os_malloc_0_s should still return a pointer */
            if(ptr) {
                if(s) {
                    CU_ASSERT (ptr[0] == 0 && !memcmp(ptr, ptr + 1, s - 1)); /* malloc_0_s should memset properly */
                }
            } else if (s <= 16) {
                /* Failure to allocate can't be considered a test fault really,
                 * except that a malloc(<=16) would fail is unlikely. */
                CU_FAIL("os_malloc_0_s(<=16) returned NULL");
            }
            os_free(ptr);
        }
    }
    CU_PASS("os_malloc_0_s");
}

CU_Test(os_heap, os_calloc_s)
{
    for(size_t i = 0; i < nof_allocsizes_s; i++) {
        for(size_t j = 0; j < nof_allocsizes_s; j++) {
            size_t s = allocsizes_s[i] * allocsizes_s[j];
            char *ptr = os_calloc_s(allocsizes_s[i], allocsizes_s[j]); /* If either one is 0, os_calloc_s should still return a pointer */
            if(ptr) {
                if(s) {
                    CU_ASSERT (ptr[0] == 0 && !memcmp(ptr, ptr + 1, s - 1)); /* malloc_0_s should memset properly */
                }
            } else if (s <= 16) {
                /* Failure to allocate can't be considered a test fault really,
                 * except that a calloc(<=16) would fail is unlikely. */
                CU_FAIL("os_calloc_s(<=16) returned NULL");
            }
            os_free(ptr);
        }
    }
    CU_PASS("os_calloc_s");
}

CU_Test(os_heap, os_realloc_s)
{
    char *newptr, *ptr = NULL;
    size_t unchanged, s, prevs = 0;

    for(size_t i = 0; i < nof_allocsizes_s; i++) {
        for(size_t j = 0; j < nof_allocsizes_s; j++) {
            s = allocsizes_s[i] * allocsizes_s[j]; /* Allocates up to 8MB */
            newptr = os_realloc_s(ptr, s);
            printf("%p = os_realloc_s(%p) %zu -> %zu\n", newptr, ptr, prevs, s);
            if (s <= 16) {
                /* Failure to allocate can't be considered a test fault really,
                 * except that a os_realloc_s(0 < s <=16) would fail is unlikely. */
                CU_ASSERT_PTR_NOT_EQUAL(newptr, NULL);
            }
            if(newptr){
                unchanged = (prevs < s) ? prevs : s;
                if(unchanged) {
                    CU_ASSERT (newptr[0] == 1 && !memcmp(newptr, newptr + 1, unchanged - 1)); /* os_realloc_s shouldn't change memory */
                }
                memset(newptr, 1, s); /* This potentially segfaults if the actual allocated block is too small */
            }
            prevs = s;
            ptr = newptr;
        }
    }
    os_free(ptr);
    CU_PASS("os_realloc_s");
}
