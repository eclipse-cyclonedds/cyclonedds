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

#include "dds/ddsrt/bswap.h"
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
#include "crypto_utils.h"

#define TEST_SHARED_SECRET_SIZE 32

static struct plugins_hdl *plugins = NULL;
static dds_security_cryptography *crypto = NULL;

static DDS_Security_IdentityHandle local_participant_identity = 1;
static DDS_Security_IdentityHandle remote_participant_identity = 2;

static DDS_Security_ParticipantCryptoHandle local_participant_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_ParticipantCryptoHandle remote_participant_handle = DDS_SECURITY_HANDLE_NIL;

static DDS_Security_SharedSecretHandle shared_secret_handle = DDS_SECURITY_HANDLE_NIL;

static const char *sample_test_data =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxy";

struct crypto_header
{
  struct CryptoTransformIdentifier transform_identifier;
  unsigned char session_id[4];
  unsigned char init_vector_suffix[8];
};

struct crypto_footer
{
  unsigned char common_mac[16];
  unsigned char length[4];
};

static void reset_exception(DDS_Security_SecurityException *ex)
{
  ex->code = 0;
  ex->minor_code = 0;
  ddsrt_free(ex->message);
  ex->message = NULL;
}

static void allocate_shared_secret(void)
{
  DDS_Security_SharedSecretHandleImpl *shared_secret_handle_impl;
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
  DDS_Security_SharedSecretHandleImpl *shared_secret_handle_impl = (DDS_Security_SharedSecretHandleImpl *)shared_secret_handle;
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

  local_participant_handle =
      crypto->crypto_key_factory->register_local_participant(
          crypto->crypto_key_factory,
          local_participant_identity,
          participant_permissions,
          &participant_properties,
          &participant_security_attributes,
          &exception);

  if (local_participant_handle == DDS_SECURITY_HANDLE_NIL)
  {
    printf("register_local_participant: %s\n", exception.message ? exception.message : "Error message missing");
  }

  return local_participant_handle ? 0 : -1;
}

static void unregister_local_participant(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  if (local_participant_handle)
  {
    crypto->crypto_key_factory->unregister_participant(crypto->crypto_key_factory, local_participant_handle, &exception);
    reset_exception(&exception);
  }
}

static int register_remote_participant(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PermissionsHandle remote_participant_permissions = 5;

  remote_participant_handle =
      crypto->crypto_key_factory->register_matched_remote_participant(
          crypto->crypto_key_factory,
          local_participant_handle,
          remote_participant_identity,
          remote_participant_permissions,
          shared_secret_handle,
          &exception);

  if (remote_participant_handle == DDS_SECURITY_HANDLE_NIL)
  {
    printf("register_matched_remote_participant: %s\n", exception.message ? exception.message : "Error message missing");
  }

  return remote_participant_handle ? 0 : -1;
}

static void unregister_remote_participant(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  if (remote_participant_handle)
  {
    crypto->crypto_key_factory->unregister_participant(crypto->crypto_key_factory, remote_participant_handle, &exception);
    reset_exception(&exception);
  }
}

static void prepare_endpoint_security_attributes(DDS_Security_EndpointSecurityAttributes *attributes)
{
  memset(attributes, 0, sizeof(DDS_Security_EndpointSecurityAttributes));
  attributes->is_discovery_protected = true;
  attributes->is_submessage_protected = true;

  attributes->plugin_endpoint_attributes |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
}

static DDS_Security_DatawriterCryptoHandle register_local_datawriter(bool encrypted)
{
  DDS_Security_DatawriterCryptoHandle writer_crypto;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PropertySeq datawriter_properties;
  DDS_Security_EndpointSecurityAttributes datawriter_security_attributes;

  memset(&datawriter_properties, 0, sizeof(datawriter_properties));
  prepare_endpoint_security_attributes(&datawriter_security_attributes);

  datawriter_security_attributes.is_payload_protected = true;
  if (encrypted)
  {
    datawriter_security_attributes.plugin_endpoint_attributes |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_PAYLOAD_ENCRYPTED;
  }

  writer_crypto =
      crypto->crypto_key_factory->register_local_datawriter(
          crypto->crypto_key_factory,
          local_participant_handle,
          &datawriter_properties,
          &datawriter_security_attributes,
          &exception);

  if (writer_crypto == 0)
  {
    printf("register_local_datawriter: %s\n", exception.message ? exception.message : "Error message missing");
  }

  assert (writer_crypto != 0);
  return writer_crypto;
}

