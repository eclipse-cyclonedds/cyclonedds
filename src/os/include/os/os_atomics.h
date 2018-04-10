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
#ifndef OS_ATOMICS_H
#define OS_ATOMICS_H

#include <stddef.h>
#include <limits.h>

#include "os/os_defs.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* Note: os_atomics_inlines.c overrules OS_HAVE_INLINE, VDDS_INLINE and
   OS_ATOMICS_OMIT_FUNCTIONS */

#if ! OS_HAVE_INLINE && ! defined OS_ATOMICS_OMIT_FUNCTIONS
#define OS_ATOMICS_OMIT_FUNCTIONS 1
#endif

#if ! OS_ATOMIC_SUPPORT && (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) >= 40100
#include "os/os_atomics_gcc.h"
#endif

#if ! OS_ATOMIC_SUPPORT && defined _WIN32
/* Windows.h causes HUGE problems when included too early, primarily
   because you can't include only a subset and later include the rest
*/
#undef OS_HAVE_INLINE
#undef VDDS_INLINE
#define VDDS_INLINE
#include "os_atomics_win32.h"
#endif

#if ! OS_ATOMIC_SUPPORT && defined __sun
#include "os_atomics_solaris.h"
#endif

#if ! OS_ATOMIC_SUPPORT && defined __INTEGRITY
#include "os_atomics_integrity.h"
#endif

#if ! OS_ATOMIC_SUPPORT && defined __VXWORKS__
#include "os_atomics_vxworks.h"
#endif

#if ! OS_ATOMIC_SUPPORT && defined __GNUC__ && defined __i386
#include "os_atomics_gcc_x86.h"
#endif

#if ! OS_ATOMIC_SUPPORT &&                                              \
  ((defined __GNUC__ && defined __ppc) ||                               \
   (defined __vxworks && defined __PPC__))
/* VxWorks uses GCC but removed the __GNUC__ macro ... */
#include "os_atomics_gcc_ppc.h"
#endif

#if ! OS_ATOMIC_SUPPORT && defined __GNUC__ && defined __sparc__
#include "os_atomics_gcc_sparc.h"
#endif

#if ! OS_ATOMIC_SUPPORT && defined __GNUC__ && defined __arm__
#include "os_atomics_gcc_arm.h"
#endif

#if ! OS_ATOMIC_SUPPORT
#error "No support for atomic operations on this platform"
#endif

#if ! OS_HAVE_INLINE

