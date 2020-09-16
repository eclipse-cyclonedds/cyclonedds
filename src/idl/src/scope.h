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
#ifndef SCOPE_H
#define SCOPE_H

#include "idl/processor.h"

idl_retcode_t
idl_create_name(
  idl_processor_t *proc,
  idl_name_t **namep,
  idl_location_t *location,
  char *identifier);

void idl_delete_name(idl_name_t *name);

idl_retcode_t
idl_create_scoped_name(
  idl_processor_t *state,
  idl_scoped_name_t **scoped_namep,
  idl_location_t *location,
  idl_name_t *name,
  bool absolute);

idl_retcode_t
idl_append_to_scoped_name(
  idl_processor_t *state,
  idl_scoped_name_t *scoped_name,
  idl_name_t *name);

void idl_delete_scoped_name(idl_scoped_name_t *scoped_name);

idl_retcode_t
idl_create_inherit_spec(
  idl_processor_t *proc,
  idl_inherit_spec_t **inherit_specp,
  idl_scoped_name_t *scoped_name,
  const idl_scope_t *scope);

void idl_delete_inherit_spec(idl_inherit_spec_t *inherit_spec);

idl_retcode_t
idl_create_scope(
  idl_processor_t *state,
  idl_scope_t **scopep,
  idl_scope_type_t type,
  idl_name_t *name);

void
idl_delete_scope(
  idl_scope_t *scope);

void idl_enter_scope(idl_processor_t *proc, idl_scope_t *scope);
void idl_exit_scope(idl_processor_t *proc);

idl_retcode_t
idl_inherit(
  idl_processor_t *proc,
  idl_scope_t *scope,
  idl_inherit_spec_t *inherit_spec);

idl_retcode_t
idl_declare(
  idl_processor_t *proc,
  idl_entry_t **entryp,
  idl_entry_type_t type,
  const idl_name_t *name,
  const void *node,
  idl_scope_t *scope);

idl_retcode_t
idl_resolve(
  idl_processor_t *proc,
  idl_entry_t **entryp,
  const idl_scoped_name_t *scoped_name);

idl_entry_t *
idl_find(
  const idl_processor_t *proc,
  const idl_scope_t *scope,
  const idl_name_t *name);

#endif /* SCOPE_H */
