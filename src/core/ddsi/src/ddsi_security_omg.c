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

#include <string.h>
#include <stdarg.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/avl.h"

#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_plist.h"
#include "dds/ddsi/q_xevent.h"
#include "dds/ddsi/ddsi_sertopic.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_security_msg.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/ddsi/ddsi_security_util.h"


static bool q_omg_writer_is_payload_protected(const struct writer *wr);

static bool endpoint_is_DCPSParticipantSecure(const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER) ||
          (guid->entityid.u == NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER) );
}

static bool endpoint_is_DCPSPublicationsSecure(const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER) ||
          (guid->entityid.u == NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER) );
}

static bool endpoint_is_DCPSSubscriptionsSecure(const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER) ||
          (guid->entityid.u == NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER) );
}

static bool endpoint_is_DCPSParticipantStatelessMessage(const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER) ||
          (guid->entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER) );
}

static bool endpoint_is_DCPSParticipantMessageSecure(const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER) ||
          (guid->entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER) );
}

static bool endpoint_is_DCPSParticipantVolatileMessageSecure(const ddsi_guid_t *guid)
{
#if 1
  /* TODO: volatile endpoint. */
  DDSRT_UNUSED_ARG(guid);
  return false;
#else
  return ((guid->entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER) ||
          (guid->entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER) );
#endif
}


bool q_omg_security_enabled(void)
{
  return false;
}

#define EXCEPTION_LOG(sc,e,cat, ...) \
  log_exception(sc, cat, e, __FILE__, __LINE__, DDS_FUNCTION, __VA_ARGS__)

#define EXCEPTION_ERROR(s, e, ...)     EXCEPTION_LOG(s, e, DDS_LC_ERROR, __VA_ARGS__)
#define EXCEPTION_WARNING(s, e, ...)   EXCEPTION_LOG(s, e, DDS_LC_WARNING, __VA_ARGS__)

#define ENDPOINT_IS_DCPSPublicationsSecure(guid) \
           ((guid.entityid.u == NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER) || \
            (guid.entityid.u == NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER) )

#define ENDPOINT_IS_DCPSSubscriptionsSecure(guid) \
           ((guid.entityid.u == NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER) || \
            (guid.entityid.u == NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER) )

#define ENDPOINT_IS_DCPSParticipantStatelessMessage(guid) \
           ((guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER) || \
            (guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER) )

#define ENDPOINT_IS_DCPSParticipantMessageSecure(guid) \
           ((guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER) || \
            (guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER) )

#define ENDPOINT_IS_DCPSParticipantVolatileMessageSecure(guid) \
           ((guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER) || \
            (guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER) )

#define ENDPOINT_IS_DCPSParticipantSecure(guid) \
           ((guid.entityid.u == NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER) || \
            (guid.entityid.u == NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER) )



/* The secure context contains the reference to the security plugins.
 * Currently this structure is used as a place holder to provide access
 * to the security plugins. This has to be reworked then the plugin loading
 * becomes available.
 *
 * TODO: Rework the access to the security plugins.
 */
struct secure_context {
  dds_security_authentication *authentication;
  dds_security_access_control *access_control;
  dds_security_cryptography *crypto;
  struct q_globals *gv;
};

struct guid_pair {
  ddsi_guid_t src;
  ddsi_guid_t dst;
};

struct pending_tokens {
  ddsrt_avl_node_t avlnode;
  ddsrt_mutex_t lock;
  struct guid_pair guids;
  DDS_Security_ParticipantCryptoTokenSeq tokens;
};

struct proxypp_pp_match {
  ddsrt_avl_node_t avlnode;
  DDS_Security_IdentityHandle participant_identity;
  DDS_Security_PermissionsHandle permissions_handle;
  DDS_Security_SharedSecretHandle shared_secret;
  bool tokens_available;
};

struct participant_sec_attributes {
  struct secure_context *sc;
  DDS_Security_ParticipantSecurityAttributes attr;
  DDS_Security_ParticipantCryptoHandle crypto_handle;
  bool plugin_attr;
};

struct proxy_participant_sec_attributes {
  dds_security_access_control *access_control;
  DDS_Security_ParticipantCryptoHandle crypto_handle;
  ddsrt_avl_tree_t local_participants;
};

struct writer_sec_attributes {
  DDS_Security_EndpointSecurityAttributes attr;
  DDS_Security_DatawriterCryptoHandle crypto_handle;
  bool plugin_attr;
};

struct reader_sec_attributes {
  DDS_Security_EndpointSecurityAttributes attr;
  DDS_Security_DatareaderCryptoHandle crypto_handle;
  bool plugin_attr;
};


static int compare_identity_handle (const void *va, const void *vb);
static int compare_guid_pair(const void *va, const void *vb);

const ddsrt_avl_treedef_t proxypp_pp_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct proxypp_pp_match, avlnode), offsetof (struct proxypp_pp_match, participant_identity), compare_identity_handle, 0);
const ddsrt_avl_treedef_t pending_tokens_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct pending_tokens, avlnode), offsetof (struct pending_tokens, guids), compare_guid_pair, 0);

static int compare_identity_handle (const void *va, const void *vb)
{
  const DDS_Security_IdentityHandle *ha = va;
  const DDS_Security_IdentityHandle *hb = vb;

  return ((*ha > *hb) ? 1 : (*ha < *hb) ?  -1 : 0);
}

static int guid_compare (const ddsi_guid_t *guid1, const ddsi_guid_t *guid2)
{
  return memcmp (guid1, guid2, sizeof (ddsi_guid_t));
}

static int compare_guid_pair(const void *va, const void *vb)
{
  const struct guid_pair *na = va;
  const struct guid_pair *nb = vb;
  int r;

  if ((r = guid_compare(&na->src, &nb->src)) == 0)
    r = guid_compare(&na->dst, &nb->dst);
  return r;
}

static void
security_exception_clear(
    DDS_Security_SecurityException *exception)
{
  if (exception->message) {
    ddsrt_free(exception->message);
    exception->message = NULL;
  }
}

static void log_exception(struct secure_context *sc, uint32_t cat, DDS_Security_SecurityException *exception, const char *file, uint32_t line, const char *func, const char *fmt, ...)
{
  char logbuffer[512];
  va_list ap;
  int l;

  va_start (ap, fmt);
  l = vsnprintf(logbuffer, sizeof(logbuffer), fmt, ap);
  va_end (ap);
  if ((size_t) l >= sizeof(logbuffer))
  {
    logbuffer[sizeof(logbuffer)-1] = '\0';
  }
  dds_log_cfg(&sc->gv->logconfig, cat, file, line, func, "%s: %s(code: %d)", logbuffer, exception->message ? exception->message : "",  exception->code);
  security_exception_clear(exception);
}

static int q_omg_create_secure_context (struct participant *pp, struct secure_context **secure_context)
{
  DDSRT_UNUSED_ARG(pp);

  *secure_context = NULL;

  return 0;
}

static struct secure_context * q_omg_security_get_secure_context(const struct participant *pp)
{
  if (pp->sec_attr)
    return pp->sec_attr->sc;
  return NULL;
}

static const char * get_builtin_topic_name(ddsi_entityid_t id)
{
  switch (id.u) {
  case NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER:
  case NN_ENTITYID_SEDP_BUILTIN_TOPIC_READER:
    return "DCPSTopic";
    break;
  case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER:
  case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER:
    return "DCPSPublication";
    break;
  case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER:
  case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER:
    return "DCPSSubscription";
    break;
  case NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER:
  case NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER:
    return "DCPSParticipant";
    break;
  case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER:
  case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER:
    return "DCPSParticipantMessage";
    break;
  case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER:
  case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER:
    return "DCPSPublicationsSecure";
    break;
  case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER:
  case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER:
    return "DCPSSubscriptionsSecure";
    break;
  case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER:
  case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER:
    return "DCPSParticipantStatelessMessage";
    break;
  case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER:
  case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER:
    return "DCPSParticipantMessageSecure";
    break;
  case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER:
  case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER:
    return "DCPSParticipantVolatileMessageSecure";
    break;
  case NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER:
  case NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER:
    return "DCPSParticipantsSecure";
    break;
  case NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_WRITER:
  case NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_READER:
    return "CMParticipant";
    break;
  case NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_WRITER:
  case NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_READER:
    return "CMPublisher";
    break;
  case NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_WRITER:
  case NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_READER:
    return "CMSubscriber";
    break;
  default:
    return "(null)";
    break;
  }

  return NULL;
}

static void pending_tokens_free(struct pending_tokens *item)
{
  if (item) {
     DDS_Security_ParticipantCryptoTokenSeq_freebuf(&item->tokens);
     ddsrt_free(item);
   }
}

static void add_pending_tokens(struct ddsi_security_globals *gs, const ddsi_guid_t *src, const ddsi_guid_t *dst, const DDS_Security_ParticipantCryptoTokenSeq *tokens)
{
  struct pending_tokens *item;

  item = ddsrt_malloc(sizeof(*item));

  item->guids.src = *src;
  item->guids.dst = *dst;
  DDS_Security_ParticipantCryptoTokenSeq_copy(&item->tokens, tokens);

  ddsrt_mutex_lock(&gs->pending_tokens_lock);
  ddsrt_avl_insert(&pending_tokens_treedef, &gs->pending_tokens, item);
  ddsrt_mutex_unlock(&gs->pending_tokens_lock);
}

static struct pending_tokens * find_pending_tokens_locked(struct ddsi_security_globals *gs, const ddsi_guid_t *src, const ddsi_guid_t *dst)
{
  struct pending_tokens *tokens;
  struct guid_pair guids;
  ddsrt_avl_dpath_t dpath;

  guids.src = *src;
  guids.dst = *dst;

  tokens = ddsrt_avl_lookup_dpath(&pending_tokens_treedef, &gs->pending_tokens, &guids, &dpath);
  if (tokens)
    ddsrt_avl_delete_dpath(&pending_tokens_treedef, &gs->pending_tokens, tokens, &dpath);

  return tokens;
}

static struct pending_tokens * find_pending_tokens(struct ddsi_security_globals *gs, const ddsi_guid_t *src, const ddsi_guid_t *dst)
{
  struct pending_tokens *tokens;

  ddsrt_mutex_lock(&gs->pending_tokens_lock);
  tokens = find_pending_tokens_locked(gs, src, dst);
  ddsrt_mutex_unlock(&gs->pending_tokens_lock);

  return tokens;
}

static void remove_pending_tokens(struct ddsi_security_globals *gs, const ddsi_guid_t *src, const ddsi_guid_t *dst)
{
  struct pending_tokens *tokens;

  ddsrt_mutex_lock(&gs->pending_tokens_lock);
  tokens = find_pending_tokens_locked(gs, src, dst);
  pending_tokens_free(tokens);
  ddsrt_mutex_unlock(&gs->pending_tokens_lock);
}

static void notify_handshake_recv_token(const struct participant *pp, const struct proxy_participant *proxypp)
{
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);
}

static const char * get_reader_topic_name(struct reader *rd)
{
  if (rd->topic) {
    return rd->topic->name;
  }
  return get_builtin_topic_name(rd->e.guid.entityid);
}

static const char * get_writer_topic_name(struct writer *wr)
{
  if (wr->topic) {
    return wr->topic->name;
  }
  return get_builtin_topic_name(wr->e.guid.entityid);
}

bool q_omg_participant_is_secure(const struct participant *pp)
{
  /* TODO: Register local participant. */
  DDSRT_UNUSED_ARG(pp);
  return false;
}

bool q_omg_proxy_participant_is_secure(const struct proxy_participant *proxypp)
{
  /* TODO: Register remote participant */
  DDSRT_UNUSED_ARG(proxypp);
  return false;
}

