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

#include "os/os.h"

#include "ddsi/q_thread.h"
#include "ddsi/q_servicelease.h"
#include "ddsi/q_error.h"
#include "ddsi/q_log.h"
#include "ddsi/q_config.h"
#include "ddsi/q_globals.h"
#include "ddsi/sysdeps.h"

static char main_thread_name[] = "main";

struct thread_states thread_states;
os_threadLocal struct thread_state1 *tsd_thread_state;


_Ret_bytecap_(size)
void * os_malloc_aligned_cacheline (_In_ size_t size)
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
  ptr = os_malloc (size + CACHE_LINE_SIZE + sizeof (void *));
  ptrA = ((uintptr_t) ptr + sizeof (void *) + clm1) & ~clm1;
  pptr = (void **) ptrA;
  pptr[-1] = ptr;
  return (void *) ptrA;
}

static void os_free_aligned ( _Pre_maybenull_ _Post_invalid_ void *ptr)
{
  if (ptr) {
    void **pptr = ptr;
    os_free (pptr[-1]);
  }
}

void thread_states_init_static (void)
{
    static struct thread_state1 ts =
        { .state = THREAD_STATE_ALIVE, .vtime = 1, .watchdog = 1, .name = "(anon)" };
    tsd_thread_state = &ts;
}

void thread_states_init (_In_ unsigned maxthreads)
{
  unsigned i;

  os_mutexInit (&thread_states.lock);
  thread_states.nthreads = maxthreads;
  thread_states.ts =
    os_malloc_aligned_cacheline (maxthreads * sizeof (*thread_states.ts));
  memset (thread_states.ts, 0, maxthreads * sizeof (*thread_states.ts));
/* The compiler doesn't realize that ts is large enough. */
OS_WARNING_MSVC_OFF(6386);
  for (i = 0; i < thread_states.nthreads; i++)
  {
    thread_states.ts[i].state = THREAD_STATE_ZERO;
    thread_states.ts[i].vtime = 1;
    thread_states.ts[i].watchdog = 1;
    thread_states.ts[i].name = NULL;
  }
OS_WARNING_MSVC_ON(6386);
}

void thread_states_fini (void)
{
  unsigned i;
  for (i = 0; i < thread_states.nthreads; i++)
    assert (thread_states.ts[i].state != THREAD_STATE_ALIVE);
  os_mutexDestroy (&thread_states.lock);
  os_free_aligned (thread_states.ts);

  /* All spawned threads are gone, but the main thread is still alive,
     downgraded to an ordinary thread (we're on it right now). We
     don't want to lose the ability to log messages, so set ts to a
     NULL pointer and rely on lookup_thread_state()'s checks
     thread_states.ts. */
  thread_states.ts = NULL;
}

static void
cleanup_thread_state(
    _In_opt_ void *data)
{
    struct thread_state1 *ts = get_thread_state(os_threadIdSelf());
    (void)data;
    assert(ts->state == THREAD_STATE_ALIVE);
    assert(vtime_asleep_p(ts->vtime));
    reset_thread_state(ts);
    os_osExit();
}

_Ret_valid_ struct thread_state1 *
lookup_thread_state(
    void)
{
    struct thread_state1 *ts1 = NULL;
    char tname[128];
    os_threadId tid;

    if ((ts1 = tsd_thread_state) == NULL) {
        if ((ts1 = lookup_thread_state_real()) == NULL) {
            /* this situation only arises for threads that were not created
               using create_thread, aka application threads. since registering
               thread state should be fully automatic the name will simply be
               the identifier */
            tid = os_threadIdSelf();
            (void)snprintf(
                tname, sizeof(tname), "0x%"PRIxMAX, os_threadIdToInteger(tid));
            os_mutexLock(&thread_states.lock);
            ts1 = init_thread_state(tname);
            if (ts1 != NULL) {
                os_osInit();
                ts1->extTid = tid;
                ts1->tid = tid;
                DDS_LOG(DDS_LC_TRACE, "started application thread %s\n", tname);
                os_threadCleanupPush(&cleanup_thread_state, NULL);
            }
            os_mutexUnlock(&thread_states.lock);
        }

        tsd_thread_state = ts1;
    }

    assert(ts1 != NULL);

    return ts1;
}

_Success_(return != NULL) _Ret_maybenull_
struct thread_state1 *lookup_thread_state_real (void)
{
  if (thread_states.ts) {
    os_threadId tid = os_threadIdSelf ();
    unsigned i;
    for (i = 0; i < thread_states.nthreads; i++) {
      if (os_threadEqual (thread_states.ts[i].tid, tid)) {
        return &thread_states.ts[i];
      }
    }
  }
  return NULL;
}

struct thread_context {
  struct thread_state1 *self;
  uint32_t (*f) (_In_opt_ void *arg);
  void *arg;
};

static uint32_t create_thread_wrapper (_In_ _Post_invalid_ struct thread_context *ctxt)
{
  uint32_t ret;
  ctxt->self->tid = os_threadIdSelf ();
  ret = ctxt->f (ctxt->arg);
  os_free (ctxt);
  return ret;
}

static int find_free_slot (_In_z_ const char *name)
{
  unsigned i;
  int cand;
  for (i = 0, cand = -1; i < thread_states.nthreads; i++)
  {
    if (thread_states.ts[i].state != THREAD_STATE_ALIVE)
      cand = (int) i;
    if (thread_states.ts[i].state == THREAD_STATE_ZERO)
      break;
  }
  if (cand == -1)
    DDS_FATAL("create_thread: %s: no free slot\n", name ? name : "(anon)");
  return cand;
}