static DDS_Security_DatawriterCryptoHandle register_remote_datawriter(DDS_Security_DatareaderCryptoHandle reader_crypto)
{
  DDS_Security_DatawriterCryptoHandle writer_crypto;
  DDS_Security_SecurityException exception = {NULL, 0, 0};

  writer_crypto =
      crypto->crypto_key_factory->register_matched_remote_datawriter(
          crypto->crypto_key_factory,
          reader_crypto,
          remote_participant_handle,
          shared_secret_handle,
          &exception);

  if (writer_crypto == 0)
  {
    printf("register_matched_remote_datareader: %s\n", exception.message ? exception.message : "Error message missing");
  }

  assert (writer_crypto != 0);
  return writer_crypto;
}

static void unregister_datawriter(DDS_Security_DatawriterCryptoHandle writer_crypto)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  if (writer_crypto)
  {
    crypto->crypto_key_factory->unregister_datawriter(crypto->crypto_key_factory, writer_crypto, &exception);
    reset_exception(&exception);
  }
}

static DDS_Security_DatareaderCryptoHandle register_local_datareader(bool encrypted)
{
  DDS_Security_DatareaderCryptoHandle reader_crypto;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PropertySeq datareader_properties;
  DDS_Security_EndpointSecurityAttributes datareader_security_attributes;

  memset(&datareader_properties, 0, sizeof(datareader_properties));
  memset(&datareader_security_attributes, 0, sizeof(datareader_security_attributes));
  datareader_security_attributes.is_payload_protected = true;
  if (encrypted)
  {
    datareader_security_attributes.plugin_endpoint_attributes |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_PAYLOAD_ENCRYPTED;
  }

  reader_crypto =
      crypto->crypto_key_factory->register_local_datareader(
          crypto->crypto_key_factory,
          local_participant_handle,
          &datareader_properties,
          &datareader_security_attributes,
          &exception);

  if (reader_crypto == 0)
  {
    printf("register_local_datawriter: %s\n", exception.message ? exception.message : "Error message missing");
  }

  return reader_crypto;
}

static DDS_Security_DatareaderCryptoHandle register_remote_datareader(DDS_Security_DatawriterCryptoHandle writer_crypto)
{
  DDS_Security_DatareaderCryptoHandle reader_crypto;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PropertySeq datawriter_properties;

  memset(&datawriter_properties, 0, sizeof(datawriter_properties));

  reader_crypto =
      crypto->crypto_key_factory->register_matched_remote_datareader(
          crypto->crypto_key_factory,
          writer_crypto,
          remote_participant_handle,
          shared_secret_handle,
          true,
          &exception);

  if (reader_crypto == 0)
  {
    printf("register_matched_remote_datareader: %s\n", exception.message ? exception.message : "Error message missing");
  }

  return reader_crypto;
}

static void unregister_datareader(DDS_Security_DatareaderCryptoHandle reader_crypto)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  if (reader_crypto)
  {
    crypto->crypto_key_factory->unregister_datareader(crypto->crypto_key_factory, reader_crypto, &exception);
    reset_exception(&exception);
  }
}

static bool
set_remote_datawriter_tokens(
    DDS_Security_DatawriterCryptoHandle local_writer_crypto,
    DDS_Security_DatareaderCryptoHandle remote_reader_crypto,
    DDS_Security_DatareaderCryptoHandle local_reader_crypto,
    DDS_Security_DatawriterCryptoHandle remote_writer_crypto)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatawriterCryptoTokenSeq tokens;

  memset(&tokens, 0, sizeof(tokens));

  /* Now call the function. */
  result = crypto->crypto_key_exchange->create_local_datawriter_crypto_tokens(
      crypto->crypto_key_exchange,
      &tokens,
      local_writer_crypto,
      remote_reader_crypto,
      &exception);

  if (result)
  {
    result = crypto->crypto_key_exchange->set_remote_datawriter_crypto_tokens(
        crypto->crypto_key_exchange,
        local_reader_crypto,
        remote_writer_crypto,
        &tokens,
        &exception);

    (void)crypto->crypto_key_exchange->return_crypto_tokens(
        crypto->crypto_key_exchange,
        &tokens,
        &exception);
  }

  return (bool)result;
}

