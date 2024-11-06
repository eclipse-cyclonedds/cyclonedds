/*
 * Copyright(c) 2006 to 2020 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSRT_ATOMICS_ARM_H
#define DDSRT_ATOMICS_ARM_H

#if defined (__cplusplus)
extern "C" {
#endif

#if !defined(__arm__)
#define __arm__
#endif

/* IAR documentation states that __CPU_MODE__ is a predefined preprocessor
   symbol reflecting the selected CPU mode and is defined to 1 for Thumb and
   2 for ARM. */
#if !defined(__thumb__) && __CPU_MODE__ == 1
#define __thumb__
#endif

/* LD, ST */

inline uint32_t ddsrt_atomic_ld32 (const volatile ddsrt_atomic_uint32_t *x) { return x->v; }
inline uintptr_t ddsrt_atomic_ldptr (const volatile ddsrt_atomic_uintptr_t *x) { return x->v; }
inline void *ddsrt_atomic_ldvoidp (const volatile ddsrt_atomic_voidp_t *x) { return (void *) ddsrt_atomic_ldptr (x); }

inline void ddsrt_atomic_st32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) { x->v = v; }
inline void ddsrt_atomic_stptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) { x->v = v; }
inline void ddsrt_atomic_stvoidp (volatile ddsrt_atomic_voidp_t *x, void *v) { ddsrt_atomic_stptr (x, (uintptr_t) v); }

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
    register int result;
    asm volatile ("ldrex    r0, [%1]\n\t"      /*exclusive load of ptr */
                  "cmp      r0,  %2\n\t"       /*compare the oldval == *ptr */
#if defined(__thumb__)
                  "ite eq\n\t"
#endif
                  "strexeq  %0,  %3, [%1]\n\t" /*store if eq, strex+eq*/
#if defined(__thumb__)
                  "clrexne"
#endif
                  : "=&r" (result)
                  : "r"(&x->v), "r"(exp),"r"(des)
                  : "r0");
    return result == 0;
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
    return ddsrt_atomic_cas32 ((volatile ddsrt_atomic_uint32_t *) x, exp, des);
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
    return ddsrt_atomic_casptr ((volatile ddsrt_atomic_uintptr_t *) x, (uintptr_t) exp, (uintptr_t) des);
}

/* ADD */

inline uint32_t ddsrt_atomic_add32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
  register unsigned int result;
  asm volatile ("1: ldrex  %0,  [%1]\n\t"
                "add       %0,   %0,  %2\n\t"
                "strex     r1,   %0,  [%1]\n\t"
                "cmp       r1,   #0\n\t"
                "bne       1b"
                : "=&r" (result)
                : "r"(&x->v), "r"(v)
                : "r1");
   return result;
}

/**
 * @brief Increment (by given amount) an uintptr atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 *
 * @return The new value of the incremented variable
 */
inline uintptr_t ddsrt_atomic_addptr_nv (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
  return ddsrt_atomic_add32_nv ((volatile ddsrt_atomic_uint32_t *) x, v);
}

/**
 * @brief Increment (by given amount) a 32-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 */
inline void ddsrt_atomic_add32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
    (void) ddsrt_atomic_add32_nv (x, v);
}

/**
 * @brief Increment (by given amount) an uintptr atomically
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 */
inline void ddsrt_atomic_addptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
    (void) ddsrt_atomic_addptr_nv (x, v);
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
    return ddsrt_atomic_add32_nv (x, v) - v;
}

/**
 * @brief Increment (by given amount) an uintptr atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to increment
 * @param[in] v The value to increment by
 *
 * @return The old value of the incremented variable
 */
inline uint32_t ddsrt_atomic_addptr_ov (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
    return ddsrt_atomic_addptr_nv (x, v) - v;
}

/* SUB */

/**
 * @brief Decrement (by given amount) a 32-bit unsigned integer atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 *
 * @return The new value of the decremented variable
 */
inline uint32_t ddsrt_atomic_sub32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
    return ddsrt_atomic_add32_nv (x, -v);
}

