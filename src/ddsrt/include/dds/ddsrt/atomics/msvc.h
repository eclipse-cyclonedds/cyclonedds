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
#ifndef DDSRT_ATOMICS_MSVC_H
#define DDSRT_ATOMICS_MSVC_H

#include "dds/ddsrt/misc.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* x86 has supported 64-bit CAS for a long time, so Windows ought to
   provide all the interlocked operations for 64-bit operands on x86
   platforms, but it doesn't. */

#define DDSRT_ATOMIC_OP32(name, ...) name ((volatile long *) __VA_ARGS__)
#if DDSRT_HAVE_ATOMIC64
#define DDSRT_ATOMIC_OP64(name, ...) name##64 ((volatile int64_t *) __VA_ARGS__)
#define DDSRT_ATOMIC_PTROP(name, ...) name##64 ((volatile int64_t *) __VA_ARGS__)
#else
#define DDSRT_ATOMIC_PTROP(name, ...) name ((volatile long *) __VA_ARGS__)
#endif

/* LD, ST */

inline uint32_t ddsrt_atomic_ld32 (const ddsrt_atomic_uint32_t *x) { return x->v; }
#if DDSRT_HAVE_ATOMIC64
inline uint64_t ddsrt_atomic_ld64 (const ddsrt_atomic_uint64_t *x) { return x->v; }
#endif
inline uintptr_t ddsrt_atomic_ldptr (const ddsrt_atomic_uintptr_t *x) { return x->v; }
inline void *ddsrt_atomic_ldvoidp (const ddsrt_atomic_voidp_t *x) { return (void *) ddsrt_atomic_ldptr (x); }

inline void ddsrt_atomic_st32 (ddsrt_atomic_uint32_t *x, uint32_t v) { x->v = v; }
#if DDSRT_HAVE_ATOMIC64
inline void ddsrt_atomic_st64 (ddsrt_atomic_uint64_t *x, uint64_t v) { x->v = v; }
#endif
inline void ddsrt_atomic_stptr (ddsrt_atomic_uintptr_t *x, uintptr_t v) { x->v = v; }
inline void ddsrt_atomic_stvoidp (ddsrt_atomic_voidp_t *x, void *v) { ddsrt_atomic_stptr (x, (uintptr_t) v); }

/* CAS */

inline int ddsrt_atomic_cas32 (ddsrt_atomic_uint32_t *x, uint32_t exp, uint32_t des) {
  return DDSRT_ATOMIC_OP32 (InterlockedCompareExchange, &x->v, des, exp) == exp;
}
#if DDSRT_HAVE_ATOMIC64
inline int ddsrt_atomic_cas64 (ddsrt_atomic_uint64_t *x, uint64_t exp, uint64_t des) {
  return DDSRT_ATOMIC_OP64 (InterlockedCompareExchange, &x->v, des, exp) == exp;
}
#endif
inline int ddsrt_atomic_casptr (ddsrt_atomic_uintptr_t *x, uintptr_t exp, uintptr_t des) {
  return DDSRT_ATOMIC_PTROP (InterlockedCompareExchange, &x->v, des, exp) == exp;
}
inline int ddsrt_atomic_casvoidp (ddsrt_atomic_voidp_t *x, void *exp, void *des) {
  return ddsrt_atomic_casptr ((ddsrt_atomic_uintptr_t *) x, (uintptr_t) exp, (uintptr_t) des);
}

/* INC */

inline void ddsrt_atomic_inc32 (ddsrt_atomic_uint32_t *x) {
  DDSRT_ATOMIC_OP32 (InterlockedIncrement, &x->v);
}
#if DDSRT_HAVE_ATOMIC64
inline void ddsrt_atomic_inc64 (ddsrt_atomic_uint64_t *x) {
  DDSRT_ATOMIC_OP64 (InterlockedIncrement, &x->v);
}
#endif
inline void ddsrt_atomic_incptr (ddsrt_atomic_uintptr_t *x) {
  DDSRT_ATOMIC_PTROP (InterlockedIncrement, &x->v);
}
inline uint32_t ddsrt_atomic_inc32_ov (ddsrt_atomic_uint32_t *x) {
  return DDSRT_ATOMIC_OP32 (InterlockedIncrement, &x->v) - 1;
}
inline uint32_t ddsrt_atomic_inc32_nv (ddsrt_atomic_uint32_t *x) {
  return DDSRT_ATOMIC_OP32 (InterlockedIncrement, &x->v);
}
#if DDSRT_HAVE_ATOMIC64
inline uint64_t ddsrt_atomic_inc64_nv (ddsrt_atomic_uint64_t *x) {
  return DDSRT_ATOMIC_OP64 (InterlockedIncrement, &x->v);
}
#endif
inline uintptr_t ddsrt_atomic_incptr_nv (ddsrt_atomic_uintptr_t *x) {
  return DDSRT_ATOMIC_PTROP (InterlockedIncrement, &x->v);
}

