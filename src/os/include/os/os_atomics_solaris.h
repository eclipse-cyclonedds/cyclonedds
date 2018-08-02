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
#include <atomic.h>

#define OS_ATOMIC64_SUPPORT 1

#if ! OS_ATOMICS_OMIT_FUNCTIONS

/* LD, ST */

VDDS_INLINE uint32_t os_atomic_ld32 (const volatile os_atomic_uint32_t *x) { return x->v; }
VDDS_INLINE uint64_t os_atomic_ld64 (const volatile os_atomic_uint64_t *x) { return x->v; }
VDDS_INLINE uintptr_t os_atomic_ldptr (const volatile os_atomic_uintptr_t *x) { return x->v; }
VDDS_INLINE void *os_atomic_ldvoidp (const volatile os_atomic_voidp_t *x) { return (void *) os_atomic_ldptr (x); }

VDDS_INLINE void os_atomic_st32 (volatile os_atomic_uint32_t *x, uint32_t v) { x->v = v; }
VDDS_INLINE void os_atomic_st64 (volatile os_atomic_uint64_t *x, uint64_t v) { x->v = v; }
VDDS_INLINE void os_atomic_stptr (volatile os_atomic_uintptr_t *x, uintptr_t v) { x->v = v; }
VDDS_INLINE void os_atomic_stvoidp (volatile os_atomic_voidp_t *x, void *v) { os_atomic_stptr (x, (uintptr_t) v); }

/* INC */

VDDS_INLINE void os_atomic_inc32 (volatile os_atomic_uint32_t *x) {
  atomic_inc_32 (&x->v);
}
VDDS_INLINE void os_atomic_inc64 (volatile os_atomic_uint64_t *x) {
  atomic_inc_64 (&x->v);
}
VDDS_INLINE void os_atomic_incptr (volatile os_atomic_uintptr_t *x) {
  atomic_inc_ulong (&x->v);
}
VDDS_INLINE uint32_t os_atomic_inc32_nv (volatile os_atomic_uint32_t *x) {
  return atomic_inc_32_nv (&x->v);
}
VDDS_INLINE uint64_t os_atomic_inc64_nv (volatile os_atomic_uint64_t *x) {
  return atomic_inc_64_nv (&x->v);
}
VDDS_INLINE uintptr_t os_atomic_incptr_nv (volatile os_atomic_uintptr_t *x) {
  return atomic_inc_ulong_nv (&x->v);
}

/* DEC */

VDDS_INLINE void os_atomic_dec32 (volatile os_atomic_uint32_t *x) {
  atomic_dec_32 (&x->v);
}
VDDS_INLINE void os_atomic_dec64 (volatile os_atomic_uint64_t *x) {
  atomic_dec_64 (&x->v);
}
VDDS_INLINE void os_atomic_decptr (volatile os_atomic_uintptr_t *x) {
  atomic_dec_ulong (&x->v);
}
VDDS_INLINE uint32_t os_atomic_dec32_nv (volatile os_atomic_uint32_t *x) {
  return atomic_dec_32_nv (&x->v);
}
VDDS_INLINE uint64_t os_atomic_dec64_nv (volatile os_atomic_uint64_t *x) {
  return atomic_dec_64_nv (&x->v);
}
VDDS_INLINE uintptr_t os_atomic_decptr_nv (volatile os_atomic_uintptr_t *x) {
  return atomic_dec_ulong_nv (&x->v);
}
VDDS_INLINE uint32_t os_atomic_dec32_ov (volatile os_atomic_uint32_t *x) {
  uint32_t oldval, newval;
  do { oldval = x->v; newval = oldval - 1; } while (atomic_cas_32 (&x->v, oldval, newval) != oldval);
  return oldval;
}
VDDS_INLINE uint64_t os_atomic_dec64_ov (volatile os_atomic_uint64_t *x) {
  uint64_t oldval, newval;
  do { oldval = x->v; newval = oldval - 1; } while (atomic_cas_64 (&x->v, oldval, newval) != oldval);
  return oldval;
}
VDDS_INLINE uintptr_t os_atomic_decptr_ov (volatile os_atomic_uintptr_t *x) {
  uintptr_t oldval, newval;
  do { oldval = x->v; newval = oldval - 1; } while (atomic_cas_64 (&x->v, oldval, newval) != oldval);
  return oldval;
}

