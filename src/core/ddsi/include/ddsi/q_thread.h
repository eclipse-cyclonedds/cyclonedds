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

#include "os/os.h"
#include "ddsc/dds_export.h"
#include "ddsi/q_static_assert.h"

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
  THREAD_STATE_ZERO,
  THREAD_STATE_ALIVE
};

struct logbuf;

/*
 * watchdog indicates progress for the service lease liveliness mechsanism, while vtime
 * indicates progress for the Garbage collection purposes.
 *  vtime even : thread awake
 *  vtime odd  : thread asleep
 */
#define THREAD_BASE                             \
  volatile vtime_t vtime;                       \
  volatile vtime_t watchdog;                    \
  os_threadId tid;                              \
  os_threadId extTid;                           \
  enum thread_state state;                      \
  char *name /* note: no semicolon! */

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
  os_mutex lock;
  unsigned nthreads;
  struct thread_state1 *ts; /* [nthreads] */
};

extern DDS_EXPORT struct thread_states thread_states;
extern os_threadLocal struct thread_state1 *tsd_thread_state;

DDS_EXPORT void thread_states_init_static (void);
DDS_EXPORT void thread_states_init (_In_ unsigned maxthreads);
DDS_EXPORT void thread_states_fini (void);

DDS_EXPORT void upgrade_main_thread (void);
DDS_EXPORT void downgrade_main_thread (void);
DDS_EXPORT const struct config_thread_properties_listelem *lookup_thread_properties (const char *name);
DDS_EXPORT struct thread_state1 *create_thread (const char *name, uint32_t (*f) (void *arg), void *arg);
DDS_EXPORT struct thread_state1 *lookup_thread_state (void);
DDS_EXPORT struct thread_state1 *lookup_thread_state_real (void);
DDS_EXPORT int join_thread (_Inout_ struct thread_state1 *ts1);
DDS_EXPORT void log_stack_traces (void);
DDS_EXPORT struct thread_state1 *get_thread_state (_In_ os_threadId id);
DDS_EXPORT struct thread_state1 * init_thread_state (_In_z_ const char *tname);
DDS_EXPORT void reset_thread_state (_Inout_opt_ struct thread_state1 *ts1);
DDS_EXPORT int thread_exists (_In_z_ const char *name);

DDS_EXPORT inline int vtime_awake_p (_In_ vtime_t vtime)
{
  return (vtime % 2) == 0;
}

DDS_EXPORT inline int vtime_asleep_p (_In_ vtime_t vtime)
{
  return (vtime % 2) == 1;
}

DDS_EXPORT inline int vtime_gt (_In_ vtime_t vtime1, _In_ vtime_t vtime0)
{
  Q_STATIC_ASSERT_CODE (sizeof (vtime_t) == sizeof (svtime_t));
  return (svtime_t) (vtime1 - vtime0) > 0;
}

DDS_EXPORT inline void thread_state_asleep (_Inout_ struct thread_state1 *ts1)
{
  vtime_t vt = ts1->vtime;
  vtime_t wd = ts1->watchdog;
  if (vtime_awake_p (vt))
  {
    os_atomic_fence_rel ();
    ts1->vtime = vt + 1;
  }
  else
  {
    os_atomic_fence_rel ();
    ts1->vtime = vt + 2;
    os_atomic_fence_acq ();
  }

  if ( wd % 2 ){
    ts1->watchdog = wd + 2;
  } else {
    ts1->watchdog = wd + 1;
  }
}

DDS_EXPORT inline void thread_state_awake (_Inout_ struct thread_state1 *ts1)
{
  vtime_t vt = ts1->vtime;
  vtime_t wd = ts1->watchdog;
  if (vtime_asleep_p (vt))
    ts1->vtime = vt + 1;
  else
  {
    os_atomic_fence_rel ();
    ts1->vtime = vt + 2;
  }
  os_atomic_fence_acq ();

  if ( wd % 2 ){
    ts1->watchdog = wd + 1;
  } else {
    ts1->watchdog = wd + 2;
  }
}

DDS_EXPORT inline void thread_state_blocked (_Inout_ struct thread_state1 *ts1)
{
  vtime_t wd = ts1->watchdog;
  if ( wd % 2 ){
    ts1->watchdog = wd + 2;
  } else {
    ts1->watchdog = wd + 1;
  }
}

DDS_EXPORT inline void thread_state_unblocked (_Inout_ struct thread_state1 *ts1)
{
  vtime_t wd = ts1->watchdog;
  if ( wd % 2 ){
    ts1->watchdog = wd + 1;
  } else {
    ts1->watchdog = wd + 2;
  }
}

#if defined (__cplusplus)
}
#endif

#endif /* Q_THREAD_H */
