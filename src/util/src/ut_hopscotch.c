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

#include "os/os.h"
#include "util/ut_hopscotch.h"

#define HH_HOP_RANGE 32
#define HH_ADD_RANGE 64

#define NOT_A_BUCKET (~(uint32_t)0)

/********** CONCURRENT VERSION ************/

#define N_BACKING_LOCKS 32
#define N_RESIZE_LOCKS 8

struct ut_chhBucket {
    os_atomic_uint32_t hopinfo;
    os_atomic_uint32_t timestamp;
    os_atomic_uint32_t lock;
    os_atomic_voidp_t data;
};

struct _Struct_size_bytes_(size) ut_chhBucketArray {
    uint32_t size; /* power of 2 */
    struct ut_chhBucket bs[];
};

struct ut_chhBackingLock {
    os_mutex lock;
    os_cond cv;
};

struct ut_chh {
    os_atomic_voidp_t buckets; /* struct ut_chhBucketArray * */
    struct ut_chhBackingLock backingLocks[N_BACKING_LOCKS];
    ut_hhHash_fn hash;
    ut_hhEquals_fn equals;
    os_rwlock resize_locks[N_RESIZE_LOCKS];
    ut_hhBucketsGc_fn gc_buckets;
};

#define CHH_MAX_TRIES 4
#define CHH_BUSY ((void *) 1)

static int ut_chhDataValid_p (void *data)
{
    return data != NULL && data != CHH_BUSY;
}

static int ut_chhInit (struct ut_chh *rt, uint32_t init_size, ut_hhHash_fn hash, ut_hhEquals_fn equals, ut_hhBucketsGc_fn gc_buckets)
{
    uint32_t size;
    uint32_t i;
    struct ut_chhBucketArray *buckets;

    size = HH_HOP_RANGE;
    while (size < init_size) {
        size *= 2;
    }
    rt->hash = hash;
    rt->equals = equals;
    rt->gc_buckets = gc_buckets;
    _Analysis_assume_(size >= HH_HOP_RANGE);
    buckets = os_malloc (offsetof (struct ut_chhBucketArray, bs) + size * sizeof (*buckets->bs));
    os_atomic_stvoidp (&rt->buckets, buckets);
    buckets->size = size;
    for (i = 0; i < size; i++) {
        struct ut_chhBucket *b = &buckets->bs[i];
        os_atomic_st32 (&b->hopinfo, 0);
        os_atomic_st32 (&b->timestamp, 0);
        os_atomic_st32 (&b->lock, 0);
        os_atomic_stvoidp (&b->data, NULL);
    }

    for (i = 0; i < N_BACKING_LOCKS; i++) {
        struct ut_chhBackingLock *s = &rt->backingLocks[i];
        os_mutexInit (&s->lock);
        os_condInit (&s->cv, &s->lock);
    }
    for (i = 0; i < N_RESIZE_LOCKS; i++) {
        os_rwlockInit (&rt->resize_locks[i]);
    }
    return 0;
}

static void ut_chhFini (struct ut_chh *rt)
{
    int i;
    os_free (os_atomic_ldvoidp (&rt->buckets));
    for (i = 0; i < N_BACKING_LOCKS; i++) {
        struct ut_chhBackingLock *s = &rt->backingLocks[i];
        os_condDestroy (&s->cv);
        os_mutexDestroy (&s->lock);
    }
    for (i = 0; i < N_RESIZE_LOCKS; i++) {
        os_rwlockDestroy (&rt->resize_locks[i]);
    }
}

struct ut_chh *ut_chhNew (uint32_t init_size, ut_hhHash_fn hash, ut_hhEquals_fn equals, ut_hhBucketsGc_fn gc_buckets)
{
    struct ut_chh *hh = os_malloc (sizeof (*hh));
    if (ut_chhInit (hh, init_size, hash, equals, gc_buckets) < 0) {
        os_free (hh);
        return NULL;
    } else {
        return hh;
    }
}

void ut_chhFree (struct ut_chh * UT_HH_RESTRICT hh)
{
    ut_chhFini (hh);
    os_free (hh);
}

#define LOCKBIT ((uint32_t)1 << 31)

