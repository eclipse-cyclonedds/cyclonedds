// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdlib.h>
#include <stddef.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_domaingv.h" /* for mattr, cattr */
#include "ddsi__thread.h"
#include "ddsi__entity_index.h"
#include "ddsi__lease.h"
#include "ddsi__log.h"
#include "ddsi__receive.h" /* for trigger_receive_threads */
#include "ddsi__gc.h"

struct ddsi_gcreq_queue {
  struct ddsi_gcreq *first;
  struct ddsi_gcreq *last;
  ddsrt_mutex_t lock;
  ddsrt_cond_t cond;
  int terminate;
  int32_t count;
  struct ddsi_domaingv *gv;
  struct ddsi_thread_state *thrst;
};

static void threads_vtime_gather_for_wait (const struct ddsi_domaingv *gv, uint32_t *nivs, struct ddsi_idx_vtime *ivs, struct ddsi_thread_states_list *cur)
{
  /* copy vtimes of threads, skipping those that are sleeping */
#ifndef NDEBUG
  const uint32_t nthreads = cur->nthreads;
#endif
  uint32_t dstidx;
  for (dstidx = 0; cur; cur = cur->next)
  {
    for (uint32_t i = 0; i < DDSI_THREAD_STATE_BATCH; i++)
    {
      ddsi_vtime_t vtime = ddsrt_atomic_ld32 (&cur->thrst[i].vtime);
      if (ddsi_vtime_awake_p (vtime))
      {
        ddsrt_atomic_fence_ldld ();
        /* thrst[i].gv is set before thrst[i].vtime indicates the thread is awake, so if the thread
           hasn't gone through another sleep/wake cycle since loading thrst[i].vtime, thrst[i].gv is
           correct; if instead it has gone through another cycle since loading thrst[i].vtime, then
           the thread will be dropped from the live threads on the next check.  So it won't ever wait
           with unknown duration for progres of threads stuck in another domain */
        if (gv == ddsrt_atomic_ldvoidp (&cur->thrst[i].gv))
        {
          assert (dstidx < nthreads);
          ivs[dstidx].thrst = &cur->thrst[i];
          ivs[dstidx].vtime = vtime;
          ++dstidx;
        }
      }
    }
  }
  *nivs = dstidx;
}

static int threads_vtime_check (const struct ddsi_domaingv *gv, uint32_t *nivs, struct ddsi_idx_vtime *ivs)
{
  /* remove all threads that have moved on from the set, return true when none remain */
  uint32_t i = 0;
  while (i < *nivs)
  {
    ddsi_vtime_t vtime = ddsrt_atomic_ld32 (&ivs[i].thrst->vtime);
    assert (ddsi_vtime_awake_p (ivs[i].vtime));
    if (!ddsi_vtime_gt (vtime, ivs[i].vtime) && ddsrt_atomic_ldvoidp (&ivs[i].thrst->gv) == gv)
      ++i;
    else
    {
      if (i + 1 < *nivs)
        ivs[i] = ivs[*nivs - 1];
      --(*nivs);
    }
  }
  return *nivs == 0;
}

