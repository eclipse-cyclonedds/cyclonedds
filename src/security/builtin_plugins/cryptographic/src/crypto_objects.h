// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef CRYPTO_OBJECTS_H
#define CRYPTO_OBJECTS_H

#include <openssl/rand.h>
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/types.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/sync.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_utils.h"
#include "crypto_defs.h"

#ifndef NDEBUG
#define CHECK_CRYPTO_OBJECT_KIND(o, k) assert(crypto_object_valid((CryptoObject *)(o), k))
#else
#define CHECK_CRYPTO_OBJECT_KIND(o, k)
#endif

#define CRYPTO_OBJECT(o) ((CryptoObject *)(o))
#define CRYPTO_OBJECT_HANDLE(o) (CRYPTO_OBJECT(o)->handle)
#define PARTICIPANT_CRYPTO_HANDLE(o) ((DDS_Security_ParticipantCryptoHandle)CRYPTO_OBJECT_HANDLE(o))
#define DATAWRITER_CRYPTO_HANDLE(o) ((DDS_Security_DatawriterCryptoHandle)CRYPTO_OBJECT_HANDLE(o))
#define DATAREADER_CRYPTO_HANDLE(o) ((DDS_Security_DatareaderCryptoHandle)CRYPTO_OBJECT_HANDLE(o))

#define CRYPTO_OBJECT_KEEP(o) crypto_object_keep((CryptoObject *)(o))
#define CRYPTO_OBJECT_RELEASE(o) crypto_object_release((CryptoObject *)(o))
#define CRYPTO_OBJECT_VALID(o, k) crypto_object_valid((CryptoObject *)(o), k)

#define CRYPTO_TRANSFORM_HAS_KEYS(k) ((k) != CRYPTO_TRANSFORMATION_KIND_NONE && (k) != CRYPTO_TRANSFORMATION_KIND_INVALID)


typedef DDS_Security_ParticipantCryptoHandle DDS_Security_LocalParticipantCryptoHandle;
typedef DDS_Security_ParticipantCryptoHandle DDS_Security_RemoteParticipantCryptoHandle;

typedef enum
{
  CRYPTO_OBJECT_KIND_UNKNOWN,
  CRYPTO_OBJECT_KIND_LOCAL_CRYPTO,
  CRYPTO_OBJECT_KIND_REMOTE_CRYPTO,
  CRYPTO_OBJECT_KIND_LOCAL_WRITER_CRYPTO,
  CRYPTO_OBJECT_KIND_REMOTE_WRITER_CRYPTO,
  CRYPTO_OBJECT_KIND_LOCAL_READER_CRYPTO,
  CRYPTO_OBJECT_KIND_REMOTE_READER_CRYPTO,
  CRYPTO_OBJECT_KIND_KEY_MATERIAL,
  CRYPTO_OBJECT_KIND_SESSION_KEY_MATERIAL,
  CRYPTO_OBJECT_KIND_PARTICIPANT_KEY_MATERIAL,
  CRYPTO_OBJECT_KIND_RELATION
} CryptoObjectKind_t;

typedef struct CryptoObject CryptoObject;
typedef void (*CryptoObjectDestructor)(CryptoObject *obj);

struct CryptoObject
{
  int64_t handle;
  ddsrt_atomic_uint32_t refcount;
  CryptoObjectKind_t kind;
  CryptoObjectDestructor destructor;
};

struct local_datawriter_crypto;
struct local_datareader_crypto;
struct remote_datawriter_crypto;
struct remote_datareader_crypto;

typedef struct master_key_material
{
  CryptoObject _parent;
  DDS_Security_CryptoTransformKind_Enum transformation_kind;
  unsigned char *master_salt;
  uint32_t sender_key_id;
  unsigned char *master_sender_key;
  uint32_t receiver_specific_key_id;
  unsigned char *master_receiver_specific_key;
} master_key_material;

typedef struct session_key_material
{
  CryptoObject _parent;
  uint32_t id;
  crypto_session_key_t key;
  uint32_t key_size;
  uint32_t block_size;
  uint64_t block_counter;
  uint64_t max_blocks_per_session;
  uint64_t init_vector_suffix;
  master_key_material *master_key_material;
} session_key_material;

typedef struct remote_session_info
{
  uint32_t key_size;
  uint32_t id;
  crypto_session_key_t key;
} remote_session_info;

typedef struct key_relation
{
  CryptoObject _parent;
  ddsrt_avl_node_t avlnode;
  DDS_Security_SecureSubmessageCategory_t kind;
  uint32_t key_id;
  CryptoObject *local_crypto;
  CryptoObject *remote_crypto;
  master_key_material *key_material;
} key_relation;

typedef struct local_participant_crypto
{
  CryptoObject _parent;
  ddsrt_mutex_t lock;
  master_key_material *key_material;
  DDS_Security_IdentityHandle identity_handle;
  ddsrt_avl_ctree_t key_material_table;
  session_key_material *session;
  DDS_Security_ProtectionKind rtps_protection_kind;
  struct local_datareader_crypto *builtin_reader;
} local_participant_crypto;

