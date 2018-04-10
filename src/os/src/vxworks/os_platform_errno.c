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
#include <assert.h>
#include <limits.h>
#include <string.h>

#include "os/os.h"

int
os_getErrno(void)
{
    return errno;
}

void
os_setErrno(int err)
{
    errno = err;
}

#ifdef _WRS_KERNEL

extern char *
strerrorIf(
    int errcode);

/* VxWorks has a non-compliant strerror_r in kernel mode which only takes a
   buffer and an error number. Providing a buffer smaller than NAME_MAX + 1
   (256) may result in a buffer overflow. See target/src/libc/strerror.c for
   details. NAME_MAX is defined in limits.h. */

int
os_strerror_r(int err, char *str, size_t len)
{
    int res = EINVAL;

    assert(str != NULL);

    if (len < (NAME_MAX + 1)) {
        res = ERANGE;
    } else if (strerrorIf(err) != NULL) {
        (void)strerror_r(err, str);
    }

    return res;
}

#else

int
os_strerror_r(int err, char *str, size_t len)
{
    int res = 0;

    assert(str != NULL);

    /* VxWorks's strerror_r always returns 0 (zero), so the only way to decide
       if the error was truncated is to check if the last position in the
       buffer is overwritten by strerror_r. */
    str[len - 1] = 'x';

    (void)strerror_r (err, str, len);

    if (str[len - 1] != 'x') {
        res = ERANGE;
    }

    str[len - 1] = '\0'; /* always null terminate, just to be safe */

    return res;
}

#endif /* _WRS_KERNEL */
