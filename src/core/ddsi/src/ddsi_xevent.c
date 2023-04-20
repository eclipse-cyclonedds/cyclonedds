// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/fibheap.h"
#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__log.h"
#include "ddsi__xevent.h"
#include "ddsi__thread.h"
#include "ddsi__transmit.h"
#include "ddsi__xmsg.h"
#include "ddsi__tran.h"
#include "ddsi__sysdeps.h"

#define EVQTRACE(...) DDS_CTRACE (&evq->gv->logconfig, __VA_ARGS__)

/* This is absolute bottom for signed integers, where -x = x and yet x
   != 0 -- and note that it had better be 2's complement machine! */
#define TSCHED_DELETE ((int64_t) ((uint64_t) 1 << 63))

enum cb_sync_on_delete_state {
  CSODS_NO_SYNC_NEEDED,
  CSODS_SCHEDULED,
  CSODS_EXECUTING
};

struct ddsi_xevent
{
  ddsrt_fibheap_node_t heapnode;
  struct ddsi_xeventq *evq;
  ddsrt_mtime_t tsched;

  enum cb_sync_on_delete_state sync_state;
  union {
    ddsi_xevent_cb_t cb;
    // ensure alignment of arg is good
    void *p;
    uint64_t u64;
    double d;
  } cb;
  char arg[];
};

enum ddsi_xeventkind_nt
{
  XEVK_MSG,
  XEVK_MSG_REXMIT,
  XEVK_MSG_REXMIT_NOMERGE,
  XEVK_NT_CALLBACK
};

struct untimed_listelem {
  struct ddsi_xevent_nt *next;
};

struct ddsi_xevent_nt
{
  struct untimed_listelem listnode;
  struct ddsi_xeventq *evq;
  enum ddsi_xeventkind_nt kind;
  union {
    struct {
      /* xmsg is self-contained / relies on reference counts */
      struct ddsi_xmsg *msg;
    } msg;
    struct {
      /* xmsg is self-contained / relies on reference counts */
      struct ddsi_xmsg *msg;
      size_t queued_rexmit_bytes;
      ddsrt_avl_node_t msg_avlnode;
    } msg_rexmit; /* and msg_rexmit_nomerge */
    struct {
      void (*cb) (void *arg);
      void *arg;
    } callback;
  } u;
};

struct ddsi_xeventq {
  ddsrt_fibheap_t xevents;
  ddsrt_avl_tree_t msg_xevents;
  struct ddsi_xevent_nt *non_timed_xmit_list_oldest;
  struct ddsi_xevent_nt *non_timed_xmit_list_newest; /* undefined if ..._oldest == NULL */
  size_t non_timed_xmit_list_length;
  size_t queued_rexmit_bytes;
  size_t queued_rexmit_msgs;
  size_t max_queued_rexmit_bytes;
  size_t max_queued_rexmit_msgs;
  int terminate;
  struct ddsi_thread_state *thrst;
  struct ddsi_domaingv *gv;
  ddsrt_mutex_t lock;
  ddsrt_cond_t cond;

  size_t cum_rexmit_bytes;
};

static uint32_t xevent_thread (struct ddsi_xeventq *xevq);
static ddsrt_mtime_t earliest_in_xeventq (struct ddsi_xeventq *evq);
static int msg_xevents_cmp (const void *a, const void *b);
static int compare_xevent_tsched (const void *va, const void *vb);
static void handle_nontimed_xevent (struct ddsi_xeventq *evq, struct ddsi_xevent_nt *xev, struct ddsi_xpack *xp);

static const ddsrt_avl_treedef_t msg_xevents_treedef = DDSRT_AVL_TREEDEF_INITIALIZER_INDKEY (offsetof (struct ddsi_xevent_nt, u.msg_rexmit.msg_avlnode), offsetof (struct ddsi_xevent_nt, u.msg_rexmit.msg), msg_xevents_cmp, 0);

