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
#ifndef TABLE_H
#define TABLE_H

#include "idl/processor.h"

const idl_symbol_t *
idl_add_symbol(
  idl_processor_t *proc,
  const char *scope,
  const char *name,
  const void *node);

const idl_symbol_t *
idl_find_symbol(
  const idl_processor_t *proc,
  const char *scope,
  const char *name,
  const idl_symbol_t *whence);

#endif /* TABLE_H */
