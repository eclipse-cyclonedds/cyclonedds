// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stddef.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_log.h"
#include "ddsi__inverse_uint32_set.h"

static int uint32_t_cmp(const void *va, const void *vb);

static ddsrt_avl_treedef_t inverse_uint32_set_td = DDSRT_AVL_TREEDEF_INITIALIZER(offsetof(struct ddsi_inverse_uint32_set_node, avlnode), offsetof(struct ddsi_inverse_uint32_set_node, min), uint32_t_cmp, 0);

static int uint32_t_cmp(const void *va, const void *vb)
{
  const uint32_t *a = va;
  const uint32_t *b = vb;
  return (*a == *b) ? 0 : (*a < *b) ? -1 : 1;
}

static void check(const struct ddsi_inverse_uint32_set *set)
{
#ifndef NDEBUG
  ddsrt_avl_iter_t it;
  struct ddsi_inverse_uint32_set_node *pn = NULL, *n;
  assert(set->min <= set->max);
  assert(set->cursor >= set->min);
  assert(set->cursor <= set->max);
  for (n = ddsrt_avl_iter_first(&inverse_uint32_set_td, &set->ids, &it); n; pn = n, n = ddsrt_avl_iter_next(&it))
  {
    assert(n->min <= n->max);
    assert(n->min >= set->min);
    assert(n->max <= set->max);
    assert(pn == NULL || n->min > pn->max+1);
  }
#else
  (void)set;
#endif
}

void ddsi_inverse_uint32_set_init(struct ddsi_inverse_uint32_set *set, uint32_t min, uint32_t max)
{
  struct ddsi_inverse_uint32_set_node *n;
  ddsrt_avl_init(&inverse_uint32_set_td, &set->ids);
  set->cursor = min;
  set->min = min;
  set->max = max;
  n = ddsrt_malloc(sizeof(*n));
  n->min = min;
  n->max = max;
  ddsrt_avl_insert(&inverse_uint32_set_td, &set->ids, n);
  check(set);
}

void ddsi_inverse_uint32_set_fini(struct ddsi_inverse_uint32_set *set)
{
  ddsrt_avl_free(&inverse_uint32_set_td, &set->ids, ddsrt_free);
}

static uint32_t inverse_uint32_set_alloc_use_min(struct ddsi_inverse_uint32_set *set, struct ddsi_inverse_uint32_set_node *n)
{
  const uint32_t id = n->min;
  if (n->min == n->max)
  {
    ddsrt_avl_delete(&inverse_uint32_set_td, &set->ids, n);
    ddsrt_free(n);
  }
  else
  {
    /* changing the key in-place here: the key value may be changing, but the structure of the tree is not */
    n->min++;
  }
  return id;
}

int ddsi_inverse_uint32_set_alloc(uint32_t * const id, struct ddsi_inverse_uint32_set *set)
{
  struct ddsi_inverse_uint32_set_node *n;
  if ((n = ddsrt_avl_lookup_pred_eq(&inverse_uint32_set_td, &set->ids, &set->cursor)) != NULL && set->cursor <= n->max) {
    /* n is [a,b] s.t. a <= C <= b, so C is available */
    *id = set->cursor;
    if (n->min == set->cursor)
    {
      (void)inverse_uint32_set_alloc_use_min(set, n);
    }
    else if (set->cursor == n->max)
    {
      assert(n->min < n->max);
      n->max--;
    }
    else
    {
      struct ddsi_inverse_uint32_set_node *n1 = ddsrt_malloc(sizeof(*n1));
      assert(n->min < set->cursor && set->cursor < n->max);
      n1->min = set->cursor + 1;
      n1->max = n->max;
      n->max = set->cursor - 1;
      ddsrt_avl_insert(&inverse_uint32_set_td, &set->ids, n1);
    }
  }
  else if ((n = ddsrt_avl_lookup_succ(&inverse_uint32_set_td, &set->ids, &set->cursor)) != NULL)
  {
    /* n is [a,b] s.t. a > C and all intervals [a',b'] in tree have a' <= C */
    *id = inverse_uint32_set_alloc_use_min(set, n);
  }
  else if ((n = ddsrt_avl_find_min(&inverse_uint32_set_td, &set->ids)) != NULL)
  {
    /* no available ids >= cursor: wrap around and use the first available */
    assert(n->max < set->cursor);
    *id = inverse_uint32_set_alloc_use_min(set, n);
  }
  else
  {
    return 0;
  }
  assert(*id >= set->min);
  set->cursor = (*id < set->max) ? (*id + 1) : set->min;
  check(set);
  return 1;
}

void ddsi_inverse_uint32_set_free(struct ddsi_inverse_uint32_set *set, uint32_t id)
{
  struct ddsi_inverse_uint32_set_node *n;
  const uint32_t idp1 = id + 1;
  ddsrt_avl_ipath_t ip;
  if ((n = ddsrt_avl_lookup_pred_eq(&inverse_uint32_set_td, &set->ids, &id)) != NULL && id <= n->max + 1) {
    if (id <= n->max)
    {
      /* n is [a,b] s.t. a <= I <= b: so it is already in the set */
      return;
    }
    else
    {
      struct ddsi_inverse_uint32_set_node *n1;
      ddsrt_avl_dpath_t dp;
      /* grow the interval, possibly coalesce with next */
      if ((n1 = ddsrt_avl_lookup_dpath(&inverse_uint32_set_td, &set->ids, &idp1, &dp)) == NULL) {
        n->max = id;
      } else {
        n->max = n1->max;
        ddsrt_avl_delete_dpath(&inverse_uint32_set_td, &set->ids, n1, &dp);
        ddsrt_free(n1);
      }
    }
  }
  else if ((n = ddsrt_avl_lookup_ipath(&inverse_uint32_set_td, &set->ids, &idp1, &ip)) != NULL) {
    /* changing the key in-place here: the key value may be changing, but the structure of the tree is not or the previous case would have applied */
    n->min = id;
  }
  else
  {
    /* no adjacent interval */
    n = ddsrt_malloc(sizeof(*n));
    n->min = n->max = id;
    ddsrt_avl_insert_ipath(&inverse_uint32_set_td, &set->ids, n, &ip);
  }
  check(set);
}

