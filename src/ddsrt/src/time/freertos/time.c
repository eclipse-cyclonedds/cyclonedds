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

extern inline TickType_t ddsrt_duration_to_ticks_ceil(dds_duration_t reltime);

dds_time_t dds_time(void)
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
  return (ddsrt_wctime_t) { dds_time() };
}

ddsrt_mtime_t ddsrt_time_monotonic (void)
{
  return (ddsrt_mtime_t) { xTaskGetTickCount() * NSECS_PER_TICK };
}

ddsrt_etime_t ddsrt_time_elapsed (void)
{
  /* Elapsed time clock not (yet) supported on this platform. */
  return (ddsrt_etime_t) { xTaskGetTickCount() * NSECS_PER_TICK };
}

void dds_sleepfor (dds_duration_t reltime)
{
  TickType_t ticks;

  ticks = ddsrt_duration_to_ticks_ceil(reltime);
  vTaskDelay(ticks);
}
