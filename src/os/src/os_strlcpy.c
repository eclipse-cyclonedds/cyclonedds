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
#include <string.h>

#include "os/os.h"

_Success_(return < size)
size_t
os_strlcpy(
    _Out_writes_z_(size) char * __restrict dest,
    _In_z_ const char * __restrict src,
    _In_ size_t size)
{
    size_t srclen = 0;

    assert(dest != NULL);
    assert(src != NULL);

    /* strlcpy must return the number of bytes that (would) have been written,
       i.e. the length of src. */
    srclen = strlen(src);
    if (size > 0) {
        size_t len = srclen;
        if (size <= srclen) {
            len = size - 1;
        }
        memcpy(dest, src, len);
        dest[len] = '\0';
    }

    return srclen;
}

/* NOTE: os_strlcat does not forward to strlcat too avoid a bug in the macOS
         implementation where it does not return the right result if dest
         contains more characters than the size specified if size is either
         0 or 1. */
_Success_(return < size)
size_t
os_strlcat(
    _Inout_updates_z_(size) char * __restrict dest,
    _In_z_ const char * __restrict src,
    _In_ size_t size)
{
    size_t destlen, srclen;

    assert(dest != NULL);
    assert(src != NULL);

    /* strlcat must return the number of bytes that (would) have been written,
       i.e. the length of dest plus the length of src. */
    destlen = strlen(dest);
    srclen = strlen(src);
    if (SIZE_MAX == destlen) {
        srclen = 0;
    } else if ((SIZE_MAX - destlen) <= srclen) {
        srclen = (SIZE_MAX - destlen) - 1;
    }
    if (size > 0 && --size > destlen) {
        size_t len = srclen;
        size -= destlen;
        if (size <= srclen) {
            len = size;
        }
        memcpy(dest + destlen, src, len);
        dest[destlen + len] = '\0';
    }

    return destlen + srclen;
}
