// Copyright(c) 2021 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "idl/heap.h"
#include "idl/string.h"

#include "expression.h"
#include "file.h" /* for ssize_t on Windows */
#include "tree.h"
#include "scope.h"
#include "symbol.h"
#include "hashid.h"
#include "fieldid.h"

void *idl_push_node(void *list, void *node)
{
  idl_node_t *last;

  if (!node)
    return list;
  if (!list)
    return node;

  for (last = list; last != node && last->next; last = last->next) ;
  assert(last != node);

  last->next = node;
  ((idl_node_t *)node)->previous = last;
  return list;
}

void *idl_reference_node(void *node)
{
  if (node)
    ((idl_node_t *)node)->references++;
  return node;
}

void *idl_unreference_node(void *ptr)
{
  idl_node_t *node = ptr;
  if (!node)
    return NULL;
  node->references--;
  assert(node->references >= 0);
  if (node->references == 0) {
    idl_node_t *previous, *next;
    previous = node->previous;
    next = node->next;
    if (previous)
      previous->next = next;
    if (next)
      next->previous = previous;
    idl_delete_node((idl_node_t *)node->annotations);
    if (node->destructor)
      node->destructor(node);
    return next;
  }
  return node;
}

idl_mask_t idl_mask(const void *node)
{
  return node ? ((idl_node_t *)node)->mask : 0u;
}

idl_type_t idl_type(const void *node)
{
  idl_mask_t mask;

  /* typedef nodes are referenced by their declarator(s) */
  if (idl_mask(node) & IDL_DECLARATOR)
    node = idl_parent(node);
  else if (idl_mask(node) & IDL_ENUMERATOR)
    return IDL_ENUM;

  mask = idl_mask(node) & IDL_TYPE_MASK;
  switch (mask) {
    case IDL_TYPEDEF:
    /* constructed types */
    case IDL_STRUCT:
    case IDL_UNION:
    case IDL_ENUM:
    case IDL_BITMASK:
    /* template types */
    case IDL_SEQUENCE:
    case IDL_STRING:
    case IDL_WSTRING:
    case IDL_FIXED_PT:
    /* simple types */
    /* miscellaneous base types */
    case IDL_CHAR:
    case IDL_WCHAR:
    case IDL_BOOL:
    case IDL_OCTET:
    case IDL_ANY:
    /* integer types */
    case IDL_SHORT:
    case IDL_USHORT:
    case IDL_LONG:
    case IDL_ULONG:
    case IDL_LLONG:
    case IDL_ULLONG:
    case IDL_INT8:
    case IDL_UINT8:
    case IDL_INT16:
    case IDL_UINT16:
    case IDL_INT32:
    case IDL_UINT32:
    case IDL_INT64:
    case IDL_UINT64:
    /* floating point types */
    case IDL_FLOAT:
    case IDL_DOUBLE:
    case IDL_LDOUBLE:
      return (idl_type_t)mask;
    default:
      break;
  }

  return IDL_NULL;
}

const idl_name_t *idl_name(const void *node)
{
  if (idl_mask(node) & IDL_FORWARD)
    return ((const idl_forward_t *)node)->name;
  if (idl_mask(node) & IDL_MODULE)
    return ((const idl_module_t *)node)->name;
  if (idl_mask(node) & IDL_STRUCT)
    return ((const idl_struct_t *)node)->name;
  if (idl_mask(node) & IDL_UNION)
    return ((const idl_union_t *)node)->name;
  if (idl_mask(node) & IDL_ENUM)
    return ((const idl_enum_t *)node)->name;
  if (idl_mask(node) & IDL_ENUMERATOR)
    return ((const idl_enumerator_t *)node)->name;
  if (idl_mask(node) & IDL_BITMASK)
    return ((const idl_bitmask_t *)node)->name;
  if (idl_mask(node) & IDL_BIT_VALUE)
    return ((const idl_bit_value_t *)node)->name;
  if (idl_mask(node) & IDL_DECLARATOR)
    return ((const idl_declarator_t *)node)->name;
  if (idl_mask(node) & IDL_CONST)
    return ((const idl_const_t *)node)->name;
  if (idl_mask(node) & IDL_ANNOTATION_APPL)
    node = ((const idl_annotation_appl_t *)node)->annotation;
  if (idl_mask(node) & IDL_ANNOTATION)
    return ((const idl_annotation_t *)node)->name;
  if (idl_mask(node) & IDL_ANNOTATION_MEMBER)
    return ((const idl_annotation_member_t *)node)->declarator->name;
  return NULL;
}

const char *idl_construct(const void *node)
{
  return ((const idl_node_t *)node)->describe(node);
}

const char *idl_identifier(const void *node)
{
  const idl_name_t *name = idl_name(node);
  return name ? name->identifier : NULL;
}

bool idl_identifier_is(const void *node, const char *identifier)
{
  if (!identifier)
    return false;
  const char *str = idl_identifier(node);
  if (!str || strcmp(str, identifier) != 0)
    return false;
  return true;
}

void *idl_ancestor(const void *node, size_t levels)
{
  const idl_node_t *root = NULL;

  if (node) {
    root = ((const idl_node_t *)node)->parent;
    for (size_t level=0; level < levels && root; level++)
      root = root->parent;
  }

  return (void *)root;
}

void *idl_parent(const void *node)
{
  return idl_ancestor(node, 0);
}

size_t idl_degree(const void *node)
{
  size_t len = 0;
  for (const idl_node_t *n = node; n; n = n->next)
    len++;
  return len;
}

void *idl_previous(const void *node)
{
  const idl_node_t *n = (const idl_node_t *)node;
  if (!n)
    return NULL;
#if 0
  /* declarators can have siblings */
  if (idl_is_masked(n, IDL_DECLARATION))
    return n->previous;
  /* as do expressions (or constants) if specifying array sizes */
  if (idl_is_masked(n, IDL_CONST) && idl_is_declarator(n->parent))
    return n->previous;
  assert(!n->previous);
  assert(!n->next);
#endif
  return n->previous;
}

void *idl_next(const void *node)
{
  const idl_node_t *n = (const idl_node_t *)node;
  if (!n)
    return NULL;
#if 0
  /* declaration can have siblings */
  if (idl_is_masked(n, IDL_DECLARATION))
    return n->next;
  /* as do expressions (or constants) if specifying array sizes */
  if (idl_is_masked(n, IDL_CONST) && idl_is_declarator(n->parent))
    return n->next;
  assert(!n->previous);
  assert(!n->next);
#endif
  return n->next;
}

void *idl_iterate(const void *root, const void *node)
{
  assert(root);
  assert(!node || ((const idl_node_t *)node)->parent == root);
  if (node && ((const idl_node_t *)node)->next)
    return ((const idl_node_t *)node)->next;
  if (((const idl_node_t *)root)->iterate)
    return ((const idl_node_t *)root)->iterate(root, node);
  return NULL;
}

void *idl_unalias(const void *node)
{
  while (idl_is_alias(node))
    node = idl_type_spec(node);

  return (void *)node;
}

void *idl_strip(const void *node, uint32_t flags)
{
  if (!flags) // unwrap every indirection by default
    flags = IDL_STRIP_ALIASES | IDL_STRIP_ALIASES_ARRAY | IDL_STRIP_FORWARD;

  do {
    if (idl_is_forward(node) && (flags & IDL_STRIP_FORWARD))
      node = idl_type_spec(node);
    else if (idl_is_alias(node) && !idl_is_array(node) && (flags & IDL_STRIP_ALIASES))
      node = idl_type_spec(node);
    else if (idl_is_alias(node) && idl_is_array(node) && (flags & IDL_STRIP_ALIASES_ARRAY))
      node = idl_type_spec(node);
    else
      break;
  } while (node);

  return (void *)node;
}

struct methods {
  idl_delete_t delete;
  idl_iterate_t iterate;
  idl_describe_t describe;
};

static idl_retcode_t
create_node(
  idl_pstate_t *pstate,
  size_t size,
  idl_mask_t mask,
  const idl_location_t *location,
  const struct methods *methods,
  void *nodep)
{
  idl_node_t *node;

  (void)pstate;
  assert(size >= sizeof(*node));
  if (!(node = idl_calloc(1, size)))
    return IDL_RETCODE_NO_MEMORY;
  node->symbol.location = *location;
  node->mask = mask;
  node->destructor = methods->delete;
  node->iterate = methods->iterate;
  node->describe = methods->describe;
  node->references = 1;

  *((void **)nodep) = (void *)node;
  return IDL_RETCODE_OK;
}

static void *delete_type_spec(void *node, idl_type_spec_t *type_spec)
{
  void *next;
  if (!type_spec)
    return NULL;
  if (((idl_node_t *)type_spec)->parent != node) {
    next = idl_unreference_node(type_spec);
    assert(next == type_spec);
  } else {
    next = idl_delete_node(type_spec);
    assert(next == NULL);
  }
  return next;
}

static void *delete_const_expr(void *node, idl_const_expr_t *const_expr)
{
  void *next;
  if (!const_expr)
    return NULL;
  if (((idl_node_t *)const_expr)->parent != node) {
    next = idl_unreference_node(const_expr);
    assert(next == const_expr);
  } else {
    next = idl_delete_node(const_expr);
    assert(next == NULL);
  }
  return next;
}

void *idl_delete_node(void *ptr)
{
  idl_node_t *next, *node = ptr;
  if (!node)
    return NULL;
  if ((next = node->next))
    next = idl_delete_node(node->next);
  node->references--;
  assert(node->references >= 0);
  if (node->references == 0) {
    idl_node_t *previous = node->previous;
    if (previous)
      previous->next = next;
    if (next)
      next->previous = previous;
    idl_delete_node((idl_node_t *)node->annotations);
    /* self-destruct */
    if (node->destructor)
      node->destructor(node);
    return next;
  }
  return node;
}

bool idl_is_declaration(const void *ptr)
{
  const idl_node_t *node = ptr;
  if (idl_mask(node) & IDL_BITMASK)
    return true;
  if (!(idl_mask(node) & IDL_DECLARATION))
    return false;
  /* a declaration must have been declared */
  assert(node->declaration);
  return true;
}

bool idl_is_module(const void *ptr)
{
#if !defined NDEBUG
  static const idl_mask_t mask =
    IDL_MODULE | IDL_CONST | IDL_STRUCT | IDL_UNION | IDL_ENUM | IDL_BITMASK |
    IDL_TYPEDEF | IDL_ANNOTATION;
#endif
  const idl_module_t *node = ptr;

  if (!(idl_mask(node) & IDL_MODULE))
    return false;
  /* a module must have a name */
  assert(node->name && node->name->identifier);
  /* a module must have a scope */
  assert(node->node.declaration);
  /* a module must have at least one definition */
  assert(!node->definitions || (idl_mask(node->definitions) & mask));
  return true;
}

static void delete_module(void *ptr)
{
  idl_module_t *node = (idl_module_t *)ptr;
  idl_delete_node(node->definitions);
  idl_delete_name(node->name);
  idl_free(node);
}

static void *iterate_module(const void *ptr, const void *cur)
{
  const idl_module_t *root = ptr;
  const idl_node_t *node = cur;
  if (node) {
    assert(idl_parent(node) == root);
    if (node->next)
      return node->next;
    if (idl_is_annotation_appl(node))
      return root->definitions;
    return NULL;
  }
  if (root->node.annotations)
    return root->node.annotations;
  return root->definitions;
}

static const char *describe_module(const void *ptr) { (void)ptr; return "module"; }

