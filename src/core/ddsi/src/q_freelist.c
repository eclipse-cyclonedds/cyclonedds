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
#include <stddef.h>

#include "os/os.h"
#include "ddsi/q_freelist.h"

#if FREELIST_TYPE == FREELIST_ATOMIC_LIFO

void nn_freelist_init (_Out_ struct nn_freelist *fl, uint32_t max, off_t linkoff)
{
  os_atomic_lifo_init (&fl->x);
  os_atomic_st32(&fl->count, 0);
  fl->max = (max == UINT32_MAX) ? max-1 : max;
  fl->linkoff = linkoff;
}

void nn_freelist_fini (_Inout_ _Post_invalid_ struct nn_freelist *fl, _In_ void (*free) (void *elem))
{
  void *e;
  while ((e = os_atomic_lifo_pop (&fl->x, fl->linkoff)) != NULL)
    free (e);
}

_Check_return_ bool nn_freelist_push (_Inout_ struct nn_freelist *fl, _Inout_ _When_ (return != 0, _Post_invalid_) void *elem)
{
  if (os_atomic_inc32_nv (&fl->count) <= fl->max)
  {
    os_atomic_lifo_push (&fl->x, elem, fl->linkoff);
    return true;
  }
  else
  {
    os_atomic_dec32 (&fl->count);
    return false;
  }
}

_Check_return_ _Ret_maybenull_ void *nn_freelist_pushmany (_Inout_ struct nn_freelist *fl, _Inout_opt_ _When_ (return != 0, _Post_invalid_) void *first, _Inout_opt_ _When_ (return != 0, _Post_invalid_) void *last, uint32_t n)
{
  os_atomic_add32 (&fl->count, n);
  os_atomic_lifo_pushmany (&fl->x, first, last, fl->linkoff);
  return NULL;
}

_Check_return_ _Ret_maybenull_ void *nn_freelist_pop (_Inout_ struct nn_freelist *fl)
{
  void *e;
  if ((e = os_atomic_lifo_pop (&fl->x, fl->linkoff)) != NULL)
  {
    os_atomic_dec32(&fl->count);
    return e;
  }
  else
  {
    return NULL;
  }
}

#elif FREELIST_TYPE == FREELIST_DOUBLE

static os_threadLocal int freelist_inner_idx = -1;

void nn_freelist_init (_Out_ struct nn_freelist *fl, uint32_t max, off_t linkoff)
{
  int i;
  os_mutexInit (&fl->lock);
  for (i = 0; i < NN_FREELIST_NPAR; i++)
  {
    os_mutexInit (&fl->inner[i].lock);
    fl->inner[i].count = 0;
    fl->inner[i].m = os_malloc (sizeof (*fl->inner[i].m));
  }
  os_atomic_st32 (&fl->cc, 0);
  fl->mlist = NULL;
  fl->emlist = NULL;
  fl->count = 0;
  fl->max = (max == UINT32_MAX) ? max-1 : max;
  fl->linkoff = linkoff;
}

static void *get_next (const struct nn_freelist *fl, const void *e)
{
  return *((void **) ((char *)e + fl->linkoff));
}

void nn_freelist_fini (_Inout_ _Post_invalid_ struct nn_freelist *fl, _In_ void (*xfree) (void *))
{
  int i;
  uint32_t j;
  struct nn_freelistM *m;
  os_mutexDestroy (&fl->lock);
  for (i = 0; i < NN_FREELIST_NPAR; i++)
  {
    os_mutexDestroy (&fl->inner[i].lock);
    for (j = 0; j < fl->inner[i].count; j++)
      xfree (fl->inner[i].m->x[j]);
    os_free(fl->inner[i].m);
  }
/* The compiler can't make sense of all these linked lists and doesn't
 * realize that the next pointers are always initialized here. */
OS_WARNING_MSVC_OFF(6001);
  while ((m = fl->mlist) != NULL)
  {
    fl->mlist = m->next;
    for (j = 0; j < NN_FREELIST_MAGSIZE; j++)
      xfree (m->x[j]);
    os_free (m);
  }
  while ((m = fl->emlist) != NULL)
  {
    fl->emlist = m->next;
    os_free (m);
  }
OS_WARNING_MSVC_ON(6001);
}

