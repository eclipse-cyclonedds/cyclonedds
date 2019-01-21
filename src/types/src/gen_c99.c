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

#include "os/os.h"
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "gen_c99.h"

/* Traverse process */

/* The traverse process takes care of flattening the definitions into a
 * sequence of calls to functions for:
 * - structure definition
 * - sequence structure definition
 * - forward structure declaration
 */

/* Structure for the call-back functions */

typedef struct
{
  void (*open_module)(void *context, dds_ts_module_t *module);
  void (*close_module)(void *context);
  void (*forward_struct)(void *context, dds_ts_struct_t *struct_def);
  void (*unembedded_sequence)(void *context, dds_ts_struct_t *base_type, const char *decl_name, dds_ts_struct_t *element_type);
  void (*unembedded_sequence_with_name)(void *context, dds_ts_struct_t *base_type, const char *decl_name, const char *element_type_name);
  void (*open_struct)(void *context, dds_ts_struct_t *struct_def);
  void (*close_struct)(void *context, dds_ts_struct_t *struct_def);
  void (*open_struct_declarator)(void *context, dds_ts_definition_t *declarator);
  void (*open_struct_declarator_with_type)(void *context, dds_ts_definition_t *declarator, dds_ts_type_spec_t *type);
  void (*close_struct_declarator)(void *context);
  void (*open_array)(void *context, unsigned long long size);
  void (*close_array)(void *context, unsigned long long size);
  void (*basic_type)(void *context, dds_ts_type_spec_t *type);
  void (*type_reference)(void *context, const char *name);
  void (*open_sequence)(void *context);
  void (*close_sequence)(void *sequence);
  bool unembed_embedded_structs;
} traverse_funcs_t;

void traverse_funcs_init(traverse_funcs_t *traverse_funcs)
{
  traverse_funcs->open_module = 0;
  traverse_funcs->close_module = 0;
  traverse_funcs->forward_struct = 0;
  traverse_funcs->unembedded_sequence = 0;
  traverse_funcs->unembedded_sequence_with_name = 0;
  traverse_funcs->open_struct = 0;
  traverse_funcs->close_struct = 0;
  traverse_funcs->open_struct_declarator = 0;
  traverse_funcs->open_struct_declarator_with_type = 0;
  traverse_funcs->close_struct_declarator = 0;
  traverse_funcs->open_array = 0;
  traverse_funcs->close_array = 0;
  traverse_funcs->basic_type = 0;
  traverse_funcs->type_reference = 0;
  traverse_funcs->open_sequence = 0;
  traverse_funcs->close_sequence = 0;
  traverse_funcs->unembed_embedded_structs = false;
}

/* Some administration used during the traverse process */

/* This keeps a list of structures for which open_struct or forward_struct
 * has been called. It is also used to determine if forward_struct needs to
 * called before a call to unembedded_sequence.
 */

typedef struct struct_def_used struct_def_used_t;
struct struct_def_used {
  dds_ts_struct_t *struct_def;
  bool as_forward;
  bool as_definition;
  struct_def_used_t *next;
};

typedef struct {
  struct_def_used_t *struct_def_used;
} traverse_admin_t;

static void init_traverse_admin(traverse_admin_t *traverse_admin)
{
  traverse_admin->struct_def_used = NULL;
}

static void free_traverse_admin(traverse_admin_t *traverse_admin)
{
  struct_def_used_t *struct_def_used;
  for (struct_def_used = traverse_admin->struct_def_used; struct_def_used != NULL;) {
    struct_def_used_t *next = struct_def_used->next;
    os_free(struct_def_used);
    struct_def_used = next;
  }
}

static struct_def_used_t *find_struct_def_used(traverse_admin_t *traverse_admin, dds_ts_struct_t *struct_def)
{
  struct_def_used_t *struct_def_used;
  for (struct_def_used = traverse_admin->struct_def_used; struct_def_used != NULL; struct_def_used = struct_def_used->next)
    if (struct_def_used->struct_def == struct_def)
      return struct_def_used;
  struct_def_used = (struct_def_used_t*)os_malloc(sizeof(struct_def_used_t));
  if (struct_def_used == NULL) {
    fprintf(stderr, "Error: memory allocation failed\n");
    return NULL;
  }
  struct_def_used->struct_def = struct_def;
  struct_def_used->as_forward = false;
  struct_def_used->as_definition = false;
  struct_def_used->next = traverse_admin->struct_def_used;
  traverse_admin->struct_def_used = struct_def_used;
  return struct_def_used;
}

/* Is embedded struct */

static bool is_embedded_struct(dds_ts_struct_t *struct_def)
{
  return    struct_def->def.type_spec.node.parent != NULL
         && struct_def->def.type_spec.node.parent->flags == DDS_TS_STRUCT;
}

/* The functions that implement the traverse process */

static void traverse_forward_struct(void *context, traverse_admin_t *admin, traverse_funcs_t *funcs, dds_ts_struct_t *struct_def)
{
  if (struct_def != NULL && funcs->forward_struct != NULL) {
    struct_def_used_t *struct_def_used = find_struct_def_used(admin, struct_def);
    if (struct_def_used != NULL && !struct_def_used->as_forward && !struct_def_used->as_definition) {
      funcs->forward_struct(context, struct_def);
      struct_def_used->as_forward = true;
    }
  }
}

