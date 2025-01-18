/*
 * Copyright(c) 2006 to 2021 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSRT_ATOMICS_GCC_H
#define DDSRT_ATOMICS_GCC_H

#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/attributes.h"
#include <stdint.h>

#if defined (__cplusplus)
extern "C" {
DDSRT_WARNING_GNUC_OFF(old-style-cast)
DDSRT_WARNING_CLANG_OFF(old-style-cast)
#endif

#if ( DDSRT_HAVE_ATOMIC64 && __GCC_HAVE_SYNC_COMPARE_AND_SWAP_16) || \
    (!DDSRT_HAVE_ATOMIC64 && __GCC_HAVE_SYNC_COMPARE_AND_SWAP_8)
# define DDSRT_HAVE_ATOMIC_LIFO 1
#endif

/* LD, ST */

DDS_INLINE_EXPORT 
inline uint32_t ddsrt_atomic_ld32(const volatile ddsrt_atomic_uint32_t *x)
{
  return __atomic_load_n((uint32_t*)x, __ATOMIC_SEQ_CST);
}

#if DDSRT_HAVE_ATOMIC64
/**
 * @brief Fetch the value of a 64-bit unsigned integer atomically
 *
 * @param[in] x Pointer to the variable to fetch the value of
 *
 * @return The value of the fetched variable
 */
DDS_INLINE_EXPORT
inline uint64_t ddsrt_atomic_ld64(const volatile ddsrt_atomic_uint64_t *x)
{
  return __atomic_load_n((uint64_t*)x, __ATOMIC_SEQ_CST);
}
#endif
DDS_INLINE_EXPORT 
inline uintptr_t ddsrt_atomic_ldptr(const volatile ddsrt_atomic_uintptr_t *x)
{
  return __atomic_load_n((uintptr_t*)x, __ATOMIC_SEQ_CST);
}
DDS_INLINE_EXPORT 
inline void *ddsrt_atomic_ldvoidp(const volatile ddsrt_atomic_voidp_t *x)
{
  return (void *) ddsrt_atomic_ldptr(x);
}

DDS_INLINE_EXPORT 
inline void ddsrt_atomic_st32(volatile ddsrt_atomic_uint32_t *x, uint32_t v)
{
  __atomic_store_n((uint32_t*)x, v, __ATOMIC_SEQ_CST);
}

#if DDSRT_HAVE_ATOMIC64
/**
 * @brief Assign a 64-bit unsigned integer atomically
 *
 * @param[out] x Pointer to the variable to update
 * @param[in] v The new value to assign to the variable
 */
DDS_INLINE_EXPORT
inline void ddsrt_atomic_st64(volatile ddsrt_atomic_uint64_t *x, uint64_t v)
{
  __atomic_store_n((uint64_t*)x, v , __ATOMIC_SEQ_CST);
}
#endif
DDS_INLINE_EXPORT 
inline void ddsrt_atomic_stptr(volatile ddsrt_atomic_uintptr_t *x, uintptr_t v)
{
  __atomic_store_n((uintptr_t*)x, v, __ATOMIC_SEQ_CST);
}
DDS_INLINE_EXPORT 
inline void ddsrt_atomic_stvoidp(volatile ddsrt_atomic_voidp_t *x, void *v)
{
  ddsrt_atomic_stptr(x, (uintptr_t)v);
}

/* INC */

DDS_INLINE_EXPORT inline void ddsrt_atomic_inc32(volatile ddsrt_atomic_uint32_t *x) {
  __atomic_fetch_add((uint32_t*)x, 1, __ATOMIC_SEQ_CST);
}

#if DDSRT_HAVE_ATOMIC64
/**
 * @brief Increment (by one) a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to increment
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_inc64 (volatile ddsrt_atomic_uint64_t *x) {
  __atomic_fetch_add((uint64_t*)x, 1, __ATOMIC_SEQ_CST);
}
#endif

/**
 * @brief Increment (by one) an uintptr atomically
 *
 * @param[in,out] x Pointer to the variable to increment
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_incptr(volatile ddsrt_atomic_uintptr_t *x) {
  __sync_fetch_and_add (&x->v, 1);
}

/**
 * @brief Increment (by one) a 32-bit unsigned integer atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The old value of the incremented variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_inc32_ov (volatile ddsrt_atomic_uint32_t *x) {
  return __atomic_fetch_add((uint32_t*)x, 1, __ATOMIC_SEQ_CST);
}

#if DDSRT_HAVE_ATOMIC64
/**
 * @brief Increment (by one) a 64-bit unsigned integer atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The old value of the incremented variable
 */