/**
 * @brief Decrement (by given amount) an uintptr atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 *
 * @return The new value of the decremented variable
 */
inline uint32_t ddsrt_atomic_subptr_nv (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
    return ddsrt_atomic_addptr_nv (x, -v);
}

/**
 * @brief Decrement (by given amount) a 32-bit unsigned integer atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 *
 * @return The new value of the decremented variable
 */
inline uint32_t ddsrt_atomic_sub32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
    return ddsrt_atomic_add32_ov (x, -v);
}

/**
 * @brief Decrement (by given amount) an uintptr atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 *
 * @return The new value of the decremented variable
 */
inline uint32_t ddsrt_atomic_subptr_ov (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
    return ddsrt_atomic_addptr_ov (x, -v);
}

/**
 * @brief Decrement (by given amount) a 32-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 */
inline void ddsrt_atomic_sub32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
    ddsrt_atomic_add32 (x, -v);
}

/**
 * @brief Decrement (by given amount) an uintptr atomically
 *
 * @param[in,out] x Pointer to the variable to decrement
 * @param[in] v The value to decrement by
 */
inline void ddsrt_atomic_subptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
    ddsrt_atomic_addptr (x, -v);
}

/* INC */

inline uint32_t ddsrt_atomic_inc32_ov (volatile ddsrt_atomic_uint32_t *x) {
    return ddsrt_atomic_add32_ov (x, 1);
}

/**
 * @brief Increment (by one) an uintptr atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The old value of the incremented variable
 */
inline uintptr_t ddsrt_atomic_incptr_ov (volatile ddsrt_atomic_uintptr_t *x) {
    return ddsrt_atomic_addptr_ov (x, 1);
}
inline uint32_t ddsrt_atomic_inc32_nv (volatile ddsrt_atomic_uint32_t *x) {
    return ddsrt_atomic_add32_nv (x, 1);
}

/**
 * @brief Increment (by one) an uintptr atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The new value of the incremented variable
 */
inline uintptr_t ddsrt_atomic_incptr_nv (volatile ddsrt_atomic_uintptr_t *x) {
    return ddsrt_atomic_addptr_nv (x, 1);
}
inline void ddsrt_atomic_inc32 (volatile ddsrt_atomic_uint32_t *x) {
    (void) ddsrt_atomic_inc32_nv (x);
}

/**
 * @brief Increment (by one) an uintptr integer atomically
 *
 * @param[in,out] x Pointer to the variable to increment
 */
inline void ddsrt_atomic_incptr (volatile ddsrt_atomic_uintptr_t *x) {
    (void) ddsrt_atomic_incptr_nv (x);
}

/* DEC */

inline uint32_t ddsrt_atomic_dec32_ov (volatile ddsrt_atomic_uint32_t *x) {
    return ddsrt_atomic_sub32_ov (x, 1);
}

/**
 * @brief Decrement (by one) a 32-bit unsigned integer atomically and return its old value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The old value of the decremented variable
 */
inline uintptr_t ddsrt_atomic_decptr_ov (volatile ddsrt_atomic_uintptr_t *x) {
    return ddsrt_atomic_subptr_ov (x, 1);
}
inline uint32_t ddsrt_atomic_dec32_nv (volatile ddsrt_atomic_uint32_t *x) {
    return ddsrt_atomic_sub32_nv (x, 1);
}

/**
 * @brief Decrement (by one) an uintptr atomically and return its new value
 *
 * @param[in,out] x Pointer to the variable to increment
 *
 * @return The new value of the decremented variable
 */
inline uintptr_t ddsrt_atomic_decptr_nv (volatile ddsrt_atomic_uintptr_t *x) {
    return ddsrt_atomic_subptr_nv (x, 1);
}

/**
 * @brief Decrement (by one) a 32-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to decrement
 */
inline void ddsrt_atomic_dec32 (volatile ddsrt_atomic_uint32_t *x) {
    (void) ddsrt_atomic_dec32_nv (x);
}