static os_atomic_uint32_t freelist_inner_idx_off = OS_ATOMIC_UINT32_INIT(0);

static int get_freelist_inner_idx (void)
{
  if (freelist_inner_idx == -1)
  {
    static const uint64_t unihashconsts[] = {
      UINT64_C (16292676669999574021),
      UINT64_C (10242350189706880077),
    };
    uintptr_t addr;
    uint64_t t = (uint64_t) ((uintptr_t) &addr) + os_atomic_ld32 (&freelist_inner_idx_off);
    freelist_inner_idx = (int) (((((uint32_t) t + unihashconsts[0]) * ((uint32_t) (t >> 32) + unihashconsts[1]))) >> (64 - NN_FREELIST_NPAR_LG2));
  }
  return freelist_inner_idx;
}

static int lock_inner (struct nn_freelist *fl)
{
  int k = get_freelist_inner_idx();
  if (os_mutexTryLock (&fl->inner[k].lock) != os_resultSuccess)
  {
    os_mutexLock (&fl->inner[k].lock);
    if (os_atomic_inc32_nv (&fl->cc) == 100)
    {
      os_atomic_st32(&fl->cc, 0);
      os_atomic_inc32(&freelist_inner_idx_off);
      freelist_inner_idx = -1;
    }
  }
  return k;
}

_Check_return_ bool nn_freelist_push (_Inout_ struct nn_freelist *fl, _Inout_ _When_ (return != 0, _Post_invalid_) void *elem)
{
  int k = lock_inner (fl);
  if (fl->inner[k].count < NN_FREELIST_MAGSIZE)
  {
    fl->inner[k].m->x[fl->inner[k].count++] = elem;
    os_mutexUnlock (&fl->inner[k].lock);
    return true;
  }
  else
  {
    struct nn_freelistM *m;
    os_mutexLock (&fl->lock);
    if (fl->count + NN_FREELIST_MAGSIZE >= fl->max)
    {
      os_mutexUnlock (&fl->lock);
      os_mutexUnlock (&fl->inner[k].lock);
      return false;
    }
    m = fl->inner[k].m;
    m->next = fl->mlist;
    fl->mlist = m;
    fl->count += NN_FREELIST_MAGSIZE;
    fl->inner[k].count = 0;
    if (fl->emlist == NULL)
      fl->inner[k].m = os_malloc (sizeof (*fl->inner[k].m));
    else
    {
      fl->inner[k].m = fl->emlist;
      fl->emlist = fl->emlist->next;
    }
    os_mutexUnlock (&fl->lock);
    fl->inner[k].m->x[fl->inner[k].count++] = elem;
    os_mutexUnlock (&fl->inner[k].lock);
    return true;
  }
}

_Check_return_ _Ret_maybenull_ void *nn_freelist_pushmany (_Inout_ struct nn_freelist *fl, _Inout_opt_ _When_ (return != 0, _Post_invalid_) void *first, _Inout_opt_ _When_ (return != 0, _Post_invalid_) void *last, uint32_t n)
{
  void *m = first;
  (void)last;
  (void)n;
  while (m)
  {
    void *mnext = get_next (fl, m);
    if (!nn_freelist_push (fl, m)) {
      return m;
    }
    m = mnext;
  }
  return NULL;
}

_Check_return_ _Ret_maybenull_ void *nn_freelist_pop (_Inout_ struct nn_freelist *fl)
{
  int k = lock_inner (fl);
  if (fl->inner[k].count > 0)
  {
    void *e = fl->inner[k].m->x[--fl->inner[k].count];
    os_mutexUnlock (&fl->inner[k].lock);
    return e;
  }
  else
  {
    os_mutexLock (&fl->lock);
    if (fl->mlist == NULL)
    {
      os_mutexUnlock (&fl->lock);
      os_mutexUnlock (&fl->inner[k].lock);
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
      os_mutexUnlock (&fl->lock);
      fl->inner[k].count = NN_FREELIST_MAGSIZE;
      e = fl->inner[k].m->x[--fl->inner[k].count];
      os_mutexUnlock (&fl->inner[k].lock);
      return e;
    }
  }
}

#endif