static void ut_chhLockBucket (struct ut_chh *rt, uint32_t bidx)
{
    /* Lock: MSB <=> LOCKBIT, LSBs <=> wait count; note that
       (o&LOCKBIT)==0 means a thread can sneak in when there are
       already waiters, changing it to o==0 would avoid that. */
    struct ut_chhBucketArray * const bsary = os_atomic_ldvoidp (&rt->buckets);
    struct ut_chhBucket * const b = &bsary->bs[bidx];
    struct ut_chhBackingLock * const s = &rt->backingLocks[bidx % N_BACKING_LOCKS];
    uint32_t o, n;
 fastpath_retry:
    o = os_atomic_ld32 (&b->lock);
    if ((o & LOCKBIT) == 0) {
        n = o | LOCKBIT;
    } else {
        n = o + 1;
    }
    if (!os_atomic_cas32 (&b->lock, o, n)) {
        goto fastpath_retry;
    }
    if ((o & LOCKBIT) == 0) {
        os_atomic_fence ();
    } else {
        os_mutexLock (&s->lock);
        do {
            while ((o = os_atomic_ld32 (&b->lock)) & LOCKBIT) {
                os_condWait (&s->cv, &s->lock);
            }
        } while (!os_atomic_cas32 (&b->lock, o, (o - 1) | LOCKBIT));
        os_mutexUnlock (&s->lock);
    }
}

static void ut_chhUnlockBucket (struct ut_chh *rt, uint32_t bidx)
{
    struct ut_chhBucketArray * const bsary = os_atomic_ldvoidp (&rt->buckets);
    struct ut_chhBucket * const b = &bsary->bs[bidx];
    struct ut_chhBackingLock * const s = &rt->backingLocks[bidx % N_BACKING_LOCKS];
    uint32_t o, n;
 retry:
    o = os_atomic_ld32 (&b->lock);
    assert (o & LOCKBIT);
    n = o & ~LOCKBIT;
    if (!os_atomic_cas32 (&b->lock, o, n)) {
        goto retry;
    }
    if (n == 0) {
        os_atomic_fence ();
    } else {
        os_mutexLock (&s->lock);
        /* Need to broadcast because the CV is shared by multiple buckets
           and the kernel wakes an arbitrary thread, it may be a thread
           waiting for another bucket's lock that gets woken up, and that
           can result in all threads waiting with all locks unlocked.
           Broadcast avoids that, and with significantly more CVs than
           cores, it shouldn't happen often. */
        os_condBroadcast (&s->cv);
        os_mutexUnlock (&s->lock);
    }
}

static void *ut_chhLookupInternal (struct ut_chhBucketArray const * const bsary, ut_hhEquals_fn equals, const uint32_t bucket, const void *template)
{
    struct ut_chhBucket const * const bs = bsary->bs;
    const uint32_t idxmask = bsary->size - 1;
    uint32_t timestamp;
    int try_counter = 0;
    uint32_t idx;
    do {
        uint32_t hopinfo;
        timestamp = os_atomic_ld32 (&bs[bucket].timestamp);
        os_atomic_fence_ldld ();
        hopinfo = os_atomic_ld32 (&bs[bucket].hopinfo);
        for (idx = 0; hopinfo != 0; hopinfo >>= 1, idx++) {
            const uint32_t bidx = (bucket + idx) & idxmask;
            void *data = os_atomic_ldvoidp (&bs[bidx].data);
            if (ut_chhDataValid_p (data) && equals (data, template)) {
                return data;
            }
        }
        os_atomic_fence_ldld ();
    } while (timestamp != os_atomic_ld32 (&bs[bucket].timestamp) && ++try_counter < CHH_MAX_TRIES);
    if (try_counter == CHH_MAX_TRIES) {
        /* Note: try_counter would not have been incremented to
           CHH_MAX_TRIES if we ended the loop because the two timestamps
           were equal, but this avoids loading the timestamp again */
        for (idx = 0; idx < HH_HOP_RANGE; idx++) {
            const uint32_t bidx = (bucket + idx) & idxmask;
            void *data = os_atomic_ldvoidp (&bs[bidx].data);
            if (ut_chhDataValid_p (data) && equals (data, template)) {
                return data;
            }
        }
    }
    return NULL;
}

#define os_atomic_rmw32_nonatomic(var_, tmp_, expr_) do {                 \
        os_atomic_uint32_t *var__ = (var_);                               \
        uint32_t tmp_ = os_atomic_ld32 (var__);                          \
        os_atomic_st32 (var__, (expr_));                                  \
    } while (0)

