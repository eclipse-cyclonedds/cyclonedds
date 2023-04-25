// Copyright(c) 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef IDL_HEAP_H
#define IDL_HEAP_H

#include <stdarg.h>
#include <stddef.h>

#include "idl/export.h"
#include "idl/attributes.h"

IDL_EXPORT void  idl_free    (void *pt);
IDL_EXPORT void* idl_malloc  (size_t size) idl_attribute_malloc idl_attribute_malloc2 ((idl_free, 1));
IDL_EXPORT void* idl_calloc  (size_t num, size_t size) idl_attribute_malloc idl_attribute_malloc2 ((idl_free, 1)) idl_attribute_alloc_size ((1, 2));
IDL_EXPORT void* idl_realloc (void *ptr, size_t new_size) idl_attribute_alloc_size ((2));

#endif /* IDL_HEAP_H */
