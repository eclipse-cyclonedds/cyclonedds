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
#ifndef OS_ATOMICS_H
#define OS_ATOMICS_H

#include <stddef.h>
#include <limits.h>

#include "os/os_defs.h"

#if defined (__cplusplus)
extern "C" {
#endif

#if ! OS_ATOMIC_SUPPORT && (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) >= 40100
#include "os/os_atomics_gcc.h"
#endif

#if ! OS_ATOMIC_SUPPORT && defined _WIN32
/* windows.h causes HUGE problems when included too early, primarily
   because you can't include only a subset and later include the rest */
#include "os_atomics_win32.h"
#endif

#if ! OS_ATOMIC_SUPPORT && defined __sun
#include "os_atomics_solaris.h"
#endif

#if ! OS_ATOMIC_SUPPORT
#error "No support for atomic operations on this platform"
#endif

/* LIFO */
#if OS_ATOMIC_LIFO_SUPPORT
typedef struct os_atomic_lifo {
  os_atomic_uintptr2_t aba_head;
} os_atomic_lifo_t;

OSAPI_EXPORT void os_atomic_lifo_init (os_atomic_lifo_t *head);
OSAPI_EXPORT void os_atomic_lifo_push (os_atomic_lifo_t *head, void *elem, size_t linkoff);
OSAPI_EXPORT void *os_atomic_lifo_pop (os_atomic_lifo_t *head, size_t linkoff);
OSAPI_EXPORT void os_atomic_lifo_pushmany (os_atomic_lifo_t *head, void *first, void *last, size_t linkoff);
#endif

#if defined (__cplusplus)
}
#endif

#endif /* OS_ATOMICS_H */
