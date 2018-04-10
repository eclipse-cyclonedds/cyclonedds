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
#ifndef NN_INLINE_H
#define NN_INLINE_H

#ifdef NN_SUPPRESS_C99_INLINE

#define NN_HAVE_C99_INLINE 0

#else
/* We want to inline these, but we don't want to emit an exernally
   visible symbol for them and we don't want warnings if we don't use
   them.

   It appears as if a plain "inline" will do just that in C99.

   In traditional GCC one had to use "extern inline" to achieve that
   effect, but that will cause an externally visible symbol to be
   emitted by a C99 compiler.

   Starting with GCC 4.3, GCC conforms to the C99 standard if
   compiling in C99 mode, unless -fgnu89-inline is specified. It
   defines __GNUC_STDC_INLINE__ if "inline"/"extern inline" behaviour
   is conforming the C99 standard.

   So: GCC >= 4.3: choose between "inline" & "extern inline" based
   upon __GNUC_STDC_INLINE__; for GCCs < 4.2, rely on the traditional
   GCC behaiour; and for other compilers assume they behave conforming
   the standard if they advertise themselves as C99 compliant (use
   "inline"), and assume they do not support the inline keywords
   otherwise.

   GCC when not optimizing ignores "extern inline" functions. So we
   need to distinguish between optimizing & non-optimizing ... */
#if __GNUC__
#  if __OPTIMIZE__
#    if 1 || __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)
#      ifdef __GNUC_STDC_INLINE__
#        define NN_HAVE_C99_INLINE 1
#        define NN_C99_INLINE inline
#      else
#        define NN_HAVE_C99_INLINE 1
#        define NN_C99_INLINE extern inline
#      endif
#    else
#      define NN_HAVE_C99_INLINE 1
#      define NN_C99_INLINE extern inline
#    endif
#  endif
#elif __STDC_VERSION__ >= 199901L
#  define NN_HAVE_C99_INLINE 1
#  define NN_C99_INLINE inline
#endif

#endif /* NN_SUPPRESS_C99_INLINE */

#if ! NN_HAVE_C99_INLINE
#define NN_C99_INLINE
#endif

#endif /* NN_INLINE_H */
