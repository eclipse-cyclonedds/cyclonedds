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
#ifndef OS_PLATFORM_THREAD_H
#define OS_PLATFORM_THREAD_H

#include "os_defs.h"

#if defined (__cplusplus)
extern "C" {
#endif

    typedef struct os_threadInfo_s {
        DWORD threadId;
        HANDLE handle;
    } os_threadId;

    os_result os_threadModuleInit (void);
    void os_threadModuleExit (void);

#if defined (__cplusplus)
}
#endif

#endif
