// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <limits.h>
#include <string.h>

#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/string.h"

#if !LWIP_SOCKET
#include <errno.h>
#endif

#if defined(__VXWORKS__)
#include <hostLib.h>
#endif /* __VXWORKS__ */

#if !defined(HOST_NAME_MAX)
# if LWIP_SOCKET
#   define HOST_NAME_MAX DNS_MAX_NAME_LENGTH
# elif defined(_POSIX_HOST_NAME_MAX)
#   define HOST_NAME_MAX _POSIX_HOST_NAME_MAX
# endif
#endif

#if DDSRT_HAVE_GETHOSTNAME
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 256
#endif

dds_return_t
ddsrt_gethostname(
  char *name,
  size_t len)
{
  char buf[HOST_NAME_MAX + 1 /* '\0' */];

  memset(buf, 0, sizeof(buf));

  if (gethostname(buf, HOST_NAME_MAX) == 0) {
    /* If truncation occurrs, no error is returned whether or not the buffer
       is null-terminated. */
    if (buf[HOST_NAME_MAX - 1] != '\0' ||
        ddsrt_strlcpy(name, buf, len) >= len)
    {
      return DDS_RETCODE_NOT_ENOUGH_SPACE;
    }

    return DDS_RETCODE_OK;
  } else {
    switch (errno) {
      case EFAULT: /* Invalid address (cannot happen). */
        return DDS_RETCODE_ERROR;
      case EINVAL: /* Negative length (cannot happen). */
        return DDS_RETCODE_ERROR;
      case ENAMETOOLONG:
        return DDS_RETCODE_NOT_ENOUGH_SPACE;
      default:
        break;
    }
  }

  return DDS_RETCODE_ERROR;
}
#endif // DDSRT_GETHOSTNAME
