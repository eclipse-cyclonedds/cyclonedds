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
#ifndef OS_DECL_ATTRIBUTES_H
#define OS_DECL_ATTRIBUTES_H

#define OS_GNUC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)

#ifndef __has_attribute
# if defined __GNUC__ && !defined __clang__
#  define __has_attribute(x) 1  /* Compatibility with GCC compilers that don't have __has_attribute. */
# else
#  define __has_attribute(x) 0  /* Compatibility with compilers that don't have __has_attribute. */
# endif
#endif

#ifndef __attribute_malloc__
# if __has_attribute(malloc)
#  define __attribute_malloc__ __attribute__((__malloc__))
# else
#  define __attribute_malloc__ /* Ignore. */
# endif
#endif

#ifndef __attribute_unused__
# if __has_attribute(unused)
#  define __attribute_unused__ __attribute__((__unused__))
# else
#  define __attribute_unused__ /* Ignore. */
# endif
#endif

#ifndef __attribute_noreturn__
# if __has_attribute(noreturn)
#  define __attribute_noreturn__ __attribute__((__noreturn__))
# else
#  define __attribute_noreturn__ /* Ignore. */
# endif
#endif

#if defined FIX_FAULTY_ATTRIBUTE_NONNULL || defined __clang__ || OS_GNUC_VERSION >= 50200
/* Some platforms have a faulty definition of the __nonnull macro where params
 * are expanded within brackets, causing the parameters to be treated as a comma-
 * expression. If FIX_FAULTY_ATTRIBUTE_NONNULL is defined, the macro is undeffed
 * so our own definition will be used instead. */
# undef __nonnull
#endif

#ifndef __nonnull
# if __has_attribute(nonnull)
#  define __nonnull(params) __attribute__((__nonnull__ params))
#  define __nonnull_all__ __attribute__((__nonnull__))
# else
#  define __nonnull(params) /* Ignore. */
#  define __nonnull_all__ /* Ignore. */
# endif
#else
# define __nonnull_all__ __attribute__((__nonnull__))
#endif

#ifndef __attribute_returns_nonnull__
# if __has_attribute(returns_nonnull) && (defined __clang__ || OS_GNUC_VERSION >= 40900)
#  define __attribute_returns_nonnull__ __attribute__((__returns_nonnull__))
# else
#  define __attribute_returns_nonnull__ /* Ignore. */
# endif
#endif

#ifndef __attribute_alloc_size__
/* Silence GCC <= V4.2.4, which reports that it has the attribute, but nags that it ignores it. */
# if __has_attribute(alloc_size) && OS_GNUC_VERSION > 40204
#  define __attribute_alloc_size__(params) __attribute__ ((__alloc_size__ params))
# else
#  define __attribute_alloc_size__(params) /* Ignore. */
# endif
#endif

#ifndef __attribute_const__
# if __has_attribute(const)
#  define __attribute_const__ __attribute__ ((__const__))
# else
#  define __attribute_const__ /* Ignore. */
# endif
#endif

#ifndef __attribute_pure__
# if __has_attribute(pure)
#  define __attribute_pure__ __attribute__ ((__pure__))
# else
#  define __attribute_pure__ /* Ignore. */
# endif
#endif

#ifndef __attribute_format__
# if __has_attribute(format)
#  define __attribute_format__(params) __attribute__ ((__format__ params))
# else
#  define __attribute_format__(params) /* Ignore. */
# endif
#endif

#ifndef __attribute_warn_unused_result__
# if __has_attribute(warn_unused_result)
#  define __attribute_warn_unused_result__ __attribute__ ((__warn_unused_result__))
# else
#  define __attribute_warn_unused_result__ /* Ignore. */
# endif
#endif

#ifndef __attribute_assume_aligned__
# if __has_attribute(assume_aligned)
#  define __attribute_assume_aligned__(params) __attribute__ ((__assume_aligned__ params))
# else
#  define __attribute_assume_aligned__(params) /* Ignore. */
# endif
#endif

#include "os_decl_attributes_sal.h"

#undef OS_GNUC_VERSION
#endif
