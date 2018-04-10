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
/** \file os/common/code/os_time.c
 *  \brief Common time management services
 *
 * Implements os_timeAdd, os_timeSub, os_timeCompare,
 * os_timeAbs, os_timeMulReal, os_timeToReal, os_realToTime
 * which are platform independent
 */

#include <time.h>
#include <stdlib.h>
#include <assert.h>
#include "os/os.h"

/* double type definition for calculations in os_timeMulReal 	*/
/* Can be adapted to available and required accuracy		*/
typedef double os_cdouble;

/** \brief return value of \b t1 + \b t2
 *
 * If the value \b t1 + \b t2 does not fit in os_time the value
 * will be incorrect.
 */
os_time os_timeAdd(os_time t1, os_time t2)
{
  os_time tr;

  assert (t1.tv_nsec >= 0);
  assert (t1.tv_nsec < 1000000000);
  assert (t2.tv_nsec >= 0);
  assert (t2.tv_nsec < 1000000000);

  tr.tv_nsec = t1.tv_nsec + t2.tv_nsec;
  tr.tv_sec = t1.tv_sec + t2.tv_sec;
  if (tr.tv_nsec >= 1000000000) {
    tr.tv_sec++;
    tr.tv_nsec = tr.tv_nsec - 1000000000;
  }
  return tr;
}

/** \brief return value of \b t1 - \b t2
 *
 * If the value \b t1 - \b t2 does not fit in os_time the value
 * will be incorrect.
 */
os_time os_timeSub(os_time t1, os_time t2)
{
  os_time tr;

  assert (t1.tv_nsec >= 0);
  assert (t1.tv_nsec < 1000000000);
  assert (t2.tv_nsec >= 0);
  assert (t2.tv_nsec < 1000000000);

  if (t1.tv_nsec >= t2.tv_nsec) {
    tr.tv_nsec = t1.tv_nsec - t2.tv_nsec;
    tr.tv_sec = t1.tv_sec - t2.tv_sec;
  } else {
    tr.tv_nsec = t1.tv_nsec - t2.tv_nsec + 1000000000;
    tr.tv_sec = t1.tv_sec - t2.tv_sec - 1;
  }
  return tr;
}

/** \brief Compare \b t1 with \b t2
 *
 * - If the value of \b t1 < value of \b t2 return \b OS_LESS
 * - If the value of \b t1 > value of \b t2 return \b OS_MORE
 * - If the value of \b t1 equals the value of \b t2 return \b OS_EQUAL
 */
int
os_timeCompare(
               os_time t1,
               os_time t2)
{
  int rv;

  assert (t1.tv_nsec >= 0);
  assert (t1.tv_nsec < 1000000000);
  assert (t2.tv_nsec >= 0);
  assert (t2.tv_nsec < 1000000000);

  if (t1.tv_sec < t2.tv_sec) {
    rv = -1;
  } else if (t1.tv_sec > t2.tv_sec) {
    rv = 1;
  } else if (t1.tv_nsec < t2.tv_nsec) {
    rv = -1;
  } else if (t1.tv_nsec > t2.tv_nsec) {
    rv = 1;
  } else {
    rv = 0;
  }
  return rv;
}

/** \brief return absolute value \b t
 *
 * If the value |\b t| does not fit in os_time the value
 * will be incorrect.
 */
os_time
os_timeAbs(
           os_time t)
{
  os_time tr;

  assert (t.tv_nsec >= 0);
  assert (t.tv_nsec < 1000000000);

  if (t.tv_sec < 0) {
    tr.tv_sec = -t.tv_sec - 1;
    tr.tv_nsec = 1000000000 - t.tv_nsec;
  } else {
    tr.tv_sec = t.tv_sec;
    tr.tv_nsec = t.tv_nsec;
  }
  return tr;
}

/** \brief return value \b t * \b multiply
 *
 * if the result value does not fit in os_time the value
 * will be incorrect.
 */