static const ddsrt_fibheap_def_t evq_xevents_fhdef = DDSRT_FIBHEAPDEF_INITIALIZER(offsetof (struct ddsi_xevent, heapnode), compare_xevent_tsched);

static int compare_xevent_tsched (const void *va, const void *vb)
{
  const struct ddsi_xevent *a = va;
  const struct ddsi_xevent *b = vb;
  return (a->tsched.v == b->tsched.v) ? 0 : (a->tsched.v < b->tsched.v) ? -1 : 1;
}

static void update_rexmit_counts (struct ddsi_xeventq *evq, size_t msg_rexmit_queued_rexmit_bytes)
{
  assert (msg_rexmit_queued_rexmit_bytes <= evq->queued_rexmit_bytes);
  assert (evq->queued_rexmit_msgs > 0);
  evq->queued_rexmit_bytes -= msg_rexmit_queued_rexmit_bytes;
  evq->queued_rexmit_msgs--;
  evq->cum_rexmit_bytes += msg_rexmit_queued_rexmit_bytes;
}

#if 0
static void trace_msg (struct ddsi_xeventq *evq, const char *func, const struct ddsi_xmsg *m)
{
  if (dds_get_log_mask() & DDS_LC_TRACE)
  {
    ddsi_guid_t wrguid;
    ddsi_seqno_t wrseq;
    ddsi_fragment_number_t wrfragid;
    ddsi_xmsg_guid_seq_fragid (m, &wrguid, &wrseq, &wrfragid);
    EVQTRACE(" %s("PGUIDFMT"/%"PRId64"/%"PRIu32")", func, PGUID (wrguid), wrseq, wrfragid);
  }
}
#else
static void trace_msg (UNUSED_ARG (struct ddsi_xeventq *evq), UNUSED_ARG (const char *func), UNUSED_ARG (const struct ddsi_xmsg *m))
{
}
#endif

static struct ddsi_xevent_nt *lookup_msg (struct ddsi_xeventq *evq, struct ddsi_xmsg *msg)
{
  switch (ddsi_xmsg_kind (msg))
  {
    case DDSI_XMSG_KIND_CONTROL:
    case DDSI_XMSG_KIND_DATA:
      assert (0);
      return NULL;
    case DDSI_XMSG_KIND_DATA_REXMIT:
      trace_msg (evq, "lookup-msg", msg);
      return ddsrt_avl_lookup (&msg_xevents_treedef, &evq->msg_xevents, msg);
    case DDSI_XMSG_KIND_DATA_REXMIT_NOMERGE:
      return NULL;
  }
  assert (0);
  return NULL;
}

static void remember_msg (struct ddsi_xeventq *evq, struct ddsi_xevent_nt *ev)
{
  assert (ev->kind == XEVK_MSG_REXMIT);
  trace_msg (evq, "remember-msg", ev->u.msg_rexmit.msg);
  ddsrt_avl_insert (&msg_xevents_treedef, &evq->msg_xevents, ev);
}

static void forget_msg (struct ddsi_xeventq *evq, struct ddsi_xevent_nt *ev)
{
  assert (ev->kind == XEVK_MSG_REXMIT);
  trace_msg (evq, "forget-msg", ev->u.msg_rexmit.msg);
  ddsrt_avl_delete (&msg_xevents_treedef, &evq->msg_xevents, ev);
}

static void add_to_non_timed_xmit_list (struct ddsi_xeventq *evq, struct ddsi_xevent_nt *ev)
{
  ev->listnode.next = NULL;
  if (evq->non_timed_xmit_list_oldest == NULL) {
    /* list is currently empty so add the first item (at the front) */
    evq->non_timed_xmit_list_oldest = ev;
  } else {
    evq->non_timed_xmit_list_newest->listnode.next = ev;
  }
  evq->non_timed_xmit_list_newest = ev;
  evq->non_timed_xmit_list_length++;

  if (ev->kind == XEVK_MSG_REXMIT)
    remember_msg (evq, ev);

  ddsrt_cond_broadcast (&evq->cond);
}

