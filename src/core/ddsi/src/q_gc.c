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
#include <stdlib.h>
#include <stddef.h>

#include "os/os.h"

#include "ddsi/q_gc.h"
#include "ddsi/q_log.h"
#include "ddsi/q_config.h"
#include "ddsi/q_time.h"
#include "ddsi/q_thread.h"
#include "ddsi/q_ephash.h"
#include "ddsi/q_unused.h"
#include "ddsi/q_lease.h"
#include "ddsi/q_globals.h" /* for mattr, cattr */

#include "ddsi/q_rtps.h" /* for guid_hash */

struct gcreq_queue {
  struct gcreq *first;
  struct gcreq *last;
  os_mutex lock;
  os_cond cond;
  int terminate;
  int32_t count;
  struct thread_state1 *ts;
};

static void threads_vtime_gather_for_wait (unsigned *nivs, struct idx_vtime *ivs)
{
  /* copy vtimes of threads, skipping those that are sleeping */
  unsigned i, j;
  for (i = j = 0; i < thread_states.nthreads; i++)
  {
    vtime_t vtime = thread_states.ts[i].vtime;
    if (vtime_awake_p (vtime))
    {
      ivs[j].idx = i;
      ivs[j].vtime = vtime;
      ++j;
    }
  }
  *nivs = j;
}