/* DEC */

inline void ddsrt_atomic_dec32 (ddsrt_atomic_uint32_t *x) {
  DDSRT_ATOMIC_OP32 (InterlockedDecrement, &x->v);
}
#if DDSRT_HAVE_ATOMIC64
inline void ddsrt_atomic_dec64 (ddsrt_atomic_uint64_t *x) {
  DDSRT_ATOMIC_OP64 (InterlockedDecrement, &x->v);
}
#endif
inline void ddsrt_atomic_decptr (ddsrt_atomic_uintptr_t *x) {
  DDSRT_ATOMIC_PTROP (InterlockedDecrement, &x->v);
}
inline uint32_t ddsrt_atomic_dec32_nv (ddsrt_atomic_uint32_t *x) {
  return DDSRT_ATOMIC_OP32 (InterlockedDecrement, &x->v);
}
#if DDSRT_HAVE_ATOMIC64
inline uint64_t ddsrt_atomic_dec64_nv (ddsrt_atomic_uint64_t *x) {
  return DDSRT_ATOMIC_OP64 (InterlockedDecrement, &x->v);
}
#endif
inline uintptr_t ddsrt_atomic_decptr_nv (ddsrt_atomic_uintptr_t *x) {
  return DDSRT_ATOMIC_PTROP (InterlockedDecrement, &x->v);
}
inline uint32_t ddsrt_atomic_dec32_ov (ddsrt_atomic_uint32_t *x) {
  return DDSRT_ATOMIC_OP32 (InterlockedDecrement, &x->v) + 1;
}
#if DDSRT_HAVE_ATOMIC64
inline uint64_t ddsrt_atomic_dec64_ov (ddsrt_atomic_uint64_t *x) {
  return DDSRT_ATOMIC_OP64 (InterlockedDecrement, &x->v) + 1;
}
#endif
inline uintptr_t ddsrt_atomic_decptr_ov (ddsrt_atomic_uintptr_t *x) {
  return DDSRT_ATOMIC_PTROP (InterlockedDecrement, &x->v) + 1;
}

/* ADD */

inline void ddsrt_atomic_add32 (ddsrt_atomic_uint32_t *x, uint32_t v) {
  DDSRT_ATOMIC_OP32 (InterlockedExchangeAdd, &x->v, v);
}
#if DDSRT_HAVE_ATOMIC64
inline void ddsrt_atomic_add64 (ddsrt_atomic_uint64_t *x, uint64_t v) {
  DDSRT_ATOMIC_OP64 (InterlockedExchangeAdd, &x->v, v);
}
#endif
inline void ddsrt_atomic_addptr (ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  DDSRT_ATOMIC_PTROP (InterlockedExchangeAdd, &x->v, v);
}
inline void ddsrt_atomic_addvoidp (ddsrt_atomic_voidp_t *x, ptrdiff_t v) {
  ddsrt_atomic_addptr ((ddsrt_atomic_uintptr_t *) x, (uintptr_t) v);
}
inline uint32_t ddsrt_atomic_add32_ov (ddsrt_atomic_uint32_t *x, uint32_t v) {
  return DDSRT_ATOMIC_OP32 (InterlockedExchangeAdd, &x->v, v);
}
inline uint32_t ddsrt_atomic_add32_nv (ddsrt_atomic_uint32_t *x, uint32_t v) {
  return DDSRT_ATOMIC_OP32 (InterlockedExchangeAdd, &x->v, v) + v;
}
#if DDSRT_HAVE_ATOMIC64
inline uint64_t ddsrt_atomic_add64_nv (ddsrt_atomic_uint64_t *x, uint64_t v) {
  return DDSRT_ATOMIC_OP64 (InterlockedExchangeAdd, &x->v, v) + v;
}
#endif
inline uintptr_t ddsrt_atomic_addptr_nv (ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  return DDSRT_ATOMIC_PTROP (InterlockedExchangeAdd, &x->v, v) + v;
}
inline void *ddsrt_atomic_addvoidp_nv (ddsrt_atomic_voidp_t *x, ptrdiff_t v) {
  return (void *) ddsrt_atomic_addptr_nv ((ddsrt_atomic_uintptr_t *) x, (uintptr_t) v);
}