idl_retcode_t
idl_finalize_module(
  idl_pstate_t *state,
  const idl_location_t *location,
  idl_module_t *node,
  idl_definition_t *definitions)
{
  idl_exit_scope(state);
  node->node.symbol.location.last = location->last;
  node->definitions = definitions;
  for (idl_node_t *n = (idl_node_t *)definitions; n; n = n->next) {
    assert(!n->parent);
    n->parent = (idl_node_t *)node;
  }
  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_create_module(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  void *nodep)
{
  idl_retcode_t ret;
  idl_module_t *node;
  idl_scope_t *scope = NULL;
  const idl_declaration_t *declaration;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_MODULE;
  static const struct methods methods = {
    delete_module, iterate_module, describe_module };
  static const enum idl_declaration_kind kind = IDL_MODULE_DECLARATION;

  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    goto err_node;
  node->name = name;

  /* an identifier declaring a module is considered to be defined by its
     first occurence in a scope. subsequent occurrences of a module
     declaration with the same identifier within the same scope reopens the
     module and hence its scope, allowing additional definitions to be added
     to it */
  declaration = idl_find(pstate, NULL, name, 0u);
  if (declaration && (idl_mask(declaration->node) & IDL_MODULE)) {
    node->node.declaration = declaration;
#if 0
    /* FIXME: the code below always adds the first module as previous, which
              is obviously wrong. for now though, it doesn't look like the
              reference is needed? */
    node->previous = (const idl_module_t *)declaration->node;
#endif
    scope = (idl_scope_t *)declaration->scope;
  } else {
    if ((ret = idl_create_scope(pstate, IDL_MODULE_SCOPE, name, node, &scope)))
      goto err_scope;
    if ((ret = idl_declare(pstate, kind, name, node, scope, NULL)))
      goto err_declare;
  }

  idl_enter_scope(pstate, scope);
  *((idl_module_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_declare:
  idl_delete_scope(scope);
err_scope:
  idl_free(node);
err_node:
  return ret;
}

bool idl_is_const(const void *ptr)
{
#if !defined(NDEBUG)
  static const idl_mask_t mask =
    IDL_BASE_TYPE | IDL_STRING | IDL_ENUMERATOR;
#endif
  const idl_const_t *node = ptr;

  if (!(idl_mask(node) & IDL_CONST))
    return false;
  /* a constant must have a name */
  assert(node->name && node->name->identifier);
  /* a constant must have a type specifier */
  assert(idl_mask(node->type_spec) & mask);
  /* a constant must have a constant value */
  assert(idl_mask(node->const_expr) & IDL_LITERAL);
  assert(idl_mask(node->const_expr) & mask);
  return true;
}

static void delete_const(void *ptr)
{
  idl_const_t *node = (idl_const_t *)ptr;
  delete_type_spec(node, node->type_spec);
  delete_const_expr(node, node->const_expr);
  idl_delete_name(node->name);
  idl_free(node);
}

static void *iterate_const(const void *ptr, const void *cur)
{
  const idl_const_t *root = ptr;
  const idl_node_t *node = cur;
  if (node) {
    assert(idl_parent(node) == root);
    if (node->next)
      return node->next;
    return NULL;
  }
  return root->node.annotations;
}

static const char *describe_const(const void *ptr) { (void)ptr; return "const"; }

idl_retcode_t
idl_create_const(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_name_t *name,
  idl_const_expr_t *const_expr,
  void *nodep)
{
  idl_retcode_t ret;
  idl_const_t *node;
  idl_type_spec_t *alias;
  idl_const_expr_t *literal = NULL;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_CONST;
  static const struct methods methods = {
    delete_const, iterate_const, describe_const };
  static const enum idl_declaration_kind kind = IDL_SPECIFIER_DECLARATION;

  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    goto err_node;
  node->name = name;
  /* type specifier can be a type definition */
  alias = type_spec;
  type_spec = idl_unalias(type_spec);
  assert(idl_mask(type_spec) & (IDL_BASE_TYPE|IDL_STRING|IDL_ENUM));
  node->type_spec = alias;
  if (!idl_scope(alias))
    ((idl_node_t *)alias)->parent = (idl_node_t*)node;
  /* evaluate constant expression */
  if ((ret = idl_evaluate(pstate, const_expr, idl_type(type_spec), &literal)))
    goto err_evaluate;
  node->const_expr = literal;
  if (!idl_scope(literal))
    ((idl_node_t *)literal)->parent = (idl_node_t*)node;
  if ((ret = idl_declare(pstate, kind, name, node, NULL, NULL)))
    goto err_declare;

  *((idl_const_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_declare:
err_evaluate:
  idl_free(node);
err_node:
  return ret;
}

bool idl_is_literal(const void *ptr)
{
  const idl_literal_t *node = ptr;

  if (!(idl_mask(node) & IDL_LITERAL))
    return false;
  if (idl_mask(node) & IDL_STRING)
    assert(node->value.str);
  return true;
}

static void delete_literal(void *ptr)
{
  idl_literal_t *node = ptr;
  assert(idl_mask(node) & IDL_LITERAL);
  if (idl_mask(node) & IDL_STRING)
    idl_free(node->value.str);
  idl_free(node);
}

static const char *describe_literal(const void *ptr)
{
  idl_type_t type;

  assert(idl_mask(ptr) & IDL_LITERAL);

  type = idl_type(ptr);
  if (type == IDL_CHAR)
    return "character literal";
  if (type == IDL_BOOL)
    return "boolean literal";
  if (type == IDL_OCTET)
    return "integer literal";
  if (type & IDL_INTEGER_TYPE)
    return "integer literal";
  if (type & IDL_FLOATING_PT_TYPE)
    return "floating point literal";
  assert(type == IDL_STRING);
  return "string literal";
}

idl_retcode_t
idl_create_literal(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_mask_t mask,
  void *nodep)
{
  idl_retcode_t ret;
  idl_literal_t *node = NULL;
  static const size_t size = sizeof(*node);
  static const struct methods methods = {
    delete_literal, 0, describe_literal };

  mask |= IDL_LITERAL;
  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    return ret;
  *((idl_literal_t **)nodep) = node;
  return IDL_RETCODE_OK;
}

static void delete_binary_expr(void *ptr)
{
  idl_binary_expr_t *node = ptr;
  delete_const_expr(node, node->left);
  delete_const_expr(node, node->right);
  idl_free(node);
}

static const char *describe_binary_expr(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) & IDL_BINARY_OPERATOR);
  return "expression";
}

idl_retcode_t
idl_create_binary_expr(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_mask_t mask,
  idl_primary_expr_t *left,
  idl_primary_expr_t *right,
  void *nodep)
{
  idl_retcode_t ret;
  idl_binary_expr_t *node = NULL;
  static const size_t size = sizeof(*node);
  static const struct methods methods = {
    &delete_binary_expr, 0, describe_binary_expr };

  mask |= IDL_BINARY_OPERATOR;
  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    return ret;
  node->left = left;
  if (!idl_scope(left))
    ((idl_node_t *)left)->parent = (idl_node_t *)node;
  node->right = right;
  if (!idl_scope(right))
    ((idl_node_t *)right)->parent = (idl_node_t *)node;
  *((idl_binary_expr_t **)nodep) = node;
  return IDL_RETCODE_OK;
}

static void delete_unary_expr(void *ptr)
{
  idl_unary_expr_t *node = ptr;
  delete_const_expr(node, node->right);
  idl_free(node);
}

static const char *describe_unary_expr(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) & IDL_UNARY_OPERATOR);
  return "expression";
}

idl_retcode_t
idl_create_unary_expr(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_mask_t mask,
  idl_primary_expr_t *right,
  void *nodep)
{
  idl_retcode_t ret;
  idl_unary_expr_t *node = NULL;
  static const size_t size = sizeof(*node);
  static const struct methods methods = {
    delete_unary_expr, 0, describe_unary_expr };

  mask |= IDL_UNARY_OPERATOR;
  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    return ret;
  node->right = right;
  if (!idl_scope(right))
    ((idl_node_t *)right)->parent = (idl_node_t *)node;
  *((idl_unary_expr_t **)nodep) = node;
  return IDL_RETCODE_OK;
}

bool idl_is_type_spec(const void *ptr)
{
  /* a declarator is a type specifier if its parent is a typedef */
  if (idl_mask(ptr) & IDL_DECLARATOR)
    ptr = idl_parent(ptr);
  return (idl_mask(ptr) & IDL_TYPE) != 0;
}

bool idl_is_base_type(const void *node)
{
  if (!(idl_mask(node) & IDL_BASE_TYPE) || (idl_mask(node) & IDL_LITERAL))
    return false;
  return true;
}

bool idl_is_floating_pt_type(const void *node)
{
  return (idl_mask(node) & IDL_FLOATING_PT_TYPE) != 0;
}

bool idl_is_integer_type(const void *node)
{
  return (idl_mask(node) & IDL_INTEGER_TYPE) != 0;
}

static void delete_base_type(void *ptr)
{
  idl_free(ptr);
}

static const char *describe_base_type(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) & IDL_BASE_TYPE);
  return "base type";
}

idl_retcode_t
idl_create_base_type(
  idl_pstate_t *state,
  const idl_location_t *location,
  idl_mask_t mask,
  void *nodep)
{
  static const size_t size = sizeof(idl_base_type_t);
  static const struct methods methods = {
    delete_base_type, 0, describe_base_type };

  return create_node(state, size, mask, location, &methods, nodep);
}

bool idl_is_templ_type(const void *ptr)
{
  return (idl_mask(ptr) & (IDL_SEQUENCE|IDL_STRING)) != 0;
}

bool idl_is_bounded(const void *node)
{
  idl_mask_t mask = idl_mask(node);
  if ((mask & IDL_STRING) == IDL_STRING)
    return ((const idl_string_t *)node)->maximum != 0;
  if ((mask & IDL_SEQUENCE) == IDL_SEQUENCE)
    return ((const idl_sequence_t *)node)->maximum != 0;
  return false;
}

uint32_t idl_bound(const void *node)
{
  idl_mask_t mask = idl_mask(node);
  if ((mask & IDL_STRING) == IDL_STRING)
    return ((const idl_string_t *)node)->maximum;
  if ((mask & IDL_SEQUENCE) == IDL_SEQUENCE)
    return ((const idl_sequence_t *)node)->maximum;
  if ((mask & IDL_BITMASK) == IDL_BITMASK)
    return ((const idl_bitmask_t *)node)->bit_bound.value;
  if ((mask & IDL_ENUM) == IDL_ENUM)
    return ((const idl_enum_t *)node)->bit_bound.value;
  return 0u;
}

const idl_literal_t *idl_default_value(const void *node)
{
  if (idl_is_member(node)) {
    return ((const idl_member_t*)node)->value.value;
  } else if (idl_is_declarator(node)) {
    const idl_node_t *parent = idl_parent(node);
    if (idl_is_member(parent))
      return idl_default_value(parent);
  }

  return NULL;
}

static bool idl_has_data_representation(const void *node)
{
  if (idl_is_module(node))
    return ((const idl_module_t*)node)->data_representation.annotation;
  else if (idl_is_struct(node))
    return ((const idl_struct_t*)node)->data_representation.annotation;
  else if (idl_is_union(node))
    return ((const idl_union_t*)node)->data_representation.annotation;
  else
    return false;
}

static allowable_data_representations_t idl_data_representation_value(const void *node)
{
  if (idl_is_module(node))
    return ((const idl_module_t*)node)->data_representation.value;
  else if (idl_is_struct(node))
    return ((const idl_struct_t*)node)->data_representation.value;
  else if (idl_is_union(node))
    return ((const idl_union_t*)node)->data_representation.value;
  else
    return 0;
}

bool idl_requires_xcdr2(const void *node)
{
  if (idl_is_struct(node))
    return ((const idl_struct_t*)node)->requires_xcdr2 == IDL_REQUIRES_XCDR2_TRUE;
  else if (idl_is_union(node))
    return ((const idl_union_t*)node)->requires_xcdr2 == IDL_REQUIRES_XCDR2_TRUE;
  return false;
}

allowable_data_representations_t idl_allowable_data_representations(const void *node)
{
  if (node == NULL)
    return IDL_ALLOWABLE_DATAREPRESENTATION_DEFAULT;
  if (idl_is_alias(node))
    return idl_allowable_data_representations(idl_unalias(node));

  return idl_has_data_representation(node) ?
    idl_data_representation_value(node) : idl_allowable_data_representations(idl_parent(node));
}

bool idl_is_sequence(const void *ptr)
{
  idl_mask_t mask;
  const idl_sequence_t *node = ptr;
  const idl_type_spec_t *type_spec;

  if (!(idl_mask(node) & IDL_SEQUENCE))
    return false;
  assert(!node->node.declaration);
  /* a sequence must have a type specifier */
  type_spec = node->type_spec;
  if (idl_mask(type_spec) & IDL_DECLARATOR)
    type_spec = ((const idl_node_t *)type_spec)->parent;
  mask = IDL_STRUCT | IDL_UNION | IDL_ENUM | IDL_BITMASK | IDL_TYPEDEF |
         IDL_SEQUENCE | IDL_STRING | IDL_BASE_TYPE;
  (void)mask;
  assert(idl_mask(type_spec) & mask);
  mask = IDL_TYPEDEF | IDL_MEMBER | IDL_CASE |
         IDL_SEQUENCE | IDL_SWITCH_TYPE_SPEC;
  assert(!node->node.parent || (idl_mask(node->node.parent) & mask));
  return true;
}

static void delete_sequence(void *ptr)
{
  idl_sequence_t *node = (idl_sequence_t *)ptr;
  delete_type_spec(node, node->type_spec);
  idl_free(node);
}

static const char *describe_sequence(const void *ptr)
{
  const idl_sequence_t *node = ptr;
  assert(idl_mask(node) & IDL_SEQUENCE);
  return node->maximum ? "bounded sequence" : "sequence";
}