typedef struct participant_key_material
{
  CryptoObject _parent;
  ddsrt_avl_node_t loc_avlnode;
  ddsrt_avl_node_t rmt_avlnode;
  DDS_Security_ParticipantCryptoHandle loc_pp_handle;
  DDS_Security_ParticipantCryptoHandle rmt_pp_handle;
  master_key_material *remote_key_material;
  master_key_material *local_P2P_key_material;
  master_key_material *P2P_kx_key_material;
  session_key_material *P2P_writer_session;
  session_key_material *P2P_reader_session;
} participant_key_material;

typedef struct remote_participant_crypto
{
  CryptoObject _parent;
  ddsrt_mutex_t lock;
  DDS_Security_GUID_t remoteGuid;
  DDS_Security_IdentityHandle identity_handle;
  ddsrt_avl_ctree_t key_material_table;
  session_key_material *session;
  DDS_Security_ProtectionKind rtps_protection_kind;
  ddsrt_avl_tree_t relation_index;
  ddsrt_avl_tree_t specific_key_index;
} remote_participant_crypto;

typedef struct local_datawriter_crypto
{
  CryptoObject _parent;
  local_participant_crypto *participant;
  master_key_material *writer_key_material_message;
  master_key_material *writer_key_material_payload;
  session_key_material *writer_session_message;
  session_key_material *writer_session_payload;
  DDS_Security_ProtectionKind metadata_protectionKind;
  DDS_Security_BasicProtectionKind data_protectionKind;
  bool is_builtin_participant_volatile_message_secure_writer;
} local_datawriter_crypto;

typedef struct remote_datawriter_crypto
{
  CryptoObject _parent;
  remote_participant_crypto *participant;
  DDS_Security_ProtectionKind metadata_protectionKind;
  DDS_Security_BasicProtectionKind data_protectionKind;
  master_key_material *reader2writer_key_material;
  master_key_material *writer2reader_key_material[2];
  session_key_material *reader_session; /* reference to the session key used by the reader */
  struct local_datareader_crypto *local_reader;
  bool is_builtin_participant_volatile_message_secure_writer;
} remote_datawriter_crypto;

typedef struct local_datareader_crypto
{
  CryptoObject _parent;
  local_participant_crypto *participant;
  master_key_material *reader_key_material;
  session_key_material *reader_session;
  DDS_Security_ProtectionKind metadata_protectionKind;
  DDS_Security_BasicProtectionKind data_protectionKind;
  bool is_builtin_participant_volatile_message_secure_reader;
} local_datareader_crypto;

typedef struct remote_datareader_crypto
{
  CryptoObject _parent;
  remote_participant_crypto *participant;
  DDS_Security_ProtectionKind metadata_protectionKind;
  DDS_Security_BasicProtectionKind data_protectionKind;
  master_key_material *reader2writer_key_material;
  master_key_material *writer2reader_key_material_message;
  master_key_material *writer2reader_key_material_payload;
  session_key_material *writer_session; /* reference to the session key used by the writer */
  local_datawriter_crypto *local_writer;
  bool is_builtin_participant_volatile_message_secure_reader;
} remote_datareader_crypto;

master_key_material *
crypto_master_key_material_new(DDS_Security_CryptoTransformKind_Enum transform_kind);

void crypto_master_key_material_set(
    master_key_material *dst,
    const master_key_material *src);

session_key_material *
crypto_session_key_material_new(
    master_key_material *master_key);

bool crypto_session_key_material_update(
    session_key_material *session,
    uint32_t size,
    DDS_Security_SecurityException *ex);

local_participant_crypto *
crypto_local_participant_crypto__new(
    DDS_Security_IdentityHandle participant_identity);

remote_participant_crypto *
crypto_remote_participant_crypto__new(
    DDS_Security_IdentityHandle participant_identity);

void crypto_object_init(
    CryptoObject *obj,
    CryptoObjectKind_t kind,
    CryptoObjectDestructor destructor);

key_relation *
crypto_key_relation_new(
    DDS_Security_SecureSubmessageCategory_t kind,
    uint32_t key_id,
    CryptoObject *local_crypto,
    CryptoObject *remote_crypto,
    master_key_material *keymat);

void
crypto_insert_endpoint_relation(
    remote_participant_crypto *rpc,
    key_relation *relation);

void
crypto_remove_endpoint_relation(
    remote_participant_crypto *rpc,
    CryptoObject *lch,
    uint32_t key_id);

key_relation *
crypto_find_endpoint_relation(
    remote_participant_crypto *rpc,
    CryptoObject *lch,
    uint32_t key_id);

void crypto_insert_specific_key_relation_locked(
    remote_participant_crypto *rpc,
    key_relation *relation);

void
crypto_insert_specific_key_relation(
    remote_participant_crypto *rpc,
    key_relation *relation);

void crypto_remove_specific_key_relation_locked(
    remote_participant_crypto *rpc,
    uint32_t key_id);

void
crypto_remove_specific_key_relation(
    remote_participant_crypto *rpc,
    uint32_t key_id);

key_relation * crypto_find_specific_key_relation_locked(
    remote_participant_crypto *rpc,
    uint32_t key_id);

