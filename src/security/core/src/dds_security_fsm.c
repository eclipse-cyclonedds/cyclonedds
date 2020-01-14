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
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/timeconv.h"
#include "dds/ddsrt/fibheap.h"
#include "dds/ddsi/q_thread.h"
#include "dds/security/core/dds_security_fsm.h"


struct fsm_event
{
  struct dds_security_fsm *fsm;
  int event_id;
  struct fsm_event *next;
  struct fsm_event *prev;
};

typedef enum fsm_timeout_kind {
  FSM_TIMEOUT_STATE,
  FSM_TIMEOUT_OVERALL
} fsm_timeout_kind_t;

struct fsm_timer_event
{
  ddsrt_fibheap_node_t heapnode;
  struct dds_security_fsm *fsm;
  fsm_timeout_kind_t kind;
  dds_time_t endtime;
};

struct dds_security_fsm
{
  struct dds_security_fsm *next_fsm;
  struct dds_security_fsm *prev_fsm;
  bool active;
  struct dds_security_fsm_control *control;
  const dds_security_fsm_transition *transitions;
  uint32_t size;
  void *arg;
  const dds_security_fsm_state *current;
  struct fsm_timer_event state_timeout_event;
  struct fsm_timer_event overall_timeout_event;
  dds_security_fsm_action overall_timeout_action;
  dds_security_fsm_debug debug_func;
};

struct dds_security_fsm_control
{
  ddsrt_mutex_t lock;
  ddsrt_cond_t cond;
  struct thread_state1 *ts;
  struct q_globals *gv;
  struct dds_security_fsm *first_fsm;
  struct dds_security_fsm *last_fsm;
  struct fsm_event *event_queue;
  ddsrt_fibheap_t timers;
  bool running;
};

static int compare_timer_event (const void *va, const void *vb);
static void fsm_delete (struct dds_security_fsm_control *control, struct dds_security_fsm *fsm);

const ddsrt_fibheap_def_t timer_events_fhdef = DDSRT_FIBHEAPDEF_INITIALIZER(offsetof (struct fsm_timer_event, heapnode), compare_timer_event);

static int compare_timer_event (const void *va, const void *vb)
{
  const struct fsm_timer_event *a = va;
  const struct fsm_timer_event *b = vb;
  return (a->endtime == b->endtime) ? 0 : (a->endtime < b->endtime) ? -1 : 1;
}

static void fsm_dispatch (struct dds_security_fsm *fsm, int event_id, bool lifo)
{
  struct dds_security_fsm_control *control = fsm->control;
  struct fsm_event *event;

  if (fsm->debug_func) {
    fsm->debug_func(fsm,
        lifo ? DDS_SECURITY_FSM_DEBUG_ACT_DISPATCH_DIRECT : DDS_SECURITY_FSM_DEBUG_ACT_DISPATCH,
        fsm->current, event_id, fsm->arg);
  }

  event = ddsrt_malloc (sizeof(struct fsm_event));
  event->fsm = fsm;
  event->event_id = event_id;
  event->next = NULL;
  event->prev = NULL;

  if (lifo) {
    /* Insert event at the top of the event list */
    if (control->event_queue) {
       control->event_queue->prev = event;
    }
    event->next = control->event_queue;
    control->event_queue = event;
  } else {
    /* Insert FIFO event */
    if (control->event_queue) {
      struct fsm_event *last = control->event_queue;
      while (last->next != NULL ) {
        last = last->next;
      }
      last->next = event;
      event->prev = last;
    } else {
      control->event_queue = event;
    }
  }
}

static void set_state_timer (struct dds_security_fsm *fsm)
{
  struct dds_security_fsm_control *control = fsm->control;

  if (fsm->current && fsm->current->timeout > 0 && fsm->current->timeout != DDS_NEVER)
  {
    fsm->state_timeout_event.endtime = ddsrt_time_add_duration (dds_time(), fsm->current->timeout);
    ddsrt_fibheap_insert (&timer_events_fhdef, &control->timers, &fsm->state_timeout_event);
  }
  else
    fsm->state_timeout_event.endtime = DDS_NEVER;
}

