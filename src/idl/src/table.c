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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "table.h"
#include "idl/string.h"

const idl_symbol_t *
idl_add_symbol(
  idl_processor_t *proc,
  const char *scope,
  const char *name,
  const void *node)
{
  idl_symbol_t *sym;

  assert(proc);
  assert(name);
  assert(node);

  if (!(sym = calloc(1, sizeof(*sym))))
    return NULL;

  if (name[0] == ':' && name[1] == ':') {
    if (!(sym->name = idl_strdup(name))) {
      free(sym);
      return NULL;
    }
  } else {
    assert(scope);
    assert(scope[0] == ':' && scope[1] == ':');
    if (idl_asprintf(&sym->name, "%s::%s", strcmp(scope, "::") == 0 ? "" : scope, name) == -1) {
      free(sym);
      return NULL;
    }
  }

  sym->node = node;
  if (proc->table.first) {
    assert(proc->table.last);
    proc->table.last->next = sym;
    proc->table.last = sym;
  } else {
    assert(!proc->table.last);
    proc->table.first = proc->table.last = sym;
  }

  return sym;
}

const idl_symbol_t *
idl_find_symbol(
  const idl_processor_t *proc,
  const char *scope,
  const char *name,
  const idl_symbol_t *whence)
{
  idl_symbol_t *sym;
  size_t len;

  assert(proc);
  assert(name);

  assert(!whence || whence->next || whence == proc->table.last);
  if (whence == proc->table.last)
    return NULL;

  /* section 7.5 of the interface definition language 4.2 specification
     contains details on names and scoping. the lookup algorithm here may not
     be entirely accurate yet. */

  /* #pragma keylist directives may require an argument to indicate an exact
     match, i.e. no scope resolution, is demanded. alternatively, a fully
     scoped name can be used in that scenario */

  /* trim trailing :: */
  len = strlen(name);
  for (; len > 0 && name[len - 1] == ':'; len--) ;
  if (name[0] == ':' && name[1] == ':') {
    /* name is fully scoped */
    sym = whence ? whence->next : proc->table.first;
    for (; sym && strncmp(name, sym->name, len) != 0; sym = sym->next) ;
  } else {
    size_t off;
    if (!scope)
      scope = "::";
    assert(scope[0] == ':' && scope[1] == ':');
    /* trim tailing :: */
    for (off = strlen(scope); off > 0 && scope[off - 1] == ':'; off--) ;
    for (;;) {
      sym = whence ? whence->next : proc->table.first;
      for (; sym; sym = sym->next) {
        if (off != 0 && strncmp(scope, sym->name, off) != 0)
          continue;
        if (!(sym->name[off+0] == ':' && sym->name[off+1] == ':'))
          continue;
        if (strcmp(name, sym->name + off + 2) == 0)
          return sym;
      }
      if (off == 0)
        break;
      /* trim scope, i.e. drop bar if scope is ::foo::bar */
      for (; off > 0 && scope[off - 1] != ':'; off--) ;
      /* trim trailing :: */
      for (; off > 0 && scope[off - 1] == ':'; off--) ;
    }
  }

  return sym;
}
