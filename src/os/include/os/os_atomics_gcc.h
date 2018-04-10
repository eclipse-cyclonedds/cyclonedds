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
#if (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) >= 40100

/* I don't quite know how to check whether 64-bit operations are
   supported, but my guess is that the size of a pointer is a fairly
   good indication.  Define OS_ATOMIC_ATOMIC64_SUPPORT beforehand to override
   this.  (Obviously, I'm also assuming that uintptr_t is an unsigned
   integer exactly as wide as a pointer, even though it may be
   wider.) */
#ifndef OS_ATOMIC64_SUPPORT
#ifdef OS_64BIT
#define OS_ATOMIC64_SUPPORT 1
#else
#define OS_ATOMIC64_SUPPORT 0
#endif
#endif

#if ! OS_ATOMICS_OMIT_FUNCTIONS

/* Eliminate C warnings */

#if ! defined (__cplusplus)
VDDS_INLINE uint32_t os_atomic_ld32 (const volatile os_atomic_uint32_t *x);
VDDS_INLINE uintptr_t os_atomic_ldptr (const volatile os_atomic_uintptr_t *x);
VDDS_INLINE void *os_atomic_ldvoidp (const volatile os_atomic_voidp_t *x);
VDDS_INLINE void os_atomic_st32 (volatile os_atomic_uint32_t *x, uint32_t v);
VDDS_INLINE void os_atomic_stptr (volatile os_atomic_uintptr_t *x, uintptr_t v);
VDDS_INLINE void os_atomic_stvoidp (volatile os_atomic_voidp_t *x, void *v);
VDDS_INLINE void os_atomic_inc32 (volatile os_atomic_uint32_t *x);
VDDS_INLINE void os_atomic_incptr (volatile os_atomic_uintptr_t *x);
VDDS_INLINE uint32_t os_atomic_inc32_nv (volatile os_atomic_uint32_t *x);
VDDS_INLINE uintptr_t os_atomic_incptr_nv (volatile os_atomic_uintptr_t *x);
VDDS_INLINE void os_atomic_dec32 (volatile os_atomic_uint32_t *x);
VDDS_INLINE void os_atomic_decptr (volatile os_atomic_uintptr_t *x);
VDDS_INLINE uint32_t os_atomic_dec32_nv (volatile os_atomic_uint32_t *x);
VDDS_INLINE uintptr_t os_atomic_decptr_nv (volatile os_atomic_uintptr_t *x);
VDDS_INLINE uint32_t os_atomic_dec32_ov (volatile os_atomic_uint32_t *x);
VDDS_INLINE uintptr_t os_atomic_decptr_ov (volatile os_atomic_uintptr_t *x);
VDDS_INLINE void os_atomic_add32 (volatile os_atomic_uint32_t *x, uint32_t v);
VDDS_INLINE void os_atomic_addptr (volatile os_atomic_uintptr_t *x, uintptr_t v);
VDDS_INLINE void os_atomic_addvoidp (volatile os_atomic_voidp_t *x, ptrdiff_t v);
VDDS_INLINE uint32_t os_atomic_add32_nv (volatile os_atomic_uint32_t *x, uint32_t v);
VDDS_INLINE uintptr_t os_atomic_addptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v);
VDDS_INLINE void *os_atomic_addvoidp_nv (volatile os_atomic_voidp_t *x, ptrdiff_t v);
VDDS_INLINE void os_atomic_sub32 (volatile os_atomic_uint32_t *x, uint32_t v);
VDDS_INLINE void os_atomic_subptr (volatile os_atomic_uintptr_t *x, uintptr_t v);
VDDS_INLINE void os_atomic_subvoidp (volatile os_atomic_voidp_t *x, ptrdiff_t v);
VDDS_INLINE uint32_t os_atomic_sub32_nv (volatile os_atomic_uint32_t *x, uint32_t v);
VDDS_INLINE uintptr_t os_atomic_subptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v);
VDDS_INLINE void *os_atomic_subvoidp_nv (volatile os_atomic_voidp_t *x, ptrdiff_t v);
VDDS_INLINE void os_atomic_and32 (volatile os_atomic_uint32_t *x, uint32_t v);
VDDS_INLINE void os_atomic_andptr (volatile os_atomic_uintptr_t *x, uintptr_t v);
VDDS_INLINE uint32_t os_atomic_and32_ov (volatile os_atomic_uint32_t *x, uint32_t v);
VDDS_INLINE uintptr_t os_atomic_andptr_ov (volatile os_atomic_uintptr_t *x, uintptr_t v);
VDDS_INLINE uint32_t os_atomic_and32_nv (volatile os_atomic_uint32_t *x, uint32_t v);
VDDS_INLINE uintptr_t os_atomic_andptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v);
VDDS_INLINE void os_atomic_or32 (volatile os_atomic_uint32_t *x, uint32_t v);
VDDS_INLINE void os_atomic_orptr (volatile os_atomic_uintptr_t *x, uintptr_t v);
VDDS_INLINE uint32_t os_atomic_or32_ov (volatile os_atomic_uint32_t *x, uint32_t v);
VDDS_INLINE uintptr_t os_atomic_orptr_ov (volatile os_atomic_uintptr_t *x, uintptr_t v);
VDDS_INLINE uint32_t os_atomic_or32_nv (volatile os_atomic_uint32_t *x, uint32_t v);
VDDS_INLINE uintptr_t os_atomic_orptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v);
VDDS_INLINE int os_atomic_cas32 (volatile os_atomic_uint32_t *x, uint32_t exp, uint32_t des);
VDDS_INLINE int os_atomic_casptr (volatile os_atomic_uintptr_t *x, uintptr_t exp, uintptr_t des);
VDDS_INLINE int os_atomic_casvoidp (volatile os_atomic_voidp_t *x, void *exp, void *des);
VDDS_INLINE void os_atomic_fence (void);
VDDS_INLINE void os_atomic_fence_acq (void);
VDDS_INLINE void os_atomic_fence_rel (void);
#if OS_ATOMIC64_SUPPORT
VDDS_INLINE uint64_t os_atomic_ld64 (const volatile os_atomic_uint64_t *x);
VDDS_INLINE void os_atomic_st64 (volatile os_atomic_uint64_t *x, uint64_t v);
VDDS_INLINE void os_atomic_inc64 (volatile os_atomic_uint64_t *x);
VDDS_INLINE uint64_t os_atomic_inc64_nv (volatile os_atomic_uint64_t *x);
VDDS_INLINE void os_atomic_dec64 (volatile os_atomic_uint64_t *x);
VDDS_INLINE uint64_t os_atomic_dec64_nv (volatile os_atomic_uint64_t *x);
VDDS_INLINE uint64_t os_atomic_dec64_ov (volatile os_atomic_uint64_t *x);
VDDS_INLINE void os_atomic_add64 (volatile os_atomic_uint64_t *x, uint64_t v);
VDDS_INLINE uint64_t os_atomic_add64_nv (volatile os_atomic_uint64_t *x, uint64_t v);
VDDS_INLINE void os_atomic_sub64 (volatile os_atomic_uint64_t *x, uint64_t v);
VDDS_INLINE uint64_t os_atomic_sub64_nv (volatile os_atomic_uint64_t *x, uint64_t v);
VDDS_INLINE void os_atomic_and64 (volatile os_atomic_uint64_t *x, uint64_t v);
VDDS_INLINE uint64_t os_atomic_and64_ov (volatile os_atomic_uint64_t *x, uint64_t v);
VDDS_INLINE uint64_t os_atomic_and64_nv (volatile os_atomic_uint64_t *x, uint64_t v);
VDDS_INLINE void os_atomic_or64 (volatile os_atomic_uint64_t *x, uint64_t v);
VDDS_INLINE uint64_t os_atomic_or64_ov (volatile os_atomic_uint64_t *x, uint64_t v);
VDDS_INLINE uint64_t os_atomic_or64_nv (volatile os_atomic_uint64_t *x, uint64_t v);
VDDS_INLINE int os_atomic_cas64 (volatile os_atomic_uint64_t *x, uint64_t exp, uint64_t des);
#endif
#endif

