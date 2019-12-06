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
#include "dds/security/core/dds_security_fsm.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/timeconv.h"
#include <stdbool.h>

typedef struct dds_security_fsm {
  const dds_security_fsm_transition *transitions;
  int size;
  void *arg;
  const dds_security_fsm_state *current;
  dds_time_t current_state_endtime;
  ddsrt_atomic_uint32_t ref_cnt;
  dds_security_fsm_debug debug_func;
  struct dds_security_fsm_context *context;

  struct dds_security_fsm *next;
  struct dds_security_fsm *prev;
} dds_security_fsm;

typedef struct fsm_event {
  struct dds_security_fsm *fsm;
  int event_id;
  struct fsm_event *next;
  struct fsm_event *prev;
} fsm_event;

typedef struct fsm_state_timeout {
  struct dds_security_fsm *fsm;
  dds_time_t endtime;
} fsm_state_timeout;

typedef struct fsm_overall_timeout {
  struct dds_security_fsm *fsm;
  dds_time_t endtime;
  dds_security_fsm_action func;
  struct fsm_overall_timeout *next;
  struct fsm_overall_timeout *prev;
} fsm_overall_timeout;

typedef struct dds_security_fsm_context {
  ddsrt_thread_t fsm_tid;
  ddsrt_thread_t fsm_timeout_tid;
  bool fsm_teardown;
  fsm_event *fsm_queue;
  fsm_overall_timeout *fsm_overall_timeouts;
  ddsrt_mutex_t fsm_fsms_mutex;
  dds_security_fsm *fsm_fsms;
  fsm_state_timeout *fsm_next_state_timeout;
  ddsrt_mutex_t fsm_state_timeout_mutex;
  ddsrt_cond_t fsm_event_cond;
  ddsrt_mutex_t fsm_event_cond_mutex;

  // Overall timeout guard
  ddsrt_cond_t fsm_overall_timeout_cond;
  ddsrt_mutex_t fsm_overall_timeout_cond_mutex;
} dds_security_fsm_context;

static dds_security_fsm_context *fsm_context = NULL;

// Thread safe initialization of the Generic State Machine Utility
bool dds_security_fsm_initialized = false;
static ddsrt_atomic_uint32_t _fsmInitCount = DDSRT_ATOMIC_UINT32_INIT(0);

static void fsm_dispatch(struct dds_security_fsm *fsm, int event_id, int lifo) {
  fsm_event *event;
  dds_security_fsm_context *context;

  assert(fsm);

  if (fsm->size < 0) {
    /* This fsm is cleaned up (but probably not freed yet).
     * So, ignore the new event. */
    return;
  }

  context = fsm->context;
  assert(context);

  if (fsm->debug_func) {
    fsm->debug_func(fsm,
        lifo ? DDS_SECURITY_FSM_DEBUG_ACT_DISPATCH_DIRECT : DDS_SECURITY_FSM_DEBUG_ACT_DISPATCH,
        fsm->current, event_id, fsm->arg);
  }

  event = ddsrt_malloc(sizeof(fsm_event));
  event->fsm = fsm;
  event->event_id = event_id;
  event->next = NULL;
  event->prev = NULL;

  if (lifo) {
    /* Insert event at the top of the event list */
    if (context->fsm_queue) {
      context->fsm_queue->prev = event;
    }
    event->next = context->fsm_queue;
    context->fsm_queue = event;
  } else {
    /* Insert FIFO event */
    if (context->fsm_queue) {
      fsm_event *last = context->fsm_queue;
      while (last->next != NULL ) {
        last = last->next;
      }
      last->next = event;
      event->prev = last;
    } else {
      context->fsm_queue = event;
    }
  }
}

