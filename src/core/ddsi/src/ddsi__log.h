// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__LOG_H
#define DDSI__LOG_H

#include <stdarg.h>

#include "dds/ddsrt/log.h"
#include "dds/ddsrt/rusage.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsi/ddsi_log.h"

#if defined (__cplusplus)
extern "C" {
#endif

#if DDSRT_HAVE_RUSAGE
#define LOG_THREAD_CPUTIME(logcfg, guard)                                \
    do {                                                                 \
        if ((logcfg)->c.mask & DDS_LC_TIMING) {                          \
            ddsrt_mtime_t tnowlt = ddsrt_time_monotonic ();              \
            if (tnowlt.v >= (guard).v) {                                 \
                ddsrt_rusage_t usage;                                    \
                if (ddsrt_getrusage(DDSRT_RUSAGE_THREAD, &usage) == 0) { \
                    DDS_CLOG(                                            \
                        DDS_LC_TIMING,                                   \
                        (logcfg),                                        \
                        "thread_cputime %d.%09d\n",                      \
                        (int)(usage.stime / DDS_NSECS_IN_SEC),           \
                        (int)(usage.stime % DDS_NSECS_IN_SEC));          \
                    (guard).v = tnowlt.v + DDS_NSECS_IN_SEC;             \
                }                                                        \
            }                                                            \
        }                                                                \
    } while (0)
#else
#define LOG_THREAD_CPUTIME(logcfg, guard) (void)(guard)
#endif /* DDSRT_HAVE_RUSAGE */

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__LOG_H */