void *ut_chhLookup (struct ut_chh * UT_HH_RESTRICT rt, const void * UT_HH_RESTRICT template)
{
    struct ut_chhBucketArray const * const bsary = os_atomic_ldvoidp (&rt->buckets);
    const uint32_t hash = rt->hash (template);
    const uint32_t idxmask = bsary->size - 1;
    const uint32_t bucket = hash & idxmask;
    return ut_chhLookupInternal (bsary, rt->equals, bucket, template);
}

static uint32_t ut_chhFindCloserFreeBucket (struct ut_chh *rt, uint32_t free_bucket, uint32_t *free_distance)
{
    struct ut_chhBucketArray * const bsary = os_atomic_ldvoidp (&rt->buckets);
    struct ut_chhBucket * const bs = bsary->bs;
    const uint32_t idxmask = bsary->size - 1;
    uint32_t move_bucket, free_dist;
    move_bucket = (free_bucket - (HH_HOP_RANGE - 1)) & idxmask;
    for (free_dist = HH_HOP_RANGE - 1; free_dist > 0; free_dist--) {
        uint32_t start_hop_info = os_atomic_ld32 (&bs[move_bucket].hopinfo);
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
            ut_chhLockBucket (rt, move_bucket);
            if (start_hop_info == os_atomic_ld32 (&bs[move_bucket].hopinfo))
            {
                uint32_t new_free_bucket = (move_bucket + move_free_distance) & idxmask;
                os_atomic_rmw32_nonatomic (&bs[move_bucket].hopinfo, x, x | (1u << free_dist));
                os_atomic_stvoidp (&bs[free_bucket].data, os_atomic_ldvoidp (&bs[new_free_bucket].data));
                os_atomic_rmw32_nonatomic (&bs[move_bucket].timestamp, x, x + 1);
                os_atomic_fence ();
                os_atomic_stvoidp (&bs[new_free_bucket].data, CHH_BUSY);
                os_atomic_rmw32_nonatomic (&bs[move_bucket].hopinfo, x, x & ~(1u << move_free_distance));

                *free_distance -= free_dist - move_free_distance;
                ut_chhUnlockBucket (rt, move_bucket);
                return new_free_bucket;
            }
            ut_chhUnlockBucket (rt, move_bucket);
        }
        move_bucket = (move_bucket + 1) & idxmask;
    }
    return NOT_A_BUCKET;
}

static void ut_chhResize (struct ut_chh *rt)
{
    /* doubles the size => bucket index gains one bit at the msb =>
       start bucket is unchanged or moved into the added half of the set
       => those for which the (new) msb is 0 are guaranteed to fit, and
       so are those for which the (new) msb is 1 => never have to resize
       recursively */
    struct ut_chhBucketArray * const bsary0 = os_atomic_ldvoidp (&rt->buckets);
    struct ut_chhBucket * const bs0 = bsary0->bs;
    struct ut_chhBucketArray *bsary1;
    struct ut_chhBucket *bs1;
    uint32_t i, idxmask0, idxmask1;

    assert (bsary0->size > 0);
    bsary1 = os_malloc (offsetof (struct ut_chhBucketArray, bs) + 2 * bsary0->size * sizeof (*bsary1->bs));
    bsary1->size = 2 * bsary0->size;
    bs1 = bsary1->bs;

    for (i = 0; i < bsary1->size; i++) {
        os_atomic_st32 (&bs1[i].hopinfo, 0);
        os_atomic_st32 (&bs1[i].timestamp, 0);
        os_atomic_st32 (&bs1[i].lock, 0);
        os_atomic_stvoidp (&bs1[i].data, NULL);
    }
    idxmask0 = bsary0->size - 1;
    idxmask1 = bsary1->size - 1;
    for (i = 0; i < bsary0->size; i++) {
        void *data = os_atomic_ldvoidp (&bs0[i].data);
        if (data && data != CHH_BUSY) {
            const uint32_t hash = rt->hash (data);
            const uint32_t old_start_bucket = hash & idxmask0;
            const uint32_t new_start_bucket = hash & idxmask1;
            const uint32_t dist = (i >= old_start_bucket) ? (i - old_start_bucket) : (bsary0->size + i - old_start_bucket);
            const uint32_t newb = (new_start_bucket + dist) & idxmask1;
            assert (dist < HH_HOP_RANGE);
            os_atomic_rmw32_nonatomic (&bs1[new_start_bucket].hopinfo, x, x | (1u << dist));
            os_atomic_stvoidp (&bs1[newb].data, data);
        }
    }

    os_atomic_fence ();
    os_atomic_stvoidp (&rt->buckets, bsary1);
    rt->gc_buckets (bsary0);
}

