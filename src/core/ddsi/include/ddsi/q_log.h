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

#include "os/os.h"
#include "ddsi/q_time.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* LOG_THREAD_CPUTIME must be considered private. */
#define LOG_THREAD_CPUTIME(guard)                                     \
    do {                                                              \
        if (dds_get_log_mask() & DDS_LC_TIMING) {                     \
            nn_mtime_t tnowlt = now_mt();                             \
            if (tnowlt.v >= (guard).v) {                              \
                const char fmt[] = "thread_cputime %d.%09d\n";        \
                int64_t ts = get_thread_cputime ();                   \
                dds_log(                                              \
                    DDS_LC_TIMING, __FILE__, __LINE__, OS_FUNCTION,   \
                    fmt, (int)(ts / T_SECOND), (int)(ts % T_SECOND)); \
                (guard).v = tnowlt.v + T_SECOND;                      \
            }                                                         \
        }                                                             \
    } while (0)

#if defined (__cplusplus)
}
#endif

#endif /* NN_LOG_H */
