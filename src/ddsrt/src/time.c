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

#include "cyclonedds/ddsrt/timeconv.h"
#include "cyclonedds/ddsrt/string.h"

extern inline dds_time_t
ddsrt_time_add_duration(dds_time_t abstime, dds_duration_t reltime);

#if !_WIN32 && !DDSRT_WITH_FREERTOS
#include <errno.h>

void dds_sleepfor(dds_duration_t n)
{
  struct timespec t, r;

  if (n >= 0) {
    t.tv_sec = n / DDS_NSECS_IN_SEC;
    t.tv_nsec = n % DDS_NSECS_IN_SEC;
    while (nanosleep(&t, &r) == -1 && errno == EINTR) {
      t = r;
    }
  }
}
#endif

void dds_sleepuntil(dds_time_t abstime)
{
  dds_time_t now = dds_time();

  if (abstime > now)
    dds_sleepfor (abstime - now);
}

size_t
ddsrt_ctime(dds_time_t n, char *str, size_t size)
{
  struct tm tm;
#if __SunOS_5_6
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
#if ! __SunOS_5_6
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

