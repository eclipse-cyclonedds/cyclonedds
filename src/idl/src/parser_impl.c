// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "idl/heap.h"
#include "idl/processor.h"
#include "idl/string.h"

#include "directive.h"
#include "parser_impl.h"
#include "parser.h"
#include "scanner.h"
#include "scope.h"
#include "symbol.h"
#include "tree.h"

typedef struct idl_parser_stream {
  idl_pstate_t *pstate;
  idl_token_t token;
  bool have_token;
} idl_parser_stream_t;

static bool
token_has_owned_string(const idl_token_t *token)
{
  switch (token->code) {
    case IDL_TOKEN_IDENTIFIER:
    case IDL_TOKEN_STRING_LITERAL:
    case IDL_TOKEN_PP_NUMBER:
    case IDL_TOKEN_COMMENT:
    case IDL_TOKEN_LINE_COMMENT:
      return token->value.str != NULL;
    default:
      return false;
  }
}

static void
token_fini(idl_token_t *token)
{
  if (token_has_owned_string(token))
    idl_free(token->value.str);
  memset(token, 0, sizeof(*token));
}

static void
stream_init(idl_parser_stream_t *stream, idl_pstate_t *pstate)
{
  memset(stream, 0, sizeof(*stream));
  stream->pstate = pstate;
}

static void
stream_fini(idl_parser_stream_t *stream)
{
  if (stream->have_token)
    token_fini(&stream->token);
}

static idl_retcode_t
stream_advance(idl_parser_stream_t *stream)
{
  idl_pstate_t *pstate = stream->pstate;

  for (;;) {
    idl_retcode_t ret;

    if (stream->have_token)
      token_fini(&stream->token);

    ret = idl_scan(pstate, &stream->token);
    stream->have_token = true;
    if (ret < 0)
      return ret;

    if (stream->token.code == '\n') {
      pstate->scanner.state = IDL_SCAN;
      continue;
    }

    if (stream->token.code == IDL_TOKEN_COMMENT ||
        stream->token.code == IDL_TOKEN_LINE_COMMENT) {
      continue;
    }

    if ((unsigned)pstate->scanner.state & (unsigned)IDL_SCAN_DIRECTIVE) {
      ret = idl_parse_directive(pstate, &stream->token);
      if (stream->token.code == '\0' &&
          (ret == IDL_RETCODE_OK || ret == IDL_RETCODE_PUSH_MORE))
        return IDL_RETCODE_OK;
      if (ret != IDL_RETCODE_OK && ret != IDL_RETCODE_PUSH_MORE)
        return ret;
      continue;
    }

    return IDL_RETCODE_OK;
  }
}

static idl_location_t
location_span(idl_position_t first, idl_position_t last)
{
  idl_location_t location;
  location.first = first;
  location.last = last;
  return location;
}

static idl_retcode_t
syntax_error(idl_parser_stream_t *stream)
{
  idl_error(stream->pstate, &stream->token.location, "syntax error");
  return IDL_RETCODE_SYNTAX_ERROR;
}

static idl_retcode_t
expect(idl_parser_stream_t *stream, int32_t code, idl_location_t *location)
{
  if (stream->token.code != code)
    return syntax_error(stream);
  if (location)
    *location = stream->token.location;
  return stream_advance(stream);
}

static idl_retcode_t
parse_identifier(idl_parser_stream_t *stream, idl_name_t **namep)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_name_t *name = NULL;
  char *identifier;
  size_t offset;
  bool nocase;
  idl_retcode_t ret;

  if (stream->token.code != IDL_TOKEN_IDENTIFIER)
    return syntax_error(stream);

  identifier = stream->token.value.str;
  nocase = (pstate->config.flags & IDL_FLAG_CASE_SENSITIVE) == 0;
  offset = (identifier[0] == '_');
  if (!offset && idl_iskeyword(pstate, identifier, nocase)) {
    idl_error(pstate, &stream->token.location,
      "Identifier '%s' collides with a keyword", identifier);
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  if (!(identifier = idl_strdup(stream->token.value.str + offset)))
    return IDL_RETCODE_NO_MEMORY;
  ret = idl_create_name(
    pstate, &stream->token.location, identifier, false, &name);
  if (ret != IDL_RETCODE_OK) {
    idl_free(identifier);
    return ret;
  }

  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK) {
    idl_delete_name(name);
    return ret;
  }

  *namep = name;
  return IDL_RETCODE_OK;
}

