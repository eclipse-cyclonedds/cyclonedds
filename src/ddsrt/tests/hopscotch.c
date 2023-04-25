// Copyright(c) 2019 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include "CUnit/Test.h"
#include "CUnit/Theory.h"

#include "dds/ddsrt/random.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/threads.h"

#define MAX_NKEYS 10000
#define MAX_ITERS 1000000

static int nkeys_hist[MAX_NKEYS+1];
static uint32_t objs[MAX_NKEYS], keys[MAX_NKEYS];
static uint32_t next_v;
static ddsrt_prng_t prng;

static uint32_t hash_uint32 (const void *v)
{
  const uint64_t m = UINT64_C (10242350189706880077);
  const uint32_t h = (uint32_t) ((*((uint32_t *) v) * m) >> 32);
  return h;
}

static int equals_uint32 (const void *a, const void *b)
{
  return *((uint32_t *) a) == *((uint32_t *) b);
}

static int compare_uint32 (const void *va, const void *vb)
{
  const uint32_t *a = va;
  const uint32_t *b = vb;
  return (*a == *b) ? 0 : (*a < *b) ? -1 : 1;
}

static void swap (uint32_t *a, uint32_t *b)
{
  uint32_t t = *a;
  *a = *b;
  *b = t;
}

static void init (bool random)
{
  uint32_t i;

  ddsrt_prng_seed_t prng_seed;
  bool haveseed = ddsrt_prng_makeseed (&prng_seed);
  CU_ASSERT_FATAL (haveseed);
  ddsrt_prng_init (&prng, &prng_seed);
  printf ("%08"PRIx32".%08"PRIx32".%08"PRIx32".%08"PRIx32".%08"PRIx32".%08"PRIx32".%08"PRIx32".%08"PRIx32"\n",
          prng_seed.key[0], prng_seed.key[1], prng_seed.key[2], prng_seed.key[3], prng_seed.key[4], prng_seed.key[5], prng_seed.key[6], prng_seed.key[7]);

  next_v = MAX_NKEYS;
  for (i = 0; i < MAX_NKEYS; i++)
  {
    nkeys_hist[i] = 0;
    keys[i] = i;
  }
  if (random)
  {
    /* Generate MAX_NKEYS unique random ints by repeatedly replacing
       duplicates with other random numbers (this'll take more time the
       larger MAX_NKEYS is, but for practical values, it is nearly
       instantaneous) */
    for (i = 0; i < MAX_NKEYS - 1; i++)
      objs[i] = ddsrt_prng_random (&prng);
    do {
      objs[i] = ddsrt_prng_random (&prng);
      qsort (objs, MAX_NKEYS, sizeof (*objs), compare_uint32);
      for (i = 1; i < MAX_NKEYS && objs[i-1] != objs[i]; i++)
        ;
    } while (i < MAX_NKEYS);
  }
  else
  {
    for (i = 0; i < MAX_NKEYS; i++)
      objs[i] = i;
  }
}

struct ops {
  const char *name;
  void * (*new) (void);
  void (*free) (void *h);
  void * (*lookup) (void *h, const void *v);
  int (*add) (void *h, void *v);
  int (*remove) (void *h, const void *v);
};

#define WRAP(ret_, f_) static ret_ f_##_w (void *h, const void *v) { return f_ (h, v); }
WRAP(void *, ddsrt_hh_lookup);
WRAP(int, ddsrt_hh_remove);
WRAP(void *, ddsrt_chh_lookup);
WRAP(int, ddsrt_chh_remove);
WRAP(void *, ddsrt_ehh_lookup);
WRAP(int, ddsrt_ehh_remove);
#undef WRAP
#define WRAP(ret_, f_) static ret_ f_##_w (void *h, void *v) { return f_ (h, v); }
WRAP(int, ddsrt_hh_add);
WRAP(int, ddsrt_chh_add);
WRAP(int, ddsrt_ehh_add);
#undef WRAP

static void free_buckets (void *bs, void *arg)
{
  /* nothing to worry about because this is single threaded */
  (void) arg;
  ddsrt_free (bs);
}

static void *hhnew (void) { return ddsrt_hh_new (1, hash_uint32, equals_uint32); }
static void hhfree (void *h) { ddsrt_hh_free (h); }
static void *chhnew (void) { return ddsrt_chh_new (1, hash_uint32, equals_uint32, free_buckets, NULL); }
static void chhfree (void *h) { ddsrt_chh_free (h); }
static void *ehhnew (void) { return ddsrt_ehh_new (sizeof (uint32_t), 1, hash_uint32, equals_uint32); }
static void ehhfree (void *h) { ddsrt_ehh_free (h); }

static const struct ops hhops = {
  .name = "hh",
  .new = hhnew,
  .free = hhfree,
  .lookup = ddsrt_hh_lookup_w,
  .add = ddsrt_hh_add_w,
  .remove = ddsrt_hh_remove_w
};
static const struct ops chhops = {
  .name = "chh",
  .new = chhnew,
  .free = chhfree,
  .lookup = ddsrt_chh_lookup_w,
  .add = ddsrt_chh_add_w,
  .remove = ddsrt_chh_remove_w
};
static const struct ops ehhops = {
  .name = "ehh",
  .new = ehhnew,
  .free = ehhfree,
  .lookup = ddsrt_ehh_lookup_w,
  .add = ddsrt_ehh_add_w,
  .remove = ddsrt_ehh_remove_w
};

