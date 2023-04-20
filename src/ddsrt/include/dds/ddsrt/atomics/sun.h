// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <atomic.h>

#if defined (__cplusplus)
extern "C" {
#endif

#define DDSRT_ATOMIC64_SUPPORT 1

/**
 * @file sun.h
 * @brief Contains functions and types for atomic operations on various types for sun.
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
inline uint32_t ddsrt_atomic_ld32 (const volatile ddsrt_atomic_uint32_t *x) { return x->v; }

/**
 * @brief Fetch the value of a 64-bit unsigned integer atomically
 *
 * @param[in] x Pointer to the variable to fetch the value of
 *
 * @return The value of the fetched variable
 */
inline uint64_t ddsrt_atomic_ld64 (const volatile ddsrt_atomic_uint64_t *x) { return x->v; }

/**
 * @brief Fetch the value of an uintptr atomically
 *
 * @param[in] x Pointer to the variable to fetch the value of
 *
 * @return The value of the fetched variable
 */
inline uintptr_t ddsrt_atomic_ldptr (const volatile ddsrt_atomic_uintptr_t *x) { return x->v; }

/**
 * @brief Fetch the value of a void pointer atomically
 *
 * @param[in] x Pointer to the variable to fetch the value of
 *
 * @return The value of the fetched variable
 */
inline void *ddsrt_atomic_ldvoidp (const volatile ddsrt_atomic_voidp_t *x) { return (void *) ddsrt_atomic_ldptr (x); }

/**
 * @brief Assign a 32-bit unsigned integer atomically
 *
 * @param[out] x Pointer to the variable to update
 * @param[in] v The new value to assign to the variable
 */
inline void ddsrt_atomic_st32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) { x->v = v; }

/**
 * @brief Assign a 64-bit unsigned integer atomically
 *
 * @param[out] x Pointer to the variable to update
 * @param[in] v The new value to assign to the variable
 */
inline void ddsrt_atomic_st64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v) { x->v = v; }

/**
 * @brief Assign an uintptr atomically
 *
 * @param[out] x Pointer to the variable to update
 * @param[in] v The new value to assign to the variable
 */
inline void ddsrt_atomic_stptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) { x->v = v; }

/**
 * @brief Assign a void pointer atomically
 *
 * @param[out] x Pointer to the variable to update
 * @param[in] v The new value to assign to the variable
 */
inline void ddsrt_atomic_stvoidp (volatile ddsrt_atomic_voidp_t *x, void *v) { ddsrt_atomic_stptr (x, (uintptr_t) v); }

/* INC */

/**
 * @brief Increment (by one) a 32-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to increment
 */
inline void ddsrt_atomic_inc32 (volatile ddsrt_atomic_uint32_t *x) {
  atomic_inc_32 (&x->v);
}

/**
 * @brief Increment (by one) a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to increment
 */
inline void ddsrt_atomic_inc64 (volatile ddsrt_atomic_uint64_t *x) {
  atomic_inc_64 (&x->v);
}

/**
 * @brief Increment (by one) a 32-bit unsigned integer atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The old value of the incremented variable
 */
inline uint32_t ddsrt_atomic_inc32_ov (volatile ddsrt_atomic_uint32_t *x) {
  uint32_t oldval, newval;
  do { oldval = x->v; newval = oldval + 1; } while (atomic_cas_32 (&x->v, oldval, newval) != oldval);
  return oldval;
}

/**
 * @brief Increment (by one) a 32-bit unsigned integer atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The new value of the incremented variable
 */
inline uint32_t ddsrt_atomic_inc32_nv (volatile ddsrt_atomic_uint32_t *x) {
  return atomic_inc_32_nv (&x->v);
}

/**
 * @brief Increment (by one) a 64-bit unsigned integer atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The new value of the incremented variable
 */
inline uint64_t ddsrt_atomic_inc64_nv (volatile ddsrt_atomic_uint64_t *x) {
  return atomic_inc_64_nv (&x->v);
}

/* DEC */

/**
 * @brief Decrement (by one) a 32-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to decrement
 */
inline void ddsrt_atomic_dec32 (volatile ddsrt_atomic_uint32_t *x) {
  atomic_dec_32 (&x->v);
}

/**
 * @brief Decrement (by one) a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to decrement
 */
