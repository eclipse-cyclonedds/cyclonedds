/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "idl.h"
#include "parser.h"

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/strtol.h"

static int32_t
push_line(idl_processor_t *proc, idl_line_t *dir)
{
  if (dir->file) {
    idl_file_t *file;
    for (file = proc->files; file; file = file->next) {
      if (strcmp(dir->file, file->name) == 0)
        break;
    }
    if (!file) {
      if (!(file = ddsrt_malloc(sizeof(*file))))
        return IDL_MEMORY_EXHAUSTED;
      file->name = dir->file;
      file->next = proc->files;
      proc->files = file;
      /* do not free filename on return */
      dir->file = NULL;
    } else {
      ddsrt_free(dir->file);
    }
    proc->scanner.position.file = (const char *)file->name;
  }
  proc->scanner.position.line = dir->line;
  proc->scanner.position.column = 1;
  ddsrt_free(dir);
  proc->directive = NULL;
  return 0;
}

static int32_t
parse_line(idl_processor_t *proc, idl_token_t *tok)
{
  idl_line_t *dir = (idl_line_t *)proc->directive;

  switch (proc->state) {
    case IDL_SCAN_LINE: {
      char *end;
      unsigned long long ullng;

      assert(!dir);

      if (tok->code != IDL_TOKEN_PP_NUMBER) {
        idl_error(proc, &tok->location,
          "no line number in #line directive");
        return IDL_PARSE_ERROR;
      }
      ddsrt_strtoull(tok->value.str, &end, 10, &ullng);
      if (end == tok->value.str || *end != '\0' || ullng > INT32_MAX) {
        idl_error(proc, &tok->location,
          "invalid line number in #line directive");
        return IDL_PARSE_ERROR;
      }
      if (!(dir = ddsrt_malloc(sizeof(*dir)))) {
        return IDL_MEMORY_EXHAUSTED;
      }
      dir->directive.type = IDL_LINE;
      dir->line = (uint32_t)ullng;
      dir->file = NULL;
      dir->extra_tokens = false;
      proc->directive = (idl_directive_t *)dir;
      proc->state = IDL_SCAN_FILENAME;
    } break;
    case IDL_SCAN_FILENAME:
      assert(dir);
      proc->state = IDL_SCAN_EXTRA_TOKEN;
      if (tok->code != '\n' && tok->code != 0) {
        if (tok->code != IDL_TOKEN_STRING_LITERAL) {
          idl_error(proc, &tok->location,
            "invalid filename in #line directive");
          return IDL_PARSE_ERROR;
        }
        assert(dir && !dir->file);
        dir->file = tok->value.str;
        /* do not free string on return */
        tok->value.str = NULL;
        break;
      }
      /* fall through */
    case IDL_SCAN_EXTRA_TOKEN:
      assert(dir);
      if (tok->code == '\n' || tok->code == 0) {
        proc->state = IDL_SCAN;
        return push_line(proc, dir);
      } else if (!dir->extra_tokens) {
        idl_warning(proc, &tok->location,
          "extra tokens at end of #line directive");
      }
      break;
    default:
      assert(0);
      break;
  }
  return 0;
}

static int32_t
push_keylist(idl_processor_t *proc, idl_keylist_t *dir)
{
  ddsts_pragma_open(proc->context);
  if (!ddsts_pragma_add_identifier(proc->context, dir->data_type))
    return IDL_MEMORY_EXHAUSTED;
  ddsrt_free(dir->data_type);
  dir->data_type = NULL;
  for (char **key = dir->keys; key && *key; key++) {
    if (!ddsts_pragma_add_identifier(proc->context, *key))
      return IDL_MEMORY_EXHAUSTED;
    ddsrt_free(*key);
    *key = NULL;
  }
  ddsrt_free(dir->keys);
  ddsrt_free(dir);
  proc->directive = NULL;
  switch (ddsts_pragma_close(proc->context)) {
    case DDS_RETCODE_OUT_OF_RESOURCES:
      return IDL_MEMORY_EXHAUSTED;
    case DDS_RETCODE_OK:
      break;
    default:
      return IDL_PARSE_ERROR;
  }
  return 0;
}

