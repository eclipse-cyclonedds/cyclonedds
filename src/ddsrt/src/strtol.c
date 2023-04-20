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

#include "dds/ddsrt/strtol.h"

int32_t ddsrt_todigit(const int chr)
{
  if (chr >= '0' && chr <= '9') {
    return chr - '0';
  } else if (chr >= 'a' && chr <= 'z') {
    return 10 + (chr - 'a');
  } else if (chr >= 'A' && chr <= 'Z') {
    return 10 + (chr - 'A');
  }

  return -1;
}

static dds_return_t
ullfstr(
  const char *str,
  char **endptr,
  int32_t base,
  unsigned long long *ullng,
  unsigned long long max)
{
  dds_return_t rc = DDS_RETCODE_OK;
  int num;
  size_t cnt = 0;
  unsigned long long tot = 0;

  assert(str != NULL);
  assert(ullng != NULL);

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

  *ullng = tot;

  return rc;
}

dds_return_t
ddsrt_strtoll(
  const char *str,
  char **endptr,
  int32_t base,
  long long *llng)
{
  dds_return_t rc = DDS_RETCODE_OK;
  size_t cnt = 0;
  int sign = 1;
  unsigned long long ullng = 0, max = INT64_MAX;

  assert(str != NULL);
  assert(llng != NULL);

  for (; isspace((unsigned char)str[cnt]); cnt++) {
    /* Ignore leading whitespace. */
  }

  if (str[cnt] == '-') {
    sign = -1;
    max++;
    cnt++;
  } else if (str[cnt] == '+') {
    cnt++;
  }

  rc = ullfstr(str + cnt, endptr, base, &ullng, max);
  if (endptr && *endptr == (str + cnt))
    *endptr = (char *)str;
  if (rc != DDS_RETCODE_BAD_PARAMETER) {
    *llng = (long long)ullng;
    if (sign == -1 && *llng != INT64_MIN)
      *llng = - *llng;
  }
  return rc;
}

dds_return_t
ddsrt_strtoull(
  const char *str,
  char **endptr,
  int32_t base,
  unsigned long long *ullng)
{
  dds_return_t rc = DDS_RETCODE_OK;
  size_t cnt = 0;
  unsigned long long tot = 1;
  unsigned long long max = UINT64_MAX;

  assert(str != NULL);
  assert(ullng != NULL);

  for (; isspace((unsigned char)str[cnt]); cnt++) {
    /* ignore leading whitespace */
  }

  if (str[cnt] == '-') {
    tot = (unsigned long long) -1;
    cnt++;
  } else if (str[cnt] == '+') {
    cnt++;
  }

  rc = ullfstr(str + cnt, endptr, base, ullng, max);
  if (endptr && *endptr == (str + cnt))
    *endptr = (char *)str;
  if (rc != DDS_RETCODE_BAD_PARAMETER)
    *ullng *= tot;

  return rc;
}

dds_return_t
ddsrt_atoll(
  const char *str,
  long long *llng)
{
  return ddsrt_strtoll(str, NULL, 10, llng);
}

dds_return_t
ddsrt_atoull(
  const char *str,
  unsigned long long *ullng)
{
  return ddsrt_strtoull(str, NULL, 10, ullng);
}

char *
ddsrt_ulltostr(
  unsigned long long num,
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
ddsrt_lltostr(
  long long num,
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
        pos = (unsigned long long)INT64_MAX + 1;
      } else {
        pos = (unsigned long long) -num;
      }

      str[cnt++] = '-';
    } else {
      pos = (unsigned long long) num;
    }

    (void)ddsrt_ulltostr(pos, str + cnt, len - cnt, &ptr);
  }

  if (endptr != NULL) {
    *endptr = ptr;
  }

  return str;
}
