// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef SYMBOL_H
#define SYMBOL_H

#include "idl/processor.h"

idl_retcode_t
idl_create_name(
  idl_pstate_t *state,
  const idl_location_t *location,
  char *identifier,
  bool is_annotation,
  idl_name_t **namep);

void idl_delete_name(idl_name_t *name);

idl_retcode_t
idl_create_scoped_name(
  idl_pstate_t *state,
  const idl_location_t *location,
  idl_name_t *name,
  bool absolute,
  idl_scoped_name_t **scoped_namep);

idl_retcode_t
idl_push_scoped_name(
  idl_pstate_t *pstate,
  idl_scoped_name_t *scoped_name,
  idl_name_t *name);

void idl_delete_scoped_name(idl_scoped_name_t *scoped_name);

idl_retcode_t
idl_create_field_name(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  idl_field_name_t **field_namep);

idl_retcode_t
idl_push_field_name(
  idl_pstate_t *pstate,
  idl_field_name_t *field_name,
  idl_name_t *name);

void idl_delete_field_name(idl_field_name_t *field_name);

#endif /* SYMBOL_H */
