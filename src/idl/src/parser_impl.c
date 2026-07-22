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
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "idl/heap.h"
#include "idl/processor.h"
#include "idl/string.h"

#include "annotation.h"
#include "directive.h"
#include "expression.h"
#include "parser_impl.h"
#include "scanner.h"
#include "scope.h"
#include "symbol.h"
#include "tree.h"

typedef struct idl_parser_stream {
  idl_pstate_t *pstate;
  idl_token_t token;
  idl_token_t peek;
  bool have_token;
  bool have_peek;
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
  if (stream->have_peek)
    token_fini(&stream->peek);
}

static idl_retcode_t
stream_scan_token(idl_parser_stream_t *stream, idl_token_t *token)
{
  idl_pstate_t *pstate = stream->pstate;

  for (;;) {
    idl_retcode_t ret = idl_scan(pstate, token);
    if (ret < 0)
      return ret;

    if (token->code == IDL_TOKEN_COMMENT ||
        token->code == IDL_TOKEN_LINE_COMMENT) {
      token_fini(token);
      continue;
    }

    if ((unsigned)pstate->scanner.state & (unsigned)IDL_SCAN_DIRECTIVE) {
      bool end_of_directive = token->code == '\n';
      ret = idl_parse_directive(pstate, token);
      if (end_of_directive)
        pstate->scanner.state = IDL_SCAN;
      if (token->code == '\0' &&
          (ret == IDL_RETCODE_OK || ret == IDL_RETCODE_PUSH_MORE)) {
        token_fini(token);
        token->code = '\0';
        return IDL_RETCODE_OK;
      }
      token_fini(token);
      if (ret != IDL_RETCODE_OK && ret != IDL_RETCODE_PUSH_MORE)
        return ret;
      continue;
    }

    if (token->code == '\n') {
      pstate->scanner.state = IDL_SCAN;
      token_fini(token);
      continue;
    }

    return IDL_RETCODE_OK;
  }
}

static idl_retcode_t
stream_advance(idl_parser_stream_t *stream)
{
  idl_retcode_t ret;

  if (stream->have_token)
    token_fini(&stream->token);

  if (stream->have_peek) {
    stream->token = stream->peek;
    memset(&stream->peek, 0, sizeof(stream->peek));
    stream->have_peek = false;
    stream->have_token = true;
    return IDL_RETCODE_OK;
  }

  ret = stream_scan_token(stream, &stream->token);
  stream->have_token = true;
  return ret;
}

static idl_retcode_t
stream_peek(idl_parser_stream_t *stream, const idl_token_t **tokenp)
{
  idl_retcode_t ret;

  if (!stream->have_peek) {
    ret = stream_scan_token(stream, &stream->peek);
    stream->have_peek = true;
    if (ret != IDL_RETCODE_OK)
      return ret;
  }

  *tokenp = &stream->peek;
  return IDL_RETCODE_OK;
}

static idl_location_t
location_span(idl_position_t first, idl_position_t last)
{
  idl_location_t location;
  location.first = first;
  location.last = last;
  return location;
}

static void
release_node(void *node)
{
  if (!node)
    return;
  if (((idl_node_t *)node)->references > 0)
    idl_unreference_node(node);
  else
    idl_delete_node(node);
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
expect_template_rangle(
  idl_parser_stream_t *stream,
  idl_location_t *location)
{
  idl_location_t first;
  idl_location_t second;

  if (stream->token.code == '>')
    return expect(stream, '>', location);
  if (stream->token.code != IDL_TOKEN_RSHIFT)
    return syntax_error(stream);

  first = stream->token.location;
  second = stream->token.location;
  first.last = first.first;
  first.last.column++;
  second.first = first.last;

  if (location)
    *location = first;
  stream->token.code = '>';
  stream->token.location = second;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
parse_identifier_ex(
  idl_parser_stream_t *stream,
  bool is_annotation,
  idl_name_t **namep)
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
  offset = is_annotation ? 0u : (identifier[0] == '_');
  if (!is_annotation && !offset && idl_iskeyword(pstate, identifier, nocase)) {
    idl_error(pstate, &stream->token.location,
      "Identifier '%s' collides with a keyword", identifier);
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  if (!(identifier = idl_strdup(stream->token.value.str + offset)))
    return IDL_RETCODE_NO_MEMORY;
  ret = idl_create_name(
    pstate, &stream->token.location, identifier, is_annotation, &name);
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

static idl_retcode_t
parse_identifier(idl_parser_stream_t *stream, idl_name_t **namep)
{
  return parse_identifier_ex(stream, false, namep);
}

static idl_retcode_t parse_definition(idl_parser_stream_t *stream, void **nodep);
static idl_retcode_t parse_type_spec(
  idl_parser_stream_t *stream,
  idl_type_spec_t **type_specp);
static idl_retcode_t parse_positive_int_const(
  idl_parser_stream_t *stream,
  idl_literal_t **literalp);
static idl_retcode_t parse_const_expr(
  idl_parser_stream_t *stream,
  idl_const_expr_t **const_exprp,
  idl_location_t *locationp);
static idl_retcode_t parse_annotation_applications(
  idl_parser_stream_t *stream,
  idl_annotation_appl_t **annotationsp);

static bool
parsing_unknown_annotation_params(const idl_parser_stream_t *stream)
{
  return stream->pstate->parser.state ==
         IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS;
}

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
    if ((ret = parse_positive_int_const(stream, &bound)) != IDL_RETCODE_OK)
      return ret;
    if ((ret = expect_template_rangle(stream, &rangle_location)) !=
        IDL_RETCODE_OK)
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
  idl_annotation_appl_t *annotations = NULL;
  idl_type_spec_t *element_type = NULL;
  idl_literal_t *bound = NULL;
  idl_sequence_t *sequence = NULL;
  idl_retcode_t ret;

  assert(stream->token.code == IDL_TOKEN_SEQUENCE);
  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = expect(stream, '<', NULL)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_annotation_applications(stream, &annotations)) !=
      IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_type_spec(stream, &element_type)) != IDL_RETCODE_OK)
    goto err;

  if (stream->token.code == ',') {
    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      goto err;
    if ((ret = parse_positive_int_const(stream, &bound)) != IDL_RETCODE_OK)
      goto err;
  }

  if ((ret = expect_template_rangle(stream, &rangle_location)) !=
      IDL_RETCODE_OK)
    goto err;

  location = location_span(first, rangle_location.last);
  ret = idl_create_sequence(
    pstate, &location, element_type, bound, &sequence);
  if (ret != IDL_RETCODE_OK)
    goto err;
  element_type = NULL;
  bound = NULL;

  if (annotations &&
      (ret = idl_annotate(pstate, sequence, annotations)) != IDL_RETCODE_OK)
    goto err;
  annotations = NULL;

  *type_specp = (idl_type_spec_t *) sequence;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(sequence);
  idl_delete_node(annotations);
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
parse_integer_literal_expr(
  idl_parser_stream_t *stream,
  idl_const_expr_t **const_exprp,
  idl_location_t *locationp)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_literal_t value;
  idl_literal_t *literal = NULL;
  idl_type_t type;
  unsigned long long raw_value;
  idl_retcode_t ret;

  if (stream->token.code != IDL_TOKEN_INTEGER_LITERAL)
    return syntax_error(stream);

  memset(&value, 0, sizeof(value));
  raw_value = stream->token.value.ullng;
  if (raw_value <= (unsigned long long) INT32_MAX) {
    type = IDL_LONG;
    value.value.int32 = (int32_t) raw_value;
  } else if (raw_value <= (unsigned long long) UINT32_MAX) {
    type = IDL_ULONG;
    value.value.uint32 = (uint32_t) raw_value;
  } else if (raw_value <= (unsigned long long) INT64_MAX) {
    type = IDL_LLONG;
    value.value.int64 = (int64_t) raw_value;
  } else {
    type = IDL_ULLONG;
    value.value.uint64 = (uint64_t) raw_value;
  }

  ret = idl_create_literal(pstate, &stream->token.location, type, &literal);
  if (ret != IDL_RETCODE_OK)
    return ret;
  literal->value = value.value;
  *locationp = stream->token.location;

  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK) {
    idl_unreference_node(literal);
    return ret;
  }

  *const_exprp = (idl_const_expr_t *) literal;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
