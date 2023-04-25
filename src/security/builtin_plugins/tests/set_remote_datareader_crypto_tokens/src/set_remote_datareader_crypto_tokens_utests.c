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
static DDS_Security_DatawriterCryptoHandle remote_reader_crypto = 0;
static DDS_Security_DatareaderCryptoHandle local_writer_crypto = 0;

static DDS_Security_SharedSecretHandleImpl *shared_secret_handle_impl = NULL;
static DDS_Security_SharedSecretHandle shared_secret_handle;

static void allocate_shared_secret(void)
{
  shared_secret_handle_impl = ddsrt_malloc(sizeof(DDS_Security_SharedSecretHandleImpl));

  shared_secret_handle_impl->shared_secret = ddsrt_malloc(TEST_SHARED_SECRET_SIZE * sizeof(unsigned char));
  shared_secret_handle_impl->shared_secret_size = TEST_SHARED_SECRET_SIZE;

  for (int i = 0; i < shared_secret_handle_impl->shared_secret_size; i++)
  {
    shared_secret_handle_impl->shared_secret[i] = (unsigned char)(i % 20);
  }
  for (int i = 0; i < 32; i++)
  {
    shared_secret_handle_impl->challenge1[i] = (unsigned char)(i % 15);
    shared_secret_handle_impl->challenge2[i] = (unsigned char)(i % 12);
  }

  shared_secret_handle = (DDS_Security_SharedSecretHandle)shared_secret_handle_impl;
}

static void
deallocate_shared_secret(void)
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

static int
register_local_participant(void)
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

static int
register_remote_participant(void)
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