inline void ddsrt_atomic_dec64 (volatile ddsrt_atomic_uint64_t *x) {
  atomic_dec_64 (&x->v);
}

/**
 * @brief Decrement (by one) a 32-bit unsigned integer atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The new value of the decremented variable
 */
inline uint32_t ddsrt_atomic_dec32_nv (volatile ddsrt_atomic_uint32_t *x) {
  return atomic_dec_32_nv (&x->v);
}

/**
 * @brief Decrement (by one) a 64-bit unsigned integer atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The new value of the decremented variable
 */
inline uint64_t ddsrt_atomic_dec64_nv (volatile ddsrt_atomic_uint64_t *x) {
  return atomic_dec_64_nv (&x->v);
}

/**
 * @brief Decrement (by one) a 32-bit unsigned integer atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The old value of the decremented variable
 */
inline uint32_t ddsrt_atomic_dec32_ov (volatile ddsrt_atomic_uint32_t *x) {
  uint32_t oldval, newval;
  do { oldval = x->v; newval = oldval - 1; } while (atomic_cas_32 (&x->v, oldval, newval) != oldval);
  return oldval;
}

/**
 * @brief Decrement (by one) a 64-bit unsigned integer atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The old value of the decremented variable
 */
inline uint64_t ddsrt_atomic_dec64_ov (volatile ddsrt_atomic_uint64_t *x) {
  uint64_t oldval, newval;
  do { oldval = x->v; newval = oldval - 1; } while (atomic_cas_64 (&x->v, oldval, newval) != oldval);
  return oldval;
}

/* ADD */

/**
 * @brief Increment (by given amount) a 32-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 */
inline void ddsrt_atomic_add32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  atomic_add_32 (&x->v, v);
}

/**
 * @brief Increment (by given amount) a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 */
inline void ddsrt_atomic_add64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  atomic_add_64 (&x->v, v);
}

/**
 * @brief Increment (by given amount) a 32-bit unsigned integer atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 *
 * @return The old value of the incremented variable
 */
inline uint32_t ddsrt_atomic_add32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  return atomic_add_32_nv (&x->v, v) - v;
}

/**
 * @brief Increment (by given amount) a 32-bit unsigned integer atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 *
 * @return The new value of the incremented variable
 */
inline uint32_t ddsrt_atomic_add32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  return atomic_add_32_nv (&x->v, v);
}

/**
 * @brief Increment (by given amount) a 64-bit unsigned integer atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 *
 * @return The new value of the incremented variable
 */
inline uint64_t ddsrt_atomic_add64_nv (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  return atomic_add_64_nv (&x->v, v);
}

/* SUB */

/**
 * @brief Decrement (by given amount) a 32-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 */
inline void ddsrt_atomic_sub32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  atomic_add_32 (&x->v, -v);
}

/**
 * @brief Decrement (by given amount) a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 */
inline void ddsrt_atomic_sub64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  atomic_add_64 (&x->v, -v);
}

/**
 * @brief Decrement (by given amount) a 32-bit unsigned integer atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 * 
 * @return The old value of the decremented variable
 */
inline uint32_t ddsrt_atomic_sub32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  return atomic_add_32_nv (&x->v, -v) + v;
}

/**
 * @brief Decrement (by given amount) a 32-bit unsigned integer atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 * 
 * @return The new value of the decremented variable
 */
inline uint32_t ddsrt_atomic_sub32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  return atomic_add_32_nv (&x->v, -v);
}

/**
 * @brief Decrement (by given amount) a 64-bit unsigned integer atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 * 
 * @return The new value of the decremented variable
 */
inline uint64_t ddsrt_atomic_sub64_nv (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  return atomic_add_64_nv (&x->v, -v);
}

/* AND */

/**
 * @brief Bitwise AND a 32-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 */
inline void ddsrt_atomic_and32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  atomic_and_32 (&x->v, v);
}

/**
 * @brief Bitwise AND a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 */
inline void ddsrt_atomic_and64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  atomic_and_64 (&x->v, v);
}

/**
 * @brief Bitwise AND a 32-bit unsigned integer atomically and return the old value
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 * 
 * @return The old value of the ANDed variable
 */
inline uint32_t ddsrt_atomic_and32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  uint32_t oldval, newval;
  do { oldval = x->v; newval = oldval & v; } while (atomic_cas_32 (&x->v, oldval, newval) != oldval);
  return oldval;
}