static session_key_material * get_datawriter_session(DDS_Security_DatawriterCryptoHandle writer_crypto)
{
  local_datawriter_crypto *writer_crypto_impl = (local_datawriter_crypto *)writer_crypto;

  return writer_crypto_impl->writer_session_message;
}

static bool check_writer_protection_kind(DDS_Security_DatawriterCryptoHandle writer_crypto, DDS_Security_BasicProtectionKind protection_kind)
{
  local_datawriter_crypto *writer_crypto_impl = (local_datawriter_crypto *)writer_crypto;
  return (writer_crypto_impl->data_protectionKind == protection_kind);
}

static uint32_t get_transformation_kind(uint32_t key_size, bool encoded)
{
  uint32_t kind = CRYPTO_TRANSFORMATION_KIND_INVALID;
  if (key_size == 128)
  {
    kind = encoded ? CRYPTO_TRANSFORMATION_KIND_AES128_GCM : CRYPTO_TRANSFORMATION_KIND_AES128_GMAC;
  }
  else if (key_size == 256)
  {
    kind = encoded ? CRYPTO_TRANSFORMATION_KIND_AES256_GCM : CRYPTO_TRANSFORMATION_KIND_AES256_GMAC;
  }
  CU_ASSERT_FATAL(kind != CRYPTO_TRANSFORMATION_KIND_INVALID);
  return kind;
}

static void suite_decode_serialized_payload_init(void)
{
  allocate_shared_secret();

  CU_ASSERT_FATAL ((plugins = load_plugins(
                      NULL    /* Access Control */,
                      NULL    /* Authentication */,
                      &crypto /* Cryptograpy    */,
                      NULL)) != NULL);
  CU_ASSERT_EQUAL_FATAL (register_local_participant(), 0);
  CU_ASSERT_EQUAL_FATAL (register_remote_participant(), 0);
}

static void suite_decode_serialized_payload_fini(void)
{
  unregister_remote_participant();
  unregister_local_participant();
  unload_plugins(plugins);
  deallocate_shared_secret();
}

static bool split_encoded_data(unsigned char *data, size_t size, struct crypto_header **header, unsigned char **contents, size_t *length, struct crypto_footer **footer)
{
  unsigned char *ptr;

  if (size < sizeof(struct crypto_header) + 4)
    return false;

  *header = (struct crypto_header *)data;
  ptr = data + sizeof(struct crypto_header);
  *length = ddsrt_fromBE4u(*(uint32_t *)ptr);

  size -= sizeof(struct crypto_header) + 4;

  /* remain should contain the ecrypted data + the footer (common_mac (16) + length (4)) */
  if (size < (*length) + 20)
    return false;

  ptr += 4;
  *contents = ptr;
  ptr += *length;

  /* The length is the length of the encrypted data and the common_mac of the footer
     * For the serialized payload the footer consists of the common_mac and the length
     * of the receiver_specific_mac which is set to 0
     */
  *footer = (struct crypto_footer *)ptr;

  return true;
}

