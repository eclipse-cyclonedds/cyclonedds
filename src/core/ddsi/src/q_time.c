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

#include "os/os.h"

#include "ddsi/q_time.h"

const nn_ddsi_time_t invalid_ddsi_timestamp = { -1, UINT32_MAX };
const nn_ddsi_time_t ddsi_time_infinite = { INT32_MAX, UINT32_MAX };
#if DDSI_DURATION_ACCORDING_TO_SPEC
const nn_duration_t duration_infinite = { INT32_MAX, INT32_MAX };
#else
const nn_duration_t duration_infinite = { INT32_MAX, UINT32_MAX };
#endif

nn_wctime_t now (void)
{
  /* This function uses the wall clock.
   * This clock is not affected by time spent in suspend mode.
   * This clock is affected when the real time system clock jumps
   * forwards/backwards */
  os_time tv;
  nn_wctime_t t;
  tv = os_timeGet ();
  t.v = ((int64_t) tv.tv_sec * T_SECOND + tv.tv_nsec);
  return t;
}

nn_mtime_t now_mt (void)
{
  /* This function uses the monotonic clock.
   * This clock stops while the system is in suspend mode.
   * This clock is not affected by any jumps of the realtime clock. */
  os_time tv;
  nn_mtime_t t;
  tv = os_timeGetMonotonic ();
  t.v = ((int64_t) tv.tv_sec * T_SECOND + tv.tv_nsec);
  return t;
}

nn_etime_t now_et (void)
{
  /* This function uses the elapsed clock.
   * This clock is not affected by any jumps of the realtime clock.
   * This clock does NOT stop when the system is in suspend mode.
   * This clock stops when the system is shut down, and starts when the system is restarted.
   * When restarted, there are no assumptions about the initial value of clock. */
  os_time tv;
  nn_etime_t t;
  tv = os_timeGetElapsed ();
  t.v = ((int64_t) tv.tv_sec * T_SECOND + tv.tv_nsec);
  return t;
}

static void time_to_sec_usec (_Out_ int * __restrict sec, _Out_ int * __restrict usec, _In_ int64_t t)
{
  *sec = (int) (t / T_SECOND);
  *usec = (int) (t % T_SECOND) / 1000;
}

void mtime_to_sec_usec (_Out_ int * __restrict sec, _Out_ int * __restrict usec, _In_ nn_mtime_t t)
{
  time_to_sec_usec (sec, usec, t.v);
}

void wctime_to_sec_usec (_Out_ int * __restrict sec, _Out_ int * __restrict usec, _In_ nn_wctime_t t)
{
  time_to_sec_usec (sec, usec, t.v);
}

void etime_to_sec_usec (_Out_ int * __restrict sec, _Out_ int * __restrict usec, _In_ nn_etime_t t)
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

int valid_ddsi_timestamp (nn_ddsi_time_t t)
{
  return t.seconds != invalid_ddsi_timestamp.seconds && t.fraction != invalid_ddsi_timestamp.fraction;
}

static nn_ddsi_time_t nn_to_ddsi_time (int64_t t)
{
  if (t == T_NEVER)
    return ddsi_time_infinite;
  else
  {
    /* ceiling(ns * 2^32/10^9) -- can't change the ceiling to round-to-nearest
       because that would break backwards compatibility, but round-to-nearest
       of the inverse is correctly rounded anyway, so it shouldn't ever matter. */
    nn_ddsi_time_t x;
    int ns = (int) (t % T_SECOND);
    x.seconds = (int) (t / T_SECOND);
    x.fraction = (unsigned) (((T_SECOND-1) + ((int64_t) ns << 32)) / T_SECOND);
    return x;
  }
}

nn_ddsi_time_t nn_wctime_to_ddsi_time (nn_wctime_t t)
{
  return nn_to_ddsi_time (t.v);
}

static int64_t nn_from_ddsi_time (nn_ddsi_time_t x)
{
  if (x.seconds == ddsi_time_infinite.seconds && x.fraction == ddsi_time_infinite.fraction)
    return T_NEVER;
  else
  {
    /* Round-to-nearest conversion of DDSI time fraction to nanoseconds */
    int ns = (int) (((int64_t) 2147483648u + (int64_t) x.fraction * T_SECOND) >> 32);
    return x.seconds * (int64_t) T_SECOND + ns;
  }
}

nn_wctime_t nn_wctime_from_ddsi_time (nn_ddsi_time_t x)
{
  nn_wctime_t t;
  t.v = nn_from_ddsi_time (x);
  return t;
}

#if DDSI_DURATION_ACCORDING_TO_SPEC
nn_duration_t nn_to_ddsi_duration (int64_t t)
{
  if (t == T_NEVER)
    return duration_infinite;
  else
  {
    nn_duration_t x;
    x.sec = (int) (t / T_SECOND);
    x.nanosec = (int) (t % T_SECOND);
    return x;
  }
}

int64_t nn_from_ddsi_duration (nn_duration_t x)
{
  int64_t t;
  if (x.sec == duration_infinite.sec && x.nanosec == duration_infinite.nanosec)
    t = T_NEVER;
  else
    t = x.sec * T_SECOND + x.nanosec;
  return t;
}
#else
nn_duration_t nn_to_ddsi_duration (int64_t x)
{
  return nn_to_ddsi_time (x);
}

int64_t nn_from_ddsi_duration (nn_duration_t x)
{
  return nn_from_ddsi_time (x);
}
#endif
