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

#include "dds/ddsrt/attributes.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/hopscotch.h"

#define HH_HOP_RANGE 32
#define HH_ADD_RANGE 64

#define NOT_A_BUCKET (~(uint32_t)0)

/************* SEQUENTIAL VERSION ***************/

struct ddsrt_hh_bucket {
  uint32_t hopinfo;
  void *data;
};

struct ddsrt_hh {
  uint32_t size; /* power of 2 */
  struct ddsrt_hh_bucket *buckets;
  ddsrt_hh_hash_fn hash;
  ddsrt_hh_equals_fn equals;
};

static void ddsrt_hh_init (struct ddsrt_hh *rt, uint32_t init_size, ddsrt_hh_hash_fn hash, ddsrt_hh_equals_fn equals)
{
  uint32_t size;
  uint32_t i;
  /* degenerate case to minimize memory use */
  if (init_size == 1) {
    size = 1;
  } else {
    size = HH_HOP_RANGE;
    while (size < init_size) {
      size *= 2;
    }
  }
  rt->hash = hash;
  rt->equals = equals;
  rt->size = size;
  rt->buckets = ddsrt_malloc (size * sizeof (*rt->buckets));
  for (i = 0; i < size; i++) {
    rt->buckets[i].hopinfo = 0;
    rt->buckets[i].data = NULL;
  }
}

static void ddsrt_hh_fini (struct ddsrt_hh *rt)
{
  ddsrt_free (rt->buckets);
}

struct ddsrt_hh *ddsrt_hh_new (uint32_t init_size, ddsrt_hh_hash_fn hash, ddsrt_hh_equals_fn equals)
{
  struct ddsrt_hh *hh = ddsrt_malloc (sizeof (*hh));
  ddsrt_hh_init (hh, init_size, hash, equals);
  return hh;
}

void ddsrt_hh_free (struct ddsrt_hh * __restrict hh)
{
  ddsrt_hh_fini (hh);
  ddsrt_free (hh);
}

static void *ddsrt_hh_lookup_internal (const struct ddsrt_hh *rt, const uint32_t bucket, const void *keyobject)
{
  const uint32_t idxmask = rt->size - 1;
  uint32_t hopinfo = rt->buckets[bucket].hopinfo;
  uint32_t idx;
  for (idx = 0; hopinfo != 0; hopinfo >>= 1, idx++) {
    if (hopinfo & 1) {
      const uint32_t bidx = (bucket + idx) & idxmask;
      void *data = rt->buckets[bidx].data;
      if (data && rt->equals (data, keyobject))
        return data;
    }
  }
  return NULL;
}

void *ddsrt_hh_lookup (const struct ddsrt_hh * __restrict rt, const void * __restrict keyobject)
{
  const uint32_t hash = rt->hash (keyobject);
  const uint32_t idxmask = rt->size - 1;
  const uint32_t bucket = hash & idxmask;
  return ddsrt_hh_lookup_internal (rt, bucket, keyobject);
}

static uint32_t ddsrt_hh_find_closer_free_bucket (struct ddsrt_hh *rt, uint32_t free_bucket, uint32_t *free_distance)
{
  const uint32_t idxmask = rt->size - 1;
  uint32_t move_bucket, free_dist;
  move_bucket = (free_bucket - (HH_HOP_RANGE - 1)) & idxmask;
  for (free_dist = HH_HOP_RANGE - 1; free_dist > 0; free_dist--) {
    uint32_t move_free_distance = NOT_A_BUCKET;
    uint32_t mask = 1;
    uint32_t i;
    for (i = 0; i < free_dist; i++, mask <<= 1) {
      if (mask & rt->buckets[move_bucket].hopinfo) {
        move_free_distance = i;
        break;
      }
    }
    if (move_free_distance != NOT_A_BUCKET) {
      uint32_t new_free_bucket = (move_bucket + move_free_distance) & idxmask;
      rt->buckets[move_bucket].hopinfo |= 1u << free_dist;
      rt->buckets[free_bucket].data = rt->buckets[new_free_bucket].data;
      rt->buckets[new_free_bucket].data = NULL;
      rt->buckets[move_bucket].hopinfo &= ~(1u << move_free_distance);
      *free_distance -= free_dist - move_free_distance;
      return new_free_bucket;
    }
    move_bucket = (move_bucket + 1) & idxmask;
  }
  return NOT_A_BUCKET;
}