/* LD, ST */
OSAPI_EXPORT uint32_t os_atomic_ld32 (const volatile os_atomic_uint32_t *x);
#if OS_ATOMIC64_SUPPORT
OSAPI_EXPORT uint64_t os_atomic_ld64 (const volatile os_atomic_uint64_t *x);
#endif
OSAPI_EXPORT uintptr_t os_atomic_ldptr (const volatile os_atomic_uintptr_t *x);
OSAPI_EXPORT void *os_atomic_ldvoidp (const volatile os_atomic_voidp_t *x);
OSAPI_EXPORT void os_atomic_st32 (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
OSAPI_EXPORT void os_atomic_st64 (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
OSAPI_EXPORT void os_atomic_stptr (volatile os_atomic_uintptr_t *x, uintptr_t v);
OSAPI_EXPORT void os_atomic_stvoidp (volatile os_atomic_voidp_t *x, void *v);
/* INC */
OSAPI_EXPORT void os_atomic_inc32 (volatile os_atomic_uint32_t *x);
#if OS_ATOMIC64_SUPPORT
OSAPI_EXPORT void os_atomic_inc64 (volatile os_atomic_uint64_t *x);
#endif
OSAPI_EXPORT void os_atomic_incptr (volatile os_atomic_uintptr_t *x);
OSAPI_EXPORT uint32_t os_atomic_inc32_nv (volatile os_atomic_uint32_t *x);
#if OS_ATOMIC64_SUPPORT
OSAPI_EXPORT uint64_t os_atomic_inc64_nv (volatile os_atomic_uint64_t *x);
#endif
OSAPI_EXPORT uintptr_t os_atomic_incptr_nv (volatile os_atomic_uintptr_t *x);
/* DEC */
OSAPI_EXPORT void os_atomic_dec32 (volatile os_atomic_uint32_t *x);
#if OS_ATOMIC64_SUPPORT
OSAPI_EXPORT void os_atomic_dec64 (volatile os_atomic_uint64_t *x);
#endif
OSAPI_EXPORT void os_atomic_decptr (volatile os_atomic_uintptr_t *x);
OSAPI_EXPORT uint32_t os_atomic_dec32_nv (volatile os_atomic_uint32_t *x);
#if OS_ATOMIC64_SUPPORT
OSAPI_EXPORT uint64_t os_atomic_dec64_nv (volatile os_atomic_uint64_t *x);
#endif
OSAPI_EXPORT uintptr_t os_atomic_decptr_nv (volatile os_atomic_uintptr_t *x);
OSAPI_EXPORT uint32_t os_atomic_dec32_ov (volatile os_atomic_uint32_t *x);
#if OS_ATOMIC64_SUPPORT
OSAPI_EXPORT uint64_t os_atomic_dec64_ov (volatile os_atomic_uint64_t *x);
#endif
OSAPI_EXPORT uintptr_t os_atomic_decptr_ov (volatile os_atomic_uintptr_t *x);
/* ADD */
OSAPI_EXPORT void os_atomic_add32 (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
OSAPI_EXPORT void os_atomic_add64 (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
OSAPI_EXPORT void os_atomic_addptr (volatile os_atomic_uintptr_t *x, uintptr_t v);
OSAPI_EXPORT void os_atomic_addvoidp (volatile os_atomic_voidp_t *x, ptrdiff_t v);
OSAPI_EXPORT uint32_t os_atomic_add32_nv (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
OSAPI_EXPORT uint64_t os_atomic_add64_nv (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
OSAPI_EXPORT uintptr_t os_atomic_addptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v);
OSAPI_EXPORT void *os_atomic_addvoidp_nv (volatile os_atomic_voidp_t *x, ptrdiff_t v);
/* SUB */
OSAPI_EXPORT void os_atomic_sub32 (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
OSAPI_EXPORT void os_atomic_sub64 (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
OSAPI_EXPORT void os_atomic_subptr (volatile os_atomic_uintptr_t *x, uintptr_t v);
OSAPI_EXPORT void os_atomic_subvoidp (volatile os_atomic_voidp_t *x, ptrdiff_t v);
OSAPI_EXPORT uint32_t os_atomic_sub32_nv (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
OSAPI_EXPORT uint64_t os_atomic_sub64_nv (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
OSAPI_EXPORT uintptr_t os_atomic_subptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v);
OSAPI_EXPORT void *os_atomic_subvoidp_nv (volatile os_atomic_voidp_t *x, ptrdiff_t v);
/* AND */
OSAPI_EXPORT void os_atomic_and32 (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
OSAPI_EXPORT void os_atomic_and64 (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
OSAPI_EXPORT void os_atomic_andptr (volatile os_atomic_uintptr_t *x, uintptr_t v);
OSAPI_EXPORT uint32_t os_atomic_and32_ov (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
OSAPI_EXPORT uint64_t os_atomic_and64_ov (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
OSAPI_EXPORT uintptr_t os_atomic_andptr_ov (volatile os_atomic_uintptr_t *x, uintptr_t v);
OSAPI_EXPORT uint32_t os_atomic_and32_nv (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
OSAPI_EXPORT uint64_t os_atomic_and64_nv (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
OSAPI_EXPORT uintptr_t os_atomic_andptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v);
/* OR */
OSAPI_EXPORT void os_atomic_or32 (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
OSAPI_EXPORT void os_atomic_or64 (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
OSAPI_EXPORT void os_atomic_orptr (volatile os_atomic_uintptr_t *x, uintptr_t v);
OSAPI_EXPORT uint32_t os_atomic_or32_ov (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
OSAPI_EXPORT uint64_t os_atomic_or64_ov (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
OSAPI_EXPORT uintptr_t os_atomic_orptr_ov (volatile os_atomic_uintptr_t *x, uintptr_t v);
OSAPI_EXPORT uint32_t os_atomic_or32_nv (volatile os_atomic_uint32_t *x, uint32_t v);
#if OS_ATOMIC64_SUPPORT
OSAPI_EXPORT uint64_t os_atomic_or64_nv (volatile os_atomic_uint64_t *x, uint64_t v);
#endif
OSAPI_EXPORT uintptr_t os_atomic_orptr_nv (volatile os_atomic_uintptr_t *x, uintptr_t v);
/* CAS */
OSAPI_EXPORT int os_atomic_cas32 (volatile os_atomic_uint32_t *x, uint32_t exp, uint32_t des);
#if OS_ATOMIC64_SUPPORT
OSAPI_EXPORT int os_atomic_cas64 (volatile os_atomic_uint64_t *x, uint64_t exp, uint64_t des);
#endif
OSAPI_EXPORT int os_atomic_casptr (volatile os_atomic_uintptr_t *x, uintptr_t exp, uintptr_t des);
OSAPI_EXPORT int os_atomic_casvoidp (volatile os_atomic_voidp_t *x, void *exp, void *des);
/* FENCES */
OSAPI_EXPORT void os_atomic_fence (void);
OSAPI_EXPORT void os_atomic_fence_acq (void);
OSAPI_EXPORT void os_atomic_fence_rel (void);

#endif /* OS_HAVE_INLINE */

#if defined (__cplusplus)
}
#endif

#endif /* OS_ATOMICS_H */