static void fsm_set_next_state_timeout(dds_security_fsm_context *context,
    dds_security_fsm *ignore) {
  dds_security_fsm *fsm;

  ddsrt_mutex_lock(&context->fsm_event_cond_mutex);

  // reset the current time
  context->fsm_next_state_timeout->endtime = DDS_NEVER;
  context->fsm_next_state_timeout->fsm = NULL;

  fsm = context->fsm_fsms;
  while (fsm) {
    if ((fsm->current) && (fsm->current->timeout) && (fsm != ignore)) {
      // first set the endtime of this state (if not set)
      if (fsm->current_state_endtime == 0) {
        fsm->current_state_endtime = ddsrt_time_add_duration(dds_time(),
            fsm->current->timeout);
      }
      // Initialize the current endtime
      if (context->fsm_next_state_timeout->fsm == NULL) {
        context->fsm_next_state_timeout->endtime = fsm->current_state_endtime;
        context->fsm_next_state_timeout->fsm = fsm;
      } else if (fsm->current_state_endtime
          < context->fsm_next_state_timeout->endtime) {
        context->fsm_next_state_timeout->endtime = fsm->current_state_endtime;
        context->fsm_next_state_timeout->fsm = fsm;
      }
    }
    fsm = fsm->next;
  }

  ddsrt_mutex_unlock(&context->fsm_event_cond_mutex);
}

static void fsm_state_change(fsm_event *event) {
  dds_security_fsm *fsm = event->fsm;
  dds_security_fsm_context *context = fsm->context;
  int event_id = event->event_id;
  int i, j;

  if (fsm->debug_func) {
    fsm->debug_func(fsm, DDS_SECURITY_FSM_DEBUG_ACT_HANDLING, fsm->current, event_id,
        fsm->arg);
  }

  for (i = 0; !context->fsm_teardown && i < fsm->size; i++) {
    if ((fsm->transitions[i].begin == fsm->current)
        && (fsm->transitions[i].event_id == event_id)) {
      /* Transition. */
      if (fsm->transitions[i].func) {
        fsm->transitions[i].func(fsm, fsm->arg);
      }
      /* New state. */
      fsm->current = fsm->transitions[i].end;
      if (fsm->current) {
        if (fsm->current->func) {
          fsm->current->func(fsm, fsm->arg);
        }
        /* Reset timeout. */
        fsm->current_state_endtime = ddsrt_time_add_duration(dds_time(),
            fsm->current->timeout);
        /* Check if an auto transition is to be dispatched */
        for (j = 0; j < fsm->size; j++) {
          if ((fsm->transitions[j].begin == fsm->current)
              && (fsm->transitions[j].event_id == DDS_SECURITY_FSM_EVENT_AUTO)) {
            dds_security_fsm_dispatch_direct(fsm, DDS_SECURITY_FSM_EVENT_AUTO);
          }
        }
      }
    }
  }
}

static uint32_t
fsm_thread(void *a) {
  dds_security_fsm_context *context = a;
  dds_duration_t dur_to_wait;
  dds_time_t now = DDS_TIME_INVALID;
  fsm_event *event;

  while (!context->fsm_teardown) {
    event = NULL;

    ddsrt_mutex_lock(&context->fsm_event_cond_mutex);
    if (!context->fsm_queue) {
      if (context->fsm_next_state_timeout->endtime == DDS_NEVER) {
        dur_to_wait = DDS_NEVER;
      } else {
        now = dds_time();
        dur_to_wait = context->fsm_next_state_timeout->endtime - now;
      }
      if (dur_to_wait > 0) {
        if (ddsrt_cond_waitfor(&context->fsm_event_cond,
            &context->fsm_event_cond_mutex, dur_to_wait) == false) {
          if (context->fsm_next_state_timeout->fsm) {
            /* Next timeout could have changed. */
            if (context->fsm_next_state_timeout->endtime != DDS_NEVER
                && (context->fsm_next_state_timeout->endtime - now <= 0)) {
              fsm_dispatch(context->fsm_next_state_timeout->fsm,
                  DDS_SECURITY_FSM_EVENT_TIMEOUT, 1);
            }
          }
        }
      } else {
        if (context->fsm_next_state_timeout->fsm) {
          fsm_dispatch(context->fsm_next_state_timeout->fsm,
              DDS_SECURITY_FSM_EVENT_TIMEOUT, 1);
        }
      }
    } else {
      event = context->fsm_queue;
      context->fsm_queue = context->fsm_queue->next;
      if (context->fsm_queue) {
        context->fsm_queue->prev = NULL;
      }
      ddsrt_atomic_inc32(&(event->fsm->ref_cnt));
    }
    ddsrt_mutex_unlock(&context->fsm_event_cond_mutex);

    if (event) {
      fsm_state_change(event);
      if (ddsrt_atomic_dec32_nv(&(event->fsm->ref_cnt)) == 0) {
        ddsrt_free(event->fsm);
      }
      ddsrt_free(event);
    }
    fsm_set_next_state_timeout(context, NULL);
  }
  return 0;
}