int ut_chhAdd (struct ut_chh * UT_HH_RESTRICT rt, const void * UT_HH_RESTRICT data)
{
    const uint32_t hash = rt->hash (data);
    uint32_t size;
    os_rwlockRead (&rt->resize_locks[hash % N_RESIZE_LOCKS]);

    {
        struct ut_chhBucketArray * const bsary = os_atomic_ldvoidp (&rt->buckets);
        struct ut_chhBucket * const bs = bsary->bs;
        uint32_t idxmask;
        uint32_t start_bucket, free_distance, free_bucket;

        size = bsary->size;
        idxmask = size - 1;
        start_bucket = hash & idxmask;

        ut_chhLockBucket (rt, start_bucket);
        if (ut_chhLookupInternal (bsary, rt->equals, start_bucket, data)) {
            ut_chhUnlockBucket (rt, start_bucket);
            os_rwlockUnlock (&rt->resize_locks[hash % N_RESIZE_LOCKS]);
            return 0;
        }

        free_bucket = start_bucket;
        for (free_distance = 0; free_distance < HH_ADD_RANGE; free_distance++) {
            if (os_atomic_ldvoidp (&bs[free_bucket].data) == NULL &&
                os_atomic_casvoidp (&bs[free_bucket].data, NULL, CHH_BUSY)) {
                break;
            }
            free_bucket = (free_bucket + 1) & idxmask;
        }
        if (free_distance < HH_ADD_RANGE) {
            do {
                if (free_distance < HH_HOP_RANGE) {
                    assert (free_bucket == ((start_bucket + free_distance) & idxmask));
                    os_atomic_rmw32_nonatomic (&bs[start_bucket].hopinfo, x, x | (1u << free_distance));
                    os_atomic_fence ();
                    os_atomic_stvoidp (&bs[free_bucket].data, (void *) data);
                    ut_chhUnlockBucket (rt, start_bucket);
                    os_rwlockUnlock (&rt->resize_locks[hash % N_RESIZE_LOCKS]);
                    return 1;
                }
                free_bucket = ut_chhFindCloserFreeBucket (rt, free_bucket, &free_distance);
                assert (free_bucket == NOT_A_BUCKET || free_bucket <= idxmask);
            } while (free_bucket != NOT_A_BUCKET);
        }
        ut_chhUnlockBucket (rt, start_bucket);
    }

    os_rwlockUnlock (&rt->resize_locks[hash % N_RESIZE_LOCKS]);

    {
        int i;
        struct ut_chhBucketArray *bsary1;
        for (i = 0; i < N_RESIZE_LOCKS; i++) {
            os_rwlockWrite (&rt->resize_locks[i]);
        }
        /* another thread may have sneaked past and grown the hash table */
        bsary1 = os_atomic_ldvoidp (&rt->buckets);
        if (bsary1->size == size) {
            ut_chhResize (rt);
        }
        for (i = 0; i < N_RESIZE_LOCKS; i++) {
            os_rwlockUnlock (&rt->resize_locks[i]);
        }
    }

    return ut_chhAdd (rt, data);
}

int ut_chhRemove (struct ut_chh * UT_HH_RESTRICT rt, const void * UT_HH_RESTRICT template)
{
    const uint32_t hash = rt->hash (template);
    os_rwlockRead (&rt->resize_locks[hash % N_RESIZE_LOCKS]);

    {
        struct ut_chhBucketArray * const bsary = os_atomic_ldvoidp (&rt->buckets);
        struct ut_chhBucket * const bs = bsary->bs;
        const uint32_t size = bsary->size;
        const uint32_t idxmask = size - 1;
        const uint32_t bucket = hash & idxmask;
        uint32_t hopinfo;
        uint32_t idx;
        ut_chhLockBucket (rt, bucket);
        hopinfo = os_atomic_ld32 (&bs[bucket].hopinfo);
        for (idx = 0; hopinfo != 0; hopinfo >>= 1, idx++) {
            if (hopinfo & 1) {
                const uint32_t bidx = (bucket + idx) & idxmask;
                void *data = os_atomic_ldvoidp (&bs[bidx].data);
                if (ut_chhDataValid_p (data) && rt->equals (data, template)) {
                    os_atomic_stvoidp (&bs[bidx].data, NULL);
                    os_atomic_rmw32_nonatomic (&bs[bucket].hopinfo, x, x & ~(1u << idx));
                    ut_chhUnlockBucket (rt, bucket);
                    os_rwlockUnlock (&rt->resize_locks[hash % N_RESIZE_LOCKS]);
                    return 1;
                }
            }
        }
        ut_chhUnlockBucket (rt, bucket);
    }

    os_rwlockUnlock (&rt->resize_locks[hash % N_RESIZE_LOCKS]);
    return 0;
}

