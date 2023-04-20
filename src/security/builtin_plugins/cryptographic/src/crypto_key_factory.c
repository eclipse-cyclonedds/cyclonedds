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
#include <string.h>

#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/types.h"
#include "dds/ddsi/ddsi_gc.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/core/dds_security_serialize.h"
#include "dds/security/core/dds_security_shared_secret.h"
#include "dds/security/openssl_support.h"
#include "crypto_defs.h"
#include "crypto_utils.h"
#include "crypto_cipher.h"
#include "crypto_key_factory.h"
#include "crypto_objects.h"

/**
 * Implementation structure for storing encapsulated members of the instance
 * while giving only the interface definition to user
 */

#define KXKEYCOOKIE "key exchange key"
#define KXKEYCOOKIE_SIZE (sizeof(KXKEYCOOKIE) - 1)
#define KXSALTCOOKIE "keyexchange salt"
#define KXSALTCOOKIE_SIZE (sizeof(KXSALTCOOKIE) - 1)

typedef struct dds_security_crypto_key_factory_impl
{
  dds_security_crypto_key_factory base;
  const dds_security_cryptography *crypto;
  ddsrt_mutex_t lock;
  struct CryptoObjectTable *crypto_objects; /* table for storing CryptoHandle - ParticipantKeyMaterial pairs */
  ddsrt_atomic_uint32_t next_key_id;
} dds_security_crypto_key_factory_impl;

static void
crypto_token_copy(
    master_key_material *dst,
    const DDS_Security_KeyMaterial_AES_GCM_GMAC *src)
{
  DDS_Security_CryptoTransformKind_Enum src_transform_kind = CRYPTO_TRANSFORM_KIND(src->transformation_kind);

  if (CRYPTO_TRANSFORM_HAS_KEYS(dst->transformation_kind))
  {
    ddsrt_free(dst->master_salt);
    ddsrt_free(dst->master_sender_key);
    ddsrt_free(dst->master_receiver_specific_key);
  }
  if (CRYPTO_TRANSFORM_HAS_KEYS(src_transform_kind))
  {
    uint32_t key_bytes = CRYPTO_KEY_SIZE_BYTES(src_transform_kind);
    dst->master_salt = ddsrt_calloc(1, key_bytes);
    dst->master_sender_key = ddsrt_calloc(1, key_bytes);
    dst->master_receiver_specific_key = ddsrt_calloc(1, key_bytes);
    memcpy(dst->master_salt, src->master_salt._buffer, key_bytes);
    dst->sender_key_id = CRYPTO_TRANSFORM_ID(src->sender_key_id);
    memcpy(dst->master_sender_key, src->master_sender_key._buffer, key_bytes);
    dst->receiver_specific_key_id = CRYPTO_TRANSFORM_ID(src->receiver_specific_key_id);
    if (dst->receiver_specific_key_id)
      memcpy(dst->master_receiver_specific_key, src->master_receiver_specific_key._buffer, key_bytes);
  }
  dst->transformation_kind = src_transform_kind;
};

/* Compute KeyMaterial_AES_GCM_GMAC as described in DDS Security spec v1.1 section 9.5.2.1.2 (table 67 and table 68) */
static bool
calculate_kx_keys(
    const DDS_Security_SharedSecretHandle shared_secret,
    master_key_material *kx_key_material,
    DDS_Security_SecurityException *ex)
{
  bool result = false;
  const DDS_Security_octet *challenge1, *challenge2, *shared_secret_key;
  unsigned char *kx_master_salt, *kx_master_sender_key;
  size_t shared_secret_size = DDS_Security_get_secret_size_from_secret_handle(shared_secret);
  unsigned char hash[SHA256_DIGEST_LENGTH];
  size_t concatenated_bytes1_size = DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE * 2 + KXSALTCOOKIE_SIZE;
  size_t concatenated_bytes2_size = DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE * 2 + KXKEYCOOKIE_SIZE;
  DDS_Security_octet *concatenated_bytes1, *concatenated_bytes2;

  memset(ex, 0, sizeof(*ex));

  if (shared_secret_size > UINT32_MAX)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, 0, "shared_secret_size > UINT32_MAX");
    goto fail_kx;
  }
  concatenated_bytes1 = ddsrt_malloc(concatenated_bytes1_size);
  concatenated_bytes2 = ddsrt_malloc(concatenated_bytes2_size);
  challenge1 = DDS_Security_get_challenge1_from_secret_handle(shared_secret);
  challenge2 = DDS_Security_get_challenge2_from_secret_handle(shared_secret);
  shared_secret_key = DDS_Security_get_secret_from_secret_handle(shared_secret);

  /* master_salt */
  memcpy(concatenated_bytes1, challenge1, DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE);
  memcpy(concatenated_bytes1 + DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE, KXSALTCOOKIE, KXSALTCOOKIE_SIZE);
  memcpy(concatenated_bytes1 + DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE + KXSALTCOOKIE_SIZE, challenge2, DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE);

  SHA256(concatenated_bytes1, concatenated_bytes1_size, hash);
  if (!(kx_master_salt = crypto_hmac256(hash, SHA256_DIGEST_LENGTH, shared_secret_key, (uint32_t) shared_secret_size, ex)))
    goto fail_kx_salt;

  /* master_sender_key */
  memcpy(concatenated_bytes2, challenge2, DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE);
  memcpy(concatenated_bytes2 + DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE, KXKEYCOOKIE, KXKEYCOOKIE_SIZE);
  memcpy(concatenated_bytes2 + DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE + KXKEYCOOKIE_SIZE, challenge1, DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE);

  SHA256(concatenated_bytes2, concatenated_bytes2_size, hash);
  if (!(kx_master_sender_key = crypto_hmac256(hash, SHA256_DIGEST_LENGTH, shared_secret_key, (uint32_t) shared_secret_size, ex)))
    goto fail_kx_key;

  assert (kx_key_material->transformation_kind == CRYPTO_TRANSFORMATION_KIND_AES256_GCM); /* set in crypto_participant_key_material_new */
  memcpy(kx_key_material->master_salt, kx_master_salt, CRYPTO_KEY_SIZE_256);
  kx_key_material->sender_key_id = 0;
  memcpy(kx_key_material->master_sender_key, kx_master_sender_key, CRYPTO_KEY_SIZE_256);
  kx_key_material->receiver_specific_key_id = 0;

  memset (kx_master_sender_key, 0, CRYPTO_KEY_SIZE_256);
  ddsrt_free(kx_master_sender_key);
  result = true;

fail_kx_key:
  memset (kx_master_salt, 0, CRYPTO_KEY_SIZE_256);
  ddsrt_free(kx_master_salt);
fail_kx_salt:
  ddsrt_free(concatenated_bytes2);
  ddsrt_free(concatenated_bytes1);
fail_kx:
  return result;
}

static uint32_t
generate_key(
    dds_security_crypto_key_factory_impl *implementation,
    master_key_material *key_material,
    DDS_Security_SecurityException *ex)
{
  assert (key_material->transformation_kind != CRYPTO_TRANSFORMATION_KIND_NONE);
  if (RAND_bytes(key_material->master_salt, (int)CRYPTO_KEY_SIZE_BYTES(key_material->transformation_kind)) < 0)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CANNOT_GENERATE_RANDOM_CODE, 0,
        DDS_SECURITY_ERR_CANNOT_GENERATE_RANDOM_MESSAGE);
    return DDS_SECURITY_ERR_CANNOT_GENERATE_RANDOM_CODE;
  }
  if (RAND_bytes(key_material->master_sender_key, (int)CRYPTO_KEY_SIZE_BYTES(key_material->transformation_kind)) < 0)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CANNOT_GENERATE_RANDOM_CODE, 0,
        DDS_SECURITY_ERR_CANNOT_GENERATE_RANDOM_MESSAGE);
    return DDS_SECURITY_ERR_CANNOT_GENERATE_RANDOM_CODE;
  }
  key_material->sender_key_id = ddsrt_atomic_inc32_ov(&implementation->next_key_id);
  return DDS_SECURITY_ERR_OK_CODE;
}

static DDS_Security_ProtectionKind
attribute_to_rtps_protection_kind(
    const DDS_Security_ParticipantSecurityAttributes *attributes)
{
  DDS_Security_ProtectionKind kind = DDS_SECURITY_PROTECTION_KIND_NONE;
  assert(attributes);
  if (attributes->is_rtps_protected)
  {
    if (attributes->plugin_participant_attributes & DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_ENCRYPTED)
    {
      if (attributes->plugin_participant_attributes & DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_AUTHENTICATED)
        kind = DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION;
      else
        kind = DDS_SECURITY_PROTECTION_KIND_ENCRYPT;
    }
    else
    {
      if (attributes->plugin_participant_attributes & DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_AUTHENTICATED)
        kind = DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION;
      else
        kind = DDS_SECURITY_PROTECTION_KIND_SIGN;
    }
  }

  return kind;
}

static DDS_Security_ProtectionKind
attribute_to_meta_protection_kind(
    const DDS_Security_EndpointSecurityAttributes *attributes)
{
  DDS_Security_ProtectionKind kind = DDS_SECURITY_PROTECTION_KIND_NONE;
  assert(attributes);
  if (attributes->is_submessage_protected)
  {
    if (attributes->plugin_endpoint_attributes & DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED)
    {
      if (attributes->plugin_endpoint_attributes & DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ORIGIN_AUTHENTICATED)
        kind = DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION;
      else
        kind = DDS_SECURITY_PROTECTION_KIND_ENCRYPT;
    }
    else
    {
      if (attributes->plugin_endpoint_attributes & DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ORIGIN_AUTHENTICATED)
        kind = DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION;
      else
        kind = DDS_SECURITY_PROTECTION_KIND_SIGN;
    }
  }

  return kind;
}

