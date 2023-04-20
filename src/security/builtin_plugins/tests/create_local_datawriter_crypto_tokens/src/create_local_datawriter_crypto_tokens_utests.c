// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/types.h"
#include "dds/ddsrt/environ.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_serialize.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/core/dds_security_shared_secret.h"
#include "dds/security/openssl_support.h"
#include "CUnit/CUnit.h"
#include "CUnit/Test.h"
#include "common/src/loader.h"
#include "crypto_objects.h"
#include "crypto_tokens.h"

#define TEST_SHARED_SECRET_SIZE 32

#define CRYPTO_TRANSFORM_KIND(k) (*(uint32_t *)&((k)[0]))
#define CRYPTO_TRANSFORM_ID(k) (*(uint32_t *)&((k)[0]))

static struct plugins_hdl *plugins = NULL;
static dds_security_cryptography *crypto = NULL;

static DDS_Security_IdentityHandle local_participant_identity = 1;
static DDS_Security_IdentityHandle remote_participant_identity = 2;

static DDS_Security_ParticipantCryptoHandle local_particpant_crypto = 0;
static DDS_Security_ParticipantCryptoHandle remote_particpant_crypto = 0;
static DDS_Security_DatawriterCryptoHandle local_writer_crypto = 0;
static DDS_Security_DatareaderCryptoHandle remote_reader_crypto = 0;

static DDS_Security_SharedSecretHandleImpl *shared_secret_handle_impl = NULL;
static DDS_Security_SharedSecretHandle shared_secret_handle;

static void allocate_shared_secret(void)
{
  int32_t i;

  shared_secret_handle_impl = ddsrt_malloc(sizeof(DDS_Security_SharedSecretHandleImpl));

  shared_secret_handle_impl->shared_secret = ddsrt_malloc(TEST_SHARED_SECRET_SIZE * sizeof(unsigned char));
  shared_secret_handle_impl->shared_secret_size = TEST_SHARED_SECRET_SIZE;

  for (i = 0; i < shared_secret_handle_impl->shared_secret_size; i++)
  {
    shared_secret_handle_impl->shared_secret[i] = (unsigned char)(i % 20);
  }

  for (i = 0; i < 32; i++)
  {
    shared_secret_handle_impl->challenge1[i] = (unsigned char)(i % 15);
    shared_secret_handle_impl->challenge2[i] = (unsigned char)(i % 12);
  }

  shared_secret_handle = (DDS_Security_SharedSecretHandle)shared_secret_handle_impl;
}

static void deallocate_shared_secret(void)
{
  ddsrt_free(shared_secret_handle_impl->shared_secret);
  ddsrt_free(shared_secret_handle_impl);
}

static void prepare_participant_security_attributes(DDS_Security_ParticipantSecurityAttributes *attributes)
{
  memset(attributes, 0, sizeof(DDS_Security_ParticipantSecurityAttributes));
  attributes->allow_unauthenticated_participants = false;
  attributes->is_access_protected = false;
  attributes->is_discovery_protected = false;
  attributes->is_liveliness_protected = false;
  attributes->is_rtps_protected = true;
  attributes->plugin_participant_attributes = DDS_SECURITY_PARTICIPANT_ATTRIBUTES_FLAG_IS_VALID;
  attributes->plugin_participant_attributes |= DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_ENCRYPTED;
}

static int register_local_participant(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PermissionsHandle participant_permissions = 3; //valid dummy value
  DDS_Security_PropertySeq participant_properties;
  DDS_Security_ParticipantSecurityAttributes participant_security_attributes;

  memset(&participant_properties, 0, sizeof(participant_properties));
  prepare_participant_security_attributes(&participant_security_attributes);

  local_particpant_crypto =
      crypto->crypto_key_factory->register_local_participant(
          crypto->crypto_key_factory,
          local_participant_identity,
          participant_permissions,
          &participant_properties,
          &participant_security_attributes,
          &exception);

  if (local_particpant_crypto == 0)
  {
    printf("register_local_participant: %s\n", exception.message ? exception.message : "Error message missing");
  }

  return local_particpant_crypto ? 0 : -1;
}

static int register_remote_participant(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PermissionsHandle remote_participant_permissions = 5;

  remote_particpant_crypto =
      crypto->crypto_key_factory->register_matched_remote_participant(
          crypto->crypto_key_factory,
          local_particpant_crypto,
          remote_participant_identity,
          remote_participant_permissions,
          shared_secret_handle,
          &exception);

  if (remote_particpant_crypto == 0)
  {
    printf("register_matched_remote_participant: %s\n", exception.message ? exception.message : "Error message missing");
  }

  return remote_particpant_crypto ? 0 : -1;
}