parse_char_literal_expr(
  idl_parser_stream_t *stream,
  idl_const_expr_t **const_exprp,
  idl_location_t *locationp)
{
  idl_literal_t *literal = NULL;
  idl_retcode_t ret;

  assert(stream->token.code == IDL_TOKEN_CHAR_LITERAL);
  ret = idl_create_literal(
    stream->pstate, &stream->token.location, IDL_CHAR, &literal);
  if (ret != IDL_RETCODE_OK)
    return ret;
  literal->value.chr = stream->token.value.chr;
  *locationp = stream->token.location;

  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK) {
    idl_unreference_node(literal);
    return ret;
  }

  *const_exprp = (idl_const_expr_t *) literal;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
parse_floating_literal_expr(
  idl_parser_stream_t *stream,
  idl_const_expr_t **const_exprp,
  idl_location_t *locationp)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_literal_t value;
  idl_literal_t *literal = NULL;
  long double raw_value;
  idl_type_t type;
  idl_retcode_t ret;

  assert(stream->token.code == IDL_TOKEN_FLOATING_PT_LITERAL);
  memset(&value, 0, sizeof(value));
  raw_value = stream->token.value.ldbl;
  if (isnan((double) raw_value) || isinf((double) raw_value)) {
    type = IDL_LDOUBLE;
    value.value.ldbl = raw_value;
  } else {
    type = IDL_DOUBLE;
    value.value.dbl = (double) raw_value;
  }

  ret = idl_create_literal(pstate, &stream->token.location, type, &literal);
  if (ret != IDL_RETCODE_OK)
    return ret;
  literal->value = value.value;
  *locationp = stream->token.location;

  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK) {
    idl_unreference_node(literal);
    return ret;
  }

  *const_exprp = (idl_const_expr_t *) literal;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
parse_string_literal_expr(
  idl_parser_stream_t *stream,
  idl_const_expr_t **const_exprp,
  idl_location_t *locationp)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_position_t first = stream->token.location.first;
  idl_position_t last = stream->token.location.last;
  idl_literal_t *literal = NULL;
  char *value = NULL;
  idl_retcode_t ret;

  (void) last;
  assert(stream->token.code == IDL_TOKEN_STRING_LITERAL);
  do {
    const char *part = stream->token.value.str;
    size_t len = strlen(part);

    if (value == NULL) {
      if (!(value = idl_strdup(part)))
        return IDL_RETCODE_NO_MEMORY;
    } else {
      size_t old_len = strlen(value);
      char *joined = idl_realloc(value, old_len + len + 1);
      if (!joined) {
        ret = IDL_RETCODE_NO_MEMORY;
        goto err;
      }
      value = joined;
      memmove(value + old_len, part, len);
      value[old_len + len] = '\0';
    }

    last = stream->token.location.last;
    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      goto err;
  } while (stream->token.code == IDL_TOKEN_STRING_LITERAL);

  *locationp = location_span(first, last);
  ret = idl_create_literal(pstate, locationp, IDL_STRING, &literal);
  if (ret != IDL_RETCODE_OK)
    goto err;
  literal->value.str = value;

  *const_exprp = (idl_const_expr_t *) literal;
  return IDL_RETCODE_OK;
err:
  idl_free(value);
  return ret;
}

static idl_retcode_t
parse_boolean_literal_expr(
  idl_parser_stream_t *stream,
  idl_const_expr_t **const_exprp,
  idl_location_t *locationp)
{
  idl_literal_t *literal = NULL;
  idl_retcode_t ret;

  assert(stream->token.code == IDL_TOKEN_TRUE ||
         stream->token.code == IDL_TOKEN_FALSE);
  ret = idl_create_literal(
    stream->pstate, &stream->token.location, IDL_BOOL, &literal);
  if (ret != IDL_RETCODE_OK)
    return ret;
  literal->value.bln = (stream->token.code == IDL_TOKEN_TRUE);
  *locationp = stream->token.location;

  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK) {
    idl_unreference_node(literal);
    return ret;
  }

  *const_exprp = (idl_const_expr_t *) literal;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
parse_scoped_const_expr(
  idl_parser_stream_t *stream,
  idl_const_expr_t **const_exprp,
  idl_location_t *locationp)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_scoped_name_t *scoped_name = NULL;
  const idl_declaration_t *declaration = NULL;
  static const char fmt[] =
    "Scoped name '%s' does not resolve to an enumerator or a constant";
  idl_retcode_t ret;

  if ((ret = parse_scoped_name(stream, &scoped_name)) != IDL_RETCODE_OK)
    return ret;
  *locationp = *idl_location(scoped_name);
  if (parsing_unknown_annotation_params(stream)) {
    *const_exprp = NULL;
    ret = IDL_RETCODE_OK;
    goto err;
  }

  ret = idl_resolve(pstate, 0u, scoped_name, &declaration);
  if (ret != IDL_RETCODE_OK)
    goto err;
  if (!declaration ||
      !(idl_mask(declaration->node) &
        (IDL_CONST | IDL_ENUMERATOR | IDL_BIT_VALUE))) {
    idl_error(pstate, idl_location(scoped_name), fmt, scoped_name->identifier);
    ret = IDL_RETCODE_SEMANTIC_ERROR;
    goto err;
  }

  *const_exprp = idl_reference_node((idl_node_t *) declaration->node);
err:
  idl_delete_scoped_name(scoped_name);
  return ret;
}

static idl_retcode_t
parse_unknown_literal_expr(
  idl_parser_stream_t *stream,
  idl_const_expr_t **const_exprp,
  idl_location_t *locationp)
{
  idl_position_t first = stream->token.location.first;
  idl_position_t last;
  bool string_literal = stream->token.code == IDL_TOKEN_STRING_LITERAL;
  idl_retcode_t ret;

  assert(parsing_unknown_annotation_params(stream));
  assert(stream->token.code == IDL_TOKEN_INTEGER_LITERAL ||
         stream->token.code == IDL_TOKEN_FLOATING_PT_LITERAL ||
         stream->token.code == IDL_TOKEN_CHAR_LITERAL ||
         stream->token.code == IDL_TOKEN_STRING_LITERAL ||
         stream->token.code == IDL_TOKEN_TRUE ||
         stream->token.code == IDL_TOKEN_FALSE);

  do {
    last = stream->token.location.last;
    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      return ret;
  } while (string_literal && stream->token.code == IDL_TOKEN_STRING_LITERAL);

  *const_exprp = NULL;
  *locationp = location_span(first, last);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
parse_primary_expr(
  idl_parser_stream_t *stream,
  idl_const_expr_t **const_exprp,
  idl_location_t *locationp)
{
  if (parsing_unknown_annotation_params(stream) &&
      (stream->token.code == IDL_TOKEN_INTEGER_LITERAL ||
       stream->token.code == IDL_TOKEN_FLOATING_PT_LITERAL ||
       stream->token.code == IDL_TOKEN_CHAR_LITERAL ||
       stream->token.code == IDL_TOKEN_STRING_LITERAL ||
       stream->token.code == IDL_TOKEN_TRUE ||
       stream->token.code == IDL_TOKEN_FALSE))
    return parse_unknown_literal_expr(stream, const_exprp, locationp);
  if (stream->token.code == IDL_TOKEN_INTEGER_LITERAL)
    return parse_integer_literal_expr(stream, const_exprp, locationp);
  if (stream->token.code == IDL_TOKEN_FLOATING_PT_LITERAL)
    return parse_floating_literal_expr(stream, const_exprp, locationp);
  if (stream->token.code == IDL_TOKEN_CHAR_LITERAL)
    return parse_char_literal_expr(stream, const_exprp, locationp);
  if (stream->token.code == IDL_TOKEN_STRING_LITERAL)
    return parse_string_literal_expr(stream, const_exprp, locationp);
  if (stream->token.code == IDL_TOKEN_TRUE ||
      stream->token.code == IDL_TOKEN_FALSE)
    return parse_boolean_literal_expr(stream, const_exprp, locationp);
  if (stream->token.code == IDL_TOKEN_IDENTIFIER ||
      stream->token.code == IDL_TOKEN_SCOPE)
    return parse_scoped_const_expr(stream, const_exprp, locationp);
  if (stream->token.code == '(') {
    idl_location_t lparen_location = stream->token.location;
    idl_location_t expr_location;
    idl_location_t rparen_location;
    idl_const_expr_t *const_expr = NULL;
    idl_retcode_t ret;

    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      return ret;
    if ((ret = parse_const_expr(
          stream, &const_expr, &expr_location)) != IDL_RETCODE_OK)
      return ret;
    if ((ret = expect(stream, ')', &rparen_location)) != IDL_RETCODE_OK) {
      idl_unreference_node(const_expr);
      return ret;
    }

    *const_exprp = const_expr;
    *locationp = location_span(lparen_location.first, rparen_location.last);
    return IDL_RETCODE_OK;
  }
  return syntax_error(stream);
}

static idl_retcode_t
parse_unary_expr(
  idl_parser_stream_t *stream,
  idl_const_expr_t **const_exprp,
  idl_location_t *locationp)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_location_t operator_location;
  idl_location_t operand_location;
  idl_const_expr_t *operand = NULL;
  idl_const_expr_t *const_expr = NULL;
  idl_mask_t operator;
  idl_retcode_t ret;

  switch (stream->token.code) {
    case '-':
      operator = IDL_MINUS;
      break;
    case '+':
      operator = IDL_PLUS;
      break;
    case '~':
      operator = IDL_NOT;
      break;
    default:
      return parse_primary_expr(stream, const_exprp, locationp);
  }

  operator_location = stream->token.location;
  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_primary_expr(
        stream, &operand, &operand_location)) != IDL_RETCODE_OK)
    return ret;

  if (parsing_unknown_annotation_params(stream)) {
    *const_exprp = NULL;
    *locationp = location_span(operator_location.first, operand_location.last);
    return IDL_RETCODE_OK;
  }

  ret = idl_create_unary_expr(
    pstate, &operator_location, operator, operand, &const_expr);
  if (ret != IDL_RETCODE_OK) {
    idl_unreference_node(operand);
    return ret;
  }

  *const_exprp = const_expr;
  *locationp = location_span(operator_location.first, operand_location.last);
  return IDL_RETCODE_OK;
}

