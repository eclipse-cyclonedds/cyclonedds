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

/*
 A C-program for MT19937, with initialization improved 2002/1/26.
 Coded by Takuji Nishimura and Makoto Matsumoto.

 Before using, initialize the state by using init_genrand(seed)
 or init_by_array(init_key, key_length).

 Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 3. The names of its contributors may not be used to endorse or promote
    products derived from this software without specific prior written
    permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


 Any feedback is very welcome.
 http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html
 email: m-mat @ math.sci.hiroshima-u.ac.jp (remove space)
*/

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "dds/ddsrt/random.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/static_assert.h"

#define N DDSRT_MT19937_N
#define M 397
#define MATRIX_A 0x9908b0dfU   /* constant vector a */
#define UPPER_MASK 0x80000000U /* most significant w-r bits */
#define LOWER_MASK 0x7fffffffU /* least significant r bits */

static ddsrt_prng_t default_prng;
static ddsrt_mutex_t default_prng_lock;

/* initializes mt[N] with a seed */
static void init_genrand (ddsrt_prng_t *prng, uint32_t s)
{
  prng->mt[0] = s;
  for (prng->mti = 1; prng->mti < N; prng->mti++)
  {
    prng->mt[prng->mti] = (1812433253U * (prng->mt[prng->mti-1] ^ (prng->mt[prng->mti-1] >> 30)) + prng->mti);
    /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
    /* In the previous versions, MSBs of the seed affect   */
    /* only MSBs of the array mt[].                        */
    /* 2002/01/09 modified by Makoto Matsumoto             */
  }
}

/* initialize by an array with array-length */
/* init_key is the array for initializing keys */
/* key_length is its length */
/* slight change for C++, 2004/2/26 */
static void init_by_array (ddsrt_prng_t *prng, const uint32_t init_key[], size_t key_length)
{
  uint32_t i, j, k;
  init_genrand (prng, 19650218U);
  i = 1; j = 0;
  k = (N > key_length ? N : (uint32_t) key_length);
  for (; k; k--)
  {
    prng->mt[i] = (prng->mt[i] ^ ((prng->mt[i-1] ^ (prng->mt[i-1] >> 30)) * 1664525U)) + init_key[j] + j; /* non linear */
    i++; j++;
    if (i >= N)
    {
      prng->mt[0] = prng->mt[N-1];
      i=1;
    }
    if (j >= key_length)
    {
      j = 0;
    }
  }
  for (k = N-1; k; k--)
  {
    prng->mt[i] = (prng->mt[i] ^ ((prng->mt[i-1] ^ (prng->mt[i-1] >> 30)) * 1566083941U)) - i; /* non linear */
    i++;
    if (i >= N)
    {
      prng->mt[0] = prng->mt[N-1];
      i = 1;
    }
  }
  prng->mt[0] = 0x80000000U; /* MSB is 1; assuring non-zero initial array */
}

void ddsrt_prng_init_simple (ddsrt_prng_t *prng, uint32_t seed)
{
  init_genrand (prng, seed);
}

void ddsrt_prng_init (ddsrt_prng_t *prng, const struct ddsrt_prng_seed *seed)
{
  init_by_array (prng, seed->key, sizeof (seed->key) / sizeof (seed->key[0]));
}

/* generates a random number on [0,0xffffffff]-interval */
uint32_t ddsrt_prng_random (ddsrt_prng_t *prng)
{
  /* mag01[x] = x * MATRIX_A  for x=0,1 */
  static const uint32_t mag01[2] = { 0x0U, MATRIX_A };
  uint32_t y;

  if (prng->mti >= N)
  {
    /* generate N words at one time */
    int kk;

    for (kk=0; kk < N-M; kk++)
    {
      y = (prng->mt[kk] & UPPER_MASK) | (prng->mt[kk+1] & LOWER_MASK);
      prng->mt[kk] = prng->mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1U];
    }
    for (; kk < N-1; kk++)
    {
      y = (prng->mt[kk] & UPPER_MASK) | (prng->mt[kk+1] & LOWER_MASK);
      prng->mt[kk] = prng->mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1U];
    }
    y = (prng->mt[N-1] & UPPER_MASK) | (prng->mt[0] & LOWER_MASK);
    prng->mt[N-1] = prng->mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1U];

    prng->mti = 0;
  }

  y = prng->mt[prng->mti++];

  /* Tempering */
  y ^= (y >> 11);
  y ^= (y << 7) & 0x9d2c5680U;
  y ^= (y << 15) & 0xefc60000U;
  y ^= (y >> 18);

  return y;
}

uint32_t ddsrt_random (void)
{
  uint32_t x;
  ddsrt_mutex_lock (&default_prng_lock);
  x = ddsrt_prng_random (&default_prng);
  ddsrt_mutex_unlock (&default_prng_lock);
  return x;
}

void ddsrt_random_init (void)
{
  ddsrt_prng_seed_t seed;
  if (!ddsrt_prng_makeseed (&seed))
  {
    static ddsrt_atomic_uint32_t counter = DDSRT_ATOMIC_UINT32_INIT (0);
    /* Poor man's initialisation */
    DDSRT_STATIC_ASSERT (sizeof (seed.key) / sizeof (seed.key[0]) >= 4);
    memset (&seed, 0, sizeof (seed));
    dds_time_t now = dds_time ();
    seed.key[0] = (uint32_t) ddsrt_getpid ();
    seed.key[1] = (uint32_t) ((uint64_t) now >> 32);
    seed.key[2] = (uint32_t) now;
    seed.key[3] = ddsrt_atomic_inc32_ov (&counter);
  }
  ddsrt_prng_init (&default_prng, &seed);
  ddsrt_mutex_init (&default_prng_lock);
}

void ddsrt_random_fini (void)
{
  ddsrt_mutex_destroy (&default_prng_lock);
}
