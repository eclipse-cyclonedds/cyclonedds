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

idl_retcode_t idl_annotate(
  idl_processor_t *proc,
  void *node,
  idl_annotation_appl_t *annotations);

/** @private */
idl_module_t *idl_create_module(void);
/** @private */
idl_const_t *idl_create_const(void);
/** @private */
idl_sequence_t *idl_create_sequence(void);
/** @private */
idl_string_t *idl_create_string(void);
/** @private */
idl_retcode_t
idl_create_struct(
  idl_processor_t *proc,
  idl_struct_t **nodeptr,
  idl_location_t *location,
  char *identifier,
  idl_struct_t *base_type);

/** @private */
idl_retcode_t
idl_create_member(
  idl_processor_t *proc,
  idl_member_t **nodeptr,
  idl_location_t *location,
  idl_type_spec_t *type_spec,
  idl_declarator_t *declarators);

/** @private */
idl_forward_t *idl_create_forward(idl_mask_t mask);
/** @private */
idl_case_label_t *idl_create_case_label(void);
/** @private */
idl_case_t *idl_create_case(void);
/** @private */
idl_union_t *idl_create_union(void);
/** @private */
idl_enumerator_t *idl_create_enumerator(void);
/** @private */
idl_enum_t *idl_create_enum(void);
/** @private */
idl_typedef_t *idl_create_typedef(void);
/** @private */
idl_declarator_t *idl_create_declarator(void);
/** @private */
idl_retcode_t
idl_create_annotation_appl(
  idl_processor_t *proc,
  idl_annotation_appl_t **nodeptr,
  idl_location_t *location,
  char *identifier,
  void *parameters);

/** @private */
idl_retcode_t
idl_create_annotation_appl_param(
  idl_processor_t *proc,
  idl_annotation_appl_param_t **nodeptr,
  idl_location_t *location,
  char *identifier,
  idl_const_expr_t *parameters);

/** @private */
idl_data_type_t *idl_create_data_type(void);
/** @private */
idl_key_t *idl_create_key(void);
/** @private */
idl_keylist_t *idl_create_keylist(void);
/** @private */
//idl_literal_t *idl_create_literal(idl_mask_t mask);
idl_retcode_t
idl_create_literal(
  idl_processor_t *proc,
  idl_literal_t **nodeptr,
  idl_location_t *location,
  idl_mask_t mask);

/** @private */
idl_binary_expr_t *idl_create_binary_expr(idl_mask_t mask);
/** @private */
idl_unary_expr_t *idl_create_unary_expr(idl_mask_t mask);
/** @private */
idl_base_type_t *idl_create_base_type(idl_mask_t mask);
/** @private */
idl_retcode_t
idl_create_constval(
  idl_processor_t *proc,
  idl_constval_t **nodeptr,
  idl_location_t *location,
  idl_mask_t mask);

/** @private */
void idl_delete_node(void *node);

#endif /* TREE_H */