static struct ddsi_xevent_nt *getnext_from_non_timed_xmit_list  (struct ddsi_xeventq *evq)
{
  /* function removes and returns the first item in the list
     (from the front) and frees the container */
  struct ddsi_xevent_nt *ev = evq->non_timed_xmit_list_oldest;
  if (ev != NULL)
  {
    evq->non_timed_xmit_list_length--;
    evq->non_timed_xmit_list_oldest = ev->listnode.next;

    if (ev->kind == XEVK_MSG_REXMIT)
    {
      assert (lookup_msg (evq, ev->u.msg_rexmit.msg) == ev);
      forget_msg (evq, ev);
    }
  }
  return ev;
}

static int non_timed_xmit_list_is_empty (struct ddsi_xeventq *evq)
{
  /* check whether the "non-timed" xevent list is empty */
  return (evq->non_timed_xmit_list_oldest == NULL);
}

#ifndef NDEBUG
static int nontimed_xevent_in_queue (struct ddsi_xeventq *evq, struct ddsi_xevent_nt *ev)
{
  if (!(evq->gv->config.enabled_xchecks & DDSI_XCHECK_XEV))
    return 0;
  struct ddsi_xevent_nt *x;
  ddsrt_mutex_lock (&evq->lock);
  for (x = evq->non_timed_xmit_list_oldest; x; x = x->listnode.next)
  {
    if (x == ev)
    {
      ddsrt_mutex_unlock (&evq->lock);
      return 1;
    }
  }
  ddsrt_mutex_unlock (&evq->lock);
  return 0;
}
#endif

static void free_xevent (struct ddsi_xeventq *evq, struct ddsi_xevent *ev)
{
  (void) evq;
  ddsrt_free (ev);
}

static void ddsi_delete_xevent_nosync (struct ddsi_xevent *ev)
{
  struct ddsi_xeventq *evq = ev->evq;
  ddsrt_mutex_lock (&evq->lock);
  assert (ev->sync_state != CSODS_EXECUTING);
  /* Can delete it only once, no matter how we implement it internally */
  assert (ev->tsched.v != TSCHED_DELETE);
  assert (TSCHED_DELETE < ev->tsched.v);
  if (ev->tsched.v != DDS_NEVER)
  {
    ev->tsched.v = TSCHED_DELETE;
    ddsrt_fibheap_decrease_key (&evq_xevents_fhdef, &evq->xevents, ev);
  }
  else
  {
    ev->tsched.v = TSCHED_DELETE;
    ddsrt_fibheap_insert (&evq_xevents_fhdef, &evq->xevents, ev);
  }
  /* TSCHED_DELETE is absolute minimum time, so chances are we need to
     wake up the thread.  The superfluous signal is harmless. */
  ddsrt_cond_broadcast (&evq->cond);
  ddsrt_mutex_unlock (&evq->lock);
}

static void ddsi_delete_xevent_sync (struct ddsi_xevent *ev)
{
  struct ddsi_xeventq *evq = ev->evq;
  ddsrt_mutex_lock (&evq->lock);
  /* wait until neither scheduled nor executing; loop in case the callback reschedules the event */
  while (ev->tsched.v != DDS_NEVER || ev->sync_state == CSODS_EXECUTING)
  {
    if (ev->tsched.v != DDS_NEVER)
    {
      assert (ev->tsched.v != TSCHED_DELETE);
      ddsrt_fibheap_delete (&evq_xevents_fhdef, &evq->xevents, ev);
      ev->tsched.v = DDS_NEVER;
    }
    if (ev->sync_state == CSODS_EXECUTING)
    {
      ddsrt_cond_wait (&evq->cond, &evq->lock);
    }
  }
  ddsrt_mutex_unlock (&evq->lock);
  free_xevent (evq, ev);
}

void ddsi_delete_xevent (struct ddsi_xevent *ev)
{
  if (ev->sync_state == CSODS_NO_SYNC_NEEDED)
    ddsi_delete_xevent_nosync (ev);
  else
    ddsi_delete_xevent_sync (ev);
}

