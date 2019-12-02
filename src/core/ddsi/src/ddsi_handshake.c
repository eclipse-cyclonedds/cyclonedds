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
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/security/dds_security_api.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/avl.h"

struct handle_pair {
  DDS_Security_IdentityHandle local_identity_handle;
  DDS_Security_IdentityHandle remote_identity_handle;
};

struct ddsi_handshake
{
  ddsrt_avl_node_t avlnode;
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

  struct handle_pair handles;
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
  ddsrt_mutex_t lock;
  ddsrt_avl_tree_t handshakes;
};

static int compare_handle_pair(const void *va, const void *vb);

const ddsrt_avl_treedef_t handshake_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct ddsi_handshake, avlnode), offsetof (struct ddsi_handshake, handles), compare_handle_pair, 0);

static int compare_handle_pair(const void *va, const void *vb)
{
  const struct handle_pair *ha = va;
  const struct handle_pair *hb = vb;

  return ((ha->local_identity_handle == hb->local_identity_handle) && (ha->remote_identity_handle == hb->remote_identity_handle));
}

static struct ddsi_handshake * ddsi_handshake_create(const struct participant *pp, const struct proxy_participant *proxypp, nn_wctime_t timestamp, ddsi_handshake_end_cb_t callback)
{
  struct ddsi_handshake *handshake = NULL;

  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);
  DDSRT_UNUSED_ARG(timestamp);
  DDSRT_UNUSED_ARG(callback);

  return handshake;
}

void ddsi_handshake_start(struct ddsi_handshake *hs)
{
  DDSRT_UNUSED_ARG(hs);
}

void ddsi_handshake_release(struct ddsi_handshake *handshake)
{
  DDSRT_UNUSED_ARG(handshake);
}

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

struct q_globals * ddsi_handshake_get_globals(const struct ddsi_handshake *handshake)
{
  DDSRT_UNUSED_ARG(handshake);

  return NULL;
}

struct ddsi_hsadmin * ddsi_handshake_admin_create(void)
{
  struct ddsi_hsadmin *admin;

  admin = ddsrt_malloc(sizeof(*admin));
  ddsrt_mutex_init(&admin->lock);
  ddsrt_avl_init(&handshake_treedef, &admin->handshakes);

  return admin;
}

static void
release_handshake(void *arg)
{
  ddsi_handshake_release((struct ddsi_handshake *)arg);
}

void ddsi_handshake_admin_delete(struct ddsi_hsadmin *hsadmin)
{
  if (hsadmin)
  {
    ddsrt_mutex_destroy(&hsadmin->lock);
    ddsrt_avl_free(&handshake_treedef, &hsadmin->handshakes, release_handshake);
    ddsrt_free(hsadmin);
  }
}

static struct ddsi_handshake *
ddsi_handshake_find_locked(
    struct ddsi_hsadmin *hsadmin,
    const struct participant *pp,
    const struct proxy_participant *proxypp)
{
  struct handle_pair handles;

  handles.local_identity_handle = pp->local_identity_handle;
  handles.remote_identity_handle = proxypp->remote_identity_handle;

  return ddsrt_avl_lookup(&handshake_treedef, &hsadmin->handshakes, &handles);
}

void
ddsi_handshake_remove(const struct participant *pp, const struct proxy_participant *proxypp, struct ddsi_handshake *handshake)
{
  struct ddsi_hsadmin *hsadmin;

  if ((hsadmin = q_omg_security_get_handhake_admin(pp)) != NULL)
  {
    ddsrt_mutex_lock(&hsadmin->lock);
    if (handshake == NULL)
    {
      handshake = ddsi_handshake_find_locked(hsadmin, pp, proxypp);
    }
    if (handshake != NULL)
    {
      ddsrt_avl_delete(&handshake_treedef, &hsadmin->handshakes, handshake);
    }
    ddsrt_mutex_unlock(&hsadmin->lock);
  }
}

struct ddsi_handshake *
ddsi_handshake_find(const struct participant *pp, const struct proxy_participant *proxypp)
{
  struct ddsi_hsadmin *hsadmin;
  struct ddsi_handshake *handshake = NULL;

  if ((hsadmin = q_omg_security_get_handhake_admin(pp)) != NULL)
  {
    ddsrt_mutex_lock(&hsadmin->lock);
    handshake = ddsi_handshake_find_locked(hsadmin, pp, proxypp);
    ddsrt_mutex_unlock(&hsadmin->lock);
  }

  return handshake;
}

struct ddsi_handshake *
ddsi_handshake_register(const struct participant *pp, const struct proxy_participant *proxypp, nn_wctime_t timestamp, ddsi_handshake_end_cb_t callback)
{
  struct ddsi_hsadmin *hsadmin;
  struct ddsi_handshake *handshake = NULL;

  if ((hsadmin = q_omg_security_get_handhake_admin(pp)) != NULL)
  {
    ddsrt_mutex_lock(&hsadmin->lock);
    handshake = ddsi_handshake_find_locked(hsadmin, pp, proxypp);
    if (handshake == NULL)
    {
      handshake = ddsi_handshake_create(pp, proxypp, timestamp, callback);
      if (handshake)
        ddsrt_avl_insert(&handshake_treedef, &hsadmin->handshakes, handshake);
    }
    ddsrt_mutex_unlock(&hsadmin->lock);
  }

  return handshake;
}


#else

#include "dds/ddsi/ddsi_handshake.h"

extern inline void ddsi_handshake_start(UNUSED_ARG(struct ddsi_handshake *hs));
extern inline void ddsi_handshake_release(UNUSED_ARG(struct ddsi_handshake *handshake));
extern inline void ddsi_handshake_crypto_tokens_received(UNUSED_ARG(struct ddsi_handshake *handshake));
extern inline int64_t ddsi_handshake_get_shared_secret(UNUSED_ARG(const struct ddsi_handshake *handshake));
extern inline int64_t ddsi_handshake_get_handle(UNUSED_ARG(const struct ddsi_handshake *handshake));
extern inline struct q_globals * ddsi_handshake_get_globals(UNUSED_ARG(const struct ddsi_handshake *handshake));
extern inline struct ddsi_handshake *ddsi_handshake_register(UNUSED_ARG(const struct participant *pp), UNUSED_ARG(const struct proxy_participant *proxypp), UNUSED_ARG(nn_wctime_t timestamp), UNUSED_ARG(ddsi_handshake_end_cb_t callback));
extern inline void ddsi_handshake_remove(UNUSED_ARG(const struct participant *pp), UNUSED_ARG(const struct proxy_participant *proxypp), UNUSED_ARG(struct ddsi_handshake *handshake));
extern inline struct ddsi_handshake * ddsi_handshake_find(UNUSED_ARG(const struct participant *pp), UNUSED_ARG(const struct proxy_participant *proxypp));

#endif /* DDSI_INCLUDE_DDS_SECURITY */
