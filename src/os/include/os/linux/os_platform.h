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
#ifndef OS_PLATFORM_H
#define OS_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <endian.h>
#include <sys/stat.h>
#include <unistd.h>

#define PRIdSIZE "zd"
#define PRIuSIZE "zu"
#define PRIxSIZE "zx"

#if defined (__cplusplus)
extern "C" {
#endif

#define OS_LINUX 1
#define OS_HAVE_GETRUSAGE 1

    typedef double os_timeReal;
    typedef int os_timeSec;
    typedef pid_t os_procId;
    #define PRIprocId "d"

#include "os/posix/os_platform_socket.h"
#include "os/posix/os_platform_sync.h"
#include "os/posix/os_platform_thread.h"
#include "os/posix/os_platform_stdlib.h"

#if defined (__cplusplus)
}
#endif

#endif