struct ddsi_security_globals * q_omg_security_globals_new(void)
{
  struct ddsi_security_globals *gs;

  gs = ddsrt_malloc(sizeof(*gs));
  gs->hsadmin = ddsi_handshake_admin_create();
  ddsrt_mutex_init(&gs->pending_tokens_lock);
  ddsrt_avl_init (&pending_tokens_treedef, &gs->pending_tokens);

  return gs;
}

static void pending_tokens_free_wrapper(void *arg)
{
  struct pending_tokens *tokens = arg;
  pending_tokens_free(tokens);
}

void q_omg_security_globals_free(struct ddsi_security_globals *gs)
{
  ddsi_handshake_admin_delete(gs->hsadmin);
  ddsrt_avl_free (&pending_tokens_treedef, &gs->pending_tokens, pending_tokens_free_wrapper);
  ddsrt_mutex_destroy(&gs->pending_tokens_lock);
  ddsrt_free(gs);
}

void q_omg_security_init_remote_participant(struct proxy_participant *proxypp)
{
  DDSRT_UNUSED_ARG(proxypp);
}

static bool q_omg_proxyparticipant_is_authenticated(const struct proxy_participant *proxy_pp)
{
  /* TODO: Handshake */
  DDSRT_UNUSED_ARG(proxy_pp);
  return false;
}

bool q_omg_participant_allow_unauthenticated(struct participant *pp)
{
  DDSRT_UNUSED_ARG(pp);

  return true;
}

bool q_omg_security_check_create_participant(struct participant *pp, uint32_t domain_id)
{
  bool allowed = false;
  DDS_Security_IdentityHandle identity_handle = DDS_SECURITY_HANDLE_NIL;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_ValidationResult_t result = 0;
  DDS_Security_IdentityToken identity_token;
  DDS_Security_PermissionsToken permissions_token = DDS_SECURITY_TOKEN_INIT;
  DDS_Security_PermissionsCredentialToken credential_token = DDS_SECURITY_TOKEN_INIT;
  struct secure_context *sc;
  DDS_Security_Qos par_qos;
  ddsi_guid_t candidate_guid;
  ddsi_guid_t adjusted_guid;
  int r;

  r = q_omg_create_secure_context(pp, &sc);
  if (r < 0)
    goto no_plugin;
  else if (r == 0)
    return true;

  /* Validate local identity */
  ETRACE (pp, "validate_local_identity: candidate_guid: "PGUIDFMT, PGUID (pp->e.guid));

  pp->sec_attr = ddsrt_malloc(sizeof(struct participant_sec_attributes));
  memset(pp->sec_attr, 0, sizeof(struct participant_sec_attributes));

  pp->sec_attr->sc = sc;

  candidate_guid = nn_hton_guid(pp->e.guid);
  q_omg_shallow_copy_security_qos(&par_qos, &(pp->plist->qos));

  result = sc->authentication->validate_local_identity(
      sc->authentication, &identity_handle,
      (DDS_Security_GUID_t *) &adjusted_guid, (DDS_Security_DomainId) domain_id, &par_qos,
      (DDS_Security_GUID_t *) &candidate_guid, &exception);
  if (result != DDS_SECURITY_VALIDATION_OK)
  {
    EXCEPTION_ERROR(sc, &exception, "Error occurred while validating local permission");
    goto validation_failed;
  }
  pp->e.guid = nn_ntoh_guid(adjusted_guid);

  ETRACE (pp, " adjusted_guid: "PGUIDFMT"", PGUID (pp->e.guid));

  /* Get the identity token and add this to the plist of the participant */
  if (!sc->authentication->get_identity_token(sc->authentication, &identity_token, identity_handle, &exception))
  {
    EXCEPTION_ERROR(sc, &exception, "Error occurred while retrieving the identity token");
    goto validation_failed;
  }

  assert(identity_token.class_id);
  q_omg_security_dataholder_copyin(&pp->plist->identity_token, &identity_token);
  DDS_Security_DataHolder_deinit(&identity_token);
  pp->plist->present |= PP_IDENTITY_TOKEN;

  /* ask to access control security plugin for create participant permissions related to this identity*/
  allowed = sc->access_control->check_create_participant(sc->access_control, pp->permissions_handle, (DDS_Security_DomainId) domain_id, &par_qos, &exception);
  if (!allowed)
  {
    EXCEPTION_ERROR(sc, &exception, "It is not allowed to create participant");
    goto not_allowed;
  }

  /* Get the identity token and add this to the plist of the participant */
  if (!sc->access_control->get_permissions_token(sc->access_control, &permissions_token, pp->permissions_handle, &exception))
  {
    EXCEPTION_ERROR(sc, &exception, "Error occurred while retrieving the permissions token");
    goto not_allowed;
  }

  assert(permissions_token.class_id);
  q_omg_security_dataholder_copyin(&pp->plist->permissions_token, &permissions_token);
  pp->plist->present |= PP_PERMISSIONS_TOKEN;

  if (!sc->access_control->get_permissions_credential_token(sc->access_control, &credential_token, pp->permissions_handle, &exception))
  {
    EXCEPTION_ERROR(sc, &exception, "Error occurred while retrieving the permissions credential token");
    goto no_credentials;
  }

  if (!sc->authentication->set_permissions_credential_and_token(sc->authentication, pp->local_identity_handle, &credential_token, &permissions_token, &exception))
  {
    EXCEPTION_ERROR(sc, &exception, "Error occurred while setting the permissions credential token");
    goto no_credentials;
  }

  if (!sc->access_control->get_participant_sec_attributes(sc->access_control, pp->permissions_handle, &pp->sec_attr->attr, &exception))
  {
    EXCEPTION_ERROR(sc, &exception, "Failed to get participant security attributes");
    goto no_sec_attr;
  }

  pp->sec_attr->plugin_attr = true;
  pp->sec_attr->crypto_handle = sc->crypto->crypto_key_factory->register_local_participant(
            sc->crypto->crypto_key_factory, pp->local_identity_handle, pp->permissions_handle, NULL, &pp->sec_attr->attr, &exception);
  if (!pp->sec_attr->crypto_handle) {
    EXCEPTION_ERROR(sc, &exception, "Failed to register participant with crypto key factory");
    goto no_crypto;
  }

  allowed = true;

no_crypto:
no_sec_attr:
  if (permissions_token.class_id)
    (void)sc->access_control->return_permissions_token(sc->access_control, &permissions_token, NULL);
  if (credential_token.class_id)
    (void)sc->access_control->return_permissions_credential_token(sc->access_control, &credential_token, NULL);
no_credentials:
  (void)sc->access_control->return_permissions_token(sc->access_control, &permissions_token, NULL);
not_allowed:
validation_failed:
  q_omg_shallow_free_security_qos(&par_qos);
no_plugin:
  return allowed;
}

static struct proxypp_pp_match * proxypp_pp_match_new(DDS_Security_IdentityHandle participant_handle, DDS_Security_PermissionsHandle permissions_hdl, DDS_Security_SharedSecretHandle shared_secret)
{
  struct proxypp_pp_match *pm;

  pm = ddsrt_malloc(sizeof(*pm));
  pm->participant_identity = participant_handle;
  pm->permissions_handle = permissions_hdl;
  pm->shared_secret = shared_secret;

  return pm;
}

static void proxypp_pp_match_free(struct secure_context *sc, struct proxypp_pp_match *pm)
{
  if (pm->permissions_handle != DDS_SECURITY_HANDLE_NIL) {
    DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;

    if (!sc->access_control->return_permissions_handle(sc->access_control, pm->permissions_handle, &exception))
    {
      EXCEPTION_ERROR(sc, &exception, "Failed to return permissions handle");
    }
  }
  ddsrt_free(pm);
}

static void q_omg_proxypp_pp_unrelate(struct secure_context *sc, struct proxy_participant *proxypp, struct participant *pp)
{
  if (proxypp->sec_attr) {
    struct proxypp_pp_match *pm;

    if ((pm = ddsrt_avl_lookup (&proxypp_pp_treedef, &proxypp->sec_attr->local_participants, &pp->local_identity_handle)) != NULL) {
      if (!pm->tokens_available)
        remove_pending_tokens(pp->e.gv->g_security, &proxypp->e.guid, &pp->e.guid);
      proxypp_pp_match_free(sc, pm);
    }
  }
}

static void remove_participant_from_remote_entities(struct secure_context *sc, struct participant *pp)
{
  struct proxy_participant *proxypp;
  struct entidx_enum_proxy_participant it;

  entidx_enum_proxy_participant_init(&it, pp->e.gv->entity_index);
  while ((proxypp = entidx_enum_proxy_participant_next(&it)) != NULL)
  {
    ddsrt_mutex_lock(&proxypp->e.lock);
    q_omg_proxypp_pp_unrelate(sc, proxypp, pp);
    ddsrt_mutex_unlock(&proxypp->e.lock);
  }
  entidx_enum_proxy_participant_fini(&it);
}

struct cleanup_participant_crypto_handle_arg {
  struct secure_context *sc;
  ddsi_guid_t guid;
  DDS_Security_ParticipantCryptoHandle handle;
};

static void cleanup_participant_crypto_handle(void *arg)
{
  struct cleanup_participant_crypto_handle_arg *info = arg;

  (void)info->sc->crypto->crypto_key_factory->unregister_participant(info->sc->crypto->crypto_key_factory, info->handle, NULL);

  ddsrt_free(arg);
}

void q_omg_security_deregister_participant(struct participant *pp)
{
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  struct secure_context *sc;

  assert(pp);

  if ((sc = q_omg_security_get_secure_context(pp)) != NULL) {
    remove_participant_from_remote_entities(sc, pp);

    /* When the participant is deleted the timed event queue may still contain
     * messages from this participant. Therefore the crypto handle should still
     * be available to ensure that the rtps message can be encoded.
     * For this purpose the cleanup of the associated crypto handle is delayed.
     * A callback is scheduled to be called after some delay to cleanup this
     * crypto handle.
     */
    if (pp->sec_attr->crypto_handle != DDS_SECURITY_HANDLE_NIL) {
      struct cleanup_participant_crypto_handle_arg *arg = ddsrt_malloc (sizeof (*arg));
      arg->sc = sc;
      arg->handle = pp->sec_attr->crypto_handle;
      arg->guid = pp->e.guid;
      qxev_nt_callback(pp->e.gv->xevents, cleanup_participant_crypto_handle, arg);
    }

    if (pp->permissions_handle != DDS_SECURITY_HANDLE_NIL)
    {
      if (!sc->access_control->return_permissions_handle(sc->access_control, pp->permissions_handle, &exception))
      {
        EXCEPTION_ERROR(sc, &exception, "Failed to return permissions handle");
      }
    }
    if (pp->local_identity_handle != DDS_SECURITY_HANDLE_NIL)
    {
      if (!sc->authentication->return_identity_handle(sc->authentication, pp->local_identity_handle, &exception))
      {
        EXCEPTION_ERROR(sc, &exception, "Failed to return identity handle");
      }

    }
    if (pp->sec_attr->plugin_attr)
    {
      if (!sc->access_control->return_participant_sec_attributes(sc->access_control, &pp->sec_attr->attr, &exception))
      {
        EXCEPTION_ERROR(sc, &exception, "Failed to return participant security attributes");
      }
    }

    ddsrt_free(pp->sec_attr);
  }
}

int64_t q_omg_security_get_local_participant_handle(struct participant *pp)
{
  assert(pp);

  if (pp->sec_attr) {
    return pp->sec_attr->crypto_handle;
  }
  return 0;
}

static bool q_omg_participant_is_access_protected(struct participant *pp)
{
  if (pp->sec_attr) {
    return pp->sec_attr->attr.is_access_protected;
  }
  return false;
}

