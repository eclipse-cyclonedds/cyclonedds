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
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "ddsi__thread.h"
#include "ddsi__gc.h"
#include "dds/cdr/dds_cdrstream.h"

#define REFC_DELETE 0x80000000
#define REFC_MASK   0x0fffffff

struct ddsi_tkmap
{
  struct ddsrt_chh *m_hh;
  struct ddsi_domaingv *gv;
  ddsrt_mutex_t m_lock;
  ddsrt_cond_t m_cond;
};

static void gc_buckets_impl (struct ddsi_gcreq *gcreq)
{
  ddsrt_free (gcreq->arg);
  ddsi_gcreq_free (gcreq);
}

static void gc_buckets (void *a, void *arg)
{
  const struct ddsi_tkmap *tkmap = arg;
  struct ddsi_gcreq *gcreq = ddsi_gcreq_new (tkmap->gv->gcreq_queue, gc_buckets_impl);
  gcreq->arg = a;
  ddsi_gcreq_enqueue (gcreq);
}

static void gc_tkmap_instance_impl (struct ddsi_gcreq *gcreq)
{
  struct ddsi_tkmap_instance *tk = gcreq->arg;
  ddsi_serdata_unref (tk->m_sample);
  dds_free (tk);
  ddsi_gcreq_free (gcreq);
}

static void gc_tkmap_instance (struct ddsi_tkmap_instance *tk, struct ddsi_gcreq_queue *gcreq_queue)
{
  struct ddsi_gcreq *gcreq = ddsi_gcreq_new (gcreq_queue, gc_tkmap_instance_impl);
  gcreq->arg = tk;
  ddsi_gcreq_enqueue (gcreq);
}

static uint32_t dds_tk_hash (const struct ddsi_tkmap_instance *inst)
{
  return inst->m_sample->hash;
}

static uint32_t dds_tk_hash_void (const void *inst)
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

struct ddsi_tkmap *ddsi_tkmap_new (struct ddsi_domaingv *gv)
{
  struct ddsi_tkmap *tkmap = dds_alloc (sizeof (*tkmap));
  tkmap->m_hh = ddsrt_chh_new (1, dds_tk_hash_void, dds_tk_equals_void, gc_buckets, tkmap);
  tkmap->gv = gv;
  ddsrt_mutex_init (&tkmap->m_lock);
  ddsrt_cond_init (&tkmap->m_cond);
  return tkmap;
}

static void free_tkmap_instance (void *vtk, UNUSED_ARG(void *f_arg))
{
  struct ddsi_tkmap_instance *tk = vtk;
  ddsi_serdata_unref (tk->m_sample);
  ddsrt_free (tk);
}

void ddsi_tkmap_free (struct ddsi_tkmap * map)
{
  ddsrt_chh_enum_unsafe (map->m_hh, free_tkmap_instance, NULL);
  ddsrt_chh_free (map->m_hh);
  ddsrt_cond_destroy (&map->m_cond);
  ddsrt_mutex_destroy (&map->m_lock);
  dds_free (map);
}

uint64_t ddsi_tkmap_lookup (struct ddsi_tkmap * map, const struct ddsi_serdata * sd)
{
  struct ddsi_tkmap_instance dummy;
  struct ddsi_tkmap_instance * tk;
  assert (ddsi_thread_is_awake ());
  dummy.m_sample = (struct ddsi_serdata *) sd;
  tk = ddsrt_chh_lookup (map->m_hh, &dummy);
  return (tk) ? tk->m_iid : DDS_HANDLE_NIL;
}

