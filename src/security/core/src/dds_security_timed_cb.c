// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/fibheap.h"

#include "dds/security/core/dds_security_timed_cb.h"

// The xevent mechanism runs on the monotonic clock, but certificate expiry
// times are expressed in terms of wall-clock time.  The wall clock may jump
// forward or backward, so we rather than wait until the next expiry event,
// limit the interval between checks so that a certificate expiring because
// the wall clock jumped forward is detected in a reasonable amount of time.
#define CHECK_TIMER_INTERVAL DDS_SECS(300) /* 5 minutes */

struct dds_security_timed_dispatcher
{
  ddsrt_mutex_t lock;
  struct ddsi_xeventq *evq;
  struct ddsi_xevent *evt;
  ddsrt_avl_tree_t events;
  ddsrt_fibheap_t timers;
  dds_security_time_event_handle_t next_timer;
};

struct dds_security_timed_event
{
  ddsrt_avl_node_t avlnode;
  ddsrt_fibheap_node_t heapnode;
  dds_security_time_event_handle_t handle;
  dds_security_timed_cb_t callback;
  dds_time_t trigger_time;
  void *arg;
};

static int compare_time_event (const void *va, const void *vb) ddsrt_nonnull_all;
static int compare_timed_cb_trigger_time (const void *va, const void *vb) ddsrt_nonnull_all;

static const ddsrt_avl_treedef_t timed_event_treedef = DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct dds_security_timed_event, avlnode), offsetof (struct dds_security_timed_event, handle), compare_time_event, 0);
static const ddsrt_fibheap_def_t timed_cb_queue_fhdef = DDSRT_FIBHEAPDEF_INITIALIZER (offsetof (struct dds_security_timed_event, heapnode), compare_timed_cb_trigger_time);

static int compare_time_event (const void *va, const void *vb)
{
  const dds_security_time_event_handle_t *ha = va;
  const dds_security_time_event_handle_t *hb = vb;
  return (*ha > *hb) ? 1 : (*ha < *hb) ? -1 : 0;
}

static int compare_timed_cb_trigger_time (const void *va, const void *vb)
{
  const struct dds_security_timed_event *a = va;
  const struct dds_security_timed_event *b = vb;
  return (a->trigger_time == b->trigger_time) ? 0 : (a->trigger_time < b->trigger_time) ? -1 : 1;
}

static struct dds_security_timed_event *timed_event_new (dds_security_time_event_handle_t handle, dds_security_timed_cb_t callback, dds_time_t trigger_time, void *arg)
{
  struct dds_security_timed_event *ev = ddsrt_malloc (sizeof (*ev));
  ev->handle = handle;
  ev->callback = callback;
  ev->trigger_time = trigger_time;
  ev->arg = arg;
  return ev;
}

static ddsrt_mtime_t calc_tsched (const struct dds_security_timed_event *ev, dds_time_t tnow_wc)
{
  if (ev == NULL)
    return DDSRT_MTIME_NEVER;
  else if (ev->trigger_time < tnow_wc)
    return ddsrt_time_monotonic ();
  else
  {
    const dds_duration_t delta = ev->trigger_time - tnow_wc;
    const dds_duration_t timeout = (delta < CHECK_TIMER_INTERVAL) ? delta : CHECK_TIMER_INTERVAL;
    return ddsrt_mtime_add_duration (ddsrt_time_monotonic (), timeout);
  }
}

struct timed_event_cb_arg {
  struct dds_security_timed_dispatcher *dispatcher;
};

static void timed_event_cb (struct ddsi_domaingv *gv, struct ddsi_xevent *xev, struct ddsi_xpack *xp, void *varg, ddsrt_mtime_t tnow)
{
  struct timed_event_cb_arg * const arg = varg;
  struct dds_security_timed_dispatcher * const dispatcher = arg->dispatcher;
  (void) gv;
  (void) xp;
  (void) tnow;

  ddsrt_mutex_lock (&dispatcher->lock);
  dds_time_t tnow_wc = dds_time ();
  struct dds_security_timed_event *ev;
  while ((ev = ddsrt_fibheap_min (&timed_cb_queue_fhdef, &dispatcher->timers)) != NULL && ev->trigger_time < tnow_wc)
  {
    (void) ddsrt_fibheap_extract_min (&timed_cb_queue_fhdef, &dispatcher->timers);
    ddsrt_avl_delete (&timed_event_treedef, &dispatcher->events, ev);
    ddsrt_mutex_unlock (&dispatcher->lock);

    ev->callback (ev->handle, ev->trigger_time, DDS_SECURITY_TIMED_CB_KIND_TIMEOUT, ev->arg);
    ddsrt_free (ev);

    ddsrt_mutex_lock (&dispatcher->lock);
    tnow_wc = dds_time ();
  }
  // Note: xev may be different from dispatcher->evt if it is being
  // disabled (in which case the latter may be a null pointer) or
  // even a different event (in case it is already being re-enabled),
  // and so we must use xev.
  (void) ddsi_resched_xevent_if_earlier (xev, calc_tsched (ev, tnow_wc));
  ddsrt_mutex_unlock (&dispatcher->lock);
}