/* LD, ST */

VDDS_INLINE uint32_t os_atomic_ld32 (const volatile os_atomic_uint32_t *x) { return x->v; }
#if OS_ATOMIC64_SUPPORT
VDDS_INLINE uint64_t os_atomic_ld64 (const volatile os_atomic_uint64_t *x) { return x->v; }
#endif
VDDS_INLINE uintptr_t os_atomic_ldptr (const volatile os_atomic_uintptr_t *x) { return x->v; }
VDDS_INLINE void *os_atomic_ldvoidp (const volatile os_atomic_voidp_t *x) { return (void *) os_atomic_ldptr (x); }

VDDS_INLINE void os_atomic_st32 (volatile os_atomic_uint32_t *x, uint32_t v) { x->v = v; }
#if OS_ATOMIC64_SUPPORT
VDDS_INLINE void os_atomic_st64 (volatile os_atomic_uint64_t *x, uint64_t v) { x->v = v; }
#endif
VDDS_INLINE void os_atomic_stptr (volatile os_atomic_uintptr_t *x, uintptr_t v) { x->v = v; }
VDDS_INLINE void os_atomic_stvoidp (volatile os_atomic_voidp_t *x, void *v) { os_atomic_stptr (x, (uintptr_t) v); }

/* INC */

VDDS_INLINE void os_atomic_inc32 (volatile os_atomic_uint32_t *x) {
  __sync_fetch_and_add (&x->v, 1);
}
#if OS_ATOMIC64_SUPPORT
VDDS_INLINE void os_atomic_inc64 (volatile os_atomic_uint64_t *x) {
  __sync_fetch_and_add (&x->v, 1);
}
#endif
VDDS_INLINE void os_atomic_incptr (volatile os_atomic_uintptr_t *x) {
  __sync_fetch_and_add (&x->v, 1);
}
VDDS_INLINE uint32_t os_atomic_inc32_nv (volatile os_atomic_uint32_t *x) {
  return __sync_add_and_fetch (&x->v, 1);
}
#if OS_ATOMIC64_SUPPORT
VDDS_INLINE uint64_t os_atomic_inc64_nv (volatile os_atomic_uint64_t *x) {
  return __sync_add_and_fetch (&x->v, 1);
}
#endif
VDDS_INLINE uintptr_t os_atomic_incptr_nv (volatile os_atomic_uintptr_t *x) {
  return __sync_add_and_fetch (&x->v, 1);
}

