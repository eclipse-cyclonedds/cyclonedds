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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expression.h"
#include "scope.h"
#include "tree.h"

bool idl_is_type_spec(const void *node, idl_mask_t mask)
{
  idl_node_t *n = (idl_node_t *)node;
  assert(!(mask & ~(IDL_DECL | IDL_TYPE)) ||
          (mask & IDL_CONSTR_TYPE) == IDL_CONSTR_TYPE ||
          (mask & IDL_STRUCT) == IDL_STRUCT ||
          (mask & IDL_UNION) == IDL_UNION ||
          (mask & IDL_ENUM) == IDL_ENUM ||
          (mask & IDL_TEMPL_TYPE) == IDL_TEMPL_TYPE ||
          (mask & IDL_BASE_TYPE) == IDL_BASE_TYPE);
  if (!n)
    return false;
  if (!(n->mask & IDL_TYPE))
    return false;
  if ((n->mask & IDL_DECL) && idl_is_module(n->parent)) {
    mask |= IDL_TYPE | IDL_DECL;
  } else {
    mask |= IDL_TYPE;
    /* type specifiers cannot have sibling nodes unless its also a declared
       type and declared in a module */
    assert(!n->previous);
    assert(!n->next);
    /* type specifiers must have a parent node */
    assert(n->parent || (n->mask & IDL_DECL));
  }
  return (n->mask & mask) == mask;
}

bool idl_is_masked(const void *node, idl_mask_t mask)
{
  return node && (((idl_node_t *)node)->mask & mask) == mask;
}

void *idl_reference(void *node)
{
  if (node) {
    assert(!((idl_node_t *)node)->deleted);
    ((idl_node_t *)node)->references++;
    return node;
  }
  return NULL;
}

const char *idl_identifier(const void *node)
{
  if (!idl_is_declaration(node))
    return NULL;
  if (idl_is_module(node))
    return ((const idl_module_t *)node)->name->identifier;
  if (idl_is_struct(node))
    return ((const idl_struct_t *)node)->name->identifier;
  if (idl_is_union(node))
    return ((const idl_union_t *)node)->name->identifier;
  if (idl_is_enum(node))
    return ((const idl_enum_t *)node)->name->identifier;
  if (idl_is_enumerator(node))
    return ((const idl_enumerator_t *)node)->name->identifier;
  if (idl_is_declarator(node))
    return ((const idl_declarator_t *)node)->name->identifier;
  if (idl_is_forward(node))
    return ((const idl_forward_t *)node)->name->identifier;
  return NULL;
}

const idl_location_t *idl_location(const void *node)
{
  return &((const idl_node_t *)node)->location;
}

void *
idl_unalias(const void *node)
{
  idl_node_t *n = (idl_node_t *)node;

  while (n && (n->mask & IDL_TYPEDEF)) {
    n = ((idl_typedef_t *)n)->type_spec;
  }

  return n;
}

idl_scope_t *
idl_scope(const void *node)
{
  const idl_node_t *n = (const idl_node_t *)node;

  if (!n) {
    return NULL;
  } else if (idl_is_masked(n, IDL_DECL)) {
    /* declarations must have a scope */
    assert(n->scope);
    return (idl_scope_t *)n->scope;
  }
  return NULL;
}

void *idl_parent(const void *node)
{
  const idl_node_t *n = (const idl_node_t *)node;
  if (!n)
    return NULL;
  /* non-declarators must have a parent */
  assert((n->mask & IDL_DECL) || n->parent);
  return (idl_node_t *)n->parent;
}

void *idl_previous(const void *node)
{
  const idl_node_t *n = (const idl_node_t *)node;
  if (!n)
    return NULL;
  /* declarators can have siblings */
  if ((n->mask & (IDL_DECL)))
    return n->previous;
  /* as do expressions (or constants) if specifying array sizes */
  if ((n->mask & (IDL_EXPR | IDL_CONST)) && idl_is_declarator(n->parent))
    return n->previous;
  assert(!n->previous);
  assert(!n->next);
  return NULL;
}

void *idl_next(const void *node)
{
  const idl_node_t *n = (const idl_node_t *)node;
  if (!n)
    return NULL;
  /* declarators can have siblings */
  if ((n->mask & (IDL_DECL)))
    return n->next;
  /* as do expressions (or constants) if specifying array sizes */
  if ((n->mask & (IDL_EXPR | IDL_CONST)) && idl_is_declarator(n->parent))
    return n->next;
  assert(!n->previous);
  assert(!n->next);
  return n->next;
}

idl_retcode_t
idl_create_node(
  idl_processor_t *proc,
  void *nodeptr,
  size_t size,
  idl_mask_t mask,
  const idl_location_t *location,
  idl_delete_t destructor)
{
  idl_node_t *node;

  (void)proc;
  assert(size >= sizeof(*node));
  if (!(node = calloc(1, size)))
    return IDL_RETCODE_NO_MEMORY;
  if (mask & IDL_DECL)
    node->scope = proc->scope;
  node->mask = mask;
  node->location = *location;
  node->destructor = destructor;
  node->references = 1;

  *((idl_node_t **)nodeptr) = node;
  return IDL_RETCODE_OK;
}

static void delete_node(void *node)
{
  if (node) {
    idl_node_t *n = (idl_node_t *)node;
    if (!n->deleted) {
      n->deleted = 1;
      /* delete siblings */
      delete_node(n->next);
    }
    n->references--;
    assert(n->references >= 0);
    if (!n->references) {
      /* delete annotations */
      delete_node(n->annotations);
      /* self-destruct */
      if (n->destructor)
        n->destructor(n);
    }
  }
}

