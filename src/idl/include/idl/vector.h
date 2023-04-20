// Copyright(c) 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef VECTOR_H
#define VECTOR_H

#include <stddef.h>
#include <stdbool.h>

typedef struct idl_boxed_vector {
  size_t n, cap;
  void **xs;
} idl_boxed_vector_t;

typedef struct idl_boxed_vector_iter {
  idl_boxed_vector_t *v;
  size_t i;
} idl_boxed_vector_iter_t;

bool idl_boxed_vector_init (struct idl_boxed_vector *v);
bool idl_boxed_vector_append (struct idl_boxed_vector *v, void *x);
void idl_boxed_vector_fini (struct idl_boxed_vector *c, void (*f) (void *x));
void *idl_boxed_vector_next (struct idl_boxed_vector_iter *it);
void *idl_boxed_vector_first (struct idl_boxed_vector *v, struct idl_boxed_vector_iter *it);
const void *idl_boxed_vector_next_c (struct idl_boxed_vector_iter *it);
const void *idl_boxed_vector_first_c (const struct idl_boxed_vector *v, struct idl_boxed_vector_iter *it);
void idl_boxed_vector_sort (struct idl_boxed_vector *v, int (*cmp) (const void *a, const void *b));

#endif