os_time
os_timeMulReal(
               os_time t,
               double multiply)
{
  os_time tr;
  os_cdouble trr;
  os_cdouble sec;
  os_cdouble nsec;

  assert (t.tv_nsec >= 0);
  assert (t.tv_nsec < 1000000000);

  sec = (os_cdouble)t.tv_sec;
  nsec = (os_cdouble)t.tv_nsec / (os_cdouble)1000000000.0;
  trr = (sec + nsec) * multiply;
  if (trr >= 0.0) {
    tr.tv_sec = (os_timeSec)trr;
    tr.tv_nsec = (int)((trr-(os_cdouble)tr.tv_sec) * (os_cdouble)1000000000.0);
  } else {
    tr.tv_sec = (os_timeSec)trr - 1;
    tr.tv_nsec = (int)((trr-(os_cdouble)tr.tv_sec) * (os_cdouble)1000000000.0);
  }
  return tr;
}

/** \brief return floating point representation of \b t
 *
 * because of the limited floating point represenation (64 bits)
 * the value will be limited to a resoltion of about 1 us.
 */
os_timeReal
os_timeToReal(
              os_time t)
{
  volatile os_timeReal tr; /* This var is volatile to bypass a GCC 3.x bug on X86 */

  assert (t.tv_nsec >= 0);
  assert (t.tv_nsec < 1000000000);

  tr = (os_timeReal)t.tv_sec + (os_timeReal)t.tv_nsec / (os_timeReal)1000000000.0;

  return tr;
}

/** \brief return os_time represenation of floating time \b t
 *
 * because of the limited floating point represenation (64 bits)
 * the value will be limited to a resoltion of about 1 us.
 */
os_time
os_realToTime(
              os_timeReal t)
{
  os_time tr;

  if (t >= 0.0) {
    tr.tv_sec = (os_timeSec)t;
    tr.tv_nsec = (int)((t-(os_timeReal)tr.tv_sec) * (os_timeReal)1000000000.0);
  } else {
    tr.tv_sec = (os_timeSec)t - 1;
    tr.tv_nsec = (int)((t-(os_timeReal)tr.tv_sec) * (os_timeReal)1000000000.0);
  }
  assert(tr.tv_nsec >= 0 && tr.tv_nsec < 1000000000);
  return tr;
}