int ddsi_resched_xevent_if_earlier (struct ddsi_xevent *ev, ddsrt_mtime_t tsched)
{
  struct ddsi_xeventq *evq = ev->evq;
  int is_resched;
  if (tsched.v == DDS_NEVER)
    return 0;
  ddsrt_mutex_lock (&evq->lock);
  /* If you want to delete it, you to say so by calling the right
     function. Don't want to reschedule an event marked for deletion,
     but with TSCHED_DELETE = MIN_INT64, tsched >= ev->tsched is
     guaranteed to be false. */
  assert (tsched.v != TSCHED_DELETE);
  if (tsched.v >= ev->tsched.v)
    is_resched = 0;
  else
  {
    ddsrt_mtime_t tbefore = earliest_in_xeventq (evq);
    if (ev->tsched.v != DDS_NEVER)
    {
      ev->tsched = tsched;
      ddsrt_fibheap_decrease_key (&evq_xevents_fhdef, &evq->xevents, ev);
    }
    else
    {
      ev->tsched = tsched;
      ddsrt_fibheap_insert (&evq_xevents_fhdef, &evq->xevents, ev);
    }
    is_resched = 1;
    if (tsched.v < tbefore.v)
      ddsrt_cond_broadcast (&evq->cond);
  }
  ddsrt_mutex_unlock (&evq->lock);
  return is_resched;
}

#ifndef NDEBUG
bool ddsi_delete_xevent_pending (struct ddsi_xevent *ev)
{
  struct ddsi_xeventq *evq = ev->evq;
  ddsrt_mutex_lock (&evq->lock);
  const bool is_pending = (ev->tsched.v == TSCHED_DELETE);
  ddsrt_mutex_unlock (&evq->lock);
  return is_pending;
}
#endif

static struct ddsi_xevent_nt *qxev_common_nt (struct ddsi_xeventq *evq, enum ddsi_xeventkind_nt kind)
{
  /* qxev_common_nt is the route by which all non-timed xevents are created. */
  struct ddsi_xevent_nt *ev = ddsrt_malloc (sizeof (*ev));
  ev->evq = evq;
  ev->kind = kind;
  return ev;
}

static ddsrt_mtime_t earliest_in_xeventq (struct ddsi_xeventq *evq)
{
  struct ddsi_xevent *min;
  ASSERT_MUTEX_HELD (&evq->lock);
  return ((min = ddsrt_fibheap_min (&evq_xevents_fhdef, &evq->xevents)) != NULL) ? min->tsched : DDSRT_MTIME_NEVER;
}

static void qxev_insert (struct ddsi_xevent *ev)
{
  /* qxev_insert is how all timed xevents are registered into the
     event administration. */
  struct ddsi_xeventq *evq = ev->evq;
  ASSERT_MUTEX_HELD (&evq->lock);
  if (ev->tsched.v != DDS_NEVER)
  {
    ddsrt_mtime_t tbefore = earliest_in_xeventq (evq);
    ddsrt_fibheap_insert (&evq_xevents_fhdef, &evq->xevents, ev);
    if (ev->tsched.v < tbefore.v)
      ddsrt_cond_broadcast (&evq->cond);
  }
}

static void qxev_insert_nt (struct ddsi_xevent_nt *ev)
{
  /* qxev_insert is how all non-timed xevents are queued. */
  struct ddsi_xeventq *evq = ev->evq;
  ASSERT_MUTEX_HELD (&evq->lock);
  add_to_non_timed_xmit_list (evq, ev);
  EVQTRACE (" (%"PRIuSIZE" in queue)\n", evq->non_timed_xmit_list_length);
}

static int msg_xevents_cmp (const void *a, const void *b)
{
  return ddsi_xmsg_compare_fragid (a, b);
}

