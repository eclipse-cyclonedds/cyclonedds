// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/dds_security_api_types.h"
#include "dds/security/core/dds_security_timed_cb.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/core/dds_security_shared_secret.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/core/dds_security_serialize.h"
#include "dds/security/openssl_support.h"
#include "auth_utils.h"
#include "authentication.h"
#include "auth_tokens.h"
#include "ac_tokens.h"

#ifndef EVP_PKEY_id
#define EVP_PKEY_id(k) ((k)->type)
#endif

#define HANDSHAKE_SIGNATURE_CONTENT_SIZE 6
#define ADJUSTED_GUID_PREFIX_FLAG 0x80

typedef unsigned char HashValue_t[SHA256_DIGEST_LENGTH];

static const char *AUTH_DSIG_ALGO_RSA_2048_SHA256_IDENT = "RSASSA-PSS-SHA256";
static const char *AUTH_DSIG_ALGO_ECDSA_SHA256_IDENT = "ECDSA-SHA256";
static const char *AUTH_KAGREE_ALGO_RSA_2048_SHA256_IDENT = "DH+MODP-2048-256";
static const char *AUTH_KAGREE_ALGO_ECDH_PRIME256V1_IDENT = "ECDH+prime256v1-CEUM";

typedef enum
{
  SECURITY_OBJECT_KIND_UNKNOWN,
  SECURITY_OBJECT_KIND_LOCAL_IDENTITY,
  SECURITY_OBJECT_KIND_REMOTE_IDENTITY,
  SECURITY_OBJECT_KIND_IDENTITY_RELATION,
  SECURITY_OBJECT_KIND_HANDSHAKE
} SecurityObjectKind_t;

typedef enum
{
  CREATEDREQUEST,
  CREATEDREPLY
} CreatedHandshakeStep_t;

typedef struct SecurityObject SecurityObject;
typedef void (*SecurityObjectDestructor)(SecurityObject *obj);

struct SecurityObject
{
  int64_t handle;
  SecurityObjectKind_t kind;
  SecurityObjectDestructor destructor;
};

#ifndef NDEBUG
#define CHECK_OBJECT_KIND(o, k) assert(security_object_valid((SecurityObject *)(o), k))
#else
#define CHECK_OBJECT_KIND(o, k)
#endif

#define SECURITY_OBJECT(o) ((SecurityObject *)(o))
#define SECURITY_OBJECT_HANDLE(o) (SECURITY_OBJECT(o)->handle)
#define IDENTITY_HANDLE(o) ((DDS_Security_IdentityHandle)SECURITY_OBJECT_HANDLE(o))
#define HANDSHAKE_HANDLE(o) ((DDS_Security_HandshakeHandle)SECURITY_OBJECT_HANDLE(o))

#define SECURITY_OBJECT_VALID(o, k) security_object_valid((SecurityObject *)(o), k)

typedef struct LocalIdentityInfo
{
  SecurityObject _parent;
  DDS_Security_DomainId domainId;
  DDS_Security_GUID_t candidateGUID;
  DDS_Security_GUID_t adjustedGUID;
  X509 *identityCert;
  X509 *identityCA;
  EVP_PKEY *privateKey;
  X509_CRL *crl;
  DDS_Security_OctetSeq pdata;
  AuthenticationAlgoKind_t dsignAlgoKind;
  AuthenticationAlgoKind_t kagreeAlgoKind;
  char *permissionsDocument;
  dds_security_time_event_handle_t timer;
} LocalIdentityInfo;

typedef struct RemoteIdentityInfo
{
  SecurityObject _parent;
  DDS_Security_GUID_t guid;
  X509 *identityCert;
  AuthenticationAlgoKind_t dsignAlgoKind;
  AuthenticationAlgoKind_t kagreeAlgoKind;
  DDS_Security_IdentityToken *remoteIdentityToken;
  DDS_Security_OctetSeq pdata;
  char *permissionsDocument;
  struct ddsrt_hh *linkHash; /* contains the IdentityRelation objects */
  dds_security_time_event_handle_t timer;
} RemoteIdentityInfo;

/* This structure contains the relation between a local and a remote identity
 * The handle for this object is the same as the handle of the associated
 * local identity object. The IdentityRelation object will be stored with the
 * remote identity.
 */
typedef struct IdentityRelation
{
  SecurityObject _parent;
  LocalIdentityInfo *localIdentity;
  RemoteIdentityInfo *remoteIdentity;
  AuthenticationChallenge *lchallenge;
  AuthenticationChallenge *rchallenge;
} IdentityRelation;

typedef struct HandshakeInfo
{
  SecurityObject _parent;
  IdentityRelation *relation;
  HashValue_t hash_c1;
  HashValue_t hash_c2;
  EVP_PKEY *ldh;
  EVP_PKEY *rdh;
  DDS_Security_SharedSecretHandleImpl *shared_secret_handle_impl;
  CreatedHandshakeStep_t created_in;
} HandshakeInfo;

typedef struct dds_security_authentication_impl
{
  dds_security_authentication base;
  ddsrt_mutex_t lock;
  struct ddsrt_hh *objectHash;
  struct ddsrt_hh *remoteGuidHash;
  struct dds_security_timed_dispatcher *dispatcher;
  const dds_security_authentication_listener *listener;
  X509Seq trustedCAList;
  bool include_optional;
} dds_security_authentication_impl;

/* data type for timer dispatcher */
typedef struct
{
  dds_security_authentication_impl *auth;
  DDS_Security_IdentityHandle hdl;
} validity_cb_info;

static bool security_object_valid(SecurityObject *obj, SecurityObjectKind_t kind)
{
  if (!obj || obj->kind != kind)
    return false;
  if (kind == SECURITY_OBJECT_KIND_IDENTITY_RELATION)
  {
    IdentityRelation *relation = (IdentityRelation *)obj;
    if (!relation->localIdentity || !relation->remoteIdentity || (ddsrt_address)obj->handle != (ddsrt_address)relation->localIdentity)
      return false;
  }
  else if ((ddsrt_address)obj->handle != (ddsrt_address)obj)
    return false;
  return true;
}

static uint32_t security_object_hash(const void *obj)
{
  const SecurityObject *object = obj;
  const uint64_t c = UINT64_C (16292676669999574021);
  const uint32_t x = (uint32_t)object->handle;
  return (uint32_t)((x * c) >> 32);
}

static int security_object_equal(const void *ha, const void *hb)
{
  const SecurityObject *la = ha;
  const SecurityObject *lb = hb;
  return la->handle == lb->handle;
}

static SecurityObject *security_object_find(const struct ddsrt_hh *hh, int64_t handle)
{
  struct SecurityObject template;
  template.handle = handle;
  return (SecurityObject *)ddsrt_hh_lookup(hh, &template);
}

static void security_object_init(SecurityObject *obj, SecurityObjectKind_t kind, SecurityObjectDestructor destructor)
{
  assert(obj);
  obj->kind = kind;
  obj->handle = (int64_t)(ddsrt_address)obj;
  obj->destructor = destructor;
}

static void security_object_deinit(SecurityObject *obj)
{
  assert(obj);
  obj->handle = DDS_SECURITY_HANDLE_NIL;
  obj->kind = SECURITY_OBJECT_KIND_UNKNOWN;
  obj->destructor = NULL;
}

static void security_object_free(SecurityObject *obj)
{
  assert(obj);
  if (obj && obj->destructor)
    obj->destructor(obj);
}

static void local_identity_info_free(SecurityObject *obj)
{
  LocalIdentityInfo *identity = (LocalIdentityInfo *)obj;
  CHECK_OBJECT_KIND(obj, SECURITY_OBJECT_KIND_LOCAL_IDENTITY);
  if (identity)
  {
    if (identity->identityCert)
      X509_free(identity->identityCert);
    if (identity->identityCA)
      X509_free(identity->identityCA);
    if (identity->privateKey)
      EVP_PKEY_free(identity->privateKey);
    if (identity->crl)
      X509_CRL_free(identity->crl);
    ddsrt_free(identity->pdata._buffer);
    ddsrt_free(identity->permissionsDocument);
    security_object_deinit((SecurityObject *)identity);
    ddsrt_free(identity);
  }
}

static LocalIdentityInfo *local_identity_info_new(DDS_Security_DomainId domainId, X509 *identityCert, X509 *identityCa, EVP_PKEY *privateKey, X509_CRL *crl, const DDS_Security_GUID_t *candidate_participant_guid, const DDS_Security_GUID_t *adjusted_participant_guid)
{
  LocalIdentityInfo *identity = NULL;
  assert(identityCert);
  assert(identityCa);
  assert(privateKey);
  assert(candidate_participant_guid);
  assert(adjusted_participant_guid);
  assert(sizeof(DDS_Security_IdentityHandle) == 8);

  identity = ddsrt_malloc(sizeof(*identity));
  memset(identity, 0, sizeof(*identity));

  security_object_init((SecurityObject *)identity, SECURITY_OBJECT_KIND_LOCAL_IDENTITY, local_identity_info_free);

  identity->domainId = domainId;
  identity->identityCert = identityCert;
  identity->identityCA = identityCa;
  identity->privateKey = privateKey;
  identity->crl = crl;
  identity->permissionsDocument = NULL;
  identity->dsignAlgoKind = get_authentication_algo_kind(identityCert);
  identity->kagreeAlgoKind = AUTH_ALGO_KIND_EC_PRIME256V1;

  memcpy(&identity->candidateGUID, candidate_participant_guid, sizeof(DDS_Security_GUID_t));
  memcpy(&identity->adjustedGUID, adjusted_participant_guid, sizeof(DDS_Security_GUID_t));

  return identity;
}

static uint32_t remote_guid_hash(const void *obj)
{
  const RemoteIdentityInfo *identity = obj;
  uint32_t tmp[4];
  memcpy(tmp, &identity->guid, sizeof(tmp));
  return (tmp[0] ^ tmp[1] ^ tmp[2] ^ tmp[3]);
}

static int remote_guid_equal(const void *ha, const void *hb)
{
  const RemoteIdentityInfo *la = ha;
  const RemoteIdentityInfo *lb = hb;
  return memcmp(&la->guid, &lb->guid, sizeof(la->guid)) == 0;
}

static RemoteIdentityInfo *find_remote_identity_by_guid(const struct ddsrt_hh *hh, const DDS_Security_GUID_t *guid)
{
  struct RemoteIdentityInfo template;
  memcpy(&template.guid, guid, sizeof(*guid));
  return (RemoteIdentityInfo *)ddsrt_hh_lookup(hh, &template);
}

static void remote_identity_info_free(SecurityObject *obj)
{
  RemoteIdentityInfo *identity = (RemoteIdentityInfo *)obj;
  CHECK_OBJECT_KIND(obj, SECURITY_OBJECT_KIND_REMOTE_IDENTITY);
  if (identity)
  {
    if (identity->identityCert)
      X509_free(identity->identityCert);
    DDS_Security_DataHolder_free(identity->remoteIdentityToken);
    ddsrt_hh_free(identity->linkHash);
    ddsrt_free(identity->pdata._buffer);
    ddsrt_free(identity->permissionsDocument);
    security_object_deinit((SecurityObject *)identity);
    ddsrt_free(identity);
  }
}

static RemoteIdentityInfo *remote_identity_info_new(const DDS_Security_GUID_t *guid, const DDS_Security_IdentityToken *remote_identity_token)
{
  assert(guid);
  assert(remote_identity_token);

  RemoteIdentityInfo *identity = ddsrt_malloc(sizeof(*identity));
  memset(identity, 0, sizeof(*identity));
  security_object_init((SecurityObject *)identity, SECURITY_OBJECT_KIND_REMOTE_IDENTITY, remote_identity_info_free);
  memcpy(&identity->guid, guid, sizeof(DDS_Security_GUID_t));
  identity->remoteIdentityToken = DDS_Security_DataHolder_alloc();
  DDS_Security_DataHolder_copy(identity->remoteIdentityToken, remote_identity_token);
  identity->identityCert = NULL;
  identity->dsignAlgoKind = AUTH_ALGO_KIND_UNKNOWN;
  identity->kagreeAlgoKind = AUTH_ALGO_KIND_UNKNOWN;
  identity->permissionsDocument = ddsrt_strdup("");
  identity->linkHash = ddsrt_hh_new(32, security_object_hash, security_object_equal);
  return identity;
}

static void identity_relation_free(SecurityObject *obj)
{
  IdentityRelation *relation = (IdentityRelation *)obj;
  CHECK_OBJECT_KIND(obj, SECURITY_OBJECT_KIND_IDENTITY_RELATION);
  if (relation)
  {
    ddsrt_free(relation->lchallenge);
    ddsrt_free(relation->rchallenge);
    security_object_deinit((SecurityObject *)relation);
    ddsrt_free(relation);
  }
}

/* The IdentityRelation provides the association between a local and a remote
 * identity. This object manages the challenges which are created for
 * each association between a local and a remote identity.
 * The lchallenge is the challenge associated with the local identity and
 * may be set when a future challenge is communicated with the auth_request_message_token.
 * The rchallenge is the challenge received from the remote identity it may be set when
 * an auth_request_message_token is received from the remote identity,
 */
static IdentityRelation *identity_relation_new(LocalIdentityInfo *localIdentity, RemoteIdentityInfo *remoteIdentity, AuthenticationChallenge *lchallenge, AuthenticationChallenge *rchallenge)
{
  IdentityRelation *relation;
  assert(localIdentity);
  assert(remoteIdentity);
  relation = ddsrt_malloc(sizeof(*relation));
  memset(relation, 0, sizeof(*relation));
  security_object_init((SecurityObject *)relation, SECURITY_OBJECT_KIND_IDENTITY_RELATION, identity_relation_free);
  relation->_parent.handle = SECURITY_OBJECT_HANDLE(localIdentity);
  relation->localIdentity = localIdentity;
  relation->remoteIdentity = remoteIdentity;
  relation->lchallenge = lchallenge;
  relation->rchallenge = rchallenge;
  return relation;
}

