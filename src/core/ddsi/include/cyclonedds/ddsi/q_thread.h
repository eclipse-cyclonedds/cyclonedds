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
#ifndef Q_THREAD_H
#define Q_THREAD_H

#include <assert.h>
#include "cyclonedds/export.h"
#include "cyclonedds/ddsrt/atomics.h"
#include "cyclonedds/ddsrt/sync.h"
#include "cyclonedds/ddsrt/threads.h"
#include "cyclonedds/ddsrt/static_assert.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* Things don't go wrong if CACHE_LINE_SIZE is defined incorrectly,
   they just run slower because of false cache-line sharing. It can be
   discovered at run-time, but in practice it's 64 for most CPUs and
   128 for some. */
#define CACHE_LINE_SIZE 64

typedef uint32_t vtime_t;
typedef int32_t svtime_t; /* signed version */
#define VTIME_NEST_MASK 0xfu
#define VTIME_TIME_MASK 0xfffffff0u
#define VTIME_TIME_SHIFT 4

/* GCC has a nifty feature allowing the specification of the required
   alignment: __attribute__ ((aligned (CACHE_LINE_SIZE))) in this
   case. Many other compilers implement it as well, but it is by no
   means a standard feature.  So we do it the old-fashioned way. */


/* These strings are used to indicate the required scheduling class to the "create_thread()" */
#define Q_THREAD_SCHEDCLASS_REALTIME  "Realtime"
#define Q_THREAD_SCHEDCLASS_TIMESHARE "Timeshare"

/* When this value is used, the platform default for scheduling priority will be used */
#define Q_THREAD_SCHEDPRIO_DEFAULT 0

enum thread_state {
  THREAD_STATE_ZERO, /* known to be dead */
  THREAD_STATE_LAZILY_CREATED, /* lazily created in response because an application used it. Reclaimed if the thread terminates, but not considered an error if all of Cyclone is shutdown while this thread hasn't terminated yet */
  THREAD_STATE_ALIVE /* known to be alive - for Cyclone internal threads */
};

struct q_globals;
struct config;
struct ddsrt_log_cfg;

/*
 * vtime indicates progress for the garbage collector and the liveliness monitoring.
 *
 * vtime is updated without using atomic operations: only the owning thread updates
 * them, and the garbage collection mechanism and the liveliness monitoring only
 * observe the value
 *
 * gv is constant for internal threads, i.e., for threads with state = ALIVE
 * gv is non-NULL for internal threads except thread liveliness monitoring
 */
#define THREAD_BASE                             \
  ddsrt_atomic_uint32_t vtime;                  \
  ddsrt_atomic_voidp_t gv;                      \
  enum thread_state state;                      \
  ddsrt_thread_t tid;                           \
  ddsrt_thread_t extTid;                        \
  char name[24] /* note: no semicolon! */

struct thread_state_base {
  THREAD_BASE;
};

struct thread_state1 {
  THREAD_BASE;
  char pad[CACHE_LINE_SIZE
           * ((sizeof (struct thread_state_base) + CACHE_LINE_SIZE - 1)
              / CACHE_LINE_SIZE)
           - sizeof (struct thread_state_base)];
};
#undef THREAD_BASE

struct thread_states {
  ddsrt_mutex_t lock;
  uint32_t nthreads;
  struct thread_state1 *ts; /* [nthreads] */
};

extern DDS_EXPORT struct thread_states thread_states;
extern ddsrt_thread_local struct thread_state1 *tsd_thread_state;

DDS_EXPORT void thread_states_init (unsigned maxthreads);
DDS_EXPORT bool thread_states_fini (void);

DDS_EXPORT const struct config_thread_properties_listelem *lookup_thread_properties (const struct config *config, const char *name);
DDS_EXPORT dds_return_t create_thread_with_properties (struct thread_state1 **ts1, struct config_thread_properties_listelem const * const tprops, const char *name, uint32_t (*f) (void *arg), void *arg);
DDS_EXPORT dds_return_t create_thread (struct thread_state1 **ts, const struct q_globals *gv, const char *name, uint32_t (*f) (void *arg), void *arg);
DDS_EXPORT struct thread_state1 *lookup_thread_state_real (void);
DDS_EXPORT dds_return_t join_thread (struct thread_state1 *ts1);
DDS_EXPORT void log_stack_traces (const struct ddsrt_log_cfg *logcfg, const struct q_globals *gv);
DDS_EXPORT void reset_thread_state (struct thread_state1 *ts1);
DDS_EXPORT int thread_exists (const char *name);

