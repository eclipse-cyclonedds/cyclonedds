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
#include <stdarg.h>
#include "dds/dds.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"
#include "test_util.h"

void tprintf (const char *msg, ...)
{
  va_list args;
  dds_time_t t = dds_time ();
  printf ("%d.%06d ", (int32_t) (t / DDS_NSECS_IN_SEC), (int32_t) (t % DDS_NSECS_IN_SEC) / 1000);
  va_start (args, msg);
  vprintf (msg, args);
  va_end (args);
}

char *create_unique_topic_name (const char *prefix, char *name, size_t size)
{
  static ddsrt_atomic_uint32_t count = DDSRT_ATOMIC_UINT64_INIT (0);
  const ddsrt_pid_t pid = ddsrt_getpid();
  const ddsrt_tid_t tid = ddsrt_gettid();
  const uint32_t nr = ddsrt_atomic_inc32_nv (&count);
  (void) snprintf (name, size, "%s%"PRIu32"_pid%" PRIdPID "_tid%" PRIdTID "", prefix, nr, pid, tid);
  return name;
}
