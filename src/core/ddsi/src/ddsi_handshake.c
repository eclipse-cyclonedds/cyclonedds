// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/features.h"
#include "ddsi__handshake.h"

#ifdef DDS_HAS_SECURITY

#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_proxy_participant.h"
#include "ddsi__entity_index.h"
#include "ddsi__plist.h"
#include "ddsi__entity.h"
#include "ddsi__participant.h"
#include "ddsi__gc.h"
#include "ddsi__security_omg.h"
#include "ddsi__security_util.h"
#include "ddsi__security_exchange.h"
#include "dds/security/dds_security_api_types.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_fsm.h"

#define HSTRACE(...)    DDS_CTRACE (&handshake->gv->logconfig, __VA_ARGS__)
#define HSWARNING(...)  DDS_CLOG (DDS_LC_WARNING, &handshake->gv->logconfig, __VA_ARGS__)
#define HSERROR(...)    DDS_CLOG (DDS_LC_ERROR, &handshake->gv->logconfig, __VA_ARGS__)

#define HSEXCEPTION(e, ...) \
  ddsi_omg_log_exception(&handshake->gv->logconfig, DDS_LC_WARNING, e, __FILE__, __LINE__, DDS_FUNCTION, __VA_ARGS__)


#define VERBOSE_HANDSHAKE_DEBUG

#if 1
#define TRACE_FUNC(ptr)
#else
#undef TRACE
#define TRACE(args) ddsi_trace args
#define TRACE_FUNC(ptr) printf("[%p] %s\n", ptr, __FUNCTION__);
#endif

typedef enum {
    EVENT_AUTO                                  = DDS_SECURITY_FSM_EVENT_AUTO,
    EVENT_TIMEOUT                               = DDS_SECURITY_FSM_EVENT_TIMEOUT,
    EVENT_VALIDATION_OK                         = DDS_SECURITY_VALIDATION_OK,
    EVENT_VALIDATION_FAILED                     = DDS_SECURITY_VALIDATION_FAILED,
    EVENT_VALIDATION_PENDING_RETRY              = DDS_SECURITY_VALIDATION_PENDING_RETRY,
    EVENT_VALIDATION_PENDING_HANDSHAKE_REQUEST  = DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_REQUEST,
    EVENT_VALIDATION_PENDING_HANDSHAKE_MESSAGE  = DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE,
    EVENT_VALIDATION_OK_FINAL_MESSAGE           = DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE,
    EVENT_RECEIVED_MESSAGE_REQUEST              = 100,
    EVENT_RECEIVED_MESSAGE_REPLY                = 101,
    EVENT_RECEIVED_MESSAGE_FINAL                = 102,
    EVENT_SEND_CRYPTO_TOKENS                    = 103,
    EVENT_RECV_CRYPTO_TOKENS                    = 104
} handshake_event_t;

struct handshake_entities {
  ddsi_guid_t lguid;
  ddsi_guid_t rguid;
};

struct ddsi_handshake
{
  ddsrt_avl_node_t avlnode;
  enum ddsi_handshake_state state;
  struct handshake_entities participants;
  DDS_Security_HandshakeHandle handshake_handle;
  ddsrt_atomic_uint32_t refc;
  ddsrt_atomic_uint32_t deleting;
  ddsi_handshake_end_cb_t end_cb;
  ddsrt_mutex_t lock;
  struct dds_security_fsm *fsm;
  const struct ddsi_domaingv *gv;
  dds_security_authentication *auth;

  DDS_Security_HandshakeMessageToken handshake_message_in_token;
  ddsi_message_identity_t handshake_message_in_id;
  DDS_Security_HandshakeMessageToken *handshake_message_out;
  DDS_Security_AuthRequestMessageToken local_auth_request_token;
  DDS_Security_AuthRequestMessageToken *remote_auth_request_token;
  DDS_Security_OctetSeq pdata;
  int64_t shared_secret;
};

struct ddsi_hsadmin {
  ddsrt_mutex_t lock;
  ddsrt_avl_tree_t handshakes;
  struct dds_security_fsm_control *fsm_control;
};

static int compare_handshake(const void *va, const void *vb);

const ddsrt_avl_treedef_t handshake_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct ddsi_handshake, avlnode), offsetof (struct ddsi_handshake, participants), compare_handshake, 0);

static int compare_handshake(const void *va, const void *vb)
{
  const struct handshake_entities *ha = va;
  const struct handshake_entities *hb = vb;
  int r;

  r = memcmp(&ha->rguid, &hb->rguid, sizeof(ha->rguid));
  if (r == 0)
    r = memcmp(&ha->lguid, &hb->lguid, sizeof(ha->lguid));
  return r;
}

static bool validate_handshake(struct ddsi_handshake *handshake, struct ddsi_participant **pp, struct ddsi_proxy_participant **proxypp)
{
  if (ddsrt_atomic_ld32(&handshake->deleting) > 0)
    return false;

  if (pp)
  {
    if ((*pp = ddsi_entidx_lookup_participant_guid(handshake->gv->entity_index, &handshake->participants.lguid)) == NULL)
      return false;
  }

  if (proxypp)
  {
    if ((*proxypp = ddsi_entidx_lookup_proxy_participant_guid(handshake->gv->entity_index, &handshake->participants.rguid)) == NULL)
      return false;
  }
  return true;
}


#define RETRY_TIMEOUT           DDS_SECS(1)
#define RESEND_TIMEOUT          DDS_SECS(1)
#define SEND_TOKENS_TIMEOUT     DDS_MSECS(100)
#define AUTHENTICATION_TIMEOUT  DDS_SECS(100)
#define INITIAL_DELAY           DDS_MSECS(10)

static void func_validate_remote_and_begin_reply (struct dds_security_fsm *fsm, void *arg);
static void func_validate_remote_identity        (struct dds_security_fsm *fsm, void *arg);
static void func_handshake_init_message_resend   (struct dds_security_fsm *fsm, void *arg);
static void func_begin_handshake_reply           (struct dds_security_fsm *fsm, void *arg);
static void func_begin_handshake_request         (struct dds_security_fsm *fsm, void *arg);
static void func_process_handshake               (struct dds_security_fsm *fsm, void *arg);
static void func_handshake_message_resend        (struct dds_security_fsm *fsm, void *arg);
static void func_validation_ok                   (struct dds_security_fsm *fsm, void *arg);
static void func_validation_failed               (struct dds_security_fsm *fsm, void *arg);
static void func_send_crypto_tokens_final        (struct dds_security_fsm *fsm, void *arg);
static void func_send_crypto_tokens              (struct dds_security_fsm *fsm, void *arg);

static dds_security_fsm_state state_initial_delay                       = { NULL,                      INITIAL_DELAY };
static dds_security_fsm_state state_validate_remote_and_begin_reply     = { func_validate_remote_and_begin_reply,  0 };
static dds_security_fsm_state state_validate_remote_identity            = { func_validate_remote_identity,         0 };
static dds_security_fsm_state state_validate_remote_identity_retry_wait = { NULL,                      RETRY_TIMEOUT };
static dds_security_fsm_state state_handshake_init_message_resend       = { func_handshake_init_message_resend,    0 };
static dds_security_fsm_state state_handshake_init_message_wait         = { NULL,                     RESEND_TIMEOUT };
static dds_security_fsm_state state_begin_handshake_reply               = { func_begin_handshake_reply,            0 };
static dds_security_fsm_state state_begin_handshake_reply_retry_wait    = { NULL,                      RETRY_TIMEOUT };
static dds_security_fsm_state state_begin_handshake_request             = { func_begin_handshake_request,          0 };
static dds_security_fsm_state state_begin_handshake_request_retry_wait  = { NULL,                      RETRY_TIMEOUT };
static dds_security_fsm_state state_process_handshake                   = { func_process_handshake,                0 };
static dds_security_fsm_state state_process_handshake_retry_wait        = { NULL,                      RETRY_TIMEOUT };
static dds_security_fsm_state state_handshake_message_resend            = { func_handshake_message_resend,         0 };
static dds_security_fsm_state state_handshake_message_wait              = { NULL,                     RESEND_TIMEOUT };
static dds_security_fsm_state state_validation_ok                       = { func_validation_ok,                    0 };
static dds_security_fsm_state state_validation_failed                   = { func_validation_failed,                0 };
static dds_security_fsm_state state_send_crypto_tokens_final_wait       = { NULL,                SEND_TOKENS_TIMEOUT };
static dds_security_fsm_state state_send_crypto_tokens_wait             = { NULL,                SEND_TOKENS_TIMEOUT };
static dds_security_fsm_state state_send_crypto_tokens_final            = { func_send_crypto_tokens_final,         0 };
static dds_security_fsm_state state_send_crypto_tokens                  = { func_send_crypto_tokens,               0 };
static dds_security_fsm_state state_wait_crypto_tokens                  = { NULL,                                  0 };
static dds_security_fsm_state state_handshake_final_resend              = { func_handshake_message_resend,         0 };

