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
#define OS_HAVE_INLINE 0 /* override automatic determination of inlining */
#define OS_INLINE        /* no "inline" in function defs (not really needed) */
#define OS_ATOMICS_OMIT_FUNCTIONS 0 /* force inclusion of functions defs */

#include "os/os.h"

#if OS_ATOMIC_LIFO_SUPPORT
void os_atomic_lifo_init (os_atomic_lifo_t *head)
{
  head->aba_head.s.a = head->aba_head.s.b = 0;
}
void os_atomic_lifo_push (os_atomic_lifo_t *head, void *elem, size_t linkoff)
{
  uintptr_t a0, b0;
  do {
    a0 = *((volatile uintptr_t *) &head->aba_head.s.a);
    b0 = *((volatile uintptr_t *) &head->aba_head.s.b);
    *((volatile uintptr_t *) ((char *) elem + linkoff)) = b0;
  } while (!os_atomic_casvoidp2 (&head->aba_head, a0, b0, a0+1, (uintptr_t)elem));
}
void *os_atomic_lifo_pop (os_atomic_lifo_t *head, size_t linkoff) {
  uintptr_t a0, b0, b1;
  do {
    a0 = *((volatile uintptr_t *) &head->aba_head.s.a);
    b0 = *((volatile uintptr_t *) &head->aba_head.s.b);
    if (b0 == 0) {
      return NULL;
    }
    b1 = (*((volatile uintptr_t *) ((char *) b0 + linkoff)));
  } while (!os_atomic_casvoidp2 (&head->aba_head, a0, b0, a0+1, b1));
  return (void *) b0;
}
void os_atomic_lifo_pushmany (os_atomic_lifo_t *head, void *first, void *last, size_t linkoff)
{
  uintptr_t a0, b0;
  do {
    a0 = *((volatile uintptr_t *) &head->aba_head.s.a);
    b0 = *((volatile uintptr_t *) &head->aba_head.s.b);
    *((volatile uintptr_t *) ((char *) last + linkoff)) = b0;
  } while (!os_atomic_casvoidp2 (&head->aba_head, a0, b0, a0+1, (uintptr_t)first));
}
#endif