static DDS_Security_BasicProtectionKind
attribute_to_data_protection_kind(
    const DDS_Security_EndpointSecurityAttributes *attributes)
{
  DDS_Security_BasicProtectionKind kind = DDS_SECURITY_BASICPROTECTION_KIND_NONE;
  assert(attributes);
  if (attributes->is_payload_protected)
  {
    if (attributes->plugin_endpoint_attributes & DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_PAYLOAD_ENCRYPTED)
      kind = DDS_SECURITY_BASICPROTECTION_KIND_ENCRYPT;
    else
      kind = DDS_SECURITY_BASICPROTECTION_KIND_SIGN;
  }
  return kind;
}

static void
remove_remote_writer_relation(
    dds_security_crypto_key_factory_impl *implementation,
    remote_datawriter_crypto *rmt_wr)
{
  remote_participant_crypto *rmt_pp;

  DDSRT_UNUSED_ARG(implementation);

  assert(rmt_wr);
  rmt_pp = rmt_wr->participant;
  assert(rmt_pp);

  if (rmt_wr->writer2reader_key_material[0])
    crypto_remove_endpoint_relation(rmt_pp, (CryptoObject *)rmt_wr->local_reader, rmt_wr->writer2reader_key_material[0]->sender_key_id);
}

static void
remove_remote_reader_relation(
    dds_security_crypto_key_factory_impl *implementation,
    remote_datareader_crypto *rmt_rd)
{
  remote_participant_crypto *rmt_pp;

  DDSRT_UNUSED_ARG(implementation);

  assert(rmt_rd);
  rmt_pp = rmt_rd->participant;
  assert(rmt_pp);

  if (rmt_rd->reader2writer_key_material)
    crypto_remove_endpoint_relation(rmt_pp, (CryptoObject *)rmt_rd->local_writer, rmt_rd->reader2writer_key_material->sender_key_id);
}

/**
 * Function implementations
 */

static DDS_Security_ParticipantCryptoHandle
register_local_participant(
    dds_security_crypto_key_factory *instance,
    const DDS_Security_IdentityHandle participant_identity,
    const DDS_Security_PermissionsHandle participant_permissions,
    const DDS_Security_PropertySeq *participant_properties,
    const DDS_Security_ParticipantSecurityAttributes *participant_security_attributes,
    DDS_Security_SecurityException *ex)
{
  local_participant_crypto *participant_crypto;
  dds_security_crypto_key_factory_impl *implementation = (dds_security_crypto_key_factory_impl *)instance;

  if ((participant_identity == DDS_SECURITY_HANDLE_NIL) ||
      (participant_permissions == DDS_SECURITY_HANDLE_NIL))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_IDENTITY_EMPTY_CODE, 0,
        DDS_SECURITY_ERR_IDENTITY_EMPTY_MESSAGE);
    goto err_invalid_argument;
  }

  /*init objects */
  participant_crypto = crypto_local_participant_crypto__new(participant_identity);
  participant_crypto->rtps_protection_kind = attribute_to_rtps_protection_kind(participant_security_attributes);
  participant_crypto->key_material = crypto_master_key_material_new(
    DDS_Security_protectionkind2transformationkind(participant_properties, participant_crypto->rtps_protection_kind));

  /* No need to create session material if there is no protection for RTPS */
  if (participant_crypto->key_material->transformation_kind != CRYPTO_TRANSFORMATION_KIND_NONE)
  {
    if (generate_key(implementation, participant_crypto->key_material, ex) != DDS_SECURITY_ERR_OK_CODE)
      goto err_random_generation;
    participant_crypto->session = crypto_session_key_material_new(participant_crypto->key_material);
  }

  crypto_object_table_insert(implementation->crypto_objects, (CryptoObject *)participant_crypto);
  CRYPTO_OBJECT_RELEASE(participant_crypto);

  return PARTICIPANT_CRYPTO_HANDLE(participant_crypto);

  /* error cases*/
err_random_generation:
  CRYPTO_OBJECT_RELEASE(participant_crypto);
err_invalid_argument:
  return DDS_SECURITY_HANDLE_NIL;
}

struct resolve_remote_part_arg
{
  DDS_Security_IdentityHandle ident;
  remote_participant_crypto *pprmte;
};

static int
resolve_remote_participant_by_id(
    CryptoObject *obj,
    void *arg)
{
  struct resolve_remote_part_arg *info = arg;

  remote_participant_crypto *pprmte;
  if (obj->kind == CRYPTO_OBJECT_KIND_REMOTE_CRYPTO)
  {
    pprmte = (remote_participant_crypto *)obj;
    if (pprmte->identity_handle == info->ident)
    {
      info->pprmte = (remote_participant_crypto *)CRYPTO_OBJECT_KEEP(pprmte);
      return 0;
    }
  }

  return 1;
}

static DDS_Security_ParticipantCryptoHandle
register_matched_remote_participant(
    dds_security_crypto_key_factory *instance,
    const DDS_Security_ParticipantCryptoHandle local_participant_crypto_handle,
    const DDS_Security_IdentityHandle remote_participant_identity,
    const DDS_Security_PermissionsHandle remote_participant_permissions,
    const DDS_Security_SharedSecretHandle shared_secret,
    DDS_Security_SecurityException *ex)
{
  /* declarations */
  dds_security_crypto_key_factory_impl *implementation = (dds_security_crypto_key_factory_impl *)instance;
  remote_participant_crypto *rmt_pp_crypto;
  local_participant_crypto *loc_pp_crypto;
  DDS_Security_SecurityException exception;
  participant_key_material *key_material;

  if (local_participant_crypto_handle == DDS_SECURITY_HANDLE_NIL)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_PARTICIPANT_CRYPTO_HANDLE_EMPTY_CODE, 0,
        DDS_SECURITY_ERR_PARTICIPANT_CRYPTO_HANDLE_EMPTY_MESSAGE);
    goto err_invalid_argument;
  }
  else if (remote_participant_identity == DDS_SECURITY_HANDLE_NIL)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_IDENTITY_EMPTY_CODE, 0,
        DDS_SECURITY_ERR_IDENTITY_EMPTY_MESSAGE);
    goto err_invalid_argument;
  }
  else if (remote_participant_permissions == DDS_SECURITY_HANDLE_NIL)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_PERMISSION_HANDLE_EMPTY_CODE, 0,
        DDS_SECURITY_ERR_PERMISSION_HANDLE_EMPTY_MESSAGE);
    goto err_invalid_argument;
  }

  /* Check if local_participant_crypto_handle exists in the map */

  loc_pp_crypto = (local_participant_crypto *)crypto_object_table_find(implementation->crypto_objects, local_participant_crypto_handle);
  if (loc_pp_crypto == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_invalid_argument;
  }

  /* find or create remote participant crypto structure */
  {
    struct resolve_remote_part_arg arg = {remote_participant_identity, NULL};
    crypto_object_table_walk(implementation->crypto_objects, resolve_remote_participant_by_id, &arg);
    if (arg.pprmte)
    {
      rmt_pp_crypto = arg.pprmte;
    }
    else
    {
      rmt_pp_crypto = crypto_remote_participant_crypto__new(remote_participant_identity);
      crypto_object_table_insert(implementation->crypto_objects, (CryptoObject *)rmt_pp_crypto);
    }
  }

  key_material = crypto_remote_participant_lookup_keymat(rmt_pp_crypto, PARTICIPANT_CRYPTO_HANDLE(loc_pp_crypto));
  if (!key_material)
  {
    key_material = crypto_participant_key_material_new(loc_pp_crypto, rmt_pp_crypto);

    /* set remote participant keymaterial with local keymaterial values */
    crypto_master_key_material_set(key_material->local_P2P_key_material, loc_pp_crypto->key_material);

    if (!calculate_kx_keys(shared_secret, key_material->P2P_kx_key_material, &exception))
      goto fail_calc_key;

    key_material->P2P_writer_session = crypto_session_key_material_new(key_material->P2P_kx_key_material);
    key_material->P2P_reader_session = crypto_session_key_material_new(key_material->P2P_kx_key_material);

    /* if we do not have OriginAuthentication, receiver specific info remains empty/NULL */
    if ((loc_pp_crypto->rtps_protection_kind == DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION) ||
        (loc_pp_crypto->rtps_protection_kind == DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION))
    {
      if (RAND_bytes(key_material->local_P2P_key_material->master_receiver_specific_key, (int)CRYPTO_KEY_SIZE_BYTES(key_material->local_P2P_key_material->transformation_kind)) < 0)
      {
        DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CANNOT_GENERATE_RANDOM_CODE, 0,
            DDS_SECURITY_ERR_CANNOT_GENERATE_RANDOM_MESSAGE);
        goto err_random_generation;
      }
      key_material->local_P2P_key_material->receiver_specific_key_id = ddsrt_atomic_inc32_ov(&implementation->next_key_id);
    }
    rmt_pp_crypto->session = (session_key_material *)CRYPTO_OBJECT_KEEP(loc_pp_crypto->session);

    crypto_local_participant_add_keymat(loc_pp_crypto, key_material);
    crypto_remote_participant_add_keymat(rmt_pp_crypto, key_material);
  }

  rmt_pp_crypto->rtps_protection_kind = loc_pp_crypto->rtps_protection_kind; /* Same as local  */

  CRYPTO_OBJECT_RELEASE(key_material);
  CRYPTO_OBJECT_RELEASE(rmt_pp_crypto);
  CRYPTO_OBJECT_RELEASE(loc_pp_crypto);

  return PARTICIPANT_CRYPTO_HANDLE(rmt_pp_crypto);

/* error cases*/
err_random_generation:
fail_calc_key:
  CRYPTO_OBJECT_RELEASE(key_material);
  CRYPTO_OBJECT_RELEASE(rmt_pp_crypto);
  CRYPTO_OBJECT_RELEASE(loc_pp_crypto);
err_invalid_argument:
  return DDS_SECURITY_HANDLE_NIL;
}