bool q_omg_get_participant_security_info(struct participant *pp, nn_security_info_t *info)
{
  assert(pp);
  assert(info);

  if (q_omg_participant_is_secure(pp)) {
    DDS_Security_ParticipantSecurityAttributes *attr = &(pp->sec_attr->attr);

    info->security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    info->plugin_security_attributes = attr->plugin_participant_attributes;

    if (attr->is_discovery_protected)
      info->security_attributes |= NN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_DISCOVERY_PROTECTED;

    if (attr->is_liveliness_protected)
      info->security_attributes |= NN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_LIVELINESS_PROTECTED;

    if (attr->is_rtps_protected)
      info->security_attributes |= NN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_RTPS_PROTECTED;

    return true;
  }

  info->security_attributes = 0;
  info->plugin_security_attributes = 0;

  return false;
}

static bool is_topic_discovery_protected(DDS_Security_PermissionsHandle permission_handle, dds_security_access_control *access_control, const char *topic_name)
{
  DDS_Security_TopicSecurityAttributes attributes = {0,0,0,0};
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;

  if (access_control->get_topic_sec_attributes(access_control, permission_handle, topic_name, &attributes, &exception))
    return attributes.is_discovery_protected;
  else
    security_exception_clear(&exception);
  return false;
}

bool q_omg_security_check_create_topic(const struct q_globals *gv, const ddsi_guid_t *pp_guid, const char *topic_name, const struct dds_qos *qos)
{
  bool result = true;
  struct participant *pp;
  struct secure_context *sc;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_Qos topic_qos;

  thread_state_awake (lookup_thread_state (), gv);
  pp = entidx_lookup_participant_guid (gv->entity_index, pp_guid);

  if ((sc = q_omg_security_get_secure_context(pp)) != NULL)
  {
    q_omg_shallow_copy_security_qos(&topic_qos, qos);
    result = sc->access_control->check_create_topic(sc->access_control, pp->permissions_handle, (DDS_Security_DomainId)gv->config.domainId, topic_name, &topic_qos, &exception);
    if (!result)
    {
      /*log if the topic discovery is not protected*/
      if (!is_topic_discovery_protected(pp->permissions_handle, sc->access_control, topic_name))
        EXCEPTION_ERROR(sc, &exception, "Local topic permission denied");
      else
        security_exception_clear(&exception);
    }
    q_omg_shallow_free_security_qos(&topic_qos);
  }
  thread_state_asleep (lookup_thread_state ());

  return result;
}

static struct writer_sec_attributes * writer_sec_attributes_new(void)
{
  struct writer_sec_attributes *attr;

  attr = ddsrt_malloc(sizeof(*attr));
  attr->crypto_handle = DDS_SECURITY_HANDLE_NIL;
  attr->plugin_attr = false;
  return attr;
}

static void writer_sec_attributes_free(struct writer_sec_attributes *attr)
{
  if (attr) {
    ddsrt_free(attr);
  }
}

bool q_omg_security_check_create_writer(struct participant *pp, uint32_t domain_id, const char *topic_name, const struct dds_qos *writer_qos)
{
  struct secure_context *sc;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_PartitionQosPolicy partitions;
  DDS_Security_Qos security_qos;
  bool result;

  if ((sc = q_omg_security_get_secure_context(pp)) == NULL)
    return true;

  if (writer_qos->present & QP_PARTITION)
    g_omg_shallow_copy_StringSeq(&partitions.name, &(writer_qos->partition));
  else
    memset(&(partitions), 0, sizeof(DDS_Security_PartitionQosPolicy));

  q_omg_shallow_copy_security_qos(&security_qos, writer_qos);

  result = sc->access_control->check_create_datawriter(sc->access_control, pp->permissions_handle, (DDS_Security_DomainId)domain_id, topic_name, &security_qos, &partitions, NULL, &exception);
  if (!result)
  {
    /*log if the topic discovery is not protected*/
    if (!is_topic_discovery_protected( pp->permissions_handle, sc->access_control, topic_name))
      EXCEPTION_ERROR(sc, &exception, "Local topic permission denied");
    else
      security_exception_clear(&exception);
  }

  q_omg_shallow_free_security_qos(&security_qos);
  g_omg_shallow_free_StringSeq(&partitions.name);

  return result;
}

void q_omg_security_register_writer(struct writer *wr)
{
  struct secure_context *sc;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_PartitionQosPolicy partitions;
  DDS_Security_PropertySeq properties;
  struct participant *pp = NULL;
  const char *topic_name;

  assert(wr);

  pp = wr->c.pp;

  if ((sc = q_omg_security_get_secure_context(pp)) == NULL)
     return;

  if (wr->xqos->present & QP_PARTITION)
    g_omg_shallow_copy_StringSeq(&partitions.name, &(wr->xqos->partition));
  else
    memset(&(partitions), 0, sizeof(DDS_Security_PartitionQosPolicy));

  wr->sec_attr = writer_sec_attributes_new();
  topic_name = get_writer_topic_name(wr);
  if (!sc->access_control->get_datawriter_sec_attributes(sc->access_control, pp->permissions_handle, topic_name, &partitions, NULL, &wr->sec_attr->attr, &exception))
  {
    EXCEPTION_ERROR(sc, &exception, "Failed to retrieve writer security attributes");
    goto no_attr;
  }
  wr->sec_attr->plugin_attr = true;

  if (wr->sec_attr->attr.is_payload_protected || wr->sec_attr->attr.is_submessage_protected)
  {
    if (wr->xqos->present & QP_PROPERTY_LIST)
      q_omg_copy_PropertySeq(&properties, &wr->xqos->property.value);
    else
      memset(&properties, 0, sizeof(DDS_Security_PropertySeq));

    wr->sec_attr->crypto_handle = sc->crypto->crypto_key_factory->register_local_datawriter(
        sc->crypto->crypto_key_factory, pp->sec_attr->crypto_handle, &properties, &wr->sec_attr->attr, &exception);
    DDS_Security_PropertySeq_freebuf(&properties);
    if (wr->sec_attr->crypto_handle == DDS_SECURITY_HANDLE_NIL)
    {
      EXCEPTION_ERROR(sc, &exception, "Failed to register writer with crypto");
      goto not_registered;
    }
  }

  if (wr->sec_attr->attr.is_key_protected)
    wr->include_keyhash = 1;

not_registered:
no_attr:
  g_omg_shallow_free_StringSeq(&partitions.name);
}

void q_omg_security_deregister_writer(struct writer *wr)
{
  struct secure_context *sc;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;

  assert(wr);

  if (wr->c.pp == NULL)
    return;

  if ((sc = q_omg_security_get_secure_context(wr->c.pp)) == NULL)
    return;

  if (wr->sec_attr)
  {
    if (wr->sec_attr->crypto_handle != DDS_SECURITY_HANDLE_NIL)
    {
      if (!sc->crypto->crypto_key_factory->unregister_datawriter(sc->crypto->crypto_key_factory, wr->sec_attr->crypto_handle, &exception))
      {
        EXCEPTION_ERROR(sc, &exception, "Failed to unregister writer with crypto");
      }
    }
    if (wr->sec_attr->plugin_attr)
    {
      if (!sc->access_control->return_datawriter_sec_attributes(sc->access_control, &wr->sec_attr->attr, &exception))
      {
        EXCEPTION_ERROR(sc, &exception, "Failed to return writer security attributes");
      }
    }
    writer_sec_attributes_free(wr->sec_attr);
    wr->sec_attr = NULL;
  }
}

static bool q_omg_writer_is_discovery_protected(const struct writer *wr)
{
  /* TODO: Register local writer. */
  DDSRT_UNUSED_ARG(wr);
  return false;
}

bool q_omg_get_writer_security_info(const struct writer *wr, nn_security_info_t *info)
{
  assert(wr);
  assert(info);
  /* TODO: Register local writer. */
  DDSRT_UNUSED_ARG(wr);

  info->plugin_security_attributes = 0;
  if (q_omg_writer_is_payload_protected(wr))
  {
    info->security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID|
                                NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_PAYLOAD_PROTECTED;
  }
  else
  {
    info->security_attributes = 0;
  }
  return true;
}



static struct reader_sec_attributes * reader_sec_attributes_new(void)
{
  struct reader_sec_attributes *attr;

  attr = ddsrt_malloc(sizeof(*attr));
  attr->crypto_handle = DDS_SECURITY_HANDLE_NIL;
  attr->plugin_attr = false;

  return attr;
}

static void reader_sec_attributes_free(struct reader_sec_attributes *attr)
{
  if (attr) {
    ddsrt_free(attr);
  }
}

bool q_omg_security_check_create_reader(struct participant *pp, uint32_t domain_id, const char *topic_name, const struct dds_qos *reader_qos)
{
  struct secure_context *sc;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_PartitionQosPolicy partitions;
  DDS_Security_Qos security_qos;
  bool result;

  if ((sc = q_omg_security_get_secure_context(pp)) == NULL)
    return true;

  if (reader_qos->present & QP_PARTITION)
    g_omg_shallow_copy_StringSeq(&partitions.name, &(reader_qos->partition));
  else
    memset(&(partitions), 0, sizeof(DDS_Security_PartitionQosPolicy));

  q_omg_shallow_copy_security_qos(&security_qos, reader_qos);

  result = sc->access_control->check_create_datareader(sc->access_control, pp->permissions_handle, (DDS_Security_DomainId)domain_id, topic_name, &security_qos, &partitions, NULL, &exception);
  if (!result)
  {
    /*log if the topic discovery is not protected*/
    if (!is_topic_discovery_protected( pp->permissions_handle, sc->access_control, topic_name))
      EXCEPTION_ERROR(sc, &exception, "Reader is not permitted");
    else
      security_exception_clear(&exception);
  }

  q_omg_shallow_free_security_qos(&security_qos);
  g_omg_shallow_free_StringSeq(&partitions.name);

  return result;
}

void q_omg_security_register_reader(struct reader *rd)
{
  struct secure_context *sc;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_PartitionQosPolicy partitions;
  DDS_Security_PropertySeq properties;
  struct participant *pp = NULL;
  const char *topic_name;

  assert(rd);

  pp = rd->c.pp;

  if ((sc = q_omg_security_get_secure_context(pp)) == NULL)
    return;

  if (rd->xqos->present & QP_PARTITION)
    g_omg_shallow_copy_StringSeq(&partitions.name, &(rd->xqos->partition));
  else
    memset(&(partitions), 0, sizeof(DDS_Security_PartitionQosPolicy));

  rd->sec_attr = reader_sec_attributes_new();

  topic_name = get_reader_topic_name(rd);
  if (!sc->access_control->get_datareader_sec_attributes(sc->access_control, pp->permissions_handle, topic_name, &partitions, NULL, &rd->sec_attr->attr, &exception))
  {
    EXCEPTION_ERROR(sc, &exception, "Failed to retrieve reader security attributes");
    goto no_attr;
  }
  rd->sec_attr->plugin_attr = true;

  if (rd->sec_attr->attr.is_payload_protected || rd->sec_attr->attr.is_submessage_protected)
  {
    if (rd->xqos->present & QP_PROPERTY_LIST)
      q_omg_copy_PropertySeq(&properties, &rd->xqos->property.value);
    else
      memset(&properties, 0, sizeof(DDS_Security_PropertySeq));

    rd->sec_attr->crypto_handle = sc->crypto->crypto_key_factory->register_local_datareader(
        sc->crypto->crypto_key_factory, pp->sec_attr->crypto_handle, &properties, &rd->sec_attr->attr, &exception);
    DDS_Security_PropertySeq_freebuf(&properties);
    if (rd->sec_attr->crypto_handle == DDS_SECURITY_HANDLE_NIL)
    {
      EXCEPTION_ERROR(sc, &exception, "Failed to register reader with crypto");
      goto not_registered;
    }
  }

not_registered:
no_attr:
  g_omg_shallow_free_StringSeq(&partitions.name);
}

