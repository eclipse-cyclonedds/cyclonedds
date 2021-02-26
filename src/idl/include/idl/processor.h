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
#ifndef IDL_COMPILER_H
#define IDL_COMPILER_H

/**
 * @file
 * Types and functions for the IDL compiler.
 */

#include <stdarg.h>
#include <stddef.h>

#include "idl/export.h"
#include "idl/retcode.h"
#include "idl/tree.h"
#include "idl/scope.h"
#include "idl/visit.h"

/* enable "#pragma keylist" for backwards compatibility */
#define IDL_FLAG_KEYLIST (1u<<0)
/* case-sensitive extension can be used to allow e.g. field names in structs
   and unions that differ solely in case from the name of the respective
   struct or union. i.e. "struct FOO_ { octet foo_[42]; };" */
#define IDL_FLAG_CASE_SENSITIVE (1u<<1)
/* enable building block extended data types */
#define IDL_FLAG_EXTENDED_DATA_TYPES (1u<<2)
/* enable building block anonymous types */
#define IDL_FLAG_ANONYMOUS_TYPES (1u<<3)
/* enable building block annotations */
#define IDL_FLAG_ANNOTATIONS (1u<<4)
/* flag used by idlc to indicate end-of-buffer (private) */
#define IDL_WRITE (1u<<31)

typedef struct idl_buffer idl_buffer_t;
struct idl_buffer {
  char *data;
  size_t size; /**< total number of bytes available */
  size_t used; /**< number of bytes used */
};

typedef struct idl_pstate idl_pstate_t;
struct idl_pstate {
  bool keylists;
  bool annotations;
  uint32_t flags; /**< processor options */
  idl_file_t *paths; /**< normalized paths used in include statements */
  idl_file_t *files; /**< filenames used in #line directives */
  idl_source_t *sources;
  idl_scope_t *global_scope, *annotation_scope, *scope;
  void *directive;
  idl_node_t *builtin_root, *root;
  idl_buffer_t buffer; /**< dynamically sized input buffer */
  struct {
    enum {
      IDL_SCAN,
      /** scanning preprocessor directive */
      IDL_SCAN_DIRECTIVE = (1<<7),
      IDL_SCAN_DIRECTIVE_NAME,
      /** scanning #line directive */
      IDL_SCAN_LINE = (IDL_SCAN_DIRECTIVE | (1<<6)),
      IDL_SCAN_FILENAME,
      IDL_SCAN_FLAGS,
      IDL_SCAN_EXTRA_TOKENS,
      /** scanning #pragma directive */
      IDL_SCAN_PRAGMA = (IDL_SCAN_DIRECTIVE | (1<<5)),
      IDL_SCAN_UNKNOWN_PRAGMA,
      /** scanning #pragma keylist directive */
      IDL_SCAN_KEYLIST = (IDL_SCAN_PRAGMA | (1<<4)),
      IDL_SCAN_DATA_TYPE,
      IDL_SCAN_NAME,
      IDL_SCAN_SCOPE,
      IDL_SCAN_KEY,
      IDL_SCAN_FIELD,
      IDL_SCAN_ACCESS,
      /** scanning IDL */
      IDL_SCAN_GRAMMAR = (1<<8),
      /* scanning "@annotation" or scoped name after "@" in IDL */
      /** expect identifier, i.e. annotation in "@annotation" */
      IDL_SCAN_ANNOTATION,
      /** expect identifier, i.e. foo in "@annotation foo" */
      IDL_SCAN_ANNOTATION_NAME,
      /** expect scope or identifier, i.e. :: in "@::" and foo in "@foo" */
      IDL_SCAN_ANNOTATION_APPL,
      /** expect scope, i.e. :: in "@foo::bar::" */
      IDL_SCAN_ANNOTATION_APPL_SCOPE,
      /** expect identifier in scoped name, i.e. foo in "@foo::bar" */
      IDL_SCAN_ANNOTATION_APPL_SCOPED_NAME,
      /** final identifier in scoped name, i.e. bar in "@foo::bar" or "@bar" */
      IDL_SCAN_ANNOTATION_APPL_NAME,
      /** end of input */
      IDL_EOF = (1<<9)
    } state;
    const char *cursor;
    const char *limit;
    idl_position_t position;
  } scanner;
  struct {
    enum {
      IDL_PARSE, /**< default state */
      IDL_PARSE_ANNOTATION,
      IDL_PARSE_ANNOTATION_BODY,
      IDL_PARSE_EXISTING_ANNOTATION_BODY,
      IDL_PARSE_ANNOTATION_APPL,
      IDL_PARSE_ANNOTATION_APPL_PARAMS,
      IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS
    } state;
    void *yypstate; /**< state of Bison generated parser */
  } parser;
};

typedef struct idl_builtin_annotation idl_builtin_annotation_t;
struct idl_builtin_annotation {
  const char *syntax; /**< precise syntax */
  const char *summary; /**< brief yet significant description */
  const idl_annotation_callback_t callback;
};

IDL_EXPORT idl_retcode_t
idl_create_pstate(
  uint32_t flags,
  const idl_builtin_annotation_t *annotations,
  idl_pstate_t **pstatep);

IDL_EXPORT void
idl_delete_pstate(idl_pstate_t *pstate);

IDL_EXPORT idl_retcode_t
idl_parse(idl_pstate_t *pstate);

IDL_EXPORT idl_retcode_t
idl_parse_string(idl_pstate_t *pstate, const char *str);

IDL_EXPORT void
idl_verror(idl_pstate_t *pstate, const idl_location_t *loc, const char *fmt, va_list ap);

IDL_EXPORT void
idl_error(idl_pstate_t *pstate, const idl_location_t *loc, const char *fmt, ...);

IDL_EXPORT void
idl_warning(idl_pstate_t *pstate, const idl_location_t *loc, const char *fmt, ...);

#endif /* IDL_COMPILER_H */
