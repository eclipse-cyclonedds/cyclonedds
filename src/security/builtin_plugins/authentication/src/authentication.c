/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */


#include <openssl/bn.h>
#include <openssl/asn1.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include "authentication.h"
#include "dds/ddsrt/heap.h"
#include "dds/security/dds_security_api.h"


#if OPENSLL_VERSION_NUMBER >= 0x10002000L
#define AUTH_INCLUDE_EC
#include <openssl/ec.h>
#endif
#include <openssl/rand.h>

/* There is a problem when compiling on windows w.r.t. X509_NAME.
 * The windows api already defines the type X509_NAME which
 * conficts with some openssl versions. The workaround is to
 * undef the openssl X509_NAME
 */
#ifdef _WIN32
#undef X509_NAME
#endif

#include "dds/security/dds_security_api.h"
#include "dds/security/dds_security_api_types.h"
#include "dds/ddsrt/atomics.h"
#include "stdbool.h"
#include <string.h>
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/hopscotch.h"

#include "dds/security/core/shared_secret.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/core/dds_security_serialize.h"
#include "auth_utils.h"
#include <assert.h>

#ifndef EVP_PKEY_id
#define EVP_PKEY_id(k) ((k)->type)
#endif

#if OPENSSL_VERSION_NUMBER >= 0x10000000L && OPENSSL_VERSION_NUMBER < 0x10100000L
#define REMOVE_THREAD_STATE()     ERR_remove_thread_state(NULL);
#elif OPENSSL_VERSION_NUMBER < 0x10000000L
#define REMOVE_THREAD_STATE()     ERR_remove_state(0);
#else
#define REMOVE_THREAD_STATE()
#endif


#define HANDSHAKE_SIGNATURE_CONTENT_SIZE 6

static const char * AUTH_PROTOCOL_CLASS                 = "DDS:Auth:PKI-DH";
static const unsigned AUTH_PROTOCOL_VERSION_MAJOR       = 1;
static const unsigned AUTH_PROTOCOL_VERSION_MINOR       = 0;

static const char * AUTH_REQUEST_TOKEN_CLASS_ID         = "DDS:Auth:PKI-DH:1.0+AuthReq";
static const char * AUTH_REQUEST_TOKEN_FUTURE_PROP_NAME = "future_challenge";

static const char * PROPERTY_IDENTITY_CA                = "dds.sec.auth.identity_ca";
static const char * PROPERTY_PRIVATE_KEY                = "dds.sec.auth.private_key";
static const char * PROPERTY_PASSWORD                   = "dds.sec.auth.password";
static const char * PROPERTY_IDENTITY_CERT              = "dds.sec.auth.identity_certificate";
static const char * PROPERTY_TRUSTED_CA_DIR             = "dds.sec.auth.trusted_ca_dir";

static const char * PROPERTY_CERT_SUBJECT_NAME          = "dds.cert.sn";
static const char * PROPERTY_CERT_ALGORITHM             = "dds.cert.algo";
static const char * PROPERTY_CA_SUBJECT_NAME            = "dds.ca.sn";
static const char * PROPERTY_CA_ALGORITHM               = "dds.ca.aglo";

static const char * AUTH_HANDSHAKE_REQUEST_TOKEN_ID     = "DDS:Auth:PKI-DH:1.0+Req";
static const char * AUTH_HANDSHAKE_REPLY_TOKEN_ID       = "DDS:Auth:PKI-DH:1.0+Reply";

static const char * AUTH_HANDSHAKE_FINAL_TOKEN_ID       = "DDS:Auth:PKI-DH:1.0+Final";

static const char * AUTH_DSIG_ALGO_RSA_2048_SHA256_IDENT   = "RSASSA-PSS-SHA256";
static const char * AUTH_DSIG_ALGO_ECDSA_SHA256_IDENT      = "ECDSA-SHA256";
static const char * AUTH_KAGREE_ALGO_RSA_2048_SHA256_IDENT = "DH+MODP-2048-256";
static const char * AUTH_KAGREE_ALGO_ECDH_PRIME256V1_IDENT = "ECDH+prime256v1-CEUM";


static const char * ACCESS_PERMISSIONS_CREDENTIAL_TOKEN_ID = "DDS:Access:PermissionsCredential";
static const char * ACCESS_PROPERTY_PERMISSION_DOCUMENT    = "dds.perm.cert";


/**
 * Implementation structure for storing encapsulated members of the instance
 * while giving only the interface definition to user
 */

typedef enum {
    SECURITY_OBJECT_KIND_UNKNOWN,
    SECURITY_OBJECT_KIND_LOCAL_IDENTITY,
    SECURITY_OBJECT_KIND_REMOTE_IDENTITY,
    SECURITY_OBJECT_KIND_IDENTITY_RELATION,
    SECURITY_OBJECT_KIND_HANDSHAKE
} SecurityObjectKind_t;

typedef enum {
  CREATEDREQUEST,
  CREATEDREPLY

} CreatedHandshakeStep_t;

typedef struct SecurityObject SecurityObject;

typedef void (*SecurityObjectDestructor)(SecurityObject *obj);

struct SecurityObject {
    int64_t handle;
    SecurityObjectKind_t kind;
    SecurityObjectDestructor destructor;
};


#ifndef NDEBUG
#define CHECK_OBJECT_KIND(o,k) assert(security_object_valid((SecurityObject *)(o), k))
#else
#define CHECK_OBJECT_KIND(o,k)
#endif

#define SECURITY_OBJECT(o)            ((SecurityObject *)(o))
#define SECURITY_OBJECT_HANDLE(o)     (SECURITY_OBJECT(o)->handle)
#define IDENTITY_HANDLE(o)            ((DDS_Security_IdentityHandle) SECURITY_OBJECT_HANDLE(o))
#define HANDSHAKE_HANDLE(o)           ((DDS_Security_HandshakeHandle) SECURITY_OBJECT_HANDLE(o))

#define SECURITY_OBJECT_VALID(o,k)    security_object_valid((SecurityObject *)(o), k)


typedef struct LocalIdentityInfo {
    SecurityObject _parent;
    DDS_Security_DomainId domainId;
    DDS_Security_GUID_t candidateGUID;
    DDS_Security_GUID_t adjustedGUID;
    X509 *identityCert;
    X509 *identityCA;
    EVP_PKEY *privateKey;
    DDS_Security_OctetSeq pdata;
    AuthenticationAlgoKind_t dsignAlgoKind;
    AuthenticationAlgoKind_t kagreeAlgoKind;
    char *permissionsDocument;
} LocalIdentityInfo;

typedef struct RemoteIdentityInfo {
    SecurityObject _parent;
    DDS_Security_GUID_t guid;
    X509 *identityCert;
    AuthenticationAlgoKind_t dsignAlgoKind;
    AuthenticationAlgoKind_t kagreeAlgoKind;
    DDS_Security_IdentityToken *remoteIdentityToken;
    DDS_Security_OctetSeq pdata;
    char *permissionsDocument;
    struct ddsrt_hh *linkHash; /* contains the IdentityRelation objects */
} RemoteIdentityInfo;


/* This structure contains the relation between a local and a remote identity
 * The handle for this object is the same as the handle of the associated
 * local identity object. The IdentityRelation object will be stored with the
 * remote identity.
 */
typedef struct IdentityRelation {
    SecurityObject _parent;
    LocalIdentityInfo *localIdentity;
    RemoteIdentityInfo *remoteIdentity;
    AuthenticationChallenge *lchallenge;
    AuthenticationChallenge *rchallenge;
} IdentityRelation;

typedef struct HandshakeInfo {
    SecurityObject _parent;
    IdentityRelation *relation;
    HashValue_t hash_c1;
    HashValue_t hash_c2;
    EVP_PKEY *ldh;
    EVP_PKEY *rdh;
    DDS_Security_SharedSecretHandleImpl *shared_secret_handle_impl;
    CreatedHandshakeStep_t created_in;
} HandshakeInfo;

typedef struct dds_security_authentication_impl {
    dds_security_authentication base;
    ddsrt_mutex_t lock;
    struct ddsrt_hh *objectHash;
    struct ddsrt_hh *remoteGuidHash;
    struct ut_timed_dispatcher_t *timed_callbacks;
    X509Seq trustedCAList;



} dds_security_authentication_impl;

/* data type for timer dispatcher */
typedef struct {
    dds_security_authentication_impl *auth;
    DDS_Security_IdentityHandle hdl;
} validity_cb_info;


static bool
security_object_valid(
    SecurityObject *obj,
    SecurityObjectKind_t kind)
{
    if (!obj) return false;
    if (obj->kind != kind) return false;
    if (kind == SECURITY_OBJECT_KIND_IDENTITY_RELATION) {
        IdentityRelation *relation = (IdentityRelation *)obj;
        if (!relation->localIdentity || !relation->remoteIdentity || (ddsrt_address)obj->handle != (ddsrt_address)relation->localIdentity) {
            return false;
        }
    } else if ((ddsrt_address)obj->handle != (ddsrt_address)obj) {
        return false;
    }
    return true;
}

static uint32_t
security_object_hash (
    const void *obj)
{
    const SecurityObject *object = obj;
#define UINT64_CONST(x, y, z) (((uint64_t) (x) * 1000000 + (y)) * 1000000 + (z))
    const uint64_t c = UINT64_CONST (16292676, 669999, 574021);
#undef UINT64_CONST
    const uint32_t x = (uint32_t) object->handle;
    return (unsigned) ((x * c) >> 32);
}

static int
security_object_equal (
    const void *ha,
    const void *hb)
{
    const SecurityObject *la = ha;
    const SecurityObject *lb = hb;

    return la->handle == lb->handle;
}

static SecurityObject *
security_object_find(
    const struct ddsrt_hh *hh,
    int64_t handle)
{
    struct SecurityObject template;

    template.handle = handle;

    return (SecurityObject *) ddsrt_hh_lookup(hh, &template);;
}

static void
security_object_init(
    SecurityObject *obj,
    SecurityObjectKind_t kind,
    SecurityObjectDestructor destructor)
{
    assert(obj);

    obj->kind = kind;
    obj->handle = (int64_t)(ddsrt_address)obj;
    obj->destructor = destructor;
}

static void
security_object_deinit(
    SecurityObject *obj)

{
    assert(obj);
    obj->handle = DDS_SECURITY_HANDLE_NIL;
    obj->kind = SECURITY_OBJECT_KIND_UNKNOWN;
    obj->destructor = NULL;
}

static void
security_object_free(
    SecurityObject *obj)
{
    assert(obj);
    if (obj && obj->destructor) {
        obj->destructor(obj);
    }
}

static void
localIdentityInfoFree(
    SecurityObject *obj);



static LocalIdentityInfo *
localIdentityInfoNew(
        DDS_Security_DomainId domainId,
        X509 *identityCert,
        X509 *identityCa,
        EVP_PKEY *privateKey,
        const DDS_Security_GUID_t *candidate_participant_guid,
        const DDS_Security_GUID_t *adjusted_participant_guid)
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

    security_object_init((SecurityObject *)identity, SECURITY_OBJECT_KIND_LOCAL_IDENTITY, localIdentityInfoFree);




    identity->domainId = domainId;
    identity->identityCert = identityCert;
    identity->identityCA = identityCa;
    identity->privateKey = privateKey;
    identity->permissionsDocument = NULL;
    identity->dsignAlgoKind = get_auhentication_algo_kind(identityCert);
    identity->kagreeAlgoKind = AUTH_ALGO_KIND_EC_PRIME256V1;

    memcpy(&identity->candidateGUID, candidate_participant_guid, sizeof(DDS_Security_GUID_t));
    memcpy(&identity->adjustedGUID, adjusted_participant_guid, sizeof(DDS_Security_GUID_t));

    return identity;
}

static void
localIdentityInfoFree(
    SecurityObject *obj)
{
    LocalIdentityInfo *identity = (LocalIdentityInfo *)obj;

    CHECK_OBJECT_KIND(obj, SECURITY_OBJECT_KIND_LOCAL_IDENTITY);

    if (identity) {
        if (identity->identityCert) {
            X509_free(identity->identityCert);
        }
        if (identity->identityCA) {
            X509_free(identity->identityCA);
        }
        if (identity->privateKey) {
            EVP_PKEY_free(identity->privateKey);
        }
        ddsrt_free(identity->pdata._buffer);
        ddsrt_free(identity->permissionsDocument);
        security_object_deinit((SecurityObject *)identity);
        ddsrt_free(identity);
    }
}

static uint32_t
remote_guid_hash (
        const void *obj)
{
    const RemoteIdentityInfo *identity = obj;
    uint32_t tmp[4];

    memcpy(tmp, &identity->guid, sizeof(tmp));

    return (tmp[0]^tmp[1]^tmp[2]^tmp[3]);
}

static int
remote_guid_equal (
        const void *ha,
        const void *hb)
{
    const RemoteIdentityInfo *la = ha;
    const RemoteIdentityInfo *lb = hb;

    return memcmp(&la->guid, &lb->guid, sizeof(la->guid)) == 0;
}

static RemoteIdentityInfo *
find_remote_identity_by_guid(
        const struct ddsrt_hh *hh,
        const DDS_Security_GUID_t *guid)
{
    struct RemoteIdentityInfo template;

    memcpy(&template.guid, guid, sizeof(*guid));

    return (RemoteIdentityInfo *) ddsrt_hh_lookup(hh, &template);
}

static void
remoteIdentityInfoFree(
    SecurityObject *obj);