void q_omg_security_deregister_reader(struct reader *rd)
{
  struct secure_context *sc;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;

  assert(rd);

  if (rd->c.pp == NULL)
     return;

  if ((sc = q_omg_security_get_secure_context(rd->c.pp)) == NULL)
     return;

  if (rd->sec_attr)
  {
    if (rd->sec_attr->crypto_handle != DDS_SECURITY_HANDLE_NIL)
    {
      if (!sc->crypto->crypto_key_factory->unregister_datareader(sc->crypto->crypto_key_factory, rd->sec_attr->crypto_handle, &exception))
      {
        EXCEPTION_ERROR(sc, &exception, "Failed to unregister reader with crypto");
      }
    }
    if (rd->sec_attr->plugin_attr)
    {
      if (!sc->access_control->return_datareader_sec_attributes(sc->access_control, &rd->sec_attr->attr, &exception))
      {
        EXCEPTION_ERROR(sc, &exception, "Failed to return reader security attributes");
      }
    }
    reader_sec_attributes_free(rd->sec_attr);
    rd->sec_attr = NULL;
  }
}

static bool q_omg_reader_is_discovery_protected(const struct reader *rd)
{
  /* TODO: Register local reader. */
  DDSRT_UNUSED_ARG(rd);
  return false;
}

bool q_omg_get_reader_security_info(const struct reader *rd, nn_security_info_t *info)
{
  assert(rd);
  assert(info);
  /* TODO: Register local reader. */
  DDSRT_UNUSED_ARG(rd);
  info->plugin_security_attributes = 0;
  info->security_attributes = 0;
  return false;
}

unsigned determine_subscription_writer(const struct reader *rd)
{
  if (q_omg_reader_is_discovery_protected(rd))
  {
    return NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER;
  }
  return NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER;
}

unsigned determine_publication_writer(const struct writer *wr)
{
  if (q_omg_writer_is_discovery_protected(wr))
  {
    return NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER;
  }
  return NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER;
}

int64_t q_omg_security_check_remote_participant_permissions(uint32_t domain_id, struct participant *pp, struct proxy_participant *proxypp)
{
  struct secure_context *sc;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  struct ddsi_handshake *handshake;
  DDS_Security_PermissionsToken permissions_token = DDS_SECURITY_TOKEN_INIT;
  DDS_Security_AuthenticatedPeerCredentialToken peer_credential_token = DDS_SECURITY_TOKEN_INIT;
  int64_t permissions_hdl = DDS_SECURITY_HANDLE_NIL;

  if ((sc = q_omg_security_get_secure_context(pp)) == NULL)
  {
    assert(false);
    return 0;
  }

  ddsrt_mutex_lock(&proxypp->e.lock);

  if (proxypp->plist->present & PP_PERMISSIONS_TOKEN)
      q_omg_shallow_copyin_DataHolder(&permissions_token, &proxypp->plist->permissions_token);
  else
      memset(&permissions_token, 0, sizeof(DDS_Security_PermissionsToken));

  handshake = ddsi_handshake_find(pp, proxypp);
  if (!handshake)
  {
    ELOG(DDS_LC_ERROR, pp, "Could not find handshake local participant "PGUIDFMT" and remote participant "PGUIDFMT,
                PGUID(pp->e.guid), PGUID(proxypp->e.guid));
      goto no_handshake;
  }

  if (!sc->authentication->get_authenticated_peer_credential_token(sc->authentication, &peer_credential_token, ddsi_handshake_get_handle(handshake), &exception))
  {
    if (q_omg_participant_is_access_protected(pp))
    {
      EXCEPTION_ERROR(sc, &exception, "Could not authenticate_peer_credential_token for local participant "PGUIDFMT" and remote participant "PGUIDFMT,
          PGUID(pp->e.guid), PGUID(proxypp->e.guid));
      goto no_credentials;
    }
    /* Failing is allowed due to the non-protection of access. */
    EXCEPTION_WARNING(sc, &exception, "Could not authenticate_peer_credential_token for local participant "PGUIDFMT" and remote participant "PGUIDFMT ,
        PGUID(pp->e.guid), PGUID(proxypp->e.guid));
  }

  permissions_hdl = sc->access_control->validate_remote_permissions(
      sc->access_control, sc->authentication, pp->local_identity_handle, proxypp->remote_identity_handle, &permissions_token, &peer_credential_token, &exception);
  if (permissions_hdl == DDS_SECURITY_HANDLE_NIL)
  {
    if (q_omg_participant_is_access_protected(pp))
    {
      EXCEPTION_ERROR(sc, &exception, "Could not get remote participant "PGUIDFMT" permissions from plugin", PGUID(proxypp->e.guid));
      goto no_permissions;
    }
    /* Failing is allowed due to the non-protection of access. */
    EXCEPTION_WARNING(sc, &exception, "Could not get remote participant "PGUIDFMT" permissions from plugin", PGUID(proxypp->e.guid));
  }

  /* Only check remote participant if joining access is protected. */
  if (q_omg_participant_is_access_protected(pp))
  {
    DDS_Security_ParticipantBuiltinTopicDataSecure participant_data;

    q_omg_shallow_copy_ParticipantBuiltinTopicDataSecure(&participant_data, &(proxypp->e.guid), proxypp->plist);
    if (!sc->access_control->check_remote_participant(sc->access_control, permissions_hdl, (DDS_Security_DomainId)domain_id, &participant_data, &exception))
    {
      EXCEPTION_WARNING(sc, &exception, "Plugin does not allow remote participant "PGUIDFMT,  PGUID(proxypp->e.guid));
      if (!sc->access_control->return_permissions_handle(sc->access_control, permissions_hdl, &exception))
      {
        EXCEPTION_ERROR(sc, &exception, "Failed to return permissions handle");
      }
      permissions_hdl = DDS_SECURITY_HANDLE_NIL;
    }
    q_omg_shallow_free_ParticipantBuiltinTopicDataSecure(&participant_data);
  }

no_permissions:
  if (!sc->authentication->return_authenticated_peer_credential_token(sc->authentication, &peer_credential_token, &exception))
  {
    EXCEPTION_ERROR(sc, &exception, "Failed to return peer credential token");
  }
no_credentials:
  ddsi_handshake_release(handshake);
no_handshake:
  q_omg_shallow_free_DataHolder(&permissions_token);
  ddsrt_mutex_unlock(&proxypp->e.lock);
  return permissions_hdl;
}

static void send_participant_crypto_tokens(struct participant *pp, struct proxy_participant *proxypp, DDS_Security_ParticipantCryptoHandle local_crypto, DDS_Security_ParticipantCryptoHandle remote_crypto)
{
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);
  DDSRT_UNUSED_ARG(local_crypto);
  DDSRT_UNUSED_ARG(remote_crypto);
}

void q_omg_security_register_remote_participant(struct participant *pp, struct proxy_participant *proxypp, int64_t shared_secret, int64_t proxy_permissions)
{
  bool r;
  struct q_globals *gv = pp->e.gv;
  struct secure_context *sc;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_ParticipantCryptoHandle crypto_handle;
  struct proxypp_pp_match *pm;
  struct pending_tokens *pending;

  if ((sc = q_omg_security_get_secure_context(pp)) == NULL)
    return;

  GVTRACE("register remote participant "PGUIDFMT" with "PGUIDFMT"\n", PGUID(proxypp->e.guid), PGUID(pp->e.guid));

  crypto_handle = sc->crypto->crypto_key_factory->register_matched_remote_participant(
      sc->crypto->crypto_key_factory, pp->sec_attr->crypto_handle,
      proxypp->remote_identity_handle, proxy_permissions, shared_secret, &exception);
  if (crypto_handle == DDS_SECURITY_HANDLE_NIL)
  {
    EXCEPTION_ERROR(sc, &exception, "Failed to register matched remote participant "PGUIDFMT" with participant "PGUIDFMT, PGUID(proxypp->e.guid), PGUID(pp->e.guid));
    goto register_failed;
  }

  if (proxypp->sec_attr->crypto_handle == DDS_SECURITY_HANDLE_NIL)
    proxypp->sec_attr->crypto_handle = crypto_handle;
  else
    assert(proxypp->sec_attr->crypto_handle == crypto_handle);

  pending = find_pending_tokens(sc->gv->g_security, &proxypp->e.guid, &pp->e.guid);

  pm = proxypp_pp_match_new(pp->local_identity_handle, proxy_permissions, shared_secret);
  ddsrt_mutex_lock(&proxypp->e.lock);
  ddsrt_avl_insert(&proxypp_pp_treedef, &proxypp->sec_attr->local_participants, pm);
  if (pending)
  {
    r = sc->crypto->crypto_key_exchange->set_remote_participant_crypto_tokens(
          sc->crypto->crypto_key_exchange, pp->sec_attr->crypto_handle,
          proxypp->sec_attr->crypto_handle, &pending->tokens, &exception);
    if (r)
    {
      pm->tokens_available = true;
      GVTRACE("set participant tokens src("PGUIDFMT") to dst("PGUIDFMT") (by registering remote)\n", PGUID(proxypp->e.guid), PGUID(pp->e.guid));
    }
    else
    {
      EXCEPTION_ERROR(sc, &exception, "Failed to set remote participant crypto tokens "PGUIDFMT" --> "PGUIDFMT, PGUID(proxypp->e.guid), PGUID(pp->e.guid));
    }
  }
  ddsrt_mutex_unlock(&proxypp->e.lock);

  if (pending)
    pending_tokens_free(pending);

  send_participant_crypto_tokens(pp, proxypp, pp->sec_attr->crypto_handle, proxypp->sec_attr->crypto_handle);

register_failed:
  return;
}

void q_omg_security_deregister_remote_participant(struct proxy_participant *proxypp)
{
  DDSRT_UNUSED_ARG(proxypp);
}

bool is_proxy_participant_deletion_allowed(struct q_globals * const gv, const struct ddsi_guid *guid, const ddsi_entityid_t pwr_entityid)
{
  struct proxy_participant *proxypp;

  assert(gv);
  assert(guid);

  /* TODO: Check if the proxy writer guid prefix matches that of the proxy
   *       participant. Deletion is not allowed when they're not equal. */

  /* Always allow deletion from a secure proxy writer. */
  if (pwr_entityid.u == NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER)
    return true;

  /* Not from a secure proxy writer.
   * Only allow deletion when proxy participant is not authenticated. */
  proxypp = entidx_lookup_proxy_participant_guid(gv->entity_index, guid);
  if (!proxypp)
  {
    GVLOGDISC (" unknown");
    return false;
  }
  return (!q_omg_proxyparticipant_is_authenticated(proxypp));
}

bool q_omg_is_similar_participant_security_info(struct participant *pp, struct proxy_participant *proxypp)
{
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);

  return true;
}

void q_omg_security_set_participant_crypto_tokens(struct participant *pp, struct proxy_participant *proxypp, const nn_dataholderseq_t *tokens)
{
  struct q_globals *gv = pp->e.gv;
  struct secure_context *sc;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;

  if ((sc = q_omg_security_get_secure_context(pp)) == NULL)
    return;

  if (proxypp->sec_attr->crypto_handle != DDS_SECURITY_HANDLE_NIL)
  {
    struct proxypp_pp_match *pm = NULL;
    DDS_Security_DatawriterCryptoTokenSeq tseq;

    q_omg_shallow_copyin_DataHolderSeq(&tseq, tokens);

    ddsrt_mutex_lock(&proxypp->e.lock);
    pm = ddsrt_avl_lookup (&proxypp_pp_treedef, &proxypp->sec_attr->local_participants, &pp->local_identity_handle);
    ddsrt_mutex_unlock(&proxypp->e.lock);

    if (pm)
    {
      if (sc->crypto->crypto_key_exchange->set_remote_participant_crypto_tokens(sc->crypto->crypto_key_exchange, pp->sec_attr->crypto_handle, proxypp->sec_attr->crypto_handle, &tseq, &exception))
      {
        pm->tokens_available = true;
        GVTRACE("set participant tokens src("PGUIDFMT") dst("PGUIDFMT")\n", PGUID(proxypp->e.guid), PGUID(pp->e.guid));
      }
      else
      {
        EXCEPTION_ERROR(sc, &exception, "Failed to set remote participant crypto tokens "PGUIDFMT" for participant "PGUIDFMT, PGUID(proxypp->e.guid), PGUID(pp->e.guid));
      }
    }
    else
    {
      GVTRACE("remember participant tokens src("PGUIDFMT") dst("PGUIDFMT")\n", PGUID(proxypp->e.guid), PGUID(pp->e.guid));
      add_pending_tokens(sc->gv->g_security, &proxypp->e.guid, &pp->e.guid, &tseq);
    }
    notify_handshake_recv_token(pp, proxypp);
    q_omg_shallow_free_DataHolderSeq(&tseq);
  }
}

