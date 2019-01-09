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

/* LD, ST */
extern inline uint32_t os_atomic_ld32 (const volatile os_atomic_uint32_t *x);
#if OS_ATOMIC64_SUPPORT
extern inline uint64_t os_atomic_ld64 (const volatile os_atomic_uint64_t *x);
#endif
extern inline uintptr_t os_atomic_ldptr (const volatile os_atomic_uintptr_t *x);
extern inline void *os_atomic_ldvoidp (const volatile os_atomic_voidp_t *x);
extern inline void os_atomic_st32 (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
extern inline void os_atomic_st64 (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
extern inline void os_atomic_stptr (volatile os_atomic_uintptr_t *x, uintptr_t v);
extern inline void os_atomic_stvoidp (volatile os_atomic_voidp_t *x, void *v);
/* INC */
extern inline void os_atomic_inc32 (volatile os_atomic_uint32_t *x);
#if OS_ATOMIC64_SUPPORT
extern inline void os_atomic_inc64 (volatile os_atomic_uint64_t *x);
#endif
extern inline void os_atomic_incptr (volatile os_atomic_uintptr_t *x);
extern inline uint32_t os_atomic_inc32_nv (volatile os_atomic_uint32_t *x);
#if OS_ATOMIC64_SUPPORT
extern inline uint64_t os_atomic_inc64_nv (volatile os_atomic_uint64_t *x);
#endif
extern inline uintptr_t os_atomic_incptr_nv (volatile os_atomic_uintptr_t *x);
/* DEC */
extern inline void os_atomic_dec32 (volatile os_atomic_uint32_t *x);
#if OS_ATOMIC64_SUPPORT
extern inline void os_atomic_dec64 (volatile os_atomic_uint64_t *x);
#endif
extern inline void os_atomic_decptr (volatile os_atomic_uintptr_t *x);
extern inline uint32_t os_atomic_dec32_nv (volatile os_atomic_uint32_t *x);
#if OS_ATOMIC64_SUPPORT
extern inline uint64_t os_atomic_dec64_nv (volatile os_atomic_uint64_t *x);
#endif
extern inline uintptr_t os_atomic_decptr_nv (volatile os_atomic_uintptr_t *x);
extern inline uint32_t os_atomic_dec32_ov (volatile os_atomic_uint32_t *x);
#if OS_ATOMIC64_SUPPORT
extern inline uint64_t os_atomic_dec64_ov (volatile os_atomic_uint64_t *x);
#endif
extern inline uintptr_t os_atomic_decptr_ov (volatile os_atomic_uintptr_t *x);
/* ADD */
extern inline void os_atomic_add32 (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
extern inline void os_atomic_add64 (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
extern inline void os_atomic_addptr (volatile os_atomic_uintptr_t *x, uintptr_t v);
extern inline void os_atomic_addvoidp (volatile os_atomic_voidp_t *x, ptrdiff_t v);
extern inline uint32_t os_atomic_add32_nv (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
extern inline uint64_t os_atomic_add64_nv (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
extern inline uintptr_t os_atomic_addptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v);
extern inline void *os_atomic_addvoidp_nv (volatile os_atomic_voidp_t *x, ptrdiff_t v);
/* SUB */
extern inline void os_atomic_sub32 (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
extern inline void os_atomic_sub64 (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
extern inline void os_atomic_subptr (volatile os_atomic_uintptr_t *x, uintptr_t v);
extern inline void os_atomic_subvoidp (volatile os_atomic_voidp_t *x, ptrdiff_t v);
extern inline uint32_t os_atomic_sub32_nv (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
extern inline uint64_t os_atomic_sub64_nv (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
extern inline uintptr_t os_atomic_subptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v);
extern inline void *os_atomic_subvoidp_nv (volatile os_atomic_voidp_t *x, ptrdiff_t v);
/* AND */
extern inline void os_atomic_and32 (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
extern inline void os_atomic_and64 (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
extern inline void os_atomic_andptr (volatile os_atomic_uintptr_t *x, uintptr_t v);
extern inline uint32_t os_atomic_and32_ov (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
extern inline uint64_t os_atomic_and64_ov (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
extern inline uintptr_t os_atomic_andptr_ov (volatile os_atomic_uintptr_t *x, uintptr_t v);
extern inline uint32_t os_atomic_and32_nv (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
extern inline uint64_t os_atomic_and64_nv (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
extern inline uintptr_t os_atomic_andptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v);
/* OR */
extern inline void os_atomic_or32 (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
extern inline void os_atomic_or64 (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
extern inline void os_atomic_orptr (volatile os_atomic_uintptr_t *x, uintptr_t v);
extern inline uint32_t os_atomic_or32_ov (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
extern inline uint64_t os_atomic_or64_ov (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
extern inline uintptr_t os_atomic_orptr_ov (volatile os_atomic_uintptr_t *x, uintptr_t v);
extern inline uint32_t os_atomic_or32_nv (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
extern inline uint64_t os_atomic_or64_nv (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
extern inline uintptr_t os_atomic_orptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v);
/* CAS */
extern inline int os_atomic_cas32 (volatile os_atomic_uint32_t *x, uint32_t exp, uint32_t des);
#if OS_ATOMIC64_SUPPORT
extern inline int os_atomic_cas64 (volatile os_atomic_uint64_t *x, uint64_t exp, uint64_t des);
#endif
extern inline int os_atomic_casptr (volatile os_atomic_uintptr_t *x, uintptr_t exp, uintptr_t des);
extern inline int os_atomic_casvoidp (volatile os_atomic_voidp_t *x, void *exp, void *des);
#if OS_ATOMIC_LIFO_SUPPORT
extern inline int os_atomic_casvoidp2 (volatile os_atomic_uintptr2_t *x, uintptr_t a0, uintptr_t b0, uintptr_t a1, uintptr_t b1);
#endif
/* FENCES */
extern inline void os_atomic_fence (void);
extern inline void os_atomic_fence_ldld (void);
extern inline void os_atomic_fence_acq (void);
extern inline void os_atomic_fence_rel (void);

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
