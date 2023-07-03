// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSRT_CDTORS_H
#define DDSRT_CDTORS_H

#include "dds/export.h"
#include "dds/ddsrt/sync.h"

/**
 * @file cdtors.h
 * @brief Contains initializing and finalizing functions for some ddsrt units, and provides a singleton mutex and cond.
 * 
 * Initializes and finalizes mutexes of ddsrt/random and ddsrt/atomics, and for Windows also the ddsrt/socket
 * (not necessary for other platforms).
 */

#if defined (__cplusplus)
extern "C" {
#endif

/**
 * @brief Initialize ddsrt units
 * 
 * This doesn't need to be done more than once per process. It keeps track of the number of invocations,
 * and if the current call isn't the first one, it just waits for initializing (by the first one) to finish.
 * Note calling @ref ddsrt_init in parallel to the last invocation of @ref ddsrt_fini results in undefined behavior.
 */
void ddsrt_init(void);

/**
 * @brief Finalize ddsrt units
 * 
 * Since @ref ddsrt_init increments the reference count, it is decremented here. Only the last invocation
 * (when the reference count is 1) actually finalizes it.
 */
void ddsrt_fini(void);

/**
 * @brief Get a pointer to the global 'init_mutex'
 * 
 * @return ddsrt_mutex_t* pointer to the mutex
 */
ddsrt_mutex_t *ddsrt_get_singleton_mutex(void);

/**
 * @brief Get a pointer to the global 'init_cond'
 * 
 * @return ddsrt_cond_t* pointer to the condition
 */
ddsrt_cond_t *ddsrt_get_singleton_cond(void);

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_CDTORS_H */