/**
 * @brief Bitwise AND a 64-bit unsigned integer atomically and return the old value
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 * 
 * @return The old value of the ANDed variable
 */
inline uint64_t ddsrt_atomic_and64_ov (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  uint64_t oldval, newval;
  do { oldval = x->v; newval = oldval & v; } while (atomic_cas_64 (&x->v, oldval, newval) != oldval);
  return oldval;
}

/**
 * @brief Bitwise AND a 32-bit unsigned integer atomically and return the new value
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 * 
 * @return The new value of the ANDed variable
 */
inline uint32_t ddsrt_atomic_and32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  return atomic_and_32_nv (&x->v, v);
}

/**
 * @brief Bitwise AND a 64-bit unsigned integer atomically and return the new value
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 * 
 * @return The new value of the ANDed variable
 */
inline uint64_t ddsrt_atomic_and64_nv (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  return atomic_and_64_nv (&x->v, v);
}

/* OR */

/**
 * @brief Bitwise OR a 32-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 */
inline void ddsrt_atomic_or32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  atomic_or_32 (&x->v, v);
}

/**
 * @brief Bitwise OR a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 */
inline void ddsrt_atomic_or64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  atomic_or_64 (&x->v, v);
}

/**
 * @brief Bitwise OR a 32-bit unsigned integer atomically and return the old value
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 * 
 * @return The old value of the ORed variable
 */
inline uint32_t ddsrt_atomic_or32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  uint32_t oldval, newval;
  do { oldval = x->v; newval = oldval | v; } while (atomic_cas_32 (&x->v, oldval, newval) != oldval);
  return oldval;
}

/**
 * @brief Bitwise OR a 64-bit unsigned integer atomically and return the old value
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 * 
 * @return The old value of the ORed variable
 */
inline uint64_t ddsrt_atomic_or64_ov (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  uint64_t oldval, newval;
  do { oldval = x->v; newval = oldval | v; } while (atomic_cas_64 (&x->v, oldval, newval) != oldval);
  return oldval;
}

/**
 * @brief Bitwise OR a 32-bit unsigned integer atomically and return the new value
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 * 
 * @return The new value of the ORed variable
 */
inline uint32_t ddsrt_atomic_or32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  return atomic_or_32_nv (&x->v, v);
}

/**
 * @brief Bitwise OR a 64-bit unsigned integer atomically and return the new value
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 * 
 * @return The new value of the ORed variable
 */
inline uint64_t ddsrt_atomic_or64_nv (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  return atomic_or_64_nv (&x->v, v);
}

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
inline int ddsrt_atomic_cas32 (volatile ddsrt_atomic_uint32_t *x, uint32_t exp, uint32_t des) {
  return atomic_cas_32 (&x->v, exp, des) == exp;
}

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
inline int ddsrt_atomic_cas64 (volatile ddsrt_atomic_uint64_t *x, uint64_t exp, uint64_t des) {
  return atomic_cas_64 (&x->v, exp, des) == exp;
}

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
inline int ddsrt_atomic_casptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t exp, uintptr_t des) {
  return atomic_cas_ulong (&x->v, exp, des) == exp;
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
inline int ddsrt_atomic_casvoidp (volatile ddsrt_atomic_voidp_t *x, void *exp, void *des) {
  return atomic_cas_ptr (&x->v, exp, des) == exp;
}

/* FENCES */

/**
 * @brief insert a memory barrier for store and load operations
 */
inline void ddsrt_atomic_fence (void) {
  membar_exit ();
  membar_enter ();
}

/**
 * @brief insert memory barrier only for load operations
 */
inline void ddsrt_atomic_fence_ldld (void) {
  membar_consumer ();
}

/**
 * @brief insert memory barrier only for store operations
 */
inline void ddsrt_atomic_fence_stst (void) {
  membar_producer ();
}

/**
 * @brief insert memory barrier for load store and load load operations
 */
inline void ddsrt_atomic_fence_acq (void) {
  membar_enter ();
}

/**
 * @brief insert memory barrier for load store and store store operations
 */
inline void ddsrt_atomic_fence_rel (void) {
  membar_exit ();
}

#if defined (__cplusplus)
}
#endif
