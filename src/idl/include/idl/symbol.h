// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef IDL_SYMBOL_H
#define IDL_SYMBOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "idl/export.h"

typedef struct idl_file idl_file_t;
struct idl_file {
  idl_file_t *next;
  char *name;
};

typedef struct idl_source idl_source_t;
struct idl_source {
  const idl_source_t *parent;
  idl_source_t *previous, *next;
  idl_source_t *includes;
  bool additional_directory; /**< file does not reside in working directory */
  const idl_file_t *path; /**< normalized path of filename in #line directive */
  const idl_file_t *file; /**< filename in #line directive */
};

typedef struct idl_position {
  const idl_source_t *source;
  /* for error reporting purposes, the "filename" provided in the #line
     directive must be kept. on includes, idlpp provides a (relative) filename
     with the proper flags, which becomes the source. user provided #line
     directives in the file are used merely for error reporting */
  const idl_file_t *file; /**< (alternate) filename in latest #line directive */
  uint32_t line;
  uint32_t column;
} idl_position_t;

typedef struct idl_location {
  idl_position_t first;
  idl_position_t last;
} idl_location_t;

/* symbols are there for the parser(s), nodes are there for the tree.
   all nodes are symbols, not all symbols are nodes */

typedef struct idl_symbol idl_symbol_t;
struct idl_symbol {
  idl_location_t location;
};

IDL_EXPORT const idl_location_t *idl_location(const void *symbol);

#endif /* IDL_SYMBOL_H */