static void ddsrt_hh_resize (struct ddsrt_hh *rt)
{
  if (rt->size == 1) {
    assert (rt->size == 1);
    assert (rt->buckets[0].hopinfo == 1);
    assert (rt->buckets[0].data != NULL);

    rt->size = HH_HOP_RANGE;
    const uint32_t hash = rt->hash (rt->buckets[0].data);
    const uint32_t idxmask = rt->size - 1;
    const uint32_t start_bucket = hash & idxmask;

    struct ddsrt_hh_bucket *newbs = ddsrt_malloc (rt->size * sizeof (*newbs));
    for (uint32_t i = 0; i < rt->size; i++) {
      newbs[i].hopinfo = 0;
      newbs[i].data = NULL;
    }
    newbs[start_bucket] = rt->buckets[0];
    ddsrt_free (rt->buckets);
    rt->buckets = newbs;
  } else {
    struct ddsrt_hh_bucket *bs1;
    uint32_t i, idxmask0, idxmask1;

    bs1 = ddsrt_malloc (2 * rt->size * sizeof (*rt->buckets));

    for (i = 0; i < 2 * rt->size; i++) {
      bs1[i].hopinfo = 0;
      bs1[i].data = NULL;
    }
    idxmask0 = rt->size - 1;
    idxmask1 = 2 * rt->size - 1;
    for (i = 0; i < rt->size; i++) {
      void *data = rt->buckets[i].data;
      if (data) {
        const uint32_t hash = rt->hash (data);
        const uint32_t old_start_bucket = hash & idxmask0;
        const uint32_t new_start_bucket = hash & idxmask1;
        const uint32_t dist = (i >= old_start_bucket) ? (i - old_start_bucket) : (rt->size + i - old_start_bucket);
        const uint32_t newb = (new_start_bucket + dist) & idxmask1;
        assert (dist < HH_HOP_RANGE);
        bs1[new_start_bucket].hopinfo |= 1u << dist;
        bs1[newb].data = data;
      }
    }

    ddsrt_free (rt->buckets);
    rt->size *= 2;
    rt->buckets = bs1;
  }
}

int ddsrt_hh_add (struct ddsrt_hh * __restrict rt, void * __restrict data)
{
  const uint32_t hash = rt->hash (data);
  const uint32_t idxmask = rt->size - 1;
  const uint32_t start_bucket = hash & idxmask;
  uint32_t free_distance, free_bucket;

  if (ddsrt_hh_lookup_internal (rt, start_bucket, data)) {
    return 0;
  }

  free_bucket = start_bucket;
  for (free_distance = 0; free_distance < HH_ADD_RANGE; free_distance++) {
    if (rt->buckets[free_bucket].data == NULL)
      break;
    free_bucket = (free_bucket + 1) & idxmask;
  }
  if (free_distance < HH_ADD_RANGE) {
    do {
      if (free_distance < HH_HOP_RANGE) {
        assert ((uint32_t) free_bucket == ((start_bucket + free_distance) & idxmask));
        rt->buckets[start_bucket].hopinfo |= 1u << free_distance;
        rt->buckets[free_bucket].data = (void *) data;
        return 1;
      }
      free_bucket = ddsrt_hh_find_closer_free_bucket (rt, free_bucket, &free_distance);
      assert (free_bucket == NOT_A_BUCKET || free_bucket <= idxmask);
    } while (free_bucket != NOT_A_BUCKET);
  }

  ddsrt_hh_resize (rt);
  return ddsrt_hh_add (rt, data);
}

int ddsrt_hh_remove (struct ddsrt_hh * __restrict rt, const void * __restrict keyobject)
{
  const uint32_t hash = rt->hash (keyobject);
  const uint32_t idxmask = rt->size - 1;
  const uint32_t bucket = hash & idxmask;
  uint32_t hopinfo;
  uint32_t idx;
  hopinfo = rt->buckets[bucket].hopinfo;
  for (idx = 0; hopinfo != 0; hopinfo >>= 1, idx++) {
    if (hopinfo & 1) {
      const uint32_t bidx = (bucket + idx) & idxmask;
      void *data = rt->buckets[bidx].data;
      if (data && rt->equals (data, keyobject)) {
        rt->buckets[bidx].data = NULL;
        rt->buckets[bucket].hopinfo &= ~(1u << idx);
        return 1;
      }
    }
  }
  return 0;
}

void ddsrt_hh_add_absent (struct ddsrt_hh * __restrict rt, void * __restrict data)
{
  const int x = ddsrt_hh_add (rt, data);
  assert (x);
  (void) x;
}

void ddsrt_hh_remove_present (struct ddsrt_hh * __restrict rt, void * __restrict keyobject)
{
  const int x = ddsrt_hh_remove (rt, keyobject);
  assert (x);
  (void) x;
}

void ddsrt_hh_enum (struct ddsrt_hh * __restrict rt, void (*f) (void *a, void *f_arg), void *f_arg)
{
  uint32_t i;
  for (i = 0; i < rt->size; i++) {
    void *data = rt->buckets[i].data;
    if (data) {
      f (data, f_arg);
    }
  }
}