static RemoteIdentityInfo *
remoteIdentityInfoNew(
        const DDS_Security_GUID_t *guid,
        const DDS_Security_IdentityToken *remote_identity_token)
{
    RemoteIdentityInfo *identity = NULL;

    assert(guid);
    assert(remote_identity_token);

    identity = ddsrt_malloc(sizeof(*identity));
    memset(identity, 0, sizeof(*identity));

    security_object_init((SecurityObject *)identity, SECURITY_OBJECT_KIND_REMOTE_IDENTITY, remoteIdentityInfoFree);

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

static void
remoteIdentityInfoFree(
    SecurityObject *obj)
{
    RemoteIdentityInfo *identity = (RemoteIdentityInfo *)obj;

    CHECK_OBJECT_KIND(obj, SECURITY_OBJECT_KIND_REMOTE_IDENTITY);

    if (identity) {
        if (identity->identityCert) {
            X509_free(identity->identityCert);
        }
        DDS_Security_DataHolder_free(identity->remoteIdentityToken);

        ddsrt_hh_free(identity->linkHash);

        ddsrt_free(identity->pdata._buffer);
        ddsrt_free(identity->permissionsDocument);
        security_object_deinit((SecurityObject *)identity);
        ddsrt_free(identity);
    }
}

static void
identityRelationFree(
    SecurityObject *obj);

/* The IdentityRelation provides the association between a local and a remote
 * identity. This object manages the challenges which are created for
 * each association between a local and a remote identity.
 * The lchallenge is the challenge associated with the local identity and
 * may be set when a future challenge is communicated with the auth_request_message_token.
 * The rchallenge is the challenge received from the remote identity it may be set when
 * an auth_request_message_token is received from the remote identity,
 */
static IdentityRelation *
identityRelationNew(
    LocalIdentityInfo *localIdentity,
    RemoteIdentityInfo *remoteIdentity,
    AuthenticationChallenge *lchallenge,
    AuthenticationChallenge *rchallenge)
{
    IdentityRelation *relation;

    assert(localIdentity);
    assert(remoteIdentity);

    relation = ddsrt_malloc(sizeof(*relation));
    memset(relation, 0, sizeof(*relation));

    security_object_init((SecurityObject *)relation, SECURITY_OBJECT_KIND_IDENTITY_RELATION, identityRelationFree);
    relation->_parent.handle = SECURITY_OBJECT_HANDLE(localIdentity);

    relation->localIdentity = localIdentity;
    relation->remoteIdentity = remoteIdentity;
    relation->lchallenge = lchallenge;
    relation->rchallenge = rchallenge;

    return relation;
}

static void
identityRelationFree(
    SecurityObject *obj)
{
    IdentityRelation *relation = (IdentityRelation *)obj;

    CHECK_OBJECT_KIND(obj, SECURITY_OBJECT_KIND_IDENTITY_RELATION);

    if (relation) {
        ddsrt_free(relation->lchallenge);
        ddsrt_free(relation->rchallenge);
        security_object_deinit((SecurityObject *)relation);
        ddsrt_free(relation);
    }
}

static void
handshakeInfoFree(
    SecurityObject *obj);

static HandshakeInfo *
handshakeInfoNew(
    LocalIdentityInfo *localIdentity,
    RemoteIdentityInfo *remoteIdentity,
    IdentityRelation *relation)
{
    HandshakeInfo *handshake;

    assert(localIdentity);
    assert(remoteIdentity);

    DDSRT_UNUSED_ARG(localIdentity);
    DDSRT_UNUSED_ARG(remoteIdentity);
    handshake = ddsrt_malloc(sizeof(*handshake));
    memset(handshake, 0, sizeof(*handshake));

    security_object_init((SecurityObject *)handshake, SECURITY_OBJECT_KIND_HANDSHAKE, handshakeInfoFree);

    handshake->relation = relation;
    handshake->shared_secret_handle_impl = NULL;

    return handshake;
}

static void
handshakeInfoFree(
    SecurityObject *obj)
{
    HandshakeInfo *handshake = (HandshakeInfo *)obj;

    CHECK_OBJECT_KIND(obj, SECURITY_OBJECT_KIND_HANDSHAKE);

    if (handshake) {
        if (handshake->ldh) {
            EVP_PKEY_free(handshake->ldh);
        }
        if (handshake->rdh) {
            EVP_PKEY_free(handshake->rdh);
        }
        if(handshake->shared_secret_handle_impl) {
            ddsrt_free( handshake->shared_secret_handle_impl->shared_secret);
            ddsrt_free( handshake->shared_secret_handle_impl );
        }
        security_object_deinit((SecurityObject *)handshake);
        ddsrt_free(handshake);
    }
}

static IdentityRelation *
find_identity_relation(
    const RemoteIdentityInfo *remote,
    int64_t lid)
{
    return (IdentityRelation *)security_object_find(remote->linkHash, lid);
}

static void
remove_identity_relation(
    RemoteIdentityInfo *remote,
    IdentityRelation *relation)
{
    (void)ddsrt_hh_remove(remote->linkHash, relation);
    security_object_free((SecurityObject *) relation);
}

static HandshakeInfo *
find_handshake(
    const dds_security_authentication_impl *auth,
    int64_t localId,
    int64_t remoteId)
{
    struct ddsrt_hh_iter it;
    SecurityObject *obj;
    IdentityRelation *relation;
    HandshakeInfo *found = NULL;

    for (obj = ddsrt_hh_iter_first(auth->objectHash, &it); obj && !found; obj = ddsrt_hh_iter_next(&it)) {
        if (obj->kind == SECURITY_OBJECT_KIND_HANDSHAKE) {
            relation = ((HandshakeInfo *)obj)->relation;
            assert(relation);
            if ((SECURITY_OBJECT_HANDLE(relation->localIdentity) == localId) &&
                (SECURITY_OBJECT_HANDLE(relation->remoteIdentity) == remoteId)) {
                found = (HandshakeInfo *)obj;
            }
        }
    }

    return found;
}

static char *
get_authentication_class_id(
    void)
{
    char *classId;
    size_t sz;

    sz = strlen(AUTH_PROTOCOL_CLASS) + 5;

    classId = ddsrt_malloc(sz);
    snprintf(classId, sz, "%s:%1u.%1u", AUTH_PROTOCOL_CLASS, AUTH_PROTOCOL_VERSION_MAJOR, AUTH_PROTOCOL_VERSION_MINOR);

    return classId;
}

static const char *
get_authentication_algo(
        AuthenticationAlgoKind_t kind)
{
    const char *result;
    switch (kind) {
    case AUTH_ALGO_KIND_RSA_2048:
        result = "RSA-2048";
        break;
    case AUTH_ALGO_KIND_EC_PRIME256V1:
        result = "EC-prime256v1";
        break;
    default:
        assert(0);
        result = "";
        break;
    }

    return result;
}

static const char *
get_dsign_algo(
        AuthenticationAlgoKind_t kind)
{
    const char *result;
    switch (kind) {
    case AUTH_ALGO_KIND_RSA_2048:
        result = AUTH_DSIG_ALGO_RSA_2048_SHA256_IDENT;
        break;
    case AUTH_ALGO_KIND_EC_PRIME256V1:
        result = AUTH_DSIG_ALGO_ECDSA_SHA256_IDENT;
        break;
    default:
        assert(0);
        result = "";
        break;
    }

    return result;
}

static const char *
get_kagree_algo(
        AuthenticationAlgoKind_t kind)
{
    const char *result;
    switch (kind) {
    case AUTH_ALGO_KIND_RSA_2048:
        result = AUTH_KAGREE_ALGO_RSA_2048_SHA256_IDENT;
        break;
    case AUTH_ALGO_KIND_EC_PRIME256V1:
        result = AUTH_KAGREE_ALGO_ECDH_PRIME256V1_IDENT;
        break;
    default:
        assert(0);
        result = "";
        break;
    }

    return result;
}

static AuthenticationAlgoKind_t
get_dsign_algo_from_string(
    const char *name)
{
    AuthenticationAlgoKind_t algoKind = AUTH_ALGO_KIND_UNKNOWN;

    if (name) {
        if (strcmp(AUTH_DSIG_ALGO_RSA_2048_SHA256_IDENT, name) == 0) {
            algoKind = AUTH_ALGO_KIND_RSA_2048;
        } else if (strcmp(AUTH_DSIG_ALGO_ECDSA_SHA256_IDENT, name) == 0) {
            algoKind = AUTH_ALGO_KIND_EC_PRIME256V1;
        }
    }

    return algoKind;
}

static AuthenticationAlgoKind_t
get_kagree_algo_from_string(
    const char *name)
{
    AuthenticationAlgoKind_t algoKind = AUTH_ALGO_KIND_UNKNOWN;

    if (name) {
        if (strcmp(AUTH_KAGREE_ALGO_RSA_2048_SHA256_IDENT, name) == 0) {
            algoKind = AUTH_ALGO_KIND_RSA_2048;
        } else if (strcmp(AUTH_KAGREE_ALGO_ECDH_PRIME256V1_IDENT, name) == 0) {
            algoKind = AUTH_ALGO_KIND_EC_PRIME256V1;
        }
    }

    return algoKind;
}

static void
free_binary_properties(
    DDS_Security_BinaryProperty_t *seq,
    uint32_t length)
{
    uint32_t i;

    for (i = 0; i < length; i++) {
        ddsrt_free(seq[i].name);
        ddsrt_free(seq[i].value._buffer);
    }
    ddsrt_free(seq);
}

static void
get_hash_binary_property_seq(
    const DDS_Security_BinaryPropertySeq *seq,
    unsigned char hash[SHA256_DIGEST_LENGTH])
{
    DDS_Security_Serializer serializer;
    unsigned char *buffer;
    size_t size;

    serializer = DDS_Security_Serializer_new(4096, 4096);

    DDS_Security_Serialize_BinaryPropertySeq(serializer, seq);
    DDS_Security_Serializer_buffer(serializer, &buffer, &size);
    SHA256(buffer, size, hash);
    ddsrt_free(buffer);
    DDS_Security_Serializer_free(serializer);
}

static DDS_Security_ValidationResult_t
create_signature(
    EVP_PKEY *pkey,
    const DDS_Security_BinaryProperty_t **binary_properties,
    const uint32_t binary_properties_length,
    unsigned char **signature,
    size_t *signatureLen,
    DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_Serializer serializer;
    unsigned char *buffer;
    size_t size;

    serializer = DDS_Security_Serializer_new(4096, 4096);

    DDS_Security_Serialize_BinaryPropertyArray(serializer,binary_properties, binary_properties_length);
    DDS_Security_Serializer_buffer(serializer, &buffer, &size);

    result = create_asymmetrical_signature(pkey, buffer, size, signature, signatureLen, ex);
    ddsrt_free(buffer);
    DDS_Security_Serializer_free(serializer);

    return result;
}

static DDS_Security_ValidationResult_t
validate_signature(
    EVP_PKEY *pkey,
    const DDS_Security_BinaryProperty_t **properties,
    const uint32_t properties_length,
    unsigned char *signature,
    size_t signatureLen,
    DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_Serializer serializer;
    unsigned char *buffer;
    size_t size;

    serializer = DDS_Security_Serializer_new(4096, 4096);

    DDS_Security_Serialize_BinaryPropertyArray(serializer, properties, properties_length);
    DDS_Security_Serializer_buffer(serializer, &buffer, &size);

    result = validate_asymmetrical_signature(pkey, buffer, size, signature, signatureLen, ex);
    ddsrt_free(buffer);
    DDS_Security_Serializer_free(serializer);

    return result;
}

static DDS_Security_ValidationResult_t
compute_hash_value(
    HashValue_t value,
    const DDS_Security_BinaryProperty_t **properties,
    const uint32_t properties_length,
    DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result = DDS_SECURITY_VALIDATION_OK;
    DDS_Security_Serializer serializer;
    unsigned char *buffer;
    size_t size;

    DDSRT_UNUSED_ARG(ex);

    serializer = DDS_Security_Serializer_new(4096, 4096);

    DDS_Security_Serialize_BinaryPropertyArray(serializer, properties, properties_length);
    DDS_Security_Serializer_buffer(serializer, &buffer, &size);
    SHA256(buffer, size, value);
    ddsrt_free(buffer);
    DDS_Security_Serializer_free(serializer);

    return result;
}

static DDS_Security_BinaryProperty_t *
hash_value_to_binary_property(
    const char *name,
    HashValue_t hash)
{
    DDS_Security_BinaryProperty_t *bp = DDS_Security_BinaryProperty_alloc();

    DDS_Security_BinaryProperty_set_by_value(bp, name, hash, sizeof(HashValue_t));

    return bp;
}


/* Will be enabled after timed callback feature implementation */
#if TIMED_CALLBACK_IMPLEMENTED

static void
validity_callback(struct ut_timed_dispatcher_t *d,
                  ut_timed_cb_kind kind,
                  void *listener,
                  void *arg)
{
    validity_cb_info *info = arg;

    assert(d);
    assert(arg);

    if (kind == UT_TIMED_CB_KIND_TIMEOUT) {
        assert(listener);
        dds_security_authentication_listener *auth_listener = (dds_security_authentication_listener*)listener;
        if (auth_listener->on_revoke_identity) {
            auth_listener->on_revoke_identity(auth_listener,
                                               (dds_security_authentication*)info->auth,
                                               info->hdl);

        }
    }
    ddsrt_free(arg);
}

#endif

static void
add_validity_end_trigger(dds_security_authentication_impl *auth,
                         const DDS_Security_IdentityHandle identity_handle,
                         dds_time_t end)
{
    DDSRT_UNUSED_ARG( auth );
    DDSRT_UNUSED_ARG( identity_handle );
    DDSRT_UNUSED_ARG( end );
    /* Will be enabled after timed call back feature implementation */
    /*
    validity_cb_info *arg = ddsrt_malloc(sizeof(validity_cb_info));
    arg->auth = auth;
    arg->hdl = identity_handle;
    ut_timed_dispatcher_add(auth->timed_callbacks,
                            validity_callback,
                            end,
                            (void*)arg);
                            */
}



#define ADJUSTED_GUID_PREFIX_FLAG 0x80

static DDS_Security_ValidationResult_t
get_adjusted_participant_guid(
        X509 *cert,
        const DDS_Security_GUID_t *candidate,
        DDS_Security_GUID_t *adjusted,
        DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result = DDS_SECURITY_VALIDATION_FAILED;
    unsigned char high[SHA256_DIGEST_LENGTH], low[SHA256_DIGEST_LENGTH];
    unsigned char *subject = NULL;
    size_t size=0;

    assert(cert);
    assert(candidate);
    assert(adjusted);

    result = get_subject_name_DER_encoded(cert, &subject, &size, ex);
    if ( result == DDS_SECURITY_VALIDATION_OK ) {
      DDS_Security_octet hb = ADJUSTED_GUID_PREFIX_FLAG;
      int i;

      SHA256(subject, size, high);
      SHA256(&candidate->prefix[0], sizeof(DDS_Security_GuidPrefix_t), low);

      adjusted->entityId = candidate->entityId;
      for (i = 0; i < 6; i++) {
        adjusted->prefix[i] = hb | high[i] >> 1;
        hb = (DDS_Security_octet) (high[i] << 7);
      }
      for (i = 0; i < 6; i++) {
        adjusted->prefix[i + 6] = low[i];
      }
      ddsrt_free(subject);
    }

    return result;
}
#undef ADJUSTED_GUID_PREFIX_FLAG

DDS_Security_ValidationResult_t
validate_local_identity(
        dds_security_authentication *instance,
        DDS_Security_IdentityHandle *local_identity_handle,
        DDS_Security_GUID_t *adjusted_participant_guid,
        const DDS_Security_DomainId domain_id,
        const DDS_Security_Qos *participant_qos,
        const DDS_Security_GUID_t *candidate_participant_guid,
        DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result = DDS_SECURITY_VALIDATION_OK;
    dds_security_authentication_impl *implementation = (dds_security_authentication_impl *) instance;
    LocalIdentityInfo *identity;
    char *identityCertPEM;
    char *identityCaPEM;
    char *privateKeyPEM;
    char *password;
    X509 *identityCert;
    X509 *identityCA;
    EVP_PKEY *privateKey;
    char *trusted_ca_dir;
    unsigned i;
    dds_time_t certExpiry = DDS_TIME_INVALID;

    /* validate provided arguments */
    if (!instance || !local_identity_handle || !adjusted_participant_guid || !participant_qos || !candidate_participant_guid) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "validate_local_identity: Invalid parameter provided");
        goto err_bad_param;
    }

    identityCertPEM = DDS_Security_Property_get_value(&participant_qos->property.value, PROPERTY_IDENTITY_CERT);
    if (!identityCertPEM) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "validate_local_identity: missing property '%s'", PROPERTY_IDENTITY_CERT);
        goto err_no_identity_cert;
    }

    identityCaPEM = DDS_Security_Property_get_value(&participant_qos->property.value, PROPERTY_IDENTITY_CA);
    if (!identityCaPEM) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "validate_local_identity: missing property '%s'", PROPERTY_IDENTITY_CA);
        goto err_no_identity_ca;
    }

    privateKeyPEM = DDS_Security_Property_get_value(&participant_qos->property.value, PROPERTY_PRIVATE_KEY);
    if (!privateKeyPEM) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "validate_local_identity: missing property '%s'", PROPERTY_PRIVATE_KEY);
        goto err_no_private_key;
    }

    password = DDS_Security_Property_get_value(&participant_qos->property.value, PROPERTY_PASSWORD);


    trusted_ca_dir = DDS_Security_Property_get_value(&participant_qos->property.value, PROPERTY_TRUSTED_CA_DIR);

    if( trusted_ca_dir ){
        result = get_trusted_ca_list(trusted_ca_dir, &(implementation->trustedCAList), ex );
        if (result != DDS_SECURITY_VALIDATION_OK) {
            goto err_inv_trusted_ca_dir;
        }
    }


    result = load_X509_certificate(identityCaPEM, &identityCA, ex);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        goto err_inv_identity_ca;
    }


    /*check for  CA if listed in trusted CA files*/
    if( implementation->trustedCAList.length != 0 ){
        const EVP_MD *digest = EVP_get_digestbyname("sha1");
        uint32_t size;
        unsigned char hash_buffer[20];
        unsigned char hash_buffer_trusted[20];
        result = DDS_SECURITY_VALIDATION_FAILED;
        X509_digest(identityCA, digest, hash_buffer, &size);
        for (i = 0; i < implementation->trustedCAList.length; ++i) {
            X509_digest(implementation->trustedCAList.buffer[i], digest, hash_buffer_trusted, &size);
            if( memcmp( hash_buffer_trusted, hash_buffer,20 ) == 0){
                result = DDS_SECURITY_VALIDATION_OK;
                break;
            }
        }

        if (result != DDS_SECURITY_VALIDATION_OK) { /*not trusted*/
            DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CA_NOT_TRUSTED_CODE, (int)result, DDS_SECURITY_ERR_CA_NOT_TRUSTED_MESSAGE);
            goto err_identity_ca_not_trusted;
        }
    }
    result = load_X509_certificate(identityCertPEM, &identityCert, ex);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        goto err_inv_identity_cert;
    }

    result = load_X509_private_key(privateKeyPEM, password, &privateKey, ex);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        goto err_inv_private_key;
    }

    result = verify_certificate(identityCert, identityCA, ex);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        goto err_verification_failed;
    }

    result = get_adjusted_participant_guid(identityCert, candidate_participant_guid, adjusted_participant_guid, ex);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        goto err_adj_guid_failed;
    }

    ddsrt_free(password);
    ddsrt_free(privateKeyPEM);
    ddsrt_free(identityCaPEM);
    ddsrt_free(identityCertPEM);
    ddsrt_free(trusted_ca_dir);

    identity = localIdentityInfoNew(domain_id, identityCert, identityCA, privateKey, candidate_participant_guid, adjusted_participant_guid);

    *local_identity_handle = IDENTITY_HANDLE(identity);

    /* setup expiry listener */
    certExpiry = get_certificate_expiry( identityCert );

    if( certExpiry == DDS_TIME_INVALID ){
      DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "Expiry date of the certificate is invalid");
      goto err_verification_failed;
    } else if ( certExpiry != DDS_NEVER ){
      add_validity_end_trigger( implementation,
                                *local_identity_handle,
                                certExpiry);
    }


    ddsrt_mutex_lock(&implementation->lock);
    (void)ddsrt_hh_add(implementation->objectHash, identity);

    ddsrt_mutex_unlock(&implementation->lock);

    return result;

