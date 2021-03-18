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
#include <math.h>
#include <stdlib.h>

#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"

#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/fibheap.h"

#include "dds/ddsi/q_log.h"
#include "dds/ddsi/q_addrset.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsi/q_xevent.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_transmit.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/q_bitset.h"
#include "dds/ddsi/q_lease.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_serdata_default.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_pmd.h"
#include "dds/ddsi/ddsi_acknack.h"
#include "dds__whc.h"

#include "dds/ddsi/sysdeps.h"

#define EVQTRACE(...) DDS_CTRACE (&evq->gv->logconfig, __VA_ARGS__)

/* This is absolute bottom for signed integers, where -x = x and yet x
   != 0 -- and note that it had better be 2's complement machine! */
#define TSCHED_DELETE ((int64_t) ((uint64_t) 1 << 63))

enum xeventkind
{
  XEVK_HEARTBEAT,
  XEVK_ACKNACK,
  XEVK_SPDP,
  XEVK_PMD_UPDATE,
  XEVK_DELETE_WRITER,
  XEVK_CALLBACK
};

struct xevent
{
  ddsrt_fibheap_node_t heapnode;
  struct xeventq *evq;
  ddsrt_mtime_t tsched;
  enum xeventkind kind;
  union {
    struct {
      ddsi_guid_t wr_guid;
    } heartbeat;
    struct {
      ddsi_guid_t pwr_guid;
      ddsi_guid_t rd_guid;
    } acknack;
    struct {
      ddsi_guid_t pp_guid;
      ddsi_guid_prefix_t dest_proxypp_guid_prefix; /* only if "directed" */
      int directed; /* if 0, undirected; if > 0, number of directed ones to send in reasonably short succession */
    } spdp;
    struct {
      ddsi_guid_t pp_guid;
    } pmd_update;
#if 0
    struct {
    } info;
#endif
    struct {
      ddsi_guid_t guid;
    } delete_writer;
    struct {
      void (*cb) (struct xevent *ev, void *arg, ddsrt_mtime_t tnow);
      void *arg;
      bool executing;
    } callback;
  } u;
};

enum xeventkind_nt
{
  XEVK_MSG,
  XEVK_MSG_REXMIT,
  XEVK_MSG_REXMIT_NOMERGE,
  XEVK_ENTITYID,
  XEVK_NT_CALLBACK
};

struct untimed_listelem {
  struct xevent_nt *next;
};

struct xevent_nt
{
  struct untimed_listelem listnode;
  struct xeventq *evq;
  enum xeventkind_nt kind;
  union {
    struct {
      /* xmsg is self-contained / relies on reference counts */
      struct nn_xmsg *msg;
    } msg;
    struct {
      /* xmsg is self-contained / relies on reference counts */
      struct nn_xmsg *msg;
      size_t queued_rexmit_bytes;
      ddsrt_avl_node_t msg_avlnode;
    } msg_rexmit; /* and msg_rexmit_nomerge */
    struct {
      /* xmsg is self-contained / relies on reference counts */
      struct nn_xmsg *msg;
    } entityid;
    struct {
      void (*cb) (void *arg);
      void *arg;
    } callback;
  } u;
};

struct xeventq {
  ddsrt_fibheap_t xevents;
  ddsrt_avl_tree_t msg_xevents;
  struct xevent_nt *non_timed_xmit_list_oldest;
  struct xevent_nt *non_timed_xmit_list_newest; /* undefined if ..._oldest == NULL */
  size_t queued_rexmit_bytes;
  size_t queued_rexmit_msgs;
  size_t max_queued_rexmit_bytes;
  size_t max_queued_rexmit_msgs;
  int terminate;
  struct thread_state1 *ts;
  struct ddsi_domaingv *gv;
  ddsrt_mutex_t lock;
  ddsrt_cond_t cond;
  ddsi_tran_conn_t tev_conn;
  uint32_t auxiliary_bandwidth_limit;

  size_t cum_rexmit_bytes;
};

static uint32_t xevent_thread (struct xeventq *xevq);
static ddsrt_mtime_t earliest_in_xeventq (struct xeventq *evq);
static int msg_xevents_cmp (const void *a, const void *b);
static int compare_xevent_tsched (const void *va, const void *vb);
static void handle_nontimed_xevent (struct xevent_nt *xev, struct nn_xpack *xp);

static const ddsrt_avl_treedef_t msg_xevents_treedef = DDSRT_AVL_TREEDEF_INITIALIZER_INDKEY (offsetof (struct xevent_nt, u.msg_rexmit.msg_avlnode), offsetof (struct xevent_nt, u.msg_rexmit.msg), msg_xevents_cmp, 0);

static const ddsrt_fibheap_def_t evq_xevents_fhdef = DDSRT_FIBHEAPDEF_INITIALIZER(offsetof (struct xevent, heapnode), compare_xevent_tsched);

static int compare_xevent_tsched (const void *va, const void *vb)
{
  const struct xevent *a = va;
  const struct xevent *b = vb;
  return (a->tsched.v == b->tsched.v) ? 0 : (a->tsched.v < b->tsched.v) ? -1 : 1;
}

static void update_rexmit_counts (struct xeventq *evq, struct xevent_nt *ev)
{
#if 0
  EVQTRACE ("ZZZ(%p,%"PRIuSIZE")", (void *) ev, ev->u.msg_rexmit.queued_rexmit_bytes);
#endif
  assert (ev->kind == XEVK_MSG_REXMIT || ev->kind == XEVK_MSG_REXMIT_NOMERGE);
  assert (ev->u.msg_rexmit.queued_rexmit_bytes <= evq->queued_rexmit_bytes);
  assert (evq->queued_rexmit_msgs > 0);
  evq->queued_rexmit_bytes -= ev->u.msg_rexmit.queued_rexmit_bytes;
  evq->queued_rexmit_msgs--;
  evq->cum_rexmit_bytes += ev->u.msg_rexmit.queued_rexmit_bytes;
}

#if 0
static void trace_msg (struct xeventq *evq, const char *func, const struct nn_xmsg *m)
{
  if (dds_get_log_mask() & DDS_LC_TRACE)
  {
    ddsi_guid_t wrguid;
    seqno_t wrseq;
    nn_fragment_number_t wrfragid;
    nn_xmsg_guid_seq_fragid (m, &wrguid, &wrseq, &wrfragid);
    EVQTRACE(" %s("PGUIDFMT"/%"PRId64"/%u)", func, PGUID (wrguid), wrseq, wrfragid);
  }
}
#else
static void trace_msg (UNUSED_ARG (struct xeventq *evq), UNUSED_ARG (const char *func), UNUSED_ARG (const struct nn_xmsg *m))
{
}
#endif

static struct xevent_nt *lookup_msg (struct xeventq *evq, struct nn_xmsg *msg)
{
  switch (nn_xmsg_kind (msg))
  {
    case NN_XMSG_KIND_CONTROL:
    case NN_XMSG_KIND_DATA:
      assert (0);
      return NULL;
    case NN_XMSG_KIND_DATA_REXMIT:
      trace_msg (evq, "lookup-msg", msg);
      return ddsrt_avl_lookup (&msg_xevents_treedef, &evq->msg_xevents, msg);
    case NN_XMSG_KIND_DATA_REXMIT_NOMERGE:
      return NULL;
  }
  assert (0);
  return NULL;
}

