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

#if defined (__cplusplus)
extern "C" {
#endif

#define DDSRT_ATOMIC64_SUPPORT 1

/* LD, ST */

inline uint32_t ddsrt_atomic_ld32 (const ddsrt_atomic_uint32_t *x) { return x->v; }
inline uint64_t ddsrt_atomic_ld64 (const ddsrt_atomic_uint64_t *x) { return x->v; }
inline uintptr_t ddsrt_atomic_ldptr (const ddsrt_atomic_uintptr_t *x) { return x->v; }
inline void *ddsrt_atomic_ldvoidp (const ddsrt_atomic_voidp_t *x) { return (void *) ddsrt_atomic_ldptr (x); }

inline void ddsrt_atomic_st32 (ddsrt_atomic_uint32_t *x, uint32_t v) { x->v = v; }
inline void ddsrt_atomic_st64 (ddsrt_atomic_uint64_t *x, uint64_t v) { x->v = v; }
inline void ddsrt_atomic_stptr (ddsrt_atomic_uintptr_t *x, uintptr_t v) { x->v = v; }
inline void ddsrt_atomic_stvoidp (ddsrt_atomic_voidp_t *x, void *v) { ddsrt_atomic_stptr (x, (uintptr_t) v); }

/* INC */

inline void ddsrt_atomic_inc32 (ddsrt_atomic_uint32_t *x) {
  atomic_inc_32 (&x->v);
}
inline void ddsrt_atomic_inc64 (ddsrt_atomic_uint64_t *x) {
  atomic_inc_64 (&x->v);
}
inline void ddsrt_atomic_incptr (ddsrt_atomic_uintptr_t *x) {
  atomic_inc_ulong (&x->v);
}
inline uint32_t ddsrt_atomic_inc32_ov (ddsrt_atomic_uint32_t *x) {
  uint32_t oldval, newval;
  do { oldval = x->v; newval = oldval + 1; } while (atomic_cas_32 (&x->v, oldval, newval) != oldval);
  return oldval;
}
inline uint32_t ddsrt_atomic_inc32_nv (ddsrt_atomic_uint32_t *x) {
  return atomic_inc_32_nv (&x->v);
}
inline uint64_t ddsrt_atomic_inc64_nv (ddsrt_atomic_uint64_t *x) {
  return atomic_inc_64_nv (&x->v);
}
inline uintptr_t ddsrt_atomic_incptr_nv (ddsrt_atomic_uintptr_t *x) {
  return atomic_inc_ulong_nv (&x->v);
}

/* DEC */

inline void ddsrt_atomic_dec32 (ddsrt_atomic_uint32_t *x) {
  atomic_dec_32 (&x->v);
}
inline void ddsrt_atomic_dec64 (ddsrt_atomic_uint64_t *x) {
  atomic_dec_64 (&x->v);
}
inline void ddsrt_atomic_decptr (ddsrt_atomic_uintptr_t *x) {
  atomic_dec_ulong (&x->v);
}
inline uint32_t ddsrt_atomic_dec32_nv (ddsrt_atomic_uint32_t *x) {
  return atomic_dec_32_nv (&x->v);
}
inline uint64_t ddsrt_atomic_dec64_nv (ddsrt_atomic_uint64_t *x) {
  return atomic_dec_64_nv (&x->v);
}
inline uintptr_t ddsrt_atomic_decptr_nv (ddsrt_atomic_uintptr_t *x) {
  return atomic_dec_ulong_nv (&x->v);
}
inline uint32_t ddsrt_atomic_dec32_ov (ddsrt_atomic_uint32_t *x) {
  uint32_t oldval, newval;
  do { oldval = x->v; newval = oldval - 1; } while (atomic_cas_32 (&x->v, oldval, newval) != oldval);
  return oldval;
}
inline uint64_t ddsrt_atomic_dec64_ov (ddsrt_atomic_uint64_t *x) {
  uint64_t oldval, newval;
  do { oldval = x->v; newval = oldval - 1; } while (atomic_cas_64 (&x->v, oldval, newval) != oldval);
  return oldval;
}
inline uintptr_t ddsrt_atomic_decptr_ov (ddsrt_atomic_uintptr_t *x) {
  uintptr_t oldval, newval;
  do { oldval = x->v; newval = oldval - 1; } while (atomic_cas_64 (&x->v, oldval, newval) != oldval);
  return oldval;
}

/* ADD */

inline void ddsrt_atomic_add32 (ddsrt_atomic_uint32_t *x, uint32_t v) {
  atomic_add_32 (&x->v, v);
}
inline void ddsrt_atomic_add64 (ddsrt_atomic_uint64_t *x, uint64_t v) {
  atomic_add_64 (&x->v, v);
}
inline void ddsrt_atomic_addptr (ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  atomic_add_long (&x->v, v);
}
inline void ddsrt_atomic_addvoidp (ddsrt_atomic_voidp_t *x, ptrdiff_t v) {
  atomic_add_ptr (&x->v, v);
}
inline uint32_t ddsrt_atomic_add32_ov (ddsrt_atomic_uint32_t *x, uint32_t v) {
  return atomic_add_32_nv (&x->v, v) - v;
}
inline uint32_t ddsrt_atomic_add32_nv (ddsrt_atomic_uint32_t *x, uint32_t v) {
  return atomic_add_32_nv (&x->v, v);
}
inline uint64_t ddsrt_atomic_add64_nv (ddsrt_atomic_uint64_t *x, uint64_t v) {
  return atomic_add_64_nv (&x->v, v);
}
inline uintptr_t ddsrt_atomic_addptr_nv (ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  return atomic_add_long_nv (&x->v, v);
}
inline void *ddsrt_atomic_addvoidp_nv (ddsrt_atomic_voidp_t *x, ptrdiff_t v) {
  return atomic_add_ptr_nv (&x->v, v);
}

/* SUB */

inline void ddsrt_atomic_sub32 (ddsrt_atomic_uint32_t *x, uint32_t v) {
  atomic_add_32 (&x->v, -v);
}
inline void ddsrt_atomic_sub64 (ddsrt_atomic_uint64_t *x, uint64_t v) {
  atomic_add_64 (&x->v, -v);
}
inline void ddsrt_atomic_subptr (ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  atomic_add_long (&x->v, -v);
}
inline void ddsrt_atomic_subvoidp (ddsrt_atomic_voidp_t *x, ptrdiff_t v) {
  atomic_add_ptr (&x->v, -v);
}
inline uint32_t ddsrt_atomic_sub32_ov (ddsrt_atomic_uint32_t *x, uint32_t v) {
  return atomic_add_32_nv (&x->v, -v) + v;
}
inline uint32_t ddsrt_atomic_sub32_nv (ddsrt_atomic_uint32_t *x, uint32_t v) {
  return atomic_add_32_nv (&x->v, -v);
}
inline uint64_t ddsrt_atomic_sub64_nv (ddsrt_atomic_uint64_t *x, uint64_t v) {
  return atomic_add_64_nv (&x->v, -v);
}
inline uintptr_t ddsrt_atomic_subptr_nv (ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  return atomic_add_long_nv (&x->v, -v);
}
inline void *ddsrt_atomic_subvoidp_nv (ddsrt_atomic_voidp_t *x, ptrdiff_t v) {
  return atomic_add_ptr_nv (&x->v, -v);
}

/* AND */

inline void ddsrt_atomic_and32 (ddsrt_atomic_uint32_t *x, uint32_t v) {
  atomic_and_32 (&x->v, v);
}
inline void ddsrt_atomic_and64 (ddsrt_atomic_uint64_t *x, uint64_t v) {
  atomic_and_64 (&x->v, v);
}
inline void ddsrt_atomic_andptr (ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  atomic_and_ulong (&x->v, v);
}
inline uint32_t ddsrt_atomic_and32_ov (ddsrt_atomic_uint32_t *x, uint32_t v) {
  uint32_t oldval, newval;
  do { oldval = x->v; newval = oldval & v; } while (atomic_cas_32 (&x->v, oldval, newval) != oldval);
  return oldval;
}
inline uint64_t ddsrt_atomic_and64_ov (ddsrt_atomic_uint64_t *x, uint64_t v) {
  uint64_t oldval, newval;
  do { oldval = x->v; newval = oldval & v; } while (atomic_cas_64 (&x->v, oldval, newval) != oldval);
  return oldval;
}
inline uintptr_t ddsrt_atomic_andptr_ov (ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  uintptr_t oldval, newval;
  do { oldval = x->v; newval = oldval & v; } while (atomic_cas_ulong (&x->v, oldval, newval) != oldval);
  return oldval;
}
inline uint32_t ddsrt_atomic_and32_nv (ddsrt_atomic_uint32_t *x, uint32_t v) {
  return atomic_and_32_nv (&x->v, v);
}
inline uint64_t ddsrt_atomic_and64_nv (ddsrt_atomic_uint64_t *x, uint64_t v) {
  return atomic_and_64_nv (&x->v, v);
}
inline uintptr_t ddsrt_atomic_andptr_nv (ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  return atomic_and_ulong_nv (&x->v, v);
}

/* OR */

inline void ddsrt_atomic_or32 (ddsrt_atomic_uint32_t *x, uint32_t v) {
  atomic_or_32 (&x->v, v);
}
inline void ddsrt_atomic_or64 (ddsrt_atomic_uint64_t *x, uint64_t v) {
  atomic_or_64 (&x->v, v);
}
inline void ddsrt_atomic_orptr (ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  atomic_or_ulong (&x->v, v);
}
inline uint32_t ddsrt_atomic_or32_ov (ddsrt_atomic_uint32_t *x, uint32_t v) {
  uint32_t oldval, newval;
  do { oldval = x->v; newval = oldval | v; } while (atomic_cas_32 (&x->v, oldval, newval) != oldval);
  return oldval;
}
inline uint64_t ddsrt_atomic_or64_ov (ddsrt_atomic_uint64_t *x, uint64_t v) {
  uint64_t oldval, newval;
  do { oldval = x->v; newval = oldval | v; } while (atomic_cas_64 (&x->v, oldval, newval) != oldval);
  return oldval;
}
inline uintptr_t ddsrt_atomic_orptr_ov (ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  uintptr_t oldval, newval;
  do { oldval = x->v; newval = oldval | v; } while (atomic_cas_ulong (&x->v, oldval, newval) != oldval);
  return oldval;
}
inline uint32_t ddsrt_atomic_or32_nv (ddsrt_atomic_uint32_t *x, uint32_t v) {
  return atomic_or_32_nv (&x->v, v);
}
inline uint64_t ddsrt_atomic_or64_nv (ddsrt_atomic_uint64_t *x, uint64_t v) {
  return atomic_or_64_nv (&x->v, v);
}
inline uintptr_t ddsrt_atomic_orptr_nv (ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  return atomic_or_ulong_nv (&x->v, v);
}

/* CAS */

inline int ddsrt_atomic_cas32 (ddsrt_atomic_uint32_t *x, uint32_t exp, uint32_t des) {
  return atomic_cas_32 (&x->v, exp, des) == exp;
}
inline int ddsrt_atomic_cas64 (ddsrt_atomic_uint64_t *x, uint64_t exp, uint64_t des) {
  return atomic_cas_64 (&x->v, exp, des) == exp;
}
inline int ddsrt_atomic_casptr (ddsrt_atomic_uintptr_t *x, uintptr_t exp, uintptr_t des) {
  return atomic_cas_ulong (&x->v, exp, des) == exp;
}
inline int ddsrt_atomic_casvoidp (ddsrt_atomic_voidp_t *x, void *exp, void *des) {
  return atomic_cas_ptr (&x->v, exp, des) == exp;
}

/* FENCES */

inline void ddsrt_atomic_fence (void) {
  membar_exit ();
  membar_enter ();
}
inline void ddsrt_atomic_fence_ldld (void) {
  membar_consumer ();
}
inline void ddsrt_atomic_fence_stst (void) {
  membar_producer ();
}
inline void ddsrt_atomic_fence_acq (void) {
  membar_enter ();
}
inline void ddsrt_atomic_fence_rel (void) {
  membar_exit ();
}

#if defined (__cplusplus)
}
#endif