err_adj_guid_failed:
err_verification_failed:
    EVP_PKEY_free(privateKey);
err_inv_private_key:
    X509_free(identityCert);
err_inv_identity_cert:
err_identity_ca_not_trusted:
    X509_free(identityCA);
err_inv_identity_ca:
err_inv_trusted_ca_dir:
    ddsrt_free(password);
    ddsrt_free(privateKeyPEM);
    ddsrt_free(trusted_ca_dir);
err_no_private_key:
    ddsrt_free(identityCaPEM);
err_no_identity_ca:
    ddsrt_free(identityCertPEM);
err_no_identity_cert:
err_bad_param:
    return DDS_SECURITY_VALIDATION_FAILED;
}

DDS_Security_boolean
get_identity_token(dds_security_authentication *instance,
        DDS_Security_IdentityToken *identity_token,
        const DDS_Security_IdentityHandle handle,
        DDS_Security_SecurityException *ex)
{
    dds_security_authentication_impl *impl = (dds_security_authentication_impl *) instance;
    SecurityObject *obj;
    LocalIdentityInfo *identity;
    char *snCert, *snCA;

    memset(identity_token, 0, sizeof(*identity_token));

    /* validate provided arguments */
    if (!instance || !identity_token) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "get_identity_token: Invalid parameter provided");
        goto err_bad_param;
    }

    ddsrt_mutex_lock(&impl->lock);

    obj = security_object_find(impl->objectHash, handle);
    if (!obj || !security_object_valid(obj, SECURITY_OBJECT_KIND_LOCAL_IDENTITY)) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "get_identity_token: Invalid handle provided");
        goto err_inv_handle;
    }
    identity = (LocalIdentityInfo *)obj;

    snCert = get_certificate_subject_name(identity->identityCert, ex);

    if (!snCert) {
        goto err_sn_cert;
    }

    snCA = get_certificate_subject_name(identity->identityCA, ex);
    if (!snCA) {
        goto err_sn_ca;
    }

    identity_token->class_id = get_authentication_class_id();
    identity_token->properties._length = 4;
    identity_token->properties._buffer = DDS_Security_PropertySeq_allocbuf(4);

    identity_token->properties._buffer[0].name = ddsrt_strdup(PROPERTY_CERT_SUBJECT_NAME);
    identity_token->properties._buffer[0].value = snCert;

    identity_token->properties._buffer[1].name = ddsrt_strdup(PROPERTY_CERT_ALGORITHM);
    identity_token->properties._buffer[1].value = ddsrt_strdup(get_authentication_algo(get_auhentication_algo_kind(identity->identityCert)));

    identity_token->properties._buffer[2].name = ddsrt_strdup(PROPERTY_CA_SUBJECT_NAME);
    identity_token->properties._buffer[2].value = snCA;

    identity_token->properties._buffer[3].name = ddsrt_strdup(PROPERTY_CA_ALGORITHM);
    identity_token->properties._buffer[3].value = ddsrt_strdup(get_authentication_algo(get_auhentication_algo_kind(identity->identityCA)));

    ddsrt_mutex_unlock(&impl->lock);

    return true;

err_sn_ca:
    ddsrt_free(snCert);
err_sn_cert:
err_inv_handle:
    ddsrt_mutex_unlock(&impl->lock);
err_bad_param:
    return false;
}

DDS_Security_boolean get_identity_status_token(
        dds_security_authentication *instance,
        DDS_Security_IdentityStatusToken *identity_status_token,
        const DDS_Security_IdentityHandle handle,
        DDS_Security_SecurityException *ex)
{
    DDSRT_UNUSED_ARG(identity_status_token);
    DDSRT_UNUSED_ARG(handle);
    DDSRT_UNUSED_ARG(ex);
    DDSRT_UNUSED_ARG(instance);

    return true;
}

DDS_Security_boolean
set_permissions_credential_and_token(
        dds_security_authentication *instance,
        const DDS_Security_IdentityHandle handle,
        const DDS_Security_PermissionsCredentialToken *permissions_credential,
        const DDS_Security_PermissionsToken *permissions_token,
        DDS_Security_SecurityException *ex)
{
    dds_security_authentication_impl *impl = (dds_security_authentication_impl *) instance;
    LocalIdentityInfo *identity;

    /* validate provided arguments */
    if ((!instance)                         ||
        (handle == DDS_SECURITY_HANDLE_NIL) ||
        (!permissions_credential)           ||
        (!permissions_token)                ){
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT,
                DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "get_identity_token: Invalid parameter provided");
        return false;
    }

    if (!permissions_credential->class_id ||
        (strcmp(permissions_credential->class_id, ACCESS_PERMISSIONS_CREDENTIAL_TOKEN_ID) != 0)) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT,
                DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "get_identity_token: Invalid parameter provided");
        return false;
    }

    if ((permissions_credential->properties._length == 0) ||
        (permissions_credential->properties._buffer[0].name == NULL) ||
        (strcmp(permissions_credential->properties._buffer[0].name, ACCESS_PROPERTY_PERMISSION_DOCUMENT) != 0)) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT,
                DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "get_identity_token: Invalid parameter provided");
        return false;
    }

    ddsrt_mutex_lock(&impl->lock);

    identity = (LocalIdentityInfo *)security_object_find(impl->objectHash, handle);
    if (!identity || !SECURITY_OBJECT_VALID(identity, SECURITY_OBJECT_KIND_LOCAL_IDENTITY)) {
        ddsrt_mutex_unlock(&impl->lock);
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "get_identity_token: Invalid handle provided");
        return false;
    }

    if (permissions_credential->properties._buffer[0].value) {
        identity->permissionsDocument = ddsrt_strdup(permissions_credential->properties._buffer[0].value);
    } else {
        identity->permissionsDocument = ddsrt_strdup("");
    }

    ddsrt_mutex_unlock(&impl->lock);

    return true;
}


static DDS_Security_ValidationResult_t
validate_remote_identity_token(
    const LocalIdentityInfo *localIdent,
    const DDS_Security_IdentityToken *token,
    DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result = DDS_SECURITY_VALIDATION_OK;

    DDSRT_UNUSED_ARG(localIdent);

    if (token->class_id) {
        size_t sz = strlen(AUTH_PROTOCOL_CLASS);

        if (strncmp(AUTH_PROTOCOL_CLASS, token->class_id, sz) == 0) {
            char *ptr = &token->class_id[sz];
            char postfix[2];
            unsigned major, minor;

      DDSRT_WARNING_MSVC_OFF(4996);
            if (sscanf(ptr, ":%u.%u%1s", &major, &minor, postfix) == 2) {
      DDSRT_WARNING_MSVC_ON(4996);
                if (major == AUTH_PROTOCOL_VERSION_MAJOR) {
                } else {
                    result = DDS_SECURITY_VALIDATION_FAILED;
                    DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "remote identity token: version %u.%u not supported", major, minor);
                }
            } else {
                result = DDS_SECURITY_VALIDATION_FAILED;
                DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE,  (int)result, "remote identity token: class_id has wrong format");
            }
        } else {
            result = DDS_SECURITY_VALIDATION_FAILED;
            DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "remote identity token: class_id='%s' not supported", token->class_id);
        }
    } else {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "remote identity token: class_id is empty");
    }

    return result;
}

static DDS_Security_ValidationResult_t
validate_auth_request_token(
        const DDS_Security_IdentityToken *token,
        AuthenticationChallenge **challenge,
        DDS_Security_SecurityException *ex)
{
    uint32_t index;
    int found = 0;

    assert(token);

    if (!token->class_id) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                "AuthRequestMessageToken invalid: missing class_id");
        goto err_inv_token;
    }

    if (strncmp(token->class_id, AUTH_REQUEST_TOKEN_CLASS_ID, strlen(AUTH_REQUEST_TOKEN_CLASS_ID)) != 0) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                "AuthRequestMessageToken invalid: class_id '%s' is invalid", token->class_id);
        goto err_inv_token;
    }

    if (!token->binary_properties._buffer) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE,  DDS_SECURITY_VALIDATION_FAILED,
                "AuthRequestMessageToken invalid: properties are missing");
        goto err_inv_token;
    }

    for (index = 0; index < token->binary_properties._length; index++) {
        size_t len = strlen(AUTH_REQUEST_TOKEN_FUTURE_PROP_NAME);
        if (token->binary_properties._buffer[index].name &&
            (strncmp(token->binary_properties._buffer[index].name, AUTH_REQUEST_TOKEN_FUTURE_PROP_NAME, len) == 0)) {
            found = 1;
            break;
        }
    }

    if (!found) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                 "AuthRequestMessageToken invalid: future_challenge not found");
        goto err_inv_token;
    }

    if (token->binary_properties._buffer[index].value._length != sizeof(AuthenticationChallenge)) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                       "AuthRequestMessageToken invalid: future_challenge invalid size");
        goto err_inv_token;
    }

    if (!token->binary_properties._buffer[index].value._buffer) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                       "AuthRequestMessageToken invalid: future_challenge invalid size");
        goto err_inv_token;
    }

    if (challenge) {
        *challenge = ddsrt_malloc(sizeof(AuthenticationChallenge));
        memcpy(*challenge, &token->binary_properties._buffer[index].value._buffer[0], sizeof(AuthenticationChallenge));
    }

    return DDS_SECURITY_VALIDATION_OK;

err_inv_token:
    return DDS_SECURITY_VALIDATION_FAILED;
}

static void
fill_auth_request_token(
    DDS_Security_AuthRequestMessageToken *token,
    AuthenticationChallenge *challenge)
{
    uint32_t len = sizeof(challenge->value);

    DDS_Security_DataHolder_deinit(token);

    token->class_id = ddsrt_strdup(AUTH_REQUEST_TOKEN_CLASS_ID);
    token->binary_properties._length = 1;
    token->binary_properties._buffer = DDS_Security_BinaryPropertySeq_allocbuf(1);
    token->binary_properties._buffer->name = ddsrt_strdup(AUTH_REQUEST_TOKEN_FUTURE_PROP_NAME);

    token->binary_properties._buffer->value._length = len;
    token->binary_properties._buffer->value._buffer = ddsrt_malloc(len);
    memcpy(token->binary_properties._buffer->value._buffer, challenge->value, len);
}

DDS_Security_ValidationResult_t
validate_remote_identity(
        dds_security_authentication *instance,
        DDS_Security_IdentityHandle *remote_identity_handle,
        DDS_Security_AuthRequestMessageToken *local_auth_request_token,
        const DDS_Security_AuthRequestMessageToken *remote_auth_request_token,
        const DDS_Security_IdentityHandle local_identity_handle,
        const DDS_Security_IdentityToken *remote_identity_token,
        const DDS_Security_GUID_t *remote_participant_guid,
        DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result = DDS_SECURITY_VALIDATION_OK;
    dds_security_authentication_impl *impl = (dds_security_authentication_impl *) instance;
    SecurityObject *obj;
    LocalIdentityInfo *localIdent;
    RemoteIdentityInfo *remoteIdent;
    IdentityRelation *relation;
    AuthenticationChallenge *lchallenge = NULL, *rchallenge = NULL;
    int r;

    /* validate provided arguments */
    if (!instance || !remote_identity_handle || !local_auth_request_token || !remote_identity_token || !remote_participant_guid) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "validate_remote_identity: Invalid parameter provided");
        goto err_bad_param;
    }

    ddsrt_mutex_lock(&impl->lock);

    obj = security_object_find(impl->objectHash, local_identity_handle);
    if (!obj || !security_object_valid(obj, SECURITY_OBJECT_KIND_LOCAL_IDENTITY)) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "validate_remote_identity: Invalid handle provided");
        goto err_inv_handle;
    }
    localIdent = (LocalIdentityInfo *) obj;

    /* Check if the provided remote_identity_token is compatible */
    result = validate_remote_identity_token(localIdent, remote_identity_token, ex);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        goto err_remote_identity_token;
    }

    /* When the remote_auth_request_token is not null, check if it's contents is valid and
     * set the futureChallenge from the data contained in the remote_auth_request_token.
     */
    if (remote_auth_request_token) {
        result  = validate_auth_request_token(remote_auth_request_token, &rchallenge, ex);
        if (result != DDS_SECURITY_VALIDATION_OK) {
            goto err_inv_auth_req_token;
        }
    }

    if ((lchallenge = generate_challenge(ex)) == NULL) {
        goto err_alloc_challenge;
    }

    /* The validate_remote_identity will also create a handshake structure which contains the
     * relation between an local an remote identity. This handshake structure is inserted in
     * the remote identity structure.
     */

    /* Check if the remote identity has already been validated by a previous validation request. */
    remoteIdent = find_remote_identity_by_guid(impl->remoteGuidHash, remote_participant_guid);
    if (!remoteIdent) {
        remoteIdent = remoteIdentityInfoNew(remote_participant_guid, remote_identity_token);
        (void)ddsrt_hh_add(impl->objectHash, remoteIdent);
        (void)ddsrt_hh_add(impl->remoteGuidHash, remoteIdent);
        relation = identityRelationNew(localIdent, remoteIdent, lchallenge, rchallenge);
        (void)ddsrt_hh_add(remoteIdent->linkHash, relation);
    } else {
        /* When the remote identity has already been validated before,
           check if the remote identity token matches with the existing one
         */
        if (!DDS_Security_DataHolder_equal(remoteIdent->remoteIdentityToken, remote_identity_token)) {
            result = DDS_SECURITY_VALIDATION_FAILED;
            DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                    "validate_remote_identity: remote_identity_token does not match with previously received one");
            goto err_inv_duplicate;
        }

        relation = find_identity_relation(remoteIdent, SECURITY_OBJECT_HANDLE(localIdent));
        if (!relation) {
            relation = identityRelationNew(localIdent, remoteIdent, lchallenge, rchallenge);
            r = ddsrt_hh_add(remoteIdent->linkHash, relation);
            assert(r);
            (void)r;
        } else {
            if (remote_auth_request_token) {
                assert(rchallenge);
                ddsrt_free(relation->rchallenge);
                relation->rchallenge = rchallenge;
            }
            ddsrt_free(lchallenge);
        }
    }

    ddsrt_mutex_unlock(&impl->lock);

    if (!remote_auth_request_token) {
        /* Create local_auth_request_token with contents set to the challenge */
        fill_auth_request_token(local_auth_request_token, relation->lchallenge);
    } else {
        /* Set local_auth_request token to TokenNil */
        DDS_Security_set_token_nil(local_auth_request_token);
    }

    *remote_identity_handle = IDENTITY_HANDLE(remoteIdent);;

    if (memcmp(&localIdent->adjustedGUID, &remoteIdent->guid, sizeof(DDS_Security_GUID_t)) < 0) {
        result = DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_REQUEST;
    } else {
        result = DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE;
    }

    return result;