static int
register_local_datawriter(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PropertySeq datawriter_properties;
  DDS_Security_EndpointSecurityAttributes datawriter_security_attributes;

  memset(&datawriter_properties, 0, sizeof(datawriter_properties));
  memset(&datawriter_security_attributes, 0, sizeof(DDS_Security_EndpointSecurityAttributes));

  datawriter_security_attributes.is_discovery_protected = true;
  datawriter_security_attributes.is_submessage_protected = true;

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

static int
register_remote_datareader(void)
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

static void
reset_exception(
    DDS_Security_SecurityException *ex)
{
  ex->code = 0;
  ex->minor_code = 0;
  ddsrt_free(ex->message);
  ex->message = NULL;
}

static void suite_set_remote_datareader_crypto_tokens_init(void)
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

static void suite_set_remote_datareader_crypto_tokens_fini(void)
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

static void create_key_material(DDS_Security_OctetSeq *seq, bool include_specific_key)
{
  DDS_Security_KeyMaterial_AES_GCM_GMAC keymat;
  DDS_Security_Serializer serializer;
  unsigned char *buffer;
  size_t length;

  memset(&keymat, 0, sizeof(keymat));

  keymat.transformation_kind[3] = CRYPTO_TRANSFORMATION_KIND_AES256_GCM;
  RAND_bytes(keymat.sender_key_id, 4);

  keymat.master_salt._length = keymat.master_salt._maximum = DDS_SECURITY_MASTER_SALT_SIZE_256;
  keymat.master_salt._buffer = DDS_Security_OctetSeq_allocbuf(DDS_SECURITY_MASTER_SALT_SIZE_256);
  RAND_bytes(keymat.master_salt._buffer, DDS_SECURITY_MASTER_SALT_SIZE_256);

  keymat.master_sender_key._length = keymat.master_sender_key._maximum = DDS_SECURITY_MASTER_SENDER_KEY_SIZE_256;
  keymat.master_sender_key._buffer = DDS_Security_OctetSeq_allocbuf(DDS_SECURITY_MASTER_SENDER_KEY_SIZE_256);
  RAND_bytes(keymat.master_sender_key._buffer, DDS_SECURITY_MASTER_SENDER_KEY_SIZE_256);

  if (include_specific_key)
  {
    RAND_bytes(keymat.receiver_specific_key_id, 4);
    keymat.master_receiver_specific_key._length = keymat.master_receiver_specific_key._maximum = DDS_SECURITY_MASTER_RECEIVER_SPECIFIC_KEY_SIZE_256;
    keymat.master_receiver_specific_key._buffer = DDS_Security_OctetSeq_allocbuf(DDS_SECURITY_MASTER_RECEIVER_SPECIFIC_KEY_SIZE_256);
    RAND_bytes(keymat.master_receiver_specific_key._buffer, DDS_SECURITY_MASTER_RECEIVER_SPECIFIC_KEY_SIZE_256);
  }

  serializer = DDS_Security_Serializer_new(256, 256);
  DDS_Security_Serialize_KeyMaterial_AES_GCM_GMAC(serializer, &keymat);
  DDS_Security_Serializer_buffer(serializer, &buffer, &length);
  DDS_Security_Serializer_free(serializer);

  DDS_Security_KeyMaterial_AES_GCM_GMAC_deinit(&keymat);

  seq->_length = seq->_maximum = (uint32_t)length;
  seq->_buffer = buffer;
}

static void init_key_material(DDS_Security_KeyMaterial_AES_GCM_GMAC *keymat, bool include_specific_key)
{
  memset(keymat, 0, sizeof(*keymat));

  keymat->transformation_kind[3] = CRYPTO_TRANSFORMATION_KIND_AES256_GCM;
  RAND_bytes(keymat->sender_key_id, 4);

  keymat->master_salt._length = keymat->master_salt._maximum = DDS_SECURITY_MASTER_SALT_SIZE_256;
  keymat->master_salt._buffer = DDS_Security_OctetSeq_allocbuf(DDS_SECURITY_MASTER_SALT_SIZE_256);
  RAND_bytes(keymat->master_salt._buffer, DDS_SECURITY_MASTER_SALT_SIZE_256);

  keymat->master_sender_key._length = keymat->master_sender_key._maximum = DDS_SECURITY_MASTER_SENDER_KEY_SIZE_256;
  keymat->master_sender_key._buffer = DDS_Security_OctetSeq_allocbuf(DDS_SECURITY_MASTER_SENDER_KEY_SIZE_256);
  RAND_bytes(keymat->master_sender_key._buffer, DDS_SECURITY_MASTER_SENDER_KEY_SIZE_256);

  if (include_specific_key)
  {
    RAND_bytes(keymat->receiver_specific_key_id, 4);
    keymat->master_receiver_specific_key._length = keymat->master_receiver_specific_key._maximum = DDS_SECURITY_MASTER_RECEIVER_SPECIFIC_KEY_SIZE_256;
    keymat->master_receiver_specific_key._buffer = DDS_Security_OctetSeq_allocbuf(DDS_SECURITY_MASTER_RECEIVER_SPECIFIC_KEY_SIZE_256);
    RAND_bytes(keymat->master_receiver_specific_key._buffer, DDS_SECURITY_MASTER_RECEIVER_SPECIFIC_KEY_SIZE_256);
  }
}

static void deinit_key_material(DDS_Security_KeyMaterial_AES_GCM_GMAC *keymat)
{
  ddsrt_free(keymat->master_salt._buffer);
  ddsrt_free(keymat->master_sender_key._buffer);
  ddsrt_free(keymat->master_receiver_specific_key._buffer);
}

static void create_reader_tokens(DDS_Security_DatawriterCryptoTokenSeq *tokens)
{
  tokens->_length = tokens->_maximum = 1;
  tokens->_buffer = DDS_Security_DataHolderSeq_allocbuf(1);
  tokens->_buffer[0].class_id = ddsrt_strdup(DDS_CRYPTOTOKEN_CLASS_ID);
  tokens->_buffer[0].binary_properties._length = 1;
  tokens->_buffer[0].binary_properties._maximum = 1;
  tokens->_buffer[0].binary_properties._buffer = DDS_Security_BinaryPropertySeq_allocbuf(1);
  tokens->_buffer[0].binary_properties._buffer[0].name = ddsrt_strdup(DDS_CRYPTOTOKEN_PROP_KEYMAT);
  create_key_material(&tokens->_buffer[0].binary_properties._buffer[0].value, false);
}

static void create_reader_tokens_no_key_material(DDS_Security_DatawriterCryptoTokenSeq *tokens)
{
  tokens->_length = tokens->_maximum = 1;
  tokens->_buffer = DDS_Security_DataHolderSeq_allocbuf(1);
  tokens->_buffer[0].class_id = ddsrt_strdup(DDS_CRYPTOTOKEN_CLASS_ID);
  tokens->_buffer[0].binary_properties._length = 1;
  tokens->_buffer[0].binary_properties._maximum = 1;
  tokens->_buffer[0].binary_properties._buffer = DDS_Security_BinaryPropertySeq_allocbuf(1);
  tokens->_buffer[0].binary_properties._buffer[0].name = ddsrt_strdup(DDS_CRYPTOTOKEN_PROP_KEYMAT);
}

static void serialize_key_material(DDS_Security_OctetSeq *seq, DDS_Security_KeyMaterial_AES_GCM_GMAC *keymat)
{
  DDS_Security_Serializer serializer;
  unsigned char *buffer;
  size_t length;

  serializer = DDS_Security_Serializer_new(256, 256);
  DDS_Security_Serialize_KeyMaterial_AES_GCM_GMAC(serializer, keymat);
  DDS_Security_Serializer_buffer(serializer, &buffer, &length);
  DDS_Security_Serializer_free(serializer);

  seq->_length = seq->_maximum = (uint32_t) length;
  seq->_buffer = buffer;
}

CU_Test(ddssec_builtin_set_remote_datareader_crypto_tokens, happy_day, .init = suite_set_remote_datareader_crypto_tokens_init, .fini = suite_set_remote_datareader_crypto_tokens_fini)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatawriterCryptoTokenSeq tokens;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange != NULL);
  assert(crypto->crypto_key_exchange != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens != NULL);
  assert(crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens != 0);

  memset(&tokens, 0, sizeof(tokens));

  create_reader_tokens(&tokens);

  /* Now call the function. */
  result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
      crypto->crypto_key_exchange,
      local_writer_crypto,
      remote_reader_crypto,
      &tokens,
      &exception);

  if (!result)
    printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  DDS_Security_DataHolderSeq_deinit(&tokens);

  CU_ASSERT_FATAL(result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);

  reset_exception(&exception);
}

