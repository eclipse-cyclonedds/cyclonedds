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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "idl/retcode.h"
#include "idl/string.h"
#include "symbol.h"
#include "scope.h"

static idl_retcode_t
create_declaration(
  idl_pstate_t *pstate,
  enum idl_declaration_kind kind,
  const idl_name_t *name,
  idl_declaration_t **declarationp)
{
  char *identifier;
  idl_declaration_t *declaration;

  if (!(declaration = calloc(1, sizeof(*declaration))))
    goto err_declaration;
  declaration->kind = kind;
  if (!(identifier = idl_strdup(name->identifier)))
    goto err_identifier;
  if (idl_create_name(pstate, idl_location(name), identifier, &declaration->name))
    goto err_name;
  *declarationp = declaration;
  return IDL_RETCODE_OK;
err_name:
  free(identifier);
err_identifier:
  free(declaration);
err_declaration:
  return IDL_RETCODE_NO_MEMORY;
}

static void delete_declaration(idl_declaration_t *declaration)
{
  if (declaration) {
    if (declaration->name)
      idl_delete_name(declaration->name);
    if (declaration->scoped_name) {
      if (declaration->scoped_name->identifier)
        free(declaration->scoped_name->identifier);
      if (declaration->scoped_name->names)
        free(declaration->scoped_name->names);
      free(declaration->scoped_name);
    }
    free(declaration);
  }
}

idl_retcode_t
idl_create_scope(
  idl_pstate_t *pstate,
  enum idl_scope_kind kind,
  const idl_name_t *name,
  const void *node,
  idl_scope_t **scopep)
{
  idl_scope_t *scope;
  idl_declaration_t *entry;

  assert(name);
  assert(node || kind == IDL_GLOBAL_SCOPE);

  if (create_declaration(pstate, IDL_SCOPE_DECLARATION, name, &entry))
    goto err_declaration;
  entry->node = node;
  if (!(scope = malloc(sizeof(*scope))))
    goto err_scope;
  scope->parent = pstate->scope;
  scope->kind = kind;
  scope->name = (const idl_name_t *)entry->name;
  scope->declarations.first = scope->declarations.last = entry;
  scope->imports.first = scope->imports.last = NULL;
  *scopep = scope;
  return IDL_RETCODE_OK;
err_scope:
  delete_declaration(entry);
err_declaration:
  return IDL_RETCODE_NO_MEMORY;
}

/* free scopes, not nodes */
void idl_delete_scope(idl_scope_t *scope)
{
  if (scope) {
    for (idl_declaration_t *q, *p = scope->declarations.first; p; p = q) {
      q = p->next;
      if (p->scope && p->kind != IDL_INSTANCE_DECLARATION)
        idl_delete_scope(p->scope);
      delete_declaration(p);
    }
    for (idl_import_t *q, *p = scope->imports.first; p; p = q) {
      q = p->next;
      free(p);
    }
    free(scope);
  }
}

idl_retcode_t
idl_import(
  idl_pstate_t *pstate,
  idl_scope_t *scope,
  const idl_scope_t *imported_scope)
{
  idl_import_t *entry;

  (void)pstate;
  /* ensure scopes are not imported twice */
  for (entry=scope->imports.first; entry; entry=entry->next) {
    if (entry->scope == imported_scope)
      return IDL_RETCODE_OK;
  }

  if (!(entry = malloc(sizeof(*entry))))
    return IDL_RETCODE_NO_MEMORY;
  entry->next = NULL;
  entry->scope = imported_scope;
  if (scope->imports.first) {
    assert(scope->imports.last);
    scope->imports.last->next = entry;
    scope->imports.last = entry;
  } else {
    scope->imports.first = scope->imports.last = entry;
  }

  return IDL_RETCODE_OK;
}

static bool
is_consistent(
  const idl_pstate_t *pstate, const void *lhs, const void *rhs)
{
  if (pstate->parser.state != IDL_PARSE_EXISTING_ANNOTATION_BODY)
    return false;
  assert(pstate->scope->kind == IDL_ANNOTATION_SCOPE);
  return idl_mask(lhs) == idl_mask(rhs);
}