DDS_EXPORT inline struct thread_state1 *lookup_thread_state (void) {
  struct thread_state1 *ts1 = tsd_thread_state;
  if (ts1)
    return ts1;
  else
    return lookup_thread_state_real ();
}

DDS_EXPORT inline bool vtime_awake_p (vtime_t vtime)
{
  return (vtime & VTIME_NEST_MASK) != 0;
}

DDS_EXPORT inline bool vtime_asleep_p (vtime_t vtime)
{
  return (vtime & VTIME_NEST_MASK) == 0;
}

DDS_EXPORT inline bool vtime_gt (vtime_t vtime1, vtime_t vtime0)
{
  DDSRT_STATIC_ASSERT_CODE (sizeof (vtime_t) == sizeof (svtime_t));
  return (svtime_t) ((vtime1 & VTIME_TIME_MASK) - (vtime0 & VTIME_TIME_MASK)) > 0;
}

DDS_EXPORT inline bool thread_is_awake (void)
{
  struct thread_state1 *ts = lookup_thread_state ();
  vtime_t vt = ddsrt_atomic_ld32 (&ts->vtime);
  return vtime_awake_p (vt);
}

DDS_EXPORT inline bool thread_is_asleep (void)
{
  struct thread_state1 *ts = lookup_thread_state ();
  vtime_t vt = ddsrt_atomic_ld32 (&ts->vtime);
  return vtime_asleep_p (vt);
}

DDS_EXPORT inline void thread_state_asleep (struct thread_state1 *ts1)
{
  vtime_t vt = ddsrt_atomic_ld32 (&ts1->vtime);
  assert (vtime_awake_p (vt));
  /* nested calls a rare and an extra fence doesn't break things */
  ddsrt_atomic_fence_rel ();
  if ((vt & VTIME_NEST_MASK) == 1)
    vt += (1u << VTIME_TIME_SHIFT) - 1u;
  else
    vt -= 1u;
  ddsrt_atomic_st32 (&ts1->vtime, vt);
}

DDS_EXPORT inline void thread_state_awake (struct thread_state1 *ts1, const struct q_globals *gv)
{
  vtime_t vt = ddsrt_atomic_ld32 (&ts1->vtime);
  assert ((vt & VTIME_NEST_MASK) < VTIME_NEST_MASK);
  assert (gv != NULL);
  assert (ts1->state != THREAD_STATE_ALIVE || gv == ddsrt_atomic_ldvoidp (&ts1->gv));
  ddsrt_atomic_stvoidp (&ts1->gv, (struct q_globals *) gv);
  ddsrt_atomic_fence_stst ();
  ddsrt_atomic_st32 (&ts1->vtime, vt + 1u);
  /* nested calls a rare and an extra fence doesn't break things */
  ddsrt_atomic_fence_acq ();
}

DDS_EXPORT inline void thread_state_awake_domain_ok (struct thread_state1 *ts1)
{
  vtime_t vt = ddsrt_atomic_ld32 (&ts1->vtime);
  assert ((vt & VTIME_NEST_MASK) < VTIME_NEST_MASK);
  assert (ddsrt_atomic_ldvoidp (&ts1->gv) != NULL);
  ddsrt_atomic_st32 (&ts1->vtime, vt + 1u);
  /* nested calls a rare and an extra fence doesn't break things */
  ddsrt_atomic_fence_acq ();
}

DDS_EXPORT inline void thread_state_awake_fixed_domain (struct thread_state1 *ts1)
{
  /* fixed domain -> must be an internal thread */
  assert (ts1->state == THREAD_STATE_ALIVE);
  thread_state_awake_domain_ok (ts1);
}

DDS_EXPORT inline void thread_state_awake_to_awake_no_nest (struct thread_state1 *ts1)
{
  vtime_t vt = ddsrt_atomic_ld32 (&ts1->vtime);
  assert ((vt & VTIME_NEST_MASK) == 1);
  ddsrt_atomic_fence_rel ();
  ddsrt_atomic_st32 (&ts1->vtime, vt + (1u << VTIME_TIME_SHIFT));
  ddsrt_atomic_fence_acq ();
}

#if defined (__cplusplus)
}
#endif

#endif /* Q_THREAD_H */
