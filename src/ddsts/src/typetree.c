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

#include "typetree.h"
#include <assert.h>
#include "dds/ddsrt/heap.h"

/* dds_ts_node_t */

extern void dds_ts_free_node(dds_ts_node_t *node)
{
  node->free_func(node);
}

static void init_node(dds_ts_node_t *node, void (*free_func)(dds_ts_node_t*), dds_ts_node_flags_t flags)
{
  node->free_func = free_func;
  node->flags = flags;
  node->parent = NULL;
  node->children = NULL;
  node->next = NULL;
}

static void free_node(dds_ts_node_t *node)
{
  dds_ts_node_t *child;
  for (child = node->children; child != NULL;) {
    dds_ts_node_t *next = child->next;
    dds_ts_free_node(child);
    child = next;
  }
  ddsrt_free((void*)node);
}

/* dds_ts_type_spec_t (is abstract) */

static void init_type_spec(dds_ts_type_spec_t *type_spec, void (*free_func)(dds_ts_node_t*), dds_ts_node_flags_t flags)
{
  init_node(&type_spec->node, free_func, flags);
}

static void free_type_spec(dds_ts_type_spec_t *type_spec)
{
  free_node(&type_spec->node);
}

/* dds_ts_type_spec_ptr_t */

void dds_ts_type_spec_ptr_assign(dds_ts_type_spec_ptr_t *type_spec_ptr, dds_ts_type_spec_t *type_spec)
{
  type_spec_ptr->type_spec = type_spec;
  type_spec_ptr->is_reference = false;
}

void dds_ts_type_spec_ptr_assign_reference(dds_ts_type_spec_ptr_t *type_spec_ptr, dds_ts_type_spec_t *type_spec)
{
  type_spec_ptr->type_spec = type_spec;
  type_spec_ptr->is_reference = true;
}

/* dds_ts_base_type_t */

static void init_base_type(dds_ts_base_type_t *base_type, void (*free_func)(dds_ts_node_t*), dds_ts_node_flags_t flags)
{
  init_type_spec(&base_type->type_spec, free_func, flags);
}

static void free_base_type(dds_ts_node_t *node)
{
  free_type_spec(&((dds_ts_base_type_t*)node)->type_spec);
}

extern dds_ts_base_type_t *dds_ts_create_base_type(dds_ts_node_flags_t flags)
{
  dds_ts_base_type_t *base_type = (dds_ts_base_type_t*)ddsrt_malloc(sizeof(dds_ts_base_type_t));
  if (base_type == NULL) {
    return NULL;
  }
  init_base_type(base_type, free_base_type, flags);
  return base_type;
}

/* dds_ts_type_spec_ptr_t */

static void free_type_spec_ptr(dds_ts_type_spec_ptr_t* type_spec_ptr)
{
  if (!type_spec_ptr->is_reference) {
    dds_ts_free_node(&type_spec_ptr->type_spec->node);
  }
}

/* dds_ts_sequence_t */

static void free_sequence(dds_ts_node_t *node)
{
  free_type_spec_ptr(&((dds_ts_sequence_t*)node)->element_type);
  free_type_spec(&((dds_ts_sequence_t*)node)->type_spec);
}

extern dds_ts_sequence_t *dds_ts_create_sequence(dds_ts_type_spec_ptr_t* element_type, bool bounded, unsigned long long max)
{
  dds_ts_sequence_t *sequence = (dds_ts_sequence_t*)ddsrt_malloc(sizeof(dds_ts_sequence_t));
  if (sequence == NULL) {
    return NULL;
  }
  init_type_spec(&sequence->type_spec, free_sequence, DDS_TS_SEQUENCE);
  sequence->element_type = *element_type;
  sequence->bounded = bounded;
  sequence->max = max;
  return sequence;
}

/* dds_ts_string_t */

static void free_string(dds_ts_node_t *node)
{
  free_type_spec(&((dds_ts_string_t*)node)->type_spec);
}

extern dds_ts_string_t *dds_ts_create_string(dds_ts_node_flags_t flags, bool bounded, unsigned long long max)
{
  dds_ts_string_t *string = (dds_ts_string_t*)ddsrt_malloc(sizeof(dds_ts_string_t));
  if (string == NULL) {
    return NULL;
  }
  init_type_spec(&string->type_spec, free_string, flags);
  string->bounded = bounded;
  string->max = max;
  return string;
}

/* dds_ts_fixed_pt_t */

static void free_fixed_pt(dds_ts_node_t *node)
{
  free_type_spec(&((dds_ts_fixed_pt_t*)node)->type_spec);
}

extern dds_ts_fixed_pt_t *dds_ts_create_fixed_pt(unsigned long long digits, unsigned long long fraction_digits)
{
  dds_ts_fixed_pt_t *fixed_pt = (dds_ts_fixed_pt_t*)ddsrt_malloc(sizeof(dds_ts_fixed_pt_t));
  if (fixed_pt == NULL) {
    return NULL;
  }
  init_type_spec(&fixed_pt->type_spec, free_fixed_pt, DDS_TS_FIXED_PT);
  fixed_pt->digits = digits;
  fixed_pt->fraction_digits = fraction_digits;
  return fixed_pt;
}

