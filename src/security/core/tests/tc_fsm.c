#include "dds/security/core/dds_security_fsm.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/misc.h"
#include <stdio.h>
#include <stdbool.h>
#include "CUnit/CUnit.h"
#include "CUnit/Test.h"

#define CHECK_BIT(var, pos) ((var) & (1<<(pos)))

#define FSM_AUTH_ARG 10

#define DB_TC_PRINT_DEBUG  (false)

static struct dds_security_fsm *fsm_auth;
static struct dds_security_fsm *fsm_test;
static struct dds_security_fsm *fsm_timeout;
static struct dds_security_fsm *fsm_timeout2;
static struct dds_security_fsm *fsm_timeout3;

static uint32_t visited_auth = 0;
static uint32_t visited_test = 0;
static uint32_t visited_timeout = 0;

uint32_t correct_fsm = 0;
uint32_t correct_arg = 0;

uint32_t correct_fsm_timeout = 0;
uint32_t correct_arg_timeout = 0;

static ddsrt_cond_t stop_timeout_cond;
static ddsrt_mutex_t stop_timeout_cond_mutex;
static uint32_t stop_timeout_cond_cnt = 0;
static int validate_remote_identity_first = 1;
static int begin_handshake_reply_first = 1;
static int do_stuff_counter = 0;
static int do_other_stuff_counter = 0;
/*
 * Authentication State Machine properties and methods
 */
typedef enum {
    VALIDATION_PENDING_RETRY,
    VALIDATION_FAILED,
    VALIDATION_OK,
    VALIDATION_OK_FINAL_MESSAGE,
    VALIDATION_PENDING_HANDSHAKE_MESSAGE,
    VALIDATION_PENDING_HANDSHAKE_REQUEST,
    PluginReturn_MAX
} PluginReturn;

static PluginReturn validate_remote_identity(void) {

  if (DB_TC_PRINT_DEBUG) {
    printf("validate_remote_identity - %d\n", validate_remote_identity_first);
  }
  if (validate_remote_identity_first) {
    validate_remote_identity_first = 0;
    return VALIDATION_PENDING_RETRY;
  }
  return VALIDATION_PENDING_HANDSHAKE_MESSAGE;
}

static PluginReturn begin_handshake_reply(void) {

  if (DB_TC_PRINT_DEBUG) {
    printf("begin_handshake_reply - %d\n", begin_handshake_reply_first);
  }
  if (begin_handshake_reply_first) {
    begin_handshake_reply_first = 0;
    return VALIDATION_PENDING_RETRY;
  }
  return VALIDATION_OK_FINAL_MESSAGE;
}

static PluginReturn get_shared_secret(void) {
  return VALIDATION_OK;
}

/* State actions. */
static void fsm_validate_remote_identity(struct dds_security_fsm *fsm, void *arg) {
  PluginReturn ret;

  DDSRT_UNUSED_ARG(arg);

  ret = validate_remote_identity();

  if (DB_TC_PRINT_DEBUG) {
    printf("[%p] State %s (ret %d)\n", fsm, __FUNCTION__, (int) ret);
  }

  dds_security_fsm_dispatch(fsm, (int32_t) ret);
}

static void fsm_begin_handshake_reply(struct dds_security_fsm *fsm, void *arg) {
  PluginReturn ret;

  DDSRT_UNUSED_ARG(arg);

  ret = begin_handshake_reply();
  if (ret == VALIDATION_OK_FINAL_MESSAGE) {
    ret = get_shared_secret();
  }

  if (DB_TC_PRINT_DEBUG) {
    printf("[%p] State %s (ret %d)\n", fsm, __FUNCTION__, (int) ret);
  }

  dds_security_fsm_dispatch(fsm, (int32_t) ret);
}

/* A few states from the handshake state-machine. */
static dds_security_fsm_state StateValidateRemoteIdentity = {
        fsm_validate_remote_identity, 0};
