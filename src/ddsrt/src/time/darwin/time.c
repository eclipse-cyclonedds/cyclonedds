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
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <AvailabilityMacros.h>

#if !(defined MAC_OS_X_VERSION_10_12 && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12)
#include <mach/mach_time.h>
#endif

#include "dds/ddsrt/time.h"

dds_time_t dds_time(void)
{
#if defined MAC_OS_X_VERSION_10_12 && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12
  return (int64_t) clock_gettime_nsec_np (CLOCK_REALTIME);
#else
  struct timeval tv;
  (void)gettimeofday(&tv, NULL);
  return ((tv.tv_sec * DDS_NSECS_IN_SEC) + (tv.tv_usec * DDS_NSECS_IN_USEC));
#endif
}

ddsrt_wctime_t ddsrt_time_wallclock(void)
{
  return (ddsrt_wctime_t) { dds_time () };
}

ddsrt_mtime_t ddsrt_time_monotonic(void)
{
#if defined MAC_OS_X_VERSION_10_12 && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12
  return (ddsrt_mtime_t) { (int64_t) clock_gettime_nsec_np (CLOCK_UPTIME_RAW) };
#else
  static mach_timebase_info_data_t timeInfo;
  uint64_t mt;

  /* The Mach absolute time returned by mach_absolute_time is very similar to
   * the QueryPerformanceCounter on Windows. The update-rate isn't fixed, so
   * that information needs to be resolved to provide a clock with real-time
   * progression.
   *
   * The mach_absolute_time does include time spent during sleep (on Intel
   * CPU's, not on PPC), but not the time spent during suspend.
   *
   * The result is not adjusted based on NTP, so long-term progression by
   * this clock may not match the time progression made by the real-time
   * clock. */
  mt = mach_absolute_time();

  if (timeInfo.denom == 0) {
    (void)mach_timebase_info(&timeInfo);
  }

  return (ddsrt_mtime_t) { mt * timeInfo.numer / timeInfo.denom };
#endif
}

ddsrt_etime_t ddsrt_time_elapsed(void)
{
#if defined MAC_OS_X_VERSION_10_12 && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_12
  return (ddsrt_etime_t) { (int64_t) clock_gettime_nsec_np (CLOCK_MONOTONIC_RAW) };
#else
  /* Elapsed time clock not (yet) supported on this platform. */
  ddsrt_mtime_t mt = ddsrt_time_monotonic();
  return (ddsrt_etime_t) { mt.v };
#endif
}
