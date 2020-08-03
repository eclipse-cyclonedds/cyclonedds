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
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsts/typetree.h"
#include "tt_create.h"

typedef struct array_size array_size_t;
struct array_size {
  unsigned long long size;
  array_size_t *next;
};

typedef struct annotation annotation_t;
struct annotation {
  ddsts_scoped_name_t *scoped_name;
  annotation_t *next;
};

typedef struct annotations_stack annotations_stack_t;
struct annotations_stack {
  annotation_t *annotations;
  annotations_stack_t *deeper;
};

typedef struct pragma_arg pragma_arg_t;
struct pragma_arg {
  ddsts_identifier_t arg;
  pragma_arg_t *next;
};

struct ddsts_context {
  ddsts_type_t *root_type;
  ddsts_type_t *cur_type;
  ddsts_type_t *type_for_declarator;
  ddsts_union_case_label_t *union_case_labels;
  bool union_case_default_label;
  array_size_t *array_sizes;
  annotations_stack_t *annotations_stack;
  pragma_arg_t *pragma_args;
  dds_return_t retcode;
  bool semantic_error;
  ddsts_identifier_t dangling_identifier;
};

extern bool ddsts_new_base_type(ddsts_context_t *context, ddsts_flags_t flags, ddsts_type_t **result)
{
  assert(context != NULL);
  ddsts_type_t *base_type;
  dds_return_t rc = ddsts_create_base_type(flags, &base_type);
  if (rc != DDS_RETCODE_OK) {
    context->retcode = rc;
    return false;
  }
  *result = base_type;
  return true;
}

static bool new_sequence(ddsts_context_t *context, ddsts_type_t *element_type, unsigned long long max, ddsts_type_t **result)
{
  assert(context != NULL);
  ddsts_type_t *sequence;
  dds_return_t rc = ddsts_create_sequence(element_type, max, &sequence);
  if (rc != DDS_RETCODE_OK) {
    context->retcode = rc;
    return false;
  }
  *result = sequence;
  return true;
}

extern bool ddsts_new_sequence(ddsts_context_t *context, ddsts_type_t *base, ddsts_literal_t *size, ddsts_type_t **result)
{
  assert(size->flags == (DDSTS_INT64 | DDSTS_UNSIGNED));
  return new_sequence(context, base, size->value.ullng, result);
}

extern bool ddsts_new_sequence_unbound(ddsts_context_t *context, ddsts_type_t *base, ddsts_type_t **result)
{
  return new_sequence(context, base, 0ULL, result);
}

static bool new_string(ddsts_context_t *context, ddsts_flags_t flags, unsigned long long max, ddsts_type_t **result)
{
  assert(context != NULL);
  ddsts_type_t *string;
  dds_return_t rc = ddsts_create_string(flags, max, &string);
  if (rc != DDS_RETCODE_OK) {
    context->retcode = rc;
    return false;
  }
  *result = string;
  return true;
}

extern bool ddsts_new_string(ddsts_context_t *context, ddsts_literal_t *size, ddsts_type_t **result)
{
  assert(size->flags == (DDSTS_INT64 | DDSTS_UNSIGNED));
  return new_string(context, DDSTS_STRING, size->value.ullng, result);
}

extern bool ddsts_new_string_unbound(ddsts_context_t *context, ddsts_type_t **result)
{
  return new_string(context, DDSTS_STRING, 0ULL, result);
}

extern bool ddsts_new_wide_string(ddsts_context_t *context, ddsts_literal_t *size, ddsts_type_t **result)
{
  assert(size->flags == (DDSTS_INT64 | DDSTS_UNSIGNED));
  return new_string(context, DDSTS_STRING | DDSTS_WIDE, size->value.ullng, result);
}

extern bool ddsts_new_wide_string_unbound(ddsts_context_t *context, ddsts_type_t **result)
{
  return new_string(context, DDSTS_STRING | DDSTS_WIDE, 0ULL, result);
}

extern bool ddsts_new_fixed_pt(ddsts_context_t *context, ddsts_literal_t *digits, ddsts_literal_t *fraction_digits, ddsts_type_t **result)
{
  assert(context != NULL);
  assert(digits->flags == (DDSTS_INT64 | DDSTS_UNSIGNED));
  assert(fraction_digits->flags == (DDSTS_INT64 | DDSTS_UNSIGNED));
  ddsts_type_t *fixed_pt;
  dds_return_t rc = ddsts_create_fixed_pt(digits->value.ullng, fraction_digits->value.ullng, &fixed_pt);
  if (rc != DDS_RETCODE_OK) {
    context->retcode = rc;
    return false;
  }
  *result = fixed_pt;
  return true;
}

static bool new_map(ddsts_context_t *context, ddsts_type_t *key_type, ddsts_type_t *value_type, unsigned long long max, ddsts_type_t **result)
{
  ddsts_type_t *map;
  dds_return_t rc = ddsts_create_map(key_type, value_type, max, &map);
  if (rc != DDS_RETCODE_OK) {
    context->retcode = rc;
    return false;
  }
  *result = map;
  return true;
}

