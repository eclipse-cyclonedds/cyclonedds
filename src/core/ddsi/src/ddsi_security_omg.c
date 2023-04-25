// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/features.h"

#ifdef DDS_HAS_SECURITY

#include <string.h>
#include <stdarg.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_log.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "ddsi__radmin.h"
#include "ddsi__misc.h"
#include "ddsi__entity_index.h"
#include "ddsi__security_msg.h"
#include "ddsi__security_omg.h"
#include "ddsi__security_util.h"
#include "ddsi__security_exchange.h"
#include "ddsi__handshake.h"
#include "ddsi__entity.h"
#include "ddsi__participant.h"
#include "ddsi__xevent.h"
#include "ddsi__sysdeps.h"
#include "ddsi__endpoint_match.h"
#include "ddsi__plist.h"
#include "ddsi__proxy_endpoint.h"
#include "ddsi__proxy_participant.h"
#include "ddsi__tran.h"
#include "ddsi__vendor.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/core/dds_security_plugins.h"

#define AUTH_NAME "Authentication"
#define AC_NAME "Access Control"
#define CRYPTO_NAME "Cryptographic"

/* TODO: This constant which determines the time pending matches are maintained
 * and not used should be made a configurable parameter,
 */
#define PENDING_MATCH_EXPIRY_TIME 300

#define EXCEPTION_LOG(gv,e,cat,...) \
  ddsi_omg_log_exception(&gv->logconfig, cat, e, __FILE__, __LINE__, DDS_FUNCTION, __VA_ARGS__)
#define EXCEPTION_VLOG(gv,e,cat,fmt,ap) \
  ddsi_omg_vlog_exception(&gv->logconfig, cat, e, __FILE__, __LINE__, DDS_FUNCTION, fmt, ap)

#define EXCEPTION_ERROR(gv,e,...)     EXCEPTION_LOG(gv, e, DDS_LC_ERROR, __VA_ARGS__)
#define EXCEPTION_WARNING(gv,e,...)   EXCEPTION_LOG(gv, e, DDS_LC_WARNING, __VA_ARGS__)


#define SECURITY_ATTR_IS_VALID(attr)                                      \
    ((attr) & DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID)

/* Security attributes are compatible ... */
#define SECURITY_ATTR_COMPATIBLE(attr_a, attr_b, is_valid_flag)           \
(                                                                         \
    /* ... if masks are equal ... */                                      \
    (attr_a == attr_b)                                                    \
    ||                                                                    \
    /* ... or if either of the masks is not valid ... */                  \
    (((attr_a & is_valid_flag) == 0) || ((attr_b & is_valid_flag) == 0))  \
)

/* Security information are compatible ... */
#define SECURITY_INFO_COMPATIBLE(info_a, info_b, is_valid_flag)           \
(                                                                         \
    /* ... if plugin attributes are compatible ... */                     \
    SECURITY_ATTR_COMPATIBLE(info_a.plugin_security_attributes,           \
                             info_b.plugin_security_attributes,           \
                             is_valid_flag)                               \
    &&                                                                    \
    /* ... and spec attributes are compatible ... */                      \
    SECURITY_ATTR_COMPATIBLE(info_a.security_attributes,                  \
                             info_b.security_attributes,                  \
                             is_valid_flag)                               \
)

/* Security information indicates clear data ... */
#define SECURITY_INFO_CLEAR(info, is_valid_flag)                          \
(                                                                         \
    /* ... if no flag was set (ignoring the is_valid flag) ... */         \
    (info.security_attributes & (~is_valid_flag)) == 0                    \
)

#define SECURITY_INFO_IS_RTPS_PROTECTED(info)                                                 \
(                                                                                             \
    (info.security_attributes & DDSI_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_VALID         ) && \
    (info.security_attributes & DDSI_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_RTPS_PROTECTED)    \
)

#define SECURITY_INFO_IS_WRITE_PROTECTED(info)                                              \
(                                                                                           \
    (info.security_attributes & DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID          ) && \
    (info.security_attributes & DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_WRITE_PROTECTED)    \
)

#define SECURITY_INFO_IS_READ_PROTECTED(info)                                               \
(                                                                                           \
    (info.security_attributes & DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID          ) && \
    (info.security_attributes & DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_READ_PROTECTED )    \
)

#define SECURITY_INFO_IS_RTPS_PROTECTED(info)                                                 \
(                                                                                             \
    (info.security_attributes & DDSI_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_VALID         ) && \
    (info.security_attributes & DDSI_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_RTPS_PROTECTED)    \
)

#define SECURITY_INFO_USE_RTPS_AUTHENTICATION(info) \
    ((info).plugin_participant_attributes & DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_AUTHENTICATED)

static bool endpoint_is_DCPSParticipantSecure (const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER) ||
          (guid->entityid.u == DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER));
}

static bool endpoint_is_DCPSPublicationsSecure (const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER) ||
          (guid->entityid.u == DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER));
}

static bool endpoint_is_DCPSSubscriptionsSecure (const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER) ||
          (guid->entityid.u == DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER));
}

static bool endpoint_is_DCPSParticipantStatelessMessage (const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER) ||
          (guid->entityid.u == DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER));
}

static bool endpoint_is_DCPSParticipantMessageSecure (const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER) ||
          (guid->entityid.u == DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER));
}

static bool endpoint_is_DCPSParticipantVolatileMessageSecure (const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER) ||
          (guid->entityid.u == DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER));
}

struct participant_sec_index {
  ddsrt_mutex_t lock;
  ddsrt_avl_ctree_t participants;
};

/* The pending _match_index uses an avl tree to store pending_match's where the
 * guid_pair is used as the key. The remote_guid is the primary key and
 * the local_guid is the secondary key. The use of the remote_guid as the primary key
 * is used in the function clear_pending_matches_by_remote_guid to clear the
 * pending matches associated with a remote entity.
 *
 * The table containing the pending matches is protected by the pending_match_index:lock.
 * It is allowed to access the fields (crypto_handle and tokens) of a pending_match outside
 * the pending_match_index:lock provided that the pending_match is protected by the
 * lock of the entity corresponding to the local_guid.
 * A pending_match is either created when registering and matching an remote entity and
 * the corresponding crypto tokens are not available or when the crypto tokens associated
 * with a remote entity are received but it has not yet been discovered.
 */
struct guid_pair {
  ddsi_guid_t remote_guid;
  ddsi_guid_t local_guid;
};

struct pending_match {
  ddsrt_avl_node_t avlnode;
  ddsrt_fibheap_node_t heapnode;
  struct guid_pair guids;
  enum ddsi_entity_kind kind;
  int64_t crypto_handle;
  DDS_Security_ParticipantCryptoTokenSeq *tokens;
  ddsrt_mtime_t expiry;
};

struct pending_match_index {
  ddsrt_mutex_t lock;
  const struct ddsi_domaingv *gv;
  ddsrt_avl_tree_t pending_matches;
  ddsrt_fibheap_t expiry_timers;
  struct ddsi_xevent *evt;
};

struct dds_security_context {
  dds_security_plugin auth_plugin;
  dds_security_plugin ac_plugin;
  dds_security_plugin crypto_plugin;

  dds_security_authentication *authentication_context;
  dds_security_cryptography *crypto_context;
  dds_security_access_control *access_control_context;
  ddsrt_mutex_t omg_security_lock;
  uint32_t next_plugin_id;

  struct pending_match_index security_matches;
  struct participant_sec_index partiticpant_index;
  struct dds_security_access_control_listener ac_listener;
  struct dds_security_authentication_listener auth_listener;
};

typedef struct dds_security_context dds_security_context;

static int compare_crypto_handle (const void *va, const void *vb);
static int compare_guid_pair(const void *va, const void *vb);
static int compare_pending_match_exptime (const void *va, const void *vb);


const ddsrt_avl_ctreedef_t pp_proxypp_treedef =
    DDSRT_AVL_CTREEDEF_INITIALIZER (offsetof (struct ddsi_pp_proxypp_match, avlnode), offsetof (struct ddsi_pp_proxypp_match, proxypp_guid), ddsi_compare_guid, 0);
const ddsrt_avl_treedef_t proxypp_pp_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct ddsi_proxypp_pp_match, avlnode), offsetof (struct ddsi_proxypp_pp_match, pp_crypto_handle), compare_crypto_handle, 0);
const ddsrt_avl_ctreedef_t participant_index_treedef =
    DDSRT_AVL_CTREEDEF_INITIALIZER (offsetof (struct ddsi_participant_sec_attributes, avlnode), offsetof (struct ddsi_participant_sec_attributes, crypto_handle), compare_crypto_handle, 0);
const ddsrt_avl_treedef_t pending_match_index_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct pending_match, avlnode), offsetof (struct pending_match, guids), compare_guid_pair, 0);

const ddsrt_fibheap_def_t pending_match_expiry_fhdef = DDSRT_FIBHEAPDEF_INITIALIZER(offsetof (struct pending_match, heapnode), compare_pending_match_exptime);


static int compare_crypto_handle (const void *va, const void *vb)
{
  const DDS_Security_ParticipantCryptoHandle *ha = va;
  const DDS_Security_ParticipantCryptoHandle *hb = vb;

  return ((*ha > *hb) ? 1 : (*ha < *hb) ?  -1 : 0);
}

static int compare_guid_pair(const void *va, const void *vb)
{
  const struct guid_pair *gpa = va;
  const struct guid_pair *gpb = vb;
  int r;

  if ((r = ddsi_compare_guid(&gpa->remote_guid, &gpb->remote_guid)) == 0)
    r = ddsi_compare_guid(&gpa->local_guid, &gpb->local_guid);
  return r;
}

static int compare_pending_match_exptime (const void *va, const void *vb)
{
  const struct pending_match *ma = va;
  const struct pending_match *mb = vb;
  return (ma->expiry.v == mb->expiry.v) ? 0 : (ma->expiry.v < mb->expiry.v) ? -1 : 1;
}

static struct dds_security_context * ddsi_omg_security_get_secure_context (const struct ddsi_participant *pp)
{
  if (pp && pp->e.gv->security_context && ddsi_omg_is_security_loaded (pp->e.gv->security_context))
    return pp->e.gv->security_context;
  return NULL;
}

struct dds_security_access_control *ddsi_omg_participant_get_access_control(const struct ddsi_participant *pp)
{
  if (pp && pp->e.gv->security_context && ddsi_omg_is_security_loaded (pp->e.gv->security_context))
    return pp->e.gv->security_context->access_control_context;
  return NULL;
}

struct dds_security_authentication *ddsi_omg_participant_get_authentication(const struct ddsi_participant *pp)
{
  if (pp && pp->e.gv->security_context && ddsi_omg_is_security_loaded (pp->e.gv->security_context))
    return pp->e.gv->security_context->authentication_context;
  return NULL;
}

struct dds_security_cryptography *ddsi_omg_participant_get_cryptography(const struct ddsi_participant *pp)
{
  if (pp && pp->e.gv->security_context && ddsi_omg_is_security_loaded (pp->e.gv->security_context))
    return pp->e.gv->security_context->crypto_context;
  return NULL;
}

static struct dds_security_context * ddsi_omg_security_get_secure_context_from_proxypp (const struct ddsi_proxy_participant *proxypp)
{
  if (proxypp && proxypp->e.gv->security_context && ddsi_omg_is_security_loaded (proxypp->e.gv->security_context))
    return proxypp->e.gv->security_context;
  return NULL;
}

void ddsi_omg_vlog_exception(const struct ddsrt_log_cfg *lc, uint32_t cat, DDS_Security_SecurityException *exception, const char *file, uint32_t line, const char *func, const char *fmt, va_list ap)
{
  char logbuffer[512];
  int l;

  l = vsnprintf(logbuffer, sizeof(logbuffer), fmt, ap);
  if ((size_t) l >= sizeof(logbuffer))
  {
    logbuffer[sizeof(logbuffer)-1] = '\0';
  }
  dds_log_cfg(lc, cat, file, line, func, "%s: %s(code: %d)\n", logbuffer, exception->message ? exception->message : "",  exception->code);
  DDS_Security_Exception_reset(exception);
}

void ddsi_omg_log_exception(const struct ddsrt_log_cfg *lc, uint32_t cat, DDS_Security_SecurityException *exception, const char *file, uint32_t line, const char *func, const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  ddsi_omg_vlog_exception(lc, cat, exception, file, line, func, fmt, ap);
  va_end (ap);
}

static void free_pending_match(struct pending_match *match)
{
  if (match)
  {
    DDS_Security_ParticipantCryptoTokenSeq_free(match->tokens);
    ddsrt_free(match);
  }
}

static struct pending_match * find_or_create_pending_entity_match(struct pending_match_index *index, enum ddsi_entity_kind kind, const ddsi_guid_t *remote_guid, const ddsi_guid_t *local_guid, int64_t crypto_handle, DDS_Security_ParticipantCryptoTokenSeq *tokens)
{
  struct guid_pair guids = { .remote_guid = *remote_guid, .local_guid = *local_guid};
  struct pending_match *match;
  ddsrt_avl_ipath_t ipath;

  ddsrt_mutex_lock(&index->lock);
  if ((match = ddsrt_avl_lookup_ipath(&pending_match_index_treedef, &index->pending_matches, &guids, &ipath)) == NULL)
  {
    match = ddsrt_malloc(sizeof(*match));
    match->crypto_handle = 0;
    match->tokens = NULL;
    match->guids = guids;
    match->kind = kind;
    match->expiry = DDSRT_MTIME_NEVER;
    ddsrt_avl_insert_ipath(&pending_match_index_treedef, &index->pending_matches, match, &ipath);
  }

  if (crypto_handle)
    match->crypto_handle = crypto_handle;

  if (tokens)
  {
    match->tokens = tokens;
    match->expiry = ddsrt_mtime_add_duration(ddsrt_time_monotonic(), DDS_SECS(PENDING_MATCH_EXPIRY_TIME));
    ddsrt_fibheap_insert(&pending_match_expiry_fhdef, &index->expiry_timers, match);
    (void)ddsi_resched_xevent_if_earlier(index->evt, match->expiry);
  }
  ddsrt_mutex_unlock(&index->lock);

  return match;
}

static void unregister_and_free_pending_match(const struct ddsi_domaingv * gv, dds_security_context *sc, struct pending_match *match)
{
  if (match->crypto_handle != 0)
  {
    DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
    const char *ename;
    bool r = true;

    assert(sc);
    switch (match->kind)
    {
    case DDSI_EK_PROXY_PARTICIPANT:
      break;
    case DDSI_EK_PROXY_READER:
      ename = "reader";
      r = sc->crypto_context->crypto_key_factory->unregister_datareader(sc->crypto_context->crypto_key_factory, match->crypto_handle, &exception);
      break;
    case DDSI_EK_PROXY_WRITER:
      ename = "writer";
      r = sc->crypto_context->crypto_key_factory->unregister_datawriter(sc->crypto_context->crypto_key_factory, match->crypto_handle, &exception);
      break;
    default:
      assert(0);
      break;
    }
    if (!r)
      EXCEPTION_ERROR(gv, &exception, "Failed to unregister remote %s crypto "PGUIDFMT" related to "PGUIDFMT, ename, PGUID(match->guids.remote_guid), PGUID(match->guids.local_guid));
  }
  free_pending_match(match);
}

static void delete_pending_match(struct pending_match_index *index, struct pending_match *match)
{
  ddsrt_mutex_lock(&index->lock);
  ddsrt_avl_delete(&pending_match_index_treedef, &index->pending_matches, match);
  if (match->expiry.v != DDS_NEVER)
    ddsrt_fibheap_delete(&pending_match_expiry_fhdef, &index->expiry_timers, match);
  free_pending_match(match);
  ddsrt_mutex_unlock(&index->lock);
}

struct pending_match_expiry_cb_arg {
  struct pending_match_index *index;
};

static void pending_match_expiry_cb(struct ddsi_domaingv *gv, struct ddsi_xevent *xev, struct ddsi_xpack *xp, void *varg, ddsrt_mtime_t tnow)
{
  struct pending_match_expiry_cb_arg * const arg = varg;
  struct pending_match_index *index = arg->index;
  (void) gv;
  (void) xp;

  ddsrt_mutex_lock(&index->lock);
  struct pending_match *match = ddsrt_fibheap_min(&pending_match_expiry_fhdef, &index->expiry_timers);
  while (match && match->expiry.v <= tnow.v)
  {
    ddsrt_fibheap_delete(&pending_match_expiry_fhdef, &index->expiry_timers, match);
    ddsrt_avl_delete(&pending_match_index_treedef, &index->pending_matches, match);
    unregister_and_free_pending_match(index->gv, index->gv->security_context, match);
    match = ddsrt_fibheap_min(&pending_match_expiry_fhdef, &index->expiry_timers);
  }
  if (match)
    ddsi_resched_xevent_if_earlier(xev, match->expiry);
  ddsrt_mutex_unlock(&index->lock);
}

static void clear_pending_matches_by_local_guid(dds_security_context *sc, struct pending_match_index *index, const ddsi_guid_t *local_guid)
{
  struct pending_match *match;

  ddsrt_mutex_lock(&index->lock);
  match = ddsrt_avl_find_min(&pending_match_index_treedef, &index->pending_matches);
  while (match)
  {
    struct pending_match *next = ddsrt_avl_find_succ(&pending_match_index_treedef, &index->pending_matches, match);
    if (ddsi_compare_guid(&match->guids.local_guid, local_guid) == 0)
    {
      ddsrt_avl_delete(&pending_match_index_treedef, &index->pending_matches, match);
      if (match->expiry.v != DDS_NEVER)
        ddsrt_fibheap_delete(&pending_match_expiry_fhdef, &index->expiry_timers, match);
      next = ddsrt_avl_lookup_succ(&pending_match_index_treedef, &index->pending_matches, &match->guids);
      unregister_and_free_pending_match(index->gv, sc, match);
    }
    match = next;
  }
  ddsrt_mutex_unlock(&index->lock);
}

static void clear_pending_matches_by_remote_guid(dds_security_context *sc, struct pending_match_index *index, const ddsi_guid_t *remote_guid)
{
  struct guid_pair template = { .remote_guid = *remote_guid, .local_guid = {.prefix.u = {0, 0, 0}, .entityid.u = 0} };
  struct pending_match *match;

  ddsrt_mutex_lock(&index->lock);
  match = ddsrt_avl_lookup_succ(&pending_match_index_treedef, &index->pending_matches, &template);
  while (match && ddsi_compare_guid(&match->guids.remote_guid, remote_guid) == 0)
  {
    struct pending_match *next = ddsrt_avl_lookup_succ(&pending_match_index_treedef, &index->pending_matches, &match->guids);
    ddsrt_avl_delete(&pending_match_index_treedef, &index->pending_matches, match);
    if (match->expiry.v != DDS_NEVER)
      ddsrt_fibheap_delete(&pending_match_expiry_fhdef, &index->expiry_timers, match);
    unregister_and_free_pending_match(index->gv, sc, match);
    match = next;
  }
  ddsrt_mutex_unlock(&index->lock);
}

static void pending_match_index_init(const struct ddsi_domaingv *gv, struct pending_match_index *index)
{
  ddsrt_mutex_init(&index->lock);
  ddsrt_avl_init(&pending_match_index_treedef, &index->pending_matches);
  ddsrt_fibheap_init(&pending_match_expiry_fhdef, &index->expiry_timers);
  index->gv = gv;
  struct pending_match_expiry_cb_arg arg = { .index = index };
  index->evt = ddsi_qxev_callback(gv->xevents, DDSRT_MTIME_NEVER, pending_match_expiry_cb, &arg, sizeof (arg), true);
}

static void pending_match_index_deinit(struct pending_match_index *index)
{
  ddsi_delete_xevent(index->evt);
  ddsrt_mutex_destroy(&index->lock);
  assert(ddsrt_avl_is_empty(&index->pending_matches));
  ddsrt_avl_free(&pending_match_index_treedef, &index->pending_matches, 0);
}

static struct ddsi_pp_proxypp_match * ddsi_pp_proxypp_match_new(struct ddsi_proxy_participant *proxypp, DDS_Security_ParticipantCryptoHandle proxypp_crypto_handle)
{
  struct ddsi_pp_proxypp_match *pm;

  pm = ddsrt_malloc(sizeof(*pm));
  pm->proxypp_guid = proxypp->e.guid;
  pm->proxypp_crypto_handle = proxypp_crypto_handle;

  return pm;
}

static void ddsi_pp_proxypp_match_free(struct dds_security_context *sc, struct ddsi_pp_proxypp_match *pm)
{
  DDSRT_UNUSED_ARG(sc);

  ddsrt_free(pm);
}

static struct ddsi_proxypp_pp_match * ddsi_proxypp_pp_match_new(struct ddsi_participant *pp, DDS_Security_PermissionsHandle permissions_hdl, DDS_Security_SharedSecretHandle shared_secret)
{
  struct ddsi_proxypp_pp_match *pm;

  pm = ddsrt_malloc(sizeof(*pm));
  pm->pp_guid = pp->e.guid;
  pm->pp_crypto_handle = pp->sec_attr->crypto_handle;
  pm->permissions_handle = permissions_hdl;
  pm->shared_secret = shared_secret;
  pm->authenticated = false;

  return pm;
}

static void ddsi_proxypp_pp_match_free(struct ddsi_domaingv *gv, struct dds_security_context *sc, struct ddsi_proxypp_pp_match *pm)
{
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;

  if (pm->permissions_handle != DDS_SECURITY_HANDLE_NIL)
  {
    if (!sc->access_control_context->return_permissions_handle(sc->access_control_context, pm->permissions_handle, &exception))
    {
      /* FIXME: enable exception warning when access control is updated to return a permission handle for each
       * matching local and remote participant.
       */
#if 0
      EXCEPTION_ERROR(gv, &exception, "Failed to return remote permissions handle");
#else
      DDSRT_UNUSED_ARG (gv);
      DDS_Security_Exception_reset(&exception);
#endif
    }
  }
  ddsrt_free(pm);
}

static void pp_proxypp_unrelate_locked(struct dds_security_context *sc, struct ddsi_participant *pp, const ddsi_guid_t *proxypp_guid)
{
  struct ddsi_pp_proxypp_match *pm;
  ddsrt_avl_dpath_t dpath;

  if ((pm = ddsrt_avl_clookup_dpath(&pp_proxypp_treedef, &pp->sec_attr->proxy_participants, proxypp_guid, &dpath)) != NULL)
  {
    ddsrt_avl_cdelete_dpath(&pp_proxypp_treedef, &pp->sec_attr->proxy_participants, pm, &dpath);
    ddsi_pp_proxypp_match_free(sc, pm);
  }
}

static void pp_proxypp_unrelate(struct dds_security_context *sc, struct ddsi_participant *pp, const ddsi_guid_t *proxypp_guid)
{
  ddsrt_mutex_lock(&pp->sec_attr->lock);
  pp_proxypp_unrelate_locked (sc, pp, proxypp_guid);
  ddsrt_mutex_unlock(&pp->sec_attr->lock);
}

