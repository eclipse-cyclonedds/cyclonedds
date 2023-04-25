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
#include <sys/timeb.h>
#include <time.h>

#include "dds/ddsrt/time.h"
#include "dds/ddsrt/misc.h"

extern inline DWORD
ddsrt_duration_to_msecs_ceil(dds_duration_t reltime);

/* GetSystemTimePreciseAsFileTime was introduced with Windows 8, so
   starting from _WIN32_WINNET = 0x0602.  When building for an older
   version we can still check dynamically. */
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0602
#define UseGetSystemTimePreciseAsFileTime
#else
typedef void (WINAPI *GetSystemTimeAsFileTimeFunc_t)(LPFILETIME);
static GetSystemTimeAsFileTimeFunc_t GetSystemTimeAsFileTimeFunc = GetSystemTimeAsFileTime;
static HANDLE Kernel32ModuleHandle;
#endif

/* GetSystemTimeAsFileTime returns the number of 100ns intervals that have elapsed
 * since January 1, 1601 (UTC). There are 11,644,473,600 seconds between 1601 and
 * the Unix epoch (January 1, 1970 (UTC)), which is the reference that is used for
 * dds_time.
 */
#define FILETIME_EPOCH_OFFSET_SECS (11644473600)
#define SECS_IN_100_NSECS (1000 * 1000 * 10)

dds_time_t dds_time(void)
{
  FILETIME ft;
  ULARGE_INTEGER ns100;

  /* GetSystemTime(Precise)AsFileTime returns the number of 100-nanosecond
   * intervals since January 1, 1601 (UTC).
   * GetSystemTimeAsFileTime has a resolution of approximately the
   * TimerResolution (~15.6ms) on Windows XP. On Windows 7 it appears to have
   * sub-millisecond resolution. GetSystemTimePreciseAsFileTime (available on
   * Windows 8) has sub-microsecond resolution.
   *
   * This call appears to be significantly (factor 8) cheaper than the
   * QueryPerformanceCounter (on the systems performance was measured on).
   *
   * TODO: When the API is extended to support retrieval of clock-properties,
   * then the actual resolution of this clock can be retrieved using the
   * GetSystemTimeAdjustment.
   */
#ifdef UseGetSystemTimePreciseAsFileTime
  GetSystemTimePreciseAsFileTime(&ft);
#else
  GetSystemTimeAsFileTimeFunc(&ft);
#endif
  ns100.LowPart = ft.dwLowDateTime;
  ns100.HighPart = ft.dwHighDateTime;
  ns100.QuadPart -= (FILETIME_EPOCH_OFFSET_SECS * SECS_IN_100_NSECS);

  return (dds_time_t)(ns100.QuadPart * 100);
}

DDSRT_WARNING_GNUC_OFF(missing-prototypes)
DDSRT_WARNING_CLANG_OFF(missing-prototypes)
void ddsrt_time_init(void)
{
#ifndef UseGetSystemTimePreciseAsFileTime
  /* Resolve the time-functions from the Kernel32-library. */
  GetSystemTimeAsFileTimeFunc_t f;

  /* This os_timeModuleInit is currently called from DllMain. This means
   * we're not allowed to do LoadLibrary. One exception is "Kernel32.DLL",
   * since that library is per definition loaded (LoadLibrary itself
   * lives there). And since we're only resolving stuff, not actually
   * invoking, this is considered safe.
   */
  Kernel32ModuleHandle = LoadLibrary("Kernel32.DLL");
  assert(Kernel32ModuleHandle);

  DDSRT_WARNING_GNUC_OFF(cast-function-type)
  f = (GetSystemTimeAsFileTimeFunc_t)GetProcAddress(Kernel32ModuleHandle, "GetSystemTimePreciseAsFileTime");
  DDSRT_WARNING_GNUC_ON(cast-function-type)
  if (f != 0) {
    GetSystemTimeAsFileTimeFunc = f;
  }
#endif
}

void ddsrt_time_fini(void)
{
#ifndef UseGetSystemTimePreciseAsFileTime
  if (Kernel32ModuleHandle) {
    GetSystemTimeAsFileTimeFunc = GetSystemTimeAsFileTime;
    FreeLibrary(Kernel32ModuleHandle);
    Kernel32ModuleHandle = NULL;
  }
#endif
}
DDSRT_WARNING_GNUC_ON(missing-prototypes)
DDSRT_WARNING_CLANG_ON(missing-prototypes)

ddsrt_wctime_t ddsrt_time_wallclock(void)
{
  return (ddsrt_wctime_t) { dds_time() } ;
}

ddsrt_mtime_t ddsrt_time_monotonic(void)
{
  ULONGLONG ubit;

  (void)QueryUnbiasedInterruptTime(&ubit); /* 100ns ticks */

  return (ddsrt_mtime_t) { (dds_time_t)ubit * 100 };
}

ddsrt_etime_t ddsrt_time_elapsed(void)
{
  LARGE_INTEGER qpc;
  static LONGLONG qpc_freq; /* Counts per nanosecond. */

  /* The QueryPerformanceCounter has a bad reputation, since it didn't behave
   * very well on older hardware. On recent OS's (Windows XP SP2 and later)
   * things have become much better, especially when combined with new hard-
   * ware.
   *
   * There is currently one bug which is not fixed, which may cause forward
   * jumps. This is currently not really important, since a forward jump may
   * be observed anyway due to the system going to standby. There is a work-
   * around available (comparing the progress with the progress made by
   * GetTickCount), but for now we live with a risk of a forward jump on buggy
   * hardware. Since Microsoft does maintain a list of hardware which exhibits
   * the behaviour, it is possible to only have the workaround in place only
   * on the faulty hardware (see KB274323 for a list and more info).
   *
   * TODO: When the API is extended to support retrieval of clock-properties,
   * then the discontinuous nature (when sleeping/hibernating) of this
   * clock and the drift tendency should be reported. */

  if (qpc_freq == 0){
    /* This block is idempotent, so don't bother with synchronisation. */
    LARGE_INTEGER frequency;

    if (QueryPerformanceFrequency(&frequency)) {
      /* Performance-counter frequency is in counts per second. */
      qpc_freq = DDS_NSECS_IN_SEC / frequency.QuadPart;
    }
    /* Since Windows XP SP2 the QueryPerformanceCounter is abstracted,
     * so QueryPerformanceFrequency is not expected to ever return 0.
     * That't why there is no fall-back for the case when no
     * QueryPerformanceCounter is available. */
  }
  assert(qpc_freq);

  /* The QueryPerformanceCounter tends to drift quite a bit, so in order to
   * accurately measure longer periods with it, there may be a need to sync
   * the time progression to actual time progression. */
  QueryPerformanceCounter(&qpc);

  return (ddsrt_etime_t) { qpc.QuadPart * qpc_freq };
}

void dds_sleepfor(dds_duration_t reltime)
{
  Sleep(ddsrt_duration_to_msecs_ceil(reltime));
}
