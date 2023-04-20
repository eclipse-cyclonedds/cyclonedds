// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/types.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/core/dds_security_serialize.h"
#include "crypto_defs.h"
#include "crypto_utils.h"
#include "cryptography.h"
#include "crypto_key_exchange.h"
#include "crypto_key_factory.h"
#include "crypto_tokens.h"

/**
 * Implementation structure for storing encapsulated members of the instance
 * while giving only the interface definition to user
 */

typedef struct dds_security_crypto_key_exchange_impl
{
  dds_security_crypto_key_exchange base;
  const dds_security_cryptography *crypto;
} dds_security_crypto_key_exchange_impl;

static bool check_crypto_tokens(const DDS_Security_DataHolderSeq *tokens)
{
  bool status = true;
  uint32_t i;

  if (tokens->_length == 0 || tokens->_buffer == NULL)
    status = false;

  for (i = 0; status && (i < tokens->_length); i++)
  {
    status = (tokens->_buffer[i].class_id != NULL) &&
             (strcmp(DDS_CRYPTOTOKEN_CLASS_ID, tokens->_buffer[i].class_id) == 0) &&
             (tokens->_buffer[i].binary_properties._length == 1) &&
             (tokens->_buffer[i].binary_properties._buffer != NULL) &&
             (tokens->_buffer[i].binary_properties._buffer[0].name != NULL) &&
             (strcmp(DDS_CRYPTOTOKEN_PROP_KEYMAT, tokens->_buffer[i].binary_properties._buffer[0].name) == 0) &&
             (tokens->_buffer[i].binary_properties._buffer[0].value._length > 0) &&
             (tokens->_buffer[i].binary_properties._buffer[0].value._buffer != NULL);
  }

  return status;
}

static bool check_not_data_empty(const DDS_Security_OctetSeq *seq)
{
  uint32_t i;

  for (i = 0; i < seq->_length; i++)
  {
    if (seq->_buffer[i] != 0)
      return true;
  }
  return false;
}

static bool check_crypto_keymaterial(
    const dds_security_crypto_key_exchange_impl *impl,
    const DDS_Security_KeyMaterial_AES_GCM_GMAC *keymat,
    const int64_t handle)
{
  bool status = false;
  uint32_t transform_kind = CRYPTO_TRANSFORM_KIND(keymat->transformation_kind);

  if (transform_kind != CRYPTO_TRANSFORMATION_KIND_NONE)
  {
    if (transform_kind <= CRYPTO_TRANSFORMATION_KIND_AES256_GCM)
    {
      uint32_t key_sz = CRYPTO_KEY_SIZE_BYTES(transform_kind);
      status = (keymat->master_salt._length == key_sz && keymat->master_salt._buffer != NULL && check_not_data_empty(&keymat->master_salt) &&
                keymat->master_sender_key._length == key_sz && keymat->master_sender_key._buffer != NULL && check_not_data_empty(&keymat->master_sender_key));
      if (status && CRYPTO_TRANSFORM_ID(keymat->receiver_specific_key_id))
      {
        status = (keymat->master_receiver_specific_key._length == key_sz &&
                  keymat->master_receiver_specific_key._buffer != NULL && check_not_data_empty(&keymat->master_receiver_specific_key));
      }
    }
  }
  else
  {
    const dds_security_crypto_key_factory *factory;
    DDS_Security_ProtectionKind kind;

    factory = cryptography_get_crypto_key_factory(impl->crypto);
    if (crypto_factory_get_protection_kind(factory, handle, &kind))
      status = (kind == DDS_SECURITY_PROTECTION_KIND_NONE);
  }

  return status;
}