void ut_chhEnumUnsafe (struct ut_chh * UT_HH_RESTRICT rt, void (*f) (void *a, void *f_arg), void *f_arg)
{
    struct ut_chhBucketArray * const bsary = os_atomic_ldvoidp (&rt->buckets);
    struct ut_chhBucket * const bs = bsary->bs;
    uint32_t i;
    for (i = 0; i < bsary->size; i++) {
        void *data = os_atomic_ldvoidp (&bs[i].data);
        if (data && data != CHH_BUSY) {
            f (data, f_arg);
        }
    }
}

void *ut_chhIterNext (struct ut_chhIter *it)
{
  uint32_t i;
  for (i = it->cursor; i < it->size; i++) {
    void *data = os_atomic_ldvoidp (&it->bs[i].data);
    if (data && data != CHH_BUSY) {
      it->cursor = i+1;
      return data;
    }
  }
  return NULL;
}

void *ut_chhIterFirst (struct ut_chh * UT_HH_RESTRICT rt, struct ut_chhIter *it)
{
  struct ut_chhBucketArray * const bsary = os_atomic_ldvoidp (&rt->buckets);
  it->bs = bsary->bs;
  it->size = bsary->size;
  it->cursor = 0;
  return ut_chhIterNext (it);
}

/************* SEQUENTIAL VERSION ***************/

struct ut_hhBucket {
    uint32_t hopinfo;
    void *data;
};

struct ut_hh {
    uint32_t size; /* power of 2 */
    struct ut_hhBucket *buckets;
    ut_hhHash_fn hash;
    ut_hhEquals_fn equals;
};

static void ut_hhInit (struct ut_hh *rt, uint32_t init_size, ut_hhHash_fn hash, ut_hhEquals_fn equals)
{
    uint32_t size = HH_HOP_RANGE;
    uint32_t i;
    while (size < init_size) {
        size *= 2;
    }
    rt->hash = hash;
    rt->equals = equals;
    rt->size = size;
    _Analysis_assume_(size >= HH_HOP_RANGE);
    rt->buckets = os_malloc (size * sizeof (*rt->buckets));
    for (i = 0; i < size; i++) {
        rt->buckets[i].hopinfo = 0;
        rt->buckets[i].data = NULL;
    }
}

static void ut_hhFini (struct ut_hh *rt)
{
    os_free (rt->buckets);
}

struct ut_hh *ut_hhNew (uint32_t init_size, ut_hhHash_fn hash, ut_hhEquals_fn equals)
{
    struct ut_hh *hh = os_malloc (sizeof (*hh));
    ut_hhInit (hh, init_size, hash, equals);
    return hh;
}

void ut_hhFree (struct ut_hh * UT_HH_RESTRICT hh)
{
    ut_hhFini (hh);
    os_free (hh);
}

static void *ut_hhLookupInternal (const struct ut_hh *rt, const uint32_t bucket, const void *template)
{
    const uint32_t idxmask = rt->size - 1;
    uint32_t hopinfo = rt->buckets[bucket].hopinfo;
    uint32_t idx;
    for (idx = 0; hopinfo != 0; hopinfo >>= 1, idx++) {
        const uint32_t bidx = (bucket + idx) & idxmask;
        void *data = rt->buckets[bidx].data;
        if (data && rt->equals (data, template))
            return data;
    }
    return NULL;
}

void *ut_hhLookup (const struct ut_hh * UT_HH_RESTRICT rt, const void * UT_HH_RESTRICT template)
{
    const uint32_t hash = rt->hash (template);
    const uint32_t idxmask = rt->size - 1;
    const uint32_t bucket = hash & idxmask;
    return ut_hhLookupInternal (rt, bucket, template);
}

