// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef IDLC_GENERATOR_H
#define IDLC_GENERATOR_H

#include "options.h"
#include "idl/print.h"

struct idlc_generator_config {
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
};

typedef struct idlc_generator_config idlc_generator_config_t;

typedef const idlc_option_t **(*idlc_generator_options_t)(void);
typedef const idl_builtin_annotation_t **(*idlc_generator_annotations_t)(void);
typedef int(*idlc_generate_t)(const idl_pstate_t *, const idlc_generator_config_t *);

typedef struct idlc_generator_plugin idlc_generator_plugin_t;
struct idlc_generator_plugin {
  void *handle;
  idlc_generator_options_t generator_options; /* optional */
  idlc_generator_annotations_t generator_annotations; /* optional */
  idlc_generate_t generate;
};

int idlc_load_generator(idlc_generator_plugin_t *plugin, const char *lang);
void idlc_unload_generator(idlc_generator_plugin_t *plugin);

#endif /* IDLC_GENERATOR_H */
