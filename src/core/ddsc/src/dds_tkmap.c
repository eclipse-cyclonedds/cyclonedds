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
#include "ddsi/sysdeps.h"
#include "dds__tkmap.h"
#include "dds__iid.h"
#include "util/ut_hopscotch.h"
#include "dds__stream.h"
#include "os/os.h"
#include "q__osplser.h"

#define REFC_DELETE 0x80000000
#define REFC_MASK   0x0fffffff

struct tkmap
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
  struct tkmap_instance *tk = gcreq->arg;
  ddsi_serdata_unref (tk->m_sample);
  dds_free (tk);
  gcreq_free (gcreq);
}

static void gc_tkmap_instance (struct tkmap_instance *tk)
{
  struct gcreq *gcreq = gcreq_new (gv.gcreq_queue, gc_tkmap_instance_impl);
  gcreq->arg = tk;
  gcreq_enqueue (gcreq);
}

/* Fixed seed and length */

#define DDS_MH3_LEN 16
#define DDS_MH3_SEED 0

#define DDS_MH3_ROTL32(x,r) (((x) << (r)) | ((x) >> (32 - (r))))

/* Really
 http://code.google.com/p/smhasher/source/browse/trunk/MurmurHash3.cpp,
 MurmurHash3_x86_32
*/

static uint32_t dds_mh3 (const void * key)
{
  const uint8_t *data = (const uint8_t *) key;
  const intptr_t nblocks = (intptr_t) (DDS_MH3_LEN / 4);
  const uint32_t c1 = 0xcc9e2d51;
  const uint32_t c2 = 0x1b873593;

  uint32_t h1 = DDS_MH3_SEED;

  const uint32_t *blocks = (const uint32_t *) (data + nblocks * 4);
  register intptr_t i;

  for (i = -nblocks; i; i++)
  {
    uint32_t k1 = blocks[i];

    k1 *= c1;
    k1 = DDS_MH3_ROTL32 (k1, 15);
    k1 *= c2;

    h1 ^= k1;
    h1 = DDS_MH3_ROTL32 (h1, 13);
    h1 = h1 * 5+0xe6546b64;
  }

  /* finalization */

  h1 ^= DDS_MH3_LEN;
  h1 ^= h1 >> 16;
  h1 *= 0x85ebca6b;
  h1 ^= h1 >> 13;
  h1 *= 0xc2b2ae35;
  h1 ^= h1 >> 16;

  return h1;
}

static uint32_t dds_tk_hash (const struct tkmap_instance * inst)
{
  volatile struct serdata * sd = (volatile struct serdata *) inst->m_sample;

  if (! sd->v.hash_valid)
  {
    const uint32_t * k = (const uint32_t *) sd->v.keyhash.m_hash;
    const uint32_t hash0 = sd->v.st->topic ? sd->v.st->topic->hash : 0;
    sd->v.hash = ((sd->v.keyhash.m_flags & DDS_KEY_IS_HASH) ? dds_mh3 (k) : (*k)) ^ hash0;
    sd->v.hash_valid = 1;
  }

  return sd->v.hash;
}

static uint32_t dds_tk_hash_void (const void * inst)
{
  return dds_tk_hash (inst);
}

static int dds_tk_equals (const struct tkmap_instance *a, const struct tkmap_instance *b)
{
  return serdata_cmp (a->m_sample, b->m_sample) == 0;
}

static int dds_tk_equals_void (const void *a, const void *b)
{
  return dds_tk_equals (a, b);
}

struct tkmap * dds_tkmap_new (void)
{
  struct tkmap *tkmap = dds_alloc (sizeof (*tkmap));
  tkmap->m_hh = ut_chhNew (1, dds_tk_hash_void, dds_tk_equals_void, gc_buckets);
  os_mutexInit (&tkmap->m_lock);
  os_condInit (&tkmap->m_cond, &tkmap->m_lock);
  return tkmap;
}

static void free_tkmap_instance (void *vtk, UNUSED_ARG(void *f_arg))
{
  struct tkmap_instance *tk = vtk;
  ddsi_serdata_unref (tk->m_sample);
  os_free (tk);
}

void dds_tkmap_free (_Inout_ _Post_invalid_ struct tkmap * map)
{
  ut_chhEnumUnsafe (map->m_hh, free_tkmap_instance, NULL);
  ut_chhFree (map->m_hh);
  os_condDestroy (&map->m_cond);
  os_mutexDestroy (&map->m_lock);
  dds_free (map);
}

uint64_t dds_tkmap_lookup (_In_ struct tkmap * map, _In_ const struct serdata * sd)
{
  struct tkmap_instance dummy;
  struct tkmap_instance * tk;
  dummy.m_sample = (struct serdata *) sd;
  tk = ut_chhLookup (map->m_hh, &dummy);
  return (tk) ? tk->m_iid : DDS_HANDLE_NIL;
}

typedef struct
{
  uint64_t m_iid;
  void * m_sample;
  bool m_ret;
}
tkmap_get_key_arg;

