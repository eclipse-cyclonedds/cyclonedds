/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

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

#define CHECK_TIMER_INTERVAL DDS_SECS(300) /* 5 minutes */

struct dds_security_timed_dispatcher
{
  ddsrt_mutex_t lock;
  struct xeventq *evq;
  struct xevent *evt;
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

static int compare_time_event (const void *va, const void *vb);
static int compare_timed_cb_trigger_time(const void *va, const void *vb);

static const ddsrt_avl_treedef_t timed_event_treedef = DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct dds_security_timed_event, avlnode), offsetof (struct dds_security_timed_event, handle), compare_time_event, 0);
static const ddsrt_fibheap_def_t timed_cb_queue_fhdef = DDSRT_FIBHEAPDEF_INITIALIZER(offsetof(struct dds_security_timed_event, heapnode), compare_timed_cb_trigger_time);

static int compare_time_event (const void *va, const void *vb)
{
  const dds_security_time_event_handle_t *ha = va;
  const dds_security_time_event_handle_t *hb = vb;
  return ((*ha > *hb) ? 1 : (*ha < *hb) ?  -1 : 0);
}

static int compare_timed_cb_trigger_time(const void *va, const void *vb)
{
  const struct dds_security_timed_event *a = va;
  const struct dds_security_timed_event *b = vb;
  return (a->trigger_time == b->trigger_time) ? 0 : (a->trigger_time < b->trigger_time) ? -1 : 1;
}

static void timed_event_cb (struct xevent *xev, void *varg, ddsrt_mtime_t tnow);

static struct dds_security_timed_event * timed_event_new(dds_security_time_event_handle_t handle, dds_security_timed_cb_t callback, dds_time_t trigger_time, void *arg)
{
  struct dds_security_timed_event *ev = ddsrt_malloc(sizeof(*ev));
  ev->handle = handle;
  ev->callback = callback;
  ev->trigger_time = trigger_time;
  ev->arg = arg;
  return ev;
}

static void timed_event_cb (struct xevent *xev, void *arg, ddsrt_mtime_t tnow)
{
  struct dds_security_timed_dispatcher *dispatcher = arg;
  dds_duration_t timeout = CHECK_TIMER_INTERVAL;
  struct dds_security_timed_event *ev = NULL;
  dds_duration_t delta = 1;

  DDSRT_UNUSED_ARG(tnow);
  DDSRT_UNUSED_ARG(xev);

  ddsrt_mutex_lock(&dispatcher->lock);

  do {
    if ((ev = ddsrt_fibheap_min(&timed_cb_queue_fhdef, &dispatcher->timers)) != NULL)
    {
      delta = ev->trigger_time - dds_time();
      if (delta > 0 && delta < CHECK_TIMER_INTERVAL)
        timeout = delta;
      else if (delta <= 0)
      {
        (void)ddsrt_fibheap_extract_min(&timed_cb_queue_fhdef, &dispatcher->timers);
        ddsrt_avl_delete(&timed_event_treedef, &dispatcher->events, ev);
        ddsrt_mutex_unlock(&dispatcher->lock);
        ev->callback(ev->handle, ev->trigger_time, DDS_SECURITY_TIMED_CB_KIND_TIMEOUT, ev->arg);
        ddsrt_mutex_lock(&dispatcher->lock);
        ddsrt_free(ev);
      }
    }
  } while (ev && delta <= 0);

  if (ev)
  {
    ddsrt_mtime_t tsched = ddsrt_mtime_add_duration(ddsrt_time_monotonic(), timeout);
    (void)resched_xevent_if_earlier(dispatcher->evt, tsched);
  }

  ddsrt_mutex_unlock(&dispatcher->lock);
}

struct dds_security_timed_dispatcher * dds_security_timed_dispatcher_new(struct xeventq *evq)
{
  struct dds_security_timed_dispatcher *d = ddsrt_malloc(sizeof(*d));
  ddsrt_mutex_init (&d->lock);
  ddsrt_avl_init(&timed_event_treedef, &d->events);
  ddsrt_fibheap_init(&timed_cb_queue_fhdef, &d->timers);
  d->evq = evq;
  d->evt = NULL;
  d->next_timer = 1;
  return d;
}