static idl_retcode_t parse_definition(idl_parser_stream_t *stream, void **nodep);
static idl_retcode_t parse_type_spec(
  idl_parser_stream_t *stream,
  idl_type_spec_t **type_specp);
static idl_retcode_t parse_positive_int_literal(
  idl_parser_stream_t *stream,
  idl_literal_t **literalp);

static idl_retcode_t
parse_scoped_name(
  idl_parser_stream_t *stream,
  idl_scoped_name_t **scoped_namep)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_position_t first = stream->token.location.first;
  idl_location_t location;
  idl_scoped_name_t *scoped_name = NULL;
  idl_name_t *name = NULL;
  bool absolute = false;
  idl_retcode_t ret;

  if (stream->token.code == IDL_TOKEN_SCOPE) {
    absolute = true;
    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      return ret;
  }

  if ((ret = parse_identifier(stream, &name)) != IDL_RETCODE_OK)
    return ret;
  location = location_span(first, name->symbol.location.last);
  ret = idl_create_scoped_name(
    pstate, &location, name, absolute, &scoped_name);
  if (ret != IDL_RETCODE_OK) {
    idl_delete_name(name);
    return ret;
  }
  name = NULL;

  while (stream->token.code == IDL_TOKEN_SCOPE) {
    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      goto err;
    if ((ret = parse_identifier(stream, &name)) != IDL_RETCODE_OK)
      goto err;
    ret = idl_push_scoped_name(pstate, scoped_name, name);
    if (ret != IDL_RETCODE_OK) {
      idl_delete_name(name);
      goto err;
    }
    name = NULL;
  }

  *scoped_namep = scoped_name;
  return IDL_RETCODE_OK;
err:
  idl_delete_name(name);
  idl_delete_scoped_name(scoped_name);
  return ret;
}

static idl_retcode_t
parse_scoped_type_spec(
  idl_parser_stream_t *stream,
  idl_type_spec_t **type_specp)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_scoped_name_t *scoped_name = NULL;
  const idl_declaration_t *declaration = NULL;
  static const char fmt[] = "Scoped name '%s' does not resolve to a type";
  idl_retcode_t ret;

  if ((ret = parse_scoped_name(stream, &scoped_name)) != IDL_RETCODE_OK)
    return ret;
  ret = idl_resolve(pstate, 0u, scoped_name, &declaration);
  if (ret != IDL_RETCODE_OK)
    goto err;
  if (!declaration || !idl_is_type_spec(declaration->node)) {
    idl_error(pstate, idl_location(scoped_name), fmt, scoped_name->identifier);
    ret = IDL_RETCODE_SEMANTIC_ERROR;
    goto err;
  }

  *type_specp = idl_reference_node((idl_node_t *) declaration->node);
  idl_delete_scoped_name(scoped_name);
  return IDL_RETCODE_OK;
err:
  idl_delete_scoped_name(scoped_name);
  return ret;
}

static bool
token_starts_base_type(int32_t code)
{
  switch (code) {
    case IDL_TOKEN_UNSIGNED:
    case IDL_TOKEN_FLOAT:
    case IDL_TOKEN_DOUBLE:
    case IDL_TOKEN_SHORT:
    case IDL_TOKEN_LONG:
    case IDL_TOKEN_CHAR:
    case IDL_TOKEN_WCHAR:
    case IDL_TOKEN_BOOLEAN:
    case IDL_TOKEN_OCTET:
    case IDL_TOKEN_INT8:
    case IDL_TOKEN_INT16:
    case IDL_TOKEN_INT32:
    case IDL_TOKEN_INT64:
    case IDL_TOKEN_UINT8:
    case IDL_TOKEN_UINT16:
    case IDL_TOKEN_UINT32:
    case IDL_TOKEN_UINT64:
      return true;
    default:
      return false;
  }
}