static void prepare_endpoint_security_attributes(DDS_Security_EndpointSecurityAttributes *attributes)
{
  memset(attributes, 0, sizeof(DDS_Security_EndpointSecurityAttributes));
  attributes->is_discovery_protected = true;
  attributes->is_submessage_protected = true;

  attributes->plugin_endpoint_attributes |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
}

static int register_local_datawriter(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PropertySeq datawriter_properties;
  DDS_Security_EndpointSecurityAttributes datawriter_security_attributes;

  memset(&datawriter_properties, 0, sizeof(datawriter_properties));
  prepare_endpoint_security_attributes(&datawriter_security_attributes);
  datawriter_security_attributes.is_payload_protected = true;
  datawriter_security_attributes.plugin_endpoint_attributes |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_PAYLOAD_ENCRYPTED;

  local_writer_crypto =
      crypto->crypto_key_factory->register_local_datawriter(
          crypto->crypto_key_factory,
          local_particpant_crypto,
          &datawriter_properties,
          &datawriter_security_attributes,
          &exception);

  if (local_writer_crypto == 0)
  {
    printf("register_local_datawriter: %s\n", exception.message ? exception.message : "Error message missing");
  }

  return local_writer_crypto ? 0 : -1;
}

static int register_remote_datareader(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};

  remote_reader_crypto =
      crypto->crypto_key_factory->register_matched_remote_datareader(
          crypto->crypto_key_factory,
          local_writer_crypto,
          remote_particpant_crypto,
          shared_secret_handle,
          true,
          &exception);

  if (remote_reader_crypto == 0)
  {
    printf("register_matched_remote_datareader: %s\n", exception.message ? exception.message : "Error message missing");
  }

  return remote_reader_crypto ? 0 : -1;
}

static void reset_exception(DDS_Security_SecurityException *ex)
{
  ex->code = 0;
  ex->minor_code = 0;
  ddsrt_free(ex->message);
  ex->message = NULL;
}

static void suite_create_local_datawriter_crypto_tokens_init(void)
{
  allocate_shared_secret();
  CU_ASSERT_FATAL ((plugins = load_plugins(
                      NULL    /* Access Control */,
                      NULL    /* Authentication */,
                      &crypto /* Cryptograpy    */,
                      NULL)) != NULL);
  CU_ASSERT_EQUAL_FATAL (register_local_participant(), 0);
  CU_ASSERT_EQUAL_FATAL (register_remote_participant(), 0);
  CU_ASSERT_EQUAL_FATAL (register_local_datawriter(), 0);
  CU_ASSERT_EQUAL_FATAL (register_remote_datareader(), 0);
}

static void suite_create_local_datawriter_crypto_tokens_fini(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};

  if (remote_reader_crypto)
  {
    crypto->crypto_key_factory->unregister_datareader(crypto->crypto_key_factory, remote_reader_crypto, &exception);
    reset_exception(&exception);
  }
  if (local_writer_crypto)
  {
    crypto->crypto_key_factory->unregister_datawriter(crypto->crypto_key_factory, local_writer_crypto, &exception);
    reset_exception(&exception);
  }
  if (remote_particpant_crypto)
  {
    crypto->crypto_key_factory->unregister_participant(crypto->crypto_key_factory, remote_particpant_crypto, &exception);
    reset_exception(&exception);
  }
  if (local_particpant_crypto)
  {
    crypto->crypto_key_factory->unregister_datareader(crypto->crypto_key_factory, local_particpant_crypto, &exception);
    reset_exception(&exception);
  }
  deallocate_shared_secret();
  unload_plugins(plugins);
}

static bool data_not_empty(unsigned char *data, uint32_t length)
{
  uint32_t i;
  for (i = 0; i < length; i++)
  {
    if (data[i])
      return true;
  }
  return false;
}

static bool check_key_material(DDS_Security_OctetSeq *data)
{
  bool status = true;
  DDS_Security_Deserializer deserializer;
  DDS_Security_KeyMaterial_AES_GCM_GMAC key_mat;

  deserializer = DDS_Security_Deserializer_new(data->_buffer, data->_length);
  if (DDS_Security_Deserialize_KeyMaterial_AES_GCM_GMAC(deserializer, &key_mat))
  {
    if (CRYPTO_TRANSFORM_KIND(key_mat.transformation_kind) == CRYPTO_TRANSFORMATION_KIND_AES256_GCM)
    {
      printf("check_key_material: incorrect transformation_kind\n");
      status = false;
    }
    else if (CRYPTO_TRANSFORM_ID(key_mat.sender_key_id) == 0)
    {
      printf("check_key_material: incorrect sender_key_id\n");
      status = false;
    }
    else if (key_mat.master_salt._length != DDS_SECURITY_MASTER_SALT_SIZE_256)
    {
      printf("check_key_material: incorrect master_salt\n");
      status = false;
    }
    else if (!key_mat.master_salt._buffer)
    {
      printf("check_key_material: incorrect master_salt\n");
      status = false;
    }
    else if (!data_not_empty(key_mat.master_salt._buffer, key_mat.master_salt._length))
    {
      printf("check_key_material: incorrect master_salt\n");
      status = false;
    }
    else if (key_mat.master_sender_key._length != DDS_SECURITY_MASTER_SENDER_KEY_SIZE_256)
    {
      printf("check_key_material: incorrect master_sender_key\n");
      status = false;
    }
    else if (!key_mat.master_salt._buffer)
    {
      printf("check_key_material: incorrect master_sender_key\n");
      status = false;
    }
    else if (!data_not_empty(key_mat.master_sender_key._buffer, key_mat.master_sender_key._length))
    {
      printf("check_key_material: incorrect master_sender_key\n");
      status = false;
    }
  }
  else
  {
    status = false;
  }

  DDS_Security_Deserializer_free(deserializer);
  DDS_Security_KeyMaterial_AES_GCM_GMAC_deinit(&key_mat);

  return status;
}