static void handshake_info_free(SecurityObject *obj)
{
  CHECK_OBJECT_KIND(obj, SECURITY_OBJECT_KIND_HANDSHAKE);
  HandshakeInfo *handshake = (HandshakeInfo *)obj;
  if (handshake)
  {
    if (handshake->ldh)
      EVP_PKEY_free(handshake->ldh);
    if (handshake->rdh)
      EVP_PKEY_free(handshake->rdh);
    if (handshake->shared_secret_handle_impl)
    {
      ddsrt_free(handshake->shared_secret_handle_impl->shared_secret);
      ddsrt_free(handshake->shared_secret_handle_impl);
    }
    security_object_deinit((SecurityObject *)handshake);
    ddsrt_free(handshake);
  }
}

static HandshakeInfo *handshake_info_new(LocalIdentityInfo *localIdentity, RemoteIdentityInfo *remoteIdentity, IdentityRelation *relation)
{
  assert(localIdentity);
  assert(remoteIdentity);
  DDSRT_UNUSED_ARG(localIdentity);
  DDSRT_UNUSED_ARG(remoteIdentity);
  HandshakeInfo *handshake = ddsrt_malloc(sizeof(*handshake));
  memset(handshake, 0, sizeof(*handshake));
  security_object_init((SecurityObject *)handshake, SECURITY_OBJECT_KIND_HANDSHAKE, handshake_info_free);
  handshake->relation = relation;
  handshake->shared_secret_handle_impl = NULL;
  return handshake;
}

static IdentityRelation *find_identity_relation(const RemoteIdentityInfo *remote, int64_t lid)
{
  return (IdentityRelation *)security_object_find(remote->linkHash, lid);
}

static void remove_identity_relation(RemoteIdentityInfo *remote, IdentityRelation *relation)
{
  (void)ddsrt_hh_remove(remote->linkHash, relation);
  security_object_free((SecurityObject *)relation);
}

static HandshakeInfo *find_handshake(const dds_security_authentication_impl *auth, int64_t localId, int64_t remoteId)
{
  struct ddsrt_hh_iter it;
  SecurityObject *obj;
  for (obj = ddsrt_hh_iter_first(auth->objectHash, &it); obj; obj = ddsrt_hh_iter_next(&it))
  {
    if (obj->kind == SECURITY_OBJECT_KIND_HANDSHAKE)
    {
      IdentityRelation *relation = ((HandshakeInfo *)obj)->relation;
      assert(relation);
      if (SECURITY_OBJECT_HANDLE(relation->localIdentity) == localId && SECURITY_OBJECT_HANDLE(relation->remoteIdentity) == remoteId)
        return (HandshakeInfo *)obj;
    }
  }
  return NULL;
}

static const char *get_authentication_algo(AuthenticationAlgoKind_t kind)
{
  switch (kind)
  {
  case AUTH_ALGO_KIND_RSA_2048:
    return "RSA-2048";
  case AUTH_ALGO_KIND_EC_PRIME256V1:
    return "EC-prime256v1";
  default:
    assert(0);
    return "";
  }
}

static const char *get_dsign_algo(AuthenticationAlgoKind_t kind)
{
  switch (kind)
  {
  case AUTH_ALGO_KIND_RSA_2048:
    return AUTH_DSIG_ALGO_RSA_2048_SHA256_IDENT;
  case AUTH_ALGO_KIND_EC_PRIME256V1:
    return AUTH_DSIG_ALGO_ECDSA_SHA256_IDENT;
  default:
    assert(0);
    return "";
  }
}

static const char *get_kagree_algo(AuthenticationAlgoKind_t kind)
{
  switch (kind)
  {
  case AUTH_ALGO_KIND_RSA_2048:
    return AUTH_KAGREE_ALGO_RSA_2048_SHA256_IDENT;
  case AUTH_ALGO_KIND_EC_PRIME256V1:
    return AUTH_KAGREE_ALGO_ECDH_PRIME256V1_IDENT;
  default:
    assert(0);
    return "";
  }
}

static bool str_octseq_equal (const char *str, const DDS_Security_OctetSeq *binstr)
{
  size_t i;
  for (i = 0; str[i] && i < binstr->_length; i++)
    if ((unsigned char) str[i] != binstr->_buffer[i])
      return false;
  /* allow zero-termination in binstr, but disallow anything other than a single \0 */
  return (str[i] == 0 &&
          (i == binstr->_length ||
           (i+1 == binstr->_length && binstr->_buffer[i] == 0)));
}

static AuthenticationAlgoKind_t get_dsign_algo_from_octseq(const DDS_Security_OctetSeq *name)
{
  if (str_octseq_equal(AUTH_DSIG_ALGO_RSA_2048_SHA256_IDENT, name))
      return AUTH_ALGO_KIND_RSA_2048;
  if (str_octseq_equal(AUTH_DSIG_ALGO_ECDSA_SHA256_IDENT, name))
    return AUTH_ALGO_KIND_EC_PRIME256V1;
  return AUTH_ALGO_KIND_UNKNOWN;
}

static AuthenticationAlgoKind_t get_kagree_algo_from_octseq(const DDS_Security_OctetSeq *name)
{
  if (str_octseq_equal(AUTH_KAGREE_ALGO_RSA_2048_SHA256_IDENT, name))
    return AUTH_ALGO_KIND_RSA_2048;
  if (str_octseq_equal(AUTH_KAGREE_ALGO_ECDH_PRIME256V1_IDENT, name))
    return AUTH_ALGO_KIND_EC_PRIME256V1;
  return AUTH_ALGO_KIND_UNKNOWN;
}

static void free_binary_properties(DDS_Security_BinaryProperty_t *seq, uint32_t length)
{
  assert(seq);
  for (uint32_t i = 0; i < length; i++)
  {
    ddsrt_free(seq[i].name);
    ddsrt_free(seq[i].value._buffer);
  }
  ddsrt_free(seq);
}

static void get_hash_binary_property_seq(const DDS_Security_BinaryPropertySeq *seq, unsigned char hash[SHA256_DIGEST_LENGTH])
{
  unsigned char *buffer;
  size_t size;
  DDS_Security_Serializer serializer = DDS_Security_Serializer_new(4096, 4096);
  DDS_Security_Serialize_BinaryPropertySeq(serializer, seq);
  DDS_Security_Serializer_buffer(serializer, &buffer, &size);
  SHA256(buffer, size, hash);
  ddsrt_free(buffer);
  DDS_Security_Serializer_free(serializer);
}

static DDS_Security_ValidationResult_t create_validate_signature_impl(bool create, EVP_PKEY *pkey, const DDS_Security_BinaryProperty_t **bprops,
    const uint32_t n_bprops, unsigned char **signature, size_t *signature_len, DDS_Security_SecurityException *ex)
{
  DDS_Security_ValidationResult_t result;
  unsigned char *buffer;
  size_t size;
  DDS_Security_Serializer serializer = DDS_Security_Serializer_new(4096, 4096);
  DDS_Security_Serialize_BinaryPropertyArray(serializer, bprops, n_bprops);
  DDS_Security_Serializer_buffer(serializer, &buffer, &size);
  result = create_validate_asymmetrical_signature(create, pkey, buffer, size, signature, signature_len, ex);
  ddsrt_free(buffer);
  DDS_Security_Serializer_free(serializer);
  return result;
}

static DDS_Security_ValidationResult_t create_signature(EVP_PKEY *pkey, const DDS_Security_BinaryProperty_t **bprops,
    const uint32_t n_bprops, unsigned char **signature, size_t *signature_len, DDS_Security_SecurityException *ex)
{
  return create_validate_signature_impl(true, pkey, bprops, n_bprops, signature, signature_len, ex);
}

static DDS_Security_ValidationResult_t validate_signature(EVP_PKEY *pkey, const DDS_Security_BinaryProperty_t **bprops,
    const uint32_t n_bprops, const unsigned char *signature, size_t signature_len, DDS_Security_SecurityException *ex)
{
  unsigned char *s = (unsigned char *)signature;
  size_t s_len = signature_len;
  return create_validate_signature_impl(false, pkey, bprops, n_bprops, &s, &s_len, ex);
}

static DDS_Security_ValidationResult_t compute_hash_value(HashValue_t value, const DDS_Security_BinaryProperty_t **properties,
    const uint32_t properties_length, DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(ex);
  unsigned char *buffer;
  size_t size;
  DDS_Security_Serializer serializer = DDS_Security_Serializer_new(4096, 4096);
  DDS_Security_Serialize_BinaryPropertyArray(serializer, properties, properties_length);
  DDS_Security_Serializer_buffer(serializer, &buffer, &size);
  SHA256(buffer, size, value);
  ddsrt_free(buffer);
  DDS_Security_Serializer_free(serializer);
  return DDS_SECURITY_VALIDATION_OK;
}

static DDS_Security_BinaryProperty_t *hash_value_to_binary_property(const char *name, HashValue_t hash)
{
  DDS_Security_BinaryProperty_t *bp = DDS_Security_BinaryProperty_alloc();
  DDS_Security_BinaryProperty_set_by_value(bp, name, hash, sizeof(HashValue_t));
  return bp;
}

static SecurityObject *get_identity_info(dds_security_authentication_impl *auth, DDS_Security_IdentityHandle handle)
{
  SecurityObject *obj;

  ddsrt_mutex_lock(&auth->lock);
  if ((obj = security_object_find(auth->objectHash, handle)) != NULL)
  {
    if ((obj->kind != SECURITY_OBJECT_KIND_LOCAL_IDENTITY) && (obj->kind != SECURITY_OBJECT_KIND_REMOTE_IDENTITY))
      obj = NULL;
  }
  ddsrt_mutex_unlock(&auth->lock);
  return obj;
}

static void validity_callback(dds_security_time_event_handle_t timer, dds_time_t trigger_time, dds_security_timed_cb_kind_t kind, void *arg)
{
  DDSRT_UNUSED_ARG(timer);
  DDSRT_UNUSED_ARG(trigger_time);

  assert(arg);
  validity_cb_info *info = arg;
  if (kind == DDS_SECURITY_TIMED_CB_KIND_TIMEOUT)
  {
    assert(info->auth->listener);
    SecurityObject * obj = get_identity_info(info->auth, info->hdl);
    if (obj)
    {
      const dds_security_authentication_listener *auth_listener = (dds_security_authentication_listener *)info->auth->listener;
      if (auth_listener->on_revoke_identity)
        auth_listener->on_revoke_identity((dds_security_authentication *)info->auth, info->hdl);
      if (obj->kind == SECURITY_OBJECT_KIND_LOCAL_IDENTITY)
        ((LocalIdentityInfo *)obj)->timer = 0;
      else
        ((RemoteIdentityInfo *)obj)->timer = 0;
    }
  }
  ddsrt_free(arg);
}

static dds_security_time_event_handle_t add_validity_end_trigger(dds_security_authentication_impl *auth, const DDS_Security_IdentityHandle identity_handle, dds_time_t end)
{
  validity_cb_info *arg = ddsrt_malloc(sizeof(validity_cb_info));
  arg->auth = auth;
  arg->hdl = identity_handle;
  return dds_security_timed_dispatcher_add(auth->dispatcher, validity_callback, end, (void *)arg);
}

static DDS_Security_ValidationResult_t get_adjusted_participant_guid(X509 *cert, const DDS_Security_GUID_t *candidate, DDS_Security_GUID_t *adjusted, DDS_Security_SecurityException *ex)
{
  unsigned char high[SHA256_DIGEST_LENGTH], low[SHA256_DIGEST_LENGTH];
  unsigned char *subject = NULL;
  size_t size = 0;

  assert(cert);
  assert(candidate);
  assert(adjusted);

  if (get_subject_name_DER_encoded(cert, &subject, &size, ex) != DDS_SECURITY_VALIDATION_OK)
    return DDS_SECURITY_VALIDATION_FAILED;

  DDS_Security_octet hb = ADJUSTED_GUID_PREFIX_FLAG;
  SHA256(subject, size, high);
  SHA256(&candidate->prefix[0], sizeof(DDS_Security_GuidPrefix_t), low);
  adjusted->entityId = candidate->entityId;
  for (int i = 0; i < 6; i++)
  {
    adjusted->prefix[i] = hb | high[i] >> 1;
    hb = (DDS_Security_octet)(high[i] << 7);
  }
  for (int i = 0; i < 6; i++)
    adjusted->prefix[i + 6] = low[i];
  ddsrt_free(subject);
  return DDS_SECURITY_VALIDATION_OK;
}
#undef ADJUSTED_GUID_PREFIX_FLAG