static fsm_overall_timeout *
fsm_get_first_overall_timeout(dds_security_fsm_context *context) {
  fsm_overall_timeout *timeout;
  fsm_overall_timeout *first_timeout;
  dds_time_t first_time = DDS_NEVER;

  timeout = context->fsm_overall_timeouts;
  first_timeout = context->fsm_overall_timeouts;
  while (timeout) {
    if (timeout->endtime < first_time) {
      first_time = timeout->endtime;
      first_timeout = timeout;
    }
    timeout = timeout->next;
  }

  return first_timeout;
}

static void fsm_remove_overall_timeout_from_list(dds_security_fsm_context *context,
    fsm_overall_timeout *timeout) {
  fsm_overall_timeout *tmp_next_timeout;
  fsm_overall_timeout *tmp_prev_timeout;

  if (timeout) {

    tmp_next_timeout = timeout->next;
    tmp_prev_timeout = timeout->prev;
    if (tmp_prev_timeout) {
      tmp_prev_timeout->next = tmp_next_timeout;
    }
    if (tmp_next_timeout) {
      tmp_next_timeout->prev = tmp_prev_timeout;
    }

    if (timeout == context->fsm_overall_timeouts) {
      context->fsm_overall_timeouts = tmp_next_timeout;
    }

    ddsrt_free(timeout);
    timeout = NULL;
  }
}

static uint32_t
fsm_run_timeout(void *arg) {
  dds_security_fsm_context *context = arg;
  dds_return_t result;
  fsm_overall_timeout *to;
  dds_time_t time_to_wait;
  dds_time_t now;

  while (!context->fsm_teardown) {
    ddsrt_mutex_lock(&context->fsm_overall_timeout_cond_mutex);
    to = fsm_get_first_overall_timeout(context);
    if (to) {
      struct dds_security_fsm *fsm = to->fsm;
      ddsrt_atomic_inc32(&(fsm->ref_cnt));

      result = DDS_RETCODE_TIMEOUT;
      now = dds_time();
      if (to->endtime > now) {
        time_to_wait = to->endtime - now;
        result = ddsrt_cond_waitfor(&context->fsm_overall_timeout_cond,
            &context->fsm_overall_timeout_cond_mutex, time_to_wait);
      }

      if (result == DDS_RETCODE_TIMEOUT) {
        /* Prevent calling timeout when the fsm has been cleaned. */
        dds_security_fsm_action func = to->func;
        fsm_remove_overall_timeout_from_list(context, to);
        if (fsm->size > 0) {
          ddsrt_mutex_unlock(&context->fsm_overall_timeout_cond_mutex);
          func(fsm, fsm->arg);
          ddsrt_mutex_lock(&context->fsm_overall_timeout_cond_mutex);
        }
      }

      if (ddsrt_atomic_dec32_nv(&(fsm->ref_cnt)) == 0) {
        ddsrt_free(fsm);
      }
    } else {
      ddsrt_cond_wait(&context->fsm_overall_timeout_cond,
          &context->fsm_overall_timeout_cond_mutex);
    }
    ddsrt_mutex_unlock(&context->fsm_overall_timeout_cond_mutex);
  }
  return 0;
}

static void fsm_remove_fsm_list(dds_security_fsm *fsm) {
  dds_security_fsm_context *context;
  dds_security_fsm *tmp_next_fsm;
  dds_security_fsm *tmp_prev_fsm;

  if (fsm) {
    context = fsm->context;

    ddsrt_mutex_lock(&context->fsm_fsms_mutex);
    tmp_next_fsm = fsm->next;
    tmp_prev_fsm = fsm->prev;
    if (tmp_prev_fsm) {
      tmp_prev_fsm->next = tmp_next_fsm;
    }
    if (tmp_next_fsm) {
      tmp_next_fsm->prev = tmp_prev_fsm;
    }
    if (fsm == context->fsm_fsms) {
      context->fsm_fsms = tmp_next_fsm;
    }
    ddsrt_mutex_unlock(&context->fsm_fsms_mutex);

    ddsrt_mutex_lock(&context->fsm_overall_timeout_cond_mutex);
    ddsrt_cond_signal(&context->fsm_overall_timeout_cond);
    ddsrt_mutex_unlock(&context->fsm_overall_timeout_cond_mutex);
  }
}

