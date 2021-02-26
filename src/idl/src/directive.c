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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "idl/processor.h"
#include "idl/file.h"
#include "idl/string.h"

#include "symbol.h"
#include "tree.h"
#include "scope.h"
#include "directive.h"
#include "parser.h"

struct directive {
  enum {
    LINE, /**< #line directive */
    LINEMARKER, /**< GCC linemarker (extended line directive) */
    KEYLIST /**< #pragma keylist directive */
  } type;
};

struct line {
  struct directive directive;
  unsigned long long line;
  char *file;
  unsigned flags;
};

struct keylist {
  struct directive directive;
  idl_scoped_name_t *data_type;
  idl_field_name_t **keys;
};

static idl_retcode_t
push_file(idl_pstate_t *pstate, const char *inc)
{
  idl_file_t *file = pstate->files;
  for (; file && strcmp(file->name, inc); file = file->next) ;
  if (!file) {
    if (!(file = calloc(1, sizeof(*file))))
      return IDL_RETCODE_NO_MEMORY;
    file->next = pstate->files;
    pstate->files = file;
    if (!(file->name = idl_strdup(inc)))
      return IDL_RETCODE_NO_MEMORY;
  }
  pstate->scanner.position.file = file;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
push_source(idl_pstate_t *pstate, const char *inc, const char *abs, bool sys)
{
  idl_file_t *path = pstate->paths;
  idl_source_t *src, *last;
  for (; path && strcmp(path->name, abs); path = path->next) ;
  if (!path) {
    if (!(path = calloc(1, sizeof(*path))))
      return IDL_RETCODE_NO_MEMORY;
    path->next = pstate->paths;
    pstate->paths = path;
    if (!(path->name = idl_strdup(abs)))
      return IDL_RETCODE_NO_MEMORY;
  }
  if (push_file(pstate, inc))
    return IDL_RETCODE_NO_MEMORY;
  if (!(src = calloc(1, sizeof(*src))))
    return IDL_RETCODE_NO_MEMORY;
  src->file = pstate->files;
  src->path = path;
  src->system = sys;
  if (!pstate->sources) {
    pstate->sources = src;
  } else if (pstate->scanner.position.source->includes) {
    last = ((idl_source_t *)pstate->scanner.position.source)->includes;
    for (; last->next; last = last->next) ;
    last->next = src;
    src->previous = last;
    src->parent = pstate->scanner.position.source;
  } else {
    ((idl_source_t *)pstate->scanner.position.source)->includes = src;
    src->parent = pstate->scanner.position.source;
  }
  pstate->scanner.position.source = src;
  return IDL_RETCODE_OK;
}

#define START_OF_FILE (1u<<0)
#define RETURN_TO_FILE (1u<<1)
#define SYSTEM_FILE (1u<<2)
#define EXTRA_TOKENS (1u<<3)

static void delete_line(void *ptr)
{
  struct line *dir = (struct line *)ptr;
  assert(dir);
  if (dir->file)
    free(dir->file);
  free(dir);
}

static idl_retcode_t
push_line(idl_pstate_t *pstate, struct line *dir)
{
  idl_retcode_t ret = IDL_RETCODE_OK;

  assert(dir);
  if (dir->flags & (START_OF_FILE|RETURN_TO_FILE)) {
    bool sys = (dir->flags & SYSTEM_FILE) != 0;
    char *norm = NULL, *abs, *inc;
    abs = inc = dir->file;
    /* convert to normalized file name */
    if (!idl_isabsolute(abs)) {
      /* include paths are relative to the current file. so, strip file name,
         postfix with "/relative/path/to/file" and normalize */
      const char *cwd = pstate->scanner.position.source->path->name;
      const char *sep = cwd;
      assert(idl_isabsolute(cwd));
      for (size_t i=0; cwd[i]; i++) {
        if (idl_isseparator(cwd[i]))
          sep = cwd + i;
      }
      if (idl_asprintf(&abs, "%.*s/%s", (sep-cwd), cwd, inc) < 0)
        return IDL_RETCODE_NO_MEMORY;
    }
    idl_normalize_path(abs, &norm);
    if (abs != dir->file)
      free(abs);
    if (!norm)
      return IDL_RETCODE_NO_MEMORY;

    if (dir->flags & START_OF_FILE) {
      ret = push_source(pstate, inc, norm, sys);
    } else {
      assert(pstate->scanner.position.source);
      const idl_source_t *src = pstate->scanner.position.source;
      while (src) {
        if (strcmp(src->path->name, norm) == 0)
          break;
        src = src->parent;
      }
      if (src) {
        pstate->scanner.position.source = src;
        pstate->scanner.position.file = src->file;
      } else {
        idl_error(pstate, idl_location(dir),
          "Invalid #line directive, file '%s' not on include stack", inc);
        ret = IDL_RETCODE_SEMANTIC_ERROR;
      }
    }

    free(norm);
  } else {
    ret = push_file(pstate, dir->file);
  }

  if (ret)
    return ret;
  pstate->scanner.position.line = (uint32_t)dir->line;
  pstate->scanner.position.column = 1;
  delete_line(dir);
  pstate->directive = NULL;
  return IDL_RETCODE_OK;
}

/* for proper handling of includes by parsing line controls, GCCs linemarkers
   are required. they are enabled in mcpp by defining the compiler to be GNUC
   instead of INDEPENDANT.
   See: https://gcc.gnu.org/onlinedocs/cpp/Preprocessor-Output.html */
static int32_t
parse_line(idl_pstate_t *pstate, idl_token_t *tok)
{
  struct line *dir = (struct line *)pstate->directive;
  unsigned long long ullng;

  switch (pstate->scanner.state) {
    case IDL_SCAN_LINE:
      if (tok->code != IDL_TOKEN_PP_NUMBER) {
        idl_error(pstate, &tok->location,
          "No line number in #line directive");
        return IDL_RETCODE_SYNTAX_ERROR;
      }
      ullng = idl_strtoull(tok->value.str, NULL, 10);
      if (ullng == 0 || ullng > INT32_MAX) {
        idl_error(pstate, &tok->location,
          "Invalid line number in #line directive");
        return IDL_RETCODE_SYNTAX_ERROR;
      } else {
        dir->line = ullng;
      }
      pstate->scanner.state = IDL_SCAN_FILENAME;
      break;
    case IDL_SCAN_FILENAME:
      if (tok->code == '\n' || tok->code == '\0') {
        return push_line(pstate, dir);
      } else if (tok->code != IDL_TOKEN_STRING_LITERAL) {
        idl_error(pstate, &tok->location,
          "Invalid filename in #line directive");
        return IDL_RETCODE_SYNTAX_ERROR;
      } else {
        dir->file = tok->value.str;
      }
      tok->value.str = NULL; /* do not free */
      pstate->scanner.state = IDL_SCAN_FLAGS;
      break;
    case IDL_SCAN_FLAGS:
      if (tok->code == '\n' || tok->code == '\0') {
        return push_line(pstate, dir);
      } else if (dir->directive.type == LINE) {
        goto extra_tokens;
      } else if (tok->code == IDL_TOKEN_PP_NUMBER) {
        if (strcmp(tok->value.str, "1") == 0) {
          if (dir->flags & (START_OF_FILE|RETURN_TO_FILE))
            goto extra_tokens;
          dir->flags |= START_OF_FILE;
        } else if (strcmp(tok->value.str, "2") == 0) {
          if (dir->flags & (START_OF_FILE|RETURN_TO_FILE))
            goto extra_tokens;
          dir->flags |= RETURN_TO_FILE;
        } else if (strcmp(tok->value.str, "3") == 0) {
          if (dir->flags & (SYSTEM_FILE))
            goto extra_tokens;
          dir->flags |= SYSTEM_FILE;
        } else {
          goto extra_tokens;
        }
      } else {
extra_tokens:
        idl_warning(pstate, &tok->location,
          "Extra tokens at end of #line directive");
        pstate->scanner.state = IDL_SCAN_EXTRA_TOKENS;
      }
      break;
    default:
      if (tok->code == '\n' || tok->code == '\0')
        return push_line(pstate, dir);
      break;
  }

  return IDL_RETCODE_OK;
}

static void delete_keylist(void *ptr)
{
  struct keylist *dir = ptr;
  assert(dir);
  idl_delete_scoped_name(dir->data_type);
  if (dir->keys) {
    for (size_t i=0; dir->keys[i]; i++)
      idl_delete_field_name(dir->keys[i]);
    free(dir->keys);
  }
  free(dir);
}

static idl_retcode_t
push_keylist(idl_pstate_t *pstate, struct keylist *dir)
{
  idl_retcode_t ret;
  idl_scope_t *scope;
  idl_struct_t *node;
  idl_keylist_t *keylist = NULL;
  const idl_declaration_t *declaration;

  assert(dir);
  if (!(declaration = idl_find_scoped_name(pstate, NULL, dir->data_type, 0u))) {
    idl_error(pstate, idl_location(dir->data_type),
      "Unknown data-type '%s' in keylist directive", dir->data_type->identifier);
    return IDL_RETCODE_SEMANTIC_ERROR;
  }
  node = (idl_struct_t *)declaration->node;
  scope = (idl_scope_t *)declaration->scope;
  if (!idl_is_struct(node)) {
    idl_error(pstate, idl_location(dir->data_type),
      "Invalid data-type '%s' in keylist directive", dir->data_type->identifier);
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (node->keylist) {
    idl_error(pstate, idl_location(dir->data_type),
      "Redefinition of keylist for data-type '%s'", dir->data_type->identifier);
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  /* check for duplicates */
  for (size_t i=0; dir->keys && dir->keys[i]; i++) {
    for (size_t j=i+1; dir->keys && dir->keys[j]; j++) {
      size_t n=0;
      if (dir->keys[i]->length != dir->keys[j]->length)
        continue;
      for (; n < dir->keys[i]->length; n++) {
        const char *s1, *s2;
        s1 = dir->keys[i]->names[n]->identifier;
        s2 = dir->keys[j]->names[n]->identifier;
        if (strcmp(s1, s2) != 0)
          break;
      }
      if (n == dir->keys[i]->length) {
        idl_error(pstate, idl_location(dir->keys[j]),
          "Duplicate key '%s' in keylist directive", dir->keys[j]->identifier);
        return IDL_RETCODE_SEMANTIC_ERROR;
      }
    }
  }

  if ((ret = idl_create_keylist(pstate, idl_location(dir->data_type), &keylist)))
    return ret;
  keylist->node.parent = (idl_node_t *)node;
  node->keylist = keylist;

  for (size_t i=0; dir->keys && dir->keys[i]; i++) {
    idl_key_t *key = NULL;
    const idl_declarator_t *declarator;
    const idl_type_spec_t *type_spec;

    if (!(declaration = idl_find_field_name(pstate, scope, dir->keys[i], 0u))) {
      idl_error(pstate, idl_location(dir->keys[i]),
        "Unknown key '%s' in keylist directive", dir->keys[i]->identifier);
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
    declarator = (const idl_declarator_t *)declaration->node;
    assert(idl_is_declarator(declarator));
    type_spec = idl_type_spec(declarator);
    type_spec = idl_unalias(type_spec, IDL_UNALIAS_IGNORE_ARRAY);
    if (!(idl_is_base_type(type_spec) || idl_is_string(type_spec))) {
      idl_error(pstate, idl_location(dir->keys[i]),
        "Invalid key '%s' type in keylist directive", dir->keys[i]->identifier);
      return IDL_RETCODE_SEMANTIC_ERROR;
    }

    if ((ret = idl_create_key(pstate, idl_location(dir->keys[i]), &key)))
      return ret;
    key->node.parent = (idl_node_t *)keylist;
    key->field_name = dir->keys[i];
    keylist->keys = idl_push_node(keylist->keys, key);
    dir->keys[i] = NULL; /* do not free */
  }

  delete_keylist(dir);
  pstate->directive = NULL;
  return IDL_RETCODE_OK;
}

static int stash_name(idl_pstate_t *pstate, idl_location_t *loc, char *str)
{
  struct keylist *dir = (struct keylist *)pstate->directive;
  idl_name_t *name = NULL;

  if (idl_create_name(pstate, loc, str, &name))
    goto err_alloc;
  if (idl_push_scoped_name(pstate, dir->data_type, name))
    goto err_alloc;
  return 0;
err_alloc:
  if (name)
    free(name);
  return -1;
}

static int stash_data_type(idl_pstate_t *pstate, idl_location_t *loc, char *str)
{
  struct keylist *dir = (struct keylist *)pstate->directive;
  idl_name_t *name = NULL;

  if (idl_create_name(pstate, loc, str, &name))
    goto err_alloc;
  if (idl_create_scoped_name(pstate, loc, name, false, &dir->data_type))
    goto err_alloc;
  return 0;
err_alloc:
  if (name)
    free(name);
  return -1;
}

static int stash_field(idl_pstate_t *pstate, idl_location_t *loc, char *str)
{
  struct keylist *dir = (struct keylist *)pstate->directive;
  idl_name_t *name = NULL;
  size_t n;

  assert(dir->keys);
  if (idl_create_name(pstate, loc, str, &name))
    goto err_alloc;
  assert(dir->keys);
  for (n=0; dir->keys[n]; n++) ;
  assert(n);
  if (idl_push_field_name(pstate, dir->keys[n-1], name))
    goto err_alloc;
  return 0;
err_alloc:
  if (name)
    free(name);
  return -1;
}

static int stash_key(idl_pstate_t *pstate, idl_location_t *loc, char *str)
{
  struct keylist *dir = (struct keylist *)pstate->directive;
  idl_name_t *name = NULL;
  idl_field_name_t **keys;
  size_t n;

  for (n=0; dir->keys && dir->keys[n]; n++) ;
  if (!(keys = realloc(dir->keys, (n + 2) * sizeof(*keys))))
    goto err_alloc;
  dir->keys = keys;
  keys[n+0] = NULL;
  if (idl_create_name(pstate, loc, str, &name))
    goto err_alloc;
  if (idl_create_field_name(pstate, loc, name, &keys[n+0]))
    goto err_alloc;
  keys[n+1] = NULL;
  return 0;
err_alloc:
  if (name)
    free(name);
  return -1;
}

static int32_t
parse_keylist(idl_pstate_t *pstate, idl_token_t *tok)
{
  struct keylist *dir = (struct keylist *)pstate->directive;
  assert(dir);

  /* #pragma keylist does not support scoped names for data-type */
  switch (pstate->scanner.state) {
    case IDL_SCAN_NAME:
      if (tok->code != IDL_TOKEN_IDENTIFIER) {
        idl_error(pstate, &tok->location,
          "Invalid keylist directive, expected identifier");
        return IDL_RETCODE_SEMANTIC_ERROR;
      } else if (idl_iskeyword(pstate, tok->value.str, 1)) {
        idl_error(pstate, &tok->location,
          "Invalid identifier '%s' in keylist data-type", tok->value.str);
        return IDL_RETCODE_SEMANTIC_ERROR;
      }

      if (stash_name(pstate, &tok->location, tok->value.str))
        return IDL_RETCODE_NO_MEMORY;
      tok->value.str = NULL;
      pstate->scanner.state = IDL_SCAN_SCOPE;
      break;
    case IDL_SCAN_KEYLIST:
      /* accept leading scope, i.e. "::" in "::foo" */
      if (tok->code == IDL_TOKEN_SCOPE) {
        pstate->scanner.state = IDL_SCAN_DATA_TYPE;
        break;
      }
      /* fall through */
    case IDL_SCAN_DATA_TYPE:
      assert(!dir->data_type);
      if (tok->code == '\n' || tok->code == '\0') {
        idl_error(pstate, &tok->location,
          "Missing data-type in #pragma keylist directive");
        return IDL_RETCODE_SYNTAX_ERROR;
      } else if (tok->code != IDL_TOKEN_IDENTIFIER) {
        idl_error(pstate, &tok->location,
          "Invalid data-type in #pragma keylist directive");
        return IDL_RETCODE_SYNTAX_ERROR;
      }

      if (stash_data_type(pstate, &tok->location, tok->value.str))
        return IDL_RETCODE_NO_MEMORY;
      tok->value.str = NULL;
      pstate->scanner.state = IDL_SCAN_SCOPE;
      break;
    case IDL_SCAN_FIELD:
      assert(dir->keys);
      if (tok->code != IDL_TOKEN_IDENTIFIER) {
        idl_error(pstate, &tok->location,
          "Invalid keylist directive, identifier expected");
        return IDL_RETCODE_SEMANTIC_ERROR;
      } else if (idl_iskeyword(pstate, tok->value.str, 1)) {
        idl_error(pstate, &tok->location,
          "Invalid key '%s' in keylist directive", tok->value.str);
        return IDL_RETCODE_SEMANTIC_ERROR;
      }

      if (stash_field(pstate, &tok->location, tok->value.str))
        return IDL_RETCODE_NO_MEMORY;
      tok->value.str = NULL;
      pstate->scanner.state = IDL_SCAN_ACCESS;
      break;
    case IDL_SCAN_SCOPE:
    case IDL_SCAN_ACCESS:
      if (pstate->scanner.state == IDL_SCAN_SCOPE) {
        /* accept scoped name for data-type, assume key otherwise */
        if (tok->code == IDL_TOKEN_SCOPE) {
          pstate->scanner.state = IDL_SCAN_NAME;
          break;
        }
      } else if (pstate->scanner.state == IDL_SCAN_ACCESS) {
        /* accept field name for key, assume key otherwise */
        if (tok->code == '.') {
          pstate->scanner.state = IDL_SCAN_FIELD;
          break;
        }
      }
      pstate->scanner.state = IDL_SCAN_KEY;
      /* fall through */
    case IDL_SCAN_KEY:
      if (tok->code == '\n' || tok->code == '\0') {
        return push_keylist(pstate, dir);
      } else if (tok->code == ',' && dir->keys) {
        /* #pragma keylist takes space or comma separated list of keys */
        break;
      } else if (tok->code != IDL_TOKEN_IDENTIFIER) {
        idl_error(pstate, &tok->location,
          "Invalid token in #pragma keylist directive");
        return IDL_RETCODE_SEMANTIC_ERROR;
      } else if (idl_iskeyword(pstate, tok->value.str, 1)) {
        idl_error(pstate, &tok->location,
          "Invalid key '%s' in #pragma keylist directive", tok->value.str);
        return IDL_RETCODE_SEMANTIC_ERROR;
      }

      if (stash_key(pstate, &tok->location, tok->value.str))
        return IDL_RETCODE_NO_MEMORY;
      tok->value.str = NULL;
      pstate->scanner.state = IDL_SCAN_ACCESS;
      break;
    default:
      assert(0);
      break;
  }

  return IDL_RETCODE_OK;
}

void idl_delete_directive(idl_pstate_t *pstate)
{
  if (pstate->directive) {
    struct directive *dir = pstate->directive;
    if (dir->type == LINE)
      delete_line(dir);
    else if (dir->type == LINEMARKER)
      delete_line(dir);
    else if (dir->type == KEYLIST)
      delete_keylist(dir);
  }
}

idl_retcode_t idl_parse_directive(idl_pstate_t *pstate, idl_token_t *tok)
{
  /* order is important here */
  if ((pstate->scanner.state & IDL_SCAN_LINE) == IDL_SCAN_LINE) {
    return parse_line(pstate, tok);
  } else if ((pstate->scanner.state & IDL_SCAN_KEYLIST) == IDL_SCAN_KEYLIST) {
    return parse_keylist(pstate, tok);
  } else if (pstate->scanner.state == IDL_SCAN_PRAGMA) {
    /* expect keylist */
    if (tok->code == IDL_TOKEN_IDENTIFIER) {
      if (strcmp(tok->value.str, "keylist") == 0) {
        struct keylist *dir;
        if (!(dir = calloc(1, sizeof(*dir))))
          return IDL_RETCODE_NO_MEMORY;
        dir->directive.type = KEYLIST;
        pstate->keylists = true; /* register keylist occurence */
        pstate->directive = dir;
        pstate->scanner.state = IDL_SCAN_KEYLIST;
        return IDL_RETCODE_OK;
      }
      idl_error(pstate, &tok->location,
        "Unsupported #pragma directive %s", tok->value.str);
      return IDL_RETCODE_SYNTAX_ERROR;
    }
  } else if (pstate->scanner.state == IDL_SCAN_DIRECTIVE_NAME) {
    if (tok->code == IDL_TOKEN_PP_NUMBER) {
      /* expect linemarker */
      struct line *dir;
      if (!(dir = calloc(1, sizeof(*dir))))
        return IDL_RETCODE_NO_MEMORY;
      dir->directive.type = LINEMARKER;
      pstate->directive = dir;
      pstate->scanner.state = IDL_SCAN_LINE;
      return parse_line(pstate, tok);
    } else if (tok->code == IDL_TOKEN_IDENTIFIER) {
      /* expect line or pragma */
      if (strcmp(tok->value.str, "line") == 0) {
        struct line *dir;
        if (!(dir = calloc(1, sizeof(*dir))))
          return IDL_RETCODE_NO_MEMORY;
        dir->directive.type = LINE;
        pstate->directive = dir;
        pstate->scanner.state = IDL_SCAN_LINE;
        return IDL_RETCODE_OK;
      } else if (strcmp(tok->value.str, "pragma") == 0) {
        /* support #pragma keylist for backwards compatibility */
        pstate->scanner.state = IDL_SCAN_PRAGMA;
        return 0;
      }
    } else if (tok->code == '\n' || tok->code == '\0') {
      pstate->scanner.state = IDL_SCAN;
      return 0;
    }
  } else if (pstate->scanner.state == IDL_SCAN_DIRECTIVE) {
    /* expect # */
    if (tok->code == '#') {
      pstate->scanner.state = IDL_SCAN_DIRECTIVE_NAME;
      return 0;
    }
  }

  idl_error(pstate, &tok->location, "Invalid compiler directive");
  return IDL_RETCODE_SYNTAX_ERROR;
}
