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
#include <assert.h>
#include <time.h>

#include "cyclonedds/ddsrt/misc.h"
#include "cyclonedds/ddsrt/time.h"

/**
 * @brief Calculate a time given an offset time and a duration.
 *
 * Negative time can become positive by adding a large enough duration, of
 * course a positive time can become negative given a large enough negative
 * duration.
 *
 * @param[in]  abstime  Timestamp in nanoseconds since UNIX Epoch.
 * @param[in]  reltime  Relative time in nanoseconds.
 *
 * @returns A timestamp in nanoseconds since UNIX Epoch.
 */
inline dds_time_t
ddsrt_time_add_duration(dds_time_t abstime, dds_duration_t reltime)
{
  assert(abstime >= 0);
  assert(reltime >= 0);

  return (reltime >= DDS_NEVER - abstime ? DDS_NEVER : abstime + reltime);
}

#if _WIN32
/**
 * @brief Convert a relative time to microseconds rounding up.
 *
 * @param[in]  reltime  Relative time to convert.
 *
 * @returns INFINITE if @reltime was @DDS_INIFINITY, relative time converted to
 *          microseconds otherwise.
 */
inline DWORD
ddsrt_duration_to_msecs_ceil(dds_duration_t reltime)
{
  if (reltime == DDS_INFINITY) {
    return INFINITE;
  } else if (reltime > 0) {
    assert(INFINITE < (DDS_INFINITY / DDS_NSECS_IN_MSEC));
    dds_duration_t max_nsecs = (INFINITE - 1) * DDS_NSECS_IN_MSEC;

    if (reltime < (max_nsecs - (DDS_NSECS_IN_MSEC - 1))) {
      reltime += (DDS_NSECS_IN_MSEC - 1);
    } else {
      reltime = max_nsecs;
    }

    return (DWORD)(reltime / DDS_NSECS_IN_MSEC);
  }

  return 0;
}
#endif
