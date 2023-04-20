// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSRT_ATOMICS_GCC_H
#define DDSRT_ATOMICS_GCC_H

#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/attributes.h"

#if defined (__cplusplus)
extern "C" {
DDSRT_WARNING_GNUC_OFF(old-style-cast)
DDSRT_WARNING_CLANG_OFF(old-style-cast)
#endif

#if ( DDSRT_HAVE_ATOMIC64 && __GCC_HAVE_SYNC_COMPARE_AND_SWAP_16) || \
    (!DDSRT_HAVE_ATOMIC64 && __GCC_HAVE_SYNC_COMPARE_AND_SWAP_8)
# define DDSRT_HAVE_ATOMIC_LIFO 1
#endif

/**
 * @file gcc.h
 * @brief Contains functions and types for atomic operations on various types for gcc.
 *
 * This header file defines functions for atomic operations on uint32_t,
 * uint64_t, uintptr_t, and void* types.
 * 
 * It also declares atomic operations for a lifo linked list.
 */

/* LD, ST */

/**
 * @brief Fetch the value of a 32-bit unsigned integer atomically
 *
 * @param[in] x Pointer to the variable to fetch the value of
 *
 * @return The value of the fetched variable
 */
DDS_INLINE_EXPORT ddsrt_attribute_no_sanitize (("thread"))
inline uint32_t ddsrt_atomic_ld32(const volatile ddsrt_atomic_uint32_t *x)
{
  return x->v;
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Fetch the value of a 64-bit unsigned integer atomically
 *
 * @param[in] x Pointer to the variable to fetch the value of
 *
 * @return The value of the fetched variable
 */
DDS_INLINE_EXPORT ddsrt_attribute_no_sanitize (("thread"))
inline uint64_t ddsrt_atomic_ld64(const volatile ddsrt_atomic_uint64_t *x)
{
  return x->v;
}
#endif

/**
 * @brief Fetch the value of an uintptr atomically
 *
 * @param[in] x Pointer to the variable to fetch the value of
 *
 * @return The value of the fetched variable
 */
DDS_INLINE_EXPORT ddsrt_attribute_no_sanitize (("thread"))
inline uintptr_t ddsrt_atomic_ldptr(const volatile ddsrt_atomic_uintptr_t *x)
{
  return x->v;
}

/**
 * @brief Fetch the value of a void pointer atomically
 *
 * @param[in] x Pointer to the variable to fetch the value of
 *
 * @return The value of the fetched variable
 */
DDS_INLINE_EXPORT ddsrt_attribute_no_sanitize (("thread"))
inline void *ddsrt_atomic_ldvoidp(const volatile ddsrt_atomic_voidp_t *x)
{
  return (void *) ddsrt_atomic_ldptr(x);
}

/**
 * @brief Assign a 32-bit unsigned integer atomically
 *
 * @param[out] x Pointer to the variable to update
 * @param[in] v The new value to assign to the variable
 */
DDS_INLINE_EXPORT ddsrt_attribute_no_sanitize (("thread"))
inline void ddsrt_atomic_st32(volatile ddsrt_atomic_uint32_t *x, uint32_t v)
{
  x->v = v;
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Assign a 64-bit unsigned integer atomically
 *
 * @param[out] x Pointer to the variable to update
 * @param[in] v The new value to assign to the variable
 */
DDS_INLINE_EXPORT ddsrt_attribute_no_sanitize (("thread"))
inline void ddsrt_atomic_st64(volatile ddsrt_atomic_uint64_t *x, uint64_t v)
{
  x->v = v;
}
#endif

/**
 * @brief Assign an uintptr atomically
 *
 * @param[out] x Pointer to the variable to update
 * @param[in] v The new value to assign to the variable
 */
DDS_INLINE_EXPORT ddsrt_attribute_no_sanitize (("thread"))
inline void ddsrt_atomic_stptr(volatile ddsrt_atomic_uintptr_t *x, uintptr_t v)
{
  x->v = v;
}

/**
 * @brief Assign a void pointer atomically
 *
 * @param[out] x Pointer to the variable to update
 * @param[in] v The new value to assign to the variable
 */
DDS_INLINE_EXPORT ddsrt_attribute_no_sanitize (("thread"))
inline void ddsrt_atomic_stvoidp(volatile ddsrt_atomic_voidp_t *x, void *v)
{
  ddsrt_atomic_stptr(x, (uintptr_t)v);
}

/* INC */

/**
 * @brief Increment (by one) a 32-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to increment
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_inc32(volatile ddsrt_atomic_uint32_t *x) {
  __sync_fetch_and_add (&x->v, 1);
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Increment (by one) a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to increment
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_inc64 (volatile ddsrt_atomic_uint64_t *x) {
  __sync_fetch_and_add (&x->v, 1);
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
  return __sync_fetch_and_add (&x->v, 1);
}

/**
 * @brief Increment (by one) a 32-bit unsigned integer atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The new value of the incremented variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_inc32_nv (volatile ddsrt_atomic_uint32_t *x) {
  return __sync_add_and_fetch (&x->v, 1);
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
  return __sync_add_and_fetch (&x->v, 1);
}
#endif

/* DEC */

/**
 * @brief Decrement (by one) a 32-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to decrement
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_dec32 (volatile ddsrt_atomic_uint32_t *x) {
  __sync_fetch_and_sub (&x->v, 1);
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Decrement (by one) a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to decrement
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_dec64 (volatile ddsrt_atomic_uint64_t *x) {
  __sync_fetch_and_sub (&x->v, 1);
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
  return __sync_sub_and_fetch (&x->v, 1);
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
  return __sync_sub_and_fetch (&x->v, 1);
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
  return __sync_fetch_and_sub (&x->v, 1);
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
  return __sync_fetch_and_sub (&x->v, 1);
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
  __sync_fetch_and_add (&x->v, v);
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Increment (by given amount) a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_add64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  __sync_fetch_and_add (&x->v, v);
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
  return __sync_fetch_and_add (&x->v, v);
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
  return __sync_add_and_fetch (&x->v, v);
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
  return __sync_add_and_fetch (&x->v, v);
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
  __sync_fetch_and_sub (&x->v, v);
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Decrement (by given amount) a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_sub64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  __sync_fetch_and_sub (&x->v, v);
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
  return __sync_fetch_and_sub (&x->v, v);
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
  return __sync_sub_and_fetch (&x->v, v);
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
  return __sync_sub_and_fetch (&x->v, v);
}
#endif

/* AND */

/**
 * @brief Bitwise AND a 32-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_and32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  __sync_fetch_and_and (&x->v, v);
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Bitwise AND a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_and64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  __sync_fetch_and_and (&x->v, v);
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
  return __sync_fetch_and_and (&x->v, v);
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
  return __sync_fetch_and_and (&x->v, v);
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
  return __sync_and_and_fetch (&x->v, v);
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
  return __sync_and_and_fetch (&x->v, v);
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
  __sync_fetch_and_or (&x->v, v);
}
#if DDSRT_HAVE_ATOMIC64

/**
 * @brief Bitwise OR a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_or64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  __sync_fetch_and_or (&x->v, v);
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
  return __sync_fetch_and_or (&x->v, v);
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
  return __sync_fetch_and_or (&x->v, v);
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
  return __sync_or_and_fetch (&x->v, v);
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
  return __sync_or_and_fetch (&x->v, v);
}
#endif

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
  return __sync_bool_compare_and_swap (&x->v, exp, des);
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
  return __sync_bool_compare_and_swap (&x->v, exp, des);
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
  return __sync_bool_compare_and_swap (&x->v, exp, des);
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
  return ddsrt_atomic_casptr (x, (uintptr_t) exp, (uintptr_t) des);
}
#if DDSRT_HAVE_ATOMIC_LIFO
#if DDSRT_HAVE_ATOMIC64
#if defined(__s390__) || defined(__s390x__) || defined(__zarch__)
typedef __int128_t __attribute__((aligned(16))) ddsrt_atomic_int128_t;
#else
typedef __int128_t ddsrt_atomic_int128_t;
#endif
typedef union { ddsrt_atomic_int128_t x; struct { uintptr_t a, b; } s; } ddsrt_atomic_uintptr2_t;
#else
typedef union { uint64_t x; struct { uintptr_t a, b; } s; } ddsrt_atomic_uintptr2_t;
#endif

/**
 * @brief A lifo linked list supporting atomic operations
 */
typedef struct {
  ddsrt_atomic_uintptr2_t aba_head;
} ddsrt_atomic_lifo_t;

/**
 * @brief Initialize the lifo list
 * 
 * @param[out] head the lifo list
 */
DDS_EXPORT void ddsrt_atomic_lifo_init(ddsrt_atomic_lifo_t *head);

/**
 * @brief Push to the lifo list
 * 
 * @param[in,out] head the lifo list
 * @param[in] elem the element to push to it
 * @param[in] linkoff the link offset with respect to elem
 */
DDS_EXPORT void ddsrt_atomic_lifo_push(ddsrt_atomic_lifo_t *head, void *elem, size_t linkoff);

/**
 * @brief Pop from the lifo list
 * 
 * @param[in,out] head the lifo list
 * @param[in] linkoff the link offset with respect to elem
 * 
 * @return pointer to the popped elem
 */
DDS_EXPORT void *ddsrt_atomic_lifo_pop(ddsrt_atomic_lifo_t *head, size_t linkoff);

/**
 * @brief Push a chain of elements to the lifo list
 * 
 * @param[in,out] head the lifo list
 * @param[in] first the element that will be at the top of the stack
 * @param[in] last the element connected to the existing chain
 * @param[in] linkoff the link offset with respect to the element
 */
DDS_EXPORT void ddsrt_atomic_lifo_pushmany(ddsrt_atomic_lifo_t *head, void *first, void *last, size_t linkoff);

/**
 * @brief Compare and swap two void pointers atomically
 * 
 * Assigns a1 to x.a and b1 to x.b iff (x.a == a0 && x.b == b0)
 *
 * @param[in,out] x pointer to two variables whose value to swap
 * @param[in] a0 the value to test x.a for
 * @param[in] b0 the value to test x.b for
 * @param[in] a1 the value to assign to x.a
 * @param[in] b1 the value to assign to x.b
 * 
 * @return int meant to be interpreted as bool (swapped: 1; not swapped: 0)
 */
DDS_INLINE_EXPORT inline int ddsrt_atomic_casvoidp2 (volatile ddsrt_atomic_uintptr2_t *x, uintptr_t a0, uintptr_t b0, uintptr_t a1, uintptr_t b1) {
  ddsrt_atomic_uintptr2_t o, n;
  o.s.a = a0; o.s.b = b0;
  n.s.a = a1; n.s.b = b1;
#if defined(__s390__) || defined(__s390x__) || defined(__zarch__)
  return __sync_bool_compare_and_swap ((ddsrt_atomic_int128_t*)__builtin_assume_aligned(&x->x, 16), o.x, n.x);
#else
  return __sync_bool_compare_and_swap (&x->x, o.x, n.x);
#endif
}
#endif

/* FENCES */

/**
 * @brief insert a memory barrier for store and load operations
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_fence (void) {
  __sync_synchronize ();
}

/**
 * @brief insert memory barrier for load operations
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_fence_ldld (void) {
#if !(defined __i386__ || defined __x86_64__ || defined _M_IX86 || defined _M_X64)
  __sync_synchronize ();
#endif
}

/**
 * @brief insert memory barrier for store operations
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_fence_stst (void) {
#if !(defined __i386__ || defined __x86_64__ || defined _M_IX86 || defined _M_X64)
  __sync_synchronize ();
#endif
}

/**
 * @brief insert memory barrier for load store and load load operations
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_fence_acq (void) {
#if !(defined __i386__ || defined __x86_64__ || defined _M_IX86 || defined _M_X64)
  ddsrt_atomic_fence ();
#else
  __asm__ volatile ("" ::: "memory");
#endif
}

/**
 * @brief insert memory barrier for load store and store store operations
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_fence_rel (void) {
#if !(defined __i386__ || defined __x86_64__ || defined _M_IX86 || defined _M_X64)
  ddsrt_atomic_fence ();
#else
  __asm__ volatile ("" ::: "memory");
#endif
}

#if defined (__cplusplus)
DDSRT_WARNING_CLANG_ON(old-style-cast)
DDSRT_WARNING_GNUC_ON(old-style-cast)
}
#endif

#endif /* DDSRT_ATOMICS_GCC_H */