typedef bool (*binary_operator_fn)(int32_t code, idl_mask_t *operator);
typedef idl_retcode_t (*parse_expr_fn)(
  idl_parser_stream_t *stream,
  idl_const_expr_t **const_exprp,
  idl_location_t *locationp);

static idl_retcode_t
parse_binary_expr(
  idl_parser_stream_t *stream,
  parse_expr_fn parse_operand,
  binary_operator_fn parse_operator,
  idl_const_expr_t **const_exprp,
  idl_location_t *locationp)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_const_expr_t *lhs = NULL;
  idl_location_t lhs_location;
  idl_retcode_t ret;

  if ((ret = parse_operand(stream, &lhs, &lhs_location)) != IDL_RETCODE_OK)
    return ret;

  for (;;) {
    idl_location_t operator_location;
    idl_location_t rhs_location;
    idl_const_expr_t *rhs = NULL;
    idl_const_expr_t *expr = NULL;
    idl_mask_t operator;

    if (!parse_operator(stream->token.code, &operator))
      break;

    operator_location = stream->token.location;
    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      goto err;
    if ((ret = parse_operand(stream, &rhs, &rhs_location)) != IDL_RETCODE_OK)
      goto err;
    if (parsing_unknown_annotation_params(stream)) {
      lhs_location = location_span(lhs_location.first, rhs_location.last);
      continue;
    }
    ret = idl_create_binary_expr(
      pstate, &operator_location, operator, lhs, rhs, &expr);
    if (ret != IDL_RETCODE_OK) {
      idl_unreference_node(rhs);
      goto err;
    }
    lhs = expr;
    lhs_location = location_span(lhs_location.first, rhs_location.last);
  }

  *const_exprp = lhs;
  *locationp = lhs_location;
  return IDL_RETCODE_OK;
err:
  idl_unreference_node(lhs);
  return ret;
}

static bool
parse_multiplicative_operator(int32_t code, idl_mask_t *operator)
{
  switch (code) {
    case '*':
      *operator = IDL_MULTIPLY;
      return true;
    case '/':
      *operator = IDL_DIVIDE;
      return true;
    case '%':
      *operator = IDL_MODULO;
      return true;
    default:
      return false;
  }
}

static idl_retcode_t
parse_multiplicative_expr(
  idl_parser_stream_t *stream,
  idl_const_expr_t **const_exprp,
  idl_location_t *locationp)
{
  return parse_binary_expr(
    stream, parse_unary_expr, parse_multiplicative_operator,
    const_exprp, locationp);
}

static bool
parse_additive_operator(int32_t code, idl_mask_t *operator)
{
  switch (code) {
    case '+':
      *operator = IDL_ADD;
      return true;
    case '-':
      *operator = IDL_SUBTRACT;
      return true;
    default:
      return false;
  }
}

static idl_retcode_t
parse_additive_expr(
  idl_parser_stream_t *stream,
  idl_const_expr_t **const_exprp,
  idl_location_t *locationp)
{
  return parse_binary_expr(
    stream, parse_multiplicative_expr, parse_additive_operator,
    const_exprp, locationp);
}

static bool
parse_shift_operator(int32_t code, idl_mask_t *operator)
{
  switch (code) {
    case IDL_TOKEN_LSHIFT:
      *operator = IDL_LSHIFT;
      return true;
    case IDL_TOKEN_RSHIFT:
      *operator = IDL_RSHIFT;
      return true;
    default:
      return false;
  }
}

static idl_retcode_t
parse_shift_expr(
  idl_parser_stream_t *stream,
  idl_const_expr_t **const_exprp,
  idl_location_t *locationp)
{
  return parse_binary_expr(
    stream, parse_additive_expr, parse_shift_operator, const_exprp, locationp);
}

static bool
parse_and_operator(int32_t code, idl_mask_t *operator)
{
  if (code != '&')
    return false;
  *operator = IDL_AND;
  return true;
}

static idl_retcode_t
parse_and_expr(
  idl_parser_stream_t *stream,
  idl_const_expr_t **const_exprp,
  idl_location_t *locationp)
{
  return parse_binary_expr(
    stream, parse_shift_expr, parse_and_operator, const_exprp, locationp);
}

static bool
parse_xor_operator(int32_t code, idl_mask_t *operator)
{
  if (code != '^')
    return false;
  *operator = IDL_XOR;
  return true;
}

static idl_retcode_t
parse_xor_expr(
  idl_parser_stream_t *stream,
  idl_const_expr_t **const_exprp,
  idl_location_t *locationp)
{
  return parse_binary_expr(
    stream, parse_and_expr, parse_xor_operator, const_exprp, locationp);
}

static bool
parse_or_operator(int32_t code, idl_mask_t *operator)
{
  if (code != '|')
    return false;
  *operator = IDL_OR;
  return true;
}

static idl_retcode_t
parse_or_expr(
  idl_parser_stream_t *stream,
  idl_const_expr_t **const_exprp,
  idl_location_t *locationp)
{
  return parse_binary_expr(
    stream, parse_xor_expr, parse_or_operator, const_exprp, locationp);
}

static idl_retcode_t
parse_const_expr(
  idl_parser_stream_t *stream,
  idl_const_expr_t **const_exprp,
  idl_location_t *locationp)
{
  return parse_or_expr(stream, const_exprp, locationp);
}

static idl_retcode_t
parse_positive_int_const(
  idl_parser_stream_t *stream,
  idl_literal_t **literalp)
{
  idl_const_expr_t *const_expr = NULL;
  idl_location_t location;
  idl_retcode_t ret;

  if ((ret = parse_const_expr(stream, &const_expr, &location)) !=
      IDL_RETCODE_OK)
    return ret;

  ret = idl_evaluate(stream->pstate, const_expr, IDL_ULONG, literalp);
  if (ret != IDL_RETCODE_OK)
    idl_unreference_node(const_expr);
  return ret;
}

