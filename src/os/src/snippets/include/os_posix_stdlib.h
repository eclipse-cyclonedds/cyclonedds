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
#ifndef OS_POSIX_STDLIB_H
#define OS_POSIX_STDLIB_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __VXWORKS__
void os_stdlibInitialize(void);
#endif

#ifdef OSPL_VERSION
 #ifdef PIKEOS_POSIX
  #include <lwip/netdb.h>
 #else
  #include <netdb.h>
 #endif
#endif

#if defined (__cplusplus)
extern "C" {
#endif

#define OS_OS_FILESEPCHAR '/'
#define OS_OS_PATHSEPCHAR ':'
#define OS_OS_EXESUFFIX   ""
#define OS_OS_BATSUFFIX   ""
#define OS_OS_LIB_LOAD_PATH_VAR "LD_LIBRARY_PATH"

#define OS_ROK	R_OK
#define OS_WOK	W_OK
#define OS_XOK	X_OK
#define OS_FOK	F_OK

#define OS_ISDIR S_ISDIR
#define OS_ISREG S_ISREG
#define OS_ISLNK S_ISLNK

#define OS_PATH_MAX _POSIX_PATH_MAX

typedef DIR *os_os_dirHandle;

#if defined (__cplusplus)
}
#endif

#endif /* OS_POSIX_STDLIB_H */
