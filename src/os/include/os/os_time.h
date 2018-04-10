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
/****************************************************************
 * Interface definition for time management of SPLICE-DDS       *
 ****************************************************************/

#ifndef OS_TIME_H
#define OS_TIME_H

#include "os/os_defs.h"
/* !!!!!!!!NOTE From here no more includes are allowed!!!!!!! */

#if defined (__cplusplus)
extern "C" {
#endif

  /** \brief Specification of maximum time values (platform dependent)
   */
#define OS_TIME_INFINITE_SEC INT32_MAX
#define OS_TIME_INFINITE_NSEC INT32_MAX

#define os_timeIsInfinite(t)                                            \
  (((t).tv_sec == OS_TIME_INFINITE_SEC) && ((t).tv_nsec == OS_TIME_INFINITE_NSEC))

  /** \brief Get the current time.
   *
   *  The returned time represents the seconds and nanoseconds since the Unix Epoch
   *  (Thursday, 1 January 1970, 00:00:00 (UTC)).
   *
   * Possible Results:
   * - returns "the current time"
   */
  OSAPI_EXPORT os_time
  os_timeGet(void);

    os_time os__timeDefaultTimeGet(void);


  /** \brief Get high resolution, monotonic time.
   *
   * The monotonic clock is a clock with near real-time progression and can be
   * used when a high-resolution time is needed without the need for it to be
   * related to the wall-clock. The resolution of the clock is typically the
   * highest available on the platform.
   *
   * The clock is not guaranteed to be strictly monotonic, but on most common
   * platforms it will be (based on performance-counters or HPET's).
   *
   * If no monotonic clock is available the real time clock is used.
   *
   * \return a high-resolution time with no guaranteed relation to wall-time
   *         when available
   * \return wall-time, otherwise
   */
  OSAPI_EXPORT os_time
  os_timeGetMonotonic(void);

  /** \brief Get high resolution, elapsed (and thus monotonic) time since some
   * fixed unspecified past time.
   *
   * The elapsed time clock is a clock with near real-time progression and can be
   * used when a high-resolution suspend-aware monotonic clock is needed, without
   * having to deal with the complications of discontinuities if for example the
   * time is changed. The fixed point from which the elapsed time is returned is
   * not guaranteed to be fixed over reboots of the system.
   *
   * If no elapsed time clock is available, the os_timeGetMonotonic() is used as a
   * fall-back.
   *
   * \return elapsed time since some unspecified fixed past time
   * \return os_timeGetMonotonic() otherwise
   */
  OSAPI_EXPORT os_time
  os_timeGetElapsed(void);

  /** \brief Add time t1 to time t2
   *
   * Possible Results:
   * - returns t1 + t2 when
   *     the result fits within the time structure
   * - returns an unspecified value when
   *     the result does not fit within the time structure
   */
  OSAPI_EXPORT os_time
  os_timeAdd(
          os_time t1,
          os_time t2);

  /** \brief Subtract time t2 from time t1
   *
   * Possible Results:
   * - returns t1 - t2 when
   *     the result fits within the time structure
   * - returns an unspecified value when
   *     the result does not fit within the time structure
   */
  OSAPI_EXPORT os_time
  os_timeSub(
          os_time t1,
          os_time t2);

  /** \brief Multiply time t with a real value
   *
   * Possible Results:
   * - returns t * multiply when
   *     the result fits within the time structure
   * - returns an unspecified value when
   *     the result does not fit within the time structure
   */
  OSAPI_EXPORT os_time
  os_timeMulReal(
          os_time t1,
          double multiply);

  /** \brief Determine the absolute value of time t
   *
   * Possible Results:
   * - returns |t| when
   *     the result fits within the time structure
   * - returns an unspecified value when
   *     the result does not fit within the time structure
   */
  OSAPI_EXPORT os_time
  os_timeAbs(
          os_time t);

  /** \brief Compare time t1 with time t2
   *
   * Possible Results:
   * - returns -1 when
   *     value t1 < value t2
   * - returns 1 when
   *     value t1 > value t2
   * - returns 0 when
   *     value t1 = value t2
   */
  OSAPI_EXPORT int
  os_timeCompare(
          os_time t1,
          os_time t2);

  /** \brief Convert time t into a floating point representation of t
   *
   * Postcondition:
   * - Due to the limited accuracy, the least significant part
   *   of the floating point time will be about 1 us.
   *
   * Possible Results:
   * - returns floating point representation of t
   */
  OSAPI_EXPORT os_timeReal
  os_timeToReal(
          os_time t);

  /** \brief Convert a floating point time representation into time
   *
   * Possible Results:
   * - returns t in os_time representation
   */
  OSAPI_EXPORT os_time
  os_realToTime(
          os_timeReal t);

  /** \brief Suspend the execution of the calling thread for the specified time
   *
   * Possible Results:
   * - returns os_resultSuccess if
   *     the thread is suspended for the specified time
   * - returns os_resultFail if
   *     the thread is not suspended for the specified time because of a failure,
   *     for example when a negative delay is supplied or when the ns-part is not
   *     normalized.
   */
  OSAPI_EXPORT os_result
  os_nanoSleep(
          _In_ os_time delay);

  /** \brief Translate calendar time into a readable string representation
   *
   * It converts the calendar time t into a '\0'-terminated string of the form:
   *
   *        "Mon Feb 03 14:28:56 2014"
   *
   * The time-zone information may not be included on all platforms and may be a
   * non-abbreviated string too. In order to obtain the time-zone, more room (at
   * least 4 bytes more for an abbreviated, and unknown more for spelled out) needs
   * to be provided in buf than the minimum of OS_CTIME_R_BUFSIZE. On Windows (if
   * enough room is provided) it may for example expand to:
   *
   *        "Wed Oct 01 15:59:53 W. Europe Daylight Time 2014"
   *
   * And on Linux to:
   *
   *        "Wed Oct 01 15:59:53 CEST 2014"
   *
   * \param t the time to be translated
   * \param buf a buffer to which the string must be written, with room for holding
   *            at least OS_CTIME_R_BUFSIZE (26) characters.
   *
   * Possible Results:
   * If buf != NULL, buf contains a readable string representation of time t. The
   * string is '\0'-terminated (and doesn't include a '\n' as the last character).
   * \return The number of bytes written (not including the terminating \0) to buf
   * and 0 if buf was NULL.
   */
  OSAPI_EXPORT size_t
  os_ctime_r(
          os_time *t,
          char *buf,
          size_t bufsz);

  /** Minimum capacity of buffer supplied to os_ctime_r
   *
   * Apparently, the French national representation of the abbreviated weekday
   * name and month name is 4 characters, so therefore added 2 to the expected
   * size of 26.
   */
#define OS_CTIME_R_BUFSIZE (28)

  typedef struct os_timePowerEvents_s {
    uint32_t   suspendCount;           /**< The number of detected suspends. */
    os_time     suspendLastDetected;    /**< The time of the last detected suspend (real time). */
    os_atomic_uint32_t resumeCount;            /**< The number of detected resumes. */
    os_time     resumeLastDetected;     /**< The time of the last detected resume (real time). */
  } os_timePowerEvents;

  /** \brief Query (and optionally synchronize) on the number of detected power events.
   *
   * This call can be used to retrieve the number of power events (suspend/resume) that were
   * detected. It is possible to block on changes by specifying a maxBlockingTime.
   *
   * The lastDetected timestamps are retrieved with os_getTime() and are the times on which the
   * event was detected (which may not be the exact time at which the events actually occurred).
   * The reported counts are monotonically increased on detection of power events.
   *
   * There is no guarantee that suspends (either hibernate or sleep) are detected. In general not
   * all events may be detectable. Only the last resume event is guaranteed to be detected.
   *
   * The initial state (when no events are detected yet) is all counts and times zeroed.
   *
   * \param [in,out] events       Pointer to a struct in which the result of the call is returned.
   *                              If maxBlockingTime == 0, events is an out-parameter, otherwise it
   *                              is in/out. The call will block until the actual state is different
   *                              from the state pointed to by events.
   * \param [in] maxBlockingTime  The maximum time the call may block for the state to change from
   *                              the state specified in events. If 0, the call will not block and
   *                              return immediately the current state.
   * \retval os_resultSuccess     When the call succeeded and the struct pointed to by events contains
   *                              the new status.
   * \retval os_resultTimeout     Iff maxBlockingTime != 0 and maxBlockingTime elapsed before the state
   *                              changed.
   *
   * \pre     The parameter events is not NULL and points to a location sufficiently large to hold an
   *          os_powerEvents struct.
   * \pre     The parameter maxBlockingTime is a valid time representation.
   * \post    The struct pointed to by events contains the current values.
   */
  OSAPI_EXPORT os_result
  os_timeGetPowerEvents(
          os_timePowerEvents *events,
          os_time maxBlockingTime);

#if defined (__cplusplus)
}
#endif

#endif /* OS_TIME_H */