/* DEC */

VDDS_INLINE void os_atomic_dec32 (volatile os_atomic_uint32_t *x) {
  __sync_fetch_and_sub (&x->v, 1);
}
#if OS_ATOMIC64_SUPPORT
VDDS_INLINE void os_atomic_dec64 (volatile os_atomic_uint64_t *x) {
  __sync_fetch_and_sub (&x->v, 1);
}
#endif
VDDS_INLINE void os_atomic_decptr (volatile os_atomic_uintptr_t *x) {
  __sync_fetch_and_sub (&x->v, 1);
}
VDDS_INLINE uint32_t os_atomic_dec32_nv (volatile os_atomic_uint32_t *x) {
  return __sync_sub_and_fetch (&x->v, 1);
}
#if OS_ATOMIC64_SUPPORT
VDDS_INLINE uint64_t os_atomic_dec64_nv (volatile os_atomic_uint64_t *x) {
  return __sync_sub_and_fetch (&x->v, 1);
}
#endif
VDDS_INLINE uintptr_t os_atomic_decptr_nv (volatile os_atomic_uintptr_t *x) {
  return __sync_sub_and_fetch (&x->v, 1);
}
VDDS_INLINE uint32_t os_atomic_dec32_ov (volatile os_atomic_uint32_t *x) {
  return __sync_fetch_and_sub (&x->v, 1);
}
#if OS_ATOMIC64_SUPPORT
VDDS_INLINE uint64_t os_atomic_dec64_ov (volatile os_atomic_uint64_t *x) {
  return __sync_fetch_and_sub (&x->v, 1);
}
#endif
VDDS_INLINE uintptr_t os_atomic_decptr_ov (volatile os_atomic_uintptr_t *x) {
  return __sync_fetch_and_sub (&x->v, 1);
}