static idl_retcode_t
parse_base_type_spec(idl_parser_stream_t *stream, idl_type_spec_t **type_specp)
{
  idl_position_t first = stream->token.location.first;
  idl_position_t last = stream->token.location.last;
  idl_location_t location;
  idl_mask_t mask;
  idl_retcode_t ret;

  switch (stream->token.code) {
    case IDL_TOKEN_FLOAT:
      mask = IDL_FLOAT;
      ret = stream_advance(stream);
      break;
    case IDL_TOKEN_DOUBLE:
      mask = IDL_DOUBLE;
      ret = stream_advance(stream);
      break;
    case IDL_TOKEN_SHORT:
      mask = IDL_SHORT;
      ret = stream_advance(stream);
      break;
    case IDL_TOKEN_CHAR:
      mask = IDL_CHAR;
      ret = stream_advance(stream);
      break;
    case IDL_TOKEN_WCHAR:
      mask = IDL_WCHAR;
      ret = stream_advance(stream);
      break;
    case IDL_TOKEN_BOOLEAN:
      mask = IDL_BOOL;
      ret = stream_advance(stream);
      break;
    case IDL_TOKEN_OCTET:
      mask = IDL_OCTET;
      ret = stream_advance(stream);
      break;
    case IDL_TOKEN_INT8:
      mask = IDL_INT8;
      ret = stream_advance(stream);
      break;
    case IDL_TOKEN_INT16:
      mask = IDL_INT16;
      ret = stream_advance(stream);
      break;
    case IDL_TOKEN_INT32:
      mask = IDL_INT32;
      ret = stream_advance(stream);
      break;
    case IDL_TOKEN_INT64:
      mask = IDL_INT64;
      ret = stream_advance(stream);
      break;
    case IDL_TOKEN_UINT8:
      mask = IDL_UINT8;
      ret = stream_advance(stream);
      break;
    case IDL_TOKEN_UINT16:
      mask = IDL_UINT16;
      ret = stream_advance(stream);
      break;
    case IDL_TOKEN_UINT32:
      mask = IDL_UINT32;
      ret = stream_advance(stream);
      break;
    case IDL_TOKEN_UINT64:
      mask = IDL_UINT64;
      ret = stream_advance(stream);
      break;
    case IDL_TOKEN_LONG:
      if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
        return ret;
      if (stream->token.code == IDL_TOKEN_LONG) {
        mask = IDL_LLONG;
        last = stream->token.location.last;
        ret = stream_advance(stream);
      } else if (stream->token.code == IDL_TOKEN_DOUBLE) {
        mask = IDL_LDOUBLE;
        last = stream->token.location.last;
        ret = stream_advance(stream);
      } else {
        mask = IDL_LONG;
        ret = IDL_RETCODE_OK;
      }
      break;
    case IDL_TOKEN_UNSIGNED:
      if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
        return ret;
      if (stream->token.code == IDL_TOKEN_SHORT) {
        mask = IDL_USHORT;
        last = stream->token.location.last;
        ret = stream_advance(stream);
      } else if (stream->token.code == IDL_TOKEN_LONG) {
        mask = IDL_ULONG;
        last = stream->token.location.last;
        if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
          return ret;
        if (stream->token.code == IDL_TOKEN_LONG) {
          mask = IDL_ULLONG;
          last = stream->token.location.last;
          ret = stream_advance(stream);
        } else {
          ret = IDL_RETCODE_OK;
        }
      } else {
        return syntax_error(stream);
      }
      break;
    default:
      return syntax_error(stream);
  }

  if (ret != IDL_RETCODE_OK)
    return ret;

  location = location_span(first, last);
  return idl_create_base_type(stream->pstate, &location, mask, type_specp);
}

static bool
token_starts_template_type(int32_t code)
{
  switch (code) {
    case IDL_TOKEN_SEQUENCE:
    case IDL_TOKEN_STRING:
    case IDL_TOKEN_WSTRING:
      return true;
    default:
      return false;
  }
}