static DDS_Security_DatawriterCryptoHandle
register_local_datawriter(
    dds_security_crypto_key_factory *instance,
    const DDS_Security_ParticipantCryptoHandle participant_crypto_handle,
    const DDS_Security_PropertySeq *datawriter_properties,
    const DDS_Security_EndpointSecurityAttributes *datawriter_security_attributes,
    DDS_Security_SecurityException *ex)
{
  local_participant_crypto *participant_crypto;
  local_datawriter_crypto *writer_crypto;
  bool is_builtin = false;
  dds_security_crypto_key_factory_impl *implementation = (dds_security_crypto_key_factory_impl *)instance;
  DDS_Security_ProtectionKind metadata_protection;
  DDS_Security_BasicProtectionKind data_protection;

  if (participant_crypto_handle == DDS_SECURITY_HANDLE_NIL)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_invalid_parameter;
  }

  participant_crypto = (local_participant_crypto *)crypto_object_table_find(implementation->crypto_objects, participant_crypto_handle);
  if (!participant_crypto)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_invalid_parameter;
  }

  if (datawriter_properties != NULL && datawriter_properties->_length > 0)
  {
    const DDS_Security_Property_t *property = DDS_Security_PropertySeq_find_property(
        datawriter_properties, DDS_SEC_PROP_BUILTIN_ENDPOINT_NAME);
    if (property != NULL && strcmp(property->value, "BuiltinParticipantVolatileMessageSecureWriter") == 0)
      is_builtin = true;
  }

  data_protection = attribute_to_data_protection_kind(datawriter_security_attributes);
  metadata_protection = attribute_to_meta_protection_kind(datawriter_security_attributes);

  writer_crypto = crypto_local_datawriter_crypto__new(participant_crypto, metadata_protection, data_protection);
  writer_crypto->is_builtin_participant_volatile_message_secure_writer = is_builtin;

  if (!is_builtin)
  {
    if (writer_crypto->metadata_protectionKind != DDS_SECURITY_PROTECTION_KIND_NONE)
    {
      writer_crypto->writer_key_material_message = crypto_master_key_material_new(
        DDS_Security_protectionkind2transformationkind(datawriter_properties, metadata_protection));
      if (generate_key(implementation, writer_crypto->writer_key_material_message, ex) != DDS_SECURITY_ERR_OK_CODE)
        goto err_random_generation;
      writer_crypto->writer_session_message = crypto_session_key_material_new(writer_crypto->writer_key_material_message);
    }

    if (writer_crypto->data_protectionKind != DDS_SECURITY_BASICPROTECTION_KIND_NONE)
    {
      writer_crypto->writer_key_material_payload = crypto_master_key_material_new(
        DDS_Security_basicprotectionkind2transformationkind(datawriter_properties, data_protection));
      if (generate_key(implementation, writer_crypto->writer_key_material_payload, ex) != DDS_SECURITY_ERR_OK_CODE)
        goto err_random_generation;
      writer_crypto->writer_session_payload = crypto_session_key_material_new(writer_crypto->writer_key_material_payload);
    }
  }

  crypto_object_table_insert(implementation->crypto_objects, (CryptoObject *)writer_crypto);
  CRYPTO_OBJECT_RELEASE(participant_crypto);
  CRYPTO_OBJECT_RELEASE(writer_crypto);

  return writer_crypto->_parent.handle;

err_random_generation:
  CRYPTO_OBJECT_RELEASE(participant_crypto);
  CRYPTO_OBJECT_RELEASE(writer_crypto);
err_invalid_parameter:
  return DDS_SECURITY_HANDLE_NIL;
}

static DDS_Security_DatareaderCryptoHandle
register_matched_remote_datareader(
    dds_security_crypto_key_factory *instance,
    const DDS_Security_DatawriterCryptoHandle local_datawriter_crypto_handle,
    const DDS_Security_ParticipantCryptoHandle remote_participant_crypto_handle,
    const DDS_Security_SharedSecretHandle shared_secret,
    const DDS_Security_boolean relay_only, DDS_Security_SecurityException *ex)
{
  remote_datareader_crypto *reader_crypto;
  dds_security_crypto_key_factory_impl *implementation = (dds_security_crypto_key_factory_impl *)instance;
  local_datawriter_crypto *local_writer;
  remote_participant_crypto *remote_participant;
  DDS_Security_ProtectionKind metadata_protectionKind;
  DDS_Security_BasicProtectionKind data_protectionKind;

  DDSRT_UNUSED_ARG(shared_secret);
  DDSRT_UNUSED_ARG(relay_only);

  if ((remote_participant_crypto_handle == DDS_SECURITY_HANDLE_NIL) ||
      (local_datawriter_crypto_handle == DDS_SECURITY_HANDLE_NIL))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_invalid_parameter;
  }

  remote_participant = (remote_participant_crypto *)crypto_object_table_find(implementation->crypto_objects, remote_participant_crypto_handle);
  if (!remote_participant)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_invalid_parameter;
  }

  local_writer = (local_datawriter_crypto *)crypto_object_table_find(implementation->crypto_objects, local_datawriter_crypto_handle);
  if (!local_writer)
  {
    CRYPTO_OBJECT_RELEASE(remote_participant);
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_invalid_parameter;
  }

  data_protectionKind = local_writer->data_protectionKind;
  metadata_protectionKind = local_writer->metadata_protectionKind;

  reader_crypto = crypto_remote_datareader_crypto__new(remote_participant, metadata_protectionKind, data_protectionKind, local_writer);

  /* check if the writer is BuiltinParticipantVolatileMessageSecureWriter */
  if (local_writer->is_builtin_participant_volatile_message_secure_writer)
  {
    participant_key_material *key_material = crypto_remote_participant_lookup_keymat(remote_participant, CRYPTO_OBJECT_HANDLE(local_writer->participant));
    assert(key_material);

    reader_crypto->reader2writer_key_material = (master_key_material *)CRYPTO_OBJECT_KEEP(key_material->P2P_kx_key_material);
    reader_crypto->writer2reader_key_material_message = (master_key_material *)CRYPTO_OBJECT_KEEP(key_material->P2P_kx_key_material);
    reader_crypto->writer_session = (session_key_material *)CRYPTO_OBJECT_KEEP(key_material->P2P_writer_session);
    reader_crypto->is_builtin_participant_volatile_message_secure_reader = true;
    CRYPTO_OBJECT_RELEASE(key_material);
  }
  else
  {
    if (local_writer->writer_key_material_message)
    {
      reader_crypto->writer2reader_key_material_message = crypto_master_key_material_new(CRYPTO_TRANSFORMATION_KIND_NONE);
      crypto_master_key_material_set(reader_crypto->writer2reader_key_material_message, local_writer->writer_key_material_message);
      if ((metadata_protectionKind == DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION) ||
          (metadata_protectionKind == DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION))
      {
        uint32_t key_bytes = CRYPTO_KEY_SIZE_BYTES(reader_crypto->writer2reader_key_material_message->transformation_kind);
        if (RAND_bytes(reader_crypto->writer2reader_key_material_message->master_receiver_specific_key, (int)key_bytes) < 0)
        {
          DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CANNOT_GENERATE_RANDOM_CODE, 0,
              DDS_SECURITY_ERR_CANNOT_GENERATE_RANDOM_MESSAGE);
          goto err_random_generation;
        }
        reader_crypto->writer2reader_key_material_message->receiver_specific_key_id = ddsrt_atomic_inc32_ov(&implementation->next_key_id);
      }
      reader_crypto->writer_session = (session_key_material *)CRYPTO_OBJECT_KEEP(local_writer->writer_session_message);
    }

    if (local_writer->writer_key_material_payload)
    {
      reader_crypto->writer2reader_key_material_payload = crypto_master_key_material_new(CRYPTO_TRANSFORMATION_KIND_NONE);
      crypto_master_key_material_set(reader_crypto->writer2reader_key_material_payload, local_writer->writer_key_material_payload);
    }
  }

  crypto_object_table_insert(implementation->crypto_objects, (CryptoObject *)reader_crypto);
  CRYPTO_OBJECT_RELEASE(remote_participant);
  CRYPTO_OBJECT_RELEASE(local_writer);
  CRYPTO_OBJECT_RELEASE(reader_crypto);

  return DATAREADER_CRYPTO_HANDLE(reader_crypto);

err_random_generation:
  CRYPTO_OBJECT_RELEASE(reader_crypto);
  CRYPTO_OBJECT_RELEASE(remote_participant);
  CRYPTO_OBJECT_RELEASE(local_writer);
err_invalid_parameter:
  return DDS_SECURITY_HANDLE_NIL;
}

static DDS_Security_DatareaderCryptoHandle
register_local_datareader(
    dds_security_crypto_key_factory *instance,
    const DDS_Security_ParticipantCryptoHandle participant_crypto_handle,
    const DDS_Security_PropertySeq *datareader_properties,
    const DDS_Security_EndpointSecurityAttributes *datareader_security_attributes,
    DDS_Security_SecurityException *ex)
{
  local_participant_crypto *participant_crypto;
  local_datareader_crypto *reader_crypto;
  bool is_builtin = false;
  dds_security_crypto_key_factory_impl *implementation = (dds_security_crypto_key_factory_impl *)instance;
  DDS_Security_ProtectionKind metadata_protection;
  DDS_Security_BasicProtectionKind data_protection;

  if (!instance || (participant_crypto_handle == DDS_SECURITY_HANDLE_NIL))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_invalid_parameter;
  }

  participant_crypto = (local_participant_crypto *)crypto_object_table_find(implementation->crypto_objects, participant_crypto_handle);
  if (!participant_crypto)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_invalid_parameter;
  }

  if (datareader_properties != NULL && datareader_properties->_length > 0)
  {
    const DDS_Security_Property_t *property = DDS_Security_PropertySeq_find_property(
        datareader_properties, DDS_SEC_PROP_BUILTIN_ENDPOINT_NAME);
    if (property != NULL && strcmp(property->value, "BuiltinParticipantVolatileMessageSecureReader") == 0)
      is_builtin = true;
  }

  data_protection = attribute_to_data_protection_kind(datareader_security_attributes);
  metadata_protection = attribute_to_meta_protection_kind(datareader_security_attributes);

  reader_crypto = crypto_local_datareader_crypto__new(participant_crypto, metadata_protection, data_protection);
  reader_crypto->is_builtin_participant_volatile_message_secure_reader = is_builtin;

  if (!is_builtin)
  {
    if (reader_crypto->metadata_protectionKind != DDS_SECURITY_PROTECTION_KIND_NONE)
    {
      reader_crypto->reader_key_material = crypto_master_key_material_new(
        DDS_Security_protectionkind2transformationkind(datareader_properties, metadata_protection));
      if (generate_key(implementation, reader_crypto->reader_key_material, ex) != DDS_SECURITY_ERR_OK_CODE)
        goto err_random_generation;
      reader_crypto->reader_session = crypto_session_key_material_new(reader_crypto->reader_key_material);
    }
  }
  else
  {
    participant_crypto->builtin_reader = reader_crypto;

  }

  crypto_object_table_insert(implementation->crypto_objects, (CryptoObject *)reader_crypto);
  CRYPTO_OBJECT_RELEASE(participant_crypto);
  CRYPTO_OBJECT_RELEASE(reader_crypto);

  return DATAREADER_CRYPTO_HANDLE(reader_crypto);