DDS_Security_ValidationResult_t validate_local_identity(dds_security_authentication *instance, DDS_Security_IdentityHandle *local_identity_handle,
    DDS_Security_GUID_t *adjusted_participant_guid, const DDS_Security_DomainId domain_id, const DDS_Security_Qos *participant_qos,
    const DDS_Security_GUID_t *candidate_participant_guid, DDS_Security_SecurityException *ex)
{
  if (!instance || !local_identity_handle || !adjusted_participant_guid || !participant_qos || !candidate_participant_guid)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "validate_local_identity: Invalid parameter provided");
    return DDS_SECURITY_VALIDATION_FAILED;
  }

  dds_security_authentication_impl *implementation = (dds_security_authentication_impl *)instance;
  LocalIdentityInfo *identity;
  char *identityCertPEM, *identityCaPEM, *privateKeyPEM, *password, *trusted_ca_dir, *crlPEM;
  X509 *identityCert, *identityCA;
  X509_CRL *crl = NULL;
  EVP_PKEY *privateKey;
  dds_time_t certExpiry = DDS_TIME_INVALID;

  if (!(identityCertPEM = DDS_Security_Property_get_value(&participant_qos->property.value, DDS_SEC_PROP_AUTH_IDENTITY_CERT)))
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "validate_local_identity: missing property '%s'", DDS_SEC_PROP_AUTH_IDENTITY_CERT);
    goto err_no_identity_cert;
  }

  if (!(identityCaPEM = DDS_Security_Property_get_value(&participant_qos->property.value, DDS_SEC_PROP_AUTH_IDENTITY_CA)))
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "validate_local_identity: missing property '%s'", DDS_SEC_PROP_AUTH_IDENTITY_CA);
    goto err_no_identity_ca;
  }

  if (!(privateKeyPEM = DDS_Security_Property_get_value(&participant_qos->property.value, DDS_SEC_PROP_AUTH_PRIV_KEY)))
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "validate_local_identity: missing property '%s'", DDS_SEC_PROP_AUTH_PRIV_KEY);
    goto err_no_private_key;
  }

  password = DDS_Security_Property_get_value(&participant_qos->property.value, DDS_SEC_PROP_AUTH_PASSWORD);

  trusted_ca_dir = DDS_Security_Property_get_value(&participant_qos->property.value, DDS_SEC_PROP_ACCESS_TRUSTED_CA_DIR);
  if (trusted_ca_dir && strlen(trusted_ca_dir) > 0)
  {
    if (get_trusted_ca_list(trusted_ca_dir, &(implementation->trustedCAList), ex) != DDS_SECURITY_VALIDATION_OK)
      goto err_inv_trusted_ca_dir;
  }

  crlPEM = DDS_Security_Property_get_value(&participant_qos->property.value, ORG_ECLIPSE_CYCLONEDDS_SEC_AUTH_CRL);

  if (load_X509_certificate(identityCaPEM, &identityCA, ex) != DDS_SECURITY_VALIDATION_OK)
    goto err_inv_identity_ca;

  /* check for CA if listed in trusted CA files */
  if (implementation->trustedCAList.length > 0)
  {
    if (crlPEM)
    {
      // FIXME: When a CRL is specified, we assume that it is associated with the "own_ca".  However, when
      // a list of CAs is presented that assumption may not hold.  Resolve this ambiguity for now by just
      // failing if both a CRL and a list of CAs is presented.  We can fix this in the future by allowing a list of CRLs.
      DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "Cannot specify both CRL and trusted_ca_list");
      goto err_identity_ca_not_trusted;
    }
    const EVP_MD *digest = EVP_get_digestbyname("sha1");
    uint32_t size;
    unsigned char hash_buffer[20], hash_buffer_trusted[20];
    DDS_Security_ValidationResult_t result = DDS_SECURITY_VALIDATION_FAILED;

    X509_digest(identityCA, digest, hash_buffer, &size);
    for (unsigned i = 0; i < implementation->trustedCAList.length; ++i)
    {
      X509_digest(implementation->trustedCAList.buffer[i], digest, hash_buffer_trusted, &size);
      if (memcmp(hash_buffer_trusted, hash_buffer, 20) == 0)
      {
        result = DDS_SECURITY_VALIDATION_OK;
        break;
      }
    }
    if (result != DDS_SECURITY_VALIDATION_OK)
    {
      DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CA_NOT_TRUSTED_CODE, DDS_SECURITY_VALIDATION_FAILED, DDS_SECURITY_ERR_CA_NOT_TRUSTED_MESSAGE);
      goto err_identity_ca_not_trusted;
    }
  }
  if (load_X509_certificate(identityCertPEM, &identityCert, ex) != DDS_SECURITY_VALIDATION_OK)
    goto err_inv_identity_cert;

  if (load_X509_private_key(privateKeyPEM, password, &privateKey, ex) != DDS_SECURITY_VALIDATION_OK)
    goto err_inv_private_key;

  if (crlPEM && strlen(crlPEM) > 0)
  {
    if (load_X509_CRL(crlPEM, &crl, ex) != DDS_SECURITY_VALIDATION_OK)
    {
      goto err_inv_crl;
    }
  }

  if (verify_certificate(identityCert, identityCA, crl, ex) != DDS_SECURITY_VALIDATION_OK)
    goto err_verification_failed;

  if ((certExpiry = get_certificate_expiry(identityCert)) == DDS_TIME_INVALID)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "Expiry date of the certificate is invalid");
    goto err_verification_failed;
  }

  if (get_adjusted_participant_guid(identityCert, candidate_participant_guid, adjusted_participant_guid, ex) != DDS_SECURITY_VALIDATION_OK)
    goto err_adj_guid_failed;

  ddsrt_free(crlPEM);
  ddsrt_free(password);
  ddsrt_free(privateKeyPEM);
  ddsrt_free(identityCaPEM);
  ddsrt_free(identityCertPEM);
  ddsrt_free(trusted_ca_dir);

  identity = local_identity_info_new(domain_id, identityCert, identityCA, privateKey, crl, candidate_participant_guid, adjusted_participant_guid);
  *local_identity_handle = IDENTITY_HANDLE(identity);

  if (certExpiry != DDS_NEVER)
    identity->timer = add_validity_end_trigger(implementation, *local_identity_handle, certExpiry);

  ddsrt_mutex_lock(&implementation->lock);
  (void)ddsrt_hh_add(implementation->objectHash, identity);
  ddsrt_mutex_unlock(&implementation->lock);
  return DDS_SECURITY_VALIDATION_OK;

err_adj_guid_failed:
err_verification_failed:
  if (crl)
  {
    X509_CRL_free(crl);
  }
err_inv_crl:
  EVP_PKEY_free(privateKey);
err_inv_private_key:
  X509_free(identityCert);
err_inv_identity_cert:
err_identity_ca_not_trusted:
  X509_free(identityCA);
err_inv_identity_ca:
  ddsrt_free(crlPEM);
err_inv_trusted_ca_dir:
  ddsrt_free(password);
  ddsrt_free(privateKeyPEM);
  ddsrt_free(trusted_ca_dir);
err_no_private_key:
  ddsrt_free(identityCaPEM);
err_no_identity_ca:
  ddsrt_free(identityCertPEM);
err_no_identity_cert:
  return DDS_SECURITY_VALIDATION_FAILED;
}

DDS_Security_boolean get_identity_token(dds_security_authentication *instance, DDS_Security_IdentityToken *identity_token, const DDS_Security_IdentityHandle handle, DDS_Security_SecurityException *ex)
{
  if (!instance || !identity_token)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "get_identity_token: Invalid parameter provided");
    return false;
  }

  dds_security_authentication_impl *impl = (dds_security_authentication_impl *)instance;
  SecurityObject *obj;
  LocalIdentityInfo *identity;
  char *snCert, *snCA;
  memset(identity_token, 0, sizeof(*identity_token));

  ddsrt_mutex_lock(&impl->lock);

  obj = security_object_find(impl->objectHash, handle);
  if (!obj || !security_object_valid(obj, SECURITY_OBJECT_KIND_LOCAL_IDENTITY))
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "get_identity_token: Invalid handle provided");
    goto err_inv_handle;
  }
  identity = (LocalIdentityInfo *)obj;
  if (!(snCert = get_certificate_subject_name(identity->identityCert, ex)))
    goto err_sn_cert;
  if (!(snCA = get_certificate_subject_name(identity->identityCA, ex)))
    goto err_sn_ca;

  identity_token->class_id = ddsrt_strdup(DDS_SECURITY_AUTH_TOKEN_CLASS_ID);
  identity_token->properties._length = 4;
  identity_token->properties._buffer = DDS_Security_PropertySeq_allocbuf(4);
  identity_token->properties._buffer[0].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_CERT_SN);
  identity_token->properties._buffer[0].value = snCert;
  identity_token->properties._buffer[1].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_CERT_ALGO);
  identity_token->properties._buffer[1].value = ddsrt_strdup(get_authentication_algo(get_authentication_algo_kind(identity->identityCert)));
  identity_token->properties._buffer[2].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_CA_SN);
  identity_token->properties._buffer[2].value = snCA;
  identity_token->properties._buffer[3].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_CA_ALGO);
  identity_token->properties._buffer[3].value = ddsrt_strdup(get_authentication_algo(get_authentication_algo_kind(identity->identityCA)));

  ddsrt_mutex_unlock(&impl->lock);
  return true;

err_sn_ca:
  ddsrt_free(snCert);
err_sn_cert:
err_inv_handle:
  ddsrt_mutex_unlock(&impl->lock);
  return false;
}

DDS_Security_boolean get_identity_status_token(dds_security_authentication *instance, DDS_Security_IdentityStatusToken *identity_status_token, const DDS_Security_IdentityHandle handle, DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(identity_status_token);
  DDSRT_UNUSED_ARG(handle);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

DDS_Security_boolean set_permissions_credential_and_token(dds_security_authentication *instance, const DDS_Security_IdentityHandle handle,
    const DDS_Security_PermissionsCredentialToken *permissions_credential, const DDS_Security_PermissionsToken *permissions_token, DDS_Security_SecurityException *ex)
{
  if (!instance || handle == DDS_SECURITY_HANDLE_NIL || !permissions_credential || !permissions_token)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "get_identity_token: Invalid parameter provided");
    return false;
  }
  if (!permissions_credential->class_id || strcmp(permissions_credential->class_id, DDS_ACTOKEN_PERMISSIONS_CREDENTIAL_CLASS_ID) != 0)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "get_identity_token: Invalid parameter provided");
    return false;
  }
  if (permissions_credential->properties._length == 0 || permissions_credential->properties._buffer[0].name == NULL
      || strcmp(permissions_credential->properties._buffer[0].name, DDS_ACTOKEN_PROP_PERM_CERT) != 0)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "get_identity_token: Invalid parameter provided");
    return false;
  }

  dds_security_authentication_impl *impl = (dds_security_authentication_impl *)instance;
  LocalIdentityInfo *identity;

  ddsrt_mutex_lock(&impl->lock);
  identity = (LocalIdentityInfo *)security_object_find(impl->objectHash, handle);
  if (!identity || !SECURITY_OBJECT_VALID(identity, SECURITY_OBJECT_KIND_LOCAL_IDENTITY))
  {
    ddsrt_mutex_unlock(&impl->lock);
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "get_identity_token: Invalid handle provided");
    return false;
  }
  identity->permissionsDocument = ddsrt_strdup(permissions_credential->properties._buffer[0].value ? permissions_credential->properties._buffer[0].value : "");
  ddsrt_mutex_unlock(&impl->lock);
  return true;
}

static DDS_Security_ValidationResult_t validate_remote_identity_token(const LocalIdentityInfo *localIdent, const DDS_Security_IdentityToken *token, DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(localIdent);
  if (!token->class_id)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "remote identity token: class_id is empty");
    return DDS_SECURITY_VALIDATION_FAILED;
  }

  const size_t class_id_base_len = strlen(DDS_SECURITY_AUTH_TOKEN_CLASS_ID_BASE);
  if (strncmp(DDS_SECURITY_AUTH_TOKEN_CLASS_ID_BASE, token->class_id, class_id_base_len) != 0)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "remote identity token: class_id='%s' not supported", token->class_id);
    return DDS_SECURITY_VALIDATION_FAILED;
  }

  char const * const received_version_str = token->class_id + class_id_base_len;
  unsigned major, minor;
  int postfix_pos;
  DDSRT_WARNING_MSVC_OFF(4996);
  if (sscanf(received_version_str, "%u.%u%n", &major, &minor, &postfix_pos) != 2 ||
      (received_version_str[postfix_pos] != 0 && received_version_str[postfix_pos] != '+'))
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "remote identity token: class_id has wrong format");
    return DDS_SECURITY_VALIDATION_FAILED;
  }
  DDSRT_WARNING_MSVC_ON(4996);

  if (major != DDS_SECURITY_AUTH_VERSION_MAJOR || minor > DDS_SECURITY_AUTH_VERSION_MINOR)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "remote identity token: version %u.%u not supported", major, minor);
    return DDS_SECURITY_VALIDATION_FAILED;
  }
  return DDS_SECURITY_VALIDATION_OK;
}

static DDS_Security_ValidationResult_t validate_auth_request_token(const DDS_Security_IdentityToken *token, AuthenticationChallenge **challenge, DDS_Security_SecurityException *ex)
{
  uint32_t index;
  int found = 0;
  assert(token);
  if (!token->class_id)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "AuthRequestMessageToken invalid: missing class_id");
    return DDS_SECURITY_VALIDATION_FAILED;
  }
  if (strncmp(token->class_id, DDS_SECURITY_AUTH_REQUEST_TOKEN_CLASS_ID, strlen(DDS_SECURITY_AUTH_REQUEST_TOKEN_CLASS_ID)) != 0)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "AuthRequestMessageToken invalid: class_id '%s' is invalid", token->class_id);
    return DDS_SECURITY_VALIDATION_FAILED;
  }
  if (!token->binary_properties._buffer)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "AuthRequestMessageToken invalid: properties are missing");
    return DDS_SECURITY_VALIDATION_FAILED;
  }

  for (index = 0; index < token->binary_properties._length; index++)
  {
    size_t len = strlen(DDS_AUTHTOKEN_PROP_FUTURE_CHALLENGE);
    if (token->binary_properties._buffer[index].name && strncmp(token->binary_properties._buffer[index].name, DDS_AUTHTOKEN_PROP_FUTURE_CHALLENGE, len) == 0)
    {
      found = 1;
      break;
    }
  }
  if (!found)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "AuthRequestMessageToken invalid: future_challenge not found");
    return DDS_SECURITY_VALIDATION_FAILED;
  }

  if (token->binary_properties._buffer[index].value._length != sizeof(AuthenticationChallenge)
      || !token->binary_properties._buffer[index].value._buffer)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "AuthRequestMessageToken invalid: future_challenge invalid size");
    return DDS_SECURITY_VALIDATION_FAILED;
  }

  if (challenge)
  {
    *challenge = ddsrt_malloc(sizeof(AuthenticationChallenge));
    memcpy(*challenge, &token->binary_properties._buffer[index].value._buffer[0], sizeof(AuthenticationChallenge));
  }

  return DDS_SECURITY_VALIDATION_OK;
}

