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
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "idl/processor.h"
#include "idl/string.h"
#include "annotation.h"
#include "directive.h"
#include "scanner.h"
#include "tree.h"
#include "scope.h"

#include "parser.h"

static const idl_file_t builtin_file =
  { NULL, "<builtin>" };
static const idl_source_t builtin_source =
  { NULL, NULL, NULL, NULL, true, &builtin_file, &builtin_file };
#define BUILTIN_POSITION { &builtin_source, &builtin_file, 1, 1 }
#define BUILTIN_LOCATION { BUILTIN_POSITION, BUILTIN_POSITION }
static const idl_name_t builtin_name =
  { { BUILTIN_LOCATION }, "" };

static idl_retcode_t parse_grammar(idl_pstate_t *pstate, idl_token_t *tok);

static idl_retcode_t
parse_builtin_annotations(
  idl_pstate_t *pstate,
  const idl_builtin_annotation_t *annotations)
{
  idl_token_t token;
  idl_retcode_t ret = IDL_RETCODE_OK;

  for (size_t i=0; annotations[i].syntax; i++) {
    unsigned seen = 0, save = 0;
    idl_scope_t *scope = NULL;
    idl_name_t name;
    pstate->scanner.state = IDL_SCAN;
    pstate->buffer.data = (char *)annotations[i].syntax;
    pstate->buffer.size = pstate->buffer.used = strlen(pstate->buffer.data);
    pstate->scanner.cursor = pstate->buffer.data;
    pstate->scanner.limit = pstate->buffer.data + pstate->buffer.used;
    pstate->scanner.position = (idl_position_t)BUILTIN_POSITION;

    memset(&name, 0, sizeof(name));
    memset(&token, 0, sizeof(token));
    do {
      save = 0;
      if ((ret = idl_scan(pstate, &token)) < 0)
        break;
      ret = IDL_RETCODE_OK;
      /* ignore comments and processor directives */
      if (token.code != '\0' &&
          token.code != '\n' &&
          token.code != IDL_TOKEN_COMMENT &&
          token.code != IDL_TOKEN_LINE_COMMENT &&
          !((unsigned)pstate->scanner.state & (unsigned)IDL_SCAN_DIRECTIVE))
      {
        if (pstate->parser.state == IDL_PARSE_ANNOTATION) {
          assert(token.code == IDL_TOKEN_IDENTIFIER);
          seen++;
          save = (seen == 1);
          scope = pstate->scope;
        }
        ret = parse_grammar(pstate, &token);
      }
      switch (token.code) {
        case '\n':
          pstate->scanner.state = IDL_SCAN;
          break;
        case IDL_TOKEN_IDENTIFIER:
          if (save) {
            name.symbol.location = token.location;
            name.identifier = token.value.str;
          }
          /* fall through */
        case IDL_TOKEN_STRING_LITERAL:
        case IDL_TOKEN_PP_NUMBER:
        case IDL_TOKEN_COMMENT:
        case IDL_TOKEN_LINE_COMMENT:
          if (token.value.str && !save) {
            free(token.value.str);
          }
          break;
        default:
          break;
      }
    } while (token.code != '\0' &&
             (ret == IDL_RETCODE_OK || ret == IDL_RETCODE_PUSH_MORE));

    if (seen == 1) {
      idl_annotation_t *annotation;
      const idl_declaration_t *declaration;
      declaration = idl_find(pstate, scope, &name, IDL_FIND_ANNOTATION);
      if (declaration) {
        annotation = (idl_annotation_t *)declaration->node;
        /* multiple definitions of the same annotation may exist, provided
           they are consistent */
        if (!memcmp(&annotation->name->symbol.location, &name.symbol.location, sizeof(name.symbol.location)))
          annotation->callback = annotations[i].callback;
      }
    }

    if (name.identifier) {
      free(name.identifier);
    }

    /* builtin annotations must not declare more than one annotation per block
       to avoid ambiguity in annotation-callback mapping */
    if (seen > 1) {
      idl_error(pstate, &token.location,
        "Multiple declarations of builtin annotations in same block");
      return IDL_RETCODE_SYNTAX_ERROR;
    }
  }

  return ret;
}

extern int idl_yydebug;

