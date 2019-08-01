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
#ifndef DDSTS_GEN_C99_H
#define DDSTS_GEN_C99_H

#include "dds/ddsrt/retcode.h"

dds_return_t ddsts_generate_C99(const char *file, ddsts_type_t *root_type);
dds_return_t ddsts_generate_C99_to_buffer(const char* file, ddsts_type_t *root_type, char *buffer, size_t buffer_len);

#endif /* DDSTS_GEN_C99_H */