static void proxypp_pp_unrelate_locked(struct dds_security_context *sc, struct ddsi_proxy_participant *proxypp, const ddsi_guid_t *pp_guid, int64_t pp_crypto_handle)
{
  DDSRT_UNUSED_ARG(pp_guid);
  struct ddsi_proxypp_pp_match *pm;
  ddsrt_avl_dpath_t dpath;

  if ((pm = ddsrt_avl_lookup_dpath(&proxypp_pp_treedef, &proxypp->sec_attr->participants, &pp_crypto_handle, &dpath)) != NULL)
  {
    ddsrt_avl_delete_dpath(&proxypp_pp_treedef, &proxypp->sec_attr->participants, pm, &dpath);
    ddsi_proxypp_pp_match_free(proxypp->e.gv, sc, pm);
  }
}

static void proxypp_pp_unrelate(struct dds_security_context *sc, struct ddsi_proxy_participant *proxypp, const ddsi_guid_t *pp_guid, int64_t pp_crypto_handle)
{
  if (proxypp->sec_attr)
  {
    ddsrt_mutex_lock(&proxypp->sec_attr->lock);
    proxypp_pp_unrelate_locked(sc, proxypp, pp_guid, pp_crypto_handle);
    ddsrt_mutex_unlock(&proxypp->sec_attr->lock);
  }
}

static struct ddsi_participant_sec_attributes * participant_sec_attributes_new(ddsi_guid_t *guid)
{
  struct ddsi_participant_sec_attributes *attr;

  attr = ddsrt_malloc(sizeof(*attr));
  ddsrt_mutex_init(&attr->lock);
  ddsrt_avl_cinit(&pp_proxypp_treedef, &attr->proxy_participants);
  attr->pp_guid = *guid;
  attr->crypto_handle = DDS_SECURITY_HANDLE_NIL;
  attr->plugin_attr = false;
  attr->initialized = false;
  return attr;
}

static void participant_sec_attributes_free(struct ddsi_participant_sec_attributes *attr)
{
  if (attr)
  {
    ddsrt_avl_cfree(&pp_proxypp_treedef, &attr->proxy_participants, 0);
    ddsrt_mutex_destroy(&attr->lock);
    ddsrt_free(attr);
  }
}

static struct ddsi_writer_sec_attributes * writer_sec_attributes_new(void)
{
  struct ddsi_writer_sec_attributes *attr;

  attr = ddsrt_malloc(sizeof(*attr));
  memset(attr, 0, sizeof(*attr));
  attr->crypto_handle = DDS_SECURITY_HANDLE_NIL;
  attr->plugin_attr = false;
  return attr;
}

static void writer_sec_attributes_free(struct ddsi_writer_sec_attributes *attr)
{
  ddsrt_free(attr);
}

static struct ddsi_reader_sec_attributes * reader_sec_attributes_new(void)
{
  struct ddsi_reader_sec_attributes *attr;

  attr = ddsrt_malloc(sizeof(*attr));
  memset(attr, 0, sizeof(*attr));
  attr->crypto_handle = DDS_SECURITY_HANDLE_NIL;
  attr->plugin_attr = false;

  return attr;
}

static void reader_sec_attributes_free(struct ddsi_reader_sec_attributes *attr)
{
   ddsrt_free(attr);
}

static void
participant_index_add(dds_security_context *sc, struct ddsi_participant_sec_attributes *attr)
{
  ddsrt_mutex_lock(&sc->partiticpant_index.lock);
  ddsrt_avl_cinsert(&participant_index_treedef, &sc->partiticpant_index.participants, attr);
  ddsrt_mutex_unlock(&sc->partiticpant_index.lock);
}

static struct ddsi_participant_sec_attributes *
participant_index_find(dds_security_context *sc, int64_t crypto_handle)
{
  struct ddsi_participant_sec_attributes *attr;

  ddsrt_mutex_lock(&sc->partiticpant_index.lock);
  attr = ddsrt_avl_clookup(&participant_index_treedef, &sc->partiticpant_index.participants, &crypto_handle);
  ddsrt_mutex_unlock(&sc->partiticpant_index.lock);

  return attr;
}

static struct ddsi_participant_sec_attributes *
participant_index_remove(dds_security_context *sc, int64_t crypto_handle)
{
  struct ddsi_participant_sec_attributes *attr;
  ddsrt_avl_dpath_t dpath;

  ddsrt_mutex_lock(&sc->partiticpant_index.lock);
  attr = ddsrt_avl_clookup_dpath(&participant_index_treedef, &sc->partiticpant_index.participants, &crypto_handle, &dpath);
  if (attr)
    ddsrt_avl_cdelete_dpath(&participant_index_treedef, &sc->partiticpant_index.participants, attr, &dpath);
  ddsrt_mutex_unlock(&sc->partiticpant_index.lock);

  return attr;
}

static uint32_t
get_matched_proxypp_crypto_handles(struct ddsi_participant_sec_attributes *attr, DDS_Security_ParticipantCryptoHandleSeq *hdls)
{
  uint32_t i;
  struct ddsi_pp_proxypp_match *pm;
  ddsrt_avl_citer_t it;

  ddsrt_mutex_lock(&attr->lock);
  hdls->_length =  hdls->_maximum = (uint32_t)ddsrt_avl_ccount(&attr->proxy_participants);
  hdls->_buffer = NULL;
  if (hdls->_length == 0)
  {
    ddsrt_mutex_unlock(&attr->lock);
    return 0;
  }
  hdls->_buffer = ddsrt_malloc(hdls->_length * sizeof(int64_t));
  for (pm = ddsrt_avl_citer_first(&pp_proxypp_treedef, &attr->proxy_participants, &it), i = 0; pm; pm = ddsrt_avl_citer_next(&it), i++)
    hdls->_buffer[i] = pm->proxypp_crypto_handle;
  ddsrt_mutex_unlock(&attr->lock);
  return hdls->_length;
}

static int64_t
get_first_matched_proxypp_crypto_handle(struct ddsi_participant_sec_attributes *attr)
{
  int64_t handle = 0;
  struct ddsi_pp_proxypp_match *pm;

  ddsrt_mutex_lock(&attr->lock);
  pm = ddsrt_avl_croot(&pp_proxypp_treedef, &attr->proxy_participants);
  if (pm)
    handle = pm->proxypp_crypto_handle;
  ddsrt_mutex_unlock(&attr->lock);

  return handle;
}

bool ddsi_omg_is_security_loaded (dds_security_context *sc)
{
  return (sc->crypto_context != NULL || sc->authentication_context != NULL || sc->access_control_context != NULL);
}

void ddsi_omg_security_init (struct ddsi_domaingv *gv)
{
  dds_security_context *sc;

  sc = ddsrt_malloc (sizeof (dds_security_context));
  memset (sc, 0, sizeof (dds_security_context));

  sc->auth_plugin.name = AUTH_NAME;
  sc->ac_plugin.name = AC_NAME;
  sc->crypto_plugin.name = CRYPTO_NAME;

  ddsrt_mutex_init(&sc->partiticpant_index.lock);
  ddsrt_avl_cinit(&participant_index_treedef, &sc->partiticpant_index.participants);
  pending_match_index_init(gv, &sc->security_matches);

  ddsrt_mutex_init (&sc->omg_security_lock);
  gv->security_context = sc;

  if (gv->config.omg_security_configuration)
    gv->handshake_include_optional = gv->config.omg_security_configuration->cfg.authentication_properties.include_optional_fields != 0;
  else
    gv->handshake_include_optional = false;

  ddsi_handshake_admin_init(gv);
}

/**
 * Releases all plugins
 */
static void release_plugins (struct ddsi_domaingv *gv, dds_security_context *sc)
{
  if (dds_security_plugin_release (&sc->auth_plugin, sc->authentication_context))
    GVERROR ("Error occurred releasing %s plugin", sc->auth_plugin.name);

  if (dds_security_plugin_release (&sc->crypto_plugin, sc->crypto_context))
    GVERROR ("Error occurred releasing %s plugin", sc->crypto_plugin.name);

  if (dds_security_plugin_release (&sc->ac_plugin, sc->access_control_context))
    GVERROR ("Error occurred releasing %s plugin", sc->ac_plugin.name);

  sc->authentication_context = NULL;
  sc->access_control_context = NULL;
  sc->crypto_context = NULL;
}

void ddsi_omg_security_stop (struct ddsi_domaingv *gv)
{
  dds_security_context *sc = gv->security_context;
  assert(sc);

  ddsi_handshake_admin_stop(gv);

  if (sc->authentication_context)
    sc->authentication_context->set_listener (sc->authentication_context, NULL, NULL);
  if (sc->access_control_context)
    sc->access_control_context->set_listener (sc->access_control_context, NULL, NULL);
}

void ddsi_omg_security_deinit (struct dds_security_context *sc)
{
  pending_match_index_deinit(&sc->security_matches);
}

void ddsi_omg_security_free (struct ddsi_domaingv *gv)
{
  dds_security_context *sc = gv->security_context;
  assert(sc);

  ddsrt_avl_cfree(&participant_index_treedef, &sc->partiticpant_index.participants, 0);
  ddsrt_mutex_destroy(&sc->partiticpant_index.lock);

  if (sc->authentication_context != NULL && sc->access_control_context != NULL && sc->crypto_context != NULL)
    release_plugins (gv, sc);

  ddsi_handshake_admin_deinit(gv);
  ddsrt_mutex_destroy (&sc->omg_security_lock);
  ddsrt_free(sc);
  gv->security_context = NULL;
}

static void dds_qos_to_security_plugin_configuration (const dds_qos_t *qos, dds_security_plugin_suite_config *suite_config)
{
  const struct { const char *name; size_t offset; } tab[] = {
    { DDS_SEC_PROP_AUTH_LIBRARY_PATH, offsetof (dds_security_plugin_suite_config, authentication.library_path) },
    { DDS_SEC_PROP_AUTH_LIBRARY_INIT, offsetof (dds_security_plugin_suite_config, authentication.library_init) },
    { DDS_SEC_PROP_AUTH_LIBRARY_FINALIZE, offsetof (dds_security_plugin_suite_config, authentication.library_finalize) },
    { DDS_SEC_PROP_CRYPTO_LIBRARY_PATH, offsetof (dds_security_plugin_suite_config, cryptography.library_path) },
    { DDS_SEC_PROP_CRYPTO_LIBRARY_INIT, offsetof (dds_security_plugin_suite_config, cryptography.library_init) },
    { DDS_SEC_PROP_CRYPTO_LIBRARY_FINALIZE, offsetof (dds_security_plugin_suite_config, cryptography.library_finalize) },
    { DDS_SEC_PROP_ACCESS_LIBRARY_PATH, offsetof (dds_security_plugin_suite_config, access_control.library_path) },
    { DDS_SEC_PROP_ACCESS_LIBRARY_INIT, offsetof (dds_security_plugin_suite_config, access_control.library_init) },
    { DDS_SEC_PROP_ACCESS_LIBRARY_FINALIZE, offsetof (dds_security_plugin_suite_config, access_control.library_finalize) }
  };

  for (size_t i = 0; i < qos->property.value.n; i++)
    for (size_t j = 0; j < sizeof (tab) / sizeof (tab[0]); j++)
      if (strcmp (qos->property.value.props[i].name, tab[j].name) == 0)
        *((char **) ((char *) suite_config + tab[j].offset)) = ddsrt_strdup (qos->property.value.props[i].value);
}

static void deinit_plugin_config (dds_security_plugin_config *plugin_config)
{
  ddsrt_free (plugin_config->library_path);
  ddsrt_free (plugin_config->library_init);
  ddsrt_free (plugin_config->library_finalize);
}

static void deinit_plugin_suite_config (dds_security_plugin_suite_config *suite_config)
{
  deinit_plugin_config (&suite_config->access_control);
  deinit_plugin_config (&suite_config->authentication);
  deinit_plugin_config (&suite_config->cryptography);
}

typedef bool (*expired_pp_check_fn_t)(const struct ddsi_participant * pp, DDS_Security_Handle handle);
typedef bool (*expired_proxypp_check_fn_t)(const struct ddsi_proxy_participant * proxypp, DDS_Security_Handle handle);

static bool delete_pp_by_handle (DDS_Security_Handle handle, expired_pp_check_fn_t expired_pp_check_fn, struct ddsi_domaingv *gv)
{
  struct ddsi_participant *pp;
  struct ddsi_entity_enum_participant epp;
  bool result = false;
  ddsi_entidx_enum_participant_init (&epp, gv->entity_index);
  while ((pp = ddsi_entidx_enum_participant_next (&epp)) != NULL)
  {
    if (ddsi_omg_participant_is_secure (pp) && expired_pp_check_fn (pp, handle))
    {
      (void) ddsi_delete_participant (gv, &pp->e.guid);
      result = true;
    }
  }
  ddsi_entidx_enum_participant_fini (&epp);
  return result;
}

static bool delete_proxypp_by_handle (const DDS_Security_Handle handle, expired_proxypp_check_fn_t expired_proxypp_check_fn, struct ddsi_domaingv *gv)
{
  struct ddsi_proxy_participant *proxypp;
  struct ddsi_entity_enum_proxy_participant eproxypp;
  bool result = false;
  ddsi_entidx_enum_proxy_participant_init (&eproxypp, gv->entity_index);
  while ((proxypp = ddsi_entidx_enum_proxy_participant_next (&eproxypp)) != NULL)
  {
    if (ddsi_omg_proxy_participant_is_secure (proxypp) && expired_proxypp_check_fn (proxypp, handle))
    {
      (void) ddsi_delete_proxy_participant_by_guid (gv, &proxypp->e.guid, ddsrt_time_wallclock (), true);
      result = true;
    }
  }
  ddsi_entidx_enum_proxy_participant_fini (&eproxypp);
  return result;
}

static bool pp_expired_by_perm (const struct ddsi_participant * pp, DDS_Security_Handle handle)
{
  return pp->sec_attr->permissions_handle == handle;
}

static bool proxypp_expired_by_perm (const struct ddsi_proxy_participant * proxypp, DDS_Security_Handle handle)
{
  bool result = false;
  uint32_t i = 0;
  ddsrt_avl_iter_t it;
  ddsrt_mutex_lock (&proxypp->sec_attr->lock);
  for (struct ddsi_proxypp_pp_match *ppm = ddsrt_avl_iter_first (&proxypp_pp_treedef, &proxypp->sec_attr->participants, &it); ppm; ppm = ddsrt_avl_iter_next (&it), i++)
  {
    if (ppm->permissions_handle == handle)
    {
      result = true;
      break;
    }
  }
  ddsrt_mutex_unlock (&proxypp->sec_attr->lock);
  return result;
}

static bool pp_expired_by_id (const struct ddsi_participant * pp, DDS_Security_Handle handle)
{
  return pp->sec_attr->local_identity_handle == handle;
}

static bool proxypp_expired_by_id (const struct ddsi_proxy_participant * proxypp, DDS_Security_Handle handle)
{
  return proxypp->sec_attr->remote_identity_handle == handle;
}

/* When a local identity (i.e. the identity of a local participant) or
  a local permissions handle (bound to a local participant) expires,
  the participant will be deleted. Strictly speaking, as described in the DDS
  Security specification, the communication for this partcipant should be
  stopped. A possible interpretation is that the participant and its
  depending endpoints remain alive in 'expired' state and e.g. unread data
  that was received earlier could still be retrieved by the application.
  As we considered this as an edge case that would not be used widely,
  the current implementation simply deletes the DDSI participant and leaves
  the participant entity in the API in an invalid state, which could result
  in error codes when calling API functions on these entities. This approach
  dramatically simplifies the code for handling the revocation of permission
  and identity handles.

  For remote identity revocation, in case any of the permission handles
  in a pp-match of a proxy participant is expired, the proxy participant
  is deleted, as the expired permission grant for a (remote) participant
  applies to the participant as a whole (bound to its subject of the
  identity certificate used by the participant) */
static DDS_Security_boolean on_revoke_permissions_cb(const dds_security_access_control *plugin, const DDS_Security_PermissionsHandle handle)
{
  struct ddsi_domaingv *gv = plugin->gv;
  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv);

  if (!delete_pp_by_handle (handle, pp_expired_by_perm, gv))
    delete_proxypp_by_handle (handle, proxypp_expired_by_perm, gv);

  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  return true;
}

/* See comment above on_revoke_permissions_cb */
static DDS_Security_boolean on_revoke_identity_cb(const dds_security_authentication *plugin, const DDS_Security_IdentityHandle handle)
{
  struct ddsi_domaingv *gv = plugin->gv;
  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv);

  if (!delete_pp_by_handle (handle, pp_expired_by_id, gv))
    delete_proxypp_by_handle (handle, proxypp_expired_by_id, gv);

  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  return true;
}

dds_return_t ddsi_omg_security_load (dds_security_context *sc, const dds_qos_t *qos, struct ddsi_domaingv *gv)
{
  dds_security_plugin_suite_config psc;
  memset (&psc, 0, sizeof (psc));

  ddsrt_mutex_lock (&sc->omg_security_lock);

  /* Get plugin information */
  dds_qos_to_security_plugin_configuration (qos, &psc);

  /* Check configuration content */
  if (dds_security_check_plugin_configuration (&psc, gv) != DDS_RETCODE_OK)
    goto error;

  if (dds_security_load_security_library (&psc.authentication, &sc->auth_plugin, (void **) &sc->authentication_context, gv) != DDS_RETCODE_OK)
  {
    GVERROR ("Could not load %s plugin.\n", sc->auth_plugin.name);
    goto error;
  }
  if (dds_security_load_security_library (&psc.access_control, &sc->ac_plugin, (void **) &sc->access_control_context, gv) != DDS_RETCODE_OK)
  {
    GVERROR ("Could not load %s library\n", sc->ac_plugin.name);
    goto error;
  }
  if (dds_security_load_security_library (&psc.cryptography, &sc->crypto_plugin, (void **) &sc->crypto_context, gv) != DDS_RETCODE_OK)
  {
    GVERROR ("Could not load %s library\n", sc->crypto_plugin.name);
    goto error;
  }

  /* now check if all plugin functions are implemented */
  if (dds_security_verify_plugin_functions (sc->authentication_context, &sc->auth_plugin, sc->crypto_context, &sc->crypto_plugin,
    sc->access_control_context, &sc->ac_plugin, gv) != DDS_RETCODE_OK)
  {
    goto error_verify;
  }

  /* Add listeners */
  DDS_Security_SecurityException ex = DDS_SECURITY_EXCEPTION_INIT;
  sc->ac_listener.on_revoke_permissions = on_revoke_permissions_cb;
  if (!sc->access_control_context->set_listener (sc->access_control_context, &sc->ac_listener, &ex))
  {
    GVERROR ("Could not set access_control listener: %s\n", ex.message ? ex.message : "<unknown error>");
    goto error_set_ac_listener;
  }
  sc->auth_listener.on_revoke_identity = on_revoke_identity_cb;
  if (!sc->authentication_context->set_listener (sc->authentication_context, &sc->auth_listener, &ex))
  {
    GVERROR ("Could not set authentication listener: %s\n", ex.message ? ex.message : "<unknown error>");
    goto error_set_auth_listener;
  }

#if HANDSHAKE_IMPLEMENTED
    (void) handshake_initialize ();
#endif

  deinit_plugin_suite_config (&psc);
  ddsrt_mutex_unlock (&sc->omg_security_lock);
  GVTRACE ("DDS Security plugins have been loaded\n");
  return DDS_RETCODE_OK;

error_set_auth_listener:
  sc->access_control_context->set_listener (sc->access_control_context, NULL, &ex);
error_set_ac_listener:
error_verify:
  release_plugins (gv, sc);
error:
  deinit_plugin_suite_config (&psc);
  ddsrt_mutex_unlock (&sc->omg_security_lock);
  return DDS_RETCODE_ERROR;
}

static void notify_handshake_recv_token(struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp)
{
  struct ddsi_handshake *handshake;

  handshake = ddsi_handshake_find(pp, proxypp);
  if (handshake) {
    ddsi_handshake_crypto_tokens_received(handshake);
    ddsi_handshake_release(handshake);
  }
}

bool ddsi_omg_participant_is_secure(const struct ddsi_participant *pp)
{
  return ((pp->sec_attr != NULL) && (pp->sec_attr->crypto_handle != DDS_SECURITY_HANDLE_NIL));
}

bool ddsi_omg_proxy_participant_is_secure (const struct ddsi_proxy_participant *proxypp)
{
  return (proxypp->sec_attr != NULL);
}

bool ddsi_omg_participant_allow_unauthenticated(struct ddsi_participant *pp)
{
  return ((pp->sec_attr != NULL) && pp->sec_attr->attr.allow_unauthenticated_participants);
}

