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

const char *
idl_scope(idl_processor_t *proc);

const char *
idl_enter_scope(idl_processor_t *proc, const char *ident);

void
idl_exit_scope(idl_processor_t *proc, const char *ident);

#endif /* SCOPE_H */
