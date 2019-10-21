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
#include <limits.h>
#include <string.h>

#include "cyclonedds/ddsrt/retcode.h"

#ifdef _WRS_KERNEL

extern char *strerrorIf(int errcode);

/* VxWorks has a non-compliant strerror_r in kernel mode which only takes a
   buffer and an error number. Providing a buffer smaller than NAME_MAX + 1
   (256) may result in a buffer overflow. See target/src/libc/strerror.c for
   details. NAME_MAX is defined in limits.h. */

int
ddsrt_strerror_r(int errnum, char *buf, size_t buflen)
{
  const char *str;

  assert(buf != NULL);
  assert(buflen > 0);

  if (buflen < (NAME_MAX + 1))
    return DDS_RETCODE_NOT_ENOUGH_SPACE;
  if (strerrorIf(errnum) == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  (void)strerror_r(errnum, buf);
  return DDS_RETCODE_OK;
}

#else

int
ddsrt_strerror_r(int errnum, char *buf, size_t buflen)
{
  int err;
  const char *str;

  assert(buf != NULL);
  assert(buflen > 0);

  /* VxWorks's strerror_r always returns 0 (zero), so the only way to decide
     if the error was truncated is to check if the last position in the buffer
     is overwritten by strerror_r. */
  err = 0;
  buf[buflen - 1] = 'x';
  (void)strerror_r(errnum, buf, buflen);
  if (buf[buflen - 1] != 'x')
    return DDS_RETCODE_NOT_ENOUGH_SPACE;

  buf[buflen - 1] = '\0'; /* Always null terminate, just to be safe. */

  return DDS_RETCODE_OK;
}

#endif /* _WRS_KERNEL */