void q_omg_security_participant_send_tokens(struct participant *pp, struct proxy_participant *proxypp)
{
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);
}

int64_t q_omg_security_get_remote_participant_handle(struct proxy_participant *proxypp)
{
  DDSRT_UNUSED_ARG(proxypp);
  return 0;
}

bool q_omg_security_is_remote_rtps_protected(struct proxy_participant *proxy_pp, ddsi_entityid_t entityid)
{
  /* TODO: Handshake */
  DDSRT_UNUSED_ARG(proxy_pp);
  DDSRT_UNUSED_ARG(entityid);
  return false;
}

bool q_omg_security_is_local_rtps_protected(struct participant *pp, ddsi_entityid_t entityid)
{
  /* TODO: Handshake */
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(entityid);
  return false;
}

void set_proxy_participant_security_info(struct proxy_participant *proxypp, const nn_plist_t *plist)
{
  assert(proxypp);
  assert(plist);
  if (plist->present & PP_PARTICIPANT_SECURITY_INFO) {
    proxypp->security_info.security_attributes = plist->participant_security_info.security_attributes;
    proxypp->security_info.plugin_security_attributes = plist->participant_security_info.plugin_security_attributes;
  } else {
    proxypp->security_info.security_attributes = 0;
    proxypp->security_info.plugin_security_attributes = 0;
  }
}

bool q_omg_security_check_remote_writer_permissions(const struct proxy_writer *pwr, uint32_t domain_id, struct participant *pp)
{
  DDSRT_UNUSED_ARG(pwr);
  DDSRT_UNUSED_ARG(domain_id);
  DDSRT_UNUSED_ARG(pp);

  assert(pwr);
  assert(pp);
  assert(pwr->c.proxypp);

  return true;
}

bool q_omg_security_match_remote_writer_enabled(struct reader *rd, struct proxy_writer *pwr)
{
  DDSRT_UNUSED_ARG(rd);
  DDSRT_UNUSED_ARG(pwr);

  assert(rd);
  assert(pwr);

  return true;
}

void q_omg_security_deregister_remote_writer_match(const struct proxy_writer *pwr, const struct reader *rd, struct rd_pwr_match *match)
{
  struct secure_context *sc;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;

  if ((sc = q_omg_security_get_secure_context(rd->c.pp)) == NULL)
    return;

  if (!match->tokens_available)
    remove_pending_tokens(rd->e.gv->g_security, &rd->e.guid, &pwr->e.guid);

  if (match->crypto_handle != 0)
  {
    if (!sc->crypto->crypto_key_factory->unregister_datawriter(sc->crypto->crypto_key_factory, match->crypto_handle, &exception))
    {
      EXCEPTION_ERROR(sc, &exception, "Failed to unregster remote writer "PGUIDFMT" for reader "PGUIDFMT, PGUID(pwr->e.guid), PGUID(rd->e.guid));
    }
  }
}

bool q_omg_security_check_remote_reader_permissions(const struct proxy_reader *prd, uint32_t domain_id, struct participant *pp)
{
  DDSRT_UNUSED_ARG(prd);
  DDSRT_UNUSED_ARG(domain_id);
  DDSRT_UNUSED_ARG(pp);

  assert(prd);
  assert(pp);
  assert(prd->c.proxypp);

  return true;
}

static void q_omg_get_proxy_endpoint_security_info(const struct entity_common *entity, nn_security_info_t *proxypp_sec_info, const nn_plist_t *plist, nn_security_info_t *info)
{
  bool proxypp_info_available;

  proxypp_info_available = (proxypp_sec_info->security_attributes != 0) ||
                           (proxypp_sec_info->plugin_security_attributes != 0);

  /*
   * If Security info is present, use that.
   * Otherwise, use the specified values for the secure builtin endpoints.
   *      (Table 20 – EndpointSecurityAttributes for all "Builtin Security Endpoints")
   * Otherwise, reset.
   */
  if (plist->present & PP_ENDPOINT_SECURITY_INFO)
  {
    info->security_attributes = plist->endpoint_security_info.security_attributes;
    info->plugin_security_attributes = plist->endpoint_security_info.plugin_security_attributes;
  }
  else if (endpoint_is_DCPSParticipantSecure(&(entity->guid)) ||
           endpoint_is_DCPSPublicationsSecure(&(entity->guid)) ||
           endpoint_is_DCPSSubscriptionsSecure(&(entity->guid)) )
  {
    info->plugin_security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    info->security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    if (proxypp_info_available)
    {
      if (proxypp_sec_info->security_attributes & NN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_DISCOVERY_PROTECTED)
      {
        info->security_attributes |= NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;
      }
      if (proxypp_sec_info->plugin_security_attributes & NN_PLUGIN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_DISCOVERY_ENCRYPTED)
      {
        info->plugin_security_attributes |= NN_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
      }
      if (proxypp_sec_info->plugin_security_attributes & NN_PLUGIN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_DISCOVERY_AUTHENTICATED)
      {
        info->plugin_security_attributes |= NN_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ORIGIN_AUTHENTICATED;
      }
    }
    else
    {
      /* No participant info: assume hardcoded OpenSplice V6.10.0 values. */
      info->security_attributes |= NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;
      info->plugin_security_attributes |= NN_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
    }
  }
  else if (endpoint_is_DCPSParticipantMessageSecure(&(entity->guid)))
  {
    info->plugin_security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    info->security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    if (proxypp_info_available)
    {
      if (proxypp_sec_info->security_attributes & NN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_LIVELINESS_PROTECTED)
      {
        info->security_attributes |= NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;
      }
      if (proxypp_sec_info->plugin_security_attributes & NN_PLUGIN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_LIVELINESS_ENCRYPTED)
      {
        info->plugin_security_attributes |= NN_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
      }
      if (proxypp_sec_info->plugin_security_attributes & NN_PLUGIN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_LIVELINESS_AUTHENTICATED)
      {
        info->plugin_security_attributes |= NN_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ORIGIN_AUTHENTICATED;
      }
    }
    else
    {
      /* No participant info: assume hardcoded OpenSplice V6.10.0 values. */
      info->security_attributes |= NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;
      info->plugin_security_attributes |= NN_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
    }
  }
  else if (endpoint_is_DCPSParticipantStatelessMessage(&(entity->guid)))
  {
    info->security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    info->plugin_security_attributes = 0;
  }
  else if (endpoint_is_DCPSParticipantVolatileMessageSecure(&(entity->guid)))
  {
    info->security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID |
                                NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;
    info->plugin_security_attributes = 0;
  }
  else
  {
    info->security_attributes = 0;
    info->plugin_security_attributes = 0;
  }
}

void q_omg_get_proxy_reader_security_info(struct proxy_reader *prd, const nn_plist_t *plist, nn_security_info_t *info)
{
  q_omg_get_proxy_endpoint_security_info(&(prd->e), &(prd->c.proxypp->security_info), plist, info);
}

void set_proxy_reader_security_info(struct proxy_reader *prd, const nn_plist_t *plist)
{
  assert(prd);
  q_omg_get_proxy_endpoint_security_info(&(prd->e),
                                         &(prd->c.proxypp->security_info),
                                         plist,
                                         &(prd->c.security_info));
}

void q_omg_get_proxy_writer_security_info(struct proxy_writer *pwr, const nn_plist_t *plist, nn_security_info_t *info)
{
  q_omg_get_proxy_endpoint_security_info(&(pwr->e), &(pwr->c.proxypp->security_info), plist, info);
}

void set_proxy_writer_security_info(struct proxy_writer *pwr, const nn_plist_t *plist)
{
  assert(pwr);
  q_omg_get_proxy_endpoint_security_info(&(pwr->e),
                                         &(pwr->c.proxypp->security_info),
                                         plist,
                                         &(pwr->c.security_info));
}

void q_omg_security_deregister_remote_reader_match(const struct proxy_reader *prd, const struct writer *wr, struct wr_prd_match *match)
{
  struct secure_context *sc;
  DDS_Security_SecurityException exception = {0};

  if ((sc = q_omg_security_get_secure_context(wr->c.pp)) == NULL)
    return;

  if (!match->tokens_available)
    remove_pending_tokens(wr->e.gv->g_security, &wr->e.guid, &prd->e.guid);

  if (match->crypto_handle != 0)
  {
    if (!sc->crypto->crypto_key_factory->unregister_datawriter(sc->crypto->crypto_key_factory, match->crypto_handle, &exception))
    {
      EXCEPTION_ERROR(sc, &exception, "Failed to unregster remote reader "PGUIDFMT" for reader "PGUIDFMT, PGUID(prd->e.guid), PGUID(wr->e.guid));
    }
  }
}

bool q_omg_security_match_remote_reader_enabled(struct writer *wr, struct proxy_reader *prd)
{
  DDSRT_UNUSED_ARG(wr);
  DDSRT_UNUSED_ARG(prd);

  assert(wr);
  assert(prd);

  return true;
}

void q_omg_security_set_remote_writer_crypto_tokens(struct reader *rd, const ddsi_guid_t *pwr_guid, const nn_dataholderseq_t *tokens)
{
  struct secure_context *sc;
  struct q_globals *gv = rd->e.gv;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  struct proxy_writer *pwr;

  if ((sc = q_omg_security_get_secure_context(rd->c.pp)) == NULL)
     return;

  pwr = entidx_lookup_proxy_writer_guid(gv->entity_index, pwr_guid);
  if (pwr) {
    DDS_Security_DatawriterCryptoTokenSeq tseq;
    struct rd_pwr_match *match;

    q_omg_shallow_copyin_DataHolderSeq(&tseq, tokens);

    ddsrt_mutex_lock(&rd->e.lock);
    match = ddsrt_avl_lookup (&rd_writers_treedef, &rd->writers, pwr_guid);
    ddsrt_mutex_unlock(&rd->e.lock);

    if (match && match->crypto_handle != 0)
    {
      if (sc->crypto->crypto_key_exchange->set_remote_datawriter_crypto_tokens(sc->crypto->crypto_key_exchange, rd->sec_attr->crypto_handle, match->crypto_handle, &tseq, &exception))
      {
        GVTRACE("set_remote_writer_crypto_tokens "PGUIDFMT" with reader "PGUIDFMT"\n", PGUID(pwr->e.guid), PGUID(rd->e.guid));
        match->tokens_available = true;
        connect_reader_with_proxy_writer_secure(rd, pwr, now_mt ());
      }
      else
      {
        EXCEPTION_ERROR(sc, &exception, "Failed to set remote writer crypto tokens "PGUIDFMT" for reader "PGUIDFMT, PGUID(pwr->e.guid), PGUID(rd->e.guid));
      }
    }
    else
    {
      GVTRACE("remember writer tokens src("PGUIDFMT") dst("PGUIDFMT")\n", PGUID(pwr->e.guid), PGUID(rd->e.guid));
      add_pending_tokens(sc->gv->g_security, pwr_guid, &rd->e.guid, &tseq);
    }
    notify_handshake_recv_token(rd->c.pp, pwr->c.proxypp);
    q_omg_shallow_free_DataHolderSeq(&tseq);
  }
}

