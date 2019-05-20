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
#ifndef NN_LOG_H
#define NN_LOG_H

#include <stdarg.h>

#include "dds/ddsrt/log.h"
#include "dds/ddsi/q_time.h"
#include "dds/ddsrt/rusage.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* LOG_THREAD_CPUTIME must be considered private. */
#if DDSRT_HAVE_RUSAGE
#define LOG_THREAD_CPUTIME(guard)                                        \
    do {                                                                 \
        if (dds_get_log_mask() & DDS_LC_TIMING) {                        \
            nn_mtime_t tnowlt = now_mt();                                \
            if (tnowlt.v >= (guard).v) {                                 \
                ddsrt_rusage_t usage;                                    \
                if (ddsrt_getrusage(DDSRT_RUSAGE_THREAD, &usage) == 0) { \
                    DDS_LOG(                                             \
                        DDS_LC_TIMING,                                   \
                        "thread_cputime %d.%09d\n",                      \
                        (int)(usage.stime / DDS_NSECS_IN_SEC),           \
                        (int)(usage.stime % DDS_NSECS_IN_SEC));          \
                    (guard).v = tnowlt.v + T_SECOND;                     \
                }                                                        \
            }                                                            \
        }                                                                \
    } while (0)
#else
#define LOG_THREAD_CPUTIME(guard) (void)(guard)
#endif /* DDSRT_HAVE_RUSAGE */

#if defined (__cplusplus)
}
#endif

#endif /* NN_LOG_H */
