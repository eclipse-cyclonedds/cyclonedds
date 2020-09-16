/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef IDL_SCOPE_H
#define IDL_SCOPE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "idl/tree.h"

typedef struct idl_name idl_name_t;
struct idl_name {
  idl_location_t location;
  char *identifier;
};

typedef struct idl_scoped_name idl_scoped_name_t;
struct idl_scoped_name {
  idl_location_t location;
  bool absolute; /**< wheter scoped name is fully qualified or not */
  char *flat;
  struct {
    size_t length; /**< number of identifiers that make up the scoped name */
    idl_name_t **names; /**< identifiers that make up the scoped name */
  } path;
};

typedef struct idl_scope idl_scope_t;

/**
 * @brief Type of entry in table of identifiers within a scope.
 *
 * Must be one of:
 *   - \c IDL_MODULE
 *   - \c IDL_DECLARATION
 *   - \c IDL_INSTANCE
 *   - \c IDL_REFERENCE
 *   - \c IDL_SCOPE
 */
typedef idl_mask_t idl_entry_type_t;

#define IDL_DECLARATION (IDL_DECL)
#define IDL_INSTANCE (1ull)
#define IDL_REFERENCE (2ull)
#define IDL_SCOPE (3ull)

typedef struct idl_entry idl_entry_t;
struct idl_entry {
  idl_entry_type_t type;
  idl_entry_t *next;
  idl_name_t *name;
  const idl_node_t *node; /**< node associated with entry (if applicable) */
  idl_scope_t *scope; /**< scope introduced by entry (if applicable) */
};

typedef struct idl_inherit_spec idl_inherit_spec_t;
struct idl_inherit_spec {
  idl_inherit_spec_t *next;
  idl_scoped_name_t *scoped_name;
  const idl_scope_t *scope;
};

/**
 * @brief Type of scope.
 *
 * Must be one of:
 *   - \c IDL_GLOBAL
 *   - \c IDL_MODULE
 *   - \c IDL_STRUCT
 *   - \c IDL_UNION
 */
typedef idl_mask_t idl_scope_type_t;

#define IDL_GLOBAL (1ull)

struct idl_scope {
  idl_scope_type_t type;
  const idl_scope_t *parent;
  const idl_name_t *name;
  struct {
    idl_entry_t *first, *last;
  } table;
  idl_inherit_spec_t *inherit_spec; /**< inherited scopes */
};

IDL_EXPORT idl_scope_t *idl_scope(const void *node);

#endif /* IDL_SCOPE_H */
