// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include "dds/ddsrt/time.h"

dds_time_t dds_time(void)
{
  struct timespec ts;
  (void)clock_gettime(CLOCK_REALTIME, &ts);
  return (ts.tv_sec * DDS_NSECS_IN_SEC) + ts.tv_nsec;
}

ddsrt_wctime_t ddsrt_time_wallclock(void)
{
  struct timespec ts;
  (void)clock_gettime(CLOCK_REALTIME, &ts);
  return (ddsrt_wctime_t) { (ts.tv_sec * DDS_NSECS_IN_SEC) + ts.tv_nsec };
}

ddsrt_mtime_t ddsrt_time_monotonic(void)
{
  struct timespec ts;
  (void)clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ddsrt_mtime_t) { (ts.tv_sec * DDS_NSECS_IN_SEC) + ts.tv_nsec };
}

ddsrt_etime_t ddsrt_time_elapsed(void)
{
  /* Elapsed time clock not worth the bother for now. */
  struct timespec ts;
  (void)clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ddsrt_etime_t) { (ts.tv_sec * DDS_NSECS_IN_SEC) + ts.tv_nsec };
}
