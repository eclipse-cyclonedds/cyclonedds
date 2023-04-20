// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include "CUnit/Test.h"

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/heap.h"

CU_Init(ddsrt_heap)
{
  ddsrt_init();
  return 0;
}

CU_Clean(ddsrt_heap)
{
  ddsrt_fini();
  return 0;
}

static const size_t allocsizes[] = {0, 1, 2, 3, 4, 5, 10, 20, 257, 1024};
static const size_t nof_allocsizes = sizeof allocsizes / sizeof *allocsizes;

CU_Test(ddsrt_heap, malloc)
{
  for(size_t i = 0; i < nof_allocsizes; i++) {
    for(size_t j = 0; j < nof_allocsizes; j++) {
      size_t s = allocsizes[i] * allocsizes[j]; /* Allocates up to 1MB */
      void *ptr = ddsrt_malloc(s);
      CU_ASSERT_PTR_NOT_NULL_FATAL(ptr); /* ddsrt_malloc is supposed to abort on failure */
      assert(ptr);
      memset(ptr, 0, s); /* This potentially segfaults if the actual allocated block is too small */
      ddsrt_free(ptr);
    }
  }
  CU_PASS("ddsrt_malloc");
}

CU_Test(ddsrt_heap, calloc)
{
  for(size_t i = 0; i < nof_allocsizes; i++) {
    for(size_t j = 0; j < nof_allocsizes; j++) {
      char *ptr = ddsrt_calloc(allocsizes[i], allocsizes[j]);
      CU_ASSERT_PTR_NOT_EQUAL(ptr, NULL); /* ddsrt_calloc is supposed to abort on failure */
      if(allocsizes[i] * allocsizes[j] > 0) {
        CU_ASSERT (ptr[0] == 0 && !memcmp(ptr, ptr + 1, (allocsizes[i] * allocsizes[j]) - 1)); /* ddsrt_calloc should memset properly */
      }
      ddsrt_free(ptr);
    }
  }
  CU_PASS("ddsrt_calloc");
}

CU_Test(ddsrt_heap, realloc)
{
  char *ptr = NULL;
  size_t unchanged, s, prevs = 0;

  for(size_t i = 0; i < nof_allocsizes; i++) {
    for(size_t j = 0; j < nof_allocsizes; j++) {
      s = allocsizes[i] * allocsizes[j]; /* Allocates up to 1MB */
      printf("ddsrt_realloc(%p) %zu -> %zu\n", ptr, prevs, s);
      ptr = ddsrt_realloc(ptr, s);
      CU_ASSERT_PTR_NOT_NULL_FATAL(ptr); /* ddsrt_realloc is supposed to abort on failure */
      assert(ptr);
      unchanged = (prevs < s) ? prevs : s;
      if(unchanged) {
        CU_ASSERT (ptr && ptr[0] == 1 && !memcmp(ptr, ptr + 1, unchanged - 1)); /* ddsrt_realloc shouldn't change memory */
      }
      memset(ptr, 1, s); /* This potentially segfaults if the actual allocated block is too small */
      prevs = s;
    }
  }
  ddsrt_free(ptr);
  CU_PASS("ddsrt_realloc");
}

static const size_t allocsizes_s[] = {0, 1, 2, 3, 4, 5, 10, 20, 257, 1024, 8192};
static const size_t nof_allocsizes_s = sizeof allocsizes_s / sizeof *allocsizes_s;

CU_Test(ddsrt_heap, malloc_s)
{
  for(size_t i = 0; i < nof_allocsizes_s; i++) {
    for(size_t j = 0; j < nof_allocsizes_s; j++) {
      size_t s = allocsizes_s[i] * allocsizes_s[j]; /* Allocates up to 8MB */
      void *ptr = ddsrt_malloc_s(s); /* If s == 0, ddsrt_malloc_s should still return a pointer */
      if(ptr) {
        memset(ptr, 0, s); /* This potentially segfaults if the actual allocated block is too small */
      } else if (s <= 16) {
        /* Failure to allocate can't be considered a test fault really,
         * except that a malloc(<=16) would fail is unlikely. */
        CU_FAIL("ddsrt_malloc_s(<=16) returned NULL");
      }
      ddsrt_free(ptr);
    }
  }
  CU_PASS("ddsrt_malloc_s");
}

CU_Test(ddsrt_heap, calloc_s)
{
  for(size_t i = 0; i < nof_allocsizes_s; i++) {
    for(size_t j = 0; j < nof_allocsizes_s; j++) {
      size_t s = allocsizes_s[i] * allocsizes_s[j];
      char *ptr = ddsrt_calloc_s(allocsizes_s[i], allocsizes_s[j]); /* If either one is 0, ddsrt_calloc_s should still return a pointer */
      if(ptr) {
        if(s) {
          CU_ASSERT (ptr[0] == 0 && !memcmp(ptr, ptr + 1, s - 1)); /* malloc_0_s should memset properly */
        }
      } else if (s <= 16) {
        /* Failure to allocate can't be considered a test fault really,
         * except that a calloc(<=16) would fail is unlikely. */
        CU_FAIL("ddsrt_calloc_s(<=16) returned NULL");
      }
      ddsrt_free(ptr);
    }
  }
  CU_PASS("ddsrt_calloc_s");
}

CU_Test(ddsrt_heap, ddsrt_realloc_s)
{
  char *newptr, *ptr = NULL;
  size_t unchanged, s, prevs = 0;

  for(size_t i = 0; i < nof_allocsizes_s; i++) {
    for(size_t j = 0; j < nof_allocsizes_s; j++) {
      s = allocsizes_s[i] * allocsizes_s[j]; /* Allocates up to 8MB */
      newptr = ddsrt_realloc_s(ptr, s);
      printf("%p = ddsrt_realloc_s(%p) %zu -> %zu\n", newptr, ptr, prevs, s);
      if (s <= 16) {
        /* Failure to allocate can't be considered a test fault really,
         * except that a ddsrt_realloc_s(0 < s <=16) would fail is unlikely. */
        CU_ASSERT_PTR_NOT_EQUAL(newptr, NULL);
      }
      if(newptr){
        unchanged = (prevs < s) ? prevs : s;
        if(unchanged) {
          CU_ASSERT (newptr[0] == 1 && !memcmp(newptr, newptr + 1, unchanged - 1)); /* ddsrt_realloc_s shouldn't change memory */
        }
        memset(newptr, 1, s); /* This potentially segfaults if the actual allocated block is too small */
      }
      prevs = s;
      ptr = newptr;
    }
  }
  ddsrt_free(ptr);
  CU_PASS("ddsrt_realloc_s");
}