void q_omg_security_set_remote_reader_crypto_tokens(struct writer *wr, const ddsi_guid_t *prd_guid, const nn_dataholderseq_t *tokens)
{
  struct secure_context *sc;
  struct q_globals *gv = wr->e.gv;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  struct proxy_reader *prd;

  if ((sc = q_omg_security_get_secure_context(wr->c.pp)) == NULL)
     return;

  prd = entidx_lookup_proxy_reader_guid(gv->entity_index, prd_guid);
  if (prd) {
    DDS_Security_DatawriterCryptoTokenSeq tseq;
    struct wr_prd_match *match;

    q_omg_shallow_copyin_DataHolderSeq(&tseq, tokens);

    ddsrt_mutex_lock(&wr->e.lock);
    match = ddsrt_avl_lookup (&wr_readers_treedef, &wr->readers, prd_guid);
    ddsrt_mutex_unlock(&wr->e.lock);

    if (match && match->crypto_handle != 0)
    {
      if (sc->crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(sc->crypto->crypto_key_exchange, wr->sec_attr->crypto_handle, match->crypto_handle, &tseq, &exception))
      {
        GVTRACE("set_remote_reader_crypto_tokens "PGUIDFMT" with writer "PGUIDFMT"\n", PGUID(prd->e.guid), PGUID(wr->e.guid));
        match->tokens_available = true;
        connect_writer_with_proxy_reader_secure(wr, prd, now_mt ());
      }
      else
      {
        EXCEPTION_ERROR(sc, &exception, "Failed to set remote reader crypto tokens "PGUIDFMT" for writer "PGUIDFMT, PGUID(prd->e.guid), PGUID(wr->e.guid));
      }
    }
    else
    {
      GVTRACE("remember reader tokens src("PGUIDFMT") dst("PGUIDFMT")\n", PGUID(prd->e.guid), PGUID(wr->e.guid));
      add_pending_tokens(sc->gv->g_security, prd_guid, &wr->e.guid, &tseq);
    }
    notify_handshake_recv_token(wr->c.pp, prd->c.proxypp);
    q_omg_shallow_free_DataHolderSeq(&tseq);
  }
}

static bool
q_omg_security_encode_datareader_submessage(
  struct reader            *rd,
  const ddsi_guid_prefix_t *dst_prefix,
  const unsigned char      *src_buf,
  const unsigned int        src_len,
  unsigned char           **dst_buf,
  unsigned int             *dst_len)
{
  /* TODO: Use proper keys to actually encode (need key-exchange). */
  DDSRT_UNUSED_ARG(rd);
  DDSRT_UNUSED_ARG(dst_prefix);
  DDSRT_UNUSED_ARG(src_buf);
  DDSRT_UNUSED_ARG(src_len);
  DDSRT_UNUSED_ARG(dst_buf);
  DDSRT_UNUSED_ARG(dst_len);
  return false;
}

static bool
q_omg_security_encode_datawriter_submessage(
  struct writer            *wr,
  const ddsi_guid_prefix_t *dst_prefix,
  const unsigned char      *src_buf,
  const unsigned int        src_len,
  unsigned char           **dst_buf,
  unsigned int             *dst_len)
{
  /* TODO: Use proper keys to actually encode (need key-exchange). */
  DDSRT_UNUSED_ARG(wr);
  DDSRT_UNUSED_ARG(dst_prefix);
  DDSRT_UNUSED_ARG(src_buf);
  DDSRT_UNUSED_ARG(src_len);
  DDSRT_UNUSED_ARG(dst_buf);
  DDSRT_UNUSED_ARG(dst_len);
  return false;
}

static bool
q_omg_security_decode_submessage(
  const ddsi_guid_prefix_t* const src_prefix,
  const ddsi_guid_prefix_t* const dst_prefix,
  const unsigned char   *src_buf,
  const unsigned int     src_len,
  unsigned char        **dst_buf,
  unsigned int          *dst_len)
{
  /* TODO: Use proper keys to actually decode (need key-exchange). */
  DDSRT_UNUSED_ARG(src_prefix);
  DDSRT_UNUSED_ARG(dst_prefix);
  DDSRT_UNUSED_ARG(src_buf);
  DDSRT_UNUSED_ARG(src_len);
  DDSRT_UNUSED_ARG(dst_buf);
  DDSRT_UNUSED_ARG(dst_len);
  return false;
}

static bool
q_omg_security_encode_serialized_payload(
  const struct writer *wr,
  const unsigned char *src_buf,
  const unsigned int   src_len,
  unsigned char     **dst_buf,
  unsigned int       *dst_len)
{
  /* TODO: Use proper keys to actually encode (need key-exchange). */
  DDSRT_UNUSED_ARG(wr);
  DDSRT_UNUSED_ARG(src_buf);
  DDSRT_UNUSED_ARG(src_len);
  DDSRT_UNUSED_ARG(dst_buf);
  DDSRT_UNUSED_ARG(dst_len);
  return false;
}

static bool
q_omg_security_decode_serialized_payload(
  struct proxy_writer *pwr,
  const unsigned char *src_buf,
  const unsigned int   src_len,
  unsigned char     **dst_buf,
  unsigned int       *dst_len)
{
  /* TODO: Use proper keys to actually decode (need key-exchange). */
  DDSRT_UNUSED_ARG(pwr);
  DDSRT_UNUSED_ARG(src_buf);
  DDSRT_UNUSED_ARG(src_len);
  DDSRT_UNUSED_ARG(dst_buf);
  DDSRT_UNUSED_ARG(dst_len);
  return false;
}

bool
q_omg_security_encode_rtps_message(
  int64_t                 src_handle,
  ddsi_guid_t            *src_guid,
  const unsigned char    *src_buf,
  const unsigned int      src_len,
  unsigned char        **dst_buf,
  unsigned int          *dst_len,
  int64_t                dst_handle)
{
  /* TODO: Use proper keys to actually encode (need key-exchange). */
  DDSRT_UNUSED_ARG(src_handle);
  DDSRT_UNUSED_ARG(src_guid);
  DDSRT_UNUSED_ARG(src_buf);
  DDSRT_UNUSED_ARG(src_len);
  DDSRT_UNUSED_ARG(dst_buf);
  DDSRT_UNUSED_ARG(dst_len);
  DDSRT_UNUSED_ARG(dst_handle);
  return false;
}

static bool
q_omg_security_decode_rtps_message(
  struct proxy_participant *proxypp,
  const unsigned char      *src_buf,
  const unsigned int        src_len,
  unsigned char          **dst_buf,
  unsigned int            *dst_len)
{
  /* TODO: Use proper keys to actually decode (need key-exchange). */
  DDSRT_UNUSED_ARG(proxypp);
  DDSRT_UNUSED_ARG(src_buf);
  DDSRT_UNUSED_ARG(src_len);
  DDSRT_UNUSED_ARG(dst_buf);
  DDSRT_UNUSED_ARG(dst_len);
  return false;
}

static bool
q_omg_writer_is_payload_protected(
  const struct writer *wr)
{
  /* TODO: Local registration. */
  DDSRT_UNUSED_ARG(wr);
  return false;
}

static bool
q_omg_writer_is_submessage_protected(
  struct writer *wr)
{
  /* TODO: Local registration. */
  DDSRT_UNUSED_ARG(wr);
  return false;
}

static bool
q_omg_reader_is_submessage_protected(
  struct reader *rd)
{
  /* TODO: Local registration. */
  DDSRT_UNUSED_ARG(rd);
  return false;
}

bool
encode_payload(
  struct writer *wr,
  ddsrt_iovec_t *vec,
  unsigned char **buf)
{
  bool ok = true;
  *buf = NULL;
  if (q_omg_writer_is_payload_protected(wr))
  {
    /* Encrypt the data. */
    unsigned char *enc_buf;
    unsigned int   enc_len;
    ok = q_omg_security_encode_serialized_payload(
                    wr,
                    vec->iov_base,
                    (unsigned int)vec->iov_len,
                    &enc_buf,
                    &enc_len);
    if (ok)
    {
      /* Replace the iov buffer, which should always be aliased. */
      vec->iov_base = (char *)enc_buf;
      vec->iov_len = enc_len;
      /* Remember the pointer to be able to free the memory. */
      *buf = enc_buf;
    }
  }
  return ok;
}


static bool
decode_payload(
  const struct q_globals *gv,
  struct nn_rsample_info *sampleinfo,
  unsigned char *payloadp,
  uint32_t *payloadsz,
  size_t *submsg_len)
{
  bool ok = true;

  assert(payloadp);
  assert(payloadsz);
  assert(*payloadsz);
  assert(submsg_len);
  assert(sampleinfo);

  if (sampleinfo->pwr == NULL)
  {
    /* No specified proxy writer means no encoding. */
    return true;
  }

  /* Only decode when the attributes tell us so. */
  if ((sampleinfo->pwr->c.security_info.security_attributes & NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_PAYLOAD_PROTECTED)
                                                           == NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_PAYLOAD_PROTECTED)
  {
    unsigned char *dst_buf = NULL;
    unsigned int   dst_len = 0;

    /* Decrypt the payload. */
    if (q_omg_security_decode_serialized_payload(sampleinfo->pwr, payloadp, *payloadsz, &dst_buf, &dst_len))
    {
      /* Expect result to always fit into the original buffer. */
      assert(*payloadsz >= dst_len);

      /* Reduce submessage and payload lengths. */
      *submsg_len -= (*payloadsz - dst_len);
      *payloadsz   = dst_len;

      /* Replace the encrypted payload with the decrypted. */
      memcpy(payloadp, dst_buf, dst_len);
      ddsrt_free(dst_buf);
    }
    else
    {
      GVWARNING("decode_payload: failed to decrypt data from "PGUIDFMT"", PGUID (sampleinfo->pwr->e.guid));
      ok = false;
    }
  }

  return ok;
}

bool
decode_Data(
  const struct q_globals *gv,
  struct nn_rsample_info *sampleinfo,
  unsigned char *payloadp,
  uint32_t payloadsz,
  size_t *submsg_len)
{
  int ok = true;
  /* Only decode when there's actual data. */
  if (payloadp && (payloadsz > 0))
  {
    ok = decode_payload(gv, sampleinfo, payloadp, &payloadsz, submsg_len);
    if (ok)
    {
      /* It's possible that the payload size (and thus the sample size) has been reduced. */
      sampleinfo->size = payloadsz;
    }
  }
  return ok;
}

bool
decode_DataFrag(
  const struct q_globals *gv,
  struct nn_rsample_info *sampleinfo,
  unsigned char *payloadp,
  uint32_t payloadsz,
  size_t *submsg_len)
{
  int ok = true;
  /* Only decode when there's actual data. */
  if (payloadp && (payloadsz > 0))
  {
    ok = decode_payload(gv, sampleinfo, payloadp, &payloadsz, submsg_len);
    /* Do not touch the sampleinfo->size in contradiction to decode_Data() (it has been calculated differently). */
  }
  return ok;
}