#ifdef VERBOSE_HANDSHAKE_DEBUG
static void handshake_fsm_debug(
    struct dds_security_fsm *fsm,
    DDS_SECURITY_FSM_DEBUG_ACT act,
    const dds_security_fsm_state *current,
    int event_id,
    void *arg)
{
  struct ddsi_handshake *handshake = arg;
  char *dispatch;
  char *state;
  char *event;

  assert(handshake);
  DDSRT_UNUSED_ARG(fsm);


  if      (current == NULL)                                        state = "NULL";
  else if (current == &state_initial_delay)                        state = "state_initial_delay";
  else if (current == &state_validate_remote_and_begin_reply)      state = "state_validate_remote_and_begin_reply";
  else if (current == &state_validate_remote_identity)             state = "state_validate_remote_identity";
  else if (current == &state_validate_remote_identity_retry_wait)  state = "state_validate_remote_identity_retry_wait";
  else if (current == &state_handshake_init_message_resend)        state = "state_handshake_init_message_resend";
  else if (current == &state_handshake_init_message_wait)          state = "state_handshake_init_message_wait";
  else if (current == &state_begin_handshake_reply)                state = "state_begin_handshake_reply";
  else if (current == &state_begin_handshake_reply_retry_wait)     state = "state_begin_handshake_reply_retry_wait";
  else if (current == &state_begin_handshake_request)              state = "state_begin_handshake_request";
  else if (current == &state_begin_handshake_request_retry_wait)   state = "state_begin_handshake_request_retry_wait";
  else if (current == &state_process_handshake)                    state = "state_process_handshake";
  else if (current == &state_process_handshake_retry_wait)         state = "state_process_handshake_retry_wait";
  else if (current == &state_handshake_message_resend)             state = "state_handshake_message_resend";
  else if (current == &state_handshake_message_wait)               state = "state_handshake_message_wait";
  else if (current == &state_validation_ok)                        state = "state_validation_ok";
  else if (current == &state_validation_failed)                    state = "state_validation_failed";
  else if (current == &state_send_crypto_tokens_final_wait)        state = "state_send_crypto_tokens_final_wait";
  else if (current == &state_send_crypto_tokens_wait)              state = "state_send_crypto_tokens_wait";
  else if (current == &state_send_crypto_tokens_final)             state = "state_send_crypto_tokens_final";
  else if (current == &state_send_crypto_tokens)                   state = "state_send_crypto_tokens";
  else if (current == &state_wait_crypto_tokens)                   state = "state_wait_crypto_tokens";
  else if (current == &state_handshake_final_resend)               state = "state_handshake_final_resend";
  else                                                             state = "else????";

  if      (event_id == EVENT_AUTO)                                 event = "EVENT_AUTO";
  else if (event_id == EVENT_TIMEOUT)                              event = "EVENT_TIMEOUT";
  else if (event_id == EVENT_VALIDATION_OK)                        event = "EVENT_VALIDATION_OK";
  else if (event_id == EVENT_VALIDATION_FAILED)                    event = "EVENT_VALIDATION_FAILED";
  else if (event_id == EVENT_VALIDATION_PENDING_RETRY)             event = "EVENT_VALIDATION_PENDING_RETRY";
  else if (event_id == EVENT_VALIDATION_PENDING_HANDSHAKE_REQUEST) event = "EVENT_VALIDATION_PENDING_HANDSHAKE_REQUEST";
  else if (event_id == EVENT_VALIDATION_PENDING_HANDSHAKE_MESSAGE) event = "EVENT_VALIDATION_PENDING_HANDSHAKE_MESSAGE";
  else if (event_id == EVENT_VALIDATION_OK_FINAL_MESSAGE)          event = "EVENT_VALIDATION_OK_FINAL_MESSAGE";
  else if (event_id == EVENT_RECEIVED_MESSAGE_REQUEST)             event = "EVENT_RECEIVED_MESSAGE_REQUEST";
  else if (event_id == EVENT_RECEIVED_MESSAGE_REPLY)               event = "EVENT_RECEIVED_MESSAGE_REPLY";
  else if (event_id == EVENT_RECEIVED_MESSAGE_FINAL)               event = "EVENT_RECEIVED_MESSAGE_FINAL";
  else if (event_id == EVENT_SEND_CRYPTO_TOKENS)                   event = "EVENT_SEND_CRYPTO_TOKENS";
  else if (event_id == EVENT_RECV_CRYPTO_TOKENS)                   event = "EVENT_RECV_CRYPTO_TOKENS";
  else                                                             event = "";

  if      (act == DDS_SECURITY_FSM_DEBUG_ACT_DISPATCH)             dispatch = "dispatching";
  else if (act == DDS_SECURITY_FSM_DEBUG_ACT_DISPATCH_DIRECT)      dispatch = "direct_dispatching";
  else if (act == DDS_SECURITY_FSM_DEBUG_ACT_HANDLING)             dispatch = "handling";
  else                                                             dispatch = "";

  HSTRACE ("FSM: handshake_debug (lguid="PGUIDFMT" rguid="PGUIDFMT") act=%s, state=%s, event=%s\n",
      PGUID (handshake->participants.lguid),
      PGUID (handshake->participants.rguid),
      dispatch,
      state,
      event);

}
#endif