// serialized_key_material
// {
//   uint32_t           transformation_kind
//   uint32_t           master_salt_len
//   uint8_t[SALT_SIZE] master_salt
//   uint32_t           sender_key_id
//   uint32_t           master_sender_key_len
//   uint8_t[KEY_SIZE]  master_sender_key
//   uint32_t           receiver_specific_key_id
//   uint32_t           master_receiver_specific_key_len
//   uint8_t[KEY_SIZE]  master_receiver_specific_key
// }
static void
serialize_master_key_material(
    const master_key_material *keymat,
    uint8_t **buffer,
    uint32_t *length)
{
  uint32_t *sd;
  size_t i = 0;
  uint32_t key_bytes = CRYPTO_KEY_SIZE_BYTES(keymat->transformation_kind);
  size_t sz = 6 * sizeof (uint32_t) + 2 * key_bytes;
  if (keymat->receiver_specific_key_id)
    sz += key_bytes;
  *buffer = ddsrt_malloc(sz);
  *length = (uint32_t)sz;
  sd = (uint32_t *)(*buffer);

  sd[i++] = ddsrt_toBE4u(keymat->transformation_kind);
  sd[i++] = ddsrt_toBE4u(key_bytes);
  if (key_bytes > 0) {
    memcpy(&sd[i], keymat->master_salt, key_bytes);
  }
  i += key_bytes / sizeof (uint32_t);
  sd[i++] = ddsrt_toBE4u(keymat->sender_key_id);
  sd[i++] = ddsrt_toBE4u(key_bytes);
  if (key_bytes > 0) {
    memcpy(&sd[i], keymat->master_sender_key, key_bytes);
  }
  i += key_bytes / sizeof (uint32_t);
  sd[i++] = ddsrt_toBE4u(keymat->receiver_specific_key_id);
  if (keymat->receiver_specific_key_id)
  {
    sd[i++] = ddsrt_toBE4u(key_bytes);
    if (key_bytes > 0) {
      memcpy(&sd[i], keymat->master_receiver_specific_key, key_bytes);
    }
  }
  else
  {
    sd[i++] = ddsrt_toBE4u(0);
  }
}


/**
 * Function implementations
 */
static DDS_Security_boolean
create_local_participant_crypto_tokens(
    dds_security_crypto_key_exchange *instance,
    DDS_Security_ParticipantCryptoTokenSeq *tokens,
    const DDS_Security_ParticipantCryptoHandle local_id,
    const DDS_Security_ParticipantCryptoHandle remote_id,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_key_exchange_impl *impl = (dds_security_crypto_key_exchange_impl *)instance;
  dds_security_crypto_key_factory *factory;
  participant_key_material *pp_key_material;
  uint8_t *buffer;
  uint32_t length;

  if (!instance || !tokens || local_id == 0 || remote_id == 0)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "create_local_participant_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    goto fail_invalid_arg;
  }

  factory = cryptography_get_crypto_key_factory(impl->crypto);
  if (!crypto_factory_get_participant_crypto_tokens(factory, local_id, remote_id, &pp_key_material, NULL, NULL, ex))
    goto fail_invalid_arg;
  serialize_master_key_material(pp_key_material->local_P2P_key_material, &buffer, &length);
  CRYPTO_OBJECT_RELEASE(pp_key_material);

  tokens->_buffer = DDS_Security_DataHolderSeq_allocbuf(1);
  tokens->_length = tokens->_maximum = 1;
  tokens->_buffer[0].class_id = ddsrt_strdup(DDS_CRYPTOTOKEN_CLASS_ID);
  tokens->_buffer[0].binary_properties._buffer = DDS_Security_BinaryPropertySeq_allocbuf(1);
  tokens->_buffer[0].binary_properties._length = tokens->_buffer[0].binary_properties._maximum = 1;
  tokens->_buffer[0].binary_properties._buffer[0].name = ddsrt_strdup(DDS_CRYPTOTOKEN_PROP_KEYMAT);
  tokens->_buffer[0].binary_properties._buffer[0].value._length =
      tokens->_buffer[0].binary_properties._buffer[0].value._maximum = length;
  tokens->_buffer[0].binary_properties._buffer[0].value._buffer = buffer;
  tokens->_buffer[0].binary_properties._buffer[0].propagate = true;
  return true;

fail_invalid_arg:
  return false;
}


static DDS_Security_boolean
allow_empty_tokens(
    const dds_security_crypto_key_exchange_impl *impl,
    const DDS_Security_ParticipantCryptoTokenSeq *tokens,
    const int64_t handle)
{
  const dds_security_crypto_key_factory *factory;
  DDS_Security_ProtectionKind kind;

  if (tokens->_length > 0)
    return false;

  factory = cryptography_get_crypto_key_factory(impl->crypto);
  if (crypto_factory_get_protection_kind(factory, handle, &kind))
    return (kind == DDS_SECURITY_PROTECTION_KIND_NONE);

  return false;
}


