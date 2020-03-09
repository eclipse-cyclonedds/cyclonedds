/*
* Copyright(c) 2019 ADLINK Technology Limited and others
*
* This program and the accompanying materials are made available under the
* terms of the Eclipse Public License v. 2.0 which is available at
* http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
* v. 1.0 which is available at
* http://www.eclipse.org/org/documents/edl-v10.php.
*
* SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
*/
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
  ddsrt_prng_init_simple (&prng, ddsrt_random ());
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
  int (*add) (void *h, const void *v);
  int (*remove) (void *h, const void *v);
};

#define WRAP(ret_, f_) static ret_ f_##_w (void *h, const void *v) { return f_ (h, v); }
WRAP(void *, ddsrt_hh_lookup);
WRAP(int, ddsrt_hh_add);
WRAP(int, ddsrt_hh_remove);
WRAP(void *, ddsrt_chh_lookup);
WRAP(int, ddsrt_chh_add);
WRAP(int, ddsrt_chh_remove);
WRAP(void *, ddsrt_ehh_lookup);
WRAP(int, ddsrt_ehh_add);
WRAP(int, ddsrt_ehh_remove);
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

CU_Theory ((const struct ops *ops, bool random, adj_fun_t adj, const char *adjname), ddsrt_hopscotch, random)
{
  printf ("%s random=%d adj=%s", ops->name, random, adjname);
  fflush (stdout);
  init (random);
  void *h = ops->new ();
  uint32_t i, nk = 0;
  uint64_t nn = 0;
  ddsrt_mtime_t t0, t1;
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
  printf (" %"PRIu64" %.0f ns/cycle\n", nn, (double) (t1.v - t0.v) / (double) nn);
}