static bool check_token_validity(const DDS_Security_DatawriterCryptoTokenSeq *tokens)
{
  bool status = true;
  uint32_t i;

  if (tokens->_length != 2 || tokens->_buffer == NULL)
  {
    status = false;
  }

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

    if (status)
    {
      status = check_key_material(&tokens->_buffer[i].binary_properties._buffer[0].value);
    }
  }

  return status;
}

CU_Test(ddssec_builtin_create_local_datawriter_crypto_tokens, happy_day, .init = suite_create_local_datawriter_crypto_tokens_init, .fini = suite_create_local_datawriter_crypto_tokens_fini)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatawriterCryptoTokenSeq tokens;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange != NULL);
  assert(crypto->crypto_key_exchange != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange->create_local_datawriter_crypto_tokens != NULL);
  assert(crypto->crypto_key_exchange->create_local_datawriter_crypto_tokens != 0);

  memset(&tokens, 0, sizeof(tokens));

  /* Now call the function. */
  result = crypto->crypto_key_exchange->create_local_datawriter_crypto_tokens(
      crypto->crypto_key_exchange,
      &tokens,
      local_writer_crypto,
      remote_reader_crypto,
      &exception);

  if (!result)
  {
    printf("create_local_datawriter_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT_FATAL(result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);

  reset_exception(&exception);

  CU_ASSERT(check_token_validity(&tokens));

  result = crypto->crypto_key_exchange->return_crypto_tokens(crypto->crypto_key_exchange, &tokens, &exception);

  if (!result)
  {
    printf("return_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT_FATAL(result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);

  reset_exception(&exception);
}

CU_Test(ddssec_builtin_create_local_datawriter_crypto_tokens, invalid_args, .init = suite_create_local_datawriter_crypto_tokens_init, .fini = suite_create_local_datawriter_crypto_tokens_fini)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatawriterCryptoTokenSeq tokens;

  /* Check if we actually have the validate_local_identity() function. */
  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange != NULL);
  assert(crypto->crypto_key_exchange != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange->create_local_datawriter_crypto_tokens != NULL);
  assert(crypto->crypto_key_exchange->create_local_datawriter_crypto_tokens != 0);

  memset(&tokens, 0, sizeof(tokens));

  /* invalid token seq = NULL */
  result = crypto->crypto_key_exchange->create_local_datawriter_crypto_tokens(
      crypto->crypto_key_exchange,
      NULL,
      local_writer_crypto,
      remote_reader_crypto,
      &exception);

  if (!result)
  {
    printf("create_local_datawriter_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* invalid local_crypto_handle = DDS_SECURITY_HANDLE_NIL */
  result = crypto->crypto_key_exchange->create_local_datawriter_crypto_tokens(
      crypto->crypto_key_exchange,
      &tokens,
      0,
      remote_reader_crypto,
      &exception);

  if (!result)
  {
    printf("create_local_datawriter_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* invalid remote_crypto_handle = DDS_SECURITY_HANDLE_NIL */
  result = crypto->crypto_key_exchange->create_local_datawriter_crypto_tokens(
      crypto->crypto_key_exchange,
      &tokens,
      local_writer_crypto,
      0,
      &exception);

  if (!result)
  {
    printf("create_local_datawriter_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* invalid local_crypto_handle = 1 */
  result = crypto->crypto_key_exchange->create_local_datawriter_crypto_tokens(
      crypto->crypto_key_exchange,
      &tokens,
      1,
      remote_reader_crypto,
      &exception);

  if (!result)
  {
    printf("create_local_datawriter_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* invalid remote_crypto_handle = 1 */
  result = crypto->crypto_key_exchange->create_local_datawriter_crypto_tokens(
      crypto->crypto_key_exchange,
      &tokens,
      local_writer_crypto,
      1,
      &exception);

  if (!result)
  {
    printf("create_local_datawriter_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);
}