static idl_retcode_t
parse_string_type(
  idl_parser_stream_t *stream,
  idl_type_spec_t **type_specp)
{
  idl_pstate_t *pstate = stream->pstate;
  int32_t keyword = stream->token.code;
  idl_position_t first = stream->token.location.first;
  idl_position_t last = stream->token.location.last;
  idl_location_t location;
  idl_literal_t *bound = NULL;
  idl_retcode_t ret;

  assert(keyword == IDL_TOKEN_STRING || keyword == IDL_TOKEN_WSTRING);
  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
    return ret;

  if (stream->token.code == '<') {
    idl_location_t rangle_location;

    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      return ret;
    if ((ret = parse_positive_int_literal(stream, &bound)) != IDL_RETCODE_OK)
      return ret;
    if ((ret = expect(stream, '>', &rangle_location)) != IDL_RETCODE_OK)
      goto err;
    last = rangle_location.last;
  }

  location = location_span(first, last);
  if (keyword == IDL_TOKEN_STRING)
    ret = idl_create_string(pstate, &location, bound, type_specp);
  else
    ret = idl_create_wstring(pstate, &location, bound, type_specp);
  if (ret != IDL_RETCODE_OK)
    goto err;

  return IDL_RETCODE_OK;
err:
  idl_delete_node(bound);
  return ret;
}

static idl_retcode_t
parse_sequence_type(
  idl_parser_stream_t *stream,
  idl_type_spec_t **type_specp)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_position_t first = stream->token.location.first;
  idl_location_t rangle_location;
  idl_location_t location;
  idl_type_spec_t *element_type = NULL;
  idl_literal_t *bound = NULL;
  idl_sequence_t *sequence = NULL;
  idl_retcode_t ret;

  assert(stream->token.code == IDL_TOKEN_SEQUENCE);
  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = expect(stream, '<', NULL)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_type_spec(stream, &element_type)) != IDL_RETCODE_OK)
    return ret;

  if (stream->token.code == ',') {
    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      goto err;
    if ((ret = parse_positive_int_literal(stream, &bound)) != IDL_RETCODE_OK)
      goto err;
  }

  if ((ret = expect(stream, '>', &rangle_location)) != IDL_RETCODE_OK)
    goto err;

  location = location_span(first, rangle_location.last);
  ret = idl_create_sequence(
    pstate, &location, element_type, bound, &sequence);
  if (ret != IDL_RETCODE_OK)
    goto err;

  *type_specp = (idl_type_spec_t *) sequence;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(bound);
  idl_delete_node(element_type);
  return ret;
}

static idl_retcode_t
parse_template_type_spec(
  idl_parser_stream_t *stream,
  idl_type_spec_t **type_specp)
{
  if (stream->token.code == IDL_TOKEN_STRING ||
      stream->token.code == IDL_TOKEN_WSTRING)
    return parse_string_type(stream, type_specp);
  if (stream->token.code == IDL_TOKEN_SEQUENCE)
    return parse_sequence_type(stream, type_specp);
  return syntax_error(stream);
}

static idl_retcode_t
parse_type_spec(idl_parser_stream_t *stream, idl_type_spec_t **type_specp)
{
  if (token_starts_base_type(stream->token.code))
    return parse_base_type_spec(stream, type_specp);
  if (token_starts_template_type(stream->token.code))
    return parse_template_type_spec(stream, type_specp);
  if (stream->token.code == IDL_TOKEN_IDENTIFIER ||
      stream->token.code == IDL_TOKEN_SCOPE)
    return parse_scoped_type_spec(stream, type_specp);
  return syntax_error(stream);
}

static idl_retcode_t
parse_positive_int_literal(
  idl_parser_stream_t *stream,
  idl_literal_t **literalp)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_literal_t *literal = NULL;
  unsigned long long value;
  idl_retcode_t ret;

  if (stream->token.code != IDL_TOKEN_INTEGER_LITERAL)
    return syntax_error(stream);

  value = stream->token.value.ullng;
  if (value > (unsigned long long) UINT32_MAX) {
    idl_error(pstate, &stream->token.location, "Integer expression overflows");
    return IDL_RETCODE_OUT_OF_RANGE;
  }

  ret = idl_create_literal(pstate, &stream->token.location, IDL_ULONG, &literal);
  if (ret != IDL_RETCODE_OK)
    return ret;
  literal->value.uint32 = (uint32_t) value;

  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK) {
    idl_delete_node(literal);
    return ret;
  }

  *literalp = literal;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
