// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <stdbool.h>
#include "CUnit/CUnit.h"
#include "CUnit/Test.h"

#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/misc.h"
#include "dds/dds.h"
#include "dds__types.h"
#include "dds__entity.h"
#include "dds/security/core/dds_security_fsm.h"

#define CHECK_BIT(var, pos) ((var) & (1<<(pos)))
#define FSM_AUTH_ARG 10

static dds_entity_t g_participant = 0;
static ddsrt_mutex_t g_lock;
static ddsrt_cond_t g_cond;
static struct dds_security_fsm_control *g_fsm_control = NULL;

#define DO_SIMPLE(name, var, bit) static void name(struct dds_security_fsm *fsm, void *arg) { \
  DDSRT_UNUSED_ARG(fsm); \
  DDSRT_UNUSED_ARG(arg); \
  printf("Transition %s\n", __FUNCTION__); \
  ddsrt_mutex_lock(&g_lock); \
  visited_##var |= 1u << (bit); \
  ddsrt_cond_broadcast(&g_cond); \
  ddsrt_mutex_unlock(&g_lock); \
}

static struct dds_security_fsm *fsm_auth;
static uint32_t visited_auth;
static uint32_t correct_fsm;
static uint32_t correct_arg;
static int validate_remote_identity_first;
static int begin_handshake_reply_first;
static bool in_handshake_init_message_wait;

static struct dds_security_fsm *fsm_test;
static uint32_t visited_test;
static int do_stuff_counter;
static int do_other_stuff_counter;

static struct dds_security_fsm *fsm_timeout;
static uint32_t visited_timeout;
static uint32_t correct_fsm_timeout;
static uint32_t correct_arg_timeout;

static struct ddsi_domaingv *get_entity_gv (dds_entity_t handle)
{
  struct dds_entity *e;
  dds_return_t rc = dds_entity_pin (handle, &e);
  CU_ASSERT_FATAL (rc == 0);
  struct ddsi_domaingv * const gv = &e->m_domain->gv;
  dds_entity_unpin (e);
  return gv;
}

static void fsm_control_init(void)
{
  g_participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL (g_participant > 0);
  ddsrt_mutex_init (&g_lock);
  ddsrt_cond_init (&g_cond);

  g_fsm_control = dds_security_fsm_control_create (get_entity_gv (g_participant));
  dds_return_t rc = dds_security_fsm_control_start (g_fsm_control, NULL);
  CU_ASSERT_EQUAL_FATAL (rc, 0);

  validate_remote_identity_first = 1;
  begin_handshake_reply_first = 1;
  visited_auth = 0;
  visited_test = 0;
  visited_timeout = 0;
  in_handshake_init_message_wait = false;
  do_stuff_counter = 0;
  do_other_stuff_counter = 0;
  visited_timeout = 0;
  correct_fsm_timeout = 0;
  correct_arg_timeout = 0;
}

static void fsm_control_fini (void)
{
  dds_security_fsm_control_stop (g_fsm_control);
  dds_security_fsm_control_free (g_fsm_control);
  ddsrt_mutex_destroy (&g_lock);
  ddsrt_cond_destroy (&g_cond);
  dds_delete (g_participant);
}

static void a (struct dds_security_fsm *fsm, void *arg)
{
  printf ("[%p] Transition %s\n", fsm, __FUNCTION__);
  ddsrt_mutex_lock (&g_lock);
  if (arg != NULL)
    correct_arg = *((int *) arg) == FSM_AUTH_ARG ? 1 : 0;
  correct_fsm = (fsm == fsm_auth) ? 1 : 0;
  visited_auth |= 1u << 0;
  ddsrt_cond_broadcast (&g_cond);
  ddsrt_mutex_unlock (&g_lock);
}

DO_SIMPLE (b, auth, 1)
DO_SIMPLE (c, auth, 2)
DO_SIMPLE (d, auth, 3)
DO_SIMPLE (e, auth, 4)
DO_SIMPLE (f, auth, 5)
DO_SIMPLE (g, auth, 6)
DO_SIMPLE (h, auth, 7)

