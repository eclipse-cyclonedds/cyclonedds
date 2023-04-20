// Copyright(c) 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdlib.h>
#include <string.h>
#include "idl/heap.h"

void* idl_malloc(size_t size) {
  return malloc(size);
}

void* idl_calloc(size_t num, size_t size) {
  return calloc(num, size);
}

void* idl_realloc(void *ptr, size_t new_size) {
  return realloc(ptr, new_size);
}

void idl_free(void *pt) {
  free(pt);
}
