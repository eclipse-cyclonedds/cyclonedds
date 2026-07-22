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
#include <string.h>

#include "idl/heap.h"
#include "idl/processor.h"

#include "directive.h"
#include "parser_impl.h"
#include "scanner.h"
#include "parser.h"

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
  idl_retcode_t ret;

  if (stream->have_token)
    token_fini(&stream->token);

  ret = idl_scan(stream->pstate, &stream->token);
  stream->have_token = true;
  return ret < 0 ? ret : IDL_RETCODE_OK;
}

static idl_retcode_t
parse_empty_specification(idl_parser_stream_t *stream)
{
  idl_pstate_t *pstate = stream->pstate;

  if (stream->token.code == '\0') {
    pstate->root = NULL;
    return IDL_RETCODE_OK;
  }

  idl_error(pstate, &stream->token.location, "syntax error");
  return IDL_RETCODE_SYNTAX_ERROR;
}

idl_retcode_t
idl_parse_hand_written(idl_pstate_t *pstate)
{
  idl_parser_stream_t stream;
  idl_retcode_t ret = IDL_RETCODE_OK;

  assert(pstate);
  stream_init(&stream, pstate);

  do {
    if ((ret = stream_advance(&stream)) < 0)
      break;

    if (stream.token.code != IDL_TOKEN_COMMENT &&
        stream.token.code != IDL_TOKEN_LINE_COMMENT) {
      if ((unsigned)pstate->scanner.state & (unsigned)IDL_SCAN_DIRECTIVE) {
        ret = idl_parse_directive(pstate, &stream.token);
        if (stream.token.code == '\0' &&
            (ret == IDL_RETCODE_OK || ret == IDL_RETCODE_PUSH_MORE))
          ret = parse_empty_specification(&stream);
      } else if (stream.token.code != '\n') {
        ret = parse_empty_specification(&stream);
      }
    }

    if (stream.token.code == '\n')
      pstate->scanner.state = IDL_SCAN;
  } while (stream.token.code != '\0' &&
           (ret == IDL_RETCODE_OK || ret == IDL_RETCODE_PUSH_MORE));

  stream_fini(&stream);
  return ret;
}