static void fill_auth_request_token(DDS_Security_AuthRequestMessageToken *token, AuthenticationChallenge *challenge)
{
  uint32_t len = sizeof(challenge->value);
  DDS_Security_DataHolder_deinit(token);
  token->class_id = ddsrt_strdup(DDS_SECURITY_AUTH_REQUEST_TOKEN_CLASS_ID);
  token->binary_properties._length = 1;
  token->binary_properties._buffer = DDS_Security_BinaryPropertySeq_allocbuf(1);
  token->binary_properties._buffer->name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_FUTURE_CHALLENGE);
  token->binary_properties._buffer->value._length = len;
  token->binary_properties._buffer->value._buffer = ddsrt_malloc(len);
  memcpy(token->binary_properties._buffer->value._buffer, challenge->value, len);
  token->binary_properties._buffer->propagate = true;
}

DDS_Security_ValidationResult_t validate_remote_identity(dds_security_authentication *instance, DDS_Security_IdentityHandle *remote_identity_handle,
    DDS_Security_AuthRequestMessageToken *local_auth_request_token, const DDS_Security_AuthRequestMessageToken *remote_auth_request_token, const DDS_Security_IdentityHandle local_identity_handle,
    const DDS_Security_IdentityToken *remote_identity_token, const DDS_Security_GUID_t *remote_participant_guid, DDS_Security_SecurityException *ex)
{
  if (!instance || !remote_identity_handle || !local_auth_request_token || !remote_identity_token || !remote_participant_guid)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "validate_remote_identity: Invalid parameter provided");
    return DDS_SECURITY_VALIDATION_FAILED;
  }

  dds_security_authentication_impl *impl = (dds_security_authentication_impl *)instance;
  SecurityObject *obj;
  LocalIdentityInfo *localIdent;
  RemoteIdentityInfo *remoteIdent;
  IdentityRelation *relation;
  AuthenticationChallenge *lchallenge = NULL, *rchallenge = NULL;

  ddsrt_mutex_lock(&impl->lock);
  obj = security_object_find(impl->objectHash, local_identity_handle);
  if (!obj || !security_object_valid(obj, SECURITY_OBJECT_KIND_LOCAL_IDENTITY))
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "validate_remote_identity: Invalid handle provided");
    goto err_inv_handle;
  }
  localIdent = (LocalIdentityInfo *)obj;

  if (validate_remote_identity_token(localIdent, remote_identity_token, ex) != DDS_SECURITY_VALIDATION_OK)
    goto err_remote_identity_token;

  /* When the remote_auth_request_token is not null, check if it's contents is valid and set the futureChallenge from the data contained in the remote_auth_request_token. */
  if (remote_auth_request_token)
  {
    if (validate_auth_request_token(remote_auth_request_token, &rchallenge, ex) != DDS_SECURITY_VALIDATION_OK)
      goto err_inv_auth_req_token;
  }

  if ((lchallenge = generate_challenge(ex)) == NULL)
    goto err_alloc_challenge;

  /* The validate_remote_identity will also create a handshake structure which contains the relation between
     a local an remote identity. This handshake structure is inserted in the remote identity structure. */

  /* Check if the remote identity has already been validated by a previous validation request. */
  if (!(remoteIdent = find_remote_identity_by_guid(impl->remoteGuidHash, remote_participant_guid)))
  {
    remoteIdent = remote_identity_info_new(remote_participant_guid, remote_identity_token);
    (void)ddsrt_hh_add(impl->objectHash, remoteIdent);
    (void)ddsrt_hh_add(impl->remoteGuidHash, remoteIdent);
    relation = identity_relation_new(localIdent, remoteIdent, lchallenge, rchallenge);
    (void)ddsrt_hh_add(remoteIdent->linkHash, relation);
  }
  else
  {
    /* When the remote identity has already been validated before, check if the remote identity token matches with the existing one */
    if (!DDS_Security_DataHolder_equal(remoteIdent->remoteIdentityToken, remote_identity_token))
    {
      DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "validate_remote_identity: remote_identity_token does not match with previously received one");
      goto err_inv_duplicate;
    }

    if (!(relation = find_identity_relation(remoteIdent, SECURITY_OBJECT_HANDLE(localIdent))))
    {
      relation = identity_relation_new(localIdent, remoteIdent, lchallenge, rchallenge);
      int r = ddsrt_hh_add(remoteIdent->linkHash, relation);
      assert(r);
      (void)r;
    }
    else
    {
      if (remote_auth_request_token)
      {
        assert(rchallenge);
        ddsrt_free(relation->rchallenge);
        relation->rchallenge = rchallenge;
      }
      ddsrt_free(lchallenge);
    }
  }
  ddsrt_mutex_unlock(&impl->lock);

  if (!remote_auth_request_token)
    fill_auth_request_token(local_auth_request_token, relation->lchallenge);
  else
    DDS_Security_set_token_nil(local_auth_request_token);

  *remote_identity_handle = IDENTITY_HANDLE(remoteIdent);
  return memcmp(&localIdent->adjustedGUID, &remoteIdent->guid, sizeof(DDS_Security_GUID_t)) < 0 ?
    DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_REQUEST : DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE;

err_inv_duplicate:
  ddsrt_free(lchallenge);
err_alloc_challenge:
  ddsrt_free(rchallenge);
err_inv_auth_req_token:
err_remote_identity_token:
err_inv_handle:
  ddsrt_mutex_unlock(&impl->lock);
  return DDS_SECURITY_VALIDATION_FAILED;
}

DDS_Security_ValidationResult_t begin_handshake_request(dds_security_authentication *instance, DDS_Security_HandshakeHandle *handshake_handle,
    DDS_Security_HandshakeMessageToken *handshake_message, const DDS_Security_IdentityHandle initiator_identity_handle, const DDS_Security_IdentityHandle replier_identity_handle,
    const DDS_Security_OctetSeq *serialized_local_participant_data, DDS_Security_SecurityException *ex)
{
  if (!instance || !handshake_handle || !handshake_message || !serialized_local_participant_data)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "begin_handshake_request: Invalid parameter provided");
    return DDS_SECURITY_VALIDATION_FAILED;
  }

  dds_security_authentication_impl *impl = (dds_security_authentication_impl *)instance;
  HandshakeInfo *handshake = NULL;
  IdentityRelation *relation = NULL;
  SecurityObject *obj;
  LocalIdentityInfo *localIdent;
  RemoteIdentityInfo *remoteIdent;
  EVP_PKEY *dhkey;
  unsigned char *certData, *dhPubKeyData = NULL;
  uint32_t certDataSize, dhPubKeyDataSize;
  uint32_t tokcount = impl->include_optional ? 8 : 7;
  int created = 0;

  ddsrt_mutex_lock(&impl->lock);

  obj = security_object_find(impl->objectHash, initiator_identity_handle);
  if (!obj || !security_object_valid(obj, SECURITY_OBJECT_KIND_LOCAL_IDENTITY))
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "begin_handshake_request: Invalid initiator_identity_handle provided");
    goto err_inv_handle;
  }
  localIdent = (LocalIdentityInfo *)obj;

  obj = security_object_find(impl->objectHash, replier_identity_handle);
  if (!obj || !security_object_valid(obj, SECURITY_OBJECT_KIND_REMOTE_IDENTITY))
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "begin_handshake_request: Invalid replier_identity_handle provided");
    goto err_inv_handle;
  }
  remoteIdent = (RemoteIdentityInfo *)obj;

  if (get_certificate_contents(localIdent->identityCert, &certData, &certDataSize, ex) != DDS_SECURITY_VALIDATION_OK)
    goto err_alloc_cid;

  if (!(handshake = find_handshake(impl, SECURITY_OBJECT_HANDLE(localIdent), SECURITY_OBJECT_HANDLE(remoteIdent))))
  {
    relation = find_identity_relation(remoteIdent, SECURITY_OBJECT_HANDLE(localIdent));
    assert(relation);
    handshake = handshake_info_new(localIdent, remoteIdent, relation);
    handshake->created_in = CREATEDREQUEST;
    (void)ddsrt_hh_add(impl->objectHash, handshake);
    created = 1;
  }
  else
  {
    relation = handshake->relation;
    assert(relation);
  }

  if (!handshake->ldh)
  {
    if (generate_dh_keys(&dhkey, localIdent->kagreeAlgoKind, ex) != DDS_SECURITY_VALIDATION_OK)
      goto err_gen_dh_keys;
    handshake->ldh = dhkey;
  }

  if (dh_public_key_to_oct(handshake->ldh, localIdent->kagreeAlgoKind, &dhPubKeyData, &dhPubKeyDataSize, ex) != DDS_SECURITY_VALIDATION_OK)
    goto err_get_public_key;

  if (localIdent->pdata._length == 0)
    DDS_Security_OctetSeq_copy(&localIdent->pdata, serialized_local_participant_data);

  DDS_Security_BinaryProperty_t *tokens = DDS_Security_BinaryPropertySeq_allocbuf(tokcount);
  uint32_t tokidx = 0;

  DDS_Security_BinaryProperty_set_by_ref(&tokens[tokidx++], DDS_AUTHTOKEN_PROP_C_ID, certData, certDataSize);
  DDS_Security_BinaryProperty_set_by_string(&tokens[tokidx++], DDS_AUTHTOKEN_PROP_C_PERM, localIdent->permissionsDocument ? localIdent->permissionsDocument : "");
  DDS_Security_BinaryProperty_set_by_value(&tokens[tokidx++], DDS_AUTHTOKEN_PROP_C_PDATA, serialized_local_participant_data->_buffer, serialized_local_participant_data->_length);
  DDS_Security_BinaryProperty_set_by_string(&tokens[tokidx++], DDS_AUTHTOKEN_PROP_C_DSIGN_ALGO, get_dsign_algo(localIdent->dsignAlgoKind));
  DDS_Security_BinaryProperty_set_by_string(&tokens[tokidx++], DDS_AUTHTOKEN_PROP_C_KAGREE_ALGO, get_kagree_algo(localIdent->kagreeAlgoKind));

  /* Todo: including hash_c1 is optional (conform spec); add a configuration option to leave it out */
  {
    DDS_Security_BinaryPropertySeq bseq = { ._length = 5, ._buffer = tokens };
    get_hash_binary_property_seq(&bseq, handshake->hash_c1);
    if (impl->include_optional)
      DDS_Security_BinaryProperty_set_by_value(&tokens[tokidx++], DDS_AUTHTOKEN_PROP_HASH_C1, handshake->hash_c1, sizeof(HashValue_t));
  }

  /* Set the DH public key associated with the local participant in dh1 property */
  assert(dhPubKeyData);
  assert(dhPubKeyDataSize < 1200);
  DDS_Security_BinaryProperty_set_by_ref(&tokens[tokidx++], DDS_AUTHTOKEN_PROP_DH1, dhPubKeyData, dhPubKeyDataSize);

  /* Set the challenge in challenge1 property */
  DDS_Security_BinaryProperty_set_by_value(&tokens[tokidx++], DDS_AUTHTOKEN_PROP_CHALLENGE1, relation->lchallenge->value, sizeof(AuthenticationChallenge));

  (void)ddsrt_hh_add(impl->objectHash, handshake);

  ddsrt_mutex_unlock(&impl->lock);

  assert(tokcount == tokidx);

  handshake_message->class_id = ddsrt_strdup(DDS_SECURITY_AUTH_HANDSHAKE_REQUEST_TOKEN_ID);
  handshake_message->properties._length = 0;
  handshake_message->properties._buffer = NULL;
  handshake_message->binary_properties._length = tokidx;
  handshake_message->binary_properties._buffer = tokens;
  *handshake_handle = HANDSHAKE_HANDLE(handshake);

  return DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE;

err_get_public_key:
err_gen_dh_keys:
  if (created)
  {
    (void)ddsrt_hh_remove(impl->objectHash, handshake);
    security_object_free((SecurityObject *)handshake);
  }
err_alloc_cid:
  ddsrt_free(certData);
err_inv_handle:
  ddsrt_mutex_unlock(&impl->lock);
  return DDS_SECURITY_VALIDATION_FAILED;
}

static DDS_Security_ValidationResult_t validate_pdata(const DDS_Security_OctetSeq *seq, X509 *cert, DDS_Security_SecurityException *ex)
{
  DDS_Security_ParticipantBuiltinTopicData *pdata;
  DDS_Security_GUID_t cguid, aguid;
  DDS_Security_Deserializer deserializer = DDS_Security_Deserializer_new(seq->_buffer, seq->_length);
  if (!deserializer)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "begin_handshake_reply: c.pdata invalid encoding");
    goto failed_deser;
  }

  pdata = DDS_Security_ParticipantBuiltinTopicData_alloc();
  if (!DDS_Security_Deserialize_ParticipantBuiltinTopicData(deserializer, pdata, ex))
    goto failed;

  memset(&cguid, 0, sizeof(DDS_Security_GUID_t));
  if (get_adjusted_participant_guid(cert, &cguid, &aguid, ex) != DDS_SECURITY_VALIDATION_OK)
    goto failed;

  DDS_Security_BuiltinTopicKey_t key;
  DDS_Security_BuiltinTopicKeyBE(key, pdata->key);
  if (memcmp(key, aguid.prefix, 6) != 0)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "begin_handshake_reply: c.pdata contains incorrect participant guid");
    goto failed;
  }
  DDS_Security_ParticipantBuiltinTopicData_free(pdata);
  DDS_Security_Deserializer_free(deserializer);
  return DDS_SECURITY_VALIDATION_OK;