extern bool ddsts_new_map(ddsts_context_t *context, ddsts_type_t *key_type, ddsts_type_t *value_type, ddsts_literal_t *size, ddsts_type_t **result)
{
  assert(size->flags == (DDSTS_INT64 | DDSTS_UNSIGNED));
  return new_map(context, key_type, value_type, size->value.ullng, result);
}

extern bool ddsts_new_map_unbound(ddsts_context_t *context, ddsts_type_t *key_type, ddsts_type_t *value_type, ddsts_type_t **result)
{
  return new_map(context, key_type, value_type, 0UL, result);
}

struct ddsts_scoped_name {
  ddsts_identifier_t name;
  bool top;
  ddsts_scoped_name_t *next;
};

extern bool ddsts_new_scoped_name(ddsts_context_t *context, ddsts_scoped_name_t* prev, bool top, ddsts_identifier_t name, ddsts_scoped_name_t **result)
{
  assert(context != NULL);
  assert(context->dangling_identifier == name);
  ddsts_scoped_name_t *scoped_name = (ddsts_scoped_name_t*)ddsrt_malloc(sizeof(ddsts_scoped_name_t));
  if (scoped_name == NULL) {
    context->retcode = DDS_RETCODE_OUT_OF_RESOURCES;
    return false;
  }
  context->dangling_identifier = NULL;
  scoped_name->name = name;
  scoped_name->top = top;
  scoped_name->next = NULL;
  if (prev == NULL) {
    *result = scoped_name;
  }
  else {
    ddsts_scoped_name_t **ref_scoped_name = &prev->next;
    while (*ref_scoped_name != NULL) {
      ref_scoped_name = &(*ref_scoped_name)->next;
    }
    *ref_scoped_name = scoped_name;
    *result = prev;
  }
  return true;
}

static bool resolve_scoped_name(ddsts_context_t *context, ddsts_scoped_name_t *scoped_name, ddsts_type_t **result)
{
  assert(context != NULL);
  assert(scoped_name != NULL);
  ddsts_type_t *cur_type = scoped_name->top ? context->root_type : context->cur_type;
  for (; cur_type != NULL; cur_type = cur_type->type.parent) {
    ddsts_type_t *found_type = cur_type;
    ddsts_scoped_name_t *cur_scoped_name;
    for (cur_scoped_name = scoped_name; cur_scoped_name != NULL && found_type != NULL; cur_scoped_name = cur_scoped_name->next) {
      ddsts_type_t *child = NULL;
      if (DDSTS_IS_TYPE(found_type, DDSTS_MODULE)) {
        child = found_type->module.members.first;
      }
      else if (DDSTS_IS_TYPE(found_type, DDSTS_STRUCT)) {
        child = found_type->struct_def.members.first;
      }
      found_type = NULL;
      for (; child != NULL; child = child->type.next) {
        if (DDSTS_IS_DEFINITION(child) && strcmp(child->type.name, cur_scoped_name->name) == 0) {
          found_type = child;
          break;
        }
      }
    }
    if (found_type != NULL) {
      *result = found_type;
      return true;
    }
  }
  /* Could not resolve scoped name */
  //DDS_ERROR("Could not resolve scoped name\n");
  *result = NULL;
  return false;
}

extern void ddsts_free_scoped_name(ddsts_scoped_name_t *scoped_name)
{
  while (scoped_name != NULL) {
    ddsts_scoped_name_t *next = scoped_name->next;
    ddsrt_free(scoped_name->name);
    ddsrt_free(scoped_name);
    scoped_name = next;
  }
}

extern bool ddsts_get_type_from_scoped_name(ddsts_context_t *context, ddsts_scoped_name_t *scoped_name, ddsts_type_t **result)
{
  ddsts_type_t *definition = NULL;
  if (!resolve_scoped_name(context, scoped_name, &definition)) {
    ddsts_free_scoped_name(scoped_name);
    return false;
  }
  ddsts_free_scoped_name(scoped_name);
  *result = definition;
  return true;
}

extern bool ddsts_get_base_type_from_scoped_name(ddsts_context_t *context, ddsts_scoped_name_t *scoped_name, ddsts_flags_t *result)
{
  ddsts_type_t *type = NULL;
  if (!ddsts_get_type_from_scoped_name(context, scoped_name, &type)) {
    return false;
  }
  assert(type != NULL);
  if ((DDSTS_TYPE_OF(type) & DDSTS_BASIC_TYPES) == 0) {
    return false;
  }
  *result = DDSTS_TYPE_OF(type);
  return true;
}

static void scoped_name_dds_error(ddsts_scoped_name_t *scoped_name)
{
  if (scoped_name == NULL) {
    return;
  }
  bool collon = scoped_name->top;
  for (; scoped_name != NULL; scoped_name = scoped_name->next) {
    if (collon) {
      DDS_ERROR("::");
    }
    DDS_ERROR("%s", scoped_name->name);
    collon = true;
  }
}

