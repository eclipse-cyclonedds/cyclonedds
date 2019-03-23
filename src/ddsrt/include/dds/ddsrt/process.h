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
#ifndef DDSRT_PROCESS_H
#define DDSRT_PROCESS_H

#include "dds/export.h"
#include "dds/ddsrt/types.h"

#if defined (__cplusplus)
extern "C" {
#endif

#if defined(_WIN32)
typedef DWORD ddsrt_pid_t;
#define PRIdPID "u"
#else /* _WIN32 */
#include <unistd.h>
#if defined(_WRS_KERNEL)
typedef RTP_ID ddsrt_pid_t; /* typedef struct wind_rtp *RTP_ID */
#define PRIdPID PRIuPTR
#else
typedef pid_t ddsrt_pid_t;
#define PRIdPID "d"
#endif
#endif /* _WIN32 */

/**
 * @brief Return process ID (PID) of the calling process.
 *
 * @returns The process ID of the calling process.
 */
DDS_EXPORT ddsrt_pid_t
ddsrt_getpid(void);

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_PROCESS_H */
