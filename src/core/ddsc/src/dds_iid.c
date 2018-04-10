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
#include "dds__iid.h"
#include "ddsi/q_time.h"
#include "ddsi/q_globals.h"

static os_mutex dds_iid_lock_g;
static dds_iid dds_iid_g;

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

uint64_t dds_iid_gen (void)
{
  uint64_t iid;
  union { uint64_t u64; uint32_t u32[2]; } tmp;

  os_mutexLock (&dds_iid_lock_g);
  tmp.u64 = ++dds_iid_g.counter;
  dds_tea_encrypt (tmp.u32, dds_iid_g.key);
  iid = tmp.u64;
  os_mutexUnlock (&dds_iid_lock_g);
  return iid;
}

void dds_iid_init (void)
{
  union { uint64_t u64; uint32_t u32[2]; } tmp;
  nn_wctime_t tnow = now ();

  os_mutexInit (&dds_iid_lock_g);

  dds_iid_g.key[0] = (uint32_t) ((uintptr_t) &dds_iid_g);
  dds_iid_g.key[1] = (uint32_t) tnow.v;
  dds_iid_g.key[2] = (uint32_t) (tnow.v >> 32);
  dds_iid_g.key[3] = 0xdeadbeef;

  tmp.u64 = 0;
  dds_tea_decrypt (tmp.u32, dds_iid_g.key);
  dds_iid_g.counter = tmp.u64;
}

void dds_iid_fini (void)
{
  os_mutexDestroy (&dds_iid_lock_g);
}