static dds_security_fsm_state StateValRemIdentityRetryWait = {NULL, 100000000};
static dds_security_fsm_state StateHandshakeInitMessageWait = {NULL, 0};
static dds_security_fsm_state StateBeginHandshakeReply = {fsm_begin_handshake_reply, 0};
static dds_security_fsm_state StateBeginHsReplyWait = {NULL, 100000000};

static void a(struct dds_security_fsm *fsm, void *arg) {
  int *fsm_arg;

  if (DB_TC_PRINT_DEBUG) {
    printf("[%p] Transition %s\n", fsm, __FUNCTION__);
  }

  if (arg != NULL) {
    fsm_arg = (int *) arg;

    if (*fsm_arg == FSM_AUTH_ARG) {
      correct_arg = 1;
    } else {
      correct_arg = 0;
    }
  }

  if (fsm == fsm_auth) {
    correct_fsm = 1;
  } else {
    correct_fsm = 0;
  }
  visited_auth |= 1UL << 0;
}

static void b(struct dds_security_fsm *fsm, void *arg) {
  DDSRT_UNUSED_ARG(fsm);
  DDSRT_UNUSED_ARG(arg);
  if (DB_TC_PRINT_DEBUG)
    printf("[%p] Transition %s\n", fsm, __FUNCTION__);
  visited_auth |= 1UL << 1;
}

static void c(struct dds_security_fsm *fsm, void *arg) {
  DDSRT_UNUSED_ARG(fsm);
  DDSRT_UNUSED_ARG(arg);
  if (DB_TC_PRINT_DEBUG)
    printf("[%p] Transition %s\n", fsm, __FUNCTION__);
  visited_auth |= 1UL << 2;
}

static void d(struct dds_security_fsm *fsm, void *arg) {
  DDSRT_UNUSED_ARG(fsm);
  DDSRT_UNUSED_ARG(arg);
  if (DB_TC_PRINT_DEBUG)
    printf("[%p] Transition %s\n", fsm, __FUNCTION__);
  visited_auth |= 1UL << 3;
}

static void e(struct dds_security_fsm *fsm, void *arg) {
  DDSRT_UNUSED_ARG(fsm);
  DDSRT_UNUSED_ARG(arg);
  if (DB_TC_PRINT_DEBUG)
    printf("[%p] Transition %s\n", fsm, __FUNCTION__);
  visited_auth |= 1UL << 4;
}

static void f(struct dds_security_fsm *fsm, void *arg) {
  DDSRT_UNUSED_ARG(fsm);
  DDSRT_UNUSED_ARG(arg);
  if (DB_TC_PRINT_DEBUG)
    printf("[%p] Transition %s\n", fsm, __FUNCTION__);
  visited_auth |= 1UL << 5;
}

static void g(struct dds_security_fsm *fsm, void *arg) {
  DDSRT_UNUSED_ARG(fsm);
  DDSRT_UNUSED_ARG(arg);
  if (DB_TC_PRINT_DEBUG)
    printf("[%p] Transition %s\n", fsm, __FUNCTION__);
  visited_auth |= 1UL << 6;
}

static void h(struct dds_security_fsm *fsm, void *arg) {
  DDSRT_UNUSED_ARG(fsm);
  DDSRT_UNUSED_ARG(arg);
  if (DB_TC_PRINT_DEBUG)
    printf("[%p] Transition %s\n", fsm, __FUNCTION__);
  visited_auth |= 1UL << 7;
}

#define SHM_MSG_RECEIVED (PluginReturn_MAX + 1)

/*
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
 *          VALIDATION_PENDING_RETRY    |        StateBeginHandshakeReply        |
 *          f()                         |----------------------------------------|
 *            .-------------------------| fsm_begin_handshake_reply()            |
 *            |                         |    - dispatch VALIDATION_PENDING_RETRY |
 *            v                         |    - dispatch VALIDATION_OK            |
 * .-----------------------.    100ms   '----------------------------------------'
 * | StateBeginHsReplyWait |    h()                ^        VALIDATION_OK |
 * |-----------------------|-----------------------'        g()           |
 * '-----------------------'                                              v
 *                                                                       .-.
 *                                                                       '-'
 */
