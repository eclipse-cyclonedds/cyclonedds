// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"


ddsrt_pid_t
ddsrt_getpid(void)
{
  /* Mapped to taskIdSelf() in VxWorks kernel mode. */
  return getpid();
}

#if (defined(__APPLE__) || defined(__FreeBSD__) || defined(_GNU_SOURCE) || defined(__ZEPHYR__))
  // _basename not needed
#else
static const char *_basename(char const *path)
{
  const char *s = strrchr(path, '/');
  return s ? s + 1 : path;
}
#endif

char *
ddsrt_getprocessname(void)
{
#if defined(__APPLE__) || defined(__FreeBSD__)
  const char * appname = getprogname();
#elif defined(_GNU_SOURCE)
  const char * appname = program_invocation_name;
#elif defined(__ZEPHYR__)
  const char * appname = NULL; /* CONFIG_KERNEL_BIN_NAME? */
#else
  const char * appname = NULL;

  char buff[400];
  FILE *fp;
  if ((fp = fopen("/proc/self/cmdline", "r")) != NULL) {
    buff[0] = '\0';
    for(size_t i = 0; i < sizeof(buff); ++i) {
      int c = fgetc(fp);
      if (c == EOF || c == '\0') {
        buff[i] = '\0';
        break;
      } else {
        buff[i] = (char) c;
      }
    }
    if (buff[0] != '\0') {
      appname = _basename(buff);
    }
    fclose(fp);
  }
#endif

  if (appname) {
    return ddsrt_strdup (appname);
  } else {
    char *ret = NULL;
    if (ddsrt_asprintf (&ret, "process-%ld", (long) ddsrt_getpid()) > 0) {
      return ret;
    } else {
      if (ret)
        ddsrt_free (ret);
      return NULL;
    }
  }
}