idl_retcode_t
idl_create_pstate(
  uint32_t flags,
  const idl_builtin_annotation_t *annotations,
  idl_pstate_t **pstatep)
{
  idl_scope_t *scope = NULL;
  idl_pstate_t *pstate;

  (void)flags;
  if (!(pstate = calloc(1, sizeof(*pstate))))
    goto err_pstate;
  if (!(pstate->parser.yypstate = idl_yypstate_new()))
    goto err_yypstate;
  if (idl_create_scope(pstate, IDL_GLOBAL_SCOPE, &builtin_name, NULL, &scope))
    goto err_scope;

  pstate->flags = flags;
  pstate->global_scope = pstate->scope = scope;

  if (pstate->flags & IDL_FLAG_ANNOTATIONS) {
    idl_retcode_t ret;
    if ((ret = parse_builtin_annotations(pstate, builtin_annotations))) {
      idl_delete_pstate(pstate);
      return ret;
    }
    if (annotations && (ret = parse_builtin_annotations(pstate, annotations))) {
      idl_delete_pstate(pstate);
      return ret;
    }
  }

  pstate->keylists = false;
  pstate->annotations = false;
  pstate->parser.state = IDL_PARSE;
  pstate->scanner.state = IDL_SCAN;
  memset(&pstate->buffer, 0, sizeof(pstate->buffer));
  memset(&pstate->scanner, 0, sizeof(pstate->scanner));
  pstate->builtin_root = pstate->root;
  *pstatep = pstate;
  return IDL_RETCODE_OK;
err_scope:
  idl_yypstate_delete(pstate->parser.yypstate);
err_yypstate:
  free(pstate);
err_pstate:
  return IDL_RETCODE_NO_MEMORY;
}

static void delete_source(idl_source_t *src)
{
  if (!src)
    return;
  for (idl_source_t *n, *s=src; s; s = n) {
    n = s->next;
    delete_source(s->includes);
    free(s);
  }
}

void idl_delete_pstate(idl_pstate_t *pstate)
{
  if (pstate) {
    /* parser */
    if (pstate->parser.yypstate) {
      idl_yypstate_delete_stack(pstate->parser.yypstate);
      idl_yypstate_delete(pstate->parser.yypstate);
    }
    idl_delete_node(pstate->builtin_root);
    /* directive */
    if (pstate->directive)
      idl_delete_directive(pstate);
    idl_delete_scope(pstate->global_scope);
    /* sources */
    delete_source(pstate->sources);
    /* files */
    for (idl_file_t *n, *f=pstate->files; f; f = n) {
      n = f->next;
      if (f->name)
        free(f->name);
      free(f);
    }
    /* paths */
    for (idl_file_t *n, *f=pstate->paths; f; f = n) {
      n = f->next;
      if (f->name)
        free(f->name);
      free(f);
    }
    /* buffer */
    if (pstate->buffer.data)
      free(pstate->buffer.data);
    free(pstate);
  }
}

static void
idl_log(
  idl_pstate_t *pstate, uint32_t prio, const idl_location_t *loc, const char *fmt, va_list ap)
{
  char buf[1024];
  int cnt = 0;
  size_t off;

  buf[0] = '\0';
  (void)pstate;
  (void)prio;
  if (loc && loc->first.file)
    cnt = snprintf(
      buf, sizeof(buf)-1, "%s:%u:%u: ", loc->first.file->name, loc->first.line, loc->first.column);
  else if (loc)
    cnt = snprintf(
      buf, sizeof(buf)-1, "%u:%u: ", loc->first.line, loc->first.column);

  if (cnt == -1)
    return;

  off = (size_t)cnt;
  cnt = vsnprintf(buf+off, sizeof(buf)-off, fmt, ap);

  if (cnt == -1)
    return;

  fprintf(stderr, "%s\n", buf);
}

#define IDL_LC_ERROR 1
#define IDL_LC_WARNING 2

void
idl_verror(
  idl_pstate_t *pstate, const idl_location_t *loc, const char *fmt, va_list ap)
{
  idl_log(pstate, IDL_LC_ERROR, loc, fmt, ap);
}

void
idl_error(
  idl_pstate_t *pstate, const idl_location_t *loc, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  idl_log(pstate, IDL_LC_ERROR, loc, fmt, ap);
  va_end(ap);
}

