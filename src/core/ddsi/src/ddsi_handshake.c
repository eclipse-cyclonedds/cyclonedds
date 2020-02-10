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

#include "dds/ddsi/ddsi_handshake.h"


#ifdef DDSI_INCLUDE_SECURITY

#include <string.h>

#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/q_entity.h"
#include "dds/security/dds_security_api_types.h"
#include "dds/security/dds_security_api.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/security/core/dds_security_fsm.h"
#include "dds/ddsi/ddsi_security_util.h"
#include "dds/ddsi/ddsi_security_exchange.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/avl.h"

#define HSTRACE(...)    DDS_CTRACE (&handshake->gv->logconfig, __VA_ARGS__)
#define HSWARNING(...)  DDS_CLOG (DDS_LC_WARNING, &handshake->gv->logconfig, __VA_ARGS__)
#define HSERROR(...)    DDS_CLOG (DDS_LC_ERROR, &handshake->gv->logconfig, __VA_ARGS__)

#define HSEXCEPTION(e, ...) \
  q_omg_log_exception(&handshake->gv->logconfig, DDS_LC_WARNING, e, __FILE__, __LINE__, DDS_FUNCTION, __VA_ARGS__)


#define VERBOSE_HANDSHAKE_DEBUG

#if 1
#define TRACE_FUNC(ptr)
#else
#undef TRACE
#define TRACE(args) nn_trace args
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
  struct participant *pp;
  struct proxy_participant *proxypp;
};

struct ddsi_handshake
{
  ddsrt_avl_node_t avlnode;
  enum ddsi_handshake_state state;
  struct handshake_entities participants;
  DDS_Security_HandshakeHandle handshake_handle;
  uint32_t refc;
  ddsi_handshake_end_cb_t end_cb;
  ddsrt_mutex_t lock;
  struct dds_security_fsm *fsm;
  const struct ddsi_domaingv *gv;
  dds_security_authentication *auth;
  ddsi_guid_t lguid;
  ddsi_guid_t rguid;
  DDS_Security_HandshakeMessageToken handshake_message_in_token;
  nn_message_identity_t handshake_message_in_id;
  DDS_Security_HandshakeMessageToken *handshake_message_out;
  DDS_Security_AuthRequestMessageToken local_auth_request_token;
  DDS_Security_AuthRequestMessageToken *remote_auth_request_token;
  DDS_Security_OctetSeq pdata;
  int64_t remote_identity_handle;
  int64_t shared_secret;
  int handled_handshake_message;
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

  if (ha->proxypp == hb->proxypp)
    return (ha->pp > hb->pp) ? 1 : (ha->pp < hb->pp) ? -1 : 0;
  else
    return (ha->proxypp > hb->proxypp) ? 1 : -1;
}

static bool validate_handshake(struct ddsi_handshake *handshake)
{
  struct participant *pp;
  struct proxy_participant *proxypp;

  if ((pp = entidx_lookup_participant_guid(handshake->gv->entity_index, &handshake->lguid)) == NULL)
  {
    HSERROR("Handshake invalid: participant "PGUIDFMT" not found", PGUID (handshake->lguid));
    ddsi_handshake_remove(handshake->participants.pp, handshake->participants.proxypp, handshake);
    return false;
  }
  else if ((proxypp = entidx_lookup_proxy_participant_guid(handshake->gv->entity_index, &handshake->rguid)) == NULL)
  {
    HSERROR("Handshake invalid: proxy participant "PGUIDFMT" not found", PGUID (handshake->rguid));
    ddsi_handshake_remove(handshake->participants.pp, handshake->participants.proxypp, handshake);
    return false;
  }

  assert(pp == handshake->participants.pp);
  assert(proxypp == handshake->participants.proxypp);

  return true;
}


#define RETRY_TIMEOUT           DDS_SECS(1)
#define RESEND_TIMEOUT          DDS_SECS(1)
#define SEND_TOKENS_TIMEOUT     DDS_MSECS(100)
#define AUTHENTICATION_TIMEOUT  DDS_SECS(100)