/* SUB */

inline void ddsrt_atomic_sub32 (ddsrt_atomic_uint32_t *x, uint32_t v) {
  /* disable unary minus applied to unsigned type, result still unsigned */
  DDSRT_WARNING_MSVC_OFF(4146)
  DDSRT_ATOMIC_OP32 (InterlockedExchangeAdd, &x->v, -v);
  DDSRT_WARNING_MSVC_ON(4146)
}
#if DDSRT_HAVE_ATOMIC64
inline void ddsrt_atomic_sub64 (ddsrt_atomic_uint64_t *x, uint64_t v) {
  /* disable unary minus applied to unsigned type, result still unsigned */
  DDSRT_WARNING_MSVC_OFF(4146)
  DDSRT_ATOMIC_OP64 (InterlockedExchangeAdd, &x->v, -v);
  DDSRT_WARNING_MSVC_ON(4146)
}
#endif
inline void ddsrt_atomic_subptr (ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  /* disable unary minus applied to unsigned type, result still unsigned */
  DDSRT_WARNING_MSVC_OFF(4146)
  DDSRT_ATOMIC_PTROP (InterlockedExchangeAdd, &x->v, -v);
  DDSRT_WARNING_MSVC_ON(4146)
}
inline void ddsrt_atomic_subvoidp (ddsrt_atomic_voidp_t *x, ptrdiff_t v) {
  ddsrt_atomic_subptr ((ddsrt_atomic_uintptr_t *) x, (uintptr_t) v);
}
inline uint32_t ddsrt_atomic_sub32_ov (ddsrt_atomic_uint32_t *x, uint32_t v) {
  /* disable unary minus applied to unsigned type, result still unsigned */
  DDSRT_WARNING_MSVC_OFF(4146)
  return DDSRT_ATOMIC_OP32 (InterlockedExchangeAdd, &x->v, -v);
  DDSRT_WARNING_MSVC_ON(4146)
}
inline uint32_t ddsrt_atomic_sub32_nv (ddsrt_atomic_uint32_t *x, uint32_t v) {
  /* disable unary minus applied to unsigned type, result still unsigned */
  DDSRT_WARNING_MSVC_OFF(4146)
  return DDSRT_ATOMIC_OP32 (InterlockedExchangeAdd, &x->v, -v) - v;
  DDSRT_WARNING_MSVC_ON(4146)
}
#if DDSRT_HAVE_ATOMIC64
inline uint64_t ddsrt_atomic_sub64_nv (ddsrt_atomic_uint64_t *x, uint64_t v) {
  /* disable unary minus applied to unsigned type, result still unsigned */
  DDSRT_WARNING_MSVC_OFF(4146)
  return DDSRT_ATOMIC_OP64 (InterlockedExchangeAdd, &x->v, -v) - v;
  DDSRT_WARNING_MSVC_ON(4146)
}
#endif
inline uintptr_t ddsrt_atomic_subptr_nv (ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  /* disable unary minus applied to unsigned type, result still unsigned */
  DDSRT_WARNING_MSVC_OFF(4146)
  return DDSRT_ATOMIC_PTROP (InterlockedExchangeAdd, &x->v, -v) - v;
  DDSRT_WARNING_MSVC_ON(4146)
}
inline void *ddsrt_atomic_subvoidp_nv (ddsrt_atomic_voidp_t *x, ptrdiff_t v) {
  return (void *) ddsrt_atomic_subptr_nv ((ddsrt_atomic_uintptr_t *) x, (uintptr_t) v);
}

/* AND */