/* ADD */

VDDS_INLINE void os_atomic_add32 (volatile os_atomic_uint32_t *x, uint32_t v) {
  __sync_fetch_and_add (&x->v, v);
}
#if OS_ATOMIC64_SUPPORT
VDDS_INLINE void os_atomic_add64 (volatile os_atomic_uint64_t *x, uint64_t v) {
  __sync_fetch_and_add (&x->v, v);
}
#endif
VDDS_INLINE void os_atomic_addptr (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  __sync_fetch_and_add (&x->v, v);
}
VDDS_INLINE void os_atomic_addvoidp (volatile os_atomic_voidp_t *x, ptrdiff_t v) {
  os_atomic_addptr ((volatile os_atomic_uintptr_t *) x, (uintptr_t) v);
}
VDDS_INLINE uint32_t os_atomic_add32_nv (volatile os_atomic_uint32_t *x, uint32_t v) {
  return __sync_add_and_fetch (&x->v, v);
}
#if OS_ATOMIC64_SUPPORT
VDDS_INLINE uint64_t os_atomic_add64_nv (volatile os_atomic_uint64_t *x, uint64_t v) {
  return __sync_add_and_fetch (&x->v, v);
}
#endif
VDDS_INLINE uintptr_t os_atomic_addptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  return __sync_add_and_fetch (&x->v, v);
}
VDDS_INLINE void *os_atomic_addvoidp_nv (volatile os_atomic_voidp_t *x, ptrdiff_t v) {
  return (void *) os_atomic_addptr_nv ((volatile os_atomic_uintptr_t *) x, (uintptr_t) v);
}

/* SUB */

VDDS_INLINE void os_atomic_sub32 (volatile os_atomic_uint32_t *x, uint32_t v) {
  __sync_fetch_and_sub (&x->v, v);
}
#if OS_ATOMIC64_SUPPORT
VDDS_INLINE void os_atomic_sub64 (volatile os_atomic_uint64_t *x, uint64_t v) {
  __sync_fetch_and_sub (&x->v, v);
}
#endif
VDDS_INLINE void os_atomic_subptr (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  __sync_fetch_and_sub (&x->v, v);
}
VDDS_INLINE void os_atomic_subvoidp (volatile os_atomic_voidp_t *x, ptrdiff_t v) {
  os_atomic_subptr ((volatile os_atomic_uintptr_t *) x, (uintptr_t) v);
}
VDDS_INLINE uint32_t os_atomic_sub32_nv (volatile os_atomic_uint32_t *x, uint32_t v) {
  return __sync_sub_and_fetch (&x->v, v);
}
#if OS_ATOMIC64_SUPPORT
VDDS_INLINE uint64_t os_atomic_sub64_nv (volatile os_atomic_uint64_t *x, uint64_t v) {
  return __sync_sub_and_fetch (&x->v, v);
}
#endif
VDDS_INLINE uintptr_t os_atomic_subptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  return __sync_sub_and_fetch (&x->v, v);
}
VDDS_INLINE void *os_atomic_subvoidp_nv (volatile os_atomic_voidp_t *x, ptrdiff_t v) {
  return (void *) os_atomic_subptr_nv ((volatile os_atomic_uintptr_t *) x, (uintptr_t) v);
}

/* AND */

