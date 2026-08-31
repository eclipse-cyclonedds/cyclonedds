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

/**
 * @file random.h
 * 
 * Pseudo random number generator known as the Mersenne Twister.
 * It generates uint32_t from a uniform distribution in the range 0 to 0xffffffff (the maximum value of uint32_t).
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "dds/export.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define DDSRT_MT19937_N 624

/** @brief A 256-bit seed */
typedef struct ddsrt_prng_seed {
  uint32_t key[8];
} ddsrt_prng_seed_t;

/** @brief The random number generator's state object */
typedef struct ddsrt_prng {
  uint32_t mt[DDSRT_MT19937_N];
  uint32_t mti;
} ddsrt_prng_t;

/**
 * @brief Initialize the hidden, global pseudo random number generator for @ref ddsrt_random
 * 
 * Initializes a hidden @ref ddsrt_prng with a seed that should be different on each call.
 * Also initializes a hidden mutex, so multiple threads can share the hidden state object.
 * With this method you can't access the seed. If that is desired,
 * use @ref ddsrt_prng_init_simple or @ref ddsrt_prng_init instead.
 * 
 * See @ref ddsrt_random_fini, @ref ddsrt_random
 */
DDS_EXPORT void ddsrt_random_init (void);

/**
 * @brief Finalize the pseudo random number generator
 * 
 * Destoys the hidden mutex initialized by @ref ddsrt_random_init.
 * 
 * See @ref ddsrt_random
 */
DDS_EXPORT void ddsrt_random_fini (void);

/**
 * @brief Initialize the pseudo random number generator with an uint32_t seed
 * 
 * @param[out] prng the state object to initialize
 * @param[in] seed the seed
 * 
 * See @ref ddsrt_prng_init
 */
DDS_EXPORT void ddsrt_prng_init_simple (ddsrt_prng_t *prng, uint32_t seed);

/**
 * @brief Generate a seed for use with @ref ddsrt_prng_init
 * 
 * It is possible to fail, which is reflected by the return value.
 * 
 * @param[out] seed the generated seed
 * @return true if success, false if failure
 */
DDS_EXPORT bool ddsrt_prng_makeseed (struct ddsrt_prng_seed *seed);

/**
 * @brief Initialize the pseudo random number generator with a @ref ddsrt_prng_seed
 * 
 * @param[out] prng the state object to initialize
 * @param[in] seed the seed
 * 
 * See @ref ddsrt_prng_init_simple, @ref ddsrt_prng_makeseed
 */
DDS_EXPORT void ddsrt_prng_init (ddsrt_prng_t *prng, const struct ddsrt_prng_seed *seed);

/**
 * @brief Sample an uint32_t from a uniform distribution in the range 0 to 0xffffffff
 * 
 * @param[in,out] prng the state object from which to generate the number
 * @return the sampled value
 * 
 * See @ref ddsrt_prng_init_simple, @ref ddsrt_prng_init
 */
DDS_EXPORT uint32_t ddsrt_prng_random (ddsrt_prng_t *prng);

/**
 * @brief Sample a name
 * 
 * Assembles a name by combining smaller strings randomly chosen from a fixed set.
 * The resulting string including null termination is copied into the buffer 'output'.
 * Names longer than (output_size - 1) are truncated.
 * 
 * @param[in,out] prng the state object from which to generate the name
 * @param[out] output the buffer into which the sampled name is copied
 * @param[in] output_size the maximum size of the buffer 'output' that may be used
 * @return the number of characters in the sampled name
 * 
 * See @ref ddsrt_prng_init_simple, @ref ddsrt_prng_init
 */
DDS_EXPORT size_t ddsrt_prng_random_name(ddsrt_prng_t *prng, char* output, size_t output_size);

/**
 * @brief Sample an uint32_t from a uniform distribution in the range 0 to 0xffffffff
 * 
 * - It uses the hidden @ref ddsrt_prng initialized by @ref ddsrt_random_init.
 * - Can be used by multiple threads.
 * 
 * @return the sampled value
 */
DDS_EXPORT uint32_t ddsrt_random (void);

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_RANDOM_H */
