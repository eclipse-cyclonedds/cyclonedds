// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

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

/**
 * @file msvc.h
 * @brief Contains functions and types for atomic operations on various types for msvc.
 *
 * This header file defines functions for atomic operations on uint32_t,
 * uint64_t, uintptr_t, and void* types.
 */

/* LD, ST */

/**
 * @brief Fetch the value of a 32-bit unsigned integer atomically
 *
 * @param[in] x Pointer to the variable to fetch the value of
 *
 * @return The value of the fetched variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_ld32 (const volatile ddsrt_atomic_uint32_t *x) { return x->v; }
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Fetch the value of a 64-bit unsigned integer atomically
 *
 * @param[in] x Pointer to the variable to fetch the value of
 *
 * @return The value of the fetched variable
 */
DDS_INLINE_EXPORT inline uint64_t ddsrt_atomic_ld64 (const volatile ddsrt_atomic_uint64_t *x) { return x->v; }
#endif

/**
 * @brief Fetch the value of an uintptr atomically
 *
 * @param[in] x Pointer to the variable to fetch the value of
 *
 * @return The value of the fetched variable
 */
DDS_INLINE_EXPORT inline uintptr_t ddsrt_atomic_ldptr (const volatile ddsrt_atomic_uintptr_t *x) { return x->v; }

/**
 * @brief Fetch the value of a void pointer atomically
 *
 * @param[in] x Pointer to the variable to fetch the value of
 *
 * @return The value of the fetched variable
 */
DDS_INLINE_EXPORT inline void *ddsrt_atomic_ldvoidp (const volatile ddsrt_atomic_voidp_t *x) { return (void *) ddsrt_atomic_ldptr (x); }

/**
 * @brief Assign a 32-bit unsigned integer atomically
 *
 * @param[out] x Pointer to the variable to update
 * @param[in] v The new value to assign to the variable
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_st32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) { x->v = v; }
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Assign a 64-bit unsigned integer atomically
 *
 * @param[out] x Pointer to the variable to update
 * @param[in] v The new value to assign to the variable
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_st64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v) { x->v = v; }
#endif

/**
 * @brief Assign an uintptr atomically
 *
 * @param[out] x Pointer to the variable to update
 * @param[in] v The new value to assign to the variable
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_stptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) { x->v = v; }

/**
 * @brief Assign a void pointer atomically
 *
 * @param[out] x Pointer to the variable to update
 * @param[in] v The new value to assign to the variable
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_stvoidp (volatile ddsrt_atomic_voidp_t *x, void *v) { ddsrt_atomic_stptr (x, (uintptr_t) v); }

/* CAS */

/**
 * @brief Compare and swap a 32-bit unsigned integer atomically
 * 
 * Assigns des to x iff x == exp
 *
 * @param[in,out] x Pointer to the variable whose value to swap
 * @param[in] exp the value to test x for
 * @param[in] des the value to assign to the variable
 * 
 * @return int meant to be interpreted as bool (swapped: 1; not swapped: 0)
 */
DDS_INLINE_EXPORT inline int ddsrt_atomic_cas32 (volatile ddsrt_atomic_uint32_t *x, uint32_t exp, uint32_t des) {
  return DDSRT_ATOMIC_OP32 (InterlockedCompareExchange, &x->v, des, exp) == exp;
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Compare and swap a 64-bit unsigned integer atomically
 * 
 * Assigns des to x iff x == exp
 *
 * @param[in,out] x Pointer to the variable whose value to swap
 * @param[in] exp the value to test x for
 * @param[in] des the value to assign to the variable
 * 
 * @return int meant to be interpreted as bool (swapped: 1; not swapped: 0)
 */
DDS_INLINE_EXPORT inline int ddsrt_atomic_cas64 (volatile ddsrt_atomic_uint64_t *x, uint64_t exp, uint64_t des) {
  return DDSRT_ATOMIC_OP64 (InterlockedCompareExchange, &x->v, des, exp) == exp;
}
#endif

/**
 * @brief Compare and swap an uintptr atomically
 * 
 * Assigns des to x iff x == exp
 *
 * @param[in,out] x Pointer to the variable whose value to swap
 * @param[in] exp the value to test x for
 * @param[in] des the value to assign to the variable
 * 
 * @return int meant to be interpreted as bool (swapped: 1; not swapped: 0)
 */
DDS_INLINE_EXPORT inline int ddsrt_atomic_casptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t exp, uintptr_t des) {
  return DDSRT_ATOMIC_PTROP (InterlockedCompareExchange, &x->v, des, exp) == exp;
}

