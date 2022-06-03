/*
 * Copyright(c) 2006 to 2022 ZettaScale Technology and others
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
#include "dds/ddsi/ddsi_config_impl.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/sysdeps.h"

struct thread_states thread_states;
ddsrt_thread_local struct thread_state *tsd_thread_state;

DDS_EXPORT extern inline bool vtime_awake_p (vtime_t vtime);
DDS_EXPORT extern inline bool vtime_asleep_p (vtime_t vtime);
DDS_EXPORT extern inline bool vtime_gt (vtime_t vtime1, vtime_t vtime0);

DDS_EXPORT extern inline struct thread_state *lookup_thread_state (void);
DDS_EXPORT extern inline bool thread_is_asleep (void);
DDS_EXPORT extern inline bool thread_is_awake (void);
DDS_EXPORT extern inline void thread_state_asleep (struct thread_state *thrst);
DDS_EXPORT extern inline void thread_state_awake (struct thread_state *thrst, const struct ddsi_domaingv *gv);
DDS_EXPORT extern inline void thread_state_awake_domain_ok (struct thread_state *thrst);
DDS_EXPORT extern inline void thread_state_awake_fixed_domain (struct thread_state *thrst);
DDS_EXPORT extern inline void thread_state_awake_to_awake_no_nest (struct thread_state *thrst);

static struct thread_state *init_thread_state (const char *tname, const struct ddsi_domaingv *gv, enum thread_state_kind state);
static void reap_thread_state (struct thread_state *thrst, bool in_thread_states_fini);

DDSRT_STATIC_ASSERT(THREAD_STATE_ZERO == 0 &&
                    THREAD_STATE_ZERO < THREAD_STATE_STOPPED &&
                    THREAD_STATE_STOPPED < THREAD_STATE_INIT &&
                    THREAD_STATE_INIT < THREAD_STATE_LAZILY_CREATED &&
                    THREAD_STATE_INIT < THREAD_STATE_ALIVE);

#if Q_THREAD_DEBUG
#include <execinfo.h>

void thread_vtime_trace (struct thread_state *thrst)
{
  if (++thrst->stks_idx == Q_THREAD_NSTACKS)
    thrst->stks_idx = 0;
  const int i = thrst->stks_idx;
  thrst->stks_depth[i] = backtrace (thrst->stks[i], Q_THREAD_STACKDEPTH);
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

void thread_states_init (void)
{
  /* Called with ddsrt's singleton mutex held (see dds_init/fini).  Application threads
     remaining alive can result in thread_states remaining alive, and as those thread
     cache the address, we must then re-use the old array. */
  if (ddsrt_atomic_ldvoidp (&thread_states.thread_states_head) == NULL)
  {
    struct thread_states_list *tslist;
    ddsrt_mutex_init (&thread_states.lock);
    tslist = ddsrt_malloc_aligned_cacheline (sizeof (*tslist));
    tslist->next = NULL;
    tslist->nthreads = THREAD_STATE_BATCH;
    memset (tslist->thrst, 0, sizeof (tslist->thrst));
    ddsrt_atomic_stvoidp (&thread_states.thread_states_head, tslist);
  }

  /* This thread should be at the same address as before, or never have had a slot
     in the past.  Also, allocate a slot for this thread if it didn't have one yet
     (not strictly required, but it'll get one eventually anyway, and this makes
     it rather more clear). */
#ifndef NDEBUG
  struct thread_state * const ts0 = tsd_thread_state;
#endif
  struct thread_state * const thrst = lookup_thread_state_real ();
  assert (ts0 == NULL || ts0 == thrst);
  (void) thrst;
}

