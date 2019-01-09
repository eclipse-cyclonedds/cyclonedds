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
#ifndef OS_PLATFORM_SYNC_H
#define OS_PLATFORM_SYNC_H

#if defined (__cplusplus)
extern "C" {
#endif

    typedef struct os_cond {
        CONDITION_VARIABLE cond;
    } os_cond;

    typedef struct os_mutex {
        SRWLOCK lock;
    } os_mutex;

    typedef struct os_rwlock {
        SRWLOCK lock;
        int state; /* -1: exclusive, 0: free, 1: shared */
    } os_rwlock;

    typedef INIT_ONCE os_once_t;
    #define OS_ONCE_T_STATIC_INIT INIT_ONCE_STATIC_INIT

#if defined (__cplusplus)
}
#endif

#endif