void *ddsrt_hh_iter_first (struct ddsrt_hh * __restrict rt, struct ddsrt_hh_iter * __restrict iter)
{
  iter->hh = rt;
  iter->cursor = 0;
  return ddsrt_hh_iter_next (iter);
}

void *ddsrt_hh_iter_next (struct ddsrt_hh_iter * __restrict iter)
{
  struct ddsrt_hh *rt = iter->hh;
  while (iter->cursor < rt->size) {
    void *data = rt->buckets[iter->cursor].data;
    iter->cursor++;
    if (data) {
      return data;
    }
  }
  return NULL;
}

/********** CONCURRENT VERSION ************/

struct ddsrt_chh_bucket {
    ddsrt_atomic_uint32_t hopinfo;
    ddsrt_atomic_uint32_t timestamp;
    ddsrt_atomic_voidp_t data;
};

struct ddsrt_chh_bucket_array {
    uint32_t size; /* power of 2 */
    struct ddsrt_chh_bucket bs[];
};

struct ddsrt_chh {
    ddsrt_atomic_voidp_t buckets; /* struct ddsrt_chh_bucket_array * */
    ddsrt_hh_hash_fn hash;
    ddsrt_hh_equals_fn equals;
    ddsrt_mutex_t change_lock;
    ddsrt_hh_buckets_gc_fn gc_buckets;
    void *gc_buckets_arg;
};

#define CHH_MAX_TRIES 4
#define CHH_BUSY ((void *) 1)

static int ddsrt_chh_data_valid_p (void *data)
{
    return data != NULL && data != CHH_BUSY;
}

static int ddsrt_chh_init (struct ddsrt_chh *rt, uint32_t init_size, ddsrt_hh_hash_fn hash, ddsrt_hh_equals_fn equals, ddsrt_hh_buckets_gc_fn gc_buckets, void *gc_buckets_arg)
{
    uint32_t size;
    uint32_t i;
    struct ddsrt_chh_bucket_array *buckets;

    size = HH_HOP_RANGE;
    while (size < init_size) {
        size *= 2;
    }
    rt->hash = hash;
    rt->equals = equals;
    rt->gc_buckets = gc_buckets;
    rt->gc_buckets_arg = gc_buckets_arg;

    buckets = ddsrt_malloc (offsetof (struct ddsrt_chh_bucket_array, bs) + size * sizeof (*buckets->bs));
    ddsrt_atomic_stvoidp (&rt->buckets, buckets);
    buckets->size = size;
    for (i = 0; i < size; i++) {
        struct ddsrt_chh_bucket *b = &buckets->bs[i];
        ddsrt_atomic_st32 (&b->hopinfo, 0);
        ddsrt_atomic_st32 (&b->timestamp, 0);
        ddsrt_atomic_stvoidp (&b->data, NULL);
    }
    ddsrt_mutex_init (&rt->change_lock);
    return 0;
}

static void ddsrt_chh_fini (struct ddsrt_chh *rt)
{
    ddsrt_free (ddsrt_atomic_ldvoidp (&rt->buckets));
    ddsrt_mutex_destroy (&rt->change_lock);
}

struct ddsrt_chh *ddsrt_chh_new (uint32_t init_size, ddsrt_hh_hash_fn hash, ddsrt_hh_equals_fn equals, ddsrt_hh_buckets_gc_fn gc_buckets, void *gc_buckets_arg)
{
    struct ddsrt_chh *hh = ddsrt_malloc (sizeof (*hh));
    if (ddsrt_chh_init (hh, init_size, hash, equals, gc_buckets, gc_buckets_arg) < 0) {
        ddsrt_free (hh);
        return NULL;
    } else {
        return hh;
    }
}

void ddsrt_chh_free (struct ddsrt_chh * __restrict hh)
{
    ddsrt_chh_fini (hh);
    ddsrt_free (hh);
}