err_random_generation:
  CRYPTO_OBJECT_RELEASE(participant_crypto);
  CRYPTO_OBJECT_RELEASE(reader_crypto);
err_invalid_parameter:
  return DDS_SECURITY_HANDLE_NIL;
}

static DDS_Security_DatawriterCryptoHandle
register_matched_remote_datawriter(
    dds_security_crypto_key_factory *instance,
    const DDS_Security_DatareaderCryptoHandle local_datareader_crypto_handle,
    const DDS_Security_ParticipantCryptoHandle remote_participant_crypto_handle,
    const DDS_Security_SharedSecretHandle shared_secret,
    DDS_Security_SecurityException *ex)
{
  remote_datawriter_crypto *writer_crypto;
  dds_security_crypto_key_factory_impl *implementation =
      (dds_security_crypto_key_factory_impl *)instance;
  local_datareader_crypto *local_reader;
  remote_participant_crypto *remote_participant;

  DDSRT_UNUSED_ARG(shared_secret);

  if ((remote_participant_crypto_handle == DDS_SECURITY_HANDLE_NIL) ||
      (local_datareader_crypto_handle == DDS_SECURITY_HANDLE_NIL))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_invalid_parameter;
  }

  remote_participant = (remote_participant_crypto *)crypto_object_table_find(implementation->crypto_objects, remote_participant_crypto_handle);
  if (!remote_participant)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_invalid_parameter;
  }

  local_reader = (local_datareader_crypto *)crypto_object_table_find(implementation->crypto_objects, local_datareader_crypto_handle);
  if (!local_reader)
  {
    CRYPTO_OBJECT_RELEASE(remote_participant);
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_invalid_parameter;
  }

  writer_crypto = crypto_remote_datawriter_crypto__new(remote_participant, local_reader->metadata_protectionKind, local_reader->data_protectionKind, local_reader);

  /* check if the writer is BuiltinParticipantVolatileMessageSecureWriter */
  if (local_reader->is_builtin_participant_volatile_message_secure_reader)
  {
    participant_key_material *key_material = crypto_remote_participant_lookup_keymat(remote_participant, CRYPTO_OBJECT_HANDLE(local_reader->participant));
    assert(key_material);

    writer_crypto->reader2writer_key_material = (master_key_material *)CRYPTO_OBJECT_KEEP(key_material->P2P_kx_key_material);
    writer_crypto->writer2reader_key_material[0] = (master_key_material *)CRYPTO_OBJECT_KEEP(key_material->P2P_kx_key_material);
    writer_crypto->writer2reader_key_material[1] = (master_key_material *)CRYPTO_OBJECT_KEEP(key_material->P2P_kx_key_material);
    writer_crypto->reader_session = (session_key_material *)CRYPTO_OBJECT_KEEP(key_material->P2P_reader_session);
    writer_crypto->is_builtin_participant_volatile_message_secure_writer = true;

    key_relation * relation = crypto_key_relation_new(DDS_SECURITY_DATAWRITER_SUBMESSAGE, 0, (CryptoObject *)local_reader, (CryptoObject *)writer_crypto, NULL);

    crypto_insert_endpoint_relation(remote_participant, relation);
    CRYPTO_OBJECT_RELEASE(relation);
    CRYPTO_OBJECT_RELEASE(key_material);
  }
  else if (local_reader->metadata_protectionKind != DDS_SECURITY_PROTECTION_KIND_NONE)
  {
    writer_crypto->reader2writer_key_material = crypto_master_key_material_new(CRYPTO_TRANSFORMATION_KIND_NONE);
    crypto_master_key_material_set(writer_crypto->reader2writer_key_material, local_reader->reader_key_material);

    if (local_reader->metadata_protectionKind == DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION
        || local_reader->metadata_protectionKind == DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION)
    {
      uint32_t key_bytes = CRYPTO_KEY_SIZE_BYTES(writer_crypto->reader2writer_key_material->transformation_kind);
      if (RAND_bytes(writer_crypto->reader2writer_key_material->master_receiver_specific_key, (int)key_bytes) < 0)
      {
        DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT,
                                   DDS_SECURITY_ERR_CANNOT_GENERATE_RANDOM_CODE, 0,
                                   DDS_SECURITY_ERR_CANNOT_GENERATE_RANDOM_MESSAGE);
        goto err_random_generation;
      }
      writer_crypto->reader2writer_key_material->receiver_specific_key_id = ddsrt_atomic_inc32_ov(&implementation->next_key_id);
      writer_crypto->reader_session = (session_key_material *)CRYPTO_OBJECT_KEEP(local_reader->reader_session);
    }
  }
  crypto_object_table_insert(implementation->crypto_objects, (CryptoObject *)writer_crypto);
  CRYPTO_OBJECT_RELEASE(remote_participant);
  CRYPTO_OBJECT_RELEASE(local_reader);
  CRYPTO_OBJECT_RELEASE(writer_crypto);

  return DATAREADER_CRYPTO_HANDLE(writer_crypto);

err_random_generation:
  CRYPTO_OBJECT_RELEASE(writer_crypto);
  CRYPTO_OBJECT_RELEASE(remote_participant);
  CRYPTO_OBJECT_RELEASE(local_reader);
err_invalid_parameter:
  return DDS_SECURITY_HANDLE_NIL;
}

static DDS_Security_boolean
unregister_participant(
    dds_security_crypto_key_factory *instance,
    const DDS_Security_ParticipantCryptoHandle participant_crypto_handle,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_key_factory_impl *implementation = (dds_security_crypto_key_factory_impl *)instance;
  DDS_Security_boolean result = false;
  CryptoObject *obj;
  local_participant_crypto *loc_pp_crypto;
  remote_participant_crypto *rmt_pp_crypto;
  participant_key_material *keymat;
  DDS_Security_ParticipantCryptoHandle *handles = NULL;
  size_t num, i;

  if ((obj = crypto_object_table_find(implementation->crypto_objects, participant_crypto_handle)) != NULL)
  {
    switch (obj->kind)
    {
    case CRYPTO_OBJECT_KIND_LOCAL_CRYPTO:
    {
      loc_pp_crypto = (local_participant_crypto *)obj;

      num = crypto_local_participnant_get_matching(loc_pp_crypto, &handles);
      for (i = 0; i < num; i++)
      {
        if ((keymat = crypto_local_participant_remove_keymat(loc_pp_crypto, handles[i])) != NULL)
          CRYPTO_OBJECT_RELEASE(keymat);

        if ((rmt_pp_crypto = (remote_participant_crypto *)crypto_object_table_find(implementation->crypto_objects, handles[i])) != NULL)
        {
          ddsrt_mutex_lock(&rmt_pp_crypto->lock);
          if ((keymat = crypto_remote_participant_remove_keymat_locked(rmt_pp_crypto, participant_crypto_handle)) != NULL)
          {
            if (keymat->remote_key_material && keymat->remote_key_material->receiver_specific_key_id != 0)
              crypto_remove_specific_key_relation_locked(rmt_pp_crypto, keymat->remote_key_material->receiver_specific_key_id);
            CRYPTO_OBJECT_RELEASE(keymat);
          }
          ddsrt_mutex_unlock(&rmt_pp_crypto->lock);
          CRYPTO_OBJECT_RELEASE(rmt_pp_crypto);
        }
      }
      ddsrt_free(handles);
      crypto_object_table_remove_object(implementation->crypto_objects, obj);
      result = true;
    }
    break;
    case CRYPTO_OBJECT_KIND_REMOTE_CRYPTO:
    {
      rmt_pp_crypto = (remote_participant_crypto *)obj;

      num = crypto_remote_participnant_get_matching(rmt_pp_crypto, &handles);
      for (i = 0; i < num; i++)
      {
        ddsrt_mutex_lock(&rmt_pp_crypto->lock);
        if ((keymat = crypto_remote_participant_remove_keymat_locked(rmt_pp_crypto, handles[i])) != NULL)
        {
          if (keymat->remote_key_material && keymat->remote_key_material->receiver_specific_key_id != 0)
            crypto_remove_specific_key_relation_locked(rmt_pp_crypto, keymat->remote_key_material->receiver_specific_key_id);
          CRYPTO_OBJECT_RELEASE(keymat);
        }
        ddsrt_mutex_unlock(&rmt_pp_crypto->lock);

        if ((loc_pp_crypto = (local_participant_crypto *)crypto_object_table_find(implementation->crypto_objects, handles[i])) != NULL)
        {
          if ((keymat = crypto_local_participant_remove_keymat(loc_pp_crypto, participant_crypto_handle)) != NULL)
            CRYPTO_OBJECT_RELEASE(keymat);
          CRYPTO_OBJECT_RELEASE(loc_pp_crypto);
        }
      }
      ddsrt_free(handles);
      crypto_object_table_remove_object(implementation->crypto_objects, obj);
      result = true;
    }
    break;
    default:
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    }
    break;
    }
    CRYPTO_OBJECT_RELEASE(obj);
  }
  else
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);

  return result;
}