idl_retcode_t
idl_create_sequence(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_literal_t *literal,
  void *nodep)
{
  idl_retcode_t ret;
  idl_sequence_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_SEQUENCE;
  static const struct methods methods = {
    delete_sequence, 0, describe_sequence };

  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    goto err_node;
  /* type specifier can be a type definition */
  node->type_spec = type_spec;
  if (!idl_scope(type_spec))
    ((idl_node_t *)type_spec)->parent = (idl_node_t*)node;
  assert(!literal || idl_type(literal) == IDL_ULONG);
  if (literal)
    node->maximum = literal->value.uint32;
  idl_delete_node(literal);
  *((idl_sequence_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_node:
  return ret;
}

bool idl_is_string(const void *ptr)
{
#if !defined(NDEBUG)
  static const idl_mask_t mask = IDL_CONST | IDL_TYPEDEF | IDL_MEMBER |
                                 IDL_CASE | IDL_SEQUENCE | IDL_ANNOTATION_APPL_PARAM |
                                 IDL_SWITCH_TYPE_SPEC;
#endif
  const idl_string_t *node = ptr;

  if (!(idl_mask(node) & IDL_STRING) || (idl_mask(node) & IDL_CONST))
    return false;
  assert(!node->node.declaration);
  assert(!node->node.parent || (idl_mask(node->node.parent) & mask));
  return true;
}

bool idl_is_unbounded_string(const void *ptr)
{
  return idl_is_string(ptr) && !idl_is_bounded(ptr);
}

bool idl_is_bounded_string(const void *ptr)
{
  return idl_is_string(ptr) && idl_is_bounded(ptr);
}

static void delete_string(void *ptr)
{
  idl_free(ptr);
}

static const char *describe_string(const void *ptr)
{
  const idl_string_t *node = ptr;
  assert(idl_mask(node) & IDL_STRING);
  return node->maximum ? "bounded string" : "string";
}

idl_retcode_t
idl_create_string(
  idl_pstate_t *state,
  const idl_location_t *location,
  idl_literal_t *literal,
  void *nodep)
{
  idl_retcode_t ret;
  idl_string_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_STRING;
  static const struct methods methods = { delete_string, 0, describe_string };

  if ((ret = create_node(state, size, mask, location, &methods, &node)))
    goto err_node;
  assert(!literal || idl_type(literal) == IDL_ULONG);
  if (literal)
    node->maximum = literal->value.uint32;
  idl_unreference_node(literal);
  *((idl_string_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_node:
  return ret;
}

bool idl_is_constr_type(const void *node)
{
  return (idl_mask(node) & (IDL_STRUCT | IDL_UNION | IDL_ENUM | IDL_BITMASK)) != 0;
}

bool idl_is_struct(const void *ptr)
{
  const idl_struct_t *node = ptr;

  if (idl_is_forward(node))
    return false;
  if (!(idl_mask(node) & IDL_STRUCT))
    return false;
  /* a struct must have a name */
  assert(node->name && node->name->identifier);
  /* structs must have at least one member unless building block extended
     data types is enabled */
#if 0
  members = ((const idl_struct_t *)node)->members;
  if (!(pstate->config.flags & IDL_FLAG_EXTENDED_DATA_TYPES))
    assert(members);
  if (members)
    assert(idl_mask(members) & IDL_MEMBER);
#endif
  return true;
}

static void delete_struct(void *ptr)
{
  idl_struct_t *node = ptr;
  idl_delete_node(node->inherit_spec);
  idl_delete_node(node->keylist);
  idl_delete_node(node->members);
  idl_delete_name(node->name);
  idl_free(node);
}

static void *iterate_struct(const void *ptr, const void *cur)
{
  const idl_struct_t *root = (const idl_struct_t *)ptr;
  const idl_node_t *node = (const idl_node_t *)cur;
  if (node) {
    if (node->next)
      return node->next;
    if (idl_is_inherit_spec(node))
      return root->members;
    if (idl_is_annotation_appl(node)) {
      if (root->inherit_spec)
        return root->inherit_spec;
      return root->members;
    }
    return NULL;
  }
  if (root->node.annotations)
    return root->node.annotations;
  if (root->inherit_spec)
    return root->inherit_spec;
  return root->members;
}

static const char *describe_struct(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) & IDL_STRUCT);
  return "struct";
}

static int compare_declarator(const void *lhs, const void *rhs)
{
  const idl_declarator_t *d_lhs = *(const idl_declarator_t **)lhs,
                         *d_rhs = *(const idl_declarator_t **)rhs;
  if (d_lhs->id.value < d_rhs->id.value)
    return -1;
  else if (d_lhs->id.value > d_rhs->id.value)
    return 1;
  else
    return 0;
}

static void
assign_id(
  idl_declarator_t *declarator,
  const idl_declarator_t *last,
  idl_autoid_t autoid)
{
  assert(declarator);

  /* id assigned through @id or @hashid annotation */
  if (declarator->id.annotation)
    return;

  assert(declarator->name);
  assert(declarator->name->identifier);

  if (autoid == IDL_HASH)
    declarator->id.value = idl_hashid(declarator->name->identifier);
  else if (last) /* identifiers silently overflow */
    declarator->id.value = (last->id.value + 1) & IDL_FIELDID_MASK;
  else
    declarator->id.value = 0u;
}

static idl_declarator_t *
next_member(const void *node, const idl_declarator_t *declarator)
{
  const idl_struct_t *derived = node;

  assert(idl_type(node) == IDL_STRUCT);
  if (!declarator) {
    /* iteration is depth first, see if struct is derived */
    while (derived && derived->inherit_spec)
      derived = derived->inherit_spec->base;
  } else if (declarator->node.next) {
    return (idl_declarator_t *)declarator->node.next;
  } else {
    const idl_struct_t *base;
    const idl_member_t *member = (const idl_member_t *)declarator->node.parent;
    assert(member);
    if (member->node.next)
      return (idl_declarator_t *)((const idl_member_t *)member->node.next)->declarators;
    base = (const idl_struct_t *)member->node.parent;
    if (base == node)
      return NULL;
    while (derived->inherit_spec && derived->inherit_spec->base != base)
      derived = derived->inherit_spec->base;
  }

  assert(derived);
  /* (intermediate) structs without members are supported in IDL4 */
  while (!derived->members && derived != node) {
    const idl_struct_t *base = derived;
    derived = node;
    while (derived->inherit_spec && derived->inherit_spec->base != base)
      derived = derived->inherit_spec->base;
  }
  assert(derived);
  if (!derived->members)
    return NULL;

  return (idl_declarator_t *)derived->members->declarators;
}

/* FIXME: does not take into account single inheritance for union types as
          introduced by the DDS-XTypes specification section  7.2.2.4.5 */
static idl_declarator_t *
next_case(const void *node, const idl_declarator_t *declarator)
{
  const idl_case_t *branch;

  assert(idl_type(node) == IDL_UNION);
  if (!declarator)
    return ((const idl_union_t *)node)->cases->declarator;
  assert(!declarator->node.next);
  branch = (const idl_case_t *)declarator->node.parent;
  assert(branch);
  branch = (const idl_case_t *)branch->node.next;
  if (!branch)
    return NULL;

  return (idl_declarator_t *)branch->declarator;
}

static idl_retcode_t
assign_field_ids(idl_pstate_t *pstate, void *node)
{
  idl_retcode_t ret = IDL_RETCODE_OK;
  idl_autoid_t autoid = IDL_SEQUENTIAL;
  idl_declarator_t *declarator = NULL, *last = NULL;
  idl_declarator_t **sorted = NULL; /* not "const" to silence MSVC */
  idl_declarator_t *(*iterate)(const void *, const idl_declarator_t *);
  size_t length = 0;

  switch (idl_type(node)) {
    case IDL_UNION:
      autoid = ((const idl_union_t *)node)->autoid.value;
      iterate = &next_case;
      break;
    default:
      assert(idl_type(node) == IDL_STRUCT);
      /* structs without members are supported in IDL4 */
      if (!((const idl_struct_t *)node)->members)
        return IDL_RETCODE_OK;
      autoid = ((const idl_struct_t *)node)->autoid.value;
      iterate = &next_member;
      break;
  }

  /* assign field identifiers */
  while ((declarator = iterate(node, declarator))) {
    assert(declarator->node.parent);
    if (((const idl_node_t *)declarator->node.parent)->parent == node)
      assign_id(declarator, last, autoid);
    last = declarator;
    length++;
  }

  /* theoretically there are no fields to assign identifiers to */
  if (!length)
    return IDL_RETCODE_OK;

  /* check for duplicate field identifiers */
  if (!(sorted = idl_malloc(length * sizeof(*sorted))))
    return IDL_RETCODE_NO_MEMORY;

  assert(!declarator);
  for (size_t count=0; (declarator = iterate(node, declarator)); count++)
    sorted[count] = declarator;
  qsort(sorted, length, sizeof(*sorted), &compare_declarator);

  for (size_t count=1; count < length; count++) {
    if (sorted[count-1]->id.value == sorted[count]->id.value) {
      idl_error(pstate, idl_location(sorted[count]),
        "Field id '0x%.07x' is already assigned", sorted[count]->id.value);
      ret = IDL_RETCODE_SEMANTIC_ERROR;
      break;
    }
  }

  idl_free(sorted);
  return ret;
}

idl_retcode_t
idl_propagate_autoid(idl_pstate_t *pstate, void *list, idl_autoid_t autoid)
{
  idl_retcode_t ret = IDL_RETCODE_OK;

  assert(pstate);
  assert(list);

  for (; ret == IDL_RETCODE_OK && list; list = idl_next(list)) {
    if (idl_mask(list) == IDL_MODULE) {
      idl_module_t *node = list;
      if (!node->autoid.annotation)
        node->autoid.value = autoid;
      ret = idl_propagate_autoid(pstate, node->definitions, node->autoid.value);
    } else if (idl_mask(list) == IDL_STRUCT) {
      idl_struct_t *node = list;
      if (!node->autoid.annotation)
        node->autoid.value = autoid;
      ret = assign_field_ids(pstate, node);
    } else if (idl_mask(list) == IDL_UNION) {
      idl_union_t *node = list;
      if (!node->autoid.annotation)
        node->autoid.value = autoid;
      ret = assign_field_ids(pstate, node);
    }
  }

  return ret;
}

static bool
set_node_xcdr2_required(void *node)
{
  if (idl_is_alias(node)) {
    return set_node_xcdr2_required(idl_unalias(node));
  } else if (idl_is_sequence(node)) {
    return set_node_xcdr2_required(idl_type_spec(node));
  } else if (idl_is_struct(node)) {
    idl_struct_t *_struct = (idl_struct_t*)node;
    if (_struct->requires_xcdr2 != IDL_REQUIRES_XCDR2_UNSET)
      return (_struct->requires_xcdr2 == IDL_REQUIRES_XCDR2_SETTING) ?
        false : (_struct->requires_xcdr2 == IDL_REQUIRES_XCDR2_TRUE);
    _struct->requires_xcdr2 = IDL_REQUIRES_XCDR2_SETTING;

    bool ret = !idl_is_extensible(node, IDL_FINAL)
        || (_struct->inherit_spec && set_node_xcdr2_required(idl_type_spec(_struct->inherit_spec)));

    idl_member_t *_member;
    IDL_FOREACH(_member, _struct->members) {
      if (idl_is_optional(&_member->node) || set_node_xcdr2_required(idl_type_spec(_member))) {
        ret = true;
        break;
      }
    };
    _struct->requires_xcdr2 = ret ? IDL_REQUIRES_XCDR2_TRUE : IDL_REQUIRES_XCDR2_FALSE;
    return ret;
  } else if (idl_is_union(node)) {
    idl_union_t *_union = (idl_union_t*)node;
    if (_union->requires_xcdr2 != IDL_REQUIRES_XCDR2_UNSET)
      return (_union->requires_xcdr2 == IDL_REQUIRES_XCDR2_SETTING) ? false : (_union->requires_xcdr2 == IDL_REQUIRES_XCDR2_TRUE);
    _union->requires_xcdr2 = IDL_REQUIRES_XCDR2_SETTING;

    bool ret = !idl_is_extensible(node, IDL_FINAL) || set_node_xcdr2_required(idl_type_spec(_union->switch_type_spec));

    idl_case_t *_case;
    IDL_FOREACH(_case, _union->cases) {
      if (set_node_xcdr2_required(idl_type_spec(_case)))
        ret = true;
    };
    _union->requires_xcdr2 = ret ? IDL_REQUIRES_XCDR2_TRUE : IDL_REQUIRES_XCDR2_FALSE;
    return ret;
  }

  return false;
}

idl_retcode_t
idl_set_xcdr2_required(void *node)
{
  idl_retcode_t ret = IDL_RETCODE_OK;
  assert(node);
  for (; ret == IDL_RETCODE_OK && node; node = idl_next(node)) {
    if (idl_mask(node) == IDL_MODULE) {
      idl_module_t *_module = node;
      ret = idl_set_xcdr2_required(_module->definitions);
    } else if (idl_mask(node) == IDL_STRUCT) {
      idl_struct_t *_struct = node;
      if (_struct->requires_xcdr2 == IDL_REQUIRES_XCDR2_UNSET)
        (void) set_node_xcdr2_required(node);
      assert(_struct->requires_xcdr2 != IDL_REQUIRES_XCDR2_SETTING);
    } else if (idl_mask(node) == IDL_UNION) {
      idl_union_t *_union = node;
      if (_union->requires_xcdr2 == IDL_REQUIRES_XCDR2_UNSET)
        (void) set_node_xcdr2_required(node);
      assert(_union->requires_xcdr2 != IDL_REQUIRES_XCDR2_SETTING);
    }
  }
  return ret;
}

idl_retcode_t
idl_finalize_struct(
  idl_pstate_t *state,
  const idl_location_t *location,
  idl_struct_t *node,
  idl_member_t *members)
{
  idl_member_t *member, *member2;
  idl_declarator_t *decl, *decl2;

  if (node->inherit_spec) {
    idl_struct_t *base = node->inherit_spec->base;
    assert(idl_is_struct(base));

    if (node->extensibility.annotation == NULL)
      node->extensibility.value = base->extensibility.value;
    else
      assert (node->extensibility.value == base->extensibility.value);
    if (base->extensibility.value == IDL_APPENDABLE) {
      static bool extensibility_inheritance_warned = false;
      if (!extensibility_inheritance_warned) {
        idl_warning(state, IDL_WARN_INHERIT_APPENDABLE, idl_location(node),
          "Inheriting from appendable structs is unsafe");
        extensibility_inheritance_warned = true;
      }
    }

    /*check for clashes between inherited and inheriting members*/
    IDL_FOREACH(member, members) {
      if (member->key.value) {
        idl_error(state, idl_location(member),
          "Derived struct is not allowed to expand on keys");
        return IDL_RETCODE_SEMANTIC_ERROR;
      }
      IDL_FOREACH(decl, member->declarators) {
        IDL_FOREACH(member2, base->members) {
          IDL_FOREACH(decl2, member2->declarators) {
            if (0 == strcmp(decl->name->identifier,decl2->name->identifier)) {
              idl_error(state, idl_location(decl),
                "Member name %s clashes with declaration in inheritance", decl->name->identifier);
              return IDL_RETCODE_SEMANTIC_ERROR;
            }
          }
        }
      }
    }
  }

  /*check for incomplete member types*/
  IDL_FOREACH(member, members) {
    IDL_FOREACH(decl, member->declarators) {
      if (idl_is_forward(idl_strip(member->type_spec, IDL_STRIP_ALIASES | IDL_STRIP_ALIASES_ARRAY))
          && idl_strip(member->type_spec, 0) == NULL
          && (!member->external.value || idl_is_array(decl))
      ) {
        idl_error(state, idl_location(member), "Member '%s' has incomplete type '%s'", decl->name->identifier, idl_identifier(member->type_spec));
        return IDL_RETCODE_SEMANTIC_ERROR;
      }
    }
  }

  idl_exit_scope(state);
  node->node.symbol.location.last = location->last;
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
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  idl_inherit_spec_t *inherit_spec,
  void *nodep)
{
  idl_retcode_t ret;
  idl_struct_t *node;
  idl_scope_t *scope;
  idl_declaration_t *declaration;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_STRUCT;
  static const struct methods methods = {
    delete_struct, iterate_struct, describe_struct };
  static const enum idl_declaration_kind kind = IDL_SPECIFIER_DECLARATION;

  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    goto err_node;
  node->name = name;
  if ((ret = idl_create_scope(pstate, IDL_STRUCT_SCOPE, name, node, &scope)))
    goto err_scope;
  if ((ret = idl_declare(pstate, kind, name, node, scope, &declaration)))
    goto err_declare;

  if (inherit_spec) {
    const idl_struct_t *base = inherit_spec->base;

    if (!(idl_mask(base) & IDL_STRUCT)) {
      idl_error(pstate, idl_location(base),
        "Struct types cannot inherit from '%s' elements", idl_construct(base));
      return IDL_RETCODE_SEMANTIC_ERROR;
    } else if (inherit_spec->node.next) {
      idl_error(pstate, idl_location(inherit_spec->node.next),
        "Struct types are limited to single inheritance");
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
    /* find scope */
    scope = declaration->scope;
    /* find imported scope */
    declaration = (void*)idl_find(pstate, idl_scope(base), idl_name(base), 0u);
    assert(declaration && declaration->scope);
    if ((ret = idl_import(pstate, scope, declaration->scope)))
      return ret;// goto err_import
    node->inherit_spec = inherit_spec;
    inherit_spec->node.parent = (idl_node_t *)node;
  }

  /* set default extensibility from parser configuration; may be overwritten
     by value from an annotation on this node, or by inheritance */
  node->extensibility.value = pstate->config.default_extensibility >= 0 ? (idl_extensibility_t)pstate->config.default_extensibility : IDL_FINAL;

  idl_enter_scope(pstate, scope);
  *((idl_struct_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_declare:
  idl_delete_scope(scope);
err_scope:
  idl_free(node);
err_node:
  return ret;
}

bool idl_is_empty(const void *node)
{
  if(!idl_is_struct(node))
    return false;
  const idl_struct_t *s = (const idl_struct_t *)node;
  if (!s->members && !s->inherit_spec)
    return true;
  bool empty = true;
  for (idl_member_t *m = s->members; empty && m; m = idl_next(m)) {
    if (m->type_spec == node)
      empty = false;
    else if (!idl_is_empty(m->type_spec))
      empty = false;
  }
  if (empty && s->inherit_spec) {
    idl_struct_t *base_struct = (idl_struct_t*)s->inherit_spec->base;
    return idl_is_empty(base_struct);
  }
  return empty;
}

static void delete_forward(void *ptr)
{
  idl_forward_t *node = ptr;
  idl_delete_name(node->name);
  idl_free(node);
}

static const char *describe_forward(const void *ptr)
{
  (void)ptr;
  idl_mask_t mask = idl_mask(ptr);
  assert(mask & IDL_FORWARD);
  if (mask & IDL_STRUCT)
    return "struct forward declaration";
  else if (mask & IDL_UNION)
    return "union forward declaration";
  else
    abort ();
}

idl_retcode_t
idl_create_forward(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  idl_type_t type,
  void *nodep)
{
  idl_retcode_t ret;
  idl_forward_t *node;
  idl_declaration_t *declaration;
  static const size_t size = sizeof(*node);
  static const struct methods methods = {
    delete_forward, 0, describe_forward };
  static const enum idl_declaration_kind kind = IDL_FORWARD_DECLARATION;

  assert(type == IDL_STRUCT || type == IDL_UNION);

  if ((ret = create_node(pstate, size, type | IDL_FORWARD, location, &methods, &node)))
    goto err_node;
  node->name = name;
  node->type_spec = NULL;
  if ((ret = idl_declare(pstate, kind, name, node, NULL, &declaration)))
    goto err_declare;
  *((idl_forward_t **)nodep) = node;
  return ret;
err_declare:
  idl_free(node);
err_node:
  return ret;
}

bool idl_is_forward(const void *ptr)
{
  const idl_forward_t *node = ptr;

  if (!(idl_mask(node) & IDL_FORWARD))
    return false;
  /* a forward declaration must have a name */
  assert(node->name && node->name->identifier);
  /* a forward must forward declare either a struct or a union */
  assert(!node->type_spec || idl_type(node->type_spec) == IDL_STRUCT
                          || idl_type(node->type_spec) == IDL_UNION);
  return true;
}

bool idl_is_inherit_spec(const void *ptr)
{
  const idl_inherit_spec_t *node = ptr;

  if (!(idl_mask(node) & IDL_INHERIT_SPEC))
    return false;
  /* inheritance specifier must define a base type */
  assert(idl_mask(node->base) & IDL_STRUCT);
  /* inheritance specifier must have a parent of type struct  */
  assert(!node->node.parent || (idl_mask(node->node.parent) & IDL_STRUCT));
  return true;
}

static void delete_inherit_spec(void *ptr)
{
  idl_inherit_spec_t *node = ptr;
  idl_unreference_node(node->base);
  idl_free(node);
}

static const char *describe_inherit_spec(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) & IDL_INHERIT_SPEC);
  return "inheritance specifier";
}

idl_retcode_t
idl_create_inherit_spec(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  void *base,
  void *nodep)
{
  idl_retcode_t ret;
  idl_inherit_spec_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_INHERIT_SPEC;
  static const struct methods methods = {
    delete_inherit_spec, 0, describe_inherit_spec };

  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    return ret;
  node->base = base;
  *((idl_inherit_spec_t **)nodep) = node;
  return IDL_RETCODE_OK;
}

static void delete_key(void *ptr)
{
  idl_key_t *node = ptr;
  idl_delete_field_name(node->field_name);
  idl_free(node);
}

static const char *describe_key(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) & IDL_KEY);
  return "key";
}

idl_retcode_t
idl_create_key(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  void *nodep)
{
  idl_key_t *node = NULL;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_KEY;
  static const struct methods methods = { delete_key, 0, describe_key };

  if (create_node(pstate, size, mask, location, &methods, &node))
    return IDL_RETCODE_NO_MEMORY;
  *((idl_key_t **)nodep) = node;
  return IDL_RETCODE_OK;
}

static void delete_keylist(void *ptr)
{
  idl_keylist_t *node = ptr;
  idl_delete_node(node->keys);
  idl_free(node);
}

static const char *describe_keylist(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) & IDL_KEYLIST);
  return "keylist";
}