DDS_INLINE_EXPORT inline uint64_t ddsrt_atomic_inc64_ov (volatile ddsrt_atomic_uint64_t *x) {
  return __sync_fetch_and_add (&x->v, 1);
}
#endif

/**
 * @brief Increment (by one) an uintptr atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The old value of the incremented variable
 */
DDS_INLINE_EXPORT inline uintptr_t ddsrt_atomic_incptr_ov (volatile ddsrt_atomic_uintptr_t *x) {
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
  return __atomic_add_fetch((uint32_t*)x, 1, __ATOMIC_SEQ_CST);
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
  return __atomic_add_fetch((uint64_t*)x, 1, __ATOMIC_SEQ_CST);
}
#endif

/**
 * @brief Increment (by one) an uintptr atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The new value of the incremented variable
 */
DDS_INLINE_EXPORT inline uintptr_t ddsrt_atomic_incptr_nv (volatile ddsrt_atomic_uintptr_t *x) {
  return __atomic_add_fetch ((uintptr_t*)x, 1, __ATOMIC_SEQ_CST);
}

/* DEC */

DDS_INLINE_EXPORT inline void ddsrt_atomic_dec32 (volatile ddsrt_atomic_uint32_t *x) {
  __atomic_fetch_sub((uint32_t*)x, 1, __ATOMIC_SEQ_CST);
}

#if DDSRT_HAVE_ATOMIC64
/**
 * @brief Decrement (by one) a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to decrement
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_dec64 (volatile ddsrt_atomic_uint64_t *x) {
  __atomic_fetch_sub((uint64_t*)x, 1, __ATOMIC_SEQ_CST);
}
#endif

/**
 * @brief Decrement (by one) an uintptr atomically
 *
 * @param[in,out] x Pointer to the variable to decrement
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_decptr (volatile ddsrt_atomic_uintptr_t *x) {
  __atomic_fetch_sub ((uintptr_t*)x, 1, __ATOMIC_SEQ_CST);
}

/**
 * @brief Decrement (by one) a 32-bit unsigned integer atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The new value of the decremented variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_dec32_nv (volatile ddsrt_atomic_uint32_t *x) {
  return __atomic_sub_fetch((uint32_t*)x, 1, __ATOMIC_SEQ_CST);
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
  return __atomic_sub_fetch((uint64_t*)x, 1, __ATOMIC_SEQ_CST);
}
#endif

/**
 * @brief Decrement (by one) an uintptr atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The new value of the decremented variable
 */
DDS_INLINE_EXPORT inline uintptr_t ddsrt_atomic_decptr_nv (volatile ddsrt_atomic_uintptr_t *x) {
  return __atomic_sub_fetch ((uintptr_t*)x, 1, __ATOMIC_SEQ_CST);
}

/**
 * @brief Decrement (by one) a 32-bit unsigned integer atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The old value of the decremented variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_dec32_ov (volatile ddsrt_atomic_uint32_t *x) {
  return __atomic_fetch_sub((uint32_t*)x, 1, __ATOMIC_SEQ_CST);
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
  return __atomic_fetch_sub((uint64_t*)x, 1, __ATOMIC_SEQ_CST);
}
#endif

/**
 * @brief Decrement (by one) an uintptr atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The old value of the decremented variable
 */
DDS_INLINE_EXPORT inline uintptr_t ddsrt_atomic_decptr_ov (volatile ddsrt_atomic_uintptr_t *x) {
  return __atomic_fetch_sub ((uintptr_t*)x, 1, __ATOMIC_SEQ_CST);
}

/* ADD */

DDS_INLINE_EXPORT inline void ddsrt_atomic_add32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  __atomic_fetch_add((uint32_t*)x, v, __ATOMIC_SEQ_CST);
}