struct ddsi_xeventq * ddsi_xeventq_new (struct ddsi_domaingv *gv, size_t max_queued_rexmit_bytes, size_t max_queued_rexmit_msgs)
{
  struct ddsi_xeventq *evq = ddsrt_malloc (sizeof (*evq));
  /* limit to 2GB to prevent overflow (4GB - 64kB should be ok, too) */
  if (max_queued_rexmit_bytes > 2147483648u)
    max_queued_rexmit_bytes = 2147483648u;
  ddsrt_fibheap_init (&evq_xevents_fhdef, &evq->xevents);
  ddsrt_avl_init (&msg_xevents_treedef, &evq->msg_xevents);
  evq->non_timed_xmit_list_oldest = NULL;
  evq->non_timed_xmit_list_newest = NULL;
  evq->non_timed_xmit_list_length = 0;
  evq->terminate = 0;
  evq->thrst = NULL;
  evq->max_queued_rexmit_bytes = max_queued_rexmit_bytes;
  evq->max_queued_rexmit_msgs = max_queued_rexmit_msgs;
  evq->queued_rexmit_bytes = 0;
  evq->queued_rexmit_msgs = 0;
  evq->gv = gv;
  ddsrt_mutex_init (&evq->lock);
  ddsrt_cond_init (&evq->cond);

  evq->cum_rexmit_bytes = 0;
  return evq;
}

dds_return_t ddsi_xeventq_start (struct ddsi_xeventq *evq, const char *name)
{
  dds_return_t rc;
  char * evqname = "tev";
  assert (evq->thrst == NULL);

  if (name)
  {
    size_t slen = strlen (name) + 5;
    evqname = ddsrt_malloc (slen);
    (void) snprintf (evqname, slen, "tev.%s", name);
  }

  evq->terminate = 0;
  rc = ddsi_create_thread (&evq->thrst, evq->gv, evqname, (uint32_t (*) (void *)) xevent_thread, evq);

  if (name)
  {
    ddsrt_free (evqname);
  }
  return rc;
}

void ddsi_xeventq_stop (struct ddsi_xeventq *evq)
{
  assert (evq->thrst != NULL);
  ddsrt_mutex_lock (&evq->lock);
  evq->terminate = 1;
  ddsrt_cond_broadcast (&evq->cond);
  ddsrt_mutex_unlock (&evq->lock);
  ddsi_join_thread (evq->thrst);
  evq->thrst = NULL;
}

void ddsi_xeventq_free (struct ddsi_xeventq *evq)
{
  struct ddsi_xevent *ev;
  assert (evq->thrst == NULL);
  while ((ev = ddsrt_fibheap_extract_min (&evq_xevents_fhdef, &evq->xevents)) != NULL)
    free_xevent (evq, ev);

  {
    struct ddsi_xpack *xp = ddsi_xpack_new (evq->gv, false);
    ddsi_thread_state_awake (ddsi_lookup_thread_state (), evq->gv);
    ddsrt_mutex_lock (&evq->lock);
    while (!non_timed_xmit_list_is_empty (evq))
    {
      ddsi_thread_state_awake_to_awake_no_nest (ddsi_lookup_thread_state ());
      handle_nontimed_xevent (evq, getnext_from_non_timed_xmit_list (evq), xp);
    }
    ddsrt_mutex_unlock (&evq->lock);
    ddsi_xpack_send (xp, false);
    ddsi_xpack_free (xp);
    ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  }

  assert (ddsrt_avl_is_empty (&evq->msg_xevents));
  ddsrt_cond_destroy (&evq->cond);
  ddsrt_mutex_destroy (&evq->lock);
  ddsrt_free (evq);
}

/* EVENT QUEUE EVENT HANDLERS ******************************************************/

