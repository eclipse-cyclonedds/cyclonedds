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
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/misc.h"

#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/ddsi_threadmon.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/sysdeps.h"

struct thread_states thread_states;
ddsrt_thread_local struct thread_state1 *tsd_thread_state;

extern inline bool vtime_awake_p (vtime_t vtime);
extern inline bool vtime_asleep_p (vtime_t vtime);
extern inline bool vtime_gt (vtime_t vtime1, vtime_t vtime0);

extern inline struct thread_state1 *lookup_thread_state (void);
extern inline bool thread_is_asleep (void);
extern inline bool thread_is_awake (void);
extern inline void thread_state_asleep (struct thread_state1 *ts1);
extern inline void thread_state_awake (struct thread_state1 *ts1, const struct ddsi_domaingv *gv);
extern inline void thread_state_awake_domain_ok (struct thread_state1 *ts1);
extern inline void thread_state_awake_fixed_domain (struct thread_state1 *ts1);
extern inline void thread_state_awake_to_awake_no_nest (struct thread_state1 *ts1);

static struct thread_state1 *init_thread_state (const char *tname, const struct ddsi_domaingv *gv, enum thread_state state);
static void reap_thread_state (struct thread_state1 *ts1, bool in_thread_states_fini);

DDSRT_STATIC_ASSERT(THREAD_STATE_ZERO == 0 &&
                    THREAD_STATE_ZERO < THREAD_STATE_STOPPED &&
                    THREAD_STATE_STOPPED < THREAD_STATE_INIT &&
                    THREAD_STATE_INIT < THREAD_STATE_LAZILY_CREATED &&
                    THREAD_STATE_INIT < THREAD_STATE_ALIVE);

#if Q_THREAD_DEBUG
#include <execinfo.h>

void thread_vtime_trace (struct thread_state1 *ts1)
{
  if (++ts1->stks_idx == Q_THREAD_NSTACKS)
    ts1->stks_idx = 0;
  const int i = ts1->stks_idx;
  ts1->stks_depth[i] = backtrace (ts1->stks[i], Q_THREAD_STACKDEPTH);
}
#endif

static void *ddsrt_malloc_aligned_cacheline (size_t size)
{
  /* This wastes some space, but we use it only once and it isn't a
     huge amount of memory, just a little over a cache line.
     Alternatively, we good use valloc() and have it aligned to a page
     boundary, but that one isn't part of the O/S abstraction layer
     ... */
  const uintptr_t clm1 = CACHE_LINE_SIZE - 1;
  uintptr_t ptrA;
  void **pptr;
  void *ptr;
  ptr = ddsrt_malloc (size + CACHE_LINE_SIZE + sizeof (void *));
  ptrA = ((uintptr_t) ptr + sizeof (void *) + clm1) & ~clm1;
  pptr = (void **) ptrA;
  pptr[-1] = ptr;
  return (void *) ptrA;
}

static void ddsrt_free_aligned (void *ptr)
{
  if (ptr) {
    void **pptr = ptr;
    ddsrt_free (pptr[-1]);
  }
}

void thread_states_init (unsigned maxthreads)
{
  /* Called with ddsrt's singleton mutex held (see dds_init/fini).  Application threads
     remaining alive can result in thread_states remaining alive, and as those thread
     cache the address, we must then re-use the old array. */
  if (thread_states.ts == NULL)
  {
    ddsrt_mutex_init (&thread_states.lock);
    thread_states.nthreads = maxthreads;
    thread_states.ts = ddsrt_malloc_aligned_cacheline (maxthreads * sizeof (*thread_states.ts));
    memset (thread_states.ts, 0, maxthreads * sizeof (*thread_states.ts));
  }

  /* This thread should be at the same address as before, or never have had a slot
     in the past.  Also, allocate a slot for this thread if it didn't have one yet
     (not strictly required, but it'll get one eventually anyway, and this makes
     it rather more clear). */
#ifndef NDEBUG
  struct thread_state1 * const ts0 = tsd_thread_state;
#endif
  struct thread_state1 * const ts1 = lookup_thread_state_real ();
  assert (ts0 == NULL || ts0 == ts1);
  (void) ts1;
}

