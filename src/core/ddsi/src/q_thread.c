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

#include "cyclonedds/ddsrt/cdtors.h"
#include "cyclonedds/ddsrt/heap.h"
#include "cyclonedds/ddsrt/log.h"
#include "cyclonedds/ddsrt/string.h"
#include "cyclonedds/ddsrt/sync.h"
#include "cyclonedds/ddsrt/threads.h"
#include "cyclonedds/ddsrt/misc.h"

#include "cyclonedds/ddsi/q_thread.h"
#include "cyclonedds/ddsi/ddsi_threadmon.h"
#include "cyclonedds/ddsi/q_log.h"
#include "cyclonedds/ddsi/q_config.h"
#include "cyclonedds/ddsi/q_globals.h"
#include "cyclonedds/ddsi/sysdeps.h"

struct thread_states thread_states;
ddsrt_thread_local struct thread_state1 *tsd_thread_state;

extern inline bool vtime_awake_p (vtime_t vtime);
extern inline bool vtime_asleep_p (vtime_t vtime);
extern inline bool vtime_gt (vtime_t vtime1, vtime_t vtime0);

extern inline struct thread_state1 *lookup_thread_state (void);
extern inline bool thread_is_asleep (void);
extern inline bool thread_is_awake (void);
extern inline void thread_state_asleep (struct thread_state1 *ts1);
extern inline void thread_state_awake (struct thread_state1 *ts1, const struct q_globals *gv);
extern inline void thread_state_awake_domain_ok (struct thread_state1 *ts1);
extern inline void thread_state_awake_fixed_domain (struct thread_state1 *ts1);
extern inline void thread_state_awake_to_awake_no_nest (struct thread_state1 *ts1);

static struct thread_state1 *init_thread_state (const char *tname, const struct q_globals *gv, enum thread_state state);
static void reap_thread_state (struct thread_state1 *ts1);

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
    /* The compiler doesn't realize that ts is large enough. */
    DDSRT_WARNING_MSVC_OFF(6386);
    for (uint32_t i = 0; i < thread_states.nthreads; i++)
    {
      thread_states.ts[i].state = THREAD_STATE_ZERO;
      ddsrt_atomic_st32 (&thread_states.ts[i].vtime, 0);
      memset (thread_states.ts[i].name, 0, sizeof (thread_states.ts[i].name));
    }
    DDSRT_WARNING_MSVC_ON(6386);
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
  reap_thread_state (ts1);
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
    assert (thread_states.ts[i].state != THREAD_STATE_ALIVE);
    others += (thread_states.ts[i].state == THREAD_STATE_LAZILY_CREATED);
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

ddsrt_attribute_no_sanitize (("thread"))
static struct thread_state1 *find_thread_state (ddsrt_thread_t tid)
{
  if (thread_states.ts) {
    for (uint32_t i = 0; i < thread_states.nthreads; i++)
    {
      if (ddsrt_thread_equal (thread_states.ts[i].tid, tid) && thread_states.ts[i].state != THREAD_STATE_ZERO)
        return &thread_states.ts[i];
    }
  }
  return NULL;
}