static bool new_module_definition(ddsts_context_t *context, ddsts_identifier_t name, ddsts_type_t *parent, ddsts_type_t **result)
{
  assert(context != NULL);
  assert(context->dangling_identifier == name);
  ddsts_type_t *module;
  dds_return_t rc = ddsts_create_module(name, &module);
  if (rc != DDS_RETCODE_OK) {
    context->retcode = rc;
    return false;
  }
  context->dangling_identifier = NULL;
  ddsts_module_add_member(parent, module);
  *result = module;
  return true;
}

static bool context_push_annotations_stack(ddsts_context_t *context)
{
  annotations_stack_t *annotations_stack = (annotations_stack_t*)ddsrt_malloc(sizeof(annotations_stack_t));
  if (annotations_stack == NULL) {
    return false;
  }
  annotations_stack->annotations = NULL;
  annotations_stack->deeper = context->annotations_stack;
  context->annotations_stack = annotations_stack;
  return true;
}

static void context_pop_annotations_stack(ddsts_context_t *context)
{
  assert(context->annotations_stack != NULL);
  annotations_stack_t *annotations_stack = context->annotations_stack;
  context->annotations_stack = annotations_stack->deeper;
  assert(annotations_stack->annotations == NULL);
  ddsrt_free(annotations_stack);
}

extern ddsts_context_t* ddsts_create_context(void)
{
  ddsts_context_t *context = (ddsts_context_t*)ddsrt_malloc(sizeof(ddsts_context_t));
  if (context == NULL) {
    return NULL;
  }
  context->dangling_identifier = NULL;
  ddsts_type_t *module;
  if (!new_module_definition(context, NULL, NULL, &module)) {
    ddsrt_free(context);
    return NULL;
  }
  context->root_type = module;
  context->cur_type = context->root_type;
  context->union_case_labels = NULL;
  context->union_case_default_label = false;
  context->type_for_declarator = NULL;
  context->array_sizes = NULL;
  context->annotations_stack = NULL;
  if (!context_push_annotations_stack(context)) {
    ddsts_free_type(module);
    ddsrt_free(context);
    return NULL;
  }
  context->pragma_args = NULL;
  context->retcode = DDS_RETCODE_OK;
  context->semantic_error = false;
  return context;
}

extern void ddsts_context_set_retcode(ddsts_context_t *context, dds_return_t retcode)
{
  assert(context != NULL);
  context->retcode = retcode;
}

extern dds_return_t ddsts_context_get_retcode(ddsts_context_t* context)
{
  assert(context != NULL);
  if (context->retcode != DDS_RETCODE_OK) {
    return context->retcode;
  } else if (context->semantic_error) {
    return DDS_RETCODE_ERROR;
  }
  return DDS_RETCODE_OK;
}

extern ddsts_type_t* ddsts_context_take_root_type(ddsts_context_t *context)
{
  ddsts_type_t* result = context->root_type;
  context->root_type = NULL;
  return result;
}

static void ddsts_context_free_array_sizes(ddsts_context_t *context)
{
  while (context->array_sizes != NULL) {
    array_size_t *next = context->array_sizes->next;
    ddsrt_free(context->array_sizes);
    context->array_sizes = next;
  }
}

static void context_free_pragma_args(ddsts_context_t *context)
{
  pragma_arg_t *pragma_arg = context->pragma_args;
  while (pragma_arg != NULL) {
    pragma_arg_t *next = pragma_arg->next;
    ddsrt_free(pragma_arg->arg);
    ddsrt_free(pragma_arg);
    pragma_arg = next;
  }
  context->pragma_args = NULL;
}

static void context_free_annotations(ddsts_context_t *context)
{
  assert(context->annotations_stack != NULL);
  annotation_t *annotation = context->annotations_stack->annotations;
  while (annotation != NULL) {
    annotation_t *next = annotation->next;
    ddsts_free_scoped_name(annotation->scoped_name);
    ddsrt_free(annotation);
    annotation = next;
  }
  context->annotations_stack->annotations = NULL;
}

static void ddsts_context_close_member(ddsts_context_t *context)
{
  ddsts_free_type(context->type_for_declarator);
  context->type_for_declarator = NULL;
  ddsts_context_free_array_sizes(context);
}

extern void ddsts_free_context(ddsts_context_t *context)
{
  assert(context != NULL);
  ddsts_context_close_member(context);
  ddsts_free_union_case_labels(context->union_case_labels);
  ddsts_free_type(context->root_type);
  while (context->annotations_stack != NULL) {
    context_free_annotations(context);
    context_pop_annotations_stack(context);
  }
  context_free_pragma_args(context);
  ddsrt_free(context->dangling_identifier);
  ddsrt_free(context);
}

bool ddsts_context_copy_identifier(ddsts_context_t *context, ddsts_identifier_t source, ddsts_identifier_t *dest)
{
  assert(context != NULL && source != NULL);
  assert(context->dangling_identifier == NULL);
  context->dangling_identifier = *dest = ddsrt_strdup(source);
  if (context->dangling_identifier == NULL) {
    context->retcode = DDS_RETCODE_OUT_OF_RESOURCES;
    return false;
  }
  return true;
}

