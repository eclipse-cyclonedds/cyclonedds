// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/ddsrt/mh3.h"

#define DDSRT_MH3_ROTL32(x,r) (((x) << (r)) | ((x) >> (32 - (r))))

// Really
// http://code.google.com/p/smhasher/source/browse/trunk/MurmurHash3.cpp,
// MurmurHash3_x86_32

uint32_t ddsrt_mh3 (const void *key, size_t len, uint32_t seed)
{
  const uint8_t *data = (const uint8_t *) key;
  const intptr_t nblocks = (intptr_t) (len / 4);
  const uint32_t c1 = 0xcc9e2d51;
  const uint32_t c2 = 0x1b873593;

  uint32_t h1 = seed;

  if(len){
    const uint32_t *blocks = (const uint32_t *) (data + nblocks * 4);
    for (intptr_t i = -nblocks; i; i++)
    {
      uint32_t k1 = blocks[i];

      k1 *= c1;
      k1 = DDSRT_MH3_ROTL32 (k1, 15);
      k1 *= c2;

      h1 ^= k1;
      h1 = DDSRT_MH3_ROTL32 (h1, 13);
      h1 = h1 * 5 + 0xe6546b64;
    }

    const uint8_t *tail = data + nblocks * 4;
    uint32_t k1 = 0;
    switch (len & 3)
    {
      case 3:
        k1 ^= (uint32_t) tail[2] << 16;
        /* FALLS THROUGH */
      case 2:
        k1 ^= (uint32_t) tail[1] << 8;
        /* FALLS THROUGH */
      case 1:
        k1 ^= (uint32_t) tail[0];
        k1 *= c1;
        k1 = DDSRT_MH3_ROTL32 (k1, 15);
        k1 *= c2;
        h1 ^= k1;
        /* FALLS THROUGH */
    }
  }

  /* finalization */
  h1 ^= (uint32_t) len;
  h1 ^= h1 >> 16;
  h1 *= 0x85ebca6b;
  h1 ^= h1 >> 13;
  h1 *= 0xc2b2ae35;
  h1 ^= h1 >> 16;
  return h1;
}
