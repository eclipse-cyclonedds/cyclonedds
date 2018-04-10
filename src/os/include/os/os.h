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
#ifndef OS_H
#define OS_H

#include "os/osapi_export.h"
#include "os_public.h"

#if __linux__ == 1
#include "linux/os_platform.h"
#elif defined(__VXWORKS__)
#include "vxworks/os_platform.h"
#elif __sun == 1
#include "solaris/os_platform.h"
#elif defined(__INTEGRITY)
#include "integrity/os_platform.h"
#elif  __PikeOS__ == 1
#include "pikeos3/os_platform.h"
#elif defined(__QNX__)
#include "qnx/os_platform.h"
#elif defined(_MSC_VER)
#ifdef _WIN32_WCE
#include "wince/os_platform.h"
#else
#include "windows/os_platform.h"
#endif
#elif defined __APPLE__
#include "darwin/os_platform.h"
#elif defined __CYGWIN__
#include "cygwin/os_platform.h"
#else
#error "Platform missing from os.h list"
#endif

#include "os_defs.h"
#include "os_thread.h"
#include "os_sync.h"
#include "os_time.h"
#include "os_atomics.h"
#include "os_socket.h"
#include "os_heap.h"
#include "os_stdlib.h"
#include "os_report.h"
#include "os_init.h"
#include "os_process.h"
#include "os_errno.h"
#include "os_iter.h"


#define OSPL_VERSION_STR "aap"
#define OSPL_HOST_STR "noot"
#define OSPL_TARGET_STR "mies"
#define OSPL_INNER_REV_STR "wim"
#define OSPL_OUTER_REV_STR "zus"

#endif
