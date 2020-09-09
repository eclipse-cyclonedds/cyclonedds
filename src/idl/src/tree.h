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
#ifndef TREE_H
#define TREE_H

#include "idl/processor.h"

idl_retcode_t
idl_annotate(
  idl_processor_t *proc,
  void *node,
  idl_annotation_appl_t *annotations);

idl_retcode_t
idl_finalize_module(
  idl_processor_t *proc,
  idl_module_t *node,
  idl_location_t *location,
  void *definitions);

idl_retcode_t
idl_create_module(
  idl_processor_t *proc,
  idl_module_t **nodeptr,
  idl_location_t *location,
  char *identifier);

idl_retcode_t
idl_create_const(
  idl_processor_t *proc,
  idl_const_t **nodeptr,
  idl_location_t *location,
  idl_type_spec_t *type_spec,
  char *identifier,
  idl_const_expr_t *const_expr);

idl_retcode_t
idl_create_sequence(
  idl_processor_t *proc,
  idl_sequence_t **nodeptr,
  idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_constval_t *constval);

idl_retcode_t
idl_create_string(
  idl_processor_t *proc,
  idl_string_t **nodeptr,
  idl_location_t *location,
  idl_constval_t *constval);

idl_retcode_t
idl_finalize_struct(
  idl_processor_t *proc,
  idl_struct_t *node,
  idl_location_t *location,
  idl_member_t *members);

idl_retcode_t
idl_create_struct(
  idl_processor_t *proc,
  idl_struct_t **nodeptr,
  idl_location_t *location,
  char *identifier,
  idl_struct_t *base_type);

idl_retcode_t
idl_create_member(
  idl_processor_t *proc,
  idl_member_t **nodeptr,
  idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_declarator_t *declarators);

idl_retcode_t
idl_create_forward(
  idl_processor_t *proc,
  idl_forward_t **nodeptr,
  idl_location_t *location,
  idl_mask_t mask,
  char *identifier);

idl_retcode_t
idl_create_case_label(
  idl_processor_t *proc,
  idl_case_label_t **nodeptr,
  idl_location_t *location,
  idl_const_expr_t *const_expr);

idl_retcode_t
idl_finalize_case(
  idl_processor_t *proc,
  idl_case_t *node,
  idl_location_t *location,
  idl_case_label_t *case_labels);

idl_retcode_t
idl_create_case(
  idl_processor_t *proc,
  idl_case_t **nodeptr,
  idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_declarator_t *declarator);

idl_retcode_t
idl_create_union(
  idl_processor_t *proc,
  idl_union_t **nodeptr,
  idl_location_t *location,
  char *identifier,
  idl_switch_type_spec_t *switch_type_spec,
  idl_case_t *cases);

idl_retcode_t
idl_create_enumerator(
  idl_processor_t *proc,
  idl_enumerator_t **nodeptr,
  idl_location_t *location,
  char *identifier);

idl_retcode_t
idl_create_enum(
  idl_processor_t *proc,
  idl_enum_t **nodeptr,
  idl_location_t *location,
  char *identifier,
  idl_enumerator_t *enumerators);

idl_retcode_t
idl_create_typedef(
  idl_processor_t *proc,
  idl_typedef_t **nodeptr,
  idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_declarator_t *declarators);

idl_retcode_t
idl_create_declarator(
  idl_processor_t *proc,
  idl_declarator_t **nodeptr,
  idl_location_t *location,
  char *identifier,
  idl_const_expr_t *const_expr);

idl_retcode_t
idl_create_annotation_appl(
  idl_processor_t *proc,
  idl_annotation_appl_t **nodeptr,
  idl_location_t *location,
  char *identifier,
  void *parameters);

idl_retcode_t
idl_create_annotation_appl_param(
  idl_processor_t *proc,
  idl_annotation_appl_param_t **nodeptr,
  idl_location_t *location,
  char *identifier,
  idl_const_expr_t *parameters);

idl_data_type_t *idl_create_data_type(void);

idl_key_t *idl_create_key(void);

idl_keylist_t *idl_create_keylist(void);

idl_retcode_t
idl_create_literal(
  idl_processor_t *proc,
  idl_literal_t **nodeptr,
  idl_location_t *location,
  idl_mask_t mask);

idl_retcode_t
idl_create_binary_expr(
  idl_processor_t *proc,
  idl_binary_expr_t **nodeptr,
  idl_location_t *location,
  idl_mask_t mask,
  idl_primary_expr_t *left,
  idl_primary_expr_t *right);

idl_retcode_t
idl_create_unary_expr(
  idl_processor_t *proc,
  idl_unary_expr_t **nodeptr,
  idl_location_t *location,
  idl_mask_t mask,
  idl_primary_expr_t *right);

idl_retcode_t
idl_create_base_type(
  idl_processor_t *proc,
  idl_base_type_t **nodeptr,
  idl_location_t *location,
  idl_mask_t mask);

idl_retcode_t
idl_create_constval(
  idl_processor_t *proc,
  idl_constval_t **nodeptr,
  idl_location_t *location,
  idl_mask_t mask);

/** @private */
void idl_delete_node(void *node);

#endif /* TREE_H */