static void traverse_struct(void *context, traverse_admin_t *admin, traverse_funcs_t *funcs, dds_ts_struct_t *struct_def);

static void traverse_type_spec(void *context, traverse_admin_t *admin, traverse_funcs_t *funcs, dds_ts_type_spec_t *type_spec)
{
  if (funcs->basic_type != 0) {
    funcs->basic_type(context, type_spec);
  }
  if (type_spec->node.flags == DDS_TS_STRUCT) {
    if (is_embedded_struct((dds_ts_struct_t*)type_spec)) {
      if (!funcs->unembed_embedded_structs) {
        traverse_struct(context, admin, funcs, (dds_ts_struct_t*)type_spec);
      }
    }
    else {
      if (funcs->type_reference != 0) {
        funcs->type_reference(context, ((dds_ts_struct_t*)type_spec)->def.name);
      }
    }
  }
  else if (type_spec->node.flags == DDS_TS_FORWARD_STRUCT) {
    dds_ts_forward_declaration_t *forward_struct = (dds_ts_forward_declaration_t*)type_spec;
    if (forward_struct->definition != NULL) {
      if (funcs->type_reference != 0) {
        funcs->type_reference(context, forward_struct->definition->name);
      }
    }
  }
  else if (type_spec->node.flags == DDS_TS_SEQUENCE) {
    if (funcs->open_sequence != 0) {
      funcs->open_sequence(context);
    }
    traverse_type_spec(context, admin, funcs, ((dds_ts_sequence_t*)type_spec)->element_type.type_spec);
    if (funcs->close_sequence != 0) {
      funcs->close_sequence(context);
    }
  }
}

static void traverse_struct(void *context, traverse_admin_t *admin, traverse_funcs_t *funcs, dds_ts_struct_t *struct_def)
{
  dds_ts_node_t *child;

  if (funcs->unembedded_sequence != 0 || funcs->unembedded_sequence_with_name != 0 || funcs->unembed_embedded_structs) {
    /* See if it has nested struct or sequence of structs for which defines need to be generated */
    for (child = struct_def->def.type_spec.node.children; child != NULL; child = child->next) {
      if (child->flags == DDS_TS_STRUCT) {
        if (funcs->unembed_embedded_structs) {
          traverse_struct(context, admin, funcs, (dds_ts_struct_t*)child);
        }
      }
      else if (child->flags == DDS_TS_STRUCT_MEMBER) {
        dds_ts_struct_member_t *member = (dds_ts_struct_member_t*)child;
        if (member->member_type.type_spec->node.flags == DDS_TS_SEQUENCE) {
          dds_ts_sequence_t *seq_member_type = (dds_ts_sequence_t*)member->member_type.type_spec;
          dds_ts_type_spec_t *element_type = seq_member_type->element_type.type_spec;
          if (   (   element_type->node.flags == DDS_TS_STRUCT
                  || element_type->node.flags == DDS_TS_FORWARD_STRUCT)
              && funcs->unembedded_sequence != 0) {
            /* A call to forward_struct may be needed */
            if (element_type->node.flags == DDS_TS_STRUCT)
              traverse_forward_struct(context, admin, funcs, (dds_ts_struct_t*)element_type);
            /* For each of the declarators, call unembedded_sequence */
            dds_ts_node_t *declarator;
            for (declarator = member->node.children; declarator != NULL; declarator = declarator->next) {
              funcs->unembedded_sequence(context, struct_def, ((dds_ts_definition_t*)declarator)->name, (dds_ts_struct_t*)seq_member_type->element_type.type_spec);
            }
          }
          else if (element_type->node.flags == DDS_TS_SEQUENCE && funcs->unembedded_sequence_with_name != 0) {
            /* This is maybe not the right solution.. */
            dds_ts_node_t *declarator;
            for (declarator = member->node.children; declarator != NULL; declarator = declarator->next) {
              funcs->unembedded_sequence_with_name(context, struct_def, ((dds_ts_definition_t*)declarator)->name, "dds_sequence_t");
            }
          }
        }
      }
    }
  }

  if (funcs->open_struct != 0) {
    funcs->open_struct(context, struct_def);
  }
  struct_def_used_t *struct_def_used = find_struct_def_used(admin, struct_def);
  if (struct_def_used != NULL)
    struct_def_used->as_definition = true;
  for (child = struct_def->def.type_spec.node.children; child != NULL; child = child->next) {
    if (child->flags == DDS_TS_STRUCT_MEMBER) {
      dds_ts_struct_member_t *member = (dds_ts_struct_member_t*)child;
      dds_ts_node_t *decl_node;
      for (decl_node = member->node.children; decl_node != NULL; decl_node = decl_node->next) {
        assert(decl_node->flags == DDS_TS_DECLARATOR);
        dds_ts_definition_t *declarator = (dds_ts_definition_t*)decl_node;
        if (funcs->open_struct_declarator != 0) {
          funcs->open_struct_declarator(context, declarator);
        }
        if (funcs->open_struct_declarator_with_type != 0) {
          funcs->open_struct_declarator_with_type(context, declarator, member->member_type.type_spec);
        }
        if (funcs->open_array != 0) {
          dds_ts_node_t *array_size_node;
          for (array_size_node = declarator->type_spec.node.children; array_size_node != NULL; array_size_node = array_size_node->next) {
            assert(array_size_node->flags == DDS_TS_ARRAY_SIZE);
            dds_ts_array_size_t *array_size = (dds_ts_array_size_t*)array_size_node;
            funcs->open_array(context, array_size->size);
          }
        }
        traverse_type_spec(context, admin, funcs, member->member_type.type_spec);
        if (funcs->close_array != 0) {
          dds_ts_node_t *array_size_node;
          for (array_size_node = declarator->type_spec.node.children; array_size_node != NULL; array_size_node = array_size_node->next) {
            assert(array_size_node->flags == DDS_TS_ARRAY_SIZE);
            dds_ts_array_size_t *array_size = (dds_ts_array_size_t*)array_size_node;
            funcs->close_array(context, array_size->size);
          }
        }
        if (funcs->close_struct_declarator != 0) {
          funcs->close_struct_declarator(context);
        }
      }
    }
  }
  if (funcs->close_struct != 0) {
    funcs->close_struct(context, struct_def);
  }
}

