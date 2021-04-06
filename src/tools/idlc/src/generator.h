/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef GENERATOR_H
#define GENERATOR_H

#include <stdio.h>

#include "idl/processor.h"

#include <stdlib.h>
#include <string.h>
#if defined(_MSC_VER)
# include <malloc.h>
# define idlc_thread_local __declspec(thread)
#elif defined(__GNUC__) || (defined(__clang__) && __clang_major__ >= 2)
# if !defined(__FreeBSD__)
#   include <alloca.h>
# endif
# define idlc_thread_local __thread
#elif defined(__SUNPROC_C) || defined(__SUNPRO_CC)
# include <alloca.h>
# define idlc_thread_local __thread
#else
# error "Thread-local storage is not supported"
#endif

/** @private */
struct idlc_auto {
  void *src;
  size_t len;
  void *dest;
};

extern idlc_thread_local struct idlc_auto idlc_auto__;

#define AUTO(str) \
  ((idlc_auto__.src = (str)) \
    ? (idlc_auto__.len = strlen(idlc_auto__.src), \
       idlc_auto__.dest = alloca(idlc_auto__.len + 1), \
       memmove(idlc_auto__.dest, idlc_auto__.src, idlc_auto__.len + 1), \
       free(idlc_auto__.src), \
       idlc_auto__.dest) \
    : (NULL))

struct generator {
  const char *path;
  struct {
    FILE *handle;
    char *path;
  } header;
  struct {
    FILE *handle;
    char *path;
  } source;
};

#if _WIN32
__declspec(dllexport)
#endif
idl_retcode_t idlc_generate(const idl_pstate_t *pstate);

#if _WIN32
__declspec(dllexport)
#endif
idl_retcode_t generate_nosetup(const idl_pstate_t *pstate, struct generator *generator);

#endif /* GENERATOR_H */