dds_security_fsm_transition HandshakeTransistions[] =
        {{NULL,                           DDS_SECURITY_FSM_EVENT_AUTO,                  a, &StateValidateRemoteIdentity}, // NULL state is the start state
         {&StateValidateRemoteIdentity, VALIDATION_PENDING_RETRY,             b,
                                                                                 &StateValRemIdentityRetryWait},
         {&StateValidateRemoteIdentity,
                                        VALIDATION_PENDING_HANDSHAKE_MESSAGE, c,
                                                                                 &StateHandshakeInitMessageWait},
         {&StateValRemIdentityRetryWait,
                                          DDS_SECURITY_FSM_EVENT_TIMEOUT,               d, &StateValidateRemoteIdentity},
         {
          &StateHandshakeInitMessageWait, SHM_MSG_RECEIVED,                   e,
                                                                                 &StateBeginHandshakeReply},
         {&StateBeginHandshakeReply,
                                        VALIDATION_PENDING_RETRY,             f, &StateBeginHsReplyWait},
         {
          &StateBeginHandshakeReply,    VALIDATION_OK,                        g, NULL}, // Reaching NULL means end of state-diagram
         {&StateBeginHsReplyWait,         DDS_SECURITY_FSM_EVENT_TIMEOUT,               h,
                                                                                 &StateBeginHandshakeReply},};

/*
 * Example State Machine properties and methods
 */
typedef enum {
    eventX, eventY, eventZ,
} test_events;

/* The functions called from the state-machine. */
static void doStart(struct dds_security_fsm *fsm, void *arg) {
  DDSRT_UNUSED_ARG(fsm);
  DDSRT_UNUSED_ARG(arg);
  if (DB_TC_PRINT_DEBUG)
    printf("Transition %s\n", __FUNCTION__);
  visited_test |= 1UL << 0;
}

static void doRestart(struct dds_security_fsm *fsm, void *arg) {
  DDSRT_UNUSED_ARG(fsm);
  DDSRT_UNUSED_ARG(arg);
  if (DB_TC_PRINT_DEBUG)
    printf("Transition %s\n", __FUNCTION__);
  visited_test |= 1UL << 1;
}

static void doEventStuff(struct dds_security_fsm *fsm, void *arg) {
  DDSRT_UNUSED_ARG(fsm);
  DDSRT_UNUSED_ARG(arg);
  if (DB_TC_PRINT_DEBUG)
    printf("Transition %s\n", __FUNCTION__);
  visited_test |= 1UL << 4;
}

static void doStuff(struct dds_security_fsm *fsm, void *arg) {

  DDSRT_UNUSED_ARG(fsm);
  DDSRT_UNUSED_ARG(arg);

  if (DB_TC_PRINT_DEBUG) {
    printf("Transition %s - %d\n", __FUNCTION__, do_stuff_counter);
  }
  visited_test |= 1UL << 2;

  if (do_stuff_counter == 0) {
    dds_security_fsm_dispatch(fsm, eventZ);
  } else if (do_stuff_counter == 2) {
    dds_security_fsm_dispatch(fsm, eventY);
  }
  ++do_stuff_counter;
}

static void doOtherStuff(struct dds_security_fsm *fsm, void *arg) {
  DDSRT_UNUSED_ARG(fsm);
  DDSRT_UNUSED_ARG(arg);

  if (DB_TC_PRINT_DEBUG) {
    printf("Transition %s - %d\n", __FUNCTION__, do_other_stuff_counter);
  }
  visited_test |= 1UL << 3;
  if (do_other_stuff_counter == 0) {
    dds_security_fsm_dispatch(fsm, DDS_SECURITY_FSM_EVENT_AUTO);
  }
  if (do_other_stuff_counter == 1) {
    dds_security_fsm_dispatch(fsm, eventY);
  } else if (do_other_stuff_counter == 2) {
    dds_security_fsm_dispatch(fsm, eventX);
  }
  ++do_other_stuff_counter;
}

dds_security_fsm_state StateA = {doStuff, 0};
dds_security_fsm_state StateB = {doStuff, 100000000};
dds_security_fsm_state StateC = {NULL, 0};
dds_security_fsm_state StateD = {doOtherStuff, 0};