static void traverse_modules(void *context, traverse_admin_t *admin, traverse_funcs_t *funcs, dds_ts_node_t *node)
{
  dds_ts_node_t *child;
  for (child = node->children; child != NULL; child = child->next)
    if (child->flags == DDS_TS_MODULE) {
      if (funcs->open_module != 0) {
        funcs->open_module(context, (dds_ts_module_t*)child);
      }
      traverse_modules(context, admin, funcs, child);
      if (funcs->close_module != 0) {
        funcs->close_module(context);
      }
    }
    else if (child->flags == DDS_TS_STRUCT) {
      traverse_struct(context, admin, funcs, (dds_ts_struct_t*)child);
    }
    else if (child->flags == DDS_TS_FORWARD_STRUCT) {
      traverse_forward_struct(context, admin, funcs, (dds_ts_struct_t*)((dds_ts_forward_declaration_t*)child)->definition);
    }
}

/* Included modules and structs */

/* Given a struct, find all modules and structs that need to be included because
   they are used (recursively) */

typedef struct included_node included_node_t;
struct included_node {
  dds_ts_node_t *node;
  included_node_t *next;
};

static void included_nodes_add(included_node_t **ref_included_nodes, dds_ts_node_t *node)
{
  included_node_t *new_included_node = (included_node_t*)malloc(sizeof(included_node_t));
  if (new_included_node == NULL) {
    return; /* do not report out-of-memory */
  }
  new_included_node->node = node;
  new_included_node->next = *ref_included_nodes;
  *ref_included_nodes = new_included_node;
}

static void included_nodes_free(included_node_t *included_nodes)
{
  while (included_nodes != NULL) {
    included_node_t *next = included_nodes->next;
    os_free((void*)included_nodes);
    included_nodes = next;
  }
}

static bool included_nodes_contains(included_node_t *included_nodes, dds_ts_node_t *node)
{
  for (; included_nodes != NULL; included_nodes = included_nodes->next)
    if (included_nodes->node == node)
      return true;
  return false;
}

static void find_used_structs(included_node_t **ref_included_nodes, dds_ts_struct_t *struct_def);

static void find_used_structs_in_members(included_node_t **ref_included_nodes, dds_ts_struct_t *struct_def)
{
  dds_ts_node_t *child;
  for (child = struct_def->def.type_spec.node.children; child != NULL; child = child->next) {
    if (child->flags == DDS_TS_STRUCT)
      find_used_structs_in_members(ref_included_nodes, (dds_ts_struct_t*)child);
    else if (child->flags == DDS_TS_STRUCT_MEMBER) {
      dds_ts_struct_member_t *member = (dds_ts_struct_member_t*)child;
      if (member->member_type.type_spec->node.flags == DDS_TS_SEQUENCE) {
        dds_ts_sequence_t *seq_member_type = (dds_ts_sequence_t*)member->member_type.type_spec;
        dds_ts_type_spec_t *element_type = seq_member_type->element_type.type_spec;
        if (element_type->node.flags == DDS_TS_STRUCT)
          find_used_structs(ref_included_nodes, (dds_ts_struct_t*)element_type);
        else if (element_type->node.flags == DDS_TS_FORWARD_STRUCT) {
          dds_ts_forward_declaration_t *forward_decl = (dds_ts_forward_declaration_t*)element_type;
          if (forward_decl->definition != NULL)
            find_used_structs(ref_included_nodes, (dds_ts_struct_t*)forward_decl->definition);
        }
      }
    }
  }
}

