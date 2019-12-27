/*
 * Copyright(c) 2020 Jeroen Koekkoek
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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "idl.h"
#include "idl.y.h"
/* disable inclusion of unistd.h in the flex generated header file, as
   %nounistd only disables inclusion in the generated source file. */
#define YY_NO_UNISTD_H
#include "idl.l.h"
#include "tt_create.h"

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsts/typetree.h"

idl_parser_t *idl_create_parser(void)
{
  idl_parser_t *parser;

  if ((parser = ddsrt_calloc(1, sizeof(*parser))) != NULL &&
      (parser->context = ddsts_create_context()) != NULL &&
      (idl_yylex_init(&parser->yylstate)) == 0 &&
      (parser->yypstate = (void *)idl_yypstate_new()) != NULL)
  {
    return parser;
  }

  idl_destroy_parser(parser);
  return NULL;
}

void idl_destroy_parser(idl_parser_t *parser)
{
  if (parser != NULL) {
    idl_file_t *file, *next;

    if (parser->yypstate != NULL) {
      idl_yypstate_delete((idl_yypstate *)parser->yypstate);
    }
    if (parser->yylstate != NULL) {
      idl_yylex_destroy((yyscan_t)parser->yylstate);
    }
    if (parser->context != NULL) {
      ddsts_free_context(parser->context);
    }

    for (file = parser->files; file != NULL; file = next) {
      next = file->next;
      if (file->name != NULL) {
        ddsrt_free(file->name);
      }
      ddsrt_free(file);
    }

    if (parser->buffer.data != NULL) {
      ddsrt_free(parser->buffer.data);
    }

    ddsrt_free(parser);
  }
}

dds_return_t idl_scan_token(idl_parser_t *parser)
{
  dds_return_t ret = 1;
  YYSTYPE yylval;
  int tok;

  assert(parser != NULL);
  memset(&yylval, 0, sizeof(yylval));

  if ((tok = idl_yylex(&yylval, &parser->location, parser, parser->yylstate)) == '\n') {
    /* ignore whitespace */
    parser->buffer.lines--;
  } else {
    if (tok == 0) {
      /* 0 (YY_NULL) is returned by flex to indicate it is finished. e.g. on
         end-of-file and yyterminate */
      ret = ddsts_context_get_retcode(parser->context);
      if (ret != DDS_RETCODE_OK) {
        return ret;
      }
    }
    int yystate = idl_yypush_parse(parser->yypstate, tok, &yylval, &parser->location, parser);
    if (tok == IDL_T_IDENTIFIER) {
      ddsrt_free(yylval.identifier);
    }
    switch (yystate) {
    case 0:
      ret = ddsts_context_get_retcode(parser->context);
      break;
    case 1:
      ret = ddsts_context_get_retcode(parser->context);
      if (ret == DDS_RETCODE_OK)
        ret = DDS_RETCODE_BAD_SYNTAX;
      break;
    case 2:
      return DDS_RETCODE_OUT_OF_RESOURCES;
    default:
      assert(yystate == YYPUSH_MORE);
      break;
    }
  }
  return ret;
}

dds_return_t idl_scan(idl_parser_t *parser)
{
  dds_return_t rc;

  assert(parser != NULL);
  while ((rc = idl_scan_token(parser)) == 1) { /* scan tokens */ }
  if (rc == DDS_RETCODE_OK) {
    assert(parser->buffer.lines == 0);
  }

  return rc;
}

dds_return_t idl_parse(const char *str, ddsts_type_t **typeptr)
{
  dds_return_t rc;
  idl_parser_t *pars;

  assert(str != NULL);
  assert(typeptr != NULL);

  if ((pars = idl_create_parser()) == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }

  if (idl_puts(pars, str, strlen(str)) != -1) {
    if ((rc = idl_scan(pars)) == DDS_RETCODE_OK) {
      *typeptr = ddsts_context_take_root_type(pars->context);
    }
  } else {
    rc = ddsts_context_get_retcode(pars->context);
    assert(rc != DDS_RETCODE_OK);
  }

  idl_destroy_parser(pars);

  return rc;
}