static DDS_Security_boolean
unregister_datawriter(
    dds_security_crypto_key_factory *instance,
    const DDS_Security_DatawriterCryptoHandle datawriter_crypto_handle,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_key_factory_impl *implementation = (dds_security_crypto_key_factory_impl *)instance;
  DDS_Security_boolean result = false;
  CryptoObject *obj;

  /* check if the handle is applicable*/
  if ((obj = crypto_object_table_find(implementation->crypto_objects, datawriter_crypto_handle)) != NULL)
  {
    switch (obj->kind)
    {
    case CRYPTO_OBJECT_KIND_LOCAL_WRITER_CRYPTO:
      crypto_object_table_remove_object(implementation->crypto_objects, obj);
      result = true;
      break;
    case CRYPTO_OBJECT_KIND_REMOTE_WRITER_CRYPTO:
    {
      remote_datawriter_crypto *rmt_wr = (remote_datawriter_crypto *)obj;
      remove_remote_writer_relation(implementation, rmt_wr);
      if (rmt_wr->writer2reader_key_material[0] && rmt_wr->writer2reader_key_material[0]->receiver_specific_key_id != 0)
        crypto_remove_specific_key_relation(rmt_wr->participant, rmt_wr->writer2reader_key_material[0]->receiver_specific_key_id);
      crypto_object_table_remove_object(implementation->crypto_objects, obj);
      result = true;
    }
    break;
    default:
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
      break;
    }
    CRYPTO_OBJECT_RELEASE(obj);
  }
  else
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);

  return result;
}

static DDS_Security_boolean
unregister_datareader(
    dds_security_crypto_key_factory *instance,
    const DDS_Security_DatareaderCryptoHandle datareader_crypto_handle,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_key_factory_impl *implementation = (dds_security_crypto_key_factory_impl *)instance;
  DDS_Security_boolean result = false;
  CryptoObject *obj;

  /* check if the handle is applicable*/
  if ((obj = crypto_object_table_find(implementation->crypto_objects, datareader_crypto_handle)) != NULL)
  {
    switch (obj->kind)
    {
    case CRYPTO_OBJECT_KIND_LOCAL_READER_CRYPTO:
      crypto_object_table_remove_object(implementation->crypto_objects, obj);
      result = true;
      break;
    case CRYPTO_OBJECT_KIND_REMOTE_READER_CRYPTO:
    {
      remote_datareader_crypto *rmt_rd = (remote_datareader_crypto *)obj;
      remove_remote_reader_relation(implementation, rmt_rd);
      if (rmt_rd->reader2writer_key_material && rmt_rd->reader2writer_key_material->receiver_specific_key_id != 0)
        crypto_remove_specific_key_relation(rmt_rd->participant, rmt_rd->reader2writer_key_material->receiver_specific_key_id);
      crypto_object_table_remove_object(implementation->crypto_objects, obj);
      result = true;
    }
    break;
    default:
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
      break;
    }
    CRYPTO_OBJECT_RELEASE(obj);
  }
  else
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);

  return result;
}

dds_security_crypto_key_factory *
dds_security_crypto_key_factory__alloc(
    const dds_security_cryptography *crypto)
{
  dds_security_crypto_key_factory_impl *instance = ddsrt_malloc(sizeof(*instance));

  ddsrt_mutex_init(&instance->lock);

  instance->crypto = crypto;
  instance->base.register_local_participant = &register_local_participant;
  instance->base.register_matched_remote_participant = &register_matched_remote_participant;
  instance->base.register_local_datawriter = &register_local_datawriter;
  instance->base.register_matched_remote_datareader = &register_matched_remote_datareader;
  instance->base.register_local_datareader = &register_local_datareader;
  instance->base.register_matched_remote_datawriter = &register_matched_remote_datawriter;
  instance->base.unregister_participant = &unregister_participant;
  instance->base.unregister_datawriter = &unregister_datawriter;
  instance->base.unregister_datareader = &unregister_datareader;

  /* init implementation specific members */
  instance->crypto_objects = crypto_object_table_new(NULL, NULL, NULL);

  ddsrt_atomic_st32 (&instance->next_key_id, 1);

  return (dds_security_crypto_key_factory *)instance;
}

void dds_security_crypto_key_factory__dealloc(dds_security_crypto_key_factory *instance)
{
  dds_security_crypto_key_factory_impl *implementation = (dds_security_crypto_key_factory_impl *)instance;
  ddsrt_mutex_destroy (&implementation->lock);
  crypto_object_table_free(implementation->crypto_objects);
  ddsrt_free(implementation);
}


bool
crypto_factory_get_protection_kind(
    const dds_security_crypto_key_factory *factory,
    int64_t handle,
    DDS_Security_ProtectionKind *kind)
{
  const dds_security_crypto_key_factory_impl *impl = (const dds_security_crypto_key_factory_impl *)factory;
  CryptoObject *obj;
  bool result = true;

  obj = crypto_object_table_find(impl->crypto_objects, handle);
  if (!obj)
  {
    return false;
  }

  switch (obj->kind)
  {
  case CRYPTO_OBJECT_KIND_LOCAL_CRYPTO:
    *kind = ((local_participant_crypto *)obj)->rtps_protection_kind;
    break;
  case CRYPTO_OBJECT_KIND_REMOTE_CRYPTO:
    *kind = ((remote_participant_crypto *)obj)->rtps_protection_kind;
    break;
  case CRYPTO_OBJECT_KIND_LOCAL_WRITER_CRYPTO:
    *kind = ((local_datawriter_crypto *)obj)->metadata_protectionKind;
    break;
  case CRYPTO_OBJECT_KIND_REMOTE_WRITER_CRYPTO:
    *kind = ((remote_datawriter_crypto *)obj)->metadata_protectionKind;
    break;
  case CRYPTO_OBJECT_KIND_LOCAL_READER_CRYPTO:
    *kind = ((local_datareader_crypto *)obj)->metadata_protectionKind;
    break;
  case CRYPTO_OBJECT_KIND_REMOTE_READER_CRYPTO:
    *kind = ((remote_datareader_crypto *)obj)->metadata_protectionKind;
    break;
  default:
    result = false;
    break;
  }
  CRYPTO_OBJECT_RELEASE(obj);
  return result;
}

bool
crypto_factory_get_participant_crypto_tokens(
    const dds_security_crypto_key_factory *factory,
    DDS_Security_ParticipantCryptoHandle local_id,
    DDS_Security_ParticipantCryptoHandle remote_id,
    participant_key_material **pp_key_material,
    master_key_material **remote_key_matarial,
    DDS_Security_ProtectionKind *protection_kind,
    DDS_Security_SecurityException *ex)
{
  assert (pp_key_material != NULL);
  dds_security_crypto_key_factory_impl *impl = (dds_security_crypto_key_factory_impl *)factory;
  remote_participant_crypto *remote_crypto = (remote_participant_crypto *)crypto_object_table_find(impl->crypto_objects, remote_id);
  bool result = false;

  if (!remote_crypto)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_no_remote;
  }
  else if (!CRYPTO_OBJECT_VALID(remote_crypto, CRYPTO_OBJECT_KIND_REMOTE_CRYPTO))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_remote;
  }

  ddsrt_mutex_lock(&remote_crypto->lock);
  if (!(*pp_key_material = (participant_key_material *)crypto_remote_participant_lookup_keymat_locked(remote_crypto, local_id)))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    ddsrt_mutex_unlock(&remote_crypto->lock);
    goto err_remote;
  }
  if (remote_key_matarial != NULL)
    *remote_key_matarial = (*pp_key_material)->remote_key_material;
  if (protection_kind != NULL)
    *protection_kind = remote_crypto->rtps_protection_kind;
  ddsrt_mutex_unlock(&remote_crypto->lock);
  result = true;

err_remote:
  CRYPTO_OBJECT_RELEASE(remote_crypto);
err_no_remote:
  return result;
}

static void gc_remote_key_material (struct ddsi_gcreq *gcreq)
{
  CRYPTO_OBJECT_RELEASE (ddsi_gcreq_get_arg (gcreq));
  ddsi_gcreq_free (gcreq);
}

bool
crypto_factory_set_participant_crypto_tokens(
    const dds_security_crypto_key_factory *factory,
    const DDS_Security_ParticipantCryptoHandle local_id,
    const DDS_Security_ParticipantCryptoHandle remote_id,
    const DDS_Security_KeyMaterial_AES_GCM_GMAC *remote_key_mat,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_key_factory_impl *impl = (dds_security_crypto_key_factory_impl *)factory;
  remote_participant_crypto *remote_crypto;
  participant_key_material *key_material;
  bool result = false;

  remote_crypto = (remote_participant_crypto *)crypto_object_table_find(impl->crypto_objects, remote_id);
  if (!remote_crypto)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_no_remote;
  }
  else if (!CRYPTO_OBJECT_VALID(remote_crypto, CRYPTO_OBJECT_KIND_REMOTE_CRYPTO))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_remote;
  }

  ddsrt_mutex_lock(&remote_crypto->lock);
  key_material = crypto_remote_participant_lookup_keymat_locked(remote_crypto, local_id);
  if (key_material)
  {
    /* Because setting crypto tokens is not done very often, we're not using an
       atomic pointer (which makes the code less readable) and do not introduce an
       additional lock for the remote key material. Instead it is protected by the
       remote_participant_crypto lock. For cleaning up the old remote key
       material, the garbage collector is used so that any pointer to the remote
       key material can be safely used until thread state sleep. */
    master_key_material *remote_key_mat_old = key_material->remote_key_material;
    master_key_material *remote_key_mat_new = crypto_master_key_material_new(CRYPTO_TRANSFORMATION_KIND_NONE);
    crypto_token_copy(remote_key_mat_new, remote_key_mat);
    key_material->remote_key_material = remote_key_mat_new;

    if (remote_key_mat_old != NULL)
    {
      struct ddsi_gcreq *gcreq = ddsi_gcreq_new (impl->crypto->gv->gcreq_queue, gc_remote_key_material);
      ddsi_gcreq_set_arg (gcreq, remote_key_mat_old);
      ddsi_gcreq_enqueue (gcreq);
    }

    uint32_t specific_key = key_material->remote_key_material->receiver_specific_key_id;
    if (specific_key != 0)
    {
      key_relation *relation = crypto_find_specific_key_relation_locked(remote_crypto, specific_key);
      if (!relation)
      {
        local_participant_crypto *local_crypto = (local_participant_crypto *)crypto_object_table_find(impl->crypto_objects, local_id);
        relation = crypto_key_relation_new(0, specific_key, CRYPTO_OBJECT(local_crypto), CRYPTO_OBJECT(remote_crypto), key_material->remote_key_material);
        crypto_insert_specific_key_relation_locked(remote_crypto, relation);
        CRYPTO_OBJECT_RELEASE(local_crypto);
      }
      CRYPTO_OBJECT_RELEASE(relation);
    }
    ddsrt_mutex_unlock(&remote_crypto->lock);
    CRYPTO_OBJECT_RELEASE(key_material);
  }
  else
  {
    ddsrt_mutex_unlock(&remote_crypto->lock);
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_remote;
  }
  result = true;

