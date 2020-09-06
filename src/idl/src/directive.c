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
#include "config.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#if HAVE_XLOCALE_H
# include <xlocale.h>
#elif HAVE_LOCALE_H
# include <locale.h>
#endif
#include <string.h>

#if !HAVE_STRTOULL_L && HAVE__STRTOULL_L
#define strtoull_l(...) _strtoull_l(__VA_ARGS__)
#endif

#if !HAVE_STRTOLD_L && HAVE__STRTOLD_L
#define strtold_l(...) _strtold_l(__VA_ARGS__)
#endif

#include "idl/processor.h"
#include "idl/string.h"
#include "directive.h"
#include "scope.h"
#include "table.h"
#include "tree.h"
#include "parser.h"

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
      idl_file_t *last;
      if (!(file = calloc(1, sizeof(*file))))
        return IDL_RETCODE_NO_MEMORY;
      file->name = dir->file;
      if (proc->files) {
        /* maintain order to ensure the first file is actually first */
        for (last = proc->files; last->next; last = last->next) ;
        last->next = file;
      } else {
        proc->files = file;
      }
      /* do not free filename on return */
      dir->file = NULL;
    } else {
      free(dir->file);
    }
    proc->scanner.position.file = (const char *)file->name;
  }
  proc->scanner.position.line = dir->line;
  proc->scanner.position.column = 1;
  free(dir);
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
        return IDL_RETCODE_SYNTAX_ERROR;
      }
      ullng = strtoull_l(tok->value.str, &end, 10, proc->locale);
      if (end == tok->value.str || *end != '\0' || ullng > INT32_MAX) {
        idl_error(proc, &tok->location,
          "invalid line number in #line directive");
        return IDL_RETCODE_SYNTAX_ERROR;
      }
      if (!(dir = malloc(sizeof(*dir)))) {
        return IDL_RETCODE_NO_MEMORY;
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
          return IDL_RETCODE_SYNTAX_ERROR;
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
push_keylist(idl_processor_t *proc, idl_pragma_keylist_t *dir)
{
  idl_struct_t *_struct;
  idl_keylist_t *keylist;
  const idl_symbol_t *sym;
  const char *name, *scope;

  keylist = dir->keylist;
  assert(keylist);

  name = keylist->data_type->identifier;
  scope = idl_scope(proc);

  {
    char *scoped_name;
    const char *fmt = strcmp(scope, "::") == 0 ? "%s%s" : "%s::%s";
    if (idl_asprintf(&scoped_name, fmt, scope, name) == -1) {
      return IDL_RETCODE_NO_MEMORY;
    }
    if ((sym = idl_find_symbol(proc, NULL, scoped_name, NULL))) {
      while (sym && idl_is_masked(sym->node, IDL_FORWARD))
        sym = idl_find_symbol(proc, NULL, scoped_name, sym);
    }
    free(scoped_name);
  }

  if (!sym) {
    idl_error(proc, idl_location(dir->keylist->data_type),
      "unknown data-type %s in keylist directive", name);
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  _struct = (idl_struct_t *)sym->node;
  if (!idl_is_struct(_struct)) {
    idl_error(proc, idl_location(dir->keylist->data_type),
      "data-type %s in keylist directive is not a struct", name);
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (_struct->keylist) {
    idl_error(proc, idl_location(dir->keylist->data_type),
      "redefinition of keylist for struct %s", name);
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  keylist->node.parent = (idl_node_t *)_struct;
  _struct->keylist = keylist;
  free(dir);
  proc->directive = NULL;

  for (idl_key_t *key = keylist->keys; key; key = idl_next(key)) {
    idl_member_t *member = _struct->members;
    for (; member; member = idl_next(member)) {
      idl_declarator_t *declarator = member->declarators;
      assert(declarator);
      for (; declarator; declarator = idl_next(declarator)) {
        if (strcmp(declarator->identifier, key->identifier) == 0)
          break;
      }
      if (declarator)
        break;
    }
    if (!member) {
      idl_error(proc, idl_location(key),
        "unknown struct member %s in keylist directive", key->identifier);
      return IDL_RETCODE_SEMANTIC_ERROR;
    } else if (idl_is_masked(member, IDL_KEY)) {
      idl_error(proc, idl_location(key),
        "redefinition of key %s in keylist directive", key->identifier);
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
    member->node.mask |= IDL_KEY;
  }

  return IDL_RETCODE_OK;
}

static int32_t
parse_keylist(idl_processor_t *proc, idl_token_t *tok)
{
  idl_pragma_keylist_t *dir = (idl_pragma_keylist_t *)proc->directive;

  /* #pragma keylist does not support scoped names */
  switch (proc->state) {
    case IDL_SCAN_KEYLIST: {
      idl_keylist_t *keylist;
      idl_data_type_t *data_type;

      if (tok->code == '\n' || tok->code == '\0') {
        idl_error(proc, &tok->location,
          "no data-type in #pragma keylist directive");
        return IDL_RETCODE_SYNTAX_ERROR;
      } else if (tok->code != IDL_TOKEN_IDENTIFIER) {
        idl_error(proc, &tok->location,
          "invalid data-type in #pragma keylist directive");
        return IDL_RETCODE_SYNTAX_ERROR;
      }
      assert(!dir);
      if (!(dir = calloc(1, sizeof(*dir))))
        return IDL_RETCODE_NO_MEMORY;
      proc->directive = (idl_directive_t *)dir;
      dir->directive.type = IDL_PRAGMA_KEYLIST;
      if (!(keylist = idl_create_keylist()))
        return IDL_RETCODE_NO_MEMORY;
      keylist->node.parent = NULL;
      keylist->node.location = tok->location;
      dir->keylist = keylist;
      if (!(data_type = idl_create_data_type()))
        return IDL_RETCODE_NO_MEMORY;
      data_type->node.parent = (idl_node_t *)dir->keylist;
      data_type->node.location = tok->location;
      data_type->identifier = tok->value.str;
      tok->value.str = NULL;
      dir->keylist->data_type = data_type;
      proc->state = IDL_SCAN_KEY;
    } break;
    case IDL_SCAN_KEY: {
      idl_key_t *key;

      if (tok->code == '\n' || tok->code == '\0') {
        proc->state = IDL_SCAN;
        return push_keylist(proc, dir);
      } else if (tok->code == ',' && dir->keylist->keys) {
        /* #pragma keylist takes space or comma separated list of keys */
        break;
      } else if (tok->code != IDL_TOKEN_IDENTIFIER) {
        idl_error(proc, &tok->location,
          "invalid key in #pragma keylist directive");
        return IDL_RETCODE_SYNTAX_ERROR;
      } else if (idl_iskeyword(proc, tok->value.str, 1)) {
        idl_error(proc, &tok->location,
          "invalid key %s in #pragma keylist directive", tok->value.str);
        return IDL_RETCODE_SYNTAX_ERROR;
      }

      if (!(key = idl_create_key()))
        return IDL_RETCODE_NO_MEMORY;
      key->node.parent = (idl_node_t *)dir->keylist;
      key->node.location = tok->location;
      key->identifier = tok->value.str;
      tok->value.str = NULL;
      if (!dir->keylist->keys) {
        dir->keylist->keys = key;
      } else {
        idl_node_t *last = (idl_node_t *)dir->keylist->keys;
        for (; last->next; last = last->next) ;
        last->next = (idl_node_t *)key;
        key->node.previous = last;
      }
    } break;
    default:
      assert(0);
      break;
  }
  return 0;
}

idl_retcode_t idl_parse_directive(idl_processor_t *proc, idl_token_t *tok)
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
      return IDL_RETCODE_SYNTAX_ERROR;
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
  return IDL_RETCODE_SYNTAX_ERROR;
}
