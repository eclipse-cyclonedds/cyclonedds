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
#ifndef OS_PLATFORM_STDLIB_H
#define OS_PLATFORM_STDLIB_H

#include <sys/stat.h>
#include <io.h>

#if defined (__cplusplus)
extern "C" {
#endif

#define OS_ROK (_S_IREAD)
#define OS_WOK (_S_IWRITE)
#define OS_XOK (_S_IEXEC)
#define OS_FOK (0)

#define OS_ISDIR(mode) (mode & _S_IFDIR)
#define OS_ISREG(mode) (mode & _S_IFREG)
#define OS_ISLNK(mode) (0) /* not supported on this platform */

    /* on this platform these permission masks are don't cares! */
#define S_IRWXU 00700
#define S_IRWXG 00070
#define S_IRWXO 00007

#define MAXHOSTNAMELEN MAX_HOSTNAME_LEN

#if _MSC_VER < 1900
extern int snprintf(char *s, size_t n, const char *format, ...);
#endif

#if defined (__cplusplus)
}
#endif

#endif /* OS_PLATFORM_STDLIB_H */
