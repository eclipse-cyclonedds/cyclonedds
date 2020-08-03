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
#include "parser.h"
#include "tt_create.h"

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/string.h"

int32_t idl_processor_init(idl_processor_t *proc)
{
  memset(proc, 0, sizeof(*proc));
  if (!(proc->context = ddsts_create_context())) {
    return IDL_MEMORY_EXHAUSTED;
  } else if (!(proc->parser.yypstate = (void *)idl_yypstate_new())) {
    ddsts_free_context(proc->context);
    return IDL_MEMORY_EXHAUSTED;
  }

  return 0;
}

void idl_processor_fini(idl_processor_t *proc)
{
  if (proc) {
    idl_file_t *file, *next;

    if (proc->parser.yypstate)
      idl_yypstate_delete((idl_yypstate *)proc->parser.yypstate);
    if (proc->context)
      ddsts_free_context(proc->context);
    if (proc->directive) {
      switch (proc->directive->type) {
        case IDL_LINE: {
          idl_line_t *dir = (idl_line_t *)proc->directive;
          ddsrt_free(dir->file);
        } break;
        case IDL_KEYLIST: {
          idl_keylist_t *dir = (idl_keylist_t *)proc->directive;
          ddsrt_free(dir->data_type);
          for (char **keys = dir->keys; keys && *keys; keys++)
            ddsrt_free(*keys);
          ddsrt_free(dir->keys);
        } break;
        default:
          break;
      }
      ddsrt_free(proc->directive);
    }
    for (file = proc->files; file; file = next) {
      next = file->next;
      if (file->name)
        ddsrt_free(file->name);
      ddsrt_free(file);
    }

    if (proc->buffer.data)
      ddsrt_free(proc->buffer.data);
  }
}

static void
idl_log(
  idl_processor_t *proc, uint32_t prio, idl_location_t *loc, const char *fmt, va_list ap)
{
  char buf[1024];
  int cnt;
  size_t off;

  (void)proc;
  (void)prio;
  if (loc->first.file)
    cnt = snprintf(
      buf, sizeof(buf)-1, "%s:%u:%u: ", loc->first.file, loc->first.line, loc->first.column);
  else
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

void
idl_verror(
  idl_processor_t *proc, idl_location_t *loc, const char *fmt, va_list ap)
{
  idl_log(proc, DDS_LC_ERROR, loc, fmt, ap);
}

void
idl_error(
  idl_processor_t *proc, idl_location_t *loc, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  idl_log(proc, DDS_LC_ERROR, loc, fmt, ap);
  va_end(ap);
}

void
idl_warning(
  idl_processor_t *proc, idl_location_t *loc, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  idl_log(proc, DDS_LC_WARNING, loc, fmt, ap);
  va_end(ap);
}

int32_t idl_parse_code(idl_processor_t *proc, idl_token_t *tok)
{
  YYSTYPE yylval;

  /* prepare Bison yylval */
  switch (tok->code) {
    case IDL_TOKEN_IDENTIFIER:
    case IDL_TOKEN_CHAR_LITERAL:
    case IDL_TOKEN_STRING_LITERAL:
      yylval.str = tok->value.str;
      break;
    case IDL_TOKEN_INTEGER_LITERAL:
      yylval.ullng = tok->value.ullng;
      break;
    default:
      memset(&yylval, 0, sizeof(yylval));
      break;
  }

  switch (idl_yypush_parse(
    proc->parser.yypstate, tok->code, &yylval, &tok->location, proc))
  {
    case YYPUSH_MORE:
      return IDL_PUSH_MORE;
    case 1: /* parse error */
      return IDL_PARSE_ERROR;
    case 2: /* out of memory */
      return IDL_MEMORY_EXHAUSTED;
    default:
      break;
  }

  return 0;
}

int32_t idl_parse(idl_processor_t *proc)
{
  int32_t code;
  idl_token_t tok;
  memset(&tok, 0, sizeof(tok));

  do {
    if ((code = idl_scan(proc, &tok)) < 0)
      break;
    if ((unsigned)proc->state & (unsigned)IDL_SCAN_DIRECTIVE)
      code = idl_parse_directive(proc, &tok);
    else if (code != '\n')
      code = idl_parse_code(proc, &tok);
    else
      code = 0;
    /* free memory associated with token value */
    switch (tok.code) {
      case '\n':
        proc->state = IDL_SCAN;
        break;
      case IDL_TOKEN_IDENTIFIER:
      case IDL_TOKEN_CHAR_LITERAL:
      case IDL_TOKEN_STRING_LITERAL:
      case IDL_TOKEN_PP_NUMBER:
        if (tok.value.str)
          ddsrt_free(tok.value.str);
        break;
      default:
        break;
    }
  } while (tok.code != '\0' && (code == 0 || code == IDL_PUSH_MORE));

  return code;
}

int32_t
idl_parse_string(const char *str, ddsts_type_t **typeptr)
{
  int32_t ret;
  idl_processor_t proc;

  assert(str != NULL);
  assert(typeptr != NULL);

  if ((ret = idl_processor_init(&proc)) != 0)
    return ret;

  proc.buffer.data = (char *)str;
  proc.buffer.size = proc.buffer.used = strlen(str);
  proc.scanner.cursor = proc.buffer.data;
  proc.scanner.limit = proc.buffer.data + proc.buffer.used;
  proc.scanner.position.line = 1;
  proc.scanner.position.column = 1;

  if ((ret = idl_parse(&proc)) == 0)
    *typeptr = ddsts_context_take_root_type(proc.context);

  proc.buffer.data = NULL;

  idl_processor_fini(&proc);

  return ret;
}
