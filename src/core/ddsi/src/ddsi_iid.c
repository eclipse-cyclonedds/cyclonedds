// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/random.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsi/ddsi_iid.h"

static struct ddsi_iid ddsi_iid;

static void dds_tea_encrypt (uint32_t v[2], const uint32_t k[4])
{
  /* TEA encryption straight from Wikipedia */
  uint32_t v0=v[0], v1=v[1], sum=0, i;           /* set up */
  uint32_t delta=0x9e3779b9;                     /* a key schedule constant */
  uint32_t k0=k[0], k1=k[1], k2=k[2], k3=k[3];   /* cache key */
  for (i=0; i < 32; i++) {                       /* basic cycle start */
    sum += delta;
    v0 += ((v1<<4) + k0) ^ (v1 + sum) ^ ((v1>>5) + k1);
    v1 += ((v0<<4) + k2) ^ (v0 + sum) ^ ((v0>>5) + k3);
  }                                              /* end cycle */
  v[0]=v0; v[1]=v1;
}

static void dds_tea_decrypt (uint32_t v[2], const uint32_t k[4])
{
  uint32_t v0=v[0], v1=v[1], sum=0xC6EF3720, i;  /* set up */
  uint32_t delta=0x9e3779b9;                     /* a key schedule constant */
  uint32_t k0=k[0], k1=k[1], k2=k[2], k3=k[3];   /* cache key */
  for (i=0; i<32; i++) {                         /* basic cycle start */
    v1 -= ((v0<<4) + k2) ^ (v0 + sum) ^ ((v0>>5) + k3);
    v0 -= ((v1<<4) + k0) ^ (v1 + sum) ^ ((v1>>5) + k1);
    sum -= delta;
  }                                              /* end cycle */
  v[0]=v0; v[1]=v1;
}

uint64_t ddsi_iid_gen (void)
{
  uint64_t iid;
  union { uint64_t u64; uint32_t u32[2]; } tmp;
  tmp.u64 = ddsrt_atomic_inc64_nv (&ddsi_iid.counter);
  dds_tea_encrypt (tmp.u32, ddsi_iid.key);
  iid = tmp.u64;
  return iid;
}

void ddsi_iid_init (void)
{
  union { uint64_t u64; uint32_t u32[2]; } tmp;
  ddsrt_prng_seed_t seed;
  DDSRT_STATIC_ASSERT (sizeof (seed.key) >= sizeof (ddsi_iid.key));
  // Try to get a good seed for the generator, and if this doesn't succeed,
  // fall back to extracting one from the global random generator that was
  // initialized "somehow".  If "makeseed" fails, most likely that generator
  // was initialized by the fallback procedure, and so not as good a source
  // of randomness as it would normally be.
  //
  // The reason for calling "makeseed" instead of relying on "ddsrt_random"
  // is forking: ddsrt gets initialized once and therefore the child process
  // gets the same random generator state as the parent. That means a process
  // doing
  //   main () {
  //     if (fork () == 0)
  //       create_participant
  //     else
  //       create_participant
  //   }
  // ends up with the same IIDs and (worse) the same GUIDs!  And that is not
  // supposed to ever happen.
  //
  // As "ddsi_iid_init" is called when the domain wakes up, this call to
  // "makeseed" should save the day.
  //
  // Note that we can't do pthread_atfork for the global random generator
  // (no matter how good well it would work for the forking itself) because
  // there is no way to *remove* the at-fork handler.
  if (ddsrt_prng_makeseed (&seed))
    memcpy (ddsi_iid.key, seed.key, sizeof (ddsi_iid.key));
  else
  {
    for (size_t i = 0; i < sizeof (ddsi_iid.key) / sizeof (ddsi_iid.key[0]); i++)
      ddsi_iid.key[i] = ddsrt_random ();
  }
  tmp.u64 = 0;
  dds_tea_decrypt (tmp.u32, ddsi_iid.key);
  ddsrt_atomic_st64 (&ddsi_iid.counter, tmp.u64);
}

void ddsi_iid_fini (void)
{
}
