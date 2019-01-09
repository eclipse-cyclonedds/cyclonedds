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
#ifndef OS_ERRNO_H
#define OS_ERRNO_H

#if defined (__cplusplus)
extern "C" {
#endif

#include "os/os_defs.h"

#if (defined(WIN32) || defined(WINCE))
#include <winerror.h>
#endif

#include <errno.h> /* Required on Windows platforms too. */


/* DNS runtime functions require custom error codes to be defined. For POSIX
   platforms a high enough 32-bit number (not too close to any system specified
   error number) should be safe. Microsoft Windows error codes, however, are
   encoded after the HRESULT (https://en.wikipedia.org/wiki/HRESULT) scheme. To
   avoid clashes the customer code bit (0x20000000) must be set. */
#define OS_ERRBASE (0x20000000)

#define OS_HOST_NOT_FOUND (OS_ERRBASE + 1)
#define OS_NO_DATA (OS_ERRBASE + 2)
#define OS_NO_RECOVERY (OS_ERRBASE + 3)
#define OS_TRY_AGAIN (OS_ERRBASE + 4)

    /** \brief Get error code set by last operation that failed
     *
     * @return Error code
     */
    OSAPI_EXPORT int
    os_getErrno (
                 void);

    /** \brief Set error code to specified value
     *
     * @return void
     * @param err Error code
     */
    OSAPI_EXPORT void
    os_setErrno (
                 int err);

    /**
     * \brief Return string describing internal error number.
     *
     * @param[in]   errnum
     * @param[out]  buf
     * @param[in]   buflen
     *
     * @returns 0 on success or a valid error number on failure.
     *
     * @retval 0
     *           Success. The error message is copied to the buffer.
     * @retval EINVAL
     *           Not a valid internal error code.
     * @retval ERANGE
     *           Buffer is not large enough to store the error message.
     */
    OSAPI_EXPORT int _Success_(return == 0)
    os_errstr(
         _In_ int errnum,
         _Out_writes_opt_z_(buflen) char *buf,
         _In_ size_t buflen);

    /**
     * \brief Get description for specified error code
     *
     * @return 0 success. On error a (positive) error number is returned
     * @param err Error number
     * @param buf Buffer to store description in
     * @oaram bufsz Number of bytes available in buf
     */
    OSAPI_EXPORT int
    os_strerror_r (
        _In_ int err,
        _Out_writes_z_(bufsz) char *buf,
        _In_ size_t bufsz);

    /**
     * \brief Get description for specified error code
     *
     * @return Pointer to string allocated in thread specific memory
     * @param err Error number
     */
    OSAPI_EXPORT const char *
    os_strerror (
        _In_ int err);

#if defined (__cplusplus)
}
#endif

#endif /* OS_ERRNO_H */
