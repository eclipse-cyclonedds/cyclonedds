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
#include "os/os.h"

#include <assert.h>

/* WSAGetLastError, GetLastError and errno

Windows supports errno (The Microsoft c Run-Time Library for Windows CE
does so since version 15 (Visual Studio 2008)). Error codes set by the
Windows Sockets implementation, however, are NOT made available via the
errno variable.

WSAGetLastError used to be the thread-safe version of GetLastError, but
nowadays is just an an alias for GetLastError as intended by Microsoft:
http://www.sockets.com/winsock.htm#Deviation_ErrorCodes

There is no relationship between GetLastError and errno.
GetLastError returns the last error that occurred in a Windows API function
(for the current thread). errno contains the last error that occurred in the C
runtime library. Normally if a WinAPI call fails, e.g. CreateFile, GetLastError
should be used to retrieve the error. If a C runtime library function fails,
e.g. fopen, errno contains the error number.
*/

int
os_getErrno(void)
{
    DWORD err = GetLastError();
    if (err != 0) {
        errno = (int)err;
    }
    return errno;
}

void
os_setErrno(int err)
{
    SetLastError(err);
    errno = err;
}

int
os_strerror_r(
    _In_ int err,
    _Out_writes_z_(len) char *str,
    _In_ size_t len)
{
    int res = 0, errs[2];
    DWORD cnt;

    assert(str != NULL);
    assert(len > 0);

    str[0] = '\0'; /* null-terminate in case nothing is written */
    errs[0] = os_getErrno();
    cnt = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS |
        FORMAT_MESSAGE_MAX_WIDTH_MASK,
        NULL,
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)str,
        (DWORD)len,
        NULL);

    errs[1] = os_getErrno();
    if (cnt == 0) {
        if (errs[1] == ERROR_MORE_DATA) {
            res = ERANGE;
        } else {
            res = EINVAL;
        }
    }

    /* os_strerror_r should not modify errno itself */
    if (errs[0] != errs[1]) {
        os_setErrno(errs[0]);
    }

    str[len - 1] = '\0'; /* always null-terminate, just to be safe */

    return res;
}