static bool
token_starts_const_base_type(int32_t code)
{
  return code != IDL_TOKEN_WCHAR && token_starts_base_type(code);
}

static idl_retcode_t
parse_scoped_const_type(
  idl_parser_stream_t *stream,
  idl_type_spec_t **type_specp)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_scoped_name_t *scoped_name = NULL;
  const idl_declaration_t *declaration = NULL;
  const idl_node_t *node;
  static const char fmt[] =
    "Scoped name '%s' does not resolve to a valid constant type";
  idl_retcode_t ret;

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
  if (!node ||
      !(idl_mask(node) & (IDL_BASE_TYPE | IDL_STRING | IDL_ENUM | IDL_BITMASK))) {
    idl_error(pstate, idl_location(scoped_name), fmt, scoped_name->identifier);
    ret = IDL_RETCODE_SEMANTIC_ERROR;
    goto err;
  }

  *type_specp = idl_reference_node((idl_node_t *) declaration->node);
err:
  idl_delete_scoped_name(scoped_name);
  return ret;
}

static idl_retcode_t
parse_const_type(
  idl_parser_stream_t *stream,
  idl_type_spec_t **type_specp)
{
  if (token_starts_const_base_type(stream->token.code))
    return parse_base_type_spec(stream, type_specp);
  if (stream->token.code == IDL_TOKEN_STRING)
    return parse_string_type(stream, type_specp);
  if (stream->token.code == IDL_TOKEN_IDENTIFIER ||
      stream->token.code == IDL_TOKEN_SCOPE)
    return parse_scoped_const_type(stream, type_specp);
  return syntax_error(stream);
}

static idl_retcode_t
parse_simple_declarator(
  idl_parser_stream_t *stream,
  idl_declarator_t **declaratorp)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_declarator_t *declarator = NULL;
  idl_name_t *name = NULL;
  idl_location_t location;
  idl_retcode_t ret;

  if ((ret = parse_identifier(stream, &name)) != IDL_RETCODE_OK)
    return ret;

  location = name->symbol.location;
  ret = idl_create_declarator(pstate, &location, name, NULL, &declarator);
  if (ret != IDL_RETCODE_OK) {
    idl_delete_name(name);
    return ret;
  }

  *declaratorp = declarator;
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
    if ((ret = parse_positive_int_const(stream, &size)) != IDL_RETCODE_OK)
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
parse_constructed_type_declaration(
  idl_parser_stream_t *stream,
  void **nodep);

static idl_retcode_t
parse_typedef(idl_parser_stream_t *stream, void **nodep)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_position_t first = stream->token.location.first;
  idl_location_t location;
  idl_type_spec_t *type_spec = NULL;
  void *constructed_type = NULL;
  idl_declarator_t *declarators = NULL;
  idl_typedef_t *node = NULL;
  bool constructed = false;
  idl_retcode_t ret;

  assert(stream->token.code == IDL_TOKEN_TYPEDEF);
  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
    return ret;
  switch (stream->token.code) {
    case IDL_TOKEN_STRUCT:
    case IDL_TOKEN_UNION:
    case IDL_TOKEN_ENUM:
    case IDL_TOKEN_BITMASK:
      if ((ret = parse_constructed_type_declaration(
            stream, &constructed_type)) != IDL_RETCODE_OK)
        return ret;
      if ((idl_mask(constructed_type) & IDL_FORWARD) &&
          ((idl_forward_t *)constructed_type)->type_spec)
        type_spec = ((idl_forward_t *)constructed_type)->type_spec;
      else
        type_spec = constructed_type;
      constructed = true;
      break;
    default:
      if ((ret = parse_type_spec(stream, &type_spec)) != IDL_RETCODE_OK)
        return ret;
      break;
  }
  if ((ret = parse_declarators(stream, &declarators)) != IDL_RETCODE_OK)
    goto err;

  location = location_span(first, declarators_last_position(declarators));
  ret = idl_create_typedef(
    pstate, &location, type_spec, declarators, &node);
  if (ret != IDL_RETCODE_OK)
    goto err;

  if (constructed) {
    idl_reference_node(type_spec);
    *nodep = idl_push_node(constructed_type, node);
    constructed_type = NULL;
  } else {
    *nodep = node;
  }
  return IDL_RETCODE_OK;
err:
  idl_delete_node(node);
  idl_delete_node(declarators);
  if (constructed_type)
    idl_delete_node(constructed_type);
  else
    release_node(type_spec);
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
  idl_position_t first;
  idl_location_t semicolon_location;
  idl_location_t location;
  idl_annotation_appl_t *annotations = NULL;
  idl_type_spec_t *type_spec = NULL;
  idl_declarator_t *declarators = NULL;
  idl_member_t *member = NULL;
  idl_retcode_t ret;

  if ((ret = parse_annotation_applications(stream, &annotations)) !=
      IDL_RETCODE_OK)
    return ret;
  first = annotations ?
    idl_location(annotations)->first : stream->token.location.first;

  if ((ret = parse_type_spec(stream, &type_spec)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = parse_declarators(stream, &declarators)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = expect(stream, ';', &semicolon_location)) != IDL_RETCODE_OK)
    goto err;

  location = location_span(first, semicolon_location.last);
  ret = idl_create_member(
    pstate, &location, type_spec, declarators, &member);
  if (ret != IDL_RETCODE_OK)
    goto err;
  type_spec = NULL;
  declarators = NULL;

  if (annotations &&
      (ret = idl_annotate(pstate, member, annotations)) != IDL_RETCODE_OK)
    goto err;
  annotations = NULL;

  *memberp = member;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(member);
  idl_delete_node(annotations);
  idl_delete_node(declarators);
  release_node(type_spec);
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
parse_struct_common(
  idl_parser_stream_t *stream,
  void **nodep,
  bool forward_requires_semicolon)
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

  if (stream->token.code == ';' ||
      (!forward_requires_semicolon &&
       stream->token.code != ':' &&
       stream->token.code != '{')) {
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
  (void) entered_scope;
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
parse_struct(idl_parser_stream_t *stream, void **nodep)
{
  return parse_struct_common(stream, nodep, true);
}

static idl_retcode_t
parse_struct_type_declaration(idl_parser_stream_t *stream, void **nodep)
{
  return parse_struct_common(stream, nodep, false);
}

static idl_retcode_t
parse_switch_header(
  idl_parser_stream_t *stream,
  idl_switch_type_spec_t **switch_type_specp,
  idl_location_t *locationp)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_position_t first = stream->token.location.first;
  idl_location_t type_location;
  idl_location_t rparen_location;
  idl_annotation_appl_t *annotations = NULL;
  idl_type_spec_t *type_spec = NULL;
  idl_switch_type_spec_t *switch_type_spec = NULL;
  idl_retcode_t ret;

  assert(stream->token.code == IDL_TOKEN_SWITCH);
  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = expect(stream, '(', NULL)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_annotation_applications(stream, &annotations)) !=
      IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_type_spec(stream, &type_spec)) != IDL_RETCODE_OK)
    goto err;
  type_location = *idl_location(type_spec);
  if ((ret = expect(stream, ')', &rparen_location)) != IDL_RETCODE_OK)
    goto err;

  ret = idl_create_switch_type_spec(
    pstate, &type_location, type_spec, &switch_type_spec);
  if (ret != IDL_RETCODE_OK)
    goto err;
  type_spec = NULL;

  if (annotations &&
      (ret = idl_annotate(
        pstate, switch_type_spec, annotations)) != IDL_RETCODE_OK)
    goto err;
  annotations = NULL;

  *switch_type_specp = switch_type_spec;
  *locationp = location_span(first, rparen_location.last);
  return IDL_RETCODE_OK;
err:
  idl_delete_node(switch_type_spec);
  idl_delete_node(annotations);
  release_node(type_spec);
  return ret;
}