err_inv_duplicate:
    ddsrt_free(lchallenge);
err_alloc_challenge:
    ddsrt_free(rchallenge);
err_inv_auth_req_token:
err_remote_identity_token:
err_inv_handle:
    ddsrt_mutex_unlock(&impl->lock);
err_bad_param:
    return DDS_SECURITY_VALIDATION_FAILED;
}

DDS_Security_ValidationResult_t
begin_handshake_request(
        dds_security_authentication *instance,
        DDS_Security_HandshakeHandle *handshake_handle,
        DDS_Security_HandshakeMessageToken *handshake_message,
        const DDS_Security_IdentityHandle initiator_identity_handle,
        const DDS_Security_IdentityHandle replier_identity_handle,
        const DDS_Security_OctetSeq *serialized_local_participant_data,
        DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result = DDS_SECURITY_VALIDATION_OK;
    dds_security_authentication_impl *impl = (dds_security_authentication_impl *) instance;
    HandshakeInfo *handshake = NULL;
    IdentityRelation *relation = NULL;
    SecurityObject *obj;
    LocalIdentityInfo *localIdent;
    RemoteIdentityInfo *remoteIdent;
    EVP_PKEY *dhkey;
    DDS_Security_BinaryProperty_t *tokens;
    DDS_Security_BinaryProperty_t *c_id;
    DDS_Security_BinaryProperty_t *c_perm;
    DDS_Security_BinaryProperty_t *c_pdata;
    DDS_Security_BinaryProperty_t *c_dsign_algo;
    DDS_Security_BinaryProperty_t *c_kagree_algo;
    DDS_Security_BinaryProperty_t *hash_c1;
    DDS_Security_BinaryProperty_t *dh1;
    DDS_Security_BinaryProperty_t *challenge;
    unsigned char *certData;
    unsigned char *dhPubKeyData = NULL;
    uint32_t certDataSize, dhPubKeyDataSize;
    int created = 0;

    /* validate provided arguments */
    if (!instance || !handshake_handle || !handshake_message || !serialized_local_participant_data) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                "begin_handshake_request: Invalid parameter provided");
        goto err_bad_param;
    }

    ddsrt_mutex_lock(&impl->lock);

    obj = security_object_find(impl->objectHash, initiator_identity_handle);
    if (!obj || !security_object_valid(obj, SECURITY_OBJECT_KIND_LOCAL_IDENTITY)) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                "begin_handshake_request: Invalid initiator_identity_handle provided");
        goto err_inv_handle;
    }
    localIdent = (LocalIdentityInfo *) obj;

    obj = security_object_find(impl->objectHash, replier_identity_handle);
    if (!obj || !security_object_valid(obj, SECURITY_OBJECT_KIND_REMOTE_IDENTITY)) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                "begin_handshake_request: Invalid replier_identity_handle provided");
        goto err_inv_handle;
    }
    remoteIdent = (RemoteIdentityInfo *)obj;

    result = get_certificate_contents(localIdent->identityCert, &certData, &certDataSize, ex);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        goto err_alloc_cid;
    }

    handshake = find_handshake(impl, SECURITY_OBJECT_HANDLE(localIdent), SECURITY_OBJECT_HANDLE(remoteIdent));
    if (!handshake) {
        relation = find_identity_relation(remoteIdent, SECURITY_OBJECT_HANDLE(localIdent));
        assert(relation);
        handshake = handshakeInfoNew(localIdent, remoteIdent, relation);
        handshake->created_in = CREATEDREQUEST;
        (void)ddsrt_hh_add(impl->objectHash, handshake);
        created = 1;
    } else {
        relation = handshake->relation;
        assert(relation);
    }

    if (!handshake->ldh) {
        result = generate_dh_keys(&dhkey, localIdent->kagreeAlgoKind, ex);
        if (result != DDS_SECURITY_VALIDATION_OK) {
            goto err_gen_dh_keys;
        }

        handshake->ldh = dhkey;
    }

    result = dh_public_key_to_oct(handshake->ldh, localIdent->kagreeAlgoKind, &dhPubKeyData, &dhPubKeyDataSize, ex);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        goto err_get_public_key;
    }

    if (localIdent->pdata._length == 0) {
        DDS_Security_OctetSeq_copy(&localIdent->pdata, serialized_local_participant_data);
    }

    tokens = DDS_Security_BinaryPropertySeq_allocbuf(8);
    c_id = &tokens[0];
    c_perm = &tokens[1];
    c_pdata = &tokens[2];
    c_dsign_algo = &tokens[3];
    c_kagree_algo = &tokens[4];
    hash_c1 = &tokens[5];
    dh1 = &tokens[6];
    challenge = &tokens[7];

    /* Store the Identity Certificate associated with the local identify in c.id property */
    DDS_Security_BinaryProperty_set_by_ref(c_id, "c.id", certData, certDataSize);

    /* Store the permission document in the c.perm property */
    if (localIdent->permissionsDocument) {
        DDS_Security_BinaryProperty_set_by_string(c_perm, "c.perm", localIdent->permissionsDocument);
    } else {
      DDS_Security_BinaryProperty_set_by_string(c_perm, "c.perm", "");
    }

    /* Store the provided local_participant_data in the c.pdata property */
    DDS_Security_BinaryProperty_set_by_value(c_pdata, "c.pdata", serialized_local_participant_data->_buffer, serialized_local_participant_data->_length);

    /* Set the used signing algorithm descriptor in c.dsign_algo */
    DDS_Security_BinaryProperty_set_by_string(c_dsign_algo, "c.dsign_algo", get_dsign_algo(localIdent->dsignAlgoKind));

    /* Set the used key algorithm descriptor in c.kagree_algo */
    DDS_Security_BinaryProperty_set_by_string(c_kagree_algo, "c.kagree_algo", get_kagree_algo(localIdent->kagreeAlgoKind));

    /* Calculate the hash_c1 */
    {
        DDS_Security_BinaryPropertySeq bseq;

        bseq._length = 5;
        bseq._buffer = tokens;

        get_hash_binary_property_seq(&bseq, handshake->hash_c1);
        DDS_Security_BinaryProperty_set_by_value(hash_c1, "hash_c1", handshake->hash_c1, sizeof(HashValue_t));
    }

    /* Set the DH public key associated with the local participant in dh1 property */
    assert(dhPubKeyData);
    assert(dhPubKeyDataSize < 1200);
    DDS_Security_BinaryProperty_set_by_ref(dh1, "dh1", dhPubKeyData, dhPubKeyDataSize);

    /* Set the challenge in challenge1 property */
    DDS_Security_BinaryProperty_set_by_value(challenge, "challenge1", relation->lchallenge->value, sizeof(AuthenticationChallenge));

    (void)ddsrt_hh_add(impl->objectHash, handshake);

    ddsrt_mutex_unlock(&impl->lock);

    handshake_message->class_id = ddsrt_strdup(AUTH_HANDSHAKE_REQUEST_TOKEN_ID);
    handshake_message->properties._length  = 0;
    handshake_message->properties._buffer  = NULL;
    handshake_message->binary_properties._length = 8;
    handshake_message->binary_properties._buffer = tokens;
    *handshake_handle = HANDSHAKE_HANDLE(handshake);

    return DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE;

err_get_public_key:
err_gen_dh_keys:
    if (created) {
        (void)ddsrt_hh_remove(impl->objectHash, handshake);
        security_object_free((SecurityObject *)handshake);
    }
err_alloc_cid:
    ddsrt_free(certData);
err_inv_handle:
    ddsrt_mutex_unlock(&impl->lock);
err_bad_param:
    return DDS_SECURITY_VALIDATION_FAILED;
}

static DDS_Security_ValidationResult_t
validate_pdata(
    const DDS_Security_OctetSeq *seq,
    X509 *cert,
    DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result = DDS_SECURITY_VALIDATION_OK;
    DDS_Security_Deserializer deserializer;
    DDS_Security_ParticipantBuiltinTopicData *pdata;
    DDS_Security_GUID_t cguid, aguid;

    deserializer = DDS_Security_Deserializer_new(seq->_buffer, seq->_length);
    if (!deserializer) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "begin_handshake_reply: c.pdata invalid encoding");
        goto err_invalid_data;
    }

    pdata = DDS_Security_ParticipantBuiltinTopicData_alloc();

    if (!DDS_Security_Deserialize_ParticipantBuiltinTopicData(deserializer, pdata, ex)) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        goto err_incorrect_data;
    }

    memset(&cguid, 0, sizeof(DDS_Security_GUID_t));
    result = get_adjusted_participant_guid(cert, &cguid, &aguid, ex);
    if (result == DDS_SECURITY_VALIDATION_OK) {
        DDS_Security_BuiltinTopicKey_t key;
        DDS_Security_BuiltinTopicKeyBE(key, pdata->key);
        if (memcmp(key, aguid.prefix, 6) != 0) {
            result = DDS_SECURITY_VALIDATION_FAILED;
              DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                      "begin_handshake_reply: c.pdata contains incorrect participant guid");
        }
    }

err_incorrect_data:
    DDS_Security_ParticipantBuiltinTopicData_free(pdata);
    DDS_Security_Deserializer_free(deserializer);
err_invalid_data:
    return result;
}

