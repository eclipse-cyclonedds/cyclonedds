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
#ifndef OS_STRLCPY_H
#define OS_STRLCPY_H

#include "os/os_defs.h"

#if defined (__cplusplus)
extern "C" {
#endif

/**
 * @brief Copy string.
 *
 * Copy string to buffer with specified size. The string is truncated if there
 * is not enough space. The resulting string is guaranteed to be null
 * terminated if there is space.
 *
 * @param[out]  dest  Destination buffer.
 * @param[in]   src   Null terminated string to copy to dest.
 * @param[in]   size  Number of bytes available in dest.
 *
 * @returns Number of characters copied to dest (excluding the null byte), or
 *          the number of characters that would have been copied if dest is not
 *          sufficiently large enough.
 */
_Success_(return < size)
OSAPI_EXPORT
size_t
os_strlcpy(
    _Out_writes_z_(size) char * __restrict dest,
    _In_z_ const char * __restrict src,
    _In_ size_t size);

/**
 * @brief Concatenate strings.
 *
 * Append the string specified by src to the string specified by dest. The
 * terminating null byte at the end of dest is overwritten. The resulting
 * string is truncated if there is not enough space. The resulting string
 * guaranteed to be null terminated if there is space.
 *
 * @param[inout]  dest  Destination buffer.
 * @param[in]     src   Null terminated string to append to dest.
 * @param[in]     size  Number of bytes available in dest.
 *
 * @returns Number of characters copied to dest (excluding the null byte), or
 *          the number of characters that would have been copied if dest is not
 *          sufficiently large enough.
 */
_Success_(return < size)
OSAPI_EXPORT
size_t
os_strlcat(
    _Inout_updates_z_(size) char * __restrict dest,
    _In_z_ const char * __restrict src,
    _In_ size_t size);

#if defined (__cplusplus)
}
#endif

#endif /* OS_STRLCPY_H */
