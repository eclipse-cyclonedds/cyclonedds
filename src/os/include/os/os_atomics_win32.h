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
/* x86 has supported 64-bit CAS for a long time, so Windows ought to
   provide all the interlocked operations for 64-bit operands on x86
   platforms, but it doesn't. */

#if defined OS_64BIT
#define OS_ATOMIC64_SUPPORT 1
#else
#define OS_ATOMIC64_SUPPORT 0
#endif

#if defined OS_64BIT
#define OS_ATOMIC_PTROP(name) name##64
#else
#define OS_ATOMIC_PTROP(name) name
#endif

/* Experience is that WinCE doesn't provide these, and that neither does VS8 */
#if ! defined OS_WINCE_DEFS_H && _MSC_VER > 1400
#if defined _M_IX86 || defined _M_ARM
#define OS_ATOMIC_INTERLOCKED_AND _InterlockedAnd
#define OS_ATOMIC_INTERLOCKED_OR _InterlockedOr
#define OS_ATOMIC_INTERLOCKED_AND64 _InterlockedAnd64
#define OS_ATOMIC_INTERLOCKED_OR64 _InterlockedOr64
#else
#define OS_ATOMIC_INTERLOCKED_AND InterlockedAnd
#define OS_ATOMIC_INTERLOCKED_OR InterlockedOr
#define OS_ATOMIC_INTERLOCKED_AND64 InterlockedAnd64
#define OS_ATOMIC_INTERLOCKED_OR64 InterlockedOr64
#endif
#endif

/* LD, ST */

inline uint32_t os_atomic_ld32 (const volatile os_atomic_uint32_t *x) { return x->v; }
#if OS_ATOMIC64_SUPPORT
inline uint64_t os_atomic_ld64 (const volatile os_atomic_uint64_t *x) { return x->v; }
#endif
inline uintptr_t os_atomic_ldptr (const volatile os_atomic_uintptr_t *x) { return x->v; }
inline void *os_atomic_ldvoidp (const volatile os_atomic_voidp_t *x) { return (void *) os_atomic_ldptr (x); }

inline void os_atomic_st32 (volatile os_atomic_uint32_t *x, uint32_t v) { x->v = v; }
#if OS_ATOMIC64_SUPPORT
inline void os_atomic_st64 (volatile os_atomic_uint64_t *x, uint64_t v) { x->v = v; }
#endif
inline void os_atomic_stptr (volatile os_atomic_uintptr_t *x, uintptr_t v) { x->v = v; }
inline void os_atomic_stvoidp (volatile os_atomic_voidp_t *x, void *v) { os_atomic_stptr (x, (uintptr_t) v); }

/* CAS */

inline int os_atomic_cas32 (volatile os_atomic_uint32_t *x, uint32_t exp, uint32_t des) {
  return InterlockedCompareExchange (&x->v, des, exp) == exp;
}
#if OS_ATOMIC64_SUPPORT
inline int os_atomic_cas64 (volatile os_atomic_uint64_t *x, uint64_t exp, uint64_t des) {
  return InterlockedCompareExchange64 (&x->v, des, exp) == exp;
}
#endif
inline int os_atomic_casptr (volatile os_atomic_uintptr_t *x, uintptr_t exp, uintptr_t des) {
  return OS_ATOMIC_PTROP (InterlockedCompareExchange) (&x->v, des, exp) == exp;
}
inline int os_atomic_casvoidp (volatile os_atomic_voidp_t *x, void *exp, void *des) {
  return os_atomic_casptr ((volatile os_atomic_uintptr_t *) x, (uintptr_t) exp, (uintptr_t) des);
}

/* INC */