static idl_retcode_t
parse_case_label(
  idl_parser_stream_t *stream,
  idl_case_label_t **case_labelp)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_position_t first = stream->token.location.first;
  idl_location_t expr_location;
  idl_location_t location;
  idl_const_expr_t *const_expr = NULL;
  idl_case_label_t *case_label = NULL;
  idl_retcode_t ret;

  if (stream->token.code == IDL_TOKEN_DEFAULT) {
    location = stream->token.location;
    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      return ret;
    if ((ret = expect(stream, ':', NULL)) != IDL_RETCODE_OK)
      return ret;
    return idl_create_case_label(pstate, &location, NULL, case_labelp);
  }

  if (stream->token.code != IDL_TOKEN_CASE)
    return syntax_error(stream);
  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_const_expr(
        stream, &const_expr, &expr_location)) != IDL_RETCODE_OK)
    return ret;
  location = location_span(first, expr_location.last);
  if ((ret = expect(stream, ':', NULL)) != IDL_RETCODE_OK)
    goto err;
  ret = idl_create_case_label(pstate, &location, const_expr, &case_label);
  if (ret != IDL_RETCODE_OK)
    goto err;

  *case_labelp = case_label;
  return IDL_RETCODE_OK;
err:
  release_node(const_expr);
  return ret;
}

static idl_retcode_t
parse_case_labels(
  idl_parser_stream_t *stream,
  idl_case_label_t **case_labelsp)
{
  idl_case_label_t *case_labels = NULL;
  idl_retcode_t ret;

  if ((ret = parse_case_label(stream, &case_labels)) != IDL_RETCODE_OK)
    return ret;

  while (stream->token.code == IDL_TOKEN_CASE ||
         stream->token.code == IDL_TOKEN_DEFAULT) {
    idl_case_label_t *case_label = NULL;

    if ((ret = parse_case_label(stream, &case_label)) != IDL_RETCODE_OK)
      goto err;
    case_labels = idl_push_node(case_labels, case_label);
  }

  *case_labelsp = case_labels;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(case_labels);
  return ret;
}

static idl_retcode_t
parse_element_spec(
  idl_parser_stream_t *stream,
  idl_case_t **casep,
  idl_location_t *locationp)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_position_t first;
  idl_location_t location;
  idl_annotation_appl_t *annotations = NULL;
  idl_type_spec_t *type_spec = NULL;
  idl_declarator_t *declarator = NULL;
  idl_case_t *case_node = NULL;
  idl_retcode_t ret;

  if ((ret = parse_annotation_applications(stream, &annotations)) !=
      IDL_RETCODE_OK)
    return ret;
  first = annotations ?
    idl_location(annotations)->first : stream->token.location.first;

  if ((ret = parse_type_spec(stream, &type_spec)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = parse_declarator(stream, &declarator)) != IDL_RETCODE_OK)
    goto err;

  location = location_span(first, idl_location(declarator)->last);
  ret = idl_create_case(
    pstate, &location, type_spec, declarator, &case_node);
  if (ret != IDL_RETCODE_OK)
    goto err;
  type_spec = NULL;
  declarator = NULL;

  if (annotations &&
      (ret = idl_annotate(pstate, case_node, annotations)) != IDL_RETCODE_OK)
    goto err;
  annotations = NULL;

  *casep = case_node;
  *locationp = location;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(case_node);
  idl_delete_node(annotations);
  idl_delete_node(declarator);
  release_node(type_spec);
  return ret;
}

static idl_retcode_t
parse_case(idl_parser_stream_t *stream, idl_case_t **casep)
{
  idl_location_t element_location;
  idl_case_label_t *case_labels = NULL;
  idl_case_t *case_node = NULL;
  idl_retcode_t ret;

  if ((ret = parse_case_labels(stream, &case_labels)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_element_spec(
        stream, &case_node, &element_location)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = expect(stream, ';', NULL)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = idl_finalize_case(
        stream->pstate, &element_location, case_node, case_labels)) !=
      IDL_RETCODE_OK)
    goto err;
  case_labels = NULL;

  *casep = case_node;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(case_labels);
  idl_delete_node(case_node);
  return ret;
}

static idl_retcode_t
parse_cases(idl_parser_stream_t *stream, idl_case_t **casesp)
{
  idl_case_t *cases = NULL;
  idl_retcode_t ret;

  while (stream->token.code != '}') {
    idl_case_t *case_node = NULL;

    if (stream->token.code == '\0') {
      ret = syntax_error(stream);
      goto err;
    }

    if ((ret = parse_case(stream, &case_node)) != IDL_RETCODE_OK)
      goto err;
    cases = idl_push_node(cases, case_node);
  }

  if (!cases) {
    ret = syntax_error(stream);
    goto err;
  }

  *casesp = cases;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(cases);
  return ret;
}

static idl_retcode_t
parse_union_common(
  idl_parser_stream_t *stream,
  void **nodep,
  bool forward_requires_semicolon)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_position_t first = stream->token.location.first;
  idl_location_t keyword_location = stream->token.location;
  idl_location_t header_location;
  idl_location_t location;
  idl_location_t rbrace_location;
  idl_switch_type_spec_t *switch_type_spec = NULL;
  idl_union_t *union_node = NULL;
  idl_case_t *cases = NULL;
  idl_name_t *name = NULL;
  bool entered_scope = false;
  idl_retcode_t ret;

  assert(stream->token.code == IDL_TOKEN_UNION);
  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_identifier(stream, &name)) != IDL_RETCODE_OK)
    return ret;

  if (stream->token.code == ';' ||
      (!forward_requires_semicolon &&
       stream->token.code != IDL_TOKEN_SWITCH)) {
    ret = idl_create_forward(pstate, &keyword_location, name, IDL_UNION, nodep);
    if (ret != IDL_RETCODE_OK)
      idl_delete_name(name);
    return ret;
  }

  if (stream->token.code != IDL_TOKEN_SWITCH) {
    idl_delete_name(name);
    return syntax_error(stream);
  }

  if ((ret = parse_switch_header(
        stream, &switch_type_spec, &header_location)) != IDL_RETCODE_OK) {
    idl_delete_name(name);
    return ret;
  }

  location = location_span(first, header_location.last);
  ret = idl_create_union(
    pstate, &location, name, switch_type_spec, &union_node);
  if (ret != IDL_RETCODE_OK) {
    idl_delete_node(switch_type_spec);
    idl_delete_name(name);
    return ret;
  }
  name = NULL;
  switch_type_spec = NULL;
  entered_scope = true;

  if ((ret = expect(stream, '{', NULL)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = parse_cases(stream, &cases)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = expect(stream, '}', &rbrace_location)) != IDL_RETCODE_OK)
    goto err;

  location = location_span(first, rbrace_location.last);
  if ((ret = idl_finalize_union(
        pstate, &location, union_node, cases)) != IDL_RETCODE_OK)
    goto err;
  entered_scope = false;
  (void) entered_scope;
  cases = NULL;

  *nodep = union_node;
  return IDL_RETCODE_OK;
err:
  if (entered_scope)
    idl_exit_scope(pstate);
  idl_delete_node(cases);
  idl_delete_node(union_node);
  return ret;
}

static idl_retcode_t
parse_union(idl_parser_stream_t *stream, void **nodep)
{
  return parse_union_common(stream, nodep, true);
}

static idl_retcode_t
parse_union_type_declaration(idl_parser_stream_t *stream, void **nodep)
{
  return parse_union_common(stream, nodep, false);
}

static idl_retcode_t
parse_enumerator(
  idl_parser_stream_t *stream,
  idl_enumerator_t **enumeratorp)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_annotation_appl_t *annotations = NULL;
  idl_enumerator_t *enumerator = NULL;
  idl_name_t *name = NULL;
  idl_location_t location;
  idl_retcode_t ret;

  if ((ret = parse_annotation_applications(stream, &annotations)) !=
      IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_identifier(stream, &name)) != IDL_RETCODE_OK)
    goto err;
  location = name->symbol.location;
  ret = idl_create_enumerator(pstate, &location, name, &enumerator);
  if (ret != IDL_RETCODE_OK)
    goto err;
  name = NULL;

  if (annotations &&
      (ret = idl_annotate(pstate, enumerator, annotations)) != IDL_RETCODE_OK)
    goto err;
  annotations = NULL;

  *enumeratorp = enumerator;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(enumerator);
  idl_delete_node(annotations);
  idl_delete_name(name);
  return ret;
}

