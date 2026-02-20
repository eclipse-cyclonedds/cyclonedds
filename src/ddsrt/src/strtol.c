// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdbool.h>

#include "dds/ddsrt/strtol.h"

int32_t ddsrt_todigit(const int chr)
{
  if (chr >= '0' && chr <= '9') {
    return (char) chr - '0';
  } else if (chr >= 'a' && chr <= 'z') {
    return 10 + ((char) chr - 'a');
  } else if (chr >= 'A' && chr <= 'Z') {
    return 10 + ((char) chr - 'A');
  }

  return -1;
}

static dds_return_t
uint64str(
  const char *str,
  char **endptr,
  int32_t base,
  uint64_t *value,
  uint64_t max)
{
  dds_return_t rc = DDS_RETCODE_OK;
  int num;
  size_t cnt = 0;
  uint64_t tot = 0;

  assert(str != NULL);
  assert(value != NULL);

  if (base == 0) {
    if (str[0] == '0') {
      if ((str[1] == 'x' || str[1] == 'X') && ddsrt_todigit(str[2]) < 16) {
        base = 16;
        cnt = 2;
      } else {
        base = 8;
      }
    } else {
      base = 10;
    }
  } else if (base == 16) {
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
      cnt = 2;
    }
  } else if (base < 2 || base > 36) {
    return DDS_RETCODE_BAD_PARAMETER;
  }

  while ((rc == DDS_RETCODE_OK) &&
         (num = ddsrt_todigit(str[cnt])) >= 0 && num < base)
  {
    if (tot <= (max / (unsigned) base)) {
      tot *= (unsigned) base;
      tot += (unsigned) num;
      cnt++;
    } else {
      rc = DDS_RETCODE_OUT_OF_RANGE;
      tot = max;
    }
  }

  if (endptr != NULL) {
    *endptr = (char *)str + cnt;
  }

  *value = tot;
  return rc;
}

dds_return_t
ddsrt_strtoint64(
  const char *str,
  char **endptr,
  int32_t base,
  int64_t *value)
{
  dds_return_t rc = DDS_RETCODE_OK;
  size_t cnt = 0;
  bool negate = false;
  uint64_t u64 = 0;

  assert(str != NULL);
  assert(value != NULL);

  for (; isspace((unsigned char)str[cnt]); cnt++) {
    /* Ignore leading whitespace. */
  }

  if (str[cnt] == '-') {
    negate = true;
    cnt++;
  } else if (str[cnt] == '+') {
    cnt++;
  }

  rc = uint64str(str + cnt, endptr, base, &u64, (uint64_t) INT64_MAX + (negate ? 1 : 0));
  if (endptr && *endptr == (str + cnt))
    *endptr = (char *)str;
  if (rc != DDS_RETCODE_BAD_PARAMETER) {
    if (!negate)
      *value = (int64_t) u64;
    else if (u64 == 1 + (uint64_t) INT64_MAX)
      *value = INT64_MIN;
    else
      *value = - (int64_t) u64;
  }
  return rc;
}

dds_return_t
ddsrt_strtouint64(
  const char *str,
  char **endptr,
  int32_t base,
  uint64_t *value)
{
  dds_return_t rc = DDS_RETCODE_OK;
  size_t cnt = 0;
  bool negate = false;

  assert(str != NULL);
  assert(value != NULL);

  for (; isspace((unsigned char)str[cnt]); cnt++) {
    /* ignore leading whitespace */
  }

  if (str[cnt] == '-') {
    negate = true;
    cnt++;
  } else if (str[cnt] == '+') {
    cnt++;
  }

  rc = uint64str(str + cnt, endptr, base, value, UINT64_MAX);
  if (endptr && *endptr == (str + cnt))
    *endptr = (char *)str;
  if (rc != DDS_RETCODE_BAD_PARAMETER && negate) {
    // Microsoft thinks unary minus on an unsigned type is worthy of a warning,
    // so write out two's complement negation by hand
    *value = (~ *value) + 1;
  }

  return rc;
}

char *
ddsrt_uint64tostr(
  uint64_t num,
  char *str,
  size_t len,
  char **endptr)
{
  char chr, *ptr;
  size_t cnt;
  size_t lim = 0;
  size_t tot = 0;

  assert (str != NULL);

  if (len > 1) {
    lim = len - 1;

    do {
      str[tot] = (char)('0' + (int)(num % 10));
      num /= 10;

      if (tot == (lim - 1)) {
        if (num > 0ULL) {
          /* Simply using memmove would have been easier, but the function is
             safe to use in asynchronous code like this. Normally this code
             should not affect performance, because ideally the buffer is
             sufficiently large enough. */
          for (cnt = 0; cnt < tot; cnt++) {
            str[cnt] = str[cnt + 1];
          }
        }
      } else if (num > 0ULL) {
        tot++;
      }
    } while (num > 0ULL);

    lim = tot + 1;
  }

  for (cnt = 0; cnt < (tot - cnt); cnt++) {
    chr = str[tot - cnt];
    str[tot - cnt] = str[cnt];
    str[cnt] = chr;
  }

  if (len == 0) {
    str = NULL;
    ptr = NULL;
  } else {
    str[lim] = '\0';
    ptr = str + lim;
  }

  if (endptr != NULL) {
    *endptr = ptr;
  }

  return str;
}

char *
ddsrt_int64tostr(
  int64_t num,
  char *str,
  size_t len,
  char **endptr)
{
  unsigned long long pos;
  char *ptr;
  size_t cnt = 0;

  assert (str != NULL);

  if (len == 0) {
    str = NULL;
    ptr = NULL;
  } else if (len == 1) {
    str[0] = '\0';
    ptr = str;
  } else {
    if (num < 0LL) {
      if (num == INT64_MIN) {
        pos = (uint64_t) INT64_MAX + 1;
      } else {
        pos = (uint64_t) -num;
      }

      str[cnt++] = '-';
    } else {
      pos = (uint64_t) num;
    }

    (void)ddsrt_uint64tostr(pos, str + cnt, len - cnt, &ptr);
  }

  if (endptr != NULL) {
    *endptr = ptr;
  }

  return str;
}
