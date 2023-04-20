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
#include <time.h>

#include "dds/ddsrt/time.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/static_assert.h"

DDS_EXPORT extern inline dds_time_t ddsrt_time_add_duration(dds_time_t abstime, dds_duration_t reltime);
DDS_EXPORT extern inline ddsrt_mtime_t ddsrt_mtime_add_duration(ddsrt_mtime_t abstime, dds_duration_t reltime);
DDS_EXPORT extern inline ddsrt_wctime_t ddsrt_wctime_add_duration(ddsrt_wctime_t abstime, dds_duration_t reltime);
DDS_EXPORT extern inline ddsrt_etime_t ddsrt_etime_add_duration(ddsrt_etime_t abstime, dds_duration_t reltime);

#if !_WIN32 && !DDSRT_WITH_FREERTOS
#include <errno.h>

void dds_sleepfor(dds_duration_t n)
{
  struct timespec t, r;

  if (n >= 0) {
    t.tv_sec = (time_t) (n / DDS_NSECS_IN_SEC);
    t.tv_nsec = (long) (n % DDS_NSECS_IN_SEC);
    while (nanosleep(&t, &r) == -1 && errno == EINTR) {
      t = r;
    }
  }
}
#endif

size_t
ddsrt_ctime(dds_time_t n, char *str, size_t size)
{
  struct tm tm;
#if __SunOS_5_6 || __MINGW32__
  /* Solaris 2.6 doesn't recognize %z so we just leave it out */
  static const char fmt[] = "%Y-%m-%d %H:%M:%S";
#else
  static const char fmt[] = "%Y-%m-%d %H:%M:%S%z";
#endif
  char buf[] = "YYYY-mm-dd HH:MM:SS.hh:mm"; /* RFC 3339 */
  size_t cnt;
  time_t sec = (time_t)(n / DDS_NSECS_IN_SEC);

  assert(str != NULL);

#if _WIN32
  (void)localtime_s(&tm, &sec);
#else
  (void)localtime_r(&sec, &tm);
#endif /* _WIN32 */

  cnt = strftime(buf, sizeof(buf), fmt, &tm);
#if ! __SunOS_5_6 && ! __MINGW32__
  /* %z is without a separator between hours and minutes, fixup */
  assert(cnt == (sizeof(buf) - 2 /* ':' + '\0' */));
  buf[sizeof(buf) - 1] = '\0';
  buf[sizeof(buf) - 2] = buf[sizeof(buf) - 3];
  buf[sizeof(buf) - 3] = buf[sizeof(buf) - 4];
  buf[sizeof(buf) - 4] = ':';
#endif
  (void)cnt;

  return ddsrt_strlcpy(str, buf, size);
}

static void time_to_sec_usec (int32_t * __restrict sec, int32_t * __restrict usec, int64_t t)
{
  *sec = (int32_t) (t / DDS_NSECS_IN_SEC);
  *usec = (int32_t) (t % DDS_NSECS_IN_SEC) / 1000;
}

void ddsrt_mtime_to_sec_usec (int32_t * __restrict sec, int32_t * __restrict usec, ddsrt_mtime_t t)
{
  time_to_sec_usec (sec, usec, t.v);
}

void ddsrt_wctime_to_sec_usec (int32_t * __restrict sec, int32_t * __restrict usec, ddsrt_wctime_t t)
{
  time_to_sec_usec (sec, usec, t.v);
}

void ddsrt_etime_to_sec_usec (int32_t * __restrict sec, int32_t * __restrict usec, ddsrt_etime_t t)
{
  time_to_sec_usec (sec, usec, t.v);
}