dds_security_fsm_transition transitions[] = {
        {NULL,    DDS_SECURITY_FSM_EVENT_AUTO, doStart,      &StateA}, // NULL state is the start state
        {&StateA, eventZ, NULL,                    &StateB},
        {&StateA, eventY,            doOtherStuff, &StateC},
        {&StateB, eventX, NULL,               NULL}, // Reaching NULL means end of state-diagram
        {&StateB, eventZ,            doRestart,    &StateA},
        {&StateC, DDS_SECURITY_FSM_EVENT_AUTO,
                                     doEventStuff, &StateD},
        {&StateD, eventY,            doEventStuff, &StateD},
        {
         &StateD, eventX,            doStuff, NULL}, // Reaching NULL means end of state-diagram
};

/*
 * Timeout State Machine properties and methods
 */
typedef enum {
    eventToTimeout, eventToEnd,
} timeout_events;

/* The functions callld from the state-machine. */
static void doInterupt(struct dds_security_fsm *fsm, void *arg) {
  DDSRT_UNUSED_ARG(fsm);
  DDSRT_UNUSED_ARG(arg);
  if (DB_TC_PRINT_DEBUG) {
    printf("Transition %s\n", __FUNCTION__);
  }
  visited_timeout |= 1UL << 0;
}

static void doTimeout(struct dds_security_fsm *fsm, void *arg) {
  dds_duration_t delay4 = 4 * DDS_NSECS_IN_SEC;

  DDSRT_UNUSED_ARG(arg);

  if (DB_TC_PRINT_DEBUG) {
    printf("Transition >>>> %s %d\n", __FUNCTION__, stop_timeout_cond_cnt);
  }
  visited_timeout |= 1UL << 1;

  stop_timeout_cond_cnt++;
  ddsrt_mutex_lock(&stop_timeout_cond_mutex);
  (void) ddsrt_cond_waitfor(&stop_timeout_cond, &stop_timeout_cond_mutex,
                            delay4);
  ddsrt_mutex_unlock(&stop_timeout_cond_mutex);
  stop_timeout_cond_cnt--;

  if (DB_TC_PRINT_DEBUG) {
    printf("Transition <<<< %s %d\n", __FUNCTION__, stop_timeout_cond_cnt);
  }

  dds_security_fsm_dispatch(fsm, eventToTimeout);
}

static void TimeoutCallback(struct dds_security_fsm *fsm, void *arg) {
  int *fsm_arg;

  if (DB_TC_PRINT_DEBUG) {
    printf("TimeoutCallback\n");
  }

  visited_timeout |= 1UL << 2;

  if (arg != NULL) {
    fsm_arg = (int *) arg;

    if (*fsm_arg == FSM_AUTH_ARG) {
      correct_arg_timeout = 1;
    } else {
      correct_arg_timeout = 0;
    }
  }
  if (fsm == fsm_timeout) {
    correct_fsm_timeout = 1;
  } else {
    correct_fsm_timeout = 0;
  }
}

static void TimeoutCallback2(struct dds_security_fsm *fsm, void *arg) {
  DDSRT_UNUSED_ARG(fsm);
  DDSRT_UNUSED_ARG(arg);
  if (DB_TC_PRINT_DEBUG) {
    printf("TimeoutCallback2\n");
  }
  visited_timeout |= 1UL << 3;
}

dds_security_fsm_state StateTimeout = {doTimeout, 0};
dds_security_fsm_state StateInterupt = {doInterupt, 0};

dds_security_fsm_transition timeout_transitions[] = {{NULL, DDS_SECURITY_FSM_EVENT_AUTO,       NULL,
                                                                                 &StateTimeout}, // NULL state is the start state
                                           {&StateTimeout, eventToTimeout, NULL, &StateInterupt},
                                           {&StateInterupt,
                                                           eventToEnd,     NULL, NULL}, // Reaching NULL means end of state-diagram
};