dds_return_t ddsi_omg_security_check_create_participant (struct ddsi_participant *pp, uint32_t domain_id)
{
  dds_return_t ret = DDS_RETCODE_NOT_ALLOWED_BY_SECURITY;
  struct dds_security_context *sc = ddsi_omg_security_get_secure_context (pp);
  struct ddsi_domaingv *gv = pp->e.gv;
  DDS_Security_IdentityHandle identity_handle = DDS_SECURITY_HANDLE_NIL;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_ValidationResult_t result = 0;
  DDS_Security_IdentityToken identity_token;
  DDS_Security_PermissionsToken permissions_token = DDS_SECURITY_TOKEN_INIT;
  DDS_Security_PermissionsCredentialToken credential_token = DDS_SECURITY_TOKEN_INIT;
  struct ddsi_participant_sec_attributes *sec_attr = NULL;
  DDS_Security_Qos par_qos;
  ddsi_guid_t candidate_guid;
  ddsi_guid_t adjusted_guid;

  /* Security context may be NULL at this point if participant has no security configured */
  if (!sc)
    return DDS_RETCODE_OK;

  /* Validate local identity */
  ETRACE (pp, "validate_local_identity: candidate_guid: "PGUIDFMT" ", PGUID (pp->e.guid));

  candidate_guid = ddsi_hton_guid(pp->e.guid);
  ddsi_omg_shallow_copy_security_qos (&par_qos, &(pp->plist->qos));

  result = sc->authentication_context->validate_local_identity(
      sc->authentication_context, &identity_handle,
      (DDS_Security_GUID_t *) &adjusted_guid, (DDS_Security_DomainId) domain_id, &par_qos,
      (DDS_Security_GUID_t *) &candidate_guid, &exception);
  if (result != DDS_SECURITY_VALIDATION_OK)
  {
    EXCEPTION_ERROR(gv, &exception, "Error occurred while validating local permission");
    goto validation_failed;
  }
  pp->e.guid = ddsi_ntoh_guid(adjusted_guid);

  sec_attr = participant_sec_attributes_new(&pp->e.guid);
  sec_attr->local_identity_handle = identity_handle;

  ETRACE (pp, "adjusted_guid: "PGUIDFMT" ", PGUID (pp->e.guid));

  /* Get the identity token and add this to the plist of the participant */
  if (!sc->authentication_context->get_identity_token(sc->authentication_context, &identity_token, identity_handle, &exception))
  {
    EXCEPTION_ERROR(gv, &exception, "Error occurred while retrieving the identity token");
    goto validation_failed;
  }
  assert(exception.code == 0);

  ddsi_omg_security_dataholder_copyin (&pp->plist->identity_token, &identity_token);
  DDS_Security_DataHolder_deinit(&identity_token);
  pp->plist->present |= PP_IDENTITY_TOKEN;

  sec_attr->permissions_handle = sc->access_control_context->validate_local_permissions(
       sc->access_control_context, sc->authentication_context, identity_handle,
       (DDS_Security_DomainId)domain_id, &par_qos, &exception);

  if (sec_attr->permissions_handle == DDS_SECURITY_HANDLE_NIL)
  {
    EXCEPTION_ERROR(gv, &exception, "Error occurred while validating local permissions");
    goto not_allowed;
  }

  /* ask to access control security plugin for create participant permissions related to this identity*/
  if (!sc->access_control_context->check_create_participant(sc->access_control_context, sec_attr->permissions_handle, (DDS_Security_DomainId) domain_id, &par_qos, &exception))
  {
    EXCEPTION_ERROR(gv, &exception, "It is not allowed to create participant");
    goto not_allowed;
  }

  /* Get the identity token and add this to the plist of the participant */
  if (!sc->access_control_context->get_permissions_token(sc->access_control_context, &permissions_token, sec_attr->permissions_handle, &exception))
  {
    EXCEPTION_ERROR(gv, &exception, "Error occurred while retrieving the permissions token");
    goto not_allowed;
  }

  ddsi_omg_security_dataholder_copyin (&pp->plist->permissions_token, &permissions_token);
  pp->plist->present |= PP_PERMISSIONS_TOKEN;

  if (!sc->access_control_context->get_permissions_credential_token(sc->access_control_context, &credential_token, sec_attr->permissions_handle, &exception))
  {
    EXCEPTION_ERROR(gv, &exception, "Error occurred while retrieving the permissions credential token");
    goto no_credentials;
  }

  if (!sc->authentication_context->set_permissions_credential_and_token(sc->authentication_context, sec_attr->local_identity_handle, &credential_token, &permissions_token, &exception))
  {
    EXCEPTION_ERROR(gv, &exception, "Error occurred while setting the permissions credential token");
    goto no_credentials;
  }

  if (!sc->access_control_context->get_participant_sec_attributes(sc->access_control_context, sec_attr->permissions_handle, &sec_attr->attr, &exception))
  {
    EXCEPTION_ERROR(gv, &exception, "Failed to get participant security attributes");
    goto no_sec_attr;
  }

  sec_attr->plugin_attr = true;
  sec_attr->crypto_handle = sc->crypto_context->crypto_key_factory->register_local_participant(
            sc->crypto_context->crypto_key_factory, sec_attr->local_identity_handle, sec_attr->permissions_handle, NULL, &sec_attr->attr, &exception);
  if (!sec_attr->crypto_handle) {
    EXCEPTION_ERROR(gv, &exception, "Failed to register participant with crypto key factory");
    goto no_crypto;
  }

  participant_index_add(sc, sec_attr);
  pp->sec_attr = sec_attr;

  ETRACE (pp, "\n");

  ret = DDS_RETCODE_OK;

no_crypto:
no_sec_attr:
no_credentials:
  if (permissions_token.class_id)
    (void)sc->access_control_context->return_permissions_token(sc->access_control_context, &permissions_token, NULL);
  if (credential_token.class_id)
    (void)sc->access_control_context->return_permissions_credential_token(sc->access_control_context, &credential_token, NULL);
not_allowed:
  if (ret != DDS_RETCODE_OK)
    participant_sec_attributes_free(sec_attr);
validation_failed:
  ddsi_omg_shallow_free_security_qos (&par_qos);
  return ret;
}

void ddsi_omg_security_participant_set_initialized (struct ddsi_participant *pp)
{
  if (pp->sec_attr)
  {
    ddsrt_mutex_lock(&pp->sec_attr->lock);
    pp->sec_attr->initialized = true;
    ddsrt_mutex_unlock(&pp->sec_attr->lock);
  }
}

bool ddsi_omg_security_participant_is_initialized (struct ddsi_participant *pp)
{
  bool initialized = false;

  if (pp->sec_attr)
  {
    ddsrt_mutex_lock(&pp->sec_attr->lock);
    initialized = pp->sec_attr->initialized;
    ddsrt_mutex_unlock(&pp->sec_attr->lock);
  }
  return initialized;
}

struct cleanup_participant_sec_attributes_arg {
  struct ddsi_domaingv *gv;
  int64_t crypto_handle;
};

static void cleanup_participant_sec_attributes(void *arg)
{
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  struct cleanup_participant_sec_attributes_arg *info = arg;
  struct ddsi_domaingv * gv = info->gv;
  dds_security_context *sc = gv->security_context;
  struct ddsi_participant_sec_attributes *attr;
  struct ddsi_pp_proxypp_match *pm;

  assert(sc);
  if ((attr = participant_index_remove(sc, info->crypto_handle)) == NULL)
    return;

  GVTRACE("cleanup participant "PGUIDFMT" security attributes\n", PGUID(attr->pp_guid));

  pm = ddsrt_avl_cfind_min(&pp_proxypp_treedef, &attr->proxy_participants);
  while (pm)
  {
    struct ddsi_pp_proxypp_match *next = ddsrt_avl_cfind_succ(&pp_proxypp_treedef, &attr->proxy_participants, pm);
    ddsrt_mutex_lock(&gv->lock);
    struct ddsi_proxy_participant *proxypp = ddsi_entidx_lookup_proxy_participant_guid(gv->entity_index, &pm->proxypp_guid);
    if (proxypp)
      proxypp_pp_unrelate(sc, proxypp, &attr->pp_guid, attr->crypto_handle);
    ddsrt_mutex_unlock(&gv->lock);
    ddsrt_avl_cdelete(&pp_proxypp_treedef, &attr->proxy_participants, pm);
    ddsrt_free(pm);
    pm = next;
  }

  if (attr->permissions_handle != DDS_SECURITY_HANDLE_NIL)
  {
    if (!sc->access_control_context->return_permissions_handle(sc->access_control_context, attr->permissions_handle, &exception))
      EXCEPTION_ERROR(gv, &exception, "Failed to return local permissions handle");
  }
  if (attr->local_identity_handle != DDS_SECURITY_HANDLE_NIL)
  {
    if (!sc->authentication_context->return_identity_handle(sc->authentication_context, attr->local_identity_handle, &exception))
      EXCEPTION_ERROR(gv, &exception, "Failed to return local identity handle");
  }
  if (attr->plugin_attr)
  {
    if (!sc->access_control_context->return_participant_sec_attributes(sc->access_control_context, &attr->attr, &exception))
      EXCEPTION_ERROR(gv, &exception, "Failed to return participant security attributes");
  }

  if (!sc->crypto_context->crypto_key_factory->unregister_participant(sc->crypto_context->crypto_key_factory, attr->crypto_handle, &exception))
    EXCEPTION_ERROR(gv, &exception, "Failed to unregister participant");

  participant_sec_attributes_free(attr);
  ddsrt_free(arg);
}

void ddsi_omg_security_deregister_participant (struct ddsi_participant *pp)
{
//  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  struct dds_security_context *sc = ddsi_omg_security_get_secure_context (pp);

  if (!sc)
    return;

  /* When the participant is deleted the timed event queue may still contain
   * messages from this participant. Therefore the crypto handle should still
   * be available to ensure that the rtps message can be encoded.
   * For this purpose the cleanup of the associated crypto handle is delayed.
   * A callback is scheduled to be called after some delay to cleanup this
   * crypto handle.
   */
  if (pp->sec_attr->crypto_handle != DDS_SECURITY_HANDLE_NIL) {
    struct cleanup_participant_sec_attributes_arg *arg = ddsrt_malloc (sizeof (*arg));
    arg->crypto_handle = pp->sec_attr->crypto_handle;
    arg->gv = pp->e.gv;
    ddsi_qxev_nt_callback(pp->e.gv->xevents, cleanup_participant_sec_attributes, arg);
  }

  clear_pending_matches_by_local_guid(sc, &sc->security_matches, &pp->e.guid);

  pp->sec_attr = NULL;
}

int64_t ddsi_omg_security_get_local_participant_handle (const struct ddsi_participant *pp)
{
  if (pp->sec_attr)
    return pp->sec_attr->crypto_handle;
  return 0;
}

bool ddsi_omg_participant_is_access_protected(const struct ddsi_participant *pp)
{
  return ((pp->sec_attr != NULL) && pp->sec_attr->attr.is_access_protected);
}

bool ddsi_omg_participant_is_rtps_protected(const struct ddsi_participant *pp)
{
  return ((pp->sec_attr != NULL) && pp->sec_attr->attr.is_rtps_protected);
}

bool ddsi_omg_participant_is_liveliness_protected(const struct ddsi_participant *pp)
{
  return ((pp->sec_attr != NULL) && pp->sec_attr->attr.is_liveliness_protected);
}

bool ddsi_omg_participant_is_discovery_protected(const struct ddsi_participant *pp)
{
  return ((pp->sec_attr != NULL) && pp->sec_attr->attr.is_discovery_protected);
}

static bool maybe_rtps_protected(ddsi_entityid_t entityid)
{
  if (!ddsi_is_builtin_entityid(entityid, DDSI_VENDORID_ECLIPSE))
    return true;

  switch (entityid.u)
  {
    case DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER:
    case DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER:
    case DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER:
    case DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER:
    case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER:
    case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER:
    case DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER:
    case DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER:
      return true;
    default:
      return false;
  }
}

static bool proxypp_is_rtps_protected(const struct ddsi_proxy_participant *proxypp)
{
  return (proxypp->sec_attr != NULL && SECURITY_INFO_IS_RTPS_PROTECTED(proxypp->security_info));
}

bool ddsi_omg_security_is_remote_rtps_protected (const struct ddsi_proxy_participant *proxypp, ddsi_entityid_t entityid)
{
  return ddsi_omg_proxy_participant_is_secure (proxypp) &&
    SECURITY_INFO_IS_RTPS_PROTECTED(proxypp->security_info) &&
    maybe_rtps_protected(entityid);
}

bool ddsi_omg_security_is_local_rtps_protected (const struct ddsi_participant *pp, ddsi_entityid_t entityid)
{
  return ddsi_omg_participant_is_rtps_protected(pp) && maybe_rtps_protected(entityid);
}

bool ddsi_omg_get_participant_security_info (const struct ddsi_participant *pp, ddsi_security_info_t *info)
{
  assert(pp);
  assert(info);

  if (ddsi_omg_participant_is_secure(pp)) {
    const DDS_Security_ParticipantSecurityAttributes *attr = &(pp->sec_attr->attr);

    info->security_attributes = DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    info->plugin_security_attributes = attr->plugin_participant_attributes;

    if (attr->is_discovery_protected)
      info->security_attributes |= DDSI_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_DISCOVERY_PROTECTED;

    if (attr->is_liveliness_protected)
      info->security_attributes |= DDSI_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_LIVELINESS_PROTECTED;

    if (attr->is_rtps_protected)
      info->security_attributes |= DDSI_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_RTPS_PROTECTED;

    return true;
  }

  info->security_attributes = 0;
  info->plugin_security_attributes = 0;

  return false;
}

static void ddsi_omg_get_endpoint_security_info (DDS_Security_EndpointSecurityAttributes *attr, ddsi_security_info_t *info)
{
    info->security_attributes = DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    info->plugin_security_attributes = attr->plugin_endpoint_attributes;

    if (attr->is_read_protected)
        info->security_attributes |= DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_READ_PROTECTED;

    if (attr->is_write_protected)
        info->security_attributes |= DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_WRITE_PROTECTED;

    if (attr->is_discovery_protected)
        info->security_attributes |= DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_DISCOVERY_PROTECTED;

    if (attr->is_liveliness_protected)
        info->security_attributes |= DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_LIVELINESS_PROTECTED;

    if (attr->is_submessage_protected)
        info->security_attributes |= DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;

    if (attr->is_payload_protected)
        info->security_attributes |= DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_PAYLOAD_PROTECTED;

    if (attr->is_key_protected)
        info->security_attributes |= DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_KEY_PROTECTED;
}

static bool is_topic_discovery_protected(DDS_Security_PermissionsHandle permission_handle, dds_security_access_control *access_control, const char *topic_name)
{
  DDS_Security_TopicSecurityAttributes attributes = {0,0,0,0};
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  bool result = false;

  if (access_control->get_topic_sec_attributes(access_control, permission_handle, topic_name, &attributes, &exception))
  {
    result = attributes.is_discovery_protected;
    access_control->return_topic_sec_attributes(access_control, &attributes, &exception);
  }
  else
  {
    DDS_Security_Exception_reset(&exception);
  }
  return result;
}

static void handle_not_allowed(
    const struct ddsi_domaingv *gv,
    DDS_Security_PermissionsHandle permissions_handle,
    dds_security_access_control * ac_ctx,
    DDS_Security_SecurityException * exception,
    const char * topic_name,
    const char * fmt,
    ...) ddsrt_attribute_format_printf(6, 7);

static void handle_not_allowed(const struct ddsi_domaingv *gv, DDS_Security_PermissionsHandle permissions_handle, dds_security_access_control * ac_ctx,
    DDS_Security_SecurityException * exception, const char * topic_name, const char * fmt, ...)
{
  /* In case topic has discovery protection enabled: don't log in log category error, as the message
      will contain the topic name which may be considered as sensitive information */
  va_list ap;
  bool discovery_protected = is_topic_discovery_protected(permissions_handle, ac_ctx, topic_name);
  va_start (ap, fmt);
  EXCEPTION_VLOG(gv, exception, discovery_protected ? DDS_LC_TRACE : DDS_LC_ERROR, fmt, ap);
  va_end (ap);
  if (discovery_protected)
    DDS_Security_Exception_reset(exception);
}

bool ddsi_omg_security_check_create_topic (const struct ddsi_domaingv *gv, const ddsi_guid_t *pp_guid, const char *topic_name, const struct dds_qos *qos)
{
  bool result = true;
  struct ddsi_participant *pp;
  struct dds_security_context *sc;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_Qos topic_qos;

  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv);
  pp = ddsi_entidx_lookup_participant_guid (gv->entity_index, pp_guid);

  if ((sc = ddsi_omg_security_get_secure_context (pp)) != NULL)
  {
    ddsi_omg_shallow_copy_security_qos (&topic_qos, qos);
    result = sc->access_control_context->check_create_topic(sc->access_control_context, pp->sec_attr->permissions_handle, (DDS_Security_DomainId)gv->config.domainId, topic_name, &topic_qos, &exception);
    if (!result)
      handle_not_allowed(gv, pp->sec_attr->permissions_handle, sc->access_control_context, &exception, topic_name, "Local topic permission denied");
    ddsi_omg_shallow_free_security_qos (&topic_qos);
  }
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());

  return result;
}

bool ddsi_omg_security_check_create_writer (struct ddsi_participant *pp, uint32_t domain_id, const char *topic_name, const struct dds_qos *writer_qos)
{
  struct dds_security_context *sc = ddsi_omg_security_get_secure_context (pp) ;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_PartitionQosPolicy partitions;
  DDS_Security_Qos security_qos;
  bool result;

  if (!sc)
    return true;

  if (writer_qos->present & DDSI_QP_PARTITION)
    ddsi_omg_shallow_copy_StringSeq(&partitions.name, &(writer_qos->partition));
  else
    memset(&(partitions), 0, sizeof(DDS_Security_PartitionQosPolicy));

  ddsi_omg_shallow_copy_security_qos (&security_qos, writer_qos);

  result = sc->access_control_context->check_create_datawriter(sc->access_control_context, pp->sec_attr->permissions_handle, (DDS_Security_DomainId)domain_id, topic_name, &security_qos, &partitions, NULL, &exception);
  if (!result)
    handle_not_allowed(pp->e.gv, pp->sec_attr->permissions_handle, sc->access_control_context, &exception, topic_name, "Writer is not permitted");

  ddsi_omg_shallow_free_security_qos (&security_qos);
  ddsi_omg_shallow_free_StringSeq(&partitions.name);

  return result;
}

void ddsi_omg_security_register_writer (struct ddsi_writer *wr)
{
  struct ddsi_participant *pp = wr->c.pp;
  struct dds_security_context *sc = ddsi_omg_security_get_secure_context (pp);
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_PartitionQosPolicy partitions;
  DDS_Security_PropertySeq properties;

  if (!sc)
    return;

  if (wr->xqos->present & DDSI_QP_PARTITION)
    ddsi_omg_shallow_copy_StringSeq(&partitions.name, &(wr->xqos->partition));
  else
    memset(&(partitions), 0, sizeof(DDS_Security_PartitionQosPolicy));

  wr->sec_attr = writer_sec_attributes_new();
  if (!sc->access_control_context->get_datawriter_sec_attributes(sc->access_control_context, pp->sec_attr->permissions_handle, wr->xqos->topic_name, &partitions, NULL, &wr->sec_attr->attr, &exception))
  {
    EXCEPTION_ERROR(pp->e.gv, &exception, "Failed to retrieve writer security attributes");
    goto no_attr;
  }
  wr->sec_attr->plugin_attr = true;

  if (wr->sec_attr->attr.is_payload_protected || wr->sec_attr->attr.is_submessage_protected)
  {
    if (wr->xqos->present & DDSI_QP_PROPERTY_LIST)
      ddsi_omg_copy_PropertySeq (&properties, &wr->xqos->property.value);
    else
      memset(&properties, 0, sizeof(DDS_Security_PropertySeq));

    wr->sec_attr->crypto_handle = sc->crypto_context->crypto_key_factory->register_local_datawriter(
        sc->crypto_context->crypto_key_factory, pp->sec_attr->crypto_handle, &properties, &wr->sec_attr->attr, &exception);
    DDS_Security_PropertySeq_freebuf(&properties);
    if (wr->sec_attr->crypto_handle == DDS_SECURITY_HANDLE_NIL)
    {
      EXCEPTION_ERROR(pp->e.gv, &exception, "Failed to register writer with crypto");
      goto not_registered;
    }
  }

  if (wr->sec_attr->attr.is_key_protected && (wr->e.guid.entityid.u & DDSI_ENTITYID_KIND_MASK) == DDSI_ENTITYID_KIND_WRITER_WITH_KEY)
  {
    wr->num_readers_requesting_keyhash++;
    wr->force_md5_keyhash = 1;
  }

not_registered:
no_attr:
  ddsi_omg_shallow_free_StringSeq(&partitions.name);
}

void ddsi_omg_security_deregister_writer (struct ddsi_writer *wr)
{
  struct dds_security_context *sc = ddsi_omg_security_get_secure_context (wr->c.pp);
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;

  if (wr->sec_attr)
  {
    assert(sc);
    clear_pending_matches_by_local_guid(sc, &sc->security_matches, &wr->e.guid);

    if (wr->sec_attr->crypto_handle != DDS_SECURITY_HANDLE_NIL)
    {
      if (!sc->crypto_context->crypto_key_factory->unregister_datawriter(sc->crypto_context->crypto_key_factory, wr->sec_attr->crypto_handle, &exception))
        EXCEPTION_ERROR(wr->e.gv, &exception, "Failed to unregister writer with crypto");
    }
    if (wr->sec_attr->plugin_attr)
    {
      if (!sc->access_control_context->return_datawriter_sec_attributes(sc->access_control_context, &wr->sec_attr->attr, &exception))
        EXCEPTION_ERROR(wr->e.gv, &exception, "Failed to return writer security attributes");
    }
    writer_sec_attributes_free(wr->sec_attr);
    wr->sec_attr = NULL;
  }
}

bool ddsi_omg_get_writer_security_info (const struct ddsi_writer *wr, ddsi_security_info_t *info)
{
  assert(wr);
  assert(info);

  if (wr->sec_attr) {
      ddsi_omg_get_endpoint_security_info (&wr->sec_attr->attr, info);
      return true;
  }
  info->plugin_security_attributes = 0;
  info->security_attributes = 0;
  return false;
}

bool ddsi_omg_security_check_create_reader (struct ddsi_participant *pp, uint32_t domain_id, const char *topic_name, const struct dds_qos *reader_qos)
{
  struct dds_security_context *sc = ddsi_omg_security_get_secure_context (pp);
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_PartitionQosPolicy partitions;
  DDS_Security_Qos security_qos;
  bool result;

  if (!sc)
    return true;

  if (reader_qos->present & DDSI_QP_PARTITION)
    ddsi_omg_shallow_copy_StringSeq(&partitions.name, &(reader_qos->partition));
  else
    memset(&(partitions), 0, sizeof(DDS_Security_PartitionQosPolicy));

  ddsi_omg_shallow_copy_security_qos (&security_qos, reader_qos);

  result = sc->access_control_context->check_create_datareader(sc->access_control_context, pp->sec_attr->permissions_handle, (DDS_Security_DomainId)domain_id, topic_name, &security_qos, &partitions, NULL, &exception);
  if (!result)
    handle_not_allowed(pp->e.gv, pp->sec_attr->permissions_handle, sc->access_control_context, &exception, topic_name, "Reader is not permitted");

  ddsi_omg_shallow_free_security_qos (&security_qos);
  ddsi_omg_shallow_free_StringSeq(&partitions.name);

  return result;
}

void ddsi_omg_security_register_reader (struct ddsi_reader *rd)
{
  struct ddsi_participant *pp = rd->c.pp;
  struct dds_security_context *sc = ddsi_omg_security_get_secure_context (pp);
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_PartitionQosPolicy partitions;
  DDS_Security_PropertySeq properties;

  if (!sc)
    return;

  if (rd->xqos->present & DDSI_QP_PARTITION)
    ddsi_omg_shallow_copy_StringSeq(&partitions.name, &(rd->xqos->partition));
  else
    memset(&(partitions), 0, sizeof(DDS_Security_PartitionQosPolicy));

  rd->sec_attr = reader_sec_attributes_new();

  if (!sc->access_control_context->get_datareader_sec_attributes(sc->access_control_context, pp->sec_attr->permissions_handle, rd->xqos->topic_name, &partitions, NULL, &rd->sec_attr->attr, &exception))
  {
    EXCEPTION_ERROR(pp->e.gv, &exception, "Failed to retrieve reader security attributes");
    goto no_attr;
  }
  rd->sec_attr->plugin_attr = true;

  if (rd->sec_attr->attr.is_payload_protected || rd->sec_attr->attr.is_submessage_protected)
  {
    if (rd->xqos->present & DDSI_QP_PROPERTY_LIST)
      ddsi_omg_copy_PropertySeq (&properties, &rd->xqos->property.value);
    else
      memset(&properties, 0, sizeof(DDS_Security_PropertySeq));

    rd->sec_attr->crypto_handle = sc->crypto_context->crypto_key_factory->register_local_datareader(
        sc->crypto_context->crypto_key_factory, pp->sec_attr->crypto_handle, &properties, &rd->sec_attr->attr, &exception);
    DDS_Security_PropertySeq_freebuf(&properties);
    if (rd->sec_attr->crypto_handle == DDS_SECURITY_HANDLE_NIL)
    {
      EXCEPTION_ERROR(pp->e.gv, &exception, "Failed to register reader with crypto");
      goto not_registered;
    }
  }

not_registered:
no_attr:
  ddsi_omg_shallow_free_StringSeq(&partitions.name);
}

