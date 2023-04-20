// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "windows.h"
#include <stddef.h>
#include <process.h>
#include <libloaderapi.h>
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/string.h"

ddsrt_pid_t
ddsrt_getpid(void)
{
  return GetCurrentProcessId();
}

char *
ddsrt_getprocessname(void)
{
  char *ret;
  char buff[256];
  if (GetModuleFileNameA(NULL, buff, sizeof(buff)) == 0) {
    if (!ddsrt_asprintf(&ret, "process-%ld", (long) ddsrt_getpid())) return NULL;
    return ret;
  }
  return ddsrt_strdup(buff);
}
