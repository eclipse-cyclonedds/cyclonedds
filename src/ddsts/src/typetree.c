/*
 * Copyright(c) 2006 to 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include <string.h>
#include <assert.h>
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsts/typetree.h"


void ddsts_free_literal(ddsts_literal_t *literal)
{
  if (literal->flags & DDSTS_STRING) {
    ddsrt_free(literal->value.str);
  }
}

dds_return_t ddsts_free_type(ddsts_type_t *type)
{
  if (type != NULL) {
    if (type->type.parent != NULL) {
      return DDS_RETCODE_ERROR;
    }
    type->type.free_func(type);
  }
  return DDS_RETCODE_OK;
}

static void init_type_ref(ddsts_type_t **ref_type, ddsts_type_t *type, ddsts_type_t *parent, ddsts_flags_t ref_flag)
{
  *ref_type = type;
  if (type != NULL) {
    if (type->type.parent == NULL) {
      type->type.parent = parent;
    }
    else {
      parent->type.flags |= ref_flag;
    }
  }
}

static void free_type_ref(ddsts_type_t *type, ddsts_type_t *parent, ddsts_flags_t ref_flag)
{
  if (type != NULL && (parent->type.flags & ref_flag) == 0) {
    type->type.free_func(type);
  }
}

static void init_children(ddsts_type_list_t *list)
{
  list->first = NULL;
  list->ref_end = &list->first;
}

static void children_add(ddsts_type_list_t *list, ddsts_type_t *child)
{
  *list->ref_end = child;
  list->ref_end = &child->type.next;
}

static void free_children(ddsts_type_list_t *list)
{
  ddsts_type_t *type = list->first; 
  while (type != NULL) {
    ddsts_type_t *next = type->type.next;
    type->type.free_func(type);
    type = next;
  }
}

static void init_type(ddsts_type_t *type, void (*free_func)(ddsts_type_t*), ddsts_flags_t flags, ddsts_identifier_t name)
{
  type->type.flags = flags;
  type->type.name = name;
  type->type.parent = NULL;
  type->type.next = NULL;
  type->type.free_func = free_func;
}

static void free_type(ddsts_type_t *type)
{
  ddsrt_free(type->type.name);
  ddsrt_free(type);
}

/* ddsts_base_type_t */