void ddsi_omg_security_deregister_reader (struct ddsi_reader *rd)
{
  struct dds_security_context *sc = ddsi_omg_security_get_secure_context (rd->c.pp);
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;

  if (rd->sec_attr)
  {
    assert(sc);

    clear_pending_matches_by_local_guid(sc, &sc->security_matches, &rd->e.guid);

    if (rd->sec_attr->crypto_handle != DDS_SECURITY_HANDLE_NIL)
    {
      if (!sc->crypto_context->crypto_key_factory->unregister_datareader(sc->crypto_context->crypto_key_factory, rd->sec_attr->crypto_handle, &exception))
      {
        EXCEPTION_ERROR(rd->e.gv, &exception, "Failed to unregister reader with crypto");
      }
    }
    if (rd->sec_attr->plugin_attr)
    {
      if (!sc->access_control_context->return_datareader_sec_attributes(sc->access_control_context, &rd->sec_attr->attr, &exception))
      {
        EXCEPTION_ERROR(rd->e.gv, &exception, "Failed to return reader security attributes");
      }
    }
    reader_sec_attributes_free(rd->sec_attr);
    rd->sec_attr = NULL;
  }
}

bool ddsi_omg_get_reader_security_info (const struct ddsi_reader *rd, ddsi_security_info_t *info)
{
  assert(rd);
  assert(info);

  if (rd->sec_attr) {
    ddsi_omg_get_endpoint_security_info (&rd->sec_attr->attr, info);
    return true;
  }
  info->plugin_security_attributes = 0;
  info->security_attributes = 0;
  return false;
}

unsigned ddsi_determine_subscription_writer(const struct ddsi_reader *rd)
{
  if (ddsi_omg_reader_is_discovery_protected (rd))
    return DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER;
  else
    return DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER;
}

unsigned ddsi_determine_publication_writer (const struct ddsi_writer *wr)
{
  if (ddsi_omg_writer_is_discovery_protected (wr))
    return DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER;
  else
    return DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER;
}

#ifdef DDS_HAS_TOPIC_DISCOVERY
unsigned ddsi_determine_topic_writer (const struct ddsi_topic *tp)
{
  if (ddsi_omg_participant_is_discovery_protected (tp->pp))
    abort (); /* FIXME: not implemented */
  return DDSI_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER;
}
#endif

static int64_t check_remote_participant_permissions(uint32_t domain_id, struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp, int64_t remote_identity_handle)
{
  struct dds_security_context *sc = ddsi_omg_security_get_secure_context (pp);
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  struct ddsi_handshake *handshake;
  DDS_Security_PermissionsToken permissions_token = DDS_SECURITY_TOKEN_INIT;
  DDS_Security_AuthenticatedPeerCredentialToken peer_credential_token = DDS_SECURITY_TOKEN_INIT;
  int64_t permissions_hdl = DDS_SECURITY_HANDLE_NIL;
  struct ddsi_domaingv *gv = pp->e.gv;

  assert (sc);
  if (proxypp->plist->present & PP_PERMISSIONS_TOKEN)
      ddsi_omg_shallow_copyin_DataHolder (&permissions_token, &proxypp->plist->permissions_token);
  else
      memset(&permissions_token, 0, sizeof(DDS_Security_PermissionsToken));

  handshake = ddsi_handshake_find(pp, proxypp);
  if (!handshake)
  {
    GVTRACE("Could not find handshake local participant "PGUIDFMT" and remote participant "PGUIDFMT, PGUID(pp->e.guid), PGUID(proxypp->e.guid));
    goto no_handshake;
  }

  if (!sc->authentication_context->get_authenticated_peer_credential_token(sc->authentication_context, &peer_credential_token, ddsi_handshake_get_handle(handshake), &exception))
  {
    if (ddsi_omg_participant_is_access_protected(pp))
    {
      EXCEPTION_ERROR(gv, &exception, "Could not authenticate_peer_credential_token for local participan1152t "PGUIDFMT" and remote participant "PGUIDFMT,
          PGUID(pp->e.guid), PGUID(proxypp->e.guid));
      goto no_credentials;
    }
    /* Failing is allowed due to the non-protection of access. */
    EXCEPTION_WARNING(gv, &exception, "Could not authenticate_peer_credential_token for local participant "PGUIDFMT" and remote participant "PGUIDFMT ,
        PGUID(pp->e.guid), PGUID(proxypp->e.guid));
  }

  permissions_hdl = sc->access_control_context->validate_remote_permissions(
      sc->access_control_context, sc->authentication_context, pp->sec_attr->local_identity_handle, remote_identity_handle, &permissions_token, &peer_credential_token, &exception);
  if (permissions_hdl == DDS_SECURITY_HANDLE_NIL)
  {
    if (ddsi_omg_participant_is_access_protected(pp))
    {
      EXCEPTION_ERROR(gv, &exception, "Could not get remote participant "PGUIDFMT" permissions from plugin", PGUID(proxypp->e.guid));
      goto no_permissions;
    }
    /* Failing is allowed due to the non-protection of access. */
    EXCEPTION_WARNING(gv, &exception, "Could not get remote participant "PGUIDFMT" permissions from plugin", PGUID(proxypp->e.guid));
  }

  /* Only check remote participant if joining access is protected. */
  if (ddsi_omg_participant_is_access_protected(pp))
  {
    DDS_Security_ParticipantBuiltinTopicDataSecure participant_data;

    ddsi_omg_shallow_copy_ParticipantBuiltinTopicDataSecure (&participant_data, &(proxypp->e.guid), proxypp->plist);
    if (!sc->access_control_context->check_remote_participant(sc->access_control_context, permissions_hdl, (DDS_Security_DomainId)domain_id, &participant_data, &exception))
    {
      EXCEPTION_WARNING(gv, &exception, "Plugin does not allow remote participant "PGUIDFMT,  PGUID(proxypp->e.guid));
      if (!sc->access_control_context->return_permissions_handle(sc->access_control_context, permissions_hdl, &exception))
        EXCEPTION_ERROR(gv, &exception, "Failed to return remote permissions handle");
      permissions_hdl = DDS_SECURITY_HANDLE_NIL;
    }
    ddsi_omg_shallow_free_ParticipantBuiltinTopicDataSecure (&participant_data);
  }

no_permissions:
  if (!sc->authentication_context->return_authenticated_peer_credential_token(sc->authentication_context, &peer_credential_token, &exception))
    EXCEPTION_ERROR(gv, &exception, "Failed to return peer credential token");
no_credentials:
  ddsi_handshake_release(handshake);
no_handshake:
  ddsi_omg_shallow_free_DataHolder (&permissions_token);
  return permissions_hdl;
}

static void send_participant_crypto_tokens(struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp, DDS_Security_ParticipantCryptoHandle local_crypto, DDS_Security_ParticipantCryptoHandle remote_crypto)
{
  struct dds_security_context *sc = ddsi_omg_security_get_secure_context (pp);
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_ParticipantCryptoTokenSeq tokens = DDS_SECURITY_SEQUENCE_INIT;
  bool r;

  assert(sc);

  r = sc->crypto_context->crypto_key_exchange->create_local_participant_crypto_tokens(sc->crypto_context->crypto_key_exchange, &tokens, local_crypto, remote_crypto, &exception);
  if (!r)
    EXCEPTION_ERROR(pp->e.gv, &exception, "Failed to create local participant crypto tokens "PGUIDFMT" for remote participant "PGUIDFMT,  PGUID(pp->e.guid), PGUID(proxypp->e.guid));
  else if (tokens._length > 0)
  {
    ddsi_dataholderseq_t tholder;

    ddsi_omg_shallow_copyout_DataHolderSeq (&tholder, &tokens);
    ddsi_write_crypto_participant_tokens(pp, proxypp, &tholder);
    ddsi_omg_shallow_free_ddsi_dataholderseq (&tholder);

    if (!sc->crypto_context->crypto_key_exchange->return_crypto_tokens(sc->crypto_context->crypto_key_exchange, &tokens, &exception))
      EXCEPTION_ERROR(pp->e.gv, &exception, "Failed to return local participant crypto tokens "PGUIDFMT" for remote participant "PGUIDFMT, PGUID(pp->e.guid), PGUID(proxypp->e.guid));
  }
}

static int64_t get_permissions_handle(struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp)
{
  int64_t hdl = 0;
  struct ddsi_proxypp_pp_match *pm;

  ddsrt_mutex_lock(&proxypp->sec_attr->lock);
  pm = ddsrt_avl_lookup(&proxypp_pp_treedef, &proxypp->sec_attr->participants, &pp->sec_attr->crypto_handle);
  if (pm)
    hdl = pm->permissions_handle;
  ddsrt_mutex_unlock(&proxypp->sec_attr->lock);

  return hdl;
}

void ddsi_omg_security_init_remote_participant (struct ddsi_proxy_participant *proxypp)
{
  proxypp->sec_attr = ddsrt_malloc(sizeof(*proxypp->sec_attr));
  ddsrt_mutex_init(&proxypp->sec_attr->lock);
  ddsrt_avl_init (&proxypp_pp_treedef, &proxypp->sec_attr->participants);
  proxypp->sec_attr->sc = proxypp->e.gv->security_context;
  proxypp->sec_attr->remote_identity_handle = 0;
  proxypp->sec_attr->crypto_handle = 0;
  proxypp->sec_attr->initialized = false;
}

void ddsi_omg_security_remote_participant_set_initialized (struct ddsi_proxy_participant *proxypp)
{
  if (proxypp->sec_attr)
  {
    ddsrt_mutex_lock(&proxypp->sec_attr->lock);
    proxypp->sec_attr->initialized = true;
    ddsrt_mutex_unlock(&proxypp->sec_attr->lock);
  }
}

bool ddsi_omg_security_remote_participant_is_initialized (struct ddsi_proxy_participant *proxypp)
{
  bool initialized = false;

  if (proxypp->sec_attr)
  {
    ddsrt_mutex_lock(&proxypp->sec_attr->lock);
    initialized = proxypp->sec_attr->initialized;
    ddsrt_mutex_unlock(&proxypp->sec_attr->lock);
  }
  return initialized;
}

static bool proxypp_is_authenticated(const struct ddsi_proxy_participant *proxypp)
{
  bool authenticated = false;

  if (proxypp->sec_attr)
  {
    ddsrt_mutex_lock(&proxypp->sec_attr->lock);
    authenticated = !ddsrt_avl_is_empty(&proxypp->sec_attr->participants);
    ddsrt_mutex_unlock(&proxypp->sec_attr->lock);
  }
  return authenticated;
}

static void match_proxypp_pp(struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp, DDS_Security_PermissionsHandle permissions_handle, DDS_Security_SharedSecretHandle shared_secret_handle)
{
  struct ddsi_proxypp_pp_match *pm;
  struct ddsi_pp_proxypp_match *pc;

  pm = ddsi_proxypp_pp_match_new(pp, permissions_handle, shared_secret_handle);
  ddsrt_mutex_lock(&proxypp->sec_attr->lock);
  ddsrt_avl_insert(&proxypp_pp_treedef, &proxypp->sec_attr->participants, pm);
  ddsrt_mutex_unlock(&proxypp->sec_attr->lock);

  pc = ddsi_pp_proxypp_match_new(proxypp, proxypp->sec_attr->crypto_handle);

  ddsrt_mutex_lock(&pp->sec_attr->lock);
  ddsrt_avl_cinsert(&pp_proxypp_treedef, &pp->sec_attr->proxy_participants, pc);
  ddsrt_mutex_unlock(&pp->sec_attr->lock);
}

bool ddsi_omg_security_register_remote_participant (struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp, int64_t shared_secret)
{
  bool ret = true;
  struct ddsi_domaingv *gv = pp->e.gv;
  struct dds_security_context *sc = ddsi_omg_security_get_secure_context (pp);
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_ParticipantCryptoHandle crypto_handle;
  int64_t permissions_handle;
  bool notify_handshake = false;

  if (!sc)
    return false;

  permissions_handle = check_remote_participant_permissions(gv->config.domainId, pp, proxypp, proxypp->sec_attr->remote_identity_handle);
  if (permissions_handle == 0)
    return false;

  GVTRACE("register remote participant "PGUIDFMT" with "PGUIDFMT"\n", PGUID(proxypp->e.guid), PGUID(pp->e.guid));

  crypto_handle = sc->crypto_context->crypto_key_factory->register_matched_remote_participant(
      sc->crypto_context->crypto_key_factory, pp->sec_attr->crypto_handle, proxypp->sec_attr->remote_identity_handle, permissions_handle, shared_secret, &exception);
  if (crypto_handle == DDS_SECURITY_HANDLE_NIL)
  {
    EXCEPTION_ERROR(gv, &exception, "Failed to register matched remote participant "PGUIDFMT" with participant "PGUIDFMT, PGUID(proxypp->e.guid), PGUID(pp->e.guid));
    ret = false;
    goto register_failed;
  }

  ddsrt_mutex_lock(&pp->e.lock);

  proxypp->sec_attr->crypto_handle = crypto_handle;

  GVTRACE("match pp->crypto=%"PRId64" proxypp->crypto=%"PRId64" permissions=%"PRId64"\n", pp->sec_attr->crypto_handle, crypto_handle, permissions_handle);
  match_proxypp_pp(pp, proxypp, permissions_handle, shared_secret);

  GVTRACE(" create proxypp-pp match pp="PGUIDFMT" proxypp="PGUIDFMT" lidh=%"PRId64"\n", PGUID(pp->e.guid), PGUID(proxypp->e.guid), pp->sec_attr->local_identity_handle);

  if (proxypp_is_rtps_protected(proxypp))
  {
    struct pending_match *match = find_or_create_pending_entity_match(&sc->security_matches, DDSI_EK_PROXY_PARTICIPANT, &proxypp->e.guid, &pp->e.guid, crypto_handle, NULL);
    if (match->tokens)
    {
      ret = sc->crypto_context->crypto_key_exchange->set_remote_participant_crypto_tokens(sc->crypto_context->crypto_key_exchange, pp->sec_attr->crypto_handle, crypto_handle, match->tokens, &exception);
      if (!ret)
        EXCEPTION_ERROR(gv, &exception, " Failed to set remote participant crypto tokens "PGUIDFMT" --> "PGUIDFMT, PGUID(proxypp->e.guid), PGUID(pp->e.guid));
      else
        GVTRACE(" set participant tokens src("PGUIDFMT") to dst("PGUIDFMT") (by registering remote)\n", PGUID(proxypp->e.guid), PGUID(pp->e.guid));
      delete_pending_match(&sc->security_matches, match);
    }
    else
      notify_handshake = true;
  }
  ddsrt_mutex_unlock(&pp->e.lock);

  if (notify_handshake)
    notify_handshake_recv_token(pp, proxypp);

register_failed:
  return ret;
}

void ddsi_omg_security_set_remote_participant_authenticated (struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp)
{
  struct ddsi_proxypp_pp_match *pm;

  ddsrt_mutex_lock(&proxypp->sec_attr->lock);
  pm = ddsrt_avl_lookup(&proxypp_pp_treedef, &proxypp->sec_attr->participants, &pp->sec_attr->crypto_handle);
  if (pm)
    pm->authenticated = true;
  ddsrt_mutex_unlock(&proxypp->sec_attr->lock);
}

static bool is_volatile_secure_endpoint(ddsi_entityid_t entityid)
{
  return ((entityid.u == DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER) || (entityid.u == DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER));
}

static struct ddsi_proxypp_pp_match * get_ddsi_pp_proxypp_match_if_authenticated(struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp, ddsi_entityid_t entityid)
{
  struct ddsi_proxypp_pp_match *pm;

  ddsrt_mutex_lock(&proxypp->sec_attr->lock);
  pm = ddsrt_avl_lookup(&proxypp_pp_treedef, &proxypp->sec_attr->participants, &pp->sec_attr->crypto_handle);
  if (pm)
  {
    if (!pm->authenticated && !is_volatile_secure_endpoint(entityid))
      pm = NULL;
  }
  ddsrt_mutex_unlock(&proxypp->sec_attr->lock);
  return pm;
}

void ddsi_omg_security_deregister_remote_participant (struct ddsi_proxy_participant *proxypp)
{
  struct ddsi_domaingv *gv = proxypp->e.gv;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;

  if (proxypp->sec_attr)
  {
    dds_security_context *sc = proxypp->sec_attr->sc;
    struct ddsi_proxypp_pp_match *pm;
    struct ddsi_participant *pp;

    pm = ddsrt_avl_find_min(&proxypp_pp_treedef, &proxypp->sec_attr->participants);
    while (pm)
    {
      struct ddsi_proxypp_pp_match *next = ddsrt_avl_find_succ(&proxypp_pp_treedef, &proxypp->sec_attr->participants, pm);
      ddsrt_avl_delete(&proxypp_pp_treedef, &proxypp->sec_attr->participants, pm);
      if ((pp = ddsi_entidx_lookup_participant_guid(gv->entity_index, &pm->pp_guid)) != NULL)
        pp_proxypp_unrelate(sc, pp, &proxypp->e.guid);
      ddsi_proxypp_pp_match_free(gv, sc, pm);
      pm = next;
    }

    clear_pending_matches_by_remote_guid(sc, &sc->security_matches, &proxypp->e.guid);

    if (proxypp->sec_attr->crypto_handle != DDS_SECURITY_HANDLE_NIL)
    {
      if (!sc->crypto_context->crypto_key_factory->unregister_participant(sc->crypto_context->crypto_key_factory, proxypp->sec_attr->crypto_handle, &exception))
        EXCEPTION_ERROR(gv, &exception, "2:Failed to return remote crypto handle");
    }

    if (proxypp->sec_attr->remote_identity_handle != DDS_SECURITY_HANDLE_NIL)
    {
      if (!sc->authentication_context->return_identity_handle(sc->authentication_context, proxypp->sec_attr->remote_identity_handle, &exception))
        EXCEPTION_ERROR(gv, &exception, "Failed to return remote identity handle");
    }

    ddsrt_mutex_destroy(&proxypp->sec_attr->lock);
    ddsrt_free(proxypp->sec_attr);
    proxypp->sec_attr = NULL;
  }
}

bool ddsi_is_proxy_participant_deletion_allowed (struct ddsi_domaingv * const gv, const struct ddsi_guid *guid, const ddsi_entityid_t pwr_entityid)
{
  struct ddsi_proxy_participant *proxypp;

  assert (gv);
  assert (guid);

  /* TODO: Check if the proxy writer guid prefix matches that of the proxy
   *       participant. Deletion is not allowed when they're not equal. */

  /* Always allow deletion from a secure proxy writer. */
  if (pwr_entityid.u == DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER)
    return true;

  /* Not from a secure proxy writer.
   * Only allow deletion when proxy participant is not authenticated. */
  proxypp = ddsi_entidx_lookup_proxy_participant_guid (gv->entity_index, guid);
  if (!proxypp)
  {
    GVLOGDISC (" unknown");
    return false;
  }

  return (!proxypp_is_authenticated(proxypp));
}

bool ddsi_omg_is_similar_participant_security_info (struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp)
{
  bool matching;
  ddsi_security_info_t pp_security_info;

  if (!ddsi_omg_get_participant_security_info (pp, &pp_security_info))
    return false;

  matching = SECURITY_INFO_COMPATIBLE(pp_security_info, proxypp->security_info, DDSI_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_VALID);
  if (!matching) {
    DDS_CLOG (DDS_LC_WARNING, &pp->e.gv->logconfig, "match remote_participant "PGUIDFMT" with participant "PGUIDFMT" security_attributes mismatch: 0x%08x.0x%08x - 0x%08x.0x%08x\n",
        PGUID(proxypp->e.guid), PGUID(pp->e.guid),
        proxypp->security_info.security_attributes, proxypp->security_info.plugin_security_attributes,
        pp_security_info.security_attributes, pp_security_info.plugin_security_attributes);
  } else {
    /* We previously checked for attribute compatibility. That doesn't
     * mean equal, because compatibility depends on the valid flag.
     * Some products don't properly send the attributes, in which case
     * the valid flag is 0. To be able to support these product, assume
     * that the attributes are the same. If there is actually a mismatch,
     * communication will fail at a later moment anyway. */
    if (!SECURITY_ATTR_IS_VALID(proxypp->security_info.security_attributes)) {
      proxypp->security_info.security_attributes = pp_security_info.security_attributes;
    }
    if (!SECURITY_ATTR_IS_VALID(proxypp->security_info.plugin_security_attributes)) {
      proxypp->security_info.plugin_security_attributes = pp_security_info.plugin_security_attributes;
    }
  }
  return matching;
}

void ddsi_omg_security_set_participant_crypto_tokens (struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp, const ddsi_dataholderseq_t *tokens)
{
  struct ddsi_domaingv *gv = pp->e.gv;
  struct dds_security_context *sc = ddsi_omg_security_get_secure_context (pp);
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  struct ddsi_proxypp_pp_match *pm;
  DDS_Security_DatawriterCryptoTokenSeq *tseq;

  if (!sc)
    return;

  tseq = DDS_Security_DataHolderSeq_alloc();
  ddsi_omg_copyin_DataHolderSeq (tseq, tokens);

  ddsrt_mutex_lock(&proxypp->sec_attr->lock);
  pm = ddsrt_avl_lookup (&proxypp_pp_treedef, &proxypp->sec_attr->participants, &pp->sec_attr->crypto_handle);
  ddsrt_mutex_unlock(&proxypp->sec_attr->lock);

  ddsrt_mutex_lock(&pp->e.lock);

  if (!pm)
  {
    GVTRACE("remember participant tokens src("PGUIDFMT") dst("PGUIDFMT")\n", PGUID(proxypp->e.guid), PGUID(pp->e.guid));
    (void)find_or_create_pending_entity_match(&sc->security_matches, DDSI_EK_PROXY_PARTICIPANT, &proxypp->e.guid, &pp->e.guid, 0, tseq);
  }
  else
  {
    if (sc->crypto_context->crypto_key_exchange->set_remote_participant_crypto_tokens(sc->crypto_context->crypto_key_exchange, pp->sec_attr->crypto_handle, proxypp->sec_attr->crypto_handle, tseq, &exception))
    {
      GVTRACE(" set participant tokens src("PGUIDFMT") dst("PGUIDFMT")\n", PGUID(proxypp->e.guid), PGUID(pp->e.guid));
      DDS_Security_DataHolderSeq_free(tseq);
    }
    else
      EXCEPTION_ERROR(gv, &exception, " Failed to set remote participant crypto tokens "PGUIDFMT" for participant "PGUIDFMT, PGUID(proxypp->e.guid), PGUID(pp->e.guid));
  }
  ddsrt_mutex_unlock(&pp->e.lock);

  notify_handshake_recv_token(pp, proxypp);
}

void ddsi_omg_security_participant_send_tokens (struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp)
{
  if (proxypp->sec_attr->crypto_handle != 0)
    send_participant_crypto_tokens(pp, proxypp, pp->sec_attr->crypto_handle, proxypp->sec_attr->crypto_handle);
}

int64_t ddsi_omg_security_get_remote_participant_handle (struct ddsi_proxy_participant *proxypp)
{
  if (proxypp->sec_attr)
    return proxypp->sec_attr->crypto_handle;

  return 0;
}

void ddsi_set_proxy_participant_security_info(struct ddsi_proxy_participant *proxypp, const ddsi_plist_t *plist)
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