static uint32_t ut_hhFindCloserFreeBucket (struct ut_hh *rt, uint32_t free_bucket, uint32_t *free_distance)
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

static void ut_hhResize (struct ut_hh *rt)
{
    struct ut_hhBucket *bs1;
    uint32_t i, idxmask0, idxmask1;

    _Analysis_assume_(rt->size >= HH_HOP_RANGE);
    bs1 = os_malloc (2 * rt->size * sizeof (*rt->buckets));

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

    os_free (rt->buckets);
    rt->size *= 2;
    rt->buckets = bs1;
}

int ut_hhAdd (struct ut_hh * UT_HH_RESTRICT rt, const void * UT_HH_RESTRICT data)
{
    const uint32_t hash = rt->hash (data);
    const uint32_t idxmask = rt->size - 1;
    const uint32_t start_bucket = hash & idxmask;
    uint32_t free_distance, free_bucket;

    if (ut_hhLookupInternal (rt, start_bucket, data)) {
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
            free_bucket = ut_hhFindCloserFreeBucket (rt, free_bucket, &free_distance);
            assert (free_bucket == NOT_A_BUCKET || free_bucket <= idxmask);
        } while (free_bucket != NOT_A_BUCKET);
    }

    ut_hhResize (rt);
    return ut_hhAdd (rt, data);
}

int ut_hhRemove (struct ut_hh * UT_HH_RESTRICT rt, const void * UT_HH_RESTRICT template)
{
    const uint32_t hash = rt->hash (template);
    const uint32_t idxmask = rt->size - 1;
    const uint32_t bucket = hash & idxmask;
    uint32_t hopinfo;
    uint32_t idx;
    hopinfo = rt->buckets[bucket].hopinfo;
    for (idx = 0; hopinfo != 0; hopinfo >>= 1, idx++) {
        if (hopinfo & 1) {
            const uint32_t bidx = (bucket + idx) & idxmask;
            void *data = rt->buckets[bidx].data;
            if (data && rt->equals (data, template)) {
                rt->buckets[bidx].data = NULL;
                rt->buckets[bucket].hopinfo &= ~(1u << idx);
                return 1;
            }
        }
    }
    return 0;
}

void ut_hhEnum (struct ut_hh * UT_HH_RESTRICT rt, void (*f) (void *a, void *f_arg), void *f_arg)
{
    uint32_t i;
    for (i = 0; i < rt->size; i++) {
        void *data = rt->buckets[i].data;
        if (data) {
            f (data, f_arg);
        }
    }
}

void *ut_hhIterFirst (struct ut_hh * UT_HH_RESTRICT rt, struct ut_hhIter * UT_HH_RESTRICT iter)
{
    iter->hh = rt;
    iter->cursor = 0;
    return ut_hhIterNext (iter);
}

void *ut_hhIterNext (struct ut_hhIter * UT_HH_RESTRICT iter)
{
    struct ut_hh *rt = iter->hh;
    while (iter->cursor < rt->size) {
        void *data = rt->buckets[iter->cursor].data;
        iter->cursor++;
        if (data) {
            return data;
        }
    }
    return NULL;
}

/************* SEQUENTIAL VERSION WITH EMBEDDED DATA ***************/

struct ut_ehhBucket {
    uint32_t hopinfo;
    uint32_t inuse;
    char data[];
};

struct ut_ehh {
    uint32_t size; /* power of 2 */
    size_t elemsz;
    size_t bucketsz;
    char *buckets; /* ehhBucket, but embedded data messes up the layout */
    ut_hhHash_fn hash;
    ut_hhEquals_fn equals;
};

static void ut_ehhInit (struct ut_ehh *rt, size_t elemsz, uint32_t init_size, ut_hhHash_fn hash, ut_hhEquals_fn equals)
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
    rt->bucketsz = sizeof (struct ut_ehhBucket) + ((elemsz+7) & ~(size_t)7);
    _Analysis_assume_(size >= HH_HOP_RANGE);
    rt->buckets = os_malloc (size * rt->bucketsz);
    for (i = 0; i < size; i++) {
        struct ut_ehhBucket *b = (struct ut_ehhBucket *) (rt->buckets + i * rt->bucketsz);
        b->hopinfo = 0;
        b->inuse = 0;
    }
}