typedef enum {
  VALIDATION_PENDING_RETRY,
  VALIDATION_FAILED,
  VALIDATION_OK,
  VALIDATION_OK_FINAL_MESSAGE,
  VALIDATION_PENDING_HANDSHAKE_MESSAGE,
  VALIDATION_PENDING_HANDSHAKE_REQUEST,
  plugin_ret_MAX
} plugin_ret;
#define SHM_MSG_RECEIVED (plugin_ret_MAX + 1)

static plugin_ret validate_remote_identity (void)
{
  printf ("validate_remote_identity - %d\n", validate_remote_identity_first);
  if (validate_remote_identity_first)
  {
    validate_remote_identity_first = 0;
    return VALIDATION_PENDING_RETRY;
  }
  return VALIDATION_PENDING_HANDSHAKE_MESSAGE;
}

static plugin_ret begin_handshake_reply (void)
{
  printf ("begin_handshake_reply - %d\n", begin_handshake_reply_first);
  if (begin_handshake_reply_first)
  {
    begin_handshake_reply_first = 0;
    return VALIDATION_PENDING_RETRY;
  }
  return VALIDATION_OK_FINAL_MESSAGE;
}

static plugin_ret get_shared_secret (void)
{
  return VALIDATION_OK;
}

/* State actions */
static void fsm_validate_remote_identity (struct dds_security_fsm *fsm, void *arg)
{
  DDSRT_UNUSED_ARG(arg);
  plugin_ret ret = validate_remote_identity ();
  printf ("[%p] State %s (ret %d)\n", fsm, __FUNCTION__, (int) ret);
  dds_security_fsm_dispatch (fsm, (int32_t) ret, false);
}

static void fsm_begin_handshake_reply (struct dds_security_fsm *fsm, void *arg)
{
  DDSRT_UNUSED_ARG (arg);
  plugin_ret ret = begin_handshake_reply ();
  if (ret == VALIDATION_OK_FINAL_MESSAGE)
    ret = get_shared_secret ();
  printf ("[%p] State %s (ret %d)\n", fsm, __FUNCTION__, (int) ret);
  dds_security_fsm_dispatch (fsm, (int32_t) ret, false);
}

static void on_handshake_init_message_wait (struct dds_security_fsm *fsm, void *arg)
{
  (void) fsm; (void) arg;
  ddsrt_mutex_lock (&g_lock);
  in_handshake_init_message_wait = true;
  ddsrt_cond_broadcast (&g_cond);
  ddsrt_mutex_unlock (&g_lock);
}

/* A few states from the handshake state-machine
 *
 * .--.
 * |##|--------------------------------------.
 * '--'       a()                            v
 *                 .----------------------------------------------------.
 *                 |            StateValidateRemoteIdentity             |
 *                 |----------------------------------------------------|
 *         .------>| fsm_validate_remote_identity()                     |
 *         |       |    - dispatch VALIDATION_PENDING_RETRY             |
 *    100ms|       |    - dispatch VALIDATION_PENDING_HANDSHAKE_MESSAGE |
 *    d()  |       '----------------------------------------------------'
 *         |          VALIDATION_PENDING_RETRY|  | VALIDATION_PENDING_HANDSHAKE_MESSAGE
 *         |          b()                     |  | c()
 *         |                                  |  |
 * .------------------------------.           |  |       .-------------------------------.
 * | StateValRemIdentityRetryWait |           |  |       | StateHandshakeInitMessageWait |
 * |------------------------------|<----------'  '------>|-------------------------------|
 * '------------------------------'                      '-------------------------------'
 *                                                SHM_MSG_RECEIVED  |
 *                                                e()               |
 *                                                                  v
 *                                      .----------------------------------------.
 *          VALIDATION_PENDING_RETRY    |        state_beginHandshakeReply        |
 *          f()                         |----------------------------------------|
 *            .-------------------------| fsm_begin_handshake_reply()            |
 *            |                         |    - dispatch VALIDATION_PENDING_RETRY |
 *            v                         |    - dispatch VALIDATION_OK            |
 * .-----------------------.    100ms   '----------------------------------------'
 * | state_beginHsReplyWait |    h()                ^        VALIDATION_OK |
 * |-----------------------|-----------------------'        g()           |
 * '-----------------------'                                              v
 *                                                                       .-.
 *                                                                       '-'
 */
