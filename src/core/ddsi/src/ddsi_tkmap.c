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
#include <assert.h>
#include <string.h>
#include "ddsi/q_thread.h"
#include "ddsi/q_unused.h"
#include "ddsi/q_gc.h"
#include "ddsi/q_globals.h"
#include "ddsi/q_config.h"
#include "ddsi/ddsi_iid.h"
#include "ddsi/ddsi_tkmap.h"
#include "util/ut_hopscotch.h"
#include "dds__stream.h"
#include "os/os.h"
#include "ddsi/ddsi_serdata.h"

#define REFC_DELETE 0x80000000
#define REFC_MASK   0x0fffffff

struct ddsi_tkmap
{
  struct ut_chh * m_hh;
  os_mutex m_lock;
  os_cond m_cond;
};

static void gc_buckets_impl (struct gcreq *gcreq)
{
  os_free (gcreq->arg);
  gcreq_free (gcreq);
}

static void gc_buckets (void *a)
{
  struct gcreq *gcreq = gcreq_new (gv.gcreq_queue, gc_buckets_impl);
  gcreq->arg = a;
  gcreq_enqueue (gcreq);
}

static void gc_tkmap_instance_impl (struct gcreq *gcreq)
{
  struct ddsi_tkmap_instance *tk = gcreq->arg;
  ddsi_serdata_unref (tk->m_sample);
  dds_free (tk);
  gcreq_free (gcreq);
}

static void gc_tkmap_instance (struct ddsi_tkmap_instance *tk)
{
  struct gcreq *gcreq = gcreq_new (gv.gcreq_queue, gc_tkmap_instance_impl);
  gcreq->arg = tk;
  gcreq_enqueue (gcreq);
}

static uint32_t dds_tk_hash (const struct ddsi_tkmap_instance * inst)
{
  return inst->m_sample->hash;
}

static uint32_t dds_tk_hash_void (const void * inst)
{
  return dds_tk_hash (inst);
}

static int dds_tk_equals (const struct ddsi_tkmap_instance *a, const struct ddsi_tkmap_instance *b)
{
  return (a->m_sample->ops == b->m_sample->ops) ? ddsi_serdata_eqkey (a->m_sample, b->m_sample) : 0;
}

static int dds_tk_equals_void (const void *a, const void *b)
{
  return dds_tk_equals (a, b);
}

struct ddsi_tkmap *ddsi_tkmap_new (void)
{
  struct ddsi_tkmap *tkmap = dds_alloc (sizeof (*tkmap));
  tkmap->m_hh = ut_chhNew (1, dds_tk_hash_void, dds_tk_equals_void, gc_buckets);
  os_mutexInit (&tkmap->m_lock);
  os_condInit (&tkmap->m_cond, &tkmap->m_lock);
  return tkmap;
}

static void free_tkmap_instance (void *vtk, UNUSED_ARG(void *f_arg))
{
  struct ddsi_tkmap_instance *tk = vtk;
  ddsi_serdata_unref (tk->m_sample);
  os_free (tk);
}

void ddsi_tkmap_free (_Inout_ _Post_invalid_ struct ddsi_tkmap * map)
{
  ut_chhEnumUnsafe (map->m_hh, free_tkmap_instance, NULL);
  ut_chhFree (map->m_hh);
  os_condDestroy (&map->m_cond);
  os_mutexDestroy (&map->m_lock);
  dds_free (map);
}

uint64_t ddsi_tkmap_lookup (_In_ struct ddsi_tkmap * map, _In_ const struct ddsi_serdata * sd)
{
  struct ddsi_tkmap_instance dummy;
  struct ddsi_tkmap_instance * tk;
  assert (vtime_awake_p(lookup_thread_state()->vtime));
  dummy.m_sample = (struct ddsi_serdata *) sd;
  tk = ut_chhLookup (map->m_hh, &dummy);
  return (tk) ? tk->m_iid : DDS_HANDLE_NIL;
}