#if DDSRT_HAVE_ATOMIC64
/**
 * @brief Increment (by given amount) a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_add64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  __atomic_fetch_add((uint64_t*)x, v, __ATOMIC_SEQ_CST);
}
#endif

/**
 * @brief Increment (by given amount) an uintptr atomically
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_addptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  __atomic_fetch_add ((uintptr_t*)x, v, __ATOMIC_SEQ_CST);
}

/**
 * @brief Increment (by given amount) a 32-bit unsigned integer atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 *
 * @return The old value of the incremented variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_add32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  return __atomic_fetch_add((uint32_t*)x, v, __ATOMIC_SEQ_CST);
}

#if DDSRT_HAVE_ATOMIC64
/**
 * @brief Increment (by given amount) a 64-bit unsigned integer atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 *
 * @return The new value of the incremented variable
 */
DDS_INLINE_EXPORT inline uint64_t ddsrt_atomic_add64_ov (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  return __atomic_fetch_add ((uintptr_t*)x, v, __ATOMIC_SEQ_CST);
}
#endif

/**
 * @brief Increment (by given amount) an uintptr atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 *
 * @return The old value of the incremented variable
 */
DDS_INLINE_EXPORT inline uintptr_t ddsrt_atomic_addptr_ov (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  return __atomic_fetch_add ((uintptr_t*)x, v, __ATOMIC_SEQ_CST);
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
  return __atomic_add_fetch((uint32_t*)x, v, __ATOMIC_SEQ_CST);
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
  return __atomic_add_fetch((uint64_t*)x, v, __ATOMIC_SEQ_CST);
}
#endif

/**
 * @brief Increment (by given amount) an uintptr atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 *
 * @return The new value of the incremented variable
 */
DDS_INLINE_EXPORT inline uintptr_t ddsrt_atomic_addptr_nv (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  return __atomic_add_fetch ((uintptr_t*)x, v, __ATOMIC_SEQ_CST);
}

/* SUB */

DDS_INLINE_EXPORT inline void ddsrt_atomic_sub32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  __atomic_fetch_sub((uint32_t*)x, v, __ATOMIC_SEQ_CST);
}

#if DDSRT_HAVE_ATOMIC64
/**
 * @brief Decrement (by given amount) a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_sub64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  __atomic_fetch_sub((uint64_t*)x, v, __ATOMIC_SEQ_CST);
}
#endif

/**
 * @brief Decrement (by given amount) an uintptr atomically
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_subptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  __atomic_fetch_sub ((uintptr_t*)x, v, __ATOMIC_SEQ_CST);
}

/**
 * @brief Decrement (by given amount) a 32-bit unsigned integer atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 *
 * @return The old value of the decremented variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_sub32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  return __atomic_fetch_sub((uint32_t*)x, v, __ATOMIC_SEQ_CST);
}

#if DDSRT_HAVE_ATOMIC64
/**
 * @brief Decrement (by given amount) a 64-bit unsigned integer atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 *
 * @return The old value of the decremented variable
 */
DDS_INLINE_EXPORT inline uint64_t ddsrt_atomic_sub64_ov (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  return __atomic_fetch_sub ((uintptr_t*)x, v, __ATOMIC_SEQ_CST);
}
#endif

/**
 * @brief Decrement (by given amount) an uintptr atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 *
 * @return The old value of the decremented variable
 */
DDS_INLINE_EXPORT inline uintptr_t ddsrt_atomic_subptr_ov (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  return __atomic_fetch_sub ((uintptr_t*)x, v, __ATOMIC_SEQ_CST);
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
  return __atomic_sub_fetch((uint32_t*)x, v, __ATOMIC_SEQ_CST);
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
  return __atomic_sub_fetch((uint64_t*)x, v, __ATOMIC_SEQ_CST);
}
#endif

/**
 * @brief Decrement (by given amount) an uintptr atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 *
 * @return The new value of the decremented variable
 */
DDS_INLINE_EXPORT inline uintptr_t ddsrt_atomic_subptr_nv (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  return __atomic_sub_fetch ((uintptr_t*)x, v, __ATOMIC_SEQ_CST);
}

