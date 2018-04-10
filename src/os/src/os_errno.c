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
#include <stddef.h>
#include <string.h>

#define MIN_BUFLEN (64)
#define MAX_BUFLEN (1024)

const char *
os_strerror(
    _In_ int err)
{
    char *mem;
    char *ptr, *str = NULL;
    size_t len = 0;
    size_t tot;
    int ret = 0;

    /* os_threadMem* does not support destructors, but does free the memory
       referred to by the index. To avoid memory leaks and extra work the
       length of the string is placed in front of the string. */
    if ((mem = os_threadMemGet(OS_THREAD_STR_ERROR)) == NULL) {
        ret = ERANGE;
    } else {
        str = mem + sizeof(len);
        memcpy(&len, mem, sizeof(len));
    }

    /* os_strerror_r returns ERANGE if the buffer is too small.
       Iteratively increase the buffer and retry until MAX_BUFLEN is reached,
       in which case, give up and print the error number as text. */
    do {
        if (ret == ERANGE && len < MAX_BUFLEN) {
            os_threadMemFree(OS_THREAD_STR_ERROR);
            if (len == 0) {
                len = MIN_BUFLEN;
            } else {
                len *= 2;
            }

            tot = sizeof(len) + len + 1;
            mem = os_threadMemMalloc(OS_THREAD_STR_ERROR, tot);
            if (mem != NULL) {
                memcpy(mem, &len, sizeof(len));
                str = mem + sizeof(len);
            } else {
                ret = ENOMEM;
                str = NULL;
                len = 0;
            }
        }

        if (str != NULL) {
            assert(len != 0 && len >= MIN_BUFLEN && len <= MAX_BUFLEN);
            assert((len % MIN_BUFLEN) == 0);
            if (ret == ERANGE && len == MAX_BUFLEN) {
                ret = 0;
                (void)snprintf(str, len, "Error (%d)", err);
            } else {
                /* Solaris 10 does not populate buffer if it is too small */
                memset(str, '\0', len + 1);
                ret = os_strerror_r(err, str, len);
            }
        }
    } while (str != NULL && ret == ERANGE);

    switch(ret) {
        case ENOMEM:
            str = "Out of memory";
            break;
        case EINVAL:
            (void)snprintf(str, len, "Unknown error (%d)", err);
            break;
        default:
            assert(str != NULL);
            /* strip newline and/or carriage return */
            ptr = str;
            while (*ptr != '\0' && *ptr != '\n' && *ptr != '\r') {
                ptr++;
            }
            if (*ptr == '\n' || *ptr == '\r') {
                *ptr = '\0';
            }
            break;
    }

    return str;
}