inline void os_atomic_inc32 (volatile os_atomic_uint32_t *x) {
  InterlockedIncrement (&x->v);
}
#if OS_ATOMIC64_SUPPORT
inline void os_atomic_inc64 (volatile os_atomic_uint64_t *x) {
  InterlockedIncrement64 (&x->v);
}
#endif
inline void os_atomic_incptr (volatile os_atomic_uintptr_t *x) {
  OS_ATOMIC_PTROP (InterlockedIncrement) (&x->v);
}
inline uint32_t os_atomic_inc32_nv (volatile os_atomic_uint32_t *x) {
  return InterlockedIncrement (&x->v);
}
#if OS_ATOMIC64_SUPPORT
inline uint64_t os_atomic_inc64_nv (volatile os_atomic_uint64_t *x) {
  return InterlockedIncrement64 (&x->v);
}
#endif
inline uintptr_t os_atomic_incptr_nv (volatile os_atomic_uintptr_t *x) {
  return OS_ATOMIC_PTROP (InterlockedIncrement) (&x->v);
}

/* DEC */

inline void os_atomic_dec32 (volatile os_atomic_uint32_t *x) {
  InterlockedDecrement (&x->v);
}
#if OS_ATOMIC64_SUPPORT
inline void os_atomic_dec64 (volatile os_atomic_uint64_t *x) {
  InterlockedDecrement64 (&x->v);
}
#endif
inline void os_atomic_decptr (volatile os_atomic_uintptr_t *x) {
  OS_ATOMIC_PTROP (InterlockedDecrement) (&x->v);
}
inline uint32_t os_atomic_dec32_nv (volatile os_atomic_uint32_t *x) {
  return InterlockedDecrement (&x->v);
}
#if OS_ATOMIC64_SUPPORT
inline uint64_t os_atomic_dec64_nv (volatile os_atomic_uint64_t *x) {
  return InterlockedDecrement64 (&x->v);
}
#endif
inline uintptr_t os_atomic_decptr_nv (volatile os_atomic_uintptr_t *x) {
  return OS_ATOMIC_PTROP (InterlockedDecrement) (&x->v);
}
inline uint32_t os_atomic_dec32_ov (volatile os_atomic_uint32_t *x) {
  return InterlockedDecrement (&x->v) + 1;
}
#if OS_ATOMIC64_SUPPORT
inline uint64_t os_atomic_dec64_ov (volatile os_atomic_uint64_t *x) {
  return InterlockedDecrement64 (&x->v) + 1;
}
#endif
inline uintptr_t os_atomic_decptr_ov (volatile os_atomic_uintptr_t *x) {
  return OS_ATOMIC_PTROP (InterlockedDecrement) (&x->v) + 1;
}

/* ADD */

inline void os_atomic_add32 (volatile os_atomic_uint32_t *x, uint32_t v) {
  InterlockedExchangeAdd (&x->v, v);
}
#if OS_ATOMIC64_SUPPORT
inline void os_atomic_add64 (volatile os_atomic_uint64_t *x, uint64_t v) {
  InterlockedExchangeAdd64 (&x->v, v);
}
#endif
inline void os_atomic_addptr (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  OS_ATOMIC_PTROP (InterlockedExchangeAdd) (&x->v, v);
}
inline void os_atomic_addvoidp (volatile os_atomic_voidp_t *x, ptrdiff_t v) {
  os_atomic_addptr ((volatile os_atomic_uintptr_t *) x, (uintptr_t) v);
}
inline uint32_t os_atomic_add32_nv (volatile os_atomic_uint32_t *x, uint32_t v) {
  return InterlockedExchangeAdd (&x->v, v) + v;
}
#if OS_ATOMIC64_SUPPORT
inline uint64_t os_atomic_add64_nv (volatile os_atomic_uint64_t *x, uint64_t v) {
  return InterlockedExchangeAdd64 (&x->v, v) + v;
}
#endif
inline uintptr_t os_atomic_addptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  return OS_ATOMIC_PTROP (InterlockedExchangeAdd) (&x->v, v) + v;
}
inline void *os_atomic_addvoidp_nv (volatile os_atomic_voidp_t *x, ptrdiff_t v) {
  return (void *) os_atomic_addptr_nv ((volatile os_atomic_uintptr_t *) x, (uintptr_t) v);
}

