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
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsi/ddsi_threadmon.h"
#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_domaingv.h" /* for mattr, cattr */
#include "ddsi__log.h"
#include "ddsi__thread.h"
#include "ddsi__receive.h"

struct alive_vt {
  bool alive;
  ddsi_vtime_t vt;
};

struct threadmon_domain {
  const struct ddsi_domaingv *gv;
  unsigned n_not_alive;
  size_t msgpos;
  char msg[2048];
};

struct ddsi_threadmon {
  int keepgoing;
  uint32_t av_ary_size;
  struct alive_vt *av_ary;
  void (*renew_cb) (void *arg);
  void *renew_arg;
  int64_t liveliness_monitoring_interval;
  bool noprogress_log_stacktraces;

  ddsrt_mutex_t lock;
  ddsrt_cond_t cond;
  struct ddsi_thread_state *thrst;
  struct ddsrt_hh *domains;
};

static struct threadmon_domain *find_domain (struct ddsi_threadmon *sl, const struct ddsi_domaingv *gv)
{
  struct threadmon_domain dummy;
  dummy.gv = gv;
  return ddsrt_hh_lookup (sl->domains, &dummy);
}

static void update_av_ary (struct ddsi_threadmon *sl, uint32_t nthreads)
{
  if (nthreads > sl->av_ary_size)
  {
    struct alive_vt *av_ary = ddsrt_malloc (nthreads * sizeof (*av_ary));
    uint32_t i;
    for (i = 0; i < nthreads - sl->av_ary_size; i++)
    {
      av_ary[i].alive = true;
      av_ary[i].vt = 0;
    }
    // passing a null pointer to memcpy is undefined behaviour, even when
    // copying 0 bytes
    if (sl->av_ary)
      memcpy (&av_ary[i], sl->av_ary, sl->av_ary_size * sizeof (*av_ary));
    ddsrt_free (sl->av_ary);
    sl->av_ary = av_ary;
    sl->av_ary_size = nthreads;
  }
}

