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
#ifndef OS_INLINE_H
#define OS_INLINE_H

/* We want to inline these, but we don't want to emit an externally visible
   symbol for them and we don't want warnings if we don't use them.

   It appears as if a plain "inline" will do just that in C99.

   In traditional GCC one had to use "extern inline" to achieve that effect,
   but that will cause an externally visible symbol to be emitted by a C99
   compiler.

   Starting with GCC 4.3, GCC conforms to the C99 standard if compiling in C99
   mode, unless -fgnu89-inline is specified. It defines __GNUC_STDC_INLINE__
   if "inline"/"extern inline" behaviour is conforming the C99 standard.

   So: GCC >= 4.3: choose between "inline" & "extern inline" based upon
   __GNUC_STDC_INLINE__; for GCCs < 4.2, rely on the traditional GCC behaviour;
   and for other compilers assume they behave conforming the standard if they
   advertise themselves as C99 compliant (use "inline"), and assume they do not
   support the inline keywords otherwise.

   GCC when not optimizing ignores "extern inline" functions. So we need to
   distinguish between optimizing & non-optimizing ... */

/* Defining OS_HAVE_INLINE is a supported way of overruling this file */
#ifndef OS_HAVE_INLINE

#if __STDC_VERSION__ >= 199901L
#  /* C99, but old GCC nonetheless doesn't implement C99 semantics ... */
#  if __GNUC__ && ! defined __GNUC_STDC_INLINE__
#    define OS_HAVE_INLINE 1
#    define OS_INLINE extern __inline__
#  else
#    define OS_HAVE_INLINE 1
#    define OS_INLINE inline
#  endif
#elif defined __STDC__ && defined __GNUC__ && ! defined __cplusplus
#  if __OPTIMIZE__
#    if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)
#      ifdef __GNUC_STDC_INLINE__
#        define OS_HAVE_INLINE 1
#        define OS_INLINE __inline__
#      else
#        define OS_HAVE_INLINE 1
#        define OS_INLINE extern __inline__
#      endif
#    else
#      define OS_HAVE_INLINE 1
#      define OS_INLINE extern __inline__
#    endif
#  endif
#endif

#if ! OS_HAVE_INLINE
#define OS_INLINE
#endif

#endif /* not defined OS_HAVE_INLINE */

#endif /* OS_INLINE_H */
