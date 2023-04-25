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
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "CUnit/Test.h"
#include "dds/ddsrt/random.h"

#define N_PRNG 4
#define N_DATA 10

CU_Test(ddsrt_random, mt19937)
{
  static const uint32_t refdata[N_DATA] = {
    3499211612u, 581869302u, 3890346734u, 3586334585u, 545404204u,
    4161255391u, 3922919429u, 949333985u, 2715962298u, 1323567403u
  };
  ddsrt_prng_t prng;
  ddsrt_prng_init_simple (&prng, 5489U);
  for (size_t i = 0; i < N_DATA; i++)
  {
    uint32_t x = ddsrt_prng_random (&prng);
    CU_ASSERT_EQUAL_FATAL(x, refdata[i]);
  }
}

CU_Test(ddsrt_random, makeseed)
{
  ddsrt_prng_seed_t seeds[N_PRNG];

  /* Until proven otherwise, assume all platforms have a good way of getting multiple seeds */
  memset (seeds, 0, sizeof (seeds));
  for (size_t i = 0; i < N_PRNG; i++)
  {
    bool ok = ddsrt_prng_makeseed (&seeds[i]);
    CU_ASSERT_FATAL(ok);
  }

  /* Any pair the same is possible, but the likelihood should be so small that it is worth accepting
     an intermittently failing test */
  for (size_t i = 0; i < N_PRNG; i++)
  {
    for (size_t j = i + 1; j < N_PRNG; j++)
      CU_ASSERT_FATAL (memcmp (&seeds[i], &seeds[j], sizeof (seeds[i])) != 0);
  }

  /* A short random sequence generated from each of the different seeds should be unique -- again,
     there is no guarantee but only an overwhelming likelihood */
  ddsrt_prng_t prngs[N_PRNG];
  uint32_t data[N_PRNG][N_DATA];
  memset (data, 0, sizeof (data));
  for (size_t i = 0; i < N_PRNG; i++)
  {
    ddsrt_prng_init (&prngs[i], &seeds[i]);
    for (size_t j = 0; j < N_DATA; j++)
      data[i][j] = ddsrt_prng_random (&prngs[i]);
  }
  for (size_t i = 0; i < N_PRNG; i++)
  {
    for (size_t j = i + 1; j < N_PRNG; j++)
      CU_ASSERT_FATAL (memcmp (&data[i], &data[j], sizeof (data[i])) != 0);
  }
}

CU_Test(ddsrt_random, default_random)
{
#define N_BINS 128
#define N_PER_BIN 100
  uint32_t bins[N_BINS];
  memset (bins, 0, sizeof (bins));
  for (size_t i = 0; i < N_PER_BIN * N_BINS; i++)
  {
    uint32_t x = ddsrt_random ();
    bins[x % N_BINS]++;
  }
  double chisq = 0.0;
  for (size_t i = 0; i < N_BINS; i++)
    chisq += ((bins[i] - N_PER_BIN) * (bins[i] - N_PER_BIN)) / (double) N_PER_BIN;
  /* Solve[CDF[ChiSquareDistribution[127], x] == 999/1000] */
  CU_ASSERT (chisq < 181.993);
}