static idl_retcode_t
parse_enumerators(
  idl_parser_stream_t *stream,
  idl_enumerator_t **enumeratorsp)
{
  idl_enumerator_t *enumerators = NULL;
  idl_retcode_t ret;

  if ((ret = parse_enumerator(stream, &enumerators)) != IDL_RETCODE_OK)
    return ret;

  while (stream->token.code == ',') {
    idl_enumerator_t *enumerator = NULL;

    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      goto err;
    if ((ret = parse_enumerator(stream, &enumerator)) != IDL_RETCODE_OK)
      goto err;
    enumerators = idl_push_node(enumerators, enumerator);
  }

  *enumeratorsp = enumerators;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(enumerators);
  return ret;
}

static idl_retcode_t
parse_enum(idl_parser_stream_t *stream, void **nodep)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_position_t first = stream->token.location.first;
  idl_location_t rbrace_location;
  idl_location_t location;
  idl_enum_t *enum_node = NULL;
  idl_enumerator_t *enumerators = NULL;
  idl_name_t *name = NULL;
  idl_retcode_t ret;

  assert(stream->token.code == IDL_TOKEN_ENUM);
  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_identifier(stream, &name)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = expect(stream, '{', NULL)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = parse_enumerators(stream, &enumerators)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = expect(stream, '}', &rbrace_location)) != IDL_RETCODE_OK)
    goto err;

  location = location_span(first, rbrace_location.last);
  ret = idl_create_enum(pstate, &location, name, enumerators, &enum_node);
  if (ret != IDL_RETCODE_OK)
    goto err;

  *nodep = enum_node;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(enumerators);
  idl_delete_name(name);
  return ret;
}

static idl_retcode_t
parse_bit_value(
  idl_parser_stream_t *stream,
  idl_bit_value_t **bit_valuep)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_annotation_appl_t *annotations = NULL;
  idl_bit_value_t *bit_value = NULL;
  idl_name_t *name = NULL;
  idl_location_t location;
  idl_retcode_t ret;

  if ((ret = parse_annotation_applications(stream, &annotations)) !=
      IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_identifier(stream, &name)) != IDL_RETCODE_OK)
    goto err;
  location = name->symbol.location;
  ret = idl_create_bit_value(pstate, &location, name, &bit_value);
  if (ret != IDL_RETCODE_OK)
    goto err;
  name = NULL;

  if (annotations &&
      (ret = idl_annotate(pstate, bit_value, annotations)) != IDL_RETCODE_OK)
    goto err;
  annotations = NULL;

  *bit_valuep = bit_value;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(bit_value);
  idl_delete_node(annotations);
  idl_delete_name(name);
  return ret;
}

static idl_retcode_t
parse_bit_values(
  idl_parser_stream_t *stream,
  idl_bit_value_t **bit_valuesp)
{
  idl_bit_value_t *bit_values = NULL;
  idl_retcode_t ret;

  if ((ret = parse_bit_value(stream, &bit_values)) != IDL_RETCODE_OK)
    return ret;

  while (stream->token.code == ',') {
    idl_bit_value_t *bit_value = NULL;

    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      goto err;
    if ((ret = parse_bit_value(stream, &bit_value)) != IDL_RETCODE_OK)
      goto err;
    bit_values = idl_push_node(bit_values, bit_value);
  }

  *bit_valuesp = bit_values;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(bit_values);
  return ret;
}

static idl_retcode_t
parse_bitmask(idl_parser_stream_t *stream, void **nodep)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_position_t first = stream->token.location.first;
  idl_location_t rbrace_location;
  idl_location_t location;
  idl_bitmask_t *bitmask = NULL;
  idl_bit_value_t *bit_values = NULL;
  idl_name_t *name = NULL;
  idl_retcode_t ret;

  assert(stream->token.code == IDL_TOKEN_BITMASK);
  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_identifier(stream, &name)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = expect(stream, '{', NULL)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = parse_bit_values(stream, &bit_values)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = expect(stream, '}', &rbrace_location)) != IDL_RETCODE_OK)
    goto err;

  location = location_span(first, rbrace_location.last);
  ret = idl_create_bitmask(pstate, &location, name, bit_values, &bitmask);
  if (ret != IDL_RETCODE_OK)
    goto err;

  *nodep = bitmask;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(bit_values);
  idl_delete_name(name);
  return ret;
}

static idl_retcode_t
parse_constructed_type_declaration(
  idl_parser_stream_t *stream,
  void **nodep)
{
  switch (stream->token.code) {
    case IDL_TOKEN_STRUCT:
      return parse_struct_type_declaration(stream, nodep);
    case IDL_TOKEN_UNION:
      return parse_union_type_declaration(stream, nodep);
    case IDL_TOKEN_ENUM:
      return parse_enum(stream, nodep);
    case IDL_TOKEN_BITMASK:
      return parse_bitmask(stream, nodep);
    default:
      return syntax_error(stream);
  }
}

static idl_retcode_t
parse_const_declaration(idl_parser_stream_t *stream, void **nodep)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_position_t first = stream->token.location.first;
  idl_location_t expr_location;
  idl_location_t location;
  idl_type_spec_t *type_spec = NULL;
  idl_name_t *name = NULL;
  idl_const_expr_t *const_expr = NULL;
  idl_const_t *const_node = NULL;
  idl_retcode_t ret;

  assert(stream->token.code == IDL_TOKEN_CONST);
  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_const_type(stream, &type_spec)) != IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_identifier(stream, &name)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = expect(stream, '=', NULL)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = parse_const_expr(
        stream, &const_expr, &expr_location)) != IDL_RETCODE_OK)
    goto err;

  location = location_span(first, expr_location.last);
  ret = idl_create_const(
    pstate, &location, type_spec, name, const_expr, &const_node);
  if (ret != IDL_RETCODE_OK)
    goto err;

  *nodep = const_node;
  return IDL_RETCODE_OK;
err:
  release_node(const_expr);
  idl_delete_name(name);
  release_node(type_spec);
  return ret;
}

static idl_retcode_t
parse_any_const_type(
  idl_parser_stream_t *stream,
  idl_type_spec_t **type_specp)
{
  idl_location_t location = stream->token.location;
  idl_retcode_t ret;

  assert(stream->token.code == IDL_TOKEN_ANY);
  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
    return ret;
  return idl_create_base_type(stream->pstate, &location, IDL_ANY, type_specp);
}

static idl_retcode_t
parse_annotation_member_type(
  idl_parser_stream_t *stream,
  idl_type_spec_t **type_specp)
{
  if (stream->token.code == IDL_TOKEN_ANY)
    return parse_any_const_type(stream, type_specp);
  return parse_const_type(stream, type_specp);
}

static idl_retcode_t
parse_annotation_member(
  idl_parser_stream_t *stream,
  idl_annotation_member_t **memberp)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_position_t first = stream->token.location.first;
  idl_position_t last;
  idl_location_t location;
  idl_location_t expr_location;
  idl_type_spec_t *type_spec = NULL;
  idl_declarator_t *declarator = NULL;
  idl_const_expr_t *default_value = NULL;
  idl_annotation_member_t *member = NULL;
  idl_retcode_t ret;

  if ((ret = parse_annotation_member_type(stream, &type_spec)) !=
      IDL_RETCODE_OK)
    return ret;
  if ((ret = parse_simple_declarator(stream, &declarator)) != IDL_RETCODE_OK)
    goto err;

  last = idl_location(declarator)->last;
  if (stream->token.code == IDL_TOKEN_DEFAULT) {
    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      goto err;
    if ((ret = parse_const_expr(
          stream, &default_value, &expr_location)) != IDL_RETCODE_OK)
      goto err;
    last = expr_location.last;
  }

  location = location_span(first, last);
  ret = idl_create_annotation_member(
    pstate, &location, type_spec, declarator, default_value, &member);
  if (ret != IDL_RETCODE_OK)
    goto err;

  *memberp = member;
  return IDL_RETCODE_OK;
