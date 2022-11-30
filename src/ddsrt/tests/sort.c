/*
 * Copyright(c) 2023 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CUnit/Test.h"
#include "dds/ddsrt/sort.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/misc.h"

static int integer_compare(const void *a, const void *b) {
    const int ai = *((const int*) a);
    const int bi = *((const int*) b);

    if (ai == bi) return 0;
    if (ai < bi) return -1;
    return 1;
}

static int was_called(const void *a, const void *b, void *context) {
    DDSRT_UNUSED_ARG(a);
    DDSRT_UNUSED_ARG(b);
    *((bool*) context) = true;
    return 0;
}

CU_Test(ddsrt_sorting, trivial)
{
    bool hascall = false;
    int A[] = {1, 2};
    ddsrt_sort_with_context(A, 0, sizeof(int), was_called, &hascall);
    CU_ASSERT_FATAL(!hascall);
    ddsrt_sort_with_context(A, 1, sizeof(int), was_called, &hascall);
    CU_ASSERT_FATAL(!hascall);
    ddsrt_sort_with_context(A, 2, sizeof(int), was_called, &hascall);
    CU_ASSERT_FATAL(hascall);
}


CU_Test(ddsrt_sorting, integers)
{
  int A[6][3] = {
    {1, 2, 3},
    {3, 2, 1},
    {2, 1, 3},
    {3, 1, 2},
    {2, 3, 1},
    {1, 3, 2}
  };
  for (int i = 0; i < 6; ++i) {
    ddsrt_sort(A[i], 3, sizeof(int), integer_compare);
    CU_ASSERT_FATAL(A[i][0] == 1 && A[i][1] == 2 &&A[i][2] == 3);
  }
}

#define BAGGAGE_SIZE 1000

typedef struct big_type {
    int sort_by;
    int baggage[BAGGAGE_SIZE];
} *big_type_t;

static int big_type_compare(const void *a, const void *b) {
    const big_type_t ai = ((const big_type_t) a);
    const big_type_t bi = ((const big_type_t) b);

    if (ai->sort_by == bi->sort_by) return 0;
    if (ai->sort_by < bi->sort_by) return -1;
    return 1;
}

CU_Test(ddsrt_sorting, big_types)
{
  big_type_t structs = (big_type_t) ddsrt_calloc(6, sizeof(struct big_type));
  // Clang static analyzer things the check below is not enough...
  CU_ASSERT_FATAL(structs != NULL);
  if (structs == NULL) abort();

  for (int i = 0; i < 6; ++i) {
    (structs + i)->sort_by = i;
    for (int j = 0; j < BAGGAGE_SIZE; ++j) (structs + i)->baggage[j] = j;
  }

  ddsrt_sort(structs, 6, sizeof(struct big_type), big_type_compare);

  bool intact = true;
  for (int i = 0; i < 6; ++i) {
    if ((structs + i)->sort_by != i) intact = false;

    for (int j = 0; j < BAGGAGE_SIZE; ++j) {
        if ((structs + i)->baggage[j] != j)
            intact = false;
    }
  }
  CU_ASSERT_FATAL(intact);
  ddsrt_free(structs);
}

#undef BAGGAGE_SIZE