static ddsrt_thread_t fsm_thread_create( const char *name,
    ddsrt_thread_routine_t f, void *arg) {
  ddsrt_thread_t tid;
  ddsrt_threadattr_t threadAttr;

  ddsrt_threadattr_init(&threadAttr);
  if (ddsrt_thread_create(&tid, name, &threadAttr, f, arg) != DDS_RETCODE_OK) {
    memset(&tid, 0, sizeof(ddsrt_thread_t));
  }
  return tid;
}
#ifdef  AT_PROC_EXIT_IMPLEMENTED
static void fsm_thread_destroy( ddsrt_thread_t tid) {
  uint32_t thread_result;

  (void) ddsrt_thread_join( tid, &thread_result);
}
#endif

struct dds_security_fsm_context *
dds_security_fsm_context_create( dds_security_fsm_thread_create_func thr_create_func) {
  struct dds_security_fsm_context *context;

  context = ddsrt_malloc(sizeof(*context));

  context->fsm_next_state_timeout = ddsrt_malloc(sizeof(fsm_state_timeout));
  context->fsm_next_state_timeout->endtime = DDS_NEVER;
  context->fsm_next_state_timeout->fsm = NULL;

  context->fsm_teardown = false;
  context->fsm_queue = NULL;
  context->fsm_overall_timeouts = NULL;
  context->fsm_fsms = NULL;

  (void) ddsrt_mutex_init( &context->fsm_fsms_mutex );

  // Overall timeout guard
  (void) ddsrt_mutex_init( &context->fsm_overall_timeout_cond_mutex );
  (void) ddsrt_cond_init( &context->fsm_overall_timeout_cond );

  // State timeouts
  (void) ddsrt_mutex_init(&context->fsm_state_timeout_mutex );

  // Events
  (void) ddsrt_mutex_init(&context->fsm_event_cond_mutex );
  (void) ddsrt_cond_init(&context->fsm_event_cond );

  context->fsm_tid = thr_create_func( "dds_security_fsm", fsm_thread, context);
  context->fsm_timeout_tid = thr_create_func( "dds_security_fsm_timeout",
      fsm_run_timeout, context);

  return context;
}

void dds_security_fsm_context_destroy(dds_security_fsm_context *context,
    dds_security_fsm_thread_destroy_func thr_destroy_func) {
  if (context) {
    context->fsm_teardown = true;

    ddsrt_mutex_lock( &context->fsm_overall_timeout_cond_mutex);
    ddsrt_cond_signal( &context->fsm_overall_timeout_cond);
    ddsrt_mutex_unlock( &context->fsm_overall_timeout_cond_mutex);

    ddsrt_mutex_lock(&context->fsm_event_cond_mutex);
    ddsrt_cond_signal(&context->fsm_event_cond);
    ddsrt_mutex_unlock(&context->fsm_event_cond_mutex);

    thr_destroy_func( context->fsm_tid);
    ddsrt_mutex_destroy(&context->fsm_event_cond_mutex);
    ddsrt_cond_destroy(&context->fsm_event_cond);

    thr_destroy_func( context->fsm_timeout_tid);
    ddsrt_mutex_destroy(&context->fsm_fsms_mutex);
    ddsrt_mutex_destroy(&context->fsm_overall_timeout_cond_mutex);
    ddsrt_cond_destroy(&context->fsm_overall_timeout_cond);

    ddsrt_free(context->fsm_next_state_timeout);
  }
}
#ifdef  AT_PROC_EXIT_IMPLEMENTED
static void fsm_fini(void) {
  dds_security_fsm_context_destroy(fsm_context, NULL, fsm_thread_destroy);

  /* os_osExit(); ???? */
}

#endif
static bool fsm_init_once(void) {
  bool ret = true;
  uint32_t initCount;

  initCount = ddsrt_atomic_inc32_nv(&_fsmInitCount);

  if (initCount == 1) {
    assert( dds_security_fsm_initialized == false );

    /* ddsrt_osInit(); ??? */

    fsm_context = dds_security_fsm_context_create( fsm_thread_create);

    if (fsm_context) {
      /* os_procAtExit( fsm_fini ); ??? */
      dds_security_fsm_initialized = true;
    } else {
      ret = false;
    }
  } else {
    if (dds_security_fsm_initialized == false) {
      /* Another thread is currently initializing the fsm. Since
       * both results (osr_fsm and osr_timeout) should be ddsrt_resultSuccess
       * a sleep is performed, to ensure that (if succeeded) successive
       * init calls will also actually pass.
       */
      dds_sleepfor( DDS_MSECS( 100 ));
    }
    if (dds_security_fsm_initialized == false) {
      /* Initialization did not succeed, undo increment and return error */
      initCount = ddsrt_atomic_dec32_nv(&_fsmInitCount);
      ret = false;
    }
  }
  return ret;
}