err_inv_remote:
  CRYPTO_OBJECT_RELEASE(remote_crypto);
err_no_remote:
  return result;
}

bool
crypto_factory_get_datawriter_crypto_tokens(
    const dds_security_crypto_key_factory *factory,
    DDS_Security_DatawriterCryptoHandle local_writer_handle,
    DDS_Security_DatareaderCryptoHandle remote_reader_handle,
    master_key_material **key_mat,
    uint32_t *num_key_mat,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_key_factory_impl *impl = (dds_security_crypto_key_factory_impl *)factory;
  remote_datareader_crypto *remote_reader_crypto;
  uint32_t index = 0;
  bool result = false;

  assert(factory);
  assert(local_writer_handle != DDS_SECURITY_HANDLE_NIL);
  assert(remote_reader_handle != DDS_SECURITY_HANDLE_NIL);
  assert(key_mat);
  assert(num_key_mat);
  assert((*num_key_mat) == 2);

  remote_reader_crypto = (remote_datareader_crypto *)crypto_object_table_find(impl->crypto_objects, remote_reader_handle);
  if (!remote_reader_crypto)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_no_remote;
  }
  else if (!CRYPTO_OBJECT_VALID(remote_reader_crypto, CRYPTO_OBJECT_KIND_REMOTE_READER_CRYPTO))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_remote;
  }

  if (CRYPTO_OBJECT_HANDLE(remote_reader_crypto->local_writer) != local_writer_handle)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_remote;
  }

  if (remote_reader_crypto->writer2reader_key_material_message)
    key_mat[index++] = (master_key_material *)CRYPTO_OBJECT_KEEP(remote_reader_crypto->writer2reader_key_material_message);

  if (remote_reader_crypto->writer2reader_key_material_payload)
    key_mat[index++] = (master_key_material *)CRYPTO_OBJECT_KEEP(remote_reader_crypto->writer2reader_key_material_payload);

  *num_key_mat = index;
  result = true;

err_inv_remote:
  CRYPTO_OBJECT_RELEASE(remote_reader_crypto);
err_no_remote:
  return result;
}

bool
crypto_factory_set_datawriter_crypto_tokens(
    const dds_security_crypto_key_factory *factory,
    const DDS_Security_DatawriterCryptoHandle local_reader_handle,
    const DDS_Security_DatareaderCryptoHandle remote_writer_handle,
    const DDS_Security_KeyMaterial_AES_GCM_GMAC *key_mat,
    const uint32_t num_key_mat,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_key_factory_impl *impl = (dds_security_crypto_key_factory_impl *)factory;
  bool result = false;
  remote_datawriter_crypto *remote_writer_crypto;
  local_datareader_crypto *local_reader_crypto;
  master_key_material *writer_master_key[2] = {NULL, NULL};
  key_relation *relation;
  uint32_t key_id, i;

  assert (num_key_mat > 0);

  remote_writer_crypto = (remote_datawriter_crypto *)crypto_object_table_find(impl->crypto_objects, remote_writer_handle);
  if (!remote_writer_crypto)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_no_remote;
  }
  else if (!CRYPTO_OBJECT_VALID(remote_writer_crypto, CRYPTO_OBJECT_KIND_REMOTE_WRITER_CRYPTO))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_remote;
  }

  local_reader_crypto = (local_datareader_crypto *)crypto_object_table_find(impl->crypto_objects, local_reader_handle);
  if (!local_reader_crypto)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_local;
  }
  else if (!CRYPTO_OBJECT_VALID(local_reader_crypto, CRYPTO_OBJECT_KIND_LOCAL_READER_CRYPTO))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_local;
  }

  if (CRYPTO_OBJECT_HANDLE(remote_writer_crypto->local_reader) != local_reader_handle)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_local;
  }

  for (i = 0; i < num_key_mat; i++)
  {
    writer_master_key[i] = crypto_master_key_material_new(CRYPTO_TRANSFORMATION_KIND_NONE);
    crypto_token_copy(writer_master_key[i], &key_mat[i]);
  }

  remove_remote_writer_relation(impl, remote_writer_crypto);
  CRYPTO_OBJECT_RELEASE(remote_writer_crypto->writer2reader_key_material[0]);
  CRYPTO_OBJECT_RELEASE(remote_writer_crypto->writer2reader_key_material[1]);

  assert(writer_master_key[0]);
  remote_writer_crypto->writer2reader_key_material[0] = writer_master_key[0];
  if (writer_master_key[1])
    remote_writer_crypto->writer2reader_key_material[1] = writer_master_key[1];
  else
    remote_writer_crypto->writer2reader_key_material[1] = (master_key_material *)CRYPTO_OBJECT_KEEP(writer_master_key[0]);

  key_id = remote_writer_crypto->writer2reader_key_material[0]->sender_key_id;

  relation = crypto_key_relation_new(DDS_SECURITY_DATAWRITER_SUBMESSAGE, key_id, (CryptoObject *)local_reader_crypto, (CryptoObject *)remote_writer_crypto, NULL);
  crypto_insert_endpoint_relation(remote_writer_crypto->participant, relation);
  CRYPTO_OBJECT_RELEASE(relation);

  uint32_t specific_key = remote_writer_crypto->writer2reader_key_material[0]->receiver_specific_key_id;
  if (specific_key != 0)
  {
    relation = crypto_find_specific_key_relation(remote_writer_crypto->participant, specific_key);
    if (!relation)
    {
      relation = crypto_key_relation_new(0, specific_key, CRYPTO_OBJECT(local_reader_crypto), CRYPTO_OBJECT(remote_writer_crypto), remote_writer_crypto->writer2reader_key_material[0]);
      crypto_insert_specific_key_relation(remote_writer_crypto->participant, relation);
    }
    CRYPTO_OBJECT_RELEASE(relation);
  }

  result = true;

err_inv_local:
  CRYPTO_OBJECT_RELEASE(local_reader_crypto);
err_inv_remote:
  CRYPTO_OBJECT_RELEASE(remote_writer_crypto);
err_no_remote:
  return result;
}

bool
crypto_factory_get_datareader_crypto_tokens(
    const dds_security_crypto_key_factory *factory,
    DDS_Security_DatawriterCryptoHandle local_reader_handle,
    DDS_Security_DatareaderCryptoHandle remote_writer_handle,
    master_key_material **key_mat,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_key_factory_impl *impl = (dds_security_crypto_key_factory_impl *)factory;
  remote_datawriter_crypto *remote_writer_crypto;
  bool result = false;

  assert(factory);
  assert(local_reader_handle != DDS_SECURITY_HANDLE_NIL);
  assert(remote_writer_handle != DDS_SECURITY_HANDLE_NIL);
  assert(key_mat);

  remote_writer_crypto = (remote_datawriter_crypto *)crypto_object_table_find(impl->crypto_objects, remote_writer_handle);
  if (!remote_writer_crypto)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_no_remote;
  }
  else if (!CRYPTO_OBJECT_VALID(remote_writer_crypto, CRYPTO_OBJECT_KIND_REMOTE_WRITER_CRYPTO))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_remote;
  }

  if (CRYPTO_OBJECT_HANDLE(remote_writer_crypto->local_reader) != local_reader_handle)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_remote;
  }

  if (remote_writer_crypto->reader2writer_key_material)
    *key_mat = (master_key_material *)CRYPTO_OBJECT_KEEP(remote_writer_crypto->reader2writer_key_material);
  else
    *key_mat = NULL; /* there is no key material to return, because of no encryption for submessages in AccessControl */
  result = true;

err_inv_remote:
  CRYPTO_OBJECT_RELEASE(remote_writer_crypto);
err_no_remote:
  return result;
}