static void adj_nop (uint32_t *v) { (void) v; }
static void adj_seq (uint32_t *v) { *v = next_v++; }

typedef void (*adj_fun_t) (uint32_t *v);

CU_TheoryDataPoints (ddsrt_hopscotch, random) = {
  CU_DataPoints (const struct ops *, &hhops,  &chhops, &ehhops,  &hhops,  &chhops, &ehhops),
  CU_DataPoints (bool,               true,    true,    true,     false,   false,   false),
  CU_DataPoints (adj_fun_t,          adj_nop, adj_nop, adj_nop,  adj_seq, adj_seq, adj_seq),
  CU_DataPoints (const char *,       "nop",   "nop",   "nop",    "seq",   "seq",   "seq")
};

CU_Theory ((const struct ops *ops, bool random, adj_fun_t adj, const char *adjname), ddsrt_hopscotch, random, .timeout = 20)
{
  printf ("%"PRId64" %s random=%d adj=%s\n", ddsrt_time_monotonic().v, ops->name, random, adjname);
  fflush (stdout);
  init (random);
  void *h = ops->new ();
  uint32_t i, nk = 0;
  uint64_t nn = 0;
  ddsrt_mtime_t t0, t1;
  printf ("%"PRId64" %s start\n", ddsrt_time_monotonic().v, ops->name);
  fflush (stdout);
  t0 = ddsrt_time_monotonic ();
  for (uint32_t iter = 0; iter < MAX_ITERS; iter++)
  {
    int r;
    assert (nk <= MAX_NKEYS);
    nkeys_hist[nk]++;
    if (nk == MAX_NKEYS || (nk > 0 && (ddsrt_prng_random (&prng) & 1)))
    {
      i = ddsrt_prng_random (&prng) % nk;
      if (!ops->lookup (h, &objs[keys[i]]))
        CU_FAIL_FATAL ("key not present\n");
      r = ops->remove (h, &objs[keys[i]]);
      if (!r)
        CU_FAIL_FATAL ("remove failed\n");
      if (ops->lookup (h, &objs[keys[i]]))
        CU_FAIL_FATAL ("key still present\n");
      adj (&objs[keys[i]]);
      swap (&keys[i], &keys[nk-1]);
      nk--;
    }
    else
    {
      i = nk + (ddsrt_prng_random (&prng) % (MAX_NKEYS - nk));
      if (ops->lookup (h, &objs[keys[i]]))
        CU_FAIL_FATAL ("key already present\n");
      r = ops->add (h, &objs[keys[i]]);
      if (!r)
        CU_FAIL_FATAL ("add failed\n");
      if (!ops->lookup (h, &objs[keys[i]]))
        CU_FAIL_FATAL ("key still not present\n");
      swap (&keys[i], &keys[nk]);
      nk++;
    }
    nn++;
  }
  t1 = ddsrt_time_monotonic ();
  ops->free (h);
  printf ("%"PRId64" %s done %"PRIu64" %.0f ns/cycle\n", ddsrt_time_monotonic().v, ops->name, nn, (double) (t1.v - t0.v) / (double) nn);
  fflush (stdout);
}

struct gcelem {
  void *block;
  struct gcelem *next;
};

static void chhtest_gc (void *block, void *arg)
{
  // simply defer freeing memory until the end of the test
  struct gcelem **gclist = arg;
  struct gcelem *elem = ddsrt_malloc (sizeof (*elem));
  elem->block = block;
  elem->next = *gclist;
  *gclist = elem;
}

struct chhtest_thread_arg {
  ddsrt_atomic_uint32_t *stop;
  struct ddsrt_chh *chh;
  uint32_t *keys;
  uint32_t nkeys;
  uint32_t adds, removes, lookups, maxnkeys;
  bool check;
};