idl_retcode_t
idl_create_keylist(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  void *nodep)
{
  idl_retcode_t ret;
  idl_keylist_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_KEYLIST;
  static const struct methods methods = {
    delete_keylist, 0, describe_keylist };

  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    goto err_node;
  *((idl_keylist_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_node:
  return ret;
}

bool idl_is_member(const void *ptr)
{
  const idl_member_t *node = ptr;

  if (!(idl_mask(node) & IDL_MEMBER))
    return false;
  /* a member must have a struct parent (except when under construction) */
  assert(!node->node.parent || (idl_mask(node->node.parent) & IDL_STRUCT));
  /* a member must have a type specifier */
  assert(node->type_spec);
  /* a member must have at least one declarator */
  assert(idl_mask(node->declarators) & IDL_DECLARATOR);
  return true;
}

static void delete_member(void *ptr)
{
  idl_member_t *node = ptr;
  delete_type_spec(node, node->type_spec);
  idl_delete_node(node->declarators);
  idl_free(node);
}

static void *iterate_member(const void *ptr, const void *cur)
{
  const idl_member_t *root = ptr;
  const idl_node_t *node = cur;
  if (node) {
    if (node->next)
      return node->next;
    if (idl_is_annotation_appl(node))
      return root->declarators;
    return NULL;
  }
  if (root->node.annotations)
    return root->node.annotations;
  return root->declarators;
}

static const char *describe_member(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) & IDL_MEMBER);
  return "member";
}

idl_retcode_t
idl_create_member(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_declarator_t *declarators,
  void *nodep)
{
  idl_retcode_t ret;
  idl_member_t *node;
  idl_scope_t *scope = NULL;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_MEMBER;
  static const struct methods methods = {
    delete_member, iterate_member, describe_member };
  static const enum idl_declaration_kind kind = IDL_INSTANCE_DECLARATION;

  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    goto err_node;

  node->type_spec = type_spec;
  if (idl_scope(type_spec)) {
    /* struct and union types introduce a scope. resolve scope and link it for
       field name lookup. e.g. #pragma keylist directives */
    type_spec = idl_strip(type_spec, IDL_STRIP_ALIASES|IDL_STRIP_ALIASES_ARRAY);
    if (idl_is_struct(type_spec) || idl_is_union(type_spec)) {
      const idl_declaration_t *declaration = idl_declaration(type_spec);
      assert(declaration);
      scope = declaration->scope;
      assert(scope);
    }
  } else {
    ((idl_node_t *)type_spec)->parent = (idl_node_t*)node;
  }

  assert(declarators);
  node->declarators = declarators;
  for (idl_declarator_t *d = declarators; d; d = idl_next(d)) {
    assert(idl_mask(d) & IDL_DECLARATOR);
    assert(!d->node.parent);
    d->node.parent = (idl_node_t *)node;
    /* FIXME: embedded structs have a scope, fix when implementing IDL3.5 */
    if ((ret = idl_declare(pstate, kind, d->name, d, scope, NULL)))
      goto err_declare;
  }

  *((idl_member_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_declare:
  idl_free(node);
err_node:
  return ret;
}

bool idl_is_union(const void *ptr)
{
  const idl_union_t *node = ptr;

  if (idl_is_forward(node))
    return false;
  if (!(idl_mask(node) & IDL_UNION))
    return false;
  /* a union must have no parent or a module parent */
  assert(!node->node.parent || (idl_mask(node->node.parent) & IDL_MODULE));
  /* a union must have a name */
  assert(node->name && node->name->identifier);
  /* a union must have a switch type specifier */
  assert(idl_mask(node->switch_type_spec) & IDL_SWITCH_TYPE_SPEC);
  /* a union must have at least one case */
  assert(!node->cases || (idl_mask(node->cases) & IDL_CASE));
  /* a union must have a default case */
  assert(!node->cases || node->default_case);
  return true;
}

static void delete_union(void *ptr)
{
  idl_union_t *node = ptr;
  idl_delete_node(node->switch_type_spec);
  idl_unreference_node(node->default_case);
  idl_delete_node(node->cases);
  idl_delete_name(node->name);
  idl_free(node);
}

static void *iterate_union(const void *ptr, const void *cur)
{
  const idl_union_t *root = ptr;
  const idl_node_t *node = cur;
  assert(root);
  if (node) {
    assert(idl_parent(node) == root);
    if (node->next)
      return node->next;
    if (idl_is_switch_type_spec(node))
      return root->cases;
    if (idl_is_annotation_appl(node))
      return root->switch_type_spec;
    return NULL;
  }
  if (root->node.annotations)
    return root->node.annotations;
  return root->switch_type_spec;
}

static const char *describe_union(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) & IDL_UNION);
  return "union";
}

static uint64_t maximum_labels(const idl_type_spec_t *type_spec)
{
  idl_type_t type;

  assert(type_spec);

  switch ((type = idl_type(type_spec))) {
    case IDL_BOOL:
      return 2u;
    case IDL_CHAR:
    case IDL_OCTET:
    case IDL_INT8:
    case IDL_UINT8:
      return UINT8_MAX;
    case IDL_WCHAR:
    case IDL_SHORT:
    case IDL_USHORT:
    case IDL_INT16:
    case IDL_UINT16:
      return UINT16_MAX;
    case IDL_LONG:
    case IDL_ULONG:
    case IDL_INT32:
    case IDL_UINT32:
      return UINT32_MAX;
    case IDL_LLONG:
    case IDL_ULLONG:
    case IDL_INT64:
    case IDL_UINT64:
      return UINT64_MAX;
    default:
      assert(type == IDL_ENUM);
      return idl_degree(((const idl_enum_t *)type_spec)->enumerators);
  }
}

static int compare_label(const void *lhs, const void *rhs)
{
  switch (idl_compare((*(const idl_case_label_t **)lhs)->const_expr,
                      (*(const idl_case_label_t **)rhs)->const_expr))
  {
    case IDL_LESS:
      return -1;
    case IDL_EQUAL:
      return 0;
    default:
      return 1;
  }
}

static void increment_literal(idl_literal_t *literal)
{
  switch (idl_type(literal)) {
    case IDL_BOOL:
      literal->value.bln = !literal->value.bln;
      break;
    case IDL_CHAR:
      literal->value.chr++;
      break;
    case IDL_INT8:
      literal->value.int8++;
      break;
    case IDL_OCTET:
    case IDL_UINT8:
      literal->value.uint8++;
      break;
    case IDL_SHORT:
    case IDL_INT16:
      literal->value.int16++;
      break;
    case IDL_USHORT:
    case IDL_UINT16:
      literal->value.uint16++;
      break;
    case IDL_LONG:
    case IDL_INT32:
      literal->value.int32++;
      break;
    case IDL_ULONG:
    case IDL_UINT32:
      literal->value.uint32++;
      break;
    case IDL_LLONG:
    case IDL_INT64:
      literal->value.int64++;
      break;
    case IDL_ULLONG:
    case IDL_UINT64:
      literal->value.uint64++;
      break;
    default:
      abort();
  }
}

