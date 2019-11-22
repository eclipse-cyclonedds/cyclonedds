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
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/avl.h"


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
  ddsi_handshake_end_cb_t end_cb;
};

struct ddsi_hsadmin {
  ddsrt_mutex_t lock;
  ddsrt_avl_tree_t handshakes;
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

static struct ddsi_handshake * ddsi_handshake_create(const struct participant *pp, const struct proxy_participant *proxypp, ddsi_handshake_end_cb_t callback)
{
  struct ddsi_handshake *handshake = NULL;

  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);
  DDSRT_UNUSED_ARG(callback);

  return handshake;
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

int64_t ddsi_handshake_get_remote_identity_handle(const struct ddsi_handshake *handshake)
{
  DDSRT_UNUSED_ARG(handshake);

  return 0;
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

static struct ddsi_hsadmin * ddsi_handshake_admin_create(void)
{
  struct ddsi_hsadmin *admin;

  admin = ddsrt_malloc(sizeof(*admin));
  ddsrt_mutex_init(&admin->lock);
  ddsrt_avl_init(&handshake_treedef, &admin->handshakes);

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

struct ddsi_handshake *
ddsi_handshake_find(struct participant *pp, struct proxy_participant *proxypp)
{
  struct ddsi_hsadmin *hsadmin = pp->e.gv->hsadmin;
  struct ddsi_handshake *handshake = NULL;

  ddsrt_mutex_lock(&hsadmin->lock);
  handshake = ddsi_handshake_find_locked(hsadmin, pp, proxypp);
  ddsrt_mutex_unlock(&hsadmin->lock);

  return handshake;
}

void
ddsi_handshake_register(struct participant *pp, struct proxy_participant *proxypp, ddsi_handshake_end_cb_t callback)
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