static DDS_Security_boolean
set_remote_participant_crypto_tokens(
    dds_security_crypto_key_exchange *instance,
    const DDS_Security_ParticipantCryptoHandle local_id,
    const DDS_Security_ParticipantCryptoHandle remote_id,
    const DDS_Security_ParticipantCryptoTokenSeq *tokens,
    DDS_Security_SecurityException *ex)
{
  DDS_Security_boolean result = false;
  dds_security_crypto_key_exchange_impl *impl = (dds_security_crypto_key_exchange_impl *)instance;
  dds_security_crypto_key_factory *factory;
  DDS_Security_KeyMaterial_AES_GCM_GMAC remote_key_mat;
  const DDS_Security_OctetSeq *tdata;
  DDS_Security_Deserializer deserializer;

  if (!instance || !tokens || local_id == 0 || remote_id == 0)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "set_remote_participant_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    return false;
  }

  if (allow_empty_tokens(impl, tokens, remote_id))
    return true;

  if (!check_crypto_tokens(tokens))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "set_remote_participant_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    return false;
  }

  tdata = &tokens->_buffer[0].binary_properties._buffer[0].value;

  deserializer = DDS_Security_Deserializer_new(tdata->_buffer, tdata->_length);
  if (!deserializer)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_TOKEN_CODE, 0,
        "set_remote_participant_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_TOKEN_MESSAGE);
    result = false;
  }
  else if (!DDS_Security_Deserialize_KeyMaterial_AES_GCM_GMAC(deserializer, &remote_key_mat))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_TOKEN_CODE, 0,
        "set_remote_participant_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_TOKEN_MESSAGE);
    result = false;
  }
  else if (!check_crypto_keymaterial(impl, &remote_key_mat, remote_id))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_TOKEN_CODE, 0,
        "set_remote_participant_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_TOKEN_MESSAGE);
    result = false;
  }
  else
  {
    factory = cryptography_get_crypto_key_factory(impl->crypto);
    result = crypto_factory_set_participant_crypto_tokens(factory, local_id, remote_id, &remote_key_mat, ex);
    DDS_Security_KeyMaterial_AES_GCM_GMAC_deinit(&remote_key_mat);
  }

  DDS_Security_Deserializer_free(deserializer);

  return result;
}

static DDS_Security_boolean
create_local_datawriter_crypto_tokens(
    dds_security_crypto_key_exchange *instance,
    DDS_Security_DatawriterCryptoTokenSeq *tokens,
    const DDS_Security_DatawriterCryptoHandle local_writer_handle,
    const DDS_Security_DatareaderCryptoHandle remote_reader_handle,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_key_exchange_impl *impl = (dds_security_crypto_key_exchange_impl *)instance;
  dds_security_crypto_key_factory *factory;
  master_key_material *key_mat[2];
  uint32_t num_key_mat = 2;
  uint32_t i;

  if (!instance || !tokens || local_writer_handle == DDS_SECURITY_HANDLE_NIL || remote_reader_handle == DDS_SECURITY_HANDLE_NIL)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "create_local_datawriter_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    return false;
  }

  factory = cryptography_get_crypto_key_factory(impl->crypto);

  if (!crypto_factory_get_datawriter_crypto_tokens(factory, local_writer_handle, remote_reader_handle, key_mat, &num_key_mat, ex))
    return false;

  tokens->_length = tokens->_maximum = num_key_mat;
  tokens->_buffer = (num_key_mat > 0) ? DDS_Security_DataHolderSeq_allocbuf(num_key_mat) : NULL;

  for (i = 0; i < num_key_mat; i++)
  {
    uint8_t *buffer;
    uint32_t length;

    serialize_master_key_material(key_mat[i], &buffer, &length);

    tokens->_buffer[i].class_id = ddsrt_strdup(DDS_CRYPTOTOKEN_CLASS_ID);
    tokens->_buffer[i].binary_properties._buffer = DDS_Security_BinaryPropertySeq_allocbuf(1);
    tokens->_buffer[i].binary_properties._length = tokens->_buffer[0].binary_properties._maximum = 1;
    tokens->_buffer[i].binary_properties._buffer[0].name = ddsrt_strdup(DDS_CRYPTOTOKEN_PROP_KEYMAT);
    tokens->_buffer[i].binary_properties._buffer[0].value._length =
        tokens->_buffer[i].binary_properties._buffer[0].value._maximum = length;
    tokens->_buffer[i].binary_properties._buffer[0].value._buffer = buffer;
    tokens->_buffer[i].binary_properties._buffer[0].propagate = true;
    CRYPTO_OBJECT_RELEASE(key_mat[i]);
  }

  return true;
}