static uint32_t chhtest_thread (void *varg)
{
  struct chhtest_thread_arg * const __restrict arg = varg;
  uint32_t ** ksptrs;
  uint32_t n = 0;

  assert(arg->nkeys > 0);
  ksptrs = ddsrt_malloc (arg->nkeys * sizeof (*ksptrs));
  for (uint32_t i = 0; i < arg->nkeys; i++)
    ksptrs[i] = &arg->keys[i];

  arg->adds = arg->removes = arg->lookups = arg->maxnkeys = 0;

  ddsrt_prng_t local_prng;
  ddsrt_prng_seed_t prng_seed;
  bool haveseed = ddsrt_prng_makeseed (&prng_seed);
  CU_ASSERT_FATAL (haveseed);
  ddsrt_prng_init (&local_prng, &prng_seed);
  printf ("%08"PRIx32".%08"PRIx32".%08"PRIx32".%08"PRIx32".%08"PRIx32".%08"PRIx32".%08"PRIx32".%08"PRIx32"\n",
          prng_seed.key[0], prng_seed.key[1], prng_seed.key[2], prng_seed.key[3], prng_seed.key[4], prng_seed.key[5], prng_seed.key[6], prng_seed.key[7]);
  while (!ddsrt_atomic_ld32 (arg->stop))
  {
    const uint32_t raw_oper = ddsrt_prng_random (&local_prng);
    switch (raw_oper % 4)
    {
      case 0: /* add */
        if (n < arg->nkeys)
        {
          bool x = ddsrt_chh_add (arg->chh, ksptrs[n]);
          if (arg->check && !x) { CU_ASSERT_FATAL (0); }
          n++;
          arg->adds++;
          if (n > arg->maxnkeys)
            arg->maxnkeys = n;
        }
        break;
      case 1: /* remove */
        if (n > 0)
        {
          const uint32_t ix = (raw_oper >> 2) % n;
          uint32_t * const obj = ksptrs[ix];
          bool x = ddsrt_chh_remove (arg->chh, obj);
          if (arg->check && !x) { CU_ASSERT_FATAL (0); }
          ksptrs[ix] = ksptrs[--n];
          ksptrs[n] = obj;
          arg->removes++;
        }
        break;
      case 2: case 3: /* lookup */
        {
          const uint32_t ix = (raw_oper >> 2) % arg->nkeys;
          bool x = ddsrt_chh_lookup (arg->chh, ksptrs[ix]);
          if (arg->check && ((ix < n) ? !x : x)) { CU_ASSERT_FATAL (0) };
          arg->lookups++;
        }
        break;
    }
  }

  // "erase" the entries not in the hash table to simplify checking
  // after all threads have stopped
  for (uint32_t i = n; i < arg->nkeys; i++)
    *ksptrs[i] = UINT32_MAX;
  ddsrt_free (ksptrs);
  return 0;
}

static void chhtest_check (void *vobj, void *varg)
{
  uint32_t *obj = vobj;
  uint32_t *count = varg;
  CU_ASSERT_FATAL (*obj != UINT32_MAX);
  (*count)++;
}

static void chhtest_count (void *vobj, void *varg)
{
  uint32_t *count = varg;
  (void) vobj;
  (*count)++;
}

CU_Test(ddsrt_hopscotch, concurrent, .timeout = 20)
{
  const uint32_t nkeys = 100;
  const bool check = false;
  struct ddsrt_chh *chh;
  struct gcelem *gclist = NULL;
  uint32_t *keyset = ddsrt_malloc (nkeys * sizeof (*keyset));
  for (uint32_t i = 0; i < nkeys; i++)
    keyset[i] = i;

  chh = ddsrt_chh_new (1, hash_uint32, equals_uint32, chhtest_gc, &gclist);
  CU_ASSERT_FATAL (chh != NULL);

  ddsrt_atomic_uint32_t stop = DDSRT_ATOMIC_UINT32_INIT (0);
  struct chhtest_thread_arg args[4];
  ddsrt_thread_t tids[4];
  for (uint32_t i = 0; i < 4; i++)
  {
    args[i].chh = chh;
    args[i].stop = &stop;
    if (check)
    {
      args[i].keys = keyset + i * (nkeys / 4);
      args[i].nkeys = nkeys / 4;
    }
    else
    {
      args[i].keys = keyset;
      args[i].nkeys = nkeys;
    }
    args[i].check = check;
  }
  for (uint32_t i = 0; i < 4; i++)
  {
    ddsrt_threadattr_t attr;
    ddsrt_threadattr_init (&attr);
    dds_return_t ret = ddsrt_thread_create (&tids[i], "x", &attr, chhtest_thread, &args[i]);
    CU_ASSERT_FATAL (ret == 0);
  }

  dds_time_t tend = dds_time () + DDS_SECS (15);
  while (dds_time () < tend)
    dds_sleepfor (DDS_MSECS (100));
  ddsrt_atomic_st32 (&stop, 1);
  ddsrt_atomic_fence ();

  for (uint32_t i = 0; i < 4; i++)
  {
    dds_return_t ret = ddsrt_thread_join (tids[i], NULL);
    CU_ASSERT_FATAL (ret == 0);
    printf ("args[%"PRIu32"] add %"PRIu32" rm %"PRIu32" lk %"PRIu32" max %"PRIu32"\n",
            i, args[i].adds, args[i].removes, args[i].lookups, args[i].maxnkeys);
  }

  uint32_t count = 0;
  ddsrt_chh_enum_unsafe (chh, check ? chhtest_check : chhtest_count, &count);
  printf ("nkeys in hash %"PRIu32"\n", count);
  if (check)
  {
    for (uint32_t i = 0; i < nkeys; i++)
    {
      if (keyset[i] != UINT32_MAX)
      {
        CU_ASSERT_FATAL (count > 0);
        count--;
      }
    }
    CU_ASSERT_FATAL (count == 0);
  }

  ddsrt_chh_free (chh);
  ddsrt_free (keyset);

  while (gclist)
  {
    struct gcelem *elem = gclist;
    gclist = gclist->next;
    ddsrt_free (elem->block);
    ddsrt_free (elem);
  }
}
