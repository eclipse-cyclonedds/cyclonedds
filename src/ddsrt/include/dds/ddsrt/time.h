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

/**
 * @file
 * @brief DDS C Time support API
 *
 * This header file defines the public API of the in the
 * CycloneDDS C language binding.
 */
#ifndef DDS_TIME_H
#define DDS_TIME_H

#include <stdint.h>

#include "dds/export.h"
#include "dds/ddsrt/types.h"

#if defined (__cplusplus)
extern "C" {
#endif

/*
  Times are represented using a 64-bit signed integer, encoding
  nanoseconds since the epoch. Considering the nature of these
  systems, one would best use TAI, International Atomic Time, rather
  than something UTC, but availability may be limited.

  Valid times are non-negative and times up to 2**63-2 can be
  represented. 2**63-1 is defined to represent, essentially, "never".
  This is good enough for a couple of centuries.
*/

/** Absolute Time definition */
typedef int64_t dds_time_t;

/** Relative Time definition */
typedef int64_t dds_duration_t;

/** @name Macro definition for time units in nanoseconds.
  @{**/
#define DDS_NSECS_IN_SEC INT64_C(1000000000)
#define DDS_NSECS_IN_MSEC INT64_C(1000000)
#define DDS_NSECS_IN_USEC INT64_C(1000)
/** @}*/

/** @name Infinite timeout for indicate absolute time */
#define DDS_NEVER ((dds_time_t) INT64_MAX)

/** @name Infinite timeout for relative time */
#define DDS_INFINITY ((dds_duration_t) INT64_MAX)

/** @name Macro definition for time conversion to nanoseconds
  @{**/
#define DDS_SECS(n) ((n) * DDS_NSECS_IN_SEC)
#define DDS_MSECS(n) ((n) * DDS_NSECS_IN_MSEC)
#define DDS_USECS(n) ((n) * DDS_NSECS_IN_USEC)
/** @}*/

/**
 * @brief Get the current time in nanoseconds since the UNIX Epoch.
 *
 * @returns Current time.
 */
DDS_EXPORT dds_time_t dds_time(void);

/**
 * @brief Suspend execution of calling thread until relative time n elapsed.
 *
 * Execution is suspended for n nanoseconds. Should the call be interrupted,
 * the call is re-entered with the remaining time.
 *
 * @param[in]  reltime  Relative time in nanoseconds.
 */
DDS_EXPORT void dds_sleepfor (dds_duration_t reltime);

/**
 * @brief Suspend execution of calling thread until absolute time n elapsed.
 *
 * Execution is suspended until the given absolute time elapsed. Should the
 * call be interrupted, it is re-entered with the remaining time.
 *
 * @param[in]  abstime  Absolute time in nanoseconds since UNIX Epoch.
 */
DDS_EXPORT void dds_sleepuntil (dds_time_t abstime);

/**
 * @brief Get high resolution, monotonic time.
 *
 * The monotonic clock is a clock with near real-time progression and can be
 * used when a high-resolution time is needed without the need for it to be
 * related to the wall-clock. The resolution of the clock is typically the
 * highest available on the platform.
 *
 * The clock is not guaranteed to be strictly monotonic, but on most common
 * platforms it will be (based on performance-counters or HPET's).
 *
 * @returns Monotonic time if available, otherwise real time.
 */
DDS_EXPORT dds_time_t ddsrt_time_monotonic(void);

/**
 * @brief Get high resolution, elapsed (and thus monotonic) time since some
 * fixed unspecified past time.
 *
 * The elapsed time clock is a clock with near real-time progression and can be
 * used when a high-resolution suspend-aware monotonic clock is needed, without
 * having to deal with the complications of discontinuities if for example the
 * time is changed. The fixed point from which the elapsed time is returned is
 * not guaranteed to be fixed over reboots of the system.
 *
 * @returns Elapsed time if available, otherwise return monotonic time.
 */
DDS_EXPORT dds_time_t ddsrt_time_elapsed(void);

/**
 * @brief Convert time into a human readable string in RFC 3339 format.
 *
 * Converts the calender time into a null-terminated string in RFC 3339 format.
 * e.g. "2014-10-24 15:32:27-04:00".
 *
 * UTC offset is omitted if time-zone information is unknown.
 *
 * @param[in]  abstime  Time in nanoseconds since UNIX Epoch.
 * @param[in]  str      String to write human readable timestamp to.
 * @param[in]  size     Number of bytes available in @str.
 *
 * @returns Number of bytes written (excluding terminating null byte). The
 *          string is truncated if str is not sufficiently large enough. Thus,
 *          a return value of size or more means the output was truncated.
 */
#define DDSRT_RFC3339STRLEN (25)

DDS_EXPORT size_t ddsrt_ctime(dds_time_t abstime, char *str, size_t size);

#if defined(__cplusplus)
}
#endif

#if DDSRT_WITH_FREERTOS
#include "dds/ddsrt/time/freertos.h"
#endif

#endif /* DDSRT_TIME_H */