idl_retcode_t
idl_finalize_union(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_union_t *node,
  idl_case_t *cases)
{
  idl_retcode_t ret;
  const idl_type_spec_t *type_spec;
  idl_case_label_t *default_case = NULL;
  idl_type_t type;
  uint64_t used = 0, maximum;
  idl_case_label_t **labels = NULL;

  assert(node);
  assert(node->switch_type_spec);
  assert(cases);
  type_spec = idl_unalias(node->switch_type_spec->type_spec);
  type = idl_type(type_spec);
  maximum = maximum_labels(type_spec);

  for (idl_case_t *c = cases; c; c = idl_next(c)) {
    assert(c->labels);
    /* iterate case labels and evaluate constant expressions */
    for (idl_case_label_t *cl = c->labels; cl; cl = idl_next(cl)) {
      if (cl->const_expr) {
        idl_const_expr_t *const_expr = NULL;

        used++;
        assert(idl_mask(cl) != IDL_DEFAULT_CASE_LABEL);
        if ((ret = idl_evaluate(pstate, cl->const_expr, type, &const_expr)))
          return ret;
        cl->const_expr = const_expr;
        if (idl_is_enumerator(const_expr)) {
          assert(idl_is_enum(type_spec));
          assert(((const idl_node_t *)const_expr)->parent);
          if (((const idl_node_t *)const_expr)->parent != type_spec) {
            idl_error(pstate, idl_location(cl),
              "Label uses an enumerator of a different enumeration");
            return IDL_RETCODE_SEMANTIC_ERROR;
          }
        } else {
          assert(!((const idl_node_t *)const_expr)->parent);
          ((idl_node_t *)const_expr)->parent = (idl_node_t *)cl;
        }
      } else {
        assert(idl_mask(cl) == IDL_DEFAULT_CASE_LABEL);
        if (default_case) {
          idl_error(pstate, idl_location(cl),
            "More than one default label in switch statement");
          return IDL_RETCODE_SEMANTIC_ERROR;
        }
        default_case = cl;
      }
    }

    /*check for incomplete member types*/
    if (idl_is_forward(idl_strip(c->type_spec, IDL_STRIP_ALIASES | IDL_STRIP_ALIASES_ARRAY))
        && idl_strip(c->type_spec, 0) == NULL
        && (!c->external.value || idl_is_array(c))
    ) {
      idl_error(pstate, idl_location(c), "Member '%s' has incomplete type '%s'", c->declarator->name->identifier, idl_identifier(c->type_spec));
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
  }

  /* a union type can contain a default label only where the values given to
     the non-default labels do not cover the entire range of the union's
     discriminant type */
  if (used == maximum && default_case) {
    idl_error(pstate, idl_location(default_case),
      "Invalid default label in switch statement, non-default labels cover "
      "entire range of switch type");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  if (used != 0) { /* sort labels to detect duplicates and determine default */
    size_t count;

    if (!(labels = idl_malloc((size_t)used * sizeof(*labels))))
      return IDL_RETCODE_NO_MEMORY;

    count = 0;
    for (const idl_case_t *c = cases; c; c = idl_next(c)) {
      for (const idl_case_label_t *cl = c->labels; cl; cl = idl_next(cl)) {
        if (!cl->const_expr)
          continue;
        assert(count < used);
        labels[count++] = (idl_case_label_t *)cl;
      }
    }

    assert(count == used);
    qsort(labels, (size_t)used, sizeof(*labels), &compare_label);

    /* if a member corresponds to the default case label, its simple modifier
       shall set the discriminant to the first available default value
       starting from a 0 index of the discriminant type */
    count = 1;
    for (; count < used; count++) {
      if (compare_label(&labels[count - 1], &labels[count]) != IDL_EQUAL)
        continue;
      idl_error(pstate, idl_location(labels[count]),
        "Duplicate case label in switch statement");
      idl_free(labels);
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
  }

  /* Java, C# and C++ map unions to a class which must provide a default
     constructor that shall set the discriminator to the default value for
     the discriminator type. if there is a default case, the union is
     initialized to this default case. in case the union has an implicit
     default member, i.e. not every permissable value of the union
     discriminant is listed and no default case is specified, it is
     initialized to that case. in all other cases it is initialized to the
     first discriminant value specified in IDL. */

  if (used == maximum) {
    node->unused_labels = 0;
    node->default_case = idl_reference_node(cases->labels);
  } else {
    size_t count;
    idl_const_expr_t *const_expr = NULL;

    node->unused_labels = maximum - used;
    if (default_case) {
      node->default_case = idl_reference_node(default_case);
    } else {
      if (idl_create_case_label(pstate, location, NULL, &default_case))
        goto err_case;
      default_case->node.mask = IDL_IMPLICIT_DEFAULT_CASE_LABEL;
      default_case->node.parent = (idl_node_t *)node;
      node->default_case = default_case;
    }

    /* if a member corresponds to the default case label, its simple modifier
       shall set the discriminant to the first available default value
       starting from a 0 index of the discriminant type */
    if (type == IDL_ENUM) {
      idl_equality_t eq;
      /* enumerators are referenced, logic is slightly different */
      const_expr = ((const idl_enum_t *)type_spec)->enumerators;
      for (count=0; const_expr && count < used; count++) {
        assert(labels);
        eq = idl_compare(const_expr, labels[count]->const_expr);
        if (eq == IDL_LESS)
          break;
        if (eq == IDL_EQUAL)
          const_expr = idl_next(const_expr);
      }
      assert(const_expr);
      default_case->const_expr = idl_reference_node(const_expr);
    } else {
      idl_equality_t eq;
      idl_literal_t *literal = NULL;
      if (idl_create_literal(pstate, location, type, &literal))
        goto err_const_expr;
      memset(&literal->value, 0, sizeof(literal->value));
      default_case->const_expr = literal;
      literal->node.parent = (idl_node_t *)default_case;
      const_expr = literal;
      for (count = 0; count < used; count++) {
        assert(labels);
        eq = idl_compare(const_expr, labels[count]->const_expr);
        if (eq == IDL_LESS)
          break;
        if (eq == IDL_EQUAL)
          increment_literal(const_expr);
      }
      assert(count <= used);
    }
  }

  node->node.symbol.location.last = location->last;
  node->cases = cases;
  for (idl_case_t *c = node->cases; c; c = idl_next(c))
    c->node.parent = (idl_node_t *)node;
  idl_exit_scope(pstate);

  idl_free(labels);
  return IDL_RETCODE_OK;
err_const_expr:
err_case:
  idl_free(labels);
  return IDL_RETCODE_NO_MEMORY;
}

idl_retcode_t
idl_create_union(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  idl_switch_type_spec_t *switch_type_spec,
  void *nodep)
{
  idl_retcode_t ret;
  idl_union_t *node;
  idl_scope_t *scope;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_UNION;
  static const struct methods methods = {
    delete_union, iterate_union, describe_union };
  static const enum idl_declaration_kind kind = IDL_SPECIFIER_DECLARATION;

  if (!idl_is_switch_type_spec(switch_type_spec)) {
    static const char *fmt =
      "Type '%s' is not a valid switch type specifier";
    idl_error(pstate, idl_location(switch_type_spec), fmt, idl_construct(switch_type_spec));
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    goto err_node;
  if ((ret = idl_create_scope(pstate, IDL_UNION_SCOPE, name, node, &scope)))
    goto err_scope;
  if ((ret = idl_declare(pstate, kind, name, node, scope, NULL)))
    goto err_declare;

  /* set default extensibility from parser configuration; may be overwritten
     by value from an annotation on this node */
  node->extensibility.value = pstate->config.default_extensibility >= 0 ? (idl_extensibility_t)pstate->config.default_extensibility : IDL_FINAL;

  idl_enter_scope(pstate, scope);
  *((idl_union_t **)nodep) = node;
  node->name = name;
  node->switch_type_spec = switch_type_spec;
  assert(!idl_scope(switch_type_spec));
  ((idl_node_t *)switch_type_spec)->parent = (idl_node_t *)node;
  return ret;
err_declare:
  idl_delete_scope(scope);
err_scope:
  idl_free(node);
err_node:
  return ret;
}

bool
idl_is_switch_type_spec(const void *ptr)
{
  const idl_switch_type_spec_t *node = ptr;
  const idl_type_spec_t *type_spec;

  if (!(idl_mask(node) & IDL_SWITCH_TYPE_SPEC))
    return false;
  /* a switch type specifier must have a union parent */
  assert(!node->node.parent || (idl_mask(node->node.parent) & IDL_UNION));
  /* a switch type specifier must have a type specifier */
  type_spec = node->type_spec;
  assert(type_spec);
  type_spec = idl_unalias(type_spec);
  switch (idl_type(type_spec)) {
    case IDL_ENUM:
      return true;
    case IDL_WCHAR:
    case IDL_OCTET:
#if 0
      assert(pstate->config.flags & IDL_FLAG_EXTENDED_DATA_TYPES);
#endif
      return true;
    default:
      assert(idl_is_base_type(type_spec));
      return true;
  }
}

static void delete_switch_type_spec(void *ptr)
{
  idl_switch_type_spec_t *node = ptr;
  delete_type_spec(node, node->type_spec);
  idl_free(node);
}

static void *iterate_switch_type_spec(const void *ptr, const void *cur)
{
  const idl_switch_type_spec_t *root = ptr;
  const idl_node_t *node = cur;
  assert(root);
  if (node) {
    assert(idl_parent(node) == root);
    if (node->next)
      return node->next;
    return NULL;
  }
  return root->node.annotations;
}

static const char *describe_switch_type_spec(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) & IDL_SWITCH_TYPE_SPEC);
  return "switch type specifier";
}

idl_retcode_t
idl_create_switch_type_spec(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_type_spec_t *type_spec,
  void *nodep)
{
  idl_retcode_t ret;
  idl_switch_type_spec_t *node = NULL;
  idl_type_t type;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_SWITCH_TYPE_SPEC;
  static const struct methods methods = { delete_switch_type_spec,
                                          iterate_switch_type_spec,
                                          describe_switch_type_spec };
  bool ext = (pstate->config.flags & IDL_FLAG_EXTENDED_DATA_TYPES) != 0;

  assert(type_spec);
  type = idl_type(idl_unalias(type_spec));
  if (!(type == IDL_ENUM) &&
      !(type == IDL_BOOL) &&
      !(type == IDL_CHAR) &&
      !(((unsigned)type & (unsigned)IDL_INTEGER_TYPE) == IDL_INTEGER_TYPE) &&
      !(ext && type == IDL_OCTET) &&
      !(ext && type == IDL_WCHAR))
  {
    idl_error(pstate, idl_location(type_spec),
      "Invalid switch type '%s'", idl_construct(type_spec));
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    return ret;
  node->type_spec = type_spec;
  if (!idl_scope(type_spec))
    ((idl_node_t *)type_spec)->parent = (idl_node_t *)node;
  *((idl_switch_type_spec_t **)nodep) = node;
  return IDL_RETCODE_OK;
}

bool idl_is_case(const void *ptr)
{
  const idl_case_t *node = ptr;
  if (!(idl_mask(node) & IDL_CASE))
    return false;
  /* a case must have a union parent */
  assert(!node->node.parent || (idl_mask(node->node.parent) & IDL_UNION));
  /* a case must have at least one case label */
  assert(!node->labels || (idl_mask(node->labels) & IDL_CASE_LABEL));
  /* a case must have exactly one declarator */
  assert(idl_mask(node->declarator) & IDL_DECLARATOR);
  return true;
}

bool idl_is_default_case(const void *ptr)
{
  const idl_case_t *node = ptr;
  static const idl_mask_t mask = IDL_DEFAULT_CASE_LABEL;
  if (!(idl_mask(node) & IDL_CASE))
    return false;
  for (const idl_case_label_t *cl = node->labels; cl; cl = idl_next(cl))
    if ((idl_mask(cl) & mask) == mask)
      return true;
  return false;
}

bool idl_is_implicit_default_case(const void *ptr)
{
  const idl_case_t *node = ptr;
  static const idl_mask_t mask = IDL_IMPLICIT_DEFAULT_CASE_LABEL;
  if (!(idl_mask(node) & IDL_CASE))
    return false;
  for (const idl_case_label_t *cl = node->labels; cl; cl = idl_next(cl))
    if ((idl_mask(cl) & mask) == mask)
      return true;
  return false;
}

static void delete_case(void *ptr)
{
  idl_case_t *node = ptr;
  delete_type_spec(node, node->type_spec);
  idl_delete_node(node->labels);
  idl_delete_node(node->declarator);
  idl_free(node);
}

static void *iterate_case(const void *ptr, const void *cur)
{
  const idl_case_t *root = ptr;
  const idl_node_t *node = cur;
  assert(root);
  if (node) {
    assert(idl_parent(node) == root);
    if (node->next)
      return node->next;
    if (idl_is_case_label(node))
      return root->declarator;
    return NULL;
  }
  return root->labels;
}

static const char *describe_case(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) == IDL_CASE);
  return "switch case";
}

idl_retcode_t
idl_finalize_case(
  idl_pstate_t *state,
  const idl_location_t *location,
  idl_case_t *node,
  idl_case_label_t *labels)
{
  (void)state;
  node->node.symbol.location.last = location->last;
  node->labels = labels;
  for (idl_case_label_t *cl = node->labels; cl; cl = idl_next(cl))
    cl->node.parent = (idl_node_t*)node;
  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_create_case(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_declarator_t *declarator,
  void *nodep)
{
  idl_retcode_t ret;
  idl_case_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_CASE;
  static const struct methods methods = {
    delete_case, iterate_case, describe_case };
  static const enum idl_declaration_kind kind = IDL_INSTANCE_DECLARATION;
  idl_scope_t *scope = NULL;

  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    goto err_node;
  node->type_spec = type_spec;
  if (idl_scope(type_spec)) {
    /* struct and union types introduce a scope. resolve the scope and link it
       for field name lookup. e.g. #pragma keylist directives */
    type_spec = idl_unalias(type_spec);
    if (idl_is_struct(type_spec) || idl_is_union(type_spec)) {
      const idl_declaration_t *declaration;

      declaration = idl_find(
        pstate, idl_scope(type_spec), idl_name(type_spec), 0u);
      assert(declaration);
      scope = declaration->scope;
    }
  } else {
    ((idl_node_t*)type_spec)->parent = (idl_node_t *)node;
  }
  node->declarator = declarator;
  assert(!declarator->node.parent);
  declarator->node.parent = (idl_node_t *)node;
  assert(!declarator->node.next);
  if ((ret = idl_declare(pstate, kind, declarator->name, declarator, scope, NULL)))
    goto err_declare;

  *((idl_case_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_declare:
  idl_free(node);
err_node:
  return ret;
}

bool idl_is_case_label(const void *ptr)
{
#if !defined(NDEBUG)
  static const idl_mask_t mask = IDL_LITERAL | IDL_ENUMERATOR;
#endif
  const idl_case_label_t *node = ptr;

  if (!(idl_mask(node) & IDL_CASE_LABEL))
    return false;
  /* a case label must have a case parent */
  assert(!node->node.parent || (idl_mask(node->node.parent) & IDL_CASE));
  /* a case labels may have an expression (default case does not) */
  assert(!node->const_expr || (idl_mask(node->const_expr) & mask));
  return true;
}

static bool idl_is_case_label_mask_impl(const void *ptr, const idl_mask_t mask)
{
  const idl_case_label_t *node = ptr;
  if (!(idl_mask(node) & IDL_CASE_LABEL))
    return false;
  if ((idl_mask(node) & mask) == mask)
    return true;
  return false;
}

bool idl_is_default_case_label(const void *ptr)
{
  return idl_is_case_label_mask_impl (ptr, IDL_DEFAULT_CASE_LABEL);
}

bool idl_is_implicit_default_case_label(const void *ptr)
{
  return idl_is_case_label_mask_impl (ptr, IDL_IMPLICIT_DEFAULT_CASE_LABEL);
}

static void delete_case_label(void *ptr)
{
  idl_case_label_t *node = ptr;
  delete_const_expr(node, node->const_expr);
  idl_free(node);
}

static const char *describe_case_label(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) == IDL_CASE_LABEL);
  if (idl_mask(ptr) == IDL_DEFAULT_CASE_LABEL)
    return "default switch case label";
  if (idl_mask(ptr) == IDL_IMPLICIT_DEFAULT_CASE_LABEL)
    return "implicit default switch case label";
  return "switch case label";
}

idl_retcode_t
idl_create_case_label(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_const_expr_t *const_expr,
  void *nodep)
{
  idl_retcode_t ret;
  idl_case_label_t *node;
  static const size_t size = sizeof(*node);
  const idl_mask_t mask =
    const_expr ? IDL_CASE_LABEL : IDL_DEFAULT_CASE_LABEL;
  static const struct methods methods = {
    delete_case_label, 0, describe_case_label };

  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    return ret;
  node->const_expr = const_expr;
  if (const_expr && !idl_scope(const_expr))
    ((idl_node_t *)const_expr)->parent = (idl_node_t *)node;

  *((idl_case_label_t **)nodep) = node;
  return IDL_RETCODE_OK;
}

int64_t idl_case_label_intvalue(const void *ptr)
{
  const idl_case_label_t *node = ptr;
  if (!(idl_mask(node) & IDL_CASE_LABEL))
    return -1;
  idl_type_t type = idl_type(node->const_expr);
  if (type & IDL_INTEGER_TYPE) {
    idl_intval_t val = idl_intval(node->const_expr);
    return val.value.llng;
  } else if (type == IDL_CHAR) {
    idl_literal_t *literal = node->const_expr;
    return literal->value.chr;
  } else if (type == IDL_BOOL) {
    idl_literal_t *literal = node->const_expr;
    return literal->value.bln;
  } else if (type == IDL_OCTET) {
    idl_literal_t *literal = node->const_expr;
    return literal->value.uint8;
  } else if (type == IDL_ENUM) {
    idl_enumerator_t *enumerator = node->const_expr;
    assert(enumerator->value.value <= INT32_MAX);
    return enumerator->value.value;
  } else {
    assert(false);
  }
  return 0;
}

bool idl_is_enum(const void *ptr)
{
  const idl_enum_t *node = ptr;

  if (!(idl_mask(node) & IDL_ENUM))
    return false;
  /* an enum must have a name */
  assert(node->name && node->name->identifier);
  /* an enum must have no parent or a module or annotation parent */
  assert(!node->node.parent || idl_mask(node->node.parent) & (IDL_MODULE | IDL_ANNOTATION));
  /* an enum must have at least one enumerator */
  assert(node->enumerators && (idl_mask(node->enumerators) & IDL_ENUMERATOR));
  return true;
}

uint32_t idl_enum_max_value(const void *ptr)
{
  const idl_enum_t *node = ptr;
  assert(idl_mask(node) & IDL_ENUM);
  uint32_t max = 0;
  for (idl_enumerator_t *e = node->enumerators; e; e = idl_next(e)) {
    if (e->value.value > max)
      max = e->value.value;
  }
  return max;
}

static void delete_enum(void *ptr)
{
  idl_enum_t *node = ptr;
  idl_delete_node(node->enumerators);
  idl_delete_name(node->name);
  idl_free(node);
}

static void *iterate_enum(const void *ptr, const void *cur)
{
  const idl_enum_t *root = ptr;
  const idl_node_t *node = cur;
  assert(root);
  if (node) {
    assert(idl_parent(node) == root);
    if (node->next)
      return node->next;
    if (idl_is_annotation_appl(node))
      return root->enumerators;
    return NULL;
  }
  if (root->node.annotations)
    return root->node.annotations;
  return root->enumerators;
}

static const char *describe_enum(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) == IDL_ENUM);
  return "enum";
}