static DDS_Security_ValidationResult_t
validate_handshake_request_token(
    const DDS_Security_HandshakeMessageToken *token,
    HandshakeInfo *handshake,
    X509Seq *trusted_ca_list,
    DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result = DDS_SECURITY_VALIDATION_OK;
    IdentityRelation *relation = handshake->relation;
    X509 *identityCert;
    const DDS_Security_BinaryProperty_t *c_id;
    const DDS_Security_BinaryProperty_t *c_perm;
    const DDS_Security_BinaryProperty_t *c_pdata;
    const DDS_Security_BinaryProperty_t *c_dsign_algo;
    const DDS_Security_BinaryProperty_t *c_kagree_algo;
    const DDS_Security_BinaryProperty_t *dh1;
    const DDS_Security_BinaryProperty_t *challenge;
    const DDS_Security_BinaryProperty_t *hash_c1;
    EVP_PKEY *pdhkey = NULL;
    AuthenticationAlgoKind_t dsignAlgoKind;
    AuthenticationAlgoKind_t kagreeAlgoKind;
    unsigned i;

    assert(relation);

    /* Check class_id */
    if (!token->class_id ||
        (strncmp(AUTH_HANDSHAKE_REQUEST_TOKEN_ID, token->class_id, strlen(AUTH_HANDSHAKE_REQUEST_TOKEN_ID)) != 0)) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "begin_handshake_reply: HandshakeMessageToken incorrect class_id: %s (expected %s)", token->class_id ? token->class_id: "NULL", AUTH_HANDSHAKE_REQUEST_TOKEN_ID);
        goto err_inv_class_id;
    }

    /* Check presents of mandatory properties
     * - c.id
     * - c.perm
     * - c.pdata
     * - c.dsign_algo
     * - c.kagree_algo
     * - dh1
     * - challenge1
     */
    c_id = DDS_Security_DataHolder_find_binary_property(token, "c.id");
    if (!c_id || (c_id->value._length == 0) || (c_id->value._buffer == NULL)) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "begin_handshake_reply: HandshakeMessageToken property c.id missing");
        goto err_no_c_id;
    }

    result = load_X509_certificate_from_data((char*)c_id->value._buffer, (int)c_id->value._length, &identityCert, ex);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        goto err_identity_cert_load;
    }

    if( trusted_ca_list->length == 0 ){ //no trusted set. check with local CA
        result = verify_certificate(identityCert, relation->localIdentity->identityCA, ex);
    }
    else{
        /* Make sure we have a clean exception, in case it was uninitialized. */
        DDS_Security_Exception_clean(ex);
        for (i = 0; i < trusted_ca_list->length; ++i) {
            /* We'll only return the exception of the last one, if it failed. */
            DDS_Security_Exception_reset(ex);
            result = verify_certificate(identityCert, trusted_ca_list->buffer[i], ex);
            if (result == DDS_SECURITY_VALIDATION_OK) {
                break;
            }
        }
    }

    if (result != DDS_SECURITY_VALIDATION_OK) {
        goto err_inv_identity_cert;
    }

    result = check_certificate_expiry( identityCert, ex);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        goto err_inv_identity_cert;
    }

    c_perm = DDS_Security_DataHolder_find_binary_property(token, "c.perm");
    if (!c_perm) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "begin_handshake_reply: HandshakeMessageToken property c.perm missing");
        goto err_no_c_perm;
    }

    if (c_perm->value._length > 0) {
        ddsrt_free(relation->remoteIdentity->permissionsDocument);
        relation->remoteIdentity->permissionsDocument = string_from_data(c_perm->value._buffer, c_perm->value._length);
    }

    c_pdata = DDS_Security_DataHolder_find_binary_property(token, "c.pdata");
    if (!c_pdata) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "begin_handshake_reply: HandshakeMessageToken property c.pdata missing");
        goto err_no_c_pdata;
    }

    result = validate_pdata(&c_pdata->value, identityCert, ex);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        goto err_inv_pdata;
    }

    c_dsign_algo = DDS_Security_DataHolder_find_binary_property(token, "c.dsign_algo");
    if (!c_dsign_algo) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "begin_handshake_reply: HandshakeMessageToken property c.dsign_algo missing");
        goto err_no_c_dsign_algo;
    }

    dsignAlgoKind = get_dsign_algo_from_string((const char *)c_dsign_algo->value._buffer);
    if (dsignAlgoKind == AUTH_ALGO_KIND_UNKNOWN) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "begin_handshake_reply: HandshakeMessageToken property c.dsign_algo not supported");
        goto err_no_c_dsign_algo;
    }

    c_kagree_algo = DDS_Security_DataHolder_find_binary_property(token, "c.kagree_algo");
    if (!c_kagree_algo) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "begin_handshake_reply: HandshakeMessageToken property c.kagree_algo missing");
        goto err_no_c_kagree_algo;
    }

    kagreeAlgoKind = get_kagree_algo_from_string((const char *)c_kagree_algo->value._buffer);
    if (kagreeAlgoKind == AUTH_ALGO_KIND_UNKNOWN) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "begin_handshake_reply: HandshakeMessageToken property c.kagree_algo not support");
        goto err_no_c_kagree_algo;
    }

    dh1 = DDS_Security_DataHolder_find_binary_property(token, "dh1");
    if (!dh1 || dh1->value._length == 0 || dh1->value._buffer == NULL) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "begin_handshake_reply: HandshakeMessageToken property dh1 missing");
        goto err_no_dh;
    }

    result = dh_oct_to_public_key(&pdhkey, kagreeAlgoKind, dh1->value._buffer, dh1->value._length, ex);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        goto err_no_dh;
    }

    challenge = DDS_Security_DataHolder_find_binary_property(token, "challenge1");
    if (!challenge) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "begin_handshake_reply: HandshakeMessageToken property challenge1 missing");
        goto err_no_challenge;
    }

    if (challenge->value._length != sizeof(AuthenticationChallenge) || challenge->value._buffer == NULL) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "begin_handshake_reply: HandshakeMessageToken property challenge1 invalid");
        goto err_no_challenge;
    }

    /* When validate_remote_identity was provided with a remote_auth_request_token
     * then the future_challenge in the remote identity was set and the challenge1
     * property of the handshake_request_token should be the same as the
     * future_challenge stored in the remote identity.
     */
    if (relation->rchallenge) {
        if (memcmp(relation->rchallenge->value, challenge->value._buffer, sizeof(AuthenticationChallenge)) != 0) {
            result = DDS_SECURITY_VALIDATION_FAILED;
             DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                     "begin_handshake_reply: HandshakeMessageToken property challenge1 does not match future_challenge");
             goto err_no_challenge;
        }
    } else {
        if (challenge->value._length == sizeof(relation->rchallenge->value)) {
            relation->rchallenge = ddsrt_malloc(sizeof(AuthenticationChallenge));
            memcpy(relation->rchallenge, challenge->value._buffer, challenge->value._length);
        } else {
            result = DDS_SECURITY_VALIDATION_FAILED;
            DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                    "begin_handshake_reply: HandshakeMessageToken property challenge1 invalid (incorrect size)");
            goto err_no_challenge;
        }
    }

    /* Compute the hash_c1 value */
    {
        const DDS_Security_BinaryProperty_t * binary_properties[5];

        binary_properties[0] = c_id;
        binary_properties[1] = c_perm;
        binary_properties[2] = c_pdata;
        binary_properties[3] = c_dsign_algo;
        binary_properties[4] = c_kagree_algo;

        (void)compute_hash_value(&handshake->hash_c1[0], binary_properties, 5, NULL);
    }

    hash_c1 = DDS_Security_DataHolder_find_binary_property(token, "hash_c1");
    if (hash_c1) {
        if ((hash_c1->value._length == sizeof(HashValue_t)) &&
            (memcmp(hash_c1->value._buffer, &handshake->hash_c1, sizeof(HashValue_t)) == 0)) {
        } else {
            result = DDS_SECURITY_VALIDATION_FAILED;
            DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                    "begin_handshake_reply: HandshakeMessageToken property hash_c1 invalid (incorrect size)");
            goto err_inv_hash_c1;
        }
    }

    if (!relation->remoteIdentity->identityCert) {
        relation->remoteIdentity->identityCert = identityCert;
    } else {
        X509_free(relation->remoteIdentity->identityCert);
        relation->remoteIdentity->identityCert = identityCert;
    }

    relation->remoteIdentity->dsignAlgoKind = dsignAlgoKind;
    relation->remoteIdentity->kagreeAlgoKind = kagreeAlgoKind;

    DDS_Security_OctetSeq_copy(&relation->remoteIdentity->pdata, &c_pdata->value);

    handshake->rdh = pdhkey;

    return result;

err_inv_hash_c1:
err_no_challenge:
    EVP_PKEY_free(pdhkey);
err_no_dh:
err_no_c_kagree_algo:
err_no_c_dsign_algo:
err_inv_pdata:
err_no_c_pdata:
err_no_c_perm:
err_inv_identity_cert:
    X509_free(identityCert);
err_identity_cert_load:
err_no_c_id:
err_inv_class_id:
    return result;
}


static DDS_Security_ValidationResult_t
validate_handshake_reply_token(
    const DDS_Security_HandshakeMessageToken *token,
    HandshakeInfo *handshake,
    EVP_PKEY **pdhkey,
    X509Seq *trusted_ca_list,
    DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result = DDS_SECURITY_VALIDATION_OK;
    IdentityRelation *relation = handshake->relation;
    X509 *identityCert;
    EVP_PKEY *public_key;
    const DDS_Security_BinaryProperty_t *c_id;
    const DDS_Security_BinaryProperty_t *c_perm;
    const DDS_Security_BinaryProperty_t *c_pdata;
    const DDS_Security_BinaryProperty_t *c_dsign_algo;
    const DDS_Security_BinaryProperty_t *c_kagree_algo;
    const DDS_Security_BinaryProperty_t *dh1;
    const DDS_Security_BinaryProperty_t *dh2;
    const DDS_Security_BinaryProperty_t *hash_c1;
    const DDS_Security_BinaryProperty_t *hash_c2;
    const DDS_Security_BinaryProperty_t *challenge1;
    const DDS_Security_BinaryProperty_t *challenge2;
    const DDS_Security_BinaryProperty_t *signature;
    AuthenticationAlgoKind_t dsignAlgoKind;
    AuthenticationAlgoKind_t kagreeAlgoKind;

    unsigned i;

    assert(relation);

    /* Check class_id */
    if (!token->class_id ||
        (strncmp(AUTH_HANDSHAKE_REPLY_TOKEN_ID, token->class_id, strlen(AUTH_HANDSHAKE_REPLY_TOKEN_ID)) != 0)) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "process_handshake: HandshakeMessageToken incorrect class_id: %s (expected %s)", token->class_id ? token->class_id: "NULL", AUTH_HANDSHAKE_REPLY_TOKEN_ID);
        goto err_inv_class_id;
    }

    /* Check presents of mandatory properties
     * - c.id
     * - c.perm
     * - c.pdata
     * - c.dsign_algo
     * - c.kagree_algo
     * - challenge1
     * - dh2
     * - challenge2
     * - signature
     */
    c_id = DDS_Security_DataHolder_find_binary_property(token, "c.id");
    if (!c_id || (c_id->value._length == 0) || (c_id->value._buffer == NULL)) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "process_handshake: HandshakeMessageToken property c.id missing");
        goto err_no_c_id;
    }

    /* Verify Identity Certificate */
    result = load_X509_certificate_from_data((char*)c_id->value._buffer, (int)c_id->value._length, &identityCert, ex);
    if (result != DDS_SECURITY_VALIDATION_OK ) {
        goto err_identity_cert_load;
    }

    if( trusted_ca_list->length == 0 ){ //no trusted set. check with local CA
        result = verify_certificate(identityCert, relation->localIdentity->identityCA, ex);
    }
    else{
        /* Make sure we have a clean exception, in case it was uninitialized. */
        DDS_Security_Exception_clean(ex);
        for (i = 0; i < trusted_ca_list->length; ++i) {
            /* We'll only return the exception of the last one, if it failed. */
            DDS_Security_Exception_reset(ex);
            result = verify_certificate(identityCert, trusted_ca_list->buffer[i], ex);
            if (result == DDS_SECURITY_VALIDATION_OK) {
                break;
            }
        }
    }

    if (result != DDS_SECURITY_VALIDATION_OK) {
        goto err_inv_identity_cert;
    }

    result = check_certificate_expiry( identityCert, ex);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        goto err_inv_identity_cert;
    }

    c_perm = DDS_Security_DataHolder_find_binary_property(token, "c.perm");
    if (!c_perm) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "process_handshake: HandshakeMessageToken property c.perm missing");
        goto err_no_c_perm;
    }

    if (c_perm->value._length > 0) {
        ddsrt_free(relation->remoteIdentity->permissionsDocument);
        relation->remoteIdentity->permissionsDocument = string_from_data(c_perm->value._buffer, c_perm->value._length);
    }

    c_pdata = DDS_Security_DataHolder_find_binary_property(token, "c.pdata");
    if (!c_pdata) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "process_handshake: HandshakeMessageToken property c.pdata missing");
        goto err_no_c_pdata;
    }

    result = validate_pdata(&c_pdata->value, identityCert, ex);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        goto err_inv_pdata;
    }

    c_dsign_algo = DDS_Security_DataHolder_find_binary_property(token, "c.dsign_algo");
    if (!c_dsign_algo) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "process_handshake: HandshakeMessageToken property c.dsign_algo missing");
        goto err_no_c_dsign_algo;
    }

    dsignAlgoKind = get_dsign_algo_from_string((const char *)c_dsign_algo->value._buffer);
    if (dsignAlgoKind == AUTH_ALGO_KIND_UNKNOWN) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "process_handshake: HandshakeMessageToken property c.dsign_algo not supported");
        goto err_no_c_dsign_algo;
    }

    c_kagree_algo = DDS_Security_DataHolder_find_binary_property(token, "c.kagree_algo");
    if (!c_kagree_algo) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "process_handshake: HandshakeMessageToken property c.kagree_algo missing");
        goto err_no_c_kagree_algo;
    }

    kagreeAlgoKind = get_kagree_algo_from_string((const char *)c_kagree_algo->value._buffer);
    if (kagreeAlgoKind == AUTH_ALGO_KIND_UNKNOWN) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "process_handshake: HandshakeMessageToken property c.kagree_algo not support");
        goto err_no_c_kagree_algo;
    }

    /* dh1 is optional  */
    dh1 = DDS_Security_DataHolder_find_binary_property(token, "dh1");
    DDSRT_UNUSED_ARG(dh1); /*print it for integration purposes */

    dh2 = DDS_Security_DataHolder_find_binary_property(token, "dh2");
    if (!dh2 || dh2->value._length == 0 || dh2->value._buffer == NULL) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "process_handshake: HandshakeMessageToken property dh2 missing");
        goto err_no_dh;
    }

    hash_c1 = DDS_Security_DataHolder_find_binary_property(token, "hash_c1");
    if (hash_c1) {
        if ((hash_c1->value._length == sizeof(HashValue_t)) &&
                (memcmp(hash_c1->value._buffer, handshake->hash_c1, sizeof(HashValue_t)) == 0)) {
        } else {
            result = DDS_SECURITY_VALIDATION_FAILED;
            DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                    "begin_handshake_reply: HandshakeMessageToken property hash_c1 invalid");
            goto err_inv_hash_c1;
        }
    }

    /* Compute the hash_c2 value */
    {
        const DDS_Security_BinaryProperty_t * binary_properties[5];

        binary_properties[0] = c_id;
        binary_properties[1] = c_perm;
        binary_properties[2] = c_pdata;
        binary_properties[3] = c_dsign_algo;
        binary_properties[4] = c_kagree_algo;

        (void)compute_hash_value(&handshake->hash_c2[0], binary_properties, 5, NULL);
    }

    hash_c2 = DDS_Security_DataHolder_find_binary_property(token, "hash_c2");
    if (hash_c2) {
        if ((hash_c2->value._length == sizeof(HashValue_t)) &&
            (memcmp(hash_c2->value._buffer, handshake->hash_c2, sizeof(HashValue_t)) == 0)) {
        } else {
            result = DDS_SECURITY_VALIDATION_FAILED;
            DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                    "begin_handshake_reply: HandshakeMessageToken property hash_c2 invalid");
            goto err_inv_hash_c2;
        }
    }

    signature = DDS_Security_DataHolder_find_binary_property(token, "signature");
    if (!signature || signature->value._length == 0 || signature->value._buffer == NULL) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "process_handshake: HandshakeMessageToken property signature missing");
        goto err_no_signature;
    }

    *pdhkey = NULL;
    result = dh_oct_to_public_key(pdhkey, kagreeAlgoKind, dh2->value._buffer, dh2->value._length, ex);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        goto err_inv_dh;
    }

    challenge1 = DDS_Security_DataHolder_find_binary_property(token, "challenge1");
    if (!challenge1) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "process_handshake: HandshakeMessageToken property challenge1 missing");
        goto err_no_challenge;
    }

    if (challenge1->value._length != sizeof(AuthenticationChallenge) || challenge1->value._buffer == NULL) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "process_handshake: HandshakeMessageToken property challenge1 invalid");
        goto err_no_challenge;
    }


    challenge2 = DDS_Security_DataHolder_find_binary_property(token, "challenge2");
    if (!challenge2) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "process_handshake: HandshakeMessageToken property challenge2 missing");
        goto err_no_challenge;
    }

    if (challenge2->value._length != sizeof(AuthenticationChallenge) || challenge2->value._buffer == NULL) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "process_handshake: HandshakeMessageToken property challenge2 invalid");
        goto err_no_challenge;
    }
    /* When validate_remote_identity was provided with a remote_auth_request_token
     * then the future_challenge in the remote identity was set and the challenge2
     * property of the handshake_reply_token should be the same as the
     * future_challenge stored in the remote identity.
     */



    if (relation->rchallenge) {
        if (memcmp(relation->rchallenge->value, challenge2->value._buffer, sizeof(AuthenticationChallenge)) != 0) {
            result = DDS_SECURITY_VALIDATION_FAILED;
             DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                     "process_handshake: HandshakeMessageToken property challenge2 does not match future_challenge");
             goto err_no_challenge;
        }
    } else {
        if (challenge2->value._length == sizeof(relation->rchallenge->value)) {
            relation->rchallenge = ddsrt_malloc(sizeof(AuthenticationChallenge));
            memcpy(relation->rchallenge, challenge2->value._buffer, challenge2->value._length);
        } else {
            result = DDS_SECURITY_VALIDATION_FAILED;
              DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                      "process_handshake: HandshakeMessageToken property challenge2 invalid (incorrect size)");
              goto err_no_challenge;
        }
    }


    if (relation->lchallenge) {
        if (memcmp(relation->lchallenge->value, challenge1->value._buffer, sizeof(AuthenticationChallenge)) != 0) {
            result = DDS_SECURITY_VALIDATION_FAILED;
            DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                 "process_handshake: HandshakeMessageToken property challenge1 does not match future_challenge");
            goto err_no_challenge;
        }
    } else {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
             "process_handshake: No future challenge exists for this token");
        goto err_no_challenge;
    }


    /* TODO: check if an identity certificate was already associated with the remote identity and
     * when that is the case both should be the same
     */
    if (!relation->remoteIdentity->identityCert) {
        relation->remoteIdentity->identityCert = identityCert;
    } else {
        X509_free(relation->remoteIdentity->identityCert);
        relation->remoteIdentity->identityCert = identityCert;
    }

    relation->remoteIdentity->dsignAlgoKind = dsignAlgoKind;
    relation->remoteIdentity->kagreeAlgoKind = kagreeAlgoKind;


    public_key = X509_get_pubkey(relation->remoteIdentity->identityCert);
    if (public_key) {
        /*prepare properties*/
        const DDS_Security_BinaryProperty_t *properties[HANDSHAKE_SIGNATURE_CONTENT_SIZE];
        DDS_Security_BinaryProperty_t *hash_c1_val = hash_value_to_binary_property("hash_c1", handshake->hash_c1);
        DDS_Security_BinaryProperty_t *hash_c2_val = hash_value_to_binary_property("hash_c2", handshake->hash_c2);

        properties[0] = hash_c2_val;
        properties[1] = challenge2;
        properties[2] = dh2;
        properties[3] = challenge1;
        properties[4] = dh1;
        properties[5] = hash_c1_val;

        result = validate_signature(public_key,properties, HANDSHAKE_SIGNATURE_CONTENT_SIZE, signature->value._buffer,signature->value._length, ex);

        EVP_PKEY_free(public_key);
        DDS_Security_BinaryProperty_free(hash_c1_val);
        DDS_Security_BinaryProperty_free(hash_c2_val);

        if( result != DDS_SECURITY_VALIDATION_OK) {
            goto err_inv_signature;
        }
    } else {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "X509_get_pubkey failed");
        goto err_inv_identity_cert;
    }


    DDS_Security_OctetSeq_copy(&relation->remoteIdentity->pdata, &c_pdata->value);

    return result;