#if (!defined(NDEBUG))
static bool cur_scope_is_definition_type(ddsts_context_t *context, ddsts_flags_t flags)
{
  assert(context != NULL && context->cur_type != NULL);
  return DDSTS_IS_TYPE(context->cur_type, flags);
}
#endif

extern bool ddsts_module_open(ddsts_context_t *context, ddsts_identifier_t name)
{
  assert(cur_scope_is_definition_type(context, DDSTS_MODULE));
  ddsts_type_t *module;
  if (!new_module_definition(context, name, context->cur_type, &module)) {
    return false;
  }
  context->cur_type = module;
  return true;
}

extern void ddsts_module_close(ddsts_context_t *context)
{
  assert(cur_scope_is_definition_type(context, DDSTS_MODULE));
  assert(context->cur_type->type.parent != NULL);
  context->cur_type = context->cur_type->type.parent;
}

extern bool ddsts_add_struct_forward(ddsts_context_t *context, ddsts_identifier_t name)
{
  assert(cur_scope_is_definition_type(context, DDSTS_MODULE));
  assert(context->dangling_identifier == name);
  ddsts_type_t *forward_dcl;
  dds_return_t rc = ddsts_create_struct_forward_dcl(name, &forward_dcl);
  if (rc != DDS_RETCODE_OK) {
    context->retcode = rc;
    return false;
  }
  context->dangling_identifier = NULL;
  ddsts_module_add_member(context->cur_type, forward_dcl);
  return true;
}

extern bool ddsts_add_struct_open(ddsts_context_t *context, ddsts_identifier_t name)
{
  assert(   cur_scope_is_definition_type(context, DDSTS_MODULE)
         || cur_scope_is_definition_type(context, DDSTS_STRUCT));
  assert(context->dangling_identifier == name);
  ddsts_type_t *new_struct;
  dds_return_t rc = ddsts_create_struct(name, &new_struct);
  if (rc != DDS_RETCODE_OK) {
    context->retcode = rc;
    return false;
  }
  context->dangling_identifier = NULL;
  if (DDSTS_IS_TYPE(context->cur_type, DDSTS_MODULE)) {
    ddsts_module_add_member(context->cur_type, new_struct);
  }
  else {
    ddsts_struct_add_member(context->cur_type, new_struct);
  }
  context->cur_type = new_struct;
  if (!context_push_annotations_stack(context)) {
    return false;
  }
  return true;
}

extern bool ddsts_add_struct_extension_open(ddsts_context_t *context, ddsts_identifier_t name, ddsts_scoped_name_t *scoped_name)
{
  assert(cur_scope_is_definition_type(context, DDSTS_MODULE));
  assert(context->dangling_identifier == name);
  ddsts_type_t *new_struct;
  dds_return_t rc = ddsts_create_struct(name, &new_struct);
  if (rc != DDS_RETCODE_OK) {
    context->retcode = rc;
    return false;
  }
  context->dangling_identifier = NULL;
  ddsts_module_add_member(context->cur_type, new_struct);
  /* find super */
  ddsts_type_t *definition;
  if (!resolve_scoped_name(context, scoped_name, &definition)) {
    DDS_ERROR("Could not resolve scoped name\n");
    ddsts_free_scoped_name(scoped_name);
    return false;
  }
  ddsts_free_scoped_name(scoped_name);
  if (definition != NULL && DDSTS_IS_TYPE(definition, DDSTS_STRUCT)) {
    new_struct->struct_def.super = definition;
  }
  context->cur_type = new_struct;
  return true;
}

extern bool ddsts_add_struct_member(ddsts_context_t *context, ddsts_type_t **ref_type)
{
  assert(cur_scope_is_definition_type(context, DDSTS_STRUCT));
  ddsts_context_close_member(context);
  ddsts_type_t *type = *ref_type;
  if (DDSTS_IS_TYPE(type, DDSTS_FORWARD_STRUCT)) {
    if (type->forward.definition == NULL) {
      //DDS_ERROR("Cannot use forward struct as type for member declaration\n");
      context->semantic_error = true;
      return false;
    }
    type = type->forward.definition;
  }
  context->type_for_declarator = type;
  *ref_type = NULL;
  return true;
}

extern void ddsts_struct_member_close(ddsts_context_t *context)
{
  context_free_annotations(context);
}

extern void ddsts_struct_close(ddsts_context_t *context, ddsts_type_t **result)
{
  assert(cur_scope_is_definition_type(context, DDSTS_STRUCT));
  ddsts_context_close_member(context);
  *result = context->cur_type;
  context->cur_type = context->cur_type->type.parent;
  context_pop_annotations_stack(context);
}

extern void ddsts_struct_empty_close(ddsts_context_t *context, ddsts_type_t **result)
{
  assert(cur_scope_is_definition_type(context, DDSTS_STRUCT));
  *result = context->cur_type;
  context->cur_type = context->cur_type->type.parent;
}