void
encode_datareader_submsg(
  struct nn_xmsg *msg,
  struct nn_xmsg_marker sm_marker,
  struct proxy_writer *pwr,
  const struct ddsi_guid *rd_guid)
{
  /* Only encode when needed. */
  if (q_omg_security_enabled())
  {
    struct reader *rd = entidx_lookup_reader_guid(pwr->e.gv->entity_index, rd_guid);
    if (rd)
    {
      if (q_omg_reader_is_submessage_protected(rd))
      {
        unsigned char *src_buf;
        unsigned int   src_len;
        unsigned char *dst_buf;
        unsigned int   dst_len;

        /* Make one blob of the current sub-message by appending the serialized payload. */
        nn_xmsg_submsg_append_refd_payload(msg, sm_marker);

        /* Get the sub-message buffer. */
        src_buf = (unsigned char*)nn_xmsg_submsg_from_marker(msg, sm_marker);
        src_len = (unsigned int)nn_xmsg_submsg_size(msg, sm_marker);

        /* Do the actual encryption. */
        if (q_omg_security_encode_datareader_submessage(rd, &(pwr->e.guid.prefix), src_buf, src_len, &dst_buf, &dst_len))
        {
          /* Replace the old sub-message with the new encoded one(s). */
          nn_xmsg_submsg_replace(msg, sm_marker, dst_buf, dst_len);
          ddsrt_free(dst_buf);
        }
        else
        {
          /* The sub-message should have been encoded, which failed.
           * Remove it to prevent it from being send. */
          nn_xmsg_submsg_remove(msg, sm_marker);
        }
      }
    }
  }
}

void
encode_datawriter_submsg(
  struct nn_xmsg *msg,
  struct nn_xmsg_marker sm_marker,
  struct writer *wr)
{
  /* Only encode when needed. */
  if (q_omg_security_enabled())
  {
    if (q_omg_writer_is_submessage_protected(wr))
    {
      unsigned char *src_buf;
      unsigned int   src_len;
      unsigned char *dst_buf;
      unsigned int   dst_len;
      ddsi_guid_prefix_t dst_guid_prefix;
      ddsi_guid_prefix_t *dst = NULL;

      /* Make one blob of the current sub-message by appending the serialized payload. */
      nn_xmsg_submsg_append_refd_payload(msg, sm_marker);

      /* Get the sub-message buffer. */
      src_buf = (unsigned char*)nn_xmsg_submsg_from_marker(msg, sm_marker);
      src_len = (unsigned int)nn_xmsg_submsg_size(msg, sm_marker);

      if (nn_xmsg_getdst1prefix(msg, &dst_guid_prefix))
      {
        dst = &dst_guid_prefix;
      }

      /* Do the actual encryption. */
      if (q_omg_security_encode_datawriter_submessage(wr, dst, src_buf, src_len, &dst_buf, &dst_len))
      {
        /* Replace the old sub-message with the new encoded one(s). */
        nn_xmsg_submsg_replace(msg, sm_marker, dst_buf, dst_len);
        ddsrt_free(dst_buf);
      }
      else
      {
        /* The sub-message should have been encoded, which failed.
         * Remove it to prevent it from being send. */
        nn_xmsg_submsg_remove(msg, sm_marker);
      }
    }
  }
}

bool
validate_msg_decoding(
  const struct entity_common *e,
  const struct proxy_endpoint_common *c,
  struct proxy_participant *proxypp,
  struct receiver_state *rst,
  SubmessageKind_t prev_smid)
{
  assert(e);
  assert(c);
  assert(proxypp);
  assert(rst);

  /* If this endpoint is expected to have submessages protected, it means that the
   * previous submessage id (prev_smid) has to be SMID_SEC_PREFIX. That caused the
   * protected submessage to be copied into the current RTPS message as a clear
   * submessage, which we are currently handling.
   * However, we have to check if the prev_smid is actually SMID_SEC_PREFIX, otherwise
   * a rascal can inject data as just a clear submessage. */
  if ((c->security_info.security_attributes & NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED)
                                           == NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED)
  {
    if (prev_smid != SMID_SEC_PREFIX)
    {
      return false;
    }
  }

  /* At this point, we should also check if the complete RTPS message was encoded when
   * that is expected. */
  if (q_omg_security_is_remote_rtps_protected(proxypp, e->guid.entityid) && !rst->rtps_encoded)
  {
    return false;
  }

  return true;
}

static int
validate_submsg(struct q_globals *gv, unsigned char smid, unsigned char *submsg, unsigned char * const end, int byteswap)
{
  int result = -1;
  if ((submsg + RTPS_SUBMESSAGE_HEADER_SIZE) <= end)
  {
    SubmessageHeader_t *hdr = (SubmessageHeader_t*)submsg;
    if ((smid == 0 /* don't care */) || (hdr->submessageId == smid))
    {
      unsigned short size = hdr->octetsToNextHeader;
      if (byteswap)
      {
         size = ddsrt_bswap2u(size);
      }
      result = (int)size + (int)RTPS_SUBMESSAGE_HEADER_SIZE;
      if ((submsg + result) > end)
      {
        result = -1;
      }
    }
    else
    {
      GVWARNING("Unexpected submsg 0x%02x (0x%02x expected)", hdr->submessageId, smid);
    }
  }
  else
  {
    GVWARNING("Submsg 0x%02x does not fit message", smid);
  }
  return result;
}


static int
padding_submsg(struct q_globals *gv, unsigned char *start, unsigned char *end, int byteswap)
{
  SubmessageHeader_t *padding = (SubmessageHeader_t*)start;
  size_t size = (size_t)(end - start);
  int result = -1;

  assert(start <= end);

  if (size > sizeof(SubmessageHeader_t))
  {
    result = (int)size;
    padding->submessageId = SMID_PAD;
    padding->flags = (byteswap ? !(DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN) : (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN));
    padding->octetsToNextHeader = (unsigned short)(size - sizeof(SubmessageHeader_t));
    if (byteswap)
    {
      padding->octetsToNextHeader = ddsrt_bswap2u(padding->octetsToNextHeader);
    }
  }
  else
  {
    GVWARNING("Padding submessage doesn't fit");
  }
  return result;
}

int
decode_SecPrefix(
  struct receiver_state *rst,
  unsigned char *submsg,
  size_t submsg_size,
  unsigned char * const msg_end,
  const ddsi_guid_prefix_t * const src_prefix,
  const ddsi_guid_prefix_t * const dst_prefix,
  int byteswap)
{
  int result = -1;
  int totalsize = (int)submsg_size;
  unsigned char *body_submsg;
  unsigned char *prefix_submsg;
  unsigned char *postfix_submsg;
  SubmessageHeader_t *hdr = (SubmessageHeader_t*)submsg;
  uint8_t flags = hdr->flags;

  if (byteswap)
  {
    if ((DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN))
      hdr->flags |= 0x01;
    else
      hdr->flags &= 0xFE;
  }

  /* First sub-message is the SEC_PREFIX. */
  prefix_submsg = submsg;

  /* Next sub-message is SEC_BODY when encrypted or the original submessage when only signed. */
  body_submsg = submsg + submsg_size;
  result = validate_submsg(rst->gv, 0 /* don't care smid */, body_submsg, msg_end, byteswap);
  if (result > 0)
  {
    totalsize += result;

    /* Third sub-message should be the SEC_POSTFIX. */
    postfix_submsg = submsg + totalsize;
    result = validate_submsg(rst->gv, SMID_SEC_POSTFIX, postfix_submsg, msg_end, byteswap);
    if (result > 0)
    {
      bool decoded;
      unsigned char *dst_buf;
      unsigned int   dst_len;

      totalsize += result;

      /* Decode all three submessages. */
      decoded = q_omg_security_decode_submessage(src_prefix, dst_prefix, submsg, (unsigned int)totalsize, &dst_buf, &dst_len);
      if (decoded && dst_buf)
      {
        /*
         * The 'normal' submessage sequence handling will continue after the
         * given security SEC_PREFIX.
         */
        if (*body_submsg == SMID_SEC_BODY)
        {
          /*
           * Copy the decoded buffer into the original message, replacing (part
           * of) SEC_BODY.
           *
           * By replacing the SEC_BODY with the decoded submessage, everything
           * can continue as if there was never an encoded submessage.
           */
          assert((int)dst_len <= ((int)totalsize - (int)submsg_size));
          memcpy(body_submsg, dst_buf, dst_len);

          /* Remainder of SEC_BODY & SEC_POSTFIX should be padded to keep the submsg sequence going. */
          result = padding_submsg(rst->gv, body_submsg + dst_len, prefix_submsg + totalsize, byteswap);
        }
        else
        {
          /*
           * When only signed, then the submessage is already available and
           * SMID_SEC_POSTFIX will be ignored.
           * So, we don't really have to do anything.
           */
        }
        ddsrt_free(dst_buf);
      }
      else
      {
        /*
         * Decoding or signing failed.
         *
         * Replace the security submessages with padding. This also removes a plain
         * submessage when a signature check failed.
         */
        result = padding_submsg(rst->gv, body_submsg, prefix_submsg + totalsize, byteswap);
      }
    }
  }
  /* Restore flags. */
  hdr->flags = flags;
  return result;
}

static nn_rtps_msg_state_t
check_rtps_message_is_secure(
    struct q_globals *gv,
    Header_t *hdr,
    unsigned char *buff,
    bool isstream,
    struct proxy_participant **proxypp)
{
  nn_rtps_msg_state_t ret = NN_RTPS_MSG_STATE_ERROR;

  SubmessageHeader_t *submsg;
  uint32_t offset = RTPS_MESSAGE_HEADER_SIZE + (isstream ? sizeof(MsgLen_t) : 0);

  submsg = (SubmessageHeader_t *)(buff + offset);
  if (submsg->submessageId == SMID_SRTPS_PREFIX)
  {
    ddsi_guid_t guid;

    guid.prefix = hdr->guid_prefix;
    guid.entityid.u = NN_ENTITYID_PARTICIPANT;

    GVTRACE(" from "PGUIDFMT, PGUID(guid));

    *proxypp = entidx_lookup_proxy_participant_guid(gv->entity_index, &guid);
    if (*proxypp)
    {
      if (q_omg_proxyparticipant_is_authenticated(*proxypp))
      {
        ret = NN_RTPS_MSG_STATE_ENCODED;
      }
      else
      {
        GVTRACE ("received encoded rtps message from unauthenticated participant");
      }
    }
    else
    {
      GVTRACE ("received encoded rtps message from unknown participant");
    }
    GVTRACE("\n");
  }
  else
  {
    ret = NN_RTPS_MSG_STATE_PLAIN;
  }

  return ret;
}

nn_rtps_msg_state_t
decode_rtps_message(
  struct thread_state1 * const ts1,
  struct q_globals *gv,
  struct nn_rmsg **rmsg,
  Header_t **hdr,
  unsigned char **buff,
  ssize_t *sz,
  struct nn_rbufpool *rbpool,
  bool isstream)
{
  nn_rtps_msg_state_t ret = NN_RTPS_MSG_STATE_ERROR;
  struct proxy_participant *proxypp = NULL;
  unsigned char *dstbuf;
  unsigned char *srcbuf;
  uint32_t srclen, dstlen;
  bool decoded;

  /* Currently the decode_rtps_message returns a new allocated buffer.
   * This could be optimized by providing a pre-allocated nn_rmsg buffer to
   * copy the decoded rtps message in.
   */
  thread_state_awake_fixed_domain (ts1);
  ret = check_rtps_message_is_secure(gv, *hdr, *buff, isstream, &proxypp);
  if (ret == NN_RTPS_MSG_STATE_ENCODED)
  {
    if (isstream)
    {
      /* Remove MsgLen Submessage which was only needed for a stream to determine the end of the message */
      srcbuf = *buff + sizeof(MsgLen_t);
      srclen = (uint32_t)((size_t)(*sz) - sizeof(MsgLen_t));
      memmove(srcbuf, *buff, RTPS_MESSAGE_HEADER_SIZE);
    }
    else
    {
      srcbuf = *buff;
      srclen = (uint32_t)*sz;
    }

    decoded = q_omg_security_decode_rtps_message(proxypp, srcbuf, srclen, &dstbuf, &dstlen);
    if (decoded)
    {
      nn_rmsg_commit (*rmsg);
      *rmsg = nn_rmsg_new (rbpool);

      *buff = (unsigned char *) NN_RMSG_PAYLOAD (*rmsg);

      memcpy(*buff, dstbuf, dstlen);
      nn_rmsg_setsize (*rmsg, dstlen);

      ddsrt_free(dstbuf);

      *hdr = (Header_t*) *buff;
      (*hdr)->guid_prefix = nn_ntoh_guid_prefix ((*hdr)->guid_prefix);
      *sz = (ssize_t)dstlen;
    } else {
      ret = NN_RTPS_MSG_STATE_ERROR;
    }
  }
  thread_state_asleep (ts1);
  return ret;
}