err_inv_signature:
err_no_challenge:
err_inv_dh:
err_no_signature:
err_inv_hash_c2:
err_inv_hash_c1:
err_no_dh:
err_no_c_kagree_algo:
err_no_c_dsign_algo:
err_inv_pdata:
err_no_c_pdata:
err_no_c_perm:
err_inv_identity_cert:
    X509_free(identityCert);
err_identity_cert_load:
err_no_c_id:
err_inv_class_id:
    return result;
}


static DDS_Security_ValidationResult_t
validate_handshake_final_token(
    const DDS_Security_HandshakeMessageToken *token,
    HandshakeInfo *handshake,
    DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result = DDS_SECURITY_VALIDATION_OK;
    IdentityRelation *relation = handshake->relation;
    const DDS_Security_BinaryProperty_t *dh1;
    const DDS_Security_BinaryProperty_t *dh2;
    const DDS_Security_BinaryProperty_t *hash_c1;
    const DDS_Security_BinaryProperty_t *hash_c2;
    const DDS_Security_BinaryProperty_t *challenge1;
    const DDS_Security_BinaryProperty_t *challenge2;
    const DDS_Security_BinaryProperty_t *signature;
    EVP_PKEY *public_key;

    assert(relation);

    /* Check class_id */
    if (!token->class_id ||
        (strncmp(AUTH_HANDSHAKE_FINAL_TOKEN_ID, token->class_id, strlen(AUTH_HANDSHAKE_FINAL_TOKEN_ID)) != 0)) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "process_handshake: HandshakeMessageToken incorrect class_id: %s (expected %s)", token->class_id ? token->class_id: "NULL", AUTH_HANDSHAKE_FINAL_TOKEN_ID);
        goto err_inv_class_id;
    }

    /* Check presents of mandatory properties
     * - challenge1
     * - challenge2
     * - signature
     */


    /* dh1 is optional  */
    dh1 = DDS_Security_DataHolder_find_binary_property(token, "dh1");

    DDSRT_UNUSED_ARG(dh1); /*print it for integration purposes */

    /* dh2 is optional  */
    dh2 = DDS_Security_DataHolder_find_binary_property(token, "dh2");
    DDSRT_UNUSED_ARG(dh2); /*print it for integration purposes */

    /* hash_c1 is optional  */
    hash_c1 = DDS_Security_DataHolder_find_binary_property(token, "hash_c1");
    if (hash_c1) {
        if ((hash_c1->value._length == sizeof(HashValue_t)) &&
                (memcmp(hash_c1->value._buffer, handshake->hash_c1, sizeof(HashValue_t)) == 0)) {
        } else {
            result = DDS_SECURITY_VALIDATION_FAILED;
            DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                    "begin_handshake_reply: HandshakeMessageToken property hash_c1 invalid");
            goto err_inv_hash_c1;
        }
    }

    /* hash_c2 is optional  */
    hash_c2 = DDS_Security_DataHolder_find_binary_property(token, "hash_c2");
    if (hash_c2) {
        if ((hash_c2->value._length == sizeof(HashValue_t)) &&
            (memcmp(hash_c2->value._buffer, handshake->hash_c2, sizeof(HashValue_t)) == 0)) {
        } else {
            result = DDS_SECURITY_VALIDATION_FAILED;
            DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                    "begin_handshake_reply: HandshakeMessageToken property hash_c2 invalid");
            goto err_inv_hash_c2;
        }
    }

    challenge1 = DDS_Security_DataHolder_find_binary_property(token, "challenge1");
    if (!challenge1) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "process_handshake: HandshakeMessageToken property challenge1 missing");
        goto err_no_challenge;
    }

    if (challenge1->value._length != sizeof(AuthenticationChallenge) || challenge1->value._buffer == NULL) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "process_handshake: HandshakeMessageToken property challenge1 invalid");
        goto err_no_challenge;
    }


    challenge2 = DDS_Security_DataHolder_find_binary_property(token, "challenge2");
    if (!challenge2) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "process_handshake: HandshakeMessageToken property challenge2 missing");
        goto err_no_challenge;
    }

    if (challenge2->value._length != sizeof(AuthenticationChallenge) || challenge2->value._buffer == NULL) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "process_handshake: HandshakeMessageToken property challenge2 invalid");
        goto err_no_challenge;
    }

    /* When validate_remote_identity was provided with a remote_auth_request_token
     * then the future_challenge in the remote identity was set and the challenge1
     * property of the handshake_reply_token should be the same as the
     * future_challenge stored in the remote identity.
     */



    if (relation->rchallenge) {
        if (memcmp(relation->rchallenge->value, challenge1->value._buffer, sizeof(AuthenticationChallenge)) != 0) {
            result = DDS_SECURITY_VALIDATION_FAILED;
             DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                     "process_handshake: HandshakeMessageToken property challenge1 does not match future_challenge");
             goto err_no_challenge;
        }
    } else {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
             "process_handshake: No challenge exists to check challenge1 in the token.");
        goto err_no_challenge;
    }

    if (relation->lchallenge) {
        if (memcmp(relation->lchallenge->value, challenge2->value._buffer, sizeof(AuthenticationChallenge)) != 0) {
            result = DDS_SECURITY_VALIDATION_FAILED;
             DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                     "process_handshake: HandshakeMessageToken property challenge2 does not match future_challenge");
             goto err_no_challenge;
        }
    } else {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
             "process_handshake: No challenge exists to check challenge2 in the token.");
        goto err_no_challenge;
    }

    signature = DDS_Security_DataHolder_find_binary_property(token, "signature");
    if (!signature) {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result,
                "process_handshake: HandshakeMessageToken property signature missing");
        goto err_no_challenge;
    }

    /* Validate signature */
    public_key = X509_get_pubkey(relation->remoteIdentity->identityCert);
    if (public_key) {
        /*prepare properties*/
        const DDS_Security_BinaryProperty_t *properties[HANDSHAKE_SIGNATURE_CONTENT_SIZE];
        DDS_Security_BinaryProperty_t *hash_c1_val = hash_value_to_binary_property("hash_c1", handshake->hash_c1);
        DDS_Security_BinaryProperty_t *hash_c2_val = hash_value_to_binary_property("hash_c2", handshake->hash_c2);

        properties[0] = hash_c1_val;
        properties[1] = challenge1;
        properties[2] = dh1;
        properties[3] = challenge2;
        properties[4] = dh2;
        properties[5] = hash_c2_val;

        result = validate_signature(public_key,properties, HANDSHAKE_SIGNATURE_CONTENT_SIZE ,signature->value._buffer,signature->value._length,ex );

        EVP_PKEY_free(public_key);
        DDS_Security_BinaryProperty_free(hash_c1_val);
        DDS_Security_BinaryProperty_free(hash_c2_val);

        if (result != DDS_SECURITY_VALIDATION_OK) {
            goto err_inv_signature;
        }
    } else {
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "X509_get_pubkey failed");
        goto err_inv_identity_cert;
    }

err_inv_hash_c2:
err_inv_hash_c1:
err_no_challenge:
err_inv_class_id:
err_inv_identity_cert:
err_inv_signature:
    return result;
}

DDS_Security_ValidationResult_t
begin_handshake_reply(
        dds_security_authentication *instance,
        DDS_Security_HandshakeHandle *handshake_handle,
        DDS_Security_HandshakeMessageToken *handshake_message_out,
        const DDS_Security_HandshakeMessageToken *handshake_message_in,
        const DDS_Security_IdentityHandle initiator_identity_handle,
        const DDS_Security_IdentityHandle replier_identity_handle,
        const DDS_Security_OctetSeq *serialized_local_participant_data,
        DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result = DDS_SECURITY_VALIDATION_OK;
    dds_security_authentication_impl *impl = (dds_security_authentication_impl *) instance;
    HandshakeInfo *handshake = NULL;
    IdentityRelation *relation = NULL;
    SecurityObject *obj;
    LocalIdentityInfo *localIdent;
    RemoteIdentityInfo *remoteIdent;
    EVP_PKEY *dhkeyLocal = NULL;
    DDS_Security_BinaryProperty_t *tokens;
    DDS_Security_BinaryProperty_t *c_id;
    DDS_Security_BinaryProperty_t *c_perm;
    DDS_Security_BinaryProperty_t *c_pdata;
    DDS_Security_BinaryProperty_t *c_dsign_algo;
    DDS_Security_BinaryProperty_t *c_kagree_algo;
    DDS_Security_BinaryProperty_t *hash_c1;
    const DDS_Security_BinaryProperty_t *hash_c1_ref;
    DDS_Security_BinaryProperty_t *hash_c2;
    DDS_Security_BinaryProperty_t *dh1;
    const DDS_Security_BinaryProperty_t *dh1_ref;
    DDS_Security_BinaryProperty_t *dh2;
    DDS_Security_BinaryProperty_t *challenge1;
    DDS_Security_BinaryProperty_t *challenge2;
    DDS_Security_BinaryProperty_t *signature;
    unsigned char *certData;
    unsigned char *dhPubKeyData;
    uint32_t certDataSize, dhPubKeyDataSize;
    uint32_t tokenSize, idx;
    int created = 0;

    /* validate provided arguments */
    if (!instance || !handshake_handle || !handshake_message_out || !handshake_message_in || !serialized_local_participant_data) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                "begin_handshake_reply: Invalid parameter provided");
        goto err_bad_param;
    }

    if ((serialized_local_participant_data->_length == 0) || (serialized_local_participant_data->_buffer == NULL)) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                "begin_handshake_reply: Invalid parameter provided");
        goto err_bad_param;
    }

    ddsrt_mutex_lock(&impl->lock);

    obj = security_object_find(impl->objectHash, replier_identity_handle);
    if (!obj || !security_object_valid(obj, SECURITY_OBJECT_KIND_LOCAL_IDENTITY)) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                "begin_handshake_reply: Invalid replier_identity_handle provided");
        goto err_inv_handle;
    }
    localIdent = (LocalIdentityInfo *) obj;

    obj = security_object_find(impl->objectHash, initiator_identity_handle);
    if (!obj || !security_object_valid(obj, SECURITY_OBJECT_KIND_REMOTE_IDENTITY)) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                "begin_handshake_reply: Invalid initiator_identity_handle provided");
        goto err_inv_handle;
    }
    remoteIdent = (RemoteIdentityInfo *)obj;

    handshake = find_handshake(impl, SECURITY_OBJECT_HANDLE(localIdent), SECURITY_OBJECT_HANDLE(remoteIdent));
    if (!handshake) {
        relation = find_identity_relation(remoteIdent, SECURITY_OBJECT_HANDLE(localIdent));
        assert(relation);
        handshake = handshakeInfoNew(localIdent, remoteIdent, relation);
        handshake->created_in = CREATEDREPLY;
        (void)ddsrt_hh_add(impl->objectHash, handshake);
        created = 1;
    } else {
        relation = handshake->relation;
        assert(relation);
    }

    result = validate_handshake_request_token(handshake_message_in, handshake, &(impl->trustedCAList), ex);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        goto err_inv_token;
    }

    result = get_certificate_contents(localIdent->identityCert, &certData, &certDataSize, ex);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        goto err_alloc_cid;
    }

    if (!handshake->ldh) {
        result = generate_dh_keys(&dhkeyLocal, remoteIdent->kagreeAlgoKind, ex);
        if (result != DDS_SECURITY_VALIDATION_OK) {
            goto err_gen_dh_keys;
        }

        handshake->ldh = dhkeyLocal;
        EVP_PKEY_copy_parameters(handshake->rdh, handshake->ldh);
    }

    result = dh_public_key_to_oct(handshake->ldh, remoteIdent->kagreeAlgoKind, &dhPubKeyData, &dhPubKeyDataSize, ex);
    if (result != DDS_SECURITY_VALIDATION_OK) {
        goto err_get_public_key;
    }

    if (localIdent->pdata._length == 0) {
        DDS_Security_OctetSeq_copy(&localIdent->pdata, serialized_local_participant_data);
    }

    hash_c1_ref = DDS_Security_DataHolder_find_binary_property(handshake_message_in, "hash_c1");
    tokenSize = hash_c1_ref ? 12 : 11;

    tokens = DDS_Security_BinaryPropertySeq_allocbuf(tokenSize);
    idx = 0;
    c_id = &tokens[idx++];
    c_perm = &tokens[idx++];
    c_pdata = &tokens[idx++];
    c_dsign_algo = &tokens[idx++];
    c_kagree_algo = &tokens[idx++];
    signature = &tokens[idx++];
    hash_c2 = &tokens[idx++];
    challenge2 = &tokens[idx++];
    dh2 = &tokens[idx++];
    challenge1 = &tokens[idx++];
    dh1 = &tokens[idx++];
    hash_c1 = hash_c1_ref ? &tokens[idx++] : NULL;

    /* Store the Identity Certificate associated with the local identify in c.id property */
    DDS_Security_BinaryProperty_set_by_ref(c_id, "c.id", certData, certDataSize);
    certData = NULL;

    /* Store the permission document in the c.perm property */
    if (localIdent->permissionsDocument) {
        DDS_Security_BinaryProperty_set_by_string(c_perm, "c.perm", localIdent->permissionsDocument);
    } else {
        DDS_Security_BinaryProperty_set_by_string(c_perm, "c.perm", "");
    }

    /* Store the provided local_participant_data in the c.pdata property */
    DDS_Security_BinaryProperty_set_by_value(c_pdata, "c.pdata", serialized_local_participant_data->_buffer, serialized_local_participant_data->_length);

    /* Set the used signing algorithm descriptor in c.dsign_algo */
    DDS_Security_BinaryProperty_set_by_string(c_dsign_algo, "c.dsign_algo", get_dsign_algo(localIdent->dsignAlgoKind));

    /* Set the used key algorithm descriptor in c.kagree_algo */
    DDS_Security_BinaryProperty_set_by_string(c_kagree_algo, "c.kagree_algo", get_kagree_algo(remoteIdent->kagreeAlgoKind));

    /* Calculate the hash_c2 */
    {
        DDS_Security_BinaryPropertySeq bseq;

        bseq._length = 5;
        bseq._buffer = tokens;

        get_hash_binary_property_seq(&bseq, handshake->hash_c2);
        DDS_Security_BinaryProperty_set_by_value(hash_c2, "hash_c2", handshake->hash_c2, sizeof(HashValue_t));
    }

    /* Set the DH public key associated with the local participant in dh2 property */
    DDS_Security_BinaryProperty_set_by_ref(dh2, "dh2", dhPubKeyData, dhPubKeyDataSize);

    /* Set the DH public key associated with the local participant in hash_c1 property */
    if (hash_c1) {
        DDS_Security_BinaryProperty_set_by_value(hash_c1, "hash_c1", hash_c1_ref->value._buffer, hash_c1_ref->value._length);
    }

    /* Set the DH public key associated with the local participant in dh1 property */
    if (dh1) {
        dh1_ref = DDS_Security_DataHolder_find_binary_property(handshake_message_in, "dh1");
        if (dh1_ref) {
            DDS_Security_BinaryProperty_set_by_value(dh1, "dh1", dh1_ref->value._buffer, dh1_ref->value._length);
        }
    }

    /* Set the challenge in challenge1 property */
    assert(relation->rchallenge);
    DDS_Security_BinaryProperty_set_by_value(challenge1, "challenge1", relation->rchallenge->value, sizeof(AuthenticationChallenge));

    /* Set the challenge in challenge2 property */
    assert(relation->lchallenge);

    DDS_Security_BinaryProperty_set_by_value(challenge2, "challenge2", relation->lchallenge->value, sizeof(AuthenticationChallenge));

    /* Calculate the signature */
    {
        unsigned char *sign;
        size_t signlen;
        const DDS_Security_BinaryProperty_t * binary_properties[ HANDSHAKE_SIGNATURE_CONTENT_SIZE ];
        DDS_Security_BinaryProperty_t *hash_c1_val = hash_value_to_binary_property("hash_c1", handshake->hash_c1);
        DDS_Security_BinaryProperty_t *hash_c2_val = hash_value_to_binary_property("hash_c2", handshake->hash_c2);

        binary_properties[0] = hash_c2_val;
        binary_properties[1] = challenge2;
        binary_properties[2] = dh2;
        binary_properties[3] = challenge1;
        binary_properties[4] = dh1;
        binary_properties[5] = hash_c1_val;

        result = create_signature(localIdent->privateKey, binary_properties, HANDSHAKE_SIGNATURE_CONTENT_SIZE , &sign, &signlen, ex);

        DDS_Security_BinaryProperty_free(hash_c1_val);
        DDS_Security_BinaryProperty_free(hash_c2_val);

        if (result != DDS_SECURITY_VALIDATION_OK) {
            goto err_signature;
        }
        DDS_Security_BinaryProperty_set_by_ref(signature, "signature", sign, (uint32_t)signlen);
    }

    (void)ddsrt_hh_add(impl->objectHash, handshake);

    handshake_message_out->class_id = ddsrt_strdup(AUTH_HANDSHAKE_REPLY_TOKEN_ID);
    handshake_message_out->binary_properties._length = tokenSize;
    handshake_message_out->binary_properties._buffer = tokens;

    ddsrt_mutex_unlock(&impl->lock);


    *handshake_handle = HANDSHAKE_HANDLE(handshake);

    if (result == DDS_SECURITY_VALIDATION_OK) {
        result = DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE;
    }

    return result;

