// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stddef.h>

#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsi/ddsi_freelist.h"

#if DDSI_FREELIST_TYPE == DDSI_FREELIST_NONE

void ddsi_freelist_init (struct ddsi_freelist *fl, uint32_t max, size_t linkoff)
{
  (void) fl; (void) max; (void) linkoff;
}

void ddsi_freelist_fini (struct ddsi_freelist *fl, void (*free) (void *elem))
{
  (void) fl; (void) free;
}

bool ddsi_freelist_push (struct ddsi_freelist *fl, void *elem)
{
  (void) fl; (void) elem;
  return false;
}

void *ddsi_freelist_pushmany (struct ddsi_freelist *fl, void *first, void *last, uint32_t n)
{
  (void) fl; (void) first; (void) last; (void) n;
  return first;
}

void *ddsi_freelist_pop (struct ddsi_freelist *fl)
{
  (void) fl;
  return NULL;
}

#elif DDSI_FREELIST_TYPE == DDSI_FREELIST_ATOMIC_LIFO

void ddsi_freelist_init (struct ddsi_freelist *fl, uint32_t max, size_t linkoff)
{
  ddsrt_atomic_lifo_init (&fl->x);
  ddsrt_atomic_st32(&fl->count, 0);
  fl->max = (max == UINT32_MAX) ? max-1 : max;
  fl->linkoff = linkoff;
}

void ddsi_freelist_fini (struct ddsi_freelist *fl, void (*free) (void *elem))
{
  void *e;
  while ((e = ddsrt_atomic_lifo_pop (&fl->x, fl->linkoff)) != NULL)
    free (e);
}

bool ddsi_freelist_push (struct ddsi_freelist *fl, void *elem)
{
  if (ddsrt_atomic_inc32_nv (&fl->count) <= fl->max)
  {
    ddsrt_atomic_lifo_push (&fl->x, elem, fl->linkoff);
    return true;
  }
  else
  {
    ddsrt_atomic_dec32 (&fl->count);
    return false;
  }
}

void *ddsi_freelist_pushmany (struct ddsi_freelist *fl, void *first, void *last, uint32_t n)
{
  ddsrt_atomic_add32 (&fl->count, n);
  ddsrt_atomic_lifo_pushmany (&fl->x, first, last, fl->linkoff);
  return NULL;
}

void *ddsi_freelist_pop (struct ddsi_freelist *fl)
{
  void *e;
  if ((e = ddsrt_atomic_lifo_pop (&fl->x, fl->linkoff)) != NULL)
  {
    ddsrt_atomic_dec32(&fl->count);
    return e;
  }
  else
  {
    return NULL;
  }
}

#elif DDSI_FREELIST_TYPE == DDSI_FREELIST_DOUBLE

static ddsrt_thread_local int freelist_inner_idx = -1;

void ddsi_freelist_init (struct ddsi_freelist *fl, uint32_t max, size_t linkoff)
{
  int i;
  ddsrt_mutex_init (&fl->lock);
  for (i = 0; i < NN_FREELIST_NPAR; i++)
  {
    ddsrt_mutex_init (&fl->inner[i].lock);
    fl->inner[i].count = 0;
    fl->inner[i].m = ddsrt_malloc (sizeof (*fl->inner[i].m));
  }
  ddsrt_atomic_st32 (&fl->cc, 0);
  fl->mlist = NULL;
  fl->emlist = NULL;
  fl->count = 0;
  fl->max = (max == UINT32_MAX) ? max-1 : max;
  fl->linkoff = linkoff;
}

static void *get_next (const struct ddsi_freelist *fl, const void *e)
{
  return *((void **) ((char *)e + fl->linkoff));
}

void ddsi_freelist_fini (struct ddsi_freelist *fl, void (*xfree) (void *))
{
  int i;
  uint32_t j;
  struct ddsi_freelist_m *m;
  ddsrt_mutex_destroy (&fl->lock);
  for (i = 0; i < NN_FREELIST_NPAR; i++)
  {
    ddsrt_mutex_destroy (&fl->inner[i].lock);
    for (j = 0; j < fl->inner[i].count; j++)
      xfree (fl->inner[i].m->x[j]);
    ddsrt_free(fl->inner[i].m);
  }
  /* The compiler can't make sense of all these linked lists and doesn't
   * realize that the next pointers are always initialized here. */
  DDSRT_WARNING_MSVC_OFF(6001);
  while ((m = fl->mlist) != NULL)
  {
    fl->mlist = m->next;
    for (j = 0; j < NN_FREELIST_MAGSIZE; j++)
      xfree (m->x[j]);
    ddsrt_free (m);
  }
  while ((m = fl->emlist) != NULL)
  {
    fl->emlist = m->next;
    ddsrt_free (m);
  }
  DDSRT_WARNING_MSVC_ON(6001);
}

