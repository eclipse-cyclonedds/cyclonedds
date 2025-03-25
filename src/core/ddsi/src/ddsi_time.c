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

#include "dds/ddsrt/time.h"
#include "dds/ddsrt/static_assert.h"
#include "ddsi__time.h"

bool ddsi_is_valid_timestamp (ddsi_time_t t)
{
  // all bit patterns are valid DDSI Timestamps (including "invalid"!), but we reject infinity
  // because it is not a point in time
  return !(t.seconds == DDSI_TIME_INFINITE_SECONDS && t.fraction == DDSI_TIME_INFINITE_FRACTION);
}

bool ddsi_is_valid_timestamp22 (ddsi_time22_t t)
{
  // all bit patterns are valid DDSI Timestamps (including "invalid"!), but we reject infinity
  // because it is not a point in time
  return !(t.seconds == DDSI_TIME22_INFINITE_SECONDS && t.fraction == DDSI_TIME22_INFINITE_FRACTION);
}

static ddsi_time_t to_ddsi_time (int64_t t)
{
  assert (t >= 0 || t == DDS_TIME_INVALID);
  if (t == DDS_NEVER)
    return DDSI_TIME_INFINITE;
  else
  {
    /* ceiling(ns * 2^32/10^9) -- can't change the ceiling to round-to-nearest
       because that would break backwards compatibility, but round-to-nearest
       of the inverse is correctly rounded anyway, so it shouldn't ever matter. */
    ddsi_time_t x;
    int ns = (int) (t % DDS_NSECS_IN_SEC);
    x.seconds = (uint32_t) (t / DDS_NSECS_IN_SEC);
    x.fraction = (unsigned) (((DDS_NSECS_IN_SEC-1) + ((int64_t) ns << 32)) / DDS_NSECS_IN_SEC);
    return x;
  }
}

static void to_ddsi_time22_duration (int32_t * restrict seconds, uint32_t * restrict fraction, int64_t t)
{
  assert (t >= 0 || t == DDS_TIME_INVALID);
  if (t == DDS_NEVER)
  {
    DDSRT_STATIC_ASSERT(DDSI_TIME22_INVALID_SECONDS == DDSI_DURATION_INVALID_SECONDS &&
                        DDSI_TIME22_INVALID_FRACTION == DDSI_DURATION_INVALID_FRACTION &&
                        DDSI_TIME22_INFINITE_SECONDS == DDSI_DURATION_INFINITE_SECONDS &&
                        DDSI_TIME22_INFINITE_FRACTION == DDSI_DURATION_INFINITE_FRACTION);
    *seconds = DDSI_TIME22_INFINITE_SECONDS;
    *fraction = DDSI_TIME22_INFINITE_FRACTION;
  }
  else
  {
    /* ceiling(ns * 2^32/10^9) -- can't change the ceiling to round-to-nearest
       because that would break backwards compatibility, but round-to-nearest
       of the inverse is correctly rounded anyway, so it shouldn't ever matter. */
    int ns = (int) (t % DDS_NSECS_IN_SEC);
    *seconds = (int) (t / DDS_NSECS_IN_SEC);
    *fraction = (unsigned) (((DDS_NSECS_IN_SEC-1) + ((int64_t) ns << 32)) / DDS_NSECS_IN_SEC);
  }
}

ddsi_time_t ddsi_wctime_to_ddsi_time (ddsrt_wctime_t t)
{
  return to_ddsi_time (t.v);
}

ddsi_time22_t ddsi_wctime_to_ddsi_time22 (ddsrt_wctime_t t)
{
  ddsi_time22_t out;
  to_ddsi_time22_duration (&out.seconds, &out.fraction, t.v);
  return out;
}

static int64_t from_ddsi_time (ddsi_time_t x)
{
  if (x.seconds == DDSI_TIME_INVALID_SECONDS && x.fraction == DDSI_TIME_INVALID_FRACTION)
    return INT64_MIN;
  if (x.seconds == DDSI_TIME_INFINITE_SECONDS && x.fraction == DDSI_TIME_INFINITE_FRACTION)
    return DDS_NEVER;
  else
  {
    /* Round-to-nearest conversion of DDSI time fraction to nanoseconds */
    int ns = (int) (((int64_t) 2147483648u + (int64_t) x.fraction * DDS_NSECS_IN_SEC) >> 32);
    return x.seconds * DDS_NSECS_IN_SEC + ns;
  }
}

ddsrt_wctime_t ddsi_wctime_from_ddsi_time (ddsi_time_t x)
{
  return (ddsrt_wctime_t) { from_ddsi_time (x) };
}

static int64_t from_ddsi_time22_duration (int32_t seconds, uint32_t fraction)
{
  // DURATION_INVALID doesn't really exist
  DDSRT_STATIC_ASSERT(DDSI_TIME22_INVALID_SECONDS == DDSI_DURATION_INVALID_SECONDS &&
                      DDSI_TIME22_INVALID_FRACTION == DDSI_DURATION_INVALID_FRACTION &&
                      DDSI_TIME22_INFINITE_SECONDS == DDSI_DURATION_INFINITE_SECONDS &&
                      DDSI_TIME22_INFINITE_FRACTION == DDSI_DURATION_INFINITE_FRACTION);
  if (seconds == DDSI_TIME22_INVALID_SECONDS && fraction == DDSI_TIME22_INVALID_FRACTION)
    return INT64_MIN;
  if (seconds == DDSI_TIME22_INFINITE_SECONDS && fraction == DDSI_TIME22_INFINITE_FRACTION)
    return DDS_NEVER;
  else
  {
    /* Round-to-nearest conversion of DDSI time fraction to nanoseconds */
    int ns = (int) (((int64_t) 2147483648u + (int64_t) fraction * DDS_NSECS_IN_SEC) >> 32);
    return seconds * DDS_NSECS_IN_SEC + ns;
  }
}

ddsrt_wctime_t ddsi_wctime_from_ddsi_time22 (ddsi_time22_t x)
{
  return (ddsrt_wctime_t) { from_ddsi_time22_duration (x.seconds, x.fraction) };
}

ddsi_duration_t ddsi_duration_from_dds (dds_duration_t x)
{
  ddsi_duration_t out;
  assert (x >= 0);
  to_ddsi_time22_duration (&out.seconds, &out.fraction, x);
  return out;
}

dds_duration_t ddsi_duration_to_dds (ddsi_duration_t x)
{
  return from_ddsi_time22_duration (x.seconds, x.fraction);
}