void idl_delete_node(void *node)
{
  delete_node(node);
}

bool idl_is_declaration(const void *node)
{
  return idl_is_masked(node, IDL_DECL);
}

bool idl_is_module(const void *node)
{
  const idl_module_t *n = (const idl_module_t *)node;
  if (!idl_is_masked(n, IDL_MODULE))
    return false;
  assert(idl_is_masked(n, IDL_DECL|IDL_MODULE));
  /* modules must have an identifier */
  assert(n->name->identifier);
  /* modules must have at least on definition */
  assert(idl_is_masked(n->definitions, IDL_DECL));
  return true;
}

static void delete_module(void *node)
{
  idl_module_t *n = (idl_module_t *)node;
  delete_node(n->definitions);
  idl_delete_name(n->name);
  free(n);
}

idl_retcode_t
idl_finalize_module(
  idl_processor_t *proc,
  idl_module_t *node,
  idl_location_t *location,
  void *definitions)
{
  idl_exit_scope(proc);
  node->node.location.last = location->last;

  /* modules can be reopened and are passed if only one definitions exists */
  if (((idl_node_t *)definitions)->parent) {
    assert(idl_is_masked(definitions, IDL_MODULE));
    return IDL_RETCODE_OK;
  }

  /* ignore definitions that were in the list already (reopened modules) */
  for (idl_node_t *n = definitions; n; n = n->next) {
    assert(!n->parent);
    n->parent = (idl_node_t*)node;
  }

  if (node->definitions) {
    idl_node_t *last = node->definitions;
    for (last = node->definitions; last->next; last = last->next) ;
    last->next = definitions;
    ((idl_node_t *)definitions)->previous = last;
  } else {
    node->definitions = definitions;
  }
  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_create_module(
  idl_processor_t *proc,
  idl_module_t **nodep,
  idl_location_t *location,
  idl_name_t *name)
{
  idl_retcode_t ret;
  idl_module_t *node;
  idl_scope_t *scope;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECL|IDL_MODULE;

  { /* an identifier declaring a module is considered to be defined by its
       first occurence in a scope. subsequent occurrences of a module
       declaration with the same identifier within the same scope reopens the
       module and hence its scope, allowing additional definitions to be added
       to it */
    idl_entry_t *entry = NULL;
    entry = idl_find(proc, NULL, name);
    if (entry) {
      /* name clashes are handled in idl_declare */
      if (idl_is_masked(entry->node, IDL_MODULE)) {
        assert(entry->scope);
        idl_enter_scope(proc, entry->scope);
        *nodep = idl_reference((void*)entry->node);
        idl_delete_name(name);
        return IDL_RETCODE_OK;
      }
    }
  }

  ret = idl_create_scope(proc, &scope, IDL_MODULE, name);
  if (ret != IDL_RETCODE_OK)
    goto err_scope;
  ret = idl_create_node(proc, &node, size, mask, location, &delete_module);
  if (ret != IDL_RETCODE_OK)
    goto err_node;
  node->name = name;
  ret = idl_declare(proc, NULL, IDL_MODULE, name, node, scope);
  if (ret != IDL_RETCODE_OK)
    goto err_declare;

  idl_enter_scope(proc, scope);
  *nodep = node;
  return IDL_RETCODE_OK;
err_declare:
  free(node);
err_node:
  idl_delete_scope(scope);
err_scope:
  return ret;
}

static void delete_const(void *node)
{
  idl_const_t *n = (idl_const_t *)node;
  idl_delete_node(n->const_expr);
  idl_delete_node(n->type_spec);
  idl_delete_name(n->name);
  free(n);
}

idl_retcode_t
idl_create_const(
  idl_processor_t *proc,
  idl_const_t **nodeptr,
  idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_name_t *name,
  idl_const_expr_t *const_expr)
{
  idl_retcode_t ret;
  idl_const_t *node;
  idl_const_expr_t *constval = NULL;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECL|IDL_CONST;

  assert(idl_is_masked(type_spec, IDL_BASE_TYPE) ||
         idl_is_masked(type_spec, IDL_ENUM));

  ret = idl_create_node(proc, &node, size, mask, location, &delete_const);
  if (ret != IDL_RETCODE_OK)
    goto err_node;
  node->name = name;
  assert(idl_is_masked(type_spec, IDL_TYPE));
  node->type_spec = type_spec;
  if (!idl_scope(type_spec))
    type_spec->parent = (idl_node_t*)node;
  /* evaluate constant expression */
  ret = idl_evaluate(proc, &constval, const_expr, type_spec->mask);
  if (ret != IDL_RETCODE_OK)
    goto err_evaluate;
  node->const_expr = constval;
  if (!idl_scope(constval))
    constval->parent = (idl_node_t*)node;
  ret = idl_declare(proc, NULL, IDL_DECLARATION, name, node, NULL);
  if (ret != IDL_RETCODE_OK)
    goto err_declare;

  *nodeptr = node;
  return IDL_RETCODE_OK;
err_declare:
  //idl_unreference(const_expr);
err_evaluate:
  free(node);
err_node:
  return ret;
}

bool idl_is_templ_type(const void *node)
{
  if (!idl_is_masked(node, IDL_TEMPL_TYPE))
    return false;
#ifndef NDEBUG
  assert(idl_is_masked(node, IDL_TYPE));
  idl_mask_t mask = ((idl_node_t *)node)->mask & ~IDL_TYPE;
  assert(mask == IDL_STRING || mask == IDL_SEQUENCE);
#endif
  return true;
}

bool idl_is_sequence(const void *node)
{
  if (!idl_is_masked(node, IDL_SEQUENCE))
    return false;
  assert(idl_is_masked(node, IDL_TYPE));
  return true;
}

static void delete_sequence(void *node)
{
  delete_node(((idl_sequence_t *)node)->type_spec);
  free(node);
}

idl_retcode_t
idl_create_sequence(
  idl_processor_t *proc,
  idl_sequence_t **nodeptr,
  idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_constval_t *constval)
{
  idl_retcode_t ret;
  idl_sequence_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_TYPE|IDL_SEQUENCE;

  ret = idl_create_node(proc, &node, size, mask, location, &delete_sequence);
  if (ret != IDL_RETCODE_OK)
    goto err_node;
  node->type_spec = type_spec;
  assert(idl_is_masked(type_spec, IDL_TYPE));
  if (!idl_scope(type_spec))
    type_spec->parent = (idl_node_t*)node;

  if (!constval) {
    node->maximum = 0u;
  } else {
    assert(idl_is_masked(constval, IDL_CONST|IDL_ULLONG));
    node->maximum = constval->value.ullng;
    delete_node(constval);
  }
  *nodeptr = node;
  return IDL_RETCODE_OK;
err_node:
  return ret;
}

bool idl_is_string(const void *node)
{
  return idl_is_masked(node, IDL_TYPE|IDL_STRING);
}

static void delete_string(void *node)
{
  free(node);
}

idl_retcode_t
idl_create_string(
  idl_processor_t *proc,
  idl_string_t **nodeptr,
  idl_location_t *location,
  idl_constval_t *constval)
{
  idl_retcode_t ret;
  idl_string_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_TYPE|IDL_STRING;

  ret = idl_create_node(proc, &node, size, mask, location, &delete_string);
  if (ret != IDL_RETCODE_OK)
    goto err_node;

  if (!constval) {
    node->maximum = 0u;
  } else {
    assert(!constval->node.parent);
    assert(idl_is_masked(constval, IDL_CONST|IDL_ULLONG));
    node->maximum = constval->value.ullng;
    delete_node(constval);
  }
  *nodeptr = node;
  return IDL_RETCODE_OK;
err_node:
  return ret;
}

bool idl_is_struct(const void *node)
{
  idl_struct_t *n = (idl_struct_t *)node;
  if (!idl_is_masked(n, IDL_STRUCT))
    return false;
  if (idl_is_masked(n, IDL_FORWARD))
    return false;
  assert(idl_is_masked(n, IDL_DECL|IDL_TYPE|IDL_STRUCT));
  /* structs must have an identifier */
  assert(n->name && n->name->identifier);
  /* structs must have at least one member */
  /* FIXME: unless building block extended data types is enabled */
  //assert(idl_is_masked(n->members, IDL_DECL|IDL_MEMBER));
  return true;
}

static void delete_struct(void *node)
{
  idl_struct_t *n = (idl_struct_t *)node;
  delete_node(n->base_type);
  delete_node(n->members);
  idl_delete_name(n->name);
  free(n);
}

idl_retcode_t
idl_finalize_struct(
  idl_processor_t *proc,
  idl_struct_t *node,
  idl_location_t *location,
  idl_member_t *members)
{
  idl_exit_scope(proc);
  node->node.location.last = location->last;
  if (members) {
    node->members = members;
    for (idl_node_t *n = (idl_node_t *)members; n; n = n->next) {
      assert(!n->parent);
      n->parent = (idl_node_t *)node;
    }
  }
  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_create_struct(
  idl_processor_t *proc,
  idl_struct_t **nodep,
  idl_location_t *location,
  idl_name_t *name,
  idl_inherit_spec_t *inherit_spec)
{
  idl_retcode_t ret;
  idl_struct_t *node, *base_type = NULL;
  idl_scope_t *scope;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECL|IDL_TYPE|IDL_CONSTR_TYPE|IDL_STRUCT;

  if (inherit_spec) {
    idl_entry_t *entry;
    /* struct inheritance is introduced in IDL4 */
    assert(proc->flags & IDL_FLAG_EXTENDED_DATA_TYPES);
    assert(!inherit_spec->next);
    ret = idl_resolve(proc, &entry, inherit_spec->scoped_name);
    if (ret != IDL_RETCODE_OK)
      return ret;
    assert(entry);
    base_type = idl_unalias(entry->node);
    if (!idl_is_masked(base_type, IDL_STRUCT) ||
         idl_is_masked(base_type, IDL_FORWARD))
    {
      idl_error(proc, &inherit_spec->scoped_name->location,
        "Scoped name '%s' does not resolve to a struct",
        inherit_spec->scoped_name->flat);
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
    base_type = (idl_struct_t *)entry->node;
  }

  ret = idl_create_scope(proc, &scope, IDL_STRUCT, name);
  if (ret != IDL_RETCODE_OK)
    goto err_scope;
  if (inherit_spec)
    ret = idl_inherit(proc, scope, inherit_spec);
  if (ret != IDL_RETCODE_OK)
    goto err_inherit;
  ret = idl_create_node(proc, &node, size, mask, location, &delete_struct);
  if (ret != IDL_RETCODE_OK)
    goto err_node;
  ret = idl_declare(proc, NULL, IDL_DECLARATION, name, node, scope);
  if (ret != IDL_RETCODE_OK)
    goto err_declare;

  node->name = name;
  node->base_type = idl_reference(base_type);
  idl_enter_scope(proc, scope);
  *nodep = node;
  return IDL_RETCODE_OK;
err_declare:
  free(node);
err_node:
err_inherit:
  idl_delete_scope(scope);
err_scope:
  return ret;
}

bool idl_is_member(const void *node)
{
  const idl_member_t *n = (const idl_member_t *)node;
  if (!idl_is_masked(n, IDL_MEMBER))
    return false;
  assert(idl_is_masked(n, IDL_DECL|IDL_MEMBER));
  /* members must have a parent node which is a struct */
  assert(idl_is_masked(n->node.parent, IDL_DECL|IDL_TYPE|IDL_STRUCT));
  /* members must have a type specifier */
  assert(idl_is_masked(n->type_spec, IDL_TYPE));
  /* members must have at least one declarator */
  assert(idl_is_masked(n->declarators, IDL_DECL|IDL_DECLARATOR));
  return true;
}

static void delete_member(void *node)
{
  idl_member_t *n = (idl_member_t *)node;
  delete_node(n->type_spec);
  delete_node(n->declarators);
  free(n);
}

idl_retcode_t
idl_create_member(
  idl_processor_t *proc,
  idl_member_t **nodeptr,
  idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_declarator_t *declarators)
{
  idl_retcode_t ret;
  idl_member_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECL|IDL_MEMBER;

  ret = idl_create_node(proc, &node, size, mask, location, &delete_member);
  if (ret != IDL_RETCODE_OK)
    goto err_node;
  assert(idl_is_masked(type_spec, IDL_TYPE));
  node->type_spec = type_spec;
  if (!idl_scope(type_spec))
    type_spec->parent = (idl_node_t*)node;
  assert(declarators);
  node->declarators = declarators;
  for (idl_node_t *n = (idl_node_t *)declarators; n; n = n->next) {
    assert(!n->parent);
    assert(n->mask & IDL_DECL);
    n->parent = (idl_node_t *)node;
    // FIXME: embedded structs have a scope, fix when implementing IDL3.5
    ret = idl_declare(proc, NULL, IDL_INSTANCE, ((idl_declarator_t *)n)->name, node, NULL);
    if (ret != IDL_RETCODE_OK)
      goto err_declare;
  }

  *nodeptr = node;
  return IDL_RETCODE_OK;
err_declare:
  free(node);
err_node:
  return ret;
}

bool idl_is_forward(const void *node)
{
  idl_forward_t *n = (idl_forward_t *)node;
  if (!idl_is_masked(n, IDL_FORWARD))
    return false;
  assert(idl_is_masked(n, IDL_DECL|IDL_TYPE|IDL_STRUCT) ||
         idl_is_masked(n, IDL_DECL|IDL_TYPE|IDL_UNION));
  return true;
}

static void delete_forward(void *node)
{
  idl_forward_t *n = (idl_forward_t *)node;
  delete_node(n->constr_type);
  idl_delete_name(n->name);
  free(n);
}

idl_retcode_t
idl_create_forward(
  idl_processor_t *proc,
  idl_forward_t **nodeptr,
  idl_location_t *location,
  idl_mask_t mask,
  idl_name_t *name)
{
  idl_retcode_t ret;
  idl_forward_t *node;
  static const size_t size = sizeof(*node);

  assert(mask == IDL_STRUCT || mask == IDL_UNION);
  mask |= IDL_DECL|IDL_TYPE|IDL_FORWARD;

  ret = idl_create_node(proc, &node, size, mask, location, &delete_forward);
  if (ret != IDL_RETCODE_OK)
    goto err_node;
  node->name = name;
  ret = idl_declare(proc, NULL, IDL_DECLARATION, name, node, NULL);
  if (ret != IDL_RETCODE_OK)
    goto err_declare;

  *nodeptr = node;
  return IDL_RETCODE_OK;
err_declare:
  free(node);
err_node:
  return ret;
}

bool idl_is_case_label(const void *node)
{
  idl_case_label_t *n = (idl_case_label_t *)node;
  if (!idl_is_masked(n, IDL_CASE_LABEL))
    return false;
  assert(idl_is_masked(n, IDL_DECL|IDL_CASE_LABEL));
  /* case labels must have a parent which is a case */
  assert(idl_is_masked(n->node.parent, IDL_DECL|IDL_CASE));
  /* case labels may have an expression (default case does not) */
  if (n->const_expr) {
    assert(idl_is_masked(n->const_expr, IDL_EXPR) ||
           idl_is_masked(n->const_expr, IDL_CONST));
  }
  return true;
}

static void delete_case_label(void *node)
{
  idl_case_label_t *n = (idl_case_label_t *)node;
  delete_node(n->const_expr);
  free(n);
}

idl_retcode_t
idl_create_case_label(
  idl_processor_t *proc,
  idl_case_label_t **nodeptr,
  idl_location_t *location,
  idl_const_expr_t *const_expr)
{
  idl_retcode_t ret;
  idl_case_label_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECL|IDL_CASE_LABEL;

  ret = idl_create_node(proc, &node, size, mask, location, &delete_case_label);
  if (ret != IDL_RETCODE_OK)
    return ret;
  node->const_expr = const_expr;
  if (const_expr && !idl_scope(const_expr))
    const_expr->parent = (idl_node_t *)node;

  *nodeptr = node;
  return IDL_RETCODE_OK;
}

bool idl_is_case(const void *node)
{
  idl_case_t *n = (idl_case_t *)node;
  if (!idl_is_masked(n, IDL_CASE))
    return false;
  assert(idl_is_masked(n, IDL_DECL|IDL_CASE));
  /* cases must have a parent which is a union */
  assert(idl_is_masked(n->node.parent, IDL_DECL|IDL_TYPE|IDL_UNION));
  /* cases must have at least one case label */
  assert(idl_is_masked(n->case_labels, IDL_DECL|IDL_CASE_LABEL));
  /* cases must have exactly one declarator */
  assert(idl_is_masked(n->declarator, IDL_DECL|IDL_DECLARATOR));
  return true;
}

bool idl_is_default_case(const void *node)
{
  const idl_case_label_t *n;
  if (!idl_is_case(node))
    return false;
  n = ((const idl_case_t *)node)->case_labels;
  for (; n; n = (const idl_case_label_t *)n->node.next) {
    if (!n->const_expr)
      return true;
  }
  return false;
}

static void delete_case(void *node)
{
  idl_case_t *n = (idl_case_t *)node;
  delete_node(n->case_labels);
  delete_node(n->type_spec);
  delete_node(n->declarator);
  free(n);
}

idl_retcode_t
idl_finalize_case(
  idl_processor_t *proc,
  idl_case_t *node,
  idl_location_t *location,
  idl_case_label_t *case_labels)
{
  (void)proc;
  node->node.location = *location;
  node->case_labels = case_labels;
  for (idl_node_t *n = (idl_node_t*)case_labels; n; n = n->next)
    n->parent = (idl_node_t*)node;
  return IDL_RETCODE_OK;
  // FIXME: warn for and ignore duplicate labels
  // FIXME: warn for and ignore for labels combined with default
}

idl_retcode_t
idl_create_case(
  idl_processor_t *proc,
  idl_case_t **nodeptr,
  idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_declarator_t *declarator)
{
  idl_retcode_t ret;
  idl_case_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECL|IDL_CASE;

  ret = idl_create_node(proc, &node, size, mask, location, &delete_case);
  if (ret != IDL_RETCODE_OK)
    goto err_node;
  node->type_spec = type_spec;
  if (!idl_scope(type_spec))
    type_spec->parent = (idl_node_t *)node;
  node->declarator = declarator;
  assert(!declarator->node.parent);
  declarator->node.parent = (idl_node_t *)node;
  assert(!declarator->node.next);
  ret = idl_declare(proc, NULL, IDL_INSTANCE, declarator->name, node, NULL);
  if (ret != IDL_RETCODE_OK)
    goto err_declare;

  *nodeptr = node;
  return IDL_RETCODE_OK;
err_declare:
  free(node);
err_node:
  return ret;
}

bool idl_is_union(const void *node)
{
  const idl_union_t *n = (const idl_union_t *)node;
  if (!idl_is_masked(n, IDL_UNION))
    return false;
  if (idl_is_masked(n, IDL_FORWARD))
    return false;
  assert(idl_is_masked(n, IDL_DECL|IDL_TYPE|IDL_UNION));
  /* unions must have an identifier */
  assert(n->name && n->name->identifier);
  /* unions must have a type specifier */
  assert(idl_is_masked(n->switch_type_spec, IDL_TYPE));
  /* unions must have at least one case */
  assert(idl_is_masked(n->cases, IDL_DECL|IDL_CASE));
  return true;
}

static void delete_union(void *node)
{
  idl_union_t *n = (idl_union_t *)node;
  delete_node(n->switch_type_spec);
  delete_node(n->cases);
  idl_delete_name(n->name);
  free(n);
}

idl_retcode_t
idl_finalize_union(
  idl_processor_t *proc,
  idl_union_t *node,
  idl_location_t *location,
  idl_case_t *cases)
{
  idl_retcode_t ret = IDL_RETCODE_OK;
  idl_case_t *default_case = NULL;
  idl_switch_type_spec_t *switch_type_spec;
  idl_mask_t mask;

  switch_type_spec = idl_unalias(node->switch_type_spec);
  assert(switch_type_spec);
  if (idl_is_masked(switch_type_spec, IDL_ENUM)) {
    mask = IDL_ENUMERATOR;
  } else {
    assert(idl_is_masked(switch_type_spec, IDL_BASE_TYPE) &&
          !idl_is_masked(switch_type_spec, IDL_FLOATING_PT_TYPE));
    mask = switch_type_spec->mask & (IDL_BASE_TYPE|(IDL_BASE_TYPE - 1u));
  }

  assert(cases);
  for (idl_case_t *c = cases; c; c = idl_next(c)) {
    /* iterate case labels and evaluate constant expressions */
    idl_case_label_t *null_label = NULL;
    /* determine if null-label is present */
    for (idl_case_label_t *cl = c->case_labels; cl; cl = idl_next(cl)) {
      if (!cl->const_expr) {
        null_label = cl;
        if (default_case) {
          idl_error(proc, idl_location(cl),
            "error about default case!");
          return IDL_RETCODE_SEMANTIC_ERROR;
        } else {
          default_case = c;
        }
        break;
      }
    }
    /* evaluate constant expressions */
    for (idl_case_label_t *cl = c->case_labels; cl; cl = idl_next(cl)) {
      if (cl->const_expr) {
        if (null_label) {
          idl_warning(proc, idl_location(cl),
            "Label in combination with null-label is not useful");
        }
        idl_constval_t *constval = NULL;
        ret = idl_evaluate(proc, (idl_node_t**)&constval, cl->const_expr, mask);
        if (ret != IDL_RETCODE_OK)
          return ret;
        if (mask == IDL_ENUMERATOR) {
          if (switch_type_spec != constval->node.parent) {
            idl_error(proc, idl_location(cl),
              "Enumerator of different enum type");
            return IDL_RETCODE_SEMANTIC_ERROR;
          }
        }
        cl->const_expr = (idl_node_t*)constval;
        if (!idl_scope(constval))
          constval->node.parent = (idl_node_t*)cl;
      }
    }
    assert(!c->node.parent);
    c->node.parent = (idl_node_t *)node;
  }

  node->node.location.last = location->last;
  node->cases = cases;

  // FIXME: for C++ the lowest value must be known. if beneficial for other
  //        language bindings we may consider marking that value or perhaps
  //        offer convenience functions to do so

  idl_exit_scope(proc);
  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_create_union(
  idl_processor_t *proc,
  idl_union_t **nodeptr,
  idl_location_t *location,
  idl_name_t *name,
  idl_switch_type_spec_t *type_spec)
{
  idl_retcode_t ret;
  idl_scope_t *scope;
  idl_union_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECL|IDL_TYPE|IDL_CONSTR_TYPE|IDL_UNION;

  //
  // extended data-type block allows for octet and some other ones as well..
  //

  ret = idl_create_scope(proc, &scope, IDL_UNION, name);
  if (ret != IDL_RETCODE_OK)
    goto err_scope;
  ret = idl_create_node(proc, &node, size, mask, location, &delete_union);
  if (ret != IDL_RETCODE_OK)
    goto err_node;
  node->name = name;
  node->switch_type_spec = type_spec;
  if (!idl_scope(type_spec))
    type_spec->parent = (idl_node_t *)node;
  ret = idl_declare(proc, NULL, IDL_DECLARATION, name, node, scope);
  if (ret != IDL_RETCODE_OK)
    goto err_declare;

  idl_enter_scope(proc, scope);
  *nodeptr = node;
  return IDL_RETCODE_OK;
err_declare:
  free(node);
err_node:
  idl_delete_scope(scope);
err_scope:
  return ret;
}

bool idl_is_enumerator(const void *node)
{
  const idl_enumerator_t *n = (const idl_enumerator_t *)node;
  return idl_is_masked(n, IDL_DECL|IDL_ENUMERATOR);
}

static void delete_enumerator(void *ptr)
{
  idl_enumerator_t *node = (idl_enumerator_t *)ptr;
  idl_delete_name(node->name);
  free(node);
}

idl_retcode_t
idl_create_enumerator(
  idl_processor_t *proc,
  idl_enumerator_t **nodeptr,
  idl_location_t *location,
  idl_name_t *name)
{
  idl_retcode_t ret;
  idl_enumerator_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECL|IDL_ENUMERATOR;

  ret = idl_create_node(proc, &node, size, mask, location, &delete_enumerator);
  if (ret != IDL_RETCODE_OK)
    goto err_node;
  node->name = name;
  ret = idl_declare(proc, NULL, IDL_DECLARATION, name, node, NULL);
  if (ret != IDL_RETCODE_OK)
    goto err_declare;

  *nodeptr = node;
  return IDL_RETCODE_OK;
err_declare:
  free(node);
err_node:
  return ret;
}

bool idl_is_enum(const void *node)
{
  const idl_enum_t *n = (const idl_enum_t *)node;
  return idl_is_masked(n, IDL_DECL|IDL_ENUM);
}

static void delete_enum(void *node)
{
  idl_enum_t *n = (idl_enum_t *)node;
  delete_node(n->enumerators);
  idl_delete_name(n->name);
  free(n);
}

idl_retcode_t
idl_create_enum(
  idl_processor_t *proc,
  idl_enum_t **nodeptr,
  idl_location_t *location,
  idl_name_t *name,
  idl_enumerator_t *enumerators)
{
  idl_retcode_t ret;
  idl_enum_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECL|IDL_TYPE|IDL_CONSTR_TYPE|IDL_ENUM;
  uint32_t value = 0;

  ret = idl_create_node(proc, &node, size, mask, location, &delete_enum);
  if (ret != IDL_RETCODE_OK)
    goto err_node;
  node->name = name;
  node->enumerators = enumerators;
  assert(enumerators);

  for (idl_enumerator_t *p = enumerators; p; p = idl_next(p), value++) {
    p->node.parent = (idl_node_t*)node;
    if (p->value)
      value = p->value;
    else
      p->value = value;
    for (idl_enumerator_t *q = enumerators; q && q != p; q = idl_next(q)) {
      if (p->value != q->value)
        continue;
      idl_error(proc, idl_location(p),
        "Value of enumerator '%s' clashes with the value of enumerator '%s'",
        p->name->identifier, q->name->identifier);
      goto err_duplicate;
    }
  }

  ret = idl_declare(proc, NULL, IDL_DECLARATION, name, node, NULL);
  if (ret != IDL_RETCODE_OK)
    goto err_declare;

  *nodeptr = node;
  return IDL_RETCODE_OK;
err_declare:
err_duplicate:
  free(node);
err_node:
  return ret;
}

bool idl_is_typedef(const void *node)
{
  const idl_typedef_t *n = (const idl_typedef_t *)node;
  if (!idl_is_masked(n, IDL_TYPEDEF))
    return false;
  assert(idl_is_masked(n, IDL_DECL|IDL_TYPEDEF));
  return true;
}

static void delete_typedef(void *node)
{
  idl_typedef_t *n = (idl_typedef_t *)node;
  delete_node(n->type_spec);
  delete_node(n->declarators);
  free(n);
}

idl_retcode_t
idl_create_typedef(
  idl_processor_t *proc,
  idl_typedef_t **nodeptr,
  idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_declarator_t *declarators)
{
  idl_retcode_t ret;
  idl_typedef_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECL|IDL_TYPE|IDL_TYPEDEF;

  ret = idl_create_node(proc, &node, size, mask, location, &delete_typedef);
  if (ret != IDL_RETCODE_OK)
    goto err_node;
  node->type_spec = type_spec;
  if (!idl_scope(type_spec))
    type_spec->parent = (idl_node_t*)node;
  node->declarators = declarators;
  for (idl_declarator_t *p = declarators; p; p = idl_next(p)) {
    p->node.parent = (idl_node_t*)node;
    ret = idl_declare(proc, NULL, IDL_DECLARATION, p->name, node, NULL);
    if (ret != IDL_RETCODE_OK)
      goto err_declare;
  }

  *nodeptr = node;
  return IDL_RETCODE_OK;
err_declare:
  free(node);
err_node:
  return ret;
}

bool idl_is_declarator(const void *node)
{
  const idl_declarator_t *n = (const idl_declarator_t *)node;
  if (!idl_is_masked(n, IDL_DECLARATOR))
    return false;
  assert(idl_is_masked(n, IDL_DECL|IDL_DECLARATOR));
  /* declarators must have an identifier */
  assert(n->name->identifier);
  /* declarators must have a parent node */
  assert(n->node.parent);
  /* declarators can have sizes */
  if (n->const_expr) {
    assert((n->const_expr->mask & IDL_EXPR) == IDL_EXPR ||
           (n->const_expr->mask & IDL_CONST) == IDL_CONST);
  }
  return true;
}

static void delete_declarator(void *node)
{
  idl_declarator_t *n = (idl_declarator_t *)node;
  delete_node(n->const_expr);
  idl_delete_name(n->name);
  free(n);
}

idl_retcode_t
idl_create_declarator(
  idl_processor_t *proc,
  idl_declarator_t **nodep,
  idl_location_t *location,
  idl_name_t *name,
  idl_const_expr_t *const_expr)
{
  idl_retcode_t ret;
  idl_declarator_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECL|IDL_DECLARATOR;

  ret = idl_create_node(proc, &node, size, mask, location, &delete_declarator);
  if (ret != IDL_RETCODE_OK)
    goto err_node;
  node->name = name;
  if (const_expr) {
    node->const_expr = const_expr;
    for (idl_node_t *n = (idl_node_t*)const_expr; n; n = n->next) {
      assert(!n->parent);
      assert(idl_is_masked(n, IDL_CONST|IDL_ULLONG));
      n->parent = (idl_node_t*)node;
    }
  }

  *nodep = node;
  return IDL_RETCODE_OK;
err_node:
  return ret;
}

static void delete_annotation_appl_param(void *node)
{
  idl_annotation_appl_param_t *n = (idl_annotation_appl_param_t *)node;
  delete_node(n->const_expr);
  idl_delete_name(n->name);
  free(n);
}

idl_retcode_t
idl_create_annotation_appl_param(
  idl_processor_t *proc,
  idl_annotation_appl_param_t **nodeptr,
  idl_location_t *location,
  idl_name_t *name,
  idl_const_expr_t *const_expr)
{
  idl_retcode_t ret;
  idl_annotation_appl_param_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECL|IDL_ANNOTATION_APPL_PARAM;

  if ((ret = idl_create_node(proc, &node, size, mask, location, &delete_annotation_appl_param)))
    return ret;
  //(void)proc;
  //if (!(node = make_node(
  //  sizeof(idl_annotation_appl_param_t), mask, location, 0, &delete_annotation_appl_param)))
  //  return IDL_RETCODE_NO_MEMORY;
  node->name = name;
  node->const_expr = const_expr;
  node->node.location = *location;
  *nodeptr = node;
  return IDL_RETCODE_OK;
}

static void delete_annotation_appl(void *node)
{
  idl_annotation_appl_t *n = (idl_annotation_appl_t *)node;
  delete_node(n->parameters);
  idl_delete_scoped_name(n->scoped_name);
  //if (n->scoped_name)
  //  free(n->scoped_name);
  free(n);
}

idl_retcode_t
idl_create_annotation_appl(
  idl_processor_t *proc,
  idl_annotation_appl_t **nodeptr,
  idl_location_t *location,
  idl_scoped_name_t *scoped_name,
  void *parameters)
{
  idl_retcode_t ret;
  idl_annotation_appl_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_ANNOTATION_APPL;

  if ((ret = idl_create_node(proc, &node, size, mask, location, &delete_annotation_appl)))
    return ret;

  node->scoped_name = scoped_name;
  node->parameters = parameters;
  *nodeptr = node;
  return IDL_RETCODE_OK;
}

static void delete_literal(void *node)
{
  idl_literal_t *n = (idl_literal_t *)node;
  assert(node && ((idl_node_t *)node)->mask & IDL_LITERAL);
  if (idl_is_masked(n, IDL_STRING_LITERAL) && n->value.str)
    free(n->value.str);
  free(n);
}

idl_retcode_t
idl_create_literal(
  idl_processor_t *proc,
  idl_literal_t **nodeptr,
  idl_location_t *location,
  idl_mask_t mask)
{
  idl_retcode_t ret;
  idl_literal_t *node;
  static const size_t size = sizeof(*node);

  assert((mask & IDL_ULLONG) == IDL_ULLONG ||
         (mask & IDL_LDOUBLE) == IDL_LDOUBLE ||
         (mask & IDL_CHAR) == IDL_CHAR ||
         (mask & IDL_BOOL) == IDL_BOOL ||
         (mask & IDL_STRING) == IDL_STRING);

  mask |= IDL_EXPR|IDL_LITERAL;
  ret = idl_create_node(proc, &node, size, mask, location, &delete_literal);
  if (ret != IDL_RETCODE_OK)
    return ret;
  *nodeptr = node;
  return IDL_RETCODE_OK;
}

static void delete_binary_expr(void *node)
{
  idl_binary_expr_t *n = (idl_binary_expr_t *)node;
  delete_node(n->left);
  delete_node(n->right);
  free(n);
}

idl_retcode_t
idl_create_binary_expr(
  idl_processor_t *proc,
  idl_binary_expr_t **nodeptr,
  idl_location_t *location,
  idl_mask_t mask,
  idl_const_expr_t *left,
  idl_const_expr_t *right)
{
  idl_retcode_t ret;
  idl_binary_expr_t *node;
  static const size_t size = sizeof(*node);

  mask |= IDL_EXPR|IDL_BINARY_EXPR;
  ret = idl_create_node(proc, &node, size, mask, location, &delete_binary_expr);
  if (ret != IDL_RETCODE_OK)
    return ret;
  node->left = left;
  if (!idl_scope(left))
    left->parent = (idl_node_t*)node;
  node->right = right;
  if (!idl_scope(right))
    right->parent = (idl_node_t*)node;
  *nodeptr = node;
  return IDL_RETCODE_OK;
}

static void delete_unary_expr(void *node)
{
  idl_unary_expr_t *n = (idl_unary_expr_t *)node;
  delete_node(n->right);
  free(n);
}

idl_retcode_t
idl_create_unary_expr(
  idl_processor_t *proc,
  idl_unary_expr_t **nodeptr,
  idl_location_t *location,
  idl_mask_t mask,
  idl_primary_expr_t *right)
{
  idl_retcode_t ret;
  idl_unary_expr_t *node;
  static const size_t size = sizeof(*node);

  mask |= IDL_EXPR|IDL_UNARY_EXPR;
  ret = idl_create_node(proc, &node, size, mask, location, &delete_unary_expr);
  if (ret != IDL_RETCODE_OK)
    return ret;
  node->right = right;
  if (!idl_scope(right->parent))
    right->parent = (idl_node_t*)node;
  *nodeptr = node;
  return IDL_RETCODE_OK;
}

bool idl_is_base_type(const void *node)
{
  if (!idl_is_masked(node, IDL_BASE_TYPE) ||
       idl_is_masked(node, IDL_CONST))
    return false;
#ifndef NDEBUG
  assert(idl_is_masked(node, IDL_TYPE));
  idl_mask_t mask = ((idl_node_t *)node)->mask & ~IDL_TYPE;
  assert(mask == IDL_CHAR ||
         mask == IDL_WCHAR ||
         mask == IDL_BOOL ||
         mask == IDL_OCTET ||
         /* integer types */
         mask == IDL_INT8 || mask == IDL_UINT8 ||
         mask == IDL_INT16 || mask == IDL_UINT16 ||
         mask == IDL_INT32 || mask == IDL_UINT32 ||
         mask == IDL_INT64 || mask == IDL_UINT64 ||
         /* floating point types*/
         mask == IDL_FLOAT ||
         mask == IDL_DOUBLE ||
         mask == IDL_LDOUBLE);
#endif
  return true;
}

static void delete_base_type(void *node)
{
  free(node);
}

idl_retcode_t
idl_create_base_type(
  idl_processor_t *proc,
  idl_base_type_t **nodeptr,
  idl_location_t *location,
  idl_mask_t mask)
{
  static const size_t size = sizeof(idl_base_type_t);

  mask |= IDL_TYPE;
  return idl_create_node(proc, nodeptr, size, mask, location, &delete_base_type);
}

static void delete_constval(void *node)
{
  idl_constval_t *n = (idl_constval_t *)node;
  if (idl_is_masked(n, IDL_CONST|IDL_STRING) && n->value.str)
    free(n->value.str);
  free(n);
}

idl_retcode_t
idl_create_constval(
  idl_processor_t *proc,
  idl_constval_t **nodeptr,
  const idl_location_t *location,
  idl_mask_t mask)
{
  static const size_t size = sizeof(idl_constval_t);

  mask |= IDL_CONST;
  return idl_create_node(proc, nodeptr, size, mask, location, &delete_constval);
}

void idl_delete_tree(idl_tree_t *tree)
{
  if (tree) {
    idl_file_t *file, *next;
    delete_node(tree->root);
    for (file = tree->files; file; file = next) {
      next = file->next;
      if (file->name)
        free(file->name);
      free(file);
    }
    free(tree);
  }
}