idl_retcode_t
idl_declare(
  idl_pstate_t *pstate,
  enum idl_declaration_kind kind,
  const idl_name_t *name,
  void *node,
  idl_scope_t *scope,
  idl_declaration_t **declarationp)
{
  idl_declaration_t *entry = NULL;
  int (*cmp)(const char *, const char *);

  assert(pstate && pstate->scope);
  cmp = (pstate->flags & IDL_FLAG_CASE_SENSITIVE) ? &strcmp : &idl_strcasecmp;

  /* ensure there is no collision with an earlier declaration */
  for (entry = pstate->scope->declarations.first; entry; entry = entry->next) {
    /* identifiers that differ only in case collide, and will yield a
       compilation error under certain circumstances */
    if (cmp(name->identifier, entry->name->identifier) == 0) {
      switch (entry->kind) {
        case IDL_SCOPE_DECLARATION:
          /* declaration of the enclosing scope, but if the enclosing scope
             is an annotation, an '@' (commercial at) was prepended in its
             declaration */
          if (pstate->scope->kind != IDL_ANNOTATION_SCOPE)
            goto clash;
          break;
        case IDL_ANNOTATION_DECLARATION:
          /* same here, declaration was actually prepended with '@' */
          if (kind == IDL_ANNOTATION_DECLARATION)
            goto clash;
          break;
        case IDL_MODULE_DECLARATION:
          /* modules can be reopened. a module is considered to be defined by
             its first occurrence in a scope */
          if (kind == IDL_MODULE_DECLARATION)
            goto exists;
          goto clash;
        case IDL_USE_DECLARATION:
          if (kind == IDL_INSTANCE_DECLARATION)
            goto exists;
          /* fall through */
        case IDL_SPECIFIER_DECLARATION:
          if (kind == IDL_USE_DECLARATION)
            goto exists;
          /* short-circuit on parsing existing annotations */
          if (is_consistent(pstate, node, entry->node))
            goto exists;
          /* fall through */
        default:
clash:
          idl_error(pstate, idl_location(node),
            "Declaration '%s%s' collides with earlier an declaration of '%s%s'",
            kind == IDL_ANNOTATION ? "@" : "", name->identifier,
            kind == IDL_ANNOTATION ? "@" : "", entry->name->identifier);
          return IDL_RETCODE_SEMANTIC_ERROR;
      }
    }
  }

  if (pstate->parser.state == IDL_PARSE_EXISTING_ANNOTATION_BODY) {
    assert(pstate->scope->kind == IDL_ANNOTATION_SCOPE);
    idl_error(pstate, idl_location(node),
      "Declaration of unknown identifier '%s' in redefinition of '@%s'",
      name->identifier, pstate->scope->name->identifier);
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  if (create_declaration(pstate, kind, name, &entry))
    return IDL_RETCODE_NO_MEMORY;
  entry->local_scope = pstate->scope;
  entry->node = node;
  entry->scope = scope;

  if (pstate->scope->declarations.first) {
    assert(pstate->scope->declarations.last);
    pstate->scope->declarations.last->next = entry;
    pstate->scope->declarations.last = entry;
  } else {
    assert(!pstate->scope->declarations.last);
    pstate->scope->declarations.last = entry;
    pstate->scope->declarations.first = entry;
  }

  switch (kind) {
    case IDL_MODULE_DECLARATION:
    case IDL_ANNOTATION_DECLARATION:
    case IDL_SPECIFIER_DECLARATION: {
      size_t cnt = 0, len, off = 0;
      const char *sep = "::";
      idl_scoped_name_t *scoped_name = NULL;

      cnt++;
      len = strlen(sep) + strlen(name->identifier);
      for (const idl_scope_t *s = pstate->scope; s != pstate->global_scope; s = s->parent) {
        cnt++;
        len += strlen(sep) + strlen(s->name->identifier);
      }

      if (!(scoped_name = calloc(1, sizeof(*scoped_name))) ||
          !(scoped_name->names = calloc(cnt, sizeof(*scoped_name->names))) ||
          !(scoped_name->identifier = malloc(len + 1)))
      {
        if (scoped_name && scoped_name->names)
          free(scoped_name->names);
        if (scoped_name)
          free(scoped_name);
        return IDL_RETCODE_NO_MEMORY;
      }

      /* construct name vector */
      scoped_name->symbol = name->symbol;
      scoped_name->absolute = true;
      scoped_name->length = cnt;
      scoped_name->names[--cnt] = (idl_name_t *)name;
      for (const idl_scope_t *s = pstate->scope; s != pstate->global_scope; s = s->parent) {
        assert(cnt);
        scoped_name->names[--cnt] = (idl_name_t *)s->name;
      }
      assert(!cnt);
      /* construct fully qualified scoped name */
      for (size_t i=0; i < scoped_name->length; i++) {
        cnt = strlen(sep);
        assert(len - cnt >= off);
        memcpy(scoped_name->identifier+off, sep, cnt);
        off += cnt;
        cnt = strlen(scoped_name->names[i]->identifier);
        assert(len - cnt >= off);
        memcpy(scoped_name->identifier+off, scoped_name->names[i]->identifier, cnt);
        off += cnt;
      }
      assert(off == len);
      scoped_name->identifier[off] = '\0';
      entry->scoped_name = scoped_name;
    } /* fall through */
    case IDL_INSTANCE_DECLARATION:
      ((idl_node_t *)node)->declaration = entry;
      /* fall through */
    default:
      break;
  }

exists:
  if (declarationp)
    *declarationp = entry;
  return IDL_RETCODE_OK;
}

static int namecmp(const idl_name_t *n1, const idl_name_t *n2)
{
  return strcmp(n1->identifier, n2->identifier);
}

static int namecasecmp(const idl_name_t *n1, const idl_name_t *n2)
{
  return idl_strcasecmp(n1->identifier, n2->identifier);
}

const idl_declaration_t *
idl_find(
  const idl_pstate_t *pstate,
  const idl_scope_t *scope,
  const idl_name_t *name,
  uint32_t flags)
{
  const idl_declaration_t *entry;
  int (*cmp)(const idl_name_t *, const idl_name_t *);

  if (!scope)
    scope = pstate->scope;
  assert(name);
  assert(name->identifier);
  /* identifiers are case insensitive. however, all references to a definition
     must use the same case as the defining occurence to allow natural
     mappings to case-sensitive languages */
  cmp = (flags & IDL_FIND_IGNORE_CASE) ? &namecasecmp : &namecmp;

  for (entry = scope->declarations.first; entry; entry = entry->next) {
    if (entry->kind == IDL_ANNOTATION_DECLARATION && !(flags & IDL_FIND_ANNOTATION))
      continue;
    if (cmp(name, entry->name) == 0)
      return entry;
  }

  if (!(flags & IDL_FIND_IGNORE_IMPORTS)) {
    idl_import_t *import;
    for (import = scope->imports.first; import; import = import->next) {
      if ((entry = idl_find(pstate, import->scope, name, flags)))
        return entry;
    }
  }

  return NULL;
}

const idl_declaration_t *
idl_find_scoped_name(
  const idl_pstate_t *pstate,
  const idl_scope_t *scope,
  const idl_scoped_name_t *scoped_name,
  uint32_t flags)
{
  const idl_declaration_t *entry = NULL;
  int (*cmp)(const idl_name_t *, const idl_name_t *);

  if (scoped_name->absolute)
    scope = pstate->global_scope;
  else if (!scope)
    scope = pstate->scope;
  cmp = (flags & IDL_FIND_IGNORE_CASE) ? &namecasecmp : &namecmp;

  for (size_t i=0; i < scoped_name->length && scope;) {
    const idl_name_t *name = scoped_name->names[i];
    entry = idl_find(pstate, scope, name, (flags|IDL_FIND_IGNORE_CASE));
    if (entry && entry->kind != IDL_USE_DECLARATION) {
      if (cmp(name, entry->name) != 0)
        return NULL;
      scope = entry->kind == IDL_SCOPE_DECLARATION ? scope : entry->scope;
      i++;
    } else if (scoped_name->absolute || i != 0) {
      return NULL;
    } else {
      /* a name can be used in an unqualified form within a particular scope;
         it will be resolved by successively searching farther out in
         enclosing scopes, while taking into consideration inheritance
         relationships amount interfaces */
      /* assume inheritance applies to extended structs in the same way */
      scope = (idl_scope_t*)scope->parent;
    }
  }

  return entry;
}

const idl_declaration_t *
idl_find_field_name(
  const idl_pstate_t *pstate,
  const idl_scope_t *scope,
  const idl_field_name_t *field_name,
  uint32_t flags)
{
  const idl_declaration_t *entry;
  const void *root;

  assert(pstate);
  assert(scope);
  assert(field_name);

  /* take declaration that introduced scope */
  entry = scope->declarations.first;
  root = entry->node;
  if (!idl_is_struct(root) && !idl_is_union(root))
    return NULL;
  if (!field_name->length)
    return NULL;

  for (size_t i=0; i < field_name->length; i++) {
    if (!scope)
      return NULL;
    if (!(entry = idl_find(pstate, scope, field_name->names[i], flags)))
      return NULL;
    if (entry->kind != IDL_INSTANCE_DECLARATION)
      return NULL;
    scope = entry->scope;
  }

  return entry;
}

idl_retcode_t
idl_resolve(
  idl_pstate_t *pstate,
  enum idl_declaration_kind kind,
  const idl_scoped_name_t *scoped_name,
  const idl_declaration_t **declarationp)
{
  idl_retcode_t ret = IDL_RETCODE_NO_MEMORY;
  const idl_declaration_t *entry = NULL;
  idl_scope_t *scope;
  idl_node_t *node = NULL;
  uint32_t flags = 0u, ignore_case = IDL_FIND_IGNORE_CASE;

  if (kind == IDL_ANNOTATION_DECLARATION)
    flags |= IDL_FIND_ANNOTATION;
  if (pstate->flags & IDL_FLAG_CASE_SENSITIVE)
    ignore_case = 0u;

  scope = (scoped_name->absolute) ? pstate->global_scope : pstate->scope;
  assert(scope);

  for (size_t i=0; i < scoped_name->length && scope;) {
    const idl_name_t *name = scoped_name->names[i];
    entry = idl_find(pstate, scope, name, flags|ignore_case);
    if (entry && entry->kind != IDL_USE_DECLARATION) {
      /* identifiers are case insensitive. however, all references to a
         definition must use the same case as the defining occurence */
      if (ignore_case && namecmp(name, entry->name) != 0) {
        idl_error(pstate, idl_location(name),
          "Scoped name matched up to '%s', but identifier differs in case from '%s'",
          name->identifier, entry->name->identifier);
        return IDL_RETCODE_SEMANTIC_ERROR;
      }
      if (i == 0)
        node = (idl_node_t*)entry->node;
      scope = entry->kind == IDL_SCOPE_DECLARATION ? scope : entry->scope;
      i++;
    } else if (scoped_name->absolute || i != 0) {
      // take parser state into account here!!!!
      idl_error(pstate, idl_location(scoped_name),
        "Scoped name '%s' cannot be resolved", scoped_name->identifier);
      return IDL_RETCODE_SEMANTIC_ERROR;
    } else {
      /* a name can be used in an unqualified form within a particular scope;
         it will be resolved by successively searching farther out in
         enclosing scopes, while taking into consideration inheritance
         relationships amount interfaces */
      /* assume inheritance applies to extended structs in the same way */
      scope = (idl_scope_t*)scope->parent;
    }
  }

  if (!entry) {
    if (kind != IDL_ANNOTATION_DECLARATION)
      idl_error(pstate, idl_location(scoped_name),
        "Scoped name '%s' cannot be resolved", scoped_name->identifier);
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  if (!scoped_name->absolute && scope && scope != pstate->scope) {
    /* non-absolute qualified names introduce the identifier of the outermost
       scope of the scoped name into the current scope */
    if (kind != IDL_ANNOTATION_DECLARATION) {
      /* annotation_appl elements do not introduce a use declaration */
      const idl_name_t *name = scoped_name->names[0];
      if ((ret = idl_declare(pstate, IDL_USE_DECLARATION, name, node, NULL, NULL)))
        return ret;
    }
  }

  *declarationp = entry;
  return IDL_RETCODE_OK;
}

void
idl_enter_scope(
  idl_pstate_t *pstate,
  idl_scope_t *scope)
{
  pstate->scope = scope;
}

void
idl_exit_scope(
  idl_pstate_t *pstate)
{
  assert(pstate->scope);
  assert(pstate->scope != pstate->global_scope);
  pstate->scope = (idl_scope_t*)pstate->scope->parent;
}

idl_scope_t *idl_scope(const void *node)
{
  const idl_declaration_t *declaration = idl_declaration(node);
  if (declaration)
    return (idl_scope_t *)declaration->local_scope;

  return NULL;
}

idl_declaration_t *idl_declaration(const void *node)
{
  if (idl_is_declaration(node)) {
    assert(((const idl_node_t *)node)->declaration);
    return (idl_declaration_t *)((const idl_node_t *)node)->declaration;
  } else {
    assert(!((const idl_node_t *)node)->declaration);
    return NULL;
  }
}