static void remember_msg (struct xeventq *evq, struct xevent_nt *ev)
{
  assert (ev->kind == XEVK_MSG_REXMIT);
  trace_msg (evq, "remember-msg", ev->u.msg_rexmit.msg);
  ddsrt_avl_insert (&msg_xevents_treedef, &evq->msg_xevents, ev);
}

static void forget_msg (struct xeventq *evq, struct xevent_nt *ev)
{
  assert (ev->kind == XEVK_MSG_REXMIT);
  trace_msg (evq, "forget-msg", ev->u.msg_rexmit.msg);
  ddsrt_avl_delete (&msg_xevents_treedef, &evq->msg_xevents, ev);
}

static void add_to_non_timed_xmit_list (struct xeventq *evq, struct xevent_nt *ev)
{
  ev->listnode.next = NULL;
  if (evq->non_timed_xmit_list_oldest == NULL) {
    /* list is currently empty so add the first item (at the front) */
    evq->non_timed_xmit_list_oldest = ev;
  } else {
    evq->non_timed_xmit_list_newest->listnode.next = ev;
  }
  evq->non_timed_xmit_list_newest = ev;

  if (ev->kind == XEVK_MSG_REXMIT)
    remember_msg (evq, ev);

  ddsrt_cond_broadcast (&evq->cond);
}

static struct xevent_nt *getnext_from_non_timed_xmit_list  (struct xeventq *evq)
{
  /* function removes and returns the first item in the list
     (from the front) and frees the container */
  struct xevent_nt *ev = evq->non_timed_xmit_list_oldest;
  if (ev != NULL)
  {
    evq->non_timed_xmit_list_oldest = ev->listnode.next;

    if (ev->kind == XEVK_MSG_REXMIT)
    {
      assert (lookup_msg (evq, ev->u.msg_rexmit.msg) == ev);
      forget_msg (evq, ev);
    }
  }
  return ev;
}

static int non_timed_xmit_list_is_empty (struct xeventq *evq)
{
  /* check whether the "non-timed" xevent list is empty */
  return (evq->non_timed_xmit_list_oldest == NULL);
}

static int compute_non_timed_xmit_list_size (struct xeventq *evq)
{
  /* returns how many "non-timed" xevents are pending by counting the
     number of events in the list -- it'd be easy to compute the
     length incrementally in the add_... and next_... functions, but
     it isn't really being used anywhere, so why bother? */
  struct xevent_nt *current = evq->non_timed_xmit_list_oldest;
  int i = 0;
  while (current)
  {
    current = current->listnode.next;
    i++;
  }
  return i;
}