static uint32_t threadmon_thread (struct ddsi_threadmon *sl)
{
  /* Do not check more often than once every 100ms (no particular
     reason why it has to be 100ms), regardless of the lease settings.
     Note: can't trust sl->self, may have been scheduled before the
     assignment. */
  ddsrt_mtime_t tlast = { 0 };
  bool was_alive = true;
  ddsrt_mutex_lock (&sl->lock);
  while (sl->keepgoing)
  {
    /* Guard against spurious wakeups by checking only when cond_waitfor signals a timeout */
    if (ddsrt_cond_waitfor (&sl->cond, &sl->lock, sl->liveliness_monitoring_interval))
      continue;
    /* Check progress only if enough time has passed: there is no
       guarantee that os_cond_timedwait wont ever return early, and we
       do want to avoid spurious warnings. */
    ddsrt_mtime_t tnow = ddsrt_time_monotonic ();
    if (tnow.v < tlast.v)
      continue;

    /* Scan threads to classify them as alive (sleeping or making progress) or dead (stuck in the same
       "awake" state), ignoring those used in domains that do not have a liveliness monitoring enabled
       (in which case find_domain returns a null pointer).

       A non-awake thread is not really bound to a domain, but it is mostly ignored because it is
       considered "alive".  An awake one may be switching to another domain immediately after loading
       the domain here, but in that case it is making progress -- and so also mostly ignored.  (This
       is a similar argument to that used for the GC). */
    struct ddsi_thread_states_list * const tslist = ddsrt_atomic_ldvoidp (&thread_states.thread_states_head);
    update_av_ary (sl, tslist->nthreads);
    uint32_t n_not_alive = 0;
    tlast = tnow;
    struct alive_vt *av_ary_cur = sl->av_ary;
    for (struct ddsi_thread_states_list *cur = tslist; cur; cur = cur->next)
    {
      for (uint32_t i = 0; i < DDSI_THREAD_STATE_BATCH; i++, av_ary_cur++)
      {
        const uint32_t threadidx = (uint32_t) (av_ary_cur - sl->av_ary);
        assert (threadidx < tslist->nthreads);
        struct ddsi_thread_state * const thrst = &cur->thrst[i];
        if (thrst->state == DDSI_THREAD_STATE_ZERO)
          continue;

        ddsi_vtime_t vt = ddsrt_atomic_ld32 (&thrst->vtime);
        ddsrt_atomic_fence_ldld ();
        struct ddsi_domaingv const * const gv = ddsrt_atomic_ldvoidp (&thrst->gv);
        struct threadmon_domain *tmdom = find_domain (sl, gv);
        if (tmdom == NULL)
          continue;

        bool alive = ddsi_vtime_asleep_p (vt) || ddsi_vtime_asleep_p (av_ary_cur->vt) || ddsi_vtime_gt (vt, av_ary_cur->vt);
        n_not_alive += (unsigned) !alive;
        tmdom->n_not_alive += (unsigned) !alive;

        /* Construct a detailed trace line for domains that have tracing enabled, domains that don't
         only get "failed to make progress"/"once again made progress" messages */
        if (tmdom->msgpos < sizeof (tmdom->msg) && (gv->logconfig.c.mask & DDS_LC_TRACE))
        {
          tmdom->msgpos += (size_t) snprintf (tmdom->msg + tmdom->msgpos, sizeof (tmdom->msg) - tmdom->msgpos,
                                              " %"PRIu32"(%s):%c:%"PRIx32"->%"PRIx32,
                                              threadidx, thrst->name, alive ? 'a' : 'd', av_ary_cur->vt, vt);
        }

        av_ary_cur->vt = vt;
        if (av_ary_cur->alive != alive)
        {
          const char *name = thrst->name;
          const char *msg;
          if (!alive)
            msg = "failed to make progress";
          else
            msg = "once again made progress";
          DDS_CLOG (alive ? DDS_LC_INFO : DDS_LC_WARNING, &gv->logconfig, "thread %s %s\n", name ? name : "(anon)", msg);
          av_ary_cur->alive = alive;
        }
      }
    }

    /* Scan all domains: there is only a single log buffer for the thread and we need the newline
       to flush the messages if we want to avoid mixing up domains; and we'd still like to dump
       stack traces only once if there are stuck threads, even though a deadlock typically involves
       multiple threads. */
    struct ddsrt_hh_iter it;
    for (struct threadmon_domain *tmdom = ddsrt_hh_iter_first (sl->domains, &it); tmdom != NULL; tmdom = ddsrt_hh_iter_next (&it))
    {
      if (tmdom->n_not_alive == 0)
        DDS_CTRACE (&tmdom->gv->logconfig, "%s: OK\n", tmdom->msg);
      else
      {
        DDS_CTRACE (&tmdom->gv->logconfig, "%s: FAIL (%u)\n", tmdom->msg, tmdom->n_not_alive);
        if (was_alive && tmdom->gv->logconfig.c.mask != 0)
        {
          if (!sl->noprogress_log_stacktraces)
            DDS_CLOG (~DDS_LC_FATAL, &tmdom->gv->logconfig, "-- stack traces requested, but traces disabled --\n");
          else
            ddsi_log_stack_traces (&tmdom->gv->logconfig, tmdom->gv);
        }
        was_alive = false;
      }
      tmdom->n_not_alive = 0;
      tmdom->msgpos = 0;
      tmdom->msg[0] = 0;

#if DDSRT_HAVE_RUSAGE
      if (tmdom->gv->logconfig.c.mask & DDS_LC_TIMING)
      {
        ddsrt_rusage_t u;
        if (ddsrt_getrusage (DDSRT_RUSAGE_SELF, &u) == DDS_RETCODE_OK)
        {
          DDS_CLOG (DDS_LC_TIMING, &tmdom->gv->logconfig,
                    "rusage: utime %d.%09d stime %d.%09d maxrss %zu data %zu vcsw %zu ivcsw %zu\n",
                    (int) (u.utime / DDS_NSECS_IN_SEC),
                    (int) (u.utime % DDS_NSECS_IN_SEC),
                    (int) (u.stime / DDS_NSECS_IN_SEC),
                    (int) (u.stime % DDS_NSECS_IN_SEC),
                    u.maxrss, u.idrss, u.nvcsw, u.nivcsw);
        }
      }
#endif /* DDSRT_HAVE_RUSAGE */
    }

    was_alive = (n_not_alive == 0);
  }
  ddsrt_mutex_unlock (&sl->lock);
  return 0;
}