err:
  release_node(default_value);
  idl_delete_node(declarator);
  release_node(type_spec);
  return ret;
}

static idl_retcode_t
parse_annotation_body(
  idl_parser_stream_t *stream,
  idl_definition_t **definitionsp)
{
  idl_definition_t *definitions = NULL;
  idl_retcode_t ret;

  while (stream->token.code != '}') {
    void *definition = NULL;

    if (stream->token.code == '\0') {
      ret = syntax_error(stream);
      goto err;
    }

    switch (stream->token.code) {
      case IDL_TOKEN_ENUM:
        ret = parse_enum(stream, &definition);
        break;
      case IDL_TOKEN_BITMASK:
        ret = parse_bitmask(stream, &definition);
        break;
      case IDL_TOKEN_CONST:
        ret = parse_const_declaration(stream, &definition);
        break;
      case IDL_TOKEN_TYPEDEF:
        ret = parse_typedef(stream, &definition);
        break;
      default:
      {
        idl_annotation_member_t *member = NULL;
        ret = parse_annotation_member(stream, &member);
        definition = member;
        break;
      }
    }
    if (ret != IDL_RETCODE_OK)
      goto err;
    if ((ret = expect(stream, ';', NULL)) != IDL_RETCODE_OK) {
      idl_delete_node(definition);
      goto err;
    }
    definitions = idl_push_node(definitions, definition);
  }

  *definitionsp = definitions;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(definitions);
  return ret;
}

static idl_retcode_t
parse_annotation_declaration_after_at(
  idl_parser_stream_t *stream,
  const idl_location_t *at_location,
  void **nodep)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_location_t annotation_location;
  idl_location_t location;
  idl_location_t rbrace_location;
  idl_annotation_t *annotation = NULL;
  idl_definition_t *definitions = NULL;
  idl_name_t *name = NULL;
  bool entered_scope = false;
  bool discard;
  idl_retcode_t ret;

  assert(stream->token.code == IDL_TOKEN_ANNOTATION);
  pstate->annotations = true;
  pstate->parser.state = IDL_PARSE_ANNOTATION;
  annotation_location = stream->token.location;
  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = parse_identifier_ex(stream, true, &name)) != IDL_RETCODE_OK)
    goto err;

  location = location_span(
    at_location->first, annotation_location.last);
  ret = idl_create_annotation(pstate, &location, name, &annotation);
  if (ret != IDL_RETCODE_OK)
    goto err;
  name = NULL;
  entered_scope = true;

  if ((ret = expect(stream, '{', NULL)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = parse_annotation_body(stream, &definitions)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = expect(stream, '}', &rbrace_location)) != IDL_RETCODE_OK)
    goto err;

  discard = pstate->parser.state == IDL_PARSE_EXISTING_ANNOTATION_BODY;
  location = location_span(at_location->first, rbrace_location.last);
  entered_scope = false;
  ret = idl_finalize_annotation(pstate, &location, annotation, definitions);
  if (ret != IDL_RETCODE_OK)
    goto err;
  definitions = NULL;

  *nodep = discard ? NULL : annotation;
  return IDL_RETCODE_OK;
err:
  if (entered_scope)
    idl_exit_scope(pstate);
  pstate->parser.state = IDL_PARSE;
  idl_delete_node(definitions);
  idl_delete_node(annotation);
  idl_delete_name(name);
  return ret;
}

static bool
is_builtin_location(const idl_location_t *location)
{
  return location->first.file &&
         location->first.file->name &&
         strcmp(location->first.file->name, "<builtin>") == 0;
}

