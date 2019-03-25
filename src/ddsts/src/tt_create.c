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
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "typetree.h"
#include "tt_create.h"

#include "dds/ddsrt/heap.h"

struct dds_ts_context {
  dds_ts_node_t *root_node;
  dds_ts_node_t *cur_node;
  dds_ts_node_t *parent_for_declarator;
  bool out_of_memory;
  void (*error_func)(int line, int column, const char *msg);
};

static void add_child(dds_ts_node_t *node, dds_ts_node_t *parent)
{
  if (parent != NULL) {
    node->parent = parent;
    dds_ts_node_t **ref_def = &parent->children;
    while (*ref_def != 0) {
      ref_def = &(*ref_def)->next;
    }
    *ref_def = node;
  }
}

extern bool dds_ts_new_base_type(dds_ts_context_t *context, dds_ts_node_flags_t flags, dds_ts_type_spec_ptr_t *result)
{
  assert(context != NULL);
  dds_ts_base_type_t *base_type = dds_ts_create_base_type(flags);
  if (base_type == NULL) {
    context->out_of_memory = true;
    return false;
  }
  dds_ts_type_spec_ptr_assign(result, (dds_ts_type_spec_t*)base_type);
  return true;
}

static bool new_sequence(dds_ts_context_t *context, dds_ts_type_spec_ptr_t *element_type, bool bounded, unsigned long long max, dds_ts_type_spec_ptr_t *result)
{
  assert(context != NULL);
  dds_ts_sequence_t *sequence = dds_ts_create_sequence(element_type, bounded, max);
  if (sequence == NULL) {
    context->out_of_memory = true;
    return false;
  }
  dds_ts_type_spec_ptr_assign(result, (dds_ts_type_spec_t*)sequence);
  return true;
}

extern bool dds_ts_new_sequence(dds_ts_context_t *context, dds_ts_type_spec_ptr_t *base, dds_ts_literal_t *size, dds_ts_type_spec_ptr_t *result)
{
  assert(size->flags == DDS_TS_UNSIGNED_LONG_LONG_TYPE);
  return new_sequence(context, base, true, size->value.ullng, result);
}

extern bool dds_ts_new_sequence_unbound(dds_ts_context_t *context, dds_ts_type_spec_ptr_t *base, dds_ts_type_spec_ptr_t *result)
{
  return new_sequence(context, base, false, 0, result);
}

static bool new_string(dds_ts_context_t *context, dds_ts_node_flags_t flags, bool bounded, unsigned long long max, dds_ts_type_spec_ptr_t *result)
{
  assert(context != NULL);
  dds_ts_string_t *string = dds_ts_create_string(flags, bounded, max);
  if (string == NULL) {
    context->out_of_memory = true;
    return false;
  }
  dds_ts_type_spec_ptr_assign(result, (dds_ts_type_spec_t*)string);
  return true;
}

extern bool dds_ts_new_string(dds_ts_context_t *context, dds_ts_literal_t *size, dds_ts_type_spec_ptr_t *result)
{
  assert(size->flags == DDS_TS_UNSIGNED_LONG_LONG_TYPE);
  return new_string(context, DDS_TS_STRING, true, size->value.ullng, result);
}

extern bool dds_ts_new_string_unbound(dds_ts_context_t *context, dds_ts_type_spec_ptr_t *result)
{
  return new_string(context, DDS_TS_STRING, false, 0, result);
}

extern bool dds_ts_new_wide_string(dds_ts_context_t *context, dds_ts_literal_t *size, dds_ts_type_spec_ptr_t *result)
{
  assert(size->flags == DDS_TS_UNSIGNED_LONG_LONG_TYPE);
  return new_string(context, DDS_TS_WIDE_STRING, true, size->value.ullng, result);
}

extern bool dds_ts_new_wide_string_unbound(dds_ts_context_t *context, dds_ts_type_spec_ptr_t *result)
{
  return new_string(context, DDS_TS_WIDE_STRING, false, 0, result);
}

