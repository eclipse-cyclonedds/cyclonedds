// Copyright(c) 2021 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef TREE_H
#define TREE_H

#include "idl/processor.h"


typedef void idl_primary_expr_t;

typedef struct idl_binary_expr idl_binary_expr_t;
struct idl_binary_expr {
  idl_node_t node;
  idl_const_expr_t *left;
  idl_const_expr_t *right;
};

typedef struct idl_unary_expr idl_unary_expr_t;
struct idl_unary_expr {
  idl_node_t node;
  idl_const_expr_t *right;
};

void *idl_push_node(void *list, void *node);
void *idl_reference_node(void *node);
void *idl_unreference_node(void *node);
void *idl_delete_node(void *node);

idl_retcode_t
idl_create_literal(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_mask_t mask,
  void *nodep);

idl_retcode_t
idl_create_binary_expr(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_mask_t mask,
  idl_primary_expr_t *left,
  idl_primary_expr_t *right,
  void *nodep);

idl_retcode_t
idl_create_unary_expr(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_mask_t mask,
  idl_primary_expr_t *right,
  void *nodep);

idl_retcode_t
idl_propagate_autoid(
  idl_pstate_t *pstate,
  void *list,
  idl_autoid_t autoid);

idl_retcode_t
idl_finalize_module(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_module_t *node,
  void *definitions);

idl_retcode_t
idl_create_module(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  void *nodep);

idl_retcode_t
idl_create_const(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  void *type_spec,
  idl_name_t *name,
  void *const_expr,
  void *nodep);

idl_retcode_t
idl_create_sequence(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  void *type_spec,
  idl_literal_t *literal,
  void *nodep);

idl_retcode_t
idl_create_string(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_literal_t *literal,
  void *nodep);

idl_retcode_t
idl_finalize_struct(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_struct_t *node,
  idl_member_t *members);

idl_retcode_t
idl_create_struct(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  idl_inherit_spec_t *inherit_spec,
  void *nodep);

idl_retcode_t
idl_create_forward(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  idl_type_t type,
  void *nodep);

idl_retcode_t
idl_create_key(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  void *nodep);

idl_retcode_t
idl_create_keylist(
   idl_pstate_t *pstate,
   const idl_location_t *location,
   void *nodep);

idl_retcode_t
idl_create_member(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  void *type_spec,
  idl_declarator_t *declarators,
  void *nodep);

idl_retcode_t
idl_create_switch_type_spec(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_type_spec_t *type_spec,
  void *nodep);

idl_retcode_t
idl_create_case_label(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  void *const_expr,
  void *nodep);

idl_retcode_t
idl_finalize_case(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_case_t *node,
  idl_case_label_t *case_labels);

idl_retcode_t
idl_create_case(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_declarator_t *declarator,
  void *nodep);

idl_retcode_t
idl_finalize_union(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_union_t *node,
  idl_case_t *cases);

idl_retcode_t
idl_create_union(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  idl_switch_type_spec_t *switch_type_spec,
  void *nodep);

idl_retcode_t
idl_create_enumerator(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  void *nodep);

idl_retcode_t
idl_create_enum(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  idl_enumerator_t *enumerators,
  void *nodep);

idl_retcode_t
idl_create_bit_value(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  void *nodep);

idl_retcode_t
idl_create_bitmask(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  idl_bit_value_t *bit_values,
  void *nodep);

idl_retcode_t
idl_create_typedef(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  void *type_spec,
  idl_declarator_t *declarators,
  void *nodep);

idl_retcode_t
idl_create_declarator(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  idl_const_expr_t *const_expr,
  void *nodep);

idl_retcode_t
idl_create_annotation_member(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_declarator_t *declarator,
  idl_const_expr_t *default_value,
  void *nodep);

idl_retcode_t
idl_finalize_annotation(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_annotation_t *node,
  idl_definition_t *definitions);

idl_retcode_t
idl_create_annotation(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  void *nodep);

idl_retcode_t
idl_finalize_annotation_appl(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_annotation_appl_t *node,
  idl_annotation_appl_param_t *parameters);

idl_retcode_t
idl_create_annotation_appl(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  const idl_annotation_t *annotation,
  void *nodep);

idl_retcode_t
idl_create_annotation_appl_param(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_annotation_member_t *member,
  idl_const_expr_t *const_expr,
  void *nodep);

idl_retcode_t
idl_create_base_type(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_mask_t mask,
  void *nodep);

idl_retcode_t
idl_create_inherit_spec(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  void *base,
  void *nodep);

idl_retcode_t
idl_set_xcdr2_required(
  void *node);

#endif /* TREE_H */