idl_retcode_t
idl_create_enum(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  idl_enumerator_t *enumerators,
  void *nodep)
{
  idl_retcode_t ret;
  idl_enum_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_ENUM;
  static const struct methods methods = {
    delete_enum, iterate_enum, describe_enum };
  static const enum idl_declaration_kind kind = IDL_SPECIFIER_DECLARATION;
  uint32_t value = UINT32_MAX;

  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    goto err_alloc;
  node->name = name;
  node->bit_bound.value = 32; /* default value, can be overwritten with @bit_bound */
  node->extensibility.value = pstate->config.default_extensibility == IDL_APPENDABLE ? (idl_extensibility_t)pstate->config.default_extensibility : IDL_FINAL;

  assert(enumerators);
  node->enumerators = enumerators;
  for (idl_enumerator_t *e1 = enumerators; e1; e1 = idl_next(e1)) {
    e1->node.parent = (idl_node_t*)node;
    if (e1->value.annotation) {
      //explicit value assigned
      value = e1->value.value;
    } else {
      //implicit value derived

      //check for possible wraparound
      if (value == UINT32_MAX && idl_previous(e1)) {
        idl_error(pstate, idl_location(e1),
          "Implicit value of enumerator '%s' will wrap around due to previous value being UINT32_MAX (%u).",
          e1->name->identifier, UINT32_MAX);
        ret = IDL_RETCODE_OUT_OF_RANGE;
        goto err_wraparound;
      }
      e1->value.value = ++value;
    }

    for (idl_enumerator_t *e2 = enumerators; e2; e2 = idl_next(e2)) {
      if (e2 == e1)
        break;
      if (e2->value.value != e1->value.value)
        continue;
      idl_error(pstate, idl_location(e1),
        "Value of enumerator '%s' clashes with the value of enumerator '%s'",
        e1->name->identifier, e2->name->identifier);
      ret = IDL_RETCODE_SEMANTIC_ERROR;
      goto err_clash;
    }
    if (idl_mask(e1) == IDL_DEFAULT_ENUMERATOR) {
      if (node->default_enumerator) {
        idl_error(pstate, idl_location(e1),
          "Assigning default to enumerator '%s' clashes with '%s' already being set as default.",
          e1->name->identifier, node->default_enumerator->name->identifier);
        ret = IDL_RETCODE_SEMANTIC_ERROR;
        goto err_defaults;
      } else {
        node->default_enumerator = e1;
      }
    }
  }

  //fallback to the first entry
  if (!node->default_enumerator) {
    node->default_enumerator = enumerators;
    node->default_enumerator->node.mask = IDL_IMPLICIT_DEFAULT_ENUMERATOR;
  }

  if ((ret = idl_declare(pstate, kind, name, node, NULL, NULL)))
    goto err_declare;

  *((idl_enum_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_declare:
err_clash:
err_defaults:
err_wraparound:
  idl_free(node);
err_alloc:
  return ret;
}

bool idl_is_enumerator(const void *ptr)
{
  const idl_enumerator_t *node = ptr;

  if (!(idl_mask(node) & IDL_ENUMERATOR))
    return false;
  /* an enumerator must have an enum parent */
  assert(!node->node.parent || (idl_mask(node->node.parent) & IDL_ENUM));
  return true;
}

static void delete_enumerator(void *ptr)
{
  idl_enumerator_t *node = ptr;
  idl_delete_name(node->name);
  idl_free(node);
}

static void *iterate_enumerator(const void *ptr, const void *cur)
{
  const idl_enumerator_t *root = ptr;
  const idl_node_t *node = cur;
  assert(root);
  assert(!node || idl_parent(node) == root);
  if (node)
    return node->next;
  return root->node.annotations;
}

static const char *describe_enumerator(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) == IDL_ENUMERATOR
      || idl_mask(ptr) == IDL_DEFAULT_ENUMERATOR
      || idl_mask(ptr) == IDL_IMPLICIT_DEFAULT_ENUMERATOR);
  return "enumerator";
}

idl_retcode_t
idl_create_enumerator(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  void *nodep)
{
  idl_retcode_t ret;
  idl_enumerator_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_ENUMERATOR;
  static const struct methods methods = {
    delete_enumerator, iterate_enumerator, describe_enumerator };
  static const enum idl_declaration_kind kind = IDL_SPECIFIER_DECLARATION;

  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    goto err_alloc;
  node->name = name;
  if ((ret = idl_declare(pstate, kind, name, node, NULL, NULL)))
    goto err_declare;

  *((idl_enumerator_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_declare:
  idl_free(node);
err_alloc:
  return ret;
}

bool idl_is_bitmask(const void *ptr)
{
  const idl_bitmask_t *node = ptr;

  if (!(idl_mask(node) & IDL_BITMASK))
    return false;
  /* a bitmask must have a name */
  assert(node->name && node->name->identifier);
  /* an bitmask must have no parent or a module parent */
  assert(!node->node.parent || (idl_mask(node->node.parent) & IDL_MODULE));
  /* an bitmask must have at least one bit value */
  assert(node->bit_values && (idl_mask(node->bit_values) & IDL_BIT_VALUE));
  return true;
}

static void delete_bitmask(void *ptr)
{
  idl_bitmask_t *node = ptr;
  idl_delete_node(node->bit_values);
  idl_delete_name(node->name);
  idl_free(node);
}

static void *iterate_bitmask(const void *ptr, const void *cur)
{
  const idl_bitmask_t *root = ptr;
  const idl_node_t *node = cur;
  assert(root);
  if (node) {
    assert(idl_parent(node) == root);
    if (node->next)
      return node->next;
    if (idl_is_annotation_appl(node))
      return root->bit_values;
    return NULL;
  }
  if (root->node.annotations)
    return root->node.annotations;
  return root->bit_values;
}

static const char *describe_bitmask(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) == IDL_BITMASK);
  return "bitmask";
}

idl_retcode_t
idl_create_bitmask(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  idl_bit_value_t *bit_values,
  void *nodep)
{
  idl_retcode_t ret;
  idl_bitmask_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_BITMASK;
  static const struct methods methods = {
    delete_bitmask, iterate_bitmask, describe_bitmask };
  static const enum idl_declaration_kind kind = IDL_SPECIFIER_DECLARATION;
  uint16_t position = 0;

  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    goto err_alloc;
  node->name = name;
  node->bit_bound.value = 32; /* default value, can be overwritten with @bit_bound */

  assert(bit_values);
  node->bit_values = bit_values;
  for (idl_bit_value_t *b1 = bit_values; b1; b1 = idl_next(b1), position++) {
    b1->node.parent = (idl_node_t*)node;
    if (b1->position.annotation)
      position = b1->position.value;
    else
      b1->position.value = position;
    for (idl_bit_value_t *b2 = bit_values; b2; b2 = idl_next(b2)) {
      if (b2 == b1)
        break;
      if (b2->position.value != b1->position.value)
        continue;
      idl_error(pstate, idl_location(b1),
        "Position of bit value '%s' clashes with the position of bit value '%s'",
        b1->name->identifier, b2->name->identifier);
      ret = IDL_RETCODE_SEMANTIC_ERROR;
      goto err_clash;
    }
  }

  if ((ret = idl_declare(pstate, kind, name, node, NULL, NULL)))
    goto err_declare;

  *((idl_bitmask_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_declare:
err_clash:
  idl_free(node);
err_alloc:
  return ret;
}

bool idl_is_bit_value(const void *ptr)
{
  const idl_bit_value_t *node = ptr;

  if (!(idl_mask(node) & IDL_BIT_VALUE))
    return false;
  /* an enumerator must have an enum parent */
  assert(!node->node.parent || (idl_mask(node->node.parent) & IDL_BITMASK));
  return true;
}

static void delete_bit_value(void *ptr)
{
  idl_bit_value_t *node = ptr;
  idl_delete_name(node->name);
  idl_free(node);
}

static void *iterate_bit_value(const void *ptr, const void *cur)
{
  const idl_bit_value_t *root = ptr;
  const idl_node_t *node = cur;
  assert(root);
  assert(!node || idl_parent(node) == root);
  if (node)
    return node->next;
  return root->node.annotations;
}

static const char *describe_bit_value(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) == IDL_BIT_VALUE);
  return "bit_value";
}

idl_retcode_t
idl_create_bit_value(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  void *nodep)
{
  idl_retcode_t ret;
  idl_bit_value_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_BIT_VALUE;
  static const struct methods methods = {
    delete_bit_value, iterate_bit_value, describe_bit_value };
  static const enum idl_declaration_kind kind = IDL_SPECIFIER_DECLARATION;

  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    goto err_alloc;
  node->name = name;
  if ((ret = idl_declare(pstate, kind, name, node, NULL, NULL)))
    goto err_declare;

  *((idl_bit_value_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_declare:
  idl_free(node);
err_alloc:
  return ret;
}

bool idl_is_alias(const void *ptr)
{
  const idl_declarator_t *node = ptr;

  if (!(idl_mask(node) & IDL_DECLARATOR))
    return false;

  /* a declarator is an alias if its parent is a typedef */
  return (idl_mask(node->node.parent) & IDL_TYPEDEF) != 0;
}

bool idl_is_typedef(const void *ptr)
{
  const idl_typedef_t *node = ptr;

  if (!(idl_mask(node) & IDL_TYPEDEF))
    return false;
  /* a typedef must have a type specifier */
  assert(node->type_spec);
  /* a typedef must have no parent or a module parent */
  assert(!node->node.parent || (idl_mask(node->node.parent) & IDL_MODULE));
  /* a typedef must have at least one declarator */
  assert(idl_mask(node->declarators) & IDL_DECLARATOR);
  return true;
}

static void delete_typedef(void *ptr)
{
  idl_typedef_t *node = ptr;
  delete_type_spec(node, node->type_spec);
  idl_delete_node(node->declarators);
  idl_free(node);
}

static const char *describe_typedef(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) == IDL_TYPEDEF);
  return "typedef";
}