bool
crypto_factory_set_datareader_crypto_tokens(
    const dds_security_crypto_key_factory *factory,
    const DDS_Security_DatawriterCryptoHandle local_writer_handle,
    const DDS_Security_DatareaderCryptoHandle remote_reader_handle,
    const DDS_Security_KeyMaterial_AES_GCM_GMAC *key_mat,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_key_factory_impl *impl = (dds_security_crypto_key_factory_impl *)factory;
  bool result = false;
  remote_datareader_crypto *remote_reader_crypto;
  local_datawriter_crypto *local_writer_crypto;
  key_relation *relation;
  uint32_t key_id;

  remote_reader_crypto = (remote_datareader_crypto *)crypto_object_table_find(impl->crypto_objects, remote_reader_handle);
  if (!remote_reader_crypto)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_remote;
  }
  else if (!CRYPTO_OBJECT_VALID(remote_reader_crypto, CRYPTO_OBJECT_KIND_REMOTE_READER_CRYPTO))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_remote;
  }

  local_writer_crypto = (local_datawriter_crypto *)crypto_object_table_find(impl->crypto_objects, local_writer_handle);
  if (!local_writer_crypto)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_local;
  }
  else if (!CRYPTO_OBJECT_VALID(local_writer_crypto, CRYPTO_OBJECT_KIND_LOCAL_WRITER_CRYPTO))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_local;
  }

  if (CRYPTO_OBJECT_HANDLE(remote_reader_crypto->local_writer) != local_writer_handle)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_local;
  }

  remove_remote_reader_relation(impl, remote_reader_crypto);
  CRYPTO_OBJECT_RELEASE(remote_reader_crypto->reader2writer_key_material);

  remote_reader_crypto->reader2writer_key_material = crypto_master_key_material_new(CRYPTO_TRANSFORMATION_KIND_NONE);
  crypto_token_copy(remote_reader_crypto->reader2writer_key_material, key_mat);

  key_id = remote_reader_crypto->reader2writer_key_material->sender_key_id;

  relation = crypto_key_relation_new(DDS_SECURITY_DATAREADER_SUBMESSAGE, key_id, (CryptoObject *)local_writer_crypto, (CryptoObject *)remote_reader_crypto, NULL);
  crypto_insert_endpoint_relation(remote_reader_crypto->participant, relation);
  CRYPTO_OBJECT_RELEASE(relation);

  uint32_t specific_key = remote_reader_crypto->reader2writer_key_material->receiver_specific_key_id;
  if (specific_key != 0)
  {
    relation = crypto_find_specific_key_relation(remote_reader_crypto->participant, specific_key);
    if (!relation)
    {
      relation = crypto_key_relation_new(0, specific_key, CRYPTO_OBJECT(local_writer_crypto), CRYPTO_OBJECT(remote_reader_crypto), remote_reader_crypto->reader2writer_key_material);
      crypto_insert_specific_key_relation(remote_reader_crypto->participant, relation);
    }
    CRYPTO_OBJECT_RELEASE(relation);
  }

  result = true;

err_inv_local:
  CRYPTO_OBJECT_RELEASE(local_writer_crypto);
err_inv_remote:
  CRYPTO_OBJECT_RELEASE(remote_reader_crypto);
  return result;
}

static bool
get_local_volatile_sec_writer_key_material(
    dds_security_crypto_key_factory_impl *factory,
    const DDS_Security_DatareaderCryptoHandle reader_id,
    session_key_material **session_key,
    DDS_Security_ProtectionKind *protection_kind,
    DDS_Security_SecurityException *ex)
{
  bool result = false;
  remote_datareader_crypto *reader_crypto;

  reader_crypto = (remote_datareader_crypto *)crypto_object_table_find(factory->crypto_objects, reader_id);
  if (!reader_crypto)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE " %"PRIx64, reader_id);
    goto err_no_crypto;
  }
  else if (!CRYPTO_OBJECT_VALID(reader_crypto, CRYPTO_OBJECT_KIND_REMOTE_READER_CRYPTO))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_crypto;
  }

  *session_key = (session_key_material *)CRYPTO_OBJECT_KEEP(reader_crypto->writer_session);
  *protection_kind = reader_crypto->metadata_protectionKind;
  result = true;

err_inv_crypto:
  CRYPTO_OBJECT_RELEASE(reader_crypto);
err_no_crypto:
  return result;
}

static bool
get_local_volatile_sec_reader_key_material(
    dds_security_crypto_key_factory_impl *factory,
    const DDS_Security_DatawriterCryptoHandle writer_id,
    session_key_material **session_key,
    DDS_Security_ProtectionKind *protection_kind,
    DDS_Security_SecurityException *ex)
{
  bool result = false;
  remote_datawriter_crypto *writer_crypto;

  writer_crypto = (remote_datawriter_crypto *)crypto_object_table_find(factory->crypto_objects, writer_id);
  if (!writer_crypto)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_no_crypto;
  }
  else if (!CRYPTO_OBJECT_VALID(writer_crypto, CRYPTO_OBJECT_KIND_REMOTE_WRITER_CRYPTO))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE " %"PRIx64, writer_id);
    goto err_inv_crypto;
  }

  *session_key = (session_key_material *)CRYPTO_OBJECT_KEEP(writer_crypto->reader_session);
  *protection_kind = writer_crypto->metadata_protectionKind;
  result = true;

err_inv_crypto:
  CRYPTO_OBJECT_RELEASE(writer_crypto);
err_no_crypto:
  return result;
}

bool
crypto_factory_get_local_participant_data_key_material(
    const dds_security_crypto_key_factory *factory,
    const DDS_Security_ParticipantCryptoHandle local_id,
    session_key_material **session_key,
    DDS_Security_ProtectionKind *protection_kind,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_key_factory_impl *impl = (dds_security_crypto_key_factory_impl *)factory;
  local_participant_crypto *participant_crypto;
  bool result = false;

  participant_crypto = (local_participant_crypto *)crypto_object_table_find(impl->crypto_objects, local_id);
  if (!(participant_crypto))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_no_crypto;
  }
  else if (!CRYPTO_OBJECT_VALID(participant_crypto, CRYPTO_OBJECT_KIND_LOCAL_CRYPTO))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_crypto;
  }

  *session_key = (session_key_material *)CRYPTO_OBJECT_KEEP(participant_crypto->session);
  *protection_kind = participant_crypto->rtps_protection_kind;
  result = true;

err_inv_crypto:
  CRYPTO_OBJECT_RELEASE(participant_crypto);
err_no_crypto:
  return result;
}

bool
crypto_factory_get_writer_key_material(
    const dds_security_crypto_key_factory *factory,
    const DDS_Security_DatawriterCryptoHandle writer_id,
    const DDS_Security_DatareaderCryptoHandle reader_id,
    bool payload,
    session_key_material **session_key,
    DDS_Security_ProtectionKind *protection_kind,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_key_factory_impl *impl = (dds_security_crypto_key_factory_impl *)factory;
  local_datawriter_crypto *writer_crypto = NULL;
  bool result = false;

  writer_crypto = (local_datawriter_crypto *)crypto_object_table_find(impl->crypto_objects, writer_id);
  if (!writer_crypto)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE " %"PRIx64, writer_id);
    goto err_remote;
  }
  if (!CRYPTO_OBJECT_VALID(writer_crypto, CRYPTO_OBJECT_KIND_LOCAL_WRITER_CRYPTO))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_remote;
  }
  if (!writer_crypto->is_builtin_participant_volatile_message_secure_writer)
  {
    if (payload)
      *session_key = (session_key_material *)CRYPTO_OBJECT_KEEP(writer_crypto->writer_session_payload);
    else
      *session_key = (session_key_material *)CRYPTO_OBJECT_KEEP(writer_crypto->writer_session_message);

    if (protection_kind)
      *protection_kind = writer_crypto->metadata_protectionKind;
    result = true;
  }
  else if (!payload)
  {
    result = get_local_volatile_sec_writer_key_material(impl, reader_id, session_key, protection_kind, ex);
  }
  else
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
  }

err_inv_remote:
  CRYPTO_OBJECT_RELEASE(writer_crypto);
err_remote:
  return result;
}

bool
crypto_factory_get_reader_key_material(
    const dds_security_crypto_key_factory *factory,
    const DDS_Security_DatareaderCryptoHandle reader_id,
    const DDS_Security_DatawriterCryptoHandle writer_id,
    session_key_material **session_key,
    DDS_Security_ProtectionKind *protection_kind,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_key_factory_impl *impl = (dds_security_crypto_key_factory_impl *)factory;
  local_datareader_crypto *reader_crypto;
  bool result = false;

  reader_crypto = (local_datareader_crypto *)crypto_object_table_find(impl->crypto_objects, reader_id);
  if (!reader_crypto)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE " %"PRIx64, reader_id);
    goto err_no_crypto;
  }
  else if (!CRYPTO_OBJECT_VALID(reader_crypto, CRYPTO_OBJECT_KIND_LOCAL_READER_CRYPTO))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_crypto;
  }
  else if (!reader_crypto->is_builtin_participant_volatile_message_secure_reader)
  {
    *session_key = (session_key_material *)CRYPTO_OBJECT_KEEP(reader_crypto->reader_session);
    if (protection_kind)
      *protection_kind = reader_crypto->metadata_protectionKind;
    result = true;
  }
  else
    result = get_local_volatile_sec_reader_key_material(impl, writer_id, session_key, protection_kind, ex);

err_inv_crypto:
  CRYPTO_OBJECT_RELEASE(reader_crypto);
err_no_crypto:
  return result;
}

bool
crypto_factory_get_remote_writer_key_material(
    const dds_security_crypto_key_factory *factory,
    const DDS_Security_DatareaderCryptoHandle reader_id,
    const DDS_Security_DatawriterCryptoHandle writer_id,
    uint32_t key_id,
    master_key_material **master_key,
    DDS_Security_ProtectionKind *protection_kind,
    DDS_Security_BasicProtectionKind *basic_protection_kind,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_key_factory_impl *impl = (dds_security_crypto_key_factory_impl *)factory;
  remote_datawriter_crypto *writer_crypto;
  bool result = false;

  writer_crypto = (remote_datawriter_crypto *)crypto_object_table_find(impl->crypto_objects, writer_id);
  if (!writer_crypto)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_no_crypto;
  }
  if (!CRYPTO_OBJECT_VALID(writer_crypto, CRYPTO_OBJECT_KIND_REMOTE_WRITER_CRYPTO))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_crypto;
  }
  if (CRYPTO_OBJECT_HANDLE(writer_crypto->local_reader) != reader_id)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
        DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_crypto;
  }

  if (writer_crypto->writer2reader_key_material[0]->sender_key_id == key_id)
  {
    *master_key = (master_key_material *)CRYPTO_OBJECT_KEEP(writer_crypto->writer2reader_key_material[0]);
  }
  else if (writer_crypto->writer2reader_key_material[1]->sender_key_id == key_id)
  {
    *master_key = (master_key_material *)CRYPTO_OBJECT_KEEP(writer_crypto->writer2reader_key_material[1]);
  }
  else
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT,
                                DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
                                DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_crypto;
  }
  if (protection_kind)
    *protection_kind = writer_crypto->metadata_protectionKind;
  if (basic_protection_kind)
    *basic_protection_kind = writer_crypto->data_protectionKind;
  result = true;