/* SUB */

inline void os_atomic_sub32 (volatile os_atomic_uint32_t *x, uint32_t v) {
  /* disable unary minus applied to unsigned type, result still unsigned */
#pragma warning (push)
#pragma warning (disable: 4146)
  InterlockedExchangeAdd (&x->v, -v);
#pragma warning (pop)
}
#if OS_ATOMIC64_SUPPORT
inline void os_atomic_sub64 (volatile os_atomic_uint64_t *x, uint64_t v) {
  /* disable unary minus applied to unsigned type, result still unsigned */
#pragma warning (push)
#pragma warning (disable: 4146)
  InterlockedExchangeAdd64 (&x->v, -v);
#pragma warning (pop)
}
#endif
inline void os_atomic_subptr (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  /* disable unary minus applied to unsigned type, result still unsigned */
#pragma warning (push)
#pragma warning (disable: 4146)
  OS_ATOMIC_PTROP (InterlockedExchangeAdd) (&x->v, -v);
#pragma warning (pop)
}
inline void os_atomic_subvoidp (volatile os_atomic_voidp_t *x, ptrdiff_t v) {
  os_atomic_subptr ((volatile os_atomic_uintptr_t *) x, (uintptr_t) v);
}
inline uint32_t os_atomic_sub32_nv (volatile os_atomic_uint32_t *x, uint32_t v) {
  /* disable unary minus applied to unsigned type, result still unsigned */
#pragma warning (push)
#pragma warning (disable: 4146)
  return InterlockedExchangeAdd (&x->v, -v) - v;
#pragma warning (pop)
}
#if OS_ATOMIC64_SUPPORT
inline uint64_t os_atomic_sub64_nv (volatile os_atomic_uint64_t *x, uint64_t v) {
  /* disable unary minus applied to unsigned type, result still unsigned */
#pragma warning (push)
#pragma warning (disable: 4146)
  return InterlockedExchangeAdd64 (&x->v, -v) - v;
#pragma warning (pop)
}
#endif
inline uintptr_t os_atomic_subptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  /* disable unary minus applied to unsigned type, result still unsigned */
#pragma warning (push)
#pragma warning (disable: 4146)
  return OS_ATOMIC_PTROP (InterlockedExchangeAdd) (&x->v, -v) - v;
#pragma warning (pop)
}
inline void *os_atomic_subvoidp_nv (volatile os_atomic_voidp_t *x, ptrdiff_t v) {
  return (void *) os_atomic_subptr_nv ((volatile os_atomic_uintptr_t *) x, (uintptr_t) v);
}

/* AND */

#if defined OS_ATOMIC_INTERLOCKED_AND

inline void os_atomic_and32 (volatile os_atomic_uint32_t *x, uint32_t v) {
  OS_ATOMIC_INTERLOCKED_AND (&x->v, v);
}
#if OS_ATOMIC64_SUPPORT
inline void os_atomic_and64 (volatile os_atomic_uint64_t *x, uint64_t v) {
  InterlockedAnd64 (&x->v, v);
}
#endif
inline void os_atomic_andptr (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  OS_ATOMIC_PTROP (OS_ATOMIC_INTERLOCKED_AND) (&x->v, v);
}
inline uint32_t os_atomic_and32_ov (volatile os_atomic_uint32_t *x, uint32_t v) {
  return OS_ATOMIC_INTERLOCKED_AND (&x->v, v);
}
#if OS_ATOMIC64_SUPPORT
inline uint64_t os_atomic_and64_ov (volatile os_atomic_uint64_t *x, uint64_t v) {
  return InterlockedAnd64 (&x->v, v);
}
#endif
inline uintptr_t os_atomic_andptr_ov (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  return OS_ATOMIC_PTROP (OS_ATOMIC_INTERLOCKED_AND) (&x->v, v);
}
inline uint32_t os_atomic_and32_nv (volatile os_atomic_uint32_t *x, uint32_t v) {
  return OS_ATOMIC_INTERLOCKED_AND (&x->v, v) & v;
}
#if OS_ATOMIC64_SUPPORT
inline uint64_t os_atomic_and64_nv (volatile os_atomic_uint64_t *x, uint64_t v) {
  return InterlockedAnd64 (&x->v, v) & v;
}
#endif
inline uintptr_t os_atomic_andptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  return OS_ATOMIC_PTROP (OS_ATOMIC_INTERLOCKED_AND) (&x->v, v) & v;
}