parse_fixed_array_sizes(
  idl_parser_stream_t *stream,
  idl_const_expr_t **sizesp,
  idl_position_t *lastp)
{
  idl_const_expr_t *sizes = NULL;
  idl_retcode_t ret;

  assert(stream->token.code == '[');
  do {
    idl_literal_t *size = NULL;
    idl_location_t rbracket_location;

    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      goto err;
    if ((ret = parse_positive_int_literal(stream, &size)) != IDL_RETCODE_OK)
      goto err;
    if ((ret = expect(stream, ']', &rbracket_location)) != IDL_RETCODE_OK) {
      idl_delete_node(size);
      goto err;
    }
    sizes = idl_push_node(sizes, size);
    *lastp = rbracket_location.last;
  } while (stream->token.code == '[');

  *sizesp = sizes;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(sizes);
  return ret;
}

static idl_retcode_t
parse_declarator(
  idl_parser_stream_t *stream,
  idl_declarator_t **declaratorp)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_declarator_t *declarator = NULL;
  idl_const_expr_t *sizes = NULL;
  idl_name_t *name = NULL;
  idl_location_t location;
  idl_position_t last;
  idl_retcode_t ret;

  if ((ret = parse_identifier(stream, &name)) != IDL_RETCODE_OK)
    return ret;
  last = name->symbol.location.last;
  if (stream->token.code == '[' &&
      (ret = parse_fixed_array_sizes(stream, &sizes, &last)) != IDL_RETCODE_OK)
    goto err;

  location = location_span(name->symbol.location.first, last);
  ret = idl_create_declarator(pstate, &location, name, sizes, &declarator);
  if (ret != IDL_RETCODE_OK) {
    goto err;
  }

  *declaratorp = declarator;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(sizes);
  idl_delete_name(name);
  return ret;
}

static idl_retcode_t
parse_declarators(
  idl_parser_stream_t *stream,
  idl_declarator_t **declaratorsp)
{
  idl_declarator_t *declarators = NULL;
  idl_retcode_t ret;

  if ((ret = parse_declarator(stream, &declarators)) != IDL_RETCODE_OK)
    return ret;

  while (stream->token.code == ',') {
    idl_declarator_t *declarator = NULL;

    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      goto err;
    if ((ret = parse_declarator(stream, &declarator)) != IDL_RETCODE_OK)
      goto err;
    declarators = idl_push_node(declarators, declarator);
  }

  *declaratorsp = declarators;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(declarators);
  return ret;
}

static idl_position_t
declarators_last_position(idl_declarator_t *declarators)
{
  idl_declarator_t *declarator = declarators;

  assert(declarator);
  while (idl_next(declarator))
    declarator = idl_next(declarator);
  return idl_location(declarator)->last;
}

static idl_retcode_t
parse_typedef(idl_parser_stream_t *stream, void **nodep)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_position_t first = stream->token.location.first;
  idl_location_t location;
  idl_type_spec_t *type_spec = NULL;
  idl_declarator_t *declarators = NULL;
  idl_typedef_t *node = NULL;
  idl_retcode_t ret;

  assert(stream->token.code == IDL_TOKEN_TYPEDEF);
  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_type_spec(stream, &type_spec)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_declarators(stream, &declarators)) != IDL_RETCODE_OK)
    goto err;

  location = location_span(first, declarators_last_position(declarators));
  ret = idl_create_typedef(
    pstate, &location, type_spec, declarators, &node);
  if (ret != IDL_RETCODE_OK)
    goto err;

  *nodep = node;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(declarators);
  idl_delete_node(type_spec);
  return ret;
}

static idl_retcode_t
parse_struct_inherit_spec(
  idl_parser_stream_t *stream,
  idl_inherit_spec_t **inherit_specp)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_scoped_name_t *scoped_name = NULL;
  const idl_declaration_t *declaration = NULL;
  idl_node_t *node;
  static const char fmt[] = "Scoped name '%s' does not resolve to a struct";
  idl_retcode_t ret;

  assert(stream->token.code == ':');
  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_scoped_name(stream, &scoped_name)) != IDL_RETCODE_OK)
    return ret;
  ret = idl_resolve(pstate, 0u, scoped_name, &declaration);
  if (ret != IDL_RETCODE_OK)
    goto err;
  if (!declaration) {
    idl_error(pstate, idl_location(scoped_name), fmt, scoped_name->identifier);
    ret = IDL_RETCODE_SEMANTIC_ERROR;
    goto err;
  }
  node = idl_unalias(declaration->node);
  if (!node || !idl_is_struct(node)) {
    idl_error(pstate, idl_location(scoped_name), fmt, scoped_name->identifier);
    ret = IDL_RETCODE_SEMANTIC_ERROR;
    goto err;
  }

  node = idl_reference_node(node);
  ret = idl_create_inherit_spec(
    pstate, idl_location(scoped_name), node, inherit_specp);
  if (ret != IDL_RETCODE_OK)
    idl_unreference_node(node);
