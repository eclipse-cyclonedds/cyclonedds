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
#include "config.h"

#include <stdlib.h>
#if HAVE_XLOCALE_H
# include <xlocale.h>
#elif HAVE_LOCALE_H
# include <locale.h>
#endif
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "parser.h"
#include "idl/processor.h"
#include "idl/string.h"
#include "directive.h"
#include "scanner.h"
#include "tree.h"
#include "scope.h"

idl_retcode_t idl_processor_init(idl_processor_t *proc)
{
  idl_name_t *name;
  idl_scope_t *scope;
  idl_entry_t *entry;
  void *yypstate, *locale;

  name = calloc(1, sizeof(*name));
  name->identifier = idl_strdup("<GLOBAL>");
  entry = calloc(1, sizeof(*entry));
  entry->type = IDL_SCOPE;
  entry->name = name;
  scope = calloc(1, sizeof(*scope));
  scope->name = entry->name;
  scope->table.first = scope->table.last = entry;

  if (!(yypstate = idl_yypstate_new()))
    goto fail_yypstate;
#if HAVE_NEWLOCALE
# if __APPLE__ || __FreeBSD__
  if (!(locale = newlocale(LC_ALL_MASK, NULL, NULL)))
    goto fail_locale;
# else
  if (!(locale = newlocale(LC_ALL, "C", (locale_t)0)))
    goto fail_locale;
# endif
#elif HAVE__CREATE_LOCALE
  if (!(locale = _create_locale(LC_ALL, "C")))
    goto fail_locale;
#endif

  memset(proc, 0, sizeof(*proc));
  proc->locale = locale;
  proc->parser.yypstate = yypstate;
  proc->global_scope = proc->scope = scope;

  return IDL_RETCODE_OK;
fail_locale:
  idl_yypstate_delete(yypstate);
fail_yypstate:
  return IDL_RETCODE_NO_MEMORY;
}

void idl_processor_fini(idl_processor_t *proc)
{
  if (proc) {
#if HAVE_FREELOCALE
    if (proc->locale)
      freelocale((locale_t)proc->locale);
#elif HAVE__FREE_LOCALE
    if (proc->locale)
      _free_locale((_locale_t)proc->locale);
#endif

    if (proc->parser.yypstate) {
      idl_yypstate_delete_stack(proc->parser.yypstate);
      idl_yypstate_delete(proc->parser.yypstate);
    }
    /* directive */
    if (proc->directive) {
      if (proc->directive->mask & IDL_LINE) {
        idl_line_t *line = (idl_line_t *)proc->directive;
        if (line->line) {
          free(line->line);
        }
        if (line->file) {
          assert(line->file->node.mask & IDL_STRING);
          if (line->file->value.str)
            free(line->file->value.str);
          free(line->file);
        }
        free(line);
      } else {
        assert(proc->directive->mask & IDL_KEYLIST);
        idl_keylist_t *keylist = (idl_keylist_t *)proc->directive;
        idl_delete_name(keylist->data_type);
        for (size_t i=0; keylist->keys && keylist->keys[i]; i++)
          idl_delete_name(keylist->keys[i]);
        free(keylist->keys);
        free(keylist);
      }
    }
    /* files */
    if (proc->files) {
      idl_file_t *file, *next;
      for (file = proc->files; file; file = next) {
        next = file->next;
        if (file->name)
          free(file->name);
        free(file);
      }
    }
    /* symbol table */
    idl_delete_scope(proc->global_scope);
    if (proc->buffer.data)
      free(proc->buffer.data);
  }
}

static void
idl_log(
  idl_processor_t *proc, uint32_t prio, const idl_location_t *loc, const char *fmt, va_list ap)
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

#define IDL_LC_ERROR 1
#define IDL_LC_WARNING 2

void
idl_verror(
  idl_processor_t *proc, const idl_location_t *loc, const char *fmt, va_list ap)
{
  idl_log(proc, IDL_LC_ERROR, loc, fmt, ap);
}

void
idl_error(
  idl_processor_t *proc, const idl_location_t *loc, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  idl_log(proc, IDL_LC_ERROR, loc, fmt, ap);
  va_end(ap);
}

void
idl_warning(
  idl_processor_t *proc, const idl_location_t *loc, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  idl_log(proc, IDL_LC_WARNING, loc, fmt, ap);
  va_end(ap);
}

static idl_retcode_t
idl_parse_code(idl_processor_t *proc, idl_token_t *tok, idl_node_t **nodeptr)
{
  idl_retcode_t ret;
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

  switch ((ret = idl_yypush_parse(
    proc->parser.yypstate, tok->code, &yylval, &tok->location, proc, nodeptr)))
  {
    case 0:
      break;
    case 1: /* parse error */
      return IDL_RETCODE_SYNTAX_ERROR;
    case 2: /* out of memory */
      return IDL_RETCODE_NO_MEMORY;
    case YYPUSH_MORE:
      return IDL_RETCODE_PUSH_MORE;
    default:
      assert(ret < 0);
      return ret;
      break;
  }

  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_parse(idl_processor_t *proc, idl_node_t **nodeptr)
{
  idl_retcode_t ret;
  idl_token_t tok;
  memset(&tok, 0, sizeof(tok));

  do {
    if ((ret = idl_scan(proc, &tok)) < 0) {
      break;
    }
    ret = IDL_RETCODE_OK;
    if (tok.code != IDL_TOKEN_COMMENT && tok.code != IDL_TOKEN_LINE_COMMENT) {
      if ((unsigned)proc->state & (unsigned)IDL_SCAN_DIRECTIVE) {
        ret = idl_parse_directive(proc, &tok);
        if ((tok.code == '\0') &&
            (ret == IDL_RETCODE_OK || ret == IDL_RETCODE_PUSH_MORE))
          ret = idl_parse_code(proc, &tok, nodeptr);
      } else if (tok.code != '\n') {
        ret = idl_parse_code(proc, &tok, nodeptr);
      }
    }
    /* free memory associated with token value */
    switch (tok.code) {
      case '\n':
        proc->state = IDL_SCAN;
        break;
      case IDL_TOKEN_IDENTIFIER:
      case IDL_TOKEN_CHAR_LITERAL:
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

  return ret;
}

int32_t
idl_parse_string(const char *str, uint32_t flags, idl_tree_t **treeptr)
{
  int32_t ret;
  idl_tree_t *tree = NULL;
  idl_node_t *root = NULL;
  idl_processor_t proc;

  assert(str);
  assert(treeptr);

  if ((ret = idl_processor_init(&proc)) != 0) {
    return ret;
  } else if (!(tree = calloc(1, sizeof(*tree)))) {
    idl_processor_fini(&proc);
    return IDL_RETCODE_NO_MEMORY;
  }

  proc.flags = flags;
  proc.buffer.data = (char *)str;
  proc.buffer.size = proc.buffer.used = strlen(str);
  proc.scanner.cursor = proc.buffer.data;
  proc.scanner.limit = proc.buffer.data + proc.buffer.used;
  proc.scanner.position.line = 1;
  proc.scanner.position.column = 1;

  if ((ret = idl_parse(&proc, &root)) == IDL_RETCODE_OK) {
    assert(root);
    tree->root = root;
    *treeptr = tree;
  } else {
    assert(!root);
    free(tree);
  }

  proc.buffer.data = NULL;
  idl_processor_fini(&proc);

  return ret;
}