static uint32_t gcreq_queue_thread (struct ddsi_gcreq_queue *q)
{
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsrt_mtime_t next_thread_cputime = { 0 };
  ddsrt_mtime_t t_ddsi_trigger_recv_threads = { 0 };
  int64_t shortsleep = DDS_MSECS (1);
  int64_t delay = DDS_MSECS (1); /* force evaluation after startup */
  struct ddsi_gcreq *gcreq = NULL;
  int trace_shortsleep = 1;
  ddsrt_mutex_lock (&q->lock);
  while (!(q->terminate && q->count == 0))
  {
    LOG_THREAD_CPUTIME (&q->gv->logconfig, next_thread_cputime);

    /* While deaf, we need to make sure the receive thread wakes up
       every now and then to try recreating sockets & rejoining multicast
       groups.  Do rate-limit it a bit. */
    if (q->gv->deaf)
    {
      ddsrt_mtime_t tnow_mt = ddsrt_time_monotonic ();
      if (tnow_mt.v > t_ddsi_trigger_recv_threads.v)
      {
        ddsi_trigger_recv_threads (q->gv);
        t_ddsi_trigger_recv_threads.v = tnow_mt.v + DDS_MSECS (100);
      }
    }

    /* If we are waiting for a gcreq to become ready, don't bother
       looking at the queue; if we aren't, wait for a request to come
       in.  We can't really wait until something came in because we're
       also checking lease expirations. */
    if (gcreq == NULL)
    {
      assert (trace_shortsleep);
      if (q->first == NULL)
      {
        /* FIXME: use absolute timeouts */
        /* avoid overflows; ensure periodic wakeups of receive thread if deaf */
        const int64_t maxdelay = q->gv->deaf ? DDS_MSECS (100) : DDS_SECS (1000);
        dds_time_t to;
        if (delay >= maxdelay) {
          to = maxdelay;
        } else {
          to = delay;
        }
        (void) ddsrt_cond_waitfor (&q->cond, &q->lock, to);
      }
      if (q->first)
      {
        gcreq = q->first;
        q->first = q->first->next;
      }
    }
    ddsrt_mutex_unlock (&q->lock);

    /* Cleanup dead proxy entities. One can argue this should be an
       independent thread, but one can also easily argue that an
       expired lease is just another form of a request for
       deletion. In any event, letting this thread do this should have
       very little impact on its primary purpose and be less of a
       burden on the system than having a separate thread or adding it
       to the workload of the data handling threads. */
    ddsi_thread_state_awake_fixed_domain (thrst);
    delay = ddsi_check_and_handle_lease_expiration (q->gv, ddsrt_time_elapsed ());
    ddsi_thread_state_asleep (thrst);

    if (gcreq)
    {
      if (!threads_vtime_check (q->gv, &gcreq->nvtimes, gcreq->vtimes))
      {
        /* Not all threads made enough progress => gcreq is not ready
           yet => sleep for a bit and retry.  Note that we can't even
           terminate while this gcreq is waiting and that there is no
           condition on which to wait, so a plain sleep is quite
           reasonable. */
        if (trace_shortsleep)
        {
          DDS_CTRACE (&q->gv->logconfig, "gc %p: not yet, shortsleep\n", (void *) gcreq);
          trace_shortsleep = 0;
        }
        dds_sleepfor (shortsleep);
      }
      else
      {
        /* Sufficient progress has been made: may now continue deleting
           it; the callback is responsible for requeueing (if complex
           multi-phase delete) or freeing the delete request.  Reset
           the current gcreq as this one obviously is no more.  */
        DDS_CTRACE (&q->gv->logconfig, "gc %p: deleting\n", (void *) gcreq);
        ddsi_thread_state_awake_fixed_domain (thrst);
        gcreq->cb (gcreq);
        ddsi_thread_state_asleep (thrst);
        gcreq = NULL;
        trace_shortsleep = 1;
      }
    }

    ddsrt_mutex_lock (&q->lock);
  }
  ddsrt_mutex_unlock (&q->lock);
  return 0;
}

struct ddsi_gcreq_queue *ddsi_gcreq_queue_new (struct ddsi_domaingv *gv)
{
  struct ddsi_gcreq_queue *q = ddsrt_malloc (sizeof (*q));

  q->first = q->last = NULL;
  q->terminate = 0;
  q->count = 0;
  q->gv = gv;
  q->thrst = NULL;
  ddsrt_mutex_init (&q->lock);
  ddsrt_cond_init (&q->cond);
  return q;
}

bool ddsi_gcreq_queue_start (struct ddsi_gcreq_queue *q)
{
  if (ddsi_create_thread (&q->thrst, q->gv, "gc", (uint32_t (*) (void *)) gcreq_queue_thread, q) == DDS_RETCODE_OK)
    return true;
  else
  {
    /* we use q->thrst as a marker whether the thread exists, protect against the possibility
       that create_thread changes it in a failing case */
    q->thrst = NULL;
    return false;
  }
}

void ddsi_gcreq_queue_drain (struct ddsi_gcreq_queue *q)
{
  ddsrt_mutex_lock (&q->lock);
  while (q->count != 0)
    ddsrt_cond_wait (&q->cond, &q->lock);
  ddsrt_mutex_unlock (&q->lock);
}

