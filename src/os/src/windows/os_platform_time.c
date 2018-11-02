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
/** \file os/mingw3.2.0/code/os_time.c
 *  \brief WIN32 time management
 *
 * Implements time management for WIN32 by
 * including the common services
 * and implementing WIN32 specific
 * os_timeGet and os_nanoSleep
 */
#include <sys/timeb.h>
#include <time.h>

#include "os/os.h"

#include <assert.h>

/* GetSystemTimePreciseAsFileTime was introduced with Windows 8, so
   starting from _WIN32_WINNET = 0x0602.  When building for an older
   version we can still check dynamically. */

#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0602
#define UseGetSystemTimePreciseAsFileTime
#else
static VOID (WINAPI *GetSystemTimeAsFileTimeFunc) (_Out_ LPFILTETIME) = GetSystemTimeAsFileTime;
static HANDLE Kernel32ModuleHandle;
#endif

/* GetSystemTimeAsFileTime returns the number of 100ns intervals that have elapsed
 * since January 1, 1601 (UTC). There are 11,644,473,600 seconds between 1601 and
 * the Unix epoch (January 1, 1970 (UTC)), which is the reference that is used for
 * os_time. */
#define OS_TIME_FILETIME_UNIXEPOCH_OFFSET_SECS (11644473600)

os_time
os__timeDefaultTimeGet(void)
{
    FILETIME ft;
    ULARGE_INTEGER ns100;
    os_time current_time;

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
     * GetSystemTimeAdjustment. See for example OSPL-4394.
     */
#ifdef UseGetSystemTimePreciseAsFileTime
    GetSystemTimePreciseAsFileTime(&ft);
#else
    GetSystemTimeAsFileTimeFunc(&ft);
#endif
    ns100.LowPart = ft.dwLowDateTime;
    ns100.HighPart = ft.dwHighDateTime;
    current_time.tv_sec = (os_timeSec)((ns100.QuadPart / 10000000) - OS_TIME_FILETIME_UNIXEPOCH_OFFSET_SECS);
    current_time.tv_nsec = (int32_t)((ns100.QuadPart % 10000000) * 100);

    return current_time;
}

void
os_timeModuleInit(void)
{
#ifndef UseGetSystemTimePreciseAsFileTime
    /* Resolve the time-functions from the Kernel32-library. */
    VOID (WINAPI *f) (_Out_ LPFILTETIME);

    /* This os_timeModuleInit is currently called from DllMain. This means
     * we're not allowed to do LoadLibrary. One exception is "Kernel32.DLL",
     * since that library is per definition loaded (LoadLibrary itself
     * lives there). And since we're only resolving stuff, not actually
     * invoking, this is considered safe. */
    Kernel32ModuleHandle = LoadLibrary("Kernel32.DLL");
    assert(Kernel32ModuleHandle);

    f = GetProcAddress(Kernel32ModuleHandle, "GetSystemTimePreciseAsFileTime");
    if (f != 0) {
        GetSystemTimeAsFileTimeFunc = f;
    }
#endif
}

void
os_timeModuleExit(void)
{
#ifndef UseGetSystemTimePreciseAsFileTime
    if (Kernel32ModuleHandle) {
        GetSystemTimeAsFileTimeFunc = GetSystemTimeAsFileTime;
        FreeLibrary(Kernel32ModuleHandle);
        Kernel32ModuleHandle = NULL;
    }
#endif
}

/** \brief Suspend the execution of the calling thread for the specified time
 *
 * \b os_nanoSleep suspends the calling thread for the required
 * time by calling \b nanosleep. First it converts the \b delay in
 * \b os_time definition into a time in \b struct \b timeval definition.
 * In case the \b nanosleep is interrupted, the call is re-enterred with
 * the remaining time.
 */
os_result
os_nanoSleep (
    _In_ os_time delay)
{
    os_result result = os_resultSuccess;
    DWORD dt;

    assert (delay.tv_nsec >= 0);
    assert (delay.tv_nsec < 1000000000);

    if (delay.tv_sec >= 0 ) {
        dt = (DWORD)delay.tv_sec * 1000 + delay.tv_nsec / 1000000;
        Sleep(dt);
    } else {
        /* Negative time-interval should return illegal param error */
        result = os_resultFail;
    }

    return result;
}

/** \brief Get high resolution, monotonic time.
 *
 */
os_time
os_timeGetMonotonic(void)
{
    os_time current_time;
    ULONGLONG ubit;

    (void) QueryUnbiasedInterruptTime(&ubit); /* 100ns ticks */

    current_time.tv_sec = (os_timeSec)(ubit / 10000000);
    current_time.tv_nsec = (int32_t)((ubit % 10000000) * 100);

    return current_time;
}

/** \brief Get high resolution, elapsed time.
 *
 */
os_time
os_timeGetElapsed(void)
{
    os_time current_time;
    LARGE_INTEGER qpc;
    static LONGLONG qpc_freq;

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
     * clock and the drift tendency should be reported. See for example
     * OSPL-4394. */

    if (qpc_freq == 0){
        /* This block is idempotent, so don't bother with synchronisation */
        LARGE_INTEGER frequency;

        if(QueryPerformanceFrequency(&frequency)){
            qpc_freq = frequency.QuadPart;
        }
        /* Since Windows XP SP2 the QueryPerformanceCounter is abstracted,
         * so QueryPerformanceFrequency is not expected to ever return 0.
         * That't why there is no fall-back for the case when no
         * QueryPerformanceCounter is available. */
    }
    assert(qpc_freq);

    /* The QueryPerformanceCounter tends to drift quite a bit, so in order to
     * accurately measure longer periods with it, there may be a need to sync
     * the time progression to actual time progression (with a PLL for example
     * as done by EB for CAE). */
    QueryPerformanceCounter(&qpc);
    current_time.tv_sec = (os_timeSec)(qpc.QuadPart / qpc_freq);
    current_time.tv_nsec = (int32_t)(((qpc.QuadPart % qpc_freq) * 1000000000) / qpc_freq);

    return current_time;
}