void dds_security_timed_dispatcher_enable(struct dds_security_timed_dispatcher *d)
{
  ddsrt_mutex_lock(&d->lock);
  if (d->evt == NULL)
  {
    struct dds_security_timed_event *ev = ddsrt_fibheap_min(&timed_cb_queue_fhdef, &d->timers);
    ddsrt_mtime_t tsched = DDSRT_MTIME_NEVER;

    if (ev)
    {
      dds_duration_t delta = ev->trigger_time - dds_time();
      if (delta < 0)
        delta = 0;
      tsched = ddsrt_mtime_add_duration(ddsrt_time_monotonic(), delta < CHECK_TIMER_INTERVAL ? delta : CHECK_TIMER_INTERVAL);
    }
    d->evt = qxev_callback (d->evq, tsched, timed_event_cb, d);
  }
  ddsrt_mutex_unlock(&d->lock);
}

void dds_security_timed_dispatcher_disable(struct dds_security_timed_dispatcher *d)
{
  ddsrt_mutex_lock(&d->lock);
  if (d->evt != NULL)
  {
    delete_xevent_callback(d->evt);
    d->evt = NULL;
  }
  ddsrt_mutex_unlock(&d->lock);
}

void dds_security_timed_dispatcher_free(struct dds_security_timed_dispatcher *d)
{
  struct dds_security_timed_event *ev;

  dds_security_timed_dispatcher_disable(d);
  ev = ddsrt_fibheap_extract_min(&timed_cb_queue_fhdef, &d->timers);
  while (ev)
  {
    ev->callback(ev->handle, ev->trigger_time, DDS_SECURITY_TIMED_CB_KIND_DELETE, ev->arg);
    ddsrt_free(ev);
    ev = ddsrt_fibheap_extract_min(&timed_cb_queue_fhdef, &d->timers);
  }
  ddsrt_mutex_destroy(&d->lock);
  ddsrt_free(d);
}

dds_security_time_event_handle_t dds_security_timed_dispatcher_add(struct dds_security_timed_dispatcher *d, dds_security_timed_cb_t cb, dds_time_t trigger_time, void *arg)
{
  struct dds_security_timed_event *ev;
  dds_duration_t delta;

  delta = trigger_time - dds_time();

  ddsrt_mutex_lock(&d->lock);
  ev = timed_event_new(d->next_timer, cb, trigger_time, arg);
  ddsrt_avl_insert(&timed_event_treedef, &d->events, ev);
  ddsrt_fibheap_insert(&timed_cb_queue_fhdef, &d->timers, ev);
  d->next_timer++;
  if (d->evt != NULL)
  {
    ddsrt_mtime_t tsched = ddsrt_mtime_add_duration(ddsrt_time_monotonic(), delta < CHECK_TIMER_INTERVAL ? delta : CHECK_TIMER_INTERVAL);
    (void)resched_xevent_if_earlier(d->evt, tsched);
  }
  ddsrt_mutex_unlock(&d->lock);

  return ev->handle;
}

void dds_security_timed_dispatcher_remove(struct dds_security_timed_dispatcher *d, dds_security_time_event_handle_t timer)
{
  bool remove = false;
  struct dds_security_timed_event *ev;
  ddsrt_avl_dpath_t dpath;

  ddsrt_mutex_lock(&d->lock);
  ev = ddsrt_avl_lookup_dpath(&timed_event_treedef, &d->events, &timer, &dpath);
  if (ev)
  {
    ddsrt_avl_delete_dpath(&timed_event_treedef, &d->events, ev, &dpath);
    ddsrt_fibheap_delete(&timed_cb_queue_fhdef, &d->timers, ev);
    remove = true;
  }
  ddsrt_mutex_unlock(&d->lock);

  if (remove)
  {
    ev->callback(ev->handle, ev->trigger_time, DDS_SECURITY_TIMED_CB_KIND_DELETE, ev->arg);
    ddsrt_free(ev);
  }
}