failed:
  DDS_Security_ParticipantBuiltinTopicData_free(pdata);
  DDS_Security_Deserializer_free(deserializer);
failed_deser:
  return DDS_SECURITY_VALIDATION_FAILED;
}

enum handshake_token_type
{
  HS_TOKEN_REQ,
  HS_TOKEN_REPLY,
  HS_TOKEN_FINAL
};

static DDS_Security_ValidationResult_t set_exception (DDS_Security_SecurityException *ex, const char *fmt, ...)
  ddsrt_attribute_format_printf(2, 3) ddsrt_attribute_warn_unused_result;

static DDS_Security_ValidationResult_t set_exception (DDS_Security_SecurityException *ex, const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  DDS_Security_Exception_vset (ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, fmt, ap);
  va_end (ap);
  return DDS_SECURITY_VALIDATION_FAILED;
}

static const DDS_Security_BinaryProperty_t *find_required_binprop (const DDS_Security_HandshakeMessageToken *token, const char *name, DDS_Security_SecurityException *ex)
{
  DDS_Security_ValidationResult_t result;
  const DDS_Security_BinaryProperty_t *prop = DDS_Security_DataHolder_find_binary_property (token, name);
  if (prop == NULL)
  {
    result = set_exception (ex, "process_handshake: HandshakeMessageToken property %s missing", name);
    (void) result;
    return NULL;
  }
  else if (prop->value._length > INT_MAX)
  {
    result = set_exception (ex, "process_handshake: HandshakeMessageToken property %s has unsupported size (%"PRIu32" bytes)", name, prop->value._length);
    (void) result;
    return NULL;
  }
  return prop;
}

static const DDS_Security_BinaryProperty_t *find_required_nonempty_binprop (const DDS_Security_HandshakeMessageToken *token, const char *name, DDS_Security_SecurityException *ex)
{
  DDS_Security_ValidationResult_t result;
  const DDS_Security_BinaryProperty_t *prop = find_required_binprop (token, name, ex);
  if (prop != NULL && (prop->value._length == 0 || prop->value._buffer == NULL))
  {
    // FIXME: _buffer == NULL check must go, that should've been guaranteed before
    result = set_exception (ex, "process_handshake: HandshakeMessageToken property %s is empty", name);
    (void) result;
    return NULL;
  }
  return prop;
}

static const DDS_Security_BinaryProperty_t *find_required_binprop_exactsize (const DDS_Security_HandshakeMessageToken *token, const char *name, size_t size, DDS_Security_SecurityException *ex)
{
  DDS_Security_ValidationResult_t result;
  const DDS_Security_BinaryProperty_t *prop = find_required_binprop (token, name, ex);
  if (prop != NULL && prop->value._length != size)
  {
    result = set_exception (ex, "process_handshake: HandshakeMessageToken property %s has wrong size (%"PRIu32" while expecting %"PRIuSIZE")", name, prop->value._length, size);
    (void) result;
    return NULL;
  }
  return prop;
}

static X509 *load_X509_certificate_from_binprop (const DDS_Security_BinaryProperty_t *prop, X509 *own_ca, X509_CRL *own_crl, const X509Seq *trusted_ca_list, DDS_Security_SecurityException *ex)
{
  X509 *cert;

  if (own_crl && trusted_ca_list->length > 0)
  {
    // FIXME: When a CRL is specified, we assume that it is associated with the "own_ca".  However, when
    // a list of CAs is presented that assumption may not hold.  Resolve this ambiguity for now by just
    // failing if both a CRL and a list of CAs is presented.  We can fix this in the future by allowing a list of CRLs.
    DDS_Security_ValidationResult_t result = set_exception (ex, "load_X509_certificate_from_binprop: Cannot specify both CRL and trusted_ca_list");
    (void) result;
    return NULL;
  }

  if (load_X509_certificate_from_data ((char *) prop->value._buffer, (int) prop->value._length, &cert, ex) != DDS_SECURITY_VALIDATION_OK)
    return NULL;

  DDS_Security_ValidationResult_t result = DDS_SECURITY_VALIDATION_FAILED;
  if (trusted_ca_list->length == 0)
    result = verify_certificate (cert, own_ca, own_crl, ex);
  else
  {
    DDS_Security_Exception_clean (ex);
    for (unsigned i = 0; i < trusted_ca_list->length; ++i)
    {
      DDS_Security_Exception_reset (ex);
      if ((result = verify_certificate (cert, trusted_ca_list->buffer[i], NULL, ex)) == DDS_SECURITY_VALIDATION_OK)
        break;
    }
  }
  if (result != DDS_SECURITY_VALIDATION_OK || check_certificate_expiry (cert, ex) != DDS_SECURITY_VALIDATION_OK)
  {
    X509_free (cert);
    return NULL;
  }
  return cert;
}

static DDS_Security_BinaryProperty_t *
create_dhkey_property(const char *name, EVP_PKEY *pkey, AuthenticationAlgoKind_t kagreeAlgoKind,  DDS_Security_SecurityException *ex)
{
  DDS_Security_BinaryProperty_t *prop;
  unsigned char *data;
  uint32_t len;

  if (dh_public_key_to_oct(pkey, kagreeAlgoKind, &data, &len, ex) != DDS_SECURITY_VALIDATION_OK)
    return NULL;

  prop = DDS_Security_BinaryProperty_alloc();
  DDS_Security_BinaryProperty_set_by_ref(prop, name, data, len);
  return prop;
}

static DDS_Security_ValidationResult_t validate_handshake_token_impl (const DDS_Security_HandshakeMessageToken *token, enum handshake_token_type token_type,
    HandshakeInfo *handshake, X509Seq *trusted_ca_list, const DDS_Security_BinaryProperty_t *dh1_ref, const DDS_Security_BinaryProperty_t *dh2_ref, DDS_Security_SecurityException *ex)
{
  IdentityRelation * const relation = handshake->relation;
  X509 *identityCert = NULL;
  const DDS_Security_BinaryProperty_t *c_pdata = NULL;
  AuthenticationAlgoKind_t dsignAlgoKind = AUTH_ALGO_KIND_UNKNOWN, kagreeAlgoKind = AUTH_ALGO_KIND_UNKNOWN;
  const DDS_Security_BinaryProperty_t *dh1 = NULL, *dh2 = NULL;
  const DDS_Security_BinaryProperty_t *hash_c1 = NULL, *hash_c2 = NULL;
  const DDS_Security_BinaryProperty_t *challenge1 = NULL, *challenge2 = NULL;
  const DDS_Security_BinaryProperty_t *signature = NULL;
  const char *token_class_id = NULL;

  assert (relation);
  assert (dh1_ref != NULL || token_type == HS_TOKEN_REQ);
  assert (dh2_ref != NULL || token_type == HS_TOKEN_REQ || token_type == HS_TOKEN_REPLY);

  switch (token_type)
  {
    case HS_TOKEN_REQ: token_class_id = DDS_SECURITY_AUTH_HANDSHAKE_REQUEST_TOKEN_ID; break;
    case HS_TOKEN_REPLY: token_class_id = DDS_SECURITY_AUTH_HANDSHAKE_REPLY_TOKEN_ID; break;
    case HS_TOKEN_FINAL: token_class_id = DDS_SECURITY_AUTH_HANDSHAKE_FINAL_TOKEN_ID; break;
  }
  assert (token_class_id);

  if (!token->class_id || strncmp (token_class_id, token->class_id, strlen (token_class_id)) != 0)
    return set_exception (ex, "process_handshake: HandshakeMessageToken incorrect class_id: %s (expected %s)", token->class_id ? token->class_id : "NULL", token_class_id);

  if (token_type == HS_TOKEN_REQ || token_type == HS_TOKEN_REPLY)
  {
    const DDS_Security_BinaryProperty_t *c_id, *c_perm, *c_dsign_algo, *c_kagree_algo;

    if ((c_id = find_required_nonempty_binprop (token, DDS_AUTHTOKEN_PROP_C_ID, ex)) == NULL)
      return DDS_SECURITY_VALIDATION_FAILED;
    if ((identityCert = load_X509_certificate_from_binprop (c_id, relation->localIdentity->identityCA, relation->localIdentity->crl, trusted_ca_list, ex)) == NULL)
      return DDS_SECURITY_VALIDATION_FAILED;

    /* TODO: check if an identity certificate was already associated with the remote identity and when that is the case both should be the same */
    if (relation->remoteIdentity->identityCert)
      X509_free (relation->remoteIdentity->identityCert);
    relation->remoteIdentity->identityCert = identityCert;

    if ((c_perm = find_required_binprop (token, DDS_AUTHTOKEN_PROP_C_PERM, ex)) == NULL)
      return DDS_SECURITY_VALIDATION_FAILED;
    if (c_perm->value._length > 0)
    {
      ddsrt_free (relation->remoteIdentity->permissionsDocument);
      relation->remoteIdentity->permissionsDocument = string_from_data (c_perm->value._buffer, c_perm->value._length);
    }

    if ((c_pdata = find_required_binprop (token, DDS_AUTHTOKEN_PROP_C_PDATA, ex)) == NULL)
      return DDS_SECURITY_VALIDATION_FAILED;
    if (validate_pdata (&c_pdata->value, identityCert, ex) != DDS_SECURITY_VALIDATION_OK)
      return DDS_SECURITY_VALIDATION_FAILED;

    if ((c_dsign_algo = find_required_nonempty_binprop (token, DDS_AUTHTOKEN_PROP_C_DSIGN_ALGO, ex)) == NULL)
      return DDS_SECURITY_VALIDATION_FAILED;
    if ((dsignAlgoKind = get_dsign_algo_from_octseq (&c_dsign_algo->value)) == AUTH_ALGO_KIND_UNKNOWN)
      return set_exception (ex, "process_handshake: HandshakeMessageToken property "DDS_AUTHTOKEN_PROP_C_DSIGN_ALGO" not supported");

    if ((c_kagree_algo = find_required_nonempty_binprop (token, DDS_AUTHTOKEN_PROP_C_KAGREE_ALGO, ex)) == NULL)
      return DDS_SECURITY_VALIDATION_FAILED;
    if ((kagreeAlgoKind = get_kagree_algo_from_octseq (&c_kagree_algo->value)) == AUTH_ALGO_KIND_UNKNOWN)
      return set_exception (ex, "process_handshake: HandshakeMessageToken property "DDS_AUTHTOKEN_PROP_C_KAGREE_ALGO" not supported");

    /* calculate the hash value and set in handshake hash_c1 (req) or hash_c2 (reply) */
    const DDS_Security_BinaryProperty_t *binary_properties[] = { c_id, c_perm, c_pdata, c_dsign_algo, c_kagree_algo };
    (void) compute_hash_value ((token_type == HS_TOKEN_REQ) ? handshake->hash_c1 : handshake->hash_c2, binary_properties, 5, NULL);
  }

  if (token_type == HS_TOKEN_REQ)
  {
    EVP_PKEY *pdhkey_req = NULL;

    if ((dh1 = find_required_nonempty_binprop (token, DDS_AUTHTOKEN_PROP_DH1, ex)) == NULL)
      return DDS_SECURITY_VALIDATION_FAILED;
    if (dh_oct_to_public_key (&pdhkey_req, kagreeAlgoKind, dh1->value._buffer, dh1->value._length, ex) != DDS_SECURITY_VALIDATION_OK)
      return DDS_SECURITY_VALIDATION_FAILED;
    if (handshake->rdh)
      EVP_PKEY_free (handshake->rdh);
    handshake->rdh = pdhkey_req;
  }
  else
  {
    dh1 = DDS_Security_DataHolder_find_binary_property (token, DDS_AUTHTOKEN_PROP_DH1);
    if (dh1 && !DDS_Security_BinaryProperty_equal(dh1_ref, dh1))
      return set_exception (ex, "process_handshake: %s token property "DDS_AUTHTOKEN_PROP_DH1" not correct", (token_type == HS_TOKEN_REPLY) ? "Reply" : "Final");
    dh1 = dh1_ref;
  }

  if ((challenge1 = find_required_binprop_exactsize (token, DDS_AUTHTOKEN_PROP_CHALLENGE1, sizeof (AuthenticationChallenge), ex)) == NULL)
    return DDS_SECURITY_VALIDATION_FAILED;

  if (token_type == HS_TOKEN_REPLY || token_type == HS_TOKEN_FINAL)
  {
    if ((challenge2 = find_required_binprop_exactsize (token, DDS_AUTHTOKEN_PROP_CHALLENGE2, sizeof (AuthenticationChallenge), ex)) == NULL)
      return DDS_SECURITY_VALIDATION_FAILED;
    if ((signature = find_required_nonempty_binprop (token, DDS_AUTHTOKEN_PROP_SIGNATURE, ex)) == NULL)
      return DDS_SECURITY_VALIDATION_FAILED;

    if (token_type == HS_TOKEN_REPLY)
    {
      EVP_PKEY *pdhkey_reply = NULL;

      if ((dh2 = find_required_nonempty_binprop (token, DDS_AUTHTOKEN_PROP_DH2, ex)) == NULL)
        return DDS_SECURITY_VALIDATION_FAILED;
      if (dh_oct_to_public_key (&pdhkey_reply, kagreeAlgoKind, dh2->value._buffer, dh2->value._length, ex) != DDS_SECURITY_VALIDATION_OK)
        return DDS_SECURITY_VALIDATION_FAILED;
      if (handshake->rdh)
           EVP_PKEY_free (handshake->rdh);
      handshake->rdh = pdhkey_reply;
    }
    else
    {
      dh2 = DDS_Security_DataHolder_find_binary_property (token, DDS_AUTHTOKEN_PROP_DH2);
      if (dh2 && !DDS_Security_BinaryProperty_equal(dh2_ref, dh2))
        return set_exception (ex, "process_handshake: Final token property "DDS_AUTHTOKEN_PROP_DH2" not correct");
      dh2 = dh2_ref;
    }
  }

  /* When validate_remote_identity was provided with a remote_auth_request_token then the future_challenge in
     the remote identity was set and the challenge(1|2) property of the handshake_(request|reply|final)_token
     should be the same as the future_challenge stored in the remote identity. */
  const DDS_Security_BinaryProperty_t *rc = (token_type == HS_TOKEN_REPLY) ? challenge2 : challenge1;
  if (relation->rchallenge)
  {
    if (memcmp (relation->rchallenge->value, rc->value._buffer, sizeof (AuthenticationChallenge)) != 0)
      return set_exception (ex, "process_handshake: HandshakeMessageToken property challenge%d does not match future_challenge", (token_type == HS_TOKEN_REPLY) ? 2 : 1);
  }
  else if (token_type != HS_TOKEN_FINAL)
  {
    relation->rchallenge = ddsrt_memdup (rc->value._buffer, sizeof (AuthenticationChallenge));
  }

  /* From DDS Security spec: inclusion of the hash_c1 property is optional. Its only purpose is to
     facilitate troubleshoot interoperability problems. */
  if ((hash_c1 = DDS_Security_DataHolder_find_binary_property (token, DDS_AUTHTOKEN_PROP_HASH_C1)))
  {
    /* hash_c1 should be set during req or reply token validation */
    assert (handshake->hash_c1 != NULL);
    if (hash_c1->value._length != sizeof (HashValue_t) || memcmp (hash_c1->value._buffer, handshake->hash_c1, sizeof (HashValue_t)) != 0)
      return set_exception (ex, "process_handshake: HandshakeMessageToken property "DDS_AUTHTOKEN_PROP_HASH_C1" invalid");
  }

  if (token_type == HS_TOKEN_REPLY || token_type == HS_TOKEN_FINAL)
  {
    /* hash_c2 should be set during reply token validation */
    assert (handshake->hash_c2 != NULL);

    /* From DDS Security spec: inclusion of the hash_c2 property is optional. Its only purpose is to
       facilitate troubleshoot interoperability problems. */
    if ((hash_c2 = DDS_Security_DataHolder_find_binary_property (token, DDS_AUTHTOKEN_PROP_HASH_C2)))
    {
      if (hash_c2->value._length != sizeof (HashValue_t) || memcmp (hash_c2->value._buffer, handshake->hash_c2, sizeof (HashValue_t)) != 0)
        return set_exception (ex, "process_handshake: HandshakeMessageToken property hash_c2 invalid");
    }
    if (relation->lchallenge == NULL)
      return set_exception (ex, "process_handshake: No future challenge exists for this token");
    const DDS_Security_BinaryProperty_t *lc = (token_type == HS_TOKEN_REPLY) ? challenge1 : challenge2;
    if (memcmp (relation->lchallenge->value, lc->value._buffer, sizeof (AuthenticationChallenge)) != 0)
      return set_exception (ex, "process_handshake: HandshakeMessageToken property challenge1 does not match future_challenge");
  }

  if (token_type == HS_TOKEN_REQ || token_type == HS_TOKEN_REPLY)
  {
    assert (dsignAlgoKind != AUTH_ALGO_KIND_UNKNOWN);
    assert (kagreeAlgoKind != AUTH_ALGO_KIND_UNKNOWN);
    assert (c_pdata != NULL);

    relation->remoteIdentity->dsignAlgoKind = dsignAlgoKind;
    relation->remoteIdentity->kagreeAlgoKind = kagreeAlgoKind;
    DDS_Security_OctetSeq_copy (&relation->remoteIdentity->pdata, &c_pdata->value);
  }

  if (token_type == HS_TOKEN_REPLY || token_type == HS_TOKEN_FINAL)
  {
    EVP_PKEY *public_key;
    if ((public_key = X509_get_pubkey (relation->remoteIdentity->identityCert)) == NULL)
      return set_exception (ex, "X509_get_pubkey failed");

    DDS_Security_BinaryProperty_t hash_c1_val = {
      .name = DDS_AUTHTOKEN_PROP_HASH_C1, .value = { ._length = sizeof (handshake->hash_c1), ._buffer = handshake->hash_c1 }
    };
    DDS_Security_BinaryProperty_t hash_c2_val = {
      .name = DDS_AUTHTOKEN_PROP_HASH_C2, .value = { ._length = sizeof (handshake->hash_c2), ._buffer = handshake->hash_c2 }
    };
    DDS_Security_ValidationResult_t result;
    if (token_type == HS_TOKEN_REPLY)
      result = validate_signature (public_key, (const DDS_Security_BinaryProperty_t *[]) {
        &hash_c2_val, challenge2, dh2, challenge1, dh1, &hash_c1_val }, 6, signature->value._buffer, signature->value._length, ex);
    else
      result = validate_signature (public_key, (const DDS_Security_BinaryProperty_t *[]) {
        &hash_c1_val, challenge1, dh1, challenge2, dh2, &hash_c2_val }, 6, signature->value._buffer, signature->value._length, ex);
    EVP_PKEY_free (public_key);

    if (result != DDS_SECURITY_VALIDATION_OK)
      return result;
  }

  return DDS_SECURITY_VALIDATION_OK;
}