static void ut_ehhFini (struct ut_ehh *rt)
{
    os_free (rt->buckets);
}

struct ut_ehh *ut_ehhNew (size_t elemsz, uint32_t init_size, ut_hhHash_fn hash, ut_hhEquals_fn equals)
{
    struct ut_ehh *hh = os_malloc (sizeof (*hh));
    ut_ehhInit (hh, elemsz, init_size, hash, equals);
    return hh;
}

void ut_ehhFree (struct ut_ehh * UT_HH_RESTRICT hh)
{
    ut_ehhFini (hh);
    os_free (hh);
}

static void *ut_ehhLookupInternal (const struct ut_ehh *rt, uint32_t bucket, const void *template)
{
    const struct ut_ehhBucket *b = (const struct ut_ehhBucket *) (rt->buckets + bucket * rt->bucketsz);
    uint32_t hopinfo = b->hopinfo;

    if (hopinfo & 1) {
        if (b->inuse && rt->equals (b->data, template)) {
            return (void *) b->data;
        }
    }

    do {
        hopinfo >>= 1;
        if (++bucket == rt->size) {
            bucket = 0;
        }
        if (hopinfo & 1) {
            b = (const struct ut_ehhBucket *) (rt->buckets + bucket * rt->bucketsz);
            if (b->inuse && rt->equals (b->data, template)) {
                return (void *) b->data;
            }
        }
    } while (hopinfo != 0);
    return NULL;
}

void *ut_ehhLookup (const struct ut_ehh * UT_HH_RESTRICT rt, const void * UT_HH_RESTRICT template)
{
    const uint32_t hash = rt->hash (template);
    const uint32_t idxmask = rt->size - 1;
    const uint32_t bucket = hash & idxmask;
    return ut_ehhLookupInternal (rt, bucket, template);
}

