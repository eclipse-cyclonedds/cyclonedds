// Copyright(c) 2021 to 2022 ZettaScale Technology and others
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "idl/heap.h"
#include "idl/processor.h"
#include "idl/string.h"
#include "idl/misc.h"

#include "file.h"
#include "symbol.h"
#include "tree.h"
#include "scope.h"
#include "directive.h"
#include "parser.h"

struct directive {
  enum {
    LINE, /**< #line directive */
    LINEMARKER, /**< linemarker (extended line directive) */
    KEYLIST /**< #pragma keylist directive */
  } type;
};

#define START_OF_FILE (1u<<0)
#define RETURN_TO_FILE (1u<<1)
#define ADDITIONAL_DIRECTORY (1u<<2)

struct line {
  struct directive directive;
  unsigned long long line;
  char *file; /**< original filename in include directive */
  char *path; /**< normalized path of file */
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
    if (!(file = idl_calloc(1, sizeof(*file))))
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
push_source(idl_pstate_t *pstate, const char *inc, const char *abs, uint32_t flags)
{
  idl_file_t *path = pstate->paths;
  idl_source_t *src, *last;
  for (; path && strcmp(path->name, abs); path = path->next) ;
  if (!path) {
    if (!(path = idl_calloc(1, sizeof(*path))))
      return IDL_RETCODE_NO_MEMORY;
    path->next = pstate->paths;
    pstate->paths = path;
    if (!(path->name = idl_strdup(abs)))
      return IDL_RETCODE_NO_MEMORY;
  }
  if (push_file(pstate, inc))
    return IDL_RETCODE_NO_MEMORY;
  if (!(src = idl_calloc(1, sizeof(*src))))
    return IDL_RETCODE_NO_MEMORY;
  src->file = pstate->scanner.position.file;
  src->path = path;
  src->additional_directory = (flags & ADDITIONAL_DIRECTORY) != 0;
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

static void delete_line(void *ptr)
{
  struct line *dir = (struct line *)ptr;
  assert(dir);
  if (dir->path)
    idl_free(dir->path);
  if (dir->file)
    idl_free(dir->file);
  idl_free(dir);
}

static idl_retcode_t push_line(idl_pstate_t *pstate, struct line *dir)
{
  idl_retcode_t ret;

  if (dir->flags & START_OF_FILE) {
    char *norm = NULL;
    const idl_source_t *src = pstate->scanner.position.source;

    if (!idl_isabsolute(dir->path)) {
      const char *cwd = src->path->name;

      if (cwd && strcmp(cwd, "<builtin>") != 0) {
        char *abs = NULL;
        int len = 0, sep = 0;
        assert(idl_isabsolute(cwd));
        for (int pos=0; cwd[pos]; pos++) {
          if (!idl_isseparator(cwd[pos]))
            sep = 0;
          else if (!sep)
            len = sep = pos;
        }
        assert(!len || idl_isseparator(cwd[len]));
        if (idl_asprintf(&abs, "%.*s/%s", len, cwd, dir->path) < 0)
          return IDL_RETCODE_NO_MEMORY;
        idl_free(dir->path);
        dir->path = abs;
      }
    }

    if ((ret = idl_normalize_path(dir->path, &norm)) < 0) {
      idl_error(pstate, NULL, "Invalid line marker: path '%s' not found", dir->path);
      return ret;
    }
    idl_free(dir->path);
    dir->path = norm;
    assert(dir->file);

    if (idl_isabsolute(dir->file)) {
      /* reuse normalized filename if include is absolute */
      idl_free(dir->file);
      if (!(dir->file = idl_strdup(dir->path)))
        return IDL_RETCODE_NO_MEMORY;
    } else {
      /* use original filename by default */
      (void)idl_untaint_path(dir->file);
    }

    if ((ret = push_source(pstate, dir->file, dir->path, dir->flags)))
      return ret;
  } else if (dir->flags & RETURN_TO_FILE) {
    const idl_source_t *src = pstate->scanner.position.source;
    assert (src);
    if (!src->parent) {
      idl_error(pstate, NULL, "Invalid line marker: cannot return to file '%s'", src->path->name);
      return IDL_RETCODE_SYNTAX_ERROR;
    }
    src = src->parent;
    pstate->scanner.position.source = src;
    pstate->scanner.position.file = src->path;
  } else {
    if (dir->path && (ret = push_file(pstate, dir->path)))
      return ret;
  }

  pstate->scanner.position.line = (uint32_t)dir->line;
  pstate->scanner.position.column = 1;
  delete_line(dir);
  pstate->directive = NULL;

  return IDL_RETCODE_OK;
}

static int32_t
parse_line(idl_pstate_t *pstate, idl_token_t *tok)
{
  struct line *dir = (struct line *)pstate->directive;
  unsigned long long ullng;
  const char *type =
    dir->directive.type == LINE ? "#line directive" : "line marker";

  switch (pstate->scanner.state) {
    case IDL_SCAN_LINE:
      if (tok->code != IDL_TOKEN_PP_NUMBER) {
        idl_error(pstate, &tok->location, "No line number in %s", type);
        return IDL_RETCODE_SYNTAX_ERROR;
      }
      ullng = idl_strtoull(tok->value.str, NULL, 10);
      if (ullng == 0 || ullng > INT32_MAX) {
        idl_error(pstate, &tok->location, "Invalid line number in %s", type);
        return IDL_RETCODE_SYNTAX_ERROR;
      }
      dir->line = ullng;
      pstate->scanner.state = IDL_SCAN_PATH;
      break;
    case IDL_SCAN_PATH:
      if (tok->code == '\n' || tok->code == '\0') {
        return push_line(pstate, dir);
      } else if (tok->code != IDL_TOKEN_STRING_LITERAL) {
        idl_error(pstate, &tok->location, "Invalid filename in %s", type);
        return IDL_RETCODE_SYNTAX_ERROR;
      }
      dir->path = tok->value.str;
      tok->value.str = NULL; /* dont idl_free */
      pstate->scanner.state = IDL_SCAN_FLAGS;
      break;
    case IDL_SCAN_FLAGS:
      if (tok->code == '\n' || tok->code == '\0') {
        return push_line(pstate, dir);
      } else if (tok->code == IDL_TOKEN_PP_NUMBER) {
        // for proper handling of includes by parsing line controls, a
        // mechanism derived from GCCs linemarkers is required. they are
        // enabled in mcpp by defining the compiler to IDLC. See
        // https://gcc.gnu.org/onlinedocs/cpp/Preprocessor-Output.html for
        // details
        uint32_t flags = 0;
        if (strcmp(tok->value.str, "1") == 0)
          flags = START_OF_FILE;
        else if (strcmp(tok->value.str, "2") == 0)
          flags = RETURN_TO_FILE;
        else if (strcmp(tok->value.str, "3") == 0)
          flags = START_OF_FILE|ADDITIONAL_DIRECTORY;

        /* either extra token or flag based on type of directive */
        if (dir->directive.type == LINE || dir->flags)
          goto extra_tokens;
        dir->flags |= flags;
        /* expect original filename on non-local file */
        if (dir->flags & START_OF_FILE)
          pstate->scanner.state = IDL_SCAN_FILE;
      } else {
extra_tokens:
        idl_warning(pstate, IDL_WARN_EXTRA_TOKEN_DIRECTIVE, &tok->location, "Extra tokens at end of %s", type);
        pstate->scanner.state = IDL_SCAN_EXTRA_TOKENS;
      }
      break;
    case IDL_SCAN_FILE: /* scan original filename */
      if (tok->code == IDL_TOKEN_STRING_LITERAL) {
        dir->file = tok->value.str;
        tok->value.str = NULL; /* dont idl_free */
        pstate->scanner.state = IDL_SCAN_EXTRA_TOKENS;
      } else {
        const char *reason;
        if (tok->code == '\n' || tok->code == '\0')
          reason = "Missing";
        else
          reason = "Invalid";
        idl_error(pstate, &tok->location, "%s filename in %s", reason, type);
        return IDL_RETCODE_SEMANTIC_ERROR;
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
    idl_free(dir->keys);
  }
  idl_free(dir);
}

static idl_retcode_t
push_keylist(idl_pstate_t *pstate, struct keylist *dir)
{
  idl_retcode_t ret;
  idl_scope_t *scope;
  idl_type_spec_t *type_spec;
  idl_struct_t *_struct;
  idl_keylist_t *keylist = NULL;
  const idl_declaration_t *declaration;

  assert(dir);
  if (!(declaration = idl_find_scoped_name(pstate, NULL, dir->data_type, 0u))) {
    idl_error(pstate, idl_location(dir->data_type),
      "Unknown data-type '%s' in keylist directive", dir->data_type->identifier);
    return IDL_RETCODE_SEMANTIC_ERROR;
  }
  /* can be an alias, forward declaration or a combination thereof */
  type_spec = idl_strip(declaration->node, IDL_STRIP_ALIASES|IDL_STRIP_FORWARD);
  if (!type_spec) {
    idl_error(pstate, idl_location(dir->data_type),
      "Incomplete data-type '%s' in keylist directive", dir->data_type->identifier);
    return IDL_RETCODE_SEMANTIC_ERROR;
  }
  if (!idl_is_struct(type_spec)) {
    idl_error(pstate, idl_location(dir->data_type),
      "Invalid data-type '%s' in keylist directive", dir->data_type->identifier);
    return IDL_RETCODE_SEMANTIC_ERROR;
  }
  _struct = type_spec;
  if (_struct->keylist) {
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
  keylist->node.parent = (idl_node_t *)_struct;
  _struct->keylist = keylist;

  declaration = ((idl_node_t *)_struct)->declaration;
  assert(declaration);
  scope = declaration->scope;

  for (size_t i=0; dir->keys && dir->keys[i]; i++) {
    idl_key_t *key = NULL;
    idl_mask_t mask = IDL_BASE_TYPE | IDL_ENUM | IDL_STRING;
    const idl_declarator_t *declarator;
    const idl_type_spec_t *ts;

    if (!(declaration = idl_find_field_name(pstate, scope, dir->keys[i], 0u))) {
      idl_error(pstate, idl_location(dir->keys[i]),
        "Unknown key '%s' in keylist directive", dir->keys[i]->identifier);
      ret = IDL_RETCODE_SEMANTIC_ERROR;
      goto err_find_decl;
    }
    declarator = (const idl_declarator_t *)declaration->node;
    assert(idl_is_declarator(declarator));
    ts = idl_type_spec(declarator);
    /* until DDS-XTypes is fully implemented, base types, enums, arrays of the
       aforementioned and strings are allowed to be used in keys */
    ts = idl_strip(ts, IDL_STRIP_ALIASES);
    if (idl_is_array(ts))
      mask &= (idl_mask_t)~IDL_STRING;
    ts = idl_strip(ts, IDL_STRIP_ALIASES|IDL_STRIP_ALIASES_ARRAY);
    if (!(idl_mask(ts) & mask)) {
      idl_error(pstate, idl_location(dir->keys[i]),
        "Invalid key '%s' in keylist directive", dir->keys[i]->identifier);
      ret = IDL_RETCODE_SEMANTIC_ERROR;
      goto err_invalid_key;
    }

    if ((ret = idl_create_key(pstate, idl_location(dir->keys[i]), &key)))
      goto err_create_key;
    key->node.parent = (idl_node_t *)keylist;
    key->field_name = dir->keys[i];
    keylist->keys = idl_push_node(keylist->keys, key);
    dir->keys[i] = NULL; /* do not idl_free */
  }
  ret = IDL_RETCODE_OK;

err_find_decl:
err_invalid_key:
err_create_key:
  delete_keylist(dir);
  pstate->directive = NULL;
  return ret;
}

static int stash_name(idl_pstate_t *pstate, idl_location_t *loc, char *str)
{
  struct keylist *dir = (struct keylist *)pstate->directive;
  idl_name_t *name = NULL;

  if (idl_create_name(pstate, loc, str, false, &name))
    goto err_alloc;
  if (idl_push_scoped_name(pstate, dir->data_type, name))
    goto err_alloc;
  return 0;
err_alloc:
  if (name)
    idl_free(name);
  return -1;
}

static int stash_data_type(idl_pstate_t *pstate, idl_location_t *loc, char *str)
{
  struct keylist *dir = (struct keylist *)pstate->directive;
  idl_name_t *name = NULL;

  if (idl_create_name(pstate, loc, str, false, &name))
    goto err_alloc;
  if (idl_create_scoped_name(pstate, loc, name, false, &dir->data_type))
    goto err_alloc;
  return 0;
err_alloc:
  if (name)
    idl_free(name);
  return -1;
}

static int stash_field(idl_pstate_t *pstate, idl_location_t *loc, char *str)
{
  struct keylist *dir = (struct keylist *)pstate->directive;
  idl_name_t *name = NULL;
  size_t n;

  assert(dir->keys);
  if (idl_create_name(pstate, loc, str, false, &name))
    goto err_alloc;
  assert(dir->keys);
  for (n=0; dir->keys[n]; n++) ;
  assert(n);
  if (idl_push_field_name(pstate, dir->keys[n-1], name))
    goto err_alloc;
  return 0;
err_alloc:
  if (name)
    idl_free(name);
  return -1;
}

static int stash_key(idl_pstate_t *pstate, idl_location_t *loc, char *str)
{
  struct keylist *dir = (struct keylist *)pstate->directive;
  idl_name_t *name = NULL;
  idl_field_name_t **keys;
  size_t n;

  for (n=0; dir->keys && dir->keys[n]; n++) ;
  if (!(keys = idl_realloc(dir->keys, (n + 2) * sizeof(*keys))))
    goto err_alloc;
  dir->keys = keys;
  keys[n+0] = NULL;
  if (idl_create_name(pstate, loc, str, false, &name))
    goto err_alloc;
  if (idl_create_field_name(pstate, loc, name, &keys[n+0]))
    goto err_alloc;
  keys[n+1] = NULL;
  return 0;
err_alloc:
  if (name)
    idl_free(name);
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
  } else if (pstate->scanner.state == IDL_SCAN_UNKNOWN_PRAGMA) {
    if (tok->code == '\n')
      pstate->scanner.state = IDL_SCAN;
    return IDL_RETCODE_OK;
  } else if (pstate->scanner.state == IDL_SCAN_PRAGMA) {
    /* expect keylist */
    if (tok->code == IDL_TOKEN_IDENTIFIER) {
      if (strcmp(tok->value.str, "keylist") == 0) {
        struct keylist *dir;
        if (!(dir = idl_calloc(1, sizeof(*dir))))
          return IDL_RETCODE_NO_MEMORY;
        dir->directive.type = KEYLIST;
        pstate->keylists = true; /* register keylist occurence */
        pstate->directive = dir;
        pstate->scanner.state = IDL_SCAN_KEYLIST;
        return IDL_RETCODE_OK;
      }
    }
    pstate->scanner.state = IDL_SCAN_UNKNOWN_PRAGMA;
    return IDL_RETCODE_OK;
  } else if (pstate->scanner.state == IDL_SCAN_DIRECTIVE_NAME) {
    if (tok->code == IDL_TOKEN_PP_NUMBER) {
      /* expect linemarker */
      struct line *dir;
      if (!(dir = idl_calloc(1, sizeof(*dir))))
        return IDL_RETCODE_NO_MEMORY;
      dir->directive.type = LINEMARKER;
      pstate->directive = dir;
      pstate->scanner.state = IDL_SCAN_LINE;
      return parse_line(pstate, tok);
    } else if (tok->code == IDL_TOKEN_IDENTIFIER) {
      /* expect line or pragma */
      if (strcmp(tok->value.str, "line") == 0) {
        struct line *dir;
        if (!(dir = idl_calloc(1, sizeof(*dir))))
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
