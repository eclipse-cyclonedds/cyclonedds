// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef IDL_DEFS_H
#define IDL_DEFS_H

#include <stdbool.h>
#include <stdint.h>
#include "idl/processor.h"

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct idlc_option idlc_option_t;
struct generator;

struct idlc_option {
  enum {
    IDLC_FLAG, /**< flag-only, i.e. (sub)option without argument */
    IDLC_STRING,
    IDLC_FUNCTION,
  } type;
  union {
    int *flag;
    const char **string;
    int (*function)(const idlc_option_t *, const char *);
  } store;
  char option; /**< option, i.e. "o" in "-o". "-h" is reserved */
  char *suboption; /**< name of suboption, i.e. "mount" in "-o mount" */
  char *argument;
  char *help;
};

typedef struct {
  char *output_dir; /* path to write completed files */
  char* base_dir; /* Path to start reconstruction of dir structure */

  /** Flag to indicate if xtypes type information is included in the generated types */
  bool generate_type_info;

  /** Generating xtypes typeinfo and typemap is logically a language independent operation that
   various language backends will need to do, but at the same time doing so requires XCDR2
  serialization, which, for an IDL compiler written in C, really means relying on the C backend.
  Passing a pointer to a generator function is a reasonable way of avoiding the layering problems
  this introduces. May be a null pointer */
  idl_retcode_t (*generate_typeinfo_typemap) (const idl_pstate_t *pstate, const idl_node_t *node, idl_typeinfo_typemap_t *result);
} idlc_generator_config_t;

typedef const idlc_option_t **(*idlc_generator_options_t)(void);
typedef const idl_builtin_annotation_t **(*idlc_generator_annotations_t)(void);
typedef int(*idlc_generate_t)(const idl_pstate_t *, const idlc_generator_config_t *);

#if defined(__cplusplus)
}
#endif

#endif /* IDL_DEFS_H */