#ifndef NDEBUG
static int nontimed_xevent_in_queue (struct xeventq *evq, struct xevent_nt *ev)
{
  if (!(evq->gv->config.enabled_xchecks & DDSI_XCHECK_XEV))
    return 0;
  struct xevent_nt *x;
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

static void free_xevent (struct xeventq *evq, struct xevent *ev)
{
  (void) evq;
  if (ev->tsched.v != TSCHED_DELETE)
  {
    switch (ev->kind)
    {
      case XEVK_HEARTBEAT:
      case XEVK_ACKNACK:
      case XEVK_SPDP:
      case XEVK_PMD_UPDATE:
      case XEVK_DELETE_WRITER:
      case XEVK_CALLBACK:
        break;
    }
  }
  ddsrt_free (ev);
}

void delete_xevent (struct xevent *ev)
{
  struct xeventq *evq = ev->evq;
  ddsrt_mutex_lock (&evq->lock);
  assert (ev->kind != XEVK_CALLBACK || ev->u.callback.executing);
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

void delete_xevent_callback (struct xevent *ev)
{
  struct xeventq *evq = ev->evq;
  assert (ev->kind == XEVK_CALLBACK);
  ddsrt_mutex_lock (&evq->lock);
  /* wait until neither scheduled nor executing; loop in case the callback reschedules the event */
  while (ev->tsched.v != DDS_NEVER || ev->u.callback.executing)
  {
    if (ev->tsched.v != DDS_NEVER)
    {
      assert (ev->tsched.v != TSCHED_DELETE);
      ddsrt_fibheap_delete (&evq_xevents_fhdef, &evq->xevents, ev);
      ev->tsched.v = DDS_NEVER;
    }
    if (ev->u.callback.executing)
    {
      ddsrt_cond_wait (&evq->cond, &evq->lock);
    }
  }
  ddsrt_mutex_unlock (&evq->lock);
  free_xevent (evq, ev);
}

int resched_xevent_if_earlier (struct xevent *ev, ddsrt_mtime_t tsched)
{
  struct xeventq *evq = ev->evq;
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

static ddsrt_mtime_t mtime_round_up (ddsrt_mtime_t t, int64_t round)
{
  /* This function rounds up t to the nearest next multiple of round.
     t is nanoseconds, round is milliseconds.  Avoid functions from
     maths libraries to keep code portable */
  assert (t.v >= 0 && round >= 0);
  if (round == 0 || t.v == DDS_INFINITY)
    return t;
  else
  {
    int64_t remainder = t.v % round;
    if (remainder == 0)
      return t;
    else
      return (ddsrt_mtime_t) { t.v + round - remainder };
  }
}

static struct xevent *qxev_common (struct xeventq *evq, ddsrt_mtime_t tsched, enum xeventkind kind)
{
  /* qxev_common is the route by which all timed xevents are
     created. */
  struct xevent *ev = ddsrt_malloc (sizeof (*ev));

  assert (tsched.v != TSCHED_DELETE);
  ASSERT_MUTEX_HELD (&evq->lock);

  /* round up the scheduled time if required */
  if (tsched.v != DDS_NEVER && evq->gv->config.schedule_time_rounding != 0)
  {
    ddsrt_mtime_t tsched_rounded = mtime_round_up (tsched, evq->gv->config.schedule_time_rounding);
    EVQTRACE ("rounded event scheduled for %"PRId64" to %"PRId64"\n", tsched.v, tsched_rounded.v);
    tsched = tsched_rounded;
  }

  ev->evq = evq;
  ev->tsched = tsched;
  ev->kind = kind;
  return ev;
}

static struct xevent_nt *qxev_common_nt (struct xeventq *evq, enum xeventkind_nt kind)
{
  /* qxev_common_nt is the route by which all non-timed xevents are created. */
  struct xevent_nt *ev = ddsrt_malloc (sizeof (*ev));
  ev->evq = evq;
  ev->kind = kind;
  return ev;
}

static ddsrt_mtime_t earliest_in_xeventq (struct xeventq *evq)
{
  struct xevent *min;
  ASSERT_MUTEX_HELD (&evq->lock);
  return ((min = ddsrt_fibheap_min (&evq_xevents_fhdef, &evq->xevents)) != NULL) ? min->tsched : DDSRT_MTIME_NEVER;
}

static void qxev_insert (struct xevent *ev)
{
  /* qxev_insert is how all timed xevents are registered into the
     event administration. */
  struct xeventq *evq = ev->evq;
  ASSERT_MUTEX_HELD (&evq->lock);
  if (ev->tsched.v != DDS_NEVER)
  {
    ddsrt_mtime_t tbefore = earliest_in_xeventq (evq);
    ddsrt_fibheap_insert (&evq_xevents_fhdef, &evq->xevents, ev);
    if (ev->tsched.v < tbefore.v)
      ddsrt_cond_broadcast (&evq->cond);
  }
}

static void qxev_insert_nt (struct xevent_nt *ev)
{
  /* qxev_insert is how all non-timed xevents are queued. */
  struct xeventq *evq = ev->evq;
  ASSERT_MUTEX_HELD (&evq->lock);
  add_to_non_timed_xmit_list (evq, ev);
  EVQTRACE ("non-timed queue now has %d items\n", compute_non_timed_xmit_list_size (evq));
}

static int msg_xevents_cmp (const void *a, const void *b)
{
  return nn_xmsg_compare_fragid (a, b);
}

struct xeventq * xeventq_new
(
  ddsi_tran_conn_t conn,
  size_t max_queued_rexmit_bytes,
  size_t max_queued_rexmit_msgs,
  uint32_t auxiliary_bandwidth_limit
)
{
  struct xeventq *evq = ddsrt_malloc (sizeof (*evq));
  /* limit to 2GB to prevent overflow (4GB - 64kB should be ok, too) */
  if (max_queued_rexmit_bytes > 2147483648u)
    max_queued_rexmit_bytes = 2147483648u;
  ddsrt_fibheap_init (&evq_xevents_fhdef, &evq->xevents);
  ddsrt_avl_init (&msg_xevents_treedef, &evq->msg_xevents);
  evq->non_timed_xmit_list_oldest = NULL;
  evq->non_timed_xmit_list_newest = NULL;
  evq->terminate = 0;
  evq->ts = NULL;
  evq->max_queued_rexmit_bytes = max_queued_rexmit_bytes;
  evq->max_queued_rexmit_msgs = max_queued_rexmit_msgs;
  evq->auxiliary_bandwidth_limit = auxiliary_bandwidth_limit;
  evq->queued_rexmit_bytes = 0;
  evq->queued_rexmit_msgs = 0;
  evq->tev_conn = conn;
  evq->gv = conn->m_base.gv;
  ddsrt_mutex_init (&evq->lock);
  ddsrt_cond_init (&evq->cond);

  evq->cum_rexmit_bytes = 0;
  return evq;
}

dds_return_t xeventq_start (struct xeventq *evq, const char *name)
{
  dds_return_t rc;
  char * evqname = "tev";
  assert (evq->ts == NULL);

  if (name)
  {
    size_t slen = strlen (name) + 5;
    evqname = ddsrt_malloc (slen);
    (void) snprintf (evqname, slen, "tev.%s", name);
  }

  evq->terminate = 0;
  rc = create_thread (&evq->ts, evq->gv, evqname, (uint32_t (*) (void *)) xevent_thread, evq);

  if (name)
  {
    ddsrt_free (evqname);
  }
  return rc;
}

void xeventq_stop (struct xeventq *evq)
{
  assert (evq->ts != NULL);
  ddsrt_mutex_lock (&evq->lock);
  evq->terminate = 1;
  ddsrt_cond_broadcast (&evq->cond);
  ddsrt_mutex_unlock (&evq->lock);
  join_thread (evq->ts);
  evq->ts = NULL;
}

void xeventq_free (struct xeventq *evq)
{
  struct xevent *ev;
  assert (evq->ts == NULL);
  while ((ev = ddsrt_fibheap_extract_min (&evq_xevents_fhdef, &evq->xevents)) != NULL)
    free_xevent (evq, ev);

  {
    struct nn_xpack *xp = nn_xpack_new (evq->tev_conn, evq->auxiliary_bandwidth_limit, false);
    thread_state_awake (lookup_thread_state (), evq->gv);
    ddsrt_mutex_lock (&evq->lock);
    while (!non_timed_xmit_list_is_empty (evq))
    {
      thread_state_awake_to_awake_no_nest (lookup_thread_state ());
      handle_nontimed_xevent (getnext_from_non_timed_xmit_list (evq), xp);
    }
    ddsrt_mutex_unlock (&evq->lock);
    nn_xpack_send (xp, false);
    nn_xpack_free (xp);
    thread_state_asleep (lookup_thread_state ());
  }

  assert (ddsrt_avl_is_empty (&evq->msg_xevents));
  ddsrt_cond_destroy (&evq->cond);
  ddsrt_mutex_destroy (&evq->lock);
  ddsrt_free (evq);
}

/* EVENT QUEUE EVENT HANDLERS ******************************************************/

static void handle_xevk_msg (struct nn_xpack *xp, struct xevent_nt *ev)
{
  assert (!nontimed_xevent_in_queue (ev->evq, ev));
  nn_xpack_addmsg (xp, ev->u.msg.msg, 0);
}

static void handle_xevk_msg_rexmit (struct nn_xpack *xp, struct xevent_nt *ev)
{
  struct xeventq *evq = ev->evq;

  assert (!nontimed_xevent_in_queue (ev->evq, ev));

  nn_xpack_addmsg (xp, ev->u.msg_rexmit.msg, 0);

  /* FIXME: less than happy about having to relock the queue for a
     little while here */
  ddsrt_mutex_lock (&evq->lock);
  update_rexmit_counts (evq, ev);
  ddsrt_mutex_unlock (&evq->lock);
}

static void handle_xevk_entityid (struct nn_xpack *xp, struct xevent_nt *ev)
{
  assert (!nontimed_xevent_in_queue (ev->evq, ev));
  nn_xpack_addmsg (xp, ev->u.entityid.msg, 0);
}

#ifdef DDS_HAS_SECURITY
static int send_heartbeat_to_all_readers_check_and_sched (struct xevent *ev, struct writer *wr, const struct whc_state *whcst, ddsrt_mtime_t tnow, ddsrt_mtime_t *t_next)
{
  int send;
  if (!writer_must_have_hb_scheduled (wr, whcst))
  {
    wr->hbcontrol.tsched = DDSRT_MTIME_NEVER;
    send = -1;
  }
  else if (!writer_hbcontrol_must_send (wr, whcst, tnow))
  {
    wr->hbcontrol.tsched = ddsrt_mtime_add_duration (tnow, writer_hbcontrol_intv (wr, whcst, tnow));
    send = -1;
  }
  else
  {
    const int hbansreq = writer_hbcontrol_ack_required (wr, whcst, tnow);
    wr->hbcontrol.tsched = ddsrt_mtime_add_duration (tnow, writer_hbcontrol_intv (wr, whcst, tnow));
    send = hbansreq;
  }

  resched_xevent_if_earlier (ev, wr->hbcontrol.tsched);
  *t_next = wr->hbcontrol.tsched;
  return send;
}

static void send_heartbeat_to_all_readers (struct nn_xpack *xp, struct xevent *ev, struct writer *wr, ddsrt_mtime_t tnow)
{
  struct whc_state whcst;
  ddsrt_mtime_t t_next;
  unsigned count = 0;

  ddsrt_mutex_lock (&wr->e.lock);

  whc_get_state(wr->whc, &whcst);
  const int hbansreq = send_heartbeat_to_all_readers_check_and_sched (ev, wr, &whcst, tnow, &t_next);
  if (hbansreq >= 0)
  {
    struct wr_prd_match *m;
    struct ddsi_guid last_guid = { .prefix = {.u = {0,0,0}}, .entityid = {0} };

    while ((m = ddsrt_avl_lookup_succ (&wr_readers_treedef, &wr->readers, &last_guid)) != NULL)
    {
      last_guid = m->prd_guid;
      if (m->seq < m->last_seq)
      {
        struct proxy_reader *prd;

        prd = entidx_lookup_proxy_reader_guid(wr->e.gv->entity_index, &m->prd_guid);
        if (prd)
        {
          ETRACE (wr, " heartbeat(wr "PGUIDFMT" rd "PGUIDFMT" %s) send, resched in %g s (min-ack %"PRId64", avail-seq %"PRId64")\n",
              PGUID (wr->e.guid),
              PGUID (m->prd_guid),
              hbansreq ? "" : " final",
              (double)(t_next.v - tnow.v) / 1e9,
              m->seq,
              m->last_seq);

          struct nn_xmsg *msg = writer_hbcontrol_p2p(wr, &whcst, hbansreq, prd);
          if (msg != NULL)
          {
            ddsrt_mutex_unlock (&wr->e.lock);
            nn_xpack_addmsg (xp, msg, 0);
            ddsrt_mutex_lock (&wr->e.lock);
          }
          count++;
        }
      }
    }
  }

  if (count == 0)
  {
    ETRACE (wr, "heartbeat(wr "PGUIDFMT") suppressed, resched in %g s (min-ack %"PRId64"%s, avail-seq %"PRId64", xmit %"PRId64")\n",
        PGUID (wr->e.guid),
        (t_next.v == DDS_NEVER) ? INFINITY : (double)(t_next.v - tnow.v) / 1e9,
        ddsrt_avl_is_empty (&wr->readers) ? (int64_t) -1 : ((struct wr_prd_match *) ddsrt_avl_root (&wr_readers_treedef, &wr->readers))->min_seq,
        ddsrt_avl_is_empty (&wr->readers) || ((struct wr_prd_match *) ddsrt_avl_root (&wr_readers_treedef, &wr->readers))->all_have_replied_to_hb ? "" : "!",
        whcst.max_seq,
        writer_read_seq_xmit(wr));
  }

  ddsrt_mutex_unlock (&wr->e.lock);
}
#endif

static void handle_xevk_heartbeat (struct nn_xpack *xp, struct xevent *ev, ddsrt_mtime_t tnow)
{
  struct ddsi_domaingv const * const gv = ev->evq->gv;
  struct nn_xmsg *msg;
  struct writer *wr;
  ddsrt_mtime_t t_next;
  int hbansreq = 0;
  struct whc_state whcst;

  if ((wr = entidx_lookup_writer_guid (gv->entity_index, &ev->u.heartbeat.wr_guid)) == NULL)
  {
    GVTRACE("heartbeat(wr "PGUIDFMT") writer gone\n", PGUID (ev->u.heartbeat.wr_guid));
    return;
  }

#ifdef DDS_HAS_SECURITY
  if (wr->e.guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER)
  {
    send_heartbeat_to_all_readers(xp, ev, wr, tnow);
    return;
  }
#endif

  ddsrt_mutex_lock (&wr->e.lock);
  assert (wr->reliable);
  whc_get_state(wr->whc, &whcst);
  if (!writer_must_have_hb_scheduled (wr, &whcst))
  {
    hbansreq = 1; /* just for trace */
    msg = NULL; /* Need not send it now, and no need to schedule it for the future */
    t_next.v = DDS_NEVER;
  }
  else if (!writer_hbcontrol_must_send (wr, &whcst, tnow))
  {
    hbansreq = 1; /* just for trace */
    msg = NULL;
    t_next.v = tnow.v + writer_hbcontrol_intv (wr, &whcst, tnow);
  }
  else
  {
    hbansreq = writer_hbcontrol_ack_required (wr, &whcst, tnow);
    msg = writer_hbcontrol_create_heartbeat (wr, &whcst, tnow, hbansreq, 0);
    t_next.v = tnow.v + writer_hbcontrol_intv (wr, &whcst, tnow);
  }

  GVTRACE ("heartbeat(wr "PGUIDFMT"%s) %s, resched in %g s (min-ack %"PRId64"%s, avail-seq %"PRId64", xmit %"PRId64")\n",
           PGUID (wr->e.guid),
           hbansreq ? "" : " final",
           msg ? "sent" : "suppressed",
           (t_next.v == DDS_NEVER) ? INFINITY : (double)(t_next.v - tnow.v) / 1e9,
           ddsrt_avl_is_empty (&wr->readers) ? (seqno_t) -1 : ((struct wr_prd_match *) ddsrt_avl_root_non_empty (&wr_readers_treedef, &wr->readers))->min_seq,
           ddsrt_avl_is_empty (&wr->readers) || ((struct wr_prd_match *) ddsrt_avl_root_non_empty (&wr_readers_treedef, &wr->readers))->all_have_replied_to_hb ? "" : "!",
           whcst.max_seq, writer_read_seq_xmit (wr));
  (void) resched_xevent_if_earlier (ev, t_next);
  wr->hbcontrol.tsched = t_next;
  ddsrt_mutex_unlock (&wr->e.lock);

  /* Can't transmit synchronously with writer lock held: trying to add
     the heartbeat to the xp may cause xp to be sent out, which may
     require updating wr->seq_xmit for other messages already in xp.
     Besides, nn_xpack_addmsg may sleep for bandwidth-limited channels
     and we certainly don't want to hold the lock during that time. */
  if (msg)
  {
    if (!wr->test_suppress_heartbeat)
      nn_xpack_addmsg (xp, msg, 0);
    else
    {
      GVTRACE ("test_suppress_heartbeat\n");
      nn_xmsg_free (msg);
    }
  }
}

static dds_duration_t preemptive_acknack_interval (const struct pwr_rd_match *rwn)
{
  if (rwn->t_last_ack.v < rwn->tcreate.v)
    return 0;
  else
  {
    const dds_duration_t age = rwn->t_last_ack.v - rwn->tcreate.v;
    if (age <= DDS_SECS (10))
      return DDS_SECS (1);
    else if (age <= DDS_SECS (60))
      return DDS_SECS (2);
    else if (age <= DDS_SECS (120))
      return DDS_SECS (5);
    else
      return DDS_SECS (10);
  }
}

static struct nn_xmsg *make_preemptive_acknack (struct xevent *ev, struct proxy_writer *pwr, struct pwr_rd_match *rwn, ddsrt_mtime_t tnow)
{
  const dds_duration_t intv = preemptive_acknack_interval (rwn);
  if (tnow.v < ddsrt_mtime_add_duration (rwn->t_last_ack, intv).v)
  {
    (void) resched_xevent_if_earlier (ev, ddsrt_mtime_add_duration (rwn->t_last_ack, intv));
    return NULL;
  }

  struct ddsi_domaingv * const gv = pwr->e.gv;
  struct participant *pp = NULL;
  if (q_omg_proxy_participant_is_secure (pwr->c.proxypp))
  {
    struct reader *rd = entidx_lookup_reader_guid (gv->entity_index, &rwn->rd_guid);
    if (rd)
      pp = rd->c.pp;
  }

  struct nn_xmsg *msg;
  if ((msg = nn_xmsg_new (gv->xmsgpool, &rwn->rd_guid, pp, ACKNACK_SIZE_MAX, NN_XMSG_KIND_CONTROL)) == NULL)
  {
    // if out of memory, try again later
    (void) resched_xevent_if_earlier (ev, ddsrt_mtime_add_duration (tnow, DDS_SECS (1)));
    return NULL;
  }

  nn_xmsg_setdstPWR (msg, pwr);
  struct nn_xmsg_marker sm_marker;
  AckNack_t *an = nn_xmsg_append (msg, &sm_marker, ACKNACK_SIZE (0));
  nn_xmsg_submsg_init (msg, sm_marker, SMID_ACKNACK);
  an->readerId = nn_hton_entityid (rwn->rd_guid.entityid);
  an->writerId = nn_hton_entityid (pwr->e.guid.entityid);
  an->readerSNState.bitmap_base = toSN (1);
  an->readerSNState.numbits = 0;
  nn_count_t * const countp =
    (nn_count_t *) ((char *) an + offsetof (AckNack_t, bits) + NN_SEQUENCE_NUMBER_SET_BITS_SIZE (0));
  *countp = 0;
  nn_xmsg_submsg_setnext (msg, sm_marker);
  encode_datareader_submsg (msg, sm_marker, pwr, &rwn->rd_guid);

  rwn->t_last_ack = tnow;
  (void) resched_xevent_if_earlier (ev, ddsrt_mtime_add_duration (rwn->t_last_ack, intv));
  return msg;
}

static void handle_xevk_acknack (struct nn_xpack *xp, struct xevent *ev, ddsrt_mtime_t tnow)
{
  /* FIXME: ought to keep track of which NACKs are being generated in
     response to a Heartbeat.  There is no point in having multiple
     readers NACK the data.

     FIXME: ought to determine the set of missing samples (as it does
     now), and then check which for of those fragments are available already.
     A little snag is that the defragmenter can throw out partial samples in
     favour of others, so MUST ensure that the defragmenter won't start
     threshing and fail to make progress! */
  struct ddsi_domaingv *gv = ev->evq->gv;
  struct proxy_writer *pwr;
  struct nn_xmsg *msg;
  struct pwr_rd_match *rwn;

  if ((pwr = entidx_lookup_proxy_writer_guid (gv->entity_index, &ev->u.acknack.pwr_guid)) == NULL)
  {
    return;
  }

  ddsrt_mutex_lock (&pwr->e.lock);
  if ((rwn = ddsrt_avl_lookup (&pwr_readers_treedef, &pwr->readers, &ev->u.acknack.rd_guid)) == NULL)
  {
    ddsrt_mutex_unlock (&pwr->e.lock);
    return;
  }

  if (!pwr->have_seen_heartbeat)
    msg = make_preemptive_acknack (ev, pwr, rwn, tnow);
  else if (!(rwn->heartbeat_since_ack || rwn->heartbeatfrag_since_ack))
    msg = NULL;
  else
    msg = make_and_resched_acknack (ev, pwr, rwn, tnow, false);
  ddsrt_mutex_unlock (&pwr->e.lock);

  /* nn_xpack_addmsg may sleep (for bandwidth-limited channels), so
     must be outside the lock */
  if (msg)
  {
    // a possible result of trying to encode a submessage is that it is removed,
    // in which case we may end up with an empty one.
    // FIXME: change encode_datareader_submsg so that it returns this and make it warn_unused_result
    if (nn_xmsg_size (msg) == 0)
      nn_xmsg_free (msg);
    else
      nn_xpack_addmsg (xp, msg, 0);
  }
}

static bool resend_spdp_sample_by_guid_key (struct writer *wr, const ddsi_guid_t *guid, struct proxy_reader *prd)
{
  /* Look up data in (transient-local) WHC by key value -- FIXME: clearly
   a slightly more efficient and elegant way of looking up the key value
   is to be preferred */
  struct ddsi_domaingv *gv = wr->e.gv;
  bool sample_found;
  ddsi_plist_t ps;
  ddsi_plist_init_empty (&ps);
  ps.present |= PP_PARTICIPANT_GUID;
  ps.participant_guid = *guid;
  struct ddsi_serdata *sd = ddsi_serdata_from_sample (gv->spdp_type, SDK_KEY, &ps);
  ddsi_plist_fini (&ps);
  struct whc_borrowed_sample sample;

  ddsrt_mutex_lock (&wr->e.lock);
  sample_found = whc_borrow_sample_key (wr->whc, sd, &sample);
  if (sample_found)
  {
    /* Claiming it is new rather than a retransmit so that the rexmit
       limiting won't kick in.  It is best-effort and therefore the
       updating of the last transmitted sequence number won't take
       place anyway.  Nor is it necessary to fiddle with heartbeat
       control stuff. */
    enqueue_spdp_sample_wrlock_held (wr, sample.seq, sample.serdata, prd);
    whc_return_sample(wr->whc, &sample, false);
  }
  ddsrt_mutex_unlock (&wr->e.lock);
  ddsi_serdata_unref (sd);
  return sample_found;
}

static void handle_xevk_spdp (UNUSED_ARG (struct nn_xpack *xp), struct xevent *ev, ddsrt_mtime_t tnow)
{
  /* Like the writer pointer in the heartbeat event, the participant pointer in the spdp event is assumed valid. */
  struct ddsi_domaingv *gv = ev->evq->gv;
  struct participant *pp;
  struct proxy_reader *prd;
  struct writer *spdp_wr;
  bool do_write;

  if ((pp = entidx_lookup_participant_guid (gv->entity_index, &ev->u.spdp.pp_guid)) == NULL)
  {
    GVTRACE ("handle_xevk_spdp "PGUIDFMT" - unknown guid\n", PGUID (ev->u.spdp.pp_guid));
    if (ev->u.spdp.directed)
      delete_xevent (ev);
    return;
  }

  if ((spdp_wr = get_builtin_writer (pp, NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER)) == NULL)
  {
    GVTRACE ("handle_xevk_spdp "PGUIDFMT" - spdp writer of participant not found\n", PGUID (ev->u.spdp.pp_guid));
    if (ev->u.spdp.directed)
      delete_xevent (ev);
    return;
  }

  if (!ev->u.spdp.directed)
  {
    /* memset is for tracing output */
    memset (&ev->u.spdp.dest_proxypp_guid_prefix, 0, sizeof (ev->u.spdp.dest_proxypp_guid_prefix));
    prd = NULL;
    do_write = true;
  }
  else
  {
    ddsi_guid_t guid;
    guid.prefix = ev->u.spdp.dest_proxypp_guid_prefix;
    guid.entityid.u = NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER;
    prd = entidx_lookup_proxy_reader_guid (gv->entity_index, &guid);
    do_write = (prd != NULL);
    if (!do_write)
      GVTRACE ("xmit spdp: no proxy reader "PGUIDFMT"\n", PGUID (guid));
  }

  if (do_write && !resend_spdp_sample_by_guid_key (spdp_wr, &ev->u.spdp.pp_guid, prd))
  {
#ifndef NDEBUG
    /* If undirected, it is pp->spdp_xevent, and that one must never
       run into an empty WHC unless it is already marked for deletion.

       If directed, it may happen in response to an SPDP packet during
       creation of the participant.  This is because pp is inserted in
       the hash table quite early on, which, in turn, is because it
       needs to be visible for creating its builtin endpoints.  But in
       this case, the initial broadcast of the SPDP packet of pp will
       happen shortly. */
    if (!ev->u.spdp.directed)
    {
      ddsrt_mutex_lock (&pp->e.lock);
      ddsrt_mutex_lock (&ev->evq->lock);
      assert (ev->tsched.v == TSCHED_DELETE);
      ddsrt_mutex_unlock (&ev->evq->lock);
      ddsrt_mutex_unlock (&pp->e.lock);
    }
    else
    {
      GVTRACE ("xmit spdp: suppressing early spdp response from "PGUIDFMT" to %"PRIx32":%"PRIx32":%"PRIx32":%x\n",
               PGUID (pp->e.guid), PGUIDPREFIX (ev->u.spdp.dest_proxypp_guid_prefix), NN_ENTITYID_PARTICIPANT);
    }
#endif
  }

  if (ev->u.spdp.directed)
  {
    /* Directed events are used to send SPDP packets to newly
       discovered peers, and used just once. */
    if (--ev->u.spdp.directed == 0 || gv->config.spdp_interval < DDS_SECS (1) || pp->lease_duration < DDS_SECS (1))
      delete_xevent (ev);
    else
    {
      ddsrt_mtime_t tnext = ddsrt_mtime_add_duration (tnow, DDS_SECS (1));
      GVTRACE ("xmit spdp "PGUIDFMT" to %"PRIx32":%"PRIx32":%"PRIx32":%x (resched %gs)\n",
               PGUID (pp->e.guid),
               PGUIDPREFIX (ev->u.spdp.dest_proxypp_guid_prefix), NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER,
               (double)(tnext.v - tnow.v) / 1e9);
      (void) resched_xevent_if_earlier (ev, tnext);
    }
  }
  else
  {
    /* schedule next when 80% of the interval has elapsed, or 2s
       before the lease ends, whichever comes first (similar to PMD),
       but never wait longer than spdp_interval */
    const dds_duration_t mindelta = DDS_MSECS (10);
    const dds_duration_t ldur = pp->lease_duration;
    ddsrt_mtime_t tnext;
    int64_t intv;

    if (ldur < 5 * mindelta / 4)
      intv = mindelta;
    else if (ldur < DDS_SECS (10))
      intv = 4 * ldur / 5;
    else
      intv = ldur - DDS_SECS (2);
    if (intv > gv->config.spdp_interval)
      intv = gv->config.spdp_interval;

    tnext = ddsrt_mtime_add_duration (tnow, intv);
    GVTRACE ("xmit spdp "PGUIDFMT" to %"PRIx32":%"PRIx32":%"PRIx32":%x (resched %gs)\n",
             PGUID (pp->e.guid),
             PGUIDPREFIX (ev->u.spdp.dest_proxypp_guid_prefix), NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER,
             (double)(tnext.v - tnow.v) / 1e9);
    (void) resched_xevent_if_earlier (ev, tnext);
  }
}

static void handle_xevk_pmd_update (struct thread_state1 * const ts1, struct nn_xpack *xp, struct xevent *ev, ddsrt_mtime_t tnow)
{
  struct ddsi_domaingv * const gv = ev->evq->gv;
  struct participant *pp;
  dds_duration_t intv;
  ddsrt_mtime_t tnext;

  if ((pp = entidx_lookup_participant_guid (gv->entity_index, &ev->u.pmd_update.pp_guid)) == NULL)
  {
    return;
  }

  write_pmd_message (ts1, xp, pp, PARTICIPANT_MESSAGE_DATA_KIND_AUTOMATIC_LIVELINESS_UPDATE);

  intv = pp_get_pmd_interval (pp);
  if (intv == DDS_INFINITY)
  {
    tnext.v = DDS_NEVER;
    GVTRACE ("resched pmd("PGUIDFMT"): never\n", PGUID (pp->e.guid));
  }
  else
  {
    /* schedule next when 80% of the interval has elapsed, or 2s
       before the lease ends, whichever comes first */
    if (intv >= DDS_SECS (10))
      tnext.v = tnow.v + intv - DDS_SECS (2);
    else
      tnext.v = tnow.v + 4 * intv / 5;
    GVTRACE ("resched pmd("PGUIDFMT"): %gs\n", PGUID (pp->e.guid), (double)(tnext.v - tnow.v) / 1e9);
  }

  (void) resched_xevent_if_earlier (ev, tnext);
}

static void handle_xevk_delete_writer (UNUSED_ARG (struct nn_xpack *xp), struct xevent *ev, UNUSED_ARG (ddsrt_mtime_t tnow))
{
  /* don't worry if the writer is already gone by the time we get here, delete_writer_nolinger checks for that. */
  struct ddsi_domaingv * const gv = ev->evq->gv;
  GVTRACE ("handle_xevk_delete_writer: "PGUIDFMT"\n", PGUID (ev->u.delete_writer.guid));
  delete_writer_nolinger (gv, &ev->u.delete_writer.guid);
  delete_xevent (ev);
}

static void handle_individual_xevent (struct thread_state1 * const ts1, struct xevent *xev, struct nn_xpack *xp, ddsrt_mtime_t tnow)
{
  struct xeventq *xevq = xev->evq;
  /* We relinquish the lock while processing the event, but require it
     held for administrative work. */
  ASSERT_MUTEX_HELD (&xevq->lock);
  if (xev->kind == XEVK_CALLBACK)
  {
    xev->u.callback.executing = true;
    ddsrt_mutex_unlock (&xevq->lock);
    xev->u.callback.cb (xev, xev->u.callback.arg, tnow);
    ddsrt_mutex_lock (&xevq->lock);
    xev->u.callback.executing = false;
    ddsrt_cond_broadcast (&xevq->cond);
  }
  else
  {
    ddsrt_mutex_unlock (&xevq->lock);
    switch (xev->kind)
    {
      case XEVK_HEARTBEAT:
        handle_xevk_heartbeat (xp, xev, tnow);
        break;
      case XEVK_ACKNACK:
        handle_xevk_acknack (xp, xev, tnow);
        break;
      case XEVK_SPDP:
        handle_xevk_spdp (xp, xev, tnow);
        break;
      case XEVK_PMD_UPDATE:
        handle_xevk_pmd_update (ts1, xp, xev, tnow);
        break;
      case XEVK_DELETE_WRITER:
        handle_xevk_delete_writer (xp, xev, tnow);
        break;
      case XEVK_CALLBACK:
        assert (0);
        break;
    }
    ddsrt_mutex_lock (&xevq->lock);
  }
  ASSERT_MUTEX_HELD (&xevq->lock);
}

static void handle_individual_xevent_nt (struct xevent_nt *xev, struct nn_xpack *xp)
{
  switch (xev->kind)
  {
    case XEVK_MSG:
      handle_xevk_msg (xp, xev);
      break;
    case XEVK_MSG_REXMIT:
    case XEVK_MSG_REXMIT_NOMERGE:
      handle_xevk_msg_rexmit (xp, xev);
      break;
    case XEVK_ENTITYID:
      handle_xevk_entityid (xp, xev);
      break;
    case XEVK_NT_CALLBACK:
      xev->u.callback.cb (xev->u.callback.arg);
      break;
  }
  ddsrt_free (xev);
}

static void handle_timed_xevent (struct thread_state1 * const ts1, struct xevent *xev, struct nn_xpack *xp, ddsrt_mtime_t tnow /* monotonic */)
{
   /* This function handles the individual xevent irrespective of
      whether it is a "timed" or "non-timed" xevent */
  assert (xev->tsched.v != TSCHED_DELETE);
  handle_individual_xevent (ts1, xev, xp, tnow /* monotonic */);
}

static void handle_nontimed_xevent (struct xevent_nt *xev, struct nn_xpack *xp)
{
   /* This function handles the individual xevent irrespective of
      whether it is a "timed" or "non-timed" xevent */
  struct xeventq *xevq = xev->evq;

  /* We relinquish the lock while processing the event, but require it
     held for administrative work. */
  ASSERT_MUTEX_HELD (&xevq->lock);

  assert (xev->evq == xevq);

  ddsrt_mutex_unlock (&xevq->lock);
  handle_individual_xevent_nt (xev, xp);
  /* non-timed xevents are freed by the handlers */
  ddsrt_mutex_lock (&xevq->lock);

  ASSERT_MUTEX_HELD (&xevq->lock);
}

static void handle_xevents (struct thread_state1 * const ts1, struct xeventq *xevq, struct nn_xpack *xp, ddsrt_mtime_t tnow /* monotonic */)
{
  int xeventsToProcess = 1;

  ASSERT_MUTEX_HELD (&xevq->lock);
  assert (thread_is_awake ());

  /* The following loops give priority to the "timed" events (heartbeats,
     acknacks etc) if there are any.  The algorithm is that we handle all
     "timed" events that are scheduled now and then handle one "non-timed"
     event.  If there weren't any "non-timed" events then the loop
     terminates.  If there was one, then after handling it, re-read the
     clock and continue the loop, i.e. test again to see whether any
     "timed" events are now due. */

  while (xeventsToProcess)
  {
    while (earliest_in_xeventq(xevq).v <= tnow.v)
    {
      struct xevent *xev = ddsrt_fibheap_extract_min (&evq_xevents_fhdef, &xevq->xevents);
      if (xev->tsched.v == TSCHED_DELETE)
      {
        free_xevent (xevq, xev);
      }
      else
      {
        /* event rescheduling functions look at xev->tsched to
           determine whether it is currently on the heap or not (i.e.,
           scheduled or not), so set to TSCHED_NEVER to indicate it
           currently isn't. */
        xev->tsched.v = DDS_NEVER;
        thread_state_awake_to_awake_no_nest (ts1);
        handle_timed_xevent (ts1, xev, xp, tnow);
      }

      /* Limited-bandwidth channels means events can take a LONG time
         to process.  So read the clock more often. */
      tnow = ddsrt_time_monotonic ();
    }

    if (!non_timed_xmit_list_is_empty (xevq))
    {
      struct xevent_nt *xev = getnext_from_non_timed_xmit_list (xevq);
      thread_state_awake_to_awake_no_nest (ts1);
      handle_nontimed_xevent (xev, xp);
      tnow = ddsrt_time_monotonic ();
    }
    else
    {
      xeventsToProcess = 0;
    }
  }

  ASSERT_MUTEX_HELD (&xevq->lock);
}

static uint32_t xevent_thread (struct xeventq * xevq)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  struct nn_xpack *xp;
  ddsrt_mtime_t next_thread_cputime = { 0 };

  xp = nn_xpack_new (xevq->tev_conn, xevq->auxiliary_bandwidth_limit, xevq->gv->config.xpack_send_async);

  ddsrt_mutex_lock (&xevq->lock);
  while (!xevq->terminate)
  {
    ddsrt_mtime_t tnow = ddsrt_time_monotonic ();

    LOG_THREAD_CPUTIME (&xevq->gv->logconfig, next_thread_cputime);

    thread_state_awake_fixed_domain (ts1);
    handle_xevents (ts1, xevq, xp, tnow);
    /* Send to the network unlocked, as it may sleep due to bandwidth limitation */
    ddsrt_mutex_unlock (&xevq->lock);
    nn_xpack_send (xp, false);
    ddsrt_mutex_lock (&xevq->lock);
    thread_state_asleep (ts1);

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
        /* Although we assumed instantaneous handling of events, we
           don't want to sleep much longer than we have to. With
           os_condTimedWait requiring a relative time, we don't have
           much choice but to read the clock now */
        tnow = ddsrt_time_monotonic ();
        if (twakeup.v > tnow.v)
        {
          twakeup.v -= tnow.v; /* ddsrt_cond_waitfor: relative timeout */
          ddsrt_cond_waitfor (&xevq->cond, &xevq->lock, twakeup.v);
        }
      }
    }
  }
  ddsrt_mutex_unlock (&xevq->lock);
  nn_xpack_send (xp, false);
  nn_xpack_free (xp);
  return 0;
}