_Check_return_
struct ddsi_tkmap_instance *ddsi_tkmap_find_by_id (_In_ struct ddsi_tkmap *map, _In_ uint64_t iid)
{
  /* This is not a function that should be used liberally, as it linearly scans the key-to-iid map. */
  struct ut_chhIter it;
  struct ddsi_tkmap_instance *tk;
  uint32_t refc;
  assert (vtime_awake_p(lookup_thread_state()->vtime));
  for (tk = ut_chhIterFirst (map->m_hh, &it); tk; tk = ut_chhIterNext (&it))
    if (tk->m_iid == iid)
      break;
  if (tk == NULL)
    /* Common case of it not existing at all */
    return NULL;
  else if (!((refc = os_atomic_ld32 (&tk->m_refc)) & REFC_DELETE) && os_atomic_cas32 (&tk->m_refc, refc, refc+1))
    /* Common case of it existing, not in the process of being deleted and no one else concurrently modifying the refcount */
    return tk;
  else
    /* Let key value lookup handle the possible CAS loop and the complicated cases */
    return ddsi_tkmap_find (tk->m_sample, false, false);
}

/* Debug keyhash generation for debug and coverage builds */

#ifdef NDEBUG
#if VL_BUILD_LCOV
#define DDS_DEBUG_KEYHASH 1
#else
#define DDS_DEBUG_KEYHASH 0
#endif
#else
#define DDS_DEBUG_KEYHASH 1
#endif

_Check_return_
struct ddsi_tkmap_instance * ddsi_tkmap_find(
        _In_ struct ddsi_serdata * sd,
        _In_ const bool rd,
        _In_ const bool create)
{
  struct ddsi_tkmap_instance dummy;
  struct ddsi_tkmap_instance * tk;
  struct ddsi_tkmap * map = gv.m_tkmap;

  assert (vtime_awake_p(lookup_thread_state()->vtime));
  dummy.m_sample = sd;
retry:
  if ((tk = ut_chhLookup(map->m_hh, &dummy)) != NULL)
  {
    uint32_t new;
    new = os_atomic_inc32_nv(&tk->m_refc);
    if (new & REFC_DELETE)
    {
      /* for the unlikely case of spinning 2^31 times across all threads ... */
      os_atomic_dec32(&tk->m_refc);

      /* simplest action would be to just spin, but that can potentially take a long time;
       we can block until someone signals some entry is removed from the map if we take
       some lock & wait for some condition */
      os_mutexLock(&map->m_lock);
      while ((tk = ut_chhLookup(map->m_hh, &dummy)) != NULL && (os_atomic_ld32(&tk->m_refc) & REFC_DELETE))
        os_condWait(&map->m_cond, &map->m_lock);
      os_mutexUnlock(&map->m_lock);
      goto retry;
    }
  }
  else if (create)
  {
    if ((tk = dds_alloc (sizeof (*tk))) == NULL)
      return NULL;

    tk->m_sample = ddsi_serdata_to_topicless (sd);
    os_atomic_st32 (&tk->m_refc, 1);
    tk->m_iid = ddsi_iid_gen ();
    if (!ut_chhAdd (map->m_hh, tk))
    {
      /* Lost a race from another thread, retry */
      ddsi_serdata_unref (tk->m_sample);
      dds_free (tk);
      goto retry;
    }
  }

  if (tk && rd)
  {
    DDS_TRACE("tk=%p iid=%"PRIx64" ", (void *) &tk, tk->m_iid);
  }
  return tk;
}

_Check_return_
struct ddsi_tkmap_instance * ddsi_tkmap_lookup_instance_ref (_In_ struct ddsi_serdata * sd)
{
  return ddsi_tkmap_find (sd, true, true);
}

void ddsi_tkmap_instance_ref (_In_ struct ddsi_tkmap_instance *tk)
{
  os_atomic_inc32 (&tk->m_refc);
}

void ddsi_tkmap_instance_unref (_In_ struct ddsi_tkmap_instance * tk)
{
  uint32_t old, new;
  assert (vtime_awake_p(lookup_thread_state()->vtime));
  do {
    old = os_atomic_ld32(&tk->m_refc);
    if (old == 1)
      new = REFC_DELETE;
    else
    {
      assert(!(old & REFC_DELETE));
      new = old - 1;
    }
  } while (!os_atomic_cas32(&tk->m_refc, old, new));
  if (new == REFC_DELETE)
  {
    struct ddsi_tkmap *map = gv.m_tkmap;

    /* Remove from hash table */
    int removed = ut_chhRemove(map->m_hh, tk);
    assert (removed);
    (void)removed;

    /* Signal any threads blocked in their retry loops in lookup */
    os_mutexLock(&map->m_lock);
    os_condBroadcast(&map->m_cond);
    os_mutexUnlock(&map->m_lock);

    /* Schedule freeing of memory until after all those who may have found a pointer have
     progressed to where they no longer hold that pointer */
    gc_tkmap_instance(tk);
  }
}