extern bool dds_ts_new_fixed_pt(dds_ts_context_t *context, dds_ts_literal_t *digits, dds_ts_literal_t *fraction_digits, dds_ts_type_spec_ptr_t *result)
{
  assert(context != NULL);
  assert(digits->flags == DDS_TS_UNSIGNED_LONG_LONG_TYPE);
  assert(fraction_digits->flags == DDS_TS_UNSIGNED_LONG_LONG_TYPE);
  dds_ts_fixed_pt_t *fixed_pt = dds_ts_create_fixed_pt(digits->value.ullng, fraction_digits->value.ullng);
  if (fixed_pt == NULL) {
    context->out_of_memory = true;
    return false;
  }
  dds_ts_type_spec_ptr_assign(result, (dds_ts_type_spec_t*)fixed_pt);
  return true;
}

static bool new_map(dds_ts_context_t *context, dds_ts_type_spec_ptr_t *key_type, dds_ts_type_spec_ptr_t *value_type, bool bounded, unsigned long long max, dds_ts_type_spec_ptr_t *result)
{
  dds_ts_map_t *map = dds_ts_create_map(key_type, value_type, bounded, max);
  if (map == NULL) {
    context->out_of_memory = true;
    return false;
  }
  dds_ts_type_spec_ptr_assign(result, (dds_ts_type_spec_t*)map);
  return true;
}

extern bool dds_ts_new_map(dds_ts_context_t *context, dds_ts_type_spec_ptr_t *key_type, dds_ts_type_spec_ptr_t *value_type, dds_ts_literal_t *size, dds_ts_type_spec_ptr_t *result)
{
  assert(size->flags == DDS_TS_UNSIGNED_LONG_LONG_TYPE);
  return new_map(context, key_type, value_type, true, size->value.ullng, result);
}

extern bool dds_ts_new_map_unbound(dds_ts_context_t *context, dds_ts_type_spec_ptr_t *key_type, dds_ts_type_spec_ptr_t *value_type, dds_ts_type_spec_ptr_t *result)
{
  return new_map(context, key_type, value_type, false, 0, result);
}

struct dds_ts_scoped_name {
  const char* name;
  bool top;
  dds_ts_scoped_name_t *next;
};

extern bool dds_ts_new_scoped_name(dds_ts_context_t *context, dds_ts_scoped_name_t* prev, bool top, dds_ts_identifier_t name, dds_ts_scoped_name_t **result)
{
  assert(context != NULL);
  dds_ts_scoped_name_t *scoped_name = (dds_ts_scoped_name_t*)ddsrt_malloc(sizeof(dds_ts_scoped_name_t));
  if (scoped_name == NULL) {
    context->out_of_memory = true;
    return false;
  }
  scoped_name->name = name;
  scoped_name->top = top;
  scoped_name->next = NULL;
  if (prev == NULL) {
    *result = scoped_name;
  }
  else {
    dds_ts_scoped_name_t **ref_scoped_name = &prev->next;
    while (*ref_scoped_name != NULL) {
      ref_scoped_name = &(*ref_scoped_name)->next;
    }
    *ref_scoped_name = scoped_name;
    *result = prev;
  }
  return true;
}

static bool resolve_scoped_name(dds_ts_context_t *context, dds_ts_scoped_name_t *scoped_name, dds_ts_definition_t **result)
{
  assert(context != NULL);
  assert(scoped_name != NULL);
  dds_ts_node_t *cur_node = scoped_name->top ? context->root_node : context->cur_node;
  for (; cur_node != NULL; cur_node = cur_node->parent) {
    dds_ts_node_t *found_node = cur_node;
    dds_ts_scoped_name_t *cur_scoped_name;
    for (cur_scoped_name = scoped_name; cur_scoped_name != NULL && found_node != NULL; cur_scoped_name = cur_scoped_name->next) {
      dds_ts_node_t *child;
      for (child = found_node->children; child != NULL; child = child->next) {
        if (DDS_TS_IS_DEFINITION(child->flags) && strcmp(((dds_ts_definition_t*)child)->name, cur_scoped_name->name) == 0) {
          break;
        }
      }
      found_node = child;
    }
    if (found_node != NULL) {
      *result = (dds_ts_definition_t*)found_node;
      return true;
    }
  }
  /* Could not resolve scoped name */
  *result = NULL;
  return false;
}

