/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDS_TS_GEN_C99_H
#define DDS_TS_GEN_C99_H

#include "typetree.h"

void dds_ts_generate_C99(const char* file, dds_ts_node_t *root_node);

void dds_ts_generate_C99_to_buffer(const char* file_name, dds_ts_node_t *root_node, char *buffer, size_t len);

#endif /* DDS_TS_GEN_C99_H */