/**
 * @brief Compare and swap a void pointer atomically
 * 
 * Assigns des to x iff x == exp
 *
 * @param[in,out] x Pointer to the variable whose value to swap
 * @param[in] exp the value to test x for
 * @param[in] des the value to assign to the variable
 * 
 * @return int meant to be interpreted as bool (swapped: 1; not swapped: 0)
 */
DDS_INLINE_EXPORT inline int ddsrt_atomic_casvoidp (volatile ddsrt_atomic_voidp_t *x, void *exp, void *des) {
  return ddsrt_atomic_casptr ((volatile ddsrt_atomic_uintptr_t *) x, (uintptr_t) exp, (uintptr_t) des);
}

/* INC */

/**
 * @brief Increment (by one) a 32-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to increment
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_inc32 (volatile ddsrt_atomic_uint32_t *x) {
  DDSRT_ATOMIC_OP32 (InterlockedIncrement, &x->v);
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Increment (by one) a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to increment
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_inc64 (volatile ddsrt_atomic_uint64_t *x) {
  DDSRT_ATOMIC_OP64 (InterlockedIncrement, &x->v);
}
#endif

/**
 * @brief Increment (by one) a 32-bit unsigned integer atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The old value of the incremented variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_inc32_ov (volatile ddsrt_atomic_uint32_t *x) {
  return DDSRT_ATOMIC_OP32 (InterlockedIncrement, &x->v) - 1;
}

/**
 * @brief Increment (by one) a 32-bit unsigned integer atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The new value of the incremented variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_inc32_nv (volatile ddsrt_atomic_uint32_t *x) {
  return DDSRT_ATOMIC_OP32 (InterlockedIncrement, &x->v);
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Increment (by one) a 64-bit unsigned integer atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The new value of the incremented variable
 */
DDS_INLINE_EXPORT inline uint64_t ddsrt_atomic_inc64_nv (volatile ddsrt_atomic_uint64_t *x) {
  return DDSRT_ATOMIC_OP64 (InterlockedIncrement, &x->v);
}
#endif

/* DEC */

/**
 * @brief Decrement (by one) a 32-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to decrement
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_dec32 (volatile ddsrt_atomic_uint32_t *x) {
  DDSRT_ATOMIC_OP32 (InterlockedDecrement, &x->v);
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Decrement (by one) a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to decrement
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_dec64 (volatile ddsrt_atomic_uint64_t *x) {
  DDSRT_ATOMIC_OP64 (InterlockedDecrement, &x->v);
}
#endif

/**
 * @brief Decrement (by one) a 32-bit unsigned integer atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The new value of the decremented variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_dec32_nv (volatile ddsrt_atomic_uint32_t *x) {
  return DDSRT_ATOMIC_OP32 (InterlockedDecrement, &x->v);
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Decrement (by one) a 64-bit unsigned integer atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The new value of the decremented variable
 */
DDS_INLINE_EXPORT inline uint64_t ddsrt_atomic_dec64_nv (volatile ddsrt_atomic_uint64_t *x) {
  return DDSRT_ATOMIC_OP64 (InterlockedDecrement, &x->v);
}
#endif

/**
 * @brief Decrement (by one) a 32-bit unsigned integer atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The old value of the decremented variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_dec32_ov (volatile ddsrt_atomic_uint32_t *x) {
  return DDSRT_ATOMIC_OP32 (InterlockedDecrement, &x->v) + 1;
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Decrement (by one) a 64-bit unsigned integer atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The old value of the decremented variable
 */