static ddsrt_atomic_uint32_t freelist_inner_idx_off = DDSRT_ATOMIC_UINT32_INIT(0);

static int get_freelist_inner_idx (void)
{
  if (freelist_inner_idx == -1)
  {
    static const uint64_t unihashconsts[] = {
      UINT64_C (16292676669999574021),
      UINT64_C (10242350189706880077),
    };
    uintptr_t addr;
    uint64_t t = (uint64_t) ((uintptr_t) &addr) + ddsrt_atomic_ld32 (&freelist_inner_idx_off);
    freelist_inner_idx = (int) (((((uint32_t) t + unihashconsts[0]) * ((uint32_t) (t >> 32) + unihashconsts[1]))) >> (64 - NN_FREELIST_NPAR_LG2));
  }
  return freelist_inner_idx;
}

static int lock_inner (struct ddsi_freelist *fl)
{
  int k = get_freelist_inner_idx();
  if (!ddsrt_mutex_trylock (&fl->inner[k].lock))
  {
    ddsrt_mutex_lock (&fl->inner[k].lock);
    if (ddsrt_atomic_inc32_nv (&fl->cc) == 100)
    {
      ddsrt_atomic_st32(&fl->cc, 0);
      ddsrt_atomic_inc32(&freelist_inner_idx_off);
      freelist_inner_idx = -1;
    }
  }
  return k;
}

bool ddsi_freelist_push (struct ddsi_freelist *fl, void *elem)
{
  int k = lock_inner (fl);
  if (fl->inner[k].count < NN_FREELIST_MAGSIZE)
  {
    fl->inner[k].m->x[fl->inner[k].count++] = elem;
    ddsrt_mutex_unlock (&fl->inner[k].lock);
    return true;
  }
  else
  {
    struct ddsi_freelist_m *m;
    ddsrt_mutex_lock (&fl->lock);
    if (fl->count + NN_FREELIST_MAGSIZE >= fl->max)
    {
      ddsrt_mutex_unlock (&fl->lock);
      ddsrt_mutex_unlock (&fl->inner[k].lock);
      return false;
    }
    m = fl->inner[k].m;
    m->next = fl->mlist;
    fl->mlist = m;
    fl->count += NN_FREELIST_MAGSIZE;
    fl->inner[k].count = 0;
    if (fl->emlist == NULL)
      fl->inner[k].m = ddsrt_malloc (sizeof (*fl->inner[k].m));
    else
    {
      fl->inner[k].m = fl->emlist;
      fl->emlist = fl->emlist->next;
    }
    ddsrt_mutex_unlock (&fl->lock);
    fl->inner[k].m->x[fl->inner[k].count++] = elem;
    ddsrt_mutex_unlock (&fl->inner[k].lock);
    return true;
  }
}

void *ddsi_freelist_pushmany (struct ddsi_freelist *fl, void *first, void *last, uint32_t n)
{
  void *m = first;
  (void)last;
  (void)n;
  while (m)
  {
    void *mnext = get_next (fl, m);
    if (!ddsi_freelist_push (fl, m)) {
      return m;
    }
    m = mnext;
  }
  return NULL;
}

void *ddsi_freelist_pop (struct ddsi_freelist *fl)
{
  int k = lock_inner (fl);
  if (fl->inner[k].count > 0)
  {
    void *e = fl->inner[k].m->x[--fl->inner[k].count];
    ddsrt_mutex_unlock (&fl->inner[k].lock);
    return e;
  }
  else
  {
    ddsrt_mutex_lock (&fl->lock);
    if (fl->mlist == NULL)
    {
      ddsrt_mutex_unlock (&fl->lock);
      ddsrt_mutex_unlock (&fl->inner[k].lock);
      return NULL;
    }
    else
    {
      void *e;
      fl->inner[k].m->next = fl->emlist;
      fl->emlist = fl->inner[k].m;
      fl->inner[k].m = fl->mlist;
      fl->mlist = fl->mlist->next;
      fl->count -= NN_FREELIST_MAGSIZE;
      ddsrt_mutex_unlock (&fl->lock);
      fl->inner[k].count = NN_FREELIST_MAGSIZE;
      e = fl->inner[k].m->x[--fl->inner[k].count];
      ddsrt_mutex_unlock (&fl->inner[k].lock);
      return e;
    }
  }
}

#endif /* DDSI_FREELIST_TYPE */