err:
  idl_delete_scoped_name(scoped_name);
  return ret;
}

static idl_retcode_t
parse_member(idl_parser_stream_t *stream, idl_member_t **memberp)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_position_t first = stream->token.location.first;
  idl_location_t semicolon_location;
  idl_location_t location;
  idl_type_spec_t *type_spec = NULL;
  idl_declarator_t *declarators = NULL;
  idl_member_t *member = NULL;
  idl_retcode_t ret;

  if ((ret = parse_type_spec(stream, &type_spec)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_declarators(stream, &declarators)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = expect(stream, ';', &semicolon_location)) != IDL_RETCODE_OK)
    goto err;

  location = location_span(first, semicolon_location.last);
  ret = idl_create_member(
    pstate, &location, type_spec, declarators, &member);
  if (ret != IDL_RETCODE_OK)
    goto err;

  *memberp = member;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(declarators);
  idl_delete_node(type_spec);
  return ret;
}

static idl_retcode_t
parse_members(idl_parser_stream_t *stream, idl_member_t **membersp)
{
  idl_member_t *members = NULL;
  idl_retcode_t ret;

  while (stream->token.code != '}') {
    idl_member_t *member = NULL;

    if (stream->token.code == '\0') {
      ret = syntax_error(stream);
      goto err;
    }

    if ((ret = parse_member(stream, &member)) != IDL_RETCODE_OK)
      goto err;
    members = idl_push_node(members, member);
  }

  *membersp = members;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(members);
  return ret;
}

static idl_retcode_t
parse_definitions(
  idl_parser_stream_t *stream,
  int32_t stop_code,
  bool allow_empty,
  void **nodep)
{
  void *nodes = NULL;
  idl_retcode_t ret;

  while (stream->token.code != stop_code) {
    void *node = NULL;

    if (stream->token.code == '\0') {
      if (stop_code == '\0')
        break;
      ret = syntax_error(stream);
      goto err;
    }

    if ((ret = parse_definition(stream, &node)) != IDL_RETCODE_OK)
      goto err;
    nodes = idl_push_node(nodes, node);
  }

  if (!nodes && !allow_empty) {
    ret = syntax_error(stream);
    goto err;
  }

  *nodep = nodes;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(nodes);
  return ret;
}

static idl_retcode_t
parse_struct(idl_parser_stream_t *stream, void **nodep)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_position_t first = stream->token.location.first;
  idl_location_t keyword_location = stream->token.location;
  idl_location_t location;
  idl_location_t rbrace_location;
  idl_struct_t *strct = NULL;
  idl_inherit_spec_t *inherit_spec = NULL;
  idl_member_t *members = NULL;
  idl_name_t *name = NULL;
  bool entered_scope = false;
  idl_retcode_t ret;

  assert(stream->token.code == IDL_TOKEN_STRUCT);
  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_identifier(stream, &name)) != IDL_RETCODE_OK)
    return ret;

  if (stream->token.code == ';') {
    ret = idl_create_forward(pstate, &keyword_location, name, IDL_STRUCT, nodep);
    if (ret != IDL_RETCODE_OK)
      idl_delete_name(name);
    return ret;
  }

  if (stream->token.code == ':' &&
      (ret = parse_struct_inherit_spec(
        stream, &inherit_spec)) != IDL_RETCODE_OK) {
    idl_delete_name(name);
    return ret;
  }

  if (stream->token.code != '{') {
    ret = syntax_error(stream);
    idl_delete_node(inherit_spec);
    idl_delete_name(name);
    return ret;
  }

  location = location_span(
    first, inherit_spec ?
      idl_location(inherit_spec)->last : name->symbol.location.last);
  ret = idl_create_struct(pstate, &location, name, inherit_spec, &strct);
  if (ret != IDL_RETCODE_OK) {
    idl_delete_node(inherit_spec);
    idl_delete_name(name);
    return ret;
  }
  name = NULL;
  entered_scope = true;

  if ((ret = expect(stream, '{', NULL)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = parse_members(stream, &members)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = expect(stream, '}', &rbrace_location)) != IDL_RETCODE_OK)
    goto err;

  location = location_span(first, rbrace_location.last);
  if ((ret = idl_finalize_struct(
        pstate, &location, strct, members)) != IDL_RETCODE_OK)
    goto err;
  entered_scope = false;
  members = NULL;

  *nodep = strct;
  return IDL_RETCODE_OK;