static int /* 1 = ok, other = error */
fsm_validate(const dds_security_fsm_transition *transitions, int size) {
  int i;

  for (i = 0; i < size; i++) {
    /* It needs to have a start. */
    if ((transitions[i].begin == NULL )
        && (transitions[i].event_id == DDS_SECURITY_FSM_EVENT_AUTO)) {
      return 1;
    }
  }

  return 0;
}

struct dds_security_fsm *
dds_security_fsm_create(struct dds_security_fsm_context *context,
    const dds_security_fsm_transition *transitions, int size, void *arg) {
  struct dds_security_fsm* fsm = NULL;
  struct dds_security_fsm_context *ctx = NULL;

  assert(transitions);
  assert(size > 0);

  if (context == NULL) {
    if (fsm_init_once()) {
      ctx = fsm_context;
    }
  } else {
    ctx = context;
  }

  if (ctx) {
    if (fsm_validate(transitions, size) == 1) {
      fsm = ddsrt_malloc(sizeof(struct dds_security_fsm));
      fsm->transitions = transitions;
      fsm->size = size;
      fsm->arg = arg;
      fsm->current = NULL;
      fsm->debug_func = NULL;
      fsm->next = NULL;
      fsm->prev = NULL;
      fsm->context = ctx;
      ddsrt_atomic_st32( &fsm->ref_cnt, 1 );
      fsm->current_state_endtime = 0;

      ddsrt_mutex_lock(&fsm->context->fsm_fsms_mutex);
      if (fsm->context->fsm_fsms) {
        dds_security_fsm *last = fsm->context->fsm_fsms;
        while (last->next != NULL ) {
          last = last->next;
        }
        last->next = fsm;
        fsm->prev = last;
      } else {
        fsm->context->fsm_fsms = fsm;
      }
      ddsrt_mutex_unlock(&fsm->context->fsm_fsms_mutex);
    }
  }
  return fsm;
}

void dds_security_fsm_start(struct dds_security_fsm *fsm) {
  assert(fsm);
  dds_security_fsm_dispatch(fsm, DDS_SECURITY_FSM_EVENT_AUTO);
}

void dds_security_fsm_set_timeout(struct dds_security_fsm *fsm, dds_security_fsm_action func,
    dds_time_t timeout) {
  fsm_overall_timeout *to;
  dds_security_fsm_context *context;

  assert(fsm);

  context = fsm->context;
  assert(context);

  to = ddsrt_malloc(sizeof(fsm_overall_timeout));
  to->fsm = fsm;
  to->func = func;
  to->endtime = ddsrt_time_add_duration( dds_time(), timeout);
  to->next = NULL;
  to->prev = NULL;

  ddsrt_mutex_lock(&context->fsm_overall_timeout_cond_mutex);
  if (context->fsm_overall_timeouts) {
    fsm_overall_timeout *last = context->fsm_overall_timeouts;
    while (last->next != NULL ) {
      last = last->next;
    }
    last->next = to;
    to->prev = last;
  } else {
    context->fsm_overall_timeouts = to;
  }
  ddsrt_cond_signal(&context->fsm_overall_timeout_cond);
  ddsrt_mutex_unlock(&context->fsm_overall_timeout_cond_mutex);
}

void dds_security_fsm_set_debug(struct dds_security_fsm *fsm, dds_security_fsm_debug func) {
  dds_security_fsm_context *context;

  assert(fsm);

  context = fsm->context;
  assert(context);

  ddsrt_mutex_lock(&context->fsm_overall_timeout_cond_mutex);
  fsm->debug_func = func;
  ddsrt_mutex_unlock(&context->fsm_overall_timeout_cond_mutex);
}

void dds_security_fsm_dispatch(struct dds_security_fsm *fsm, int32_t event_id) {
  dds_security_fsm_context *context;

  assert(fsm);

  context = fsm->context;
  assert(context);

  ddsrt_mutex_lock(&context->fsm_event_cond_mutex);
  fsm_dispatch(fsm, event_id, 0);
  ddsrt_cond_signal(&context->fsm_event_cond);
  ddsrt_mutex_unlock(&context->fsm_event_cond_mutex);
}

