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
#include <string.h>

#include "CUnit/Theory.h"
#include "cyclonedds/ddsrt/string.h"

typedef enum { eq, lt, gt } eq_t;

CU_TheoryDataPoints(ddsrt_strcasecmp, basic) = {
  CU_DataPoints(const char *, "a", "aa", "a",  "a", "A", "a", "b", "a", "B", "A", "", "a"),
  CU_DataPoints(const char *, "a", "a",  "aa", "A", "a", "b", "a", "b", "A", "B", "a", ""),
  CU_DataPoints(eq_t, eq, gt, lt, eq, eq, lt, gt, lt, gt, lt, lt, gt)
};

CU_Theory((const char *s1, const char *s2, eq_t e), ddsrt_strcasecmp, basic)
{
  int r = ddsrt_strcasecmp(s1, s2);
  CU_ASSERT((e == eq && r == 0) || (e == lt && r < 0) || (e == gt && r > 0));
}

CU_TheoryDataPoints(ddsrt_strncasecmp, basic) = {
  CU_DataPoints(const char *, "a", "aa", "a",  "A", "a", "b", "a", "B", "A", "", "a"),
  CU_DataPoints(const char *, "a", "a",  "aa", "a", "A", "a", "b", "A", "B", "a", ""),
  CU_DataPoints(size_t, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
  CU_DataPoints(eq_t, eq, gt, lt, eq, eq, gt, lt, gt, lt, lt, gt)
};

CU_Theory((const char *s1, const char *s2, size_t n, eq_t e), ddsrt_strncasecmp, basic)
{
  int r = ddsrt_strncasecmp(s1, s2, n);
  CU_ASSERT((e == eq && r == 0) || (e == lt && r < 0) || (e == gt && r > 0));
}

CU_TheoryDataPoints(ddsrt_strncasecmp, empty) = {
  CU_DataPoints(const char *, "a", "", "a", "", "a", ""),
  CU_DataPoints(const char *, "", "a", "", "a", "", "a"),
  CU_DataPoints(size_t, 1, 1, 0, 0, 2, 2),
  CU_DataPoints(eq_t, gt, lt, eq, eq, gt, lt)
};

CU_Theory((const char *s1, const char *s2, size_t n, eq_t e), ddsrt_strncasecmp, empty)
{
  int r = ddsrt_strncasecmp(s1, s2, n);
  CU_ASSERT((e == eq && r == 0) || (e == lt && r < 0) || (e == gt && r > 0));
}

CU_TheoryDataPoints(ddsrt_strncasecmp, length) = {
  CU_DataPoints(const char *, "aBcD", "AbCX", "aBcD", "AbCX", "aBcD"),
  CU_DataPoints(const char *, "AbCX", "aBcD", "AbCX", "aBcD", "AbCd"),
  CU_DataPoints(size_t, 3, 3, 4, 4, 5, 5),
  CU_DataPoints(eq_t, eq, eq, lt, gt, eq, eq)
};

CU_Theory((const char *s1, const char *s2, size_t n, eq_t e), ddsrt_strncasecmp, length)
{
  int r = ddsrt_strncasecmp(s1, s2, n);
  CU_ASSERT((e == eq && r == 0) || (e == lt && r < 0) || (e == gt && r > 0));
}