static void *ddsrt_chh_lookup_internal (struct ddsrt_chh_bucket_array const * const bsary, ddsrt_hh_equals_fn equals, const uint32_t bucket, const void *keyobject)
{
    struct ddsrt_chh_bucket const * const bs = bsary->bs;
    const uint32_t idxmask = bsary->size - 1;
    uint32_t timestamp;
    int try_counter = 0;
    uint32_t idx;
    do {
        uint32_t hopinfo;
        timestamp = ddsrt_atomic_ld32 (&bs[bucket].timestamp);
        ddsrt_atomic_fence_ldld ();
        hopinfo = ddsrt_atomic_ld32 (&bs[bucket].hopinfo);
        for (idx = 0; hopinfo != 0; hopinfo >>= 1, idx++) {
            if (hopinfo & 1) {
                const uint32_t bidx = (bucket + idx) & idxmask;
                void *data = ddsrt_atomic_ldvoidp (&bs[bidx].data);
                if (ddsrt_chh_data_valid_p (data) && equals (data, keyobject)) {
                    return data;
                }
            }
        }
        ddsrt_atomic_fence_ldld ();
    } while (timestamp != ddsrt_atomic_ld32 (&bs[bucket].timestamp) && ++try_counter < CHH_MAX_TRIES);
    if (try_counter == CHH_MAX_TRIES) {
        /* Note: try_counter would not have been incremented to
           CHH_MAX_TRIES if we ended the loop because the two timestamps
           were equal, but this avoids loading the timestamp again */
        for (idx = 0; idx < HH_HOP_RANGE; idx++) {
            const uint32_t bidx = (bucket + idx) & idxmask;
            void *data = ddsrt_atomic_ldvoidp (&bs[bidx].data);
            if (ddsrt_chh_data_valid_p (data) && equals (data, keyobject)) {
                return data;
            }
        }
    }
    return NULL;
}

#define ddsrt_atomic_rmw32_nonatomic(var_, tmp_, expr_) do {                 \
        ddsrt_atomic_uint32_t *var__ = (var_);                               \
        uint32_t tmp_ = ddsrt_atomic_ld32 (var__);                           \
        ddsrt_atomic_st32 (var__, (expr_));                                  \
    } while (0)

void *ddsrt_chh_lookup (struct ddsrt_chh * __restrict rt, const void * __restrict keyobject)
{
    struct ddsrt_chh_bucket_array const * const bsary = ddsrt_atomic_ldvoidp (&rt->buckets);
    const uint32_t hash = rt->hash (keyobject);
    const uint32_t idxmask = bsary->size - 1;
    const uint32_t bucket = hash & idxmask;
    return ddsrt_chh_lookup_internal (bsary, rt->equals, bucket, keyobject);
}

static uint32_t ddsrt_chh_find_closer_free_bucket (struct ddsrt_chh *rt, uint32_t free_bucket, uint32_t *free_distance)
{
    struct ddsrt_chh_bucket_array * const bsary = ddsrt_atomic_ldvoidp (&rt->buckets);
    struct ddsrt_chh_bucket * const bs = bsary->bs;
    const uint32_t idxmask = bsary->size - 1;
    uint32_t move_bucket, free_dist;
    move_bucket = (free_bucket - (HH_HOP_RANGE - 1)) & idxmask;
    for (free_dist = HH_HOP_RANGE - 1; free_dist > 0; free_dist--) {
        uint32_t start_hop_info = ddsrt_atomic_ld32 (&bs[move_bucket].hopinfo);
        uint32_t move_free_distance = NOT_A_BUCKET;
        uint32_t mask = 1;
        uint32_t i;
        for (i = 0; i < free_dist; i++, mask <<= 1) {
            if (mask & start_hop_info) {
                move_free_distance = i;
                break;
            }
        }
        if (move_free_distance != NOT_A_BUCKET) {
            if (start_hop_info == ddsrt_atomic_ld32 (&bs[move_bucket].hopinfo))
            {
                uint32_t new_free_bucket = (move_bucket + move_free_distance) & idxmask;
                ddsrt_atomic_rmw32_nonatomic (&bs[move_bucket].hopinfo, x, x | (1u << free_dist));
                ddsrt_atomic_stvoidp (&bs[free_bucket].data, ddsrt_atomic_ldvoidp (&bs[new_free_bucket].data));
                ddsrt_atomic_rmw32_nonatomic (&bs[move_bucket].timestamp, x, x + 1);
                ddsrt_atomic_fence ();
                ddsrt_atomic_stvoidp (&bs[new_free_bucket].data, CHH_BUSY);
                ddsrt_atomic_rmw32_nonatomic (&bs[move_bucket].hopinfo, x, x & ~(1u << move_free_distance));

                *free_distance -= free_dist - move_free_distance;
                return new_free_bucket;
            }
        }
        move_bucket = (move_bucket + 1) & idxmask;
    }
    return NOT_A_BUCKET;
}

