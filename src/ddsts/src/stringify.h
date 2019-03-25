/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef IDL_STRINGIFY_H
#define IDL_STRINGIFY_H

#include <stddef.h>

#include "typetree.h"

void dds_ts_stringify(dds_ts_node_t *context, char *buffer, size_t len);

#endif /* IDL_STRINGIFY_H */