key_relation *
crypto_find_specific_key_relation(
    remote_participant_crypto *rpc,
    uint32_t key_id);

local_datawriter_crypto *
crypto_local_datawriter_crypto__new(
    const local_participant_crypto *participant,
    DDS_Security_ProtectionKind meta_protection,
    DDS_Security_BasicProtectionKind data_protection);

remote_datareader_crypto *
crypto_remote_datareader_crypto__new(
    const remote_participant_crypto *participant,
    DDS_Security_ProtectionKind metadata_protectionKind,
    DDS_Security_BasicProtectionKind data_protectionKind,
    local_datawriter_crypto *local_writer);

local_datareader_crypto *
crypto_local_datareader_crypto__new(
    const local_participant_crypto *participant,
    DDS_Security_ProtectionKind meta_protection,
    DDS_Security_BasicProtectionKind data_protection);

remote_datawriter_crypto *
crypto_remote_datawriter_crypto__new(
    const remote_participant_crypto *participant,
    DDS_Security_ProtectionKind meta_protection,
    DDS_Security_BasicProtectionKind data_protection,
    local_datareader_crypto *local_reader);

void *
crypto_object_keep(
    CryptoObject *obj);

void crypto_object_release(
    CryptoObject *obj);

bool crypto_object_valid(
    CryptoObject *obj,
    CryptoObjectKind_t kind);

void crypto_object_free(
    CryptoObject *obj);

local_participant_crypto *
crypto_local_participant_crypto__new(
    DDS_Security_IdentityHandle participant_identity);

remote_participant_crypto *
crypto_remote_participant_crypto__new(
    DDS_Security_IdentityHandle participant_identity);

participant_key_material *
crypto_participant_key_material_new(
    const local_participant_crypto *loc_pp_crypto,
    const remote_participant_crypto *rmt_pp_crypto);

struct CryptoObjectTable;

typedef uint32_t (*CryptoObjectHashFunction)(const void *obj);
typedef int (*CryptoObjectEqualFunction)(const void *ha, const void *hb);
typedef CryptoObject *(*CryptoObjectFindFunction)(const struct CryptoObjectTable *table, const void *arg);

struct CryptoObjectTable *
crypto_object_table_new(
    CryptoObjectHashFunction hashfnc,
    CryptoObjectEqualFunction equalfnc,
    CryptoObjectFindFunction findfnc);

void crypto_object_table_free(
    struct CryptoObjectTable *table);

CryptoObject *
crypto_object_table_insert(
    struct CryptoObjectTable *table,
    CryptoObject *object);

void crypto_object_table_remove_object(
    struct CryptoObjectTable *table,
    CryptoObject *object);

CryptoObject *
crypto_object_table_remove(
    struct CryptoObjectTable *table,
    int64_t handle);

CryptoObject *
crypto_object_table_find_by_template(
    const struct CryptoObjectTable *table,
    const void *template);

CryptoObject *
crypto_object_table_find(
    struct CryptoObjectTable *table,
    int64_t handle);

typedef int (*CryptoObjectTableCallback)(CryptoObject *obj, void *arg);

struct CryptoObjectTable
{
  struct ddsrt_hh *htab;
  ddsrt_mutex_t lock;
  CryptoObjectFindFunction findfnc;
};

void crypto_object_table_walk(
    struct CryptoObjectTable *table,
    CryptoObjectTableCallback callback,
    void *arg);

void
crypto_local_participant_add_keymat(
    local_participant_crypto *loc_pp_crypte,
    participant_key_material *keymat);

participant_key_material *
crypto_local_participant_remove_keymat(
    local_participant_crypto *loc_pp_crypte,
    DDS_Security_ParticipantCryptoHandle rmt_pp_handle);

participant_key_material *
crypto_local_participant_lookup_keymat(
    local_participant_crypto *loc_pp_crypte,
    DDS_Security_ParticipantCryptoHandle rmt_pp_handle);

void
crypto_remote_participant_add_keymat(
    remote_participant_crypto *rmt_pp_crypte,
    participant_key_material *keymat);

participant_key_material *
crypto_remote_participant_remove_keymat_locked(
    remote_participant_crypto *rmt_pp_crypte,
    DDS_Security_ParticipantCryptoHandle loc_pp_handle);

participant_key_material *
crypto_remote_participant_lookup_keymat_locked(
    remote_participant_crypto *rmt_pp_crypto,
    DDS_Security_ParticipantCryptoHandle loc_pp_handle);

participant_key_material *
crypto_remote_participant_lookup_keymat(
    remote_participant_crypto *rmt_pp_crypte,
    DDS_Security_ParticipantCryptoHandle loc_pp_handle);

size_t
crypto_local_participnant_get_matching(
    local_participant_crypto *loc_pp_crypto,
    DDS_Security_ParticipantCryptoHandle **handles);

size_t
crypto_remote_participnant_get_matching(
    remote_participant_crypto *rmt_pp_crypto,
    DDS_Security_ParticipantCryptoHandle **handles);

#endif /* CRYPTO_OBJECTS_H */
