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

#include "dds/ddsi/q_time.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/q_addrset.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsi/q_xevent.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_globals.h"
#include "dds/ddsi/q_ephash.h"
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
  nn_mtime_t tsched;
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
      void (*cb) (struct xevent *ev, void *arg, nn_mtime_t tnow);
      void *arg;
    } callback;
  } u;
};

enum xeventkind_nt
{
  XEVK_MSG,
  XEVK_MSG_REXMIT,
  XEVK_ENTITYID
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
    } msg_rexmit;
    struct {
      /* xmsg is self-contained / relies on reference counts */
      struct nn_xmsg *msg;
    } entityid;
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
  struct q_globals *gv;
  ddsrt_mutex_t lock;
  ddsrt_cond_t cond;
  ddsi_tran_conn_t tev_conn;
  uint32_t auxiliary_bandwidth_limit;
};

static uint32_t xevent_thread (struct xeventq *xevq);
static nn_mtime_t earliest_in_xeventq (struct xeventq *evq);
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
  assert (ev->kind == XEVK_MSG_REXMIT);
  assert (ev->u.msg_rexmit.queued_rexmit_bytes <= evq->queued_rexmit_bytes);
  assert (evq->queued_rexmit_msgs > 0);
  evq->queued_rexmit_bytes -= ev->u.msg_rexmit.queued_rexmit_bytes;
  evq->queued_rexmit_msgs--;
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
  assert (nn_xmsg_kind (msg) == NN_XMSG_KIND_DATA_REXMIT);
  trace_msg (evq, "lookup-msg", msg);
  return ddsrt_avl_lookup (&msg_xevents_treedef, &evq->msg_xevents, msg);
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

  ddsrt_cond_signal (&evq->cond);
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
  /* Can delete it only once, no matter how we implement it internally */
  assert (ev->tsched.v != TSCHED_DELETE);
  assert (TSCHED_DELETE < ev->tsched.v);
  if (ev->tsched.v != T_NEVER)
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
  ddsrt_cond_signal (&evq->cond);
  ddsrt_mutex_unlock (&evq->lock);
}

int resched_xevent_if_earlier (struct xevent *ev, nn_mtime_t tsched)
{
  struct xeventq *evq = ev->evq;
  int is_resched;
  ddsrt_mutex_lock (&evq->lock);
  assert (tsched.v != TSCHED_DELETE);
  /* If you want to delete it, you to say so by calling the right
     function. Don't want to reschedule an event marked for deletion,
     but with TSCHED_DELETE = MIN_INT64, tsched >= ev->tsched is
     guaranteed to be false. */
  assert (tsched.v > TSCHED_DELETE);
  if (tsched.v >= ev->tsched.v)
    is_resched = 0;
  else
  {
    nn_mtime_t tbefore = earliest_in_xeventq (evq);
    assert (tsched.v != T_NEVER);
    if (ev->tsched.v != T_NEVER)
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
      ddsrt_cond_signal (&evq->cond);
  }
  ddsrt_mutex_unlock (&evq->lock);
  return is_resched;
}

static struct xevent *qxev_common (struct xeventq *evq, nn_mtime_t tsched, enum xeventkind kind)
{
  /* qxev_common is the route by which all timed xevents are
     created. */
  struct xevent *ev = ddsrt_malloc (sizeof (*ev));

  assert (tsched.v != TSCHED_DELETE);
  ASSERT_MUTEX_HELD (&evq->lock);