#if 0
os_result
os_timeGetPowerEvents(
                      os_timePowerEvents *events,
                      os_time maxBlockingTime)
{
  static os_time mt_el_offset; /* offset of os_timeGetElapsed() to os_timeGetMonotonic */
  static int mt_el_offset_isset;
  const os_time maxPowerEventDiffTime = { 3, 0 };     /* use 3 seconds as the threshold to determine whether a power event has occurred or not */
  const os_time delay = { 0, 100000000 };             /* 100ms; seems a good balance between performance vs. reactivy */
  static const os_time zeroTime = { 0, 0 };
  os_time toBlock, delta, mt, el;
  os_boolean stateEqualsEvents;
  os_boolean timeoutDetected = OS_FALSE;
  static os_timePowerEvents state;

  assert(events);

  toBlock = maxBlockingTime;

  do {
    /* TODO: When OSPL-4394 (clock-property querying) is done, this loop can
     * be skipped when either of the clocks isn't available with the right
     * properties. Perhaps the call should even return something to signal
     * that this call isn't supported. */
    mt = os_timeGetMonotonic();
    el = os_timeGetElapsed(); /* Determine el last */

    /* This isn't thread-safe, but since syncing should be more or less
     * idempotent, this doesn't matter functionally. */
    if(!mt_el_offset_isset){
      mt_el_offset = os_timeSub(mt, el);
      mt_el_offset_isset = 1;
    }

    /* A resume event is detected when the elapsed time differs from the
     * monotonic time (expressed in elapsed-time) > maxPowerEventDiffTime. */
    delta = os_timeSub(el, os_timeSub(mt, mt_el_offset));
    if (os_timeCompare(delta, maxPowerEventDiffTime) == OS_MORE) {
      pa_inc32_nv(&state.resumeCount);
      /* Updating state.resumeLastDetected is NOT thread-safe! Consequently,
       * these could be assigned an incorrect time value that does not
       * reflect the actual time of occurrence of a power event when
       * different threads are setting this value concurrently. The time
       * value is (according to the interface) supposed to be used for
       * logging only. */
      state.resumeLastDetected = os_timeGet();
    }

    /* In all cases after the above check, the clocks can be re-synced. This
     * isn't thread-safe, but since re-syncing should be more or less
     * idempotent, this doesn't matter functionally. */
    mt_el_offset = os_timeSub(mt, el);

    /* If maxBlockingTime == 0, events is not an in-parameter, so its
     * contents shouldn't be inspected. Furthermore, the function should
     * never return os_resultTimeOut in this case, so break out of the loop. */
    if(os_timeCompare(maxBlockingTime, zeroTime) == OS_EQUAL){
      break;
    }

    stateEqualsEvents = (memcmp(&state, events, sizeof state) == 0) ? OS_TRUE : OS_FALSE;

    if (stateEqualsEvents == OS_TRUE) {
      if (os_timeCompare(toBlock, zeroTime) == OS_EQUAL) {
        /* maxBlockingTime reached, break the loop */
        timeoutDetected = OS_TRUE;
      } else if (os_timeCompare(delay, toBlock) == OS_LESS) {
        /* It is safe to sleep for delay */
        os_nanoSleep(delay);
        /* Set the new max blocking time and redo the loop. */
        toBlock = os_timeSub(toBlock, delay);
      } else {
        /* The time to block is less than delay. */
        os_nanoSleep(toBlock);
        /* Set the resulting max blocking time zero to check for power
         * events one more time. */
        toBlock = zeroTime;
      }
    }
  } while ( (stateEqualsEvents == OS_TRUE) && (timeoutDetected == OS_FALSE) );

  /* Store current state in events */
  *events = state;

  return timeoutDetected ? os_resultTimeout : os_resultSuccess;
}
#endif

/* All implementations have to implement the os__timeDefaultTimeGet() function.
 * This is the default clock that is used for os_timeGet(). This wrapper
 * implements the ability to set a user-clock. */

/* Const-pointer to the default time implementation function. */
static os_time (*const os_time_clockGetDefaultFunc)(void) = os__timeDefaultTimeGet;
/* Pointer to the actual time implementation function. */
static os_time (*os_time_clockGetFunc)(void) = os__timeDefaultTimeGet;

/** \brief Set the user clock
 *
 * \b os_timeSetUserClock sets the current time source
 * get function.
 */
void
os_timeSetUserClock(
                    os_time (*userClock)(void))
{
  if (userClock) {
    os_time_clockGetFunc = userClock;
  } else {
    os_time_clockGetFunc = os_time_clockGetDefaultFunc;
  }
}

/** \brief Get the current time
 *
 * This common wrapper implements the user-clock overloading.
 */
os_time
os_timeGet (
            void)
{
  return os_time_clockGetFunc();
}

size_t os_ctime_r (os_time *t, char *buf, size_t bufsz)
{
  size_t result = 0;
  time_t tt = t->tv_sec;

  assert(bufsz >= OS_CTIME_R_BUFSIZE);

  if (buf) {
    /* This should be common code. But unfortunately, VS2012 C Runtime contains
     * a bug that causes a crash when using %Z with a buffer that is too small:
     * https://connect.microsoft.com/VisualStudio/feedback/details/782889/
     * So, don't execute strftime with %Z when VS2012 is the compiler. */
#if !(_MSC_VER == 1700)
    result = strftime(buf, bufsz, "%a %b %d %H:%M:%S %Z %Y", localtime(&tt));
#endif

    if(result == 0) {
      /* If not enough room was available, the %Z (time-zone) is left out
       * resulting in the output as expected from ctime_r. */
      result = strftime(buf, bufsz, "%a %b %d %H:%M:%S %Y", localtime(&tt));
      assert(result);
    }
  }
  return result;
}