DDS_INLINE_EXPORT inline uint64_t ddsrt_atomic_dec64_ov (volatile ddsrt_atomic_uint64_t *x) {
  return DDSRT_ATOMIC_OP64 (InterlockedDecrement, &x->v) + 1;
}
#endif

/* ADD */

/**
 * @brief Increment (by given amount) a 32-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_add32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  DDSRT_ATOMIC_OP32 (InterlockedExchangeAdd, &x->v, v);
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Increment (by given amount) a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_add64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  DDSRT_ATOMIC_OP64 (InterlockedExchangeAdd, &x->v, v);
}
#endif

/**
 * @brief Increment (by given amount) a 32-bit unsigned integer atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 *
 * @return The old value of the incremented variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_add32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  return DDSRT_ATOMIC_OP32 (InterlockedExchangeAdd, &x->v, v);
}

/**
 * @brief Increment (by given amount) a 32-bit unsigned integer atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 *
 * @return The new value of the incremented variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_add32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  return DDSRT_ATOMIC_OP32 (InterlockedExchangeAdd, &x->v, v) + v;
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Increment (by given amount) a 64-bit unsigned integer atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 *
 * @return The new value of the incremented variable
 */
DDS_INLINE_EXPORT inline uint64_t ddsrt_atomic_add64_nv (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  return DDSRT_ATOMIC_OP64 (InterlockedExchangeAdd, &x->v, v) + v;
}
#endif

/* SUB */

/**
 * @brief Decrement (by given amount) a 32-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_sub32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  /* disable unary minus applied to unsigned type, result still unsigned */
  DDSRT_WARNING_MSVC_OFF(4146)
  DDSRT_ATOMIC_OP32 (InterlockedExchangeAdd, &x->v, -v);
  DDSRT_WARNING_MSVC_ON(4146)
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Decrement (by given amount) a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_sub64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  /* disable unary minus applied to unsigned type, result still unsigned */
  DDSRT_WARNING_MSVC_OFF(4146)
  DDSRT_ATOMIC_OP64 (InterlockedExchangeAdd, &x->v, -v);
  DDSRT_WARNING_MSVC_ON(4146)
}
#endif

/**
 * @brief Decrement (by given amount) a 32-bit unsigned integer atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 * 
 * @return The old value of the decremented variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_sub32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  /* disable unary minus applied to unsigned type, result still unsigned */
  DDSRT_WARNING_MSVC_OFF(4146)
  return DDSRT_ATOMIC_OP32 (InterlockedExchangeAdd, &x->v, -v);
  DDSRT_WARNING_MSVC_ON(4146)
}

/**
 * @brief Decrement (by given amount) a 32-bit unsigned integer atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 * 
 * @return The new value of the decremented variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_sub32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  /* disable unary minus applied to unsigned type, result still unsigned */
  DDSRT_WARNING_MSVC_OFF(4146)
  return DDSRT_ATOMIC_OP32 (InterlockedExchangeAdd, &x->v, -v) - v;
  DDSRT_WARNING_MSVC_ON(4146)
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Decrement (by given amount) a 64-bit unsigned integer atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 * 
 * @return The new value of the decremented variable
 */
DDS_INLINE_EXPORT inline uint64_t ddsrt_atomic_sub64_nv (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  /* disable unary minus applied to unsigned type, result still unsigned */
  DDSRT_WARNING_MSVC_OFF(4146)
  return DDSRT_ATOMIC_OP64 (InterlockedExchangeAdd, &x->v, -v) - v;
  DDSRT_WARNING_MSVC_ON(4146)
}
#endif

/* AND */