void dds_security_fsm_dispatch_direct(struct dds_security_fsm *fsm, int32_t event_id) {
  dds_security_fsm_context *context;

  assert(fsm);

  context = fsm->context;
  assert(context);

  ddsrt_mutex_lock(&context->fsm_event_cond_mutex);
  fsm_dispatch(fsm, event_id, 1);
  ddsrt_cond_signal(&context->fsm_event_cond);
  ddsrt_mutex_unlock(&context->fsm_event_cond_mutex);
}

const dds_security_fsm_state*
dds_security_fsm_current_state(struct dds_security_fsm *fsm) {
  assert(fsm);
  return fsm->current;
}

void dds_security_fsm_cleanup(struct dds_security_fsm *fsm) {
  dds_security_fsm_context *context;
  fsm_event *event;
  fsm_event *tmp_prev_event;
  fsm_event *tmp_next_event;
  fsm_overall_timeout *timeout;

  assert(fsm);

  context = fsm->context;
  assert(context);

  // Signal the timeout thread.
  // First hold to lock to the overall timeout list
  // so that the next timeout can't be determined until
  // we've done removing the overall timeout of this fsm

  // Signal the thread so that it's not using timeout structs
  ddsrt_mutex_lock(&context->fsm_overall_timeout_cond_mutex);
  ddsrt_cond_signal(&context->fsm_overall_timeout_cond);

  timeout = context->fsm_overall_timeouts;

  // Search the overall timeout of this fsm
  while (timeout) {
    if (timeout->fsm == fsm) {
      break;
    }
    timeout = timeout->next;
  }
  fsm_remove_overall_timeout_from_list(context, timeout);
  ddsrt_mutex_unlock(&context->fsm_overall_timeout_cond_mutex);

  /* The current fsm could be the one that would trigger a possible timeout.
   * Reset the state timeout and make sure it's not the current fsm. */
  fsm_set_next_state_timeout(context, fsm);

  /* Now, remove all possible events from the queue related to the fsm. */
  ddsrt_mutex_lock(&context->fsm_event_cond_mutex);
  event = context->fsm_queue;
  while (event) {
    if (event->fsm == fsm) {
      tmp_next_event = event->next;
      tmp_prev_event = event->prev;
      if (tmp_prev_event) {
        tmp_prev_event->next = tmp_next_event;
      }
      if (tmp_next_event) {
        tmp_next_event->prev = tmp_prev_event;
      }
      if (event == context->fsm_queue) {
        context->fsm_queue = tmp_next_event;
      }
      ddsrt_free(event);
      event = tmp_next_event;
    } else {
      event = event->next;
    }
  }
  ddsrt_cond_signal(&context->fsm_event_cond);
  ddsrt_mutex_unlock(&context->fsm_event_cond_mutex);
}

void dds_security_fsm_free(struct dds_security_fsm *fsm) {
  ddsrt_tid_t self = ddsrt_gettid_for_thread( ddsrt_thread_self() );
  dds_security_fsm_context *context;

  assert(fsm);

  context = fsm->context;
  assert(context);

  /* Indicate termination. */
  fsm->size = -1;

  /* Cleanup stuff. */
  dds_security_fsm_cleanup(fsm);
  fsm_remove_fsm_list(fsm);

  /* Is this being freed from the FSM context? */
  if ((self ==  ddsrt_gettid_for_thread( context->fsm_tid ) )
      || (self ==  ddsrt_gettid_for_thread( context->fsm_timeout_tid ) ) ) {
    /* Yes.
     * Just reduce the reference count and let the garbage collection be
     * done by the FSM context after event handling. */
    ddsrt_atomic_dec32(&(fsm->ref_cnt));
  } else {
    /* No.
     * Block the outside thread until a possible concurrent event
     * has being handled. */
    while (ddsrt_atomic_ld32( &(fsm->ref_cnt)) > 1) {
      /* Currently, an event is still being handled for this FSM. */
      dds_sleepfor( 10 * DDS_NSECS_IN_MSEC );
    }
    /* We have the only reference, so it's safe to free the FSM. */
    ddsrt_free(fsm);
  }
}