static dds_time_t time0 = 0;
static dds_time_t time1 = 0;
static dds_time_t time2 = 0;
static dds_time_t time3 = 0;

static void StateParTime1(struct dds_security_fsm *fsm, void *arg) {
  DDSRT_UNUSED_ARG(fsm);
  DDSRT_UNUSED_ARG(arg);
  time1 = dds_time();
}

static void StateParTime2(struct dds_security_fsm *fsm, void *arg) {
  DDSRT_UNUSED_ARG(fsm);
  DDSRT_UNUSED_ARG(arg);
  time2 = dds_time();
}

static void StateParTime3(struct dds_security_fsm *fsm, void *arg) {
  DDSRT_UNUSED_ARG(fsm);
  DDSRT_UNUSED_ARG(arg);
  time3 = dds_time();
}

dds_security_fsm_state StateParTimeout1 = {NULL, DDS_NSECS_IN_SEC};
dds_security_fsm_state StateParTimeout2 = {NULL, 2 * DDS_NSECS_IN_SEC};
dds_security_fsm_state StateParTimeout3 = {NULL, DDS_NSECS_IN_SEC};

dds_security_fsm_transition par_timeout_transitions_1[] = {{NULL,              DDS_SECURITY_FSM_EVENT_AUTO,
                                                         NULL, &StateParTimeout1}, // NULL state is the start state
                                                 {&StateParTimeout1, DDS_SECURITY_FSM_EVENT_TIMEOUT, &StateParTime1, NULL}, // Reaching NULL means end of state-diagram
};

dds_security_fsm_transition par_timeout_transitions_2[] = {{NULL,              DDS_SECURITY_FSM_EVENT_AUTO,
                                                         NULL, &StateParTimeout2}, // NULL state is the start state
                                                 {&StateParTimeout2, DDS_SECURITY_FSM_EVENT_TIMEOUT, &StateParTime2, NULL}, // Reaching NULL means end of state-diagram
};

dds_security_fsm_transition par_timeout_transitions_3[] = {{NULL,              DDS_SECURITY_FSM_EVENT_AUTO,
                                                         NULL, &StateParTimeout3}, // NULL state is the start state
                                                 {&StateParTimeout3, DDS_SECURITY_FSM_EVENT_TIMEOUT, &StateParTime3, NULL}, // Reaching NULL means end of state-diagram
};

int fsm_arg = FSM_AUTH_ARG;
dds_time_t delay1 = DDS_NSECS_IN_SEC;
dds_time_t delay2 = 2 * DDS_NSECS_IN_SEC;
dds_time_t delay30 = 30 * DDS_NSECS_IN_SEC;
int timeout;
static const dds_duration_t msec100 = 100 * DDS_NSECS_IN_MSEC;

static void init_testcase(void) {
  (void) ddsrt_mutex_init(&stop_timeout_cond_mutex);
  (void) ddsrt_cond_init(&stop_timeout_cond);

}

static void fini_testcase(void) {
  ddsrt_cond_destroy(&stop_timeout_cond);
  ddsrt_mutex_destroy(&stop_timeout_cond_mutex);
}