static void decode_serialized_payload_check(uint32_t key_size, bool encrypted)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatawriterCryptoHandle local_writer_crypto;
  DDS_Security_DatareaderCryptoHandle local_reader_crypto;
  DDS_Security_DatawriterCryptoHandle remote_writer_crypto;
  DDS_Security_DatareaderCryptoHandle remote_reader_crypto;
  DDS_Security_OctetSeq encoded_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq decoded_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq extra_inline_qos;
  DDS_Security_OctetSeq plain_buffer;
  session_key_material *session_keys;
  size_t length;

  CU_ASSERT_FATAL(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->encode_serialized_payload != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->decode_serialized_payload != NULL);

  memset(&extra_inline_qos, 0, sizeof(extra_inline_qos));

  length = strlen(sample_test_data) + 1;
  plain_buffer._length = plain_buffer._maximum = (uint32_t) length;
  plain_buffer._buffer = DDS_Security_OctetSeq_allocbuf((uint32_t)length);
  memcpy((char *)plain_buffer._buffer, sample_test_data, length);

  local_writer_crypto = register_local_datawriter(encrypted);
  CU_ASSERT_FATAL(local_writer_crypto != 0);
  CU_ASSERT(check_writer_protection_kind(local_writer_crypto, encrypted ? DDS_SECURITY_BASICPROTECTION_KIND_ENCRYPT : DDS_SECURITY_BASICPROTECTION_KIND_SIGN));

  session_keys = get_datawriter_session(local_writer_crypto);
  session_keys->master_key_material->transformation_kind = get_transformation_kind(key_size, encrypted);
  session_keys->key_size = key_size;

  local_reader_crypto = register_local_datareader(encrypted);
  CU_ASSERT_FATAL(local_reader_crypto != 0);

  remote_reader_crypto = register_remote_datareader(local_writer_crypto);
  CU_ASSERT_FATAL(remote_reader_crypto != 0);

  remote_writer_crypto = register_remote_datawriter(local_reader_crypto);
  CU_ASSERT_FATAL(remote_writer_crypto != 0);

  result = set_remote_datawriter_tokens(local_writer_crypto, remote_reader_crypto, local_reader_crypto, remote_writer_crypto);
  CU_ASSERT_FATAL(result);

  /* Encrypt the data. */
  result = crypto->crypto_transform->encode_serialized_payload(
      crypto->crypto_transform,
      &encoded_buffer,
      &extra_inline_qos,
      &plain_buffer,
      local_writer_crypto,
      &exception);

  if (!result)
  {
    printf("encode_serialized_payload: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT_FATAL(result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);

  reset_exception(&exception);

  /* Decrypt the data */
  result = crypto->crypto_transform->decode_serialized_payload(
      crypto->crypto_transform,
      &decoded_buffer,
      &encoded_buffer,
      &extra_inline_qos,
      local_reader_crypto,
      remote_writer_crypto,
      &exception);

  if (!result)
  {
    printf("decode_serialized_payload: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT_FATAL(result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);

  unregister_datareader(remote_reader_crypto);
  unregister_datawriter(remote_writer_crypto);
  unregister_datareader(local_reader_crypto);
  unregister_datawriter(local_writer_crypto);

  DDS_Security_OctetSeq_deinit(&encoded_buffer);
  DDS_Security_OctetSeq_deinit(&decoded_buffer);
  DDS_Security_OctetSeq_deinit(&plain_buffer);

  reset_exception(&exception);
}

CU_Test(ddssec_builtin_decode_serialized_payload, decrypt_128, .init = suite_decode_serialized_payload_init, .fini = suite_decode_serialized_payload_fini)
{
  decode_serialized_payload_check(128, true);
}

CU_Test(ddssec_builtin_decode_serialized_payload, decrypt_256, .init = suite_decode_serialized_payload_init, .fini = suite_decode_serialized_payload_fini)
{
  decode_serialized_payload_check(256, true);
}

CU_Test(ddssec_builtin_decode_serialized_payload, signcheck_128, .init = suite_decode_serialized_payload_init, .fini = suite_decode_serialized_payload_fini)
{
  decode_serialized_payload_check(128, false);
}

CU_Test(ddssec_builtin_decode_serialized_payload, signcheck_256, .init = suite_decode_serialized_payload_init, .fini = suite_decode_serialized_payload_fini)
{
  decode_serialized_payload_check(256, false);
}

CU_Test(ddssec_builtin_decode_serialized_payload, invalid_args, .init = suite_decode_serialized_payload_init, .fini = suite_decode_serialized_payload_fini)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatawriterCryptoHandle local_writer_crypto;
  DDS_Security_DatareaderCryptoHandle local_reader_crypto;
  DDS_Security_DatawriterCryptoHandle remote_writer_crypto;
  DDS_Security_DatareaderCryptoHandle remote_reader_crypto;
  DDS_Security_OctetSeq encoded_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq decoded_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq extra_inline_qos;
  DDS_Security_OctetSeq plain_buffer;
  DDS_Security_OctetSeq empty_buffer;
  session_key_material *session_keys;
  size_t length;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform != NULL);
  assert(crypto->crypto_transform != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->encode_serialized_payload != NULL);
  assert(crypto->crypto_transform->encode_serialized_payload != 0);

  memset(&extra_inline_qos, 0, sizeof(extra_inline_qos));
  memset(&empty_buffer, 0, sizeof(empty_buffer));

  length = strlen(sample_test_data) + 1;
  plain_buffer._length = plain_buffer._maximum = (uint32_t)length;
  plain_buffer._buffer = DDS_Security_OctetSeq_allocbuf((uint32_t)length);
  memcpy((char *)plain_buffer._buffer, sample_test_data, length);

  local_writer_crypto = register_local_datawriter(true);
  CU_ASSERT_FATAL(local_writer_crypto != 0);

  session_keys = get_datawriter_session(local_writer_crypto);
  session_keys->master_key_material->transformation_kind = CRYPTO_TRANSFORMATION_KIND_AES256_GCM;
  session_keys->key_size = 256;

  local_reader_crypto = register_local_datareader(true);
  CU_ASSERT_FATAL(local_reader_crypto != 0);

  remote_reader_crypto = register_remote_datareader(local_writer_crypto);
  CU_ASSERT_FATAL(remote_reader_crypto != 0);

  remote_writer_crypto = register_remote_datawriter(local_reader_crypto);
  CU_ASSERT_FATAL(remote_writer_crypto != 0);

  result = set_remote_datawriter_tokens(local_writer_crypto, remote_reader_crypto, local_reader_crypto, remote_writer_crypto);
  CU_ASSERT_FATAL(result);

  /* encrypt the data */
  result = crypto->crypto_transform->encode_serialized_payload(
      crypto->crypto_transform,
      &encoded_buffer,
      &extra_inline_qos,
      &plain_buffer,
      local_writer_crypto,
      &exception);

  if (!result)
  {
    printf("encode_serialized_payload: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT_FATAL(result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);

  reset_exception(&exception);

  /* unknown local reader crypto handle specified */
  result = crypto->crypto_transform->decode_serialized_payload(
      crypto->crypto_transform,
      &decoded_buffer,
      &encoded_buffer,
      &extra_inline_qos,
      0,
      remote_writer_crypto,
      &exception);

  if (!result)
  {
    printf("decode_serialized_payload: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* invalid local reader crypto handle specified */
  result = crypto->crypto_transform->decode_serialized_payload(
      crypto->crypto_transform,
      &decoded_buffer,
      &encoded_buffer,
      &extra_inline_qos,
      remote_writer_crypto,
      remote_writer_crypto,
      &exception);

  if (!result)
  {
    printf("decode_serialized_payload: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* unknown remote writer crypto handle specified */
  result = crypto->crypto_transform->decode_serialized_payload(
      crypto->crypto_transform,
      &decoded_buffer,
      &encoded_buffer,
      &extra_inline_qos,
      local_reader_crypto,
      0,
      &exception);

  if (!result)
  {
    printf("decode_serialized_payload: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* invalid remote writer crypto handle specified */
  result = crypto->crypto_transform->decode_serialized_payload(
      crypto->crypto_transform,
      &decoded_buffer,
      &encoded_buffer,
      &extra_inline_qos,
      local_reader_crypto,
      local_reader_crypto,
      &exception);

  if (!result)
  {
    printf("decode_serialized_payload: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  unregister_datareader(remote_reader_crypto);
  unregister_datawriter(remote_writer_crypto);
  unregister_datareader(local_reader_crypto);
  unregister_datawriter(local_writer_crypto);

  DDS_Security_OctetSeq_deinit(&encoded_buffer);
  DDS_Security_OctetSeq_deinit(&plain_buffer);

  reset_exception(&exception);
}

CU_Test(ddssec_builtin_decode_serialized_payload, invalid_data, .init = suite_decode_serialized_payload_init, .fini = suite_decode_serialized_payload_fini)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatawriterCryptoHandle local_writer_crypto;
  DDS_Security_DatareaderCryptoHandle local_reader_crypto;
  DDS_Security_DatawriterCryptoHandle remote_writer_crypto;
  DDS_Security_DatareaderCryptoHandle remote_reader_crypto;
  DDS_Security_OctetSeq encoded_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq decoded_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq extra_inline_qos;
  DDS_Security_OctetSeq plain_buffer;
  session_key_material *session_keys;
  size_t length;
  struct crypto_header *header = NULL;
  struct crypto_footer *footer = NULL;
  unsigned char *contents = NULL;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform != NULL);
  assert(crypto->crypto_transform != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->encode_serialized_payload != NULL);
  assert(crypto->crypto_transform->encode_serialized_payload != 0);

  memset(&extra_inline_qos, 0, sizeof(extra_inline_qos));

  length = strlen(sample_test_data) + 1;
  plain_buffer._length = plain_buffer._maximum = (uint32_t) length;
  plain_buffer._buffer = DDS_Security_OctetSeq_allocbuf((uint32_t) length);
  memcpy((char *)plain_buffer._buffer, sample_test_data, length);

  local_writer_crypto = register_local_datawriter(true);
  CU_ASSERT_FATAL(local_writer_crypto != 0);

  session_keys = get_datawriter_session(local_writer_crypto);
  session_keys->master_key_material->transformation_kind = CRYPTO_TRANSFORMATION_KIND_AES256_GCM;
  session_keys->key_size = 256;

  local_reader_crypto = register_local_datareader(true);
  CU_ASSERT_FATAL(local_reader_crypto != 0);

  remote_reader_crypto = register_remote_datareader(local_writer_crypto);
  CU_ASSERT_FATAL(remote_reader_crypto != 0);

  remote_writer_crypto = register_remote_datawriter(local_reader_crypto);
  CU_ASSERT_FATAL(remote_writer_crypto != 0);

  result = set_remote_datawriter_tokens(local_writer_crypto, remote_reader_crypto, local_reader_crypto, remote_writer_crypto);
  CU_ASSERT_FATAL(result);

  /* Encrypt the data. */
  result = crypto->crypto_transform->encode_serialized_payload(
      crypto->crypto_transform,
      &encoded_buffer,
      &extra_inline_qos,
      &plain_buffer,
      local_writer_crypto,
      &exception);

  if (!result)
  {
    printf("encode_serialized_payload: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT_FATAL(result);
  assert(result); // for Clang's static analyzer
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);

  reset_exception(&exception);

  result = split_encoded_data(encoded_buffer._buffer, encoded_buffer._length, &header, &contents, &length, &footer);
  CU_ASSERT_FATAL(result);
  assert(result); // for Clang's static analyzer

  /* use incorrect transformation kind */
  {
    DDS_Security_CryptoTransformKind_Enum kind = header->transform_identifier.transformation_kind[3];
    header->transform_identifier.transformation_kind[3] = CRYPTO_TRANSFORMATION_KIND_AES256_GMAC;
    result = crypto->crypto_transform->decode_serialized_payload(
        crypto->crypto_transform,
        &decoded_buffer,
        &encoded_buffer,
        &extra_inline_qos,
        local_reader_crypto,
        remote_writer_crypto,
        &exception);

    if (!result)
    {
      printf("decode_serialized_payload: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    header->transform_identifier.transformation_kind[3] = (unsigned char) kind;
  }

  /* use incorrect transformation key id */
  {
    unsigned char key[4];
    uint32_t val = crypto_get_random_uint32();

    memcpy(key, header->transform_identifier.transformation_key_id, 4);
    memcpy(header->transform_identifier.transformation_key_id, &val, 4);

    result = crypto->crypto_transform->decode_serialized_payload(
        crypto->crypto_transform,
        &decoded_buffer,
        &encoded_buffer,
        &extra_inline_qos,
        local_reader_crypto,
        remote_writer_crypto,
        &exception);

    if (!result)
    {
      printf("decode_serialized_payload: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    memcpy(header->transform_identifier.transformation_key_id, key, 4);
  }

  /* use incorrect session id*/
  {
    unsigned char sid[4];
    uint32_t val = crypto_get_random_uint32();

    memcpy(sid, header->session_id, 4);
    memcpy(header->session_id, &val, 4);

    result = crypto->crypto_transform->decode_serialized_payload(
        crypto->crypto_transform,
        &decoded_buffer,
        &encoded_buffer,
        &extra_inline_qos,
        local_reader_crypto,
        remote_writer_crypto,
        &exception);

    if (!result)
    {
      printf("decode_serialized_payload: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    memcpy(header->session_id, sid, 4);
  }

  /* use incorrect init vector suffix*/
  {
    unsigned char iv[8];
    struct
    {
      uint32_t h;
      uint32_t l;
    } val;

    val.h = crypto_get_random_uint32();
    val.l = crypto_get_random_uint32();

    memcpy(iv, header->init_vector_suffix, 8);
    memcpy(header->init_vector_suffix, &val, 8);

    result = crypto->crypto_transform->decode_serialized_payload(
        crypto->crypto_transform,
        &decoded_buffer,
        &encoded_buffer,
        &extra_inline_qos,
        local_reader_crypto,
        remote_writer_crypto,
        &exception);

    if (!result)
    {
      printf("decode_serialized_payload: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    memcpy(header->init_vector_suffix, iv, 8);
  }

  /* use incorrect data length */
  {
    uint32_t saved, len;
    unsigned char *ptr;

    ptr = encoded_buffer._buffer + sizeof(struct crypto_header);

    memcpy(&saved, ptr, 4);

    len = ddsrt_toBE4u(saved);
    len += 4;
    len = ddsrt_fromBE4u(len);

    memcpy(ptr, &len, 4);

    result = crypto->crypto_transform->decode_serialized_payload(
        crypto->crypto_transform,
        &decoded_buffer,
        &encoded_buffer,
        &extra_inline_qos,
        local_reader_crypto,
        remote_writer_crypto,
        &exception);

    if (!result)
    {
      printf("decode_serialized_payload: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    memcpy(ptr, &saved, 4);
  }

  /* use incorrect data */
  {
    unsigned char saved[10];
    unsigned char *ptr;

    ptr = contents + 20;

    memcpy(&saved, ptr, 10);
    memset(ptr, 0xFF, 10);

    result = crypto->crypto_transform->decode_serialized_payload(
        crypto->crypto_transform,
        &decoded_buffer,
        &encoded_buffer,
        &extra_inline_qos,
        local_reader_crypto,
        remote_writer_crypto,
        &exception);

    if (!result)
    {
      printf("decode_serialized_payload: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    memcpy(ptr, &saved, 10);
  }

  /* use incorrect hmac */
  {
    unsigned char hmac[16];
    uint32_t i, j;

    memcpy(hmac, footer->common_mac, 16);
    for (i = 0, j = 15; i < 8; ++i, --j)
    {
      unsigned char c = footer->common_mac[j];
      footer->common_mac[j] = footer->common_mac[i];
      footer->common_mac[i] = c;
    }

    result = crypto->crypto_transform->decode_serialized_payload(
        crypto->crypto_transform,
        &decoded_buffer,
        &encoded_buffer,
        &extra_inline_qos,
        local_reader_crypto,
        remote_writer_crypto,
        &exception);

    if (!result)
    {
      printf("decode_serialized_payload: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    memcpy(footer->common_mac, &hmac, 16);
  }

  /* use incorrect footer*/
  {
    footer->length[0] = 1;

    result = crypto->crypto_transform->decode_serialized_payload(
        crypto->crypto_transform,
        &decoded_buffer,
        &encoded_buffer,
        &extra_inline_qos,
        local_reader_crypto,
        remote_writer_crypto,
        &exception);

    if (!result)
    {
      printf("decode_serialized_payload: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    footer->length[0] = 0;
  }

  unregister_datareader(remote_reader_crypto);
  unregister_datawriter(remote_writer_crypto);
  unregister_datareader(local_reader_crypto);
  unregister_datawriter(local_writer_crypto);

  DDS_Security_OctetSeq_deinit(&encoded_buffer);
  DDS_Security_OctetSeq_deinit(&plain_buffer);
}

