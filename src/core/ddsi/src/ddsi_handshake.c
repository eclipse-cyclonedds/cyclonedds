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
#ifdef DDSI_INCLUDE_SECURITY

#include "dds/ddsi/ddsi_handshake.h"
#include "dds/security/dds_security_api.h"
#include "dds/ddsrt/hopscotch.h"


struct ddsi_handshake
{
  //    struct ut_fsm* fsm;
  enum ddsi_handshake_state state;

  ddsi_guid_t local_pguid;  /* the guid of the local participant */
  ddsi_guid_t remote_pguid; /* the guid of the remote participant */
  nn_wctime_t timestamp;
  ddsi_handshake_end_cb_t end_cb;
  struct q_globals *gv;
  ddsrt_mutex_t lock;
  ddsrt_cond_t cv;

  bool deleting;
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

struct ddsi_hsadmin {
  struct ddsrt_h *hstab;
  ddsrt_mutex_t lock;
};


void ddsi_handshake_start(struct ddsi_handshake *hs)
{
  DDSRT_UNUSED_ARG(hs);
}

struct ddsi_handshake * ddsi_handshake_create(const struct participant *pp, const struct proxy_participant *proxypp, nn_wctime_t timestamp, ddsi_handshake_end_cb_t callback)
{
  struct ddsi_handshake *handshake = NULL;

  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);
  DDSRT_UNUSED_ARG(timestamp);
  DDSRT_UNUSED_ARG(callback);

  return handshake;
}

void ddsi_handshake_handle_message(struct ddsi_handshake *handshake, const struct participant *pp, const struct proxy_participant *proxypp, const struct nn_participant_generic_message *msg)
{
  DDSRT_UNUSED_ARG(handshake);
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);
  DDSRT_UNUSED_ARG(msg);
}

void ddsi_handshake_crypto_tokens_received(const struct ddsi_handshake *handshake)
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

struct q_globals * ddsi_handshake_get_globals(const struct ddsi_handshake *handshake)
{
  DDSRT_UNUSED_ARG(handshake);

  return NULL;
}

struct ddsi_hsadmin * ddsi_hsadmin_create(void)
{
  struct ddsi_hsadmin *admin = NULL;

  return admin;
}

void ddsi_hsadmin_clear(struct ddsi_hsadmin *admin)
{
  DDSRT_UNUSED_ARG(admin);
}

void ddsi_hsadmin_delete(struct ddsi_hsadmin *admin)
{
  DDSRT_UNUSED_ARG(admin);
}

struct ddsi_handshake * ddsi_hsadmin_find(const struct ddsi_hsadmin *admin, const ddsi_guid_t *lguid)
{
  struct ddsi_handshake *handshake = NULL;

  DDSRT_UNUSED_ARG(admin);
  DDSRT_UNUSED_ARG(lguid);

  return handshake;
}

void ddsi_hsadmin_lock(struct ddsi_hsadmin *admin)
{
  DDSRT_UNUSED_ARG(admin);
}

void ddsi_hsadmin_unlock(struct ddsi_hsadmin *admin)
{
  DDSRT_UNUSED_ARG(admin);
}

struct ddsi_handshake * ddsi_hsadmin_register_locked(struct ddsi_hsadmin *admin, const struct participant *pp, const struct proxy_participant *proxypp, nn_wctime_t timestamp, ddsi_handshake_end_cb_t callback)
{
  struct ddsi_handshake *handshake = NULL;

  DDSRT_UNUSED_ARG(admin);
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);
  DDSRT_UNUSED_ARG(timestamp);
  DDSRT_UNUSED_ARG(callback);

  return handshake;
}

void ddsi_hsadmin_remove_by_guid(struct ddsi_hsadmin *admin, const ddsi_guid_t *lguid)
{
  DDSRT_UNUSED_ARG(admin);
  DDSRT_UNUSED_ARG(lguid);
}

void ddsi_hsadmin_remove_from_fsm(struct ddsi_hsadmin *admin, struct ddsi_handshake *handshake)
{
  DDSRT_UNUSED_ARG(admin);
  DDSRT_UNUSED_ARG(handshake);
}

#else

#include "dds/ddsi/ddsi_handshake.h"

extern inline void ddsi_handshake_start(UNUSED_ARG(struct ddsi_handshake *hs));
extern inline struct ddsi_handshake * ddsi_handshake_create(UNUSED_ARG(const struct participant *pp), UNUSED_ARG(const struct proxy_participant *proxypp), UNUSED_ARG(nn_wctime_t timestamp), UNUSED_ARG(ddsi_handshake_end_cb_t callback));
extern inline int64_t ddsi_handshake_get_shared_secret(UNUSED_ARG(const struct ddsi_handshake *handshake));
inline int64_t ddsi_handshake_get_handle(UNUSED_ARG(const struct ddsi_handshake *handshake));
inline struct ddsi_hsadmin * ddsi_hsadmin_create(void);
inline void ddsi_handshake_release(UNUSED_ARG(struct ddsi_handshake *handshake));
inline void ddsi_hsadmin_clear(UNUSED_ARG(struct ddsi_hsadmin *admin));
inline void ddsi_hsadmin_delete(UNUSED_ARG(struct ddsi_hsadmin *admin));
inline void ddsi_hsadmin_lock(UNUSED_ARG(struct ddsi_hsadmin *admin));
inline void ddsi_hsadmin_unlock(UNUSED_ARG(struct ddsi_hsadmin *admin));
inline struct ddsi_handshake * ddsi_hsadmin_register_locked(UNUSED_ARG(struct ddsi_hsadmin *admin), UNUSED_ARG(const struct participant *pp), UNUSED_ARG(const struct proxy_participant *proxypp), UNUSED_ARG(nn_wctime_t timestamp), UNUSED_ARG(ddsi_handshake_end_cb_t callback));
inline struct ddsi_handshake * ddsi_hsadmin_find(UNUSED_ARG(const struct ddsi_hsadmin *admin), UNUSED_ARG(const ddsi_guid_t *lguid));
inline void ddsi_hsadmin_remove_by_guid(UNUSED_ARG(struct ddsi_hsadmin *admin), UNUSED_ARG(const nn_guid_t *lguid));
inline void ddsi_hsadmin_remove_from_fsm(UNUSED_ARG(struct ddsi_hsadmin *admin), UNUSED_ARG(struct ddsi_handshake *hs));

#endif /* DDSI_INCLUDE_DDS_SECURITY */