static void clear_state_timer (struct dds_security_fsm *fsm)
{
  struct dds_security_fsm_control *control = fsm->control;

  if (fsm->current && fsm->state_timeout_event.endtime != DDS_NEVER)
    ddsrt_fibheap_delete (&timer_events_fhdef, &control->timers, &fsm->state_timeout_event);
}

static void clear_overall_timer (struct dds_security_fsm *fsm)
{
  struct dds_security_fsm_control *control = fsm->control;

  if (fsm->current && fsm->overall_timeout_event.endtime != DDS_NEVER)
    ddsrt_fibheap_delete (&timer_events_fhdef, &control->timers, &fsm->overall_timeout_event);
}

static dds_time_t first_timeout (struct dds_security_fsm_control *control)
{
  struct fsm_timer_event *min;
  if ((min = ddsrt_fibheap_min (&timer_events_fhdef, &control->timers)) != NULL)
    return min->endtime;
  return DDS_NEVER;
}

static void fsm_check_auto_state_change (struct dds_security_fsm *fsm)
{
  if (fsm->current)
  {
    uint32_t i;

    for (i = 0; i < fsm->size; i++)
    {
      if (fsm->transitions[i].begin == fsm->current && fsm->transitions[i].event_id == DDS_SECURITY_FSM_EVENT_AUTO)
      {
        fsm_dispatch (fsm, DDS_SECURITY_FSM_EVENT_AUTO, true);
        break;
      }
    }
  }
}

static void fsm_state_change (struct dds_security_fsm_control *control, struct fsm_event *event)
{
  struct dds_security_fsm *fsm = event->fsm;
  int event_id = event->event_id;
  uint32_t i;

  if (fsm->active)
  {
    if (fsm->debug_func)
      fsm->debug_func (fsm, DDS_SECURITY_FSM_DEBUG_ACT_HANDLING, fsm->current, event_id, fsm->arg);

    for (i = 0; i < fsm->size; i++)
    {
      if ((fsm->transitions[i].begin == fsm->current) && (fsm->transitions[i].event_id == event_id))
      {
        clear_state_timer (fsm);
        fsm->current = fsm->transitions[i].end;
        set_state_timer (fsm);

        ddsrt_mutex_unlock (&control->lock);
        if (fsm->transitions[i].func)
          fsm->transitions[i].func (fsm, fsm->arg);
        if (fsm->current && fsm->current->func)
          fsm->current->func (fsm, fsm->arg);
        ddsrt_mutex_lock (&control->lock);
        fsm_check_auto_state_change (fsm);
        break;
      }
    }
  }
  else if (event_id == DDS_SECURITY_FSM_EVENT_DELETE)
    fsm_delete (control, fsm);

}

static void fsm_handle_timeout (struct dds_security_fsm_control *control, struct fsm_timer_event *timer_event)
{
  struct dds_security_fsm *fsm = timer_event->fsm;

  if (fsm->active)
  {
    switch (timer_event->kind)
    {
    case FSM_TIMEOUT_STATE:
      fsm_dispatch (fsm, DDS_SECURITY_FSM_EVENT_TIMEOUT, true);
      break;
    case FSM_TIMEOUT_OVERALL:
      ddsrt_mutex_unlock (&control->lock);
      if (fsm->overall_timeout_action)
        fsm->overall_timeout_action (fsm, fsm->arg);
      ddsrt_mutex_lock (&control->lock);
      break;
    }
  }

  /* mark timer event as being processed */
  timer_event->endtime = DDS_NEVER;
}

static uint32_t handle_events (struct dds_security_fsm_control *control)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();

  ddsrt_mutex_lock (&control->lock);
  thread_state_awake (ts1, control->gv);
  while (control->running)
  {
    if (control->event_queue)
    {
      struct fsm_event *event = control->event_queue;

      control->event_queue = event->next;
      if (control->event_queue)
        control->event_queue->prev = NULL;
      fsm_state_change (control, event);
      ddsrt_free (event);
    }
    else
    {
      dds_time_t timeout = first_timeout (control);

      if (timeout > dds_time ())
      {
        thread_state_asleep (ts1);
        (void)ddsrt_cond_waituntil( &control->cond, &control->lock, timeout);
        thread_state_awake (ts1, control->gv);
      }
      else
      {
        struct fsm_timer_event *timer_event = ddsrt_fibheap_extract_min (&timer_events_fhdef, &control->timers);
        fsm_handle_timeout (control, timer_event);
      }
    }
  }
  thread_state_asleep (ts1);
  ddsrt_mutex_unlock (&control->lock);

  return 0;
}

