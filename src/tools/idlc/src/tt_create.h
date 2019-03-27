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
#ifndef DDSTS_CREATE_H
#define DDSTS_CREATE_H

#include <stdbool.h>
#include "dds/ddsrt/retcode.h"

/* Some types only used during parsing */

typedef struct ddsts_scoped_name ddsts_scoped_name_t;
typedef struct ddsts_context ddsts_context_t;

ddsts_context_t* ddsts_create_context(void);
void ddsts_context_error(ddsts_context_t *context, int line, int column, const char *msg);
void ddsts_context_set_error_func(ddsts_context_t *context, void (*error)(int line, int column, const char *msg));
dds_return_t ddsts_context_get_retcode(ddsts_context_t* context);
ddsts_type_t* ddsts_context_take_root_type(ddsts_context_t *context);
void ddsts_free_context(ddsts_context_t* context);

bool ddsts_context_copy_identifier(ddsts_context_t *context, ddsts_identifier_t source, ddsts_identifier_t *dest);

bool ddsts_new_base_type(ddsts_context_t *context, ddsts_flags_t flags, ddsts_type_t **result);
bool ddsts_new_sequence(ddsts_context_t *context, ddsts_type_t *element_type, ddsts_literal_t *size, ddsts_type_t **result);
bool ddsts_new_sequence_unbound(ddsts_context_t *context, ddsts_type_t *base, ddsts_type_t **result);
bool ddsts_new_string(ddsts_context_t *context, ddsts_literal_t *size, ddsts_type_t **result);
bool ddsts_new_string_unbound(ddsts_context_t *context, ddsts_type_t **result);
bool ddsts_new_wide_string(ddsts_context_t *context, ddsts_literal_t *size, ddsts_type_t **result);
bool ddsts_new_wide_string_unbound(ddsts_context_t *context, ddsts_type_t **result);
bool ddsts_new_fixed_pt(ddsts_context_t *context, ddsts_literal_t *digits, ddsts_literal_t *fraction_digits, ddsts_type_t **result);
bool ddsts_new_map(ddsts_context_t *context, ddsts_type_t *key_type, ddsts_type_t *value_type, ddsts_literal_t *size, ddsts_type_t **result);
bool ddsts_new_map_unbound(ddsts_context_t *context, ddsts_type_t *key_type, ddsts_type_t *value_type, ddsts_type_t **result);
bool ddsts_new_scoped_name(ddsts_context_t *context, ddsts_scoped_name_t* prev, bool top, ddsts_identifier_t name, ddsts_scoped_name_t **result);
bool ddsts_get_type_from_scoped_name(ddsts_context_t *context, ddsts_scoped_name_t *scoped_name, ddsts_type_t **result);
void ddsts_free_scoped_name(ddsts_scoped_name_t *scoped_name);

bool ddsts_module_open(ddsts_context_t *context, ddsts_identifier_t name);
void ddsts_module_close(ddsts_context_t *context);

bool ddsts_add_struct_forward(ddsts_context_t *context, ddsts_identifier_t name);
bool ddsts_add_struct_open(ddsts_context_t *context, ddsts_identifier_t name);
bool ddsts_add_struct_extension_open(ddsts_context_t *context, ddsts_identifier_t name, ddsts_scoped_name_t *scoped_name);
bool ddsts_add_struct_member(ddsts_context_t *context, ddsts_type_t **ref_type);
void ddsts_struct_close(ddsts_context_t *context, ddsts_type_t **result);
void ddsts_struct_empty_close(ddsts_context_t *context, ddsts_type_t **result);

bool ddsts_add_declarator(ddsts_context_t *context, ddsts_identifier_t name);

bool ddsts_add_array_size(ddsts_context_t *context, ddsts_literal_t *value);

void ddsts_accept(ddsts_context_t *context);

#endif /* DDSTS_CREATE_H */