idl_retcode_t
idl_create_typedef(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_declarator_t *declarators,
  void *nodep)
{
  idl_retcode_t ret;
  idl_typedef_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_TYPEDEF;
  static const struct methods methods = {
    delete_typedef, 0, describe_typedef };
  static const enum idl_declaration_kind kind = IDL_SPECIFIER_DECLARATION;

  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    goto err_alloc;
  node->type_spec = type_spec;
  if (!idl_scope(type_spec))
    ((idl_node_t*)type_spec)->parent = (idl_node_t*)node;
  node->declarators = declarators;
  for (idl_declarator_t *d = declarators; d; d = idl_next(d)) {
    if (idl_is_array(d)
        && idl_is_forward(idl_strip(type_spec, IDL_STRIP_ALIASES | IDL_STRIP_ALIASES_ARRAY))
        && idl_strip(type_spec, 0) == NULL
    ) {
      idl_error(pstate, idl_location(d), "Array has incomplete element type '%s'", idl_identifier(type_spec));
      ret = IDL_RETCODE_SEMANTIC_ERROR;
      goto err_declare;
    }

    assert(!d->node.parent);
    d->node.parent = (idl_node_t*)node;
    /* declarators can introduce an array, hence type definitions must refer
       to the declarator node, not the typedef node */
    if ((ret = idl_declare(pstate, kind, d->name, d, NULL, NULL)))
      goto err_declare;
  }

  *((idl_typedef_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_declare:
  idl_free(node);
err_alloc:
  return ret;
}

bool idl_is_declarator(const void *ptr)
{
  const idl_declarator_t *node = ptr;

  if (!(idl_mask(node) & IDL_DECLARATOR))
    return false;
  /* a declarator must have an identifier */
  assert(node->name->identifier);
  /* a declarator must have a parent */
  assert(node->node.parent);
  /* a declarator can have sizes */
  assert(!node->const_expr || (idl_mask(node->const_expr) & IDL_LITERAL));
  return true;
}

bool idl_is_array(const void *ptr)
{
  const idl_declarator_t *node = ptr;

  if (!(idl_mask(node) & IDL_DECLARATOR))
    return false;
  return node->const_expr != NULL;
}

static void delete_declarator(void *ptr)
{
  idl_declarator_t *node = ptr;
  idl_delete_node(node->const_expr);
  idl_delete_name(node->name);
  idl_free(node);
}

static const char *describe_declarator(const void *ptr)
{
  const idl_node_t *node = ptr;
  assert(idl_mask(node) == IDL_DECLARATOR);
  if (idl_mask(node->parent) == IDL_TYPEDEF)
    return "typedef declarator";
  else if (idl_mask(node->parent) == IDL_MEMBER)
    return "member declarator";
  else if (idl_mask(node->parent) == IDL_CASE)
    return "element declarator";
  else if (idl_mask(node->parent) == IDL_ANNOTATION_MEMBER)
    return "annotation member declarator";
  assert(!node->parent);
  return "declarator";
}

idl_retcode_t
idl_create_declarator(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  idl_const_expr_t *const_expr,
  void *nodep)
{
  idl_retcode_t ret;
  idl_declarator_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_DECLARATOR;
  static const struct methods methods = {
    delete_declarator, 0, describe_declarator };

  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    return ret;
  node->name = name;
  node->const_expr = const_expr;
  for (idl_const_expr_t *ce = const_expr; ce; ce = idl_next(ce)) {
    assert(idl_mask(ce) & IDL_LITERAL);
    assert(!((idl_node_t *)ce)->parent);
    ((idl_node_t *)ce)->parent = (idl_node_t *)node;
  }
  *((idl_declarator_t **)nodep) = node;
  return ret;
}

bool idl_is_annotation_member(const void *ptr)
{
  const idl_annotation_member_t *node = ptr;
  if (!(idl_mask(node) & IDL_ANNOTATION_MEMBER))
    return false;
  assert(node->type_spec);
  assert(node->declarator);
  return true;
}

static void delete_annotation_member(void *ptr)
{
  idl_annotation_member_t *node = ptr;
  delete_type_spec(node, node->type_spec);
  delete_const_expr(node, node->const_expr);
  idl_delete_node(node->declarator);
  idl_free(node);
}

static const char *describe_annotation_member(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) == IDL_ANNOTATION_MEMBER);
  return "annotation member";
}

idl_retcode_t
idl_create_annotation_member(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_declarator_t *declarator,
  idl_const_expr_t *const_expr,
  void *nodep)
{
  idl_retcode_t ret;
  idl_annotation_member_t *node = NULL;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_ANNOTATION_MEMBER;
  static const struct methods methods = {
    delete_annotation_member, 0, describe_annotation_member };
  static const enum idl_declaration_kind kind = IDL_SPECIFIER_DECLARATION;

  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    goto err_alloc;
  if ((ret = idl_declare(pstate, kind, declarator->name, declarator, NULL, NULL)))
    goto err_declare;
  node->type_spec = type_spec;
  if (!idl_scope(type_spec))
    ((idl_node_t *)type_spec)->parent = (idl_node_t *)node;
  node->declarator = declarator;
  ((idl_node_t *)declarator)->parent = (idl_node_t *)node;
  if (idl_is_enumerator(const_expr)) {
    assert(((idl_node_t *)const_expr)->references > 1);
    /* verify type specifier is an enum */
    if (!idl_is_enum(type_spec)) {
      idl_error(pstate, idl_location(const_expr),
        "Invalid default %s for %s", idl_identifier(const_expr), idl_identifier(declarator));
      goto err_enum;
    /* verify enumerator is defined within enum */
    } else if (((const idl_node_t *)const_expr)->parent != (const idl_node_t *)type_spec) {
      idl_error(pstate, idl_location(const_expr),
        "Invalid default %s for %s", idl_identifier(const_expr), idl_identifier(declarator));
      goto err_enum;
    }
    node->const_expr = const_expr;
  } else if (const_expr) {
    idl_type_t type = idl_type(type_spec);
    idl_literal_t *literal = NULL;
    assert(idl_mask(const_expr) & (IDL_CONST|IDL_LITERAL));
    if ((ret = idl_evaluate(pstate, const_expr, type, &literal)))
      goto err_evaluate;
    assert(literal);
    node->const_expr = literal;
    ((idl_node_t *)literal)->parent = (idl_node_t *)node;
  }

  *((idl_annotation_member_t **)nodep) = node;
  return IDL_RETCODE_OK;
err_enum:
  ret = IDL_RETCODE_SEMANTIC_ERROR;
err_evaluate:
err_declare:
  idl_free(node);
err_alloc:
  return ret;
}

static bool
type_is_consistent(
  idl_pstate_t *pstate,
  const idl_type_spec_t *lhs,
  const idl_type_spec_t *rhs)
{
  const idl_scope_t *lscp, *rscp;
  const idl_name_t *lname, *rname;

  (void)pstate;
  lscp = idl_scope(lhs);
  rscp = idl_scope(rhs);
  if (!lscp != !rscp)
    return false;
  if (!lscp)
    return idl_type(lhs) == idl_type(rhs);
  if (lscp->kind != rscp->kind)
    return false;
  if (lscp->kind != IDL_ANNOTATION_SCOPE)
    return lhs == rhs;
  if (idl_type(lhs) != idl_type(rhs))
    return false;
  if (idl_is_typedef(lhs)) {
    assert(idl_is_typedef(rhs));
    lname = idl_name(((idl_typedef_t *)lhs)->declarators);
    rname = idl_name(((idl_typedef_t *)rhs)->declarators);
  } else {
    lname = idl_name(lhs);
    rname = idl_name(rhs);
  }
  return strcmp(lname->identifier, rname->identifier) == 0;
}

static idl_retcode_t
enum_is_consistent(
  idl_pstate_t *pstate,
  const idl_enum_t *lhs,
  const idl_enum_t *rhs)
{
  const idl_enumerator_t *a, *b;
  size_t n = 0;

  (void)pstate;
  for (a = lhs->enumerators; a; a = idl_next(a), n++) {
    for (b = rhs->enumerators; b; b = idl_next(b))
      if (strcmp(idl_identifier(a), idl_identifier(b)) == 0)
        break;
    if (!b || a->value.value != b->value.value)
      return false;
  }

  return n == idl_degree(rhs->enumerators);
}

static bool
const_is_consistent(
  idl_pstate_t *pstate,
  const idl_const_t *lhs,
  const idl_const_t *rhs)
{
  if (strcmp(idl_identifier(lhs), idl_identifier(rhs)) != 0)
    return false;
  if (!type_is_consistent(pstate, lhs->type_spec, rhs->type_spec))
    return false;
  return idl_compare(lhs->const_expr, rhs->const_expr) == IDL_EQUAL;
}

static bool
typedef_is_consistent(
  idl_pstate_t *pstate,
  const idl_typedef_t *lhs,
  const idl_typedef_t *rhs)
{
  const idl_declarator_t *a, *b;
  size_t n = 0;

  /* typedefs may have multiple associated declarators */
  for (a = lhs->declarators; a; a = idl_next(a), n++) {
    for (b = rhs->declarators; b; b = idl_next(b))
      if (strcmp(idl_identifier(a), idl_identifier(b)) == 0)
        break;
    if (!b)
      return false;
  }

  if (n != idl_degree(rhs->declarators))
    return false;
  return type_is_consistent(pstate, lhs->type_spec, rhs->type_spec);
}

static bool
member_is_consistent(
  idl_pstate_t *pstate,
  const idl_annotation_member_t *lhs,
  const idl_annotation_member_t *rhs)
{
  if (strcmp(idl_identifier(lhs), idl_identifier(rhs)) != 0)
    return false;
  if (!type_is_consistent(pstate, lhs->type_spec, rhs->type_spec))
    return false;
  if (!lhs->const_expr != !rhs->const_expr)
    return false;
  if (!lhs->const_expr)
    return true;
  return idl_compare(lhs->const_expr, rhs->const_expr) == IDL_EQUAL;
}

static bool
is_consistent(idl_pstate_t *pstate, const void *lhs, const void *rhs)
{
  if (idl_mask(lhs) != idl_mask(rhs))
    return false;
  else if (idl_mask(lhs) & IDL_ENUM)
    return enum_is_consistent(pstate, lhs, rhs);
  else if (idl_mask(lhs) & IDL_CONST)
    return const_is_consistent(pstate, lhs, rhs);
  else if (idl_mask(lhs) & IDL_TYPEDEF)
    return typedef_is_consistent(pstate, lhs, rhs);
  assert(idl_is_annotation_member(lhs));
  return member_is_consistent(pstate, lhs, rhs);
}

idl_retcode_t
idl_finalize_annotation(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_annotation_t *node,
  idl_definition_t *definitions)
{
  bool discard = false;
  const idl_scope_t *scope;

  node->node.symbol.location.last = location->last;
  scope = pstate->scope;
  idl_exit_scope(pstate);

  if (pstate->parser.state == IDL_PARSE_EXISTING_ANNOTATION_BODY) {
    const idl_name_t *name;
    const idl_declaration_t *decl;
    idl_definition_t *d;
    ssize_t n;

    decl = idl_find(pstate, NULL, node->name, IDL_FIND_ANNOTATION);
    /* earlier declaration must exist given the current state */
    assert(decl);
    assert(decl->node && decl->node != (void *)node);
    assert(decl->scope && decl->scope == scope);
    n = (ssize_t)idl_degree(((const idl_annotation_t *)decl->node)->definitions);
    for (d = definitions; n >= 0 && d; d = idl_next(d), n--) {
      if (idl_is_typedef(d))
        name = idl_name(((idl_typedef_t *)d)->declarators);
      else
        name = idl_name(d);
      decl = idl_find(pstate, scope, name, 0u);
      if (!decl || !is_consistent(pstate, d, idl_parent(decl->node)))
        goto err_incompat;
    }
    if (n != 0)
      goto err_incompat;
    /* declarations are compatible, discard redefinition */
    discard = true;
  }

  node->definitions = definitions;
  for (idl_node_t *n = (idl_node_t *)definitions; n; n = n->next)
    n->parent = (idl_node_t *)node;
  if (discard)
    idl_unreference_node(node);
  pstate->parser.state = IDL_PARSE;
  return IDL_RETCODE_OK;
err_incompat:
  idl_error(pstate, idl_location(node),
    "Incompatible redefinition of '@%s'", idl_identifier(node));
  return IDL_RETCODE_SEMANTIC_ERROR;
}

static void delete_annotation(void *ptr)
{
  idl_annotation_t *node = ptr;
  idl_delete_node(node->definitions);
  idl_delete_name(node->name);
  idl_free(node);
}

static void *iterate_annotation(const void *ptr, const void *cur)
{
  const idl_annotation_t *root = ptr;
  const idl_node_t *node = cur;
  assert(root);
  if (node) {
    assert(idl_parent(node) == root);
    return node->next;
  }
  return root->definitions;
}

static const char *describe_annotation(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) == IDL_ANNOTATION);
  return "annotation";
}

idl_retcode_t
idl_create_annotation(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  void *nodep)
{
  idl_retcode_t ret;
  idl_annotation_t *node = NULL;
  idl_scope_t *scope = NULL;
  const idl_declaration_t *declaration;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_ANNOTATION;
  static const struct methods methods = {
    delete_annotation, iterate_annotation, describe_annotation };
  static const enum idl_declaration_kind kind = IDL_ANNOTATION_DECLARATION;

  assert(name->is_annotation);
  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    goto err_node;
  declaration = idl_find(pstate, NULL, name, IDL_FIND_ANNOTATION);
  if (declaration) {
    /* annotations should not cause more compile errors than strictly needed.
       therefore, in case of multiple definitions of the same annotation in
       one IDL specification, the compiler should accept them, provided that
       they are consistent */
    assert(declaration->kind == IDL_ANNOTATION_DECLARATION);
    node->name = name;
    idl_enter_scope(pstate, declaration->scope);
    *((idl_annotation_t **)nodep) = node;
    pstate->parser.state = IDL_PARSE_EXISTING_ANNOTATION_BODY;
    return IDL_RETCODE_OK;
  }
  if ((ret = idl_create_scope(pstate, IDL_ANNOTATION_SCOPE, name, node, &scope)))
    goto err_scope;
  if ((ret = idl_declare(pstate, kind, name, node, scope, NULL)))
    goto err_declare;
  node->name = name;
  idl_enter_scope(pstate, scope);
  *((idl_annotation_t **)nodep) = node;
  pstate->parser.state = IDL_PARSE_ANNOTATION_BODY;
  return IDL_RETCODE_OK;

err_declare:
  idl_delete_scope(scope);
err_scope:
  idl_free(node);
err_node:
  return ret;
}

static void delete_annotation_appl_param(void *ptr)
{
  idl_annotation_appl_param_t *node = ptr;
  delete_const_expr(node, node->const_expr);
  idl_unreference_node(node->member);
  idl_free(node);
}

static const char *describe_annotation_appl_param(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) == IDL_ANNOTATION_APPL_PARAM);
  return "annotation application parameter";
}