void qxev_msg (struct xeventq *evq, struct nn_xmsg *msg)
{
  struct xevent_nt *ev;
  assert (evq);
  assert (nn_xmsg_kind (msg) != NN_XMSG_KIND_DATA_REXMIT);
  ddsrt_mutex_lock (&evq->lock);
  ev = qxev_common_nt (evq, XEVK_MSG);
  ev->u.msg.msg = msg;
  qxev_insert_nt (ev);
  ddsrt_mutex_unlock (&evq->lock);
}

void qxev_nt_callback (struct xeventq *evq, void (*cb) (void *arg), void *arg)
{
  struct xevent_nt *ev;
  assert (evq);
  ddsrt_mutex_lock (&evq->lock);
  ev = qxev_common_nt (evq, XEVK_NT_CALLBACK);
  ev->u.callback.cb = cb;
  ev->u.callback.arg = arg;
  qxev_insert_nt (ev);
  ddsrt_mutex_unlock (&evq->lock);
}

void qxev_prd_entityid (struct proxy_reader *prd, const ddsi_guid_t *guid)
{
  struct ddsi_domaingv * const gv = prd->e.gv;
  struct nn_xmsg *msg;
  struct xevent_nt *ev;

  /* For connected transports, may need to establish and identify connection */

  if (! gv->xevents->tev_conn->m_connless)
  {
    msg = nn_xmsg_new (gv->xmsgpool, guid, NULL, sizeof (EntityId_t), NN_XMSG_KIND_CONTROL);
    nn_xmsg_setdstPRD (msg, prd);
    GVTRACE ("  qxev_prd_entityid (%"PRIx32":%"PRIx32":%"PRIx32")\n", PGUIDPREFIX (guid->prefix));
    nn_xmsg_add_entityid (msg);
    ddsrt_mutex_lock (&gv->xevents->lock);
    ev = qxev_common_nt (gv->xevents, XEVK_ENTITYID);
    ev->u.entityid.msg = msg;
    qxev_insert_nt (ev);
    ddsrt_mutex_unlock (&gv->xevents->lock);
  }
}

