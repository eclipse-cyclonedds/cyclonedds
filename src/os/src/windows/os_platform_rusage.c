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
#include <stdint.h>

#include "os/os.h"

#include <psapi.h>

static void
filetime_to_time(_In_ const FILETIME *ft, _Out_ os_time *t)
{
    /* FILETIME structures express times in 100-nanosecond time units. */
    uint64_t ns = ((ft->dwHighDateTime << 31) + (ft->dwLowDateTime));
    t->tv_sec = (os_timeSec)(ns / (1000 * 1000 * 10));
    t->tv_nsec = (int32_t)(ns % (1000 * 1000 * 10)) * 100;
}

_Pre_satisfies_((who == OS_RUSAGE_SELF) || \
                (who == OS_RUSAGE_THREAD))
_Success_(return == 0)
int os_getrusage(_In_ int who, _Out_ os_rusage_t *usage)
{
    FILETIME stime, utime, ctime, etime;
    PROCESS_MEMORY_COUNTERS pmctrs;

    assert(who == OS_RUSAGE_SELF || who == OS_RUSAGE_THREAD);
    assert(usage != NULL);

    /* Memory counters are per process, but populate them if thread resource
       usage is requested to keep in sync with Linux. */
    if ((!GetProcessMemoryInfo(GetCurrentProcess(), &pmctrs, sizeof(pmctrs)))
     || (who == OS_RUSAGE_SELF &&
         !GetProcessTimes(GetCurrentProcess(), &ctime, &etime, &stime, &utime))
     || (who == OS_RUSAGE_THREAD &&
         !GetThreadTimes(GetCurrentThread(), &ctime, &etime, &stime, &utime)))
    {
        return GetLastError();
    }

    memset(usage, 0, sizeof(*usage));
    filetime_to_time(&stime, &usage->stime);
    filetime_to_time(&utime, &usage->utime);
    usage->maxrss = pmctrs.PeakWorkingSetSize;

    return 0;
}
