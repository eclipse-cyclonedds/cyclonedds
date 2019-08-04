/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
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
#include <ctype.h>
#include <assert.h>
#include <stdarg.h>
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/strtol.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsc/dds_public_impl.h"
#include "dds/ddsts/typetree.h"
#include "dds/ddsts/type_walk.h"
#include "gen_ostream.h"
#include "gen_c99.h"

/* Some administration used during the traverse process */

/* For each sequence of struct in IDL, a separate struct is defined in the C header,
 * which uses the struct definition generated for the struct (from IDL). In case
 * for this IDL struct no struct has been generated in the C header, a forward
 * declaration (with a typedef) needs to be generated. For this reason we need
 * to track for which IDL structs a forward definition and/or a struct declaration
 * have occured.
 */

typedef struct struct_def_used struct_def_used_t;
struct struct_def_used {
  ddsts_type_t *struct_def;
  bool as_forward;
  bool as_definition;
  struct_def_used_t *next;
};

typedef struct {
  ddsts_ostream_t *ostream;
} output_context_t;

typedef struct {
  output_context_t output_context;
  struct_def_used_t *struct_def_used;
} gen_header_context_t;

static void gen_header_context_init(gen_header_context_t *gen_header_context, ddsts_ostream_t *ostream)
{
  gen_header_context->output_context.ostream = ostream;
  gen_header_context->struct_def_used = NULL;
}

static void gen_header_context_fini(gen_header_context_t *gen_header_context)
{
  struct_def_used_t *struct_def_used;
  for (struct_def_used = gen_header_context->struct_def_used; struct_def_used != NULL;) {
    struct_def_used_t *next = struct_def_used->next;
    ddsrt_free(struct_def_used);
    struct_def_used = next;
  }
}

static struct_def_used_t *find_struct_def_used(gen_header_context_t *gen_header_context, ddsts_type_t *struct_def)
{
  struct_def_used_t *struct_def_used;
  for (struct_def_used = gen_header_context->struct_def_used; struct_def_used != NULL; struct_def_used = struct_def_used->next)
    if (struct_def_used->struct_def == struct_def)
      return struct_def_used;
  struct_def_used = (struct_def_used_t*)ddsrt_malloc(sizeof(struct_def_used_t));
  if (struct_def_used == NULL) {
    return NULL;
  }
  struct_def_used->struct_def = struct_def;
  struct_def_used->as_forward = false;
  struct_def_used->as_definition = false;
  struct_def_used->next = gen_header_context->struct_def_used;
  gen_header_context->struct_def_used = struct_def_used;
  return struct_def_used;
}

/* Included modules and structs */

/* Given a struct, find all modules and structs that need to be included because
 * they are used (recursively)
 */

typedef struct included_type included_type_t;
struct included_type {
  ddsts_type_t *type;
  included_type_t *next;
};