VDDS_INLINE void os_atomic_and32 (volatile os_atomic_uint32_t *x, uint32_t v) {
  __sync_fetch_and_and (&x->v, v);
}
#if OS_ATOMIC64_SUPPORT
VDDS_INLINE void os_atomic_and64 (volatile os_atomic_uint64_t *x, uint64_t v) {
  __sync_fetch_and_and (&x->v, v);
}
#endif
VDDS_INLINE void os_atomic_andptr (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  __sync_fetch_and_and (&x->v, v);
}
VDDS_INLINE uint32_t os_atomic_and32_ov (volatile os_atomic_uint32_t *x, uint32_t v) {
  return __sync_fetch_and_and (&x->v, v);
}
#if OS_ATOMIC64_SUPPORT
VDDS_INLINE uint64_t os_atomic_and64_ov (volatile os_atomic_uint64_t *x, uint64_t v) {
  return __sync_fetch_and_and (&x->v, v);
}
#endif
VDDS_INLINE uintptr_t os_atomic_andptr_ov (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  return __sync_fetch_and_and (&x->v, v);
}
VDDS_INLINE uint32_t os_atomic_and32_nv (volatile os_atomic_uint32_t *x, uint32_t v) {
  return __sync_and_and_fetch (&x->v, v);
}
#if OS_ATOMIC64_SUPPORT
VDDS_INLINE uint64_t os_atomic_and64_nv (volatile os_atomic_uint64_t *x, uint64_t v) {
  return __sync_and_and_fetch (&x->v, v);
}
#endif
VDDS_INLINE uintptr_t os_atomic_andptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  return __sync_and_and_fetch (&x->v, v);
}

/* OR */

VDDS_INLINE void os_atomic_or32 (volatile os_atomic_uint32_t *x, uint32_t v) {
  __sync_fetch_and_or (&x->v, v);
}
#if OS_ATOMIC64_SUPPORT
VDDS_INLINE void os_atomic_or64 (volatile os_atomic_uint64_t *x, uint64_t v) {
  __sync_fetch_and_or (&x->v, v);
}
#endif
VDDS_INLINE void os_atomic_orptr (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  __sync_fetch_and_or (&x->v, v);
}
VDDS_INLINE uint32_t os_atomic_or32_ov (volatile os_atomic_uint32_t *x, uint32_t v) {
  return __sync_fetch_and_or (&x->v, v);
}
#if OS_ATOMIC64_SUPPORT
VDDS_INLINE uint64_t os_atomic_or64_ov (volatile os_atomic_uint64_t *x, uint64_t v) {
  return __sync_fetch_and_or (&x->v, v);
}
#endif
VDDS_INLINE uintptr_t os_atomic_orptr_ov (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  return __sync_fetch_and_or (&x->v, v);
}
VDDS_INLINE uint32_t os_atomic_or32_nv (volatile os_atomic_uint32_t *x, uint32_t v) {
  return __sync_or_and_fetch (&x->v, v);
}
#if OS_ATOMIC64_SUPPORT
VDDS_INLINE uint64_t os_atomic_or64_nv (volatile os_atomic_uint64_t *x, uint64_t v) {
  return __sync_or_and_fetch (&x->v, v);
}
#endif
VDDS_INLINE uintptr_t os_atomic_orptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  return __sync_or_and_fetch (&x->v, v);
}

/* CAS */

VDDS_INLINE int os_atomic_cas32 (volatile os_atomic_uint32_t *x, uint32_t exp, uint32_t des) {
  return __sync_bool_compare_and_swap (&x->v, exp, des);
}
#if OS_ATOMIC64_SUPPORT
VDDS_INLINE int os_atomic_cas64 (volatile os_atomic_uint64_t *x, uint64_t exp, uint64_t des) {
  return __sync_bool_compare_and_swap (&x->v, exp, des);
}
#endif
VDDS_INLINE int os_atomic_casptr (volatile os_atomic_uintptr_t *x, uintptr_t exp, uintptr_t des) {
  return __sync_bool_compare_and_swap (&x->v, exp, des);
}
VDDS_INLINE int os_atomic_casvoidp (volatile os_atomic_voidp_t *x, void *exp, void *des) {
  return os_atomic_casptr (x, (uintptr_t) exp, (uintptr_t) des);
}

/* FENCES */

VDDS_INLINE void os_atomic_fence (void) {
  __sync_synchronize ();
}
VDDS_INLINE void os_atomic_fence_acq (void) {
  os_atomic_fence ();
}
VDDS_INLINE void os_atomic_fence_rel (void) {
  os_atomic_fence ();
}

#endif /* not omit functions */

#define OS_ATOMIC_SUPPORT 1
#endif
