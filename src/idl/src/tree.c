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

#include "tree.h"
#include "idl/processor.h"

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

const char *idl_identifier(const void *node)
{
  if (!idl_is_declaration(node))
    return NULL;
  if (idl_is_module(node))
    return ((const idl_module_t *)node)->identifier;
  if (idl_is_struct(node))
    return ((const idl_struct_t *)node)->identifier;
  if (idl_is_union(node))
    return ((const idl_union_t *)node)->identifier;
  if (idl_is_enum(node))
    return ((const idl_enum_t *)node)->identifier;
  if (idl_is_enumerator(node))
    return ((const idl_enumerator_t *)node)->identifier;
  if (idl_is_declarator(node))
    return ((const idl_declarator_t *)node)->identifier;
  if (idl_is_forward(node))
    return ((const idl_forward_t *)node)->identifier;
  return NULL;
}

const char *idl_label(const void *node)
{
  assert(node);
  (void)node;
#if 0
  switch (((idl_node_t *)node)->mask) {
    case IDL_INT8:
      return "int8";
    case IDL_UINT8:
      return "uint8";
    case IDL_INT16:
      return "int16";
    case IDL_UINT16:
      return "uint16";
    case IDL_INT32:
      return "int32";
    case IDL_UINT32:
      return "uint32";
    case IDL_INT64:
      return "int64";
    case IDL_UINT64:
      return "uint64";
    default:
      break;
  }
#endif

  return "unknown";
}

const idl_location_t *idl_location(const void *node)
{
  return &((const idl_node_t *)node)->location;
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
  if ((n->mask & (IDL_DECL | IDL_PRAGMA)))
    return n->next;
  /* as do expressions (or constants) if specifying array sizes */
  if ((n->mask & (IDL_EXPR | IDL_CONST)) && idl_is_declarator(n->parent))
    return n->next;
  assert(!n->previous);
  assert(!n->next);
  return n->next;
}

static void *
make_node(
  size_t size,
  idl_mask_t mask,
  idl_print_t printer,
  idl_delete_t destructor)
{
  idl_node_t *node;
  assert(size >= sizeof(*node));
  if ((node = calloc(1, size))) {
    node->mask = mask;
    node->printer = printer;
    node->destructor = destructor;
    node->references = 1;
  }
  return node;
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
  assert(n->identifier);
  /* modules must have at least on definition */
  assert(idl_is_masked(n->definitions, IDL_DECL));
  return true;
}

static void delete_module(void *node)
{
  idl_module_t *n = (idl_module_t *)node;
  delete_node(n->definitions);
  if (n->identifier)
    free(n->identifier);
  free(n);
}

idl_module_t *idl_create_module(void)
{
  return make_node(
    sizeof(idl_module_t), IDL_DECL|IDL_MODULE, 0, &delete_module);
}

static void delete_const(void *node)
{
  idl_const_t *n = (idl_const_t *)node;
  delete_node(n->const_expr);
  if (n->identifier)
    free(n->identifier);
  free(n);
}

idl_const_t *idl_create_const(void)
{
  return make_node(
    sizeof(idl_const_t), IDL_DECL|IDL_CONST, 0, &delete_const);
}

static void delete_sequence(void *node)
{
  idl_sequence_t *n = (idl_sequence_t *)node;
  delete_node(n->type_spec);
  delete_node(n->const_expr);
  free(n);
}

idl_sequence_t *idl_create_sequence(void)
{
  return make_node(
    sizeof(idl_sequence_t), IDL_TYPE|IDL_SEQUENCE, 0, &delete_sequence);
}

static void delete_string(void *node)
{
  idl_string_t *n = (idl_string_t *)node;
  delete_node(n->const_expr);
  free(n);
}

idl_string_t *idl_create_string(void)
{
  return make_node(
    sizeof(idl_string_t), IDL_TYPE|IDL_STRING, 0, &delete_string);
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
  assert(n->identifier);
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
  delete_node(n->keylist);
  if (n->identifier)
    free(n->identifier);
  free(n);
}