/************************************************************************************************************
                            [START]
                               |
                     .---------------------.                                .----------------------------------------.
                     | state_initial_delay |                                | state_validate_remote_and_begin_reply  |
                     |---------------------|------------------------------->|----------------------------------------|------------------.
                     | initial_delay       |    RECEIVED_MESSAGE_REQUEST    | func_validate_remote_and_begin_reply() |                  |
                     '---------------------'                                '----------------------------------------'                  |
                               |                                                                                              VALIDATION_PENDING_RETRY
                            TIMEOUT                                                                                     VALIDATION_PENDING_HANDSHAKE_MESSAGE
                               |                                                                                                 VALIDATION_OK
                               v                                                                                                 VALIDATION_FAILED
               .---------------------------------.                                                                                      |
               | state_validate_remote_identity  |                                                                                      |
  .------------|---------------------------------|----------.--------------------.                                                      |
  |            | func_validate_remote_identity() |          |                    |                                                      |
  |            '---------------------------------'          |  VALIDATION_PENDING_HANDSHAKE_MESSAGE                                     |
VALIDATION_FAILED             ^  | VALIDATION_PENDING_RETRY |                    |                                                      |
VALIDATION_OK                 |  |                          |                    |                                                      |
  |                   TIMEOUT |  v                          |                    v                                                      |
  |       .-------------------------------------------.     |  .-----------------------------------.                                    |
  |       | state_validate_remote_identity_retry_wait |     |  | state_handshake_init_message_wait |<---------------.                   |
  |       |-------------------------------------------|     |  |-----------------------------------|           AUTO |                   |
  |       | retry_timeout                             |     |  | resend_timeout                    |---------.      |                   |
  |       '-------------------------------------------'     |  '-----------------------------------' TIMEOUT |      |                   |
  |                                                         |                    |                           |      |                   |
  |                     .-----------------------------------'                    |                           |      |                   |
  |                     |      VALIDATION_PENDING_HANDSHAKE_REQUEST              |                           v      |                   |
  |                     |                                                        |       .--------------------------------------.       |
  |                     v                                      RECEIVED_MESSAGE_REQUEST  | state_handshake_init_message_resend  |       |
  |    .--------------------------------.                                        |       |--------------------------------------|       |
  |    | state_begin_handshake_request  | VALIDATION_PENDING_RETRY               |       | func_handshake_init_message_resend() |       |
  |    |--------------------------------|------------.                           |       '--------------------------------------'       |
  |    | func_begin_handshake_request() |            |                           |                           ^                          |
  |    '--------------------------------'            |                           |                           |                          |
  |        |            |        ^                   |                           |                           |                          |
  |        |            |        | TIMEOUT           v                           |                           |                          |
  | VALIDATION_FAILED   |      .------------------------------------------.      |                           |                          |
  | VALIDATION_OK       |      | state_begin_handshake_request_retry_wait |      |                           |                          |
  |        |            |      |------------------------------------------|      |                           |                          |
  |--------'            |      | retry_timeout                            |      |                           |                          |
  |                     |      '------------------------------------------'      |                           |                          |
  |                     |                                                        v                  VALIDATION_FAILED                   |
  |                     |                                        .------------------------------.            |                          |
  |   VALIDATION_PENDING_HANDSHAKE_MESSAGE                       | state_begin_handshake_reply  |------------'                          |
  |                     |                                .-------|------------------------------|                                       |
  |                     |                                |       | func_begin_handshake_reply() |------------.                          |
  |                     |                                |       '------------------------------'            |                          |
  |                     |                                |       VALIDATION_OK |              ^     VALIDATION_PENDING_RETRY            |
  |                     |                                |                     |              |              |                          |
  |                     |                                |                     |              |              | VALIDATION_PENDING_RETRY |
  |                     |     VALIDATION_PENDING_HANDSHAKE_MESSAGE             v              | TIMEOUT      |--------------------------|
  |                     |                                |         goto state_validation_ok   |              |                          |
  |                     |                                v                                    |              v                          |
  |                     |                .------------------------------.            .------------------------------------------.       |
  |                     |                | state_handshake_message_wait |<--------.  | state_begin_handshake_reply_retry_wait   |       |
  |                     .--------------->|------------------------------|------.  |  |------------------------------------------|       |
  |                     |                | resend_timeout               |      |  |  | retry_timeout                            |       |
  |                     |                '------------------------------'      |  |  '------------------------------------------'       |
  |                     |                                |          ^          |  |                                                     |
  |                     | AUTO                           |          |          |  |        VALIDATION_PENDING_HANDSHAKE_MESSAGE         |
  |                     |                                |          |          |  '-----------------------------------------------------|
  |                     |                        TIMEOUT |          |          |                                                        |
  |    .---------------------------------.               |          |          | RECEIVED_MESSAGE_REPLY                                 |
  |    | state_handshake_message_resend  |               |  VALIDATION_FAILED  | RECEIVED_MESSAGE_FINAL                                 |
  |    |---------------------------------|<--------------'          |          |                                                        |
  |    | func_handshake_message_resend() |                          |          v                                                        |
  |    '---------------------------------'                        .--------------------------.                                          |
  |                                                               | state_process_handshake  |                                          |
  |                              .--------------------------------|--------------------------|--------------------------.               |
  |                              |            .------------------>| func_process_handshake() |                          |               |
  |                              |            |                   '--------------------------'                          |               |
  |                              |            |                                 |                                       |               |
  |          VALIDATION_PENDING_RETRY      TIMEOUT                VALIDATION_OK |                                       |               |
  |                              v            |                                 v                                       |               |
  |           .------------------------------------.            .-------------------------------.                       |               |
  |           | state_process_handshake_retry_wait |            | state_send_crypto_tokens_wait |                       |               |
  |           |------------------------------------|            |-------------------------------|                       |               |
  |           | retry_timeout                      |            | send_tokens_timeout           |                       |               |
  |           '------------------------------------'            '-------------------------------'                       |               |
  |                                                                     |              |              VALIDATION_OK_FINAL_MESSAGE       |
  |                                                       .-------------'              '---------.                      |               |
  |                                                       | RECV_CRYPTO_TOKENS           TIMEOUT |                      |               |
  |                                                       v                                      v                      |               |
  |                                      .---------------------------------.       .---------------------------.        |               |
  |                                      | state_send_crypto_tokens_final  |       | state_send_crypto_tokens  |        |               |
  |                       .--------------|---------------------------------|       |---------------------------|        |               |
  |                       |              | func_send_crypto_tokens_final() |       | func_send_crypto_tokens() |        |               |
  |                       |              '---------------------------------'       '---------------------------'        |               |
  |                       |                               ^                                      |                      |               |
  |                   VALIDATION_OK                       |       .--------------------.   VALIDATION_OK_FINAL_MESSAGE  |               |
  |                       |                       TIMEOUT |       | RECV_CRYPTO_TOKENS |         |                      |               |
  |                       |                               |       v                    |         v                      |               |
  |                       |            .-------------------------------------.     .--------------------------.         |               |
  |                       |            | state_send_crypto_tokens_final_wait |     | state_wait_crypto_tokens |<--------'               |
  |                       |            |-------------------------------------|     |--------------------------|                         |
  |                       |            | send_tokens_timeout                 |     |                          |---------.               |
  |                       |            '-------------------------------------'     '--------------------------'         |               |
  |                       |                               ^                              |               ^              |               |
  |                       |                               |                  RECEIVED_MESSAGE_REPLY     AUTO      VALIDATION_OK         |
  |                       |                      RECV_CRYPTO_TOKENS                      v               |              |               |
  |                       |                               |                     .---------------------------------.     |               |
  |                       |                               |                     |  state_handshake_final_resend   |     |               |
  |                       |                               '---------------------|---------------------------------|     |               |
  | VALIDATION_OK         |                                                     | func_handshake_message_resend() |     |               |
  |---------------------------------------------------------.                   '---------------------------------'     |               |
  |                                                         |                                                           |               |
  '---------------.                                         |                                                           |               |
VALIDATION_FAILED |                                         |                                                           |               |
                  v                                         v                                                           |               |
            .--------------------------.               .----------------------.                                         |               |
            | state_validation_failed  |               | state_validation_ok  |                                         |               |
            |--------------------------|               |----------------------|<----------------------------------------'               |
            | func_validation_failed() |               | func_validation_ok() |                                                         |
            '--------------------------'               '----------------------'                                                         |
                          |         ^                              |       ^                                                            |
                          v         |                              v       | VALIDATION_OK                                              |
                        [END]       |                            [END]     |                                                            |
                                    |                                      |                                                            |
                                    |       VALIDATION_FAILED              |                                                            |
                                    '---------------------------------------------------------------------------------------------------'

 .----------------------------------------.
 | state_begin_handshake_reply_retry_wait |
 |----------------------------------------|
 | retry_timeout                          |
 '----------------------------------------'


*************************************************************************************************************/
static const dds_security_fsm_transition handshake_transistions [] =
{   /* Start */
    { NULL,                                 EVENT_AUTO,                                 NULL,
                                            &state_initial_delay                           },
    /* initial delay: a short delay to give the remote node some time for matching the
       BuiltinParticipantStatelessMessageWriter, so that it won't drop the auth_request
       we're sending (that would result in a time-out and the handshake taking longer
       than required). For the node that receives the auth_request, the transition for
       the event EVENT_RECEIVED_MESSAGE_REQUEST is added, because that node may receive the
       auth_request during this delay and can continue immediately (as the sender already
       waited for this delay before sending the request) */
    { &state_initial_delay,                 EVENT_TIMEOUT,                              NULL,
                                            &state_validate_remote_identity                },
    { &state_initial_delay,                 EVENT_RECEIVED_MESSAGE_REQUEST,             NULL,
                                            &state_validate_remote_and_begin_reply         },
    /* validate remote and begin reply */
    { &state_validate_remote_and_begin_reply, EVENT_VALIDATION_PENDING_RETRY,             NULL,
                                            &state_begin_handshake_reply_retry_wait          },
    { &state_validate_remote_and_begin_reply, EVENT_VALIDATION_FAILED,                    NULL,
                                            &state_handshake_init_message_resend             },
    { &state_validate_remote_and_begin_reply, EVENT_VALIDATION_OK,                        NULL,
                                            &state_validation_ok                             },
    { &state_validate_remote_and_begin_reply, EVENT_VALIDATION_PENDING_HANDSHAKE_MESSAGE, NULL,
                                            &state_handshake_message_wait                    },
    /* validate remote identity */
    { &state_validate_remote_identity,      EVENT_VALIDATION_PENDING_RETRY,             NULL,
                                            &state_validate_remote_identity_retry_wait     },
    { &state_validate_remote_identity,      EVENT_VALIDATION_FAILED,                    NULL,
                                            &state_validation_failed                       },
    { &state_validate_remote_identity,      EVENT_VALIDATION_PENDING_HANDSHAKE_REQUEST, NULL,
                                            &state_begin_handshake_request                 },
    { &state_validate_remote_identity,      EVENT_VALIDATION_OK,                        NULL,
                                            &state_validation_ok                           },
    { &state_validate_remote_identity,      EVENT_VALIDATION_PENDING_HANDSHAKE_MESSAGE, NULL,
                                            &state_handshake_init_message_wait             },
    /* ValRemIdentityRetryWait */
    { &state_validate_remote_identity_retry_wait, EVENT_TIMEOUT,                        NULL,
                                            &state_validate_remote_identity                },
    /* HandshakeInitMessageWait */
    { &state_handshake_init_message_wait,   EVENT_TIMEOUT,                              NULL,
                                            &state_handshake_init_message_resend           },
    { &state_handshake_init_message_wait,   EVENT_RECEIVED_MESSAGE_REQUEST,             NULL,
                                            &state_begin_handshake_reply                   },
    /* resend message */
    { &state_handshake_init_message_resend, EVENT_AUTO,                                 NULL,
                                            &state_handshake_init_message_wait             },
    /* begin handshake reply */
    { &state_begin_handshake_reply,         EVENT_VALIDATION_PENDING_RETRY,             NULL,
                                            &state_begin_handshake_reply_retry_wait        },
    { &state_begin_handshake_reply,         EVENT_VALIDATION_FAILED,                    NULL,
                                            &state_handshake_init_message_resend           },
    { &state_begin_handshake_reply,         EVENT_VALIDATION_PENDING_HANDSHAKE_MESSAGE, NULL,
                                            &state_handshake_message_wait                  },
    { &state_begin_handshake_reply,         EVENT_VALIDATION_OK,                        NULL,
                                            &state_validation_ok                           },
    /* BeginHsRepRetryWait */
    { &state_begin_handshake_reply_retry_wait, EVENT_TIMEOUT,                           NULL,
                                            &state_begin_handshake_reply                   },
    /* begin handshake request */
    { &state_begin_handshake_request,       EVENT_VALIDATION_PENDING_RETRY,             NULL,
                                            &state_begin_handshake_request_retry_wait      },
    { &state_begin_handshake_request,       EVENT_VALIDATION_FAILED,                    NULL,
                                            &state_validation_failed                       },
    { &state_begin_handshake_request,       EVENT_VALIDATION_PENDING_HANDSHAKE_MESSAGE, NULL,
                                            &state_handshake_message_wait                  },
    { &state_begin_handshake_request,       EVENT_VALIDATION_OK,                        NULL,
                                            &state_validation_ok                           },
    /* BeginHsReqRetryWait */
    { &state_begin_handshake_request_retry_wait, EVENT_TIMEOUT,                         NULL,
                                            &state_begin_handshake_request                 },
    /* HandshakeMessageWait */
    { &state_handshake_message_wait,        EVENT_TIMEOUT,                              NULL,
                                            &state_handshake_message_resend                },
    { &state_handshake_message_wait,        EVENT_RECEIVED_MESSAGE_REPLY,               NULL,
                                            &state_process_handshake                       },
    { &state_handshake_message_wait,        EVENT_RECEIVED_MESSAGE_FINAL,               NULL,
                                            &state_process_handshake                       },
    /* resend message */
    { &state_handshake_message_resend,      EVENT_AUTO,                                 NULL,
                                            &state_handshake_message_wait                  },
    /* process handshake */
    { &state_process_handshake,             EVENT_VALIDATION_PENDING_RETRY,             NULL,
                                            &state_process_handshake_retry_wait            },
    { &state_process_handshake,             EVENT_VALIDATION_FAILED,                    NULL,
                                            &state_handshake_message_wait                  },
    { &state_process_handshake,             EVENT_VALIDATION_OK,                        NULL,
                                            &state_send_crypto_tokens_wait                 },
    { &state_process_handshake,             EVENT_VALIDATION_OK_FINAL_MESSAGE,          NULL,
                                            &state_wait_crypto_tokens                      },
    /* ProcessHsRetryWait */
    { &state_process_handshake_retry_wait,  EVENT_TIMEOUT,                              NULL,
                                            &state_process_handshake                       },

    { &state_send_crypto_tokens_wait,       EVENT_TIMEOUT,                              NULL,
                                            &state_send_crypto_tokens                      },
    { &state_send_crypto_tokens_wait,       EVENT_RECV_CRYPTO_TOKENS,                   NULL,
                                            &state_send_crypto_tokens_final                },

    { &state_send_crypto_tokens_final_wait, EVENT_TIMEOUT,                              NULL,
                                            &state_send_crypto_tokens_final                },

    /* Process_handshake returned VALIDATION_OK_FINAL_MESSAGE notify user and wait for tokens */
    { &state_send_crypto_tokens,            EVENT_VALIDATION_OK_FINAL_MESSAGE,          NULL,
                                            &state_wait_crypto_tokens                      },
    /* Process_handshake returned VALIDATION_OK notify user and goto ready state */
    { &state_send_crypto_tokens_final,      EVENT_VALIDATION_OK,                        NULL,
                                            &state_validation_ok                           },

    { &state_handshake_final_resend,        EVENT_AUTO,                                 NULL,
                                            &state_wait_crypto_tokens                      },

    { &state_handshake_final_resend,        EVENT_RECV_CRYPTO_TOKENS,                   NULL,
                                            &state_send_crypto_tokens_final_wait           },

    { &state_wait_crypto_tokens,            EVENT_RECV_CRYPTO_TOKENS,                   NULL,
                                            &state_send_crypto_tokens_final_wait           },

    { &state_wait_crypto_tokens,            EVENT_VALIDATION_OK,                        NULL,
                                            &state_validation_ok                           },

    { &state_wait_crypto_tokens,            EVENT_RECEIVED_MESSAGE_REPLY,               NULL,
                                            &state_handshake_final_resend                  },

    /* End */
    { &state_validation_ok,                 EVENT_AUTO,                                 NULL,
                                            NULL                                           },
    { &state_validation_failed,             EVENT_AUTO,                                 NULL,
                                            NULL                                           },
};