static void func_validate_remote_identity     (struct dds_security_fsm *fsm, void *arg);
static void func_handshake_init_message_resend(struct dds_security_fsm *fsm, void *arg);
static void func_begin_handshake_reply        (struct dds_security_fsm *fsm, void *arg);
static void func_begin_handshake_request      (struct dds_security_fsm *fsm, void *arg);
static void func_process_handshake            (struct dds_security_fsm *fsm, void *arg);
static void func_handshake_message_resend     (struct dds_security_fsm *fsm, void *arg);
static void func_validation_ok                (struct dds_security_fsm *fsm, void *arg);
static void func_validation_failed            (struct dds_security_fsm *fsm, void *arg);
static void func_send_crypto_tokens_final     (struct dds_security_fsm *fsm, void *arg);
static void func_send_crypto_tokens           (struct dds_security_fsm *fsm, void *arg);

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
static void q_handshake_fsm_debug(
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
      PGUID (handshake->lguid),
      PGUID (handshake->rguid),
      dispatch,
      state,
      event);

}
#endif


/************************************************************************************************************
 Inspiration from https://confluence.prismtech.com/display/VC/Authentication?preview=/30379826/34340895/PT_StateMachine_3g.gif

                            [START]
                               |
                               v
               .---------------------------------.
               | state_validate_remote_identity  |
  .------------|---------------------------------|----------.--------------------.
  |            | func_validate_remote_identity() |          |                    |
  |            '---------------------------------'          |  VALIDATION_PENDING_HANDSHAKE_MESSAGE
VALIDATION_FAILED             ^  | VALIDATION_PENDING_RETRY |                    |
VALIDATION_OK                 |  |                          |                    |
  |                   TIMEOUT |  v                          |                    v
  |       .-------------------------------------------.     |  .-----------------------------------.
  |       | state_validate_remote_identity_retry_wait |     |  | state_handshake_init_message_wait |<---------------.
  |       |-------------------------------------------|     |  |-----------------------------------|           AUTO |
  |       | retry_timeout                             |     |  | resend_timeout                    |---------.      |
  |       '-------------------------------------------'     |  '-----------------------------------' TIMEOUT |      |
  |                                                         |                    |                           |      |
  |                     .-----------------------------------'                    |                           |      |
  |                     |      VALIDATION_PENDING_HANDSHAKE_REQUEST              |                           v      |
  |                     |                                                        |       .--------------------------------------.
  |                     v                                      RECEIVED_MESSAGE_REQUEST  | state_handshake_init_message_resend  |
  |    .--------------------------------.                                        |       |--------------------------------------|
  |    | state_begin_handshake_request  | VALIDATION_PENDING_RETRY               |       | func_handshake_init_message_resend() |
  |    |--------------------------------|------------.                           |       '--------------------------------------'
  |    | func_begin_handshake_request() |            |                           |                           ^
  |    '--------------------------------'            |                           |                           |
  |        |            |        ^                   |                           |                           |
  |        |            |        | TIMEOUT           v                           |                           |
  | VALIDATION_FAILED   |      .------------------------------------------.      |                           |
  | VALIDATION_OK       |      | state_begin_handshake_request_retry_wait |      |                           |
  |        |            |      |------------------------------------------|      |                           |
  |--------'            |      | retry_timeout                            |      |                           |
  |                     |      '------------------------------------------'      |                           |
  |                     |                                                        v                  VALIDATION_FAILED
  |                     |                                        .------------------------------.            |
  |   VALIDATION_PENDING_HANDSHAKE_MESSAGE                       | state_begin_handshake_reply  |------------'
  |                     |                                .-------|------------------------------|
  |                     |                                |       | func_begin_handshake_reply() |------------.
  |                     |                                |       '------------------------------'            |
  |                     |                                |       VALIDATION_OK |              ^     VALIDATION_PENDING_RETRY
  |                     |                                |                     |              |              |
  |                     |     VALIDATION_PENDING_HANDSHAKE_MESSAGE             v              | TIMEOUT      |
  |                     |                                |         goto state_validation_ok   |              |
  |                     |                                v                                    |              v
  |                     |                .------------------------------.            .------------------------------------------.
  |                     |                | state_handshake_message_wait |            | state_begin_handshake_reply_retry_wait   |
  |                     .--------------->|------------------------------|-------.    |------------------------------------------|
  |                     |                | resend_timeout               |       |    | retry_timeout                            |
  |                     |                '------------------------------'       |    '------------------------------------------'
  |                     | AUTO                           |          ^           |
  |                     |                        TIMEOUT |          |           |
  |    .---------------------------------.               |          |           | RECEIVED_MESSAGE_REPLY
  |    | state_handshake_message_resend  |               |  VALIDATION_FAILED   | RECEIVED_MESSAGE_FINAL
  |    |---------------------------------|<--------------'          |           |
  |    | func_handshake_message_resend() |                          |           v
  |    '---------------------------------'                        .--------------------------.
  |                                                               | state_process_handshake  |
  |                              .--------------------------------|--------------------------|--------------------------.
  |                              |            .------------------>| func_process_handshake() |                          |
  |                              |            |                   '--------------------------'                          |
  |                              |            |                                 |                                       |
  |          VALIDATION_PENDING_RETRY      TIMEOUT                VALIDATION_OK |                                       |
  |                              v            |                                 v                                       |
  |           .------------------------------------.            .-------------------------------.                       |
  |           | state_process_handshake_retry_wait |            | state_send_crypto_tokens_wait |                       |
  |           |------------------------------------|            |-------------------------------|                       |
  |           | retry_timeout                      |            | send_tokens_timeout           |                       |
  |           '------------------------------------'            '-------------------------------'                       |
  |                                                                     |              |              VALIDATION_OK_FINAL_MESSAGE
  |                                                       .-------------'              '---------.                      |
  |                                                       | RECV_CRYPTO_TOKENS           TIMEOUT |                      |
  |                                                       v                                      v                      |
  |                                      .---------------------------------.       .---------------------------.        |
  |                                      | state_send_crypto_tokens_final  |       | state_send_crypto_tokens  |        |
  |                       .--------------|---------------------------------|       |---------------------------|        |
  |                       |              | func_send_crypto_tokens_final() |       | func_send_crypto_tokens() |        |
  |                       |              '---------------------------------'       '---------------------------'        |
  |                       |                               ^                                      |                      |
  |                   VALIDATION_OK                       |       .--------------------.   VALIDATION_OK_FINAL_MESSAGE  |
  |                       |                       TIMEOUT |       | RECV_CRYPTO_TOKENS |         |                      |
  |                       |                               |       v                    |         v                      |
  |                       |            .-------------------------------------.     .--------------------------.         |
  |                       |            | state_send_crypto_tokens_final_wait |     | state_wait_crypto_tokens |<--------'
  |                       |            |-------------------------------------|     |--------------------------|
  |                       |            | send_tokens_timeout                 |     |                          |---------.
  |                       |            '-------------------------------------'     '--------------------------'         |
  |                       |                               ^                              |               ^              |
  |                       |                               |                  RECEIVED_MESSAGE_REPLY     AUTO      VALIDATION_OK
  |                       |                      RECV_CRYPTO_TOKENS                      v               |              |
  |                       |                               |                     .---------------------------------.     |
  |                       |                               |                     |  state_handshake_final_resend   |     |
  |                       |                               '---------------------|---------------------------------|     |
  | VALIDATION_OK         |                                                     | func_handshake_message_resend() |     |
  |---------------------------------------------------------.                   '---------------------------------'     |
  |                                                         |                                                           |
  '---------------.                                         |                                                           |
VALIDATION_FAILED |                                         |                                                           |
                  v                                         v                                                           |
            .--------------------------.               .----------------------.                                         |
            | state_validation_failed  |               | state_validation_ok  |                                         |
            |--------------------------|               |----------------------|<----------------------------------------'
            | func_validation_failed() |               | func_validation_ok() |
            '--------------------------'               '----------------------'
                          |                                       |
                          v                                       v
                        [END]                                   [END]


 .----------------------------------------.
 | state_begin_handshake_reply_retry_wait |
 |----------------------------------------|
 | retry_timeout                          |
 '----------------------------------------'


*************************************************************************************************************/
static const dds_security_fsm_transition handshake_transistions [] =
{   /* Start */
    { NULL,                                 EVENT_AUTO,                                 NULL,
                                            &state_validate_remote_identity                },
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



static bool send_handshake_message(const struct ddsi_handshake *handshake, DDS_Security_DataHolder *token, struct proxy_participant *proxypp, int request)
{
  bool ret = false;
  struct participant *pp;
  nn_dataholderseq_t mdata;
  DDS_Security_DataHolderSeq tseq;

  tseq._length = tseq._maximum = 1;
  tseq._buffer = token;

  q_omg_shallow_copyout_DataHolderSeq(&mdata, &tseq);

  if (!(pp = entidx_lookup_participant_guid(handshake->gv->entity_index, &handshake->lguid)))
  {
    /* The participant is most likely in the process of being deleted. */
    HSWARNING("Send handshake: failed to find local participant (lguid="PGUIDFMT" rguid="PGUIDFMT")", PGUID (handshake->lguid), PGUID (handshake->rguid));
  }
  else if (!(ret = write_auth_handshake_message(pp, proxypp, &mdata, request, &handshake->handshake_message_in_id)))
  {
    HSWARNING("Send handshake: failed to send message (lguid="PGUIDFMT" rguid="PGUIDFMT")", PGUID (pp->e.guid), PGUID (proxypp->e.guid));
  }

  q_omg_shallow_free_nn_dataholderseq(&mdata);

  return ret;
}

static void func_validate_remote_identity(struct dds_security_fsm *fsm, void *arg)
{
  DDS_Security_ValidationResult_t ret;
  DDS_Security_SecurityException exception = {0};
  struct ddsi_handshake *handshake = (struct ddsi_handshake*)arg;
  dds_security_authentication *auth = handshake->auth;
  struct participant *pp = handshake->participants.pp;
  struct proxy_participant *proxypp = handshake->participants.proxypp;
  DDS_Security_IdentityToken remote_identity_token;
  ddsi_guid_t remote_guid;

  if (!validate_handshake(handshake))
    return;

  TRACE_FUNC(fsm);

  if (!(proxypp->plist->present & PP_IDENTITY_TOKEN))
  {
    HSERROR("validate remote identity failed: remote participant ("PGUIDFMT") identity token missing", PGUID (handshake->rguid));
    ret = DDS_SECURITY_VALIDATION_FAILED;
    goto ident_token_missing;
  }

  remote_guid = nn_hton_guid(handshake->rguid);
  q_omg_security_dataholder_copyout(&remote_identity_token, &proxypp->plist->identity_token);

  ddsrt_mutex_lock(&handshake->lock);
  ret = auth->validate_remote_identity(
      auth, &handshake->remote_identity_handle, &handshake->local_auth_request_token, handshake->remote_auth_request_token,
      pp->local_identity_handle, &remote_identity_token, (DDS_Security_GUID_t *)&remote_guid, &exception);
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

  HSTRACE("FSM: validate_remote_identity (lguid="PGUIDFMT" rguid="PGUIDFMT") ret=%d\n", PGUID (handshake->lguid), PGUID (handshake->rguid), ret);

  DDS_Security_DataHolder_deinit(&remote_identity_token);

  /* When validate_remote_identity returns a local_auth_request_token
   * which does not equal TOKEN_NIL then an AUTH_REQUEST message has
   * to be send.
   */
  if (handshake->local_auth_request_token.class_id && strlen(handshake->local_auth_request_token.class_id) != 0)
    (void)send_handshake_message(handshake, &handshake->local_auth_request_token, proxypp, 1);

validation_failed:
ident_token_missing:
  /* Use return value as state machine event. */
  dds_security_fsm_dispatch(fsm, (int32_t)ret, true);
}

static void func_handshake_init_message_resend(struct dds_security_fsm *fsm, void *arg)
{
  struct ddsi_handshake *handshake = arg;

  DDSRT_UNUSED_ARG(fsm);

  if (!validate_handshake(handshake))
     return;

  TRACE_FUNC(fsm);

  HSTRACE("FSM: handshake init_message_resend (lguid="PGUIDFMT" rguid="PGUIDFMT")\n", PGUID(handshake->lguid), PGUID(handshake->rguid));

  if (strlen(handshake->local_auth_request_token.class_id) != 0)
    (void)send_handshake_message(handshake, &handshake->local_auth_request_token, handshake->participants.proxypp, 1);
}

static void func_begin_handshake_reply(struct dds_security_fsm *fsm, void *arg)
{
  DDS_Security_ValidationResult_t ret;
  DDS_Security_SecurityException exception = {0};
  struct ddsi_handshake *handshake = arg;
  dds_security_authentication *auth = handshake->auth;
  struct participant *pp = handshake->participants.pp;
  struct proxy_participant *proxypp = handshake->participants.proxypp;

  if (!validate_handshake(handshake))
     return;

  TRACE_FUNC(fsm);

  ddsrt_mutex_lock(&handshake->lock);

  if (handshake->handshake_message_out)
    DDS_Security_DataHolder_free(handshake->handshake_message_out);
  handshake->handshake_message_out = DDS_Security_DataHolder_alloc();

  ret = auth->begin_handshake_reply(
      auth, &(handshake->handshake_handle), handshake->handshake_message_out, &handshake->handshake_message_in_token,
      handshake->remote_identity_handle, pp->local_identity_handle, &handshake->pdata, &exception);

  ddsrt_mutex_unlock(&handshake->lock);

  HSTRACE("FSM: begin_handshake_reply (lguid="PGUIDFMT" rguid="PGUIDFMT") ret=%d\n", PGUID (handshake->lguid), PGUID (handshake->rguid), ret);

  /* Trace a failed handshake. */
  if ((ret != DDS_SECURITY_VALIDATION_OK                       ) &&
      (ret != DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE         ) &&
      (ret != DDS_SECURITY_VALIDATION_PENDING_RETRY            ) &&
      (ret != DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE))
  {
    HSEXCEPTION(&exception, "Begin handshake reply failed");
    ret = DDS_SECURITY_VALIDATION_FAILED;
    goto handshake_failed;
  }

  if (ret == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE) {
    if (!send_handshake_message(handshake, handshake->handshake_message_out, proxypp, 0)) {
      ret = DDS_SECURITY_VALIDATION_FAILED;
      goto handshake_failed;
    }
  }
  else if (ret == DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE)
  {
    if (send_handshake_message(handshake, handshake->handshake_message_out, proxypp, 0))
      ret = DDS_SECURITY_VALIDATION_OK;
    else
    {
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

static void func_begin_handshake_request(struct dds_security_fsm *fsm, void *arg)
{
  DDS_Security_ValidationResult_t ret;
  DDS_Security_SecurityException exception = {0};
  struct ddsi_handshake *handshake = arg;
  dds_security_authentication *auth = handshake->auth;
  struct participant *pp = handshake->participants.pp;
  struct proxy_participant *proxypp = handshake->participants.proxypp;

  if (!validate_handshake(handshake))
    return;

  TRACE_FUNC(fsm);

  ddsrt_mutex_lock(&handshake->lock);

  if (handshake->handshake_message_out)
    DDS_Security_DataHolder_free(handshake->handshake_message_out);
  handshake->handshake_message_out = DDS_Security_DataHolder_alloc();

  ret = auth->begin_handshake_request(auth, &(handshake->handshake_handle), handshake->handshake_message_out, pp->local_identity_handle, handshake->remote_identity_handle, &handshake->pdata, &exception);
  ddsrt_mutex_unlock(&handshake->lock);

  HSTRACE("FSM: begin_handshake_request (lguid="PGUIDFMT" rguid="PGUIDFMT") ret=%d\n", PGUID (handshake->lguid), PGUID (handshake->rguid), ret);

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
    if (!send_handshake_message(handshake, handshake->handshake_message_out, proxypp, 0))
    {
      ret = DDS_SECURITY_VALIDATION_FAILED;
      goto handshake_failed;
    }
  }
  else if (ret == DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE)
  {
    if (send_handshake_message(handshake, handshake->handshake_message_out, proxypp, 0))
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

  if (!validate_handshake(handshake))
    return;

  TRACE_FUNC(fsm);

  ddsrt_mutex_lock(&handshake->lock);

  if (handshake->handshake_message_out)
    DDS_Security_DataHolder_free(handshake->handshake_message_out);
  handshake->handshake_message_out = DDS_Security_DataHolder_alloc();

  ret = auth->process_handshake(auth, handshake->handshake_message_out, &handshake->handshake_message_in_token, handshake->handshake_handle, &exception);
  ddsrt_mutex_unlock(&handshake->lock);

  HSTRACE("FSM: process_handshake (lguid="PGUIDFMT" rguid="PGUIDFMT") ret=%d\n", PGUID (handshake->lguid), PGUID (handshake->rguid), ret);

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
    handshake->end_cb(handshake, handshake->participants.pp, handshake->participants.proxypp, STATE_HANDSHAKE_PROCESSED);
  }

  if (ret == DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE)
   {
     if (!send_handshake_message(handshake, handshake->handshake_message_out, handshake->participants.proxypp, 0))
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

  if (!validate_handshake(handshake))
    return;

  TRACE_FUNC(fsm);

  /* The final handshake message has been send to the remote
   * participant. Call the callback function to signal that
   * the corresponding crypto tokens can be send. Amd start
   * waiting for the crypto tokens from the remote participant
   */

  HSTRACE("FSM: handshake send crypto tokens (lguid="PGUIDFMT" rguid="PGUIDFMT")\n", PGUID (handshake->lguid), PGUID (handshake->rguid));
  handshake->end_cb(handshake, handshake->participants.pp, handshake->participants.proxypp, STATE_HANDSHAKE_SEND_TOKENS);
  dds_security_fsm_dispatch(fsm, EVENT_VALIDATION_OK_FINAL_MESSAGE, true);
}

static void func_send_crypto_tokens_final(struct dds_security_fsm *fsm, void *arg)
{
  struct ddsi_handshake *handshake = arg;

  if (!validate_handshake(handshake))
    return;

  TRACE_FUNC(fsm);

  /* The final handshake message has been send to the remote
   * participant. Call the callback function to signal that
   * the corresponding crypto tokens can be send. Amd start
   * waiting for the crypto tokens from the remote participant
   */

  HSTRACE("FSM: handshake send crypto tokens final (lguid="PGUIDFMT" rguid="PGUIDFMT")\n", PGUID (handshake->lguid), PGUID (handshake->rguid));
  handshake->end_cb(handshake, handshake->participants.pp, handshake->participants.proxypp, STATE_HANDSHAKE_SEND_TOKENS);
  dds_security_fsm_dispatch(fsm, EVENT_VALIDATION_OK, true);
}

static void func_handshake_message_resend(struct dds_security_fsm *fsm, void *arg)
{
  struct ddsi_handshake *handshake = arg;

  DDSRT_UNUSED_ARG(fsm);
  DDSRT_UNUSED_ARG(arg);

  if (!validate_handshake(handshake))
    return;

  TRACE_FUNC(fsm);

  HSTRACE("handshake resend (lguid="PGUIDFMT" rguid="PGUIDFMT")\n", PGUID(handshake->lguid), PGUID(handshake->rguid));
  if (handshake->handshake_message_out) {
    (void)send_handshake_message(handshake, handshake->handshake_message_out, handshake->participants.proxypp, 0);
  }
}

static void func_validation_ok(struct dds_security_fsm *fsm, void *arg)
{
  struct ddsi_handshake *handshake = arg;

  DDSRT_UNUSED_ARG(fsm);

  if (!validate_handshake(handshake))
    return;

  TRACE_FUNC(fsm);

  HSTRACE("FSM: handshake succeeded (lguid="PGUIDFMT" rguid="PGUIDFMT")\n", PGUID(handshake->lguid), PGUID(handshake->rguid));
  handshake->state = STATE_HANDSHAKE_OK;
  handshake->end_cb(handshake, handshake->participants.pp, handshake->participants.proxypp, STATE_HANDSHAKE_OK);
}

static void func_validation_failed(struct dds_security_fsm *fsm, void *arg)
{
  struct ddsi_handshake *handshake = arg;

  DDSRT_UNUSED_ARG(fsm);

  if (!validate_handshake(handshake))
    return;

  TRACE_FUNC(fsm);

  HSTRACE("FSM: handshake failed (lguid="PGUIDFMT" rguid="PGUIDFMT")\n", PGUID(handshake->lguid), PGUID(handshake->rguid));
  handshake->state = STATE_HANDSHAKE_FAILED;
  handshake->end_cb(handshake, handshake->participants.pp, handshake->participants.proxypp, STATE_HANDSHAKE_FAILED);
}

static void func_handshake_timeout(struct dds_security_fsm *fsm, void *arg)
{
  struct ddsi_handshake *handshake = arg;

  DDSRT_UNUSED_ARG(fsm);

  if (!validate_handshake(handshake))
    return;

  TRACE_FUNC(fsm);

  HSTRACE("FSM: handshake timeout (lguid="PGUIDFMT" rguid="PGUIDFMT")\n", PGUID(handshake->lguid), PGUID(handshake->rguid));
  handshake->state = STATE_HANDSHAKE_TIMED_OUT;
  handshake->end_cb(handshake, handshake->participants.pp, handshake->participants.proxypp, STATE_HANDSHAKE_TIMED_OUT);
}


static struct ddsi_handshake * ddsi_handshake_create(struct participant *pp, struct proxy_participant *proxypp, ddsi_handshake_end_cb_t callback)
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
  handshake->auth = q_omg_participant_get_authentication(pp);
  handshake->refc = 1;
  handshake->participants.pp = pp;
  handshake->participants.proxypp = proxypp;
  handshake->lguid = pp->e.guid;
  handshake->rguid = proxypp->e.guid;
  handshake->gv = gv;
  handshake->handshake_handle = 0;
  handshake->shared_secret = 0;
  handshake->remote_identity_handle = 0;
  auth_get_serialized_participant_data(pp, &pdata);

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
  dds_security_fsm_set_debug(handshake->fsm, q_handshake_fsm_debug);
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
  bool release = false;
  if (!handshake) return;

  ddsrt_mutex_lock(&handshake->lock);
  release = (--handshake->refc) == 0;
  ddsrt_mutex_unlock(&handshake->lock);

  if (release)
  {
    HSTRACE("handshake delete (lguid="PGUIDFMT" rguid="PGUIDFMT")\n", PGUID(handshake->lguid), PGUID(handshake->rguid));
    if (handshake->fsm) {
      dds_security_fsm_free(handshake->fsm);
      handshake->fsm = NULL;
    }
    DDS_Security_DataHolder_deinit(&handshake->local_auth_request_token);
    DDS_Security_DataHolder_deinit(&handshake->handshake_message_in_token);
    DDS_Security_DataHolder_free(handshake->handshake_message_out);
    DDS_Security_DataHolder_free(handshake->remote_auth_request_token);
    DDS_Security_OctetSeq_deinit(&handshake->pdata);
    ddsrt_mutex_destroy(&handshake->lock);
    ddsrt_free(handshake);
  }
}

void ddsi_handshake_handle_message(struct ddsi_handshake *handshake, const struct participant *pp, const struct proxy_participant *proxypp, const struct nn_participant_generic_message *msg)
{
  handshake_event_t event = EVENT_VALIDATION_FAILED;

  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);

  assert(handshake);
  assert(pp);
  assert(proxypp);
  assert(msg);

  TRACE_FUNC(handshake->fsm);

  HSTRACE ("FSM: handshake_handle_message (lguid="PGUIDFMT" rguid="PGUIDFMT") class_id=%s\n",
      PGUID (handshake->lguid), PGUID (handshake->rguid),
      msg->message_class_id ? msg->message_class_id: "NULL");

  if (!msg->message_class_id || msg->message_data.n == 0 || !msg->message_data.tags[0].class_id)
  {
    HSERROR("received handshake message ("PGUIDFMT" --> "PGUIDFMT") does not contain a handshake message token\n", PGUID (handshake->rguid), PGUID (handshake->lguid));
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
      q_omg_security_dataholder_copyout(handshake->remote_auth_request_token, &msg->message_data.tags[0]);
      ddsrt_mutex_unlock(&handshake->lock);
    }
    else
    {
      HSERROR("received handshake message ("PGUIDFMT" --> "PGUIDFMT") does not contain a valid handshake message token\n", PGUID (handshake->rguid), PGUID (handshake->lguid));
    }
  }
  else if (strcmp(msg->message_class_id, DDS_SECURITY_AUTH_HANDSHAKE) == 0)
  {
    if (msg->message_data.tags[0].class_id == NULL)
      HSERROR("received handshake message ("PGUIDFMT" --> "PGUIDFMT") does not contain a valid handshake message token\n", PGUID (handshake->rguid), PGUID (handshake->lguid));
    else if (strcmp(msg->message_data.tags[0].class_id, DDS_SECURITY_AUTH_HANDSHAKE_REQUEST_TOKEN_ID) == 0)
      event = EVENT_RECEIVED_MESSAGE_REQUEST;
    else if (strcmp(msg->message_data.tags[0].class_id, DDS_SECURITY_AUTH_HANDSHAKE_REPLY_TOKEN_ID) == 0)
      event = EVENT_RECEIVED_MESSAGE_REPLY;
    else if (strcmp(msg->message_data.tags[0].class_id, DDS_SECURITY_AUTH_HANDSHAKE_FINAL_TOKEN_ID) == 0)
      event = EVENT_RECEIVED_MESSAGE_FINAL;
    else
    {
      HSERROR("received handshake message ("PGUIDFMT" --> "PGUIDFMT") does not contain a valid handshake message token\n", PGUID (handshake->rguid), PGUID (handshake->lguid));
      goto invalid_message;
    }

    ddsrt_mutex_lock(&handshake->lock);
    DDS_Security_DataHolder_deinit(&handshake->handshake_message_in_token);
    q_omg_security_dataholder_copyout(&handshake->handshake_message_in_token, &msg->message_data.tags[0]);
    memcpy(&handshake->handshake_message_in_id, &msg->message_identity, sizeof(handshake->handshake_message_in_id));
    handshake->handled_handshake_message = 0;
    dds_security_fsm_dispatch(handshake->fsm, event, false);
    ddsrt_mutex_unlock(&handshake->lock);
  }
  else
  {
    HSERROR("received handshake message ("PGUIDFMT" --> "PGUIDFMT") does not contain a valid message_class_id\n", PGUID (handshake->rguid), PGUID (handshake->lguid));
  }

invalid_message:
  return;
}

void ddsi_handshake_crypto_tokens_received(struct ddsi_handshake *handshake)
{
  assert(handshake);
  assert(handshake->fsm);
  assert(validate_handshake(handshake));

  HSTRACE("FSM: tokens received (lguid="PGUIDFMT" rguid="PGUIDFMT")\n", PGUID(handshake->lguid), PGUID(handshake->rguid));

  dds_security_fsm_dispatch(handshake->fsm, EVENT_RECV_CRYPTO_TOKENS, false);
}

int64_t ddsi_handshake_get_remote_identity_handle(const struct ddsi_handshake *handshake)
{
  return handshake->remote_identity_handle;
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

static void ddsi_handshake_admin_delete(struct ddsi_hsadmin *hsadmin)
{
  if (hsadmin)
  {
    ddsrt_mutex_destroy(&hsadmin->lock);
    ddsrt_avl_free(&handshake_treedef, &hsadmin->handshakes, release_handshake);
    if (hsadmin->fsm_control)
    {
      dds_security_fsm_control_stop(hsadmin->fsm_control);
      dds_security_fsm_control_free(hsadmin->fsm_control);
    }
    ddsrt_free(hsadmin);
  }
}

static struct ddsi_handshake * ddsi_handshake_find_locked(
    struct ddsi_hsadmin *hsadmin,
    struct participant *pp,
    struct proxy_participant *proxypp)
{
  struct handshake_entities handles;

  handles.pp = pp;
  handles.proxypp = proxypp;

  return ddsrt_avl_lookup(&handshake_treedef, &hsadmin->handshakes, &handles);
}

void ddsi_handshake_remove(struct participant *pp, struct proxy_participant *proxypp, struct ddsi_handshake *handshake)
{
  struct ddsi_hsadmin *hsadmin = pp->e.gv->hsadmin;

  ddsrt_mutex_lock(&hsadmin->lock);
  if (!handshake)
    handshake = ddsi_handshake_find_locked(hsadmin, pp, proxypp);
  if (handshake)
  {
    ddsrt_avl_delete(&handshake_treedef, &hsadmin->handshakes, handshake);
    ddsi_handshake_release(handshake);
  }
  ddsrt_mutex_unlock(&hsadmin->lock);
}

struct ddsi_handshake * ddsi_handshake_find(struct participant *pp, struct proxy_participant *proxypp)
{
  struct ddsi_hsadmin *hsadmin = pp->e.gv->hsadmin;
  struct ddsi_handshake *handshake = NULL;

  ddsrt_mutex_lock(&hsadmin->lock);
  handshake = ddsi_handshake_find_locked(hsadmin, pp, proxypp);
  if (handshake)
    handshake->refc++;
  ddsrt_mutex_unlock(&hsadmin->lock);

  return handshake;
}

void ddsi_handshake_register(struct participant *pp, struct proxy_participant *proxypp, ddsi_handshake_end_cb_t callback)
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
  assert(gv);
  ddsi_handshake_admin_delete(gv->hsadmin);
}


#else

extern inline void ddsi_handshake_release(UNUSED_ARG(struct ddsi_handshake *handshake));
extern inline void ddsi_handshake_crypto_tokens_received(UNUSED_ARG(struct ddsi_handshake *handshake));
extern inline int64_t ddsi_handshake_get_shared_secret(UNUSED_ARG(const struct ddsi_handshake *handshake));
extern inline int64_t ddsi_handshake_get_handle(UNUSED_ARG(const struct ddsi_handshake *handshake));
extern inline void ddsi_handshake_register(UNUSED_ARG(struct participant *pp), UNUSED_ARG(struct proxy_participant *proxypp), UNUSED_ARG(ddsi_handshake_end_cb_t callback));
extern inline void ddsi_handshake_remove(UNUSED_ARG(struct participant *pp), UNUSED_ARG(struct proxy_participant *proxypp), UNUSED_ARG(struct ddsi_handshake *handshake));
extern inline struct ddsi_handshake * ddsi_handshake_find(UNUSED_ARG(struct participant *pp), UNUSED_ARG(struct proxy_participant *proxypp));

#endif /* DDSI_INCLUDE_DDS_SECURITY */
