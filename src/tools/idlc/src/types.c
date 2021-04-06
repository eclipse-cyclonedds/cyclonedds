/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "idl/stream.h"
#include "idl/string.h"
#include "idl/processor.h"

#include "generator.h"
#include "descriptor.h"

extern char *typename(const void *node);

static idl_retcode_t
emit_implicit_sequence(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  struct generator *gen = user_data;
  char *name, *type, *macro, dims[32] = "";
  const char *fmt, *star = "", *lpar = "", *rpar = "";
  const idl_type_spec_t *type_spec = idl_type_spec(node);

  (void)pstate;
  (void)path;
  if (revisit) {
    assert(idl_is_sequence(node));
  } else if (idl_is_sequence(node)) {
    if (idl_is_sequence(type_spec))
      return IDL_VISIT_REVISIT | IDL_VISIT_TYPE_SPEC;
  } else {
    assert(idl_is_member(node));
    if (!idl_is_sequence(type_spec))
      return IDL_VISIT_DONT_RECURSE;
    return IDL_VISIT_TYPE_SPEC;
  }

  /* strings are special */
  if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
    lpar = "(";
    rpar = ")";
    if (idl_is_bounded(type_spec))
      idl_snprintf(dims, sizeof(dims), "[%"PRIu32"]", idl_bound(type_spec)+1);
  } else if (idl_is_string(type_spec)) {
    star = "*";
  }

  /* https://www.omg.org/spec/C/1.0/PDF section 1.11 */
  if (!(name = AUTO(typename(node))))
    return IDL_RETCODE_NO_MEMORY;
  if (!(type = AUTO(typename(type_spec))))
    return IDL_RETCODE_NO_MEMORY;
  if (!(macro = AUTO(idl_strdup(name))))
    return IDL_RETCODE_NO_MEMORY;;
  for (char *ptr=macro; *ptr; ptr++)
    if (idl_islower((unsigned char)*ptr))
      *ptr = (char)idl_toupper((unsigned char)*ptr);
  fmt = "#ifndef %1$s_DEFINED\n"
        "#define %1$s_DEFINED\n"
        "typedef struct %2$s\n{\n"
        "  uint32_t _maximum;\n"
        "  uint32_t _length;\n"
        "  %3$s %4$s%5$s*_buffer%6$s%7$s;\n"
        "  bool _release;\n"
        "} %2$s;\n\n"
        "#define %2$s__alloc() \\\n"
        "((%2$s*) dds_alloc (sizeof (%2$s)));\n\n"
        "#define %2$s_allocbuf(l) \\\n"
        "((%3$s %4$s%5$s*%6$s%7$s) dds_alloc ((l) * sizeof (%3$s%4$s%7$s)))\n"
        "#endif /* %1$s_DEFINED */\n\n";
  if (idl_fprintf(gen->header.handle, fmt, macro, name, type, star, lpar, rpar, dims) < 0)
    return IDL_RETCODE_NO_MEMORY;

  return IDL_VISIT_DONT_RECURSE;
}

static idl_retcode_t
generate_implicit_sequences(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  idl_visitor_t visitor;

  (void)pstate;
  (void)revisit;
  (void)path;
  memset(&visitor, 0, sizeof(visitor));
  visitor.visit = IDL_MEMBER | IDL_SEQUENCE;
  visitor.accept[IDL_ACCEPT] = &emit_implicit_sequence;
  assert(idl_is_member(node) || idl_is_sequence(node));
  if ((ret = idl_visit(pstate, node, &visitor, user_data)) < 0)
    return ret;
  return IDL_RETCODE_OK;
}

/* members with multiple declarators result in multiple members */
static idl_retcode_t
emit_field(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  struct generator *gen = user_data;
  char *type, dims[32] = "";
  const char *fmt, *indent, *name, *star = "";
  const void *root;
  idl_literal_t *literal;
  idl_type_spec_t *type_spec;

  (void)pstate;
  (void)revisit;
  (void)path;
  root = idl_parent(node);
  indent = idl_is_case(root) ? "    " : "  ";

  name = idl_identifier(node);
  type_spec = idl_type_spec(node);
  if (!(type = AUTO(typename(type_spec))))
    return IDL_RETCODE_NO_MEMORY;

  /* strings are special */
  if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
    idl_snprintf(dims, sizeof(dims), "[%"PRIu32"]", idl_bound(type_spec)+1);
  } else if (idl_is_string(type_spec)) {
    star = "* ";
  }

  fmt = "%s%s %s%s%s";
  if (idl_fprintf(gen->header.handle, fmt, indent, type, star, name, dims) < 0)
    return IDL_RETCODE_NO_MEMORY;
  fmt = "[%" PRIu32 "]";
  literal = ((const idl_declarator_t *)node)->const_expr;
  for (; literal; literal = idl_next(literal)) {
    assert(idl_type(literal) == IDL_ULONG);
    if (idl_fprintf(gen->header.handle, fmt, literal->value.uint32) < 0)
      return IDL_RETCODE_NO_MEMORY;
  }
  if (fputs(";\n", gen->header.handle) < 0)
    return IDL_RETCODE_NO_MEMORY;

  return IDL_RETCODE_OK;
}

