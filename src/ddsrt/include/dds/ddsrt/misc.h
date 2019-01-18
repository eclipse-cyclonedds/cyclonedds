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
#ifndef DDSRT_MISC_H
#define DDSRT_MISC_H

#include <limits.h>

#if defined (__cplusplus)
extern "C" {
#endif

#if defined(__GNUC__) && ((__GNUC__ * 100) + __GNUC_MINOR__) >= 402
# define DDSRT_GNUC_STR(s) #s
# define DDSRT_GNUC_JOINSTR(x,y) DDSRT_GNUC_DIAG_STR(x ## y)
# define DDSRT_GNUC_DO_PRAGMA(x) _Pragma (#x)
# define DDSRT_GNUC_PRAGMA(x) DDSRT_GNUC_DO_PRAGMA(GCC diagnostic x)
# if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
#   define DDSRT_WARNING_GNUC_OFF(x) \
      DDSRT_GNUC_PRAGMA(push) \
      DDSRT_GNUC_PRAGMA(ignored DDSRT_GNUC_JOINSTR(-W,x))
#   define DDSRT_WARNING_GNUC_ON(x) \
      DDSRT_GNUC_PRAGMA(pop)
# else
#   define DDSRT_WARNING_GNUC_OFF(x) \
      DDSRT_GNUC_PRAGMA(ignored DDSRT_GNUC_JOINSTR(-W,x))
#   define DDSRT_WARNING_GNUC_ON(x) \
      DDSRT_GNUC_PRAGMA(warning DDSRT_GNUC_JOINSTR(-W,x))
# endif
#else
# define DDSRT_WARNING_GNUC_OFF(x)
# define DDSRT_WARNING_GNUC_ON(x)
#endif


#if defined(_MSC_VER)
# define DDSRT_WARNING_MSVC_OFF(x) \
    __pragma (warning(push)) \
    __pragma (warning(disable: ## x))
# define DDSRT_WARNING_MSVC_ON(x) \
    __pragma (warning(pop))
#else
# define DDSRT_WARNING_MSVC_OFF(x)
# define DDSRT_WARNING_MSVC_ON(x)
#endif

/**
 * @brief Calculate maximum value of an integer type
 *
 * A somewhat complex, but efficient way to calculate the maximum value of an
 * integer type at compile time.
 *
 * For unsigned numerical types the first part up to XOR is enough. The second
 * part is to make up for signed numerical types.
 */
#define DDSRT_MAX_INTEGER(T) \
    ((T)(((T)~0) ^ ((T)!((T)~0 > 0) << (CHAR_BIT * sizeof(T) - 1))))
/**
 * @brief Calculate minimum value of an integer type
 */
#define DDSRT_MIN_INTEGER(T) \
    ((-DDSRT_MAX_INTEGER(T)) - 1)

/**
 * @brief Macro to disable unused argument warnings
 */
#define DDSRT_UNUSED_ARG(a) (void)(a)

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_MISC_H */