idl_retcode_t
idl_create_struct(
  idl_processor_t *proc,
  idl_struct_t **nodeptr,
  idl_location_t *location,
  char *identifier,
  idl_struct_t *base_type)
{
  idl_struct_t *node;
  static const idl_mask_t mask = IDL_DECL|IDL_TYPE|IDL_STRUCT;

  (void)proc;

  if (!(node = make_node(sizeof(*node), mask, 0, &delete_struct)))
    return IDL_RETCODE_NO_MEMORY;
  node->node.location = *location;
  node->identifier = identifier;
  node->base_type = base_type;
  *nodeptr = node;
  return IDL_RETCODE_OK;
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
  idl_member_t *node;
  static const idl_mask_t mask = IDL_DECL|IDL_MEMBER;

  (void)proc;
  if (!(node = make_node(sizeof(*node), mask, 0, &delete_member)))
    return IDL_RETCODE_NO_MEMORY;
  node->node.location = *location;

  assert(type_spec && (type_spec->mask & IDL_TYPE));
  if (type_spec->mask & IDL_DECL) {
    /* struct types can be embedded in IDL in 3.5 */
    if (!type_spec->parent) {
      assert((type_spec->mask & (IDL_STRUCT|IDL_FORWARD)) == IDL_STRUCT ||
             (type_spec->mask & (IDL_ENUM)) == IDL_ENUM);
      //assert(proc->flags & IDL_FLAG_EXTENDED_DATA_TYPES);
      // FIXME: >> this is not correct!
      type_spec->parent = (idl_node_t *)node;
    }
  } else {
    type_spec->parent = (idl_node_t *)node;
  }

  assert(declarators);
  for (idl_node_t *n = (idl_node_t *)declarators; n; n = n->next) {
    assert(!n->parent);
    assert(n->mask & IDL_DECL);
    n->parent = (idl_node_t *)node;
  }

  node->type_spec = type_spec;
  node->declarators = (idl_declarator_t *)declarators;

  *nodeptr = node;
  return IDL_RETCODE_OK;
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
  if (n->identifier)
    free(n->identifier);
  free(n);
}

