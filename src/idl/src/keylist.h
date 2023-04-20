// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef KEYLIST_H
#define KEYLIST_H

#include <stdbool.h>
#include "idl/processor.h"

idl_retcode_t
idl_validate_keylists(
  idl_pstate_t *pstate);

void
idl_set_keylist_key_flags(
  idl_pstate_t *pstate,
  void *list);

#endif /* KEYLIST_H */