static const dds_security_fsm_state StateValidateRemoteIdentity   = { fsm_validate_remote_identity, 0 };
static const dds_security_fsm_state StateValRemIdentityRetryWait  = { NULL, 100000000 };
static const dds_security_fsm_state StateHandshakeInitMessageWait = { on_handshake_init_message_wait, 0 };
static const dds_security_fsm_state state_beginHandshakeReply     = { fsm_begin_handshake_reply, 0 };
static const dds_security_fsm_state state_beginHsReplyWait        = { NULL, 100000000 };
static const dds_security_fsm_transition HandshakeTransistions[] = {
  { NULL,                           DDS_SECURITY_FSM_EVENT_AUTO,          a, &StateValidateRemoteIdentity }, // NULL state is the start state
  { &StateValidateRemoteIdentity,   VALIDATION_PENDING_RETRY,             b, &StateValRemIdentityRetryWait },
  { &StateValidateRemoteIdentity,   VALIDATION_PENDING_HANDSHAKE_MESSAGE, c, &StateHandshakeInitMessageWait },
  { &StateValRemIdentityRetryWait,  DDS_SECURITY_FSM_EVENT_TIMEOUT,       d, &StateValidateRemoteIdentity },
  { &StateHandshakeInitMessageWait, SHM_MSG_RECEIVED,                     e, &state_beginHandshakeReply },
  { &state_beginHandshakeReply,     VALIDATION_PENDING_RETRY,             f, &state_beginHsReplyWait},
  { &state_beginHandshakeReply,     VALIDATION_OK,                        g, NULL }, // Reaching NULL means end of state-diagram
  { &state_beginHsReplyWait,        DDS_SECURITY_FSM_EVENT_TIMEOUT,       h, &state_beginHandshakeReply }
};
static const uint32_t HandshakeTransistionsSize = sizeof (HandshakeTransistions) / sizeof (HandshakeTransistions[0]);

typedef enum {
  eventX, eventY, eventZ,
} test_events;

DO_SIMPLE (do_start, test, 0)
DO_SIMPLE (do_restart, test, 1)
DO_SIMPLE (do_event_stuff, test, 4)

static void do_stuff (struct dds_security_fsm *fsm, void *arg)
{
  DDSRT_UNUSED_ARG (fsm);
  DDSRT_UNUSED_ARG (arg);
  printf ("Transition %s - %d\n", __FUNCTION__, do_stuff_counter);
  ddsrt_mutex_lock (&g_lock);
  visited_test |= 1u << 2;
  ddsrt_cond_broadcast (&g_cond);
  ddsrt_mutex_unlock (&g_lock);
  if (do_stuff_counter < 2)
    dds_security_fsm_dispatch (fsm, eventZ, false);
  else if (do_stuff_counter == 2)
    dds_security_fsm_dispatch (fsm, eventY, false);
  ++do_stuff_counter;
}

static void do_other_stuff (struct dds_security_fsm *fsm, void *arg)
{
  DDSRT_UNUSED_ARG (fsm);
  DDSRT_UNUSED_ARG (arg);
  printf ("Transition %s - %d\n", __FUNCTION__, do_other_stuff_counter);
  ddsrt_mutex_lock (&g_lock);
  visited_test |= 1u << 3;
  ddsrt_cond_broadcast (&g_cond);
  ddsrt_mutex_unlock (&g_lock);
  if (do_other_stuff_counter == 0)
    dds_security_fsm_dispatch (fsm, DDS_SECURITY_FSM_EVENT_AUTO, false);
  else if (do_other_stuff_counter == 1)
    dds_security_fsm_dispatch (fsm, eventY, false);
  else if (do_other_stuff_counter == 2)
    dds_security_fsm_dispatch (fsm, eventX, false);
  ++do_other_stuff_counter;
}

