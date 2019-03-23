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
#define _GNU_SOURCE /* Required for RUSAGE_THREAD. */
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/resource.h>

#if defined(__APPLE__)
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/thread_act.h>
#endif

#include "dds/ddsrt/rusage.h"

dds_retcode_t
ddsrt_getrusage(int who, ddsrt_rusage_t *usage)
{
  int err = 0;
  struct rusage buf;
  dds_retcode_t rc;

  assert(who == DDSRT_RUSAGE_SELF || who == DDSRT_RUSAGE_THREAD);
  assert(usage != NULL);

  memset(&buf, 0, sizeof(buf));

#if defined(__linux)
  if ((who == DDSRT_RUSAGE_SELF && getrusage(RUSAGE_SELF, &buf) == -1) ||
      (who == DDSRT_RUSAGE_THREAD && getrusage(RUSAGE_THREAD, &buf) == -1))
  {
    err = errno;
  } else {
    buf.ru_maxrss *= 1024;
  }
#else
  if (getrusage(RUSAGE_SELF, &buf) == -1) {
    err = errno;
  } else if (who == DDSRT_RUSAGE_THREAD) {
    memset(&buf.ru_utime, 0, sizeof(buf.ru_utime));
    memset(&buf.ru_stime, 0, sizeof(buf.ru_stime));
    buf.ru_nvcsw = 0;
    buf.ru_nivcsw = 0;

#if defined(__APPLE__)
    kern_return_t ret;
    mach_port_t thr;
    mach_msg_type_number_t cnt;
    thread_basic_info_data_t info;

    thr = mach_thread_self();
    assert(thr != MACH_PORT_DEAD);
    if (thr == MACH_PORT_NULL) {
      /* Resource shortage prevented reception of send right. */
      err = ENOMEM;
    } else {
      cnt = THREAD_BASIC_INFO_COUNT;
      ret = thread_info(
        thr, THREAD_BASIC_INFO, (thread_info_t)&info, &cnt);
      assert(ret != KERN_INVALID_ARGUMENT);
      /* Assume MIG_ARRAY_TOO_LARGE will not happen. */
      buf.ru_utime.tv_sec = info.user_time.seconds;
      buf.ru_utime.tv_usec = info.user_time.microseconds;
      buf.ru_stime.tv_sec = info.system_time.seconds;
      buf.ru_stime.tv_usec = info.system_time.microseconds;
      mach_port_deallocate(mach_task_self(), thr);
    }
#endif /* __APPLE__ */
  }
#endif /* __linux */

  if (err == 0) {
    rc = DDS_RETCODE_OK;
    usage->utime =
      (buf.ru_utime.tv_sec * DDS_NSECS_IN_SEC) +
      (buf.ru_utime.tv_usec * DDS_NSECS_IN_USEC);
    usage->stime =
      (buf.ru_stime.tv_sec * DDS_NSECS_IN_SEC) +
      (buf.ru_stime.tv_usec * DDS_NSECS_IN_USEC);
    usage->maxrss = (size_t)buf.ru_maxrss;
    usage->idrss = (size_t)buf.ru_idrss;
    usage->nvcsw = (size_t)buf.ru_nvcsw;
    usage->nivcsw = (size_t)buf.ru_nivcsw;
  } else if (err == ENOMEM) {
    rc = DDS_RETCODE_OUT_OF_RESOURCES;
  } else {
    rc = DDS_RETCODE_ERROR;
  }

  return rc;
}