struct ddsi_tkmap_instance *ddsi_tkmap_find_by_id (struct ddsi_tkmap *map, uint64_t iid)
{
  /* This is not a function that should be used liberally, as it linearly scans the key-to-iid map. */
  struct ddsrt_chh_iter it;
  struct ddsi_tkmap_instance *tk;
  uint32_t refc;
  assert (ddsi_thread_is_awake ());
  for (tk = ddsrt_chh_iter_first (map->m_hh, &it); tk; tk = ddsrt_chh_iter_next (&it))
    if (tk->m_iid == iid)
      break;
  if (tk == NULL)
    /* Common case of it not existing at all */
    return NULL;
  else if (!((refc = ddsrt_atomic_ld32 (&tk->m_refc)) & REFC_DELETE) && ddsrt_atomic_cas32 (&tk->m_refc, refc, refc+1))
    /* Common case of it existing, not in the process of being deleted and no one else concurrently modifying the refcount */
    return tk;
  else
    /* Let key value lookup handle the possible CAS loop and the complicated cases */
    return ddsi_tkmap_find (map, tk->m_sample, false);
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

struct ddsi_tkmap_instance *ddsi_tkmap_find (struct ddsi_tkmap *map, struct ddsi_serdata *sd, const bool create)
{
  struct ddsi_tkmap_instance dummy;
  struct ddsi_tkmap_instance *tk;

  assert (ddsi_thread_is_awake ());
  dummy.m_sample = sd;
retry:
  if ((tk = ddsrt_chh_lookup(map->m_hh, &dummy)) != NULL)
  {
    uint32_t new;
    new = ddsrt_atomic_inc32_nv(&tk->m_refc);
    if (new & REFC_DELETE)
    {
      /* for the unlikely case of spinning 2^31 times across all threads ... */
      ddsrt_atomic_dec32(&tk->m_refc);

      /* simplest action would be to just spin, but that can potentially take a long time;
       we can block until someone signals some entry is removed from the map if we take
       some lock & wait for some condition */
      ddsrt_mutex_lock(&map->m_lock);
      while ((tk = ddsrt_chh_lookup(map->m_hh, &dummy)) != NULL && (ddsrt_atomic_ld32(&tk->m_refc) & REFC_DELETE))
        ddsrt_cond_wait(&map->m_cond, &map->m_lock);
      ddsrt_mutex_unlock(&map->m_lock);
      goto retry;
    }
  }
  else if (create)
  {
    if ((tk = dds_alloc (sizeof (*tk))) == NULL)
      return NULL;

    tk->m_sample = ddsi_serdata_to_untyped (sd);
    ddsrt_atomic_st32 (&tk->m_refc, 1);
    tk->m_iid = ddsi_iid_gen ();
    if (!ddsrt_chh_add (map->m_hh, tk))
    {
      /* Lost a race from another thread, retry */
      ddsi_serdata_unref (tk->m_sample);
      dds_free (tk);
      goto retry;
    }
  }
  return tk;
}

struct ddsi_tkmap_instance *ddsi_tkmap_lookup_instance_ref (struct ddsi_tkmap *map, struct ddsi_serdata *sd)
{
  return ddsi_tkmap_find (map, sd, true);
}

void ddsi_tkmap_instance_ref (struct ddsi_tkmap_instance *tk)
{
  ddsrt_atomic_inc32 (&tk->m_refc);
}

void ddsi_tkmap_instance_unref (struct ddsi_tkmap *map, struct ddsi_tkmap_instance *tk)
{
  uint32_t old, new;
  assert (ddsi_thread_is_awake ());
  do {
    old = ddsrt_atomic_ld32(&tk->m_refc);
    if (old == 1)
      new = REFC_DELETE;
    else
    {
      assert(!(old & REFC_DELETE));
      new = old - 1;
    }
  } while (!ddsrt_atomic_cas32(&tk->m_refc, old, new));
  if (new == REFC_DELETE)
  {
    /* Remove from hash table */
    int removed = ddsrt_chh_remove(map->m_hh, tk);
    assert (removed);
    (void)removed;

    /* Signal any threads blocked in their retry loops in lookup */
    ddsrt_mutex_lock(&map->m_lock);
    ddsrt_cond_broadcast(&map->m_cond);
    ddsrt_mutex_unlock(&map->m_lock);

    /* Schedule freeing of memory until after all those who may have found a pointer have
     progressed to where they no longer hold that pointer */
    gc_tkmap_instance(tk, map->gv->gcreq_queue);
  }
}