static const dds_security_fsm_state state_a = { do_stuff, 0 };
static const dds_security_fsm_state state_b = { do_stuff, 100000000 };
static const dds_security_fsm_state state_c = { NULL, 0 };
static const dds_security_fsm_state state_d = { do_other_stuff, 0 };
static const dds_security_fsm_transition Transitions[] = {
  { NULL,     DDS_SECURITY_FSM_EVENT_AUTO, do_start,       &state_a }, // NULL state is the start state
  { &state_a, eventZ,                      NULL,           &state_b },
  { &state_a, eventY,                      do_other_stuff, &state_c },
  { &state_b, eventX,                      NULL,           NULL }, // Reaching NULL means end of state-diagram
  { &state_b, eventZ,                      do_restart,     &state_a },
  { &state_c, DDS_SECURITY_FSM_EVENT_AUTO, do_event_stuff, &state_d },
  { &state_d, eventY,                      do_event_stuff, &state_d },
  { &state_d, eventX,                      do_stuff,       NULL }, // Reaching NULL means end of sttimeoutate-diagram
};
static const uint32_t TransitionsSize = sizeof (Transitions) / sizeof (Transitions[0]);

/* Timeout State Machine properties and methods */
typedef enum {
  event_to_timeout, event_to_interrupt, event_to_end,
} timeout_events;

struct fsm_timeout_arg {
  int id;
};

static struct fsm_timeout_arg fsm_arg = { .id = FSM_AUTH_ARG };

DO_SIMPLE (do_interrupt, timeout, 0)
DO_SIMPLE (timeout_cb2, timeout, 3)

static void do_timeout (struct dds_security_fsm *fsm, void *arg)
{
  DDSRT_UNUSED_ARG (arg);
  printf ("Transition >>>> %s\n", __FUNCTION__);
  ddsrt_mutex_lock (&g_lock);
  visited_timeout |= 1u << 1;
  ddsrt_cond_broadcast (&g_cond);
  ddsrt_mutex_unlock (&g_lock);
  printf ("Transition <<<< %s\n", __FUNCTION__);
  dds_security_fsm_dispatch (fsm, event_to_timeout, false);
}

static void timeout_cb (struct dds_security_fsm *fsm, void *arg)
{
  struct fsm_timeout_arg *farg = arg;
  printf ("timeout_cb\n");
  ddsrt_mutex_lock (&g_lock);
  visited_timeout |= 1u << 2;
  if (farg != NULL)
    correct_arg_timeout = (farg->id == FSM_AUTH_ARG ? 1 : 0);
  correct_fsm_timeout = (fsm == fsm_timeout ? 1 : 0);
  ddsrt_cond_broadcast (&g_cond);
  ddsrt_mutex_unlock (&g_lock);
}

static const dds_security_fsm_state state_initial      = { do_timeout, 0 };
static const dds_security_fsm_state state_wait_timeout = { NULL, DDS_SECS (4) };
static const dds_security_fsm_state state_interrupt    = { do_interrupt, 0 };
static const dds_security_fsm_transition timeout_transitions[] = {
  { NULL,                DDS_SECURITY_FSM_EVENT_AUTO,    NULL, &state_initial }, // NULL state is the start state
  { &state_initial,      event_to_timeout,               NULL, &state_wait_timeout },
  { &state_wait_timeout, DDS_SECURITY_FSM_EVENT_TIMEOUT, NULL, &state_interrupt },
  { &state_wait_timeout, event_to_interrupt,             NULL, &state_interrupt },
  { &state_interrupt,    event_to_end,                   NULL, NULL }, // Reaching NULL means end of state-diagram
};
static const uint32_t timeout_transitionsSize = sizeof (timeout_transitions) / sizeof (timeout_transitions[0]);

/* Parallel Timeout State Machines properties and methods */
static struct dds_security_fsm *fsm_timeout1;
static struct dds_security_fsm *fsm_timeout2;
static struct dds_security_fsm *fsm_timeout3;

