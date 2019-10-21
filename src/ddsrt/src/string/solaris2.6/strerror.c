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
/* Make sure we get the XSI compliant version of strerror_r */
#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE
#undef _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "cyclonedds/ddsrt/string.h"

dds_return_t
ddsrt_strerror_r(int errnum, char *buf, size_t buflen)
{
  assert(buf != NULL);
  assert(buflen > 0);
  if (snprintf (buf, buflen, "errno=%d", errnum) >= buflen)
    return DDS_RETCODE_NOT_ENOUGH_SPACE;
  else
    return DDS_RETCODE_OK;
}