static void handle_timed_xevent (struct ddsi_xeventq *evq, struct ddsi_xevent *xev, struct ddsi_xpack *xp, ddsrt_mtime_t tnow)
{
  /* event rescheduling functions look at xev->tsched to
     determine whether it is currently on the heap or not (i.e.,
     scheduled or not), so set to TSCHED_NEVER to indicate it
     currently isn't. */
  xev->tsched.v = DDS_NEVER;

  /* We relinquish the lock while processing the event. */
  if (xev->sync_state == CSODS_NO_SYNC_NEEDED)
  {
    ddsrt_mutex_unlock (&evq->lock);
    xev->cb.cb (evq->gv, xev, xp, xev->arg, tnow);
    ddsrt_mutex_lock (&evq->lock);
  }
  else
  {
    xev->sync_state = CSODS_EXECUTING;
    ddsrt_mutex_unlock (&evq->lock);
    xev->cb.cb (evq->gv, xev, xp, xev->arg, tnow);
    ddsrt_mutex_lock (&evq->lock);
    xev->sync_state = CSODS_SCHEDULED;
    ddsrt_cond_broadcast (&evq->cond);
  }
}

static void handle_nontimed_xevent (struct ddsi_xeventq *evq, struct ddsi_xevent_nt *xev, struct ddsi_xpack *xp)
{
   /* This function handles the individual xevent irrespective of
      whether it is a "timed" or "non-timed" xevent */
  size_t msg_rexmit_queued_rexmit_bytes = SIZE_MAX;

  /* We relinquish the lock while processing the event, but require it
     held for administrative work. */
  ASSERT_MUTEX_HELD (&evq->lock);
  ddsrt_mutex_unlock (&evq->lock);
  switch (xev->kind)
  {
    case XEVK_MSG:
      assert (!nontimed_xevent_in_queue (evq, xev));
      ddsi_xpack_addmsg (xp, xev->u.msg.msg, 0);
      break;
    case XEVK_MSG_REXMIT:
    case XEVK_MSG_REXMIT_NOMERGE:
      assert (!nontimed_xevent_in_queue (evq, xev));
      ddsi_xpack_addmsg (xp, xev->u.msg_rexmit.msg, 0);
      msg_rexmit_queued_rexmit_bytes = xev->u.msg_rexmit.queued_rexmit_bytes;
      assert (msg_rexmit_queued_rexmit_bytes < SIZE_MAX);
      break;
    case XEVK_NT_CALLBACK:
      xev->u.callback.cb (xev->u.callback.arg);
      break;
  }
  ddsrt_free (xev);
  ddsrt_mutex_lock (&evq->lock);
  if (msg_rexmit_queued_rexmit_bytes < SIZE_MAX) {
    update_rexmit_counts (evq, msg_rexmit_queued_rexmit_bytes);
  }
}

static void handle_xevents (struct ddsi_thread_state * const thrst, struct ddsi_xeventq *xevq, struct ddsi_xpack *xp, ddsrt_mtime_t tnow)
{
  ASSERT_MUTEX_HELD (&xevq->lock);
  assert (ddsi_thread_is_awake ());

  /* The following loops give priority to the "timed" events (heartbeats,
     acknacks etc) if there are any.  The algorithm is that we handle all
     "timed" events that are scheduled now and then handle one "non-timed"
     event.  If there weren't any "non-timed" events then the loop
     terminates.  If there was one, then after handling it, re-read the
     clock and continue the loop, i.e. test again to see whether any
     "timed" events are now due. */

  bool cont;
  do {
    cont = false;
    while (earliest_in_xeventq (xevq).v <= tnow.v)
    {
      struct ddsi_xevent *xev = ddsrt_fibheap_extract_min (&evq_xevents_fhdef, &xevq->xevents);
      if (xev->tsched.v == TSCHED_DELETE)
        free_xevent (xevq, xev);
      else
      {
        ddsi_thread_state_awake_to_awake_no_nest (thrst);
        handle_timed_xevent (xevq, xev, xp, tnow);
        cont = true;
      }
    }

    if (!non_timed_xmit_list_is_empty (xevq))
    {
      struct ddsi_xevent_nt *xev = getnext_from_non_timed_xmit_list (xevq);
      ddsi_thread_state_awake_to_awake_no_nest (thrst);
      handle_nontimed_xevent (xevq, xev, xp);
      cont = true;
    }

    tnow = ddsrt_time_monotonic ();
  } while (cont);
  ASSERT_MUTEX_HELD (&xevq->lock);
}

