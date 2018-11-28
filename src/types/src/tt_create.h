/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDS_TS_CREATE_H
#define DDS_TS_CREATE_H

#include <stdbool.h>

/* Some types only used during parsing */

typedef struct dds_ts_scoped_name dds_ts_scoped_name_t;
typedef struct dds_ts_context dds_ts_context_t;

dds_ts_context_t* dds_ts_create_context(void);
void dds_ts_context_error(dds_ts_context_t *context, int line, int column, const char *msg);
void dds_ts_context_set_error_func(dds_ts_context_t *context, void (*error)(int line, int column, const char *msg));
void dds_ts_context_set_out_of_memory_error(dds_ts_context_t* context);
bool dds_ts_context_get_out_of_memory_error(dds_ts_context_t* context);
dds_ts_node_t* dds_ts_context_get_root_node(dds_ts_context_t *context);
void dds_ts_free_context(dds_ts_context_t* context);

bool dds_ts_new_base_type(dds_ts_context_t *context, dds_ts_node_flags_t flags, dds_ts_type_spec_ptr_t *result);
bool dds_ts_new_sequence(dds_ts_context_t *context, dds_ts_type_spec_ptr_t *element_type, dds_ts_literal_t *size, dds_ts_type_spec_ptr_t *result);
bool dds_ts_new_sequence_unbound(dds_ts_context_t *context, dds_ts_type_spec_ptr_t *base, dds_ts_type_spec_ptr_t *result);
bool dds_ts_new_string(dds_ts_context_t *context, dds_ts_literal_t *size, dds_ts_type_spec_ptr_t *result);
bool dds_ts_new_string_unbound(dds_ts_context_t *context, dds_ts_type_spec_ptr_t *result);
bool dds_ts_new_wide_string(dds_ts_context_t *context, dds_ts_literal_t *size, dds_ts_type_spec_ptr_t *result);
bool dds_ts_new_wide_string_unbound(dds_ts_context_t *context, dds_ts_type_spec_ptr_t *result);
bool dds_ts_new_fixed_pt(dds_ts_context_t *context, dds_ts_literal_t *digits, dds_ts_literal_t *fraction_digits, dds_ts_type_spec_ptr_t *result);
bool dds_ts_new_map(dds_ts_context_t *context, dds_ts_type_spec_ptr_t *key_type, dds_ts_type_spec_ptr_t *value_type, dds_ts_literal_t *size, dds_ts_type_spec_ptr_t *result);
bool dds_ts_new_map_unbound(dds_ts_context_t *context, dds_ts_type_spec_ptr_t *key_type, dds_ts_type_spec_ptr_t *value_type, dds_ts_type_spec_ptr_t *result);
bool dds_ts_new_scoped_name(dds_ts_context_t *context, dds_ts_scoped_name_t* prev, bool top, dds_ts_identifier_t name, dds_ts_scoped_name_t **result);
bool dds_ts_get_type_spec_from_scoped_name(dds_ts_context_t *context, dds_ts_scoped_name_t *scoped_name, dds_ts_type_spec_ptr_t *result);

bool dds_ts_module_open(dds_ts_context_t *context, dds_ts_identifier_t name);
void dds_ts_module_close(dds_ts_context_t *context);

bool dds_ts_add_struct_forward(dds_ts_context_t *context, dds_ts_identifier_t name);
bool dds_ts_add_struct_open(dds_ts_context_t *context, dds_ts_identifier_t name);
bool dds_ts_add_struct_extension_open(dds_ts_context_t *context, dds_ts_identifier_t name, dds_ts_scoped_name_t *scoped_name);
bool dds_ts_add_struct_member(dds_ts_context_t *context, dds_ts_type_spec_ptr_t *type);
void dds_ts_struct_close(dds_ts_context_t *context);
void dds_ts_struct_empty_close(dds_ts_context_t *context);

bool dds_ts_add_declarator(dds_ts_context_t *context, dds_ts_identifier_t name);

bool dds_ts_add_array_size(dds_ts_context_t *context, dds_ts_literal_t *value);

#endif /* DDS_TS_CREATE_H */