CU_Test(ddssec_fsm, create, .init = init_testcase, .fini = fini_testcase) {

  /*
   * Test single running state machine
   * Check creation of a single State Machine
   */

  fsm_auth = dds_security_fsm_create(NULL, HandshakeTransistions,
                           sizeof(HandshakeTransistions) / sizeof(HandshakeTransistions[0]),
                           &fsm_arg);
  CU_ASSERT_FATAL(fsm_auth != NULL)

  // set a delay that doesn't expire. Should be terminate when fsm is freed.
  dds_security_fsm_set_timeout(fsm_auth, TimeoutCallback, delay30);
  dds_security_fsm_start(fsm_auth);

  /**
   * Check the result of one running State Machine
   */

  // Wait for the last state to occur
  timeout = 100; /* 10 sec */
  while ((dds_security_fsm_current_state(fsm_auth) != &StateHandshakeInitMessageWait)
         && (timeout > 0)) {
    dds_sleepfor(msec100);
    timeout--;
  }
  CU_ASSERT(timeout > 0);
  dds_security_fsm_dispatch(fsm_auth, SHM_MSG_RECEIVED);

  timeout = 100; /* 10 sec */
  while ((dds_security_fsm_current_state(fsm_auth) != NULL) && (timeout > 0)) {
    dds_sleepfor(msec100);
    timeout--;
  }

  CU_ASSERT(timeout > 0);
  CU_ASSERT(
          CHECK_BIT(visited_auth, 0) && CHECK_BIT(visited_auth, 1) && CHECK_BIT(visited_auth, 2) &&
          CHECK_BIT(visited_auth, 3) && CHECK_BIT(visited_auth, 4) && CHECK_BIT(visited_auth, 5) &&
          CHECK_BIT(visited_auth, 6) && CHECK_BIT(visited_auth, 7));

  /*
   * "Check correct callback parameter passing (from fsm to user defined methods) ");
   */
  CU_ASSERT(correct_arg && correct_fsm);
  dds_security_fsm_free(fsm_auth);

  /* Check whether timeout callback has NOT been invoked */
  CU_ASSERT(visited_timeout == 0);
}

/*
 * Test multiple (2) running state machines
 */
CU_Test(ddssec_fsm, multiple, .init = init_testcase, .fini = fini_testcase) {

  /*Check creation of multiple (2) State Machines*/
  validate_remote_identity_first = 0;
  begin_handshake_reply_first = 0;
  visited_auth = 0;

  fsm_auth = dds_security_fsm_create(NULL, HandshakeTransistions,
                           sizeof(HandshakeTransistions) / sizeof(HandshakeTransistions[0]), NULL);
  fsm_test = dds_security_fsm_create(NULL, transitions,
                           sizeof(transitions) / sizeof(transitions[0]), NULL);
  CU_ASSERT_FALSE(fsm_auth == NULL || fsm_test == NULL);
  dds_security_fsm_start(fsm_auth);
  dds_security_fsm_start(fsm_test);

  /*Check the results of multiple running State Machines */

  // Wait for the last state to occur
  timeout = 100; /* 10 sec */
  while ((dds_security_fsm_current_state(fsm_auth) != &StateHandshakeInitMessageWait)
         && (timeout > 0)) {
    dds_sleepfor(100 * DDS_NSECS_IN_MSEC);
    timeout--;
  }
  CU_ASSERT_FATAL(timeout > 0);
  timeout = 100; /* 10 sec */
  dds_security_fsm_dispatch(fsm_auth, SHM_MSG_RECEIVED);
  while ((dds_security_fsm_current_state(fsm_auth) != NULL) && (timeout > 0)) {
    dds_sleepfor(100 * DDS_NSECS_IN_MSEC);
    timeout--;
  }
  CU_ASSERT_FATAL(timeout > 0);
  // not all bits are set since we're running the state machine a second time
  CU_ASSERT_FATAL(
          CHECK_BIT(visited_auth, 0) && !CHECK_BIT(visited_auth, 1) && CHECK_BIT(visited_auth, 2) &&
          !CHECK_BIT(visited_auth, 3) && CHECK_BIT(visited_auth, 4) && !CHECK_BIT(visited_auth, 5) &&
          CHECK_BIT(visited_auth, 6) && !CHECK_BIT(visited_auth, 7));

  timeout = 100; /* 10 sec */
  // Wait for the last state to occur
  while ((dds_security_fsm_current_state(fsm_test) != NULL) && timeout > 0) {
    dds_sleepfor(100 * DDS_NSECS_IN_MSEC);
    timeout--;
  }
  CU_ASSERT_FATAL(timeout > 0);
  CU_ASSERT_FATAL(
          CHECK_BIT(visited_test, 0) && CHECK_BIT(visited_test, 1) && CHECK_BIT(visited_test, 2) &&
          CHECK_BIT(visited_test, 3));
  dds_security_fsm_free(fsm_auth);
  dds_security_fsm_free(fsm_test);

}

/**
 * Check creation of State Machine for timeout purposes
 */
