// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__THREAD_H
#define DDSI__THREAD_H

#include <assert.h>
#include "dds/export.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsi/ddsi_thread.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_domaingv;
struct ddsi_config;
struct ddsrt_log_cfg;

typedef int32_t ddsi_svtime_t; /* signed version of ddsi_vtime_t */

#define DDSI_THREAD_STATE_BATCH 32

struct ddsi_thread_states_list {
  struct ddsi_thread_state thrst[DDSI_THREAD_STATE_BATCH];
  struct ddsi_thread_states_list *next;
  uint32_t nthreads; // = DDSI_THREAD_STATE_BATCH + (next ? next->nthreads : 0)
};

/** @component thread_support */
const struct ddsi_config_thread_properties_listelem *ddsi_lookup_thread_properties (const struct ddsi_config *config, const char *name);

/** @component thread_support */
dds_return_t ddsi_create_thread_with_properties (struct ddsi_thread_state **thrst, struct ddsi_config_thread_properties_listelem const * const tprops, const char *name, uint32_t (*f) (void *arg), void *arg);

/** @component thread_support */
void ddsi_log_stack_traces (const struct ddsrt_log_cfg *logcfg, const struct ddsi_domaingv *gv);

/** @component thread_support */
inline bool ddsi_vtime_gt (ddsi_vtime_t vtime1, ddsi_vtime_t vtime0)
{
  DDSRT_STATIC_ASSERT_CODE (sizeof (ddsi_vtime_t) == sizeof (ddsi_svtime_t));
  return (ddsi_svtime_t) ((vtime1 & DDSI_VTIME_TIME_MASK) - (vtime0 & DDSI_VTIME_TIME_MASK)) > 0;
}

/** @component thread_support */
inline void ddsi_thread_state_awake_domain_ok (struct ddsi_thread_state *thrst)
{
  ddsi_vtime_t vt = ddsrt_atomic_ld32 (&thrst->vtime);
  assert ((vt & DDSI_VTIME_NEST_MASK) < DDSI_VTIME_NEST_MASK);
  assert (ddsrt_atomic_ldvoidp (&thrst->gv) != NULL);
  ddsi_thread_vtime_trace (thrst);
  ddsrt_atomic_st32 (&thrst->vtime, vt + 1u);
  /* nested calls a rare and an extra fence doesn't break things */
  ddsrt_atomic_fence_acq ();
}

/** @component thread_support */
inline void ddsi_thread_state_awake_fixed_domain (struct ddsi_thread_state *thrst)
{
  /* fixed domain -> must be an internal thread */
  assert (thrst->state == DDSI_THREAD_STATE_ALIVE);
  ddsi_thread_state_awake_domain_ok (thrst);
}

/** @component thread_support */
inline void ddsi_thread_state_awake_to_awake_no_nest (struct ddsi_thread_state *thrst)
{
  ddsi_vtime_t vt = ddsrt_atomic_ld32 (&thrst->vtime);
  assert ((vt & DDSI_VTIME_NEST_MASK) == 1);
  ddsrt_atomic_fence_rel ();
  ddsi_thread_vtime_trace (thrst);
  ddsrt_atomic_st32 (&thrst->vtime, vt + (1u << DDSI_VTIME_TIME_SHIFT));
  ddsrt_atomic_fence_acq ();
}

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__THREAD_H */
