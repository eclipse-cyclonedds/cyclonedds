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
#ifndef SCANNER_H
#define SCANNER_H

#include "idl/export.h"

IDL_EXPORT idl_retcode_t idl_scan(idl_processor_t *proc, idl_token_t *tok);

#endif /* IDL_SCANNER_H */
