// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_THREAD_H
#define DDSI_THREAD_H

#include <assert.h>
#include "dds/export.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/static_assert.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_domaingv;
struct ddsi_thread_state;

/* Things don't go wrong if DDSI_CACHE_LINE_SIZE is defined incorrectly,
   they just run slower because of false cache-line sharing. It can be
   discovered at run-time, but in practice it's 64 for most CPUs and
   128 for some. */
#define DDSI_CACHE_LINE_SIZE 64

typedef uint32_t ddsi_vtime_t;
#define DDSI_VTIME_NEST_MASK 0xfu
#define DDSI_VTIME_TIME_MASK 0xfffffff0u
#define DDSI_VTIME_TIME_SHIFT 4

enum ddsi_thread_state_kind {
  DDSI_THREAD_STATE_ZERO, /* known to be dead */
  DDSI_THREAD_STATE_STOPPED, /* internal thread, stopped-but-not-reaped */
  DDSI_THREAD_STATE_INIT, /* internal thread, initializing */
  DDSI_THREAD_STATE_LAZILY_CREATED, /* lazily created in response because an application used it. Reclaimed if the thread terminates, but not considered an error if all of Cyclone is shutdown while this thread hasn't terminated yet */
  DDSI_THREAD_STATE_ALIVE /* known to be alive - for Cyclone internal threads */
};

/*
 * vtime indicates progress for the garbage collector and the liveliness monitoring.
 *
 * vtime is updated without using atomic operations: only the owning thread updates
 * them, and the garbage collection mechanism and the liveliness monitoring only
 * observe the value
 *
 * gv is constant for internal threads, i.e., for threads with state = ALIVE
 * gv is non-NULL for internal threads except thread liveliness monitoring
 *
 * DDSI_THREAD_DEBUG enables some really costly debugging stuff that may not be fully
 * portable (I used it once, might as well keep it)
 */
#define DDSI_THREAD_DEBUG 0
#if DDSI_THREAD_DEBUG
#define DDSI_THREAD_NSTACKS 20
#define DDSI_THREAD_STACKDEPTH 10
#define THREAD_BASE_DEBUG \
  void *stks[DDSI_THREAD_NSTACKS][DDSI_THREAD_STACKDEPTH]; \
  int stks_depth[DDSI_THREAD_NSTACKS]; \
  int stks_idx;

/** @component thread_support */
void ddsi_thread_vtime_trace (struct ddsi_thread_state *thrst);
#else /* DDSI_THREAD_DEBUG */
#define THREAD_BASE_DEBUG
#define ddsi_thread_vtime_trace(thrst) do { } while (0)
#endif /* DDSI_THREAD_DEBUG */

#define THREAD_BASE                             \
  ddsrt_atomic_uint32_t vtime;                  \
  enum ddsi_thread_state_kind state;            \
  ddsrt_atomic_voidp_t gv;                      \
  ddsrt_thread_t tid;                           \
  uint32_t (*f) (void *arg);                    \
  void *f_arg;                                  \
  THREAD_BASE_DEBUG /* note: no semicolon! */   \
  char name[24] /* note: no semicolon! */

struct ddsi_thread_state_base {
  THREAD_BASE;
};

/* GCC has a nifty feature allowing the specification of the required
   alignment: __attribute__ ((aligned (DDSI_CACHE_LINE_SIZE))) in this
   case. Many other compilers implement it as well, but it is by no
   means a standard feature.  So we do it the old-fashioned way. */

struct ddsi_thread_state {
  THREAD_BASE;
  char pad[DDSI_CACHE_LINE_SIZE
           * ((sizeof (struct ddsi_thread_state_base) + DDSI_CACHE_LINE_SIZE - 1)
              / DDSI_CACHE_LINE_SIZE)
           - sizeof (struct ddsi_thread_state_base)];
};
#undef THREAD_BASE

struct ddsi_thread_states {
  ddsrt_mutex_t lock;
  ddsrt_atomic_voidp_t thread_states_head;
};