err_signature:
    free_binary_properties(tokens, tokenSize);
err_get_public_key:
err_gen_dh_keys:
    ddsrt_free(certData);
err_alloc_cid:
err_inv_token:
    if (created) {
        (void)ddsrt_hh_remove(impl->objectHash, handshake);
        security_object_free((SecurityObject *)handshake);
    }
err_inv_handle:
    ddsrt_mutex_unlock(&impl->lock);
err_bad_param:
    return DDS_SECURITY_VALIDATION_FAILED;
}


static bool
generate_shared_secret(
    const HandshakeInfo *handshake,
    unsigned char **shared_secret,
    DDS_Security_long *length,
    DDS_Security_SecurityException *ex)
{
    bool result = false;
    EVP_PKEY_CTX *ctx;
    size_t skeylen;
    unsigned char *secret = NULL;

    *shared_secret = NULL;

    ctx = EVP_PKEY_CTX_new( handshake->ldh, NULL /* no engine */);
    if (!ctx)
    {
        DDS_Security_Exception_set_with_openssl_error(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                        "process_handshake: Shared secret failed to create context: ");
        goto fail_ctx_new;
    }

    if (EVP_PKEY_derive_init(ctx) <= 0)
    {
      DDS_Security_Exception_set_with_openssl_error(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                        "process_handshake: Shared secret failed to initialize context: ");
        goto fail_derive;
    }
    if (EVP_PKEY_derive_set_peer(ctx, handshake->rdh) <= 0)
    {
      DDS_Security_Exception_set_with_openssl_error(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                        "process_handshake: Shared secret failed to set peer key: ");
        goto fail_derive;
    }

    /* Determine buffer length */
    if (EVP_PKEY_derive(ctx, NULL, &skeylen) <= 0)
    {
      DDS_Security_Exception_set_with_openssl_error(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                        "process_handshake:  Shared secret failed to determine key length: ");
        goto fail_derive;
    }

    secret = ddsrt_malloc(skeylen);
    if (EVP_PKEY_derive(ctx, secret, &skeylen) <= 0)
    {
      DDS_Security_Exception_set_with_openssl_error(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                "process_handshake: Could not compute the shared secret: ");
        goto fail_derive;
    }

    *shared_secret = ddsrt_malloc(SHA256_DIGEST_LENGTH);
    *length = SHA256_DIGEST_LENGTH;

    SHA256(secret, skeylen, *shared_secret);

    result = true;

fail_derive:
    ddsrt_free(secret);
    EVP_PKEY_CTX_free(ctx);
fail_ctx_new:
    return result;
}




DDS_Security_ValidationResult_t
process_handshake(
        dds_security_authentication *instance,
        DDS_Security_HandshakeMessageToken *handshake_message_out,
        const DDS_Security_HandshakeMessageToken *handshake_message_in,
        const DDS_Security_HandshakeHandle handshake_handle,
        DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result = DDS_SECURITY_VALIDATION_OK;
    dds_security_authentication_impl *impl = (dds_security_authentication_impl *) instance;
    HandshakeInfo *handshake = NULL;
    IdentityRelation *relation = NULL;
    SecurityObject *obj;
    EVP_PKEY *dhkeyRemote = NULL;
    DDS_Security_BinaryProperty_t *tokens = NULL;
    DDS_Security_BinaryProperty_t *hash_c1 = NULL;
    const DDS_Security_BinaryProperty_t *hash_c1_ref;
    const DDS_Security_BinaryProperty_t *hash_c2_ref;
    const DDS_Security_BinaryProperty_t *challenge1_ref;
    const DDS_Security_BinaryProperty_t *challenge2_ref;
    const DDS_Security_BinaryProperty_t *dh1_ref;
    const DDS_Security_BinaryProperty_t *dh2_ref;
    DDS_Security_BinaryProperty_t *hash_c2 = NULL;
    DDS_Security_BinaryProperty_t *dh1;
    DDS_Security_BinaryProperty_t *dh2;
    DDS_Security_BinaryProperty_t *challenge1;
    DDS_Security_BinaryProperty_t *challenge2;
    DDS_Security_BinaryProperty_t *signature;
    uint32_t tokenSize=0, idx;
    DDS_Security_octet * challenge1_ref_for_shared_secret, *challenge2_ref_for_shared_secret;

    /* validate provided arguments */
    if (!instance || !handshake_handle || !handshake_message_out || !handshake_message_in) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT,
        DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                        "process_handshake: Invalid parameter provided");
        goto err_bad_param;
    }

    memset(handshake_message_out, 0, sizeof(DDS_Security_HandshakeMessageToken));

    ddsrt_mutex_lock(&impl->lock);

    obj = security_object_find(impl->objectHash, handshake_handle);
    if (!obj || !security_object_valid(obj, SECURITY_OBJECT_KIND_HANDSHAKE)) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT,
        DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                        "process_handshake: Invalid replier_identity_handle provided");
        goto err_inv_handle;
    }
    handshake = (HandshakeInfo *) obj;
    relation = handshake->relation;
    assert(relation);

    /* check if the handle created by a handshake_request or handshake_reply */

    switch (handshake->created_in) {
    case CREATEDREQUEST:
        /* The source of the handshake_handle is a begin_handshake_request function
         * So, handshake_message_in should have been came from a remote begin_handshake_reply function
         */
        /* Verify Message Token contents according to Spec 9.3.2.5.2 (Reply Message)  */
        result = validate_handshake_reply_token(handshake_message_in, handshake, &dhkeyRemote, &(impl->trustedCAList), ex);
        if (result != DDS_SECURITY_VALIDATION_OK) {
            goto err_inv_token;
        }

        /*set received remote DH (dh2) */
        handshake->rdh = dhkeyRemote;

        EVP_PKEY_copy_parameters(handshake->rdh, handshake->ldh);

        /* Prepare HandshakeFinalMessageToken */

        /* Get references from message_in */
        hash_c1_ref = DDS_Security_DataHolder_find_binary_property(handshake_message_in, "hash_c1");
        hash_c2_ref = DDS_Security_DataHolder_find_binary_property(handshake_message_in, "hash_c2");
        dh1_ref = DDS_Security_DataHolder_find_binary_property(handshake_message_in, "dh1");
        dh2_ref = DDS_Security_DataHolder_find_binary_property(handshake_message_in, "dh2");
        challenge1_ref = DDS_Security_DataHolder_find_binary_property(handshake_message_in,
                        "challenge1");
        challenge2_ref = DDS_Security_DataHolder_find_binary_property(handshake_message_in,
                        "challenge2");

        tokenSize = 3; /* challenge1, challenge2 and signature are already exist */
        if (hash_c1_ref)
            tokenSize++;
        if (hash_c2_ref)
            tokenSize++;
        if (dh1_ref)
            tokenSize++;
        if (dh2_ref)
            tokenSize++;

        tokens = DDS_Security_BinaryPropertySeq_allocbuf(tokenSize);
        idx = 0;
        signature = &tokens[idx++];
        hash_c2 = hash_c2_ref ? &tokens[idx++] : NULL;
        challenge2 = &tokens[idx++];
        dh2 = dh2_ref ? &tokens[idx++] : NULL;
        challenge1 = &tokens[idx++];
        dh1 = dh1_ref ? &tokens[idx++] : NULL;
        hash_c1 = hash_c1_ref ? &tokens[idx++] : NULL;

        if (hash_c1) {
            DDS_Security_BinaryProperty_set_by_value(hash_c1, "hash_c1", hash_c1_ref->value._buffer,
                            hash_c1_ref->value._length);
        }
        if (hash_c2) {
            DDS_Security_BinaryProperty_set_by_value(hash_c2, "hash_c2", hash_c2_ref->value._buffer,
                            hash_c2_ref->value._length);
        }
        if (dh1) {
            DDS_Security_BinaryProperty_set_by_value(dh1, "dh1", dh1_ref->value._buffer,
                            dh1_ref->value._length);
        }
        if (dh2) {
            DDS_Security_BinaryProperty_set_by_value(dh2, "dh2", dh2_ref->value._buffer,
                            dh2_ref->value._length);
        }
        assert(relation->lchallenge);
        if (challenge1 && challenge1_ref) {
            DDS_Security_BinaryProperty_set_by_value(challenge1, "challenge1", challenge1_ref->value._buffer,
                            challenge1_ref->value._length);
        }
        assert(relation->rchallenge);
        if (challenge2 && challenge2_ref) {
            DDS_Security_BinaryProperty_set_by_value(challenge2, "challenge2", challenge2_ref->value._buffer,
                            challenge2_ref->value._length);
        }


        /* Calculate the signature */
        {
           const DDS_Security_BinaryProperty_t * binary_properties[ HANDSHAKE_SIGNATURE_CONTENT_SIZE ];
           DDS_Security_BinaryProperty_t *hash_c1_val = hash_value_to_binary_property("hash_c1", handshake->hash_c1);
           DDS_Security_BinaryProperty_t *hash_c2_val = hash_value_to_binary_property("hash_c2", handshake->hash_c2);
           unsigned char *sign;
           size_t signlen;

           binary_properties[0] = hash_c1_val;
           binary_properties[1] = challenge1;
           binary_properties[2] = dh1;
           binary_properties[3] = challenge2;
           binary_properties[4] = dh2;
           binary_properties[5] = hash_c2_val;

           result = create_signature(relation->localIdentity->privateKey, binary_properties, HANDSHAKE_SIGNATURE_CONTENT_SIZE, &sign, &signlen, ex);

           DDS_Security_BinaryProperty_free(hash_c1_val);
           DDS_Security_BinaryProperty_free(hash_c2_val);

           if (result != DDS_SECURITY_VALIDATION_OK) {
               goto err_signature;
           }

           DDS_Security_BinaryProperty_set_by_ref(signature, "signature", sign, (uint32_t)signlen);
        }

        handshake_message_out->class_id = ddsrt_strdup(AUTH_HANDSHAKE_FINAL_TOKEN_ID);
        handshake_message_out->binary_properties._length = tokenSize;
        handshake_message_out->binary_properties._buffer = tokens;

        challenge1_ref_for_shared_secret = (DDS_Security_octet*)(handshake->relation->lchallenge);
        challenge2_ref_for_shared_secret = (DDS_Security_octet*)(handshake->relation->rchallenge);

        result =  DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE;

        break;
    case CREATEDREPLY:
        /* The source of the handshake_handle is a begin_handshake_reply function
         * So, handshake_message_in should have been came from a remote process_handshake function
         */

        /* Verify Message Token contents according to Spec 9.3.2.5.3 (Final Message)   */
        result = validate_handshake_final_token(handshake_message_in, handshake, ex);
        if (result != DDS_SECURITY_VALIDATION_OK) {
            goto err_inv_token;
        }


        challenge2_ref_for_shared_secret = (DDS_Security_octet*)(handshake->relation->lchallenge);
        challenge1_ref_for_shared_secret = (DDS_Security_octet*)(handshake->relation->rchallenge);

        result =  DDS_SECURITY_VALIDATION_OK;

        break;
    default:
        ddsrt_mutex_unlock(&impl->lock);
        goto err_bad_param;
    }

    /* Compute shared secret */
    {
        DDS_Security_long shared_secret_length;
        unsigned char *shared_secret;

        if (!generate_shared_secret(handshake, &shared_secret, &shared_secret_length, ex)) {
            goto err_openssl;
        }

        handshake->shared_secret_handle_impl = ddsrt_malloc( sizeof(DDS_Security_SharedSecretHandleImpl));
        handshake->shared_secret_handle_impl->shared_secret = shared_secret;
        handshake->shared_secret_handle_impl->shared_secret_size = shared_secret_length;

        /* put references to challenge1 and challenge2 into shared secret object */
        memcpy( handshake->shared_secret_handle_impl->challenge1, challenge1_ref_for_shared_secret, DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE);
        memcpy( handshake->shared_secret_handle_impl->challenge2, challenge2_ref_for_shared_secret, DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE);
    }

    { /* setup expiry listener */
        dds_time_t certExpiry = get_certificate_expiry( handshake->relation->remoteIdentity->identityCert );

        if( certExpiry == DDS_TIME_INVALID ){
          DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "Expiry date of the certificate is invalid");
          goto err_invalid_expiry;
        } else if( certExpiry != DDS_NEVER ){
            add_validity_end_trigger( impl,
                            IDENTITY_HANDLE( handshake->relation->remoteIdentity ),
                            certExpiry);
        }

    }

    ddsrt_mutex_unlock(&impl->lock);

    return result;