static uint32_t xevent_thread (struct ddsi_xeventq * xevq)
{
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsrt_mtime_t next_thread_cputime = { 0 };

  struct ddsi_xpack * const xp = ddsi_xpack_new (xevq->gv, false);
  ddsrt_mutex_lock (&xevq->lock);
  while (!xevq->terminate)
  {
    ddsrt_mtime_t tnow = ddsrt_time_monotonic ();

    LOG_THREAD_CPUTIME (&xevq->gv->logconfig, next_thread_cputime);

    ddsi_thread_state_awake_fixed_domain (thrst);
    handle_xevents (thrst, xevq, xp, tnow);
    /* Send to the network unlocked, as it may sleep due to bandwidth limitation */
    ddsrt_mutex_unlock (&xevq->lock);
    ddsi_xpack_send (xp, false);
    ddsrt_mutex_lock (&xevq->lock);
    ddsi_thread_state_asleep (thrst);

    if (!non_timed_xmit_list_is_empty (xevq) || xevq->terminate)
    {
      /* continue immediately */
    }
    else
    {
      ddsrt_mtime_t twakeup = earliest_in_xeventq (xevq);
      if (twakeup.v == DDS_NEVER)
      {
        /* no scheduled events nor any non-timed events */
        ddsrt_cond_wait (&xevq->cond, &xevq->lock);
      }
      else
      {
        /* "Wrong" time-base here ... should make cond_waitfor time-base aware */
        tnow = ddsrt_time_monotonic ();
        if (twakeup.v > tnow.v)
        {
          twakeup.v -= tnow.v;
          ddsrt_cond_waitfor (&xevq->cond, &xevq->lock, twakeup.v);
        }
      }
    }
  }
  ddsrt_mutex_unlock (&xevq->lock);
  ddsi_xpack_send (xp, false);
  ddsi_xpack_free (xp);
  return 0;
}

void ddsi_qxev_msg (struct ddsi_xeventq *evq, struct ddsi_xmsg *msg)
{
  struct ddsi_xevent_nt *ev;
  assert (evq);
  assert (ddsi_xmsg_kind (msg) != DDSI_XMSG_KIND_DATA_REXMIT);
  ddsrt_mutex_lock (&evq->lock);
  ev = qxev_common_nt (evq, XEVK_MSG);
  ev->u.msg.msg = msg;
  qxev_insert_nt (ev);
  ddsrt_mutex_unlock (&evq->lock);
}

void ddsi_qxev_nt_callback (struct ddsi_xeventq *evq, void (*cb) (void *arg), void *arg)
{
  struct ddsi_xevent_nt *ev;
  assert (evq);
  ddsrt_mutex_lock (&evq->lock);
  ev = qxev_common_nt (evq, XEVK_NT_CALLBACK);
  ev->u.callback.cb = cb;
  ev->u.callback.arg = arg;
  qxev_insert_nt (ev);
  ddsrt_mutex_unlock (&evq->lock);
}

enum ddsi_qxev_msg_rexmit_result ddsi_qxev_msg_rexmit_wrlock_held (struct ddsi_xeventq *evq, struct ddsi_xmsg *msg, int force)
{
  struct ddsi_domaingv * const gv = evq->gv;
  size_t msg_size = ddsi_xmsg_size (msg);
  struct ddsi_xevent_nt *existing_ev;

