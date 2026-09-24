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
#include <stdint.h>
#include <assert.h>

#include "idl/vector.h"
#include "idl/heap.h"

bool idl_boxed_vector_init (struct idl_boxed_vector *v)
{
  v->n = 0;
  v->cap = 1;
  v->xs = idl_malloc (v->cap * sizeof (*v->xs));
  return (v->xs != NULL);
}

bool idl_boxed_vector_append (struct idl_boxed_vector *v, void *x)
{
  if (v->n == v->cap)
  {
    assert (v->cap <= SIZE_MAX / 2);
    const size_t cap1 = 2 * v->cap;
    void **xs1 = idl_realloc (v->xs, cap1 * sizeof (*xs1));
    if (xs1 == NULL)
      return false;
    v->xs = xs1;
    v->cap = cap1;
  }
  v->xs[v->n++] = x;
  return true;
}

void idl_boxed_vector_fini (struct idl_boxed_vector *c, void (*f) (void *x))
{
  for (size_t i = 0; i < c->n; i++)
    f (c->xs[i]);
  idl_free (c->xs);
}

void *idl_boxed_vector_next (struct idl_boxed_vector_iter *it)
{
  return (it->i < it->v->n) ? it->v->xs[it->i++] : NULL;
}

void *idl_boxed_vector_first (struct idl_boxed_vector *v, struct idl_boxed_vector_iter *it)
{
  it->v = v;
  it->i = 0;
  return idl_boxed_vector_next (it);
}

const void *idl_boxed_vector_next_c (struct idl_boxed_vector_iter *it)
{
  return idl_boxed_vector_next (it);
}

const void *idl_boxed_vector_first_c (const struct idl_boxed_vector *v, struct idl_boxed_vector_iter *it)
{
  return idl_boxed_vector_first ((struct idl_boxed_vector *) v, it);
}

void idl_boxed_vector_sort (struct idl_boxed_vector *v, int (*cmp) (const void **a, const void **b))
{
  qsort (v->xs, v->n, sizeof (*v->xs), (int (*) (const void *, const void *)) cmp);
}