static void free_scoped_name(dds_ts_scoped_name_t *scoped_name)
{
  while (scoped_name != NULL) {
    dds_ts_scoped_name_t *next = scoped_name->next;
    ddsrt_free((void*)scoped_name->name);
    ddsrt_free((void*)scoped_name);
    scoped_name = next;
  }
}

extern bool dds_ts_get_type_spec_from_scoped_name(dds_ts_context_t *context, dds_ts_scoped_name_t *scoped_name, dds_ts_type_spec_ptr_t *result)
{
  dds_ts_definition_t *definition;
  resolve_scoped_name(context, scoped_name, &definition);
  free_scoped_name(scoped_name);
  if (definition == NULL) {
    /* Create a Boolean type, just to prevent a NULL pointer */
    return dds_ts_new_base_type(context, DDS_TS_BOOLEAN_TYPE, result);
  }
  dds_ts_type_spec_ptr_assign_reference(result, (dds_ts_type_spec_t*)definition);
  return true;
}

static bool new_module_definition(dds_ts_context_t *context, dds_ts_identifier_t name, dds_ts_node_t *parent, dds_ts_module_t **result)
{
  dds_ts_module_t *module = dds_ts_create_module(name);
  if (module == NULL) {
    context->out_of_memory = true;
    return false;
  }
  add_child(&module->def.type_spec.node, parent);
  *result = module;
  return true;
}

extern dds_ts_context_t* dds_ts_create_context(void)
{
  dds_ts_context_t *context = (dds_ts_context_t*)ddsrt_malloc(sizeof(dds_ts_context_t));
  if (context == NULL) {
    return NULL;
  }
  context->error_func = NULL;
  dds_ts_module_t *module;
  if (!new_module_definition(context, NULL, NULL, &module)) {
    ddsrt_free(context);
    return NULL;
  }
  context->root_node = (dds_ts_node_t*)module;
  context->cur_node = context->root_node;
  context->parent_for_declarator = NULL;
  return context;
}

extern void dds_ts_context_error(dds_ts_context_t *context, int line, int column, const char *msg)
{
  assert(context != NULL);
  if (context->error_func != NULL) {
    context->error_func(line, column, msg);
  }
}

extern void dds_ts_context_set_error_func(dds_ts_context_t *context, void (*error_func)(int line, int column, const char *msg))
{
  assert(context != NULL);
  context->error_func = error_func;
}

void dds_ts_context_set_out_of_memory_error(dds_ts_context_t* context)
{
  assert(context != NULL);
  context->out_of_memory = true;
}

bool dds_ts_context_get_out_of_memory_error(dds_ts_context_t* context)
{
  assert(context != NULL);
  return context->out_of_memory;
}

extern dds_ts_node_t* dds_ts_context_get_root_node(dds_ts_context_t *context)
{
  return context->root_node;
}

extern void dds_ts_free_context(dds_ts_context_t *context)
{
  assert(context != NULL);
  dds_ts_free_node(context->root_node);
  ddsrt_free(context);
}

#if (!defined(NDEBUG))
static bool cur_scope_is_definition_type(dds_ts_context_t *context, dds_ts_node_flags_t flags)
{
  assert(context != NULL && context->cur_node != NULL);
  return context->cur_node->flags == flags;
}
#endif

extern bool dds_ts_module_open(dds_ts_context_t *context, dds_ts_identifier_t name)
{
  assert(cur_scope_is_definition_type(context, DDS_TS_MODULE));
  dds_ts_module_t *module;
  if (!new_module_definition(context, name, (dds_ts_node_t*)context->cur_node, &module)) {
    return false;
  }
  context->cur_node = (dds_ts_node_t*)module;
  return true;
}

extern void dds_ts_module_close(dds_ts_context_t *context)
{
  assert(cur_scope_is_definition_type(context, DDS_TS_MODULE));
  assert(context->cur_node->parent != NULL);
  context->cur_node = context->cur_node->parent;
}