extern idl_retcode_t generate_descriptor(const idl_pstate_t *, struct generator *, const idl_node_t *);

static idl_retcode_t
emit_struct(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret = IDL_RETCODE_NO_MEMORY;
  struct generator *gen = user_data;
  char *name = NULL;
  const char *fmt;

  if (!(name = AUTO(typename(node))))
    return IDL_RETCODE_NO_MEMORY;

  if (revisit) {
    fmt = "} %1$s;\n"
          "\n";
    if (idl_fprintf(gen->header.handle, fmt, name) < 0)
      return IDL_RETCODE_NO_MEMORY;
    if (idl_is_topic(node, (pstate->flags & IDL_FLAG_KEYLIST) != 0)) {
      fmt = "extern const dds_topic_descriptor_t %1$s_desc;\n"
            "\n"
            "#define %1$s__alloc() \\\n"
            "((%1$s*) dds_alloc (sizeof (%1$s)));\n"
            "\n"
            "#define %1$s_free(d,o) \\\n"
            "dds_sample_free ((d), &%1$s_desc, (o))\n"
            "\n";
      if (idl_fprintf(gen->header.handle, fmt, name) < 0)
        return IDL_RETCODE_NO_MEMORY;
      if ((ret = generate_descriptor(pstate, gen, node)))
        return ret;
    }
  } else {
    const idl_member_t *members = ((const idl_struct_t *)node)->members;
    /* ensure typedefs for unnamed sequences exist beforehand */
    if ((ret = generate_implicit_sequences(pstate, revisit, path, members, user_data)))
      return ret;
    fmt = "typedef struct %1$s\n"
          "{\n";
    if (idl_fprintf(gen->header.handle, fmt, name) < 0)
      return IDL_RETCODE_NO_MEMORY;
    return IDL_VISIT_REVISIT;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_union(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  struct generator *gen = user_data;
  char *name, *type;
  const char *fmt;
  const idl_switch_type_spec_t *switch_type_spec;

  (void)pstate;
  (void)path;
  assert(idl_is_union(node));
  if (!(name = AUTO(typename(node))))
    return IDL_RETCODE_NO_MEMORY;
  switch_type_spec = ((const idl_union_t *)node)->switch_type_spec;
  assert(idl_is_switch_type_spec(switch_type_spec));
  if (!(type = AUTO(typename(switch_type_spec->type_spec))))
    return IDL_RETCODE_NO_MEMORY;

  if (revisit) {
    fmt = "  } _u;\n"
          "} %1$s;\n"
          "\n"
          "#define %1$s__alloc() \\\n"
          "((%1$s*) dds_alloc (sizeof (%1$s)));\n"
          "\n";
    if (idl_fprintf(gen->header.handle, fmt, name) < 0)
      return IDL_RETCODE_NO_MEMORY;
  } else {
    fmt = "typedef struct %1$s\n"
          "{\n"
          "  %2$s _d;\n"
          "  union\n"
          "  {\n";
    if (idl_fprintf(gen->header.handle, fmt, name, type) < 0)
      return IDL_RETCODE_NO_MEMORY;
    return IDL_VISIT_REVISIT;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_sequence_typedef(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct generator *gen = user_data;
  char *type, *name, dims[32] = "";
  const char *fmt, *spc = " ", *star = "", *lpar = "", *rpar = "";
  const idl_declarator_t *declarator;
  const idl_literal_t *literal;
  const idl_type_spec_t *type_spec;

  type_spec = idl_type_spec(node);
  assert(idl_is_sequence(type_spec));
  type_spec = idl_type_spec(type_spec);
  /* ensure typedefs for implicit sequences exist beforehand */
  if (idl_is_sequence(type_spec) &&
      (ret = generate_implicit_sequences(pstate, revisit, path, type_spec, user_data)))
    return ret;

  /* strings are special */
  if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
    lpar = "(";
    rpar = ")";
    if (idl_is_bounded(type_spec))
      idl_snprintf(dims, sizeof(dims), "[%"PRIu32"]", idl_bound(type_spec)+1);
  } else if (idl_is_string(type_spec)) {
    star = "*";
  }

  if (!(type = AUTO(typename(type_spec))))
    return IDL_RETCODE_NO_MEMORY;
  declarator = ((const idl_typedef_t *)node)->declarators;
  for (; declarator; declarator = idl_next(declarator)) {
    if (!(name = AUTO(typename(declarator))))
      return IDL_RETCODE_NO_MEMORY;
    fmt = "typedef struct %1$s\n{\n"
          "  uint32_t _maximum;\n"
          "  uint32_t _length;\n"
          "  %2$s %3$s%4$s*_buffer%5$s%6$s;\n"
          "  bool _release;\n"
          "} %1$s";
    if (idl_fprintf(gen->header.handle, fmt, name, type, star, lpar, rpar, dims) < 0)
      return IDL_RETCODE_NO_MEMORY;
    literal = declarator->const_expr;
    for (; literal; literal = idl_next(literal)) {
      fmt = "%s[%" PRIu32 "]";
      if (idl_fprintf(gen->header.handle, fmt, spc, literal->value.uint32) < 0)
        return IDL_RETCODE_NO_MEMORY;
    }
    fmt = ";\n\n"
          "#define %1$s__alloc() \\\n"
          "((%1$s*) dds_alloc (sizeof (%1$s)));\n\n"
          "#define %1$s_allocbuf(l) \\\n"
          "((%2$s %3$s%4$s*%5$s%6$s) dds_alloc ((l) * sizeof (%2$s%3$s%6$s)))\n";
    if (idl_fprintf(gen->header.handle, fmt, name, type, star, lpar, rpar, dims) < 0)
      return IDL_RETCODE_NO_MEMORY;
  }

  return IDL_VISIT_DONT_RECURSE;
}

static idl_retcode_t
emit_typedef(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  struct generator *gen = user_data;
  char dims[32] = "";
  const char *fmt;
  const char *name = NULL, *type = NULL, *star = "";
  const idl_declarator_t *declarator;
  const idl_literal_t *literal;
  const idl_type_spec_t *type_spec;

  type_spec = idl_type_spec(node);
  /* typedef of sequence requires a little magic */
  if (idl_is_sequence(type_spec))
    return emit_sequence_typedef(pstate, revisit, path, node, user_data);
  if (idl_is_string(type_spec) && idl_is_bounded(type_spec))
    idl_snprintf(dims, sizeof(dims), "[%" PRIu32 "]", idl_bound(type_spec)+1);
  else if (idl_is_string(type_spec))
    star = "*";
  if (!(type = AUTO(typename(type_spec))))
    return IDL_RETCODE_NO_MEMORY;
  declarator = ((const idl_typedef_t *)node)->declarators;
  for (; declarator; declarator = idl_next(declarator)) {
    if (!(name = AUTO(typename(declarator))))
      return IDL_RETCODE_NO_MEMORY;
    fmt = "typedef %1$s %2$s%3$s%4$s";
    if (idl_fprintf(gen->header.handle, fmt, type, star, name, dims) < 0)
      return IDL_RETCODE_NO_MEMORY;
    literal = declarator->const_expr;
    for (; literal; literal = idl_next(literal)) {
      fmt = "[%" PRIu32 "]";
      if (idl_fprintf(gen->header.handle, fmt, literal->value.uint32) < 0)
        return IDL_RETCODE_NO_MEMORY;
    }
    fmt = ";\n\n"
          "#define %1$s__alloc() \\\n"
          "((%1$s*) dds_alloc (sizeof (%1$s)));\n\n";
    if (idl_fprintf(gen->header.handle, fmt, name) < 0)
      return IDL_RETCODE_NO_MEMORY;
  }

  return IDL_VISIT_DONT_RECURSE;
}

static idl_retcode_t
emit_enum(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  struct generator *gen = user_data;
  const char *name = NULL, *type = NULL;
  const char *fmt, *sep = "";
  const idl_enumerator_t *enumerator;
  uint32_t skip = 0, value = 0;

  (void)pstate;
  (void)revisit;
  (void)path;
  if (!(type = AUTO(typename(node))))
    return IDL_RETCODE_NO_MEMORY;
  if (idl_fprintf(gen->header.handle, "typedef enum %s\n{\n", type) < 0)
    return IDL_RETCODE_NO_MEMORY;

  enumerator = ((const idl_enum_t *)node)->enumerators;
  for (; enumerator; enumerator = idl_next(enumerator)) {
    if (!(name = AUTO(typename(enumerator))))
      return IDL_RETCODE_NO_MEMORY;
    value = enumerator->value;
    /* FIXME: IDL 3.5 did not support fixed enumerator values */
    if (value == skip)
      fmt = "%s  %s";
    else
      fmt = "%s  %s = %" PRIu32;
    if (idl_fprintf(gen->header.handle, fmt, sep, name, value) < 0)
      return IDL_RETCODE_NO_MEMORY;
    sep = ",\n";
    skip = value + 1;
  }

  fmt = "\n} %1$s;\n\n"
        "#define %1$s__alloc() \\\n"
        "((%1$s*) dds_alloc (sizeof (%1$s)));\n\n";
  if (idl_fprintf(gen->header.handle, fmt, type) < 0)
    return IDL_RETCODE_NO_MEMORY;

  return IDL_VISIT_DONT_RECURSE;
}

static int
print_literal(
  const idl_pstate_t *pstate,
  struct generator *gen,
  const idl_literal_t *literal)
{
  idl_type_t type;
  FILE *fp = gen->header.handle;

  (void)pstate;
  switch ((type = idl_type(literal))) {
    case IDL_CHAR:
      return idl_fprintf(fp, "'%c'", literal->value.chr);
    case IDL_BOOL:
      return idl_fprintf(fp, "%s", literal->value.bln ? "true" : "false");
    case IDL_INT8:
      return idl_fprintf(fp, "%" PRId8, literal->value.int8);
    case IDL_OCTET:
    case IDL_UINT8:
      return idl_fprintf(fp, "%" PRIu8, literal->value.uint8);
    case IDL_SHORT:
    case IDL_INT16:
      return idl_fprintf(fp, "%" PRId16, literal->value.int16);
    case IDL_USHORT:
    case IDL_UINT16:
      return idl_fprintf(fp, "%" PRIu16, literal->value.uint16);
    case IDL_LONG:
    case IDL_INT32:
      return idl_fprintf(fp, "%" PRId32, literal->value.int32);
    case IDL_ULONG:
    case IDL_UINT32:
      return idl_fprintf(fp, "%" PRIu32, literal->value.uint32);
    case IDL_LLONG:
    case IDL_INT64:
      return idl_fprintf(fp, "%" PRId64, literal->value.int64);
    case IDL_ULLONG:
    case IDL_UINT64:
      return idl_fprintf(fp, "%" PRIu64, literal->value.uint64);
    case IDL_FLOAT:
      return idl_fprintf(fp, "%.6f", literal->value.flt);
    case IDL_DOUBLE:
      return idl_fprintf(fp, "%f", literal->value.dbl);
    case IDL_LDOUBLE:
      return idl_fprintf(fp, "%lf", literal->value.ldbl);
    case IDL_STRING:
      return idl_fprintf(fp, "\"%s\"", literal->value.str);
    default: {
      const char *name;
      assert(type == IDL_ENUM);
      if (!(name = AUTO(typename(literal))))
        return -1;
      return idl_fprintf(fp, "%s", name);
    }
  }
}

static idl_retcode_t
emit_const(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  struct generator *gen = user_data;
  char *type;
  const char *lparen = "", *rparen = "";
  const idl_literal_t *literal = ((const idl_const_t *)node)->const_expr;

  (void)revisit;
  (void)path;
  if (!(type = AUTO(typename(node))))
    return IDL_RETCODE_NO_MEMORY;
  switch (idl_type(literal)) {
    case IDL_CHAR:
    case IDL_STRING:
      lparen = "(";
      rparen = ")";
      break;
    default:
      break;
  }
  if (idl_fprintf(gen->header.handle, "#define %s %s", type, lparen) < 0)
    return IDL_RETCODE_NO_MEMORY;
  if (print_literal(pstate, gen, literal) < 0)
    return IDL_RETCODE_NO_MEMORY;
  if (idl_fprintf(gen->header.handle, "%s\n", rparen) < 0)
    return IDL_RETCODE_NO_MEMORY;
  return IDL_RETCODE_OK;
}

idl_retcode_t generate_types(const idl_pstate_t *pstate, struct generator *generator);

idl_retcode_t generate_types(const idl_pstate_t *pstate, struct generator *generator)
{
  idl_retcode_t ret;
  idl_visitor_t visitor;

  memset(&visitor, 0, sizeof(visitor));
  visitor.visit = IDL_CONST | IDL_TYPEDEF | IDL_STRUCT | IDL_UNION | IDL_ENUM | IDL_DECLARATOR;
  visitor.accept[IDL_ACCEPT_CONST] = &emit_const;
  visitor.accept[IDL_ACCEPT_TYPEDEF] = &emit_typedef;
  visitor.accept[IDL_ACCEPT_STRUCT] = &emit_struct;
  visitor.accept[IDL_ACCEPT_UNION] = &emit_union;
  visitor.accept[IDL_ACCEPT_ENUM] = &emit_enum;
  visitor.accept[IDL_ACCEPT_DECLARATOR] = &emit_field;
  visitor.sources = (const char *[]){ pstate->sources->path->name, NULL };
  if ((ret = idl_visit(pstate, pstate->root, &visitor, generator)))
    return ret;
  return IDL_RETCODE_OK;
}