#else /* synthesize via CAS */

inline uint32_t os_atomic_and32_ov (volatile os_atomic_uint32_t *x, uint32_t v) {
  uint64_t oldval, newval;
  do { oldval = x->v; newval = oldval & v; } while (!os_atomic_cas32 (x, oldval, newval));
  return oldval;
}
#if OS_ATOMIC64_SUPPORT
inline uint64_t os_atomic_and64_ov (volatile os_atomic_uint64_t *x, uint64_t v) {
  uint64_t oldval, newval;
  do { oldval = x->v; newval = oldval & v; } while (!os_atomic_cas64 (x, oldval, newval));
  return oldval;
}
#endif
inline uintptr_t os_atomic_andptr_ov (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  uintptr_t oldval, newval;
  do { oldval = x->v; newval = oldval & v; } while (!os_atomic_casptr (x, oldval, newval));
  return oldval;
}
inline uint32_t os_atomic_and32_nv (volatile os_atomic_uint32_t *x, uint32_t v) {
  uint32_t oldval, newval;
  do { oldval = x->v; newval = oldval & v; } while (!os_atomic_cas32 (x, oldval, newval));
  return newval;
}
#if OS_ATOMIC64_SUPPORT
inline uint64_t os_atomic_and64_nv (volatile os_atomic_uint64_t *x, uint64_t v) {
  uint64_t oldval, newval;
  do { oldval = x->v; newval = oldval & v; } while (!os_atomic_cas64 (x, oldval, newval));
  return newval;
}
#endif
inline uintptr_t os_atomic_andptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  uintptr_t oldval, newval;
  do { oldval = x->v; newval = oldval & v; } while (!os_atomic_casptr (x, oldval, newval));
  return newval;
}
inline void os_atomic_and32 (volatile os_atomic_uint32_t *x, uint32_t v) {
  (void) os_atomic_and32_nv (x, v);
}
#if OS_ATOMIC64_SUPPORT
inline void os_atomic_and64 (volatile os_atomic_uint64_t *x, uint64_t v) {
  (void) os_atomic_and64_nv (x, v);
}
#endif
inline void os_atomic_andptr (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  (void) os_atomic_andptr_nv (x, v);
}

#endif

/* OR */

#if defined OS_ATOMIC_INTERLOCKED_OR

inline void os_atomic_or32 (volatile os_atomic_uint32_t *x, uint32_t v) {
  OS_ATOMIC_INTERLOCKED_OR (&x->v, v);
}
#if OS_ATOMIC64_SUPPORT
inline void os_atomic_or64 (volatile os_atomic_uint64_t *x, uint64_t v) {
  InterlockedOr64 (&x->v, v);
}
#endif
inline void os_atomic_orptr (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  OS_ATOMIC_PTROP (OS_ATOMIC_INTERLOCKED_OR) (&x->v, v);
}
inline uint32_t os_atomic_or32_ov (volatile os_atomic_uint32_t *x, uint32_t v) {
  return OS_ATOMIC_INTERLOCKED_OR (&x->v, v);
}
#if OS_ATOMIC64_SUPPORT
inline uint64_t os_atomic_or64_ov (volatile os_atomic_uint64_t *x, uint64_t v) {
  return InterlockedOr64 (&x->v, v);
}
#endif
inline uintptr_t os_atomic_orptr_ov (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  return OS_ATOMIC_PTROP (OS_ATOMIC_INTERLOCKED_OR) (&x->v, v);
}
inline uint32_t os_atomic_or32_nv (volatile os_atomic_uint32_t *x, uint32_t v) {
  return OS_ATOMIC_INTERLOCKED_OR (&x->v, v) | v;
}
#if OS_ATOMIC64_SUPPORT
inline uint64_t os_atomic_or64_nv (volatile os_atomic_uint64_t *x, uint64_t v) {
  return InterlockedOr64 (&x->v, v) | v;
}
#endif
inline uintptr_t os_atomic_orptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  return OS_ATOMIC_PTROP (OS_ATOMIC_INTERLOCKED_OR) (&x->v, v) | v;
}