extern bool ddsts_add_union_forward(ddsts_context_t *context, ddsts_identifier_t name)
{
  assert(cur_scope_is_definition_type(context, DDSTS_MODULE));
  assert(context->dangling_identifier == name);
  ddsts_type_t *forward_dcl;
  dds_return_t rc = ddsts_create_union_forward_dcl(name, &forward_dcl);
  if (rc != DDS_RETCODE_OK) {
    context->retcode = rc;
    return false;
  }
  context->dangling_identifier = NULL;
  ddsts_module_add_member(context->cur_type, forward_dcl);
  return true;
}

extern bool ddsts_add_union_open(ddsts_context_t *context, ddsts_identifier_t name)
{
  assert(   cur_scope_is_definition_type(context, DDSTS_MODULE)
         || cur_scope_is_definition_type(context, DDSTS_STRUCT));
  assert(context->dangling_identifier == name);
  context->dangling_identifier = NULL;
  ddsts_type_t *new_union;
  dds_return_t rc = ddsts_create_union(name, DDSTS_NOTYPE, &new_union);
  if (rc != DDS_RETCODE_OK) {
    context->retcode = rc;
    return false;
  }
  context->dangling_identifier = NULL;
  if (DDSTS_IS_TYPE(context->cur_type, DDSTS_MODULE)) {
    ddsts_module_add_member(context->cur_type, new_union);
  }
  else {
    ddsts_struct_add_member(context->cur_type, new_union);
  }
  context->cur_type = new_union;
  if (!context_push_annotations_stack(context)) {
    return false;
  }
  return true;
}

extern bool ddsts_union_set_switch_type(ddsts_context_t *context, ddsts_flags_t base_type)
{
  assert(cur_scope_is_definition_type(context, DDSTS_UNION));
  context->cur_type->union_def.switch_type = base_type;
  return true;
}

static const char* name_of_type(ddsts_flags_t flags)
{
  switch(flags) {
    case DDSTS_INT16:                       return "short"; break;
    case DDSTS_INT32:                       return "long"; break;
    case DDSTS_INT64:                       return "long long"; break;
    case DDSTS_INT16 | DDSTS_UNSIGNED:      return "unsigned short"; break;
    case DDSTS_INT32 | DDSTS_UNSIGNED:      return "unsigned long"; break;
    case DDSTS_INT64 | DDSTS_UNSIGNED:      return "unsigned long long"; break;
    case DDSTS_CHAR:                        return "char"; break;
    case DDSTS_CHAR | DDSTS_WIDE:           return "wide char"; break;
    case DDSTS_BOOLEAN:                     return "boolean"; break;
    case DDSTS_OCTET:                       return "octet"; break;
    case DDSTS_INT8:                        return "int8"; break;
    case DDSTS_INT8 | DDSTS_UNSIGNED:       return "uint8"; break;
    case DDSTS_FLOAT:                       return "float"; break;
    case DDSTS_DOUBLE:                      return "double"; break;
    case DDSTS_LONGDOUBLE:                  return "long double"; break;
    case DDSTS_FIXED_PT_CONST:              return "fixed point const"; break;
    case DDSTS_ANY:                         return "any";
    case DDSTS_SEQUENCE:                    return "sequence";
    case DDSTS_ARRAY:                       return "array";
    case DDSTS_STRING:                      return "string";
    case DDSTS_STRING | DDSTS_WIDE:         return "wide string";
    case DDSTS_FIXED_PT:                    return "fixed point";
    case DDSTS_MAP:                         return "map";
    case DDSTS_MODULE:                      return "module";
    case DDSTS_FORWARD_STRUCT:              return "forward struct";
    case DDSTS_STRUCT:                      return "struct";
    case DDSTS_FORWARD_UNION:               return "forward union";
    case DDSTS_UNION:                       return "union";
    default:
      assert(0);
      break;
  }
  return "";
}

static dds_return_t cast_value_to_type(ddsts_literal_t *value, ddsts_flags_t flags, ddsts_literal_t *cast_type)
{
  switch (value->flags) {
    case DDSTS_INT64 | DDSTS_UNSIGNED:
      switch (flags) {
        case DDSTS_OCTET:
        case DDSTS_INT8 | DDSTS_UNSIGNED:
        case DDSTS_INT16 | DDSTS_UNSIGNED:
        case DDSTS_INT32 | DDSTS_UNSIGNED:
        case DDSTS_INT64 | DDSTS_UNSIGNED:
          cast_type->value.ullng = value->value.ullng;
          cast_type->flags = flags;
          return DDS_RETCODE_OK;
        case DDSTS_INT8:
        case DDSTS_INT16:
        case DDSTS_INT32:
          cast_type->value.llng = (signed long long)value->value.ullng;
          cast_type->flags = flags;
          return DDS_RETCODE_OK;
        default:
          DDS_ERROR("Cannot cast unsigned long long to %s\n", name_of_type(flags));
          return DDS_RETCODE_ERROR;
          break;
      }
      break;
    default:
      assert(0);
      break;
  }
  return DDS_RETCODE_ERROR;
}