static DDS_Security_ValidationResult_t validate_handshake_token(const DDS_Security_HandshakeMessageToken *token, enum handshake_token_type token_type, HandshakeInfo *handshake,
    X509Seq *trusted_ca_list, const DDS_Security_BinaryProperty_t *dh1_ref, const DDS_Security_BinaryProperty_t *dh2_ref, DDS_Security_SecurityException *ex)
{
  const DDS_Security_ValidationResult_t ret = validate_handshake_token_impl (token, token_type, handshake, trusted_ca_list, dh1_ref, dh2_ref, ex);

  if (ret != DDS_SECURITY_VALIDATION_OK)
  {
    if (token_type == HS_TOKEN_REQ || token_type == HS_TOKEN_REPLY)
    {
      IdentityRelation *relation = handshake->relation;

      if (relation->remoteIdentity->identityCert)
      {
        X509_free (relation->remoteIdentity->identityCert);
        relation->remoteIdentity->identityCert = NULL;
      }

      if (handshake->rdh)
      {
        EVP_PKEY_free (handshake->rdh);
        handshake->rdh = NULL;
      }
    }
  }

  return ret;
}

DDS_Security_ValidationResult_t begin_handshake_reply(dds_security_authentication *instance, DDS_Security_HandshakeHandle *handshake_handle,
    DDS_Security_HandshakeMessageToken *handshake_message_out, const DDS_Security_HandshakeMessageToken *handshake_message_in,
    const DDS_Security_IdentityHandle initiator_identity_handle, const DDS_Security_IdentityHandle replier_identity_handle,
    const DDS_Security_OctetSeq *serialized_local_participant_data, DDS_Security_SecurityException *ex)
{
  if (!instance || !handshake_handle || !handshake_message_out || !handshake_message_in || !serialized_local_participant_data)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "begin_handshake_reply: Invalid parameter provided");
    return DDS_SECURITY_VALIDATION_FAILED;
  }
  if (serialized_local_participant_data->_length == 0 || serialized_local_participant_data->_buffer == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "begin_handshake_reply: Invalid parameter provided");
    return DDS_SECURITY_VALIDATION_FAILED;
  }

  dds_security_authentication_impl *impl = (dds_security_authentication_impl *)instance;
  HandshakeInfo *handshake = NULL;
  IdentityRelation *relation = NULL;
  SecurityObject *obj;
  LocalIdentityInfo *localIdent;
  RemoteIdentityInfo *remoteIdent;
  EVP_PKEY *dhkeyLocal = NULL;
  unsigned char *certData, *dhPubKeyData;
  uint32_t certDataSize, dhPubKeyDataSize;
  uint32_t tokcount = impl->include_optional ? 12 : 9;
  int created = 0;

  ddsrt_mutex_lock(&impl->lock);

  obj = security_object_find(impl->objectHash, replier_identity_handle);
  if (!obj || !security_object_valid(obj, SECURITY_OBJECT_KIND_LOCAL_IDENTITY))
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "begin_handshake_reply: Invalid replier_identity_handle provided");
    goto err_inv_handle;
  }
  localIdent = (LocalIdentityInfo *)obj;

  obj = security_object_find(impl->objectHash, initiator_identity_handle);
  if (!obj || !security_object_valid(obj, SECURITY_OBJECT_KIND_REMOTE_IDENTITY))
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "begin_handshake_reply: Invalid initiator_identity_handle provided");
    goto err_inv_handle;
  }
  remoteIdent = (RemoteIdentityInfo *)obj;
  if (!(handshake = find_handshake(impl, SECURITY_OBJECT_HANDLE(localIdent), SECURITY_OBJECT_HANDLE(remoteIdent))))
  {
    relation = find_identity_relation(remoteIdent, SECURITY_OBJECT_HANDLE(localIdent));
    assert(relation);
    handshake = handshake_info_new(localIdent, remoteIdent, relation);
    handshake->created_in = CREATEDREPLY;
    (void)ddsrt_hh_add(impl->objectHash, handshake);
    created = 1;
  }
  else
  {
    relation = handshake->relation;
    assert(relation);
  }

  if (validate_handshake_token(handshake_message_in, HS_TOKEN_REQ, handshake, &(impl->trustedCAList), NULL, NULL, ex) != DDS_SECURITY_VALIDATION_OK)
    goto err_inv_token;
  if (get_certificate_contents(localIdent->identityCert, &certData, &certDataSize, ex) != DDS_SECURITY_VALIDATION_OK)
    goto err_alloc_cid;

  if (!handshake->ldh)
  {
    if (generate_dh_keys(&dhkeyLocal, remoteIdent->kagreeAlgoKind, ex) != DDS_SECURITY_VALIDATION_OK)
      goto err_gen_dh_keys;

    handshake->ldh = dhkeyLocal;
    EVP_PKEY_copy_parameters(handshake->rdh, handshake->ldh);
  }

  if (dh_public_key_to_oct(handshake->ldh, remoteIdent->kagreeAlgoKind, &dhPubKeyData, &dhPubKeyDataSize, ex) != DDS_SECURITY_VALIDATION_OK)
    goto err_get_public_key;

  if (localIdent->pdata._length == 0)
    DDS_Security_OctetSeq_copy(&localIdent->pdata, serialized_local_participant_data);

  DDS_Security_BinaryProperty_t *tokens = DDS_Security_BinaryPropertySeq_allocbuf(tokcount);
  uint32_t tokidx = 0;

  /* Store the Identity Certificate associated with the local identify in c.id property */
  DDS_Security_BinaryProperty_set_by_ref(&tokens[tokidx++], DDS_AUTHTOKEN_PROP_C_ID, certData, certDataSize);
  certData = NULL;
  DDS_Security_BinaryProperty_set_by_string(&tokens[tokidx++], DDS_AUTHTOKEN_PROP_C_PERM, localIdent->permissionsDocument ? localIdent->permissionsDocument : "");
  DDS_Security_BinaryProperty_set_by_value(&tokens[tokidx++], DDS_AUTHTOKEN_PROP_C_PDATA, serialized_local_participant_data->_buffer, serialized_local_participant_data->_length);
  DDS_Security_BinaryProperty_set_by_string(&tokens[tokidx++], DDS_AUTHTOKEN_PROP_C_DSIGN_ALGO, get_dsign_algo(localIdent->dsignAlgoKind));
  DDS_Security_BinaryProperty_set_by_string(&tokens[tokidx++], DDS_AUTHTOKEN_PROP_C_KAGREE_ALGO, get_kagree_algo(remoteIdent->kagreeAlgoKind));

  /* Calculate the hash_c2 */
  DDS_Security_BinaryPropertySeq bseq = { ._length = 5, ._buffer = tokens };
  get_hash_binary_property_seq(&bseq, handshake->hash_c2);

  /* Set the DH public key associated with the local participant in dh2 property */
  DDS_Security_BinaryProperty_t *dh2 = &tokens[tokidx++];
  DDS_Security_BinaryProperty_set_by_ref(dh2, DDS_AUTHTOKEN_PROP_DH2, dhPubKeyData, dhPubKeyDataSize);

  /* Find the dh1 property from the received request token */
  const DDS_Security_BinaryProperty_t *dh1 = DDS_Security_DataHolder_find_binary_property(handshake_message_in, DDS_AUTHTOKEN_PROP_DH1);
  assert(dh1);

  assert(relation->rchallenge);
  DDS_Security_BinaryProperty_t *challenge1 = &tokens[tokidx++];
  DDS_Security_BinaryProperty_set_by_value(challenge1, DDS_AUTHTOKEN_PROP_CHALLENGE1, relation->rchallenge->value, sizeof(AuthenticationChallenge));
  assert(relation->lchallenge);
  DDS_Security_BinaryProperty_t *challenge2 = &tokens[tokidx++];
  DDS_Security_BinaryProperty_set_by_value(challenge2, DDS_AUTHTOKEN_PROP_CHALLENGE2, relation->lchallenge->value, sizeof(AuthenticationChallenge));

  /* THe dh1 and hash_c1 and hash_c2 are optional */
  if (impl->include_optional)
  {
    DDS_Security_BinaryProperty_set_by_value(&tokens[tokidx++], DDS_AUTHTOKEN_PROP_DH1, dh1->value._buffer, dh1->value._length);
    DDS_Security_BinaryProperty_set_by_value(&tokens[tokidx++], DDS_AUTHTOKEN_PROP_HASH_C2, handshake->hash_c2, sizeof(HashValue_t));
    DDS_Security_BinaryProperty_set_by_value(&tokens[tokidx++], DDS_AUTHTOKEN_PROP_HASH_C1, handshake->hash_c1, sizeof(HashValue_t));
  }

  /* Calculate the signature */
  {
    unsigned char *sign;
    size_t signlen;
    DDS_Security_BinaryProperty_t *hash_c1_val = hash_value_to_binary_property(DDS_AUTHTOKEN_PROP_HASH_C1, handshake->hash_c1);
    DDS_Security_BinaryProperty_t *hash_c2_val = hash_value_to_binary_property(DDS_AUTHTOKEN_PROP_HASH_C2, handshake->hash_c2);
    const DDS_Security_BinaryProperty_t *binary_properties[HANDSHAKE_SIGNATURE_CONTENT_SIZE] = { hash_c2_val, challenge2, dh2, challenge1, dh1, hash_c1_val };
    DDS_Security_ValidationResult_t result = create_signature(localIdent->privateKey, binary_properties, HANDSHAKE_SIGNATURE_CONTENT_SIZE, &sign, &signlen, ex);
    DDS_Security_BinaryProperty_free(hash_c1_val);
    DDS_Security_BinaryProperty_free(hash_c2_val);
    if (result != DDS_SECURITY_VALIDATION_OK)
      goto err_signature;
    DDS_Security_BinaryProperty_set_by_ref(&tokens[tokidx++], DDS_AUTHTOKEN_PROP_SIGNATURE, sign, (uint32_t)signlen);
  }

  assert(tokidx == tokcount);

  (void)ddsrt_hh_add(impl->objectHash, handshake);
  handshake_message_out->class_id = ddsrt_strdup(DDS_SECURITY_AUTH_HANDSHAKE_REPLY_TOKEN_ID);
  handshake_message_out->binary_properties._length = tokidx;
  handshake_message_out->binary_properties._buffer = tokens;

  ddsrt_mutex_unlock(&impl->lock);

  *handshake_handle = HANDSHAKE_HANDLE(handshake);
  return DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE;