err:
  if (entered_scope)
    idl_exit_scope(pstate);
  idl_delete_node(members);
  idl_delete_node(strct);
  return ret;
}

static idl_retcode_t
parse_module(idl_parser_stream_t *stream, void **nodep)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_position_t first = stream->token.location.first;
  idl_location_t location;
  idl_location_t rbrace_location;
  idl_module_t *module = NULL;
  idl_name_t *name = NULL;
  void *definitions = NULL;
  bool entered_scope = false;
  idl_retcode_t ret;

  assert(stream->token.code == IDL_TOKEN_MODULE);
  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_identifier(stream, &name)) != IDL_RETCODE_OK)
    return ret;

  location = location_span(first, name->symbol.location.last);
  ret = idl_create_module(pstate, &location, name, &module);
  if (ret != IDL_RETCODE_OK) {
    idl_delete_name(name);
    return ret;
  }
  name = NULL;
  entered_scope = true;

  if ((ret = expect(stream, '{', NULL)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = parse_definitions(
        stream, '}', false, &definitions)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = expect(stream, '}', &rbrace_location)) != IDL_RETCODE_OK)
    goto err;

  location = location_span(first, rbrace_location.last);
  if ((ret = idl_finalize_module(
        pstate, &location, module, definitions)) != IDL_RETCODE_OK)
    goto err;
  entered_scope = false;
  definitions = NULL;

  *nodep = module;
  return IDL_RETCODE_OK;
err:
  if (entered_scope)
    idl_exit_scope(pstate);
  idl_delete_node(definitions);
  idl_delete_node(module);
  return ret;
}

static idl_retcode_t
parse_definition(idl_parser_stream_t *stream, void **nodep)
{
  void *node = NULL;
  idl_retcode_t ret;

  switch (stream->token.code) {
    case IDL_TOKEN_MODULE:
      ret = parse_module(stream, &node);
      break;
    case IDL_TOKEN_STRUCT:
      ret = parse_struct(stream, &node);
      break;
    case IDL_TOKEN_TYPEDEF:
      ret = parse_typedef(stream, &node);
      break;
    default:
      return syntax_error(stream);
  }

  if (ret != IDL_RETCODE_OK)
    return ret;
  if ((ret = expect(stream, ';', NULL)) != IDL_RETCODE_OK) {
    idl_delete_node(node);
    return ret;
  }

  *nodep = node;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
parse_specification(idl_parser_stream_t *stream)
{
  idl_pstate_t *pstate = stream->pstate;
  void *definitions = NULL;
  idl_retcode_t ret;

  if ((ret = parse_definitions(
        stream, '\0', true, &definitions)) != IDL_RETCODE_OK)
    return ret;

  if (definitions) {
    pstate->root = pstate->root ?
      idl_push_node(pstate->root, definitions) : definitions;
    return IDL_RETCODE_OK;
  }

  if (!pstate->root)
    pstate->root = NULL;
  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_parse_hand_written(idl_pstate_t *pstate)
{
  idl_parser_stream_t stream;
  idl_retcode_t ret = IDL_RETCODE_OK;

  assert(pstate);
  stream_init(&stream, pstate);

  if ((ret = stream_advance(&stream)) == IDL_RETCODE_OK)
    ret = parse_specification(&stream);

  stream_fini(&stream);
  return ret;
}