static bool equal_values(ddsts_literal_t *a, ddsts_literal_t *b)
{
  assert(a->flags == b->flags);
  switch (a->flags) {
    case DDSTS_OCTET:
    case DDSTS_INT8 | DDSTS_UNSIGNED:
    case DDSTS_INT16 | DDSTS_UNSIGNED:
    case DDSTS_INT32 | DDSTS_UNSIGNED:
    case DDSTS_INT64 | DDSTS_UNSIGNED:
      return a->value.ullng == b->value.ullng;
      break;
    case DDSTS_INT8:
    case DDSTS_INT16:
    case DDSTS_INT32:
      return a->value.llng == b->value.llng;
      break;
    default:
      assert(0);
      break;
  }
  return false;
}

extern dds_return_t ddsts_union_add_case_label(ddsts_context_t *context, ddsts_literal_t *value)
{
  assert(cur_scope_is_definition_type(context, DDSTS_UNION));
  ddsts_literal_t casted_value;
  dds_return_t rc = cast_value_to_type(value, context->cur_type->union_def.switch_type, &casted_value);
  if (rc != DDS_RETCODE_OK) {
    return context->retcode = rc;
  }
  for (ddsts_type_t *cases = context->cur_type->union_def.cases.first; cases != NULL; cases = cases->type.next) {
    for (ddsts_union_case_label_t *label = cases->union_case.labels; label != NULL; label = label->next) {
      if (equal_values(&label->value, &casted_value)) {
        DDS_ERROR("Same case label used in more than one case.\n");
        return context->retcode = DDS_RETCODE_ERROR;
      }
    }
  } 
  if (context->union_case_default_label) {
    DDS_WARNING("Case label in combination with default is not useful.\n");
  }
  ddsts_union_case_label_t **ref_label = &context->union_case_labels;
  while (*ref_label != NULL) {
    if (equal_values(&(*ref_label)->value, &casted_value)) {
      DDS_WARNING("Label value in case is repeated. It is ignored.\n");
      return DDS_RETCODE_OK;
    }
    ref_label = &(*ref_label)->next;
  }
  *ref_label = (ddsts_union_case_label_t*)ddsrt_malloc(sizeof(ddsts_union_case_label_t));
  if (*ref_label == NULL) {
    return context->retcode = DDS_RETCODE_OUT_OF_RESOURCES;
  }
  (*ref_label)->value = casted_value;
  (*ref_label)->next = NULL;
  return DDS_RETCODE_OK;
}

extern bool ddsts_union_add_case_default(ddsts_context_t *context)
{
  for (ddsts_type_t *cases = context->cur_type->union_def.cases.first; cases != NULL; cases = cases->type.next) {
    if (cases->union_case.default_label) {
      DDS_ERROR("More than one default case is not allowed.\n");
      context->retcode = DDS_RETCODE_ERROR;
      return false;
    }
  }
  if (context->union_case_default_label) {
    DDS_WARNING("Default is repeated.\n");
  }
  if (context->union_case_labels != NULL) {
    DDS_WARNING("Case label in combination with default is not usefull.\n");
  }
  context->union_case_default_label = true;
  return true;
}

extern bool ddsts_union_add_element(ddsts_context_t *context, ddsts_type_t **type)
{
  assert(cur_scope_is_definition_type(context, DDSTS_UNION));
  ddsts_type_t *union_case;
  dds_return_t rc = ddsts_union_add_case(context->cur_type, context->union_case_labels, context->union_case_default_label, &union_case);
  if (rc != DDS_RETCODE_OK) {
    ddsts_free_union_case_labels(context->union_case_labels);
    context->union_case_labels = NULL;
    context->union_case_default_label = false;
    return false;
  }
  context->type_for_declarator = *type;
  *type = NULL;
  context->cur_type = union_case;
  context->union_case_labels = NULL;
  context->union_case_default_label = false;
  return true;
}

extern void ddsts_union_close(ddsts_context_t *context)
{
  assert(cur_scope_is_definition_type(context, DDSTS_UNION));
  context->cur_type = context->cur_type->type.parent;
  context_pop_annotations_stack(context);
}

static ddsts_type_t *create_array_type(array_size_t *array_size, ddsts_type_t *type)
{
  if (array_size == NULL) {
    return type;
  }
  ddsts_type_t *array;
  dds_return_t rc = ddsts_create_array(NULL, array_size->size, &array);
  if (rc != DDS_RETCODE_OK) {
    return NULL;
  }
  ddsts_type_t *element_type = create_array_type(array_size->next, type);
  if (element_type == NULL) {
    ddsts_free_type(array);
    return NULL;
  }
  ddsts_array_set_element_type(array, element_type);
  return array;
}