/* AND */

DDS_INLINE_EXPORT inline void ddsrt_atomic_and32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  __atomic_fetch_and((uint32_t*)x, v, __ATOMIC_SEQ_CST);
}

#if DDSRT_HAVE_ATOMIC64
/**
 * @brief Bitwise AND a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_and64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  __atomic_fetch_and((uint64_t*)x, v, __ATOMIC_SEQ_CST);
}
#endif

/**
 * @brief Bitwise AND an uintptr atomically
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_andptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  __atomic_fetch_and ((uintptr_t*)x, v, __ATOMIC_SEQ_CST);
}

/**
 * @brief Bitwise AND a 32-bit unsigned integer atomically and return the old value
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 *
 * @return The old value of the ANDed variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_and32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  return __atomic_fetch_and((uint32_t*)x, v, __ATOMIC_SEQ_CST);
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
  return __atomic_fetch_and((uint64_t*)x, v, __ATOMIC_SEQ_CST);
}
#endif

/**
 * @brief Bitwise AND an uintptr atomically and return the old value
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 *
 * @return The old value of the ANDed variable
 */
DDS_INLINE_EXPORT inline uintptr_t ddsrt_atomic_andptr_ov (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  return __atomic_fetch_and ((uintptr_t*)x, v, __ATOMIC_SEQ_CST);
}

/**
 * @brief Bitwise AND a 32-bit unsigned integer atomically and return the new value
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 *
 * @return The new value of the ANDed variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_and32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  return __atomic_and_fetch((uint32_t*)x, v, __ATOMIC_SEQ_CST);
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
  return __atomic_and_fetch((uint64_t*)x, v, __ATOMIC_SEQ_CST);
}
#endif

/**
 * @brief Bitwise AND an uintptr atomically and return the new value
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 *
 * @return The new value of the ANDed variable
 */
DDS_INLINE_EXPORT inline uintptr_t ddsrt_atomic_andptr_nv (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  return __atomic_and_fetch ((uintptr_t*)x, v, __ATOMIC_SEQ_CST);
}

/* OR */

DDS_INLINE_EXPORT inline void ddsrt_atomic_or32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  __atomic_fetch_or((uint32_t*)x, v, __ATOMIC_SEQ_CST);
}

#if DDSRT_HAVE_ATOMIC64
/**
 * @brief Bitwise OR a 64-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_or64 (volatile ddsrt_atomic_uint64_t *x, uint64_t v) {
  __atomic_fetch_or((uint64_t*)x, v, __ATOMIC_SEQ_CST);
}
#endif

/**
 * @brief Bitwise OR an uintptr atomically
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 */
DDS_INLINE_EXPORT inline void ddsrt_atomic_orptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  __atomic_fetch_or ((uintptr_t*)x, v, __ATOMIC_SEQ_CST);
}

/**
 * @brief Bitwise OR a 32-bit unsigned integer atomically and return the old value
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 *
 * @return The old value of the ORed variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_or32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  return __atomic_fetch_or((uint32_t*)x, v, __ATOMIC_SEQ_CST);
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
  return __atomic_fetch_or((uint64_t*)x, v, __ATOMIC_SEQ_CST);
}
#endif

/**
 * @brief Bitwise OR an uintptr  atomically and return the old value
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 *
 * @return The old value of the ORed variable
 */
DDS_INLINE_EXPORT inline uintptr_t ddsrt_atomic_orptr_ov (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  return __atomic_fetch_or ((uintptr_t*)x, v, __ATOMIC_SEQ_CST);
}

/**
 * @brief Bitwise OR a 32-bit unsigned integer atomically and return the new value
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 *
 * @return The new value of the ORed variable
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_atomic_or32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  return __atomic_or_fetch((uint32_t*)x, v, __ATOMIC_SEQ_CST);
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
  return __atomic_or_fetch((uint64_t*)x, v, __ATOMIC_SEQ_CST);
}
#endif

/**
 * @brief Bitwise OR an uintptr atomically and return the new value
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 *
 * @return The new value of the ORed variable
 */