static void ddsrt_chh_resize (struct ddsrt_chh *rt)
{
    /* doubles the size => bucket index gains one bit at the msb =>
       start bucket is unchanged or moved into the added half of the set
       => those for which the (new) msb is 0 are guaranteed to fit, and
       so are those for which the (new) msb is 1 => never have to resize
       recursively */
    struct ddsrt_chh_bucket_array * const bsary0 = ddsrt_atomic_ldvoidp (&rt->buckets);
    struct ddsrt_chh_bucket * const bs0 = bsary0->bs;
    struct ddsrt_chh_bucket_array *bsary1;
    struct ddsrt_chh_bucket *bs1;
    uint32_t i, idxmask0, idxmask1;

    assert (bsary0->size > 0);
    bsary1 = ddsrt_malloc (offsetof (struct ddsrt_chh_bucket_array, bs) + 2 * bsary0->size * sizeof (*bsary1->bs));
    bsary1->size = 2 * bsary0->size;
    bs1 = bsary1->bs;

    for (i = 0; i < bsary1->size; i++) {
        ddsrt_atomic_st32 (&bs1[i].hopinfo, 0);
        ddsrt_atomic_st32 (&bs1[i].timestamp, 0);
        ddsrt_atomic_stvoidp (&bs1[i].data, NULL);
    }
    idxmask0 = bsary0->size - 1;
    idxmask1 = bsary1->size - 1;
    for (i = 0; i < bsary0->size; i++) {
        void *data = ddsrt_atomic_ldvoidp (&bs0[i].data);
        if (data && data != CHH_BUSY) {
            const uint32_t hash = rt->hash (data);
            const uint32_t old_start_bucket = hash & idxmask0;
            const uint32_t new_start_bucket = hash & idxmask1;
            const uint32_t dist = (i >= old_start_bucket) ? (i - old_start_bucket) : (bsary0->size + i - old_start_bucket);
            const uint32_t newb = (new_start_bucket + dist) & idxmask1;
            assert (dist < HH_HOP_RANGE);
            ddsrt_atomic_rmw32_nonatomic (&bs1[new_start_bucket].hopinfo, x, x | (1u << dist));
            ddsrt_atomic_stvoidp (&bs1[newb].data, data);
        }
    }

    ddsrt_atomic_fence ();
    ddsrt_atomic_stvoidp (&rt->buckets, bsary1);
    rt->gc_buckets (bsary0, rt->gc_buckets_arg);
}

static int ddsrt_chh_add_locked (struct ddsrt_chh * __restrict rt, const void * __restrict data)
{
    const uint32_t hash = rt->hash (data);
    uint32_t size;

    {
        struct ddsrt_chh_bucket_array * const bsary = ddsrt_atomic_ldvoidp (&rt->buckets);
        struct ddsrt_chh_bucket * const bs = bsary->bs;
        uint32_t idxmask;
        uint32_t start_bucket, free_distance, free_bucket;

        size = bsary->size;
        idxmask = size - 1;
        start_bucket = hash & idxmask;

        if (ddsrt_chh_lookup_internal (bsary, rt->equals, start_bucket, data)) {
            return 0;
        }

        free_bucket = start_bucket;
        for (free_distance = 0; free_distance < HH_ADD_RANGE; free_distance++) {
            if (ddsrt_atomic_ldvoidp (&bs[free_bucket].data) == NULL &&
                ddsrt_atomic_casvoidp (&bs[free_bucket].data, NULL, CHH_BUSY)) {
                break;
            }
            free_bucket = (free_bucket + 1) & idxmask;
        }
        if (free_distance < HH_ADD_RANGE) {
            do {
                if (free_distance < HH_HOP_RANGE) {
                    assert (free_bucket == ((start_bucket + free_distance) & idxmask));
                    ddsrt_atomic_rmw32_nonatomic (&bs[start_bucket].hopinfo, x, x | (1u << free_distance));
                    ddsrt_atomic_fence ();
                    ddsrt_atomic_stvoidp (&bs[free_bucket].data, (void *) data);
                    return 1;
                }
                free_bucket = ddsrt_chh_find_closer_free_bucket (rt, free_bucket, &free_distance);
                assert (free_bucket == NOT_A_BUCKET || free_bucket <= idxmask);
            } while (free_bucket != NOT_A_BUCKET);
        }
    }

    ddsrt_chh_resize (rt);
    return ddsrt_chh_add_locked (rt, data);
}

int ddsrt_chh_add (struct ddsrt_chh * __restrict rt, void * __restrict data)
{
    ddsrt_mutex_lock (&rt->change_lock);
    const int ret = ddsrt_chh_add_locked (rt, data);
    ddsrt_mutex_unlock (&rt->change_lock);
    return ret;
}