static bool keyable_type(ddsts_type_t *type)
{
  bool is_array = false;
  while (type != NULL && DDSTS_IS_TYPE(type, DDSTS_ARRAY)) {
    type = type->array.element_type;
    is_array = true;
  }
  if (type == NULL) {
    return false;
  }
  if (   DDSTS_IS_TYPE(type,
                       DDSTS_INT16 | DDSTS_INT32 | DDSTS_INT64 |
                       DDSTS_CHAR | DDSTS_BOOLEAN | DDSTS_OCTET |
                       DDSTS_INT8 | DDSTS_FLOAT | DDSTS_DOUBLE)
      || (DDSTS_IS_TYPE(type, DDSTS_STRING) && !is_array)) {
    return true;
  }
  if (DDSTS_IS_TYPE(type, DDSTS_STRUCT)) {
    if (type->struct_def.keys == NULL) {
      /* All fields should be keyable */
      for (ddsts_type_t *member = type->struct_def.members.first; member != NULL; member = member->type.next) {
        if (DDSTS_IS_TYPE(member, DDSTS_DECLARATION)) {
          if (!keyable_type(member->declaration.decl_type)) {
            return false;
          }
        }
      }
    }
    return true;
  }
  return false;
}

extern dds_return_t ddsts_add_declarator(ddsts_context_t *context, ddsts_identifier_t name)
{
  assert(context != NULL);
  assert(context->dangling_identifier == name);
  if (DDSTS_IS_TYPE(context->cur_type, DDSTS_STRUCT)) {
    assert(context->type_for_declarator != NULL);
    ddsts_type_t *decl = NULL;
    dds_return_t rc;
    rc = ddsts_create_declaration(name, NULL, &decl);
    if (rc != DDS_RETCODE_OK) {
      ddsts_context_free_array_sizes(context);
      return context->retcode = rc;
    }
    context->dangling_identifier = NULL;
    ddsts_type_t* type = create_array_type(context->array_sizes, context->type_for_declarator);
    if (type == NULL) {
      ddsts_context_free_array_sizes(context);
      ddsts_free_type(decl);
      return context->retcode = DDS_RETCODE_OUT_OF_RESOURCES;
    }
    ddsts_context_free_array_sizes(context);
    ddsts_declaration_set_type(decl, type);
    ddsts_struct_add_member(context->cur_type, decl);
    /* Process annotations */
    assert(context->annotations_stack != NULL);
    for (annotation_t *annotation = context->annotations_stack->annotations; annotation != NULL; annotation = annotation->next) {
      if (   annotation->scoped_name != NULL
          && !annotation->scoped_name->top
          && strcmp(annotation->scoped_name->name, "key") == 0
          && annotation->scoped_name->next == NULL) {
        if (keyable_type(type)) {
          rc = ddsts_struct_add_key(context->cur_type, decl);
          if (rc == DDS_RETCODE_ERROR) {
            /* FIXME: does not return an error, so warning instead? */
            DDS_ERROR("Field '%s' already defined as key\n", name);
            context->semantic_error = true;
            return DDS_RETCODE_ERROR;
          }
          else if (rc != DDS_RETCODE_OK) {
            ddsts_context_free_array_sizes(context);
            return context->retcode = rc;
          }
        }
        else {
          DDS_ERROR("Type of '%s' is not valid for key\n", name);
          context->semantic_error = true;
          return DDS_RETCODE_ERROR;
        }
      }
      else {
        DDS_ERROR("Unsupported annotation '");
        scoped_name_dds_error(annotation->scoped_name);
        DDS_ERROR("' is ignored\n");
        return DDS_RETCODE_ERROR;
      }
    }

    return DDS_RETCODE_OK;
  }
  else if (DDSTS_IS_TYPE(context->cur_type, DDSTS_UNION_CASE)) {
    assert(context->type_for_declarator != NULL);
    ddsts_type_t* type = create_array_type(context->array_sizes, context->type_for_declarator);
    if (type == NULL) {
      ddsts_context_free_array_sizes(context);
      return context->retcode = DDS_RETCODE_OUT_OF_RESOURCES;
    }
    ddsts_context_free_array_sizes(context);
    context->dangling_identifier = NULL;
    ddsts_union_case_set_decl(context->cur_type, name, type);
    context->cur_type = context->cur_type->type.parent;
    return DDS_RETCODE_OK;
  }
  assert(false);
  return DDS_RETCODE_ERROR;
}

extern bool ddsts_add_array_size(ddsts_context_t *context, ddsts_literal_t *value)
{
  assert(context != NULL);
  assert(value->flags == (DDSTS_INT64 | DDSTS_UNSIGNED));
  array_size_t **ref_array_size = &context->array_sizes;
  while (*ref_array_size != NULL) {
    ref_array_size = &(*ref_array_size)->next;
  }
  *ref_array_size = (array_size_t*)ddsrt_malloc(sizeof(array_size_t));
  if (*ref_array_size == NULL) {
    context->retcode = DDS_RETCODE_OUT_OF_RESOURCES;
    return false;
  }
  (*ref_array_size)->size = value->value.ullng;
  (*ref_array_size)->next = NULL;
  return true;
}