static uint32_t ut_ehhFindCloserFreeBucket (struct ut_ehh *rt, uint32_t free_bucket, uint32_t *free_distance)
{
    const uint32_t idxmask = rt->size - 1;
    uint32_t move_bucket, free_dist;
    move_bucket = (free_bucket - (HH_HOP_RANGE - 1)) & idxmask;
    for (free_dist = HH_HOP_RANGE - 1; free_dist > 0; free_dist--) {
        struct ut_ehhBucket * const mb = (struct ut_ehhBucket *) (rt->buckets + move_bucket * rt->bucketsz);
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
            struct ut_ehhBucket * const fb = (struct ut_ehhBucket *) (rt->buckets + free_bucket * rt->bucketsz);
            struct ut_ehhBucket * const nfb = (struct ut_ehhBucket *) (rt->buckets + new_free_bucket * rt->bucketsz);
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

static void ut_ehhResize (struct ut_ehh *rt)
{
    char *bs1;
    uint32_t i, idxmask0, idxmask1;

    bs1 = os_malloc (2 * rt->size * rt->bucketsz);

    for (i = 0; i < 2 * rt->size; i++) {
        struct ut_ehhBucket *b = (struct ut_ehhBucket *) (bs1 + i * rt->bucketsz);
        b->hopinfo = 0;
        b->inuse = 0;
    }
    idxmask0 = rt->size - 1;
    idxmask1 = 2 * rt->size - 1;
    for (i = 0; i < rt->size; i++) {
        struct ut_ehhBucket const * const b = (struct ut_ehhBucket *) (rt->buckets + i * rt->bucketsz);
        if (b->inuse) {
            const uint32_t hash = rt->hash (b->data);
            const uint32_t old_start_bucket = hash & idxmask0;
            const uint32_t new_start_bucket = hash & idxmask1;
            const uint32_t dist = (i >= old_start_bucket) ? (i - old_start_bucket) : (rt->size + i - old_start_bucket);
            const uint32_t newb = (new_start_bucket + dist) & idxmask1;
            struct ut_ehhBucket * const nsb = (struct ut_ehhBucket *) (bs1 + new_start_bucket * rt->bucketsz);
            struct ut_ehhBucket * const nb = (struct ut_ehhBucket *) (bs1 + newb * rt->bucketsz);
            assert (dist < HH_HOP_RANGE);
            assert (!nb->inuse);
            nsb->hopinfo |= 1u << dist;
            nb->inuse = 1;
            memcpy (nb->data, b->data, rt->elemsz);
        }
    }

    os_free (rt->buckets);
    rt->size *= 2;
    rt->buckets = bs1;
}

int ut_ehhAdd (struct ut_ehh * UT_HH_RESTRICT rt, const void * UT_HH_RESTRICT data)
{
    const uint32_t hash = rt->hash (data);
    const uint32_t idxmask = rt->size - 1;
    const uint32_t start_bucket = hash & idxmask;
    uint32_t free_distance, free_bucket;

    if (ut_ehhLookupInternal (rt, start_bucket, data)) {
        return 0;
    }

    free_bucket = start_bucket;
    for (free_distance = 0; free_distance < HH_ADD_RANGE; free_distance++) {
        struct ut_ehhBucket const * const fb = (struct ut_ehhBucket *) (rt->buckets + free_bucket * rt->bucketsz);
        if (!fb->inuse) {
            break;
        }
        free_bucket = (free_bucket + 1) & idxmask;
    }
    if (free_distance < HH_ADD_RANGE) {
        do {
            if (free_distance < HH_HOP_RANGE) {
                struct ut_ehhBucket * const sb = (struct ut_ehhBucket *) (rt->buckets + start_bucket * rt->bucketsz);
                struct ut_ehhBucket * const fb = (struct ut_ehhBucket *) (rt->buckets + free_bucket * rt->bucketsz);
                assert ((uint32_t) free_bucket == ((start_bucket + free_distance) & idxmask));
                sb->hopinfo |= 1u << free_distance;
                fb->inuse = 1;
                memcpy (fb->data, data, rt->elemsz);
                assert (ut_ehhLookupInternal (rt, start_bucket, data));
                return 1;
            }
            free_bucket = ut_ehhFindCloserFreeBucket (rt, free_bucket, &free_distance);
            assert (free_bucket == NOT_A_BUCKET || free_bucket <= idxmask);
        } while (free_bucket != NOT_A_BUCKET);
    }

    ut_ehhResize (rt);
    return ut_ehhAdd (rt, data);
}

int ut_ehhRemove (struct ut_ehh * UT_HH_RESTRICT rt, const void * UT_HH_RESTRICT template)
{
    const uint32_t hash = rt->hash (template);
    const uint32_t idxmask = rt->size - 1;
    const uint32_t bucket = hash & idxmask;
    uint32_t hopinfo;
    struct ut_ehhBucket *sb;
    uint32_t idx;
    sb = (struct ut_ehhBucket *) (rt->buckets + bucket * rt->bucketsz);
    hopinfo = sb->hopinfo;
    for (idx = 0; hopinfo != 0; hopinfo >>= 1, idx++) {
        if (hopinfo & 1) {
            const uint32_t bidx = (bucket + idx) & idxmask;
            struct ut_ehhBucket *b = (struct ut_ehhBucket *) (rt->buckets + bidx * rt->bucketsz);
            if (b->inuse && rt->equals (b->data, template)) {
                assert (ut_ehhLookupInternal(rt, bucket, template));
                b->inuse = 0;
                sb->hopinfo &= ~(1u << idx);
                return 1;
            }
        }
    }
    assert (!ut_ehhLookupInternal(rt, bucket, template));
    return 0;
}

void ut_ehhEnum (struct ut_ehh * UT_HH_RESTRICT rt, void (*f) (void *a, void *f_arg), void *f_arg)
{
    uint32_t i;
    for (i = 0; i < rt->size; i++) {
        struct ut_ehhBucket *b = (struct ut_ehhBucket *) (rt->buckets + i * rt->bucketsz);
        if (b->inuse) {
            f (b->data, f_arg);
        }
    }
}

void *ut_ehhIterFirst (struct ut_ehh * UT_HH_RESTRICT rt, struct ut_ehhIter * UT_HH_RESTRICT iter)
{
    iter->hh = rt;
    iter->cursor = 0;
    return ut_ehhIterNext (iter);
}

void *ut_ehhIterNext (struct ut_ehhIter * UT_HH_RESTRICT iter)
{
    struct ut_ehh *rt = iter->hh;
    while (iter->cursor < rt->size) {
        struct ut_ehhBucket *b = (struct ut_ehhBucket *) (rt->buckets + iter->cursor * rt->bucketsz);
        iter->cursor++;
        if (b->inuse) {
            return b->data;
        }
    }
    return NULL;
}
