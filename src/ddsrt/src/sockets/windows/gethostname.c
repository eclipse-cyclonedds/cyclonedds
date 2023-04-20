// Copyright(c) 2006 to 2021 ZettaScale Technology and others
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

#include "dds/ddsrt/sockets.h"

dds_return_t
ddsrt_gethostname(
  char *name,
  size_t len)
{
  assert(name != NULL);
  assert(len <= INT_MAX);

  if (len == 0) {
    return DDS_RETCODE_BAD_PARAMETER;
  } else if (gethostname(name, (int)len) == 0) {
    return DDS_RETCODE_OK;
  }

  switch(WSAGetLastError()) {
    case WSAEFAULT:
      return DDS_RETCODE_NOT_ENOUGH_SPACE;
    case WSAENETDOWN:
      return DDS_RETCODE_NO_NETWORK;
    case WSAEINPROGRESS:
      return DDS_RETCODE_IN_PROGRESS;
    default:
      break;
  }

  return DDS_RETCODE_ERROR;
}