bool ddsi_omg_writer_is_discovery_protected (const struct ddsi_writer *wr)
{
  assert (wr != NULL);
  return wr->sec_attr != NULL && wr->sec_attr->attr.is_discovery_protected;
}

bool ddsi_omg_writer_is_submessage_protected (const struct ddsi_writer *wr)
{
  assert (wr != NULL);
  return wr->sec_attr != NULL && wr->sec_attr->attr.is_submessage_protected;
}

bool ddsi_omg_writer_is_payload_protected (const struct ddsi_writer *wr)
{
  assert (wr != NULL);
  return wr->sec_attr != NULL && wr->sec_attr->attr.is_payload_protected;
}

bool ddsi_omg_security_check_remote_writer_permissions (const struct ddsi_proxy_writer *pwr, uint32_t domain_id, struct ddsi_participant *pp)
{
  struct ddsi_domaingv *gv = pp->e.gv;
  struct dds_security_context *sc = ddsi_omg_security_get_secure_context (pp);
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_PublicationBuiltinTopicDataSecure publication_data;
  DDS_Security_TopicBuiltinTopicData topic_data;

  if (!sc)
    return true;

  if (!ddsi_omg_proxy_participant_is_secure (pwr->c.proxypp))
  {
    if (ddsi_omg_participant_allow_unauthenticated(pp))
    {
      GVTRACE(" allow non-secure remote writer "PGUIDFMT, PGUID(pwr->e.guid));
      return true;
    }
    else
    {
      GVWARNING("Non secure remote writer "PGUIDFMT" is not allowed.", PGUID(pwr->e.guid));
      return false;
    }
  }

  if (!SECURITY_INFO_IS_WRITE_PROTECTED(pwr->c.security_info))
    return true;

  DDS_Security_PermissionsHandle permissions_handle;
  if ((permissions_handle = get_permissions_handle(pp, pwr->c.proxypp)) == 0)
  {
    GVTRACE("Secure remote writer "PGUIDFMT" proxypp does not have permissions handle yet\n", PGUID(pwr->e.guid));
    return false;
  }

  ddsi_omg_shallow_copy_PublicationBuiltinTopicDataSecure (&publication_data, &pwr->e.guid, pwr->c.xqos, &pwr->c.security_info);
  bool result = sc->access_control_context->check_remote_datawriter(sc->access_control_context, permissions_handle, (int)domain_id, &publication_data, &exception);
  if (!result)
  {
    handle_not_allowed(gv, pp->sec_attr->permissions_handle, sc->access_control_context, &exception, publication_data.topic_name,
      "Access control does not allow remote writer "PGUIDFMT, PGUID(pwr->e.guid));
  }
  else
  {
    ddsi_omg_shallow_copy_TopicBuiltinTopicData (&topic_data, publication_data.topic_name, publication_data.type_name);
    result = sc->access_control_context->check_remote_topic(sc->access_control_context, permissions_handle, (int)domain_id, &topic_data, &exception);
    ddsi_omg_shallow_free_TopicBuiltinTopicData (&topic_data);
    if (!result)
      handle_not_allowed(gv, pp->sec_attr->permissions_handle, sc->access_control_context, &exception, publication_data.topic_name,
        "Access control does not allow remote topic %s", publication_data.topic_name);
  }
  ddsi_omg_shallow_free_PublicationBuiltinTopicDataSecure (&publication_data);

  return result;
}

static void send_reader_crypto_tokens(struct ddsi_reader *rd, struct ddsi_proxy_writer *pwr, DDS_Security_DatareaderCryptoHandle local_crypto, DDS_Security_DatawriterCryptoHandle remote_crypto)
{
  struct dds_security_context *sc = ddsi_omg_security_get_secure_context (rd->c.pp);
  struct ddsi_domaingv *gv = rd->e.gv;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_DatawriterCryptoTokenSeq tokens = {0, 0, NULL};
  bool r;

  GVTRACE("send reader tokens "PGUIDFMT" to writer "PGUIDFMT"\n", PGUID(rd->e.guid), PGUID(pwr->e.guid));
  assert(sc);
  r = sc->crypto_context->crypto_key_exchange->create_local_datareader_crypto_tokens(sc->crypto_context->crypto_key_exchange, &tokens, local_crypto, remote_crypto, &exception);
  if (!r)
    EXCEPTION_ERROR(gv, &exception,"Failed to create local reader crypto tokens "PGUIDFMT" for remote writer "PGUIDFMT, PGUID(rd->e.guid), PGUID(pwr->e.guid));
  else if (tokens._length > 0)
  {
    ddsi_dataholderseq_t tholder;

    ddsi_omg_shallow_copyout_DataHolderSeq (&tholder, &tokens);
    ddsi_write_crypto_reader_tokens(rd, pwr, &tholder);
    ddsi_omg_shallow_free_ddsi_dataholderseq (&tholder);

    if (!sc->crypto_context->crypto_key_exchange->return_crypto_tokens(sc->crypto_context->crypto_key_exchange, &tokens, &exception))
      EXCEPTION_ERROR(gv, &exception, "Failed to return local reader crypto tokens "PGUIDFMT" for remote writer "PGUIDFMT, PGUID(rd->e.guid), PGUID(pwr->e.guid));
  }
}

static bool ddsi_omg_security_register_remote_writer_match (struct ddsi_proxy_writer *pwr, struct ddsi_reader *rd, int64_t *crypto_handle)
{
  struct ddsi_participant *pp = rd->c.pp;
  struct ddsi_proxy_participant *proxypp = pwr->c.proxypp;
  struct ddsi_domaingv *gv = pp->e.gv;
  struct dds_security_context *sc = ddsi_omg_security_get_secure_context (pp);
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  struct ddsi_proxypp_pp_match *proxypp_match;
  bool send_tokens = false;
  bool allowed = false;

  if (!sc)
    return false;

  if ((proxypp_match = get_ddsi_pp_proxypp_match_if_authenticated(pp, proxypp, pwr->e.guid.entityid)) == NULL)
    return false;

  ddsrt_mutex_lock(&rd->e.lock);
  if (ddsrt_avl_lookup (&ddsi_rd_writers_treedef, &rd->writers, &pwr->e.guid) != NULL)
    allowed = true;
  else if (rd->e.guid.entityid.u == DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER)
  {
    /* The builtin ParticipantVolatileSecure endpoints do not exchange tokens.
     * Simulate that we already got them. */

    *crypto_handle = sc->crypto_context->crypto_key_factory->register_matched_remote_datawriter(
        sc->crypto_context->crypto_key_factory, rd->sec_attr->crypto_handle, proxypp->sec_attr->crypto_handle, proxypp_match->shared_secret, &exception);
    if (*crypto_handle != 0)
    {
      GVTRACE(" volatile secure reader: proxypp_crypto=%"PRId64" rd_crypto=%"PRId64" pwr_crypto=%"PRId64"\n", proxypp->sec_attr->crypto_handle, rd->sec_attr->crypto_handle, *crypto_handle);
      allowed = true;
    }
    else
      EXCEPTION_ERROR(gv, &exception, "Failed to register remote writer "PGUIDFMT" with reader "PGUIDFMT, PGUID(pwr->e.guid), PGUID(rd->e.guid));
  }
  else
  {
    struct pending_match *pending_match = find_or_create_pending_entity_match(&sc->security_matches, DDSI_EK_PROXY_WRITER, &pwr->e.guid, &rd->e.guid, 0, NULL);

    /* Generate writer crypto info. */
    if (pending_match->crypto_handle == 0)
    {
      *crypto_handle = sc->crypto_context->crypto_key_factory->register_matched_remote_datawriter(
          sc->crypto_context->crypto_key_factory, rd->sec_attr->crypto_handle, proxypp->sec_attr->crypto_handle, proxypp_match->shared_secret, &exception);
      if (*crypto_handle == 0)
        EXCEPTION_ERROR(gv, &exception, "Failed to register remote writer "PGUIDFMT" with reader "PGUIDFMT, PGUID(pwr->e.guid), PGUID(rd->e.guid));
      else
      {
        pending_match->crypto_handle = *crypto_handle;
        send_tokens = true;
        if (pending_match->tokens)
        {
          if (!sc->crypto_context->crypto_key_exchange->set_remote_datawriter_crypto_tokens(
              sc->crypto_context->crypto_key_exchange, rd->sec_attr->crypto_handle, *crypto_handle, pending_match->tokens, &exception))
            EXCEPTION_ERROR(gv, &exception, "Failed to set remote writer crypto tokens "PGUIDFMT" --> "PGUIDFMT, PGUID(pwr->e.guid), PGUID(rd->e.guid));
          else
          {
            GVTRACE("match_remote_writer "PGUIDFMT" with reader "PGUIDFMT": tokens available\n", PGUID(pwr->e.guid), PGUID(rd->e.guid));
            allowed = true;
          }
          delete_pending_match(&sc->security_matches, pending_match);
        }
      }
    }
  }
  ddsrt_mutex_unlock(&rd->e.lock);

  if (send_tokens)
    (void)send_reader_crypto_tokens(rd, pwr, rd->sec_attr->crypto_handle, *crypto_handle);

  return allowed;
}

bool ddsi_omg_security_match_remote_writer_enabled (struct ddsi_reader *rd, struct ddsi_proxy_writer *pwr, int64_t *crypto_handle)
{
  struct ddsi_domaingv *gv = rd->e.gv;
  ddsi_security_info_t info;

  *crypto_handle = 0;

  if (!rd->sec_attr)
    return true;

  /*
   * Check if the security settings match by checking the attributes.
   *
   * The attributes will be 0 when security is not enabled for the related
   * federation or the security configuration told that this endpoint should
   * not be protected.
   *
   * This can mean that an unprotected endpoint of a secure federation can
   * connect to an endpoint of a non-secure federation. However, that will
   * be blocked by ddsi_omg_security_check_remote_writer_permissions () if
   * ddsi_omg_participant_allow_unauthenticated() returns FALSE there.
   */
  (void)ddsi_omg_get_reader_security_info (rd, &info);
  if (!SECURITY_INFO_COMPATIBLE(pwr->c.security_info, info, DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID))
  {
    GVWARNING("match_remote_writer "PGUIDFMT" with reader "PGUIDFMT" security_attributes mismatch: 0x%08x.0x%08x - 0x%08x.0x%08x\n",
                PGUID(pwr->e.guid), PGUID(rd->e.guid),
                pwr->c.security_info.security_attributes, pwr->c.security_info.plugin_security_attributes,
                info.security_attributes, info.plugin_security_attributes);
    return false;
  }

  if ((!rd->sec_attr->attr.is_payload_protected ) && (!rd->sec_attr->attr.is_submessage_protected))
    return true;

  if (!ddsi_omg_proxy_participant_is_secure (pwr->c.proxypp))
  {
    /* Remote proxy was downgraded to a non-secure participant,
     * but the local endpoint is protected. */
    return false;
  }

  /* We previously checked for attribute compatibility. That doesn't
   * mean equal, because compatibility depends on the valid flag.
   * Some products don't properly send the attributes, in which case
   * the valid flag is 0. To be able to support these product, assume
   * that the attributes are the same. If there is actually a mismatch,
   * communication will fail at a later moment anyway. */
  if (!SECURITY_ATTR_IS_VALID(pwr->c.security_info.security_attributes)) {
    pwr->c.security_info.security_attributes = info.security_attributes;
  }
  if (!SECURITY_ATTR_IS_VALID(pwr->c.security_info.plugin_security_attributes)) {
    pwr->c.security_info.plugin_security_attributes = info.plugin_security_attributes;
  }

  return ddsi_omg_security_register_remote_writer_match (pwr, rd, crypto_handle);
}

void ddsi_omg_security_deregister_remote_writer_match (const struct ddsi_domaingv *gv, const ddsi_guid_t *rd_guid, struct ddsi_rd_pwr_match *m)
{
  struct dds_security_context *sc = gv->security_context;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;

  if (m->crypto_handle != 0)
  {
    assert(sc);
    if (!sc->crypto_context->crypto_key_factory->unregister_datawriter(sc->crypto_context->crypto_key_factory, m->crypto_handle, &exception))
      EXCEPTION_ERROR(gv, &exception, "Failed to unregister remote writer "PGUIDFMT" for reader "PGUIDFMT, PGUID(m->pwr_guid), PGUID(*rd_guid));
  }
}

void ddsi_omg_security_deregister_remote_writer (const struct ddsi_proxy_writer *pwr)
{
  struct ddsi_domaingv *gv = pwr->e.gv;
  struct dds_security_context *sc = gv->security_context;

  if (ddsi_omg_proxy_participant_is_secure (pwr->c.proxypp))
  {
    assert(sc);
    clear_pending_matches_by_remote_guid(sc, &sc->security_matches, &pwr->e.guid);
  }
}

bool ddsi_omg_security_check_remote_reader_permissions (const struct ddsi_proxy_reader *prd, uint32_t domain_id, struct ddsi_participant *pp, bool *relay_only)
{
  struct ddsi_domaingv *gv = pp->e.gv;
  struct dds_security_context *sc = ddsi_omg_security_get_secure_context (pp);
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_SubscriptionBuiltinTopicDataSecure subscription_data;
  DDS_Security_TopicBuiltinTopicData topic_data;
  DDS_Security_boolean sec_relay_only;

  /* relay_only is meaningless in all cases except the one where the access control plugin says otherwise */
  *relay_only = false;

  if (!sc)
    return true;

  if (!ddsi_omg_proxy_participant_is_secure (prd->c.proxypp))
  {
    if (ddsi_omg_participant_allow_unauthenticated(pp))
    {
      GVTRACE(" allow non-secure remote reader "PGUIDFMT, PGUID(prd->e.guid));
      return true;
    }
    else
    {
      GVWARNING("Non secure remote reader "PGUIDFMT" is not allowed.", PGUID(prd->e.guid));
      return false;
    }
  }

  if (!SECURITY_INFO_IS_READ_PROTECTED(prd->c.security_info))
    return true;

  DDS_Security_PermissionsHandle permissions_handle;
  if ((permissions_handle = get_permissions_handle(pp, prd->c.proxypp)) == 0)
  {
    GVTRACE("Secure remote reader "PGUIDFMT" proxypp does not have permissions handle yet\n", PGUID(prd->e.guid));
    return false;
  }

  ddsi_omg_shallow_copy_SubscriptionBuiltinTopicDataSecure (&subscription_data, &prd->e.guid, prd->c.xqos, &prd->c.security_info);
  bool result = sc->access_control_context->check_remote_datareader(sc->access_control_context, permissions_handle, (int)domain_id, &subscription_data, &sec_relay_only, &exception);
  if (!result)
  {
    handle_not_allowed(gv, pp->sec_attr->permissions_handle, sc->access_control_context, &exception, subscription_data.topic_name,
      "Access control does not allow remote reader "PGUIDFMT, PGUID(prd->e.guid));
  }
  else
  {
    *relay_only = !!sec_relay_only;
    ddsi_omg_shallow_copy_TopicBuiltinTopicData (&topic_data, subscription_data.topic_name, subscription_data.type_name);
    result = sc->access_control_context->check_remote_topic(sc->access_control_context, permissions_handle, (int)domain_id, &topic_data, &exception);
    ddsi_omg_shallow_free_TopicBuiltinTopicData (&topic_data);
    if (!result)
      handle_not_allowed(gv, pp->sec_attr->permissions_handle, sc->access_control_context, &exception, subscription_data.topic_name,
        "Access control does not allow remote topic %s", subscription_data.topic_name);
  }
  ddsi_omg_shallow_free_SubscriptionBuiltinTopicDataSecure (&subscription_data);

  return result;
}

void ddsi_omg_get_proxy_endpoint_security_info (const struct ddsi_entity_common *entity, ddsi_security_info_t *proxypp_sec_info, const ddsi_plist_t *plist, ddsi_security_info_t *info)
{
  const bool proxypp_info_available =
    (proxypp_sec_info->security_attributes != 0 || proxypp_sec_info->plugin_security_attributes != 0);

  info->security_attributes = 0;
  info->plugin_security_attributes = 0;

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
  else if (endpoint_is_DCPSParticipantSecure (&entity->guid)||
           endpoint_is_DCPSPublicationsSecure (&entity->guid) ||
           endpoint_is_DCPSSubscriptionsSecure (&entity->guid))
  {
    /* Discovery protection flags */
    info->plugin_security_attributes = DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    info->security_attributes = DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    if (proxypp_info_available)
    {
      if (proxypp_sec_info->security_attributes & DDSI_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_DISCOVERY_PROTECTED)
        info->security_attributes |= DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;
      if (proxypp_sec_info->plugin_security_attributes & DDSI_PLUGIN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_DISCOVERY_ENCRYPTED)
        info->plugin_security_attributes |= DDSI_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
      if (proxypp_sec_info->plugin_security_attributes & DDSI_PLUGIN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_DISCOVERY_AUTHENTICATED)
        info->plugin_security_attributes |= DDSI_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ORIGIN_AUTHENTICATED;
    }
    else
    {
      /* No participant info: assume hardcoded OpenSplice V6.10.0 values. */
      info->security_attributes |= DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;
      info->plugin_security_attributes |= DDSI_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
    }
  }
  else if (endpoint_is_DCPSParticipantMessageSecure (&entity->guid))
  {
    /* Liveliness protection flags */
    info->plugin_security_attributes = DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    info->security_attributes = DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    if (proxypp_info_available)
    {
      if (proxypp_sec_info->security_attributes & DDSI_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_LIVELINESS_PROTECTED)
        info->security_attributes |= DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;
      if (proxypp_sec_info->plugin_security_attributes & DDSI_PLUGIN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_LIVELINESS_ENCRYPTED)
        info->plugin_security_attributes |= DDSI_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
      if (proxypp_sec_info->plugin_security_attributes & DDSI_PLUGIN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_LIVELINESS_AUTHENTICATED)
        info->plugin_security_attributes |= DDSI_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ORIGIN_AUTHENTICATED;
    }
    else
    {
      /* No participant info: assume hardcoded OpenSplice V6.10.0 values. */
      info->security_attributes |= DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;
      info->plugin_security_attributes |= DDSI_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
    }
  }
  else if (endpoint_is_DCPSParticipantStatelessMessage (&entity->guid))
  {
    info->security_attributes = DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    info->plugin_security_attributes = 0;
  }
  else if (endpoint_is_DCPSParticipantVolatileMessageSecure (&entity->guid))
  {
    info->security_attributes =
      DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID | DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;
    info->plugin_security_attributes = 0;
  }
}

void ddsi_omg_security_deregister_remote_reader_match (const struct ddsi_domaingv *gv, const ddsi_guid_t *wr_guid, struct ddsi_wr_prd_match *m)
{
  struct dds_security_context *sc = gv->security_context;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;

  if (m->crypto_handle != 0)
  {
    assert(sc);
    if (!sc->crypto_context->crypto_key_factory->unregister_datareader(sc->crypto_context->crypto_key_factory, m->crypto_handle, &exception))
      EXCEPTION_ERROR(gv, &exception, "Failed to unregister remote reader "PGUIDFMT" for writer "PGUIDFMT, PGUID(m->prd_guid), PGUID(*wr_guid));
  }
}

void ddsi_omg_security_deregister_remote_reader (const struct ddsi_proxy_reader *prd)
{
  struct ddsi_domaingv *gv = prd->e.gv;
  struct dds_security_context *sc = gv->security_context;

  if (ddsi_omg_proxy_participant_is_secure (prd->c.proxypp))
  {
    assert(sc);
    clear_pending_matches_by_remote_guid(sc, &sc->security_matches, &prd->e.guid);
  }
}

static void send_writer_crypto_tokens(struct ddsi_writer *wr, struct ddsi_proxy_reader *prd, DDS_Security_DatawriterCryptoHandle local_crypto, DDS_Security_DatareaderCryptoHandle remote_crypto)
{
  struct dds_security_context *sc = ddsi_omg_security_get_secure_context (wr->c.pp);
  struct ddsi_domaingv *gv = wr->e.gv;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_DatawriterCryptoTokenSeq tokens = {0, 0, NULL};
  bool r;

  GVTRACE("send writer tokens "PGUIDFMT" to reader "PGUIDFMT"\n", PGUID(wr->e.guid), PGUID(prd->e.guid));
  assert(sc);
  r = sc->crypto_context->crypto_key_exchange->create_local_datawriter_crypto_tokens(sc->crypto_context->crypto_key_exchange, &tokens, local_crypto, remote_crypto, &exception);
  if (!r)
    EXCEPTION_ERROR(gv, &exception,"Failed to create local writer crypto tokens "PGUIDFMT" for remote reader "PGUIDFMT, PGUID(wr->e.guid), PGUID(prd->e.guid));
  else if (tokens._length > 0)
  {
    ddsi_dataholderseq_t tholder;

    ddsi_omg_shallow_copyout_DataHolderSeq (&tholder, &tokens);
    ddsi_write_crypto_writer_tokens(wr, prd, &tholder);
    ddsi_omg_shallow_free_ddsi_dataholderseq (&tholder);

    if (!sc->crypto_context->crypto_key_exchange->return_crypto_tokens(sc->crypto_context->crypto_key_exchange, &tokens, &exception))
      EXCEPTION_ERROR(gv, &exception, "Failed to return local writer crypto tokens "PGUIDFMT" for remote reader "PGUIDFMT, PGUID(wr->e.guid), PGUID(prd->e.guid));
  }
}

