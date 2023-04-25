// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef IDL_SCOPE_H
#define IDL_SCOPE_H

#include <stdbool.h>

#include "idl/tree.h"

typedef struct idl_scope idl_scope_t;

typedef struct idl_declaration idl_declaration_t;
struct idl_declaration {
  enum idl_declaration_kind {
    /** module */
    IDL_MODULE_DECLARATION,
    /** annotation */
    IDL_ANNOTATION_DECLARATION,
    /** const, enumerator, type, ... */
    IDL_SPECIFIER_DECLARATION,
    /** declarator, e.g. declarator of struct member or union element */
    IDL_INSTANCE_DECLARATION,
    /** introduced through use of non-absolute qualified name */
    IDL_USE_DECLARATION,
    /** enclosing scope, for convenience */
    IDL_SCOPE_DECLARATION,
    /** forward declarator (struct/union) */
    IDL_FORWARD_DECLARATION
  } kind;
  idl_declaration_t *next;
  const idl_scope_t *local_scope; /**< scope local to declaration */
  idl_name_t *name;
  idl_scoped_name_t *scoped_name;
  /* not a reference, used to populate forward declarations */
  idl_node_t *node;
  idl_scope_t *scope; /**< scope introduced by declaration (optional) */
};

typedef struct idl_import idl_import_t;
struct idl_import {
  idl_import_t *next;
  const idl_scope_t *scope;
};

struct idl_scope {
  enum idl_scope_kind {
    IDL_GLOBAL_SCOPE,
    IDL_MODULE_SCOPE,
    IDL_ANNOTATION_SCOPE,
    IDL_STRUCT_SCOPE,
    IDL_UNION_SCOPE
  } kind;
  const idl_scope_t *parent;
  const idl_name_t *name;
  struct {
    idl_declaration_t *first, *last;
  } declarations;
  struct {
    idl_import_t *first, *last;
  } imports;
};

IDL_EXPORT idl_scope_t *idl_scope(const void *node);
IDL_EXPORT idl_declaration_t *idl_declaration(const void *node);

struct idl_pstate;

#define IDL_FIND_IGNORE_CASE (1u<<0)
#define IDL_FIND_IGNORE_IMPORTS (1u<<1)
#define IDL_FIND_ANNOTATION (1u<<2)
#define IDL_FIND_SCOPE_DECLARATION (1u<<3)

IDL_EXPORT const idl_declaration_t *
idl_find(
  const struct idl_pstate *pstate,
  const idl_scope_t *scope,
  const idl_name_t *name,
  uint32_t flags);

IDL_EXPORT const idl_declaration_t *
idl_find_scoped_name(
  const struct idl_pstate *pstate,
  const idl_scope_t *scope,
  const idl_scoped_name_t *scoped_name,
  uint32_t flags);

IDL_EXPORT const idl_declaration_t *
idl_find_field_name(
  const struct idl_pstate *pstate,
  const idl_scope_t *scope,
  const idl_field_name_t *field_name,
  uint32_t flags);

#endif /* IDL_SCOPE_H */