  assert (evq);
  assert (ddsi_xmsg_kind (msg) == DDSI_XMSG_KIND_DATA_REXMIT || ddsi_xmsg_kind (msg) == DDSI_XMSG_KIND_DATA_REXMIT_NOMERGE);
  ddsrt_mutex_lock (&evq->lock);
  if ((existing_ev = lookup_msg (evq, msg)) != NULL && ddsi_xmsg_merge_rexmit_destinations_wrlock_held (gv, existing_ev->u.msg_rexmit.msg, msg))
  {
    /* MSG got merged with a pending retransmit, so it has effectively been queued */
    ddsrt_mutex_unlock (&evq->lock);
    ddsi_xmsg_free (msg);
    return DDSI_QXEV_MSG_REXMIT_MERGED;
  }
  else if ((evq->queued_rexmit_bytes > evq->max_queued_rexmit_bytes ||
            evq->queued_rexmit_msgs == evq->max_queued_rexmit_msgs) &&
           !force)
  {
    /* drop it if insufficient resources available */
    ddsrt_mutex_unlock (&evq->lock);
    ddsi_xmsg_free (msg);
#if 0
    GVTRACE (" qxev_msg_rexmit%s drop (sz %"PA_PRIuSIZE" qb %"PA_PRIuSIZE" qm %"PA_PRIuSIZE")", force ? "!" : "",
             msg_size, evq->queued_rexmit_bytes, evq->queued_rexmit_msgs);
#endif
    return DDSI_QXEV_MSG_REXMIT_DROPPED;
  }
  else
  {
    // kind == rexmit && existing_ev != NULL (i.e., same writer, sequence number and fragment already enqueued,
    // but not mergeable despite both entries not being of the NOMERGE kind) is really rare but not impossible.
    // Treating it as XEVK_MSG_REXMIT would lead to attempting to insert a duplicate for (GUID,seq#,frag#) into
    // the table of enqueued retransmits and that is disallowed by the default settings of the AVL tree used
    // to implement the table. That leaves two options:
    // - Preventing this new message from getting inserted. Only those of kind REXMIT get inserted, so simply
    //   marking it as REXMIT_NOMERGE will do that. Downside, the new copy will never be a candidate for
    //   merging.
    // - Marking the AVL tree as ALLOWDUPS. Once upon a time a heavily used feature of this tree implementation
    //   but not used for a long time and perhaps best removed. Reintroducing a usage for doubtful benefit had
    //   better have a positive effect. The lookup logic always returns the first match in insertion order, so
    //   the new entry won't be considered for merging until the first one has been sent.
    // So in a very rare case, there'd be a (presumably) even rarer case where the second option confers a
    // benefit. The first has the advantage of being simpler, which weighs heaver in my opinion.
    const enum ddsi_xeventkind_nt kind =
      (ddsi_xmsg_kind (msg) == DDSI_XMSG_KIND_DATA_REXMIT && existing_ev == NULL) ? XEVK_MSG_REXMIT : XEVK_MSG_REXMIT_NOMERGE;
    struct ddsi_xevent_nt *ev = qxev_common_nt (evq, kind);
    ev->u.msg_rexmit.msg = msg;
    ev->u.msg_rexmit.queued_rexmit_bytes = msg_size;
    evq->queued_rexmit_bytes += msg_size;
    evq->queued_rexmit_msgs++;
    qxev_insert_nt (ev);
#if 0
    GVTRACE ("AAA(%p,%"PA_PRIuSIZE")", (void *) ev, msg_size);
#endif
    ddsrt_mutex_unlock (&evq->lock);
    return DDSI_QXEV_MSG_REXMIT_QUEUED;
  }
}

struct ddsi_xevent *ddsi_qxev_callback (struct ddsi_xeventq *evq, ddsrt_mtime_t tsched, ddsi_xevent_cb_t cb, const void *arg, size_t arg_size, bool sync_on_delete)
{
  assert (tsched.v != TSCHED_DELETE);
  struct ddsi_xevent *ev = ddsrt_malloc (sizeof (*ev) + arg_size);
  ev->evq = evq;
  ev->tsched = tsched;
  ev->cb.cb = cb;
  ev->sync_state = sync_on_delete ? CSODS_SCHEDULED : CSODS_NO_SYNC_NEEDED;
  if (arg_size) // so arg = NULL, arg_size = 0 is allowed
    memcpy (ev->arg, arg, arg_size);
  ddsrt_mutex_lock (&evq->lock);
  qxev_insert (ev);
  ddsrt_mutex_unlock (&evq->lock);
  return ev;
}
