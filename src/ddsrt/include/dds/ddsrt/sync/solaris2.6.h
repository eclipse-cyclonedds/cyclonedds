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
#ifndef DDSRT_POSIX_SYNC_H
#define DDSRT_POSIX_SYNC_H

#include <stdint.h>
#include <pthread.h>
#if HAVE_LKST
#include "lkst.h"
#endif

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct {
    pthread_cond_t cond;
} ddsrt_cond_t;

typedef struct {
    pthread_mutex_t mutex;
} ddsrt_mutex_t;

typedef struct {
    pthread_mutex_t rwlock;
} ddsrt_rwlock_t;

typedef pthread_once_t ddsrt_once_t;
#define DDSRT_ONCE_INIT PTHREAD_ONCE_INIT

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_POSIX_SYNC_H */