CU_Test(ddssec_builtin_set_remote_datareader_crypto_tokens, single_token, .init = suite_set_remote_datareader_crypto_tokens_init, .fini = suite_set_remote_datareader_crypto_tokens_fini)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatawriterCryptoTokenSeq tokens;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange != NULL);
  assert(crypto->crypto_key_exchange != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens != NULL);
  assert(crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens != NULL);
  memset(&tokens, 0, sizeof(tokens));
  create_reader_tokens(&tokens);

  /* Now call the function. */
  result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
      crypto->crypto_key_exchange,
      local_writer_crypto,
      remote_reader_crypto,
      &tokens,
      &exception);

  if (!result)
    printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  DDS_Security_DataHolderSeq_deinit(&tokens);

  CU_ASSERT_FATAL(result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);

  reset_exception(&exception);
}

CU_Test(ddssec_builtin_set_remote_datareader_crypto_tokens, invalid_args, .init = suite_set_remote_datareader_crypto_tokens_init, .fini = suite_set_remote_datareader_crypto_tokens_fini)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatawriterCryptoTokenSeq tokens;

  /* Check if we actually have the validate_local_identity() function. */
  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange != NULL);
  assert(crypto->crypto_key_exchange != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens != NULL);
  assert(crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens != 0);

  memset(&tokens, 0, sizeof(tokens));

  create_reader_tokens(&tokens);

  /* invalid token seq = NULL */
  result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
      crypto->crypto_key_exchange,
      local_writer_crypto,
      remote_reader_crypto,
      NULL,
      &exception);

  if (!result)
    printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* invalid local_crypto_handle = DDS_SECURITY_HANDLE_NIL */
  result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
      crypto->crypto_key_exchange,
      0,
      remote_reader_crypto,
      &tokens,
      &exception);

  if (!result)
    printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);

  /* invalid remote_crypto_handle = DDS_SECURITY_HANDLE_NIL */
  result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
      crypto->crypto_key_exchange,
      local_writer_crypto,
      0,
      &tokens,
      &exception);

  if (!result)
    printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);

  /* invalid local_crypto_handle = 1 */
  result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
      crypto->crypto_key_exchange,
      1,
      remote_reader_crypto,
      &tokens,
      &exception);

  if (!result)
    printf("set_remote_datawriter_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);

  /* invalid remote_crypto_handle = 1 */
  result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
      crypto->crypto_key_exchange,
      local_writer_crypto,
      1,
      &tokens,
      &exception);

  if (!result)
    printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);

  DDS_Security_DataHolderSeq_deinit(&tokens);
}