static void dds_tkmap_get_key_fn (void * vtk, void * varg)
{
  struct tkmap_instance * tk = vtk;
  tkmap_get_key_arg * arg = (tkmap_get_key_arg*) varg;
  if (tk->m_iid == arg->m_iid)
  {
    deserialize_into (arg->m_sample, tk->m_sample);
    arg->m_ret = true;
  }
}

_Check_return_
bool dds_tkmap_get_key (_In_ struct tkmap * map, _In_ uint64_t iid, _Out_ void * sample)
{
  tkmap_get_key_arg arg = { iid, sample, false };
  os_mutexLock (&map->m_lock);
  ut_chhEnumUnsafe (map->m_hh, dds_tkmap_get_key_fn, &arg);
  os_mutexUnlock (&map->m_lock);
  return arg.m_ret;
}

typedef struct
{
  uint64_t m_iid;
  struct tkmap_instance * m_inst;
}
tkmap_get_inst_arg;

static void dds_tkmap_get_inst_fn (void * vtk, void * varg)
{
  struct tkmap_instance * tk = vtk;
  tkmap_get_inst_arg * arg = (tkmap_get_inst_arg*) varg;
  if (tk->m_iid == arg->m_iid)
  {
    arg->m_inst = tk;
  }
}

_Check_return_
struct tkmap_instance * dds_tkmap_find_by_id (_In_ struct tkmap * map, _In_ uint64_t iid)
{
  tkmap_get_inst_arg arg = { iid, NULL };
  ut_chhEnumUnsafe (map->m_hh, dds_tkmap_get_inst_fn, &arg);
  return arg.m_inst;
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
struct tkmap_instance * dds_tkmap_find(
        _In_opt_ const struct dds_topic * topic,
        _In_ struct serdata * sd,
        _In_ const bool rd,
        _In_ const bool create)
{
  struct tkmap_instance dummy;
  struct tkmap_instance * tk;
  struct tkmap * map = gv.m_tkmap;

  dummy.m_sample = sd;

  /* Generate key hash if required and not provided */

  if (topic && topic->m_descriptor->m_nkeys)
  {
    if ((sd->v.keyhash.m_flags & DDS_KEY_HASH_SET) == 0)
    {
      dds_stream_t is;
      dds_stream_from_serstate (&is, sd->v.st);
      dds_stream_read_keyhash (&is, &sd->v.keyhash, topic->m_descriptor, sd->v.st->kind == STK_KEY);
    }
    else
    {
      if (topic->m_descriptor->m_flagset & DDS_TOPIC_FIXED_KEY)
      {
        sd->v.keyhash.m_flags |= DDS_KEY_IS_HASH;
      }

#if DDS_DEBUG_KEYHASH

      {
        dds_stream_t is;
        dds_key_hash_t kh;

        /* Check that we generate same keyhash as provided */

        memset (&kh, 0, sizeof (kh));
        dds_stream_from_serstate (&is, sd->v.st);
        dds_stream_read_keyhash (&is, &kh, topic->m_descriptor, sd->v.st->kind == STK_KEY);
        assert (memcmp (kh.m_hash, sd->v.keyhash.m_hash, 16) == 0);
        if (kh.m_key_buff_size)
        {
          dds_free (kh.m_key_buff);
        }
      }
#endif
    }
  }

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

    tk->m_sample = ddsi_serdata_ref (sd);
    tk->m_map = map;
    os_atomic_st32 (&tk->m_refc, 1);
    tk->m_iid = dds_iid_gen ();
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
    TRACE (("tk=%p iid=%"PRIx64"", &tk, tk->m_iid));
  }
  return tk;
}

_Check_return_
struct tkmap_instance * dds_tkmap_lookup_instance_ref (_In_ struct serdata * sd)
{
  dds_topic * topic = sd->v.st->topic ? sd->v.st->topic->status_cb_entity : NULL;

  assert (vtime_awake_p (lookup_thread_state ()->vtime));

#if 0
  /* Topic might have been deleted -- FIXME: no way the topic may be deleted when there're still users out there */
  if (topic == NULL)
  {
    return NULL;
  }
#endif

  return dds_tkmap_find (topic, sd, true, true);
}

void dds_tkmap_instance_ref (_In_ struct tkmap_instance *tk)
{
  os_atomic_inc32 (&tk->m_refc);
}

void dds_tkmap_instance_unref (_In_ struct tkmap_instance * tk)
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
    struct tkmap *map = tk->m_map;

    /* Remove from hash table */
    (void)ut_chhRemove(map->m_hh, tk);

    /* Signal any threads blocked in their retry loops in lookup */
    os_mutexLock(&map->m_lock);
    os_condBroadcast(&map->m_cond);
    os_mutexUnlock(&map->m_lock);

    /* Schedule freeing of memory until after all those who may have found a pointer have
     progressed to where they no longer hold that pointer */
    gc_tkmap_instance(tk);
  }
}