void dds_security_fsm_set_timeout (struct dds_security_fsm *fsm, dds_security_fsm_action action, dds_duration_t timeout)
{
  assert(fsm);
  assert(fsm->control);
  assert(timeout > 0);

  ddsrt_mutex_lock (&fsm->control->lock);
  if (fsm->active)
  {
    if (timeout != DDS_NEVER)
    {
      clear_overall_timer(fsm);
      fsm->overall_timeout_action = action;
      fsm->overall_timeout_event.endtime = ddsrt_time_add_duration(dds_time(), timeout);
      ddsrt_fibheap_insert (&timer_events_fhdef, &fsm->control->timers, &fsm->overall_timeout_event);
      if (fsm->overall_timeout_event.endtime < first_timeout(fsm->control))
        ddsrt_cond_signal (&fsm->control->cond);
    }
    else
      clear_overall_timer (fsm);
  }
  ddsrt_mutex_unlock (&fsm->control->lock);
}

void dds_security_fsm_dispatch (struct dds_security_fsm *fsm, int32_t event_id, bool prio)
{
  assert(fsm);
  assert(fsm->control);

  ddsrt_mutex_lock (&fsm->control->lock);
  if (fsm->active)
  {
    fsm_dispatch (fsm, event_id, prio);
    ddsrt_cond_signal (&fsm->control->cond);
  }
  ddsrt_mutex_unlock (&fsm->control->lock);
}

const dds_security_fsm_state * dds_security_fsm_current_state (struct dds_security_fsm *fsm)
{
  const dds_security_fsm_state *state;

  assert(fsm);
  assert(fsm->active);

  ddsrt_mutex_lock (&fsm->control->lock);
  state = fsm->current;
  ddsrt_mutex_unlock (&fsm->control->lock);

  return state;
}

void dds_security_fsm_set_debug (struct dds_security_fsm *fsm, dds_security_fsm_debug func)
{
  assert(fsm);

  ddsrt_mutex_lock (&fsm->control->lock);
  fsm->debug_func = func;
  ddsrt_mutex_unlock (&fsm->control->lock);
}

static bool fsm_validate (const dds_security_fsm_transition *transitions, uint32_t size)
{
  uint32_t i;

  for (i = 0; i < size; i++)
  {
    /* It needs to have a start. */
    if (transitions[i].begin && transitions[i].event_id == DDS_SECURITY_FSM_EVENT_AUTO)
      return true;
  }
  return true;
}

static void add_fsm_to_list (struct dds_security_fsm_control *control, struct dds_security_fsm *fsm)
{
  fsm->next_fsm = NULL;
  fsm->prev_fsm = control->last_fsm;
  if (control->last_fsm)
  {
    assert(control->first_fsm != NULL);
    control->last_fsm->next_fsm = fsm;
  }
  else
  {
    assert(control->first_fsm == NULL);
    control->first_fsm = fsm;
  }
  control->last_fsm = fsm;
}

static void remove_fsm_from_list (struct dds_security_fsm_control *control, struct dds_security_fsm *fsm)
{
  if (fsm->prev_fsm)
    fsm->prev_fsm->next_fsm = fsm->next_fsm;
  else
    control->first_fsm = fsm->next_fsm;

  if (fsm->next_fsm)
    fsm->next_fsm->prev_fsm = fsm->prev_fsm;
  else
    control->last_fsm = fsm->prev_fsm;
}

struct dds_security_fsm * dds_security_fsm_create (struct dds_security_fsm_control *control, const dds_security_fsm_transition *transitions, uint32_t size, void *arg)
{
  struct dds_security_fsm *fsm = NULL;

  assert(control);
  assert(transitions);

