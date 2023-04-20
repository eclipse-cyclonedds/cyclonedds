// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSRT_RANDOM_H
#define DDSRT_RANDOM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "dds/export.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define DDSRT_MT19937_N 624

typedef struct ddsrt_prng_seed {
  uint32_t key[8];
} ddsrt_prng_seed_t;

typedef struct ddsrt_prng {
  uint32_t mt[DDSRT_MT19937_N];
  uint32_t mti;
} ddsrt_prng_t;

void ddsrt_random_init (void);
void ddsrt_random_fini (void);
void ddsrt_prng_init_simple (ddsrt_prng_t *prng, uint32_t seed);
bool ddsrt_prng_makeseed (struct ddsrt_prng_seed *seed);
void ddsrt_prng_init (ddsrt_prng_t *prng, const struct ddsrt_prng_seed *seed);
uint32_t ddsrt_prng_random (ddsrt_prng_t *prng);
size_t ddsrt_prng_random_name(ddsrt_prng_t *prng, char* output, size_t output_size);

DDS_EXPORT uint32_t ddsrt_random (void);

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_RANDOM_H */