int ddsrt_chh_remove (struct ddsrt_chh * __restrict rt, const void * __restrict keyobject)
{
    const uint32_t hash = rt->hash (keyobject);
    ddsrt_mutex_lock (&rt->change_lock);

    {
        struct ddsrt_chh_bucket_array * const bsary = ddsrt_atomic_ldvoidp (&rt->buckets);
        struct ddsrt_chh_bucket * const bs = bsary->bs;
        const uint32_t size = bsary->size;
        const uint32_t idxmask = size - 1;
        const uint32_t bucket = hash & idxmask;
        uint32_t hopinfo;
        uint32_t idx;
        hopinfo = ddsrt_atomic_ld32 (&bs[bucket].hopinfo);
        for (idx = 0; hopinfo != 0; hopinfo >>= 1, idx++) {
            if (hopinfo & 1) {
                const uint32_t bidx = (bucket + idx) & idxmask;
                void *data = ddsrt_atomic_ldvoidp (&bs[bidx].data);
                if (ddsrt_chh_data_valid_p (data) && rt->equals (data, keyobject)) {
                    ddsrt_atomic_stvoidp (&bs[bidx].data, NULL);
                    ddsrt_atomic_rmw32_nonatomic (&bs[bucket].hopinfo, x, x & ~(1u << idx));
                    ddsrt_mutex_unlock (&rt->change_lock);
                    return 1;
                }
            }
        }
    }

    ddsrt_mutex_unlock (&rt->change_lock);
    return 0;
}

void ddsrt_chh_enum_unsafe (struct ddsrt_chh * __restrict rt, void (*f) (void *a, void *f_arg), void *f_arg)
{
    struct ddsrt_chh_bucket_array * const bsary = ddsrt_atomic_ldvoidp (&rt->buckets);
    struct ddsrt_chh_bucket * const bs = bsary->bs;
    uint32_t i;
    for (i = 0; i < bsary->size; i++) {
        void *data = ddsrt_atomic_ldvoidp (&bs[i].data);
        if (data && data != CHH_BUSY) {
            f (data, f_arg);
        }
    }
}

void *ddsrt_chh_iter_next (struct ddsrt_chh_iter *it)
{
  uint32_t i;
  for (i = it->cursor; i < it->size; i++) {
    void *data = ddsrt_atomic_ldvoidp (&it->bs[i].data);
    if (data && data != CHH_BUSY) {
      it->cursor = i+1;
      return data;
    }
  }
  return NULL;
}

void *ddsrt_chh_iter_first (struct ddsrt_chh * __restrict rt, struct ddsrt_chh_iter *it)
{
  struct ddsrt_chh_bucket_array * const bsary = ddsrt_atomic_ldvoidp (&rt->buckets);
  it->bs = bsary->bs;
  it->size = bsary->size;
  it->cursor = 0;
  return ddsrt_chh_iter_next (it);
}

/************* SEQUENTIAL VERSION WITH EMBEDDED DATA ***************/

struct ddsrt_ehh_bucket {
    uint32_t hopinfo;
    uint32_t inuse;
    char data[];
};

struct ddsrt_ehh {
    uint32_t size; /* power of 2 */
    size_t elemsz;
    size_t bucketsz;
    char *buckets; /* ehhBucket, but embedded data messes up the layout */
    ddsrt_hh_hash_fn hash;
    ddsrt_hh_equals_fn equals;
};

static void ddsrt_ehh_init (struct ddsrt_ehh *rt, size_t elemsz, uint32_t init_size, ddsrt_hh_hash_fn hash, ddsrt_hh_equals_fn equals)
{
    uint32_t size = HH_HOP_RANGE;
    uint32_t i;
    while (size < init_size) {
        size *= 2;
    }
    rt->hash = hash;
    rt->equals = equals;
    rt->size = size;
    rt->elemsz = elemsz;
    rt->bucketsz = sizeof (struct ddsrt_ehh_bucket) + ((elemsz+7) & ~(size_t)7);
    rt->buckets = ddsrt_malloc (size * rt->bucketsz);
    for (i = 0; i < size; i++) {
        struct ddsrt_ehh_bucket *b = (struct ddsrt_ehh_bucket *) (rt->buckets + i * rt->bucketsz);
        b->hopinfo = 0;
        b->inuse = 0;
    }
}

static void ddsrt_ehh_fini (struct ddsrt_ehh *rt)
{
    ddsrt_free (rt->buckets);
}

struct ddsrt_ehh *ddsrt_ehh_new (size_t elemsz, uint32_t init_size, ddsrt_hh_hash_fn hash, ddsrt_hh_equals_fn equals)
{
    struct ddsrt_ehh *hh = ddsrt_malloc (sizeof (*hh));
    ddsrt_ehh_init (hh, elemsz, init_size, hash, equals);
    return hh;
}

void ddsrt_ehh_free (struct ddsrt_ehh * __restrict hh)
{
    ddsrt_ehh_fini (hh);
    ddsrt_free (hh);
}