static bool ddsi_omg_security_register_remote_reader_match (struct ddsi_proxy_reader *prd, struct ddsi_writer *wr, int64_t *crypto_handle, bool relay_only)
{
  struct ddsi_participant *pp = wr->c.pp;
  struct ddsi_proxy_participant *proxypp = prd->c.proxypp;
  struct ddsi_domaingv *gv = pp->e.gv;
  struct dds_security_context *sc = ddsi_omg_security_get_secure_context (pp);
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  struct ddsi_proxypp_pp_match *proxypp_match;
  bool send_tokens = false;
  bool allowed = false;

  if ((proxypp_match = get_ddsi_pp_proxypp_match_if_authenticated(pp, proxypp, prd->e.guid.entityid)) == NULL)
    return false;

  ddsrt_mutex_lock(&wr->e.lock);
  if (ddsrt_avl_lookup (&ddsi_wr_readers_treedef, &wr->readers, &prd->e.guid) != NULL)
    allowed = true;
  else if (wr->e.guid.entityid.u == DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER || !wr->sec_attr->attr.is_submessage_protected)
  {
    /* The builtin ParticipantVolatileSecure endpoints do not exchange tokens.
     * Simulate that we already got them. */
    assert(sc);
    *crypto_handle = sc->crypto_context->crypto_key_factory->register_matched_remote_datareader(
        sc->crypto_context->crypto_key_factory, wr->sec_attr->crypto_handle, proxypp->sec_attr->crypto_handle, proxypp_match->shared_secret, relay_only, &exception);
    if (*crypto_handle != 0)
    {
      GVTRACE(" match_remote_reader: proxypp_crypto=%"PRId64" wr_crypto=%"PRId64" prd_crypto=%"PRId64"\n", proxypp->sec_attr->crypto_handle, wr->sec_attr->crypto_handle, *crypto_handle);
      send_tokens = (wr->e.guid.entityid.u != DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER);
      allowed = true;
    }
    else
      EXCEPTION_ERROR(gv, &exception, "Failed to register remote reader "PGUIDFMT" with writer "PGUIDFMT, PGUID(prd->e.guid), PGUID(wr->e.guid));
  }
  else
  {
    struct pending_match *pending_match = find_or_create_pending_entity_match(&sc->security_matches, DDSI_EK_PROXY_READER, &prd->e.guid, &wr->e.guid, 0, NULL);

    /* Generate writer crypto info. */
    if (pending_match->crypto_handle == 0)
    {
      assert(sc);
      *crypto_handle = sc->crypto_context->crypto_key_factory->register_matched_remote_datareader(
          sc->crypto_context->crypto_key_factory, wr->sec_attr->crypto_handle, proxypp->sec_attr->crypto_handle, proxypp_match->shared_secret, relay_only, &exception);
      if (*crypto_handle == 0)
        EXCEPTION_ERROR(gv, &exception, "Failed to register remote reader "PGUIDFMT" with writer "PGUIDFMT, PGUID(prd->e.guid), PGUID(wr->e.guid));
      else
      {
        pending_match->crypto_handle = *crypto_handle;
        send_tokens = true;
        GVTRACE(" register_remote_reader_match: proxypp_crypto=%"PRId64" wr_crypto=%"PRId64" prd_crypto=%"PRId64"\n", proxypp->sec_attr->crypto_handle, wr->sec_attr->crypto_handle, *crypto_handle);
        if (pending_match->tokens)
        {
          if (!sc->crypto_context->crypto_key_exchange->set_remote_datareader_crypto_tokens(
              sc->crypto_context->crypto_key_exchange, wr->sec_attr->crypto_handle, *crypto_handle, pending_match->tokens, &exception))
            EXCEPTION_ERROR(gv, &exception, "Failed to set remote reader crypto tokens "PGUIDFMT" --> "PGUIDFMT, PGUID(prd->e.guid), PGUID(wr->e.guid));
          else
          {
            GVTRACE(" match_remote_reader "PGUIDFMT" with writer "PGUIDFMT": tokens available\n", PGUID(prd->e.guid), PGUID(wr->e.guid));
            allowed = true;
          }
          delete_pending_match(&sc->security_matches, pending_match);
        }
      }
    }
  }
  ddsrt_mutex_unlock(&wr->e.lock);

  if (send_tokens)
    (void)send_writer_crypto_tokens(wr, prd, wr->sec_attr->crypto_handle, *crypto_handle);

  return allowed;
}

bool ddsi_omg_security_match_remote_reader_enabled (struct ddsi_writer *wr, struct ddsi_proxy_reader *prd, bool relay_only, int64_t *crypto_handle)
{
  struct ddsi_domaingv *gv = wr->e.gv;
  ddsi_security_info_t info;

  *crypto_handle = 0;

  if (!wr->sec_attr)
    return true;

  /*
   * Check if the security settings match by checking the attributes.
   *
   * The attributes will be 0 when security is not enabled for the related
   * federation or the security configuration told that this endpoint should
   * not be protected.
   *
   * This can mean that an unprotected endpoint of a secure federation can
   * connect to an endpoint of a non-secure federation. However, that will
   * be blocked by ddsi_omg_security_check_remote_reader_permissions () if
   * ddsi_omg_participant_allow_unauthenticated() returns FALSE there.
   */
  (void)ddsi_omg_get_writer_security_info (wr, &info);
  if (!SECURITY_INFO_COMPATIBLE(prd->c.security_info, info, DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID))
  {
    GVWARNING("match_remote_reader "PGUIDFMT" with writer "PGUIDFMT" security_attributes mismatch: 0x%08x.0x%08x - 0x%08x.0x%08x\n",
        PGUID(prd->e.guid), PGUID(wr->e.guid),
        prd->c.security_info.security_attributes, prd->c.security_info.plugin_security_attributes,
        info.security_attributes, info.plugin_security_attributes);
    return false;
  }

  if (!wr->sec_attr->attr.is_submessage_protected  && !wr->sec_attr->attr.is_payload_protected)
    return true;

  if (!ddsi_omg_proxy_participant_is_secure (prd->c.proxypp))
  {
    /* Remote proxy was downgraded to a non-secure participant,
     * but the local endpoint is protected. */
    return false;
  }

  /* We previously checked for attribute compatibility. That doesn't
   * mean equal, because compatibility depends on the valid flag.
   * Some products don't properly send the attributes, in which case
   * the valid flag is 0. To be able to support these product, assume
   * that the attributes are the same. If there is actually a mismatch,
   * communication will fail at a later moment anyway. */
  if (!SECURITY_ATTR_IS_VALID(prd->c.security_info.security_attributes)) {
    prd->c.security_info.security_attributes = info.security_attributes;
  }
  if (!SECURITY_ATTR_IS_VALID(prd->c.security_info.plugin_security_attributes)) {
    prd->c.security_info.plugin_security_attributes = info.plugin_security_attributes;
  }

  return ddsi_omg_security_register_remote_reader_match (prd, wr, crypto_handle, relay_only);
}

void ddsi_omg_security_set_remote_writer_crypto_tokens (struct ddsi_reader *rd, const ddsi_guid_t *pwr_guid, const ddsi_dataholderseq_t *tokens)
{
  struct dds_security_context *sc = ddsi_omg_security_get_secure_context (rd->c.pp);
  struct ddsi_domaingv *gv = rd->e.gv;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  struct pending_match *match;
  struct ddsi_proxy_writer *pwr = NULL;
  int64_t crypto_handle = 0;

  if (!sc)
     return;

  DDS_Security_DatawriterCryptoTokenSeq * tseq = DDS_Security_DataHolderSeq_alloc();
  ddsi_omg_copyin_DataHolderSeq (tseq, tokens);

  ddsrt_mutex_lock(&rd->e.lock);
  match = find_or_create_pending_entity_match(&sc->security_matches, DDSI_EK_PROXY_WRITER, pwr_guid, &rd->e.guid, 0, tseq);
  if ((pwr = ddsi_entidx_lookup_proxy_writer_guid (gv->entity_index, pwr_guid)) == NULL || match->crypto_handle == 0)
    GVTRACE("remember writer tokens src("PGUIDFMT") dst("PGUIDFMT")\n", PGUID(*pwr_guid), PGUID(rd->e.guid));
  else
  {
    if (!sc->crypto_context->crypto_key_exchange->set_remote_datawriter_crypto_tokens(sc->crypto_context->crypto_key_exchange, rd->sec_attr->crypto_handle, match->crypto_handle, tseq, &exception))
      EXCEPTION_ERROR(gv, &exception, "Failed to set remote writer crypto tokens "PGUIDFMT" for reader "PGUIDFMT, PGUID(pwr->e.guid), PGUID(rd->e.guid));
    else
    {
      GVTRACE("set_remote_writer_crypto_tokens "PGUIDFMT" with reader "PGUIDFMT"\n", PGUID(pwr->e.guid), PGUID(rd->e.guid));
      crypto_handle = match->crypto_handle;
    }
    delete_pending_match(&sc->security_matches, match);
  }
  ddsrt_mutex_unlock(&rd->e.lock);

  if (crypto_handle != 0)
    ddsi_connect_reader_with_proxy_writer_secure(rd, pwr, ddsrt_time_monotonic (), crypto_handle);

  if (pwr)
    notify_handshake_recv_token(rd->c.pp, pwr->c.proxypp);
}

void ddsi_omg_security_set_remote_reader_crypto_tokens (struct ddsi_writer *wr, const ddsi_guid_t *prd_guid, const ddsi_dataholderseq_t *tokens)
{
  struct dds_security_context *sc = ddsi_omg_security_get_secure_context (wr->c.pp);
  struct ddsi_domaingv *gv = wr->e.gv;
  DDS_Security_SecurityException exception = DDS_SECURITY_EXCEPTION_INIT;
  struct pending_match *match;
  struct ddsi_proxy_reader *prd = NULL;
  int64_t crypto_handle = 0;

  if (!sc)
    return;

   DDS_Security_DatawriterCryptoTokenSeq *tseq = DDS_Security_DataHolderSeq_alloc();
   ddsi_omg_copyin_DataHolderSeq (tseq, tokens);

   ddsrt_mutex_lock(&wr->e.lock);
   match = find_or_create_pending_entity_match(&sc->security_matches, DDSI_EK_PROXY_READER, prd_guid, &wr->e.guid, 0, tseq);
   if (((prd = ddsi_entidx_lookup_proxy_reader_guid (gv->entity_index, prd_guid)) == NULL) || (match->crypto_handle == 0))
     GVTRACE("remember reader tokens src("PGUIDFMT") dst("PGUIDFMT")\n", PGUID(*prd_guid), PGUID(wr->e.guid));
   else
   {
     if (!sc->crypto_context->crypto_key_exchange->set_remote_datareader_crypto_tokens(sc->crypto_context->crypto_key_exchange, wr->sec_attr->crypto_handle, match->crypto_handle, tseq, &exception))
       EXCEPTION_ERROR(gv, &exception, "Failed to set remote reader crypto tokens "PGUIDFMT" for writer "PGUIDFMT, PGUID(prd->e.guid), PGUID(wr->e.guid));
     else
     {
       GVTRACE("set_remote_reader_crypto_tokens "PGUIDFMT" with writer "PGUIDFMT"\n", PGUID(prd->e.guid), PGUID(wr->e.guid));
       crypto_handle = match->crypto_handle;
     }
     delete_pending_match(&sc->security_matches, match);
   }
   ddsrt_mutex_unlock(&wr->e.lock);

   if (crypto_handle != 0)
     ddsi_connect_writer_with_proxy_reader_secure(wr, prd, ddsrt_time_monotonic (), crypto_handle);

   if (prd)
     notify_handshake_recv_token(wr->c.pp, prd->c.proxypp);
}

bool ddsi_omg_reader_is_discovery_protected (const struct ddsi_reader *rd)
{
  assert (rd != NULL);
  return rd->sec_attr != NULL && rd->sec_attr->attr.is_discovery_protected;
}

static bool ddsi_omg_security_encode_datareader_submessage (struct ddsi_reader *rd, const ddsi_guid_prefix_t *dst_prefix, const unsigned char *src_buf, size_t src_len, unsigned char **dst_buf, size_t *dst_len)
{
  DDS_Security_SecurityException ex = DDS_SECURITY_EXCEPTION_INIT;
  struct ddsi_rd_pwr_match *m;
  ddsrt_avl_iter_t it;
  DDS_Security_DatareaderCryptoHandleSeq hdls = { 0, 0, NULL };
  DDS_Security_OctetSeq encoded_buffer;
  DDS_Security_OctetSeq plain_buffer;
  bool result = false;
  int32_t idx = 0;

  assert (rd);
  assert (src_len <= UINT32_MAX);
  assert (src_buf);
  assert (dst_len);
  assert (dst_buf);
  assert (rd->sec_attr);
  assert (ddsi_omg_reader_is_submessage_protected (rd));

  const struct ddsi_domaingv *gv = rd->e.gv;
  const struct dds_security_context *sc = ddsi_omg_security_get_secure_context (rd->c.pp);
  assert (sc);

  GVTRACE (" encode_datareader_submessage "PGUIDFMT" %s/%s", PGUID (rd->e.guid), rd->xqos->topic_name, rd->type->type_name);
  // FIXME: print_buf(src_buf, src_len, "ddsi_omg_security_encode_datareader_submessage (SOURCE)");

  ddsrt_mutex_lock (&rd->e.lock);
  hdls._buffer = DDS_Security_DatawriterCryptoHandleSeq_allocbuf (rd->num_writers);
  hdls._maximum = rd->num_writers;
  for (m = ddsrt_avl_iter_first (&ddsi_rd_writers_treedef, &rd->writers, &it); m; m = ddsrt_avl_iter_next (&it))
  {
    if (m->crypto_handle && (!dst_prefix || ddsi_guid_prefix_eq (&m->pwr_guid.prefix, dst_prefix)))
      hdls._buffer[idx++] = m->crypto_handle;
  }
  ddsrt_mutex_unlock (&rd->e.lock);

  if ((hdls._length = (DDS_Security_unsigned_long) idx) == 0)
  {
    GVTRACE ("Submsg encoding failed for datareader "PGUIDFMT" %s/%s: no matching writers\n", PGUID (rd->e.guid), rd->xqos->topic_name, rd->type->type_name);
    goto err_enc_drd_subm;
  }

  memset (&encoded_buffer, 0, sizeof (encoded_buffer));
  plain_buffer._buffer = (DDS_Security_octet*) src_buf;
  plain_buffer._length = (uint32_t) src_len;
  plain_buffer._maximum = (uint32_t) src_len;

  if (!(result = sc->crypto_context->crypto_transform->encode_datareader_submessage (
      sc->crypto_context->crypto_transform, &encoded_buffer, &plain_buffer, rd->sec_attr->crypto_handle, &hdls, &ex)))
  {
    GVWARNING ("Submsg encoding failed for datareader "PGUIDFMT" %s/%s: %s", PGUID (rd->e.guid), rd->xqos->topic_name,
        rd->type->type_name, ex.message ? ex.message : "Unknown error");
    GVTRACE ("\n");
    DDS_Security_Exception_reset (&ex);
    goto err_enc_drd_subm;
  }
  assert (encoded_buffer._buffer);
  *dst_buf = encoded_buffer._buffer;
  *dst_len = encoded_buffer._length;
  // FIXME: print_buf (*dst_buf, *dst_len, "ddsi_omg_security_encode_datareader_submessage (DEST)");
  goto end_enc_drd_subm;

err_enc_drd_subm:
  *dst_buf = NULL;
  *dst_len = 0;

end_enc_drd_subm:
  DDS_Security_DatawriterCryptoHandleSeq_freebuf (&hdls);
  return result;
}

static bool ddsi_omg_security_encode_datawriter_submessage (struct ddsi_writer *wr, const ddsi_guid_prefix_t *dst_prefix, const unsigned char *src_buf, size_t src_len, unsigned char **dst_buf, size_t *dst_len)
{
  DDS_Security_SecurityException ex = DDS_SECURITY_EXCEPTION_INIT;
  struct ddsi_wr_prd_match *m;
  ddsrt_avl_iter_t it;
  DDS_Security_DatareaderCryptoHandleSeq hdls = { 0, 0, NULL };
  DDS_Security_OctetSeq encoded_buffer;
  DDS_Security_OctetSeq plain_buffer;
  bool result = false;
  int32_t idx = 0;

  assert (wr);
  assert (src_len <= UINT32_MAX);
  assert (src_buf);
  assert (dst_len);
  assert (dst_buf);
  assert (wr->sec_attr);
  assert (ddsi_omg_writer_is_submessage_protected (wr));
  ASSERT_MUTEX_HELD (wr->e.lock);

  const struct ddsi_domaingv *gv = wr->e.gv;
  const struct dds_security_context *sc = ddsi_omg_security_get_secure_context (wr->c.pp);
  assert (sc);

  GVTRACE (" encode_datawriter_submessage "PGUIDFMT" %s/%s", PGUID (wr->e.guid), wr->xqos->topic_name, wr->type->type_name);

  // FIXME: print_buf(src_buf, src_len, "ddsi_omg_security_encode_datawriter_submessage (SOURCE)");

  hdls._buffer = DDS_Security_DatareaderCryptoHandleSeq_allocbuf (wr->num_readers);
  hdls._maximum = wr->num_readers;
  for (m = ddsrt_avl_iter_first (&ddsi_wr_readers_treedef, &wr->readers, &it); m; m = ddsrt_avl_iter_next (&it))
  {
    if (m->crypto_handle && (!dst_prefix || ddsi_guid_prefix_eq (&m->prd_guid.prefix, dst_prefix)))
      hdls._buffer[idx++] = m->crypto_handle;
  }

  if ((hdls._length = (DDS_Security_unsigned_long) idx) == 0)
  {
    GVTRACE ("Submsg encoding failed for datawriter "PGUIDFMT" %s/%s: no matching readers\n", PGUID (wr->e.guid),
        wr->xqos->topic_name, wr->type->type_name);
    goto err_enc_dwr_subm;
  }

  memset (&encoded_buffer, 0, sizeof (encoded_buffer));
  plain_buffer._buffer = (DDS_Security_octet*) src_buf;
  plain_buffer._length = (uint32_t) src_len;
  plain_buffer._maximum = (uint32_t) src_len;
  result = true;
  idx = 0;
  while (result && idx < (int32_t)hdls._length)
  {
    /* If the plugin thinks a new call is unnecessary, the index will be set to the size of the hdls sequence. */
    result = sc->crypto_context->crypto_transform->encode_datawriter_submessage (sc->crypto_context->crypto_transform,
        &encoded_buffer, &plain_buffer, wr->sec_attr->crypto_handle, &hdls, &idx, &ex);

    /* With a possible second call to encode, the plain buffer should be NULL. */
    plain_buffer._buffer = NULL;
    plain_buffer._length = 0;
    plain_buffer._maximum = 0;
  }

  if (!result)
  {
    GVWARNING ("Submsg encoding failed for datawriter "PGUIDFMT" %s/%s: %s", PGUID (wr->e.guid), wr->xqos->topic_name, wr->type->type_name, ex.message ? ex.message : "Unknown error");
    GVTRACE ("\n");
    DDS_Security_Exception_reset (&ex);
    goto err_enc_dwr_subm;
  }

  assert (encoded_buffer._buffer);
  *dst_buf = encoded_buffer._buffer;
  *dst_len = encoded_buffer._length;
  // FIXME: print_buf (*dst_buf, *dst_len, "ddsi_omg_security_encode_datawriter_submessage (DEST)");
  goto end_enc_dwr_subm;

err_enc_dwr_subm:
  *dst_buf = NULL;
  *dst_len = 0;

end_enc_dwr_subm:
  DDS_Security_DatareaderCryptoHandleSeq_freebuf (&hdls);
  return result;
}

static bool ddsi_omg_security_decode_submessage (const struct ddsi_domaingv *gv, const ddsi_guid_prefix_t * const src_prefix, const ddsi_guid_prefix_t * const dst_prefix, const unsigned char *src_buf, size_t src_len, unsigned char **dst_buf, size_t *dst_len)
{
  DDS_Security_SecurityException ex = DDS_SECURITY_EXCEPTION_INIT;
  struct dds_security_context *sc = gv->security_context;
  DDS_Security_SecureSubmessageCategory_t cat = 0;
  DDS_Security_DatawriterCryptoHandle pp_crypto_hdl = DDS_SECURITY_HANDLE_NIL;
  DDS_Security_DatawriterCryptoHandle proxypp_crypto_hdl = DDS_SECURITY_HANDLE_NIL;
  DDS_Security_DatawriterCryptoHandle wr_crypto_hdl = DDS_SECURITY_HANDLE_NIL;
  DDS_Security_DatareaderCryptoHandle rd_crypto_hdl = DDS_SECURITY_HANDLE_NIL;
  DDS_Security_OctetSeq encoded_buffer;
  DDS_Security_OctetSeq plain_buffer;
  struct ddsi_participant *pp = NULL;
  struct ddsi_proxy_participant *proxypp;
  ddsi_guid_t proxypp_guid, pp_guid  = { .prefix= {.u = {0,0,0} }, .entityid.u = 0 };
  bool result;

  assert (src_len <= UINT32_MAX);
  assert (src_buf);
  assert (dst_len);
  assert (dst_buf);

  // FIXME: print_buf(src_buf, src_len, "ddsi_omg_security_decode_submessage (SOURCE)");

  proxypp_guid.prefix = *src_prefix;
  proxypp_guid.entityid.u = DDSI_ENTITYID_PARTICIPANT;
  if (!(proxypp = ddsi_entidx_lookup_proxy_participant_guid (gv->entity_index, &proxypp_guid)))
  {
    GVTRACE (" Unknown remote participant "PGUIDFMT" for decoding submsg\n", PGUID (proxypp_guid));
    return false;
  }
  if (!proxypp->sec_attr)
  {
    GVTRACE (" Remote participant "PGUIDFMT" not secure for decoding submsg\n", PGUID (proxypp_guid));
    return false;
  }
  proxypp_crypto_hdl = proxypp->sec_attr->crypto_handle;

  if (proxypp_crypto_hdl == DDS_SECURITY_HANDLE_NIL)
  {
    GVTRACE (" Remote participant "PGUIDFMT" not matched yet for decoding submsg\n", PGUID (proxypp_guid));
    return false;
  }

  if (dst_prefix && !ddsi_guid_prefix_zero (dst_prefix))
  {
    pp_guid.prefix = *dst_prefix;
    pp_guid.entityid.u = DDSI_ENTITYID_PARTICIPANT;
    if (!(pp = ddsi_entidx_lookup_participant_guid (gv->entity_index, &pp_guid)))
      return false;
    pp_crypto_hdl = pp->sec_attr->crypto_handle;
  }

  GVTRACE(" decode: pp_crypto=%"PRId64" proxypp_crypto=%"PRId64"\n", pp_crypto_hdl, proxypp_crypto_hdl);
  /* Prepare buffers. */
  memset (&plain_buffer, 0, sizeof (plain_buffer));
  encoded_buffer._buffer = (DDS_Security_octet*) src_buf;
  encoded_buffer._length = (uint32_t) src_len;
  encoded_buffer._maximum = (uint32_t) src_len;

  /* Determine how the RTPS sub-message was encoded. */
  assert (sc);
  result = sc->crypto_context->crypto_transform->preprocess_secure_submsg (sc->crypto_context->crypto_transform, &wr_crypto_hdl, &rd_crypto_hdl,
      &cat, &encoded_buffer, pp_crypto_hdl, proxypp_crypto_hdl, &ex);
  GVTRACE ( "decode_submessage: pp("PGUIDFMT") proxypp("PGUIDFMT"), cat(%d)", PGUID (pp_guid), PGUID (proxypp_guid), (int) cat);
  if (!result)
  {
    GVTRACE (" Pre-process submsg failed: %s\n", ex.message ? ex.message : "Unknown error");
    DDS_Security_Exception_reset (&ex);
    return false;
  }

  switch (cat)
  {
  case DDS_SECURITY_DATAWRITER_SUBMESSAGE:
    result = sc->crypto_context->crypto_transform->decode_datawriter_submessage(sc->crypto_context->crypto_transform, &plain_buffer, &encoded_buffer, rd_crypto_hdl, wr_crypto_hdl, &ex);
    break;
  case DDS_SECURITY_DATAREADER_SUBMESSAGE:
    result = sc->crypto_context->crypto_transform->decode_datareader_submessage(sc->crypto_context->crypto_transform, &plain_buffer, &encoded_buffer, wr_crypto_hdl, rd_crypto_hdl, &ex);
    break;
  case DDS_SECURITY_INFO_SUBMESSAGE:
    /* No decoding needed.
     * TODO: Is DDS_SECURITY_INFO_SUBMESSAGE even possible when there's a DDSI_RTPS_SMID_SEC_PREFIX?
     *
     * This function is only called when there is a prefix. If it is possible,
     * then I might have a problem because the further parsing expects a new
     * buffer (without the security sub-messages).
     *
     */
    result = true;
    break;
  default:
    result = false;
    break;
  }

  if (!result)
  {
    GVTRACE (" Submsg decoding failed: %s\n", ex.message ? ex.message : "Unknown error");
    DDS_Security_Exception_reset (&ex);
    *dst_buf = NULL;
    *dst_len = 0;
    return false;
  }

  assert (plain_buffer._buffer);
  *dst_buf = plain_buffer._buffer;
  *dst_len = plain_buffer._length;
  // FIXME: print_buf(*dst_buf, *dst_len, "ddsi_omg_security_decode_submessage (DEST-DATAWRITER)");
  return true;
}

