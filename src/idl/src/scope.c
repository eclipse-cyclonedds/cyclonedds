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

#include "idl/string.h"
#include "scope.h"

const char *idl_scope(idl_processor_t *proc)
{
  assert(proc);
  return proc->scope ? (const char *)proc->scope : "::";
}

const char *idl_enter_scope(idl_processor_t *proc, const char *ident)
{
  char *scope;

  assert(proc);
  assert(ident);

  if (idl_asprintf(&scope, "%s::%s", proc->scope ? proc->scope : "", ident) == -1)
    return NULL;

  free(proc->scope);
  return proc->scope = scope;
}

void idl_exit_scope(idl_processor_t *proc, const char *ident)
{
  size_t ident_len, scope_len;

  assert(proc);
  assert(proc->scope);
  assert(ident);

  ident_len = strlen(ident);
  scope_len = strlen(proc->scope);
  if (ident_len + 2 == scope_len) {
    free(proc->scope);
    proc->scope = NULL;
  } else {
    assert((ident_len + 2) < scope_len);
    assert(strcmp(proc->scope + (scope_len - ident_len), ident) == 0);
    proc->scope[(scope_len - (ident_len + 2))] = '\0';
  }
}