static void find_used_structs(included_node_t **ref_included_nodes, dds_ts_struct_t *struct_def)
{
  dds_ts_node_t *node = &struct_def->def.type_spec.node;
  if (included_nodes_contains(*ref_included_nodes, node))
    return;
  included_nodes_add(ref_included_nodes, node);

  /* include modules in which this struct occurs */
  for (node = node->parent; node != NULL && node->parent != NULL ; node = node->parent) {
    if (included_nodes_contains(*ref_included_nodes, node))
      break;
    included_nodes_add(ref_included_nodes, node);
  }

  find_used_structs_in_members(ref_included_nodes, struct_def);
}

/* Generic output stream, so we can write to file or a buffer */

typedef struct {
  bool (*open)(void *data, const char *name);
  void (*close)(void *data);
  void (*put)(void *data, char ch);
  void *data;
} ostream_t;

bool ostream_open(ostream_t *stream, const char *name)
{
  return stream->open(stream->data, name);
}

void ostream_close(ostream_t *stream)
{
  stream->close(stream->data);
}

void ostream_put(ostream_t *stream, char ch)
{
  stream->put(stream->data, ch);
}

/* file output stream */

bool file_ostream_open(void *data, const char *name)
{
OS_WARNING_MSVC_OFF(4996);
  return (*((FILE**)data) = fopen(name, "wt")) != 0;
OS_WARNING_MSVC_ON(4996);
}

void file_ostream_close(void *data)
{
  fclose(*((FILE**)data));
}

void file_ostream_put(void *data, char ch)
{
  fputc(ch, *(FILE**)data);
}

void init_file_ostream(ostream_t *stream, FILE **f)
{
  stream->open = file_ostream_open;
  stream->close = file_ostream_close;
  stream->put = file_ostream_put;
  stream->data = (void*)f;
}

/* buffer output stream */

typedef struct {
  char *s;
  const char *e;
} output_buffer_t;

void buffer_ostream_put(void *data, char ch)
{
  output_buffer_t* buffer = (output_buffer_t*)data;
  if (buffer->s < buffer->e)
    *buffer->s++ = ch;
}

bool buffer_ostream_open(void *data, const char* name)
{
  OS_UNUSED_ARG(data);
  OS_UNUSED_ARG(name);
  return true;
}

void buffer_ostream_close(void *data)
{
  OS_UNUSED_ARG(data);
}

void init_buffer_ostream(ostream_t *stream, output_buffer_t *buf)
{
  stream->open = buffer_ostream_open;
  stream->close = buffer_ostream_close;
  stream->put = buffer_ostream_put;
  stream->data = (void*)buf;
}

/* Output function with named string arguments */

static void output(ostream_t *stream, const char *fmt, ...)
{
  const char *s;
  for (s = fmt; *s != '\0'; s++)
    if (*s == '$') {
      s++;
      if (*s == '$')
        ostream_put(stream, *s);
      else {
        va_list args;
        va_start(args, fmt);
        for (;;) {
          char letter = (char)va_arg(args, int);
          if (letter == '\0')
            break;
          const char *str = va_arg(args, const char*);
          if (letter == *s) {
            const char *t;
            for (t = str; *t != '\0'; t++)
              ostream_put(stream, *t);
            break;
          }
        }
        va_end(args);
      }
    }
    else
      ostream_put(stream, *s);
}


/* Finding specific parent of a node */

static dds_ts_node_t *dds_ts_node_get_parent(dds_ts_node_t *node, dds_ts_node_flags_t flags)
{
  while (node != NULL) {
    node = node->parent;
    if (node != NULL && node->flags == flags) {
      return node;
    }
  }
  return NULL;
}

/* Generating output */

static const char *output_file_name(const char* file_name, const char *ext)
{
  size_t file_name_len = strlen(file_name);
  if (file_name_len < 4 || strcmp(file_name + file_name_len - 4, ".idl") != 0) {
    fprintf(stderr, "Error: File name '%s' does not have '.idl' extension\n", file_name);
    return NULL;
  }
  size_t result_len = file_name_len - 2 + strlen(ext);
  char *result = (char*)os_malloc(sizeof(char)*(result_len));
  if (result == NULL) {
    fprintf(stderr, "Error: memory allocation failed\n");
    return NULL;
  }
  os_strlcpy(result, file_name, file_name_len - 2);
  os_strlcat(result, ext, result_len);
  return result;
}

static const char *uppercase_file_name(const char* file_name)
{
  size_t file_name_len = strlen(file_name);
  if (file_name_len < 4 || strcmp(file_name + file_name_len - 4, ".idl") != 0) {
    fprintf(stderr, "Error: File name '%s' does not have '.idl' extension\n", file_name);
    return NULL;
  }
  char *result = (char*)os_malloc(sizeof(char)*(file_name_len - 3));
  if (result == NULL) {
    fprintf(stderr, "Error: memory allocation failed\n");
    return NULL;
  }
  size_t i;
  for (i = 0; i < file_name_len - 4; i++)
    result[i] = (char)toupper(file_name[i]);
  result[i] = '\0';
  return result;
}

/* Generate header file */