/**
 * @brief Bitwise AND a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_and32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  DDSRT_ATOMIC_OP32 (InterlockedAnd, &x->v, v);
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Bitwise AND a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_and64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  DDSRT_ATOMIC_OP64 (InterlockedAnd, &x->v, v);
}
#endif

/**
 * @brief Bitwise AND a 32-bit unsigned integer atomically and return the old value
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 * 
 * @return The old value of the ANDed variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_and32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  return DDSRT_ATOMIC_OP32 (InterlockedAnd, &x->v, v);
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Bitwise AND a 64-bit unsigned integer atomically and return the old value
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 * 
 * @return The old value of the ANDed variable
 */
DDS_INLINE_EXPORT inline uint64_t ddsrt_atomic_and64_ov (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  return DDSRT_ATOMIC_OP64 (InterlockedAnd, &x->v, v);
}
#endif

/**
 * @brief Bitwise AND a 32-bit unsigned integer atomically and return the new value
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 * 
 * @return The new value of the ANDed variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_and32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  return DDSRT_ATOMIC_OP32 (InterlockedAnd, &x->v, v) & v;
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Bitwise AND a 64-bit unsigned integer atomically and return the new value
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 * 
 * @return The new value of the ANDed variable
 */
DDS_INLINE_EXPORT inline uint64_t ddsrt_atomic_and64_nv (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  return DDSRT_ATOMIC_OP64 (InterlockedAnd, &x->v, v) & v;
}
#endif

/* OR */

/**
 * @brief Bitwise OR a 32-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_or32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  DDSRT_ATOMIC_OP32 (InterlockedOr, &x->v, v);
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Bitwise OR a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_or64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  DDSRT_ATOMIC_OP64 (InterlockedOr, &x->v, v);
}
#endif

/**
 * @brief Bitwise OR a 32-bit unsigned integer atomically and return the old value
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 * 
 * @return The old value of the ORed variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_or32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  return DDSRT_ATOMIC_OP32 (InterlockedOr, &x->v, v);
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Bitwise OR a 64-bit unsigned integer atomically and return the old value
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 * 
 * @return The old value of the ORed variable
 */
DDS_INLINE_EXPORT inline uint64_t ddsrt_atomic_or64_ov (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  return DDSRT_ATOMIC_OP64 (InterlockedOr, &x->v, v);
}
#endif

/**
 * @brief Bitwise OR a 32-bit unsigned integer atomically and return the new value
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 * 
 * @return The new value of the ORed variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_or32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  return DDSRT_ATOMIC_OP32 (InterlockedOr, &x->v, v) | v;
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Bitwise OR a 64-bit unsigned integer atomically and return the new value
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 * 
 * @return The new value of the ORed variable
 */
DDS_INLINE_EXPORT inline uint64_t ddsrt_atomic_or64_nv (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  return DDSRT_ATOMIC_OP64 (InterlockedOr, &x->v, v) | v;
}
#endif

/* FENCES */

/**
 * @brief insert a memory barrier for store and load operations
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_fence (void) {
  /* 28113: accessing a local variable tmp via an Interlocked
     function: This is an unusual usage which could be reconsidered.
     It is too heavyweight, true, but it does the trick. */
  DDSRT_WARNING_MSVC_OFF(28113)
  volatile LONG tmp = 0;
  InterlockedExchange (&tmp, 0);
  DDSRT_WARNING_MSVC_ON(28113)
}

/**
 * @brief insert memory barrier for load operations
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_fence_ldld (void) {
#if !(defined _M_IX86 || defined _M_X64)
  ddsrt_atomic_fence ();
#endif
}

/**
 * @brief insert memory barrier for store operations
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_fence_stst (void) {
#if !(defined _M_IX86 || defined _M_X64)
  ddsrt_atomic_fence ();
#endif
}

/**
 * @brief insert memory barrier for load store and load load operations
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_fence_acq (void) {
  ddsrt_atomic_fence ();
}

/**
 * @brief insert memory barrier for load store and store store operations
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_fence_rel (void) {
  ddsrt_atomic_fence ();
}

#undef DDSRT_ATOMIC_PTROP

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_ATOMICS_MSVC_H */