static dds_return_t included_types_add(included_type_t **ref_included_nodes, ddsts_type_t *type)
{
  included_type_t *new_included_node = (included_type_t*)ddsrt_malloc(sizeof(included_type_t));
  if (new_included_node == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  new_included_node->type = type;
  new_included_node->next = *ref_included_nodes;
  *ref_included_nodes = new_included_node;
  return DDS_RETCODE_OK;
}

static void free_included_types(included_type_t *included_types)
{
  while (included_types != NULL) {
    included_type_t *next = included_types->next;
    ddsrt_free((void*)included_types);
    included_types = next;
  }
}

static bool included_types_contains(included_type_t *included_types, ddsts_type_t *type)
{
  for (; included_types != NULL; included_types = included_types->next)
    if (included_types->type == type)
      return true;
  return false;
}

static ddsts_type_t *get_struct_def(ddsts_type_t *type)
{
  return DDSTS_IS_TYPE(type, DDSTS_FORWARD_STRUCT) ? type->forward.definition : type;
}

static dds_return_t find_used_structs(included_type_t **ref_included_nodes, ddsts_type_t *struct_def);

static dds_return_t find_used_structs_in_type_def(included_type_t **ref_included_nodes, ddsts_type_t *type)
{
  switch (DDSTS_TYPE_OF(type)) {
    case DDSTS_SEQUENCE:
      return find_used_structs_in_type_def(ref_included_nodes, type->sequence.element_type);
      break;
    case DDSTS_STRUCT:
    case DDSTS_FORWARD_STRUCT: {
      ddsts_type_t *struct_def = get_struct_def(type);
      if (struct_def != NULL)
        return find_used_structs(ref_included_nodes, struct_def);
      break;
    }
    default:
      break;
  }
  return DDS_RETCODE_OK;
}

static dds_return_t find_used_structs(included_type_t **ref_included_nodes, ddsts_type_t *struct_def)
{
  if (included_types_contains(*ref_included_nodes, struct_def))
    return DDS_RETCODE_OK;
  dds_return_t rc = included_types_add(ref_included_nodes, struct_def);
  if (rc != DDS_RETCODE_OK) {
    return rc;
  }

  /* include modules in which this struct occurs */
  ddsts_type_t *type = struct_def;
  for (type = type->type.parent; type != NULL && type->type.parent != NULL ; type = type->type.parent) {
    if (included_types_contains(*ref_included_nodes, type))
      break;
    rc = included_types_add(ref_included_nodes, type);
    if (rc != DDS_RETCODE_OK) {
      return rc;
    }
  }

  ddsts_type_t *member;
  for (member = struct_def->struct_def.members.first; member != NULL; member = member->type.next) {
    if (DDSTS_IS_TYPE(member, DDSTS_DECLARATION)) {
      rc = find_used_structs_in_type_def(ref_included_nodes, member->declaration.decl_type);
      if (rc != DDS_RETCODE_OK) {
        return rc;
      }
    }
  }

  return DDS_RETCODE_OK;
}

/* Finding specific parent of a type */

static ddsts_type_t *ddsts_type_get_parent(ddsts_type_t *type, ddsts_flags_t flags)
{
  while (type != NULL) {
    type = type->type.parent;
    if (type != NULL && DDSTS_IS_TYPE(type, flags)) {
      return type;
    }
  }
  return NULL;
}

/* Output function with named string arguments */

static void output(ddsts_ostream_t *ostream, const char *fmt, ...)
{
  const char *values[26];
  for (int i = 0; i < 26; i++) {
    values[i] = "";
  }

  /* Parse variable arguments: pairs of a capital letter and a string */
  va_list args;
  va_start(args, fmt);
  for (;;) {
    char letter = (char)va_arg(args, int);
    if (letter < 'A' || letter > 'Z') {
      break;
    }
    values[letter - 'A'] = va_arg(args, const char*);
  }
  va_end(args);

  const char *s;
  for (s = fmt; *s != '\0'; s++) {
    if (*s == '$') {
      s++;
      if (*s == '$')
        ddsts_ostream_put(ostream, *s);
      else if ('A' <= *s && *s <= 'Z') {
        const char *t;
        for (t = values[*s - 'A']; *t != '\0'; t++) {
          ddsts_ostream_put(ostream, *t);
        }
      }
    }
    else {
      ddsts_ostream_put(ostream, *s);
    }
  }
}


/* File name functions */

static dds_return_t output_file_name(const char *file_name, const char *ext, const char **ref_result)
{
  *ref_result = NULL;
  size_t file_name_len = strlen(file_name);
  if (file_name_len < 4 || strcmp(file_name + file_name_len - 4, ".idl") != 0) {
    DDS_ERROR("Error: File name '%s' does not have '.idl' extension", file_name);
    return DDS_RETCODE_ERROR;
  }
  size_t result_len = file_name_len - 2 + strlen(ext);
  char *result = (char*)ddsrt_malloc(sizeof(char)*(result_len));
  if (result == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  ddsrt_strlcpy(result, file_name, file_name_len - 2);
  ddsrt_strlcat(result, ext, result_len);
  *ref_result = result;
  return DDS_RETCODE_OK;
}

static dds_return_t uppercase_file_name(const char *file_name, const char **ref_result)
{
  *ref_result = NULL;
  size_t file_name_len = strlen(file_name);
  if (file_name_len < 4 || strcmp(file_name + file_name_len - 4, ".idl") != 0) {
    DDS_ERROR("Error: File name '%s' does not have '.idl' extension", file_name);
    return DDS_RETCODE_ERROR;
  }
  char *result = (char*)ddsrt_malloc(sizeof(char)*(file_name_len - 3));
  if (result == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  size_t i;
  for (i = 0; i < file_name_len - 4; i++)
    result[i] = (char)toupper(file_name[i]);
  result[i] = '\0';
  *ref_result = result;
  return DDS_RETCODE_OK;
}

/* Generate file header */

static void write_file_header(ddsts_ostream_t *ostream, const char *target_file_name, const char *source_file_name)
{
  dds_time_t time_now = dds_time();
  char time_descr[DDSRT_RFC3339STRLEN+1];
  ddsrt_ctime(time_now, time_descr, DDSRT_RFC3339STRLEN);

  output(ostream,
         "/****************************************************************\n"
         "\n"
         "  Generated by Eclipse Cyclone DDS IDL to C Translator\n"
         "  File name: $T\n"
         "  Source: $S\n"
         "  Generated: $D\n"
         "  Eclipse Cyclone DDS: V0.1.0\n"
         "\n"
         "*****************************************************************/\n"
         "\n",
         'T', target_file_name,
         'S', source_file_name,
         'D', time_descr,
         '\0');
}

/* Generate C99 header file */

static dds_return_t write_header_intro(ddsts_ostream_t *ostream, const char *file_name)
{
  const char *uc_file_name = NULL;
  dds_return_t rc = uppercase_file_name(file_name, &uc_file_name);
  if (rc != DDS_RETCODE_OK) {
    return rc;
  }

  output(ostream,
          "#include \"ddsc/dds_public_impl.h\"\n"
         "\n"
         "#ifndef _DDSL_$U_H_\n"
         "#define _DDSL_$U_H_\n"
         "\n"
         "\n"
         "#ifdef __cplusplus\n"
         "extern \"C\" {\n"
         "#endif\n"
         "\n",
         'U', uc_file_name,
         '\0');

  ddsrt_free((void*)uc_file_name);

  return DDS_RETCODE_OK;
}

static dds_return_t write_header_close(ddsts_ostream_t *ostream, const char *file_name)
{
  const char *uc_file_name = NULL;
  dds_return_t rc = uppercase_file_name(file_name, &uc_file_name);
  if (rc != DDS_RETCODE_OK) {
    return rc;
  }

  output(ostream,
         "#ifdef __cplusplus\n"
         "}\n"
         "#endif\n"
         "#endif /* _DDSL_$U_H_ */\n",
         'U', uc_file_name,
         '\0');

  ddsrt_free((void*)uc_file_name);

  return DDS_RETCODE_OK;
}

static const char *name_with_module_prefix(ddsts_type_t *def_type, const char *infix)
{
  size_t infix_len = strlen(infix);
  size_t len = 0;
  ddsts_type_t *cur = def_type;
  while (cur != NULL) {
    len += strlen(cur->type.name);
    if (   cur->type.parent != NULL
        && DDSTS_IS_TYPE(cur->type.parent, DDSTS_MODULE | DDSTS_STRUCT)
        && cur->type.parent->type.parent != NULL) {
      cur = cur->type.parent;
      len += infix_len;
    }
    else
      cur = NULL;
  }
  char *result = (char*)ddsrt_malloc(sizeof(char)*(len+1));
  if (result == NULL) {
    return NULL;
  }
  result[len] = '\0';
  for (cur = def_type; cur != NULL;) {
    size_t cur_name_len = strlen(cur->type.name);
    len -= cur_name_len;
    size_t i;
    for (i = 0; i < cur_name_len; i++) {
       result[len + i] = cur->type.name[i];
    }
    if (   cur->type.parent != NULL
        && DDSTS_IS_TYPE(cur->type.parent, DDSTS_MODULE | DDSTS_STRUCT)
        && cur->type.parent->type.parent != NULL) {
      cur = cur->type.parent;
      len -= infix_len;
      for (i = 0; i < infix_len; i++) {
        result[len + i] = infix[i];
      }
    }
    else {
      cur = NULL;
    }
  }
  return result;
}

/* Functions called from walker for the include file */

static dds_return_t write_header_forward_struct(ddsts_call_path_t *path, void *context)
{
  ddsts_type_t *struct_def = NULL;
  if (DDSTS_IS_TYPE(path->type, DDSTS_STRUCT | DDSTS_FORWARD_STRUCT)) {
    struct_def = get_struct_def(path->type);
  }
  if (struct_def == NULL) {
    DDS_ERROR("no struct definition");
    return DDS_RETCODE_ERROR;
  }
  struct_def_used_t *struct_def_used = find_struct_def_used((gen_header_context_t*)context, struct_def);
  if (struct_def_used == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  if (struct_def_used->as_forward || struct_def_used->as_definition) {
    return DDS_RETCODE_OK;
  }
  struct_def_used->as_forward = true;

  const char *full_name = name_with_module_prefix(struct_def, "_");
  if (full_name == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }

  output(((output_context_t*)context)->ostream,
         "typedef struct $N $N;\n"
         "\n",
         'N', full_name,
         '\0');

  ddsrt_free((void*)full_name);

  return DDS_RETCODE_OK;
}

static dds_return_t write_header_seq_struct_with_name(ddsts_type_t *type, const char *element_type_name, ddsts_ostream_t *ostream)
{
  const char *base_name = name_with_module_prefix(type, "_");
  if (base_name == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }

  output(ostream,
         "typedef struct $B_seq\n"
         "{\n"
         "  uint32_t _maximum;\n"
         "  uint32_t _length;\n"
         "  $E *_buffer;\n"
         "  bool _release;\n"
         "} $B_seq;\n"
         "\n"
         "#define $B_seq__alloc() \\\n"
         "(($B_seq*) dds_alloc (sizeof ($B_seq)));\n"
         "\n"
         "#define $B_seq_allocbuf(l) \\\n"
         "(($E *) dds_alloc ((l) * sizeof ($E)))\n"
         "\n",
         'B', base_name,
         'E', element_type_name,
         '\0');

  ddsrt_free((void*)base_name);
  return DDS_RETCODE_OK;
}

static dds_return_t write_header_open_struct(ddsts_call_path_t *path, void *context)
{
  assert(DDSTS_IS_TYPE(path->type, DDSTS_STRUCT));

  struct_def_used_t *struct_def_used = find_struct_def_used((gen_header_context_t*)context, path->type);
  if (struct_def_used == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  struct_def_used->as_definition = true;

  const char *full_name = name_with_module_prefix(path->type, "_");
  if (full_name == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }

  output(((output_context_t*)context)->ostream,
         "typedef struct $N\n"
         "{\n",
         'N', full_name,
         '\0');

  ddsrt_free((void*)full_name);

  return DDS_RETCODE_OK;
}

static dds_return_t write_header_close_struct(ddsts_call_path_t *path, void *context)
{
  DDSRT_UNUSED_ARG(context);

  assert(DDSTS_IS_TYPE(path->type, DDSTS_STRUCT));
  ddsts_type_t *struct_def = path->type;

  const char *full_name = name_with_module_prefix(struct_def, "_");
  if (full_name == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }

  output(((output_context_t*)context)->ostream,
         "} $N;\n"
         "\n",
         'N', full_name,
         '\0');
  if (struct_def->struct_def.keys != NULL) {
    output(((output_context_t*)context)->ostream,
           "extern const dds_topic_descriptor_t $N_desc;\n"
           "\n"
           "#define $N__alloc() \\\n"
           "(($N*) dds_alloc (sizeof ($N)));\n"
           "\n"
           "#define $N_free(d,o) \\\n"
           "dds_sample_free ((d), &$N_desc, (o))\n",
           'N', full_name,
           '\0');
  }
  output(((output_context_t*)context)->ostream, "\n", '\0');

  ddsrt_free((void*)full_name);

  return DDS_RETCODE_OK;
}


static dds_return_t write_header_struct_member(ddsts_call_path_t *path, void *context)
{
  DDSRT_UNUSED_ARG(context);

  ddsts_ostream_puts(((output_context_t*)context)->ostream, "  ");

  assert(DDSTS_IS_TYPE(path->type, DDSTS_DECLARATION));

  const char *decl_name = path->type->type.name;
  ddsts_type_t *type = path->type->declaration.decl_type;
  while (DDSTS_IS_TYPE(type, DDSTS_ARRAY)) {
    type = type->array.element_type;
    assert(type != NULL);
  }

  switch (DDSTS_TYPE_OF(type)) {
    case DDSTS_SHORT:      output(((output_context_t*)context)->ostream, "int16_t $D", 'D', decl_name, '\0'); break;
    case DDSTS_LONG:       output(((output_context_t*)context)->ostream, "int32_t $D", 'D', decl_name, '\0'); break;
    case DDSTS_LONGLONG:   output(((output_context_t*)context)->ostream, "int64_t $D", 'D', decl_name, '\0'); break;
    case DDSTS_USHORT:     output(((output_context_t*)context)->ostream, "uint16_t $D", 'D', decl_name, '\0'); break;
    case DDSTS_ULONG:      output(((output_context_t*)context)->ostream, "uint32_t $D", 'D', decl_name, '\0'); break;
    case DDSTS_ULONGLONG:  output(((output_context_t*)context)->ostream, "uint64_t $D", 'D', decl_name, '\0'); break;
    case DDSTS_CHAR:       output(((output_context_t*)context)->ostream, "char $D", 'D', decl_name, '\0'); break;
    case DDSTS_BOOLEAN:    output(((output_context_t*)context)->ostream, "bool $D", 'D', decl_name, '\0'); break;
    case DDSTS_OCTET:      output(((output_context_t*)context)->ostream, "uint8_t $D", 'D', decl_name, '\0'); break;
    case DDSTS_INT8:       output(((output_context_t*)context)->ostream, "int8_t $D", 'D', decl_name, '\0'); break;
    case DDSTS_UINT8:      output(((output_context_t*)context)->ostream, "uint8_t $D", 'D', decl_name, '\0'); break;
    case DDSTS_FLOAT:      output(((output_context_t*)context)->ostream, "float $D", 'D', decl_name, '\0'); break;
    case DDSTS_DOUBLE:     output(((output_context_t*)context)->ostream, "double $D", 'D', decl_name, '\0'); break;
    case DDSTS_STRING: {
      if (type->string.max > 0) {
        char size_string[30];
        ddsrt_ulltostr(type->string.max + 1, size_string, 30, NULL);
        output(((output_context_t*)context)->ostream, "char $D[$S]", 'D', decl_name, 'S', size_string, '\0');
      }
      else {
        output(((output_context_t*)context)->ostream, "char * $D", 'D', decl_name, '\0');
      }
      break;
    }
    case DDSTS_SEQUENCE: {
      if (DDSTS_IS_TYPE(type->sequence.element_type, DDSTS_STRUCT | DDSTS_FORWARD_STRUCT | DDSTS_SEQUENCE)) {
        ddsts_type_t *struct_node = ddsts_type_get_parent(type, DDSTS_STRUCT);
        if (struct_node == NULL) {
          assert(false);
          return DDS_RETCODE_ERROR;
        }
        const char *full_name = name_with_module_prefix(struct_node, "_");
        if (full_name == NULL) {
          return DDS_RETCODE_OUT_OF_RESOURCES;
        }
        output(((output_context_t*)context)->ostream, "$N_$D_seq $D", 'N', full_name, 'D', decl_name, '\0');
        ddsrt_free((void*)full_name);
      }
      else {
        output(((output_context_t*)context)->ostream, "dds_sequence_t $D", 'D', decl_name, '\0');
      }
      break;
    }
    case DDSTS_STRUCT:
    case DDSTS_FORWARD_STRUCT: {
      ddsts_type_t *struct_def = get_struct_def(type);
      if (struct_def != NULL) {
        const char *struct_name = name_with_module_prefix(struct_def, "_");
        if (struct_name == NULL) {
          return DDS_RETCODE_OUT_OF_RESOURCES;
        }
        output(((output_context_t*)context)->ostream, "$S $D", 'S', struct_name, 'D', decl_name, '\0');
        ddsrt_free((void*)struct_name);
      }
      break;
    }
    default:
      output(((output_context_t*)context)->ostream, "// type not supported: $D", 'D', decl_name, '\0');
      break;
  }

  type = path->type->declaration.decl_type;
  while (DDSTS_IS_TYPE(type, DDSTS_ARRAY)) {
    char size_string[30];
    ddsrt_ulltostr(type->array.size, size_string, 30, NULL);
    output(((output_context_t*)context)->ostream, "[$S]", 'S', size_string, '\0');
    type = type->array.element_type;
    assert(type != NULL);
  }

  ddsts_ostream_puts(((output_context_t*)context)->ostream, ";\n");
  return DDS_RETCODE_OK;
}

static dds_return_t write_header_struct(ddsts_call_path_t *path, void *context);

static dds_return_t write_header_struct_pre(ddsts_call_path_t *path, void *context)
{
  switch (DDSTS_TYPE_OF(path->type)) {
    case DDSTS_STRUCT:
      return write_header_struct(path, context);
      break;
    case DDSTS_DECLARATION: {
      ddsts_type_t *type = path->type->declaration.decl_type;
      while (DDSTS_IS_TYPE(type, DDSTS_ARRAY)) {
        type = type->array.element_type;
      }
      if (DDSTS_IS_TYPE(type, DDSTS_SEQUENCE)) {
        ddsts_type_t *element_type = type->sequence.element_type;
        if (DDSTS_IS_TYPE(element_type, DDSTS_STRUCT | DDSTS_FORWARD_STRUCT)) {
          const char *element_type_name = name_with_module_prefix(element_type, "_");
          if (element_type_name == NULL) {
            return DDS_RETCODE_OUT_OF_RESOURCES;
          }
          ddsts_call_path_t elem_path;
          elem_path.call_parent = path;
          elem_path.type = element_type;
          dds_return_t rc;
          rc = write_header_forward_struct(&elem_path, context);
          if (rc != DDS_RETCODE_OK) {
            ddsrt_free((void*)element_type_name);
            return rc;
          }
          rc = write_header_seq_struct_with_name(path->type, element_type_name, ((output_context_t*)context)->ostream);
          ddsrt_free((void*)element_type_name);
          return rc;
        }
        else if (DDSTS_IS_TYPE(element_type, DDSTS_SEQUENCE)) {
          return write_header_seq_struct_with_name(path->type, "dds_sequence_t", ((output_context_t*)context)->ostream);
        }
      }
      break;
    }
    default:
      assert(false);
      break;
   }
   return DDS_RETCODE_OK;
}

static dds_return_t write_header_struct(ddsts_call_path_t *path, void *context)
{
  dds_return_t rc;
  rc = ddsts_walk(path, 0, DDSTS_STRUCT | DDSTS_DECLARATION, write_header_struct_pre, context);
  if (rc != DDS_RETCODE_OK) {
    return rc;
  }

  rc = write_header_open_struct(path, context);
  if (rc != DDS_RETCODE_OK) {
    return rc;
  }
  rc = ddsts_walk(path, 0, DDSTS_DECLARATION, write_header_struct_member, context);
  if (rc != DDS_RETCODE_OK) {
    return rc;
  }
  return write_header_close_struct(path, context);
}

static dds_return_t write_header_modules(ddsts_call_path_t *path, void *context)
{
  switch (DDSTS_TYPE_OF(path->type)) {
    case DDSTS_STRUCT:
      return write_header_struct(path, context);
      break;
    case DDSTS_FORWARD_STRUCT:
      return write_header_forward_struct(path, context);
      break;
    default:
      break;
  }
  assert(false);
  return DDS_RETCODE_ERROR;
}

static dds_return_t generate_header_file(const char *file_name, ddsts_type_t *root_node, ddsts_ostream_t *ostream)
{
  DDSRT_UNUSED_ARG(root_node);
  const char *h_file_name = NULL;
  dds_return_t rc;
  rc = output_file_name(file_name, "h", &h_file_name);
  if (rc != DDS_RETCODE_OK) {
    return rc;
  }
  if (!ddsts_ostream_open(ostream, h_file_name)) {
    DDS_ERROR("Could not open file '%s' for writing", h_file_name);
    ddsrt_free((void*)h_file_name);
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }

  write_file_header(ostream, h_file_name, file_name);
  rc = write_header_intro(ostream, file_name);
  if (rc != DDS_RETCODE_OK) {
    ddsts_ostream_close(ostream);
    ddsrt_free((void*)h_file_name);
    return rc;
  }

  gen_header_context_t context;
  gen_header_context_init(&context, ostream);

  ddsts_call_path_t path;
  path.call_parent = NULL;
  path.type = root_node;

  rc = ddsts_walk(&path, DDSTS_MODULE, DDSTS_STRUCT | DDSTS_FORWARD_STRUCT, write_header_modules, &context.output_context);
  if (rc != DDS_RETCODE_OK) {
    ddsts_ostream_close(ostream);
    ddsrt_free((void*)h_file_name);
    return rc;
  }

  rc = write_header_close(ostream, file_name);
  if (rc != DDS_RETCODE_OK) {
    ddsts_ostream_close(ostream);
    ddsrt_free((void*)h_file_name);
    return rc;
  }

  gen_header_context_fini(&context);

  ddsts_ostream_close(ostream);
  ddsrt_free((void*)h_file_name);

  return DDS_RETCODE_OK;
}

/* Generate source file */

/*   Context for storing sizes and positions for op codes */

typedef struct type_dim type_dim_t;
struct type_dim {
  ddsts_type_t *type;
  bool generated;
  uint32_t start;
  uint32_t size;
  type_dim_t *next;
};

typedef struct key_offset key_offset_t;
struct key_offset {
  ddsts_type_t *decl;
  uint32_t offset;
  key_offset_t *next;
};

typedef struct {
  output_context_t output_context;
  type_dim_t *struct_dims;
  key_offset_t *key_offsets;
  uint32_t cur_offset;
  uint32_t nr_ops;
  uint32_t nr_keys;
  uint32_t bounded_key_size;
  uint32_t key_size;
} op_codes_context_t;

static void op_codes_context_init(op_codes_context_t *op_codes_context, ddsts_ostream_t *ostream)
{
  op_codes_context->output_context.ostream = ostream;
  op_codes_context->struct_dims = NULL;
  op_codes_context->key_offsets = NULL;
  op_codes_context->cur_offset = 0U;
  op_codes_context->nr_ops = 0U;
  op_codes_context->nr_keys = 0U;
  op_codes_context->bounded_key_size = true;
  op_codes_context->key_size = 0U;
}

static void op_codes_context_reset(op_codes_context_t *op_codes_context, ddsts_ostream_t *ostream)
{
  op_codes_context->output_context.ostream = ostream;
  for (type_dim_t *type_dim = op_codes_context->struct_dims; type_dim != NULL; type_dim = type_dim->next) {
    type_dim->generated = false;
  }
  op_codes_context->cur_offset = 0U;
  op_codes_context->nr_ops = 0U;
}

static void op_codes_context_fini(op_codes_context_t *context)
{
  for (type_dim_t *type_dim = context->struct_dims; type_dim != NULL;) {
    type_dim_t *next = type_dim->next;
    ddsrt_free((void*)type_dim);
    type_dim = next;
  }
  for (key_offset_t *key_offset = context->key_offsets; key_offset != NULL;) {
    key_offset_t *next =key_offset->next;
    ddsrt_free((void*)key_offset);
    key_offset = next;
  }
}

static type_dim_t *op_codes_context_find(op_codes_context_t *context, ddsts_type_t *type)
{
  type_dim_t **ref_type_dim = &context->struct_dims;
  for (; *ref_type_dim != NULL; ref_type_dim = &(*ref_type_dim)->next) {
    if ((*ref_type_dim)->type == type) {
      return (*ref_type_dim);
    }
  }
  (*ref_type_dim) = (type_dim_t*)ddsrt_malloc(sizeof(type_dim_t));
  if ((*ref_type_dim) == NULL) {
    return NULL;
  }
  (*ref_type_dim)->type = type;
  (*ref_type_dim)->generated = false;
  (*ref_type_dim)->start = 0U;
  (*ref_type_dim)->size = 0U;
  (*ref_type_dim)->next = NULL;
  return (*ref_type_dim);
}

static key_offset_t *op_codes_context_find_key(op_codes_context_t *context, ddsts_type_t *decl)
{
  key_offset_t **ref_key_offset = &context->key_offsets;
  for (; *ref_key_offset != NULL; ref_key_offset = &(*ref_key_offset)->next) {
    if ((*ref_key_offset)->decl == decl) {
      return (*ref_key_offset);
    }
  }
  (*ref_key_offset) = (key_offset_t*)ddsrt_malloc(sizeof(key_offset_t));
  if ((*ref_key_offset) == NULL) {
    return NULL;
  }
  (*ref_key_offset)->decl = decl;
  (*ref_key_offset)->offset = 0U;
  (*ref_key_offset)->next = NULL;
  return (*ref_key_offset);
}

/*   Generate operation codes */

static dds_return_t generate_op_codes_field_name(ddsts_call_path_t *path, bool top, ddsts_ostream_t *ostream)
{
  if (   DDSTS_IS_TYPE(path->type, DDSTS_STRUCT | DDSTS_FORWARD_STRUCT)
      && !DDSTS_IS_TYPE(path->call_parent->type, DDSTS_DECLARATION)) {
    const char *full_name = name_with_module_prefix(path->type, "_");
    if (full_name == NULL) {
      return DDS_RETCODE_OUT_OF_RESOURCES;
    }
    ddsts_ostream_puts(ostream, full_name);
    ddsts_ostream_puts(ostream, ", ");
    ddsrt_free((void*)full_name);
  }
  else {
    if (DDSTS_IS_TYPE(path->type, DDSTS_DECLARATION)) {
      dds_return_t rc = generate_op_codes_field_name(path->call_parent, false, ostream);
      if (rc != DDS_RETCODE_OK) {
        return rc;
      }
      ddsts_ostream_puts(ostream, path->type->type.name);
      if (!top) {
        ddsts_ostream_put(ostream, '.');
      }
    }
    else {
      return generate_op_codes_field_name(path->call_parent, top, ostream);
    }
  }
  return DDS_RETCODE_OK;
}

static dds_return_t generate_op_codes_offsetof(ddsts_call_path_t *declaration, void *context)
{
  ddsts_ostream_t *ostream = ((op_codes_context_t*)context)->output_context.ostream;

  if (declaration == 0 || !DDSTS_IS_TYPE(declaration->type, DDSTS_DECLARATION)) {
    ddsts_ostream_puts(ostream, " 0u,");
  }
  else {
    ddsts_ostream_puts(ostream, " offsetof (");
    dds_return_t rc = generate_op_codes_field_name(declaration, true, ostream);
    if (rc != DDS_RETCODE_OK) {
      return rc;
    }
    ddsts_ostream_puts(ostream, "),");
  }
  ((op_codes_context_t*)context)->cur_offset += 1U;
  return DDS_RETCODE_OK;
}

static void generate_op_codes_array_size(unsigned long long array_size, void *context)
{
  char array_size_string[30];
  ddsrt_ulltostr(array_size & 0xffffffff, array_size_string, 30, NULL);
  output(((op_codes_context_t*)context)->output_context.ostream, " $N,", 'N', array_size_string, '\0');
  ((op_codes_context_t*)context)->cur_offset += 1U;
}

static dds_return_t generate_op_codes_simple_type(ddsts_call_path_t *declaration, uint32_t dds_op_type, unsigned long long array_size, void *context)
{
  ddsts_ostream_t *ostream = ((op_codes_context_t*)context)->output_context.ostream;
  ddsts_ostream_puts(ostream, "  DDS_OP_ADR");

  if (array_size > 0ULL) {
    ddsts_ostream_puts(ostream, " | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_");
  }
  else {
    ddsts_ostream_puts(ostream, " | DDS_OP_TYPE_");
  }
  switch (dds_op_type) {
    case DDS_OP_TYPE_1BY: ddsts_ostream_puts(ostream, "1BY"); break;
    case DDS_OP_TYPE_2BY: ddsts_ostream_puts(ostream, "2BY"); break;
    case DDS_OP_TYPE_4BY: ddsts_ostream_puts(ostream, "4BY"); break;
    case DDS_OP_TYPE_8BY: ddsts_ostream_puts(ostream, "8BY"); break;
    case DDS_OP_TYPE_STR: ddsts_ostream_puts(ostream, "STR"); break;
    default:
      assert(false);
      return DDS_RETCODE_ERROR;
      break;
  }
  bool declaration_is_key = false;
  dds_return_t rc;
  (void)ddsts_declaration_is_key(declaration, &declaration_is_key);

  if (declaration_is_key) {
    ddsts_ostream_puts(ostream, " | DDS_OP_FLAG_KEY");
    key_offset_t *key_offset = op_codes_context_find_key((op_codes_context_t*)context, declaration->type);
    if (key_offset == NULL) {
      return DDS_RETCODE_OUT_OF_RESOURCES;
    }
    key_offset->offset = ((op_codes_context_t*)context)->cur_offset;
  }
  ddsts_ostream_puts(ostream, ",");
  ((op_codes_context_t*)context)->cur_offset += 1U;
  ((op_codes_context_t*)context)->nr_ops += 1U;

  rc = generate_op_codes_offsetof(declaration, context);
  if (rc != DDS_RETCODE_OK) {
    return rc;
  }

  if (array_size > 0ULL) {
    generate_op_codes_array_size(array_size, context);
  }

  ddsts_ostream_puts(ostream, "\n");

  return DDS_RETCODE_OK;
}

static dds_return_t generate_op_codes_sequence(ddsts_call_path_t *declaration, uint32_t dds_op_type, void *context)
{
  ddsts_ostream_t *ostream = ((op_codes_context_t*)context)->output_context.ostream;
  ddsts_ostream_puts(ostream, "  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_");

  switch (dds_op_type) {
    case DDS_OP_TYPE_1BY: ddsts_ostream_puts(ostream, "1BY"); break;
    case DDS_OP_TYPE_2BY: ddsts_ostream_puts(ostream, "2BY"); break;
    case DDS_OP_TYPE_4BY: ddsts_ostream_puts(ostream, "4BY"); break;
    case DDS_OP_TYPE_8BY: ddsts_ostream_puts(ostream, "8BY"); break;
    case DDS_OP_TYPE_STR: ddsts_ostream_puts(ostream, "STR"); break;
    case DDS_OP_TYPE_SEQ: ddsts_ostream_puts(ostream, "SEQ"); break;
    case DDS_OP_TYPE_STU: ddsts_ostream_puts(ostream, "STU"); break;
    default:
      assert(false);
      return DDS_RETCODE_ERROR;
      break;
  }
  ddsts_ostream_puts(ostream, ",");
  ((op_codes_context_t*)context)->cur_offset += 1U;
  ((op_codes_context_t*)context)->nr_ops += 1U;

  dds_return_t rc = generate_op_codes_offsetof(declaration, context);
  if (rc != DDS_RETCODE_OK) {
    return rc;
  }
  ddsts_ostream_puts(ostream, "\n");
  return DDS_RETCODE_OK;
}

static dds_return_t generate_op_codes_sequence_struct(ddsts_call_path_t *declaration, uint32_t dds_op_type, const char* struct_name, uint32_t op_code_size, void *context)
{
  dds_return_t rc;
  rc = generate_op_codes_sequence(declaration, dds_op_type, context);
  if (rc != DDS_RETCODE_OK) {
    return rc;
  }
  char size_string[30];
  ddsrt_ulltostr(op_code_size + 4, size_string, 30, NULL);
  output(((op_codes_context_t*)context)->output_context.ostream,
        "  sizeof ($T), ($Su << 16u) + 4u,\n",
        'T', struct_name,
        'S', size_string, '\0');
  ((op_codes_context_t*)context)->cur_offset += 2U;
  return DDS_RETCODE_OK;
}

static dds_return_t generate_op_codes_struct(ddsts_call_path_t *path, void *context);

static void generate_op_codes_generated_struct(type_dim_t *type_dim, void *context)
{
  char jump_string[30];
  ddsrt_ulltostr((unsigned long long)(((op_codes_context_t*)context)->cur_offset - type_dim->start), jump_string, 30, NULL);
  output(((op_codes_context_t*)context)->output_context.ostream,
        "  DDS_OP_JSR, (uint32_t)-$J,\n"
        "  DDS_OP_RTS,\n",
        'J', jump_string,
        '\0');
  ((op_codes_context_t*)context)->cur_offset += 3U;
  ((op_codes_context_t*)context)->nr_ops += 2U;
}

static dds_return_t generate_op_codes_seq_element(ddsts_call_path_t *path, ddsts_call_path_t *declaration, void *context)
{
  ddsts_ostream_t *ostream = ((op_codes_context_t*)context)->output_context.ostream;
  ddsts_type_t *type = path->type;

  switch (DDSTS_TYPE_OF(type)) {
    case DDSTS_SHORT:     return generate_op_codes_sequence(declaration, DDS_OP_TYPE_2BY, context); break;
    case DDSTS_LONG:      return generate_op_codes_sequence(declaration, DDS_OP_TYPE_4BY, context); break;
    case DDSTS_LONGLONG:  return generate_op_codes_sequence(declaration, DDS_OP_TYPE_8BY, context); break;
    case DDSTS_USHORT:    return generate_op_codes_sequence(declaration, DDS_OP_TYPE_2BY, context); break;
    case DDSTS_ULONG:     return generate_op_codes_sequence(declaration, DDS_OP_TYPE_4BY, context); break;
    case DDSTS_ULONGLONG: return generate_op_codes_sequence(declaration, DDS_OP_TYPE_8BY, context); break;
    case DDSTS_CHAR:      return generate_op_codes_sequence(declaration, DDS_OP_TYPE_1BY, context); break;
    case DDSTS_BOOLEAN:   return generate_op_codes_sequence(declaration, DDS_OP_TYPE_1BY, context); break;
    case DDSTS_OCTET:     return generate_op_codes_sequence(declaration, DDS_OP_TYPE_1BY, context); break;
    case DDSTS_INT8:      return generate_op_codes_sequence(declaration, DDS_OP_TYPE_1BY, context); break;
    case DDSTS_UINT8:     return generate_op_codes_sequence(declaration, DDS_OP_TYPE_1BY, context); break;
    case DDSTS_FLOAT:     return generate_op_codes_sequence(declaration, DDS_OP_TYPE_4BY, context); break;
    case DDSTS_DOUBLE:    return generate_op_codes_sequence(declaration, DDS_OP_TYPE_8BY, context); break;
    case DDSTS_STRING: {
      if (type->string.max > 0) {
        ddsts_ostream_puts(ostream, "  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_BST,");
        ((op_codes_context_t*)context)->cur_offset += 1U;
        ((op_codes_context_t*)context)->nr_ops += 1U;
        dds_return_t rc = generate_op_codes_offsetof(declaration, context);
        if (rc != DDS_RETCODE_OK) {
          return rc;
        }
        char size_string[30];
        ddsrt_ulltostr(type->string.max + 1, size_string, 30, NULL);
        output(ostream, "\n  $S,\n", 'S', size_string, '\0');
        ((op_codes_context_t*)context)->cur_offset += 1U;
      }
      else {
        return generate_op_codes_sequence(declaration, DDS_OP_TYPE_STR, context);
      }
      break;
    }
    case DDSTS_ARRAY: {
      assert(false);
      return DDS_RETCODE_ERROR;
      break;
    }
    case DDSTS_SEQUENCE: {
      type_dim_t *type_dim = op_codes_context_find((op_codes_context_t*)context, type);
      if (type_dim == NULL) {
        return DDS_RETCODE_OUT_OF_RESOURCES;
      }
      dds_return_t rc;
      rc = generate_op_codes_sequence_struct(declaration, DDS_OP_TYPE_SEQ, "dds_sequence_t", type_dim->size, context);
      if (rc != DDS_RETCODE_OK) {
        return rc;
      }
      uint32_t offset = ((op_codes_context_t*)context)->cur_offset;
      ddsts_call_path_t type_spec_path;
      type_spec_path.type = type->sequence.element_type;
      type_spec_path.call_parent = path;
      rc = generate_op_codes_seq_element(&type_spec_path, NULL, context);
      if (rc != DDS_RETCODE_OK) {
        return rc;
      }
      ddsts_ostream_puts(ostream, "  DDS_OP_RTS,\n");
      ((op_codes_context_t*)context)->cur_offset += 1U;
      ((op_codes_context_t*)context)->nr_ops += 1U;
      type_dim->size = ((op_codes_context_t*)context)->cur_offset - offset;
      break;
    }
    case DDSTS_STRUCT:
    case DDSTS_FORWARD_STRUCT: {
      ddsts_type_t *struct_def = get_struct_def(type);
      if (struct_def == NULL) {
        DDS_ERROR("forward definition '%s' has no definition", path->type->type.name);
        return DDS_RETCODE_ERROR;
      }
      const char *struct_full_name = name_with_module_prefix(struct_def, "_");
      if (struct_full_name == NULL) {
        return DDS_RETCODE_OUT_OF_RESOURCES;
      }
      type_dim_t *type_dim = op_codes_context_find((op_codes_context_t*)context, struct_def);
      if (type_dim == NULL) {
        ddsrt_free((void*)struct_full_name);
        return DDS_RETCODE_OUT_OF_RESOURCES;
      }
      if (type_dim->generated) {
        dds_return_t rc = generate_op_codes_sequence_struct(declaration, DDS_OP_TYPE_STU, struct_full_name, 3, context);
        if (rc != DDS_RETCODE_OK) {
          ddsrt_free((void*)struct_full_name);
          return rc;
        }
        generate_op_codes_generated_struct(type_dim, context);
      }
      else {
        dds_return_t rc;
        rc = generate_op_codes_sequence_struct(declaration, DDS_OP_TYPE_STU, struct_full_name, type_dim->size, context);
        if (rc != DDS_RETCODE_OK) {
          ddsrt_free((void*)struct_full_name);
          return rc;
        }
        ddsts_call_path_t struct_path;
        struct_path.type = struct_def;
        struct_path.call_parent = path;
        rc = generate_op_codes_struct(&struct_path, context);
        if (rc != DDS_RETCODE_OK) {
          ddsrt_free((void*)struct_full_name);
          return rc;
        }
        ddsts_ostream_puts(((op_codes_context_t*)context)->output_context.ostream, ",\n");
      }
      ddsrt_free((void*)struct_full_name);
      break;
    }
    default:
      assert(false);
      return DDS_RETCODE_ERROR;
      break;
  }
  return DDS_RETCODE_OK;
}

static dds_return_t generate_op_codes_array_struct(ddsts_call_path_t *declaration, uint32_t dds_op_type, unsigned long long array_size, uint32_t op_code_size, void *context)
{
  ddsts_ostream_t *ostream = ((op_codes_context_t*)context)->output_context.ostream;
  ddsts_ostream_puts(ostream, "  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_");
  switch (dds_op_type) {
    case DDS_OP_TYPE_SEQ: ddsts_ostream_puts(ostream, "SEQ"); break;
    case DDS_OP_TYPE_STU: ddsts_ostream_puts(ostream, "STU"); break;
    default:
      assert(false);
      return DDS_RETCODE_ERROR;
      break;
  }
  ddsts_ostream_puts(ostream, ",");
  ((op_codes_context_t*)context)->cur_offset += 1U;
  ((op_codes_context_t*)context)->nr_ops += 1U;
  dds_return_t rc;
  rc = generate_op_codes_offsetof(declaration, context);
  if (rc != DDS_RETCODE_OK) {
    return rc;
  }
  generate_op_codes_array_size(array_size, context);
  ddsts_ostream_puts(ostream, "\n");
  char size_string[30];
  ddsrt_ulltostr(op_code_size + 5, size_string, 30, NULL);
  output(ostream, "  ($Su << 16u) + 5u,", 'S', size_string, '\0');
  ((op_codes_context_t*)context)->cur_offset += 1U;
  return DDS_RETCODE_OK;
}

static dds_return_t generate_op_codes_declaration(ddsts_call_path_t *path, void *context)
{
  op_codes_context_t *op_codes_context = (op_codes_context_t*)context;
  ddsts_ostream_t *ostream = op_codes_context->output_context.ostream;

  assert(DDSTS_IS_TYPE(path->type, DDSTS_DECLARATION));

  ddsts_call_path_t *declaration = path;

  ddsts_type_t *type = path->type->declaration.decl_type;

  if (DDSTS_IS_TYPE(type, DDSTS_STRUCT | DDSTS_FORWARD_STRUCT)) {
    ddsts_type_t *struct_def = get_struct_def(type);
    if (struct_def == NULL) {
      return DDS_RETCODE_ERROR;
    }
    /* embedded struct */
    ddsts_call_path_t struct_path;
    struct_path.type = struct_def;
    struct_path.call_parent = path;
    dds_return_t rc = ddsts_walk(&struct_path, 0, DDSTS_DECLARATION, generate_op_codes_declaration, context);
    if (rc != DDS_RETCODE_OK) {
      return rc;
    }
  }
  else {
    /* Check if we have a (simple) array type. If so, array_size will be multiplied size. If not, array_size is zero. */
    unsigned long long array_size = 0ULL;
    if (DDSTS_IS_TYPE(type, DDSTS_ARRAY)) {
      ddsts_type_t *element_type = type;
      array_size = 1ULL;
      while (DDSTS_IS_TYPE(element_type, DDSTS_ARRAY)) {
        array_size *= element_type->array.size;
        element_type = element_type->array.element_type;
      }
      if (DDSTS_IS_TYPE(element_type,
                        DDSTS_SHORT | DDSTS_LONG | DDSTS_LONGLONG |
                        DDSTS_USHORT | DDSTS_ULONG | DDSTS_ULONGLONG |
                        DDSTS_CHAR | DDSTS_BOOLEAN | DDSTS_OCTET | DDSTS_UINT8 |
                        DDSTS_FLOAT | DDSTS_DOUBLE | DDSTS_SEQUENCE | DDSTS_STRING |
                        DDSTS_STRUCT | DDSTS_FORWARD_STRUCT)) {
        type = element_type;
      }
      else {
        /* This cannot be handled as simple array */
        array_size = 0ULL;
      }
    }
    switch (DDSTS_TYPE_OF(type)) {
      case DDSTS_SHORT:     return generate_op_codes_simple_type(declaration, DDS_OP_TYPE_2BY, array_size, context); break;
      case DDSTS_LONG:      return generate_op_codes_simple_type(declaration, DDS_OP_TYPE_4BY, array_size, context); break;
      case DDSTS_LONGLONG:  return generate_op_codes_simple_type(declaration, DDS_OP_TYPE_8BY, array_size, context); break;
      case DDSTS_USHORT:    return generate_op_codes_simple_type(declaration, DDS_OP_TYPE_2BY, array_size, context); break;
      case DDSTS_ULONG:     return generate_op_codes_simple_type(declaration, DDS_OP_TYPE_4BY, array_size, context); break;
      case DDSTS_ULONGLONG: return generate_op_codes_simple_type(declaration, DDS_OP_TYPE_8BY, array_size, context); break;
      case DDSTS_CHAR:      return generate_op_codes_simple_type(declaration, DDS_OP_TYPE_1BY, array_size, context); break;
      case DDSTS_BOOLEAN:   return generate_op_codes_simple_type(declaration, DDS_OP_TYPE_1BY, array_size, context); break;
      case DDSTS_OCTET:     return generate_op_codes_simple_type(declaration, DDS_OP_TYPE_1BY, array_size, context); break;
      case DDSTS_INT8:      return generate_op_codes_simple_type(declaration, DDS_OP_TYPE_1BY, array_size, context); break;
      case DDSTS_UINT8:     return generate_op_codes_simple_type(declaration, DDS_OP_TYPE_1BY, array_size, context); break;
      case DDSTS_FLOAT:     return generate_op_codes_simple_type(declaration, DDS_OP_TYPE_4BY, array_size, context); break;
      case DDSTS_DOUBLE:    return generate_op_codes_simple_type(declaration, DDS_OP_TYPE_8BY, array_size, context); break;
      case DDSTS_STRING:
        if (type->string.max > 0ULL) {
          if (array_size == 0ULL) {
            ddsts_ostream_puts(ostream, "  DDS_OP_ADR | DDS_OP_TYPE_BST,");
          }
          else {
            ddsts_ostream_puts(ostream, "  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_BST,");
          }
          op_codes_context->cur_offset += 1U;
          op_codes_context->nr_ops += 1U;
          dds_return_t rc = generate_op_codes_offsetof(declaration, context);
          if (rc != DDS_RETCODE_OK) {
            return rc;
          }
          if (array_size > 0ULL) {
            generate_op_codes_array_size(array_size, context);
            ddsts_ostream_puts(ostream, "\n  0,");
            op_codes_context->cur_offset += 1U;
          }
          char size_string[30];
          ddsrt_ulltostr(type->string.max + 1, size_string, 30, NULL);
          output(ostream, " $S,\n", 'S', size_string, '\0');
          op_codes_context->cur_offset += 1U;
        }
        else {
          return generate_op_codes_simple_type(declaration, DDS_OP_TYPE_STR, array_size, context);
        }
        break;
      case DDSTS_SEQUENCE: {
        if (array_size == 0ULL) {
          ddsts_call_path_t type_spec_path;
          type_spec_path.type = type->sequence.element_type;
          type_spec_path.call_parent = path;
          return generate_op_codes_seq_element(&type_spec_path, declaration, context);
        }
        else {
          type_dim_t *type_dim = op_codes_context_find((op_codes_context_t*)context, type);
          if (type_dim == NULL) {
            return DDS_RETCODE_OUT_OF_RESOURCES;
          }
          dds_return_t rc;
          rc = generate_op_codes_array_struct(declaration, DDS_OP_TYPE_SEQ, array_size, type_dim->size, context);
          if (rc != DDS_RETCODE_OK) {
            return rc;
          }
          declaration = NULL;
          if (DDSTS_IS_TYPE(type->sequence.element_type, DDSTS_STRUCT | DDSTS_FORWARD_STRUCT | DDSTS_SEQUENCE)) {
            ddsts_type_t *struct_node = ddsts_type_get_parent(path->type, DDSTS_STRUCT);
            if (struct_node == NULL) {
              assert(false);
              return DDS_RETCODE_ERROR;
            }
            const char *full_name = name_with_module_prefix(struct_node, "_");
            if (full_name == NULL) {
              return DDS_RETCODE_OUT_OF_RESOURCES;
            }
            output(ostream, " sizeof ($N_$D_seq),\n", 'N', full_name, 'D', path->type->type.name, '\0');
            ((op_codes_context_t*)context)->cur_offset += 1U;
            ddsrt_free((void*)full_name);
          }
          else {
            output(ostream, " sizeof (dds_sequence_t),\n", '\0');
            ((op_codes_context_t*)context)->cur_offset += 1U;
          }
          uint32_t offset = ((op_codes_context_t*)context)->cur_offset;

          ddsts_call_path_t type_spec_path;
          type_spec_path.type = type->sequence.element_type;
          type_spec_path.call_parent = path;
          rc = generate_op_codes_seq_element(&type_spec_path, declaration, context);
          if (rc != DDS_RETCODE_OK) {
            return rc;
          }
          ddsts_ostream_puts(ostream, "  DDS_OP_RTS,\n");
          op_codes_context->cur_offset += 1U;
          op_codes_context->nr_ops += 1U;
          type_dim->size = ((op_codes_context_t*)context)->cur_offset - offset;
        }
        break;
      }
      case DDSTS_STRUCT:
      case DDSTS_FORWARD_STRUCT: {
        ddsts_type_t *struct_def = get_struct_def(type);
        if (struct_def == NULL) {
          return DDS_RETCODE_ERROR;
        }
        assert(array_size > 0ULL);
        type_dim_t *type_dim = op_codes_context_find((op_codes_context_t*)context, struct_def);
        if (type_dim == NULL) {
          return DDS_RETCODE_OUT_OF_RESOURCES;
        }
        const char *struct_full_name = name_with_module_prefix(struct_def, "_");
        if (struct_full_name == NULL) {
          return DDS_RETCODE_OUT_OF_RESOURCES;
        }
        if (type_dim->generated) {
          dds_return_t rc = generate_op_codes_array_struct(declaration, DDS_OP_TYPE_STU, array_size, 3, context);
          if (rc != DDS_RETCODE_OK) {
            ddsrt_free((void*)struct_full_name);
            return rc;
          }
          output(((op_codes_context_t*)context)->output_context.ostream, " sizeof ($T),\n", 'T', struct_full_name, '\0');
          ((op_codes_context_t*)context)->cur_offset += 1U;
          generate_op_codes_generated_struct(type_dim, context);
        }
        else {
          dds_return_t rc;
          rc = generate_op_codes_array_struct(declaration, DDS_OP_TYPE_STU, array_size, type_dim->size, context);
          if (rc != DDS_RETCODE_OK) {
            ddsrt_free((void*)struct_full_name);
            return rc;
          }
          output(((op_codes_context_t*)context)->output_context.ostream, " sizeof ($T),\n", 'T', struct_full_name, '\0');
          ((op_codes_context_t*)context)->cur_offset += 1U;
          ddsts_call_path_t array_path;
          array_path.type = path->type->declaration.decl_type;
          array_path.call_parent = path;
          ddsts_call_path_t struct_path;
          struct_path.type = struct_def;
          struct_path.call_parent = &array_path;
          rc = generate_op_codes_struct(&struct_path, context);
          if (rc != DDS_RETCODE_OK) {
            ddsrt_free((void*)struct_full_name);
            return rc;
          }
          ddsts_ostream_puts(((op_codes_context_t*)context)->output_context.ostream, ",\n");
        }
        ddsrt_free((void*)struct_full_name);
        break;
      }
      default:
        /* Type not supported. No op codes are generated */
        break;
    }
  }
  return DDS_RETCODE_OK;
}

static dds_return_t generate_op_codes_struct(ddsts_call_path_t *path, void *context)
{
  op_codes_context_t *op_codes_context = (op_codes_context_t*)context;

  assert(DDSTS_IS_TYPE(path->type, DDSTS_STRUCT));

  ddsts_type_t *struct_def = path->type;
  type_dim_t *type_dim = op_codes_context_find(op_codes_context, struct_def);
  if (type_dim == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  assert(!type_dim->generated);
  type_dim->generated = true;
  type_dim->start = op_codes_context->cur_offset;

  dds_return_t rc = ddsts_walk(path, 0, DDSTS_DECLARATION, generate_op_codes_declaration, context);
  if (rc != DDS_RETCODE_OK) {
    return rc;
  }

  ddsts_ostream_puts(op_codes_context->output_context.ostream, "  DDS_OP_RTS");
  op_codes_context->cur_offset += 1U;
  op_codes_context->nr_ops += 1U;

  type_dim->size = (uint32_t)(op_codes_context->cur_offset - type_dim->start);

  return DDS_RETCODE_OK;
}

/*   Functions for generating meta data */

typedef struct {
  output_context_t output_context;
  included_type_t *included_types;
} meta_data_context_t;

static void meta_data_context_init(meta_data_context_t *context, ddsts_ostream_t *ostream, included_type_t *included_types)
{
  context->output_context.ostream = ostream;
  context->included_types = included_types;
}

static dds_return_t write_meta_data_elem(ddsts_call_path_t *path, void *context);

static dds_return_t write_meta_data_type_spec(ddsts_call_path_t *path, void *context)
{
  ddsts_type_t *type = path->type;
  switch (DDSTS_TYPE_OF(type)) {
    case DDSTS_SHORT:      output(((output_context_t*)context)->ostream, "<Short/>", '\0'); break;
    case DDSTS_LONG:       output(((output_context_t*)context)->ostream, "<Long/>", '\0'); break;
    case DDSTS_LONGLONG:   output(((output_context_t*)context)->ostream, "<LongLong/>", '\0'); break;
    case DDSTS_USHORT:     output(((output_context_t*)context)->ostream, "<UShort/>", '\0'); break;
    case DDSTS_ULONG:      output(((output_context_t*)context)->ostream, "<ULong/>", '\0'); break;
    case DDSTS_ULONGLONG:  output(((output_context_t*)context)->ostream, "<ULongLong/>", '\0'); break;
    case DDSTS_CHAR:       output(((output_context_t*)context)->ostream, "<Char/>", '\0'); break;
    case DDSTS_BOOLEAN:    output(((output_context_t*)context)->ostream, "<Boolean/>", '\0'); break;
    case DDSTS_OCTET:      output(((output_context_t*)context)->ostream, "<Octet/>", '\0'); break;
    case DDSTS_INT8:       output(((output_context_t*)context)->ostream, "<Int8/>", '\0'); break;
    case DDSTS_UINT8:      output(((output_context_t*)context)->ostream, "<UInt8/>", '\0'); break;
    case DDSTS_FLOAT:      output(((output_context_t*)context)->ostream, "<Float/>", '\0'); break;
    case DDSTS_DOUBLE:     output(((output_context_t*)context)->ostream, "<Double/>", '\0'); break;
    case DDSTS_LONGDOUBLE: output(((output_context_t*)context)->ostream, "<LongDouble/>", '\0'); break;
    case DDSTS_STRING: {
      if (type->string.max > 0ULL) {
        char size_string[30];
        ddsrt_ulltostr(type->string.max, size_string, 30, NULL);
        output(((output_context_t*)context)->ostream, "<String length=\\\"$S\\\"/>", 'S', size_string, '\0');
      }
      else {
        output(((output_context_t*)context)->ostream, "<String/>", '\0');
      }
      break;
    }
    case DDSTS_ARRAY: {
      char size_string[30];
      ddsrt_ulltostr(type->array.size, size_string, 30, NULL);
      output(((output_context_t*)context)->ostream, "<Array size=\\\"$S\\\">", 'S', size_string, '\0');
      ddsts_call_path_t element_path;
      element_path.type = path->type->array.element_type;
      element_path.call_parent = path;
      dds_return_t rc = write_meta_data_type_spec(&element_path, context);
      if (rc != DDS_RETCODE_OK) {
        return rc;
      }
      ddsts_ostream_puts(((output_context_t*)context)->ostream, "</Array>");
      break;
    }
    case DDSTS_SEQUENCE: {
      if (type->sequence.max > 0ULL) {
        char size_string[30];
        ddsrt_ulltostr(type->sequence.max, size_string, 30, NULL);
        output(((output_context_t*)context)->ostream, "<Sequence size=\\\"$S\\\">", 'S', size_string, '\0');
      }
      else {
        ddsts_ostream_puts(((output_context_t*)context)->ostream, "<Sequence>");
      }
      ddsts_call_path_t element_path;
      element_path.type = path->type->sequence.element_type;
      element_path.call_parent = path;
      dds_return_t rc = write_meta_data_type_spec(&element_path, context);
      if (rc != DDS_RETCODE_OK) {
        return rc;
      }
      ddsts_ostream_puts(((output_context_t*)context)->ostream, "</Sequence>");
      break;
    }
    case DDSTS_STRUCT:
    case DDSTS_FORWARD_STRUCT:
      if (DDSTS_IS_TYPE(type, DDSTS_STRUCT) && type->type.parent != NULL && DDSTS_IS_TYPE(type->type.parent, DDSTS_STRUCT)) {
        /* embedded struct */
        write_meta_data_elem(path, context);
      }
      else {
        ddsts_type_t *struct_def = get_struct_def(type);
        if (struct_def != NULL) {
          bool in_context = false;
          for (ddsts_call_path_t *parent = path->call_parent; parent != NULL; parent = parent->call_parent) {
            if (parent->type == struct_def->type.parent) {
              in_context = true;
              break;
            }
          }
          if (in_context) {
            output(((output_context_t*)context)->ostream, "<Type name=\\\"$N\\\"/>", 'N', type->type.name, '\0');
          }
          else {
            const char *full_name = name_with_module_prefix(struct_def, "::");
            if (full_name == NULL) {
              return DDS_RETCODE_OUT_OF_RESOURCES;
            }
            output(((output_context_t*)context)->ostream, "<Type name=\\\"::$N\\\"/>", 'N', full_name, '\0');
            ddsrt_free((void*)full_name);
          }
        }
      }
      break;
    default:
      //assert(false);
      break;
  }
  return DDS_RETCODE_OK;
}

static dds_return_t write_meta_data_elem(ddsts_call_path_t *path, void *context)
{
  switch (DDSTS_TYPE_OF(path->type)) {
    case DDSTS_MODULE:
      if (included_types_contains(((meta_data_context_t*)context)->included_types, path->type)) {
        output(((output_context_t*)context)->ostream, "<Module name=\\\"$N\\\">", 'N', path->type->type.name, '\0');
        dds_return_t rc = ddsts_walk(path, 0, DDSTS_MODULE | DDSTS_STRUCT, write_meta_data_elem, context);
        if (rc != DDS_RETCODE_OK) {
          return rc;
        }
        ddsts_ostream_puts(((output_context_t*)context)->ostream, "</Module>");
      }
      break;
    case DDSTS_STRUCT:
      if (included_types_contains(((meta_data_context_t*)context)->included_types, path->type)) {
        output(((output_context_t*)context)->ostream, "<Struct name=\\\"$N\\\">", 'N', path->type->type.name, '\0');
        dds_return_t rc = ddsts_walk(path, 0, DDSTS_DECLARATION, write_meta_data_elem, context);
        if (rc != DDS_RETCODE_OK) {
          return rc;
        }
        ddsts_ostream_puts(((output_context_t*)context)->ostream, "</Struct>");
      }
      break;
    case DDSTS_DECLARATION: {
      output(((output_context_t*)context)->ostream, "<Member name=\\\"$N\\\">", 'N', path->type->type.name, '\0');
      ddsts_call_path_t type_spec_path;
      type_spec_path.type = path->type->declaration.decl_type;
      type_spec_path.call_parent = path;
      dds_return_t rc = write_meta_data_type_spec(&type_spec_path, context);
      if (rc != DDS_RETCODE_OK) {
        return rc;
      }
      ddsts_ostream_puts(((output_context_t*)context)->ostream, "</Member>");
      break;
    }
    default:
      assert(false);
      return DDS_RETCODE_ERROR;
      break;
  }
  return DDS_RETCODE_OK;
}

static dds_return_t write_meta_data(ddsts_ostream_t *ostream, ddsts_type_t *struct_def)
{
  /* Determine which structs should be include */
  included_type_t *included_types = NULL;
  dds_return_t rc = find_used_structs(&included_types, struct_def);
  if (rc != DDS_RETCODE_OK) {
    return rc;
  }

  ddsts_type_t *root_type = struct_def;
  while (root_type->type.parent != NULL)
    root_type = root_type->type.parent;

  output(ostream, "<MetaData version=\\\"1.0.0\\\">", '\0');

  ddsts_call_path_t path;
  path.type = root_type;
  path.call_parent = NULL;

  meta_data_context_t context;
  meta_data_context_init(&context, ostream, included_types);
  rc = ddsts_walk(&path, 0, DDSTS_MODULE | DDSTS_STRUCT, write_meta_data_elem, &context.output_context);
  if (rc != DDS_RETCODE_OK) {
    free_included_types(included_types);
    return rc;
  }

  output(ostream, "</MetaData>", '\0');

  free_included_types(included_types);

  return DDS_RETCODE_OK;
}

/*   Functions for generating the source file */

typedef struct {
  int value;
  int ordering;
  const char *rendering;
} alignment_t;

alignment_t alignment_types[8] = {
#define ALIGNMENT_ONE         (&alignment_types[0])
  { 1, 0, "1u"},
#define ALIGNMENT_BOOL        (&alignment_types[1])
  { 0, 0, "sizeof(bool)"},
#define ALIGNMENT_ONE_OR_BOOL (&alignment_types[2])
  { 0, 1, "(sizeof(bool)>1u)?sizeof(bool):1u"},
#define ALIGNMENT_TWO         (&alignment_types[3])
  { 2, 2, "2u"},
#define ALIGNMENT_TWO_OR_BOOL (&alignment_types[4])
  { 0, 3, "(sizeof(bool)>2u)?sizeof(bool):2u"},
#define ALIGNMENT_FOUR        (&alignment_types[5])
  { 4, 4, "4u"},
#define ALIGNMENT_PTR         (&alignment_types[6])
  { 0, 6, "sizeof (char *)"},
#define ALIGNMENT_EIGHT       (&alignment_types[7])
  { 8, 8, "8u"}
};

static alignment_t *max_alignment(alignment_t *a, alignment_t *b)
{
   if (   (a == ALIGNMENT_BOOL && b == ALIGNMENT_ONE)
       || (b == ALIGNMENT_BOOL && a == ALIGNMENT_ONE)) {
     return ALIGNMENT_ONE_OR_BOOL;
   }
   if (   (a == ALIGNMENT_BOOL && b == ALIGNMENT_TWO)
       || (b == ALIGNMENT_BOOL && a == ALIGNMENT_TWO)) {
     return ALIGNMENT_TWO_OR_BOOL;
   }
   return a->ordering > b->ordering ? a : b;
}

static alignment_t *ddsts_type_alignment(ddsts_type_t *type)
{
  switch (DDSTS_TYPE_OF(type)) {
    case DDSTS_CHAR:       return ALIGNMENT_ONE; break;
    case DDSTS_OCTET:      return ALIGNMENT_ONE; break;
    case DDSTS_INT8:       return ALIGNMENT_ONE; break;
    case DDSTS_UINT8:      return ALIGNMENT_ONE; break;
    case DDSTS_BOOLEAN:    return ALIGNMENT_BOOL; break;
    case DDSTS_SHORT:      return ALIGNMENT_TWO; break;
    case DDSTS_USHORT:     return ALIGNMENT_TWO; break;
    case DDSTS_LONG:       return ALIGNMENT_FOUR; break;
    case DDSTS_ULONG:      return ALIGNMENT_FOUR; break;
    case DDSTS_FLOAT:      return ALIGNMENT_FOUR; break;
    case DDSTS_LONGLONG:   return ALIGNMENT_EIGHT; break;
    case DDSTS_ULONGLONG:  return ALIGNMENT_EIGHT; break;
    case DDSTS_DOUBLE:     return ALIGNMENT_EIGHT; break;
    case DDSTS_SEQUENCE:   return ALIGNMENT_PTR; break;
    case DDSTS_MAP:        return ALIGNMENT_PTR; break;
    case DDSTS_STRING:
      return DDSTS_IS_UNBOUND(type) ? ALIGNMENT_PTR : ALIGNMENT_ONE;
      break;
    case DDSTS_ARRAY:
      return ddsts_type_alignment(type->array.element_type);
      break;
    case DDSTS_STRUCT: {
      alignment_t *alignment = ALIGNMENT_ONE;
      for (ddsts_type_t *member = type->struct_def.members.first; member != NULL; member = member->type.next) {
        if (DDSTS_IS_TYPE(member, DDSTS_DECLARATION)) {
          alignment = max_alignment(alignment, ddsts_type_alignment(member->declaration.decl_type));
        }
      }
      return alignment;
      break;
    }
    case DDSTS_FORWARD_STRUCT:
      if (type->forward.definition != NULL) {
        return ddsts_type_alignment(type->forward.definition);
      }
      break;
    case DDSTS_WIDE_CHAR:
    case DDSTS_LONGDOUBLE:
    case DDSTS_FIXED_PT_CONST:
    case DDSTS_ANY:
    case DDSTS_WIDE_STRING:
    case DDSTS_FIXED_PT:
      /* not supported */
      return ALIGNMENT_ONE;
      break;
    default:
      assert(0);
  }
  return ALIGNMENT_ONE;
}

static bool ddsts_type_optimizable(ddsts_type_t *type)
{
  if (DDSTS_IS_TYPE(type, DDSTS_STRUCT | DDSTS_FORWARD_STRUCT)) {
    ddsts_type_t *struct_def = get_struct_def(type);
    if (struct_def == NULL) {
      return false;
    }
    for (ddsts_type_t *member = struct_def->struct_def.members.first; member != NULL; member = member->type.next) {
      if (DDSTS_IS_TYPE(member, DDSTS_DECLARATION)) {
        if (!ddsts_type_optimizable(member->declaration.decl_type)) {
          return false;
        }
      }
    }
    return true;
  }
  if (DDSTS_IS_TYPE(type, DDSTS_FORWARD_STRUCT)) {
    return ddsts_type_optimizable(type->forward.definition);
  }
  if (DDSTS_IS_TYPE(type, DDSTS_STRING) && !DDSTS_IS_UNBOUND(type)) {
    return false;
  }
  return ddsts_type_alignment(type) != ALIGNMENT_PTR;
}

static dds_return_t walk_struct_keys(ddsts_call_path_t *path, bool top, ddsts_walk_call_func_t func, op_codes_context_t *context)
{
  assert(path != NULL && DDSTS_IS_TYPE(path->type, DDSTS_STRUCT));
  ddsts_type_t *struct_def = get_struct_def(path->type);
  if (struct_def == NULL) {
    return DDS_RETCODE_ERROR;
  }
  for (ddsts_type_t *member = struct_def->struct_def.members.first; member != NULL; member = member->type.next) {
    if (DDSTS_IS_TYPE(member, DDSTS_DECLARATION)) {
      bool member_is_key = !top && struct_def->struct_def.keys == NULL;
      for (ddsts_struct_key_t *key = struct_def->struct_def.keys; key != NULL; key = key->next) {
        if (key->member == member) {
          member_is_key = true;
          break;
        }
      }
      if (member_is_key) {
        ddsts_call_path_t declaration_path;
        declaration_path.type = member;
        declaration_path.call_parent = path;
        if (DDSTS_IS_TYPE(member->declaration.decl_type, DDSTS_STRUCT | DDSTS_FORWARD_STRUCT)) {
          ddsts_type_t *sd = get_struct_def(member->declaration.decl_type);
          if (sd == NULL) {
            return DDS_RETCODE_ERROR;
          }
          ddsts_call_path_t struct_decl_path;
          struct_decl_path.type = sd;
          struct_decl_path.call_parent = &declaration_path;
          dds_return_t rc = walk_struct_keys(&struct_decl_path, false, func, context);
          if (rc != DDS_RETCODE_OK) {
            return rc;
          }
        }
        else {
          dds_return_t rc = func(&declaration_path, context);
          if (rc != DDS_RETCODE_OK) {
            return rc;
          }
        }
      }
    }
  }

  return DDS_RETCODE_OK;
}

static dds_return_t collect_key_properties(ddsts_call_path_t *path, void *context)
{
  ((op_codes_context_t*)context)->nr_keys++;
  ddsts_type_t *type = path->type->declaration.decl_type;
  unsigned long long size = 1;
  while (DDSTS_IS_TYPE(type, DDSTS_ARRAY)) {
    size *= type->array.size;
    type = type->array.element_type;
  }
  switch (DDSTS_TYPE_OF(type)) {
    case DDSTS_CHAR:
    case DDSTS_BOOLEAN:
    case DDSTS_OCTET:
    case DDSTS_INT8:
    case DDSTS_UINT8:
      break;
    case DDSTS_SHORT:
    case DDSTS_USHORT:
      size *= 2ULL;
      break;
    case DDSTS_LONG:
    case DDSTS_ULONG:
    case DDSTS_FLOAT:
      size *= 4ULL;
      break;
    case DDSTS_LONGLONG:
    case DDSTS_ULONGLONG:
    case DDSTS_DOUBLE:
      size *= 8ULL;
      break;
    case DDSTS_STRING:
      if (type->string.max > 0) {
        size *= type->string.max + 5ULL;
      }
      else {
        size = 0ULL;
        ((op_codes_context_t*)context)->bounded_key_size = false;
      }
      break;
    default:
      assert(0);
      return DDS_RETCODE_ERROR;
      break;
  }
  if (size >= (1ULL << 32)) {
    return DDS_RETCODE_ERROR;
  }
  ((op_codes_context_t*)context)->key_size += (uint32_t)size;
  return DDS_RETCODE_OK;
}

static void write_key_name(ddsts_call_path_t *path, bool top, ddsts_ostream_t *ostream)
{
  if (DDSTS_IS_TYPE(path->type, DDSTS_MODULE)) {
    return;
  }
  if (DDSTS_IS_TYPE(path->type, DDSTS_DECLARATION)) {
    write_key_name(path->call_parent, false, ostream);
    ddsts_ostream_puts(ostream, path->type->type.name);
    if (!top) {
      ddsts_ostream_put(ostream, '.');
    }
  }
  else {
    write_key_name(path->call_parent, top, ostream);
  }
}

static dds_return_t write_key_description(ddsts_call_path_t *path, void *context)
{
  ddsts_ostream_t *ostream = ((op_codes_context_t*)context)->output_context.ostream;

  key_offset_t *key_offset = op_codes_context_find_key((op_codes_context_t*)context, path->type);
  if (key_offset == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  if (((op_codes_context_t*)context)->nr_keys != 0) {
    ddsts_ostream_puts(ostream, ",\n");
  }
  ((op_codes_context_t*)context)->nr_keys++;
  ddsts_ostream_puts(ostream, "  { \"");
  write_key_name(path, true, ostream);
  char offset_string[30];
  ddsrt_ulltostr(key_offset->offset, offset_string, 30, NULL);
  output(((output_context_t*)context)->ostream,
         "\", $O }",
         'O', offset_string,
         '\0');
 return DDS_RETCODE_OK;
}

static dds_return_t write_source_struct(ddsts_call_path_t *path, void *context)
{
  DDSRT_UNUSED_ARG(context);

  assert(DDSTS_IS_TYPE(path->type, DDSTS_STRUCT));
  ddsts_type_t *struct_def = path->type;

  if (struct_def->struct_def.keys == NULL) {
    return DDS_RETCODE_OK;
  }

  const char *full_name = name_with_module_prefix(struct_def, "_");
  if (full_name == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }

  /* We first run the op-code generation, discarding output, to calculate
   * the sizes of the various types and the positions of the topic keys
   */
  ddsts_ostream_t *null_ostream = NULL;
  dds_return_t rc;
  rc = ddsts_create_ostream_to_null(&null_ostream);
  if (rc != DDS_RETCODE_OK) {
    ddsrt_free((void*)full_name);
    return rc;
  }
  op_codes_context_t op_codes_context;
  op_codes_context_init(&op_codes_context, null_ostream);

  rc = generate_op_codes_struct(path, &op_codes_context);
  if (rc != DDS_RETCODE_OK) {
    op_codes_context_fini(&op_codes_context);
    ddsrt_free(null_ostream);
    ddsrt_free((void*)full_name);
    return rc;
  }
  op_codes_context_reset(&op_codes_context, ((output_context_t*)context)->ostream);
  ddsrt_free(null_ostream);

  rc = walk_struct_keys(path, true, collect_key_properties, &op_codes_context);
  if (rc != DDS_RETCODE_OK) {
    return rc;
  }

  char nr_keys_string[30];
  ddsrt_ulltostr(op_codes_context.nr_keys, nr_keys_string, 30, NULL);

  output(((output_context_t*)context)->ostream,
         "\n\n"
         "static const dds_key_descriptor_t $N_keys[$C] =\n"
         "{\n",
         'N', full_name,
         'C', nr_keys_string,
         '\0');
  op_codes_context.nr_keys = 0;
  rc = walk_struct_keys(path, true, write_key_description, &op_codes_context);
  if (rc != DDS_RETCODE_OK) {
    return rc;
  }
  output(((output_context_t*)context)->ostream,
         "\n};\n"
         "\n"
         "static const uint32_t $N_ops [] =\n"
         "{\n",
         'N', full_name,
         '\0');


  /* Next we run the op-code generation to the stream */
  rc = generate_op_codes_struct(path, &op_codes_context);
  if (rc != DDS_RETCODE_OK) {
    op_codes_context_fini(&op_codes_context);
    ddsrt_free((void*)full_name);
    return rc;
  }

  op_codes_context_fini(&op_codes_context);

  const char *dotted_full_name = name_with_module_prefix(struct_def, "::");
  if (dotted_full_name == NULL) {
    ddsrt_free((void*)full_name);
    return rc;
  }

  bool fixed_key = op_codes_context.bounded_key_size && op_codes_context.key_size <= 16;
  bool no_optimize = !ddsts_type_optimizable(struct_def);
  char nr_ops_string[30];
  ddsrt_ulltostr(op_codes_context.nr_ops, nr_ops_string, 30, NULL);
  output(((output_context_t*)context)->ostream,
         "\n};\n"
         "\n"
         "const dds_topic_descriptor_t $N_desc =\n"
         "{\n"
         "  sizeof ($N),\n"
         "  $A,\n"
         "  $F,\n"
         "  $Ku,\n"
         "  \"$M\",\n"
         "  $N_keys,\n"
         "  $S,\n"
         "  $N_ops,\n"
         "  \"",
         'N', full_name,
         'A', ddsts_type_alignment(struct_def)->rendering,
         'K', nr_keys_string,
         'F',   fixed_key
              ? (no_optimize ? "DDS_TOPIC_FIXED_KEY | DDS_TOPIC_NO_OPTIMIZE" : "DDS_TOPIC_FIXED_KEY")
              : (no_optimize ? "DDS_TOPIC_NO_OPTIMIZE" : "0u"),
         'M', dotted_full_name,
         'S', nr_ops_string,
         '\0');
  rc = write_meta_data(((output_context_t*)context)->ostream, struct_def);
  if (rc != DDS_RETCODE_OK) {
    ddsrt_free((void*)dotted_full_name);
    ddsrt_free((void*)full_name);
    return rc;
  }
  output(((output_context_t*)context)->ostream,
         "\"\n"
         "};\n",
         '\0');

  ddsrt_free((void*)dotted_full_name);
  ddsrt_free((void*)full_name);

  return DDS_RETCODE_OK;
}

static dds_return_t generate_source_file(const char *file_name, ddsts_type_t *root_node, ddsts_ostream_t *ostream)
{
  DDSRT_UNUSED_ARG(root_node);
  const char *c_file_name = NULL;
  dds_return_t rc;
  rc = output_file_name(file_name, "c", &c_file_name);
  if (rc != DDS_RETCODE_OK) {
    return rc;
  }
  if (!ddsts_ostream_open(ostream, c_file_name)) {
    DDS_ERROR("Could not open file '%s' for writing", c_file_name);
    ddsrt_free((void*)c_file_name);
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  const char *h_file_name = NULL;
  rc = output_file_name(file_name, "h", &h_file_name);
  if (rc != DDS_RETCODE_OK) {
    ddsts_ostream_close(ostream);
    ddsrt_free((void*)c_file_name);
    return rc;
  }

  write_file_header(ostream, c_file_name, file_name);
  output(ostream, "#include \"$F\"\n\n\n\n", 'F', h_file_name, '\0');

  ddsts_call_path_t path;
  path.type = root_node;
  path.call_parent = NULL;

  output_context_t output_context;
  output_context.ostream = ostream;
  rc = ddsts_walk(&path, DDSTS_MODULE, DDSTS_STRUCT, write_source_struct, &output_context);

  ddsrt_free((void*)h_file_name);
  ddsts_ostream_close(ostream);
  ddsrt_free((void*)c_file_name);

  return rc;
}

/* Function for generating C99 files */

static dds_return_t ddsts_generate_C99_to_ostream(const char *file_name, ddsts_type_t *root_node, ddsts_ostream_t *ostream)
{
  dds_return_t rc = generate_header_file(file_name, root_node, ostream);
  if (rc != DDS_RETCODE_OK) {
    return rc;
  }
  return generate_source_file(file_name, root_node, ostream);
}

extern dds_return_t ddsts_generate_C99(const char *file_name, ddsts_type_t *root_node)
{
  ddsts_ostream_t *ostream = NULL;
  dds_return_t rc;
  rc = ddsts_create_ostream_to_files(&ostream);
  if (rc != DDS_RETCODE_OK) {
    return rc;
  }
  rc = ddsts_generate_C99_to_ostream(file_name, root_node, ostream);
  ddsrt_free((void*)ostream);
  return rc;
}

extern dds_return_t ddsts_generate_C99_to_buffer(const char *file_name, ddsts_type_t *root_node, char *buffer, size_t buffer_len)
{
  ddsts_ostream_t *ostream = NULL;
  dds_return_t rc;
  rc = ddsts_create_ostream_to_buffer(buffer, buffer_len, &ostream);
  if (rc != DDS_RETCODE_OK) {
    return rc;
  }
  rc = ddsts_generate_C99_to_ostream(file_name, root_node, ostream);
  ddsrt_free((void*)ostream);
  return rc;
}