static dds_time_t time0 = 0;
static dds_time_t time1 = 0;
static dds_time_t time2 = 0;
static dds_time_t time3 = 0;

static void state_par_time1 (struct dds_security_fsm *fsm, void *arg)
{
  DDSRT_UNUSED_ARG (fsm);
  DDSRT_UNUSED_ARG (arg);
  time1 = dds_time ();
}

static void state_par_time2 (struct dds_security_fsm *fsm, void *arg)
{
  DDSRT_UNUSED_ARG (fsm);
  DDSRT_UNUSED_ARG (arg);
  time2 = dds_time ();
}

static void state_par_time3 (struct dds_security_fsm *fsm, void *arg)
{
  DDSRT_UNUSED_ARG (fsm);
  DDSRT_UNUSED_ARG (arg);
  time3 = dds_time ();
}

static const dds_security_fsm_state state_par_timeout1 = { NULL, DDS_SECS (1) };
static const dds_security_fsm_state state_par_timeout2 = { NULL, DDS_SECS (2) };
static const dds_security_fsm_state state_par_timeout3 = { NULL, DDS_SECS (1) };

static const dds_security_fsm_transition parallel_timeout_transitions_1[] = {
  { NULL,                DDS_SECURITY_FSM_EVENT_AUTO,    NULL,             &state_par_timeout1 }, // NULL state is the startfsm_control_thread state
  { &state_par_timeout1, DDS_SECURITY_FSM_EVENT_TIMEOUT, &state_par_time1, NULL }, // Reaching NULL means end of state-diagram
};
static const uint32_t parallel_timeout_transitionsSize_1 = sizeof (parallel_timeout_transitions_1) / sizeof (parallel_timeout_transitions_1[0]);

static const dds_security_fsm_transition parallel_timeout_transitions_2[] = {
  { NULL,                DDS_SECURITY_FSM_EVENT_AUTO,    NULL,             &state_par_timeout2 }, // NULL state is the start state
  { &state_par_timeout2, DDS_SECURITY_FSM_EVENT_TIMEOUT, &state_par_time2, NULL }, // Reaching NULL means end of state-diagram
};
static const uint32_t parallel_timeout_transitionsSize_2 = sizeof (parallel_timeout_transitions_2) / sizeof (parallel_timeout_transitions_2[0]);

static const dds_security_fsm_transition parallel_timeout_transitions_3[] = {
  { NULL,                DDS_SECURITY_FSM_EVENT_AUTO,    NULL,             &state_par_timeout3 }, // NULL state is the start state
  { &state_par_timeout3, DDS_SECURITY_FSM_EVENT_TIMEOUT, &state_par_time3, NULL }, // Reaching NULL means end of state-diagram
};
static const uint32_t parallel_timeout_transitionsSize_3 = sizeof (parallel_timeout_transitions_3) / sizeof (parallel_timeout_transitions_3[0]);

CU_Test(ddssec_fsm, create, .init = fsm_control_init, .fini = fsm_control_fini)
{
  /* Test single running state machine. Check creation of a single State Machine */
  fsm_auth = dds_security_fsm_create (g_fsm_control, HandshakeTransistions, HandshakeTransistionsSize, &fsm_arg);
  CU_ASSERT_FATAL (fsm_auth != NULL)

  /* set a delay that doesn't expire. Should be terminate when fsm is freed. */
  dds_security_fsm_set_timeout (fsm_auth, timeout_cb, DDS_SECS(30));
  dds_security_fsm_start (fsm_auth);

  ddsrt_mutex_lock (&g_lock);
  while (!in_handshake_init_message_wait)
  {
    printf ("waiting until handshake_init_message_wait state reached\n");
    ddsrt_cond_wait (&g_cond, &g_lock);
  }
  ddsrt_mutex_unlock (&g_lock);
  dds_security_fsm_dispatch (fsm_auth, SHM_MSG_RECEIVED, false);
  while (dds_security_fsm_running (fsm_auth))
    dds_sleepfor (DDS_MSECS (10));
  ddsrt_mutex_lock (&g_lock);
  printf ("visited_auth == 0x%x\n", visited_auth);
  CU_ASSERT (visited_auth == 0xff);
  ddsrt_mutex_unlock (&g_lock);

  /* Check correct callback parameter passing (from fsm to user defined methods) */
  CU_ASSERT(correct_arg && correct_fsm);
  dds_security_fsm_free (fsm_auth);

  /* Check whether timeout callback has NOT been invoked */
  ddsrt_mutex_lock (&g_lock);
  CU_ASSERT (visited_timeout == 0);
  ddsrt_mutex_unlock (&g_lock);
}