extern struct ddsi_thread_states thread_states;

// thread_local cannot (and doesn't need to?) be exported on Windows
#if defined _WIN32 && !defined __MINGW32__
extern ddsrt_thread_local struct ddsi_thread_state *tsd_thread_state;
#else
DDS_EXPORT extern ddsrt_thread_local struct ddsi_thread_state *tsd_thread_state;
#endif

/** @component thread_support */
void ddsi_thread_states_init (void);

/** @component thread_support */
bool ddsi_thread_states_fini (void);

/** @component thread_support */
dds_return_t ddsi_create_thread (struct ddsi_thread_state **thrst, const struct ddsi_domaingv *gv, const char *name, uint32_t (*f) (void *arg), void *arg);

/** @component thread_support */
dds_return_t ddsi_join_thread (struct ddsi_thread_state *thrst);

/** @component thread_support */
DDS_EXPORT struct ddsi_thread_state *ddsi_lookup_thread_state_real (void);

/** @component thread_support */
DDS_INLINE_EXPORT inline struct ddsi_thread_state *ddsi_lookup_thread_state (void) {
  struct ddsi_thread_state *thrst = tsd_thread_state;
  if (thrst)
    return thrst;
  else
    return ddsi_lookup_thread_state_real ();
}

/** @component thread_support */
inline bool ddsi_vtime_awake_p (ddsi_vtime_t vtime)
{
  return (vtime & DDSI_VTIME_NEST_MASK) != 0;
}

/** @component thread_support */
inline bool ddsi_vtime_asleep_p (ddsi_vtime_t vtime)
{
  return (vtime & DDSI_VTIME_NEST_MASK) == 0;
}

/** @component thread_support */
inline bool ddsi_thread_is_awake (void)
{
  struct ddsi_thread_state *thrst = ddsi_lookup_thread_state ();
  ddsi_vtime_t vt = ddsrt_atomic_ld32 (&thrst->vtime);
  return ddsi_vtime_awake_p (vt);
}

/** @component thread_support */
inline bool ddsi_thread_is_asleep (void)
{
  struct ddsi_thread_state *thrst = ddsi_lookup_thread_state ();
  ddsi_vtime_t vt = ddsrt_atomic_ld32 (&thrst->vtime);
  return ddsi_vtime_asleep_p (vt);
}

/** @component thread_support */
inline void ddsi_thread_state_asleep (struct ddsi_thread_state *thrst)
{
  ddsi_vtime_t vt = ddsrt_atomic_ld32 (&thrst->vtime);
  assert (ddsi_vtime_awake_p (vt));
  /* nested calls a rare and an extra fence doesn't break things */
  ddsrt_atomic_fence_rel ();
  ddsi_thread_vtime_trace (thrst);
  if ((vt & DDSI_VTIME_NEST_MASK) == 1)
    vt += (1u << DDSI_VTIME_TIME_SHIFT) - 1u;
  else
    vt -= 1u;
  ddsrt_atomic_st32 (&thrst->vtime, vt);
}

/** @component thread_support */
inline void ddsi_thread_state_awake (struct ddsi_thread_state *thrst, const struct ddsi_domaingv *gv)
{
  ddsi_vtime_t vt = ddsrt_atomic_ld32 (&thrst->vtime);
  assert ((vt & DDSI_VTIME_NEST_MASK) < DDSI_VTIME_NEST_MASK);
  assert (gv != NULL);
  assert (thrst->state != DDSI_THREAD_STATE_ALIVE || gv == ddsrt_atomic_ldvoidp (&thrst->gv));
  ddsi_thread_vtime_trace (thrst);
  ddsrt_atomic_stvoidp (&thrst->gv, (struct ddsi_domaingv *) gv);
  ddsrt_atomic_fence_stst ();
  ddsrt_atomic_st32 (&thrst->vtime, vt + 1u);
  /* nested calls a rare and an extra fence doesn't break things */
  ddsrt_atomic_fence_acq ();
}

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_THREAD_H */