err_signature:
  free_binary_properties(tokens, tokcount);
err_get_public_key:
err_gen_dh_keys:
  ddsrt_free(certData);
err_alloc_cid:
err_inv_token:
  if (created)
  {
    (void)ddsrt_hh_remove(impl->objectHash, handshake);
    security_object_free((SecurityObject *)handshake);
  }
err_inv_handle:
  ddsrt_mutex_unlock(&impl->lock);
  return DDS_SECURITY_VALIDATION_FAILED;
}

static bool generate_shared_secret(const HandshakeInfo *handshake, unsigned char **shared_secret, DDS_Security_long *length, DDS_Security_SecurityException *ex)
{
  EVP_PKEY_CTX *ctx;
  size_t skeylen;
  unsigned char *secret = NULL;
  *shared_secret = NULL;

  if (!(ctx = EVP_PKEY_CTX_new(handshake->ldh, NULL /* no engine */)))
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "process_handshake: Shared secret failed to create context: ");
    goto fail_ctx_new;
  }

  if (EVP_PKEY_derive_init(ctx) <= 0)
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "process_handshake: Shared secret failed to initialize context: ");
    goto fail_derive;
  }
  if (EVP_PKEY_derive_set_peer(ctx, handshake->rdh) <= 0)
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "process_handshake: Shared secret failed to set peer key: ");
    goto fail_derive;
  }

  /* Determine buffer length */
  if (EVP_PKEY_derive(ctx, NULL, &skeylen) <= 0)
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "process_handshake:  Shared secret failed to determine key length: ");
    goto fail_derive;
  }

  secret = ddsrt_malloc(skeylen);
  if (EVP_PKEY_derive(ctx, secret, &skeylen) <= 0)
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "process_handshake: Could not compute the shared secret: ");
    goto fail_derive;
  }

  *shared_secret = ddsrt_malloc(SHA256_DIGEST_LENGTH);
  *length = SHA256_DIGEST_LENGTH;
  SHA256(secret, skeylen, *shared_secret);
  ddsrt_free(secret);
  EVP_PKEY_CTX_free(ctx);
  return true;

fail_derive:
  ddsrt_free(secret);
  EVP_PKEY_CTX_free(ctx);
fail_ctx_new:
  return false;
}

DDS_Security_ValidationResult_t process_handshake(dds_security_authentication *instance, DDS_Security_HandshakeMessageToken *handshake_message_out,
    const DDS_Security_HandshakeMessageToken *handshake_message_in, const DDS_Security_HandshakeHandle handshake_handle, DDS_Security_SecurityException *ex)
{
  if (!instance || !handshake_handle || !handshake_message_out || !handshake_message_in)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "process_handshake: Invalid parameter provided");
    return DDS_SECURITY_VALIDATION_FAILED;
  }

  DDS_Security_ValidationResult_t hs_result = DDS_SECURITY_VALIDATION_OK;
  dds_security_authentication_impl *impl = (dds_security_authentication_impl *)instance;
  HandshakeInfo *handshake = NULL;
  IdentityRelation *relation = NULL;
  SecurityObject *obj;
  DDS_Security_BinaryProperty_t *dh1_gen = NULL, *dh2_gen = NULL;
  const uint32_t tsz = impl->include_optional ? 7 : 3;
  DDS_Security_octet *challenge1_ref_for_shared_secret, *challenge2_ref_for_shared_secret;

  memset(handshake_message_out, 0, sizeof(DDS_Security_HandshakeMessageToken));

  ddsrt_mutex_lock(&impl->lock);
  obj = security_object_find(impl->objectHash, handshake_handle);
  if (!obj || !security_object_valid(obj, SECURITY_OBJECT_KIND_HANDSHAKE))
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "process_handshake: Invalid replier_identity_handle provided");
    goto err_inv_handle;
  }
  handshake = (HandshakeInfo *)obj;
  relation = handshake->relation;
  assert(relation);

  /* check if the handle created by a handshake_request or handshake_reply */
  switch (handshake->created_in)
  {
  case CREATEDREQUEST:
    if ((dh1_gen = create_dhkey_property(DDS_AUTHTOKEN_PROP_DH1, handshake->ldh, relation->localIdentity->kagreeAlgoKind, ex)) == NULL)
      goto err_inv_token;

    /* The source of the handshake_handle is a begin_handshake_request function. So, handshake_message_in is from a remote begin_handshake_reply function */
    /* Verify Message Token contents according to Spec 9.3.2.5.2 (Reply Message)  */
    if (validate_handshake_token(handshake_message_in, HS_TOKEN_REPLY, handshake, &(impl->trustedCAList), dh1_gen, NULL, ex) != DDS_SECURITY_VALIDATION_OK)
      goto err_inv_token;

    EVP_PKEY_copy_parameters(handshake->rdh, handshake->ldh);

    /* Find the dh1 property from the received request token */
    const DDS_Security_BinaryProperty_t *dh2 = DDS_Security_DataHolder_find_binary_property(handshake_message_in, DDS_AUTHTOKEN_PROP_DH2);
    assert(dh2);

    DDS_Security_BinaryProperty_t *tokens = DDS_Security_BinaryPropertySeq_allocbuf(tsz);
    uint32_t idx = 0;

    assert(relation->lchallenge);
    DDS_Security_BinaryProperty_t *challenge1 = &tokens[idx++];
    DDS_Security_BinaryProperty_set_by_value(challenge1, DDS_AUTHTOKEN_PROP_CHALLENGE1, relation->lchallenge->value, sizeof(AuthenticationChallenge));
    assert(relation->rchallenge);
    DDS_Security_BinaryProperty_t *challenge2 = &tokens[idx++];
    DDS_Security_BinaryProperty_set_by_value(challenge2, DDS_AUTHTOKEN_PROP_CHALLENGE2, relation->rchallenge->value, sizeof(AuthenticationChallenge));

    if (impl->include_optional)
    {
      DDS_Security_BinaryProperty_set_by_value(&tokens[idx++], DDS_AUTHTOKEN_PROP_DH1, dh1_gen->value._buffer, dh1_gen->value._length);
      DDS_Security_BinaryProperty_set_by_value(&tokens[idx++], DDS_AUTHTOKEN_PROP_DH2, dh2->value._buffer, dh2->value._length);
      DDS_Security_BinaryProperty_set_by_value(&tokens[idx++], DDS_AUTHTOKEN_PROP_HASH_C2, handshake->hash_c2, sizeof(HashValue_t));
      DDS_Security_BinaryProperty_set_by_value(&tokens[idx++], DDS_AUTHTOKEN_PROP_HASH_C1, handshake->hash_c1, sizeof(HashValue_t));
    }

    /* Calculate the signature */
    {
      unsigned char *sign;
      size_t signlen;
      DDS_Security_BinaryProperty_t *hash_c1_val = hash_value_to_binary_property(DDS_AUTHTOKEN_PROP_HASH_C1, handshake->hash_c1);
      DDS_Security_BinaryProperty_t *hash_c2_val = hash_value_to_binary_property(DDS_AUTHTOKEN_PROP_HASH_C2, handshake->hash_c2);
      const DDS_Security_BinaryProperty_t *binary_properties[HANDSHAKE_SIGNATURE_CONTENT_SIZE] = { hash_c1_val, challenge1, dh1_gen, challenge2, dh2, hash_c2_val };
      DDS_Security_ValidationResult_t result = create_signature(relation->localIdentity->privateKey, binary_properties, HANDSHAKE_SIGNATURE_CONTENT_SIZE, &sign, &signlen, ex);
      DDS_Security_BinaryProperty_free(hash_c1_val);
      DDS_Security_BinaryProperty_free(hash_c2_val);
      if (result != DDS_SECURITY_VALIDATION_OK)
        goto err_signature;
      DDS_Security_BinaryProperty_set_by_ref(&tokens[idx++], DDS_AUTHTOKEN_PROP_SIGNATURE, sign, (uint32_t)signlen);
    }

    handshake_message_out->class_id = ddsrt_strdup(DDS_SECURITY_AUTH_HANDSHAKE_FINAL_TOKEN_ID);
    handshake_message_out->binary_properties._length = tsz;
    handshake_message_out->binary_properties._buffer = tokens;
    challenge1_ref_for_shared_secret = (DDS_Security_octet *)(handshake->relation->lchallenge);
    challenge2_ref_for_shared_secret = (DDS_Security_octet *)(handshake->relation->rchallenge);
    hs_result = DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE;
    break;

  case CREATEDREPLY:
    if ((dh1_gen = create_dhkey_property(DDS_AUTHTOKEN_PROP_DH1, handshake->rdh, relation->remoteIdentity->kagreeAlgoKind, ex)) == NULL)
      goto err_inv_token;
    if ((dh2_gen = create_dhkey_property(DDS_AUTHTOKEN_PROP_DH2, handshake->ldh, relation->remoteIdentity->kagreeAlgoKind, ex)) == NULL)
      goto err_inv_token;

    /* The source of the handshake_handle is a begin_handshake_reply function So, handshake_message_in is from a remote process_handshake function */
    /* Verify Message Token contents according to Spec 9.3.2.5.3 (Final Message)   */
    if (validate_handshake_token(handshake_message_in, HS_TOKEN_FINAL, handshake, NULL, dh1_gen, dh2_gen, ex) != DDS_SECURITY_VALIDATION_OK)
      goto err_inv_token;
    challenge2_ref_for_shared_secret = (DDS_Security_octet *)(handshake->relation->lchallenge);
    challenge1_ref_for_shared_secret = (DDS_Security_octet *)(handshake->relation->rchallenge);
    hs_result = DDS_SECURITY_VALIDATION_OK;
    break;

  default:
    ddsrt_mutex_unlock(&impl->lock);
    goto err_bad_param;
  }

  {
    DDS_Security_long shared_secret_length;
    unsigned char *shared_secret;
    if (!generate_shared_secret(handshake, &shared_secret, &shared_secret_length, ex))
      goto err_openssl;
    handshake->shared_secret_handle_impl = ddsrt_malloc(sizeof(DDS_Security_SharedSecretHandleImpl));
    handshake->shared_secret_handle_impl->shared_secret = shared_secret;
    handshake->shared_secret_handle_impl->shared_secret_size = shared_secret_length;
    memcpy(handshake->shared_secret_handle_impl->challenge1, challenge1_ref_for_shared_secret, DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE);
    memcpy(handshake->shared_secret_handle_impl->challenge2, challenge2_ref_for_shared_secret, DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE);
  }

  {
    /* setup expiry listener */
    RemoteIdentityInfo *remoteIdentity = handshake->relation->remoteIdentity;
    dds_time_t cert_exp = get_certificate_expiry(remoteIdentity->identityCert);
    if (cert_exp == DDS_TIME_INVALID)
    {
      DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "Expiry date of the certificate is invalid");
      goto err_invalid_expiry;
    }
    else if (cert_exp != DDS_NEVER && remoteIdentity->timer == 0)
      remoteIdentity->timer = add_validity_end_trigger(impl, IDENTITY_HANDLE(remoteIdentity), cert_exp);
  }
  ddsrt_mutex_unlock(&impl->lock);

  DDS_Security_BinaryProperty_free(dh1_gen);
  DDS_Security_BinaryProperty_free(dh2_gen);

  return hs_result;

err_invalid_expiry:
  ddsrt_free(handshake->shared_secret_handle_impl->shared_secret);
  ddsrt_free(handshake->shared_secret_handle_impl);
  handshake->shared_secret_handle_impl = NULL;
err_openssl:
err_signature:
  if (handshake_message_out->class_id)
    DDS_Security_DataHolder_deinit(handshake_message_out);