dds_return_t ddsts_create_base_type(ddsts_flags_t flags, ddsts_type_t **result)
{
  ddsts_type_t *type = (ddsts_type_t*)ddsrt_malloc_s(sizeof(ddsts_base_type_t));
  if (type == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  init_type(type, free_type, flags, NULL);
  *result = type;
  return DDS_RETCODE_OK;
}

/* ddsts_sequence_t */

static void free_sequence(ddsts_type_t *type)
{
  free_type_ref(type->sequence.element_type, type, DDSTS_REFERENCE_1);
  free_type(type);
}

dds_return_t ddsts_create_sequence(ddsts_type_t* element_type, unsigned long long max, ddsts_type_t **result)
{
  ddsts_type_t *type = (ddsts_type_t*)ddsrt_malloc_s(sizeof(ddsts_sequence_t));
  if (type == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  ddsts_flags_t flags = DDSTS_SEQUENCE;
  if (max == 0ULL) {
    flags |= DDSTS_UNBOUND;
  }
  init_type(type, free_sequence, flags, NULL);
  init_type_ref(&type->sequence.element_type, element_type, type, DDSTS_REFERENCE_1);
  type->sequence.max = max;
  *result = type;
  return DDS_RETCODE_OK;
}

/* ddsts_array_t */

static void free_array(ddsts_type_t *type)
{
  free_type_ref(type->array.element_type, type, DDSTS_REFERENCE_1);
  free_type(type);
}

dds_return_t ddsts_create_array(ddsts_type_t* element_type, unsigned long long size, ddsts_type_t **result)
{
  ddsts_type_t *type = (ddsts_type_t*)ddsrt_malloc_s(sizeof(ddsts_array_t));
  if (type == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  init_type(type, free_array, DDSTS_ARRAY, NULL);
  init_type_ref(&type->array.element_type, element_type, type, DDSTS_REFERENCE_1);
  type->array.size = size;
  *result = type;
  return DDS_RETCODE_OK;
}

dds_return_t ddsts_array_set_element_type(ddsts_type_t *array, ddsts_type_t *element_type)
{
  assert(array->array.element_type == NULL);
  init_type_ref(&array->array.element_type, element_type, array, DDSTS_REFERENCE_1);
  return DDS_RETCODE_OK;
}

/* ddsts_string_t */

static void free_string(ddsts_type_t *type)
{
  free_type(type);
}

dds_return_t ddsts_create_string(ddsts_flags_t flags, unsigned long long max, ddsts_type_t **result)
{
  ddsts_type_t *type = (ddsts_type_t*)ddsrt_malloc_s(sizeof(ddsts_string_t));
  if (type == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  if (max == 0ULL) {
    flags |= DDSTS_UNBOUND;
  }
  init_type(type, free_string, flags, NULL);
  type->string.max = max;
  *result = type;
  return DDS_RETCODE_OK;
}

/* ddsts_fixed_pt_t */

static void free_fixed_pt(ddsts_type_t *type)
{
  free_type(type);
}

dds_return_t ddsts_create_fixed_pt(unsigned long long digits, unsigned long long fraction_digits, ddsts_type_t **result)
{
  ddsts_type_t *type = (ddsts_type_t*)ddsrt_malloc_s(sizeof(ddsts_fixed_pt_t));
  if (type == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  init_type(type, free_fixed_pt, DDSTS_FIXED_PT, NULL);
  type->fixed_pt.digits = digits;
  type->fixed_pt.fraction_digits = fraction_digits;
  *result = type;
  return DDS_RETCODE_OK;
}

/* ddsts_map_t */

static void free_map(ddsts_type_t *type)
{
  free_type_ref(type->map.key_type, type, DDSTS_REFERENCE_1);
  free_type_ref(type->map.value_type, type, DDSTS_REFERENCE_2);
  free_type(type);
}

dds_return_t ddsts_create_map(ddsts_type_t *key_type, ddsts_type_t *value_type, unsigned long long max, ddsts_type_t **result)
{
  ddsts_type_t *type = (ddsts_type_t*)ddsrt_malloc_s(sizeof(ddsts_map_t));
  if (type == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  ddsts_flags_t flags = DDSTS_MAP;
  if (max == 0ULL) {
    flags |= DDSTS_UNBOUND;
  }
  init_type(type, free_map, flags, NULL);
  init_type_ref(&type->map.key_type, key_type, type, DDSTS_REFERENCE_1);
  init_type_ref(&type->map.value_type, value_type, type, DDSTS_REFERENCE_2);
  type->map.max = max;
  *result = type;
  return DDS_RETCODE_OK;
}

/* ddsts_module_t */

static void free_module(ddsts_type_t *type)
{
  free_children(&type->module.members);
  free_type(type);
}

dds_return_t ddsts_create_module(ddsts_identifier_t name, ddsts_type_t **result)
{
  ddsts_type_t *type = (ddsts_type_t*)ddsrt_malloc_s(sizeof(ddsts_module_t));
  if (type == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  init_type(type, free_module, DDSTS_MODULE, name);
  init_children(&type->module.members);
  type->module.previous = NULL;
  *result = type;
  return DDS_RETCODE_OK;
}

dds_return_t ddsts_module_add_member(ddsts_type_t *module, ddsts_type_t *member)
{
  if (module != NULL) {
    member->type.parent = module;
    children_add(&module->module.members, member);

    /* if the member is a module, find previous open of this module, if any */
    if (DDSTS_IS_TYPE(member, DDSTS_MODULE)) {
      ddsts_module_t *parent_module;
      for (parent_module = &module->module; parent_module != NULL; parent_module = parent_module->previous) {
        ddsts_type_t *child;
        for (child = parent_module->members.first; child != NULL; child = child->type.next) {
          if (DDSTS_IS_TYPE(child, DDSTS_MODULE) && strcmp(child->type.name, member->type.name) == 0 && child != member) {
            member->module.previous = &child->module;
          }
        }
        if (member->module.previous != NULL) {
          break;
        }
      }
    }

    /* if the member is a struct, set 'definition' of matching forward declarations */
    if (DDSTS_IS_TYPE(member, DDSTS_STRUCT)) {
      ddsts_module_t *parent_module;
      for (parent_module = &module->module; parent_module != NULL; parent_module = parent_module->previous) {
        ddsts_type_t *child;
        for (child = parent_module->members.first; child != NULL; child = child->type.next) {
          if (DDSTS_IS_TYPE(child, DDSTS_FORWARD_STRUCT) && strcmp(child->type.name, member->type.name) == 0) {
            child->forward.definition = member;
          }
        }
      }
    }
  }
  return DDS_RETCODE_OK;
}

/* ddsts_forward_declaration_t */

static void free_forward(ddsts_type_t *type)
{
  free_type(type);
}

dds_return_t ddsts_create_struct_forward_dcl(ddsts_identifier_t name, ddsts_type_t **result)
{
  ddsts_type_t *type = (ddsts_type_t*)ddsrt_malloc_s(sizeof(ddsts_forward_t));
  if (type == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  init_type(type, free_forward, DDSTS_FORWARD_STRUCT, name);
  type->forward.definition = NULL;
  *result = type;
  return DDS_RETCODE_OK;
}

/* ddsts_struct_t */

static void free_struct(ddsts_type_t *type)
{
  ddsts_struct_key_t *key = type->struct_def.keys;
  while (key != NULL) {
    ddsts_struct_key_t *next = key->next;
    ddsrt_free(key);
    key = next;
  }
  free_children(&type->struct_def.members);
  free_type(type);
}

dds_return_t ddsts_create_struct(ddsts_identifier_t name, ddsts_type_t **result)
{
  ddsts_type_t *type = (ddsts_type_t*)ddsrt_malloc_s(sizeof(ddsts_struct_t));
  if (type == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  init_type(type, free_struct, DDSTS_STRUCT, name);
  init_children(&type->struct_def.members);
  type->struct_def.super = NULL;
  type->struct_def.keys = NULL;
  *result = type;
  return DDS_RETCODE_OK;
}

dds_return_t ddsts_struct_add_member(ddsts_type_t *struct_def, ddsts_type_t *member)
{
  if (struct_def != NULL) {
    member->type.parent = struct_def;
    children_add(&struct_def->struct_def.members, member);
  }
  return DDS_RETCODE_OK;
}

dds_return_t ddsts_struct_add_key(ddsts_type_t *struct_def, ddsts_type_t *member)
{
  /* We traverse the list to the end and check if 'member' is already included */
  ddsts_struct_key_t **ref_key = &struct_def->struct_def.keys;
  while (*ref_key != NULL) {
    if ((*ref_key)->member == member) {
      return DDS_RETCODE_ERROR;
    }
    ref_key = &(*ref_key)->next;
  }
  (*ref_key) = (ddsts_struct_key_t*)ddsrt_malloc(sizeof(ddsts_struct_key_t));
  if (*ref_key == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  (*ref_key)->member = member;
  (*ref_key)->next = NULL;
  return DDS_RETCODE_OK;
}

/* ddsts_declaration_t */

static void free_declaration(ddsts_type_t *type)
{
  free_type_ref(type->declaration.decl_type, type, DDSTS_REFERENCE_1);
  free_type(type);
}

dds_return_t ddsts_create_declaration(ddsts_identifier_t name, ddsts_type_t *decl_type, ddsts_type_t **result)
{
  ddsts_type_t *type = (ddsts_type_t*)ddsrt_malloc_s(sizeof(ddsts_declaration_t));
  if (type == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  init_type(type, free_declaration, DDSTS_DECLARATION, name);
  init_type_ref(&type->declaration.decl_type, decl_type, type, DDSTS_REFERENCE_1);
  *result = type;
  return DDS_RETCODE_OK;
}

dds_return_t ddsts_declaration_set_type(ddsts_type_t *declaration, ddsts_type_t *type)
{
  assert(declaration->declaration.decl_type == NULL);
  init_type_ref(&declaration->declaration.decl_type, type, declaration, DDSTS_REFERENCE_1);
  return DDS_RETCODE_OK;
}

dds_return_t ddsts_create_union_forward_dcl(ddsts_identifier_t name, ddsts_type_t **result)
{
  ddsts_type_t *type = (ddsts_type_t*)ddsrt_malloc_s(sizeof(ddsts_forward_t));
  if (type == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  init_type(type, free_forward, DDSTS_FORWARD_UNION, name);
  type->forward.definition = NULL;
  *result = type;
  return DDS_RETCODE_OK;
}

/* ddsts_union_t */

static void free_union(ddsts_type_t *type)
{
  free_children(&type->union_def.cases);
  free_type(type);
}

dds_return_t
ddsts_create_union(ddsts_identifier_t name, ddsts_flags_t switch_type, ddsts_type_t **result)
{
  ddsts_type_t *type = (ddsts_type_t*)ddsrt_malloc_s(sizeof(ddsts_union_t));
  if (type == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  init_type(type, free_union, DDSTS_UNION, name);
  type->union_def.switch_type = switch_type;
  init_children(&type->union_def.cases);
  *result = type;
  return DDS_RETCODE_OK;
}

void ddsts_free_union_case_labels(ddsts_union_case_label_t *labels)
{
  while (labels != NULL) {
    ddsts_union_case_label_t *next = labels->next;
    ddsts_free_literal(&labels->value);
    ddsrt_free(labels);
    labels = next;
  }
}

static void free_union_case(ddsts_type_t *type)
{
  ddsts_free_union_case_labels(type->union_case.labels);
  free_declaration(type);
}

dds_return_t
ddsts_union_add_case(ddsts_type_t *union_def, ddsts_union_case_label_t *labels, bool default_label, ddsts_type_t **result)
{
  ddsts_type_t *type = (ddsts_type_t*)ddsrt_malloc_s(sizeof(ddsts_union_case_t));
  if (type == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  init_type(type, free_union_case, DDSTS_UNION_CASE, NULL);
  type->declaration.decl_type = NULL;
  type->union_case.labels = labels;
  type->union_case.default_label = default_label;
  type->type.parent = union_def;
  children_add(&union_def->union_def.cases, type);
  *result = type;
  return DDS_RETCODE_OK;
}

dds_return_t
ddsts_union_case_set_decl(ddsts_type_t *union_case, ddsts_identifier_t name, ddsts_type_t *type)
{
  union_case->type.name = name;
  ddsts_declaration_set_type(union_case, type);
  return DDS_RETCODE_OK;
}
/* Utility functions */

dds_return_t
ddsts_declaration_is_key(ddsts_call_path_t *path, bool *is_key)
{
  *is_key = false;
  while (path != NULL && DDSTS_IS_TYPE(path->type, DDSTS_DECLARATION)) {
    ddsts_call_path_t *struct_path = path->call_parent;
    if (struct_path == NULL || !DDSTS_IS_TYPE(struct_path->type, DDSTS_STRUCT)) {
      return DDS_RETCODE_ERROR;
    }
    *is_key = false;
    if (struct_path->type->struct_def.keys != NULL) {
      for (ddsts_struct_key_t *key = struct_path->type->struct_def.keys; key != NULL; key = key->next) {
        if (key->member == path->type) {
          *is_key = true;
          break;
        }
      }
      if (!*is_key) {
        /* The structure has keys, but the declartion is not one of them. */
        return DDS_RETCODE_OK;
      }
    }
    path = struct_path->call_parent;
  }
  if (path == NULL || !DDSTS_IS_TYPE(path->type, DDSTS_MODULE)) {
    return DDS_RETCODE_ERROR;
  }
  return DDS_RETCODE_OK;
}

