// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DIRECTIVE_H
#define DIRECTIVE_H

#include <stdbool.h>

#include "idl/processor.h"
#include "tree.h"
#include "scanner.h"

IDL_EXPORT void
idl_delete_directive(idl_pstate_t *pstate);

IDL_EXPORT idl_retcode_t
idl_parse_directive(idl_pstate_t *pstate, idl_token_t *tok);

#endif /* DIRECTIVE_H */
