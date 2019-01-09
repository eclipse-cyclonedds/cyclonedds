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
#define _GNU_SOURCE /* Required for RUSAGE_THREAD. */
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/resource.h>

#if defined(__APPLE__)
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/thread_act.h>
#endif

#include "os/os.h"

_Pre_satisfies_((who == OS_RUSAGE_SELF) || \
                (who == OS_RUSAGE_THREAD))
_Success_(return == 0)
int os_getrusage(_In_ int who, _Out_ os_rusage_t *usage)
{
    int err = 0;
    struct rusage buf;

    assert(who == OS_RUSAGE_SELF || who == OS_RUSAGE_THREAD);
    assert(usage != NULL);

    memset(&buf, 0, sizeof(buf));

#if defined(__linux)
    if (getrusage(who, &buf) == -1) {
        err = errno;
    } else {
        buf.ru_maxrss *= 1024;
    }
#else
    if (getrusage(RUSAGE_SELF, &buf) == -1) {
        err = errno;
    } else if (who == OS_RUSAGE_THREAD) {
        memset(&buf.ru_utime, 0, sizeof(buf.ru_utime));
        memset(&buf.ru_stime, 0, sizeof(buf.ru_stime));
        buf.ru_nvcsw = 0;
        buf.ru_nivcsw = 0;

#if defined(__APPLE__)
        kern_return_t ret;
        mach_port_t thr;
        mach_msg_type_number_t cnt;
        thread_basic_info_data_t info;

        thr = mach_thread_self();
        assert(thr != MACH_PORT_DEAD);
        if (thr == MACH_PORT_NULL) {
            /* Resource shortage prevented reception of send right. */
            err = ENOMEM;
        } else {
            cnt = THREAD_BASIC_INFO_COUNT;
            ret = thread_info(
                thr, THREAD_BASIC_INFO, (thread_info_t)&info, &cnt);
            assert(ret != KERN_INVALID_ARGUMENT);
            /* Assume MIG_ARRAY_TOO_LARGE will not happen. */
            buf.ru_utime.tv_sec = info.user_time.seconds;
            buf.ru_utime.tv_usec = info.user_time.microseconds;
            buf.ru_stime.tv_sec = info.system_time.seconds;
            buf.ru_stime.tv_usec = info.system_time.microseconds;
            mach_port_deallocate(mach_task_self(), thr);
        }
#endif /* __APPLE__ */
    }
#endif /* __linux */

    if (err == 0) {
        usage->utime.tv_sec = (os_timeSec)buf.ru_utime.tv_sec;
        usage->utime.tv_nsec = (int32_t)buf.ru_utime.tv_usec * 1000;
        usage->stime.tv_sec = (os_timeSec)buf.ru_stime.tv_sec;
        usage->stime.tv_nsec = (int32_t)buf.ru_stime.tv_usec * 1000;
        usage->maxrss = (size_t)buf.ru_maxrss;
        usage->idrss = (size_t)buf.ru_idrss;
        usage->nvcsw = (size_t)buf.ru_nvcsw;
        usage->nivcsw = (size_t)buf.ru_nivcsw;
    }

    return err;
}