CU_Test(ddssec_builtin_set_remote_datareader_crypto_tokens, invalid_tokens, .init = suite_set_remote_datareader_crypto_tokens_init, .fini = suite_set_remote_datareader_crypto_tokens_fini)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatawriterCryptoTokenSeq tokens;
  DDS_Security_DatawriterCryptoTokenSeq empty_tokens;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange != NULL);
  assert(crypto->crypto_key_exchange != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens != NULL);
  assert(crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens != 0);

  memset(&tokens, 0, sizeof(tokens));
  memset(&empty_tokens, 0, sizeof(empty_tokens));
  create_reader_tokens(&tokens);

  /* empty token sequence */
  {
    result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
        crypto->crypto_key_exchange,
        local_writer_crypto,
        remote_reader_crypto,
        &empty_tokens,
        &exception);

    if (!result)
      printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
  }

  /* DDS_Security_DatawriterCryptoTokenSeq with empty token */
  {
    empty_tokens._length = 1;
    empty_tokens._buffer = DDS_Security_DataHolderSeq_allocbuf(1);
    result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
        crypto->crypto_key_exchange,
        local_writer_crypto,
        remote_reader_crypto,
        &empty_tokens,
        &exception);

    if (!result)
      printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
    DDS_Security_DataHolderSeq_deinit(&empty_tokens);
  }

  /* invalid token class id */
  {
    ddsrt_free(tokens._buffer[0].class_id);
    tokens._buffer[0].class_id = ddsrt_strdup("invalid class");
    result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
        crypto->crypto_key_exchange,
        local_writer_crypto,
        remote_reader_crypto,
        &tokens,
        &exception);

    if (!result)
      printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
    ddsrt_free(tokens._buffer[0].class_id);
    tokens._buffer[0].class_id = ddsrt_strdup(DDS_CRYPTOTOKEN_CLASS_ID);
  }

  /* no key material, binary_property missing */
  {
    tokens._buffer[0].binary_properties._length = 0;
    result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
        crypto->crypto_key_exchange,
        local_writer_crypto,
        remote_reader_crypto,
        &tokens,
        &exception);

    if (!result)
      printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
    tokens._buffer[0].binary_properties._length = 1;
  }

  /* no key material, property is empty */
  {
    DDS_Security_BinaryProperty_t *saved_buffer = tokens._buffer[0].binary_properties._buffer;
    tokens._buffer[0].binary_properties._buffer = DDS_Security_BinaryPropertySeq_allocbuf(1);

    result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
        crypto->crypto_key_exchange,
        local_writer_crypto,
        remote_reader_crypto,
        &tokens,
        &exception);

    if (!result)
      printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
    ddsrt_free(tokens._buffer[0].binary_properties._buffer);
    tokens._buffer[0].binary_properties._buffer = saved_buffer;
  }

  /* invalid property name */
  {
    ddsrt_free(tokens._buffer[0].binary_properties._buffer[0].name);
    tokens._buffer[0].binary_properties._buffer[0].name = ddsrt_strdup("invalid_key_mat_name");

    result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
        crypto->crypto_key_exchange,
        local_writer_crypto,
        remote_reader_crypto,
        &tokens,
        &exception);

    if (!result)
      printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
    ddsrt_free(tokens._buffer[0].binary_properties._buffer[0].name);
    tokens._buffer[0].binary_properties._buffer[0].name = ddsrt_strdup(DDS_CRYPTOTOKEN_PROP_KEYMAT);
  }

  DDS_Security_DataHolderSeq_deinit(&tokens);
}

