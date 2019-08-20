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
inline int ddsrt_atomic_casptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t exp, uintptr_t des) {
    return ddsrt_atomic_cas32 ((volatile ddsrt_atomic_uint32_t *) x, exp, des);
}
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
inline uintptr_t ddsrt_atomic_addptr_nv (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
    return ddsrt_atomic_add32_nv ((volatile ddsrt_atomic_uint32_t *) x, v);
}
inline void *ddsrt_atomic_addvoidp_nv (volatile ddsrt_atomic_voidp_t *x, ptrdiff_t v) {
    return (void *) ddsrt_atomic_addptr_nv ((volatile ddsrt_atomic_uintptr_t *) x, (uintptr_t) v);
}
inline void ddsrt_atomic_add32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
    (void) ddsrt_atomic_add32_nv (x, v);
}
inline uint32_t ddsrt_atomic_add32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
    return ddsrt_atomic_add32_nv (x, v) - v;
}
inline void ddsrt_atomic_addptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
    (void) ddsrt_atomic_addptr_nv (x, v);
}
inline void ddsrt_atomic_addvoidp (volatile ddsrt_atomic_voidp_t *x, ptrdiff_t v) {
    (void) ddsrt_atomic_addvoidp_nv (x, v);
}

/* SUB */

inline uint32_t ddsrt_atomic_sub32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
    return ddsrt_atomic_add32_nv (x, -v);
}
inline uintptr_t ddsrt_atomic_subptr_nv (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
    return ddsrt_atomic_addptr_nv (x, -v);
}
inline void *ddsrt_atomic_subvoidp_nv (volatile ddsrt_atomic_voidp_t *x, ptrdiff_t v) {
    return ddsrt_atomic_addvoidp_nv (x, -v);
}
inline void ddsrt_atomic_sub32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
    ddsrt_atomic_add32 (x, -v);
}
inline void ddsrt_atomic_subptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
    ddsrt_atomic_subptr (x, -v);
}
inline void ddsrt_atomic_subvoidp (volatile ddsrt_atomic_voidp_t *x, ptrdiff_t v) {
    ddsrt_atomic_subvoidp (x, -v);
}

/* INC */

inline uint32_t ddsrt_atomic_inc32_ov (volatile ddsrt_atomic_uint32_t *x) {
    return ddsrt_atomic_add32_nv (x, 1) - 1;
}
inline uint32_t ddsrt_atomic_inc32_nv (volatile ddsrt_atomic_uint32_t *x) {
    return ddsrt_atomic_add32_nv (x, 1);
}
inline uintptr_t ddsrt_atomic_incptr_nv (volatile ddsrt_atomic_uintptr_t *x) {
    return ddsrt_atomic_addptr_nv (x, 1);
}
inline void ddsrt_atomic_inc32 (volatile ddsrt_atomic_uint32_t *x) {
    (void) ddsrt_atomic_inc32_nv (x);
}
inline void ddsrt_atomic_incptr (volatile ddsrt_atomic_uintptr_t *x) {
    (void) ddsrt_atomic_incptr_nv (x);
}

/* DEC */

inline uint32_t ddsrt_atomic_dec32_ov (volatile ddsrt_atomic_uint32_t *x) {
    return ddsrt_atomic_sub32_nv (x, 1) + 1;
}
inline uint32_t ddsrt_atomic_dec32_nv (volatile ddsrt_atomic_uint32_t *x) {
    return ddsrt_atomic_sub32_nv (x, 1);
}
inline uintptr_t ddsrt_atomic_decptr_nv (volatile ddsrt_atomic_uintptr_t *x) {
    return ddsrt_atomic_subptr_nv (x, 1);
}
inline void ddsrt_atomic_dec32 (volatile ddsrt_atomic_uint32_t *x) {
    (void) ddsrt_atomic_dec32_nv (x);
}
inline void ddsrt_atomic_decptr (volatile ddsrt_atomic_uintptr_t *x) {
    (void) ddsrt_atomic_decptr_nv (x);
}

/* AND */

inline uint32_t ddsrt_atomic_and32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
    uint32_t oldval, newval;
    do { oldval = x->v; newval = oldval & v; } while (!ddsrt_atomic_cas32 (x, oldval, newval));
    return oldval;
}
inline uintptr_t ddsrt_atomic_andptr_ov (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
    uintptr_t oldval, newval;
    do { oldval = x->v; newval = oldval & v; } while (!ddsrt_atomic_casptr (x, oldval, newval));
    return oldval;
}
inline uint32_t ddsrt_atomic_and32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
    uint32_t oldval, newval;
    do { oldval = x->v; newval = oldval & v; } while (!ddsrt_atomic_cas32 (x, oldval, newval));
    return newval;
}
inline uintptr_t ddsrt_atomic_andptr_nv (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
    uintptr_t oldval, newval;
    do { oldval = x->v; newval = oldval & v; } while (!ddsrt_atomic_casptr (x, oldval, newval));
    return newval;
}
inline void ddsrt_atomic_and32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
    (void) ddsrt_atomic_and32_nv (x, v);
}
inline void ddsrt_atomic_andptr (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
    (void) ddsrt_atomic_andptr_nv (x, v);
}

/* OR */

inline uint32_t ddsrt_atomic_or32_ov (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
    uint32_t oldval, newval;
    do { oldval = x->v; newval = oldval | v; } while (!ddsrt_atomic_cas32 (x, oldval, newval));
    return oldval;
}
inline uintptr_t ddsrt_atomic_orptr_ov (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
    uintptr_t oldval, newval;
    do { oldval = x->v; newval = oldval | v; } while (!ddsrt_atomic_casptr (x, oldval, newval));
    return oldval;
}
inline uint32_t ddsrt_atomic_or32_nv (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
    uint32_t oldval, newval;
    do { oldval = x->v; newval = oldval | v; } while (!ddsrt_atomic_cas32 (x, oldval, newval));
    return newval;
}
inline uintptr_t ddsrt_atomic_orptr_nv (volatile ddsrt_atomic_uintptr_t *x, uintptr_t v) {
    uintptr_t oldval, newval;
    do { oldval = x->v; newval = oldval | v; } while (!ddsrt_atomic_casptr (x, oldval, newval));
    return newval;
}
inline void ddsrt_atomic_or32 (volatile ddsrt_atomic_uint32_t *x, uint32_t v) {
    (void) ddsrt_atomic_or32_nv (x, v);
}
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
