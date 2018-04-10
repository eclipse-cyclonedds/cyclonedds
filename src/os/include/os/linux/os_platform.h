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
#define OS_SOCKET_USE_FCNTL 1
#define OS_SOCKET_USE_IOCTL 0
#define OS_HAS_UCONTEXT_T 1
#define OS_FILESEPCHAR '/'
#define OS_HAS_NO_SET_NAME_PRCTL 1

#define OS_ENDIANNESS OS_LITTLE_ENDIAN

#ifdef _LP64
#define OS_64BIT
#endif

    typedef double os_timeReal;
    typedef int os_timeSec;
    typedef uid_t os_uid;
    typedef gid_t os_gid;
    typedef mode_t os_mode_t;
    typedef pid_t os_procId;
    #define PRIprocId "d"

#include "os/posix/os_platform_socket.h"
#include "os/posix/os_platform_sync.h"
#include "os/posix/os_platform_thread.h"
#include "os/posix/os_platform_stdlib.h"
#include "os/posix/os_platform_process.h"

#if defined (__cplusplus)
}
#endif

#endif
