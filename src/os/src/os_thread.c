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
/** \file os/common/code/os_thread_attr.c
 *  \brief Common thread attributes
 *
 * Implements os_threadAttrInit and sets attributes
 * to platform independent values:
 * - scheduling class is OS_SCHED_DEFAULT
 * - thread priority is 0
 */

#include <assert.h>
#include "os/os.h"

/** \brief Initialize thread attributes
 *
 * - Set \b procAttr->schedClass to \b OS_SCHED_DEFAULT
 *   (take the platforms default scheduling class, Time-sharing for
 *   non realtime platforms, Real-time for realtime platforms)
 * - Set \b procAttr->schedPriority to \b 0
 */
void
os_threadAttrInit (
    os_threadAttr *threadAttr)
{
    assert (threadAttr != NULL);
    threadAttr->schedClass = OS_SCHED_DEFAULT;
    threadAttr->schedPriority = 0;
    threadAttr->stackSize = 0;
}

int32_t
os_threadFigureIdentity(char *str, uint32_t size)
{
    int32_t cnt;
    uintmax_t id;
    char *fmt, *ptr, buf[1] = { '\0' };
    uint32_t sz;

    assert(str != NULL);
    assert(size >= 1);

    id = os_threadIdToInteger(os_threadIdSelf());
    cnt = os_threadGetThreadName(str, size);
    if (cnt >= 0) {
        fmt = (cnt > 0 ? " 0x%"PRIxMAX : "0x%"PRIxMAX);
        if ((uint32_t)cnt < size) {
            ptr = str + (uint32_t)cnt;
            sz = size - (uint32_t)cnt;
        } else {
            ptr = buf;
            sz = sizeof(buf);
        }

        cnt += snprintf(ptr, sz, fmt, id);
    }

    return cnt;
}