DDS_INLINE_EXPORT inline uintptr_t ddsrt_atomic_orptr_nv (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  return __atomic_or_fetch ((uintptr_t*)x, v, __ATOMIC_SEQ_CST);
}

/* CAS */

DDS_INLINE_EXPORT inline int ddsrt_atomic_cas32 (volatile ddsrt_atomic_uint32_t *x, uint32_t exp, uint32_t des) {
  return __atomic_compare_exchange_n((uint32_t*)x, &exp, des, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
#if DDSRT_HAVE_ATOMIC64
DDS_INLINE_EXPORT inline int ddsrt_atomic_cas64 (volatile ddsrt_atomic_uint64_t *x, uint64_t exp, uint64_t des) {
  return __atomic_compare_exchange_n((uint64_t*)x, &exp, des, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
#endif
DDS_INLINE_EXPORT inline int ddsrt_atomic_casptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t exp, uintptr_t des) {
  return __atomic_compare_exchange_n((uintptr_t*)x, &exp, des, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}
DDS_INLINE_EXPORT inline int ddsrt_atomic_casvoidp (volatile ddsrt_atomic_voidp_t *x, void *exp, void *des) {
  return ddsrt_atomic_casptr(x, (uintptr_t) exp, (uintptr_t) des);
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

typedef struct {
  ddsrt_atomic_uintptr2_t aba_head;
} ddsrt_atomic_lifo_t;

DDS_EXPORT void ddsrt_atomic_lifo_init(ddsrt_atomic_lifo_t *head);
DDS_EXPORT void ddsrt_atomic_lifo_push(ddsrt_atomic_lifo_t *head, void *elem, size_t linkoff);
DDS_EXPORT void *ddsrt_atomic_lifo_pop(ddsrt_atomic_lifo_t *head, size_t linkoff);
DDS_EXPORT void ddsrt_atomic_lifo_pushmany(ddsrt_atomic_lifo_t *head, void *first, void *last, size_t linkoff);

DDS_INLINE_EXPORT inline int ddsrt_atomic_casvoidp2 (volatile ddsrt_atomic_uintptr2_t *x, uintptr_t a0, uintptr_t b0, uintptr_t a1, uintptr_t b1) {
  ddsrt_atomic_uintptr2_t o, n;
  o.s.a = a0; o.s.b = b0;
  n.s.a = a1; n.s.b = b1;
#if defined(__s390__) || defined(__s390x__) || defined(__zarch__)
  return __sync_bool_compare_and_swap ((ddsrt_atomic_int128_t*)__builtin_assume_aligned(&x->x, 16), o.x, n.x);
#else
  return __atomic_compare_exchange_n((uintptr_t*)x, &o.x, n.x, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}
#endif

/* FENCES */

DDS_INLINE_EXPORT inline void ddsrt_atomic_fence (void) {
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
}
DDS_INLINE_EXPORT inline void ddsrt_atomic_fence_ldld (void) {
#if !(defined __i386__ || defined __x86_64__ || defined _M_IX86 || defined _M_X64)
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
}
DDS_INLINE_EXPORT inline void ddsrt_atomic_fence_stst (void) {
#if !(defined __i386__ || defined __x86_64__ || defined _M_IX86 || defined _M_X64)
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
}
DDS_INLINE_EXPORT inline void ddsrt_atomic_fence_acq (void) {
#if !(defined __i386__ || defined __x86_64__ || defined _M_IX86 || defined _M_X64)
  ddsrt_atomic_fence();
#else
  __atomic_thread_fence(__ATOMIC_ACQUIRE);
#endif
}
DDS_INLINE_EXPORT inline void ddsrt_atomic_fence_rel (void) {
#if !(defined __i386__ || defined __x86_64__ || defined _M_IX86 || defined _M_X64)
  ddsrt_atomic_fence();
#else
  __atomic_thread_fence(__ATOMIC_RELAXED);
#endif
}

#if defined (__cplusplus)
DDSRT_WARNING_CLANG_ON(old-style-cast)
DDSRT_WARNING_GNUC_ON(old-style-cast)
}
#endif

#endif /* DDSRT_ATOMICS_GCC_H */