/* ADD */

VDDS_INLINE void os_atomic_add32 (volatile os_atomic_uint32_t *x, uint32_t v) {
  atomic_add_32 (&x->v, v);
}
VDDS_INLINE void os_atomic_add64 (volatile os_atomic_uint64_t *x, uint64_t v) {
  atomic_add_64 (&x->v, v);
}
VDDS_INLINE void os_atomic_addptr (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  atomic_add_long (&x->v, v);
}
VDDS_INLINE void os_atomic_addvoidp (volatile os_atomic_voidp_t *x, ptrdiff_t v) {
  atomic_add_ptr (&x->v, v);
}
VDDS_INLINE uint32_t os_atomic_add32_nv (volatile os_atomic_uint32_t *x, uint32_t v) {
  return atomic_add_32_nv (&x->v, v);
}
VDDS_INLINE uint64_t os_atomic_add64_nv (volatile os_atomic_uint64_t *x, uint64_t v) {
  return atomic_add_64_nv (&x->v, v);
}
VDDS_INLINE uintptr_t os_atomic_addptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  return atomic_add_long_nv (&x->v, v);
}
VDDS_INLINE void *os_atomic_addvoidp_nv (volatile os_atomic_voidp_t *x, ptrdiff_t v) {
  return atomic_add_ptr_nv (&x->v, v);
}

/* SUB */

VDDS_INLINE void os_atomic_sub32 (volatile os_atomic_uint32_t *x, uint32_t v) {
  atomic_add_32 (&x->v, -v);
}
VDDS_INLINE void os_atomic_sub64 (volatile os_atomic_uint64_t *x, uint64_t v) {
  atomic_add_64 (&x->v, -v);
}
VDDS_INLINE void os_atomic_subptr (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  atomic_add_long (&x->v, -v);
}
VDDS_INLINE void os_atomic_subvoidp (volatile os_atomic_voidp_t *x, ptrdiff_t v) {
  atomic_add_ptr (&x->v, -v);
}
VDDS_INLINE uint32_t os_atomic_sub32_nv (volatile os_atomic_uint32_t *x, uint32_t v) {
  return atomic_add_32_nv (&x->v, -v);
}
VDDS_INLINE uint64_t os_atomic_sub64_nv (volatile os_atomic_uint64_t *x, uint64_t v) {
  return atomic_add_64_nv (&x->v, -v);
}
VDDS_INLINE uintptr_t os_atomic_subptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  return atomic_add_long_nv (&x->v, -v);
}
VDDS_INLINE void *os_atomic_subvoidp_nv (volatile os_atomic_voidp_t *x, ptrdiff_t v) {
  return atomic_add_ptr_nv (&x->v, -v);
}

/* AND */

VDDS_INLINE void os_atomic_and32 (volatile os_atomic_uint32_t *x, uint32_t v) {
  atomic_and_32 (&x->v, v);
}
VDDS_INLINE void os_atomic_and64 (volatile os_atomic_uint64_t *x, uint64_t v) {
  atomic_and_64 (&x->v, v);
}
VDDS_INLINE void os_atomic_andptr (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  atomic_and_ulong (&x->v, v);
}
VDDS_INLINE uint32_t os_atomic_and32_ov (volatile os_atomic_uint32_t *x, uint32_t v) {
  uint32_t oldval, newval;
  do { oldval = x->v; newval = oldval & v; } while (atomic_cas_32 (&x->v, oldval, newval) != oldval);
  return oldval;
}
VDDS_INLINE uint64_t os_atomic_and64_ov (volatile os_atomic_uint64_t *x, uint64_t v) {
  uint64_t oldval, newval;
  do { oldval = x->v; newval = oldval & v; } while (atomic_cas_64 (&x->v, oldval, newval) != oldval);
  return oldval;
}
VDDS_INLINE uintptr_t os_atomic_andptr_ov (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  uintptr_t oldval, newval;
  do { oldval = x->v; newval = oldval & v; } while (atomic_cas_ulong (&x->v, oldval, newval) != oldval);
  return oldval;
}
VDDS_INLINE uint32_t os_atomic_and32_nv (volatile os_atomic_uint32_t *x, uint32_t v) {
  return atomic_and_32_nv (&x->v, v);
}
VDDS_INLINE uint64_t os_atomic_and64_nv (volatile os_atomic_uint64_t *x, uint64_t v) {
  return atomic_and_64_nv (&x->v, v);
}
VDDS_INLINE uintptr_t os_atomic_andptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  return atomic_and_ulong_nv (&x->v, v);
}