static void cleanup_thread_state (void *data)
{
  struct thread_state1 *ts = find_thread_state(ddsrt_thread_self());
  (void)data;
  if (ts)
  {
    assert(ts->state == THREAD_STATE_LAZILY_CREATED);
    assert(vtime_asleep_p(ddsrt_atomic_ld32 (&ts->vtime)));
    reset_thread_state(ts);
  }
  ddsrt_fini();
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
  if ((ts1 = init_thread_state (name, NULL, THREAD_STATE_LAZILY_CREATED)) != NULL) {
    ddsrt_init ();
    ts1->extTid = self;
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
  assert(ts1 != NULL);
  return ts1;
}

struct thread_context {
  struct thread_state1 *self;
  uint32_t (*f) (void *arg);
  void *arg;
};

static uint32_t create_thread_wrapper (void *ptr)
{
  uint32_t ret;
  struct thread_context *ctx = ptr;
  struct q_globals const * const gv = ddsrt_atomic_ldvoidp (&ctx->self->gv);
  if (gv)
    GVTRACE ("started new thread %"PRIdTID": %s\n", ddsrt_gettid (), ctx->self->name);
  ctx->self->tid = ddsrt_thread_self ();
  ret = ctx->f (ctx->arg);
  ddsrt_free (ctx);
  return ret;
}

static int find_free_slot (const char *name)
{
  for (uint32_t i = 0; i < thread_states.nthreads; i++)
    if (thread_states.ts[i].state == THREAD_STATE_ZERO)
      return (int) i;
  DDS_FATAL ("create_thread: %s: no free slot\n", name ? name : "(anon)");
  return -1;
}

const struct config_thread_properties_listelem *lookup_thread_properties (const struct config *config, const char *name)
{
  const struct config_thread_properties_listelem *e;
  for (e = config->thread_properties; e != NULL; e = e->next)
    if (strcmp (e->name, name) == 0)
      break;
  return e;
}

static struct thread_state1 *init_thread_state (const char *tname, const struct q_globals *gv, enum thread_state state)
{
  int cand;
  struct thread_state1 *ts;

  if ((cand = find_free_slot (tname)) < 0)
    return NULL;

  ts = &thread_states.ts[cand];
  ddsrt_atomic_stvoidp (&ts->gv, (struct q_globals *) gv);
  assert (vtime_asleep_p (ddsrt_atomic_ld32 (&ts->vtime)));
  ddsrt_strlcpy (ts->name, tname, sizeof (ts->name));
  ts->state = state;

  return ts;
}

static dds_return_t create_thread_int (struct thread_state1 **ts1, const struct q_globals *gv, struct config_thread_properties_listelem const * const tprops, const char *name, uint32_t (*f) (void *arg), void *arg)
{
  ddsrt_threadattr_t tattr;
  ddsrt_thread_t tid;
  struct thread_context *ctxt;
  ctxt = ddsrt_malloc (sizeof (*ctxt));
  ddsrt_mutex_lock (&thread_states.lock);

  *ts1 = init_thread_state (name, gv, THREAD_STATE_ALIVE);
  if (*ts1 == NULL)
    goto fatal;

  ctxt->self = *ts1;
  ctxt->f = f;
  ctxt->arg = arg;
  ddsrt_threadattr_init (&tattr);
  if (tprops != NULL)
  {
    if (!tprops->sched_priority.isdefault)
      tattr.schedPriority = tprops->sched_priority.value;
    tattr.schedClass = tprops->sched_class; /* explicit default value in the enum */
    if (!tprops->stack_size.isdefault)
      tattr.stackSize = tprops->stack_size.value;
  }
  if (gv)
  {
    GVTRACE ("create_thread: %s: class %d priority %"PRId32" stack %"PRIu32"\n", name, (int) tattr.schedClass, tattr.schedPriority, tattr.stackSize);
  }

  if (ddsrt_thread_create (&tid, name, &tattr, &create_thread_wrapper, ctxt) != DDS_RETCODE_OK)
  {
    (*ts1)->state = THREAD_STATE_ZERO;
    DDS_FATAL ("create_thread: %s: ddsrt_thread_create failed\n", name);
    goto fatal;
  }
  (*ts1)->extTid = tid; /* overwrite the temporary value with the correct external one */
  ddsrt_mutex_unlock (&thread_states.lock);
  return DDS_RETCODE_OK;
fatal:
  ddsrt_mutex_unlock (&thread_states.lock);
  ddsrt_free (ctxt);
  *ts1 = NULL;
  abort ();
  return DDS_RETCODE_ERROR;
}

dds_return_t create_thread_with_properties (struct thread_state1 **ts1, struct config_thread_properties_listelem const * const tprops, const char *name, uint32_t (*f) (void *arg), void *arg)
{
  return create_thread_int (ts1, NULL, tprops, name, f, arg);
}

dds_return_t create_thread (struct thread_state1 **ts1, const struct q_globals *gv, const char *name, uint32_t (*f) (void *arg), void *arg)
{
  struct config_thread_properties_listelem const * const tprops = lookup_thread_properties (&gv->config, name);
  return create_thread_int (ts1, gv, tprops, name, f, arg);
}

static void reap_thread_state (struct thread_state1 *ts1)
{
  ddsrt_mutex_lock (&thread_states.lock);
  ts1->state = THREAD_STATE_ZERO;
  ddsrt_mutex_unlock (&thread_states.lock);
}

dds_return_t join_thread (struct thread_state1 *ts1)
{
  dds_return_t ret;
  assert (ts1->state == THREAD_STATE_ALIVE);
  ret = ddsrt_thread_join (ts1->extTid, NULL);
  assert (vtime_asleep_p (ddsrt_atomic_ld32 (&ts1->vtime)));
  reap_thread_state (ts1);
  return ret;
}

void reset_thread_state (struct thread_state1 *ts1)
{
  if (ts1)
    reap_thread_state (ts1);
}

void log_stack_traces (const struct ddsrt_log_cfg *logcfg, const struct q_globals *gv)
{
  for (uint32_t i = 0; i < thread_states.nthreads; i++)
  {
    if (thread_states.ts[i].state != THREAD_STATE_ZERO &&
        (gv == NULL || ddsrt_atomic_ldvoidp (&thread_states.ts[i].gv) == gv))
    {
      log_stacktrace (logcfg, thread_states.ts[i].name, thread_states.ts[i].tid);
    }
  }
}