/**
 * @brief Decrement (by one) an uintptr atomically
 *
 * @param[in,out] x Pointer to the variable to decrement
 */
inline void ddsrt_atomic_decptr (volatile ddsrt_atomic_uintptr_t *x) {
    (void) ddsrt_atomic_decptr_nv (x);
}

/* AND */

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
    do { oldval = x->v; newval = oldval & v; } while (!ddsrt_atomic_cas32 (x, oldval, newval));
    return oldval;
}

/**
 * @brief Bitwise AND an uintptr atomically and return the old value
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 *
 * @return The old value of the ANDed variable
 */
inline uintptr_t ddsrt_atomic_andptr_ov (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
    uintptr_t oldval, newval;
    do { oldval = x->v; newval = oldval & v; } while (!ddsrt_atomic_casptr (x, oldval, newval));
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
    uint32_t oldval, newval;
    do { oldval = x->v; newval = oldval & v; } while (!ddsrt_atomic_cas32 (x, oldval, newval));
    return newval;
}

/**
 * @brief Bitwise AND an uintptr atomically and return the new value
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 *
 * @return The new value of the ANDed variable
 */
inline uintptr_t ddsrt_atomic_andptr_nv (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
    uintptr_t oldval, newval;
    do { oldval = x->v; newval = oldval & v; } while (!ddsrt_atomic_casptr (x, oldval, newval));
    return newval;
}

/**
 * @brief Bitwise AND a 32-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 */
inline void ddsrt_atomic_and32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
    (void) ddsrt_atomic_and32_nv (x, v);
}

/**
 * @brief Bitwise AND an uintptr atomically
 *
 * @param[in,out] x Pointer to the variable to AND
 * @param[in] v The value to AND with the variable
 */
inline void ddsrt_atomic_andptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
    (void) ddsrt_atomic_andptr_nv (x, v);
}

/* OR */

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
    do { oldval = x->v; newval = oldval | v; } while (!ddsrt_atomic_cas32 (x, oldval, newval));
    return oldval;
}

/**
 * @brief Bitwise OR an uintptr atomically and return the old value
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 *
 * @return The old value of the ORed variable
 */
inline uintptr_t ddsrt_atomic_orptr_ov (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
    uintptr_t oldval, newval;
    do { oldval = x->v; newval = oldval | v; } while (!ddsrt_atomic_casptr (x, oldval, newval));
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
    uint32_t oldval, newval;
    do { oldval = x->v; newval = oldval | v; } while (!ddsrt_atomic_cas32 (x, oldval, newval));
    return newval;
}

/**
 * @brief Bitwise OR an uintptr atomically and return the new value
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 *
 * @return The new value of the ORed variable
 */
inline uintptr_t ddsrt_atomic_orptr_nv (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
    uintptr_t oldval, newval;
    do { oldval = x->v; newval = oldval | v; } while (!ddsrt_atomic_casptr (x, oldval, newval));
    return newval;
}

/**
 * @brief Bitwise OR a 32-bit unsigned integer atomically
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 */
inline void ddsrt_atomic_or32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
    (void) ddsrt_atomic_or32_nv (x, v);
}

/**
 * @brief Bitwise OR an uintptr atomically
 *
 * @param[in,out] x Pointer to the variable to OR
 * @param[in] v The value to OR with the variable
 */
inline void ddsrt_atomic_orptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
    (void) ddsrt_atomic_orptr_nv (x, v);
}

/* FENCES */

inline void ddsrt_atomic_fence (void) {
  __asm volatile ("dmb" : : : "memory");
}
inline void ddsrt_atomic_fence_ldld (void) {
    ddsrt_atomic_fence ();
}
inline void ddsrt_atomic_fence_stst (void) {
    ddsrt_atomic_fence ();
}
inline void ddsrt_atomic_fence_acq (void) {
    ddsrt_atomic_fence ();
}
inline void ddsrt_atomic_fence_rel (void) {
    ddsrt_atomic_fence ();
}

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_ATOMICS_ARM_H */