static void *ddsrt_ehh_lookup_internal (const struct ddsrt_ehh *rt, uint32_t bucket, const void *keyobject)
{
    const struct ddsrt_ehh_bucket *b = (const struct ddsrt_ehh_bucket *) (rt->buckets + bucket * rt->bucketsz);
    uint32_t hopinfo = b->hopinfo;

    if (hopinfo & 1) {
        if (b->inuse && rt->equals (b->data, keyobject)) {
            return (void *) b->data;
        }
    }

    do {
        hopinfo >>= 1;
        if (++bucket == rt->size) {
            bucket = 0;
        }
        if (hopinfo & 1) {
            b = (const struct ddsrt_ehh_bucket *) (rt->buckets + bucket * rt->bucketsz);
            if (b->inuse && rt->equals (b->data, keyobject)) {
                return (void *) b->data;
            }
        }
    } while (hopinfo != 0);
    return NULL;
}

void *ddsrt_ehh_lookup (const struct ddsrt_ehh * __restrict rt, const void * __restrict keyobject)
{
    const uint32_t hash = rt->hash (keyobject);
    const uint32_t idxmask = rt->size - 1;
    const uint32_t bucket = hash & idxmask;
    return ddsrt_ehh_lookup_internal (rt, bucket, keyobject);
}

static uint32_t ddsrt_ehh_find_closer_free_bucket (struct ddsrt_ehh *rt, uint32_t free_bucket, uint32_t *free_distance)
{
    const uint32_t idxmask = rt->size - 1;
    uint32_t move_bucket, free_dist;
    move_bucket = (free_bucket - (HH_HOP_RANGE - 1)) & idxmask;
    for (free_dist = HH_HOP_RANGE - 1; free_dist > 0; free_dist--) {
        struct ddsrt_ehh_bucket * const mb = (struct ddsrt_ehh_bucket *) (rt->buckets + move_bucket * rt->bucketsz);
        uint32_t move_free_distance = NOT_A_BUCKET;
        uint32_t mask = 1;
        uint32_t i;
        for (i = 0; i < free_dist; i++, mask <<= 1) {
            if (mask & mb->hopinfo) {
                move_free_distance = i;
                break;
            }
        }
        if (move_free_distance != NOT_A_BUCKET) {
            uint32_t new_free_bucket = (move_bucket + move_free_distance) & idxmask;
            struct ddsrt_ehh_bucket * const fb = (struct ddsrt_ehh_bucket *) (rt->buckets + free_bucket * rt->bucketsz);
            struct ddsrt_ehh_bucket * const nfb = (struct ddsrt_ehh_bucket *) (rt->buckets + new_free_bucket * rt->bucketsz);
            mb->hopinfo |= 1u << free_dist;
            fb->inuse = 1;
            memcpy (fb->data, nfb->data, rt->elemsz);
            nfb->inuse = 0;
            mb->hopinfo &= ~(1u << move_free_distance);
            *free_distance -= free_dist - move_free_distance;
            return new_free_bucket;
        }
        move_bucket = (move_bucket + 1) & idxmask;
    }
    return NOT_A_BUCKET;
}

static void ddsrt_ehh_resize (struct ddsrt_ehh *rt)
{
    char *bs1;
    uint32_t i, idxmask0, idxmask1;

    bs1 = ddsrt_malloc (2 * rt->size * rt->bucketsz);

    for (i = 0; i < 2 * rt->size; i++) {
        struct ddsrt_ehh_bucket *b = (struct ddsrt_ehh_bucket *) (bs1 + i * rt->bucketsz);
        b->hopinfo = 0;
        b->inuse = 0;
    }
    idxmask0 = rt->size - 1;
    idxmask1 = 2 * rt->size - 1;
    for (i = 0; i < rt->size; i++) {
        struct ddsrt_ehh_bucket const * const b = (struct ddsrt_ehh_bucket *) (rt->buckets + i * rt->bucketsz);
        if (b->inuse) {
            const uint32_t hash = rt->hash (b->data);
            const uint32_t old_start_bucket = hash & idxmask0;
            const uint32_t new_start_bucket = hash & idxmask1;
            const uint32_t dist = (i >= old_start_bucket) ? (i - old_start_bucket) : (rt->size + i - old_start_bucket);
            const uint32_t newb = (new_start_bucket + dist) & idxmask1;
            struct ddsrt_ehh_bucket * const nsb = (struct ddsrt_ehh_bucket *) (bs1 + new_start_bucket * rt->bucketsz);
            struct ddsrt_ehh_bucket * const nb = (struct ddsrt_ehh_bucket *) (bs1 + newb * rt->bucketsz);
            assert (dist < HH_HOP_RANGE);
            assert (!nb->inuse);
            nsb->hopinfo |= 1u << dist;
            nb->inuse = 1;
            memcpy (nb->data, b->data, rt->elemsz);
        }
    }

    ddsrt_free (rt->buckets);
    rt->size *= 2;
    rt->buckets = bs1;
}