/* Test multiple (2) running state machines */
CU_Test(ddssec_fsm, multiple, .init = fsm_control_init, .fini = fsm_control_fini)
{
  validate_remote_identity_first = 0;
  begin_handshake_reply_first = 0;

  fsm_auth = dds_security_fsm_create (g_fsm_control, HandshakeTransistions, HandshakeTransistionsSize, NULL);
  CU_ASSERT_FATAL (fsm_auth != NULL);

  fsm_test = dds_security_fsm_create (g_fsm_control, Transitions, TransitionsSize, NULL);
  CU_ASSERT_FATAL (fsm_test != NULL);

  dds_security_fsm_start (fsm_auth);
  dds_security_fsm_start (fsm_test);

  /* Check the results of multiple running State Machines */
  ddsrt_mutex_lock (&g_lock);
  while (!in_handshake_init_message_wait)
  {
    printf ("waiting until handshake_init_message_wait state reached\n");
    ddsrt_cond_wait (&g_cond, &g_lock);
  }
  ddsrt_mutex_unlock (&g_lock);
  dds_security_fsm_dispatch (fsm_auth, SHM_MSG_RECEIVED, false);
  while (dds_security_fsm_running (fsm_auth))
    dds_sleepfor (DDS_MSECS (10));
  ddsrt_mutex_lock (&g_lock);
  printf ("visited_auth == 0x%x\n", visited_auth);
  CU_ASSERT (visited_auth == 0x55);
  ddsrt_mutex_unlock (&g_lock);

  /* Wait for the last state to occur */
  while (dds_security_fsm_running (fsm_auth))
    dds_sleepfor (DDS_MSECS (10));
  ddsrt_mutex_lock (&g_lock);
  printf ("visited_test == 0x%x\n", visited_test);
  CU_ASSERT (visited_test == 0x1f);
  ddsrt_mutex_unlock (&g_lock);

  dds_security_fsm_free (fsm_auth);
  dds_security_fsm_free (fsm_test);
}

/**
 * Check creation of State Machine for timeout purposes
 */
CU_Test(ddssec_fsm, timeout, .init = fsm_control_init, .fini = fsm_control_fini)
{
  /* Test timeout monitoring of state machines */
  fsm_timeout = dds_security_fsm_create (g_fsm_control, timeout_transitions, timeout_transitionsSize, &fsm_arg);
  CU_ASSERT_FATAL (fsm_timeout != NULL);
  dds_security_fsm_set_timeout (fsm_timeout, timeout_cb, DDS_SECS(1));
  dds_security_fsm_start (fsm_timeout);
  ddsrt_mutex_lock (&g_lock);
  while (visited_timeout != 0x7)
    ddsrt_cond_wait (&g_cond, &g_lock);
  CU_ASSERT (correct_arg_timeout && correct_fsm_timeout);
  ddsrt_mutex_unlock (&g_lock);
  dds_security_fsm_free (fsm_timeout);
}