/* OR */

VDDS_INLINE void os_atomic_or32 (volatile os_atomic_uint32_t *x, uint32_t v) {
  atomic_or_32 (&x->v, v);
}
VDDS_INLINE void os_atomic_or64 (volatile os_atomic_uint64_t *x, uint64_t v) {
  atomic_or_64 (&x->v, v);
}
VDDS_INLINE void os_atomic_orptr (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  atomic_or_ulong (&x->v, v);
}
VDDS_INLINE uint32_t os_atomic_or32_ov (volatile os_atomic_uint32_t *x, uint32_t v) {
  uint32_t oldval, newval;
  do { oldval = x->v; newval = oldval | v; } while (atomic_cas_32 (&x->v, oldval, newval) != oldval);
  return oldval;
}
VDDS_INLINE uint64_t os_atomic_or64_ov (volatile os_atomic_uint64_t *x, uint64_t v) {
  uint64_t oldval, newval;
  do { oldval = x->v; newval = oldval | v; } while (atomic_cas_64 (&x->v, oldval, newval) != oldval);
  return oldval;
}
VDDS_INLINE uintptr_t os_atomic_orptr_ov (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  uintptr_t oldval, newval;
  do { oldval = x->v; newval = oldval | v; } while (atomic_cas_ulong (&x->v, oldval, newval) != oldval);
  return oldval;
}
VDDS_INLINE uint32_t os_atomic_or32_nv (volatile os_atomic_uint32_t *x, uint32_t v) {
  return atomic_or_32_nv (&x->v, v);
}
VDDS_INLINE uint64_t os_atomic_or64_nv (volatile os_atomic_uint64_t *x, uint64_t v) {
  return atomic_or_64_nv (&x->v, v);
}
VDDS_INLINE uintptr_t os_atomic_orptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  return atomic_or_ulong_nv (&x->v, v);
}

/* CAS */

VDDS_INLINE int os_atomic_cas32 (volatile os_atomic_uint32_t *x, uint32_t exp, uint32_t des) {
  return atomic_cas_32 (&x->v, exp, des) == exp;
}
VDDS_INLINE int os_atomic_cas64 (volatile os_atomic_uint64_t *x, uint64_t exp, uint64_t des) {
  return atomic_cas_64 (&x->v, exp, des) == exp;
}
VDDS_INLINE int os_atomic_casptr (volatile os_atomic_uintptr_t *x, uintptr_t exp, uintptr_t des) {
  return atomic_cas_ulong (&x->v, exp, des) == exp;
}
VDDS_INLINE int os_atomic_casvoidp (volatile os_atomic_voidp_t *x, void *exp, void *des) {
  return atomic_cas_ptr (&x->v, exp, des) == exp;
}

/* FENCES */

VDDS_INLINE void os_atomic_fence (void) {
  membar_exit ();
  membar_enter ();
}
VDDS_INLINE void os_atomic_fence_acq (void) {
  membar_enter ();
}
VDDS_INLINE void os_atomic_fence_rel (void) {
  membar_exit ();
}

#endif /* not omit functions */

#define OS_ATOMIC_SUPPORT 1