static uint32_t threadmon_domain_hash (const void *va)
{
  const struct threadmon_domain *a = va;
  const uint32_t u = (uint16_t) ((uintptr_t) a->gv >> 3);
  const uint32_t v = u * 0xb4817365;
  return v >> 16;
}

static int threadmon_domain_eq (const void *va, const void *vb)
{
  const struct threadmon_domain *a = va;
  const struct threadmon_domain *b = vb;
  return a->gv == b->gv;
}

struct ddsi_threadmon *ddsi_threadmon_new (int64_t liveliness_monitoring_interval, bool noprogress_log_stacktraces)
{
  struct ddsi_threadmon *sl;

  sl = ddsrt_malloc (sizeof (*sl));
  sl->keepgoing = -1;
  sl->thrst = NULL;
  sl->liveliness_monitoring_interval = liveliness_monitoring_interval;
  sl->noprogress_log_stacktraces = noprogress_log_stacktraces;
  sl->domains = ddsrt_hh_new (1, threadmon_domain_hash, threadmon_domain_eq);
  /* service lease update thread dynamically grows av_ary */
  sl->av_ary_size = 0;
  sl->av_ary = NULL;

  ddsrt_mutex_init (&sl->lock);
  ddsrt_cond_init (&sl->cond);
  return sl;
}

dds_return_t ddsi_threadmon_start (struct ddsi_threadmon *sl, const char *name)
{
  ddsrt_mutex_lock (&sl->lock);
  assert (sl->keepgoing == -1);
  sl->keepgoing = 1;
  ddsrt_mutex_unlock (&sl->lock);

  /* FIXME: thread properties */
  if (ddsi_create_thread_with_properties (&sl->thrst, NULL, name, (uint32_t (*) (void *)) threadmon_thread, sl) != DDS_RETCODE_OK)
    goto fail_thread;
  return 0;

 fail_thread:
  sl->keepgoing = -1;
  return DDS_RETCODE_ERROR;
}

void ddsi_threadmon_register_domain (struct ddsi_threadmon *sl, const struct ddsi_domaingv *gv)
{
  if (gv->config.liveliness_monitoring)
  {
    struct threadmon_domain *tmdom = ddsrt_malloc (sizeof (*tmdom));
    tmdom->gv = gv;
    tmdom->n_not_alive = 0;
    tmdom->msgpos = 0;
    tmdom->msg[0] = 0;

    ddsrt_mutex_lock (&sl->lock);
    ddsrt_hh_add_absent (sl->domains, tmdom);
    ddsrt_mutex_unlock (&sl->lock);
  }
}

void ddsi_threadmon_unregister_domain (struct ddsi_threadmon *sl, const struct ddsi_domaingv *gv)
{
  if (gv->config.liveliness_monitoring)
  {
    ddsrt_mutex_lock (&sl->lock);
    struct threadmon_domain dummy;
    dummy.gv = gv;
    struct threadmon_domain *tmdom = ddsrt_hh_lookup (sl->domains, &dummy);
    assert (tmdom);
    ddsrt_hh_remove_present (sl->domains, tmdom);
    ddsrt_mutex_unlock (&sl->lock);
    ddsrt_free (tmdom);
  }
}

void ddsi_threadmon_stop (struct ddsi_threadmon *sl)
{
  if (sl->keepgoing != -1)
  {
    ddsrt_mutex_lock (&sl->lock);
    sl->keepgoing = 0;
    ddsrt_cond_signal (&sl->cond);
    ddsrt_mutex_unlock (&sl->lock);
    ddsi_join_thread (sl->thrst);
  }
}

void ddsi_threadmon_free (struct ddsi_threadmon *sl)
{
#ifndef NDEBUG
  struct ddsrt_hh_iter it;
  assert (ddsrt_hh_iter_first (sl->domains, &it) == NULL);
#endif
  ddsrt_cond_destroy (&sl->cond);
  ddsrt_mutex_destroy (&sl->lock);
  ddsrt_hh_free (sl->domains);
  ddsrt_free (sl->av_ary);
  ddsrt_free (sl);
}
