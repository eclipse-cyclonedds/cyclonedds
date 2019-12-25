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
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"

int
ddsrt_asprintf(
  char **strp,
  const char *fmt,
  ...)
{
  int ret;
  unsigned int len;
  char buf[1] = { '\0' };
  char *str = NULL;
  va_list args1, args2;

  assert(strp != NULL);
  assert(fmt != NULL);

  va_start(args1, fmt);
  va_copy(args2, args1); /* va_list cannot be reused */

  if ((ret = vsnprintf(buf, sizeof(buf), fmt, args1)) >= 0) {
    len = (unsigned int)ret; /* +1 for null byte */
    if ((str = ddsrt_malloc(len + 1)) == NULL) {
      ret = -1;
    } else if ((ret = vsnprintf(str, len + 1, fmt, args2)) >= 0) {
      assert(((unsigned int)ret) == len);
      *strp = str;
    } else {
      ddsrt_free(str);
    }
  }

  va_end(args1);
  va_end(args2);

  return ret;
}

int
ddsrt_vasprintf(
  char **strp,
  const char *fmt,
  va_list ap)
{
  int ret;
  unsigned int len;
  char buf[1] = { '\0' };
  char *str = NULL;
  va_list ap2;

  assert(strp != NULL);
  assert(fmt != NULL);

  va_copy(ap2, ap); /* va_list cannot be reused */

  if ((ret = vsnprintf(buf, sizeof(buf), fmt, ap)) >= 0) {
    len = (unsigned int)ret;
    if ((str = ddsrt_malloc(len + 1)) == NULL) {
      ret = -1;
    } else if ((ret = vsnprintf(str, len + 1, fmt, ap2)) >= 0) {
      assert(((unsigned int)ret) == len);
      *strp = str;
    } else {
      ddsrt_free(str);
    }
  }

  va_end(ap2);

  return ret;
}

#if defined(_MSC_VER) && (_MSC_VER < 1900)
int
snprintf(
  char *str,
  size_t size,
  const char *fmt,
  ...)
{
  int cnt;
  va_list args;

  va_start(args, fmt);
  cnt = vsnprintf(str, size, fmt, args);
  va_end(args);

  return cnt;
}
#endif
