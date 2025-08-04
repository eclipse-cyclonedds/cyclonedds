// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <FreeRTOS.h>
#include <task.h>
#define _POSIX_TIMERS
#include <time.h>

#include "dds/ddsrt/time.h"

dds_time_t dds_time (void)
{
  struct timespec ts;
#if __STDC_VERSION__ >= 201112L
  timespec_get(&ts, TIME_UTC);
#else
  (void)clock_gettime(CLOCK_REALTIME, &ts);
#endif
  return (ts.tv_sec * DDS_NSECS_IN_SEC) + ts.tv_nsec;
}

#define NSECS_PER_TICK (DDS_NSECS_IN_SEC / configTICK_RATE_HZ)

ddsrt_wctime_t ddsrt_time_wallclock (void)
{
  return (ddsrt_wctime_t) { dds_time () };
}

ddsrt_mtime_t ddsrt_time_monotonic (void)
{
  return (ddsrt_mtime_t) { xTaskGetTickCount () * NSECS_PER_TICK };
}

ddsrt_etime_t ddsrt_time_elapsed (void)
{
  /* Elapsed time clock not (yet) supported on this platform. */
  return (ddsrt_etime_t) { xTaskGetTickCount () * NSECS_PER_TICK };
}

ddsrt_hrtime_t ddsrt_time_highres(void)
{
  ddsrt_mtime_t mt = ddsrt_time_monotonic();
  return (ddsrt_hrtime_t) { (uint64_t) mt.v };
}

void dds_sleepfor (dds_duration_t reltime)
{
#define NSECS_PER_TICK (DDS_NSECS_IN_SEC / configTICK_RATE_HZ)
  TickType_t ticks;
  assert (portMAX_DELAY > configTICK_RATE_HZ);
  const int64_t max_nsecs =
    (DDS_INFINITY / NSECS_PER_TICK < portMAX_DELAY
     ? DDS_INFINITY - 1 : portMAX_DELAY * NSECS_PER_TICK);
  if (reltime < max_nsecs - (NSECS_PER_TICK - 1))
  {
    ticks = (TickType_t) ((reltime + (NSECS_PER_TICK - 1)) / NSECS_PER_TICK);
  }
  else
  {
    ticks = portMAX_DELAY;
  }
  vTaskDelay (ticks);
#undef NSECS_PER_TICK
}