void ddsi_gcreq_queue_free (struct ddsi_gcreq_queue *q)
{
  struct ddsi_gcreq *gcreq;

  /* Shutdown is complicated only gcreq_eueu */
  if (q->thrst)
  {
    /* Create a no-op not dependent on any thread */
  gcreq = ddsi_gcreq_new (q, ddsi_gcreq_free);
    gcreq->nvtimes = 0;

    ddsrt_mutex_lock (&q->lock);
    q->terminate = 1;
    /* Wait until there is only request in existence, the one we just
       allocated (this is also why we can't use "drain" here). Then
       we know the gc system is quiet. */
    while (q->count != 1)
      ddsrt_cond_wait (&q->cond, &q->lock);
    ddsrt_mutex_unlock (&q->lock);

    /* Force the gc thread to wake up by enqueueing our no-op. The
       callback, gcreq_free, will be called immediately, which causes
       q->count to 0 before the loop condition is evaluated again, at
       which point the thread terminates. */
    ddsi_gcreq_enqueue (gcreq);

    ddsi_join_thread (q->thrst);
    assert (q->first == NULL);
  }
  ddsrt_cond_destroy (&q->cond);
  ddsrt_mutex_destroy (&q->lock);
  ddsrt_free (q);
}

struct ddsi_gcreq *ddsi_gcreq_new (struct ddsi_gcreq_queue *q, ddsi_gcreq_cb_t cb)
{
  struct ddsi_gcreq *gcreq;
  struct ddsi_thread_states_list * const tslist = ddsrt_atomic_ldvoidp (&thread_states.thread_states_head);
  gcreq = ddsrt_malloc (offsetof (struct ddsi_gcreq, vtimes) + tslist->nthreads * sizeof (*gcreq->vtimes));
  gcreq->cb = cb;
  gcreq->queue = q;
  threads_vtime_gather_for_wait (q->gv, &gcreq->nvtimes, gcreq->vtimes, tslist);
  ddsrt_mutex_lock (&q->lock);
  q->count++;
  ddsrt_mutex_unlock (&q->lock);
  return gcreq;
}

void ddsi_gcreq_free (struct ddsi_gcreq *gcreq)
{
  struct ddsi_gcreq_queue *gcreq_queue = gcreq->queue;
  ddsrt_mutex_lock (&gcreq_queue->lock);
  --gcreq_queue->count;
  if (gcreq_queue->count <= 1)
    ddsrt_cond_broadcast (&gcreq_queue->cond);
  ddsrt_mutex_unlock (&gcreq_queue->lock);
  ddsrt_free (gcreq);
}

static int gcreq_enqueue_common (struct ddsi_gcreq *gcreq)
{
  struct ddsi_gcreq_queue *gcreq_queue = gcreq->queue;
  int isfirst;
  ddsrt_mutex_lock (&gcreq_queue->lock);
  gcreq->next = NULL;
  if (gcreq_queue->first)
  {
    gcreq_queue->last->next = gcreq;
    isfirst = 0;
  }
  else
  {
    gcreq_queue->first = gcreq;
    isfirst = 1;
  }
  gcreq_queue->last = gcreq;
  if (isfirst)
    ddsrt_cond_broadcast (&gcreq_queue->cond);
  ddsrt_mutex_unlock (&gcreq_queue->lock);
  return isfirst;
}

void ddsi_gcreq_enqueue (struct ddsi_gcreq *gcreq)
{
  gcreq_enqueue_common (gcreq);
}

int ddsi_gcreq_requeue (struct ddsi_gcreq *gcreq, ddsi_gcreq_cb_t cb)
{
  gcreq->cb = cb;
  return gcreq_enqueue_common (gcreq);
}

void * ddsi_gcreq_get_arg (struct ddsi_gcreq *gcreq)
{
  assert (gcreq);
  return gcreq->arg;
}

void ddsi_gcreq_set_arg (struct ddsi_gcreq *gcreq, void *arg)
{
  assert (gcreq);
  gcreq->arg = arg;
}
