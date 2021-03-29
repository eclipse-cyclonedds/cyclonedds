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

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsi/q_gc.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_lease.h"
#include "dds/ddsi/ddsi_domaingv.h" /* for mattr, cattr */
#include "dds/ddsi/q_receive.h" /* for trigger_receive_threads */

struct gcreq_queue {
  struct gcreq *first;
  struct gcreq *last;
  ddsrt_mutex_t lock;
  ddsrt_cond_t cond;
  int terminate;
  int32_t count;
  struct ddsi_domaingv *gv;
  struct thread_state1 *ts;
};

static void threads_vtime_gather_for_wait (const struct ddsi_domaingv *gv, uint32_t *nivs, struct idx_vtime *ivs)
{
  /* copy vtimes of threads, skipping those that are sleeping */
  uint32_t i, j;
  for (i = j = 0; i < thread_states.nthreads; i++)
  {
    vtime_t vtime = ddsrt_atomic_ld32 (&thread_states.ts[i].vtime);
    if (vtime_awake_p (vtime))
    {
      ddsrt_atomic_fence_ldld ();
      /* ts[i].gv is set before ts[i].vtime indicates the thread is awake, so if the thread hasn't
         gone through another sleep/wake cycle since loading ts[i].vtime, ts[i].gv is correct; if
         instead it has gone through another cycle since loading ts[i].vtime, then the thread will
         be dropped from the live threads on the next check.  So it won't ever wait with unknown
         duration for progres of threads stuck in another domain */
      if (gv == ddsrt_atomic_ldvoidp (&thread_states.ts[i].gv))
      {
        ivs[j].idx = i;
        ivs[j].vtime = vtime;
        ++j;
      }
    }
  }
  *nivs = j;
}

static int threads_vtime_check (const struct ddsi_domaingv *gv, uint32_t *nivs, struct idx_vtime *ivs)
{
  /* check all threads in ts have made progress those that have are
     removed from the set */
  uint32_t i = 0;
  while (i < *nivs)
  {
    uint32_t thridx = ivs[i].idx;
    vtime_t vtime = ddsrt_atomic_ld32 (&thread_states.ts[thridx].vtime);
    assert (vtime_awake_p (ivs[i].vtime));
    if (!vtime_gt (vtime, ivs[i].vtime) && ddsrt_atomic_ldvoidp (&thread_states.ts[thridx].gv) == gv)
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
  struct thread_state1 * const ts1 = lookup_thread_state ();
  ddsrt_mtime_t next_thread_cputime = { 0 };
  ddsrt_mtime_t t_trigger_recv_threads = { 0 };
  int64_t shortsleep = DDS_MSECS (1);
  int64_t delay = DDS_MSECS (1); /* force evaluation after startup */
  struct gcreq *gcreq = NULL;
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
      if (tnow_mt.v > t_trigger_recv_threads.v)
      {
        trigger_recv_threads (q->gv);
        t_trigger_recv_threads.v = tnow_mt.v + DDS_MSECS (100);
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
    thread_state_awake_fixed_domain (ts1);
    delay = check_and_handle_lease_expiration (q->gv, ddsrt_time_elapsed ());
    thread_state_asleep (ts1);

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
        thread_state_awake_fixed_domain (ts1);
        gcreq->cb (gcreq);
        thread_state_asleep (ts1);
        gcreq = NULL;
        trace_shortsleep = 1;
      }
    }

    ddsrt_mutex_lock (&q->lock);
  }
  ddsrt_mutex_unlock (&q->lock);
  return 0;
}

struct gcreq_queue *gcreq_queue_new (struct ddsi_domaingv *gv)
{
  struct gcreq_queue *q = ddsrt_malloc (sizeof (*q));

  q->first = q->last = NULL;
  q->terminate = 0;
  q->count = 0;
  q->gv = gv;
  ddsrt_mutex_init (&q->lock);
  ddsrt_cond_init (&q->cond);
  if (create_thread (&q->ts, gv, "gc", (uint32_t (*) (void *)) gcreq_queue_thread, q) == DDS_RETCODE_OK)
    return q;
  else
  {
    ddsrt_mutex_destroy (&q->lock);
    ddsrt_cond_destroy (&q->cond);
    ddsrt_free (q);
    return NULL;
  }
}

void gcreq_queue_drain (struct gcreq_queue *q)
{
  ddsrt_mutex_lock (&q->lock);
  while (q->count != 0)
    ddsrt_cond_wait (&q->cond, &q->lock);
  ddsrt_mutex_unlock (&q->lock);
}

void gcreq_queue_free (struct gcreq_queue *q)
{
  struct gcreq *gcreq;

  /* Create a no-op not dependent on any thread */
  gcreq = gcreq_new (q, gcreq_free);
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
  gcreq_enqueue (gcreq);

  join_thread (q->ts);
  assert (q->first == NULL);
  ddsrt_cond_destroy (&q->cond);
  ddsrt_mutex_destroy (&q->lock);
  ddsrt_free (q);
}

struct gcreq *gcreq_new (struct gcreq_queue *q, gcreq_cb_t cb)
{
  struct gcreq *gcreq;
  gcreq = ddsrt_malloc (offsetof (struct gcreq, vtimes) + thread_states.nthreads * sizeof (*gcreq->vtimes));
  gcreq->cb = cb;
  gcreq->queue = q;
  threads_vtime_gather_for_wait (q->gv, &gcreq->nvtimes, gcreq->vtimes);
  ddsrt_mutex_lock (&q->lock);
  q->count++;
  ddsrt_mutex_unlock (&q->lock);
  return gcreq;
}

void gcreq_free (struct gcreq *gcreq)
{
  struct gcreq_queue *gcreq_queue = gcreq->queue;
  ddsrt_mutex_lock (&gcreq_queue->lock);
  --gcreq_queue->count;
  if (gcreq_queue->count <= 1)
    ddsrt_cond_broadcast (&gcreq_queue->cond);
  ddsrt_mutex_unlock (&gcreq_queue->lock);
  ddsrt_free (gcreq);
}

static int gcreq_enqueue_common (struct gcreq *gcreq)
{
  struct gcreq_queue *gcreq_queue = gcreq->queue;
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

void gcreq_enqueue (struct gcreq *gcreq)
{
  gcreq_enqueue_common (gcreq);
}

int gcreq_requeue (struct gcreq *gcreq, gcreq_cb_t cb)
{
  gcreq->cb = cb;
  return gcreq_enqueue_common (gcreq);
}