static bool ddsi_omg_security_encode_serialized_payload (const struct ddsi_writer *wr, const unsigned char *src_buf, size_t src_len, unsigned char **dst_buf, size_t *dst_len)
{
  DDS_Security_SecurityException ex = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_OctetSeq extra_inline_qos;
  DDS_Security_OctetSeq encoded_buffer;
  DDS_Security_OctetSeq plain_buffer;

  assert (wr);
  assert (src_buf);
  assert (src_len <= UINT32_MAX);
  assert (dst_buf);
  assert (dst_len);
  assert (wr->sec_attr);
  assert (ddsi_omg_writer_is_payload_protected (wr));

  const struct ddsi_domaingv *gv = wr->e.gv;
  const struct dds_security_context *sc = ddsi_omg_security_get_secure_context (wr->c.pp);
  assert (sc);

  // FIXME: print_buf(src_buf, src_len, "ddsi_omg_security_encode_serialized_payload (SOURCE)");

  GVTRACE (" ddsi_security_encode_payload "PGUIDFMT" %s/%s\n", PGUID (wr->e.guid), wr->xqos->topic_name, wr->type->type_name);

  memset (&extra_inline_qos, 0, sizeof (extra_inline_qos));
  memset (&encoded_buffer, 0, sizeof (encoded_buffer));
  plain_buffer._buffer = (DDS_Security_octet *) src_buf;
  plain_buffer._length = (uint32_t) src_len;
  plain_buffer._maximum = (uint32_t) src_len;

  if (!sc->crypto_context->crypto_transform->encode_serialized_payload (sc->crypto_context->crypto_transform,
      &encoded_buffer, &extra_inline_qos, &plain_buffer, wr->sec_attr->crypto_handle, &ex))
  {
    GVERROR ("Payload encoding failed for datawriter "PGUIDFMT": %s\n", PGUID (wr->e.guid), ex.message ? ex.message : "Unknown error");
    DDS_Security_Exception_reset (&ex);
    *dst_buf = NULL;
    *dst_len = 0;
    return false;
  }

  *dst_buf = encoded_buffer._buffer;
  *dst_len = encoded_buffer._length;
  // FIXME: print_buf(*dst_buf, *dst_len, "ddsi_omg_security_encode_serialized_payload (DEST)");

  return true;
}

static bool ddsi_omg_security_decode_serialized_payload (struct ddsi_proxy_writer *pwr, const unsigned char *src_buf, size_t src_len, unsigned char **dst_buf, size_t *dst_len)
{
  DDS_Security_SecurityException ex = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_OctetSeq extra_inline_qos;
  DDS_Security_OctetSeq encoded_buffer;
  DDS_Security_OctetSeq plain_buffer;
  struct ddsi_pwr_rd_match *pwr_rd_match;
  struct ddsi_reader *rd;
  ddsrt_avl_iter_t it;

  assert (pwr);
  assert (src_buf);
  assert (src_len <= UINT32_MAX);
  assert (dst_buf);
  assert (dst_len);

  const struct ddsi_domaingv *gv = pwr->e.gv;
  const struct dds_security_context *sc = ddsi_omg_security_get_secure_context_from_proxypp (pwr->c.proxypp);
  assert (gv);
  assert (sc);

  // FIXME: print_buf(src_buf, src_len, "ddsi_omg_security_decode_serialized_payload (SOURCE)");

  *dst_buf = NULL;
  *dst_len = 0;
  GVTRACE ("decode_payload "PGUIDFMT"", PGUID (pwr->e.guid));

  /* Only one reader is enough to decrypt the data, so use only the first match. */
  ddsrt_mutex_lock (&pwr->e.lock);
  pwr_rd_match = ddsrt_avl_iter_first (&ddsi_pwr_readers_treedef, &pwr->readers, &it);
  ddsrt_mutex_unlock (&pwr->e.lock);
  if (!pwr_rd_match)
  {
    GVTRACE (" Payload decoding failed for from remote datawriter "PGUIDFMT": no local reader\n", PGUID (pwr->e.guid));
    return false;
  }
  if (!pwr_rd_match->crypto_handle)
  {
    GVTRACE (" Payload decoding from datawriter "PGUIDFMT": no crypto handle\n", PGUID (pwr->e.guid));
    return false;
  }
  if (!(rd = ddsi_entidx_lookup_reader_guid (gv->entity_index, &pwr_rd_match->rd_guid)))
  {
    GVTRACE (" No datareader "PGUIDFMT" for decoding data from datawriter "PGUIDFMT"", PGUID (pwr_rd_match->rd_guid), PGUID (pwr->e.guid));
    return false;
  }

  memset (&extra_inline_qos, 0, sizeof (extra_inline_qos));
  memset (&plain_buffer, 0, sizeof (plain_buffer));
  encoded_buffer._buffer  = (DDS_Security_octet *) src_buf;
  encoded_buffer._length  = (uint32_t) src_len;
  encoded_buffer._maximum = (uint32_t) src_len;
  if (!sc->crypto_context->crypto_transform->decode_serialized_payload (sc->crypto_context->crypto_transform,
      &plain_buffer, &encoded_buffer, &extra_inline_qos, rd->sec_attr->crypto_handle, pwr_rd_match->crypto_handle, &ex))
  {
    GVTRACE (" Payload decoding failed for datareader "PGUIDFMT" from datawriter "PGUIDFMT": %s\n", PGUID (pwr_rd_match->rd_guid), PGUID (pwr->e.guid), ex.message ? ex.message : "Unknown error");
    DDS_Security_Exception_reset (&ex);
    return false;
  }
  *dst_buf = plain_buffer._buffer;
  *dst_len = plain_buffer._length;
  // FIXME: print_buf(*dst_buf, *dst_len, "ddsi_omg_security_decode_serialized_payload (DEST)");
  return true;
}

bool ddsi_omg_security_encode_rtps_message (const struct ddsi_domaingv *gv, int64_t src_handle, const ddsi_guid_t *src_guid, const unsigned char *src_buf, size_t src_len, unsigned char **dst_buf, size_t *dst_len, int64_t dst_handle)
{
  struct dds_security_context *sc = gv->security_context;
  DDS_Security_SecurityException ex = DDS_SECURITY_EXCEPTION_INIT;
  DDS_Security_ParticipantCryptoHandleSeq hdls = { 0, 0, NULL };
  DDS_Security_OctetSeq encoded_buffer;
  DDS_Security_OctetSeq plain_buffer;
  struct ddsi_participant_sec_attributes *pp_attr = NULL;
  bool result = false;
  int32_t idx = 0;

  assert (src_buf);
  assert (src_len <= UINT32_MAX);
  assert (dst_buf);
  assert (dst_len);
  assert (sc);

  if (dst_handle != 0)
  {
    hdls._buffer = (DDS_Security_long_long *) &dst_handle;
    hdls._length = hdls._maximum = 1;
  }
  else if ((pp_attr = participant_index_find(sc, src_handle)) != NULL)
  {
    if (SECURITY_INFO_USE_RTPS_AUTHENTICATION(pp_attr->attr))
    {
      if (get_matched_proxypp_crypto_handles(pp_attr, &hdls) == 0)
        return false;
    }
    else
    {
      if ((dst_handle = get_first_matched_proxypp_crypto_handle(pp_attr)) != DDS_SECURITY_HANDLE_NIL)
      {
        hdls._buffer = (DDS_Security_long_long *) &dst_handle;
        hdls._length = hdls._maximum = 1;
      }
    }
  }
  else
    return false;

  GVTRACE (" ] encode_rtps_message ["PGUIDFMT, PGUID (*src_guid));

  if (hdls._length > 0)
  {
    memset (&encoded_buffer, 0, sizeof (encoded_buffer));
    plain_buffer._buffer = (DDS_Security_octet *) src_buf;
    plain_buffer._length = (uint32_t) src_len;
    plain_buffer._maximum = (uint32_t) src_len;

    result = true;
    idx = 0;
    while (result && idx < (int32_t) hdls._length)
    {
      /* If the plugin thinks a new call is unnecessary, the index will be set to the size of the hdls sequence. */
      result = sc->crypto_context->crypto_transform->encode_rtps_message (sc->crypto_context->crypto_transform,
          &encoded_buffer, &plain_buffer, src_handle, &hdls, &idx, &ex);

      /* With a possible second call to encode, the plain buffer should be NULL. */
      plain_buffer._buffer = NULL;
      plain_buffer._length = 0;
      plain_buffer._maximum = 0;
    }

    if (!result)
    {
      GVTRACE ("]\n");
      GVTRACE ("encoding rtps message for participant "PGUIDFMT" failed: %s", PGUID (*src_guid), ex.message ? ex.message : "Unknown error");
      GVTRACE ("[");
      DDS_Security_Exception_reset (&ex);
      *dst_buf = NULL;
      *dst_len = 0;
    }
    else
    {
      assert (encoded_buffer._buffer);
      *dst_buf = encoded_buffer._buffer;
      *dst_len = encoded_buffer._length;
    }
  }

  if (dst_handle == DDS_SECURITY_HANDLE_NIL)
    ddsrt_free(hdls._buffer);

  return result;
}

static bool ddsi_omg_security_decode_rtps_message (struct ddsi_proxy_participant *proxypp, const unsigned char *src_buf, size_t src_len, unsigned char **dst_buf, size_t *dst_len)
{
  DDS_Security_SecurityException ex = DDS_SECURITY_EXCEPTION_INIT;
  struct dds_security_context *sc;
  DDS_Security_OctetSeq encoded_buffer;
  DDS_Security_OctetSeq plain_buffer = {0, 0, NULL};
  ddsrt_avl_iter_t it;

  assert (proxypp);
  assert (src_buf);
  assert (src_len <= UINT32_MAX);
  assert (dst_buf);
  assert (dst_len);

  const struct ddsi_domaingv *gv = proxypp->e.gv;
  GVTRACE ("decode_rtps_message from "PGUIDFMT"\n", PGUID (proxypp->e.guid));

  *dst_buf = NULL;
  *dst_len = 0;
  encoded_buffer._buffer = (DDS_Security_octet *) src_buf;
  encoded_buffer._length = (uint32_t) src_len;
  encoded_buffer._maximum = (uint32_t) src_len;

  ddsrt_mutex_lock (&proxypp->sec_attr->lock);
  for (struct ddsi_proxypp_pp_match *pm = ddsrt_avl_iter_first (&proxypp_pp_treedef, &proxypp->sec_attr->participants, &it); pm; pm = ddsrt_avl_iter_next (&it))
  {
    sc = ddsi_omg_security_get_secure_context_from_proxypp (proxypp);
    assert (sc);
    if (!sc->crypto_context->crypto_transform->decode_rtps_message (sc->crypto_context->crypto_transform, &plain_buffer, &encoded_buffer, pm->pp_crypto_handle, proxypp->sec_attr->crypto_handle, &ex))
    {
      if (ex.code == DDS_SECURITY_ERR_INVALID_CRYPTO_RECEIVER_SIGN_CODE)
      {
        DDS_Security_Exception_reset (&ex);
        continue; /* Could be caused by 'with_origin_authentication' being used, so try next match */
      }
      GVTRACE ("decoding rtps message from remote participant "PGUIDFMT" failed: %s\n", PGUID (proxypp->e.guid), ex.message ? ex.message : "Unknown error");
      DDS_Security_Exception_reset (&ex);
      ddsrt_mutex_unlock (&proxypp->sec_attr->lock);
      return false;
    }
    *dst_buf = plain_buffer._buffer;
    *dst_len = plain_buffer._length;
  }
  ddsrt_mutex_unlock (&proxypp->sec_attr->lock);
  if (*dst_buf == NULL)
  {
    GVTRACE ("No match found for remote participant "PGUIDFMT" for decoding rtps message\n", PGUID (proxypp->e.guid));
    return false;
  }

  return true;
}

bool ddsi_omg_reader_is_submessage_protected (const struct ddsi_reader *rd)
{
  assert (rd != NULL);
  return rd->sec_attr != NULL && rd->sec_attr->attr.is_submessage_protected;
}

bool ddsi_security_encode_payload (struct ddsi_writer *wr, ddsrt_iovec_t *vec, unsigned char **buf)
{
  *buf = NULL;
  if (!ddsi_omg_writer_is_payload_protected (wr))
    return true;

  unsigned char *enc_buf;
  size_t enc_len;
  if (!ddsi_omg_security_encode_serialized_payload (wr, vec->iov_base, vec->iov_len, &enc_buf, &enc_len))
    return false;

  /* Replace the iov buffer, which should always be aliased. */
  vec->iov_base = (char *) enc_buf;
  vec->iov_len = (ddsrt_iov_len_t) enc_len;
  assert ((size_t) vec->iov_len == enc_len);
  *buf = enc_buf;
  return true;
}

static bool decode_payload (const struct ddsi_domaingv *gv, struct ddsi_rsample_info *sampleinfo, unsigned char *payloadp, uint32_t *payloadsz, size_t *submsg_len)
{
  assert (payloadp);
  assert (payloadsz);
  assert (*payloadsz);
  assert (submsg_len);
  assert (sampleinfo);

  if (sampleinfo->pwr == NULL)
    /* No specified proxy writer means no encoding. */
    return true;

  /* Only decode when the attributes tell us so. */
  if ((sampleinfo->pwr->c.security_info.security_attributes & DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_PAYLOAD_PROTECTED)
      != DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_PAYLOAD_PROTECTED)
    return true;

  unsigned char *dst_buf = NULL;
  size_t dst_len = 0;
  if (!ddsi_omg_security_decode_serialized_payload (sampleinfo->pwr, payloadp, *payloadsz, &dst_buf, &dst_len))
  {
    GVTRACE ("decode_payload: failed to decrypt data from "PGUIDFMT"\n", PGUID (sampleinfo->pwr->e.guid));
    return false;
  }

  assert (dst_buf);
  /* Expect result to always fit into the original buffer. */
  assert (*payloadsz >= dst_len);

  /* Reduce submessage and payload lengths. */
  *submsg_len -= *payloadsz - (uint32_t) dst_len;
  *payloadsz = (uint32_t) dst_len;
  memcpy (payloadp, dst_buf, dst_len);
  ddsrt_free (dst_buf);
  return true;
}

bool ddsi_security_decode_data (const struct ddsi_domaingv *gv, struct ddsi_rsample_info *sampleinfo, unsigned char *payloadp, uint32_t payloadsz, size_t *submsg_len)
{
  /* Only decode when there's actual data. */
  if (payloadp == NULL || payloadsz == 0)
    return true;
  else if (!decode_payload (gv, sampleinfo, payloadp, &payloadsz, submsg_len))
    return false;
  else
  {
    /* It's possible that the payload size (and thus the sample size) has been reduced. */
    sampleinfo->size = payloadsz;
    return true;
  }
}

bool ddsi_security_decode_datafrag (const struct ddsi_domaingv *gv, struct ddsi_rsample_info *sampleinfo, unsigned char *payloadp, uint32_t payloadsz, size_t *submsg_len)
{
  /* Only decode when there's actual data; do not touch the sampleinfo->size in
     contradiction to ddsi_security_decode_data() (it has been calculated differently). */
  if (payloadp == NULL || payloadsz == 0)
    return true;
  else
    return decode_payload (gv, sampleinfo, payloadp, &payloadsz, submsg_len);
}

void ddsi_security_encode_datareader_submsg (struct ddsi_xmsg *msg, struct ddsi_xmsg_marker sm_marker, const struct ddsi_proxy_writer *pwr, const struct ddsi_guid *rd_guid)
{
  /* FIXME: avoid this lookup */
  struct ddsi_reader * const rd = ddsi_entidx_lookup_reader_guid (pwr->e.gv->entity_index, rd_guid);
  /* surely a reader can only be protected if the participant has security enabled? */
  if (rd == NULL || !ddsi_omg_reader_is_submessage_protected (rd))
    return;
  assert (ddsi_omg_participant_is_secure (rd->c.pp));

  unsigned char *src_buf;
  size_t src_len;
  unsigned char *dst_buf;
  size_t dst_len;

  /* Make one blob of the current sub-message by appending the serialized payload. */
  ddsi_xmsg_submsg_append_refd_payload (msg, sm_marker);

  /* Get the sub-message buffer. */
  src_buf = ddsi_xmsg_submsg_from_marker (msg, sm_marker);
  src_len = ddsi_xmsg_submsg_size (msg, sm_marker);

  if (ddsi_omg_security_encode_datareader_submessage (rd, &pwr->e.guid.prefix, src_buf, src_len, &dst_buf, &dst_len))
  {
    ddsi_xmsg_submsg_replace (msg, sm_marker, dst_buf, dst_len);
    ddsrt_free (dst_buf);
  }
  else
  {
    /* The sub-message should have been encoded, which failed. Remove it to prevent it from being send. */
    ddsi_xmsg_submsg_remove (msg, sm_marker);
  }
}

void ddsi_security_encode_datawriter_submsg (struct ddsi_xmsg *msg, struct ddsi_xmsg_marker sm_marker, struct ddsi_writer *wr)
{
  if (!ddsi_omg_writer_is_submessage_protected (wr))
    return;

  /* Only encode when needed.  Surely a writer can only be protected if the participant has security enabled? */
  assert (ddsi_omg_participant_is_secure (wr->c.pp));

  unsigned char *src_buf;
  size_t src_len;
  unsigned char *dst_buf;
  size_t dst_len;
  ddsi_guid_prefix_t dst_guid_prefix;
  ddsi_guid_prefix_t *dst = NULL;

  /* Make one blob of the current sub-message by appending the serialized payload. */
  ddsi_xmsg_submsg_append_refd_payload (msg, sm_marker);

  /* Get the sub-message buffer. */
  src_buf = ddsi_xmsg_submsg_from_marker (msg, sm_marker);
  src_len = ddsi_xmsg_submsg_size (msg, sm_marker);

  if (ddsi_xmsg_getdst1_prefix (msg, &dst_guid_prefix))
    dst = &dst_guid_prefix;

  if (ddsi_omg_security_encode_datawriter_submessage (wr, dst, src_buf, src_len, &dst_buf, &dst_len))
  {
    ddsi_xmsg_submsg_replace (msg, sm_marker, dst_buf, dst_len);
    ddsrt_free (dst_buf);
  }
  else
  {
    /* The sub-message should have been encoded, which failed. Remove it to prevent it from being send. */
    ddsi_xmsg_submsg_remove (msg, sm_marker);
  }
}

bool ddsi_security_validate_msg_decoding (const struct ddsi_entity_common *e, const struct ddsi_proxy_endpoint_common *c, const struct ddsi_proxy_participant *proxypp, const struct ddsi_receiver_state *rst, ddsi_rtps_submessage_kind_t prev_smid)
{
  assert (e);
  assert (c);
  assert (proxypp);
  assert (rst);

  /* If this endpoint is expected to have submessages protected, it means that the
   * previous submessage id (prev_smid) has to be DDSI_RTPS_SMID_SEC_PREFIX. That caused the
   * protected submessage to be copied into the current RTPS message as a clear
   * submessage, which we are currently handling.
   * However, we have to check if the prev_smid is actually DDSI_RTPS_SMID_SEC_PREFIX, otherwise
   * a rascal can inject data as just a clear submessage. */
  if ((c->security_info.security_attributes & DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED)
      == DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED)
  {
    if (prev_smid != DDSI_RTPS_SMID_SEC_PREFIX)
      return false;
  }

  /* At this point, we should also check if the complete RTPS message was encoded when
   * that is expected. */
  if (ddsi_omg_security_is_remote_rtps_protected (proxypp, e->guid.entityid) && !rst->rtps_encoded)
  {
    return false;
  }

  return true;
}

static int32_t validate_submsg (struct ddsi_domaingv *gv, ddsi_rtps_submessage_kind_t smid, const unsigned char *submsg, unsigned char const * const end, int byteswap)
{
  assert (end >= submsg);
  if ((size_t) (end - submsg) < DDSI_RTPS_SUBMESSAGE_HEADER_SIZE)
  {
    GVWARNING ("Submsg 0x%02x does not fit message", smid);
    return -1;
  }

  ddsi_rtps_submessage_header_t const * const hdr = (ddsi_rtps_submessage_header_t *) submsg;
  if (hdr->submessageId != smid && smid != DDSI_RTPS_SMID_PAD)
  {
    GVWARNING("Unexpected submsg 0x%02x (0x%02x expected)", hdr->submessageId, smid);
    return -1;
  }

  uint16_t size = hdr->octetsToNextHeader;
  if (byteswap)
    size = ddsrt_bswap2u (size);
  const int32_t result = (int32_t) size + (int32_t) DDSI_RTPS_SUBMESSAGE_HEADER_SIZE;
  if (end - submsg < result)
  {
    GVWARNING ("Submsg 0x%02x does not fit message", smid);
    return -1;
  }
  return result;
}

static int32_t padding_submsg (struct ddsi_domaingv *gv, unsigned char *start, unsigned char *end, int byteswap)
{
  assert (end >= start);
  const size_t size = (size_t) (end - start);
  if (size < DDSI_RTPS_SUBMESSAGE_HEADER_SIZE)
  {
    GVWARNING("Padding submessage doesn't fit");
    return -1;
  }

  assert (size <= UINT16_MAX + DDSI_RTPS_SUBMESSAGE_HEADER_SIZE);
  ddsi_rtps_submessage_header_t * const padding = (ddsi_rtps_submessage_header_t *) start;
  padding->submessageId = DDSI_RTPS_SMID_PAD;
  DDSRT_STATIC_ASSERT (DDSI_RTPS_SUBMESSAGE_FLAG_ENDIANNESS == 1);
  DDSRT_WARNING_MSVC_OFF(6326)
  padding->flags = (byteswap ? !(DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN) : (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN));
  DDSRT_WARNING_MSVC_ON(6326)
  padding->octetsToNextHeader = (uint16_t) (size - DDSI_RTPS_SUBMESSAGE_HEADER_SIZE);
  if (byteswap)
    padding->octetsToNextHeader = ddsrt_bswap2u (padding->octetsToNextHeader);
  return (int32_t) size;
}