static int threads_vtime_check (unsigned *nivs, struct idx_vtime *ivs)
{
  /* check all threads in ts have made progress those that have are
     removed from the set */
  unsigned i = 0;
  while (i < *nivs)
  {
    unsigned thridx = ivs[i].idx;
    vtime_t vtime = thread_states.ts[thridx].vtime;
    assert (vtime_awake_p (ivs[i].vtime));
    if (!vtime_gt (vtime, ivs[i].vtime))
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

static uint32_t gcreq_queue_thread (struct gcreq_queue *q)
{
  struct thread_state1 *self = lookup_thread_state ();
  nn_mtime_t next_thread_cputime = { 0 };
  struct os_time shortsleep = { 0, 1 * T_MILLISECOND };
  int64_t delay = T_MILLISECOND; /* force evaluation after startup */
  struct gcreq *gcreq = NULL;
  int trace_shortsleep = 1;
  os_mutexLock (&q->lock);
  while (!(q->terminate && q->count == 0))
  {
    LOG_THREAD_CPUTIME (next_thread_cputime);

    /* If we are waiting for a gcreq to become ready, don't bother
       looking at the queue; if we aren't, wait for a request to come
       in.  We can't really wait until something came in because we're
       also checking lease expirations. */
    if (gcreq == NULL)
    {
      assert (trace_shortsleep);
      if (q->first == NULL)
      {
        /* FIXME: fix os_time and use absolute timeouts */
        struct os_time to;
        if (delay >= 1000 * T_SECOND) {
          /* avoid overflow */
          to.tv_sec = 1000;
          to.tv_nsec = 0;
        } else {
          to.tv_sec = (os_timeSec)(delay / T_SECOND);
          to.tv_nsec = (int32_t)(delay % T_SECOND);
        }
        os_condTimedWait (&q->cond, &q->lock, &to);
      }
      if (q->first)
      {
        gcreq = q->first;
        q->first = q->first->next;
      }
    }
    os_mutexUnlock (&q->lock);

    /* Cleanup dead proxy entities. One can argue this should be an
       independent thread, but one can also easily argue that an
       expired lease is just another form of a request for
       deletion. In any event, letting this thread do this should have
       very little impact on its primary purpose and be less of a
       burden on the system than having a separate thread or adding it
       to the workload of the data handling threads. */
    thread_state_awake (self);
    delay = check_and_handle_lease_expiration (self, now_et ());
    thread_state_asleep (self);

    if (gcreq)
    {
      if (!threads_vtime_check (&gcreq->nvtimes, gcreq->vtimes))
      {
        /* Not all threads made enough progress => gcreq is not ready
           yet => sleep for a bit and retry.  Note that we can't even
           terminate while this gcreq is waiting and that there is no
           condition on which to wait, so a plain sleep is quite
           reasonable. */
        if (trace_shortsleep)
        {
          DDS_TRACE("gc %p: not yet, shortsleep\n", (void*)gcreq);
          trace_shortsleep = 0;
        }
        os_nanoSleep (shortsleep);
      }
      else
      {
        /* Sufficient progress has been made: may now continue deleting
           it; the callback is responsible for requeueing (if complex
           multi-phase delete) or freeing the delete request.  Reset
           the current gcreq as this one obviously is no more.  */
        DDS_TRACE("gc %p: deleting\n", (void*)gcreq);
        thread_state_awake (self);
        gcreq->cb (gcreq);
        thread_state_asleep (self);
        gcreq = NULL;
        trace_shortsleep = 1;
      }
    }

    os_mutexLock (&q->lock);
  }
  os_mutexUnlock (&q->lock);
  return 0;
}

struct gcreq_queue *gcreq_queue_new (void)
{
  struct gcreq_queue *q = os_malloc (sizeof (*q));

  q->first = q->last = NULL;
  q->terminate = 0;
  q->count = 0;
  os_mutexInit (&q->lock);
  os_condInit (&q->cond, &q->lock);
  q->ts = create_thread ("gc", (uint32_t (*) (void *)) gcreq_queue_thread, q);
  assert (q->ts);
  return q;
}

void gcreq_queue_drain (struct gcreq_queue *q)
{
  os_mutexLock (&q->lock);
  while (q->count != 0)
    os_condWait (&q->cond, &q->lock);
  os_mutexUnlock (&q->lock);
}

void gcreq_queue_free (struct gcreq_queue *q)
{
  struct gcreq *gcreq;

  /* Create a no-op not dependent on any thread */
  gcreq = gcreq_new (q, gcreq_free);
  gcreq->nvtimes = 0;

  os_mutexLock (&q->lock);
  q->terminate = 1;
  /* Wait until there is only request in existence, the one we just
     allocated (this is also why we can't use "drain" here). Then
     we know the gc system is quiet. */
  while (q->count != 1)
    os_condWait (&q->cond, &q->lock);
  os_mutexUnlock (&q->lock);

  /* Force the gc thread to wake up by enqueueing our no-op. The
     callback, gcreq_free, will be called immediately, which causes
     q->count to 0 before the loop condition is evaluated again, at
     which point the thread terminates. */
  gcreq_enqueue (gcreq);

  join_thread (q->ts);
  assert (q->first == NULL);
  os_condDestroy (&q->cond);
  os_mutexDestroy (&q->lock);
  os_free (q);
}

struct gcreq *gcreq_new (struct gcreq_queue *q, gcreq_cb_t cb)
{
  struct gcreq *gcreq;
  gcreq = os_malloc (offsetof (struct gcreq, vtimes) + thread_states.nthreads * sizeof (*gcreq->vtimes));
  gcreq->cb = cb;
  gcreq->queue = q;
  threads_vtime_gather_for_wait (&gcreq->nvtimes, gcreq->vtimes);
  os_mutexLock (&q->lock);
  q->count++;
  os_mutexUnlock (&q->lock);
  return gcreq;
}

void gcreq_free (struct gcreq *gcreq)
{
  struct gcreq_queue *gcreq_queue = gcreq->queue;
  os_mutexLock (&gcreq_queue->lock);
  --gcreq_queue->count;
  if (gcreq_queue->count <= 1)
    os_condBroadcast (&gcreq_queue->cond);
  os_mutexUnlock (&gcreq_queue->lock);
  os_free (gcreq);
}

static int gcreq_enqueue_common (struct gcreq *gcreq)
{
  struct gcreq_queue *gcreq_queue = gcreq->queue;
  int isfirst;
  os_mutexLock (&gcreq_queue->lock);
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
    os_condBroadcast (&gcreq_queue->cond);
  os_mutexUnlock (&gcreq_queue->lock);
  return isfirst;
}

void gcreq_enqueue (struct gcreq *gcreq)
{
  gcreq_enqueue_common (gcreq);
}

int gcreq_requeue (struct gcreq *gcreq, gcreq_cb_t cb)
{
  gcreq->cb = cb;
  return gcreq_enqueue_common (gcreq);
}
