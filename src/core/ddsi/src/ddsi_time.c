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
#include "ddsi__time.h"

bool ddsi_is_valid_timestamp (ddsi_time_t t)
{
  return t.seconds != DDSI_TIME_INVALID.seconds && t.fraction != DDSI_TIME_INVALID.fraction;
}

static ddsi_time_t to_ddsi_time (int64_t t)
{
  if (t == DDS_NEVER)
    return DDSI_TIME_INFINITE;
  else
  {
    /* ceiling(ns * 2^32/10^9) -- can't change the ceiling to round-to-nearest
       because that would break backwards compatibility, but round-to-nearest
       of the inverse is correctly rounded anyway, so it shouldn't ever matter. */
    ddsi_time_t x;
    int ns = (int) (t % DDS_NSECS_IN_SEC);
    x.seconds = (int) (t / DDS_NSECS_IN_SEC);
    x.fraction = (unsigned) (((DDS_NSECS_IN_SEC-1) + ((int64_t) ns << 32)) / DDS_NSECS_IN_SEC);
    return x;
  }
}

ddsi_time_t ddsi_wctime_to_ddsi_time (ddsrt_wctime_t t)
{
  return to_ddsi_time (t.v);
}

static int64_t from_ddsi_time (ddsi_time_t x)
{
  if (x.seconds == DDSI_TIME_INFINITE.seconds && x.fraction == DDSI_TIME_INFINITE.fraction)
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

ddsi_duration_t ddsi_duration_from_dds (dds_duration_t x)
{
  return to_ddsi_time (x);
}

dds_duration_t ddsi_duration_to_dds (ddsi_duration_t x)
{
  return from_ddsi_time (x);
}

