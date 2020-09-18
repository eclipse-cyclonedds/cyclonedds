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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "idl/retcode.h"
#include "idl/string.h"
#include "scope.h"

idl_retcode_t
idl_create_name(
  idl_processor_t *proc,
  idl_name_t **namep,
  idl_location_t *location,
  char *identifier)
{
  idl_name_t *name;

  (void)proc;
  if (!(name = malloc(sizeof(*name))))
    return IDL_RETCODE_NO_MEMORY;
  name->location = *location;
  name->identifier = identifier;
  *namep = name;
  return IDL_RETCODE_OK;
}

void
idl_delete_name(idl_name_t *name)
{
  if (name) {
    if (name->identifier)
      free(name->identifier);
    free(name);
  }
}

idl_retcode_t
idl_create_scoped_name(
  idl_processor_t *proc,
  idl_scoped_name_t **scoped_namep,
  idl_location_t *location,
  idl_name_t *name,
  bool absolute)
{
  idl_scoped_name_t *scoped_name;
  const char *fmt;

  (void)proc;
  if (!(scoped_name = malloc(sizeof(*scoped_name)))) {
    return IDL_RETCODE_NO_MEMORY;
  }
  fmt = absolute ? "::%s" : "%s";
  if (idl_asprintf(&scoped_name->flat, fmt, name->identifier) == -1) {
    free(scoped_name);
    return IDL_RETCODE_NO_MEMORY;
  }
  if (!(scoped_name->path.names = calloc(1, sizeof(idl_name_t*)))) {
    free(scoped_name->flat);
    free(scoped_name);
    return IDL_RETCODE_NO_MEMORY;
  }
  scoped_name->location.first = location->first;
  scoped_name->location.last = name->location.last;
  scoped_name->path.length = 1;
  scoped_name->path.names[0] = name;
  scoped_name->absolute = absolute;
  *scoped_namep = scoped_name;
  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_append_to_scoped_name(
  idl_processor_t *proc,
  idl_scoped_name_t *scoped_name,
  idl_name_t *name)
{
  size_t size;
  idl_name_t **names;
  static const char fmt[] = "%s::%s";
  char *flat = NULL;

  (void)proc;
  assert(scoped_name);
  assert(scoped_name->path.length >= 1);
  assert(name);

  if (idl_asprintf(&flat, fmt, scoped_name->flat, name->identifier) == -1) {
    return IDL_RETCODE_NO_MEMORY;
  }
  size = (scoped_name->path.length + 1) * sizeof(idl_name_t*);
  if (!(names = realloc(scoped_name->path.names, size))) {
    free(flat);
    return IDL_RETCODE_NO_MEMORY;
  }
  free(scoped_name->flat);
  scoped_name->flat = flat;
  scoped_name->location.last = name->location.last;
  scoped_name->path.names = names;
  scoped_name->path.names[scoped_name->path.length++] = name;
  return IDL_RETCODE_OK;
}

void
idl_delete_scoped_name(idl_scoped_name_t *scoped_name)
{
  if (scoped_name) {
    for (size_t i=0; i < scoped_name->path.length; i++)
      idl_delete_name(scoped_name->path.names[i]);
    free(scoped_name->path.names);
    if (scoped_name->flat)
      free(scoped_name->flat);
    free(scoped_name);
  }
}

idl_retcode_t
idl_create_inherit_spec(
  idl_processor_t *proc,
  idl_inherit_spec_t **inherit_specp,
  idl_scoped_name_t *scoped_name,
  const idl_scope_t *scope)
{
  idl_inherit_spec_t *inherit_spec;

  (void)proc;
  if (!(inherit_spec = malloc(sizeof(*inherit_spec))))
    return IDL_RETCODE_NO_MEMORY;
  inherit_spec->next = NULL;
  inherit_spec->scoped_name = scoped_name;
  inherit_spec->scope = scope;
  *inherit_specp = inherit_spec;
  return IDL_RETCODE_OK;
}

void idl_delete_inherit_spec(idl_inherit_spec_t *inherit_spec)
{
  if (inherit_spec) {
    assert(!inherit_spec->next);
    idl_delete_scoped_name(inherit_spec->scoped_name);
    free(inherit_spec);
  }
}

idl_retcode_t
idl_create_scope(
  idl_processor_t *proc,
  idl_scope_t **scopep,
  idl_scope_type_t type,
  idl_name_t *name)
{
  idl_scope_t *scope;
  idl_entry_t *entry;

  if (!(entry = malloc(sizeof(*entry))))
    goto err_entry;
  if (!(entry->name = malloc(sizeof(*entry->name))))
    goto err_name;
  if (!(entry->name->identifier = idl_strdup(name->identifier)))
    goto err_identifier;
  entry->next = NULL;
  entry->type = IDL_SCOPE;
  entry->name->location = name->location;
  entry->node = NULL;
  entry->scope = NULL;
  if (!(scope = malloc(sizeof(*scope))))
    goto err_scope;
  scope->type = type;
  if (proc)
    scope->parent = (const idl_scope_t *)proc->scope;
  scope->name = (const idl_name_t *)&entry->name;
  scope->table.first = scope->table.last = entry;
  scope->inherit_spec = NULL;

  *scopep = scope;
  return IDL_RETCODE_OK;
err_scope:
  free(entry->name->identifier);
err_identifier:
  free(entry->name);
err_name:
  free(entry);
err_entry:
  return IDL_RETCODE_NO_MEMORY;
}

/* free scopes, not nodes */
void
idl_delete_scope(
  idl_scope_t *scope)
{
  if (scope) {
    for (idl_entry_t *eq, *ep = scope->table.first; ep; ep = eq) {
      eq = ep->next;
      idl_delete_name(ep->name);
      if (ep->scope)
        idl_delete_scope(ep->scope);
      free(ep);
    }
    for (idl_inherit_spec_t *ihq, *ihp = scope->inherit_spec; ihp; ihp = ihq) {
      ihq = ihp->next;
      idl_delete_inherit_spec(ihp);
    }
    free(scope);
  }
}

idl_retcode_t
idl_inherit(
  idl_processor_t *proc,
  idl_scope_t *scope,
  idl_inherit_spec_t *inherit_spec)
{
  assert(!scope->inherit_spec);
  scope->inherit_spec = inherit_spec;

  (void)proc;
  /* FIXME: interface inheritance requires elements of a base interface can
            be referred to as if they were elements in the derived interface.
            struct inheritance does not have that requirement, hence no new
            identifiers are declared in the derived scope */

  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_declare(
  idl_processor_t *proc,
  idl_entry_t **entryp,
  idl_entry_type_t type,
  const idl_name_t *name,
  const void *node,
  idl_scope_t *scope)
{
  idl_entry_t *entry;

  assert(proc && proc->scope);

  /* ensure there is no collision with an earlier declaration */
  for (idl_entry_t *e = proc->scope->table.first; e; e = e->next) {
    /* identifiers that differ only in case collide, and will yield a
       compilation error under certain circumstances */
    if (idl_strcasecmp(name->identifier, e->name->identifier) == 0) {
      entry = e;
      switch (e->type) {
        case IDL_REFERENCE:
          if (type == IDL_REFERENCE || type == IDL_INSTANCE)
            goto ok;
          break;
        case IDL_DECLARATION:
          if (type == IDL_REFERENCE)
            goto ok;
          break;
        default:
          break;
      }
      idl_error(proc, idl_location(node),
        "Declaration '%s' collides with declaration '%s'",
        name->identifier, e->name->identifier);
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
  }

  if (!(entry = malloc(sizeof(*entry))))
    goto err_entry;
  if (!(entry->name = malloc(sizeof(*name))))
    goto err_name;
  if (!(entry->name->identifier = idl_strdup(name->identifier)))
    goto err_identifier;
  entry->type = type;
  entry->next = NULL;
  entry->name->location = name->location;
  entry->node = node;
  entry->scope = scope;

  if (proc->scope->table.first) {
    assert(proc->scope->table.last);
    proc->scope->table.last->next = entry;
    proc->scope->table.last = entry;
  } else {
    assert(!proc->scope->table.last);
    proc->scope->table.last = proc->scope->table.first = entry;
  }

ok:
  if (entryp)
    *entryp = entry;
  return IDL_RETCODE_OK;
err_identifier:
  free(entry->name);
err_name:
  free(entry);
err_entry:
  return IDL_RETCODE_NO_MEMORY;
}

idl_entry_t *
idl_find(
  const idl_processor_t *proc,
  const idl_scope_t *scope,
  const idl_name_t *name)
{
  idl_entry_t *e;

  if (!scope)
    scope = proc->scope;

  for (e = scope->table.first; e; e = e->next) {
    if (idl_strcasecmp(e->name->identifier, name->identifier) == 0)
      return e;
  }

  return NULL;
}

idl_retcode_t
idl_resolve(
  idl_processor_t *proc,
  idl_entry_t **entryp,
  const idl_scoped_name_t *scoped_name)
{
  idl_retcode_t ret = IDL_RETCODE_NO_MEMORY;
  idl_entry_t *entry = NULL;
  idl_scope_t *scope;
  idl_node_t *node = NULL;

  scope = (scoped_name->absolute) ? proc->global_scope : proc->scope;
  assert(scope);

  for (size_t i=0; i < scoped_name->path.length && scope;) {
    const char *identifier = scoped_name->path.names[i]->identifier;
    entry = idl_find(proc, scope, scoped_name->path.names[i]);
    if (entry && entry->type != IDL_REFERENCE) {
      /* identifiers are case insensitive. however, all references to a
         definition must use the same case as the defining occurence */
      if (strcmp(identifier, entry->name->identifier) != 0) {
        idl_error(proc, &scoped_name->path.names[i]->location,
          "Scoped name matched up to '%s', but identifier differs in case from '%s'",
          identifier, entry->name->identifier);
        return IDL_RETCODE_SEMANTIC_ERROR;
      }
      if (i == 0)
        node = (idl_node_t*)entry->node;
      scope = entry->scope;
      i++;
    } else if (scoped_name->absolute || i != 0) {
      idl_error(proc, &scoped_name->location,
        "Scoped name '%s' cannot be resolved", scoped_name->flat);
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

  if (!scoped_name->absolute && scope != proc->scope) {
    /* non-absolute qualified names introduce the identifier of the outermost
       scope of the scoped name into the current scope */
    const idl_name_t *name = scoped_name->path.names[0];
    if ((ret = idl_declare(proc, NULL, IDL_REFERENCE, name, node, NULL)))
      return ret;
  }

  *entryp = entry;
  return IDL_RETCODE_OK;
}

void
idl_enter_scope(
  idl_processor_t *proc,
  idl_scope_t *scope)
{
  proc->scope = scope;
}

void
idl_exit_scope(
  idl_processor_t *proc)
{
  assert(proc->scope);
  assert(proc->scope != proc->global_scope);
  proc->scope = (idl_scope_t*)proc->scope->parent;
}