static void write_copyright_header(ostream_t *stream, const char *target_file_name, const char *source_file_name)
{
  os_time time_now = os_timeGet();
  char time_descr[OS_CTIME_R_BUFSIZE+1];
  os_ctime_r(&time_now, time_descr, OS_CTIME_R_BUFSIZE);

  output(stream,
         "/*\n"
         " * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others\n"
         " *\n"
         " * This program and the accompanying materials are made available under the\n"
         " * terms of the Eclipse Public License v. 2.0 which is available at\n"
         " * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License\n"
         " * v. 1.0 which is available at\n"
         " * http://www.eclipse.org/org/documents/edl-v10.php.\n"
         " *\n"
         " * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause\n"
         " */\n"
         "/****************************************************************\n"
         "\n"
         "  Generated by Cyclone DDS IDL to C Translator\n"
         "  File name: $T\n"
         "  Source: $S\n"
         "  Generated: $D\n"
         "  Cyclone DDS: V0.1.0\n"
         "\n"
         "*****************************************************************/\n"
         "\n",
         'T', target_file_name,
         'S', source_file_name,
         'D', time_descr,
         '\0');
}

static void write_header_intro(ostream_t *stream, const char *file_name)
{
  const char *uc_file_name = uppercase_file_name(file_name);
  if (uc_file_name == NULL)
    return;

  output(stream,
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

  os_free((void*)uc_file_name);
}

static void write_header_close(ostream_t *stream, const char *file_name)
{
  const char *uc_file_name = uppercase_file_name(file_name);
  if (uc_file_name == NULL)
    return;

  output(stream,
         "#ifdef __cplusplus\n"
         "}\n"
         "#endif\n"
         "#endif /* _DDSL_$U_H_ */\n",
         'U', uc_file_name,
         '\0');

  os_free((void*)uc_file_name);
}

static const char *name_with_module_prefix(dds_ts_definition_t *def_node)
{
  size_t len = 0;
  dds_ts_definition_t *cur = def_node;
  for (cur = def_node; cur != NULL;) {
    len += strlen(cur->name);
    if (   cur->type_spec.node.parent != NULL
        && (   cur->type_spec.node.parent->flags == DDS_TS_MODULE
            || cur->type_spec.node.parent->flags == DDS_TS_STRUCT)
        && cur->type_spec.node.parent->parent != NULL) {
      cur = (dds_ts_definition_t*)(cur->type_spec.node.parent);
      len++;
    }
    else
      cur = NULL;
  }
  char *result = (char*)os_malloc(sizeof(char)*(len+1));
  if (result == NULL) {
    fprintf(stderr, "Error: memory allocation failed\n");
    return NULL;
  }
  result[len] = '\0';
  for (cur = def_node; cur != NULL;) {
    size_t cur_name_len = strlen(cur->name);
    len -= cur_name_len;
    size_t i;
    for (i = 0; i < cur_name_len; i++) {
       result[len + i] = cur->name[i];
    }
    if (   cur->type_spec.node.parent != NULL
        && (   cur->type_spec.node.parent->flags == DDS_TS_MODULE
            || cur->type_spec.node.parent->flags == DDS_TS_STRUCT)
        && cur->type_spec.node.parent->parent != NULL) {
      cur = (dds_ts_definition_t*)cur->type_spec.node.parent;
      result[--len] = '_';
    }
    else {
      cur = NULL;
    }
  }
  return result;
}

static void write_header_open_struct(void *context, dds_ts_struct_t *struct_def)
{
  ostream_t *stream = (ostream_t*)context;

  const char *full_name = name_with_module_prefix(&struct_def->def);
  if (full_name == NULL)
    return;

  output(stream,
         "typedef struct $N\n"
         "{\n",
         'N', full_name,
         '\0');

  os_free((void*)full_name);
}

static void write_header_close_struct(void *context, dds_ts_struct_t *struct_def)
{
  ostream_t *stream = (ostream_t*)context;

  const char *full_name = name_with_module_prefix(&struct_def->def);
  if (full_name == NULL)
    return;

  output(stream,
         "} $N;\n"
         "\n",
         'N', full_name,
         '\0');
  if (!struct_def->part_of) {
    output(stream,
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
  output(stream, "\n", '\0');

  os_free((void*)full_name);
}

static void write_header_open_struct_declarator_with_type(void *context, dds_ts_definition_t *declarator, dds_ts_type_spec_t *type_spec)
{
  ostream_t *stream = (ostream_t*)context;

  output(stream, "  ", '\0');
  const char *decl_name = ((dds_ts_definition_t*)declarator)->name;
  switch (type_spec->node.flags) {
    case DDS_TS_SHORT_TYPE:              output(stream, "int16_t $D", 'D', decl_name, '\0'); break;
    case DDS_TS_LONG_TYPE:               output(stream, "int32_t $D", 'D', decl_name, '\0'); break;
    case DDS_TS_LONG_LONG_TYPE:          output(stream, "int64_t $D", 'D', decl_name, '\0'); break;
    case DDS_TS_UNSIGNED_SHORT_TYPE:     output(stream, "uint16_t $D", 'D', decl_name, '\0'); break;
    case DDS_TS_UNSIGNED_LONG_TYPE:      output(stream, "uint32_t $D", 'D', decl_name, '\0'); break;
    case DDS_TS_UNSIGNED_LONG_LONG_TYPE: output(stream, "uint64_t $D", 'D', decl_name, '\0'); break;
    case DDS_TS_CHAR_TYPE:               output(stream, "char $D", 'D', decl_name, '\0'); break;
    case DDS_TS_BOOLEAN_TYPE:            output(stream, "bool $D", 'D', decl_name, '\0'); break;
    case DDS_TS_OCTET_TYPE:              output(stream, "uint8_t $D", 'D', decl_name, '\0'); break;
    case DDS_TS_INT8_TYPE:               output(stream, "int8_t $D", 'D', decl_name, '\0'); break;
    case DDS_TS_UINT8_TYPE:              output(stream, "uint8_t $D", 'D', decl_name, '\0'); break;
    case DDS_TS_FLOAT_TYPE:              output(stream, "float $D", 'D', decl_name, '\0'); break;
    case DDS_TS_DOUBLE_TYPE:             output(stream, "double $D", 'D', decl_name, '\0'); break;
    case DDS_TS_STRING: {
      dds_ts_string_t *string_type = (dds_ts_string_t*)type_spec;
      if (string_type->bounded) {
        char size_string[30];
        os_ulltostr(string_type->max + 1, size_string, 30, NULL);
        output(stream, "char $D[$S]", 'D', decl_name, 'S', size_string, '\0');
      }
      else
        output(stream, "char * $D", 'D', decl_name, '\0');
      break;
    }
    case DDS_TS_STRUCT: {
      dds_ts_struct_t *struct_type = (dds_ts_struct_t*)type_spec;
      const char *struct_name = name_with_module_prefix(&struct_type->def);
      if (struct_name != NULL) {
        output(stream, "$S $D", 'S', struct_name, 'D', decl_name, '\0');
        os_free((void*)struct_name);
      }
      break;
    }
    case DDS_TS_SEQUENCE: {
      dds_ts_sequence_t *sequence_type = (dds_ts_sequence_t*)type_spec;
      if (   sequence_type->element_type.type_spec->node.flags == DDS_TS_STRUCT
          || sequence_type->element_type.type_spec->node.flags == DDS_TS_FORWARD_STRUCT
          || sequence_type->element_type.type_spec->node.flags == DDS_TS_SEQUENCE) {
        dds_ts_node_t *struct_node = dds_ts_node_get_parent(&declarator->type_spec.node, DDS_TS_STRUCT);
        if (struct_node != NULL) {
          const char *full_name = name_with_module_prefix(&((dds_ts_struct_t*)struct_node)->def);
          if (full_name != NULL) {
            output(stream, "$N_$D_seq $D", 'N', full_name, 'D', decl_name, '\0');
            os_free((void*)full_name);
          }
        }
      }
      else
        output(stream, "dds_sequence_t $D", 'D', decl_name, '\0');
      break;
    }
    default:
      output(stream, "// type not supported: $D", 'D', decl_name, '\0');
      break;
  }
}

static void write_header_close_array(void *context, unsigned long long size)
{
  ostream_t *stream = (ostream_t*)context;

  char size_string[30];
  os_ulltostr(size, size_string, 30, NULL);
  output(stream, "[$S]", 'S', size_string, '\0');
}

static void write_header_close_struct_declarator(void *context)
{
  ostream_t *stream = (ostream_t*)context;

  output(stream, ";\n", '\0');
}

static void write_header_seq_struct_with_name(void *context, dds_ts_struct_t *base_type, const char *decl_name, const char *element_type_name)
{
  ostream_t *stream = (ostream_t*)context;
  const char *base_name = name_with_module_prefix(&base_type->def);
  if (base_name == NULL)
    return;

  output(stream,
         "typedef struct $B_$D_seq\n"
         "{\n"
         "  uint32_t _maximum;\n"
         "  uint32_t _length;\n"
         "  $E *_buffer;\n"
         "  bool _release;\n"
         "} $B_$D_seq;\n"
         "\n"
         "#define $B_$D_seq__alloc() \\\n"
         "(($B_$D_seq*) dds_alloc (sizeof ($B_$D_seq)));\n"
         "\n"
         "#define $B_$D_seq_allocbuf(l) \\\n"
         "(($E *) dds_alloc ((l) * sizeof ($E)))\n"
         "\n",
         'B', base_name,
         'D', decl_name,
         'E', element_type_name,
         '\0');

  os_free((void*)base_name);
}

static void write_header_seq_struct(void *context, dds_ts_struct_t *base_type, const char *decl_name, dds_ts_struct_t *element_type)
{
  const char *element_type_name = name_with_module_prefix(&element_type->def);
  if (element_type_name == NULL)
    return;

  write_header_seq_struct_with_name(context, base_type, decl_name, element_type_name);

  os_free((void*)element_type_name);
}

static void write_header_forward_struct(void *context, dds_ts_struct_t *struct_def)
{
  ostream_t *stream = (ostream_t*)context;

  const char *full_name = name_with_module_prefix(&struct_def->def);
  if (full_name == NULL)
    return;

  output(stream,
         "typedef struct $N $N;\n"
         "\n",
         'N', full_name,
         '\0');

  os_free((void*)full_name);
}


static void generate_header_file(const char* file_name, dds_ts_node_t *root_node, ostream_t *stream)
{
  OS_UNUSED_ARG(root_node);
  const char *h_file_name = output_file_name(file_name, "h");
  if (h_file_name == NULL)
    return;
  if (!ostream_open(stream, h_file_name)) {
    fprintf(stderr, "Could not open file '%s' for writing\n", h_file_name);
    return;
  }

  write_copyright_header(stream, h_file_name, file_name);
  write_header_intro(stream, file_name);
  traverse_funcs_t write_funcs;
  traverse_funcs_init(&write_funcs);
  write_funcs.forward_struct = write_header_forward_struct;
  write_funcs.unembedded_sequence = write_header_seq_struct;
  write_funcs.unembedded_sequence_with_name = write_header_seq_struct_with_name;
  write_funcs.open_struct = write_header_open_struct;
  write_funcs.close_struct = write_header_close_struct;
  write_funcs.open_struct_declarator_with_type = write_header_open_struct_declarator_with_type;
  write_funcs.close_struct_declarator = write_header_close_struct_declarator;
  write_funcs.close_array = write_header_close_array;
  write_funcs.unembed_embedded_structs = true;
  traverse_admin_t admin;
  init_traverse_admin(&admin);
  traverse_modules((void*)stream, &admin, &write_funcs, root_node);
  free_traverse_admin(&admin);
  write_header_close(stream, file_name);

  ostream_close(stream);
  os_free((void*)h_file_name);
}

/* Generate source file */

static void write_meta_data_open_module(void *context, dds_ts_module_t *module)
{
  output((ostream_t*)context, "<Module name=\\\"$N\\\">", 'N', module->def.name, '\0');
}

static void write_meta_data_close_module(void *context)
{
  output((ostream_t*)context, "</Module>", '\0');
}

static void write_meta_data_open_struct(void *context, dds_ts_struct_t *struct_def)
{
  output((ostream_t*)context, "<Struct name=\\\"$N\\\">", 'N', struct_def->def.name, '\0');
}

static void write_meta_data_close_struct(void *context, dds_ts_struct_t *struct_def)
{
  OS_UNUSED_ARG(struct_def);
  output((ostream_t*)context, "</Struct>", '\0');
}

static void write_meta_data_open_struct_declarator(void *context, dds_ts_definition_t *declarator)
{
  output((ostream_t*)context, "<Member name=\\\"$N\\\">", 'N', declarator->name, '\0');
}

static void write_meta_data_close_struct_declarator(void *context)
{
  output((ostream_t*)context, "</Member>", '\0');
}

static void write_meta_data_open_array(void *context, unsigned long long size)
{
  char size_string[30];
  os_ulltostr(size, size_string, 30, NULL);
  output((ostream_t*)context, "<Array size=\\\"$S\\\">", 'S', size_string, '\0');
}

static void write_meta_data_close_array(void *context, unsigned long long size)
{
  OS_UNUSED_ARG(size);
  output((ostream_t*)context, "</Array>", '\0');
}

static void write_meta_data_basic_type(void *context, dds_ts_type_spec_t *type_spec)
{
  ostream_t *stream = (ostream_t*)context;
  switch (type_spec->node.flags) {
    case DDS_TS_SHORT_TYPE:              output(stream, "<Short/>", '\0'); break;
    case DDS_TS_LONG_TYPE:               output(stream, "<Long/>", '\0'); break;
    case DDS_TS_LONG_LONG_TYPE:          output(stream, "<LongLong/>", '\0'); break;
    case DDS_TS_UNSIGNED_SHORT_TYPE:     output(stream, "<UShort/>", '\0'); break;
    case DDS_TS_UNSIGNED_LONG_TYPE:      output(stream, "<ULong/>", '\0'); break;
    case DDS_TS_UNSIGNED_LONG_LONG_TYPE: output(stream, "<ULongLong/>", '\0'); break;
    case DDS_TS_CHAR_TYPE:               output(stream, "<Char/>", '\0'); break;
    case DDS_TS_BOOLEAN_TYPE:            output(stream, "<Boolean/>", '\0'); break;
    case DDS_TS_OCTET_TYPE:              output(stream, "<Octet/>", '\0'); break;
    case DDS_TS_INT8_TYPE:               output(stream, "<Int8/>", '\0'); break;
    case DDS_TS_UINT8_TYPE:              output(stream, "<UInt8/>", '\0'); break;
    case DDS_TS_FLOAT_TYPE:              output(stream, "<Float/>", '\0'); break;
    case DDS_TS_DOUBLE_TYPE:             output(stream, "<Double/>", '\0'); break;
    case DDS_TS_STRING: {
      dds_ts_string_t *string_type = (dds_ts_string_t*)type_spec;
      if (string_type->bounded) {
        char size_string[30];
        os_ulltostr(string_type->max, size_string, 30, NULL);
        output(stream, "<String length=\\\"$S\\\"/>", 'S', size_string, '\0');
      }
      else
        output(stream, "<String/>", '\0');
      break;
    }
  }
}

static void write_meta_data_type_reference(void *context, const char *name)
{
  output((ostream_t*)context, "<Type name=\\\"$T\\\"/>", 'T', name, '\0');
}

static void write_meta_data_open_sequence(void *context)
{
  output((ostream_t*)context, "<Sequence>", '\0');
}

static void write_meta_data_close_sequence(void *context)
{
  output((ostream_t*)context, "</Sequence>", '\0');
}

static void write_meta_data(ostream_t *stream, dds_ts_struct_t *struct_def)
{
  /* Determine which structs should be include */
  included_node_t *included_nodes = NULL;
  find_used_structs(&included_nodes, struct_def);

  dds_ts_node_t *root_node = &struct_def->def.type_spec.node;
  while (root_node->parent != NULL)
    root_node = root_node->parent;

  output(stream, "<MetaData version=\\\"1.0.0\\\">", '\0');
  traverse_funcs_t write_meta_data;
  traverse_funcs_init(&write_meta_data);
  write_meta_data.open_module = write_meta_data_open_module;
  write_meta_data.close_module = write_meta_data_close_module;
  write_meta_data.open_struct = write_meta_data_open_struct;
  write_meta_data.close_struct = write_meta_data_close_struct;
  write_meta_data.open_struct_declarator = write_meta_data_open_struct_declarator;
  write_meta_data.close_struct_declarator = write_meta_data_close_struct_declarator;
  write_meta_data.open_array = write_meta_data_open_array;
  write_meta_data.close_array = write_meta_data_close_array;
  write_meta_data.basic_type = write_meta_data_basic_type;
  write_meta_data.type_reference = write_meta_data_type_reference;
  write_meta_data.open_sequence = write_meta_data_open_sequence;
  write_meta_data.close_sequence = write_meta_data_close_sequence;
  traverse_admin_t admin;
  init_traverse_admin(&admin);
  /* FIXME: pass included_nodes */
  traverse_modules((void*)stream, &admin, &write_meta_data, root_node);
  free_traverse_admin(&admin);
  output(stream, "</MetaData>", '\0');

  included_nodes_free(included_nodes);
}


static void write_source_struct(void *context, dds_ts_struct_t *struct_def)
{
  if (struct_def->part_of)
    return;

  ostream_t *stream = (ostream_t*)context;

  const char *full_name = name_with_module_prefix(&struct_def->def);
  if (full_name == NULL)
    return;

  output(stream,
         "\n\n"
         "static const dds_key_descriptor_t $N_keys[0] =\n"
         "{\n"
         "};\n"
         "\n"
         "static const uint32_t $N_ops [] =\n"
         "{\n"
         "};\n"
         "\n"
         "const dds_topic_descriptor_t $N_desc =\n"
         "{\n"
         "  sizeof ($N),\n"
         "  8u,\n"
         "  DDS_TOPIC_FIXED_KEY | DDS_TOPIC_NO_OPTIMIZE,\n"
         "  3u,\n"
         "  \"\",\n"
         "  $N_keys,\n"
         "  45,\n"
         "  $N_ops,\n"
         "  \"",
         'N', full_name,
         '\0');
  write_meta_data(stream, struct_def);
  output(stream,
         "\"\n"
         "};\n",
         '\0');

  os_free((void*)full_name);
}


static void generate_source_file(const char* file_name, dds_ts_node_t *root_node, ostream_t *stream)
{
  OS_UNUSED_ARG(root_node);
  const char *c_file_name = output_file_name(file_name, "c");
  if (c_file_name == NULL)
    return;
  if (!ostream_open(stream, c_file_name)) {
    fprintf(stderr, "Could not open file '%s' for writing\n", c_file_name);
    return;
  }
  const char *h_file_name = output_file_name(file_name, "h");
  if (h_file_name == NULL)
    return;

  write_copyright_header(stream, c_file_name, file_name);
  output(stream, "#include \"$F\"\n\n\n\n", 'F', h_file_name, '\0');
  traverse_funcs_t write_funcs;
  traverse_funcs_init(&write_funcs);
  write_funcs.open_struct = write_source_struct;
  traverse_admin_t admin;
  init_traverse_admin(&admin);
  traverse_modules((void*)stream, &admin, &write_funcs, root_node);
  free_traverse_admin(&admin);

  ostream_close(stream);
  os_free((void*)h_file_name);
  os_free((void*)c_file_name);
}

void dds_ts_generate_C99(const char* file_name, dds_ts_node_t *root_node)
{
  FILE *fout;
  ostream_t stream;
  init_file_ostream(&stream, &fout);
  generate_header_file(file_name, root_node, &stream);
  generate_source_file(file_name, root_node, &stream);
}

void dds_ts_generate_C99_to_buffer(const char* file_name, dds_ts_node_t *root_node, char *buffer, size_t len)
{
  output_buffer_t output_buffer;
  output_buffer.s = buffer;
  output_buffer.e = buffer + len - 1;
  ostream_t stream;
  init_buffer_ostream(&stream, &output_buffer);
  generate_header_file(file_name, root_node, &stream);
  generate_source_file(file_name, root_node, &stream);
  *output_buffer.s = '\0';
}