void upgrade_main_thread (void)
{
  int cand;
  struct thread_state1 *ts1;
  os_mutexLock (&thread_states.lock);
  if ((cand = find_free_slot ("name")) < 0)
    abort ();
  ts1 = &thread_states.ts[cand];
  if (ts1->state == THREAD_STATE_ZERO)
    assert (vtime_asleep_p (ts1->vtime));
  ts1->state = THREAD_STATE_ALIVE;
  ts1->tid = os_threadIdSelf ();
  ts1->name = main_thread_name;
  os_mutexUnlock (&thread_states.lock);
  tsd_thread_state = ts1;
}

const struct config_thread_properties_listelem *lookup_thread_properties (_In_z_ const char *name)
{
  const struct config_thread_properties_listelem *e;
  for (e = config.thread_properties; e != NULL; e = e->next)
    if (strcmp (e->name, name) == 0)
      break;
  return e;
}

struct thread_state1 * init_thread_state (_In_z_ const char *tname)
{
  int cand;
  struct thread_state1 *ts;

  if ((cand = find_free_slot (tname)) < 0)
    return NULL;

  ts = &thread_states.ts[cand];
  if (ts->state == THREAD_STATE_ZERO)
    assert (vtime_asleep_p (ts->vtime));
  ts->name = os_strdup (tname);
  ts->state = THREAD_STATE_ALIVE;

  return ts;
}

_Success_(return != NULL)
_Ret_maybenull_
struct thread_state1 *create_thread (_In_z_ const char *name, _In_ uint32_t (*f) (void *arg), _In_opt_ void *arg)
{
  struct config_thread_properties_listelem const * const tprops = lookup_thread_properties (name);
  os_threadAttr tattr;
  struct thread_state1 *ts1;
  os_threadId tid;
  struct thread_context *ctxt;
  ctxt = os_malloc (sizeof (*ctxt));
  os_mutexLock (&thread_states.lock);

  ts1 = init_thread_state (name);

  if (ts1 == NULL)
    goto fatal;

  ctxt->self = ts1;
  ctxt->f = f;
  ctxt->arg = arg;
  os_threadAttrInit (&tattr);
  if (tprops != NULL)
  {
    if (!tprops->sched_priority.isdefault)
      tattr.schedPriority = tprops->sched_priority.value;
    tattr.schedClass = tprops->sched_class; /* explicit default value in the enum */
    if (!tprops->stack_size.isdefault)
      tattr.stackSize = tprops->stack_size.value;
  }
  DDS_TRACE("create_thread: %s: class %d priority %d stack %u\n", name, (int) tattr.schedClass, tattr.schedPriority, tattr.stackSize);

  if (os_threadCreate (&tid, name, &tattr, (os_threadRoutine)&create_thread_wrapper, ctxt) != os_resultSuccess)
  {
    ts1->state = THREAD_STATE_ZERO;
    DDS_FATAL("create_thread: %s: os_threadCreate failed\n", name);
    goto fatal;
  }
  DDS_LOG(DDS_LC_TRACE, "started new thread 0x%"PRIxMAX" : %s\n", os_threadIdToInteger (tid), name);
  ts1->extTid = tid; /* overwrite the temporary value with the correct external one */
  os_mutexUnlock (&thread_states.lock);
  return ts1;
 fatal:
  os_mutexUnlock (&thread_states.lock);
  os_free (ctxt);
  abort ();
  return NULL;
}

static void reap_thread_state (_Inout_ struct thread_state1 *ts1, _In_ int sync_with_servicelease)
{
  os_mutexLock (&thread_states.lock);
  ts1->state = THREAD_STATE_ZERO;
  if (sync_with_servicelease && gv.servicelease)
    nn_servicelease_statechange_barrier (gv.servicelease);
  if (ts1->name != main_thread_name)
    os_free (ts1->name);
  os_mutexUnlock (&thread_states.lock);
}

_Success_(return == 0)
int join_thread (_Inout_ struct thread_state1 *ts1)
{
  int ret;
  assert (ts1->state == THREAD_STATE_ALIVE);
  if (os_threadWaitExit (ts1->extTid, NULL) == os_resultSuccess)
    ret = 0;
  else
    ret = ERR_UNSPECIFIED;
  assert (vtime_asleep_p (ts1->vtime));
  reap_thread_state (ts1, 1);
  return ret;
}

void reset_thread_state (_Inout_opt_ struct thread_state1 *ts1)
{
  if (ts1)
  {
    reap_thread_state (ts1, 1);
    ts1->name = NULL;
  }
}

void downgrade_main_thread (void)
{
  struct thread_state1 *ts1 = lookup_thread_state ();
  thread_state_asleep (ts1);
  /* no need to sync with service lease: already stopped */
  reap_thread_state (ts1, 0);
  thread_states_init_static ();
}

struct thread_state1 *get_thread_state (_In_ os_threadId id)
{
  unsigned i;
  struct thread_state1 *ts = NULL;

  for (i = 0; i < thread_states.nthreads; i++)
  {
    if (os_threadEqual (thread_states.ts[i].extTid, id))
    {
      ts = &thread_states.ts[i];
      break;
    }
  }
  return ts;
}

void log_stack_traces (void)
{
  unsigned i;
  for (i = 0; i < thread_states.nthreads; i++)
  {
    if (thread_states.ts[i].state == THREAD_STATE_ALIVE)
    {
      log_stacktrace (thread_states.ts[i].name, thread_states.ts[i].tid);
    }
  }
}

