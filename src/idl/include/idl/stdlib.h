/*
 * Copyright(c) 2022 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef IDL_STDLIB_H
#define IDL_STDLIB_H

#include <stdarg.h>
#include <stddef.h>

#include "idl/export.h"
#include "idl/attributes.h"

IDL_EXPORT void* idl_malloc  (size_t size);
IDL_EXPORT void* idl_calloc  (size_t num, size_t size);
IDL_EXPORT void* idl_realloc (void *ptr, size_t new_size);
IDL_EXPORT void  idl_free    (void *pt);

#endif /* IDL_STDLIB_H */
