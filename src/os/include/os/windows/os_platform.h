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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <VersionHelpers.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#define PRIdSIZE "zd"
#define PRIuSIZE "zu"
#define PRIxSIZE "zx"

#if defined (__cplusplus)
extern "C" {
#endif

#define OS_WIN32 1
#define OS_SOCKET_USE_FCNTL 0
#define OS_SOCKET_USE_IOCTL 1
#define OS_FILESEPCHAR '/'
#define OS_HAS_NO_SET_NAME_PRCTL 1
#define OS_HAS_UCONTEXT_T 0

#ifdef __BIG_ENDIAN
#define OS_ENDIANNESS OS_BIG_ENDIAN
#else
#define OS_ENDIANNESS OS_LITTLE_ENDIAN
#endif

#ifdef _WIN64
#define OS_64BIT
#endif

    typedef double os_timeReal;
    typedef int os_timeSec;
    typedef DWORD os_procId;
    #define PRIprocId "u"
        /* keep in sync with st_mode field def in struct stat in sys/stat.h */
        typedef unsigned short os_mode_t;
        typedef SSIZE_T ssize_t;

#include "os/windows/os_platform_socket.h"
#include "os/windows/os_platform_sync.h"
#include "os/windows/os_platform_process.h"
#include "os/windows/os_platform_thread.h"
#include "os/windows/os_platform_stdlib.h"
#include "os/windows/os_platform_time.h"

#if defined (__cplusplus)
}
#endif

#endif