idl_retcode_t
idl_create_annotation_appl_param(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_annotation_member_t *member,
  idl_const_expr_t *const_expr,
  void *nodep)
{
  idl_retcode_t ret;
  idl_annotation_appl_param_t *node;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_ANNOTATION_APPL_PARAM;
  static const struct methods methods = {
    delete_annotation_appl_param, 0, describe_annotation_appl_param };

  if ((ret = create_node(pstate, size, mask, location, &methods, &node)))
    return ret;
  node->member = member;
  assert((idl_mask(const_expr) & IDL_EXPRESSION) ||
         (idl_mask(const_expr) & IDL_CONST) ||
         (idl_mask(const_expr) & IDL_ENUMERATOR) ||
         (idl_mask(const_expr) & IDL_BIT_VALUE));
  node->const_expr = const_expr;
  *((idl_annotation_appl_param_t **)nodep) = node;
  return ret;
}

bool idl_is_annotation_appl(const void *ptr)
{
#if !defined(NDEBUG)
  static const idl_mask_t mask = IDL_MODULE |
                                 IDL_ENUM |
                                 IDL_STRUCT | IDL_MEMBER |
                                 IDL_UNION | IDL_SWITCH_TYPE_SPEC |
                                 IDL_BITMASK;
#endif
  const idl_annotation_appl_t *node = ptr;

  if (!(idl_mask(node) & IDL_ANNOTATION_APPL))
    return false;
  /* an annotation application must have a parent */
  assert(!node->node.parent || (idl_mask(node->node.parent) & mask));
  /* an annotation application must have an annotation */
  assert(idl_mask(node->annotation) & IDL_ANNOTATION);
  return true;
}

static void delete_annotation_appl(void *ptr)
{
  idl_annotation_appl_t *node = ptr;
  idl_unreference_node((idl_annotation_t *)node->annotation);
  idl_delete_node(node->parameters);
  idl_free(node);
}

static void *iterate_annotation_appl(const void *ptr, const void *cur)
{
  const idl_annotation_appl_t *root = ptr;
  const idl_node_t *node = cur;
  assert(root);
  assert(!node || idl_parent(node) == root);
  if (node)
    return node->next;
  return root->parameters;
}

static const char *describe_annotation_appl(const void *ptr)
{
  (void)ptr;
  assert(idl_mask(ptr) == IDL_ANNOTATION_APPL);
  return "annotation application";
}

idl_retcode_t
idl_finalize_annotation_appl(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_annotation_appl_t *node,
  idl_annotation_appl_param_t *parameters)
{
  assert(node);
  assert(node->annotation);
  node->node.symbol.location.last = location->last;

  /* constant expressions cannot be evaluated until annotations are applied
     as values for members of type any must match with the element under
     annotation */
  if (idl_mask(parameters) & (IDL_EXPRESSION|IDL_ENUMERATOR|IDL_BIT_VALUE|IDL_CONST)) {
    idl_definition_t *definition = node->annotation->definitions;
    idl_annotation_member_t *member = NULL;
    while (definition && !member) {
      if (idl_is_annotation_member(definition))
        member = definition;
      definition = ((idl_node_t *)definition)->next;
    }
    idl_annotation_appl_param_t *parameter = NULL;
    static const size_t size = sizeof(*parameter);
    static const idl_mask_t mask = IDL_ANNOTATION_APPL_PARAM;
    static const struct methods methods = {
      delete_annotation_appl_param, 0, describe_annotation_appl_param };
    if (!member) {
      idl_error(pstate, idl_location(parameters),
        "@%s does not take any parameters", idl_identifier(node->annotation));
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
    if (create_node(pstate, size, mask, location, &methods, &parameter)) {
      return IDL_RETCODE_NO_MEMORY;
    }
    node->parameters = parameter;
    ((idl_node_t *)parameter)->parent = (idl_node_t *)node;
    parameter->member = idl_reference_node(member);
    parameter->const_expr = parameters;
    if (!idl_scope(parameters))
      ((idl_node_t *)parameters)->parent = (idl_node_t *)parameter;
  } else if (idl_mask(parameters) & IDL_ANNOTATION_APPL_PARAM) {
    node->parameters = parameters;
    for (idl_annotation_appl_param_t *ap = parameters; ap; ap = idl_next(ap))
      ((idl_node_t *)ap)->parent = (idl_node_t *)node;
  } else if (parameters) {
    idl_error(pstate, idl_location(parameters),
      "internal error in parsing annotation @%s", idl_identifier(node->annotation));
    return IDL_RETCODE_BAD_PARAMETER;
  }

  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_create_annotation_appl(
  idl_pstate_t *state,
  const idl_location_t *location,
  const idl_annotation_t *annotation,
  void *nodep)
{
  idl_retcode_t ret;
  idl_annotation_appl_t *node = NULL;
  static const size_t size = sizeof(*node);
  static const idl_mask_t mask = IDL_ANNOTATION_APPL;
  static const struct methods methods = { delete_annotation_appl,
                                          iterate_annotation_appl,
                                          describe_annotation_appl };

  if ((ret = create_node(state, size, mask, location, &methods, &node)))
    return ret;

  node->annotation = annotation;
  *((idl_annotation_appl_t **)nodep) = node;
  return IDL_RETCODE_OK;
}

idl_type_spec_t *idl_type_spec(const void *node)
{
  idl_mask_t mask;

  mask = idl_mask(node);
  if (mask & IDL_DECLARATOR)
    node = idl_parent(node);
  mask = idl_mask(node);
  if (mask & IDL_FORWARD)
    return ((const idl_forward_t *)node)->type_spec;
  if (mask & IDL_TYPEDEF)
    return ((const idl_typedef_t *)node)->type_spec;
  if (mask & IDL_MEMBER)
    return ((const idl_member_t *)node)->type_spec;
  if (mask & IDL_CASE)
    return ((const idl_case_t *)node)->type_spec;
  if (mask & IDL_SEQUENCE)
    return ((const idl_sequence_t *)node)->type_spec;
  if (mask & IDL_SWITCH_TYPE_SPEC)
    return ((const idl_switch_type_spec_t *)node)->type_spec;
  if (mask & IDL_CONST)
    return ((const idl_const_t *)node)->type_spec;
  if (mask & IDL_INHERIT_SPEC)
    return ((const idl_inherit_spec_t *)node)->base;
  return NULL;
}

uint32_t idl_array_size(const void *node)
{
  uint32_t dims = 1;
  const idl_literal_t *literal;
  if (!(idl_mask(node) & IDL_DECLARATOR))
    return 0u;
  literal = ((const idl_declarator_t *)node)->const_expr;
  if (!literal)
    return 0u;
  for (; literal; literal = idl_next(literal))
    dims *= literal->value.uint32;
  return dims;
}

bool idl_is_topic(const void *node, bool keylist)
{
  if (keylist) {
    if (!idl_is_struct(node))
      return false;
    if (((const idl_struct_t *)node)->keylist)
      return true;
  } else {
    if (idl_is_struct(node))
      return !((const idl_struct_t *)node)->nested.value;
    else if (idl_is_union(node))
      return !((const idl_union_t *)node)->nested.value;
    return false;
  }
  return false;
}

static bool no_specific_key(const void *node)
{
  /* @key(FALSE) is equivalent to missing @key(?) */
  if (idl_mask(node) & IDL_STRUCT) {
    const idl_member_t *member = ((const idl_struct_t *)node)->members;
    for (; member; member = idl_next(member)) {
      if (member->key.value)
        return false;
    }
  } else if (idl_mask(node) & IDL_UNION) {
    if (((const idl_union_t*)node)->switch_type_spec->key.value)
      return false;
  }

  return true;
}

bool idl_is_keyless(const void *node, bool keylist)
{
  if (!keylist)
    return no_specific_key(node);

  if (idl_mask(node) & IDL_STRUCT)
    return !(((const idl_struct_t *)node)->keylist &&
            ((const idl_struct_t *)node)->keylist->keys);
  return true;
}

idl_keytype_t idl_is_topic_key(const void *node, bool keylist, const idl_path_t *path, uint32_t *order)
{
  bool all_keys = false;
  idl_keytype_t key = IDL_KEYTYPE_NONE;
  uint32_t id = 0;

  if (!idl_is_topic(node, keylist))
    return IDL_KEYTYPE_NONE;
  if (!path->length || node != path->nodes[0])
    return IDL_KEYTYPE_NONE;

  /* constructed types, sequences, aliases and declarators carry the key */
  static const idl_mask_t mask =
    IDL_CONSTR_TYPE | IDL_MEMBER | IDL_SWITCH_TYPE_SPEC | IDL_CASE |
    IDL_SEQUENCE | IDL_DECLARATOR;

  /* start from level 1, the members of the topic node */
  for (size_t i = 1; (key != IDL_KEYTYPE_NONE || i == 1) && i < path->length; i++) {
    assert(path->nodes[i]);

    /* struct members can be explicitly annotated */
    if (idl_is_member(path->nodes[i])) {
      const idl_member_t *member = (const idl_member_t *)path->nodes[i];
      /* path cannot be valid if not preceeded by struct */
      if (i > 1 && !idl_is_struct(path->nodes[i - 1]))
        return IDL_KEYTYPE_NONE;

      if (member->key.value)
        key = IDL_KEYTYPE_EXPLICIT;
      /* possibly implicit @key, but only if no other members are explicitly
        annotated, an intermediate aggregate type has no explicitly annotated
        fields and node is not on the first level */
      else if (all_keys || no_specific_key(idl_parent(member)))
      {
        all_keys = (i > 1);
        key = (i > 1) ? IDL_KEYTYPE_IMPLICIT : IDL_KEYTYPE_NONE;
      }
      else
      {
        key = IDL_KEYTYPE_NONE;
      }

      /* if key member found, get the id from the declarator node which
         should be next in the path (can be overwritten by the id of a key
         field further down in the path in a next iteration) */
      if (key != IDL_KEYTYPE_NONE) {
        if (!idl_is_declarator(path->nodes[i + 1]))
          return IDL_KEYTYPE_NONE;
        const idl_declarator_t *declarator = (const idl_declarator_t *)path->nodes[i + 1];
        id = declarator->id.value;
      }

    /* union cases cannot be explicitly annotated */
    } else if (idl_is_case(path->nodes[i])) {
      const idl_case_t *_case = (const idl_case_t *)path->nodes[i];
      /* path cannot be valid if not preceeded by union */
      if (i > 0 && !idl_is_union(path->nodes[i - 1]))
        return IDL_KEYTYPE_NONE;

      /* union cases cannot be annotated, but can be part of the key if an
        intermediate aggregate type has no explicitly annotated fields or if
        the switch type specifier is not annotated */
      if (all_keys || no_specific_key(idl_parent(_case)))
      {
        all_keys = (i > 1);
        key = (i > 1) ? IDL_KEYTYPE_IMPLICIT : IDL_KEYTYPE_NONE;
      }

    /* switch type specifiers can be explicitly annotated */
    } else if (idl_is_switch_type_spec(path->nodes[i])) {
      const idl_switch_type_spec_t *switch_type_spec = (const idl_switch_type_spec_t *)path->nodes[i];
      /* path cannot be valid if not preceeded by union */
      if (i != 0 && !idl_is_union(path->nodes[i - 1]))
        return IDL_KEYTYPE_NONE;

      /* possibly (implicit) @key, but only if last in path and not first */
      if (switch_type_spec->key.value)
        key = (i == path->length - 1) ? IDL_KEYTYPE_EXPLICIT : IDL_KEYTYPE_NONE;
      else
        key = ((i == path->length - 1) ? (i > 1) : 0) ? IDL_KEYTYPE_IMPLICIT : IDL_KEYTYPE_NONE;
    } else if (!(idl_mask(node) & mask)) {
      key = IDL_KEYTYPE_NONE;
    }
  }

  if (key != IDL_KEYTYPE_NONE && (idl_mask(path->nodes[path->length - 1]) & mask)) {
    *order = id;
    return key;
  }
  *order = 0;
  return IDL_KEYTYPE_NONE;
}

bool idl_is_extensible(const idl_node_t *node, idl_extensibility_t extensibility)
{
  if (idl_is_struct(node)) {
    const idl_struct_t *_struct = (const idl_struct_t *)node;
    return _struct->extensibility.value == extensibility;
  } else if (idl_is_union(node)) {
    const idl_union_t *_union = (const idl_union_t *)node;
    return _union->extensibility.value == extensibility;
  } else if (idl_is_enum(node)) {
    const idl_enum_t *_enum = (const idl_enum_t *)node;
    return _enum->extensibility.value == extensibility;
  } else if (idl_is_bitmask(node)) {
    const idl_bitmask_t *_bitmask = (const idl_bitmask_t *)node;
    return _bitmask->extensibility.value == extensibility;
  }
  return false;
}

bool idl_has_unset_extensibility_r(idl_node_t *node)
{
  idl_retcode_t ret = IDL_RETCODE_OK;
  assert(node);
  for (; node; node = idl_next(node)) {
    if (idl_mask(node) == IDL_MODULE) {
      idl_module_t *module = (idl_module_t *)node;
      return idl_has_unset_extensibility_r(module->definitions);
    } else if (idl_mask(node) == IDL_STRUCT) {
      idl_struct_t *_struct = (idl_struct_t *)node;
      if (_struct->extensibility.annotation)
        continue;
      if (!_struct->inherit_spec)
        return true;
      idl_struct_t *base = _struct->inherit_spec->base;
      while (base && !base->extensibility.annotation)
        base = base->inherit_spec ? base->inherit_spec->base : NULL;
      if (!base || !base->extensibility.annotation)
        return true;
    } else if (idl_mask(node) == IDL_UNION) {
      idl_union_t *_union = (idl_union_t *)node;
      if (!_union->extensibility.annotation)
        return true;
    }
  }
  return ret;
}


bool idl_is_external(const idl_node_t *node)
{
  return (idl_is_member(node) && ((idl_member_t *)node)->external.value)
    || (idl_is_case(node) && ((idl_case_t *)node)->external.value);
}

bool idl_is_optional(const idl_node_t *node)
{
  return (idl_is_member(node) && ((idl_member_t *)node)->optional.value);
}

bool idl_is_must_understand(const idl_node_t *node)
{
  return (idl_is_member(node) && ((idl_member_t *)node)->must_understand.value);
}