/* Check the double global timeout */
CU_Test(ddssec_fsm, double_timeout, .init = fsm_control_init, .fini = fsm_control_fini)
{
  fsm_timeout = dds_security_fsm_create (g_fsm_control, timeout_transitions, timeout_transitionsSize, &fsm_arg);
  CU_ASSERT_FATAL (fsm_timeout != NULL);
  fsm_timeout2 = dds_security_fsm_create (g_fsm_control, timeout_transitions, timeout_transitionsSize, &fsm_arg);
  CU_ASSERT_FATAL (fsm_timeout2 != NULL);

  dds_security_fsm_set_timeout (fsm_timeout, timeout_cb, DDS_SECS (1));
  dds_security_fsm_set_timeout (fsm_timeout2, timeout_cb2, DDS_SECS (2));

  dds_security_fsm_start (fsm_timeout);
  dds_security_fsm_start (fsm_timeout2);
  ddsrt_mutex_lock (&g_lock);
  while (visited_timeout != 0xf)
    ddsrt_cond_wait (&g_cond, &g_lock);
  ddsrt_mutex_unlock (&g_lock);
  dds_security_fsm_free (fsm_timeout);
  dds_security_fsm_free (fsm_timeout2);
}

/* Check parallel state timeouts */
CU_Test(ddssec_fsm, parallel_timeout, .init = fsm_control_init, .fini = fsm_control_fini)
{
  fsm_timeout1 = dds_security_fsm_create (g_fsm_control, parallel_timeout_transitions_1, parallel_timeout_transitionsSize_1, &fsm_arg);
  CU_ASSERT_FATAL (fsm_timeout1 != NULL);
  fsm_timeout2 = dds_security_fsm_create (g_fsm_control, parallel_timeout_transitions_2, parallel_timeout_transitionsSize_2, &fsm_arg);
  CU_ASSERT_FATAL (fsm_timeout2 != NULL);
  fsm_timeout3 = dds_security_fsm_create (g_fsm_control, parallel_timeout_transitions_3, parallel_timeout_transitionsSize_3, &fsm_arg);
  CU_ASSERT_FATAL (fsm_timeout3 != NULL);

  time0 = dds_time ();
  dds_security_fsm_start (fsm_timeout1);
  dds_security_fsm_start (fsm_timeout2);
  dds_security_fsm_start (fsm_timeout3);

  while (!(dds_security_fsm_running (fsm_timeout1) && dds_security_fsm_running (fsm_timeout2) && dds_security_fsm_running (fsm_timeout3)))
    dds_sleepfor (DDS_MSECS (10));
  while (dds_security_fsm_running (fsm_timeout1) || dds_security_fsm_running (fsm_timeout2) || dds_security_fsm_running (fsm_timeout3))
    dds_sleepfor (DDS_MSECS (10));

  dds_duration_t delta1 = time1 - time0;
  dds_duration_t delta2 = time2 - time0;
  dds_duration_t delta3 = time3 - time0;
  printf ("time0 %"PRId64"\n", time0);
  printf ("time1 %"PRId64", delta1 %"PRId64"\n", time1, delta1);
  printf ("time2 %"PRId64", delta2 %"PRId64"\n", time2, delta2);
  printf ("time3 %"PRId64", delta3 %"PRId64"\n", time3, delta3);
  CU_ASSERT (delta1 > DDS_MSECS (750));
  CU_ASSERT (delta1 < DDS_MSECS (1250));
  CU_ASSERT (delta2 > DDS_MSECS (1750));
  CU_ASSERT (delta2 < DDS_MSECS (2250));
  CU_ASSERT (delta3 > DDS_MSECS (750));
  CU_ASSERT (delta3 < DDS_MSECS (1250));

  dds_security_fsm_free (fsm_timeout1);
  dds_security_fsm_free (fsm_timeout2);
  dds_security_fsm_free (fsm_timeout3);
}

/* Delete with event timeout */
CU_Test(ddssec_fsm, delete_with_timeout, .init = fsm_control_init, .fini = fsm_control_fini)
{
  fsm_timeout = dds_security_fsm_create (g_fsm_control, timeout_transitions, timeout_transitionsSize, &fsm_arg);
  CU_ASSERT_FATAL (fsm_timeout != NULL)
  dds_security_fsm_start (fsm_timeout);
  ddsrt_mutex_lock (&g_lock);
  while (visited_timeout == 0)
    ddsrt_cond_wait (&g_cond, &g_lock);
  ddsrt_mutex_unlock (&g_lock);
  dds_security_fsm_free (fsm_timeout);
}

