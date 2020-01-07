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

#include "dds/security/dds_security_api.h"
#include "dds/ddsrt/hopscotch.h"

struct ddsi_handshake
{
  enum ddsi_handshake_state state;

  ddsi_guid_t local_pguid;  /* the guid of the local participant */
  ddsi_guid_t remote_pguid; /* the guid of the remote participant */
  ddsi_handshake_end_cb_t end_cb;
  struct q_globals *gv;
  ddsrt_mutex_t lock;
  ddsrt_cond_t cv;

  ddsrt_atomic_uint32_t refc;

  DDS_Security_IdentityHandle local_identity_handle;
  DDS_Security_IdentityHandle remote_identity_handle;
  DDS_Security_HandshakeHandle handshake_handle;

  DDS_Security_HandshakeMessageToken handshake_message_in_token;
  nn_message_identity_t handshake_message_in_id;
  DDS_Security_HandshakeMessageToken *handshake_message_out;
  DDS_Security_AuthRequestMessageToken local_auth_request_token;
  DDS_Security_AuthRequestMessageToken *remote_auth_request_token;
  DDS_Security_OctetSeq pdata;
  DDS_Security_SharedSecretHandle shared_secret;
  int handled_handshake_message;
};


void ddsi_handshake_handle_message(struct ddsi_handshake *handshake, const struct participant *pp, const struct proxy_participant *proxypp, const struct nn_participant_generic_message *msg)
{
  DDSRT_UNUSED_ARG(handshake);
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);
  DDSRT_UNUSED_ARG(msg);
}

void ddsi_handshake_crypto_tokens_received(struct ddsi_handshake *handshake)
{
  DDSRT_UNUSED_ARG(handshake);
}

int64_t ddsi_handshake_get_shared_secret(const struct ddsi_handshake *handshake)
{
  DDSRT_UNUSED_ARG(handshake);

  return 0;
}

int64_t ddsi_handshake_get_handle(const struct ddsi_handshake *handshake)
{
  DDSRT_UNUSED_ARG(handshake);

  return 0;
}

void ddsi_handshake_register(const struct participant *pp, const struct proxy_participant *proxypp, ddsi_handshake_end_cb_t callback)
{
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);
  DDSRT_UNUSED_ARG(callback);
}

void ddsi_handshake_remove(const struct participant *pp, const struct proxy_participant *proxypp, struct ddsi_handshake *handshake)
{
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);
  DDSRT_UNUSED_ARG(handshake);
}

struct ddsi_handshake * ddsi_handshake_find(const struct participant *pp, const struct proxy_participant *proxypp)
{
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);

  return NULL;
}


#else

extern inline void ddsi_handshake_release(UNUSED_ARG(struct ddsi_handshake *handshake));
extern inline void ddsi_handshake_crypto_tokens_received(UNUSED_ARG(struct ddsi_handshake *handshake));
extern inline int64_t ddsi_handshake_get_shared_secret(UNUSED_ARG(const struct ddsi_handshake *handshake));
extern inline int64_t ddsi_handshake_get_handle(UNUSED_ARG(const struct ddsi_handshake *handshake));
extern inline void ddsi_handshake_register(UNUSED_ARG(const struct participant *pp), UNUSED_ARG(const struct proxy_participant *proxypp), UNUSED_ARG(ddsi_handshake_end_cb_t callback));
extern inline void ddsi_handshake_remove(UNUSED_ARG(const struct participant *pp), UNUSED_ARG(const struct proxy_participant *proxypp), UNUSED_ARG(struct ddsi_handshake *handshake));
extern inline struct ddsi_handshake * ddsi_handshake_find(UNUSED_ARG(const struct participant *pp), UNUSED_ARG(const struct proxy_participant *proxypp));


#endif /* DDSI_INCLUDE_DDS_SECURITY */