ssize_t
secure_conn_write(
    ddsi_tran_conn_t conn,
    const nn_locator_t *dst,
    size_t niov,
    const ddsrt_iovec_t *iov,
    uint32_t flags,
    MsgLen_t *msg_len,
    bool dst_one,
    nn_msg_sec_info_t *sec_info,
    ddsi_tran_write_fn_t conn_write_cb)
{
  ssize_t ret = -1;

  unsigned i;
  Header_t *hdr;
  ddsi_guid_t guid;
  unsigned char stbuf[2048];
  unsigned char *srcbuf;
  unsigned char *dstbuf = NULL;
  uint32_t srclen, dstlen;
  int64_t dst_handle = 0;

  assert(iov);
  assert(conn);
  assert(msg_len);
  assert(sec_info);
  assert(niov > 0);
  assert(conn_write_cb);

  if (dst_one)
  {
    dst_handle = sec_info->dst_pp_handle;
    if (dst_handle == 0) {
      return -1;
    }
  }

  hdr = (Header_t *)iov[0].iov_base;
  guid.prefix = nn_ntoh_guid_prefix(hdr->guid_prefix);
  guid.entityid.u = NN_ENTITYID_PARTICIPANT;

  /* first determine the size of the message, then select the
   *  on-stack buffer or allocate one on the heap ...
   */
  srclen = 0;
  for (i = 0; i < (unsigned)niov; i++)
  {
    /* Do not copy MsgLen submessage in case of a stream connection */
    if ((i != 1) || !conn->m_stream)
      srclen += (uint32_t) iov[i].iov_len;
  }
  if (srclen <= sizeof (stbuf))
  {
    srcbuf = stbuf;
  }
  else
  {
    srcbuf = ddsrt_malloc (srclen);
  }

  /* ... then copy data into buffer */
  srclen = 0;
  for (i = 0; i < (unsigned)niov; i++)
  {
    if ((i != 1) || !conn->m_stream)
    {
      memcpy(srcbuf + srclen, iov[i].iov_base, iov[i].iov_len);
      srclen += (uint32_t) iov[i].iov_len;
    }
  }

  if (q_omg_security_encode_rtps_message(sec_info->src_pp_handle, &guid, srcbuf, srclen, &dstbuf, &dstlen, dst_handle))
  {
    ddsrt_iovec_t tmp_iov[3];
    size_t tmp_niov;

    if (conn->m_stream)
    {
      /* Add MsgLen submessage after Header */
      msg_len->length = dstlen + (uint32_t)sizeof(*msg_len);

      tmp_iov[0].iov_base = dstbuf;
      tmp_iov[0].iov_len = RTPS_MESSAGE_HEADER_SIZE;
      tmp_iov[1].iov_base = (void*) msg_len;
      tmp_iov[1].iov_len = sizeof (*msg_len);
      tmp_iov[2].iov_base = dstbuf + RTPS_MESSAGE_HEADER_SIZE;
      tmp_iov[2].iov_len = dstlen - RTPS_MESSAGE_HEADER_SIZE;
      tmp_niov = 3;
    }
    else
    {
      msg_len->length = dstlen;

      tmp_iov[0].iov_base = dstbuf;
      tmp_iov[0].iov_len = dstlen;
      tmp_niov = 1;
    }
    ret = conn_write_cb (conn, dst, tmp_niov, tmp_iov, flags);
  }

  if (srcbuf != stbuf)
  {
    ddsrt_free (srcbuf);
  }

  ddsrt_free(dstbuf);

  return ret;
}

#else /* DDSI_INCLUDE_SECURITY */

#include "dds/ddsi/ddsi_security_omg.h"

extern inline bool q_omg_participant_is_secure(UNUSED_ARG(const struct participant *pp));
extern inline bool q_omg_proxy_participant_is_secure(UNUSED_ARG(const struct proxy_participant *proxypp));
extern inline bool q_omg_security_enabled(void);

extern inline unsigned determine_subscription_writer(UNUSED_ARG(const struct reader *rd));

extern inline bool q_omg_security_match_remote_writer_enabled(UNUSED_ARG(struct reader *rd), UNUSED_ARG(struct proxy_writer *pwr));
extern inline bool q_omg_security_match_remote_reader_enabled(UNUSED_ARG(struct writer *wr), UNUSED_ARG(struct proxy_reader *prd));

extern inline void q_omg_get_proxy_writer_security_info(UNUSED_ARG(struct proxy_writer *pwr), UNUSED_ARG(const nn_plist_t *plist), UNUSED_ARG(nn_security_info_t *info));
extern inline bool q_omg_security_check_remote_writer_permissions(UNUSED_ARG(const struct proxy_writer *pwr), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(struct participant *pp));
extern inline void q_omg_security_deregister_remote_writer_match(UNUSED_ARG(const struct proxy_writer *pwr), UNUSED_ARG(const struct reader *rd), UNUSED_ARG(struct rd_pwr_match *match));
extern inline void q_omg_get_proxy_reader_security_info(UNUSED_ARG(struct proxy_reader *prd), UNUSED_ARG(const nn_plist_t *plist), UNUSED_ARG(nn_security_info_t *info));
extern inline bool q_omg_security_check_remote_reader_permissions(UNUSED_ARG(const struct proxy_reader *prd), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(struct participant *par));
extern inline void q_omg_security_deregister_remote_reader_match(UNUSED_ARG(const struct proxy_reader *prd), UNUSED_ARG(const struct writer *wr), UNUSED_ARG(struct wr_prd_match *match));

extern inline unsigned determine_publication_writer(
  UNUSED_ARG(const struct writer *wr));

extern inline bool is_proxy_participant_deletion_allowed(
  UNUSED_ARG(struct q_globals * const gv),
  UNUSED_ARG(const struct ddsi_guid *guid),
  UNUSED_ARG(const ddsi_entityid_t pwr_entityid));

extern inline bool q_omg_is_similar_participant_security_info(UNUSED_ARG(struct participant *pp), UNUSED_ARG(struct proxy_participant *proxypp));

extern inline bool q_omg_participant_allow_unauthenticated(UNUSED_ARG(struct participant *pp));

extern inline bool q_omg_security_check_create_participant(UNUSED_ARG(struct participant *pp), UNUSED_ARG(uint32_t domain_id));

extern inline void q_omg_security_deregister_participant(UNUSED_ARG(struct participant *pp));

extern inline bool q_omg_security_check_create_topic(UNUSED_ARG(const struct q_globals *gv), UNUSED_ARG(const ddsi_guid_t *pp_guid), UNUSED_ARG(const char *topic_name), UNUSED_ARG(const struct dds_qos *qos));

extern inline int64_t q_omg_security_get_local_participant_handle(UNUSED_ARG(struct participant *pp));

extern inline bool q_omg_security_check_create_writer(UNUSED_ARG(struct participant *pp), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(const char *topic_name), UNUSED_ARG(const struct dds_qos *writer_qos));

extern inline void q_omg_security_register_writer(UNUSED_ARG(struct writer *wr));

extern inline void q_omg_security_deregister_writer(UNUSED_ARG(struct writer *wr));

extern inline bool q_omg_security_check_create_reader(UNUSED_ARG(struct participant *pp), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(const char *topic_name), UNUSED_ARG(const struct dds_qos *reader_qos));

extern inline void q_omg_security_register_reader(UNUSED_ARG(struct reader *rd));

extern inline void q_omg_security_deregister_reader(UNUSED_ARG(struct reader *rd));

/* initialize the proxy participant security attributes */
extern inline void q_omg_security_init_remote_participant(UNUSED_ARG(struct proxy_participant *proxypp));

/* ask to access control security plugin for the remote participant permissions */
extern inline int64_t q_omg_security_check_remote_participant_permissions(UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(struct participant *pp), UNUSED_ARG(struct proxy_participant *proxypp));

extern inline void q_omg_security_register_remote_participant(UNUSED_ARG(struct participant *pp), UNUSED_ARG(struct proxy_participant *proxypp), UNUSED_ARG(int64_t shared_secret), UNUSED_ARG(int64_t proxy_permissions));

extern inline void q_omg_security_deregister_remote_participant(UNUSED_ARG(struct proxy_participant *proxypp));

extern inline void q_omg_security_participant_send_tokens(UNUSED_ARG(struct participant *pp), UNUSED_ARG(struct proxy_participant *proxypp));

extern inline void set_proxy_participant_security_info(
  UNUSED_ARG(struct proxy_participant *prd),
  UNUSED_ARG(const nn_plist_t *plist));

extern inline void set_proxy_reader_security_info(
  UNUSED_ARG(struct proxy_reader *prd),
  UNUSED_ARG(const nn_plist_t *plist));

extern inline void set_proxy_writer_security_info(
  UNUSED_ARG(struct proxy_writer *pwr),
  UNUSED_ARG(const nn_plist_t *plist));

extern inline bool decode_Data(
  UNUSED_ARG(const struct q_globals *gv),
  UNUSED_ARG(struct nn_rsample_info *sampleinfo),
  UNUSED_ARG(unsigned char *payloadp),
  UNUSED_ARG(uint32_t payloadsz),
  UNUSED_ARG(size_t *submsg_len));

extern inline bool decode_DataFrag(
  UNUSED_ARG(const struct q_globals *gv),
  UNUSED_ARG(struct nn_rsample_info *sampleinfo),
  UNUSED_ARG(unsigned char *payloadp),
  UNUSED_ARG(uint32_t payloadsz),
  UNUSED_ARG(size_t *submsg_len));

extern inline void encode_datareader_submsg(
  UNUSED_ARG(struct nn_xmsg *msg),
  UNUSED_ARG(struct nn_xmsg_marker sm_marker),
  UNUSED_ARG(struct proxy_writer *pwr),
  UNUSED_ARG(const struct ddsi_guid *rd_guid));

extern inline void encode_datawriter_submsg(
  UNUSED_ARG(struct nn_xmsg *msg),
  UNUSED_ARG(struct nn_xmsg_marker sm_marker),
  UNUSED_ARG(struct writer *wr));

extern inline bool validate_msg_decoding(
  UNUSED_ARG(const struct entity_common *e),
  UNUSED_ARG(const struct proxy_endpoint_common *c),
  UNUSED_ARG(struct proxy_participant *proxypp),
  UNUSED_ARG(struct receiver_state *rst),
  UNUSED_ARG(SubmessageKind_t prev_smid));

extern inline int decode_SecPrefix(
  UNUSED_ARG(struct receiver_state *rst),
  UNUSED_ARG(unsigned char *submsg),
  UNUSED_ARG(size_t submsg_size),
  UNUSED_ARG(unsigned char * const msg_end),
  UNUSED_ARG(const ddsi_guid_prefix_t * const src_prefix),
  UNUSED_ARG(const ddsi_guid_prefix_t * const dst_prefix),
  UNUSED_ARG(int byteswap));

extern inline nn_rtps_msg_state_t decode_rtps_message(
  UNUSED_ARG(struct thread_state1 * const ts1),
  UNUSED_ARG(struct q_globals *gv),
  UNUSED_ARG(struct nn_rmsg **rmsg),
  UNUSED_ARG(Header_t **hdr),
  UNUSED_ARG(unsigned char **buff),
  UNUSED_ARG(ssize_t *sz),
  UNUSED_ARG(struct nn_rbufpool *rbpool),
  UNUSED_ARG(bool isstream));

extern inline int64_t q_omg_security_get_remote_participant_handle(UNUSED_ARG(struct proxy_participant *proxypp));


#endif /* DDSI_INCLUDE_SECURITY */