struct dds_security_timed_dispatcher *dds_security_timed_dispatcher_new (struct ddsi_xeventq *evq)
{
  struct dds_security_timed_dispatcher *d = ddsrt_malloc (sizeof (*d));
  ddsrt_mutex_init (&d->lock);
  ddsrt_avl_init (&timed_event_treedef, &d->events);
  ddsrt_fibheap_init (&timed_cb_queue_fhdef, &d->timers);
  d->evq = evq;
  d->evt = NULL;
  d->next_timer = 1;
  return d;
}

void dds_security_timed_dispatcher_enable (struct dds_security_timed_dispatcher *d)
{
  ddsrt_mutex_lock (&d->lock);
  if (d->evt == NULL)
  {
    struct dds_security_timed_event const * const ev = ddsrt_fibheap_min (&timed_cb_queue_fhdef, &d->timers);
    struct timed_event_cb_arg arg = { .dispatcher = d };
    d->evt = ddsi_qxev_callback (d->evq, calc_tsched (ev, dds_time ()), timed_event_cb, &arg, sizeof (arg), true);
  }
  ddsrt_mutex_unlock (&d->lock);
}

bool dds_security_timed_dispatcher_disable (struct dds_security_timed_dispatcher *d)
{
  // ddsi_delete_xevent_callback() blocks while a possible concurrent
  // callback invocation completes, and so disable() can't hold the
  // dispatcher lock when it calls ddsi_delete_xevent_callback().
  //
  // Two obvious ways of dealing with this are:
  //
  // - Protect the "dispatcher->evt" and the existence of a callback
  //   event field by a separate lock, serializing enable/disable
  //   calls but without interfering with callback execution.
  //
  // - Have just one lock protecting all state (including "evt"),
  //   but call ddsi_delete_xevent_callback outside it.  This means
  //   the callbacks can execute while "evt" is a null pointer,
  //   and that enable() can be called, installing a new callback
  //   event while the "old" one is still executing.
  //
  //   It also means that for two concurrent calls to disable(),
  //   one will reset "evt" and wait for the callback to be removed,
  //   while the one that loses the race may return before the
  //   callback is removed.
  //
  // Having multiple callback events merely leads to a minor amount
  // of superfluous work, provided they don't depend on "evt", and
  // the case of two concurrent calls to disable() doesn't seem to
  // be an issue.  So the second option is sufficient.
  struct ddsi_xevent *evt;

  ddsrt_mutex_lock (&d->lock);
  evt = d->evt;
  d->evt = NULL;
  ddsrt_mutex_unlock (&d->lock);

  if (evt != NULL)
    ddsi_delete_xevent (evt);
  return (evt != NULL);
}

void dds_security_timed_dispatcher_free (struct dds_security_timed_dispatcher *d)
{
  struct dds_security_timed_event *ev;

  // By this time, no other thread may be referencing "d" anymore, and so there
  // can't be any concurrent calls to enable() or disable().  Therefore, either
  // d->evt != NULL and the callback exists, or d->evt == NULL and the callback
  // does not exist.
  //
  // Thus, following the call, there will be no callback.
  (void) dds_security_timed_dispatcher_disable (d);

  while ((ev = ddsrt_fibheap_extract_min (&timed_cb_queue_fhdef, &d->timers)) != NULL)
  {
    ev->callback (ev->handle, ev->trigger_time, DDS_SECURITY_TIMED_CB_KIND_DELETE, ev->arg);
    ddsrt_free (ev);
  }
  ddsrt_mutex_destroy (&d->lock);
  ddsrt_free (d);
}

dds_security_time_event_handle_t dds_security_timed_dispatcher_add (struct dds_security_timed_dispatcher *d, dds_security_timed_cb_t cb, dds_time_t trigger_time, void *arg)
{
  ddsrt_mutex_lock (&d->lock);
  struct dds_security_timed_event * const ev = timed_event_new (d->next_timer, cb, trigger_time, arg);
  ddsrt_avl_insert (&timed_event_treedef, &d->events, ev);
  ddsrt_fibheap_insert (&timed_cb_queue_fhdef, &d->timers, ev);
  d->next_timer++;
  if (d->evt != NULL)
    (void) ddsi_resched_xevent_if_earlier (d->evt, calc_tsched (ev, dds_time ()));
  ddsrt_mutex_unlock (&d->lock);
  return ev->handle;
}

void dds_security_timed_dispatcher_remove (struct dds_security_timed_dispatcher *d, dds_security_time_event_handle_t timer)
{
  ddsrt_avl_dpath_t dpath;
  ddsrt_mutex_lock (&d->lock);
  struct dds_security_timed_event * const ev = ddsrt_avl_lookup_dpath (&timed_event_treedef, &d->events, &timer, &dpath);
  if (ev == NULL)
  {
    // if the timer id doesn't exist anymore, it already expired and this is a no-op
    ddsrt_mutex_unlock (&d->lock);
  }
  else
  {
    ddsrt_avl_delete_dpath (&timed_event_treedef, &d->events, ev, &dpath);
    ddsrt_fibheap_delete (&timed_cb_queue_fhdef, &d->timers, ev);
    ddsrt_mutex_unlock (&d->lock);
    ev->callback (ev->handle, ev->trigger_time, DDS_SECURITY_TIMED_CB_KIND_DELETE, ev->arg);
    ddsrt_free (ev);
  }
}
