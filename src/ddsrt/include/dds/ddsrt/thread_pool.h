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
#ifndef DDSRT_THREAD_POOL_H
#define DDSRT_THREAD_POOL_H

#include <stdint.h>

#include "dds/export.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/threads.h"

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct ddsrt_thread_pool_s *ddsrt_thread_pool;

/*
  ddsrt_thread_pool_new: Creates a new thread pool. Returns NULL if
  cannot create initial set of threads. Threads are created with
  the optional atribute argument. Additional threads may be created
  on demand up to max_threads.
*/

DDS_EXPORT ddsrt_thread_pool ddsrt_thread_pool_new
(
  uint32_t threads,     /* Initial number of threads in pool (can be 0) */
  uint32_t max_threads, /* Maximum number of threads in pool (0 == infinite) */
  uint32_t max_queue,   /* Maximum number of queued requests (0 == infinite) */
  ddsrt_threadattr_t * attr   /* Attributes used to create pool threads (can be NULL) */
);

/* ddsrt_thread_pool_free: Frees pool, destroying threads. */

DDS_EXPORT void ddsrt_thread_pool_free (ddsrt_thread_pool pool);

/* ddsrt_thread_pool_purge: Purge threads from pool back to initial set. */

DDS_EXPORT void ddsrt_thread_pool_purge (ddsrt_thread_pool pool);

/*
  ddsrt_thread_pool_submit: Submit a thread function and associated argument
  to be invoked by a thread from the pool. If no threads are available a
  new thread will be created on demand to handle the function unless the
  pool thread maximum has been reached, in which case the function is queued.
  Note that if the pool queue has reached it's maximum DDS_RETCODE_TRY_AGAIN is returned.
*/

DDS_EXPORT dds_return_t ddsrt_thread_pool_submit
(
  ddsrt_thread_pool pool,  /* Thread pool instance */
  void (*fn) (void *arg),  /* Function to be invoked by thread from pool */
  void * arg            /* Argument passed to invoked function */
);

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_THREAD_POOL_H */