int ddsrt_ehh_add (struct ddsrt_ehh * __restrict rt, const void * __restrict data)
{
    const uint32_t hash = rt->hash (data);
    const uint32_t idxmask = rt->size - 1;
    const uint32_t start_bucket = hash & idxmask;
    uint32_t free_distance, free_bucket;

    if (ddsrt_ehh_lookup_internal (rt, start_bucket, data)) {
        return 0;
    }

    free_bucket = start_bucket;
    for (free_distance = 0; free_distance < HH_ADD_RANGE; free_distance++) {
        struct ddsrt_ehh_bucket const * const fb = (struct ddsrt_ehh_bucket *) (rt->buckets + free_bucket * rt->bucketsz);
        if (!fb->inuse) {
            break;
        }
        free_bucket = (free_bucket + 1) & idxmask;
    }
    if (free_distance < HH_ADD_RANGE) {
        do {
            if (free_distance < HH_HOP_RANGE) {
                struct ddsrt_ehh_bucket * const sb = (struct ddsrt_ehh_bucket *) (rt->buckets + start_bucket * rt->bucketsz);
                struct ddsrt_ehh_bucket * const fb = (struct ddsrt_ehh_bucket *) (rt->buckets + free_bucket * rt->bucketsz);
                assert ((uint32_t) free_bucket == ((start_bucket + free_distance) & idxmask));
                sb->hopinfo |= 1u << free_distance;
                fb->inuse = 1;
                memcpy (fb->data, data, rt->elemsz);
                assert (ddsrt_ehh_lookup_internal (rt, start_bucket, data));
                return 1;
            }
            free_bucket = ddsrt_ehh_find_closer_free_bucket (rt, free_bucket, &free_distance);
            assert (free_bucket == NOT_A_BUCKET || free_bucket <= idxmask);
        } while (free_bucket != NOT_A_BUCKET);
    }

    ddsrt_ehh_resize (rt);
    return ddsrt_ehh_add (rt, data);
}

int ddsrt_ehh_remove (struct ddsrt_ehh * __restrict rt, const void * __restrict keyobject)
{
    const uint32_t hash = rt->hash (keyobject);
    const uint32_t idxmask = rt->size - 1;
    const uint32_t bucket = hash & idxmask;
    uint32_t hopinfo;
    struct ddsrt_ehh_bucket *sb;
    uint32_t idx;
    sb = (struct ddsrt_ehh_bucket *) (rt->buckets + bucket * rt->bucketsz);
    hopinfo = sb->hopinfo;
    for (idx = 0; hopinfo != 0; hopinfo >>= 1, idx++) {
        if (hopinfo & 1) {
            const uint32_t bidx = (bucket + idx) & idxmask;
            struct ddsrt_ehh_bucket *b = (struct ddsrt_ehh_bucket *) (rt->buckets + bidx * rt->bucketsz);
            if (b->inuse && rt->equals (b->data, keyobject)) {
                assert (ddsrt_ehh_lookup_internal(rt, bucket, keyobject));
                b->inuse = 0;
                sb->hopinfo &= ~(1u << idx);
                return 1;
            }
        }
    }
    assert (!ddsrt_ehh_lookup_internal(rt, bucket, keyobject));
    return 0;
}

void ddsrt_ehh_enum (struct ddsrt_ehh * __restrict rt, void (*f) (void *a, void *f_arg), void *f_arg)
{
    uint32_t i;
    for (i = 0; i < rt->size; i++) {
        struct ddsrt_ehh_bucket *b = (struct ddsrt_ehh_bucket *) (rt->buckets + i * rt->bucketsz);
        if (b->inuse) {
            f (b->data, f_arg);
        }
    }
}

void *ddsrt_ehh_iter_first (struct ddsrt_ehh * __restrict rt, struct ddsrt_ehh_iter * __restrict iter)
{
    iter->hh = rt;
    iter->cursor = 0;
    return ddsrt_ehh_iter_next (iter);
}

void *ddsrt_ehh_iter_next (struct ddsrt_ehh_iter * __restrict iter)
{
    struct ddsrt_ehh *rt = iter->hh;
    while (iter->cursor < rt->size) {
        struct ddsrt_ehh_bucket *b = (struct ddsrt_ehh_bucket *) (rt->buckets + iter->cursor * rt->bucketsz);
        iter->cursor++;
        if (b->inuse) {
            return b->data;
        }
    }
    return NULL;
}