err_inv_crypto:
  CRYPTO_OBJECT_RELEASE(writer_crypto);
err_no_crypto:

  return result;
}

bool
crypto_factory_get_remote_reader_key_material(
    const dds_security_crypto_key_factory *factory,
    const DDS_Security_DatawriterCryptoHandle writer_id,
    const DDS_Security_DatareaderCryptoHandle reader_id,
    uint32_t key_id,
    master_key_material **master_key,
    DDS_Security_ProtectionKind *protection_kind,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_key_factory_impl *impl = (dds_security_crypto_key_factory_impl *)factory;
  remote_datareader_crypto *reader_crypto;
  bool result = false;

  reader_crypto = (remote_datareader_crypto *)crypto_object_table_find(impl->crypto_objects, reader_id);
  if (!reader_crypto)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_no_crypto;
  }
  if (!CRYPTO_OBJECT_VALID(reader_crypto, CRYPTO_OBJECT_KIND_REMOTE_READER_CRYPTO))
  {
    CRYPTO_OBJECT_RELEASE(reader_crypto);
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_crypto;
  }
  if (CRYPTO_OBJECT_HANDLE(reader_crypto->local_writer) != writer_id)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_crypto;
  }
  if (reader_crypto->reader2writer_key_material->sender_key_id == key_id)
  {
    *master_key = (master_key_material *)CRYPTO_OBJECT_KEEP(reader_crypto->reader2writer_key_material);
  }
  else
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT,
                                DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
                                DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_crypto;
  }
  if (protection_kind)
    *protection_kind = reader_crypto->metadata_protectionKind;
  result = true;

err_inv_crypto:
  CRYPTO_OBJECT_RELEASE(reader_crypto);
err_no_crypto:
  return result;
}

bool
crypto_factory_get_remote_writer_sign_key_material(
    const dds_security_crypto_key_factory *factory,
    const DDS_Security_DatareaderCryptoHandle writer_id,
    master_key_material **key_material,
    session_key_material **session_key,
    DDS_Security_ProtectionKind *protection_kind,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_key_factory_impl *impl = (dds_security_crypto_key_factory_impl *)factory;
  remote_datawriter_crypto *writer_crypto;
  bool result = false;

  assert(key_material);
  assert(session_key);
  assert(protection_kind);

  writer_crypto = (remote_datawriter_crypto *)crypto_object_table_find(impl->crypto_objects, writer_id);
  if (!writer_crypto)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_no_crypto;
  }
  else if (!CRYPTO_OBJECT_VALID(writer_crypto, CRYPTO_OBJECT_KIND_REMOTE_WRITER_CRYPTO))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_crypto;
  }

  *key_material = (master_key_material *)CRYPTO_OBJECT_KEEP(writer_crypto->reader2writer_key_material);
  *session_key = (session_key_material *)CRYPTO_OBJECT_KEEP(writer_crypto->reader_session);
  *protection_kind = writer_crypto->metadata_protectionKind;
  result = true;

err_inv_crypto:
  CRYPTO_OBJECT_RELEASE(writer_crypto);
err_no_crypto:
  return result;
}

bool
crypto_factory_get_remote_reader_sign_key_material(
    const dds_security_crypto_key_factory *factory,
    const DDS_Security_DatareaderCryptoHandle reader_id,
    master_key_material **key_material,
    session_key_material **session_key,
    DDS_Security_ProtectionKind *protection_kind,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_key_factory_impl *impl = (dds_security_crypto_key_factory_impl *)factory;
  remote_datareader_crypto *reader_crypto;
  bool result = false;

  assert(key_material);
  assert(session_key);
  assert(protection_kind);

  reader_crypto = (remote_datareader_crypto *)crypto_object_table_find(impl->crypto_objects, reader_id);
  if (!reader_crypto)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_no_crypto;
  }
  else if (!CRYPTO_OBJECT_VALID(reader_crypto, CRYPTO_OBJECT_KIND_REMOTE_READER_CRYPTO))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto err_inv_crypto;
  }

  *key_material = (master_key_material *)CRYPTO_OBJECT_KEEP(reader_crypto->writer2reader_key_material_message);
  *session_key = (session_key_material *)CRYPTO_OBJECT_KEEP(reader_crypto->writer_session);
  *protection_kind = reader_crypto->metadata_protectionKind;
  result = true;

err_inv_crypto:
  CRYPTO_OBJECT_RELEASE(reader_crypto);
err_no_crypto:
  return result;
}

bool
crypto_factory_get_endpoint_relation(
    const dds_security_crypto_key_factory *factory,
    DDS_Security_ParticipantCryptoHandle local_participant_handle,
    DDS_Security_ParticipantCryptoHandle remote_participant_handle,
    uint32_t key_id,
    DDS_Security_Handle *remote_handle,
    DDS_Security_Handle *local_handle,
    DDS_Security_SecureSubmessageCategory_t *category,
    DDS_Security_SecurityException *ex)
{
  bool result = false;
  dds_security_crypto_key_factory_impl *impl = (dds_security_crypto_key_factory_impl *)factory;
  remote_participant_crypto *rmt_pp;
  local_participant_crypto *loc_pp = NULL;
  local_datareader_crypto *loc_rd = NULL;
  key_relation *relation = NULL;

  rmt_pp = (remote_participant_crypto *)crypto_object_table_find(impl->crypto_objects, remote_participant_handle);
  if (!rmt_pp)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto invalid_handle;
  }
  else if (!CRYPTO_OBJECT_VALID(rmt_pp, CRYPTO_OBJECT_KIND_REMOTE_CRYPTO))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
    goto invalid_handle;
  }

  if (key_id == 0 && local_participant_handle != DDS_SECURITY_HANDLE_NIL)
  {
    loc_pp = (local_participant_crypto *)crypto_object_table_find(impl->crypto_objects, local_participant_handle);
    if (!loc_pp)
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT,
          DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
          DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
      goto invalid_handle;
    }
    else if (!CRYPTO_OBJECT_VALID(loc_pp, CRYPTO_OBJECT_KIND_LOCAL_CRYPTO))
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT,
          DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
          DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE);
      goto invalid_handle;
    }
    loc_rd = loc_pp->builtin_reader;
  }

  relation = crypto_find_endpoint_relation(rmt_pp, CRYPTO_OBJECT(loc_rd), key_id);
  if (!relation)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE, 0,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE " key_id=%u", key_id);
    goto invalid_handle;
  }

  assert(key_id == relation->key_id);

  *category = relation->kind;
  *remote_handle = CRYPTO_OBJECT_HANDLE(relation->remote_crypto);
  *local_handle = CRYPTO_OBJECT_HANDLE(relation->local_crypto);
  result = true;

invalid_handle:
  CRYPTO_OBJECT_RELEASE(relation);
  CRYPTO_OBJECT_RELEASE(loc_pp);
  CRYPTO_OBJECT_RELEASE(rmt_pp);
  return result;
}

bool
crypto_factory_get_specific_keymat(
    const dds_security_crypto_key_factory *factory,
    CryptoObjectKind_t kind,
    DDS_Security_Handle rmt_handle,
    const struct receiver_specific_mac * const mac_list,
    uint32_t num_mac,
    uint32_t *index,
    master_key_material **key_mat)
{
  dds_security_crypto_key_factory_impl *impl = (dds_security_crypto_key_factory_impl *)factory;
  CryptoObject *obj;
  remote_participant_crypto *rmt_pp = NULL;
  remote_datawriter_crypto *rmt_wr = NULL;
  remote_datareader_crypto *rmt_rd = NULL;
  key_relation *relation = NULL;
  bool result = false;

  obj = crypto_object_table_find(impl->crypto_objects, rmt_handle);
  if (!obj)
    return false;

  switch (kind)
  {
  case CRYPTO_OBJECT_KIND_REMOTE_CRYPTO:
    rmt_pp = (remote_participant_crypto *)obj;
    break;
  case CRYPTO_OBJECT_KIND_REMOTE_WRITER_CRYPTO:
    rmt_wr = (remote_datawriter_crypto *)obj;
    rmt_pp = rmt_wr->participant;
    break;
  case CRYPTO_OBJECT_KIND_REMOTE_READER_CRYPTO:
    rmt_rd = (remote_datareader_crypto *)obj;
    rmt_pp = rmt_rd->participant;
    break;
  default:
    goto invalid_handle;
    break;
  }

  for (uint32_t i = 0; i < num_mac; i++)
  {
    uint32_t key_id = CRYPTO_TRANSFORM_ID(mac_list[i].receiver_mac_key_id);
    relation = crypto_find_specific_key_relation(rmt_pp, key_id);
    if (relation)
    {
      *index = i;
      *key_mat = CRYPTO_OBJECT_KEEP(relation->key_material);
      result = true;
      break;
    }
  }

invalid_handle:
  CRYPTO_OBJECT_RELEASE(relation);
  CRYPTO_OBJECT_RELEASE(obj);
  return result;
}

master_key_material *
crypto_factory_get_master_key_material_for_test(
    const dds_security_crypto_key_factory *factory,
    DDS_Security_ParticipantCryptoHandle local_id,
    DDS_Security_ParticipantCryptoHandle remote_id)
{
  dds_security_crypto_key_factory_impl *impl = (dds_security_crypto_key_factory_impl *)factory;
  remote_participant_crypto *rmt_cr = (remote_participant_crypto *)crypto_object_table_find(impl->crypto_objects, remote_id);
  participant_key_material *keymat;
  master_key_material *master_keymat = NULL;

  if (rmt_cr)
  {
    keymat = crypto_remote_participant_lookup_keymat(rmt_cr, local_id);
    if (keymat)
    {
      master_keymat = keymat->local_P2P_key_material;
      CRYPTO_OBJECT_RELEASE(keymat);
    }
  }

  return master_keymat;
}
