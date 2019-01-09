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
/* Make sure we get the XSI compliant version of strerror_r */
#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE
#undef _GNU_SOURCE
#define _POSIX_C_SOURCE 200112L

#include <string.h>
#include <assert.h>

#include "os/os.h"

int
os_getErrno (void)
{
    return errno;
}

void
os_setErrno (int err)
{
    errno = err;
}

int
os_strerror_r (int errnum, char *buf, size_t buflen)
{
    int err;

    assert(buf != NULL);
    assert(buflen > 0);

    if ((err = os_errstr(errnum, buf, buflen)) == EINVAL) {
        buf[0] = '\0'; /* null-terminate in case nothing is written */
        err = strerror_r(errnum, buf, buflen);
        buf[buflen - 1] = '\0'; /* always null-terminate, just to be safe */
    }

    return err;
}