bool thread_states_fini (void)
{
  /* Calling thread is the one shutting everything down, so it certainly won't (well, shouldn't)
     need its slot anymore.  Clean it up so that if all other threads happen to have been stopped
     already, we can release all resources. */
  struct thread_state *thrst = lookup_thread_state ();
  assert (vtime_asleep_p (ddsrt_atomic_ld32 (&thrst->vtime)));
  reap_thread_state (thrst, true);
  tsd_thread_state = NULL;

  /* Some applications threads that, at some point, required a thread state, may still be around.
     Of those, the cleanup routine is invoked when the thread terminates.  This should be rewritten
     to not rely on this global thing and with each thread owning its own bit state, e.g., linked
     together in a list to give the GC access to it.  Until then, we can't release these resources
     if there are still users. */
  uint32_t others = 0;
  ddsrt_mutex_lock (&thread_states.lock);
  for (struct thread_states_list *cur = ddsrt_atomic_ldvoidp (&thread_states.thread_states_head); cur; cur = cur->next)
  {
    for (uint32_t i = 0; i < THREAD_STATE_BATCH; i++)
    {
      switch (cur->thrst[i].state)
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
  }
  ddsrt_mutex_unlock (&thread_states.lock);
  if (others == 0)
  {
    // no other threads active, no need to worry about atomicity
    ddsrt_mutex_destroy (&thread_states.lock);
    struct thread_states_list *head = ddsrt_atomic_ldvoidp (&thread_states.thread_states_head);
    ddsrt_atomic_stvoidp (&thread_states.thread_states_head, NULL);
    while (head)
    {
      struct thread_states_list *next = head->next;
      ddsrt_free_aligned (head);
      head = next;
    }
    return true;
  }
  else
  {
    return false;
  }
}

static struct thread_state *find_thread_state (ddsrt_thread_t tid)
{
  if (ddsrt_atomic_ldvoidp (&thread_states.thread_states_head))
  {
    ddsrt_mutex_lock (&thread_states.lock);
    for (struct thread_states_list *cur = ddsrt_atomic_ldvoidp (&thread_states.thread_states_head); cur; cur = cur->next)
    {
      for (uint32_t i = 0; i < THREAD_STATE_BATCH; i++)
      {
        if (cur->thrst[i].state > THREAD_STATE_INIT && ddsrt_thread_equal (cur->thrst[i].tid, tid))
        {
          ddsrt_mutex_unlock (&thread_states.lock);
          return &cur->thrst[i];
        }
      }
    }
    ddsrt_mutex_unlock (&thread_states.lock);
  }
  return NULL;
}

static void cleanup_thread_state (void *data)
{
  struct thread_state *thrst = find_thread_state (ddsrt_thread_self ());
  (void) data;
  if (thrst)
  {
    assert (thrst->state == THREAD_STATE_LAZILY_CREATED);
    assert (vtime_asleep_p (ddsrt_atomic_ld32 (&thrst->vtime)));
    reap_thread_state (thrst, false);
  }
  ddsrt_fini ();
}

static struct thread_state *lazy_create_thread_state (ddsrt_thread_t self)
{
  /* This situation only arises for threads that were not created using
     create_thread, aka application threads. Since registering thread
     state should be fully automatic the name is simply the identifier. */
  struct thread_state *thrst;
  char name[128];
  ddsrt_thread_getname (name, sizeof (name));
  ddsrt_mutex_lock (&thread_states.lock);
  if ((thrst = init_thread_state (name, NULL, THREAD_STATE_LAZILY_CREATED)) != NULL)
  {
    ddsrt_init ();
    thrst->tid = self;
    DDS_LOG (DDS_LC_TRACE, "started application thread %s\n", name);
    ddsrt_thread_cleanup_push (&cleanup_thread_state, NULL);
  }
  ddsrt_mutex_unlock (&thread_states.lock);
  return thrst;
}

struct thread_state *lookup_thread_state_real (void)
{
  struct thread_state *thrst = tsd_thread_state;
  if (thrst == NULL)
  {
    ddsrt_thread_t self = ddsrt_thread_self ();
    if ((thrst = find_thread_state (self)) == NULL)
      thrst = lazy_create_thread_state (self);
    tsd_thread_state = thrst;
  }
  assert (thrst != NULL);
  return thrst;
}

static uint32_t create_thread_wrapper (void *ptr)
{
  struct thread_state * const thrst = ptr;
  struct ddsi_domaingv const * const gv = ddsrt_atomic_ldvoidp (&thrst->gv);
  if (gv)
    GVTRACE ("started new thread %"PRIdTID": %s\n", ddsrt_gettid (), thrst->name);
  assert (thrst->state == THREAD_STATE_INIT);
  tsd_thread_state = thrst;
  ddsrt_mutex_lock (&thread_states.lock);
  thrst->state = THREAD_STATE_ALIVE;
  ddsrt_mutex_unlock (&thread_states.lock);
  const uint32_t ret = thrst->f (thrst->f_arg);
  ddsrt_mutex_lock (&thread_states.lock);
  thrst->state = THREAD_STATE_STOPPED;
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

static struct thread_state *grow_thread_states (void)
{
  struct thread_states_list *x;
  if ((x = ddsrt_malloc_aligned_cacheline (sizeof (*x))) == NULL)
    return NULL;
  memset (x->thrst, 0, sizeof (x->thrst));
  do {
    x->next = ddsrt_atomic_ldvoidp (&thread_states.thread_states_head);
    x->nthreads = THREAD_STATE_BATCH + x->next->nthreads;
  } while (!ddsrt_atomic_casvoidp (&thread_states.thread_states_head, x->next, x));
  return &x->thrst[0];
}

static struct thread_state *get_available_thread_slot ()
{
  struct thread_states_list *cur;
  uint32_t i;
  for (cur = ddsrt_atomic_ldvoidp (&thread_states.thread_states_head); cur; cur = cur->next)
    for (i = 0; i < THREAD_STATE_BATCH; i++)
      if (cur->thrst[i].state == THREAD_STATE_ZERO)
        return &cur->thrst[i];
  return grow_thread_states ();
}

static struct thread_state *init_thread_state (const char *tname, const struct ddsi_domaingv *gv, enum thread_state_kind state)
{
  struct thread_state * const thrst = get_available_thread_slot ();
  if (thrst == NULL)
    return thrst;

  assert (vtime_asleep_p (ddsrt_atomic_ld32 (&thrst->vtime)));
  ddsrt_atomic_stvoidp (&thrst->gv, (struct ddsi_domaingv *) gv);
  (void) ddsrt_strlcpy (thrst->name, tname, sizeof (thrst->name));
  thrst->state = state;
  return thrst;
}

static dds_return_t create_thread_int (struct thread_state **ts1_out, const struct ddsi_domaingv *gv, struct ddsi_config_thread_properties_listelem const * const tprops, const char *name, uint32_t (*f) (void *arg), void *arg)
{
  ddsrt_threadattr_t tattr;
  struct thread_state *thrst;
  ddsrt_mutex_lock (&thread_states.lock);

  thrst = *ts1_out = init_thread_state (name, gv, THREAD_STATE_INIT);
  if (thrst == NULL)
    goto fatal;

  thrst->f = f;
  thrst->f_arg = arg;
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

  if (ddsrt_thread_create (&thrst->tid, name, &tattr, &create_thread_wrapper, thrst) != DDS_RETCODE_OK)
  {
    thrst->state = THREAD_STATE_ZERO;
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

dds_return_t create_thread_with_properties (struct thread_state **thrst, struct ddsi_config_thread_properties_listelem const * const tprops, const char *name, uint32_t (*f) (void *arg), void *arg)
{
  return create_thread_int (thrst, NULL, tprops, name, f, arg);
}

dds_return_t create_thread (struct thread_state **thrst, const struct ddsi_domaingv *gv, const char *name, uint32_t (*f) (void *arg), void *arg)
{
  struct ddsi_config_thread_properties_listelem const * const tprops = lookup_thread_properties (&gv->config, name);
  return create_thread_int (thrst, gv, tprops, name, f, arg);
}

static void reap_thread_state (struct thread_state *thrst, bool in_thread_states_fini)
{
  ddsrt_mutex_lock (&thread_states.lock);
  switch (thrst->state)
  {
    case THREAD_STATE_INIT:
    case THREAD_STATE_STOPPED:
    case THREAD_STATE_LAZILY_CREATED:
      thrst->state = THREAD_STATE_ZERO;
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

dds_return_t join_thread (struct thread_state *thrst)
{
  dds_return_t ret;
  ddsrt_mutex_lock (&thread_states.lock);
  switch (thrst->state)
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
  ret = ddsrt_thread_join (thrst->tid, NULL);
  assert (vtime_asleep_p (ddsrt_atomic_ld32 (&thrst->vtime)));
  reap_thread_state (thrst, false);
  return ret;
}

void log_stack_traces (const struct ddsrt_log_cfg *logcfg, const struct ddsi_domaingv *gv)
{
  for (struct thread_states_list *cur = ddsrt_atomic_ldvoidp (&thread_states.thread_states_head); cur; cur = cur->next)
  {
    for (uint32_t i = 0; i < THREAD_STATE_BATCH; i++)
    {
      struct thread_state * const thrst = &cur->thrst[i];
      if (thrst->state > THREAD_STATE_INIT && (gv == NULL || ddsrt_atomic_ldvoidp (&thrst->gv) == gv))
      {
        /* There's a race condition here that may cause us to call log_stacktrace with an invalid
           thread id (or even with a thread id mapping to a newly created thread that isn't really
           relevant in this context!) but this is an optional debug feature, so it's not worth the
           bother to avoid it. */
        log_stacktrace (logcfg, thrst->name, thrst->tid);
      }
    }
  }
}