err_invalid_expiry:
    ddsrt_free( handshake->shared_secret_handle_impl->shared_secret );
    ddsrt_free( handshake->shared_secret_handle_impl );
err_openssl:
err_signature:
    if (handshake_message_out->class_id) {
        DDS_Security_DataHolder_deinit(handshake_message_out);
    }
err_inv_token:
err_inv_handle:
    ddsrt_mutex_unlock(&impl->lock);
err_bad_param:
    return DDS_SECURITY_VALIDATION_FAILED;
}

DDS_Security_SharedSecretHandle get_shared_secret(
        dds_security_authentication *instance,
        const DDS_Security_HandshakeHandle handshake_handle,
        DDS_Security_SecurityException *ex)
{

    dds_security_authentication_impl *impl = (dds_security_authentication_impl *) instance;
    SecurityObject *obj;

    /* validate provided arguments */
    if (!instance || !handshake_handle) {
       DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "return_handshake_handle: Invalid parameter provided");
       goto err_bad_param;
    }

    ddsrt_mutex_lock(&impl->lock);
    obj = security_object_find(impl->objectHash, handshake_handle);
    if (!obj || !security_object_valid(obj, SECURITY_OBJECT_KIND_HANDSHAKE)) {
       DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "return_handshake_handle: Invalid handle provided");
       goto err_invalid_handle;
    }

    ddsrt_mutex_unlock(&impl->lock);
    return (DDS_Security_SharedSecretHandle)(ddsrt_address)((HandshakeInfo*)obj)->shared_secret_handle_impl;


    err_invalid_handle:
        ddsrt_mutex_unlock(&impl->lock);
    err_bad_param:
    return DDS_SECURITY_HANDLE_NIL;
}

DDS_Security_boolean
get_authenticated_peer_credential_token(
        dds_security_authentication *instance,
        DDS_Security_AuthenticatedPeerCredentialToken *peer_credential_token,
        const DDS_Security_HandshakeHandle handshake_handle,
        DDS_Security_SecurityException *ex)
{
    dds_security_authentication_impl *impl = (dds_security_authentication_impl *) instance;
    HandshakeInfo *handshake = NULL;
    X509 *identity_cert;
    char *permissions_doc;
    unsigned char *cert_data;
    uint32_t cert_data_size;

    /* validate provided arguments */
    if (!instance || !handshake_handle || !peer_credential_token) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT,
                DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0,
                DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
        return false;
    }

    ddsrt_mutex_lock(&impl->lock);

    handshake = (HandshakeInfo *) security_object_find(impl->objectHash, handshake_handle);
    if (!handshake || !SECURITY_OBJECT_VALID(handshake, SECURITY_OBJECT_KIND_HANDSHAKE)) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT,
                DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0,
                DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
        goto err_inv_handle;
    }

    identity_cert = handshake->relation->remoteIdentity->identityCert;
    if (!identity_cert) {
         DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT,
                 DDS_SECURITY_ERR_OPERATION_NOT_PERMITTED_CODE, 0,
                 DDS_SECURITY_ERR_OPERATION_NOT_PERMITTED_MESSAGE);
         goto err_missing_attr;
     }

    permissions_doc = handshake->relation->remoteIdentity->permissionsDocument;
    if (!permissions_doc) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT,
                DDS_SECURITY_ERR_MISSING_REMOTE_PERMISSIONS_DOCUMENT_CODE, 0,
                DDS_SECURITY_ERR_MISSING_REMOTE_PERMISSIONS_DOCUMENT_MESSAGE);
        goto err_missing_attr;
    }

    if (get_certificate_contents(identity_cert, &cert_data, &cert_data_size, ex) != DDS_SECURITY_VALIDATION_OK) {
        goto err_alloc_cid;
    }

    memset(peer_credential_token, 0, sizeof(*peer_credential_token));

    peer_credential_token->class_id = get_authentication_class_id();

    peer_credential_token->properties._length = 2;
    peer_credential_token->properties._buffer = DDS_Security_PropertySeq_allocbuf(peer_credential_token->properties._length);

    peer_credential_token->properties._buffer[0].name = ddsrt_strdup("c.id");
    peer_credential_token->properties._buffer[0].value = (char *)cert_data;
    peer_credential_token->properties._buffer[0].propagate = false;

    peer_credential_token->properties._buffer[1].name = ddsrt_strdup("c.perm");
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

DDS_Security_boolean set_listener(dds_security_authentication *instance,
        const dds_security_authentication_listener *listener,
        DDS_Security_SecurityException *ex)
{
    dds_security_authentication_impl *auth = (dds_security_authentication_impl*)instance;

    DDSRT_UNUSED_ARG(auth);
    DDSRT_UNUSED_ARG(listener);
    DDSRT_UNUSED_ARG(ex);

    /* Will be enabled after timed call back feature implementation */
#if TIMED_CALLBACK_IMPLEMENTED
    if (listener) {
        ut_timed_dispatcher_enable(auth->timed_callbacks, (void*)listener);
    } else {
        ut_timed_dispatcher_disable(auth->timed_callbacks);
    }
#endif
    return true;
}

DDS_Security_boolean return_identity_token(dds_security_authentication *instance,
        const DDS_Security_IdentityToken *token,
        DDS_Security_SecurityException *ex)
{
    DDSRT_UNUSED_ARG(token);
    DDSRT_UNUSED_ARG(ex);
    DDSRT_UNUSED_ARG(instance);

    return true;
}

DDS_Security_boolean return_identity_status_token(
        dds_security_authentication *instance,
        const DDS_Security_IdentityStatusToken *token,
        DDS_Security_SecurityException *ex)
{
    DDSRT_UNUSED_ARG(token);
    DDSRT_UNUSED_ARG(ex);
    DDSRT_UNUSED_ARG(instance);

    return true;
}

DDS_Security_boolean return_authenticated_peer_credential_token(
        dds_security_authentication *instance,
        const DDS_Security_AuthenticatedPeerCredentialToken *peer_credential_token,
        DDS_Security_SecurityException *ex)
{
    if ((!instance) || (!peer_credential_token)) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
        return false;
    }

    DDS_Security_DataHolder_deinit((DDS_Security_DataHolder *)peer_credential_token);

    return true;
}

DDS_Security_boolean
return_handshake_handle(dds_security_authentication *instance,
        const DDS_Security_HandshakeHandle handshake_handle,
        DDS_Security_SecurityException *ex)
{
    dds_security_authentication_impl *impl = (dds_security_authentication_impl *) instance;
    SecurityObject *obj;
    HandshakeInfo *handshake;

    /* validate provided arguments */
    if (!instance || !handshake_handle) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "return_handshake_handle: Invalid parameter provided");
        goto err_bad_param;
    }

    ddsrt_mutex_lock(&impl->lock);
    obj = security_object_find(impl->objectHash, handshake_handle);
    if (!obj || !security_object_valid(obj, SECURITY_OBJECT_KIND_HANDSHAKE)) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "return_handshake_handle: Invalid handle provided");
        goto err_invalid_handle;
    }
    handshake = (HandshakeInfo *)obj;

    assert(handshake->relation);

    (void)ddsrt_hh_remove(impl->objectHash, obj);
    security_object_free((SecurityObject *)handshake);

    ddsrt_mutex_unlock(&impl->lock);

    return true;

err_invalid_handle:
    ddsrt_mutex_unlock(&impl->lock);
err_bad_param:
    return false;
}


static void
invalidate_local_related_objects(
     dds_security_authentication_impl *impl,
     LocalIdentityInfo *localIdent)
{
    struct ddsrt_hh_iter it;
    SecurityObject *obj;

    for (obj = ddsrt_hh_iter_first(impl->objectHash, &it); obj != NULL; obj = ddsrt_hh_iter_next(&it)) {
        if (obj->kind == SECURITY_OBJECT_KIND_REMOTE_IDENTITY) {
            RemoteIdentityInfo *remoteIdent = (RemoteIdentityInfo *)obj;
            IdentityRelation *relation;
            HandshakeInfo *handshake;

            handshake = find_handshake(impl, SECURITY_OBJECT_HANDLE(localIdent), SECURITY_OBJECT_HANDLE(remoteIdent));
            if (handshake) {
                (void)ddsrt_hh_remove(impl->objectHash, handshake);
                security_object_free((SecurityObject *) handshake);
            }

            relation = find_identity_relation(remoteIdent, SECURITY_OBJECT_HANDLE(localIdent));
            if (relation) {
                remove_identity_relation(remoteIdent, relation);
            }
        }
    }
}

static void
invalidate_remote_related_objects(
    dds_security_authentication_impl *impl,
    RemoteIdentityInfo *remoteIdentity)
{
    struct ddsrt_hh_iter it;
    IdentityRelation *relation;
    HandshakeInfo *handshake;

    for (relation = ddsrt_hh_iter_first(remoteIdentity->linkHash, &it); relation != NULL; relation = ddsrt_hh_iter_next(&it)) {
        handshake = find_handshake(impl, SECURITY_OBJECT_HANDLE(relation->localIdentity), SECURITY_OBJECT_HANDLE(remoteIdentity));
        if (handshake) {
            (void)ddsrt_hh_remove(impl->objectHash, handshake);
            security_object_free((SecurityObject *) handshake);
        }

        (void)ddsrt_hh_remove(remoteIdentity->linkHash, relation);
        security_object_free((SecurityObject *) relation);
    }
}

DDS_Security_boolean
return_identity_handle(
        dds_security_authentication *instance,
        const DDS_Security_IdentityHandle identity_handle,
        DDS_Security_SecurityException *ex)
{
    DDS_Security_boolean result = true;
    dds_security_authentication_impl *impl = (dds_security_authentication_impl *) instance;
    SecurityObject *obj;
    LocalIdentityInfo *localIdent;
    RemoteIdentityInfo *remoteIdent;

    /* validate provided arguments */
    if (!instance || !identity_handle) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "return_identity_handle: Invalid parameter provided");
        goto err_bad_param;
    }

    /* Currently the implementation of the handle does not provide information
     * about the kind of handle. In this case a valid handle could refer to a
     * LocalIdentityObject or a RemoteIdentityObject
     */

    ddsrt_mutex_lock(&impl->lock);

    obj = security_object_find(impl->objectHash, identity_handle);
    if (!obj) {
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "return_identity_handle: Invalid handle provided");
        goto err_invalid_handle;
    }

    switch (obj->kind) {
    case SECURITY_OBJECT_KIND_LOCAL_IDENTITY:
        localIdent = (LocalIdentityInfo *) obj;
        invalidate_local_related_objects(impl, localIdent);
        (void)ddsrt_hh_remove(impl->objectHash, obj);
        security_object_free(obj);
        break;
    case SECURITY_OBJECT_KIND_REMOTE_IDENTITY:
        remoteIdent = (RemoteIdentityInfo *) obj;
        invalidate_remote_related_objects(impl, remoteIdent);
        (void)ddsrt_hh_remove(impl->remoteGuidHash, remoteIdent);
        (void)ddsrt_hh_remove(impl->objectHash, obj);
        security_object_free(obj);
        break;
    default:
        DDS_Security_Exception_set(ex, DDS_AUTH_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "return_identity_handle: Invalid handle provided");
        result = false;
        break;
    }

    ddsrt_mutex_unlock(&impl->lock);

    return result;

err_invalid_handle:
    ddsrt_mutex_unlock(&impl->lock);
err_bad_param:
    return false;
}

DDS_Security_boolean return_sharedsecret_handle(
        dds_security_authentication *instance,
        const DDS_Security_SharedSecretHandle sharedsecret_handle,
        DDS_Security_SecurityException *ex)
{
    DDSRT_UNUSED_ARG(sharedsecret_handle);
    DDSRT_UNUSED_ARG(ex);
    DDSRT_UNUSED_ARG(instance);

    return true;
}

int32_t
init_authentication( const char *argument, void **context)
{

    dds_security_authentication_impl *authentication;

    DDSRT_UNUSED_ARG(argument);

    /* allocate implementation wrapper */
    authentication = (dds_security_authentication_impl*) ddsrt_malloc(
      sizeof(dds_security_authentication_impl));
    memset(authentication, 0, sizeof(dds_security_authentication_impl));

    /* assign dispatcher to be notified when a validity date ends */
    /* Disable it until timed callback is ready */
    /*authentication->timed_callbacks = ut_timed_dispatcher_new(); */

    /* assign the interface functions */
    authentication->base.validate_local_identity = &validate_local_identity;

    authentication->base.get_identity_token = &get_identity_token;

    authentication->base.get_identity_status_token = &get_identity_status_token;

    authentication->base.set_permissions_credential_and_token =
            &set_permissions_credential_and_token;

    authentication->base.validate_remote_identity = &validate_remote_identity;

    authentication->base.begin_handshake_request = &begin_handshake_request;

    authentication->base.begin_handshake_reply = &begin_handshake_reply;

    authentication->base.process_handshake = &process_handshake;

    authentication->base.get_shared_secret = &get_shared_secret;

    authentication->base.get_authenticated_peer_credential_token =
            &get_authenticated_peer_credential_token;

    authentication->base.set_listener = &set_listener;

    authentication->base.return_identity_token = &return_identity_token;

    authentication->base.return_identity_status_token =
            &return_identity_status_token;

    authentication->base.return_authenticated_peer_credential_token =
            &return_authenticated_peer_credential_token;

    authentication->base.return_handshake_handle = &return_handshake_handle;

    authentication->base.return_identity_handle = &return_identity_handle;

    authentication->base.return_sharedsecret_handle = &return_sharedsecret_handle;

    ddsrt_mutex_init(&authentication->lock);

    authentication->objectHash = ddsrt_hh_new(32, security_object_hash, security_object_equal);
    authentication->remoteGuidHash = ddsrt_hh_new(32, remote_guid_hash, remote_guid_equal);

    memset( &authentication->trustedCAList, 0, sizeof(X509Seq));


    /* Initialize openssl */
    OpenSSL_add_all_algorithms();
    OpenSSL_add_all_ciphers();
    OpenSSL_add_all_digests();
    ERR_load_BIO_strings();
    ERR_load_crypto_strings();

    //return the instance
    *context = authentication;
    return 0;

/*    we can not get ddsrt_mutex_init result. So ignore the lines below */
#if MUTEX_INIT_RESULT_IMPLEMENTED
err_mutex_failed:
    ddsrt_free(authentication);
    return -1;
#endif
}

int32_t finalize_authentication(void *instance)
{
    dds_security_authentication_impl *authentication = instance;

    if( authentication ){
        ddsrt_mutex_lock(&authentication->lock);

        /* Will be enabled after timed call back feature implementation */
        /* ut_timed_dispatcher_free(authentication->timed_callbacks); */
        if (authentication->remoteGuidHash) {
            ddsrt_hh_free(authentication->remoteGuidHash);
        }

        if (authentication->objectHash) {
            struct ddsrt_hh_iter it;
            SecurityObject *obj;
            for (obj = ddsrt_hh_iter_first(authentication->objectHash, &it); obj != NULL; obj = ddsrt_hh_iter_next(&it)) {
                security_object_free(obj);
            }
            ddsrt_hh_free(authentication->objectHash);
        }

        free_ca_list_contents(&(authentication->trustedCAList));

        ddsrt_mutex_unlock(&authentication->lock);

        ddsrt_mutex_destroy(&authentication->lock);

        ddsrt_free((dds_security_authentication_impl*) instance);
    }

    RAND_cleanup();
    EVP_cleanup();
    CRYPTO_cleanup_all_ex_data();
    REMOVE_THREAD_STATE();
    ERR_free_strings();

    return 0;
}