inline void ddsrt_atomic_and32 (ddsrt_atomic_uint32_t *x, uint32_t v) {
  DDSRT_ATOMIC_OP32 (InterlockedAnd, &x->v, v);
}
#if DDSRT_HAVE_ATOMIC64
inline void ddsrt_atomic_and64 (ddsrt_atomic_uint64_t *x, uint64_t v) {
  DDSRT_ATOMIC_OP64 (InterlockedAnd, &x->v, v);
}
#endif
inline void ddsrt_atomic_andptr (ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  DDSRT_ATOMIC_PTROP (InterlockedAnd, &x->v, v);
}
inline uint32_t ddsrt_atomic_and32_ov (ddsrt_atomic_uint32_t *x, uint32_t v) {
  return DDSRT_ATOMIC_OP32 (InterlockedAnd, &x->v, v);
}
#if DDSRT_HAVE_ATOMIC64
inline uint64_t ddsrt_atomic_and64_ov (ddsrt_atomic_uint64_t *x, uint64_t v) {
  return DDSRT_ATOMIC_OP64 (InterlockedAnd, &x->v, v);
}
#endif
inline uintptr_t ddsrt_atomic_andptr_ov (ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  return DDSRT_ATOMIC_PTROP (InterlockedAnd, &x->v, v);
}
inline uint32_t ddsrt_atomic_and32_nv (ddsrt_atomic_uint32_t *x, uint32_t v) {
  return DDSRT_ATOMIC_OP32 (InterlockedAnd, &x->v, v) & v;
}
#if DDSRT_HAVE_ATOMIC64
inline uint64_t ddsrt_atomic_and64_nv (ddsrt_atomic_uint64_t *x, uint64_t v) {
  return DDSRT_ATOMIC_OP64 (InterlockedAnd, &x->v, v) & v;
}
#endif
inline uintptr_t ddsrt_atomic_andptr_nv (ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  return DDSRT_ATOMIC_PTROP (InterlockedAnd, &x->v, v) & v;
}

/* OR */

inline void ddsrt_atomic_or32 (ddsrt_atomic_uint32_t *x, uint32_t v) {
  DDSRT_ATOMIC_OP32 (InterlockedOr, &x->v, v);
}
#if DDSRT_HAVE_ATOMIC64
inline void ddsrt_atomic_or64 (ddsrt_atomic_uint64_t *x, uint64_t v) {
  DDSRT_ATOMIC_OP64 (InterlockedOr, &x->v, v);
}
#endif
inline void ddsrt_atomic_orptr (ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  DDSRT_ATOMIC_PTROP (InterlockedOr, &x->v, v);
}
inline uint32_t ddsrt_atomic_or32_ov (ddsrt_atomic_uint32_t *x, uint32_t v) {
  return DDSRT_ATOMIC_OP32 (InterlockedOr, &x->v, v);
}
#if DDSRT_HAVE_ATOMIC64
inline uint64_t ddsrt_atomic_or64_ov (ddsrt_atomic_uint64_t *x, uint64_t v) {
  return DDSRT_ATOMIC_OP64 (InterlockedOr, &x->v, v);
}
#endif
inline uintptr_t ddsrt_atomic_orptr_ov (ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  return DDSRT_ATOMIC_PTROP (InterlockedOr, &x->v, v);
}
inline uint32_t ddsrt_atomic_or32_nv (ddsrt_atomic_uint32_t *x, uint32_t v) {
  return DDSRT_ATOMIC_OP32 (InterlockedOr, &x->v, v) | v;
}
#if DDSRT_HAVE_ATOMIC64
inline uint64_t ddsrt_atomic_or64_nv (ddsrt_atomic_uint64_t *x, uint64_t v) {
  return DDSRT_ATOMIC_OP64 (InterlockedOr, &x->v, v) | v;
}
#endif
inline uintptr_t ddsrt_atomic_orptr_nv (ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  return DDSRT_ATOMIC_PTROP (InterlockedOr, &x->v, v) | v;
}

/* FENCES */

inline void ddsrt_atomic_fence (void) {
  /* 28113: accessing a local variable tmp via an Interlocked
     function: This is an unusual usage which could be reconsidered.
     It is too heavyweight, true, but it does the trick. */
  DDSRT_WARNING_MSVC_OFF(28113)
  volatile LONG tmp = 0;
  InterlockedExchange (&tmp, 0);
  DDSRT_WARNING_MSVC_ON(28113)
}
inline void ddsrt_atomic_fence_ldld (void) {
#if !(defined _M_IX86 || defined _M_X64)
  ddsrt_atomic_fence ();
#endif
}
inline void ddsrt_atomic_fence_stst (void) {
#if !(defined _M_IX86 || defined _M_X64)
  ddsrt_atomic_fence ();
#endif
}
inline void ddsrt_atomic_fence_acq (void) {
  ddsrt_atomic_fence ();
}
inline void ddsrt_atomic_fence_rel (void) {
  ddsrt_atomic_fence ();
}

#undef DDSRT_ATOMIC_PTROP

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_ATOMICS_MSVC_H */