bool thread_states_fini (void)
{
  /* Calling thread is the one shutting everything down, so it certainly won't (well, shouldn't)
     need its slot anymore.  Clean it up so that if all other threads happen to have been stopped
     already, we can release all resources. */
  struct thread_state1 *ts1 = lookup_thread_state ();
  assert (vtime_asleep_p (ddsrt_atomic_ld32 (&ts1->vtime)));
  reap_thread_state (ts1, true);
  tsd_thread_state = NULL;

  /* Some applications threads that, at some point, required a thread state, may still be around.
     Of those, the cleanup routine is invoked when the thread terminates.  This should be rewritten
     to not rely on this global thing and with each thread owning its own bit state, e.g., linked
     together in a list to give the GC access to it.  Until then, we can't release these resources
     if there are still users. */
  uint32_t others = 0;
  ddsrt_mutex_lock (&thread_states.lock);
  for (uint32_t i = 0; i < thread_states.nthreads; i++)
  {
    switch (thread_states.ts[i].state)
    {
      case THREAD_STATE_ZERO:
        break;
      case THREAD_STATE_LAZILY_CREATED:
        others++;
        break;
      case THREAD_STATE_STOPPED:
      case THREAD_STATE_INIT:
      case THREAD_STATE_ALIVE:
        assert (0);
    }
  }
  ddsrt_mutex_unlock (&thread_states.lock);
  if (others == 0)
  {
    ddsrt_mutex_destroy (&thread_states.lock);
    ddsrt_free_aligned (thread_states.ts);
    thread_states.ts = NULL;
    return true;
  }
  else
  {
    return false;
  }
}

static struct thread_state1 *find_thread_state (ddsrt_thread_t tid)
{
  if (thread_states.ts)
  {
    ddsrt_mutex_lock (&thread_states.lock);
    for (uint32_t i = 0; i < thread_states.nthreads; i++)
    {
      if (thread_states.ts[i].state > THREAD_STATE_INIT && ddsrt_thread_equal (thread_states.ts[i].tid, tid))
      {
        ddsrt_mutex_unlock (&thread_states.lock);
        return &thread_states.ts[i];
      }
    }
    ddsrt_mutex_unlock (&thread_states.lock);
  }
  return NULL;
}

static void cleanup_thread_state (void *data)
{
  struct thread_state1 *ts1 = find_thread_state (ddsrt_thread_self ());
  (void) data;
  if (ts1)
  {
    assert (ts1->state == THREAD_STATE_LAZILY_CREATED);
    assert (vtime_asleep_p (ddsrt_atomic_ld32 (&ts1->vtime)));
    reap_thread_state (ts1, false);
  }
  ddsrt_fini ();
}

static struct thread_state1 *lazy_create_thread_state (ddsrt_thread_t self)
{
  /* This situation only arises for threads that were not created using
     create_thread, aka application threads. Since registering thread
     state should be fully automatic the name is simply the identifier. */
  struct thread_state1 *ts1;
  char name[128];
  ddsrt_thread_getname (name, sizeof (name));
  ddsrt_mutex_lock (&thread_states.lock);
  if ((ts1 = init_thread_state (name, NULL, THREAD_STATE_LAZILY_CREATED)) != NULL)
  {
    ddsrt_init ();
    ts1->tid = self;
    DDS_LOG (DDS_LC_TRACE, "started application thread %s\n", name);
    ddsrt_thread_cleanup_push (&cleanup_thread_state, NULL);
  }
  ddsrt_mutex_unlock (&thread_states.lock);
  return ts1;
}

