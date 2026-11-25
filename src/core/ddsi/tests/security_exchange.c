// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>
#include "CUnit/Test.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_init.h"
#include "dds/ddsi/ddsi_thread.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/ddsi/ddsi_proxy_participant.h"
#include "ddsi__radmin.h"
#include "ddsi__protocol.h"
#include "ddsi__serdata_pserop.h"
#include "ddsi__security_exchange.h"

static struct ddsi_cfgst *cfgst;
static struct ddsi_domaingv gv;

static void setup (void)
{
  ddsrt_init ();
  ddsi_iid_init ();
  ddsi_thread_states_init ();
  const char *config = "";
  (void) ddsrt_getenv ("CYCLONEDDS_URI", &config);
  cfgst = ddsi_config_init (config, &gv.config, 0);
  CU_ASSERT_NEQ_FATAL (cfgst, NULL);
  ddsi_config_domain_init (&gv, cfgst);
  CU_ASSERT_EQ_FATAL (ddsi_init (&gv, NULL), 0);
}

static void teardown (void)
{
  ddsi_fini (&gv);
  ddsi_config_domain_fini (&gv, cfgst);
  ddsi_iid_fini ();
  ddsi_thread_states_fini ();
  ddsrt_fini ();
}

/* A DATA submessage with the K flag set and the D flag cleared, carrying only a
   CDR_LE encapsulation header, as a remote peer can send to the builtin stateless
   message reader (or the volatile secure reader) before authentication. */
static struct ddsi_serdata *make_key_only_sample (const struct ddsi_sertype *type)
{
  static const unsigned char payload[] = { 0x00, 0x01, 0x00, 0x00 };
  const ddsrt_iovec_t iov = { .iov_base = (void *) payload, .iov_len = sizeof (payload) };
  struct ddsi_serdata *sd = ddsi_serdata_from_ser_iov (type, SDK_KEY, 1, &iov, sizeof (payload));
  CU_ASSERT_NEQ_FATAL (sd, NULL);
  /* the message types are keyless, so there is no deserialized sample */
  CU_ASSERT_EQ_FATAL (((struct ddsi_serdata_pserop *) sd)->sample, NULL);
  /* printing happens when tracing with DDS_LC_CONTENT is enabled */
  char tmp[256];
  (void) ddsi_serdata_print (sd, tmp, sizeof (tmp));
  return sd;
}

static void init_receiver_state (struct ddsi_receiver_state *rst)
{
  memset (rst, 0, sizeof (*rst));
  rst->gv = &gv;
}

CU_Test (ddsi_security_exchange, auth_handshake_key_only, .init = setup, .fini = teardown)
{
  struct ddsi_receiver_state rst;
  init_receiver_state (&rst);
  struct ddsi_serdata *sd = make_key_only_sample (gv.pgm_stateless_type);
  const ddsi_entityid_t wrid = { .u = DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER };
  ddsi_handle_auth_handshake_message (&rst, wrid, sd);
  ddsi_serdata_unref (sd);
}

CU_Test (ddsi_security_exchange, crypto_exchange_key_only, .init = setup, .fini = teardown)
{
  struct ddsi_receiver_state rst;
  init_receiver_state (&rst);
  struct ddsi_serdata *sd = make_key_only_sample (gv.pgm_volatile_type);
  ddsi_handle_crypto_exchange_message (&rst, sd);
  ddsi_serdata_unref (sd);
}