  if (fsm_validate (transitions, size))
  {
    fsm = ddsrt_malloc (sizeof(struct dds_security_fsm));
    fsm->transitions = transitions;
    fsm->size = size;
    fsm->arg = arg;
    fsm->current = NULL;
    fsm->debug_func = NULL;
    fsm->overall_timeout_action = NULL;
    fsm->state_timeout_event.kind = FSM_TIMEOUT_STATE;
    fsm->state_timeout_event.endtime = DDS_NEVER;
    fsm->state_timeout_event.fsm = fsm;
    fsm->overall_timeout_event.kind = FSM_TIMEOUT_OVERALL;
    fsm->overall_timeout_event.endtime = DDS_NEVER;
    fsm->overall_timeout_event.fsm = fsm;
    fsm->active = true;
    fsm->next_fsm = NULL;
    fsm->prev_fsm = NULL;
    fsm->control = control;

    ddsrt_mutex_lock (&control->lock);
    add_fsm_to_list (control, fsm);
    ddsrt_mutex_unlock (&control->lock);
  }
  return fsm;
}

void
dds_security_fsm_start (struct dds_security_fsm *fsm)
{
  dds_security_fsm_dispatch(fsm, DDS_SECURITY_FSM_EVENT_AUTO, false);
}

static void fsm_deactivate (struct dds_security_fsm *fsm, bool gen_del_event)
{
  if (fsm->active)
  {
    fsm->active = false;
    clear_state_timer (fsm);
    clear_overall_timer (fsm);
    fsm->current = NULL;
    if (gen_del_event)
      fsm_dispatch (fsm, DDS_SECURITY_FSM_EVENT_DELETE, false);
  }
}

void dds_security_fsm_free (struct dds_security_fsm *fsm)
{
  struct dds_security_fsm_control *control;

  assert(fsm);
  assert(fsm->control);

  control = fsm->control;
  ddsrt_mutex_lock (&control->lock);
  fsm_deactivate (fsm, true);
  ddsrt_mutex_unlock (&control->lock);
}

static void fsm_delete (struct dds_security_fsm_control *control, struct dds_security_fsm *fsm)
{
  fsm_deactivate (fsm, false);
  remove_fsm_from_list (control, fsm);
  ddsrt_free(fsm);
}

struct dds_security_fsm_control * dds_security_fsm_control_create (struct q_globals *gv)
{
  struct dds_security_fsm_control *control;

  control = ddsrt_malloc (sizeof(*control));
  control->running = false;
  control->event_queue = NULL;
  control->first_fsm = NULL;
  control->last_fsm = NULL;
  control->gv = gv;
  ddsrt_mutex_init (&control->lock);
  ddsrt_cond_init (&control->cond);
  ddsrt_fibheap_init (&timer_events_fhdef, &control->timers);

  return control;
}

void dds_security_fsm_control_free (struct dds_security_fsm_control *control)
{
  struct dds_security_fsm *fsm;
  struct fsm_event *event;

  assert(control);
  assert(!control->running);

  while ((fsm = control->first_fsm) != NULL)
  {
    control->first_fsm = fsm->next_fsm;
    fsm_deactivate (fsm, false);
    ddsrt_free (fsm);
  }
  while ((event = control->event_queue) != NULL)
  {
    control->event_queue = event->next;
    ddsrt_free (event);
  }

  ddsrt_cond_destroy (&control->cond);
  ddsrt_mutex_destroy (&control->lock);
  ddsrt_free (control);
}

dds_return_t dds_security_fsm_control_start (struct dds_security_fsm_control *control, const char *name)
{
  dds_return_t rc;
  const char *fsm_name = name ? name : "fsm";

  assert(control);

  control->running = true;
  rc = create_thread (&control->ts, control->gv, fsm_name, (uint32_t (*) (void *)) handle_events, control);

  return rc;
}

void dds_security_fsm_control_stop (struct dds_security_fsm_control *control)
{
  assert(control);
  assert(control->running);

  ddsrt_mutex_lock (&control->lock);
  control->running = false;
  ddsrt_cond_signal (&control->cond);
  ddsrt_mutex_unlock (&control->lock);

  join_thread (control->ts);
  control->ts = NULL;
}