static bool ddsi_security_decode_sec_prefix_patched_hdr_flags (const struct ddsi_receiver_state *rst, unsigned char *submsg, size_t submsg_size, unsigned char * const msg_end, const ddsi_guid_prefix_t * const src_prefix, const ddsi_guid_prefix_t * const dst_prefix, int byteswap)
{
  int smsize = -1;
  size_t totalsize = submsg_size;
  unsigned char *body_submsg;
  unsigned char *prefix_submsg;
  unsigned char *postfix_submsg;

  /* First sub-message is the SEC_PREFIX. */
  prefix_submsg = submsg;

  /* Next sub-message is SEC_BODY when encrypted or the original submessage when only signed. */
  if ((submsg_size % 4) != 0)
    return false;
  body_submsg = submsg + submsg_size;
  if ((smsize = validate_submsg (rst->gv, DDSI_RTPS_SMID_PAD, body_submsg, msg_end, byteswap)) <= 0)
    return false;
  if ((smsize % 4) != 0)
    return false;
  totalsize += (size_t) smsize;

  /* Third sub-message should be the SEC_POSTFIX. */
  postfix_submsg = submsg + totalsize;
  if ((smsize = validate_submsg (rst->gv, DDSI_RTPS_SMID_SEC_POSTFIX, postfix_submsg, msg_end, byteswap)) <= 0)
    return false;
  totalsize += (size_t) smsize;

  /* Decode all three submessages. */
  unsigned char *dst_buf;
  size_t dst_len;
  const bool decoded = ddsi_omg_security_decode_submessage (rst->gv, src_prefix, dst_prefix, submsg, totalsize, &dst_buf, &dst_len);
  if (decoded && dst_buf)
  {
    /*
     * The 'normal' submessage sequence handling will continue after the
     * given security SEC_PREFIX.
     */
    ddsi_rtps_submessage_header_t const * const body_submsg_hdr = (ddsi_rtps_submessage_header_t const *) body_submsg;
    if (body_submsg_hdr->submessageId == DDSI_RTPS_SMID_SEC_BODY)
    {
      /*
       * Copy the decoded buffer into the original message, replacing (part
       * of) SEC_BODY.
       *
       * By replacing the SEC_BODY with the decoded submessage, everything
       * can continue as if there was never an encoded submessage.
       */
      assert (totalsize >= submsg_size);
      assert (dst_len <= totalsize - submsg_size);
      memcpy (body_submsg, dst_buf, dst_len);

      /* Remainder of SEC_BODY & SEC_POSTFIX should be padded to keep the submsg sequence going. */
      smsize = padding_submsg (rst->gv, body_submsg + dst_len, prefix_submsg + totalsize, byteswap);
    }
    else
    {
      /*
       * When only signed, then the submessage is already available and
       * DDSI_RTPS_SMID_SEC_POSTFIX will be ignored.
       * So, we don't really have to do anything.
       */
    }
    ddsrt_free (dst_buf);
  }
  else
  {
    /*
     * Decoding or signing failed.
     *
     * Replace the security submessages with padding. This also removes a plain
     * submessage when a signature check failed.
     */
    smsize = padding_submsg (rst->gv, body_submsg, prefix_submsg + totalsize, byteswap);
  }

  return (smsize > 0);
}

bool ddsi_security_decode_sec_prefix (const struct ddsi_receiver_state *rst, unsigned char *submsg, size_t submsg_size, unsigned char * const msg_end, const ddsi_guid_prefix_t * const src_prefix, const ddsi_guid_prefix_t * const dst_prefix, int byteswap)
{
  /* FIXME: eliminate the patching of hdr->flags if possible */
  ddsi_rtps_submessage_header_t *hdr = (ddsi_rtps_submessage_header_t *) submsg;
  const uint8_t saved_flags = hdr->flags;
  if (byteswap)
  {
    DDSRT_WARNING_MSVC_OFF(6326)
    if (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN)
      hdr->flags |= 0x01;
    else
      hdr->flags &= 0xFE;
    DDSRT_WARNING_MSVC_ON(6326)
  }
  bool result = ddsi_security_decode_sec_prefix_patched_hdr_flags (rst, submsg, submsg_size, msg_end, src_prefix, dst_prefix, byteswap);
  hdr->flags = saved_flags;
  return result;
}

static ddsi_rtps_msg_state_t check_rtps_message_is_secure (struct ddsi_domaingv *gv, ddsi_rtps_header_t *hdr, const unsigned char *buff, bool isstream, struct ddsi_proxy_participant **proxypp)
{
  const uint32_t offset = DDSI_RTPS_MESSAGE_HEADER_SIZE + (isstream ? sizeof (ddsi_rtps_msg_len_t) : 0);
  const ddsi_rtps_submessage_header_t *submsg = (const ddsi_rtps_submessage_header_t *) (buff + offset);
  if (submsg->submessageId != DDSI_RTPS_SMID_SRTPS_PREFIX)
    return DDSI_RTPS_MSG_STATE_PLAIN;

  ddsi_guid_t guid;
  guid.prefix = hdr->guid_prefix;
  guid.entityid.u = DDSI_ENTITYID_PARTICIPANT;

  GVTRACE (" from "PGUIDFMT, PGUID (guid));

  if ((*proxypp = ddsi_entidx_lookup_proxy_participant_guid (gv->entity_index, &guid)) == NULL)
  {
    GVTRACE ("received encoded rtps message from unknown participant\n");
    return DDSI_RTPS_MSG_STATE_ERROR;
  }
  else if (!proxypp_is_authenticated (*proxypp))
  {
    GVTRACE ("received encoded rtps message from unauthenticated participant\n");
    return DDSI_RTPS_MSG_STATE_ERROR;
  }
  else
  {
    return DDSI_RTPS_MSG_STATE_ENCODED;
  }
}

static ddsi_rtps_msg_state_t
decode_rtps_message_awake (
  struct ddsi_rmsg **rmsg,
  ddsi_rtps_header_t **hdr,
  unsigned char **buff,
  size_t *sz,
  struct ddsi_rbufpool *rbpool,
  bool isstream,
  struct ddsi_proxy_participant *proxypp)
{
  unsigned char *dstbuf;
  unsigned char *srcbuf;
  size_t srclen, dstlen;

  /* Currently the decode_rtps_message returns a new allocated buffer.
   * This could be optimized by providing a pre-allocated ddsi_rmsg buffer to
   * copy the decoded rtps message in.
   */
  if (isstream)
  {
    /* Remove MsgLen Submessage which was only needed for a stream to determine the end of the message */
    assert (*sz > sizeof (ddsi_rtps_msg_len_t));
    srcbuf = *buff + sizeof (ddsi_rtps_msg_len_t);
    srclen = *sz - sizeof (ddsi_rtps_msg_len_t);
    memmove (srcbuf, *buff, DDSI_RTPS_MESSAGE_HEADER_SIZE);
  }
else
  {
    assert (*sz > 0);
    srcbuf = *buff;
    srclen = *sz;
  }

  if (!ddsi_omg_security_decode_rtps_message (proxypp, srcbuf, srclen, &dstbuf, &dstlen))
    return DDSI_RTPS_MSG_STATE_ERROR;
  assert (dstbuf);
  assert (dstlen <= UINT32_MAX);

  ddsi_rmsg_commit (*rmsg);
  *rmsg = ddsi_rmsg_new (rbpool);
  *buff = DDSI_RMSG_PAYLOAD (*rmsg);

  memcpy(*buff, dstbuf, dstlen);
  ddsi_rmsg_setsize (*rmsg, (uint32_t) dstlen);

  ddsrt_free (dstbuf);

  *hdr = (ddsi_rtps_header_t *) *buff;
  (*hdr)->guid_prefix = ddsi_ntoh_guid_prefix ((*hdr)->guid_prefix);
  *sz = dstlen;
  return DDSI_RTPS_MSG_STATE_ENCODED;
}

ddsi_rtps_msg_state_t
ddsi_security_decode_rtps_message (
  struct ddsi_thread_state * const thrst,
  struct ddsi_domaingv *gv,
  struct ddsi_rmsg **rmsg,
  ddsi_rtps_header_t **hdr,
  unsigned char **buff,
  size_t *sz,
  struct ddsi_rbufpool *rbpool,
  bool isstream)
{
  struct ddsi_proxy_participant *proxypp;
  ddsi_rtps_msg_state_t ret;
  ddsi_thread_state_awake_fixed_domain (thrst);
  ret = check_rtps_message_is_secure (gv, *hdr, *buff, isstream, &proxypp);
  if (ret == DDSI_RTPS_MSG_STATE_ENCODED)
    ret = decode_rtps_message_awake (rmsg, hdr, buff, sz, rbpool, isstream, proxypp);
  ddsi_thread_state_asleep (thrst);
  return ret;
}

ssize_t
ddsi_security_secure_conn_write(
    const struct ddsi_domaingv *gv,
    struct ddsi_tran_conn * conn,
    const ddsi_locator_t *dst,
    size_t niov,
    const ddsrt_iovec_t *iov,
    uint32_t flags,
    ddsi_rtps_msg_len_t *msg_len,
    bool dst_one,
    ddsi_msg_sec_info_t *sec_info,
    ddsi_tran_write_fn_t conn_write_cb)
{
  ddsi_rtps_header_t *hdr;
  ddsi_guid_t guid;
  unsigned char stbuf[2048];
  unsigned char *srcbuf;
  unsigned char *dstbuf;
  size_t srclen, dstlen;
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

  hdr = (ddsi_rtps_header_t *) iov[0].iov_base;
  guid.prefix = ddsi_ntoh_guid_prefix (hdr->guid_prefix);
  guid.entityid.u = DDSI_ENTITYID_PARTICIPANT;

  /* first determine the size of the message, then select the
   *  on-stack buffer or allocate one on the heap ...
   */
  srclen = 0;
  for (size_t i = 0; i < niov; i++)
  {
    /* Do not copy MsgLen submessage in case of a stream connection */
    if (i != 1 || !conn->m_stream)
      srclen += iov[i].iov_len;
  }
  if (srclen <= sizeof (stbuf))
    srcbuf = stbuf;
  else
    srcbuf = ddsrt_malloc (srclen);

  /* ... then copy data into buffer */
  srclen = 0;
  for (size_t i = 0; i < niov; i++)
  {
    if (i != 1 || !conn->m_stream)
    {
      memcpy (srcbuf + srclen, iov[i].iov_base, iov[i].iov_len);
      srclen += iov[i].iov_len;
    }
  }

  ssize_t ret = -1;
  if (!ddsi_omg_security_encode_rtps_message (gv, sec_info->src_pp_handle, &guid, srcbuf, srclen, &dstbuf, &dstlen, dst_handle))
    ret = -1;
  else
  {
    ddsrt_iovec_t tmp_iov[3];
    size_t tmp_niov;

    if (conn->m_stream)
    {
      /* Add MsgLen submessage after Header */
      assert (dstlen <= UINT32_MAX - sizeof (*msg_len));
      msg_len->length = (uint32_t) (dstlen + sizeof (*msg_len));

      tmp_iov[0].iov_base = dstbuf;
      tmp_iov[0].iov_len = DDSI_RTPS_MESSAGE_HEADER_SIZE;
      tmp_iov[1].iov_base = (void *) msg_len;
      tmp_iov[1].iov_len = sizeof (*msg_len);
      tmp_iov[2].iov_base = dstbuf + DDSI_RTPS_MESSAGE_HEADER_SIZE;
      tmp_iov[2].iov_len = (ddsrt_iov_len_t) (dstlen - DDSI_RTPS_MESSAGE_HEADER_SIZE);
      tmp_niov = 3;
    }
    else
    {
      assert (dstlen <= UINT32_MAX);
      msg_len->length = (uint32_t) dstlen;

      tmp_iov[0].iov_base = dstbuf;
      tmp_iov[0].iov_len = (ddsrt_iov_len_t) dstlen;
      tmp_niov = 1;
    }
    ret = conn_write_cb (conn, dst, tmp_niov, tmp_iov, flags);
    ddsrt_free (dstbuf);
  }

  if (srcbuf != stbuf)
    ddsrt_free (srcbuf);
  return ret;
}

bool ddsi_omg_plist_keyhash_is_protected (const ddsi_plist_t *plist)
{
  assert(plist);
  if (plist->present & PP_ENDPOINT_SECURITY_INFO)
  {
    unsigned attr = plist->endpoint_security_info.security_attributes;
    return attr & DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID &&
           attr & DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_KEY_PROTECTED;
  }
  return false;
}

bool ddsi_omg_is_endpoint_protected (const ddsi_plist_t *plist)
{
  assert(plist);
  return plist->present & PP_ENDPOINT_SECURITY_INFO &&
         !SECURITY_INFO_CLEAR(plist->endpoint_security_info, DDSI_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID);
}

void ddsi_omg_log_endpoint_protection (struct ddsi_domaingv * const gv, const ddsi_plist_t *plist)
{
  GVLOGDISC (" p(");
  if (plist->present & PP_ENDPOINT_SECURITY_INFO)
    GVLOGDISC ("0x%08x.0x%08x", plist->endpoint_security_info.security_attributes, plist->endpoint_security_info.plugin_security_attributes);
  else
    GVLOGDISC ("open");
  GVLOGDISC (")");
}

#else /* DDS_HAS_SECURITY */

#include "ddsi__security_omg.h"

extern inline bool ddsi_omg_security_enabled (void);

extern inline bool ddsi_omg_participant_is_access_protected(UNUSED_ARG(const struct ddsi_participant *pp));
extern inline bool ddsi_omg_participant_is_rtps_protected(UNUSED_ARG(const struct ddsi_participant *pp));
extern inline bool ddsi_omg_participant_is_liveliness_protected(UNUSED_ARG(const struct ddsi_participant *pp));
extern inline bool ddsi_omg_participant_is_discovery_protected(UNUSED_ARG(const struct ddsi_participant *pp));
extern inline bool ddsi_omg_participant_is_secure(UNUSED_ARG(const struct ddsi_participant *pp));
extern inline bool ddsi_omg_proxy_participant_is_secure (UNUSED_ARG(const struct ddsi_proxy_participant *proxypp));

extern inline bool ddsi_omg_security_match_remote_writer_enabled (UNUSED_ARG(struct ddsi_reader *rd), UNUSED_ARG(struct ddsi_proxy_writer *pwr), UNUSED_ARG(int64_t *crypto_handle));
extern inline bool ddsi_omg_security_match_remote_reader_enabled (UNUSED_ARG(struct ddsi_writer *wr), UNUSED_ARG(struct ddsi_proxy_reader *prd), UNUSED_ARG(bool relay_only), UNUSED_ARG(int64_t *crypto_handle));

extern inline bool ddsi_omg_writer_is_discovery_protected (UNUSED_ARG(const struct ddsi_writer *wr));
extern inline bool ddsi_omg_writer_is_submessage_protected (UNUSED_ARG(const struct ddsi_writer *wr));
extern inline bool ddsi_omg_writer_is_payload_protected (UNUSED_ARG(const struct ddsi_writer *wr));

extern inline void ddsi_omg_get_proxy_writer_security_info (UNUSED_ARG(struct ddsi_proxy_writer *pwr), UNUSED_ARG(const ddsi_plist_t *plist), UNUSED_ARG(ddsi_security_info_t *info));
extern inline bool ddsi_omg_security_check_remote_writer_permissions (UNUSED_ARG(const struct ddsi_proxy_writer *pwr), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(struct ddsi_participant *pp));
extern inline void ddsi_omg_security_deregister_remote_writer_match (UNUSED_ARG(const struct ddsi_proxy_writer *pwr), UNUSED_ARG(const struct ddsi_reader *rd), UNUSED_ARG(struct ddsi_rd_pwr_match *match));
extern inline bool ddsi_omg_security_check_remote_reader_permissions (UNUSED_ARG(const struct ddsi_proxy_reader *prd), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(struct ddsi_participant *par), UNUSED_ARG(bool *relay_only));
extern inline void ddsi_omg_security_deregister_remote_reader_match (UNUSED_ARG(const struct ddsi_proxy_reader *prd), UNUSED_ARG(const struct ddsi_writer *wr), UNUSED_ARG(struct ddsi_wr_prd_match *match));

extern inline unsigned ddsi_determine_subscription_writer(UNUSED_ARG(const struct ddsi_reader *rd));
extern inline unsigned ddsi_determine_publication_writer(UNUSED_ARG(const struct ddsi_writer *wr));
#ifdef DDS_HAS_TOPIC_DISCOVERY
extern inline unsigned ddsi_determine_topic_writer(UNUSED_ARG(const struct ddsi_topic *tp));
#endif

extern inline bool ddsi_is_proxy_participant_deletion_allowed(UNUSED_ARG(struct ddsi_domaingv * const gv), UNUSED_ARG(const struct ddsi_guid *guid), UNUSED_ARG(const ddsi_entityid_t pwr_entityid));

extern inline bool ddsi_omg_is_similar_participant_security_info (UNUSED_ARG(struct ddsi_participant *pp), UNUSED_ARG(struct ddsi_proxy_participant *proxypp));

extern inline bool ddsi_omg_participant_allow_unauthenticated(UNUSED_ARG(struct ddsi_participant *pp));

extern inline bool ddsi_omg_security_check_create_participant (UNUSED_ARG(struct ddsi_participant *pp), UNUSED_ARG(uint32_t domain_id));

extern inline void ddsi_omg_security_deregister_participant (UNUSED_ARG(struct ddsi_participant *pp));

extern inline bool ddsi_omg_security_check_create_topic (UNUSED_ARG(const struct ddsi_domaingv *gv), UNUSED_ARG(const ddsi_guid_t *pp_guid), UNUSED_ARG(const char *topic_name), UNUSED_ARG(const struct dds_qos *qos));

extern inline int64_t ddsi_omg_security_get_local_participant_handle (UNUSED_ARG(const struct ddsi_participant *pp));

extern inline bool ddsi_omg_security_check_create_writer (UNUSED_ARG(struct ddsi_participant *pp), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(const char *topic_name), UNUSED_ARG(const struct dds_qos *writer_qos));

extern inline void ddsi_omg_security_register_writer (UNUSED_ARG(struct ddsi_writer *wr));

extern inline void ddsi_omg_security_deregister_writer (UNUSED_ARG(struct ddsi_writer *wr));

extern inline bool ddsi_omg_security_check_create_reader (UNUSED_ARG(struct ddsi_participant *pp), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(const char *topic_name), UNUSED_ARG(const struct dds_qos *reader_qos));

extern inline void ddsi_omg_security_register_reader (UNUSED_ARG(struct ddsi_reader *rd));

extern inline void ddsi_omg_security_deregister_reader (UNUSED_ARG(struct ddsi_reader *rd));

extern inline bool ddsi_omg_security_is_remote_rtps_protected (UNUSED_ARG(const struct ddsi_proxy_participant *proxypp), UNUSED_ARG(ddsi_entityid_t entityid));

/* initialize the proxy participant security attributes */
extern inline void ddsi_omg_security_init_remote_participant (UNUSED_ARG(struct ddsi_proxy_participant *proxypp));

/* ask to access control security plugin for the remote participant permissions */
extern inline int64_t ddsi_omg_security_check_remote_participant_permissions (UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(struct ddsi_participant *pp), UNUSED_ARG(struct ddsi_proxy_participant *proxypp));

extern inline bool ddsi_omg_security_register_remote_participant (UNUSED_ARG(struct ddsi_participant *pp), UNUSED_ARG(struct ddsi_proxy_participant *proxypp), UNUSED_ARG(int64_t identity_handle), UNUSED_ARG(int64_t shared_secret));

extern inline void ddsi_omg_security_deregister_remote_participant (UNUSED_ARG(struct ddsi_proxy_participant *proxypp));

extern inline void ddsi_omg_security_participant_send_tokens (UNUSED_ARG(struct ddsi_participant *pp), UNUSED_ARG(struct ddsi_proxy_participant *proxypp));

extern inline void ddsi_set_proxy_participant_security_info(UNUSED_ARG(struct ddsi_proxy_participant *prd), UNUSED_ARG(const ddsi_plist_t *plist));

extern inline void ddsi_set_proxy_writer_security_info(UNUSED_ARG(struct ddsi_proxy_writer *pwr), UNUSED_ARG(const ddsi_plist_t *plist));

extern inline bool ddsi_security_decode_data(
  UNUSED_ARG(const struct ddsi_domaingv *gv),
  UNUSED_ARG(struct ddsi_rsample_info *sampleinfo),
  UNUSED_ARG(unsigned char *payloadp),
  UNUSED_ARG(uint32_t payloadsz),
  UNUSED_ARG(size_t *submsg_len));

extern inline bool ddsi_security_decode_datafrag(
  UNUSED_ARG(const struct ddsi_domaingv *gv),
  UNUSED_ARG(struct ddsi_rsample_info *sampleinfo),
  UNUSED_ARG(unsigned char *payloadp),
  UNUSED_ARG(uint32_t payloadsz),
  UNUSED_ARG(size_t *submsg_len));

extern inline void ddsi_security_encode_datareader_submsg(
  UNUSED_ARG(struct ddsi_xmsg *msg),
  UNUSED_ARG(struct ddsi_xmsg_marker sm_marker),
  UNUSED_ARG(const struct ddsi_proxy_writer *pwr),
  UNUSED_ARG(const struct ddsi_guid *rd_guid));

extern inline void ddsi_security_encode_datawriter_submsg(
  UNUSED_ARG(struct ddsi_xmsg *msg),
  UNUSED_ARG(struct ddsi_xmsg_marker sm_marker),
  UNUSED_ARG(struct ddsi_writer *wr));

extern inline bool ddsi_security_validate_msg_decoding(
  UNUSED_ARG(const struct ddsi_entity_common *e),
  UNUSED_ARG(const struct ddsi_proxy_endpoint_common *c),
  UNUSED_ARG(struct ddsi_proxy_participant *proxypp),
  UNUSED_ARG(struct ddsi_receiver_state *rst),
  UNUSED_ARG(ddsi_rtps_submessage_kind_t prev_smid));

extern inline int ddsi_security_decode_sec_prefix(
  UNUSED_ARG(struct ddsi_receiver_state *rst),
  UNUSED_ARG(unsigned char *submsg),
  UNUSED_ARG(size_t submsg_size),
  UNUSED_ARG(unsigned char * const msg_end),
  UNUSED_ARG(const ddsi_guid_prefix_t * const src_prefix),
  UNUSED_ARG(const ddsi_guid_prefix_t * const dst_prefix),
  UNUSED_ARG(int byteswap));

extern inline ddsi_rtps_msg_state_t ddsi_security_decode_rtps_message (
  UNUSED_ARG(struct ddsi_thread_state * const thrst),
  UNUSED_ARG(struct ddsi_domaingv *gv),
  UNUSED_ARG(struct ddsi_rmsg **rmsg),
  UNUSED_ARG(ddsi_rtps_header_t **hdr),
  UNUSED_ARG(unsigned char **buff),
  UNUSED_ARG(size_t *sz),
  UNUSED_ARG(struct ddsi_rbufpool *rbpool),
  UNUSED_ARG(bool isstream));

extern inline int64_t ddsi_omg_security_get_remote_participant_handle (UNUSED_ARG(struct ddsi_proxy_participant *proxypp));

extern inline bool ddsi_omg_reader_is_discovery_protected (UNUSED_ARG(const struct ddsi_reader *rd));

extern inline bool ddsi_omg_reader_is_submessage_protected (UNUSED_ARG(const struct ddsi_reader *rd));

extern inline bool ddsi_omg_plist_keyhash_is_protected (UNUSED_ARG(const ddsi_plist_t *plist));

extern inline bool ddsi_omg_is_endpoint_protected (UNUSED_ARG(const ddsi_plist_t *plist));

extern inline void ddsi_omg_log_endpoint_protection (UNUSED_ARG(struct ddsi_domaingv * const gv), UNUSED_ARG(const ddsi_plist_t *plist));


#endif /* DDS_HAS_SECURITY */