  /* round up the scheduled time if required */
  if (tsched.v != T_NEVER && evq->gv->config.schedule_time_rounding != 0)
  {
    nn_mtime_t tsched_rounded = mtime_round_up (tsched, evq->gv->config.schedule_time_rounding);
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

static nn_mtime_t earliest_in_xeventq (struct xeventq *evq)
{
  struct xevent *min;
  ASSERT_MUTEX_HELD (&evq->lock);
  if ((min = ddsrt_fibheap_min (&evq_xevents_fhdef, &evq->xevents)) != NULL)
    return min->tsched;
  else
  {
    nn_mtime_t r = { T_NEVER };
    return r;
  }
}

static void qxev_insert (struct xevent *ev)
{
  /* qxev_insert is how all timed xevents are registered into the
     event administration. */
  struct xeventq *evq = ev->evq;
  ASSERT_MUTEX_HELD (&evq->lock);
  if (ev->tsched.v != T_NEVER)
  {
    nn_mtime_t tbefore = earliest_in_xeventq (evq);
    ddsrt_fibheap_insert (&evq_xevents_fhdef, &evq->xevents, ev);
    if (ev->tsched.v < tbefore.v)
      ddsrt_cond_signal (&evq->cond);
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
  ddsrt_cond_signal (&evq->cond);
  ddsrt_mutex_unlock (&evq->lock);
  join_thread (evq->ts);
  evq->ts = NULL;
}

void xeventq_free (struct xeventq *evq)
{
  struct xevent *ev;
  assert (evq->ts == NULL);
  while ((ev = ddsrt_fibheap_extract_min (&evq_xevents_fhdef, &evq->xevents)) != NULL)
  {
    if (ev->tsched.v == TSCHED_DELETE || ev->kind != XEVK_CALLBACK)
      free_xevent (evq, ev);
    else
    {
      ev->tsched.v = T_NEVER;
      ev->u.callback.cb (ev, ev->u.callback.arg, ev->tsched);
      if (ev->tsched.v != TSCHED_DELETE)
      {
        union { void *v; void (*f) (struct xevent *ev, void *arg, nn_mtime_t tnow); } fp;
        fp.f = ev->u.callback.cb;
        DDS_CWARNING (&evq->gv->logconfig, "xeventq_free: callback %p did not schedule deletion as required, deleting event anyway\n", fp.v);
        delete_xevent (ev);
      }
    }
  }

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

#ifdef DDSI_INCLUDE_SECURITY
static void send_heartbeat_to_all_readers(struct nn_xpack *xp, struct xevent *ev, struct writer *wr, nn_mtime_t tnow)
{
  struct whc_state whcst;
  nn_mtime_t t_next;
  int hbansreq = 0;
  unsigned count = 0;

  ddsrt_mutex_lock (&wr->e.lock);

  whc_get_state(wr->whc, &whcst);

  if (!writer_must_have_hb_scheduled (wr, &whcst))
  {
    hbansreq = 1; /* just for trace */
    t_next.v = T_NEVER;
  }
  else if (!writer_hbcontrol_must_send (wr, &whcst, tnow))
  {
    hbansreq = 1; /* just for trace */
    t_next.v = tnow.v + writer_hbcontrol_intv (wr, &whcst, tnow);
  }
  else
  {
    struct wr_prd_match *m;
    struct ddsi_guid last_guid = { .prefix = {.u = {0,0,0}}, .entityid = {0} };

    hbansreq = writer_hbcontrol_ack_required (wr, &whcst, tnow);
    t_next.v = tnow.v + writer_hbcontrol_intv (wr, &whcst, tnow);

    while ((m = ddsrt_avl_lookup_succ (&wr_readers_treedef, &wr->readers, &last_guid)) != NULL)
    {
      last_guid = m->prd_guid;
      if (m->seq < m->last_seq)
      {
        struct proxy_reader *prd;

        prd = ephash_lookup_proxy_reader_guid(wr->e.gv->guid_hash, &m->prd_guid);
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

  resched_xevent_if_earlier (ev, t_next);
  wr->hbcontrol.tsched = t_next;

  if (count == 0)
  {
    (void)resched_xevent_if_earlier (ev, t_next);
    ETRACE (wr, "heartbeat(wr "PGUIDFMT") suppressed, resched in %g s (min-ack %"PRId64"%s, avail-seq %"PRId64", xmit %"PRId64")\n",
        PGUID (wr->e.guid),
        (t_next.v == T_NEVER) ? INFINITY : (double)(t_next.v - tnow.v) / 1e9,
        ddsrt_avl_is_empty (&wr->readers) ? (int64_t) -1 : ((struct wr_prd_match *) ddsrt_avl_root (&wr_readers_treedef, &wr->readers))->min_seq,
        ddsrt_avl_is_empty (&wr->readers) || ((struct wr_prd_match *) ddsrt_avl_root (&wr_readers_treedef, &wr->readers))->all_have_replied_to_hb ? "" : "!",
        whcst.max_seq,
        writer_read_seq_xmit(wr));
  }

  ddsrt_mutex_unlock (&wr->e.lock);
}


#endif

static void handle_xevk_heartbeat (struct nn_xpack *xp, struct xevent *ev, nn_mtime_t tnow /* monotonic */)
{
  struct q_globals const * const gv = ev->evq->gv;
  struct nn_xmsg *msg;
  struct writer *wr;
  nn_mtime_t t_next;
  int hbansreq = 0;
  struct whc_state whcst;

  if ((wr = ephash_lookup_writer_guid (gv->guid_hash, &ev->u.heartbeat.wr_guid)) == NULL)
  {
    GVTRACE("heartbeat(wr "PGUIDFMT") writer gone\n", PGUID (ev->u.heartbeat.wr_guid));
    return;
  }

#ifdef DDSI_INCLUDE_SECURITY
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
    t_next.v = T_NEVER;
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
           (t_next.v == T_NEVER) ? INFINITY : (double)(t_next.v - tnow.v) / 1e9,
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
    nn_xpack_addmsg (xp, msg, 0);
  }
}

static seqno_t next_deliv_seq (const struct proxy_writer *pwr, const seqno_t next_seq)
{
  /* We want to determine next_deliv_seq, the next sequence number to
     be delivered to all in-sync readers, so that we can acknowledge
     what we have actually delivered.  This is different from next_seq
     tracks, which tracks the sequence number up to which all samples
     have been received.  The difference is the delivery queue.

     There is always but a single delivery queue, and hence delivery
     thread, associated with a single proxy writer; but the ACKs are
     always generated by another thread.  Therefore, updates to
     next_deliv_seq need only be atomic with respect to these reads.
     On all supported platforms we can atomically load and store 32
     bits without issue, and so we store just the low word of the
     sequence number.

     We know 1 <= next_deliv_seq AND next_seq - N <= next_deliv_seq <=
     next_seq for N << 2**32.  With n = next_seq, nd = next_deliv_seq,
     H the upper half and L the lower half:

       - H(nd) <= H(n) <= H(nd)+1         { n >= nd AND N << 2*32}
       - H(n) = H(nd)   => L(n) >= L(nd)  { n >= nd }
       - H(n) = H(nd)+1 => L(n) < L(nd)   { N << 2*32 }

     Therefore:

       L(n) < L(nd) <=> H(n) = H(nd+1)

     a.k.a.:

       nd = nd' - if nd' > nd then 2**32 else 0
         where nd' = 2**32 * H(n) + L(nd)

     By not locking next_deliv_seq, we may have nd a bit lower than it
     could be, but that only means we are acknowledging slightly less
     than we could; but that is perfectly acceptible.

     FIXME: next_seq - #dqueue could probably be used instead,
     provided #dqueue is decremented after delivery, rather than
     before delivery. */
  const uint32_t lw = ddsrt_atomic_ld32 (&pwr->next_deliv_seq_lowword);
  seqno_t next_deliv_seq;
  next_deliv_seq = (next_seq & ~(seqno_t) UINT32_MAX) | lw;
  if (next_deliv_seq > next_seq)
    next_deliv_seq -= ((seqno_t) 1) << 32;
  assert (0 < next_deliv_seq && next_deliv_seq <= next_seq);
  return next_deliv_seq;
}

static void add_AckNack (struct nn_xmsg *msg, struct proxy_writer *pwr, struct pwr_rd_match *rwn, seqno_t *nack_seq)
{
  /* If pwr->have_seen_heartbeat == 0, no heartbeat has been received
     by this proxy writer yet, so we'll be sending a pre-emptive
     AckNack.  NACKing data now will most likely cause another NACK
     upon reception of the first heartbeat, and so cause the data to
     be resent twice. */
  const unsigned max_numbits = 256; /* as spec'd */
  int notail = 0; /* all known missing ones are nack'd */
  struct nn_reorder *reorder;
  AckNack_t *an;
  struct nn_xmsg_marker sm_marker;
  uint32_t i, numbits;
  seqno_t base, last_seq;

  DDSRT_STATIC_ASSERT ((NN_FRAGMENT_NUMBER_SET_MAX_BITS % 32) == 0);
  struct {
    struct nn_fragment_number_set_header set;
    uint32_t bits[NN_FRAGMENT_NUMBER_SET_MAX_BITS / 32];
  } nackfrag;
  int nackfrag_numbits;
  seqno_t nackfrag_seq = 0;
  seqno_t bitmap_base;

  ASSERT_MUTEX_HELD (pwr->e.lock);

  /* if in sync, look at proxy writer status, else look at
     proxy-writer--reader match status */
  if (rwn->in_sync != PRMSS_OUT_OF_SYNC && !rwn->filtered)
  {
    reorder = pwr->reorder;
    if (!pwr->e.gv->config.late_ack_mode)
      bitmap_base = nn_reorder_next_seq (reorder);
    else
    {
      bitmap_base = next_deliv_seq (pwr, nn_reorder_next_seq (reorder));
      if (nn_dqueue_is_full (pwr->dqueue))
        notail = 1;
    }
  }
  else
  {
    reorder = rwn->u.not_in_sync.reorder;
    bitmap_base = nn_reorder_next_seq (reorder);
  }

  if (rwn->filtered)
    last_seq = rwn->last_seq;
  else
    last_seq = pwr->last_seq;

  an = nn_xmsg_append (msg, &sm_marker, ACKNACK_SIZE_MAX);
  nn_xmsg_submsg_init (msg, sm_marker, SMID_ACKNACK);
  an->readerId = nn_hton_entityid (rwn->rd_guid.entityid);
  an->writerId = nn_hton_entityid (pwr->e.guid.entityid);

  /* Make bitmap; note that we've made sure to have room for the
     maximum bitmap size. */
  numbits = nn_reorder_nackmap (reorder, bitmap_base, last_seq, &an->readerSNState, an->bits, max_numbits, notail);
  base = fromSN (an->readerSNState.bitmap_base);

  /* Scan through bitmap, cutting it off at the first missing sample
     that the defragmenter knows about. Then note the sequence number
     & add a NACKFRAG for that sample */
  nackfrag_numbits = -1;
  for (i = 0; i < numbits && nackfrag_numbits < 0; i++)
  {
    uint32_t fragnum;
    nackfrag_seq = base + i;
    if (!nn_bitset_isset (numbits, an->bits, i))
      continue;
    if (nackfrag_seq == last_seq)
      fragnum = pwr->last_fragnum;
    else
      fragnum = UINT32_MAX;
    nackfrag_numbits = nn_defrag_nackmap (pwr->defrag, nackfrag_seq, fragnum, &nackfrag.set, nackfrag.bits, max_numbits);
  }
  if (nackfrag_numbits >= 0) {
    /* Cut the NACK short, NACKFRAG will be added after the NACK's is
       properly formatted */
    assert (i > 0);
    an->readerSNState.numbits = numbits = i - 1;
  }

  /* Let caller know whether it is a nack, and, in steady state, set
     final to prevent a response if it isn't.  The initial
     (pre-emptive) acknack is different: it'd be nice to get a
     heartbeat in response.

     Who cares about an answer to an acknowledgment!? -- actually,
     that'd a very useful feature in combination with directed
     heartbeats, or somesuch, to get reliability guarantees. */
  *nack_seq = (numbits > 0) ? base + numbits : 0;
  if (!pwr->have_seen_heartbeat) {
    /* We must have seen a heartbeat for us to consider setting FINAL */
  } else if (*nack_seq && base + numbits <= last_seq) {
    /* If it's a NACK and it doesn't cover samples all the way up to
       the highest known sequence number, there's some reason to expect
       we may to do another round.  For which we need a Heartbeat.

       Note: last_seq exists, base is first in bitmap, numbits is
       length of bitmap, hence less-than-or-equal. */
  } else {
    /* An ACK or we think we'll get everything now. */
    an->smhdr.flags |= ACKNACK_FLAG_FINAL;
  }

  {
    /* Count field is at a variable offset ... silly DDSI spec. */
    nn_count_t *countp =
      (nn_count_t *) ((char *) an + offsetof (AckNack_t, bits) + NN_SEQUENCE_NUMBER_SET_BITS_SIZE (an->readerSNState.numbits));
    *countp = ++rwn->count;

    /* Reset submessage size, now that we know the real size, and update
       the offset to the next submessage. */
    nn_xmsg_shrink (msg, sm_marker, ACKNACK_SIZE (an->readerSNState.numbits));
    nn_xmsg_submsg_setnext (msg, sm_marker);

    ETRACE (pwr, "acknack "PGUIDFMT" -> "PGUIDFMT": #%"PRId32":%"PRId64"/%"PRIu32":",
            PGUID (rwn->rd_guid), PGUID (pwr->e.guid), rwn->count,
            base, an->readerSNState.numbits);
    for (uint32_t ui = 0; ui != an->readerSNState.numbits; ui++)
      ETRACE (pwr, "%c", nn_bitset_isset (numbits, an->bits, ui) ? '1' : '0');

    /* Encode the sub-message when needed. */
    encode_datareader_submsg(msg, sm_marker, pwr, &rwn->rd_guid);
  }

  if (nackfrag_numbits > 0)
  {
    NackFrag_t *nf;

    /* We use 0-based fragment numbers, but externally have to provide
       1-based fragment numbers */
    assert ((unsigned) nackfrag_numbits == nackfrag.set.numbits);

    nf = nn_xmsg_append (msg, &sm_marker, NACKFRAG_SIZE ((unsigned) nackfrag_numbits));

    nn_xmsg_submsg_init (msg, sm_marker, SMID_NACK_FRAG);
    nf->readerId = nn_hton_entityid (rwn->rd_guid.entityid);
    nf->writerId = nn_hton_entityid (pwr->e.guid.entityid);
    nf->writerSN = toSN (nackfrag_seq);
    nf->fragmentNumberState.bitmap_base = nackfrag.set.bitmap_base + 1;
    nf->fragmentNumberState.numbits = nackfrag.set.numbits;
    memcpy (nf->bits, nackfrag.bits, NN_FRAGMENT_NUMBER_SET_BITS_SIZE (nackfrag_numbits));

    {
      nn_count_t *countp =
        (nn_count_t *) ((char *) nf + offsetof (NackFrag_t, bits) + NN_FRAGMENT_NUMBER_SET_BITS_SIZE (nf->fragmentNumberState.numbits));
      *countp = ++pwr->nackfragcount;
      nn_xmsg_submsg_setnext (msg, sm_marker);

      ETRACE (pwr, " + nackfrag #%"PRId32":%"PRId64"/%u/%"PRIu32":", *countp, fromSN (nf->writerSN), nf->fragmentNumberState.bitmap_base, nf->fragmentNumberState.numbits);
      for (uint32_t ui = 0; ui != nf->fragmentNumberState.numbits; ui++)
        ETRACE (pwr, "%c", nn_bitset_isset (nf->fragmentNumberState.numbits, nf->bits, ui) ? '1' : '0');
    }

    /* Encode the sub-message when needed. */
    encode_datareader_submsg(msg, sm_marker, pwr, &rwn->rd_guid);
  }

  ETRACE (pwr, "\n");
}

static void handle_xevk_acknack (struct nn_xpack *xp, struct xevent *ev, nn_mtime_t tnow)
{
  /* FIXME: ought to keep track of which NACKs are being generated in
     response to a Heartbeat.  There is no point in having multiple
     readers NACK the data.

     FIXME: ought to determine the set of missing samples (as it does
     now), and then check which for of those fragments are available already.
     A little snag is that the defragmenter can throw out partial samples in
     favour of others, so MUST ensure that the defragmenter won't start
     threshing and fail to make progress! */
  struct q_globals *gv = ev->evq->gv;
  struct proxy_writer *pwr;
  struct nn_xmsg *msg;
  struct pwr_rd_match *rwn;
  nn_locator_t loc;

  if ((pwr = ephash_lookup_proxy_writer_guid (gv->guid_hash, &ev->u.acknack.pwr_guid)) == NULL)
  {
    return;
  }

  ddsrt_mutex_lock (&pwr->e.lock);
  if ((rwn = ddsrt_avl_lookup (&pwr_readers_treedef, &pwr->readers, &ev->u.acknack.rd_guid)) == NULL)
  {
    ddsrt_mutex_unlock (&pwr->e.lock);
    return;
  }

  if (addrset_any_uc (pwr->c.as, &loc) || addrset_any_mc (pwr->c.as, &loc))
  {
    seqno_t nack_seq;

    struct participant *pp = NULL;
    struct reader *rd = ephash_lookup_reader_guid(pwr->e.gv->guid_hash, &ev->u.acknack.rd_guid);
    if (rd){
      pp = rd->c.pp;
    }

    if ((msg = nn_xmsg_new (gv->xmsgpool, &ev->u.acknack.rd_guid, pp, ACKNACK_SIZE_MAX, NN_XMSG_KIND_CONTROL)) == NULL)
      goto outofmem;
    nn_xmsg_setdst1 (gv, msg, &ev->u.acknack.pwr_guid.prefix, &loc);
    if (gv->config.meas_hb_to_ack_latency && rwn->hb_timestamp.v)
    {
      /* If HB->ACK latency measurement is enabled, and we have a
         timestamp available, add it and clear the time stamp.  There
         is no real guarantee that the two match, but I haven't got a
         solution for that yet ...  If adding the time stamp fails,
         too bad, but no reason to get worried. */
      nn_xmsg_add_timestamp (msg, rwn->hb_timestamp);
      rwn->hb_timestamp.v = 0;
    }
    add_AckNack (msg, pwr, rwn, &nack_seq);
    if (nn_xmsg_size(msg) == 0)
    {
      /* No AckNack added. */
      nn_xmsg_free(msg);
      msg = NULL;
    }
    else if (nack_seq)
    {
      rwn->t_last_nack = tnow;
      rwn->seq_last_nack = nack_seq;
      /* If NACKing, make sure we don't give up too soon: even though
         we're not allowed to send an ACKNACK unless in response to a
         HEARTBEAT, I've seen too many cases of not sending an NACK
         because the writing side got confused ...  Better to recover
         eventually. */
      (void) resched_xevent_if_earlier (ev, add_duration_to_mtime (tnow, gv->config.auto_resched_nack_delay));
    }
    GVTRACE ("send acknack(rd "PGUIDFMT" -> pwr "PGUIDFMT")\n",
             PGUID (ev->u.acknack.rd_guid), PGUID (ev->u.acknack.pwr_guid));
  }
  else
  {
    GVTRACE ("skip acknack(rd "PGUIDFMT" -> pwr "PGUIDFMT"): no address\n",
             PGUID (ev->u.acknack.rd_guid), PGUID (ev->u.acknack.pwr_guid));
    msg = NULL;
  }

  if (!pwr->have_seen_heartbeat && tnow.v - rwn->tcreate.v <= 300 * T_SECOND)
  {
     /* Force pre-emptive AckNacks out until we receive a heartbeat,
        but let the frequency drop over time and stop after a couple
        of minutes. */
    int intv, age = (int) ((tnow.v - rwn->tcreate.v) / T_SECOND + 1);
    if (age <= 10)
      intv = 1;
    else if (age <= 60)
      intv = 2;
    else if (age <= 120)
      intv = 5;
    else
      intv = 10;
    (void) resched_xevent_if_earlier (ev, add_duration_to_mtime (tnow, intv * T_SECOND));
  }
  ddsrt_mutex_unlock (&pwr->e.lock);

  /* nn_xpack_addmsg may sleep (for bandwidth-limited channels), so
     must be outside the lock */
  if (msg)
    nn_xpack_addmsg (xp, msg, 0);
  return;

 outofmem:
  /* What to do if out of memory?  Crash or burn? */
  ddsrt_mutex_unlock (&pwr->e.lock);
  (void) resched_xevent_if_earlier (ev, add_duration_to_mtime (tnow, 100 * T_MILLISECOND));
}

static bool resend_spdp_sample_by_guid_key (struct writer *wr, const ddsi_guid_t *guid, struct proxy_reader *prd)
{
  /* Look up data in (transient-local) WHC by key value -- FIXME: clearly
   a slightly more efficient and elegant way of looking up the key value
   is to be preferred */
  struct q_globals *gv = wr->e.gv;
  bool sample_found;
  nn_plist_t ps;
  nn_plist_init_empty (&ps);
  ps.present |= PP_PARTICIPANT_GUID;
  ps.participant_guid = *guid;
  struct nn_xmsg *mpayload = nn_xmsg_new (gv->xmsgpool, guid, wr->c.pp, 0, NN_XMSG_KIND_DATA);
  nn_plist_addtomsg (mpayload, &ps, ~(uint64_t)0, ~(uint64_t)0);
  nn_xmsg_addpar_sentinel (mpayload);
  nn_plist_fini (&ps);
  struct ddsi_plist_sample plist_sample;
  nn_xmsg_payload_to_plistsample (&plist_sample, PID_PARTICIPANT_GUID, mpayload);
  struct ddsi_serdata *sd = ddsi_serdata_from_sample (gv->plist_topic, SDK_KEY, &plist_sample);
  struct whc_borrowed_sample sample;
  nn_xmsg_free (mpayload);

  ddsrt_mutex_lock (&wr->e.lock);
  sample_found = whc_borrow_sample_key (wr->whc, sd, &sample);
  if (sample_found)
  {
    /* Claiming it is new rather than a retransmit so that the rexmit
     limiting won't kick in.  It is best-effort and therefore the
     updating of the last transmitted sequence number won't take
     place anyway.  Nor is it necessary to fiddle with heartbeat
     control stuff. */
    enqueue_sample_wrlock_held (wr, sample.seq, sample.plist, sample.serdata, prd, 1);
    whc_return_sample(wr->whc, &sample, false);
  }
  ddsrt_mutex_unlock (&wr->e.lock);
  ddsi_serdata_unref (sd);
  return sample_found;
}

static void handle_xevk_spdp (UNUSED_ARG (struct nn_xpack *xp), struct xevent *ev, nn_mtime_t tnow)
{
  /* Like the writer pointer in the heartbeat event, the participant pointer in the spdp event is assumed valid. */
  struct q_globals *gv = ev->evq->gv;
  struct participant *pp;
  struct proxy_reader *prd;
  struct writer *spdp_wr;
  bool do_write;

  if ((pp = ephash_lookup_participant_guid (gv->guid_hash, &ev->u.spdp.pp_guid)) == NULL)
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
    prd = ephash_lookup_proxy_reader_guid (gv->guid_hash, &guid);
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
    if (--ev->u.spdp.directed == 0 || gv->config.spdp_interval < T_SECOND || pp->lease_duration < T_SECOND)
      delete_xevent (ev);
    else
    {
      nn_mtime_t tnext = add_duration_to_mtime (tnow, T_SECOND);
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
    const dds_duration_t mindelta = 10 * T_MILLISECOND;
    const dds_duration_t ldur = pp->lease_duration;
    nn_mtime_t tnext;
    int64_t intv;

    if (ldur < 5 * mindelta / 4)
      intv = mindelta;
    else if (ldur < 10 * T_SECOND)
      intv = 4 * ldur / 5;
    else
      intv = ldur - 2 * T_SECOND;
    if (intv > gv->config.spdp_interval)
      intv = gv->config.spdp_interval;

    tnext = add_duration_to_mtime (tnow, intv);
    GVTRACE ("xmit spdp "PGUIDFMT" to %"PRIx32":%"PRIx32":%"PRIx32":%x (resched %gs)\n",
             PGUID (pp->e.guid),
             PGUIDPREFIX (ev->u.spdp.dest_proxypp_guid_prefix), NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER,
             (double)(tnext.v - tnow.v) / 1e9);
    (void) resched_xevent_if_earlier (ev, tnext);
  }
}

static void handle_xevk_pmd_update (struct thread_state1 * const ts1, struct nn_xpack *xp, struct xevent *ev, nn_mtime_t tnow)
{
  struct q_globals * const gv = ev->evq->gv;
  struct participant *pp;
  dds_duration_t intv;
  nn_mtime_t tnext;

  if ((pp = ephash_lookup_participant_guid (gv->guid_hash, &ev->u.pmd_update.pp_guid)) == NULL)
  {
    return;
  }

  write_pmd_message (ts1, xp, pp, PARTICIPANT_MESSAGE_DATA_KIND_AUTOMATIC_LIVELINESS_UPDATE);

  intv = pp_get_pmd_interval (pp);
  if (intv == T_NEVER)
  {
    tnext.v = T_NEVER;
    GVTRACE ("resched pmd("PGUIDFMT"): never\n", PGUID (pp->e.guid));
  }
  else
  {
    /* schedule next when 80% of the interval has elapsed, or 2s
       before the lease ends, whichever comes first */
    if (intv >= 10 * T_SECOND)
      tnext.v = tnow.v + intv - 2 * T_SECOND;
    else
      tnext.v = tnow.v + 4 * intv / 5;
    GVTRACE ("resched pmd("PGUIDFMT"): %gs\n", PGUID (pp->e.guid), (double)(tnext.v - tnow.v) / 1e9);
  }

  (void) resched_xevent_if_earlier (ev, tnext);
}

static void handle_xevk_delete_writer (UNUSED_ARG (struct nn_xpack *xp), struct xevent *ev, UNUSED_ARG (nn_mtime_t tnow))
{
  /* don't worry if the writer is already gone by the time we get here. */
  struct q_globals * const gv = ev->evq->gv;
  GVTRACE ("handle_xevk_delete_writer: "PGUIDFMT"\n", PGUID (ev->u.delete_writer.guid));
  delete_writer_nolinger (gv, &ev->u.delete_writer.guid);
  delete_xevent (ev);
}

static void handle_individual_xevent (struct thread_state1 * const ts1, struct xevent *xev, struct nn_xpack *xp, nn_mtime_t tnow)
{
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
      xev->u.callback.cb (xev, xev->u.callback.arg, tnow);
      break;
  }
}

static void handle_individual_xevent_nt (struct xevent_nt *xev, struct nn_xpack *xp)
{
  switch (xev->kind)
  {
    case XEVK_MSG:
      handle_xevk_msg (xp, xev);
      break;
    case XEVK_MSG_REXMIT:
      handle_xevk_msg_rexmit (xp, xev);
      break;
    case XEVK_ENTITYID:
      handle_xevk_entityid (xp, xev);
      break;
  }
  ddsrt_free (xev);
}

static void handle_timed_xevent (struct thread_state1 * const ts1, struct xevent *xev, struct nn_xpack *xp, nn_mtime_t tnow /* monotonic */)
{
   /* This function handles the individual xevent irrespective of
      whether it is a "timed" or "non-timed" xevent */
  struct xeventq *xevq = xev->evq;

  /* We relinquish the lock while processing the event, but require it
     held for administrative work. */
  ASSERT_MUTEX_HELD (&xevq->lock);

  assert (xev->evq == xevq);
  assert (xev->tsched.v != TSCHED_DELETE);

  ddsrt_mutex_unlock (&xevq->lock);
  handle_individual_xevent (ts1, xev, xp, tnow /* monotonic */);
  ddsrt_mutex_lock (&xevq->lock);

  ASSERT_MUTEX_HELD (&xevq->lock);
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

static void handle_xevents (struct thread_state1 * const ts1, struct xeventq *xevq, struct nn_xpack *xp, nn_mtime_t tnow /* monotonic */)
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
        xev->tsched.v = T_NEVER;
        thread_state_awake_to_awake_no_nest (ts1);
        handle_timed_xevent (ts1, xev, xp, tnow);
      }

      /* Limited-bandwidth channels means events can take a LONG time
         to process.  So read the clock more often. */
      tnow = now_mt ();
    }

    if (!non_timed_xmit_list_is_empty (xevq))
    {
      struct xevent_nt *xev = getnext_from_non_timed_xmit_list (xevq);
      thread_state_awake_to_awake_no_nest (ts1);
      handle_nontimed_xevent (xev, xp);
      tnow = now_mt ();
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
  nn_mtime_t next_thread_cputime = { 0 };

  xp = nn_xpack_new (xevq->tev_conn, xevq->auxiliary_bandwidth_limit, xevq->gv->config.xpack_send_async);

  ddsrt_mutex_lock (&xevq->lock);
  while (!xevq->terminate)
  {
    nn_mtime_t tnow = now_mt ();

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
      nn_mtime_t twakeup = earliest_in_xeventq (xevq);
      if (twakeup.v == T_NEVER)
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
        tnow = now_mt ();
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

void qxev_prd_entityid (struct proxy_reader *prd, const ddsi_guid_t *guid)
{
  struct q_globals * const gv = prd->e.gv;
  struct nn_xmsg *msg;
  struct xevent_nt *ev;

  /* For connected transports, may need to establish and identify connection */

  if (! gv->xevents->tev_conn->m_connless)
  {
    msg = nn_xmsg_new (gv->xmsgpool, guid, NULL, sizeof (EntityId_t), NN_XMSG_KIND_CONTROL);
    if (nn_xmsg_setdstPRD (msg, prd) == 0)
    {
      GVTRACE ("  qxev_prd_entityid (%"PRIx32":%"PRIx32":%"PRIx32")\n", PGUIDPREFIX (guid->prefix));
      nn_xmsg_add_entityid (msg);
      ddsrt_mutex_lock (&gv->xevents->lock);
      ev = qxev_common_nt (gv->xevents, XEVK_ENTITYID);
      ev->u.entityid.msg = msg;
      qxev_insert_nt (ev);
      ddsrt_mutex_unlock (&gv->xevents->lock);
    }
    else
    {
      nn_xmsg_free (msg);
    }
  }
}

void qxev_pwr_entityid (struct proxy_writer *pwr, const ddsi_guid_t *guid)
{
  struct q_globals * const gv = pwr->e.gv;
  struct nn_xmsg *msg;
  struct xevent_nt *ev;

  /* For connected transports, may need to establish and identify connection */

  if (! pwr->evq->tev_conn->m_connless)
  {
    msg = nn_xmsg_new (gv->xmsgpool, guid, NULL, sizeof (EntityId_t), NN_XMSG_KIND_CONTROL);
    if (nn_xmsg_setdstPWR (msg, pwr) == 0)
    {
      GVTRACE ("  qxev_pwr_entityid (%"PRIx32":%"PRIx32":%"PRIx32")\n", PGUIDPREFIX (guid->prefix));
      nn_xmsg_add_entityid (msg);
      ddsrt_mutex_lock (&pwr->evq->lock);
      ev = qxev_common_nt (pwr->evq, XEVK_ENTITYID);
      ev->u.entityid.msg = msg;
      qxev_insert_nt (ev);
      ddsrt_mutex_unlock (&pwr->evq->lock);
    }
    else
    {
      nn_xmsg_free (msg);
    }
  }
}

int qxev_msg_rexmit_wrlock_held (struct xeventq *evq, struct nn_xmsg *msg, int force)
{
  struct q_globals * const gv = evq->gv;
  size_t msg_size = nn_xmsg_size (msg);
  struct xevent_nt *ev;

  assert (evq);
  assert (nn_xmsg_kind (msg) == NN_XMSG_KIND_DATA_REXMIT);
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
    ev = qxev_common_nt (evq, XEVK_MSG_REXMIT);
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

struct xevent *qxev_heartbeat (struct xeventq *evq, nn_mtime_t tsched, const ddsi_guid_t *wr_guid)
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

struct xevent *qxev_acknack (struct xeventq *evq, nn_mtime_t tsched, const ddsi_guid_t *pwr_guid, const ddsi_guid_t *rd_guid)
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

struct xevent *qxev_spdp (struct xeventq *evq, nn_mtime_t tsched, const ddsi_guid_t *pp_guid, const ddsi_guid_t *dest_proxypp_guid)
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

struct xevent *qxev_pmd_update (struct xeventq *evq, nn_mtime_t tsched, const ddsi_guid_t *pp_guid)
{
  struct xevent *ev;
  ddsrt_mutex_lock (&evq->lock);
  ev = qxev_common (evq, tsched, XEVK_PMD_UPDATE);
  ev->u.pmd_update.pp_guid = *pp_guid;
  qxev_insert (ev);
  ddsrt_mutex_unlock (&evq->lock);
  return ev;
}

struct xevent *qxev_delete_writer (struct xeventq *evq, nn_mtime_t tsched, const ddsi_guid_t *guid)
{
  struct xevent *ev;
  ddsrt_mutex_lock (&evq->lock);
  ev = qxev_common (evq, tsched, XEVK_DELETE_WRITER);
  ev->u.delete_writer.guid = *guid;
  qxev_insert (ev);
  ddsrt_mutex_unlock (&evq->lock);
  return ev;
}

struct xevent *qxev_callback (struct xeventq *evq, nn_mtime_t tsched, void (*cb) (struct xevent *ev, void *arg, nn_mtime_t tnow), void *arg)
{
  struct xevent *ev;
  ddsrt_mutex_lock (&evq->lock);
  ev = qxev_common (evq, tsched, XEVK_CALLBACK);
  ev->u.callback.cb = cb;
  ev->u.callback.arg = arg;
  qxev_insert (ev);
  ddsrt_mutex_unlock (&evq->lock);
  return ev;
}