static bool send_handshake_message(const struct ddsi_handshake *handshake, DDS_Security_DataHolder *token, struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp, int request)
{
  bool ret = false;
  ddsi_dataholderseq_t mdata;
  DDS_Security_DataHolderSeq tseq;

  tseq._length = tseq._maximum = 1;
  tseq._buffer = token;

  ddsi_omg_shallow_copyout_DataHolderSeq (&mdata, &tseq);

  if (!(ret = ddsi_write_auth_handshake_message(pp, proxypp, &mdata, request, &handshake->handshake_message_in_id)))
  {
    HSWARNING("Send handshake: failed to send message (lguid="PGUIDFMT" rguid="PGUIDFMT")", PGUID (pp->e.guid), PGUID (proxypp->e.guid));
  }

  ddsi_omg_shallow_free_ddsi_dataholderseq (&mdata);

  return ret;
}

static DDS_Security_ValidationResult_t validate_remote_identity_impl(struct ddsi_handshake *handshake, dds_security_authentication *auth,
    struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp)
{
  DDS_Security_ValidationResult_t ret;
  DDS_Security_IdentityToken remote_identity_token;
  int64_t remote_identity_handle;
  ddsi_guid_t remote_guid;
  DDS_Security_SecurityException exception = {0};

  if (!(proxypp->plist->present & PP_IDENTITY_TOKEN))
  {
    HSERROR("validate remote identity failed: remote participant ("PGUIDFMT") identity token missing", PGUID (proxypp->e.guid));
    ret = DDS_SECURITY_VALIDATION_FAILED;
    goto ident_token_missing;
  }

  remote_guid = ddsi_hton_guid(proxypp->e.guid);
  ddsi_omg_security_dataholder_copyout (&remote_identity_token, &proxypp->plist->identity_token);

  ddsrt_mutex_lock(&handshake->lock);
  ret = auth->validate_remote_identity(
      auth, &remote_identity_handle, &handshake->local_auth_request_token, handshake->remote_auth_request_token,
      pp->sec_attr->local_identity_handle, &remote_identity_token, (DDS_Security_GUID_t *)&remote_guid, &exception);
  ddsrt_mutex_unlock(&handshake->lock);

  /* Trace a failed handshake. */
  if ((ret != DDS_SECURITY_VALIDATION_OK                       ) &&
      (ret != DDS_SECURITY_VALIDATION_PENDING_RETRY            ) &&
      (ret != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_REQUEST) &&
      (ret != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE))
  {
    HSEXCEPTION(&exception, "Validate remote identity failed");
    ret = DDS_SECURITY_VALIDATION_FAILED;
    goto validation_failed;
  }

  HSTRACE("FSM: validate_remote_identity (lguid="PGUIDFMT" rguid="PGUIDFMT") ret=%d\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid), ret);

  assert(proxypp->sec_attr->remote_identity_handle == 0 || proxypp->sec_attr->remote_identity_handle == remote_identity_handle);
  proxypp->sec_attr->remote_identity_handle = remote_identity_handle;

  DDS_Security_DataHolder_deinit(&remote_identity_token);

  /* When validate_remote_identity returns a local_auth_request_token
   * which does not equal TOKEN_NIL then an AUTH_REQUEST message has
   * to be send.
   */
  if (handshake->local_auth_request_token.class_id && strlen(handshake->local_auth_request_token.class_id) != 0)
    (void)send_handshake_message(handshake, &handshake->local_auth_request_token, pp, proxypp, 1);

ident_token_missing:
validation_failed:
  return ret;
}

static void func_validate_remote_identity(struct dds_security_fsm *fsm, void *arg)
{
  DDS_Security_ValidationResult_t ret;
  struct ddsi_handshake *handshake = (struct ddsi_handshake*)arg;
  dds_security_authentication *auth = handshake->auth;
  struct ddsi_participant *pp;
  struct ddsi_proxy_participant *proxypp;

  if (!validate_handshake(handshake, &pp, &proxypp))
    return;

  TRACE_FUNC(fsm);

  ret = validate_remote_identity_impl(handshake, auth, pp, proxypp);
  dds_security_fsm_dispatch(fsm, (int32_t)ret, true);
}

static void func_handshake_init_message_resend(struct dds_security_fsm *fsm, void *arg)
{
  struct ddsi_handshake *handshake = arg;
  struct ddsi_participant *pp;
  struct ddsi_proxy_participant *proxypp;

  DDSRT_UNUSED_ARG(fsm);

  if (!validate_handshake(handshake, &pp, &proxypp))
    return;

  TRACE_FUNC(fsm);

  HSTRACE("FSM: handshake init_message_resend (lguid="PGUIDFMT" rguid="PGUIDFMT")\n", PGUID(pp->e.guid), PGUID(proxypp->e.guid));

  if (strlen(handshake->local_auth_request_token.class_id) != 0)
    (void)send_handshake_message(handshake, &handshake->local_auth_request_token, pp, proxypp, 1);
}

static DDS_Security_ValidationResult_t begin_handshake_reply_impl(struct ddsi_handshake *handshake, dds_security_authentication *auth,
    struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp)
{
  DDS_Security_ValidationResult_t ret;
  DDS_Security_SecurityException exception = {0};

  ddsrt_mutex_lock(&handshake->lock);

  if (handshake->handshake_message_out)
    DDS_Security_DataHolder_free(handshake->handshake_message_out);
  handshake->handshake_message_out = DDS_Security_DataHolder_alloc();

  ret = auth->begin_handshake_reply(
      auth, &(handshake->handshake_handle), handshake->handshake_message_out, &handshake->handshake_message_in_token,
      proxypp->sec_attr->remote_identity_handle, pp->sec_attr->local_identity_handle, &handshake->pdata, &exception);

  ddsrt_mutex_unlock(&handshake->lock);

  HSTRACE("FSM: begin_handshake_reply (lguid="PGUIDFMT" rguid="PGUIDFMT") ret=%d\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid), ret);

  /* Trace a failed handshake. */
  if (ret != DDS_SECURITY_VALIDATION_OK
      && ret != DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE
      && ret != DDS_SECURITY_VALIDATION_PENDING_RETRY
      && ret != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE)
  {
    HSEXCEPTION(&exception, "Begin handshake reply failed");
    goto handshake_failed;
  }

  if (ret == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE)
  {
    if (!send_handshake_message(handshake, handshake->handshake_message_out, pp, proxypp, 0))
      goto handshake_failed;
  }
  else if (ret == DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE)
  {
    if (send_handshake_message(handshake, handshake->handshake_message_out, pp, proxypp, 0))
      ret = DDS_SECURITY_VALIDATION_OK;
    else
      goto handshake_failed;
  }

  if (ret == DDS_SECURITY_VALIDATION_OK)
  {
    handshake->shared_secret = auth->get_shared_secret(auth, handshake->handshake_handle, &exception);
    if (handshake->shared_secret == DDS_SECURITY_HANDLE_NIL)
    {
      HSEXCEPTION(&exception, "Getting shared secret failed");
      goto handshake_failed;
    }
  }
  return ret;

handshake_failed:
  DDS_Security_DataHolder_free(handshake->handshake_message_out);
  handshake->handshake_message_out = NULL;
  return DDS_SECURITY_VALIDATION_FAILED;
}

static void func_begin_handshake_reply(struct dds_security_fsm *fsm, void *arg)
{
  DDS_Security_ValidationResult_t ret;
  struct ddsi_handshake *handshake = arg;
  dds_security_authentication *auth = handshake->auth;
  struct ddsi_participant *pp;
  struct ddsi_proxy_participant *proxypp;

  if (!validate_handshake(handshake, &pp, &proxypp))
     return;

  TRACE_FUNC(fsm);

  ret = begin_handshake_reply_impl(handshake, auth, pp, proxypp);
  dds_security_fsm_dispatch(fsm, (int32_t)ret, true);
}

static void func_validate_remote_and_begin_reply(struct dds_security_fsm *fsm, void *arg)
{
  DDS_Security_ValidationResult_t ret;
  struct ddsi_handshake *handshake = arg;
  dds_security_authentication *auth = handshake->auth;
  struct ddsi_participant *pp;
  struct ddsi_proxy_participant *proxypp;

  if (!validate_handshake(handshake, &pp, &proxypp))
    return;

  TRACE_FUNC(fsm);

  ret = validate_remote_identity_impl(handshake, auth, pp, proxypp);
  /* In the only path to this state an auth_request is received so the result
     of validate_remote_identity should be PENDING_HANDSHAKE_MESSAGE, or failed
     in case of an error. */
  if (ret != DDS_SECURITY_VALIDATION_FAILED)
  {
    if (ret != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE)
    {
      HSWARNING("func_validate_remote_and_begin_reply: invalid result %d from validate_remote_identity", ret);
      ret = DDS_SECURITY_VALIDATION_FAILED;
    }
    else
      ret = begin_handshake_reply_impl(handshake, auth, pp, proxypp);
  }
  dds_security_fsm_dispatch(fsm, (int32_t)ret, true);
}

static void func_begin_handshake_request(struct dds_security_fsm *fsm, void *arg)
{
  DDS_Security_ValidationResult_t ret;
  DDS_Security_SecurityException exception = {0};
  struct ddsi_handshake *handshake = arg;
  dds_security_authentication *auth = handshake->auth;
  struct ddsi_participant *pp;
  struct ddsi_proxy_participant *proxypp;

  if (!validate_handshake(handshake, &pp, &proxypp))
    return;

  TRACE_FUNC(fsm);

  ddsrt_mutex_lock(&handshake->lock);

  if (handshake->handshake_message_out)
    DDS_Security_DataHolder_free(handshake->handshake_message_out);
  handshake->handshake_message_out = DDS_Security_DataHolder_alloc();

  ret = auth->begin_handshake_request(auth, &(handshake->handshake_handle), handshake->handshake_message_out, pp->sec_attr->local_identity_handle, proxypp->sec_attr->remote_identity_handle, &handshake->pdata, &exception);
  ddsrt_mutex_unlock(&handshake->lock);

  HSTRACE("FSM: begin_handshake_request (lguid="PGUIDFMT" rguid="PGUIDFMT") ret=%d\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid), ret);

  /* Trace a failed handshake. */
  if ((ret != DDS_SECURITY_VALIDATION_OK                       ) &&
      (ret != DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE         ) &&
      (ret != DDS_SECURITY_VALIDATION_PENDING_RETRY            ) &&
      (ret != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE))
  {
    HSEXCEPTION(&exception, "Begin handshake request failed");
    ret = DDS_SECURITY_VALIDATION_FAILED;
    goto handshake_failed;
  }

  if (ret == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE)
  {
    if (!send_handshake_message(handshake, handshake->handshake_message_out, pp, proxypp, 0))
    {
      ret = DDS_SECURITY_VALIDATION_FAILED;
      goto handshake_failed;
    }
  }
  else if (ret == DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE)
  {
    if (send_handshake_message(handshake, handshake->handshake_message_out, pp, proxypp, 0))
    {
      ret = DDS_SECURITY_VALIDATION_OK;
    } else {
      ret = DDS_SECURITY_VALIDATION_FAILED;
      goto handshake_failed;
    }
  }

  if (ret == DDS_SECURITY_VALIDATION_OK)
  {
    handshake->shared_secret = auth->get_shared_secret(auth, handshake->handshake_handle, &exception);
    if (handshake->shared_secret == DDS_SECURITY_HANDLE_NIL)
    {
      HSEXCEPTION(&exception, "Getting shared secret failed");
      ret = DDS_SECURITY_VALIDATION_FAILED;
      goto handshake_failed;
    }
  }

  dds_security_fsm_dispatch(fsm, (int32_t)ret, true);
  return;

handshake_failed:
  DDS_Security_DataHolder_free(handshake->handshake_message_out);
  handshake->handshake_message_out = NULL;
  /* Use return value as state machine event. */
  dds_security_fsm_dispatch(fsm, (int32_t)ret, true);
}

static void func_process_handshake(struct dds_security_fsm *fsm, void *arg)
{
  DDS_Security_ValidationResult_t ret;
  DDS_Security_SecurityException exception = {0};
  struct ddsi_handshake *handshake = arg;
  dds_security_authentication *auth = handshake->auth;
  struct ddsi_participant *pp;
  struct ddsi_proxy_participant *proxypp;

  if (!validate_handshake(handshake, &pp, &proxypp))
    return;

  TRACE_FUNC(fsm);

  ddsrt_mutex_lock(&handshake->lock);

  if (handshake->handshake_message_out)
    DDS_Security_DataHolder_free(handshake->handshake_message_out);
  handshake->handshake_message_out = DDS_Security_DataHolder_alloc();

  ret = auth->process_handshake(auth, handshake->handshake_message_out, &handshake->handshake_message_in_token, handshake->handshake_handle, &exception);
  ddsrt_mutex_unlock(&handshake->lock);

  HSTRACE("FSM: process_handshake (lguid="PGUIDFMT" rguid="PGUIDFMT") ret=%d\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid), ret);

  /* Trace a failed handshake. */
  if ((ret != DDS_SECURITY_VALIDATION_OK                       ) &&
      (ret != DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE         ) &&
      (ret != DDS_SECURITY_VALIDATION_PENDING_RETRY            ))
  {
    HSEXCEPTION(&exception, "Process handshake failed");
    ret = DDS_SECURITY_VALIDATION_FAILED;
    goto handshake_failed;
  }

  if ((ret == DDS_SECURITY_VALIDATION_OK) || (ret == DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE))
  {
    handshake->shared_secret = auth->get_shared_secret(auth, handshake->handshake_handle, &exception);
    if (handshake->shared_secret == DDS_SECURITY_HANDLE_NIL)
    {
      HSEXCEPTION(&exception, "Getting shared secret failed");
      ret = DDS_SECURITY_VALIDATION_FAILED;
      goto handshake_failed;
    }
    handshake->end_cb(handshake, pp, proxypp, STATE_HANDSHAKE_PROCESSED);
  }

  if (ret == DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE)
  {
    if (!send_handshake_message(handshake, handshake->handshake_message_out, pp, proxypp, 0))
    {
      ret = DDS_SECURITY_VALIDATION_FAILED;
      goto handshake_failed;
    }
  }

  dds_security_fsm_dispatch(fsm, (int32_t)ret, true);
  return;

handshake_failed:
  DDS_Security_DataHolder_free(handshake->handshake_message_out);
  handshake->handshake_message_out = NULL;
  /* Use return value as state machine event. */
  dds_security_fsm_dispatch(fsm, (int32_t)ret, true);
}

static void func_send_crypto_tokens(struct dds_security_fsm *fsm, void *arg)
{
  struct ddsi_handshake *handshake = arg;
  struct ddsi_participant *pp;
  struct ddsi_proxy_participant *proxypp;

  if (!validate_handshake(handshake, &pp, &proxypp))
    return;

  TRACE_FUNC(fsm);

  /* The final handshake message has been send to the remote
   * participant. Call the callback function to signal that
   * the corresponding crypto tokens can be send. Amd start
   * waiting for the crypto tokens from the remote participant
   */

  HSTRACE("FSM: handshake send crypto tokens (lguid="PGUIDFMT" rguid="PGUIDFMT")\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid));
  handshake->end_cb(handshake, pp, proxypp, STATE_HANDSHAKE_SEND_TOKENS);
  dds_security_fsm_dispatch(fsm, EVENT_VALIDATION_OK_FINAL_MESSAGE, true);
}

static void func_send_crypto_tokens_final(struct dds_security_fsm *fsm, void *arg)
{
  struct ddsi_handshake *handshake = arg;
  struct ddsi_participant *pp;
  struct ddsi_proxy_participant *proxypp;

  if (!validate_handshake(handshake, &pp, &proxypp))
    return;

  TRACE_FUNC(fsm);

  /* The final handshake message has been send to the remote
   * participant. Call the callback function to signal that
   * the corresponding crypto tokens can be send. Amd start
   * waiting for the crypto tokens from the remote participant
   */

  HSTRACE("FSM: handshake send crypto tokens final (lguid="PGUIDFMT" rguid="PGUIDFMT")\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid));
  handshake->end_cb(handshake, pp, proxypp, STATE_HANDSHAKE_SEND_TOKENS);
  dds_security_fsm_dispatch(fsm, EVENT_VALIDATION_OK, true);
}

static void func_handshake_message_resend(struct dds_security_fsm *fsm, void *arg)
{
  struct ddsi_handshake *handshake = arg;
  struct ddsi_participant *pp;
  struct ddsi_proxy_participant *proxypp;

  DDSRT_UNUSED_ARG(fsm);
  DDSRT_UNUSED_ARG(arg);

  if (!validate_handshake(handshake, &pp, &proxypp))
    return;

  TRACE_FUNC(fsm);

  HSTRACE("handshake resend (lguid="PGUIDFMT" rguid="PGUIDFMT")\n", PGUID(pp->e.guid), PGUID(proxypp->e.guid));
  if (handshake->handshake_message_out) {
    (void)send_handshake_message(handshake, handshake->handshake_message_out, pp, proxypp, 0);
  }
}

static void func_validation_ok(struct dds_security_fsm *fsm, void *arg)
{
  struct ddsi_handshake *handshake = arg;
  struct ddsi_participant *pp;
  struct ddsi_proxy_participant *proxypp;

  DDSRT_UNUSED_ARG(fsm);

  if (!validate_handshake(handshake, &pp, &proxypp))
    return;

  TRACE_FUNC(fsm);

  HSTRACE("FSM: handshake succeeded (lguid="PGUIDFMT" rguid="PGUIDFMT")\n", PGUID(pp->e.guid), PGUID(proxypp->e.guid));
  handshake->state = STATE_HANDSHAKE_OK;
  handshake->end_cb(handshake, pp, proxypp, STATE_HANDSHAKE_OK);
}

static void func_validation_failed(struct dds_security_fsm *fsm, void *arg)
{
  struct ddsi_handshake *handshake = arg;
  struct ddsi_participant *pp;
  struct ddsi_proxy_participant *proxypp;

  DDSRT_UNUSED_ARG(fsm);

  if (!validate_handshake(handshake, &pp, &proxypp))
    return;

  TRACE_FUNC(fsm);

  HSTRACE("FSM: handshake failed (lguid="PGUIDFMT" rguid="PGUIDFMT")\n", PGUID(pp->e.guid), PGUID(proxypp->e.guid));
  handshake->state = STATE_HANDSHAKE_FAILED;
  handshake->end_cb(handshake, pp, proxypp, STATE_HANDSHAKE_FAILED);
}

static void func_handshake_timeout(struct dds_security_fsm *fsm, void *arg)
{
  struct ddsi_handshake *handshake = arg;
  struct ddsi_participant *pp;
  struct ddsi_proxy_participant *proxypp;

  DDSRT_UNUSED_ARG(fsm);

  if (!validate_handshake(handshake, &pp, &proxypp))
    return;

  TRACE_FUNC(fsm);

  HSTRACE("FSM: handshake timeout (lguid="PGUIDFMT" rguid="PGUIDFMT")\n", PGUID(pp->e.guid), PGUID(proxypp->e.guid));
  handshake->state = STATE_HANDSHAKE_TIMED_OUT;
  handshake->end_cb(handshake, pp, proxypp, STATE_HANDSHAKE_TIMED_OUT);
}


static struct ddsi_handshake * ddsi_handshake_create(struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp, ddsi_handshake_end_cb_t callback)
{
  const struct ddsi_domaingv * gv = pp->e.gv;
  struct ddsi_hsadmin *hsadmin = pp->e.gv->hsadmin;
  struct ddsi_handshake *handshake;
  dds_return_t rc;
  ddsi_octetseq_t pdata;

  TRACE_FUNC(NULL);

  handshake = ddsrt_malloc(sizeof(struct ddsi_handshake));
  memset(handshake, 0, sizeof(struct ddsi_handshake));

  ddsrt_mutex_init(&handshake->lock);
  handshake->auth = ddsi_omg_participant_get_authentication(pp);
  ddsrt_atomic_st32(&handshake->refc, 1);
  ddsrt_atomic_st32(&handshake->deleting, 0);
  handshake->participants.lguid = pp->e.guid;
  handshake->participants.rguid = proxypp->e.guid;
  handshake->gv = gv;
  handshake->handshake_handle = 0;
  handshake->shared_secret = 0;
  ddsi_auth_get_serialized_participant_data(pp, &pdata);

  handshake->pdata._length =  handshake->pdata._maximum = pdata.length;
  handshake->pdata._buffer = pdata.value;

  handshake->end_cb = callback;

  handshake->state = STATE_HANDSHAKE_IN_PROGRESS;
  if (!hsadmin->fsm_control)
  {
    hsadmin->fsm_control = dds_security_fsm_control_create(pp->e.gv);
    rc = dds_security_fsm_control_start(hsadmin->fsm_control, NULL);
    if (rc < 0)
    {
      GVERROR("Failed to create FSM control");
      goto fsm_control_failed;
    }
  }

  handshake->fsm = dds_security_fsm_create(hsadmin->fsm_control, handshake_transistions, sizeof(handshake_transistions)/sizeof(handshake_transistions[0]), handshake);
  if (!handshake->fsm)
  {
    GVERROR("Failed to create FSM");
    goto fsm_failed;
  }
  dds_security_fsm_set_timeout(handshake->fsm, func_handshake_timeout, AUTHENTICATION_TIMEOUT);

#ifdef VERBOSE_HANDSHAKE_DEBUG
  dds_security_fsm_set_debug(handshake->fsm, handshake_fsm_debug);
#endif
  dds_security_fsm_start(handshake->fsm);

  return handshake;

fsm_failed:
fsm_control_failed:
  ddsrt_free(handshake);
  return NULL;
}

void ddsi_handshake_release(struct ddsi_handshake *handshake)
{
  if (!handshake) return;

  if (ddsrt_atomic_dec32_nv(&handshake->refc) == 0)
  {
    HSTRACE("handshake delete (lguid="PGUIDFMT" rguid="PGUIDFMT")\n", PGUID(handshake->participants.lguid), PGUID(handshake->participants.rguid));
    DDS_Security_DataHolder_deinit(&handshake->local_auth_request_token);
    DDS_Security_DataHolder_deinit(&handshake->handshake_message_in_token);
    DDS_Security_DataHolder_free(handshake->handshake_message_out);
    DDS_Security_DataHolder_free(handshake->remote_auth_request_token);
    DDS_Security_OctetSeq_deinit(&handshake->pdata);
    dds_security_fsm_free(handshake->fsm);
    ddsrt_mutex_destroy(&handshake->lock);
    ddsrt_free(handshake);
  }
}

void ddsi_handshake_handle_message(struct ddsi_handshake *handshake, const struct ddsi_participant *pp, const struct ddsi_proxy_participant *proxypp, const struct ddsi_participant_generic_message *msg)
{
  handshake_event_t event = EVENT_VALIDATION_FAILED;

  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);

  assert(handshake);
  assert(pp);
  assert(proxypp);
  assert(msg);

  TRACE_FUNC(handshake->fsm);

  if (!validate_handshake(handshake, NULL, NULL))
    return;

  HSTRACE ("FSM: handshake_handle_message (lguid="PGUIDFMT" rguid="PGUIDFMT") class_id=%s\n",
      PGUID (pp->e.guid), PGUID (proxypp->e.guid),
      msg->message_class_id ? msg->message_class_id: "NULL");

  if (!msg->message_class_id || msg->message_data.n == 0 || !msg->message_data.tags[0].class_id)
  {
    HSERROR("received handshake message ("PGUIDFMT" --> "PGUIDFMT") does not contain a handshake message token\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid));
    goto invalid_message;
  }
  else if (strcmp(msg->message_class_id, DDS_SECURITY_AUTH_REQUEST) == 0)
  {
    if (strcmp(msg->message_data.tags[0].class_id, DDS_SECURITY_AUTH_REQUEST_TOKEN_CLASS_ID) == 0)
    {
      /* Note the state machine is started by discovery
       * currently and not by the reception of an auth_request message which was send as the
       * result of validate_remote_entity at the opposite participant
       */
      ddsrt_mutex_lock(&handshake->lock);
      if (handshake->remote_auth_request_token)
        DDS_Security_DataHolder_free(handshake->remote_auth_request_token);
      handshake->remote_auth_request_token = DDS_Security_DataHolder_alloc();
      ddsi_omg_security_dataholder_copyout (handshake->remote_auth_request_token, &msg->message_data.tags[0]);
      ddsrt_mutex_unlock(&handshake->lock);
    }
    else
    {
      HSERROR("received handshake message ("PGUIDFMT" --> "PGUIDFMT") does not contain a valid handshake message token\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid));
    }
  }
  else if (strcmp(msg->message_class_id, DDS_SECURITY_AUTH_HANDSHAKE) == 0)
  {
    if (msg->message_data.tags[0].class_id == NULL)
      HSERROR("received handshake message ("PGUIDFMT" --> "PGUIDFMT") does not contain a valid handshake message token\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid));
    else if (strcmp(msg->message_data.tags[0].class_id, DDS_SECURITY_AUTH_HANDSHAKE_REQUEST_TOKEN_ID) == 0)
      event = EVENT_RECEIVED_MESSAGE_REQUEST;
    else if (strcmp(msg->message_data.tags[0].class_id, DDS_SECURITY_AUTH_HANDSHAKE_REPLY_TOKEN_ID) == 0)
      event = EVENT_RECEIVED_MESSAGE_REPLY;
    else if (strcmp(msg->message_data.tags[0].class_id, DDS_SECURITY_AUTH_HANDSHAKE_FINAL_TOKEN_ID) == 0)
      event = EVENT_RECEIVED_MESSAGE_FINAL;
    else
    {
      HSERROR("received handshake message ("PGUIDFMT" --> "PGUIDFMT") does not contain a valid handshake message token\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid));
      goto invalid_message;
    }

    ddsrt_mutex_lock(&handshake->lock);
    DDS_Security_DataHolder_deinit(&handshake->handshake_message_in_token);
    ddsi_omg_security_dataholder_copyout (&handshake->handshake_message_in_token, &msg->message_data.tags[0]);
    memcpy(&handshake->handshake_message_in_id, &msg->message_identity, sizeof(handshake->handshake_message_in_id));
    dds_security_fsm_dispatch(handshake->fsm, event, false);
    ddsrt_mutex_unlock(&handshake->lock);
  }
  else
  {
    HSERROR("received handshake message ("PGUIDFMT" --> "PGUIDFMT") does not contain a valid message_class_id\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid));
  }

invalid_message:
  return;
}

void ddsi_handshake_crypto_tokens_received(struct ddsi_handshake *handshake)
{
  struct ddsi_participant *pp;
  struct ddsi_proxy_participant *proxypp;

  assert(handshake);
  assert(handshake->fsm);

  if (!validate_handshake(handshake, &pp, &proxypp))
    return;

  HSTRACE("FSM: tokens received (lguid="PGUIDFMT" rguid="PGUIDFMT")\n", PGUID(pp->e.guid), PGUID(proxypp->e.guid));

  dds_security_fsm_dispatch(handshake->fsm, EVENT_RECV_CRYPTO_TOKENS, false);
}

int64_t ddsi_handshake_get_shared_secret(const struct ddsi_handshake *handshake)
{
  return handshake->shared_secret;
}

int64_t ddsi_handshake_get_handle(const struct ddsi_handshake *handshake)
{
  return handshake->handshake_handle;
}

static struct ddsi_hsadmin * ddsi_handshake_admin_create(void)
{
  struct ddsi_hsadmin *admin;

  admin = ddsrt_malloc(sizeof(*admin));
  ddsrt_mutex_init(&admin->lock);
  ddsrt_avl_init(&handshake_treedef, &admin->handshakes);
  admin->fsm_control = NULL;

  return admin;
}

static void release_handshake(void *arg)
{
  ddsi_handshake_release((struct ddsi_handshake *)arg);
}

static struct ddsi_handshake * ddsi_handshake_find_locked(
    struct ddsi_hsadmin *hsadmin,
    struct ddsi_participant *pp,
    struct ddsi_proxy_participant *proxypp)
{
  struct handshake_entities handles;

  handles.lguid = pp->e.guid;
  handles.rguid = proxypp->e.guid;

  return ddsrt_avl_lookup(&handshake_treedef, &hsadmin->handshakes, &handles);
}

static void gc_delete_handshale (struct ddsi_gcreq *gcreq)
{
  struct ddsi_handshake *handshake = gcreq->arg;

  ddsi_handshake_release(handshake);
  ddsi_gcreq_free (gcreq);
}

void ddsi_handshake_remove(struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp)
{
  struct ddsi_hsadmin *hsadmin = pp->e.gv->hsadmin;
  struct ddsi_handshake *handshake = NULL;

  ddsrt_mutex_lock(&hsadmin->lock);
  handshake = ddsi_handshake_find_locked(hsadmin, pp, proxypp);
  if (handshake)
  {
    struct ddsi_gcreq *gcreq = ddsi_gcreq_new (pp->e.gv->gcreq_queue, gc_delete_handshale);
    ddsrt_avl_delete(&handshake_treedef, &hsadmin->handshakes, handshake);
    ddsrt_atomic_st32(&handshake->deleting, 1);
    dds_security_fsm_stop(handshake->fsm);
    gcreq->arg = handshake;
    ddsi_gcreq_enqueue (gcreq);
  }
  ddsrt_mutex_unlock(&hsadmin->lock);
}

struct ddsi_handshake * ddsi_handshake_find(struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp)
{
  struct ddsi_hsadmin *hsadmin = pp->e.gv->hsadmin;
  struct ddsi_handshake *handshake = NULL;

  ddsrt_mutex_lock(&hsadmin->lock);
  handshake = ddsi_handshake_find_locked(hsadmin, pp, proxypp);
  if (handshake)
    ddsrt_atomic_inc32(&handshake->refc);
  ddsrt_mutex_unlock(&hsadmin->lock);

  return handshake;
}

void ddsi_handshake_register(struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp, ddsi_handshake_end_cb_t callback)
{
  struct ddsi_hsadmin *hsadmin = pp->e.gv->hsadmin;
  struct ddsi_handshake *handshake = NULL;

  ddsrt_mutex_lock(&hsadmin->lock);
  handshake = ddsi_handshake_find_locked(hsadmin, pp, proxypp);
  if (!handshake)
  {
    handshake = ddsi_handshake_create(pp, proxypp, callback);
    if (handshake)
      ddsrt_avl_insert(&handshake_treedef, &hsadmin->handshakes, handshake);
  }
  ddsrt_mutex_unlock(&hsadmin->lock);
}

void ddsi_handshake_admin_init(struct ddsi_domaingv *gv)
{
  assert(gv);
  gv->hsadmin = ddsi_handshake_admin_create();
}

void ddsi_handshake_admin_deinit(struct ddsi_domaingv *gv)
{
  struct ddsi_hsadmin *hsadmin = gv->hsadmin;
  if (hsadmin)
  {
    ddsrt_mutex_destroy(&hsadmin->lock);
    ddsrt_avl_free(&handshake_treedef, &hsadmin->handshakes, release_handshake);
    if (hsadmin->fsm_control)
      dds_security_fsm_control_free(hsadmin->fsm_control);
    ddsrt_free(hsadmin);
  }
}

void ddsi_handshake_admin_stop(struct ddsi_domaingv *gv)
{
  struct ddsi_hsadmin *hsadmin = gv->hsadmin;
  if (hsadmin && hsadmin->fsm_control)
    dds_security_fsm_control_stop(hsadmin->fsm_control);
}

#else

extern inline void ddsi_handshake_release(UNUSED_ARG(struct ddsi_handshake *handshake));
extern inline void ddsi_handshake_crypto_tokens_received(UNUSED_ARG(struct ddsi_handshake *handshake));
extern inline int64_t ddsi_handshake_get_shared_secret(UNUSED_ARG(const struct ddsi_handshake *handshake));
extern inline int64_t ddsi_handshake_get_handle(UNUSED_ARG(const struct ddsi_handshake *handshake));
extern inline void ddsi_handshake_register(UNUSED_ARG(struct ddsi_participant *pp), UNUSED_ARG(struct ddsi_proxy_participant *proxypp), UNUSED_ARG(ddsi_handshake_end_cb_t callback));
extern inline void ddsi_handshake_remove(UNUSED_ARG(struct ddsi_participant *pp), UNUSED_ARG(struct ddsi_proxy_participant *proxypp), UNUSED_ARG(struct ddsi_handshake *handshake));
extern inline struct ddsi_handshake * ddsi_handshake_find(UNUSED_ARG(struct ddsi_participant *pp), UNUSED_ARG(struct ddsi_proxy_participant *proxypp));

#endif /* DDS_HAS_DDS_SECURITY */