CU_Test(ddssec_builtin_set_remote_datareader_crypto_tokens, invalid_key_material, .init = suite_set_remote_datareader_crypto_tokens_init, .fini = suite_set_remote_datareader_crypto_tokens_fini)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatawriterCryptoTokenSeq tokens;
  DDS_Security_KeyMaterial_AES_GCM_GMAC keymat;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange != NULL);
  assert(crypto->crypto_key_exchange != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens != NULL);
  assert(crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens != 0);

  memset(&tokens, 0, sizeof(tokens));

  create_reader_tokens_no_key_material(&tokens);

  /* empty key material */
  {
    result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
        crypto->crypto_key_exchange,
        local_writer_crypto,
        remote_reader_crypto,
        &tokens,
        &exception);

    if (!result)
      printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
  }

  /* invalid transform kind */
  {
    init_key_material(&keymat, false);
    keymat.transformation_kind[2] = 1;
    serialize_key_material(&tokens._buffer[0].binary_properties._buffer[0].value, &keymat);

    result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
        crypto->crypto_key_exchange,
        local_writer_crypto,
        remote_reader_crypto,
        &tokens,
        &exception);

    if (!result)
      printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
    DDS_Security_OctetSeq_deinit(&tokens._buffer[0].binary_properties._buffer[0].value);
    deinit_key_material(&keymat);
  }

  /* no master salt */
  {
    init_key_material(&keymat, false);

    DDS_Security_OctetSeq_deinit(&keymat.master_salt);
    keymat.master_salt._buffer = NULL;
    keymat.master_salt._length = keymat.master_salt._maximum = 0;
    serialize_key_material(&tokens._buffer[0].binary_properties._buffer[0].value, &keymat);

    result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
        crypto->crypto_key_exchange,
        local_writer_crypto,
        remote_reader_crypto,
        &tokens,
        &exception);

    if (!result)
      printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
    DDS_Security_OctetSeq_deinit(&tokens._buffer[0].binary_properties._buffer[0].value);
    deinit_key_material(&keymat);
  }

  /* empty master salt */
  {
    init_key_material(&keymat, false);

    memset(keymat.master_salt._buffer, 0, keymat.master_salt._length);
    serialize_key_material(&tokens._buffer[0].binary_properties._buffer[0].value, &keymat);

    result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
        crypto->crypto_key_exchange,
        local_writer_crypto,
        remote_reader_crypto,
        &tokens,
        &exception);

    if (!result)
      printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
    DDS_Security_OctetSeq_deinit(&tokens._buffer[0].binary_properties._buffer[0].value);
    deinit_key_material(&keymat);
  }

  /* incorrect master salt */
  {
    init_key_material(&keymat, false);

    keymat.master_salt._length = 16;
    serialize_key_material(&tokens._buffer[0].binary_properties._buffer[0].value, &keymat);

    result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
        crypto->crypto_key_exchange,
        local_writer_crypto,
        remote_reader_crypto,
        &tokens,
        &exception);

    if (!result)
      printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
    DDS_Security_OctetSeq_deinit(&tokens._buffer[0].binary_properties._buffer[0].value);
    deinit_key_material(&keymat);
  }

  /* no master sender key */
  {
    init_key_material(&keymat, false);

    DDS_Security_OctetSeq_deinit(&keymat.master_sender_key);

    keymat.master_sender_key._buffer = NULL;
    keymat.master_sender_key._length = keymat.master_sender_key._maximum = 0;
    serialize_key_material(&tokens._buffer[0].binary_properties._buffer[0].value, &keymat);

    result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
        crypto->crypto_key_exchange,
        local_writer_crypto,
        remote_reader_crypto,
        &tokens,
        &exception);

    if (!result)
      printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
    DDS_Security_OctetSeq_deinit(&tokens._buffer[0].binary_properties._buffer[0].value);
    deinit_key_material(&keymat);
  }

  /* empty master sender key */
  {
    init_key_material(&keymat, false);

    memset(keymat.master_sender_key._buffer, 0, keymat.master_sender_key._length);
    serialize_key_material(&tokens._buffer[0].binary_properties._buffer[0].value, &keymat);

    result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
        crypto->crypto_key_exchange,
        local_writer_crypto,
        remote_reader_crypto,
        &tokens,
        &exception);

    if (!result)
      printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
    DDS_Security_OctetSeq_deinit(&tokens._buffer[0].binary_properties._buffer[0].value);
    deinit_key_material(&keymat);
  }

  /* incorrect master sender key */
  {
    init_key_material(&keymat, false);

    memset(keymat.master_sender_key._buffer, 0, keymat.master_sender_key._length);
    keymat.master_sender_key._length = 16;
    serialize_key_material(&tokens._buffer[0].binary_properties._buffer[0].value, &keymat);

    result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
        crypto->crypto_key_exchange,
        local_writer_crypto,
        remote_reader_crypto,
        &tokens,
        &exception);

    if (!result)
      printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
    DDS_Security_OctetSeq_deinit(&tokens._buffer[0].binary_properties._buffer[0].value);
    deinit_key_material(&keymat);
  }

  /* no master receiver specific key */
  {
    init_key_material(&keymat, true);

    DDS_Security_OctetSeq_deinit(&keymat.master_receiver_specific_key);

    keymat.master_receiver_specific_key._buffer = NULL;
    keymat.master_receiver_specific_key._length = keymat.master_receiver_specific_key._maximum = 0;
    serialize_key_material(&tokens._buffer[0].binary_properties._buffer[0].value, &keymat);

    result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
        crypto->crypto_key_exchange,
        local_writer_crypto,
        remote_reader_crypto,
        &tokens,
        &exception);

    if (!result)
      printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
    DDS_Security_OctetSeq_deinit(&tokens._buffer[0].binary_properties._buffer[0].value);
    deinit_key_material(&keymat);
  }

  /* nempty master receiver specific key */
  {
    init_key_material(&keymat, true);

    memset(keymat.master_receiver_specific_key._buffer, 0, keymat.master_receiver_specific_key._length);
    serialize_key_material(&tokens._buffer[0].binary_properties._buffer[0].value, &keymat);

    result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
        crypto->crypto_key_exchange,
        local_writer_crypto,
        remote_reader_crypto,
        &tokens,
        &exception);

    if (!result)
      printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
    DDS_Security_OctetSeq_deinit(&tokens._buffer[0].binary_properties._buffer[0].value);
    deinit_key_material(&keymat);
  }

  /* incorrect master receiver specific key */
  {
    init_key_material(&keymat, true);

    memset(keymat.master_receiver_specific_key._buffer, 0, keymat.master_receiver_specific_key._length);
    keymat.master_receiver_specific_key._length = 16;
    serialize_key_material(&tokens._buffer[0].binary_properties._buffer[0].value, &keymat);

    result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
        crypto->crypto_key_exchange,
        local_writer_crypto,
        remote_reader_crypto,
        &tokens,
        &exception);

    if (!result)
      printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
    DDS_Security_OctetSeq_deinit(&tokens._buffer[0].binary_properties._buffer[0].value);
    deinit_key_material(&keymat);
  }

  /*  invalid key material */
  {
    init_key_material(&keymat, true);

    memset(keymat.master_receiver_specific_key._buffer, 0, keymat.master_receiver_specific_key._length);
    serialize_key_material(&tokens._buffer[0].binary_properties._buffer[0].value, &keymat);

    RAND_bytes(tokens._buffer[0].binary_properties._buffer[0].value._buffer, (int) tokens._buffer[0].binary_properties._buffer[0].value._length);

    result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
        crypto->crypto_key_exchange,
        local_writer_crypto,
        remote_reader_crypto,
        &tokens,
        &exception);

    if (!result)
      printf("set_remote_datareader_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
    DDS_Security_OctetSeq_deinit(&tokens._buffer[0].binary_properties._buffer[0].value);
    deinit_key_material(&keymat);
  }

  DDS_Security_DataHolderSeq_deinit(&tokens);
}