struct thread_state1 *lookup_thread_state_real (void)
{
  struct thread_state1 *ts1 = tsd_thread_state;
  if (ts1 == NULL)
  {
    ddsrt_thread_t self = ddsrt_thread_self ();
    if ((ts1 = find_thread_state (self)) == NULL)
      ts1 = lazy_create_thread_state (self);
    tsd_thread_state = ts1;
  }
  assert (ts1 != NULL);
  return ts1;
}

static uint32_t create_thread_wrapper (void *ptr)
{
  struct thread_state1 * const ts1 = ptr;
  struct ddsi_domaingv const * const gv = ddsrt_atomic_ldvoidp (&ts1->gv);
  if (gv)
    GVTRACE ("started new thread %"PRIdTID": %s\n", ddsrt_gettid (), ts1->name);
  assert (ts1->state == THREAD_STATE_INIT);
  tsd_thread_state = ts1;
  ddsrt_mutex_lock (&thread_states.lock);
  ts1->state = THREAD_STATE_ALIVE;
  ddsrt_mutex_unlock (&thread_states.lock);
  const uint32_t ret = ts1->f (ts1->f_arg);
  ddsrt_mutex_lock (&thread_states.lock);
  ts1->state = THREAD_STATE_STOPPED;
  ddsrt_mutex_unlock (&thread_states.lock);
  tsd_thread_state = NULL;
  return ret;
}

const struct ddsi_config_thread_properties_listelem *lookup_thread_properties (const struct ddsi_config *config, const char *name)
{
  const struct ddsi_config_thread_properties_listelem *e;
  for (e = config->thread_properties; e != NULL; e = e->next)
    if (strcmp (e->name, name) == 0)
      break;
  return e;
}

static struct thread_state1 *init_thread_state (const char *tname, const struct ddsi_domaingv *gv, enum thread_state state)
{
  uint32_t i;
  for (i = 0; i < thread_states.nthreads; i++)
    if (thread_states.ts[i].state == THREAD_STATE_ZERO)
      break;
  if (i == thread_states.nthreads)
  {
    DDS_FATAL ("create_thread: %s: no free slot\n", tname ? tname : "(anon)");
    return NULL;
  }

  struct thread_state1 * const ts1 = &thread_states.ts[i];
  assert (vtime_asleep_p (ddsrt_atomic_ld32 (&ts1->vtime)));
  ddsrt_atomic_stvoidp (&ts1->gv, (struct ddsi_domaingv *) gv);
  (void) ddsrt_strlcpy (ts1->name, tname, sizeof (ts1->name));
  ts1->state = state;
  return ts1;
}

static dds_return_t create_thread_int (struct thread_state1 **ts1_out, const struct ddsi_domaingv *gv, struct ddsi_config_thread_properties_listelem const * const tprops, const char *name, uint32_t (*f) (void *arg), void *arg)
{
  ddsrt_threadattr_t tattr;
  struct thread_state1 *ts1;
  ddsrt_mutex_lock (&thread_states.lock);

  ts1 = *ts1_out = init_thread_state (name, gv, THREAD_STATE_INIT);
  if (ts1 == NULL)
    goto fatal;

  ts1->f = f;
  ts1->f_arg = arg;
  ddsrt_threadattr_init (&tattr);
  if (tprops != NULL)
  {
    if (!tprops->schedule_priority.isdefault)
      tattr.schedPriority = tprops->schedule_priority.value;
    tattr.schedClass = tprops->sched_class; /* explicit default value in the enum */
    if (!tprops->stack_size.isdefault)
      tattr.stackSize = tprops->stack_size.value;
  }
  if (gv)
  {
    GVTRACE ("create_thread: %s: class %d priority %"PRId32" stack %"PRIu32"\n", name, (int) tattr.schedClass, tattr.schedPriority, tattr.stackSize);
  }

  if (ddsrt_thread_create (&ts1->tid, name, &tattr, &create_thread_wrapper, ts1) != DDS_RETCODE_OK)
  {
    ts1->state = THREAD_STATE_ZERO;
    DDS_FATAL ("create_thread: %s: ddsrt_thread_create failed\n", name);
    goto fatal;
  }
  ddsrt_mutex_unlock (&thread_states.lock);
  return DDS_RETCODE_OK;
fatal:
  ddsrt_mutex_unlock (&thread_states.lock);
  *ts1_out = NULL;
  abort ();
  return DDS_RETCODE_ERROR;
}