extern bool dds_ts_add_struct_forward(dds_ts_context_t *context, dds_ts_identifier_t name)
{
  assert(cur_scope_is_definition_type(context, DDS_TS_MODULE));
  dds_ts_forward_declaration_t *forward_dcl = dds_ts_create_struct_forward_dcl(name);
  if (forward_dcl == NULL) {
    context->out_of_memory = true;
    return false;
  }
  add_child(&forward_dcl->def.type_spec.node, context->cur_node);
  return true;
}

extern bool dds_ts_add_struct_open(dds_ts_context_t *context, dds_ts_identifier_t name)
{
  assert(cur_scope_is_definition_type(context, DDS_TS_MODULE));
  dds_ts_struct_t *new_struct = dds_ts_create_struct(name);
  if (new_struct == NULL) {
    context->out_of_memory = true;
    return false;
  }
  add_child(&new_struct->def.type_spec.node, context->cur_node);
  context->cur_node = (dds_ts_node_t*)new_struct;
  return true;
}

extern bool dds_ts_add_struct_extension_open(dds_ts_context_t *context, dds_ts_identifier_t name, dds_ts_scoped_name_t *scoped_name)
{
  assert(cur_scope_is_definition_type(context, DDS_TS_MODULE));
  dds_ts_struct_t *new_struct = dds_ts_create_struct(name);
  if (new_struct == NULL) {
    context->out_of_memory = true;
    return false;
  }
  add_child(&new_struct->def.type_spec.node, context->cur_node);
  /* find super */
  dds_ts_definition_t *definition;
  if (!resolve_scoped_name(context, scoped_name, &definition)) {
    free_scoped_name(scoped_name);
    return false;
  }
  free_scoped_name(scoped_name);
  if (definition != NULL && definition->type_spec.node.flags == DDS_TS_STRUCT) {
    new_struct->super = definition;
  }
  context->cur_node = (dds_ts_node_t*)new_struct;
  return true;
}

extern bool dds_ts_add_struct_member(dds_ts_context_t *context, dds_ts_type_spec_ptr_t *spec_type_ptr)
{
  assert(cur_scope_is_definition_type(context, DDS_TS_STRUCT));
  dds_ts_struct_member_t *member = dds_ts_create_struct_member(spec_type_ptr);
  if (member == NULL) {
    context->out_of_memory = true;
    return false;
  }
  add_child(&member->node, context->cur_node);
  context->parent_for_declarator = (dds_ts_node_t*)member;
  return true;
}

extern void dds_ts_struct_close(dds_ts_context_t *context)
{
  assert(cur_scope_is_definition_type(context, DDS_TS_STRUCT));
  context->cur_node = context->cur_node->parent;
  context->parent_for_declarator = NULL;
}

extern void dds_ts_struct_empty_close(dds_ts_context_t *context)
{
  assert(cur_scope_is_definition_type(context, DDS_TS_STRUCT));
  context->cur_node = context->cur_node->parent;
  context->parent_for_declarator = NULL;
}

extern bool dds_ts_add_declarator(dds_ts_context_t *context, dds_ts_identifier_t name)
{
  assert(context != NULL);
  if (context->parent_for_declarator != NULL && context->cur_node->flags == DDS_TS_STRUCT) {
    dds_ts_definition_t* declarator = dds_ts_create_declarator(name);
    if (declarator == NULL) {
      context->out_of_memory = true;
      return false;
    }
    add_child(&declarator->type_spec.node, context->parent_for_declarator);
    return true;
  }
  assert(false);
  return false;
}

extern bool dds_ts_add_array_size(dds_ts_context_t *context, dds_ts_literal_t *value)
{
  assert(context != NULL);
  assert(value->flags == DDS_TS_UNSIGNED_LONG_LONG_TYPE);
  dds_ts_array_size_t *array_size = dds_ts_create_array_size(value->value.ullng);
  if (array_size == NULL) {
    context->out_of_memory = true;
    return false;
  }
  add_child(&array_size->node, context->cur_node);
  return true;
}

