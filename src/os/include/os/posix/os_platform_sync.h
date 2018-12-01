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

#include <stdint.h>
#include <pthread.h>
#if HAVE_LKST
#include "lkst.h"
#endif

#if defined (__cplusplus)
extern "C" {
#endif

    typedef struct os_cond {
        pthread_cond_t cond;
    } os_cond;

    typedef struct os_mutex {
        pthread_mutex_t mutex;
    } os_mutex;

    typedef struct os_rwlock {
      pthread_rwlock_t rwlock;
    } os_rwlock;

    typedef pthread_once_t os_once_t;
    #define OS_ONCE_T_STATIC_INIT PTHREAD_ONCE_INIT

    void os_syncModuleInit(void);
    void os_syncModuleExit(void);

#if defined (__cplusplus)
}
#endif

#endif
