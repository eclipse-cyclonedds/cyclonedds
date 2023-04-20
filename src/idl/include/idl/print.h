// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef IDL_PRINT_H
#define IDL_PRINT_H

#include <stdio.h>

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#if defined(_WIN32)
# include <malloc.h>
#elif defined(__GNUC__) || (defined(__clang__) && __clang_major__ >= 2)
# if !defined(__FreeBSD__)
#   include <alloca.h>
# endif
#elif defined(__SUNPROC_C) || defined(__SUNPRO_CC)
# include <alloca.h>
#endif

#include "idl/export.h"

typedef int(*idl_print_t)(char *, size_t, const void *, void *);

/** @private */
IDL_EXPORT int idl_printa_arguments__(
  char **strp, idl_print_t print, const void *object, void *user_data);

/** @private */
IDL_EXPORT size_t idl_printa_size__(void);

/** @private */
IDL_EXPORT char **idl_printa_strp__(void);

/** @private */
IDL_EXPORT int idl_printa__(void);

#define IDL_COMMA__() ,
#define IDL_SHIFT__(shift_, ...) shift_
#define IDL_PICK__(drop1_, drop2_, pick_, ...) pick_
#define IDL_EVALUATE__(eval_) eval_ /* required for MSVC */
#define IDL_EXPAND__(...) IDL_EXPAND_AGAIN__(__VA_ARGS__)
#define IDL_EXPAND_AGAIN__(...) IDL_EVALUATE__(IDL_PICK__(__VA_ARGS__, ))
#define IDL_MAYBE__(...) \
  IDL_EXPAND__(IDL_COMMA__ IDL_SHIFT__(__VA_ARGS__, ) (), NULL, __VA_ARGS__)

#if _MSC_VER
# define IDL_ALLOCA__(size_) (__pragma (warning(suppress: 6255)) alloca(size_))
# define IDL_PRINTA IDL_PRINTA__
#else
# define IDL_ALLOCA__(size_) (alloca(size_))
# define IDL_PRINTA(strp_, print_, ...) \
    IDL_PRINTA__((strp_), (print_), __VA_ARGS__, )
#endif

#define IDL_PRINTA__(strp_, print_, object_, ...) \
  ((idl_printa_arguments__((strp_), (print_), (object_), IDL_MAYBE__(__VA_ARGS__)) >= 0) \
   ? ((*(idl_printa_strp__()) = IDL_ALLOCA__(idl_printa_size__())), \
       idl_printa__()) \
   : (-1))

IDL_EXPORT int idl_print__(
  char **strp, idl_print_t print, const void *object, void *user_data);

#define IDL_PRINT__(strp_, print_, object_, ...) \
  idl_print__((strp_), (print_), (object_), IDL_MAYBE__(__VA_ARGS__))

#define IDL_PRINT(strp_, print_, ...) \
  IDL_PRINT__(strp_, print_, __VA_ARGS__, )

#endif /* IDL_PRINT_H */
