// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef SCOPE_H
#define SCOPE_H

#include <stdbool.h>

#include "idl/processor.h"

idl_retcode_t
idl_create_scope(
  idl_pstate_t *pstate,
  enum idl_scope_kind kind,
  const idl_name_t *name,
  void *node,
  idl_scope_t **scopep);

void idl_delete_scope(idl_scope_t *scope);

void idl_enter_scope(idl_pstate_t *pstate, idl_scope_t *scope);
void idl_exit_scope(idl_pstate_t *pstate);

idl_retcode_t
idl_import(
  idl_pstate_t *pstate,
  idl_scope_t *scope,
  const idl_scope_t *imported_scope);

idl_retcode_t
idl_declare(
  idl_pstate_t *pstate,
  enum idl_declaration_kind kind,
  const idl_name_t *name,
  void *node,
  idl_scope_t *scope,
  idl_declaration_t **declarationp);

idl_retcode_t
idl_resolve(
  idl_pstate_t *pstate,
  enum idl_declaration_kind kind,
  const idl_scoped_name_t *scoped_name,
  const idl_declaration_t **declarationp);

#endif /* SCOPE_H */