void
idl_warning(
  idl_pstate_t *pstate, const idl_location_t *loc, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  idl_log(pstate, IDL_LC_WARNING, loc, fmt, ap);
  va_end(ap);
}

static idl_retcode_t parse_grammar(idl_pstate_t *pstate, idl_token_t *tok)
{
  idl_retcode_t ret;
  IDL_YYSTYPE yylval;

  switch (tok->code) {
    case IDL_TOKEN_CHAR_LITERAL:
      yylval.chr = tok->value.chr;
      break;
    case IDL_TOKEN_IDENTIFIER:
    case IDL_TOKEN_STRING_LITERAL:
      yylval.str = tok->value.str;
      break;
    case IDL_TOKEN_INTEGER_LITERAL:
      yylval.ullng = tok->value.ullng;
      break;
    case IDL_TOKEN_FLOATING_PT_LITERAL:
      yylval.ldbl = tok->value.ldbl;
      break;
    default:
      memset(&yylval, 0, sizeof(yylval));
      break;
  }

  switch ((ret = idl_yypush_parse(
    pstate->parser.yypstate, tok->code, &yylval, &tok->location, pstate)))
  {
    case 0: /* success */
      break;
    case 1: /* parse error */
      return IDL_RETCODE_SYNTAX_ERROR;
    case 2: /* out of memory */
      return IDL_RETCODE_NO_MEMORY;
    case 4: /* push more */
      return IDL_RETCODE_PUSH_MORE;
    default:
      assert(ret < 0);
      return ret;
  }

  return IDL_RETCODE_OK;
}

idl_retcode_t idl_parse(idl_pstate_t *pstate)
{
  idl_retcode_t ret;
  idl_token_t tok;
  memset(&tok, 0, sizeof(tok));

  do {
    if ((ret = idl_scan(pstate, &tok)) < 0)
      break;
    ret = IDL_RETCODE_OK;
    if (tok.code != IDL_TOKEN_COMMENT && tok.code != IDL_TOKEN_LINE_COMMENT) {
      if ((unsigned)pstate->scanner.state & (unsigned)IDL_SCAN_DIRECTIVE) {
        ret = idl_parse_directive(pstate, &tok);
        if ((tok.code == '\0') &&
            (ret == IDL_RETCODE_OK || ret == IDL_RETCODE_PUSH_MORE))
          goto grammar;
      } else if (tok.code != '\n') {
grammar:
        ret = parse_grammar(pstate, &tok);
      }
    }
    /* free memory associated with token value */
    switch (tok.code) {
      case '\n':
        pstate->scanner.state = IDL_SCAN;
        break;
      case IDL_TOKEN_IDENTIFIER:
      case IDL_TOKEN_STRING_LITERAL:
      case IDL_TOKEN_PP_NUMBER:
      case IDL_TOKEN_COMMENT:
      case IDL_TOKEN_LINE_COMMENT:
        if (tok.value.str)
          free(tok.value.str);
        break;
      default:
        break;
    }
  } while ((tok.code != '\0') &&
           (ret == IDL_RETCODE_OK || ret == IDL_RETCODE_PUSH_MORE));

  pstate->builtin_root = pstate->root;
  for (idl_node_t *node = pstate->root; node; node = node->next) {
    if (node->symbol.location.first.file != &builtin_file) {
      pstate->root = node;
      break;
    }
  }

  return ret;
}

idl_retcode_t idl_parse_string(idl_pstate_t *pstate, const char *str)
{
  idl_retcode_t ret;

  assert(str);

  pstate->buffer.data = (char *)str;
  pstate->buffer.size = pstate->buffer.used = strlen(str);
  pstate->scanner.cursor = pstate->buffer.data;
  pstate->scanner.limit = pstate->buffer.data + pstate->buffer.used;
  pstate->scanner.position.file = NULL;
  pstate->scanner.position.source = NULL;
  pstate->scanner.position.line = 1;
  pstate->scanner.position.column = 1;

  if ((ret = idl_parse(pstate)) == IDL_RETCODE_OK) {
    assert(pstate->root);
  }

  pstate->buffer.data = NULL;
  pstate->buffer.size = pstate->buffer.used = 0;
  pstate->scanner.cursor = pstate->scanner.limit = NULL;
  pstate->scanner.position.line = 0;
  pstate->scanner.position.column = 0;

  return ret;
}