static DDS_Security_boolean
set_remote_datawriter_crypto_tokens(
    dds_security_crypto_key_exchange *instance,
    const DDS_Security_DatareaderCryptoHandle local_reader_handle,
    const DDS_Security_DatawriterCryptoHandle remote_writer_handle,
    const DDS_Security_DatawriterCryptoTokenSeq *tokens,
    DDS_Security_SecurityException *ex)
{
  DDS_Security_boolean result = true;
  dds_security_crypto_key_exchange_impl *impl = (dds_security_crypto_key_exchange_impl *)instance;
  dds_security_crypto_key_factory *factory;
  DDS_Security_KeyMaterial_AES_GCM_GMAC remote_key_mat[2];
  uint32_t i;

  if (!instance || !tokens || local_reader_handle == DDS_SECURITY_HANDLE_NIL || remote_writer_handle == DDS_SECURITY_HANDLE_NIL)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "set_remote_datawriter_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    return false;
  }

  if (allow_empty_tokens(impl, tokens, remote_writer_handle))
    return true;

  if (!check_crypto_tokens(tokens))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "set_remote_datawriter_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    return false;
  }

  if (tokens->_length > 2)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "set_remote_datawriter_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    return false;
  }

  for (i = 0; result && (i < tokens->_length); i++)
  {
    const DDS_Security_OctetSeq *tdata = &tokens->_buffer[i].binary_properties._buffer[0].value;
    DDS_Security_Deserializer deserializer = DDS_Security_Deserializer_new(tdata->_buffer, tdata->_length);
    if (!deserializer)
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_TOKEN_CODE, 0,
          "set_remote_datawriter_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_TOKEN_MESSAGE);
      result = false;
    }
    else if (!DDS_Security_Deserialize_KeyMaterial_AES_GCM_GMAC(deserializer, &remote_key_mat[i]))
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_TOKEN_CODE, 0,
          "set_remote_datawriter_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_TOKEN_MESSAGE);
      result = false;
    }
    else if (!check_crypto_keymaterial(impl, &remote_key_mat[i], remote_writer_handle))
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_TOKEN_CODE, 0,
          "set_remote_datawriter_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_TOKEN_MESSAGE);
      result = false;
    }
    DDS_Security_Deserializer_free(deserializer);
  }

  if (result)
  {
    factory = cryptography_get_crypto_key_factory(impl->crypto);
    result = crypto_factory_set_datawriter_crypto_tokens(factory, local_reader_handle, remote_writer_handle, remote_key_mat, tokens->_length, ex);
  }

  for (i = 0; i < tokens->_length; i++)
    DDS_Security_KeyMaterial_AES_GCM_GMAC_deinit(&remote_key_mat[i]);

  return result;
}

static DDS_Security_boolean
create_local_datareader_crypto_tokens(
    dds_security_crypto_key_exchange *instance,
    DDS_Security_DatareaderCryptoTokenSeq *tokens,
    const DDS_Security_DatareaderCryptoHandle local_reader_handle,
    const DDS_Security_DatawriterCryptoHandle remote_writer_handle,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_key_exchange_impl *impl = (dds_security_crypto_key_exchange_impl *)instance;
  dds_security_crypto_key_factory *factory;
  master_key_material *key_mat = NULL;
  uint8_t *buffer;
  uint32_t length;

  if (!instance || !tokens || local_reader_handle == DDS_SECURITY_HANDLE_NIL || remote_writer_handle == DDS_SECURITY_HANDLE_NIL)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "create_local_datareader_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    return false;
  }

  factory = cryptography_get_crypto_key_factory(impl->crypto);

  if (!crypto_factory_get_datareader_crypto_tokens(factory, local_reader_handle, remote_writer_handle, &key_mat, ex))
    return false;

  if (key_mat != NULL)
  { /* there may be no keymaterial according to configuration */

    serialize_master_key_material(key_mat, &buffer, &length);

    tokens->_buffer = DDS_Security_DataHolderSeq_allocbuf(1);
    tokens->_length = tokens->_maximum = 1;

    tokens->_buffer[0].class_id = ddsrt_strdup(DDS_CRYPTOTOKEN_CLASS_ID);
    tokens->_buffer[0].binary_properties._buffer = DDS_Security_BinaryPropertySeq_allocbuf(1);
    tokens->_buffer[0].binary_properties._length = tokens->_buffer[0].binary_properties._maximum = 1;
    tokens->_buffer[0].binary_properties._buffer[0].name = ddsrt_strdup(DDS_CRYPTOTOKEN_PROP_KEYMAT);
    tokens->_buffer[0].binary_properties._buffer[0].value._length =
        tokens->_buffer[0].binary_properties._buffer[0].value._maximum = length;
    tokens->_buffer[0].binary_properties._buffer[0].value._buffer = buffer;
    tokens->_buffer[0].binary_properties._buffer[0].propagate = true;

    CRYPTO_OBJECT_RELEASE(key_mat);
  }
  else
  {
    tokens->_buffer = NULL;
    tokens->_length = 0;
    tokens->_maximum = 0;
  }

  return true;
}

