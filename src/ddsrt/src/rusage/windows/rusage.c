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
#include <stdint.h>
#include <string.h>

#include "dds/ddsrt/rusage.h"

#include <psapi.h>

static dds_time_t
filetime_to_time (const FILETIME *ft)
{
    /* FILETIME structures express times in 100-nanosecond time units. */
    return (dds_time_t) ((((uint64_t) ft->dwHighDateTime << 32) + (ft->dwLowDateTime)) * 100);
}

dds_return_t
ddsrt_getrusage_anythread (ddsrt_thread_list_id_t tid, ddsrt_rusage_t * __restrict usage)
{
    FILETIME stime, utime, ctime, etime;

    assert (usage != NULL);

    /* Memory counters are per process, but populate them if thread resource
       usage is requested to keep in sync with Linux. */
    if (!GetThreadTimes (tid, &ctime, &etime, &stime, &utime))
      return DDS_RETCODE_ERROR;

    memset (usage, 0, sizeof (*usage));
    usage->stime = filetime_to_time (&stime);
    usage->utime = filetime_to_time (&utime);
    return DDS_RETCODE_OK;
}

dds_return_t
ddsrt_getrusage (enum ddsrt_getrusage_who who, ddsrt_rusage_t *usage)
{
    PROCESS_MEMORY_COUNTERS pmctrs;

    assert (who == DDSRT_RUSAGE_SELF || who == DDSRT_RUSAGE_THREAD);
    assert (usage != NULL);

    /* Memory counters are per process, but populate them if thread resource
       usage is requested to keep in sync with Linux. */
    if (!GetProcessMemoryInfo (GetCurrentProcess (), &pmctrs, sizeof (pmctrs)))
      return DDS_RETCODE_ERROR;

    if (who == DDSRT_RUSAGE_THREAD)
    {
      dds_return_t rc;
      if ((rc = ddsrt_getrusage_anythread (GetCurrentThread (), usage)) < 0)
        return rc;
    }
    else
    {
      FILETIME stime, utime, ctime, etime;
      if (!GetProcessTimes (GetCurrentProcess (), &ctime, &etime, &stime, &utime))
        return DDS_RETCODE_ERROR;
      memset(usage, 0, sizeof(*usage));
      usage->stime = filetime_to_time(&stime);
      usage->utime = filetime_to_time(&utime);
    }

    usage->maxrss = pmctrs.PeakWorkingSetSize;
    return DDS_RETCODE_OK;
}