static int32_t
parse_keylist(idl_processor_t *proc, idl_token_t *tok)
{
  idl_keylist_t *dir = (idl_keylist_t *)proc->directive;

  /* #pragma keylist does not support scoped names */

  switch (proc->state) {
    case IDL_SCAN_KEYLIST:
      if (tok->code == '\n' || tok->code == '\0') {
        idl_error(proc, &tok->location,
          "no data-type in #pragma keylist directive");
        return IDL_PARSE_ERROR;
      } else if (tok->code != IDL_TOKEN_IDENTIFIER) {
        idl_error(proc, &tok->location,
          "invalid data-type in #pragma keylist directive");
        return IDL_PARSE_ERROR;
      }
      assert(!dir);
      if (!(dir = ddsrt_malloc(sizeof(*dir))))
        return IDL_MEMORY_EXHAUSTED;
      dir->directive.type = IDL_KEYLIST;
      dir->data_type = tok->value.str;
      dir->keys = NULL;
      proc->directive = (idl_directive_t *)dir;
      /* do not free identifier on return */
      tok->value.str = NULL;
      proc->state = IDL_SCAN_KEY;
      break;
    case IDL_SCAN_DATA_TYPE:
    case IDL_SCAN_KEY: {
      char **keys = dir->keys;
      size_t cnt = 0;

      if (tok->code == '\n' || tok->code == '\0') {
        proc->state = IDL_SCAN;
        return push_keylist(proc, dir);
      } else if (tok->code == ',' && keys) {
        /* #pragma keylist takes space or comma separated list of keys */
        break;
      } else if (tok->code != IDL_TOKEN_IDENTIFIER) {
        idl_error(proc, &tok->location,
          "invalid key in #pragma keylist directive");
        return IDL_PARSE_ERROR;
      } else if (idl_istoken(tok->value.str, 1)) {
        idl_error(proc, &tok->location,
          "invalid key %s in #pragma keylist directive", tok->value.str);
        return IDL_PARSE_ERROR;
      }

      for (; keys && *keys; keys++, cnt++) /* count keys */ ;

      if (!(keys = ddsrt_realloc(dir->keys, sizeof(*keys) * (cnt + 2))))
        return IDL_MEMORY_EXHAUSTED;

      keys[cnt++] = tok->value.str;
      keys[cnt  ] = NULL;
      dir->keys = keys;
      /* do not free identifier on return */
      tok->value.str = NULL;
    } break;
    default:
      assert(0);
      break;
  }
  return 0;
}

int32_t idl_parse_directive(idl_processor_t *proc, idl_token_t *tok)
{
  /* order is important here */
  if ((proc->state & IDL_SCAN_LINE) == IDL_SCAN_LINE) {
    return parse_line(proc, tok);
  } else if ((proc->state & IDL_SCAN_KEYLIST) == IDL_SCAN_KEYLIST) {
    return parse_keylist(proc, tok);
  } else if (proc->state == IDL_SCAN_PRAGMA) {
    /* expect keylist */
    if (tok->code == IDL_TOKEN_IDENTIFIER) {
      if (strcmp(tok->value.str, "keylist") == 0) {
        proc->state = IDL_SCAN_KEYLIST;
        return 0;
      }
      idl_error(proc, &tok->location,
        "unsupported #pragma directive %s", tok->value.str);
      return IDL_PARSE_ERROR;
    }
  } else if (proc->state == IDL_SCAN_DIRECTIVE_NAME) {
    if (tok->code == IDL_TOKEN_IDENTIFIER) {
      /* expect line or pragma */
      if (strcmp(tok->value.str, "line") == 0) {
        proc->state = IDL_SCAN_LINE;
        return 0;
      } else if (strcmp(tok->value.str, "pragma") == 0) {
        /* support #pragma keylist for backwards compatibility */
        proc->state = IDL_SCAN_PRAGMA;
        return 0;
      }
    } else if (tok->code == '\n' || tok->code == '\0') {
      proc->state = IDL_SCAN;
      return 0;
    }
  } else if (proc->state == IDL_SCAN_DIRECTIVE) {
    /* expect # */
    if (tok->code == '#') {
      proc->state = IDL_SCAN_DIRECTIVE_NAME;
      return 0;
    }
  }

  idl_error(proc, &tok->location, "invalid compiler directive");
  return IDL_PARSE_ERROR;
}