void qxev_pwr_entityid (struct proxy_writer *pwr, const ddsi_guid_t *guid)
{
  struct ddsi_domaingv * const gv = pwr->e.gv;
  struct nn_xmsg *msg;
  struct xevent_nt *ev;

  /* For connected transports, may need to establish and identify connection */

  if (! pwr->evq->tev_conn->m_connless)
  {
    msg = nn_xmsg_new (gv->xmsgpool, guid, NULL, sizeof (EntityId_t), NN_XMSG_KIND_CONTROL);
    nn_xmsg_setdstPWR (msg, pwr);
    GVTRACE ("  qxev_pwr_entityid (%"PRIx32":%"PRIx32":%"PRIx32")\n", PGUIDPREFIX (guid->prefix));
    nn_xmsg_add_entityid (msg);
    ddsrt_mutex_lock (&pwr->evq->lock);
    ev = qxev_common_nt (pwr->evq, XEVK_ENTITYID);
    ev->u.entityid.msg = msg;
    qxev_insert_nt (ev);
    ddsrt_mutex_unlock (&pwr->evq->lock);
  }
}

int qxev_msg_rexmit_wrlock_held (struct xeventq *evq, struct nn_xmsg *msg, int force)
{
  struct ddsi_domaingv * const gv = evq->gv;
  size_t msg_size = nn_xmsg_size (msg);
  struct xevent_nt *ev;

  assert (evq);
  assert (nn_xmsg_kind (msg) == NN_XMSG_KIND_DATA_REXMIT || nn_xmsg_kind (msg) == NN_XMSG_KIND_DATA_REXMIT_NOMERGE);
  ddsrt_mutex_lock (&evq->lock);
  if ((ev = lookup_msg (evq, msg)) != NULL && nn_xmsg_merge_rexmit_destinations_wrlock_held (gv, ev->u.msg_rexmit.msg, msg))
  {
    /* MSG got merged with a pending retransmit, so it has effectively been queued */
    ddsrt_mutex_unlock (&evq->lock);
    nn_xmsg_free (msg);
    return 1;
  }
  else if ((evq->queued_rexmit_bytes > evq->max_queued_rexmit_bytes ||
            evq->queued_rexmit_msgs == evq->max_queued_rexmit_msgs) &&
           !force)
  {
    /* drop it if insufficient resources available */
    ddsrt_mutex_unlock (&evq->lock);
    nn_xmsg_free (msg);
#if 0
    GVTRACE (" qxev_msg_rexmit%s drop (sz %"PA_PRIuSIZE" qb %"PA_PRIuSIZE" qm %"PA_PRIuSIZE")", force ? "!" : "",
             msg_size, evq->queued_rexmit_bytes, evq->queued_rexmit_msgs);
#endif
    return 0;
  }
  else
  {
    const enum xeventkind_nt kind =
      (nn_xmsg_kind (msg) == NN_XMSG_KIND_DATA_REXMIT) ? XEVK_MSG_REXMIT : XEVK_MSG_REXMIT_NOMERGE;
    ev = qxev_common_nt (evq, kind);
    ev->u.msg_rexmit.msg = msg;
    ev->u.msg_rexmit.queued_rexmit_bytes = msg_size;
    evq->queued_rexmit_bytes += msg_size;
    evq->queued_rexmit_msgs++;
    qxev_insert_nt (ev);
#if 0
    GVTRACE ("AAA(%p,%"PA_PRIuSIZE")", (void *) ev, msg_size);
#endif
    ddsrt_mutex_unlock (&evq->lock);
    return 2;
  }
}