bool ddsts_add_annotation(ddsts_context_t *context, ddsts_scoped_name_t *scoped_name)
{
  assert(context != NULL);
  assert(context->annotations_stack != NULL);

  annotation_t **ref_annotation = &context->annotations_stack->annotations;
  while (*ref_annotation != NULL) {
    ref_annotation = &(*ref_annotation)->next;
  }
  (*ref_annotation) = (annotation_t*)ddsrt_malloc(sizeof(annotation_t));
  if ((*ref_annotation) == NULL) {
    context->retcode = DDS_RETCODE_OUT_OF_RESOURCES;
    return false;
  }
  context->dangling_identifier = NULL;
  (*ref_annotation)->scoped_name = scoped_name;
  (*ref_annotation)->next = NULL;
  return true;
}

void ddsts_pragma_open(ddsts_context_t *context)
{
  assert(context != NULL);
  assert(context->pragma_args == NULL);
  DDSRT_UNUSED_ARG(context);
}

bool ddsts_pragma_add_identifier(ddsts_context_t *context, ddsts_identifier_t name)
{
  ddsts_identifier_t copy;
  assert(context != NULL);

  pragma_arg_t **ref_pragma_arg = &context->pragma_args;
  while (*ref_pragma_arg != NULL) {
    ref_pragma_arg = &(*ref_pragma_arg)->next;
  }
  copy = ddsrt_strdup(name);
  if (copy == NULL) {
    context->retcode = DDS_RETCODE_OUT_OF_RESOURCES;
    return false;
  }
  (*ref_pragma_arg) = (pragma_arg_t*)ddsrt_malloc(sizeof(pragma_arg_t));
  if ((*ref_pragma_arg) == NULL) {
    ddsrt_free(copy);
    context->retcode = DDS_RETCODE_OUT_OF_RESOURCES;
    return false;
  }
  context->dangling_identifier = NULL;
  (*ref_pragma_arg)->arg = copy;
  (*ref_pragma_arg)->next = NULL;
  return true;
}

dds_return_t ddsts_pragma_close(ddsts_context_t *context)
{
  assert(context != NULL);
  assert(context->cur_type != NULL);

  pragma_arg_t *pragma_arg = context->pragma_args;

  /* Find struct in currect context */
  ddsts_type_t *member = NULL;
  if (DDSTS_IS_TYPE(context->cur_type, DDSTS_MODULE)) {
    member = context->cur_type->module.members.first;
  } else if (DDSTS_IS_TYPE(context->cur_type, DDSTS_STRUCT)) {
    member = context->cur_type->struct_def.members.first;
  }

  ddsts_type_t *struct_def = NULL;
  for (; member != NULL; member = member->type.next) {
    if (DDSTS_IS_TYPE(member, DDSTS_STRUCT) && strcmp(pragma_arg->arg, member->type.name) == 0) {
      struct_def = member;
      break;
    }
  }
  if (struct_def == NULL) {
    DDS_ERROR("Struct '%s' for keylist pragma is undefined here\n", pragma_arg->arg);
    context->semantic_error = true;
    context_free_pragma_args(context);
    return DDS_RETCODE_ERROR;
  }
  /* The '@key' and '#pragma keylist' may not be mixed */
  if (struct_def->struct_def.keys != NULL) {
    DDS_ERROR("Cannot use keylist pragma for struct '%s' in combination with @key annotation\n", pragma_arg->arg);
    context->semantic_error = true;
    context_free_pragma_args(context);
    return DDS_RETCODE_ERROR;
  }

  for (pragma_arg = pragma_arg->next; pragma_arg != NULL; pragma_arg = pragma_arg->next) {
    /* Find declarator in struct */
    ddsts_type_t *declaration = NULL;
    for (ddsts_type_t *m = struct_def->struct_def.members.first; m != NULL && declaration == NULL; m = m->type.next) {
      if (DDSTS_IS_TYPE(m, DDSTS_DECLARATION) && strcmp(m->type.name, pragma_arg->arg) == 0) {
        declaration = m;
        break;
      }
    }
    if (declaration == NULL) {
      DDS_ERROR("Member '%s' in struct '%s' for keylist pragma is undefined\n", pragma_arg->arg, struct_def->type.name);
      context->semantic_error = true;
      return DDS_RETCODE_ERROR;
    }
    else if (keyable_type(declaration->declaration.decl_type)) {
      dds_return_t rc = ddsts_struct_add_key(struct_def, declaration);
      if (rc == DDS_RETCODE_ERROR) {
        DDS_ERROR("Field '%s' already defined as key\n", pragma_arg->arg);
        context->semantic_error = true;
        return rc;
      }
      else if (rc != DDS_RETCODE_OK) {
        context->retcode = rc;
        context_free_pragma_args(context);
        return rc;
      }
    }
    else {
      DDS_ERROR("Type of '%s' is not valid for key\n", pragma_arg->arg);
      context->semantic_error = true;
      return DDS_RETCODE_ERROR;
    }
  }

  context_free_pragma_args(context);
  return DDS_RETCODE_OK;
}

extern void ddsts_accept(ddsts_context_t *context)
{
  assert(context != NULL);
  context->retcode = context->semantic_error ? DDS_RETCODE_ERROR : DDS_RETCODE_OK;
}