CU_Test(ddssec_fsm, timeout, .init = init_testcase, .fini = fini_testcase) {

  /*
   * Test timeout monitoring of state machines
   */
  fsm_timeout = dds_security_fsm_create(NULL, timeout_transitions,
                              sizeof(timeout_transitions) / sizeof(timeout_transitions[0]), &fsm_arg);
  CU_ASSERT(fsm_timeout != NULL);

  dds_security_fsm_set_timeout(fsm_timeout, TimeoutCallback, delay1);
  dds_security_fsm_start(fsm_timeout);

  /*Check the result of the running State Machine for timeout purposes*/

  // Wait for the last state to occur
  timeout = 100; /* 10 sec */
  while ((dds_security_fsm_current_state(fsm_timeout) != &StateInterupt) && (timeout > 0)) {
    dds_sleepfor(100 * DDS_NSECS_IN_MSEC);
    timeout--;
  }
  CU_ASSERT(timeout > 0);
  CU_ASSERT(
          CHECK_BIT(visited_timeout, 0) && CHECK_BIT(visited_timeout, 1) && CHECK_BIT(visited_timeout, 2));
  CU_ASSERT(correct_arg_timeout && correct_fsm_timeout);

  dds_security_fsm_free(fsm_timeout);

}

/**
 * Check the double global timeout
 */
CU_Test(ddssec_fsm, double_timeout, .init = init_testcase, .fini = fini_testcase) {

  visited_timeout = 0;
  fsm_timeout = dds_security_fsm_create(NULL, timeout_transitions,
                              sizeof(timeout_transitions) / sizeof(timeout_transitions[0]), &fsm_arg);
  CU_ASSERT(fsm_timeout != NULL);
  fsm_timeout2 = dds_security_fsm_create(NULL, timeout_transitions,
                               sizeof(timeout_transitions) / sizeof(timeout_transitions[0]), &fsm_arg);
  CU_ASSERT(fsm_timeout2 != NULL);
  dds_security_fsm_set_timeout(fsm_timeout, TimeoutCallback, delay1);
  dds_security_fsm_set_timeout(fsm_timeout2, TimeoutCallback2, delay2);
  dds_security_fsm_start(fsm_timeout);
  dds_security_fsm_start(fsm_timeout2);
  timeout = 100; /* 10 sec */
  while ((CHECK_BIT(visited_timeout, 2) == 0) && (timeout > 0)) {
    dds_sleepfor(100 * DDS_NSECS_IN_MSEC);
    timeout--;
  }
  CU_ASSERT(CHECK_BIT(visited_timeout, 2));
  dds_security_fsm_cleanup(fsm_timeout);
  timeout = 100; /* 10 sec */
  while ((CHECK_BIT(visited_timeout, 3) == 0) && (timeout > 0)) {
    dds_sleepfor(100 * DDS_NSECS_IN_MSEC);
    timeout--;
  }
  CU_ASSERT(CHECK_BIT(visited_timeout, 3));
  dds_security_fsm_free(fsm_timeout);
  dds_security_fsm_free(fsm_timeout2);
  ddsrt_mutex_lock(&stop_timeout_cond_mutex);
  ddsrt_cond_signal(&stop_timeout_cond);
  ddsrt_mutex_unlock(&stop_timeout_cond_mutex);

}

/**
 * Check parallel state timeouts
 */