struct xevent *qxev_heartbeat (struct xeventq *evq, ddsrt_mtime_t tsched, const ddsi_guid_t *wr_guid)
{
  /* Event _must_ be deleted before enough of the writer is freed to
     cause trouble.  Currently used exclusively for
     wr->heartbeat_xevent.  */
  struct xevent *ev;
  assert(evq);
  ddsrt_mutex_lock (&evq->lock);
  ev = qxev_common (evq, tsched, XEVK_HEARTBEAT);
  ev->u.heartbeat.wr_guid = *wr_guid;
  qxev_insert (ev);
  ddsrt_mutex_unlock (&evq->lock);
  return ev;
}

struct xevent *qxev_acknack (struct xeventq *evq, ddsrt_mtime_t tsched, const ddsi_guid_t *pwr_guid, const ddsi_guid_t *rd_guid)
{
  struct xevent *ev;
  assert(evq);
  ddsrt_mutex_lock (&evq->lock);
  ev = qxev_common (evq, tsched, XEVK_ACKNACK);
  ev->u.acknack.pwr_guid = *pwr_guid;
  ev->u.acknack.rd_guid = *rd_guid;
  qxev_insert (ev);
  ddsrt_mutex_unlock (&evq->lock);
  return ev;
}

struct xevent *qxev_spdp (struct xeventq *evq, ddsrt_mtime_t tsched, const ddsi_guid_t *pp_guid, const ddsi_guid_t *dest_proxypp_guid)
{
  struct xevent *ev;
  ddsrt_mutex_lock (&evq->lock);
  ev = qxev_common (evq, tsched, XEVK_SPDP);
  ev->u.spdp.pp_guid = *pp_guid;
  if (dest_proxypp_guid == NULL)
    ev->u.spdp.directed = 0;
  else
  {
    ev->u.spdp.dest_proxypp_guid_prefix = dest_proxypp_guid->prefix;
    ev->u.spdp.directed = 4;
  }
  qxev_insert (ev);
  ddsrt_mutex_unlock (&evq->lock);
  return ev;
}

