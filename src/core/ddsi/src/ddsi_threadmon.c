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

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/threads.h"

#include "dds/ddsi/ddsi_threadmon.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/q_time.h"
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_globals.h" /* for mattr, cattr */
#include "dds/ddsi/q_receive.h"

struct alive_vt {
  bool alive;
  vtime_t vt;
};

struct ddsi_threadmon {
  int keepgoing;
  struct alive_vt *av_ary;
  void (*renew_cb) (void *arg);
  void *renew_arg;

  ddsrt_mutex_t lock;
  ddsrt_cond_t cond;
  struct thread_state1 *ts;
};

static uint32_t threadmon_thread (struct ddsi_threadmon *sl)
{
  /* Do not check more often than once every 100ms (no particular
     reason why it has to be 100ms), regardless of the lease settings.
     Note: can't trust sl->self, may have been scheduled before the
     assignment. */
  nn_mtime_t next_thread_cputime = { 0 };
  nn_mtime_t tlast = { 0 };
  bool was_alive = true;
  unsigned i;
  for (i = 0; i < thread_states.nthreads; i++)
  {
    sl->av_ary[i].alive = true;
  }
  ddsrt_mutex_lock (&sl->lock);
  while (sl->keepgoing)
  {
    /* Guard against spurious wakeups by checking only when cond_waitfor signals a timeout */
    if (ddsrt_cond_waitfor (&sl->cond, &sl->lock, config.liveliness_monitoring_interval))
      continue;

    unsigned n_alive = 0, n_unused = 0;
    nn_mtime_t tnow = now_mt ();

    LOG_THREAD_CPUTIME (next_thread_cputime);

    DDS_TRACE("threadmon: tnow %"PRId64":", tnow.v);

    /* Check progress only if enough time has passed: there is no
       guarantee that os_cond_timedwait wont ever return early, and we
       do want to avoid spurious warnings. */
    if (tnow.v < tlast.v)
    {
      n_alive = thread_states.nthreads;
    }
    else
    {
      tlast = tnow;
      for (i = 0; i < thread_states.nthreads; i++)
      {
        if (thread_states.ts[i].state == THREAD_STATE_ZERO)
          n_unused++;
        else
        {
          vtime_t vt = thread_states.ts[i].vtime;
          bool alive = vtime_asleep_p (vt) || vtime_asleep_p (sl->av_ary[i].vt) || vtime_gt (vt, sl->av_ary[i].vt);
          n_alive += (unsigned) alive;
          DDS_TRACE(" %u(%s):%c:%"PRIx32"->%"PRIx32, i, thread_states.ts[i].name, alive ? 'a' : 'd', sl->av_ary[i].vt, vt);
          sl->av_ary[i].vt = vt;
          if (sl->av_ary[i].alive != alive)
          {
            const char *name = thread_states.ts[i].name;
            const char *msg;
            if (!alive)
              msg = "failed to make progress";
            else
              msg = "once again made progress";
            DDS_INFO("thread %s %s\n", name ? name : "(anon)", msg);
            sl->av_ary[i].alive = alive;
          }
        }
      }
    }

    if (n_alive + n_unused == thread_states.nthreads)
    {
      DDS_TRACE(": [%u] OK\n", n_alive);
      was_alive = true;
    }
    else
    {
      DDS_TRACE(": [%u] FAIL\n", n_alive);
      if (was_alive)
        log_stack_traces ();
      was_alive = false;
    }

#if DDSRT_HAVE_RUSAGE
    if (dds_get_log_mask() & DDS_LC_TIMING)
    {
      ddsrt_rusage_t u;
      if (ddsrt_getrusage (DDSRT_RUSAGE_SELF, &u) == DDS_RETCODE_OK)
      {
        DDS_LOG(DDS_LC_TIMING,
                "rusage: utime %d.%09d stime %d.%09d maxrss %zu data %zu vcsw %zu ivcsw %zu\n",
                (int) (u.utime / DDS_NSECS_IN_SEC),
                (int) (u.utime % DDS_NSECS_IN_SEC),
                (int) (u.stime / DDS_NSECS_IN_SEC),
                (int) (u.stime % DDS_NSECS_IN_SEC),
                u.maxrss, u.idrss, u.nvcsw, u.nivcsw);
      }
    }
#endif /* DDSRT_HAVE_RUSAGE */

    /* While deaf, we need to make sure the receive thread wakes up
       every now and then to try recreating sockets & rejoining multicast
       groups */
    if (gv.deaf)
      trigger_recv_threads ();
  }
  ddsrt_mutex_unlock (&sl->lock);
  return 0;
}

struct ddsi_threadmon *ddsi_threadmon_new (void)
{
  struct ddsi_threadmon *sl;

  sl = ddsrt_malloc (sizeof (*sl));
  sl->keepgoing = -1;
  sl->ts = NULL;

  if ((sl->av_ary = ddsrt_malloc (thread_states.nthreads * sizeof (*sl->av_ary))) == NULL)
    goto fail_vtimes;
  /* service lease update thread initializes av_ary */

  ddsrt_mutex_init (&sl->lock);
  ddsrt_cond_init (&sl->cond);
  return sl;

 fail_vtimes:
  ddsrt_free (sl);
  return NULL;
}

dds_return_t ddsi_threadmon_start (struct ddsi_threadmon *sl)
{
  ddsrt_mutex_lock (&sl->lock);
  assert (sl->keepgoing == -1);
  sl->keepgoing = 1;
  ddsrt_mutex_unlock (&sl->lock);

  if (create_thread (&sl->ts, "lease", (uint32_t (*) (void *)) threadmon_thread, sl) != DDS_RETCODE_OK)
    goto fail_thread;
  return 0;

 fail_thread:
  sl->keepgoing = -1;
  return DDS_RETCODE_ERROR;
}

void ddsi_threadmon_statechange_barrier (struct ddsi_threadmon *sl)
{
  ddsrt_mutex_lock (&sl->lock);
  ddsrt_mutex_unlock (&sl->lock);
}

void ddsi_threadmon_stop (struct ddsi_threadmon *sl)
{
  if (sl->keepgoing != -1)
  {
    ddsrt_mutex_lock (&sl->lock);
    sl->keepgoing = 0;
    ddsrt_cond_signal (&sl->cond);
    ddsrt_mutex_unlock (&sl->lock);
    join_thread (sl->ts);
  }
}

void ddsi_threadmon_free (struct ddsi_threadmon *sl)
{
  ddsrt_cond_destroy (&sl->cond);
  ddsrt_mutex_destroy (&sl->lock);
  ddsrt_free (sl->av_ary);
  ddsrt_free (sl);
}