/* dds_ts_map_t */

static void free_map(dds_ts_node_t *node)
{
  free_type_spec_ptr(&((dds_ts_map_t*)node)->key_type);
  free_type_spec_ptr(&((dds_ts_map_t*)node)->value_type);
  free_type_spec(&((dds_ts_map_t*)node)->type_spec);
}

extern dds_ts_map_t *dds_ts_create_map(dds_ts_type_spec_ptr_t *key_type, dds_ts_type_spec_ptr_t *value_type, bool bounded, unsigned long long max)
{
  dds_ts_map_t *map = (dds_ts_map_t*)ddsrt_malloc(sizeof(dds_ts_map_t));
  if (map == NULL) {
    return NULL;
  }
  init_type_spec(&map->type_spec, free_map, DDS_TS_MAP);
  map->key_type = *key_type;
  map->value_type = *value_type;
  map->bounded = bounded;
  map->max = max;
  return map;
}

/* dds_ts_definition_t (is abstract) */

static void init_definition(dds_ts_definition_t *definition, void (*free_func)(dds_ts_node_t*), dds_ts_node_flags_t flags, dds_ts_identifier_t name)
{
  init_type_spec(&definition->type_spec, free_func, flags);
  definition->name = name;
}

static void free_definition(dds_ts_node_t *node)
{
  ddsrt_free((void*)((dds_ts_definition_t*)node)->name);
  free_type_spec(&((dds_ts_definition_t*)node)->type_spec);
}

/* dds_ts_module_t */

static void free_module(dds_ts_node_t *node)
{
  free_definition(node);
}

dds_ts_module_t *dds_ts_create_module(dds_ts_identifier_t name)
{
  dds_ts_module_t *module = (dds_ts_module_t*)ddsrt_malloc(sizeof(dds_ts_module_t));
  if (module == NULL) {
    return NULL;
  }
  init_definition(&module->def, free_module, DDS_TS_MODULE, name);
  return module;
}

/* dds_ts_forward_declaration_t */

static void free_forward_declaration(dds_ts_node_t *node)
{
  free_definition(node);
}

extern dds_ts_forward_declaration_t *dds_ts_create_struct_forward_dcl(dds_ts_identifier_t name)
{
  dds_ts_forward_declaration_t *forward_dcl = (dds_ts_forward_declaration_t*)ddsrt_malloc(sizeof(dds_ts_forward_declaration_t));
  if (forward_dcl == NULL) {
    return NULL;
  }
  init_definition(&forward_dcl->def, free_forward_declaration, DDS_TS_FORWARD_STRUCT, name);
  forward_dcl->definition = NULL;
  return forward_dcl;
}

/* dds_ts_struct_t */

static void free_struct(dds_ts_node_t *node)
{
  free_definition(node);
}

extern dds_ts_struct_t *dds_ts_create_struct(dds_ts_identifier_t name)
{
  dds_ts_struct_t *new_struct = (dds_ts_struct_t*)ddsrt_malloc(sizeof(dds_ts_struct_t));
  if (new_struct == NULL) {
    return NULL;
  }
  init_definition(&new_struct->def, free_struct, DDS_TS_STRUCT, name);
  new_struct->super = NULL;
  return new_struct;
}

/* dds_ts_struct_member_t */

static void free_struct_member(dds_ts_node_t *node)
{
  free_type_spec_ptr(&((dds_ts_struct_member_t*)node)->member_type);
  free_node(&((dds_ts_struct_member_t*)node)->node);
}

extern dds_ts_struct_member_t *dds_ts_create_struct_member(dds_ts_type_spec_ptr_t *member_type)
{
  dds_ts_struct_member_t *member = (dds_ts_struct_member_t*)ddsrt_malloc(sizeof(dds_ts_struct_member_t));
  if (member == NULL) {
    return NULL;
  }
  init_node(&member->node, free_struct_member, DDS_TS_STRUCT_MEMBER);
  member->member_type = *member_type;
  return member;
}

/* Declarator */

extern dds_ts_definition_t *dds_ts_create_declarator(dds_ts_identifier_t name)
{
  dds_ts_definition_t* declarator = (dds_ts_definition_t*)ddsrt_malloc(sizeof(dds_ts_definition_t));
  if (declarator == NULL) {
    return NULL;
  }
  init_definition(declarator, free_definition, DDS_TS_DECLARATOR, name);
  return declarator;
}

/* dds_ts_array_size_t */

static void free_array_size(dds_ts_node_t *node)
{
  free_node(&((dds_ts_array_size_t*)node)->node);
}

extern dds_ts_array_size_t *dds_ts_create_array_size(unsigned long long size)
{
  dds_ts_array_size_t *array_size = (dds_ts_array_size_t*)ddsrt_malloc(sizeof(dds_ts_array_size_t));
  if (array_size == NULL) {
    return NULL;
  }
  init_node(&array_size->node, free_array_size, DDS_TS_ARRAY_SIZE);
  array_size->size = size;
  return array_size;
}