struct xevent *qxev_pmd_update (struct xeventq *evq, ddsrt_mtime_t tsched, const ddsi_guid_t *pp_guid)
{
  struct xevent *ev;
  ddsrt_mutex_lock (&evq->lock);
  ev = qxev_common (evq, tsched, XEVK_PMD_UPDATE);
  ev->u.pmd_update.pp_guid = *pp_guid;
  qxev_insert (ev);
  ddsrt_mutex_unlock (&evq->lock);
  return ev;
}

struct xevent *qxev_delete_writer (struct xeventq *evq, ddsrt_mtime_t tsched, const ddsi_guid_t *guid)
{
  struct xevent *ev;
  ddsrt_mutex_lock (&evq->lock);
  ev = qxev_common (evq, tsched, XEVK_DELETE_WRITER);
  ev->u.delete_writer.guid = *guid;
  qxev_insert (ev);
  ddsrt_mutex_unlock (&evq->lock);
  return ev;
}

struct xevent *qxev_callback (struct xeventq *evq, ddsrt_mtime_t tsched, void (*cb) (struct xevent *ev, void *arg, ddsrt_mtime_t tnow), void *arg)
{
  struct xevent *ev;
  ddsrt_mutex_lock (&evq->lock);
  ev = qxev_common (evq, tsched, XEVK_CALLBACK);
  ev->u.callback.cb = cb;
  ev->u.callback.arg = arg;
  ev->u.callback.executing = false;
  qxev_insert (ev);
  ddsrt_mutex_unlock (&evq->lock);
  return ev;
}
