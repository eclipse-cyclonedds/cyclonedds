// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <string.h>

#include "CUnit/Theory.h"
#include "dds/ddsrt/string.h"

CU_TheoryDataPoints(ddsrt_strlcpy, dest_size) = {
  CU_DataPoints(char *, "foo", "foo", "foo", "foo", "foo", "", "", ""),
  CU_DataPoints(size_t, 0,     1,     3,     4,     5,     0,  1,  2)
};

CU_Theory((char *src, size_t size), ddsrt_strlcpy, dest_size)
{
  char dest[] = "................";
  size_t len, srclen;

  srclen = strlen(src);
  len = ddsrt_strlcpy(dest, src, size);
  CU_ASSERT_EQUAL(len, srclen);
  if (size > 0) {
    if ((size - 1) < len) {
      len = size - 1;
    }
    CU_ASSERT_EQUAL(dest[len], '\0');
    CU_ASSERT_EQUAL(dest[len+1], '.');
    CU_ASSERT((strncmp(dest, src, len) == 0));
  } else {
    CU_ASSERT_EQUAL(dest[0], '.');
  }
}

CU_TheoryDataPoints(ddsrt_strlcat, dest_size) = {
  CU_DataPoints(char *, "",    "",    "",    "",    "foo", "foo", "foo", "foo", "foo", "foo", "foo", "", "", "foo", "foo", "foo"),
  CU_DataPoints(char *, "bar", "bar", "bar", "bar", "bar", "bar", "bar", "bar", "bar", "bar", "bar", "", "", "",    "",    ""),
  CU_DataPoints(size_t, 0,     1,     3,     4,     0,     1,     3,     4,     5,     6,     7,     0,  1,  3,     4,     5)
};

CU_Theory((char *seed, char *src, size_t size), ddsrt_strlcat, dest_size)
{
  char dest[] = "................";
  size_t len, seedlen, srclen;
  seedlen = strlen(seed);
  srclen = strlen(src);
  memcpy(dest, seed, seedlen);
  dest[seedlen] = '\0';

  len = ddsrt_strlcat(dest, src, size);
  CU_ASSERT_EQUAL(len, (seedlen + srclen));
  if (size > 0) {
    char foobar[sizeof(dest)];

    if ((size - 1) <= seedlen) {
      len = seedlen;
    } else if ((size - 1) <= len) {
      len = size - 1;
    }

    CU_ASSERT_EQUAL(dest[len], '\0');

    if (seedlen < (size - 1)) {
      CU_ASSERT_EQUAL(dest[len+1], '.');
    }

    (void)snprintf(foobar, len+1, "%s%s", seed, src);
    CU_ASSERT((strncmp(dest, foobar, len) == 0));
  } else {
    CU_ASSERT((strcmp(dest, seed) == 0));
  }
}