static DDS_Security_boolean
set_remote_datareader_crypto_tokens(
    dds_security_crypto_key_exchange *instance,
    const DDS_Security_DatawriterCryptoHandle local_writer_handle,
    const DDS_Security_DatareaderCryptoHandle remote_reader_handle,
    const DDS_Security_DatareaderCryptoTokenSeq *tokens,
    DDS_Security_SecurityException *ex)
{
  DDS_Security_boolean result = false;
  dds_security_crypto_key_exchange_impl *impl = (dds_security_crypto_key_exchange_impl *)instance;
  dds_security_crypto_key_factory *factory;
  DDS_Security_KeyMaterial_AES_GCM_GMAC remote_key_mat;
  const DDS_Security_OctetSeq *tdata;
  DDS_Security_Deserializer deserializer;

  if (!instance || !tokens || local_writer_handle == DDS_SECURITY_HANDLE_NIL || remote_reader_handle == DDS_SECURITY_HANDLE_NIL)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "set_remote_datareader_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    return false;
  }

  if (allow_empty_tokens(impl, tokens, remote_reader_handle))
    return true;

  if (!check_crypto_tokens(tokens))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "set_remote_datareader_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    return false;
  }

  tdata = &tokens->_buffer[0].binary_properties._buffer[0].value;

  deserializer = DDS_Security_Deserializer_new(tdata->_buffer, tdata->_length);
  if (!deserializer)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_TOKEN_CODE, 0,
        "set_remote_datareader_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_TOKEN_MESSAGE);
    result = false;
  }
  else if (!DDS_Security_Deserialize_KeyMaterial_AES_GCM_GMAC(deserializer, &remote_key_mat))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_TOKEN_CODE, 0,
        "set_remote_datareader_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_TOKEN_MESSAGE);
    result = false;
  }
  else if (!check_crypto_keymaterial(impl, &remote_key_mat, remote_reader_handle))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_TOKEN_CODE, 0,
        "set_remote_datareader_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_TOKEN_MESSAGE);
    result = false;
  }
  else
  {
    factory = cryptography_get_crypto_key_factory(impl->crypto);
    result = crypto_factory_set_datareader_crypto_tokens(factory, local_writer_handle, remote_reader_handle, &remote_key_mat, ex);
  }

  DDS_Security_KeyMaterial_AES_GCM_GMAC_deinit(&remote_key_mat);

  DDS_Security_Deserializer_free(deserializer);

  return result;
}

static DDS_Security_boolean
return_crypto_tokens(
    dds_security_crypto_key_exchange *instance,
    DDS_Security_CryptoTokenSeq *tokens,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_key_exchange_impl *impl = (dds_security_crypto_key_exchange_impl *)instance;

  if (!impl || !tokens)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "return_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    goto fail_invalid_arg;
  }

  if (!check_crypto_tokens(tokens))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "set_remote_participant_crypto_tokens: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    goto fail_invalid_arg;
  }

  DDS_Security_CryptoTokenSeq_freebuf(tokens);
  memset(tokens, 0, sizeof(*tokens));
  return true;

fail_invalid_arg:
  return false;
}

dds_security_crypto_key_exchange *
dds_security_crypto_key_exchange__alloc(
    const dds_security_cryptography *crypto)
{
  dds_security_crypto_key_exchange_impl *instance = ddsrt_malloc(sizeof(*instance));
  instance->crypto = crypto;
  instance->base.create_local_participant_crypto_tokens = &create_local_participant_crypto_tokens;
  instance->base.set_remote_participant_crypto_tokens = &set_remote_participant_crypto_tokens;
  instance->base.create_local_datawriter_crypto_tokens = &create_local_datawriter_crypto_tokens;
  instance->base.set_remote_datawriter_crypto_tokens = &set_remote_datawriter_crypto_tokens;
  instance->base.create_local_datareader_crypto_tokens = &create_local_datareader_crypto_tokens;
  instance->base.set_remote_datareader_crypto_tokens = &set_remote_datareader_crypto_tokens;
  instance->base.return_crypto_tokens = &return_crypto_tokens;
  return (dds_security_crypto_key_exchange *)instance;
}

void dds_security_crypto_key_exchange__dealloc(
    dds_security_crypto_key_exchange *instance)
{
  ddsrt_free(instance);
}