CU_Test(ddssec_fsm, parallel_timeout, .init = init_testcase, .fini = fini_testcase) {
  visited_timeout = 0;
  fsm_timeout = dds_security_fsm_create(NULL, par_timeout_transitions_1,
                              sizeof(par_timeout_transitions_1) / sizeof(par_timeout_transitions_1[0]),
                              &fsm_arg);
  CU_ASSERT(fsm_timeout != NULL);
  fsm_timeout2 = dds_security_fsm_create(NULL, par_timeout_transitions_2,
                               sizeof(par_timeout_transitions_2) / sizeof(par_timeout_transitions_2[0]),
                               &fsm_arg);
  CU_ASSERT(fsm_timeout2 != NULL);
  fsm_timeout3 = dds_security_fsm_create(NULL, par_timeout_transitions_3,
                               sizeof(par_timeout_transitions_3) / sizeof(par_timeout_transitions_3[0]),
                               &fsm_arg);
  CU_ASSERT(fsm_timeout3 != NULL);
  dds_duration_t delta1;
  dds_duration_t delta2;
  dds_duration_t delta3;

  time0 = dds_time();
  dds_security_fsm_start(fsm_timeout);
  dds_security_fsm_start(fsm_timeout2);
  dds_security_fsm_start(fsm_timeout3);

  /* Wait for both to end. */
  timeout = 100; /* 10 sec */
  /* First, they have to be started. */
  while (((dds_security_fsm_current_state(fsm_timeout) == NULL)
          || (dds_security_fsm_current_state(fsm_timeout2) == NULL)
          || (dds_security_fsm_current_state(fsm_timeout3) == NULL)) && (timeout > 0)) {
    dds_sleepfor(100 * DDS_NSECS_IN_MSEC);
    timeout--;
  }
  /* Then, they have to have ended. */
  while (((dds_security_fsm_current_state(fsm_timeout) != NULL)
          || (dds_security_fsm_current_state(fsm_timeout2) != NULL)
          || (dds_security_fsm_current_state(fsm_timeout3) != NULL)) && (timeout > 0)) {
    dds_sleepfor(100 * DDS_NSECS_IN_MSEC);
    timeout--;
  }

  /*
   * There should be about 1 second difference between all times:
   *      time1 = time0 + 1
   *      time2 = time0 + 2
   *      time3 = time0 + 1
   */
  delta1 = time1 - time0;
  delta2 = time2 - time0;
  delta3 = time3 - time0;
  printf("Time0 %" PRId64 "\n", time0);
  printf("Time1 %" PRId64 "\n", time1);
  printf("Time2 %" PRId64 "\n", time2);
  printf("Time3 %" PRId64 "\n", time3);
  printf("Delta1 %" PRId64 "\n", delta1);
  printf("Delta2 %" PRId64 "\n", delta2);
  printf("Delta3 %" PRId64 "\n", delta3);
  CU_ASSERT(delta1 > 750 * DDS_NSECS_IN_MSEC);
  CU_ASSERT(delta1 < 1250 * DDS_NSECS_IN_MSEC);
  CU_ASSERT(delta2 > 1750 * DDS_NSECS_IN_MSEC);
  CU_ASSERT(delta2 < 2250 * DDS_NSECS_IN_MSEC);
  CU_ASSERT(delta3 > 750 * DDS_NSECS_IN_MSEC);
  CU_ASSERT(delta3 < 1250 * DDS_NSECS_IN_MSEC);

  dds_security_fsm_free(fsm_timeout);
  dds_security_fsm_free(fsm_timeout2);
  dds_security_fsm_free(fsm_timeout3);

}

/**
 * Delete with event timeout
 */
CU_Test(ddssec_fsm, delete_with_timeout, .init = init_testcase, .fini = fini_testcase) {

  fsm_timeout = dds_security_fsm_create(NULL, timeout_transitions,
                              sizeof(timeout_transitions) / sizeof(timeout_transitions[0]), &fsm_arg);
  CU_ASSERT (fsm_timeout != NULL);
  visited_timeout = 0;
  dds_security_fsm_start(fsm_timeout);

  /* Wait until we're in the timeout function. */
  timeout = 100; /* 10 sec */
  while ((visited_timeout == 0) && (timeout > 0)) {
    dds_sleepfor(100 * DDS_NSECS_IN_MSEC);
    timeout--;
  }

  dds_security_fsm_free(fsm_timeout);
  dds_sleepfor(100 * DDS_NSECS_IN_MSEC);

  /* Just for safety to be certain that the condition isn't used anymore before destroying it. */
  while (stop_timeout_cond_cnt > 0) {
    dds_time_t d = DDS_NSECS_IN_SEC;
    ddsrt_cond_signal(&stop_timeout_cond);
    dds_sleepfor(d);
  }


}

