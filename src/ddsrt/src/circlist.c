// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "dds/ddsrt/circlist.h"

void ddsrt_circlist_init (struct ddsrt_circlist *list)
{
  list->latest = NULL;
}

bool ddsrt_circlist_isempty (const struct ddsrt_circlist *list)
{
  return list->latest == NULL;
}

void ddsrt_circlist_append (struct ddsrt_circlist *list, struct ddsrt_circlist_elem *elem)
{
  if (list->latest == NULL)
    elem->next = elem->prev = elem;
  else
  {
    struct ddsrt_circlist_elem * const hd = list->latest;
#ifndef NDEBUG
    {
      const struct ddsrt_circlist_elem *x = hd;
      do { assert (x != elem); x = x->next; } while (x != hd);
    }
#endif
    elem->next = hd->next;
    elem->prev = hd;
    hd->next = elem;
    elem->next->prev = elem;
  }
  list->latest = elem;
}

void ddsrt_circlist_remove (struct ddsrt_circlist *list, struct ddsrt_circlist_elem *elem)
{
#ifndef NDEBUG
  {
    const struct ddsrt_circlist_elem *x = list->latest;
    assert (x);
    do { if (x == elem) break; x = x->next; } while (x != list->latest);
    assert (x == elem);
  }
#endif
  if (elem->next == elem)
    list->latest = NULL;
  else
  {
    struct ddsrt_circlist_elem * const elem_prev = elem->prev;
    struct ddsrt_circlist_elem * const elem_next = elem->next;
    elem_prev->next = elem_next;
    elem_next->prev = elem_prev;
    if (list->latest == elem)
      list->latest = elem_prev;
  }
}

struct ddsrt_circlist_elem *ddsrt_circlist_oldest (const struct ddsrt_circlist *list)
{
  assert (!ddsrt_circlist_isempty (list));
  return list->latest->next;
}

struct ddsrt_circlist_elem *ddsrt_circlist_latest (const struct ddsrt_circlist *list)
{
  assert (!ddsrt_circlist_isempty (list));
  return list->latest;
}