dds_return_t create_thread_with_properties (struct thread_state1 **ts1, struct ddsi_config_thread_properties_listelem const * const tprops, const char *name, uint32_t (*f) (void *arg), void *arg)
{
  return create_thread_int (ts1, NULL, tprops, name, f, arg);
}

dds_return_t create_thread (struct thread_state1 **ts1, const struct ddsi_domaingv *gv, const char *name, uint32_t (*f) (void *arg), void *arg)
{
  struct ddsi_config_thread_properties_listelem const * const tprops = lookup_thread_properties (&gv->config, name);
  return create_thread_int (ts1, gv, tprops, name, f, arg);
}

static void reap_thread_state (struct thread_state1 *ts1, bool in_thread_states_fini)
{
  ddsrt_mutex_lock (&thread_states.lock);
  switch (ts1->state)
  {
    case THREAD_STATE_INIT:
    case THREAD_STATE_STOPPED:
    case THREAD_STATE_LAZILY_CREATED:
      ts1->state = THREAD_STATE_ZERO;
      break;
    case THREAD_STATE_ZERO:
      // Trying to reap a deceased thread twice is not a good thing and it
      // doesn't normally happen.  On Windows, however, a C++ process that
      // has only guard conditions and waitsets alive when it leaves main
      // may end up deleting those in a global destructor.  Those global
      // destructors on Windows are weird: they run after all other threads
      // have been killed by Windows and after the thread finalization
      // routine for the calling thread has been called.
      //
      // That means thread_states_fini() may not see its own thread as
      // alive anymore.  It also means that you cannot ever rely on global
      // destructors to shut down Cyclone in Windows.
#ifdef _WIN32
      assert (in_thread_states_fini);
#else
      assert (0);
#endif
      (void) in_thread_states_fini;
      break;
    case THREAD_STATE_ALIVE:
      assert (0);
  }
  ddsrt_mutex_unlock (&thread_states.lock);
}

dds_return_t join_thread (struct thread_state1 *ts1)
{
  dds_return_t ret;
  ddsrt_mutex_lock (&thread_states.lock);
  switch (ts1->state)
  {
    case THREAD_STATE_INIT:
    case THREAD_STATE_STOPPED:
    case THREAD_STATE_ALIVE:
      break;
    case THREAD_STATE_ZERO:
    case THREAD_STATE_LAZILY_CREATED:
      assert (0);
  }
  ddsrt_mutex_unlock (&thread_states.lock);
  ret = ddsrt_thread_join (ts1->tid, NULL);
  assert (vtime_asleep_p (ddsrt_atomic_ld32 (&ts1->vtime)));
  reap_thread_state (ts1, false);
  return ret;
}

void log_stack_traces (const struct ddsrt_log_cfg *logcfg, const struct ddsi_domaingv *gv)
{
  for (uint32_t i = 0; i < thread_states.nthreads; i++)
  {
    struct thread_state1 * const ts1 = &thread_states.ts[i];
    if (ts1->state > THREAD_STATE_INIT && (gv == NULL || ddsrt_atomic_ldvoidp (&ts1->gv) == gv))
    {
      /* There's a race condition here that may cause us to call log_stacktrace with an invalid
         thread id (or even with a thread id mapping to a newly created thread that isn't really
         relevant in this context!) but this is an optional debug feature, so it's not worth the
         bother to avoid it. */
      log_stacktrace (logcfg, ts1->name, ts1->tid);
    }
  }
}