#else /* synthesize via CAS */

inline uint32_t os_atomic_or32_ov (volatile os_atomic_uint32_t *x, uint32_t v) {
  uint32_t oldval, newval;
  do { oldval = x->v; newval = oldval | v; } while (!os_atomic_cas32 (x, oldval, newval));
  return oldval;
}
#if OS_ATOMIC64_SUPPORT
inline uint64_t os_atomic_or64_ov (volatile os_atomic_uint64_t *x, uint64_t v) {
  uint64_t oldval, newval;
  do { oldval = x->v; newval = oldval | v; } while (!os_atomic_cas64 (x, oldval, newval));
  return oldval;
}
#endif
inline uintptr_t os_atomic_orptr_ov (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  uintptr_t oldval, newval;
  do { oldval = x->v; newval = oldval | v; } while (!os_atomic_casptr (x, oldval, newval));
  return oldval;
}
inline uint32_t os_atomic_or32_nv (volatile os_atomic_uint32_t *x, uint32_t v) {
  uint32_t oldval, newval;
  do { oldval = x->v; newval = oldval | v; } while (!os_atomic_cas32 (x, oldval, newval));
  return newval;
}
#if OS_ATOMIC64_SUPPORT
inline uint64_t os_atomic_or64_nv (volatile os_atomic_uint64_t *x, uint64_t v) {
  uint64_t oldval, newval;
  do { oldval = x->v; newval = oldval | v; } while (!os_atomic_cas64 (x, oldval, newval));
  return newval;
}
#endif
inline uintptr_t os_atomic_orptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  uintptr_t oldval, newval;
  do { oldval = x->v; newval = oldval | v; } while (!os_atomic_casptr (x, oldval, newval));
  return newval;
}
inline void os_atomic_or32 (volatile os_atomic_uint32_t *x, uint32_t v) {
  (void) os_atomic_or32_nv (x, v);
}
#if OS_ATOMIC64_SUPPORT
inline void os_atomic_or64 (volatile os_atomic_uint64_t *x, uint64_t v) {
  (void) os_atomic_or64_nv (x, v);
}
#endif
inline void os_atomic_orptr (volatile os_atomic_uintptr_t *x, uintptr_t v) {
  (void) os_atomic_orptr_nv (x, v);
}

#endif

/* FENCES */

inline void os_atomic_fence (void) {
  /* 28113: accessing a local variable tmp via an Interlocked
     function: This is an unusual usage which could be reconsidered.
     It is too heavyweight, true, but it does the trick. */
#pragma warning (push)
#pragma warning (disable: 28113)
  volatile LONG tmp = 0;
  InterlockedExchange (&tmp, 0);
#pragma warning (pop)
}
inline void os_atomic_fence_ldld (void) {
#if !(defined _M_IX86 || defined _M_X64)
  os_atomic_fence ();
#endif
}
inline void os_atomic_fence_acq (void) {
  os_atomic_fence ();
}
inline void os_atomic_fence_rel (void) {
  os_atomic_fence ();
}

#undef OS_ATOMIC_INTERLOCKED_AND
#undef OS_ATOMIC_INTERLOCKED_OR
#undef OS_ATOMIC_INTERLOCKED_AND64
#undef OS_ATOMIC_INTERLOCKED_OR64

#undef OS_ATOMIC_PTROP
#define OS_ATOMIC_SUPPORT 1
