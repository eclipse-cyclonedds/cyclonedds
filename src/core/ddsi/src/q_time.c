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

#include "cyclonedds/ddsrt/time.h"
#include "cyclonedds/ddsi/q_time.h"

nn_wctime_t now (void)
{
  /* This function uses the wall clock.
   * This clock is not affected by time spent in suspend mode.
   * This clock is affected when the real time system clock jumps
   * forwards/backwards */
  nn_wctime_t t;
  t.v = dds_time();
  return t;
}

nn_mtime_t now_mt (void)
{
  /* This function uses the monotonic clock.
   * This clock stops while the system is in suspend mode.
   * This clock is not affected by any jumps of the realtime clock. */
  nn_mtime_t t;
  t.v = ddsrt_time_monotonic();
  return t;
}

nn_etime_t now_et (void)
{
  /* This function uses the elapsed clock.
   * This clock is not affected by any jumps of the realtime clock.
   * This clock does NOT stop when the system is in suspend mode.
   * This clock stops when the system is shut down, and starts when the system is restarted.
   * When restarted, there are no assumptions about the initial value of clock. */
  nn_etime_t t;
  t.v = ddsrt_time_elapsed();
  return t;
}

static void time_to_sec_usec (int32_t * __restrict sec, int32_t * __restrict usec, int64_t t)
{
  *sec = (int32_t) (t / T_SECOND);
  *usec = (int32_t) (t % T_SECOND) / 1000;
}

void mtime_to_sec_usec (int32_t * __restrict sec, int32_t * __restrict usec, nn_mtime_t t)
{
  time_to_sec_usec (sec, usec, t.v);
}

void wctime_to_sec_usec (int32_t * __restrict sec, int32_t * __restrict usec, nn_wctime_t t)
{
  time_to_sec_usec (sec, usec, t.v);
}

void etime_to_sec_usec (int32_t * __restrict sec, int32_t * __restrict usec, nn_etime_t t)
{
  time_to_sec_usec (sec, usec, t.v);
}


nn_mtime_t mtime_round_up (nn_mtime_t t, int64_t round)
{
  /* This function rounds up t to the nearest next multiple of round.
     t is nanoseconds, round is milliseconds.  Avoid functions from
     maths libraries to keep code portable */
  assert (t.v >= 0 && round >= 0);
  if (round == 0 || t.v == T_NEVER)
  {
    return t;
  }
  else
  {
    int64_t remainder = t.v % round;
    if (remainder == 0)
    {
      return t;
    }
    else
    {
      nn_mtime_t u;
      u.v = t.v + round - remainder;
      return u;
    }
  }
}

static int64_t add_duration_to_time (int64_t t, int64_t d)
{
  uint64_t sum;
  assert (t >= 0 && d >= 0);
  sum = (uint64_t)t + (uint64_t)d;
  return sum >= T_NEVER ? T_NEVER : (int64_t)sum;
}

nn_mtime_t add_duration_to_mtime (nn_mtime_t t, int64_t d)
{
  /* assumed T_NEVER <=> MAX_INT64 */
  nn_mtime_t u;
  u.v = add_duration_to_time (t.v, d);
  return u;
}

nn_wctime_t add_duration_to_wctime (nn_wctime_t t, int64_t d)
{
  /* assumed T_NEVER <=> MAX_INT64 */
  nn_wctime_t u;
  u.v = add_duration_to_time (t.v, d);
  return u;
}

nn_etime_t add_duration_to_etime (nn_etime_t t, int64_t d)
{
  /* assumed T_NEVER <=> MAX_INT64 */
  nn_etime_t u;
  u.v = add_duration_to_time (t.v, d);
  return u;
}

int valid_ddsi_timestamp (ddsi_time_t t)
{
  return t.seconds != DDSI_TIME_INVALID.seconds && t.fraction != DDSI_TIME_INVALID.fraction;
}

static ddsi_time_t nn_to_ddsi_time (int64_t t)
{
  if (t == T_NEVER)
    return DDSI_TIME_INFINITE;
  else
  {
    /* ceiling(ns * 2^32/10^9) -- can't change the ceiling to round-to-nearest
       because that would break backwards compatibility, but round-to-nearest
       of the inverse is correctly rounded anyway, so it shouldn't ever matter. */
    ddsi_time_t x;
    int ns = (int) (t % T_SECOND);
    x.seconds = (int) (t / T_SECOND);
    x.fraction = (unsigned) (((T_SECOND-1) + ((int64_t) ns << 32)) / T_SECOND);
    return x;
  }
}

ddsi_time_t nn_wctime_to_ddsi_time (nn_wctime_t t)
{
  return nn_to_ddsi_time (t.v);
}

static int64_t nn_from_ddsi_time (ddsi_time_t x)
{
  if (x.seconds == DDSI_TIME_INFINITE.seconds && x.fraction == DDSI_TIME_INFINITE.fraction)
    return T_NEVER;
  else
  {
    /* Round-to-nearest conversion of DDSI time fraction to nanoseconds */
    int ns = (int) (((int64_t) 2147483648u + (int64_t) x.fraction * T_SECOND) >> 32);
    return x.seconds * (int64_t) T_SECOND + ns;
  }
}

nn_wctime_t nn_wctime_from_ddsi_time (ddsi_time_t x)
{
  nn_wctime_t t;
  t.v = nn_from_ddsi_time (x);
  return t;
}

ddsi_duration_t nn_to_ddsi_duration (int64_t x)
{
  return nn_to_ddsi_time (x);
}

int64_t nn_from_ddsi_duration (ddsi_duration_t x)
{
  return nn_from_ddsi_time (x);
}