idl_forward_t *idl_create_forward(idl_mask_t mask)
{
  assert((mask & IDL_STRUCT) == IDL_STRUCT ||
         (mask & IDL_UNION) == IDL_UNION);
  return make_node(
    sizeof(idl_forward_t), IDL_DECL|IDL_TYPE|IDL_FORWARD|mask, 0, &delete_forward);
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

idl_case_label_t *idl_create_case_label(void)
{
  return make_node(
    sizeof(idl_case_label_t), IDL_DECL|IDL_CASE_LABEL, 0, &delete_case_label);
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

idl_case_t *idl_create_case(void)
{
  return make_node(
    sizeof(idl_case_t), IDL_DECL|IDL_CASE, 0, &delete_case);
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
  assert(n->identifier);
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
  if (n->identifier)
    free(n->identifier);
  free(n);
}

idl_union_t *idl_create_union(void)
{
  return make_node(
    sizeof(idl_union_t), IDL_DECL|IDL_TYPE|IDL_UNION, 0, &delete_union);
}

bool idl_is_enumerator(const void *node)
{
  const idl_enumerator_t *n = (const idl_enumerator_t *)node;
  return idl_is_masked(n, IDL_DECL|IDL_ENUMERATOR);
}

static void delete_enumerator(void *ptr)
{
  idl_enumerator_t *node = (idl_enumerator_t *)ptr;
  if (node->identifier)
    free(node->identifier);
  free(node);
}

idl_enumerator_t *idl_create_enumerator(void)
{
  return make_node(
    sizeof(idl_enumerator_t), IDL_DECL|IDL_ENUMERATOR, 0, &delete_enumerator);
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
  if (n->identifier)
    free(n->identifier);
  free(n);
}

idl_enum_t *idl_create_enum(void)
{
  return make_node(
    sizeof(idl_enum_t), IDL_DECL|IDL_TYPE|IDL_ENUM, 0, &delete_enum);
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

idl_typedef_t *idl_create_typedef(void)
{
  return make_node(
    sizeof(idl_typedef_t), IDL_DECL|IDL_TYPE|IDL_TYPEDEF, 0, &delete_typedef);
}

bool idl_is_declarator(const void *node)
{
  const idl_declarator_t *n = (const idl_declarator_t *)node;
  if (!idl_is_masked(n, IDL_DECLARATOR))
    return false;
  assert(idl_is_masked(n, IDL_DECL|IDL_DECLARATOR));
  /* declarators must have an identifier */
  assert(n->identifier);
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
  if (n->identifier)
    free(n->identifier);
  free(n);
}

idl_declarator_t *idl_create_declarator(void)
{
  return make_node(
    sizeof(idl_declarator_t), IDL_DECL|IDL_DECLARATOR, 0, &delete_declarator);
}

static void delete_annotation_appl_param(void *node)
{
  idl_annotation_appl_param_t *n = (idl_annotation_appl_param_t *)node;
  delete_node(n->const_expr);
  if (n->identifier)
    free(n->identifier);
  free(n);
}

idl_retcode_t
idl_create_annotation_appl_param(
  idl_processor_t *proc,
  idl_annotation_appl_param_t **nodeptr,
  idl_location_t *location,
  char *identifier,
  idl_const_expr_t *const_expr)
{
  idl_annotation_appl_param_t *node;
  static const idl_mask_t mask = IDL_DECL|IDL_ANNOTATION_APPL_PARAM;

  (void)proc;
  if (!(node = make_node(
    sizeof(idl_annotation_appl_param_t), mask, 0, &delete_annotation_appl_param)))
    return IDL_RETCODE_NO_MEMORY;
  node->identifier = identifier;
  node->const_expr = const_expr;
  node->node.location = *location;
  *nodeptr = node;
  return IDL_RETCODE_OK;
}

static void delete_annotation_appl(void *node)
{
  idl_annotation_appl_t *n = (idl_annotation_appl_t *)node;
  delete_node(n->parameters);
  if (n->scoped_name)
    free(n->scoped_name);
  free(n);
}

idl_retcode_t
idl_create_annotation_appl(
  idl_processor_t *proc,
  idl_annotation_appl_t **nodeptr,
  idl_location_t *location,
  char *identifier,
  void *parameters)
{
  idl_annotation_appl_t *node;
  static const idl_mask_t mask = IDL_ANNOTATION_APPL;

  (void)proc;
  if (!(node = make_node(sizeof(*node), mask, 0, &delete_annotation_appl)))
    return IDL_RETCODE_NO_MEMORY;
  node->scoped_name = identifier;
  node->parameters = parameters;
  node->node.location = *location;
  *nodeptr = node;
  return IDL_RETCODE_OK;
}

static void delete_key(void *node)
{
  idl_key_t *n = (idl_key_t *)node;
  if (n->identifier)
    free(n->identifier);
  free(n);
}

idl_key_t *idl_create_key(void)
{
  return make_node(
    sizeof(idl_key_t), IDL_PRAGMA|IDL_KEY, 0, &delete_key);
}

static void delete_data_type(void *node)
{
  idl_data_type_t *n = (idl_data_type_t *)node;
  if (n->identifier)
    free(n->identifier);
  free(n);
}

idl_data_type_t *idl_create_data_type(void)
{
  return make_node(
    sizeof(idl_data_type_t), IDL_DATA_TYPE, 0, &delete_data_type);
}

static void delete_keylist(void *node)
{
  idl_keylist_t *n = (idl_keylist_t *)node;
  delete_node(n->keys);
  delete_node(n->data_type);
  free(n);
}

idl_keylist_t *idl_create_keylist(void)
{
  return make_node(
    sizeof(idl_keylist_t), IDL_KEYLIST, 0, &delete_keylist);
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
  idl_literal_t *node;
  assert((mask & IDL_ULLONG) == IDL_ULLONG ||
         (mask & IDL_LDOUBLE) == IDL_LDOUBLE ||
         (mask & IDL_CHAR) == IDL_CHAR ||
         (mask & IDL_BOOL) == IDL_BOOL ||
         (mask & IDL_STRING) == IDL_STRING);
  (void)proc;
  if (!(node = make_node(
    sizeof(idl_literal_t), IDL_EXPR|IDL_LITERAL|mask, 0, &delete_literal)))
    return IDL_RETCODE_NO_MEMORY;
  node->node.location = *location;
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

idl_binary_expr_t *idl_create_binary_expr(idl_mask_t mask)
{
  return make_node(
    sizeof(idl_binary_expr_t), IDL_EXPR|IDL_BINARY_EXPR|mask, 0, &delete_binary_expr);
}

static void delete_unary_expr(void *node)
{
  idl_unary_expr_t *n = (idl_unary_expr_t *)node;
  delete_node(n->right);
  free(n);
}

idl_unary_expr_t *idl_create_unary_expr(idl_mask_t mask)
{
  return make_node(
    sizeof(idl_unary_expr_t), IDL_EXPR|IDL_UNARY_EXPR|mask, 0, &delete_unary_expr);
}

static void delete_base_type(void *node)
{
  free(node);
}

idl_base_type_t *idl_create_base_type(idl_mask_t mask)
{
  return make_node(sizeof(idl_base_type_t), IDL_TYPE|mask, 0, &delete_base_type);
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
  idl_location_t *location,
  idl_mask_t mask)
{
  idl_constval_t *n;
  (void)proc;
  if (!(n = make_node(sizeof(*n), IDL_CONST|mask, 0, &delete_constval)))
    return IDL_RETCODE_NO_MEMORY;
  n->node.location = *location;
  *nodeptr = n;
  return IDL_RETCODE_OK;
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