err_inv_token:
  DDS_Security_BinaryProperty_free(dh1_gen);
  DDS_Security_BinaryProperty_free(dh2_gen);
err_inv_handle:
  ddsrt_mutex_unlock(&impl->lock);
err_bad_param:
  return DDS_SECURITY_VALIDATION_FAILED;
}

DDS_Security_SharedSecretHandle get_shared_secret(dds_security_authentication *instance, const DDS_Security_HandshakeHandle handshake_handle, DDS_Security_SecurityException *ex)
{
  if (!instance || !handshake_handle)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "return_handshake_handle: Invalid parameter provided");
    return DDS_SECURITY_HANDLE_NIL;
  }

  dds_security_authentication_impl *impl = (dds_security_authentication_impl *)instance;
  SecurityObject *obj;
  ddsrt_mutex_lock(&impl->lock);
  obj = security_object_find(impl->objectHash, handshake_handle);
  if (!obj || !security_object_valid(obj, SECURITY_OBJECT_KIND_HANDSHAKE))
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "return_handshake_handle: Invalid handle provided");
    goto err_invalid_handle;
  }
  ddsrt_mutex_unlock(&impl->lock);
  return (DDS_Security_SharedSecretHandle)(ddsrt_address)((HandshakeInfo *)obj)->shared_secret_handle_impl;

err_invalid_handle:
  ddsrt_mutex_unlock(&impl->lock);
  return DDS_SECURITY_HANDLE_NIL;
}

DDS_Security_boolean get_authenticated_peer_credential_token(dds_security_authentication *instance, DDS_Security_AuthenticatedPeerCredentialToken *peer_credential_token,
    const DDS_Security_HandshakeHandle handshake_handle, DDS_Security_SecurityException *ex)
{
  if (!instance || !handshake_handle || !peer_credential_token)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }

  dds_security_authentication_impl *impl = (dds_security_authentication_impl *)instance;
  HandshakeInfo *handshake = NULL;
  X509 *identity_cert;
  char *permissions_doc;
  unsigned char *cert_data;
  uint32_t cert_data_size;

  ddsrt_mutex_lock(&impl->lock);

  handshake = (HandshakeInfo *)security_object_find(impl->objectHash, handshake_handle);
  if (!handshake || !SECURITY_OBJECT_VALID(handshake, SECURITY_OBJECT_KIND_HANDSHAKE))
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    goto err_inv_handle;
  }

  if (!(identity_cert = handshake->relation->remoteIdentity->identityCert))
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_OPERATION_NOT_PERMITTED_CODE, 0, DDS_SECURITY_ERR_OPERATION_NOT_PERMITTED_MESSAGE);
    goto err_missing_attr;
  }

  if (!(permissions_doc = handshake->relation->remoteIdentity->permissionsDocument))
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_MISSING_REMOTE_PERMISSIONS_DOCUMENT_CODE, 0, DDS_SECURITY_ERR_MISSING_REMOTE_PERMISSIONS_DOCUMENT_MESSAGE);
    goto err_missing_attr;
  }

  if (get_certificate_contents(identity_cert, &cert_data, &cert_data_size, ex) != DDS_SECURITY_VALIDATION_OK)
    goto err_alloc_cid;

  memset(peer_credential_token, 0, sizeof(*peer_credential_token));
  peer_credential_token->class_id = ddsrt_strdup(DDS_SECURITY_AUTH_TOKEN_CLASS_ID);
  peer_credential_token->properties._length = 2;
  peer_credential_token->properties._buffer = DDS_Security_PropertySeq_allocbuf(peer_credential_token->properties._length);
  peer_credential_token->properties._buffer[0].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_C_ID);
  peer_credential_token->properties._buffer[0].value = (char *)cert_data;
  peer_credential_token->properties._buffer[0].propagate = false;
  peer_credential_token->properties._buffer[1].name = ddsrt_strdup(DDS_AUTHTOKEN_PROP_C_PERM);
  peer_credential_token->properties._buffer[1].value = ddsrt_strdup(permissions_doc);
  peer_credential_token->properties._buffer[1].propagate = false;
  ddsrt_mutex_unlock(&impl->lock);
  return true;

err_alloc_cid:
err_missing_attr:
err_inv_handle:
  ddsrt_mutex_unlock(&impl->lock);
  return false;
}

DDS_Security_boolean set_listener(dds_security_authentication *instance, const dds_security_authentication_listener *listener, DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(ex);
  dds_security_authentication_impl *auth = (dds_security_authentication_impl *)instance;
  auth->listener = listener;
  if (listener)
    dds_security_timed_dispatcher_enable(auth->dispatcher);
  else
    (void) dds_security_timed_dispatcher_disable(auth->dispatcher);
  return true;
}

DDS_Security_boolean return_identity_token(dds_security_authentication *instance, const DDS_Security_IdentityToken *token, DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(token);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

DDS_Security_boolean return_identity_status_token(dds_security_authentication *instance, const DDS_Security_IdentityStatusToken *token, DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(token);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

DDS_Security_boolean return_authenticated_peer_credential_token(dds_security_authentication *instance, const DDS_Security_AuthenticatedPeerCredentialToken *peer_credential_token, DDS_Security_SecurityException *ex)
{
  if (!instance || !peer_credential_token)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }
  DDS_Security_DataHolder_deinit((DDS_Security_DataHolder *)peer_credential_token);
  return true;
}

DDS_Security_boolean return_handshake_handle(dds_security_authentication *instance, const DDS_Security_HandshakeHandle handshake_handle, DDS_Security_SecurityException *ex)
{
  if (!instance || !handshake_handle)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "return_handshake_handle: Invalid parameter provided");
    return false;
  }

  dds_security_authentication_impl *impl = (dds_security_authentication_impl *)instance;
  ddsrt_mutex_lock(&impl->lock);
  SecurityObject *obj = security_object_find(impl->objectHash, handshake_handle);
  if (!obj || !security_object_valid(obj, SECURITY_OBJECT_KIND_HANDSHAKE))
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "return_handshake_handle: Invalid handle provided");
    goto err_invalid_handle;
  }
  HandshakeInfo *handshake = (HandshakeInfo *)obj;
  assert(handshake->relation);
  (void)ddsrt_hh_remove(impl->objectHash, obj);
  security_object_free((SecurityObject *)handshake);
  ddsrt_mutex_unlock(&impl->lock);
  return true;

err_invalid_handle:
  ddsrt_mutex_unlock(&impl->lock);
  return false;
}

static void invalidate_local_related_objects(dds_security_authentication_impl *impl, LocalIdentityInfo *localIdent)
{
  struct ddsrt_hh_iter it;
  SecurityObject *obj;

  for (obj = ddsrt_hh_iter_first(impl->objectHash, &it); obj != NULL; obj = ddsrt_hh_iter_next(&it))
  {
    if (obj->kind == SECURITY_OBJECT_KIND_REMOTE_IDENTITY)
    {
      RemoteIdentityInfo *remoteIdent = (RemoteIdentityInfo *)obj;
      HandshakeInfo *handshake = find_handshake(impl, SECURITY_OBJECT_HANDLE(localIdent), SECURITY_OBJECT_HANDLE(remoteIdent));
      if (handshake)
      {
        (void)ddsrt_hh_remove(impl->objectHash, handshake);
        security_object_free((SecurityObject *)handshake);
      }
      IdentityRelation *relation = find_identity_relation(remoteIdent, SECURITY_OBJECT_HANDLE(localIdent));
      if (relation)
        remove_identity_relation(remoteIdent, relation);
    }
  }
}

static void invalidate_remote_related_objects(dds_security_authentication_impl *impl, RemoteIdentityInfo *remoteIdentity)
{
  struct ddsrt_hh_iter it;
  for (IdentityRelation *relation = ddsrt_hh_iter_first(remoteIdentity->linkHash, &it); relation != NULL; relation = ddsrt_hh_iter_next(&it))
  {
    HandshakeInfo *handshake = find_handshake(impl, SECURITY_OBJECT_HANDLE(relation->localIdentity), SECURITY_OBJECT_HANDLE(remoteIdentity));
    if (handshake)
    {
      (void)ddsrt_hh_remove(impl->objectHash, handshake);
      security_object_free((SecurityObject *)handshake);
    }
    (void)ddsrt_hh_remove(remoteIdentity->linkHash, relation);
    security_object_free((SecurityObject *)relation);
  }
}

DDS_Security_boolean return_identity_handle(dds_security_authentication *instance, const DDS_Security_IdentityHandle identity_handle, DDS_Security_SecurityException *ex)
{
  if (!instance || !identity_handle)
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "return_identity_handle: Invalid parameter provided");
    return false;
  }

  dds_security_authentication_impl *impl = (dds_security_authentication_impl *)instance;
  SecurityObject *obj;
  LocalIdentityInfo *localIdent;
  RemoteIdentityInfo *remoteIdent;

  /* Currently the implementation of the handle does not provide information about the kind of handle. In this case a valid handle could refer to a LocalIdentityObject or a RemoteIdentityObject */
  ddsrt_mutex_lock(&impl->lock);
  if (!(obj = security_object_find(impl->objectHash, identity_handle)))
  {
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "return_identity_handle: Invalid handle provided");
    goto failed;
  }
  switch (obj->kind)
  {
  case SECURITY_OBJECT_KIND_LOCAL_IDENTITY:
    localIdent = (LocalIdentityInfo *)obj;
    if (localIdent->timer != 0)
      dds_security_timed_dispatcher_remove(impl->dispatcher, localIdent->timer);
    invalidate_local_related_objects(impl, localIdent);
    (void)ddsrt_hh_remove(impl->objectHash, obj);
    security_object_free(obj);
    break;
  case SECURITY_OBJECT_KIND_REMOTE_IDENTITY:
    remoteIdent = (RemoteIdentityInfo *)obj;
    if (remoteIdent->timer != 0)
      dds_security_timed_dispatcher_remove(impl->dispatcher, remoteIdent->timer);
    invalidate_remote_related_objects(impl, remoteIdent);
    (void)ddsrt_hh_remove(impl->remoteGuidHash, remoteIdent);
    (void)ddsrt_hh_remove(impl->objectHash, obj);
    security_object_free(obj);
    break;
  default:
    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "return_identity_handle: Invalid handle provided");
    goto failed;
  }
  ddsrt_mutex_unlock(&impl->lock);
  return true;

failed:
  ddsrt_mutex_unlock(&impl->lock);
  return false;
}

DDS_Security_boolean return_sharedsecret_handle(dds_security_authentication *instance, const DDS_Security_SharedSecretHandle sharedsecret_handle, DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(sharedsecret_handle);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

int32_t init_authentication(const char *argument, void **context, struct ddsi_domaingv *gv)
{
  DDSRT_UNUSED_ARG(argument);
  dds_security_authentication_impl *authentication;

  authentication = (dds_security_authentication_impl *)ddsrt_malloc(sizeof(dds_security_authentication_impl));
  memset(authentication, 0, sizeof(dds_security_authentication_impl));
  authentication->base.gv = gv;
  authentication->listener = NULL;
  authentication->dispatcher = dds_security_timed_dispatcher_new(gv->xevents);
  authentication->base.validate_local_identity = &validate_local_identity;
  authentication->base.get_identity_token = &get_identity_token;
  authentication->base.get_identity_status_token = &get_identity_status_token;
  authentication->base.set_permissions_credential_and_token = &set_permissions_credential_and_token;
  authentication->base.validate_remote_identity = &validate_remote_identity;
  authentication->base.begin_handshake_request = &begin_handshake_request;
  authentication->base.begin_handshake_reply = &begin_handshake_reply;
  authentication->base.process_handshake = &process_handshake;
  authentication->base.get_shared_secret = &get_shared_secret;
  authentication->base.get_authenticated_peer_credential_token = &get_authenticated_peer_credential_token;
  authentication->base.set_listener = &set_listener;
  authentication->base.return_identity_token = &return_identity_token;
  authentication->base.return_identity_status_token = &return_identity_status_token;
  authentication->base.return_authenticated_peer_credential_token = &return_authenticated_peer_credential_token;
  authentication->base.return_handshake_handle = &return_handshake_handle;
  authentication->base.return_identity_handle = &return_identity_handle;
  authentication->base.return_sharedsecret_handle = &return_sharedsecret_handle;
  ddsrt_mutex_init(&authentication->lock);
  authentication->objectHash = ddsrt_hh_new(32, security_object_hash, security_object_equal);
  authentication->remoteGuidHash = ddsrt_hh_new(32, remote_guid_hash, remote_guid_equal);
  memset(&authentication->trustedCAList, 0, sizeof(X509Seq));
  authentication->include_optional = gv->handshake_include_optional;

  dds_openssl_init ();
  *context = authentication;
  return 0;
}

int32_t finalize_authentication(void *instance)
{
  dds_security_authentication_impl *authentication = instance;
  if (authentication)
  {
    ddsrt_mutex_lock(&authentication->lock);
    dds_security_timed_dispatcher_free(authentication->dispatcher);
    if (authentication->remoteGuidHash)
      ddsrt_hh_free(authentication->remoteGuidHash);
    if (authentication->objectHash)
    {
      struct ddsrt_hh_iter it;
      for (SecurityObject *obj = ddsrt_hh_iter_first(authentication->objectHash, &it); obj != NULL; obj = ddsrt_hh_iter_next(&it))
        security_object_free(obj);
      ddsrt_hh_free(authentication->objectHash);
    }
    free_ca_list_contents(&(authentication->trustedCAList));
    ddsrt_mutex_unlock(&authentication->lock);
    ddsrt_mutex_destroy(&authentication->lock);
    ddsrt_free((dds_security_authentication_impl *)instance);
  }
  return 0;
}