static idl_retcode_t
parse_annotation_appl_name(
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

  if (stream->token.code == IDL_TOKEN_SCOPE_NO_SPACE) {
    absolute = true;
    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      return ret;
  }

  if ((ret = parse_identifier_ex(stream, true, &name)) != IDL_RETCODE_OK)
    return ret;
  location = location_span(first, name->symbol.location.last);
  ret = idl_create_scoped_name(
    pstate, &location, name, absolute, &scoped_name);
  if (ret != IDL_RETCODE_OK) {
    idl_delete_name(name);
    return ret;
  }
  name = NULL;

  while (stream->token.code == IDL_TOKEN_SCOPE_NO_SPACE) {
    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      goto err;
    if ((ret = parse_identifier_ex(stream, true, &name)) != IDL_RETCODE_OK)
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
parse_annotation_appl_positional_param(
  idl_parser_stream_t *stream,
  idl_annotation_appl_param_t **paramsp)
{
  idl_location_t expr_location;
  idl_const_expr_t *const_expr = NULL;
  idl_retcode_t ret;

  if ((ret = parse_const_expr(
        stream, &const_expr, &expr_location)) != IDL_RETCODE_OK)
    return ret;

  *paramsp = (idl_annotation_appl_param_t *) const_expr;
  return IDL_RETCODE_OK;
}

static void
delete_annotation_appl_params(idl_annotation_appl_param_t *params)
{
  if (!params)
    return;
  if (idl_mask(params) & IDL_ANNOTATION_APPL_PARAM)
    idl_delete_node(params);
  else
    idl_unreference_node(params);
}

static idl_retcode_t
parse_annotation_appl_keyword_param(
  idl_parser_stream_t *stream,
  idl_annotation_appl_param_t **paramp)
{
  idl_pstate_t *pstate = stream->pstate;
  const idl_declaration_t *declaration = NULL;
  idl_annotation_member_t *member = NULL;
  idl_annotation_appl_param_t *param = NULL;
  idl_const_expr_t *const_expr = NULL;
  idl_name_t *name = NULL;
  idl_location_t name_location;
  idl_location_t expr_location;
  idl_location_t location;
  idl_retcode_t ret;

  if (parsing_unknown_annotation_params(stream)) {
    if (stream->token.code != IDL_TOKEN_IDENTIFIER)
      return syntax_error(stream);
    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      return ret;
    if ((ret = expect(stream, '=', NULL)) != IDL_RETCODE_OK)
      return ret;
    if ((ret = parse_const_expr(
          stream, &const_expr, &expr_location)) != IDL_RETCODE_OK)
      return ret;
    release_node(const_expr);
    *paramp = NULL;
    return IDL_RETCODE_OK;
  }

  if ((ret = parse_identifier(stream, &name)) != IDL_RETCODE_OK)
    return ret;
  name_location = name->symbol.location;

  declaration = idl_find(pstate, pstate->annotation_scope, name, 0u);
  if (declaration && (idl_mask(declaration->node) & IDL_DECLARATOR))
    member = (idl_annotation_member_t *)
      ((const idl_node_t *) declaration->node)->parent;
  if (!member || !(idl_mask(member) & IDL_ANNOTATION_MEMBER)) {
    idl_error(pstate, &name_location,
      "Unknown annotation member '%s'", name->identifier);
    ret = IDL_RETCODE_SEMANTIC_ERROR;
    goto err;
  }
  member = idl_reference_node((idl_node_t *) member);
  idl_delete_name(name);
  name = NULL;

  if ((ret = expect(stream, '=', NULL)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = parse_const_expr(
        stream, &const_expr, &expr_location)) != IDL_RETCODE_OK)
    goto err;

  location = location_span(name_location.first, expr_location.last);
  ret = idl_create_annotation_appl_param(
    pstate, &location, member, const_expr, &param);
  if (ret != IDL_RETCODE_OK)
    goto err;
  member = NULL;
  const_expr = NULL;

  *paramp = param;
  return IDL_RETCODE_OK;
err:
  idl_delete_name(name);
  idl_unreference_node(member);
  release_node(const_expr);
  return ret;
}

static idl_retcode_t
parse_annotation_appl_keyword_params(
  idl_parser_stream_t *stream,
  idl_annotation_appl_param_t **paramsp)
{
  idl_annotation_appl_param_t *params = NULL;
  idl_retcode_t ret;

  for (;;) {
    idl_annotation_appl_param_t *param = NULL;

    if ((ret = parse_annotation_appl_keyword_param(
          stream, &param)) != IDL_RETCODE_OK)
      goto err;
    params = idl_push_node(params, param);

    if (stream->token.code != ',')
      break;
    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      goto err;
  }

  *paramsp = params;
  return IDL_RETCODE_OK;
err:
  delete_annotation_appl_params(params);
  return ret;
}

static idl_retcode_t
parse_annotation_appl_params(
  idl_parser_stream_t *stream,
  idl_annotation_appl_param_t **paramsp,
  idl_location_t *locationp)
{
  const idl_token_t *peek = NULL;
  idl_location_t lparen_location;
  idl_location_t rparen_location;
  idl_annotation_appl_param_t *params = NULL;
  idl_retcode_t ret;

  assert(stream->token.code == '(');
  lparen_location = stream->token.location;
  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
    return ret;

  if (stream->token.code == IDL_TOKEN_IDENTIFIER &&
      (ret = stream_peek(stream, &peek)) != IDL_RETCODE_OK)
    return ret;

  if (peek && peek->code == '=') {
    ret = parse_annotation_appl_keyword_params(stream, &params);
  } else {
    ret = parse_annotation_appl_positional_param(stream, &params);
  }
  if (ret != IDL_RETCODE_OK)
    goto err;

  if ((ret = expect(stream, ')', &rparen_location)) != IDL_RETCODE_OK)
    goto err;

  *paramsp = params;
  *locationp = location_span(lparen_location.first, rparen_location.last);
  return IDL_RETCODE_OK;
err:
  delete_annotation_appl_params(params);
  return ret;
}

static idl_retcode_t
parse_annotation_application_after_at(
  idl_parser_stream_t *stream,
  const idl_location_t *at_location,
  idl_annotation_appl_t **annotationp)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_scoped_name_t *scoped_name = NULL;
  idl_annotation_appl_t *annotation_appl = NULL;
  idl_annotation_appl_param_t *params = NULL;
  const idl_declaration_t *declaration;
  idl_location_t location;
  idl_retcode_t ret;

  pstate->parser.state = IDL_PARSE_ANNOTATION_APPL;
  if ((ret = parse_annotation_appl_name(stream, &scoped_name)) !=
      IDL_RETCODE_OK)
    goto err;

  location = location_span(
    at_location->first, idl_location(scoped_name)->last);
  declaration = idl_find_scoped_name(
    pstate, NULL, scoped_name, IDL_FIND_ANNOTATION);
  pstate->annotations = true;
  if (declaration) {
    const idl_annotation_t *annotation =
      idl_reference_node((idl_node_t *) declaration->node);
    ret = idl_create_annotation_appl(
      pstate, &location, annotation, &annotation_appl);
    if (ret != IDL_RETCODE_OK) {
      idl_unreference_node((void *) annotation);
      goto err;
    }
    pstate->parser.state = IDL_PARSE_ANNOTATION_APPL_PARAMS;
    pstate->annotation_scope = declaration->scope;
  } else {
    pstate->parser.state = IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS;
    if (!is_builtin_location(at_location) &&
        !is_builtin_location(idl_location(scoped_name))) {
      idl_warning(pstate, IDL_WARN_UNSUPPORTED_ANNOTATIONS, &location,
        "Unrecognized annotation: @%s", scoped_name->identifier);
    }
  }

  if (stream->token.code == '(') {
    idl_location_t params_location;

    if ((ret = parse_annotation_appl_params(
          stream, &params, &params_location)) != IDL_RETCODE_OK)
      goto err;
    location = location_span(location.first, params_location.last);
  }

  if (annotation_appl) {
    ret = idl_finalize_annotation_appl(
      pstate,
      &location,
      annotation_appl,
      params);
    if (ret != IDL_RETCODE_OK)
      goto err;
  } else {
    delete_annotation_appl_params(params);
  }
  params = NULL;

  pstate->parser.state = IDL_PARSE;
  pstate->annotation_scope = NULL;
  idl_delete_scoped_name(scoped_name);
  *annotationp = annotation_appl;
  return IDL_RETCODE_OK;
err:
  pstate->parser.state = IDL_PARSE;
  pstate->annotation_scope = NULL;
  delete_annotation_appl_params(params);
  idl_delete_node(annotation_appl);
  idl_delete_scoped_name(scoped_name);
  return ret;
}

static idl_retcode_t
parse_annotation_applications_after_at(
  idl_parser_stream_t *stream,
  const idl_location_t *at_location,
  idl_annotation_appl_t **annotationsp)
{
  idl_annotation_appl_t *annotations = NULL;
  idl_location_t current_at = *at_location;
  idl_retcode_t ret;

  for (;;) {
    idl_annotation_appl_t *annotation = NULL;

    if ((ret = parse_annotation_application_after_at(
          stream, &current_at, &annotation)) != IDL_RETCODE_OK)
      goto err;
    annotations = idl_push_node(annotations, annotation);

    if (stream->token.code != IDL_TOKEN_ANNOTATION_SYMBOL)
      break;
    current_at = stream->token.location;
    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      goto err;
  }

  *annotationsp = annotations;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(annotations);
  return ret;
}

static idl_retcode_t
parse_annotation_applications(
  idl_parser_stream_t *stream,
  idl_annotation_appl_t **annotationsp)
{
  idl_location_t at_location;
  idl_retcode_t ret;

  *annotationsp = NULL;
  if (stream->token.code != IDL_TOKEN_ANNOTATION_SYMBOL)
    return IDL_RETCODE_OK;

  at_location = stream->token.location;
  if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
    return ret;
  return parse_annotation_applications_after_at(
    stream, &at_location, annotationsp);
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
  (void) entered_scope;
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
parse_unannotated_definition(idl_parser_stream_t *stream, void **nodep)
{
  switch (stream->token.code) {
    case IDL_TOKEN_MODULE:
      return parse_module(stream, nodep);
    case IDL_TOKEN_STRUCT:
      return parse_struct(stream, nodep);
    case IDL_TOKEN_UNION:
      return parse_union(stream, nodep);
    case IDL_TOKEN_TYPEDEF:
      return parse_typedef(stream, nodep);
    case IDL_TOKEN_ENUM:
      return parse_enum(stream, nodep);
    case IDL_TOKEN_BITMASK:
      return parse_bitmask(stream, nodep);
    case IDL_TOKEN_CONST:
      return parse_const_declaration(stream, nodep);
    default:
      return syntax_error(stream);
  }
}

static idl_retcode_t
parse_definition(idl_parser_stream_t *stream, void **nodep)
{
  idl_pstate_t *pstate = stream->pstate;
  idl_annotation_appl_t *annotations = NULL;
  void *node = NULL;
  idl_retcode_t ret;

  if (stream->token.code == IDL_TOKEN_ANNOTATION_SYMBOL) {
    idl_location_t at_location = stream->token.location;

    if ((ret = stream_advance(stream)) != IDL_RETCODE_OK)
      return ret;
    if (stream->token.code == IDL_TOKEN_ANNOTATION) {
      ret = parse_annotation_declaration_after_at(
        stream, &at_location, &node);
      if (ret != IDL_RETCODE_OK)
        return ret;
      goto expect_semicolon;
    }

    if ((ret = parse_annotation_applications_after_at(
          stream, &at_location, &annotations)) != IDL_RETCODE_OK)
      return ret;
  }

  ret = parse_unannotated_definition(stream, &node);
  if (ret != IDL_RETCODE_OK)
    goto err;

  if (annotations &&
      (ret = idl_annotate(pstate, node, annotations)) != IDL_RETCODE_OK)
    goto err;
  annotations = NULL;

expect_semicolon:
  if (ret != IDL_RETCODE_OK)
    return ret;
  if ((ret = expect(stream, ';', NULL)) != IDL_RETCODE_OK) {
    idl_delete_node(node);
    return ret;
  }

  *nodep = node;
  return IDL_RETCODE_OK;
err:
  idl_delete_node(annotations);
  idl_delete_node(node);
  return ret;
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
