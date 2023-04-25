// Copyright(c) 2006 to 2020 ZettaScale Technology and others
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
#include "common/src/crypto_helper.h"
#include "crypto_objects.h"
#include "crypto_utils.h"

#define TEST_SHARED_SECRET_SIZE 32

static struct plugins_hdl *plugins = NULL;
static dds_security_cryptography *crypto = NULL;

static DDS_Security_IdentityHandle g_local_participant_identity = 1;
static DDS_Security_IdentityHandle g_remote_participant_identity = 2;

static DDS_Security_ParticipantCryptoHandle g_local_participant_crypto = 0;
static DDS_Security_ParticipantCryptoHandle g_remote_participant_crypto = 0;

static DDS_Security_SharedSecretHandle g_shared_secret_handle = DDS_SECURITY_HANDLE_NIL;

static const char *SAMPLE_TEST_DATA =
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
  uint32_t length;
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

  g_shared_secret_handle = (DDS_Security_SharedSecretHandle)shared_secret_handle_impl;
}

static void deallocate_shared_secret(void)
{
  DDS_Security_SharedSecretHandleImpl *shared_secret_handle_impl = (DDS_Security_SharedSecretHandleImpl *)g_shared_secret_handle;
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

  g_local_participant_crypto =
      crypto->crypto_key_factory->register_local_participant(
          crypto->crypto_key_factory,
          g_local_participant_identity,
          participant_permissions,
          &participant_properties,
          &participant_security_attributes,
          &exception);

  if (g_local_participant_crypto == 0)
  {
    printf("[ERROR] register_local_participant: %s\n", exception.message ? exception.message : "Error message missing");
  }

  return g_local_participant_crypto ? 0 : -1;
}

static void unregister_local_participant(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  if (g_local_participant_crypto)
  {
    crypto->crypto_key_factory->unregister_participant(crypto->crypto_key_factory, g_local_participant_crypto, &exception);
    reset_exception(&exception);
  }
}

static int register_remote_participant(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PermissionsHandle remote_participant_permissions = 5;

  g_remote_participant_crypto =
      crypto->crypto_key_factory->register_matched_remote_participant(
          crypto->crypto_key_factory,
          g_local_participant_crypto,
          g_remote_participant_identity,
          remote_participant_permissions,
          g_shared_secret_handle,
          &exception);

  if (g_remote_participant_crypto == 0)
  {
    printf("[ERROR] register_matched_remote_participant: %s\n", exception.message ? exception.message : "Error message missing");
  }

  return g_remote_participant_crypto ? 0 : -1;
}

static void unregister_remote_participant(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  if (g_remote_participant_crypto)
  {
    crypto->crypto_key_factory->unregister_participant(crypto->crypto_key_factory, g_remote_participant_crypto, &exception);
    reset_exception(&exception);
  }
}

static DDS_Security_DatawriterCryptoHandle register_local_datawriter(bool encrypted)
{
  DDS_Security_DatawriterCryptoHandle writer_crypto;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PropertySeq datawriter_properties;
  DDS_Security_EndpointSecurityAttributes datawriter_security_attributes;

  memset(&datawriter_properties, 0, sizeof(datawriter_properties));
  memset(&datawriter_security_attributes, 0, sizeof(datawriter_security_attributes));
  datawriter_security_attributes.is_payload_protected = true;
  if (encrypted)
  {
    datawriter_security_attributes.plugin_endpoint_attributes |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_PAYLOAD_ENCRYPTED;
  }

  writer_crypto =
      crypto->crypto_key_factory->register_local_datawriter(
          crypto->crypto_key_factory,
          g_local_participant_crypto,
          &datawriter_properties,
          &datawriter_security_attributes,
          &exception);

  if (writer_crypto == 0)
  {
    printf("[ERROR] register_local_datawriter: %s\n", exception.message ? exception.message : "Error message missing");
  }

  return writer_crypto;
}

static void unregister_local_datawriter(DDS_Security_DatawriterCryptoHandle writer_crypto)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  if (writer_crypto)
  {
    crypto->crypto_key_factory->unregister_datawriter(crypto->crypto_key_factory, writer_crypto, &exception);
    reset_exception(&exception);
  }
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
          g_remote_participant_crypto,
          g_shared_secret_handle,
          true,
          &exception);

  if (reader_crypto == 0)
  {
    printf("[ERROR] register_matched_remote_datareader: %s\n", exception.message ? exception.message : "Error message missing");
  }

  return reader_crypto;
}

static void unregister_remote_datareader(DDS_Security_DatareaderCryptoHandle reader_crypto)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  if (reader_crypto)
  {
    crypto->crypto_key_factory->unregister_datareader(crypto->crypto_key_factory, reader_crypto, &exception);
    reset_exception(&exception);
  }
}

static bool split_encoded_data(unsigned char *data, uint32_t size, struct crypto_header **header, DDS_Security_OctetSeq *payload, struct crypto_footer **footer, bool encrypted)
{
  /* The length is the length of the encrypted data and the common_mac of the footer
     * For the serialized payload the footer consists of the common_mac and the length
     * of the receiver_specific_mac which is set to 0 */
  static const uint32_t FOOTER_SIZE = 20;
  unsigned char *header_ptr;
  unsigned char *payload_ptr;
  unsigned char *footer_ptr;
  uint32_t payload_size;

  if (size < (sizeof(struct crypto_header) + FOOTER_SIZE))
  {
    return false;
  }

  header_ptr = data;
  payload_ptr = data + sizeof(struct crypto_header);
  footer_ptr = data + size - FOOTER_SIZE;

  /* Get header. */
  *header = (struct crypto_header *)header_ptr;

  /* Get payload */
  payload_size = (uint32_t)(footer_ptr - payload_ptr);
  if (encrypted)
  {
    /* CryptoContent starts with 4 bytes length. */
    if (payload_size < 4)
    {
      return false;
    }
    payload->_length = ddsrt_fromBE4u(*(uint32_t *)payload_ptr);
    payload->_buffer = payload_ptr + 4;
    if ((payload_size - 4) != payload->_length)
    {
      return false;
    }
  }
  else
  {
    /* Just the clear payload */
    payload->_length = payload_size;
    payload->_buffer = payload_ptr;
  }
  payload->_maximum = payload->_length;

  /* Get footer. */
  *footer = (struct crypto_footer *)footer_ptr;

  return true;
}

static bool crypto_decrypt_data(uint32_t session_id, unsigned char *iv, DDS_Security_CryptoTransformKind transformation_kind, master_key_material *key_material, DDS_Security_OctetSeq *encrypted, DDS_Security_OctetSeq *decoded, unsigned char *tag)
{
  bool result = true;
  EVP_CIPHER_CTX *ctx;
  crypto_session_key_t session_key;
  uint32_t key_size = crypto_get_key_size(CRYPTO_TRANSFORM_KIND(transformation_kind));
  int len = 0;

  if (!crypto_calculate_session_key_test(&session_key, session_id, key_material->master_salt, key_material->master_sender_key, key_material->transformation_kind))
  {
    printf("[ERROR] (%d) crypto_decrypt_data: could not calculate session key!\n", __LINE__);
    return false;
  }

  /* create the cipher context */
  ctx = EVP_CIPHER_CTX_new();
  if (ctx)
  {
    if (key_size == 128)
    {
      if (!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL))
      {
        printf("[ERROR] (%d) crypto_decrypt_data: could not get init CIPHER_CTX (128)\n", __LINE__);
        ERR_print_errors_fp(stderr);
        result = false;
      }
    }
    else if (key_size == 256)
    {
      if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
      {
        printf("[ERROR] (%d) crypto_decrypt_data: could not get init CIPHER_CTX (256)\n", __LINE__);
        ERR_print_errors_fp(stderr);
        result = false;
      }
    }
    else
    {
      printf("[ERROR] (%d) crypto_decrypt_data: could not determine keysize\n", __LINE__);
      result = false;
    }
  }
  else
  {
    printf("[ERROR] (%d) crypto_decrypt_data: could not get new CIPHER_CTX\n", __LINE__);
    result = false;
  }

  if (result)
  {
    if (!EVP_DecryptInit_ex(ctx, NULL, NULL, session_key.data, iv))
    {
      printf("[ERROR] (%d) crypto_decrypt_data: could not init Decrypt\n", __LINE__);
      ERR_print_errors_fp(stderr);
      result = false;
    }
  }

  if (result)
  {
    if (decoded)
    {
      if (EVP_DecryptUpdate(ctx, decoded->_buffer, &len, encrypted->_buffer, (int) encrypted->_length))
      {
        decoded->_length = (uint32_t) len;
      }
      else
      {
        printf("[ERROR] (%d) crypto_decrypt_data: could not update Decrypt (decoded)\n", __LINE__);
        ERR_print_errors_fp(stderr);
        result = false;
      }
    }
    else
    {
      if (!EVP_DecryptUpdate(ctx, NULL, &len, encrypted->_buffer, (int) encrypted->_length))
      {
        printf("[ERROR] (%d) crypto_decrypt_data: could not update Decrypt (!decoded)\n", __LINE__);
        ERR_print_errors_fp(stderr);
        result = false;
      }
    }
  }

  if (result)
  {
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, CRYPTO_HMAC_SIZE, tag))
    {
      printf("[ERROR] (%d) crypto_decrypt_data: could not ctrl CIPHER_CTX\n", __LINE__);
      ERR_print_errors_fp(stderr);
      result = false;
    }
  }

  if (result)
  {
    if (decoded)
    {
      if (EVP_DecryptFinal_ex(ctx, decoded->_buffer + len, &len))
      {
        decoded->_length += (uint32_t) len;
      }
      else
      {
        printf("[ERROR] (%d) crypto_decrypt_data: could not finalize Decrypt (decoded)\n", __LINE__);
        ERR_print_errors_fp(stderr);
        result = false;
      }
    }
    else
    {
      unsigned char temp[32];
      if (!EVP_DecryptFinal_ex(ctx, temp, &len))
      {
        printf("[ERROR] (%d) crypto_decrypt_data: could not finalize Decrypt (!decoded)\n", __LINE__);
        ERR_print_errors_fp(stderr);
        result = false;
      }
    }
  }

  if (ctx)
    EVP_CIPHER_CTX_free(ctx);

  return result;
}

static session_key_material * get_datawriter_session(DDS_Security_DatawriterCryptoHandle writer_crypto)
{
  local_datawriter_crypto *writer_crypto_impl = (local_datawriter_crypto *)writer_crypto;
  return writer_crypto_impl->writer_session_payload;
}

static bool check_protection_kind(DDS_Security_DatawriterCryptoHandle writer_crypto, DDS_Security_BasicProtectionKind protection_kind)
{
  local_datawriter_crypto *writer_crypto_impl = (local_datawriter_crypto *)writer_crypto;
  return (writer_crypto_impl->data_protectionKind == protection_kind);
}

static void suite_encode_serialized_payload_init(void)
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

static void suite_encode_serialized_payload_fini(void)
{
  unregister_remote_participant();
  unregister_local_participant();
  unload_plugins(plugins);
  deallocate_shared_secret();
}

static uint32_t get_transformation_kind(uint32_t key_size, bool encrypted)
{
  uint32_t kind = CRYPTO_TRANSFORMATION_KIND_INVALID;
  if (key_size == 128)
  {
    kind = encrypted ? CRYPTO_TRANSFORMATION_KIND_AES128_GCM : CRYPTO_TRANSFORMATION_KIND_AES128_GMAC;
  }
  else if (key_size == 256)
  {
    kind = encrypted ? CRYPTO_TRANSFORMATION_KIND_AES256_GCM : CRYPTO_TRANSFORMATION_KIND_AES256_GMAC;
  }
  CU_ASSERT_FATAL(kind != CRYPTO_TRANSFORMATION_KIND_INVALID);
  return kind;
}

static bool seq_equal(DDS_Security_OctetSeq *seq1, DDS_Security_OctetSeq *seq2)
{
  bool ok = false;
  if (seq1->_length == seq2->_length)
  {
    if (memcmp(seq1->_buffer, seq2->_buffer, seq1->_length) == 0)
    {
      ok = true;
    }
  }
  return ok;
}

static bool check_payload_signed(DDS_Security_OctetSeq *payload, DDS_Security_OctetSeq *plain_buffer)
{
  /* When only signed, the payload should not have changed. */
  return seq_equal(payload, plain_buffer);
}

static bool check_payload_encrypted(DDS_Security_OctetSeq *payload, DDS_Security_OctetSeq *plain_buffer)
{
  bool ok = false;
  /* When encrypted, the payload should differ from the original data. */
  if (payload->_length >= plain_buffer->_length)
  {
    if (memcmp(payload->_buffer, plain_buffer->_buffer, plain_buffer->_length) != 0)
    {
      ok = true;
    }
  }
  return ok;
}

static bool check_payload_encoded(DDS_Security_OctetSeq *payload, DDS_Security_OctetSeq *plain_buffer, bool encrypted)
{
  bool ok;
  if (encrypted)
  {
    ok = check_payload_encrypted(payload, plain_buffer);
  }
  else
  {
    ok = check_payload_signed(payload, plain_buffer);
  }
  return ok;
}

static bool check_payload_decoded(DDS_Security_OctetSeq *payload, DDS_Security_OctetSeq *plain_buffer)
{
  /* After decoding, the payload should match the orignal. */
  return seq_equal(payload, plain_buffer);
}

static void encode_serialized_payload_check(uint32_t key_size, bool encrypted)
{
  DDS_Security_boolean result;
  DDS_Security_DatawriterCryptoHandle writer_crypto;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_OctetSeq encoded_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq extra_inline_qos;
  DDS_Security_OctetSeq encoded_payload;
  DDS_Security_OctetSeq plain_buffer;
  session_key_material *session_keys;
  struct crypto_header *header = NULL;
  struct crypto_footer *footer = NULL;
  uint32_t session_id;
  size_t length;

  CU_ASSERT_FATAL(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->encode_serialized_payload != NULL);

  memset(&extra_inline_qos, 0, sizeof(extra_inline_qos));

  length = strlen(SAMPLE_TEST_DATA) + 1;
  plain_buffer._length = plain_buffer._maximum = (uint32_t) length;
  plain_buffer._buffer = DDS_Security_OctetSeq_allocbuf((uint32_t) length);
  memcpy((char *)plain_buffer._buffer, SAMPLE_TEST_DATA, length);

  writer_crypto = register_local_datawriter(encrypted);
  CU_ASSERT_FATAL(writer_crypto != 0);
  assert(writer_crypto != 0); // for Clang's static analyzer

  CU_ASSERT(check_protection_kind(writer_crypto, encrypted ? DDS_SECURITY_BASICPROTECTION_KIND_ENCRYPT : DDS_SECURITY_BASICPROTECTION_KIND_SIGN));

  session_keys = get_datawriter_session(writer_crypto);
  session_keys->master_key_material->transformation_kind = get_transformation_kind(key_size, encrypted);
  session_keys->key_size = key_size;

  /* Now call the function. */
  result = crypto->crypto_transform->encode_serialized_payload(
      crypto->crypto_transform,
      &encoded_buffer,
      &extra_inline_qos,
      &plain_buffer,
      writer_crypto,
      &exception);

  if (!result)
  {
    printf("[ERROR] encode_serialized_payload: %s\n", exception.message ? exception.message : "Error message missing");
  }
  CU_ASSERT_FATAL(result);
  assert(result); // for Clang's static analyzer
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);
  reset_exception(&exception);

  result = split_encoded_data(encoded_buffer._buffer, encoded_buffer._length, &header, &encoded_payload, &footer, encrypted);
  CU_ASSERT_FATAL(result == true);
  assert(result); // for Clang's static analyzer
  CU_ASSERT(check_payload_encoded(&encoded_payload, &plain_buffer, encrypted));

  session_id = ddsrt_fromBE4u(*(uint32_t *)header->session_id);

  if (encrypted)
  {
    DDS_Security_OctetSeq decoded_buffer;
    decoded_buffer._buffer = ddsrt_malloc(length);
    decoded_buffer._length = (uint32_t) length;
    decoded_buffer._maximum = (uint32_t) length;
    result = crypto_decrypt_data(session_id, &header->session_id[0], header->transform_identifier.transformation_kind, session_keys->master_key_material, &encoded_payload, &decoded_buffer, footer->common_mac);
    if (!result)
    {
      printf("[ERROR] Decryption failed\n");
    }
    CU_ASSERT_FATAL(result);
    CU_ASSERT(check_payload_decoded(&decoded_buffer, &plain_buffer));
    DDS_Security_OctetSeq_deinit(&decoded_buffer);
  }
  else
  {
    result = crypto_decrypt_data(session_id, &header->session_id[0], header->transform_identifier.transformation_kind, session_keys->master_key_material, &encoded_payload, NULL, footer->common_mac);
    if (!result)
    {
      printf("[ERROR] Signature check failed\n");
    }
    CU_ASSERT_FATAL(result);
  }

  DDS_Security_OctetSeq_deinit(&encoded_buffer);
  DDS_Security_OctetSeq_deinit(&plain_buffer);

  unregister_local_datawriter(writer_crypto);
}

CU_Test(ddssec_builtin_encode_serialized_payload, encrypt_128, .init = suite_encode_serialized_payload_init, .fini = suite_encode_serialized_payload_fini)
{
  encode_serialized_payload_check(128, true);
}

CU_Test(ddssec_builtin_encode_serialized_payload, encrypt_256, .init = suite_encode_serialized_payload_init, .fini = suite_encode_serialized_payload_fini)
{
  encode_serialized_payload_check(256, true);
}

CU_Test(ddssec_builtin_encode_serialized_payload, sign_128, .init = suite_encode_serialized_payload_init, .fini = suite_encode_serialized_payload_fini)
{
  encode_serialized_payload_check(128, false);
}

CU_Test(ddssec_builtin_encode_serialized_payload, sign_256, .init = suite_encode_serialized_payload_init, .fini = suite_encode_serialized_payload_fini)
{
  encode_serialized_payload_check(256, false);
}

CU_Test(ddssec_builtin_encode_serialized_payload, invalid_args, .init = suite_encode_serialized_payload_init, .fini = suite_encode_serialized_payload_fini)
{
  DDS_Security_boolean result;
  DDS_Security_DatawriterCryptoHandle writer_crypto;
  DDS_Security_DatareaderCryptoHandle reader_crypto;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_OctetSeq encoded_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq extra_inline_qos;
  DDS_Security_OctetSeq plain_buffer;
  DDS_Security_OctetSeq empty_buffer;
  size_t length;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform != NULL);
  assert(crypto->crypto_transform != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->encode_serialized_payload != NULL);
  assert(crypto->crypto_transform->encode_serialized_payload != 0);

  writer_crypto = register_local_datawriter(true);
  CU_ASSERT_FATAL(writer_crypto != 0);

  reader_crypto = register_remote_datareader(writer_crypto);
  CU_ASSERT_FATAL(reader_crypto != 0);

  memset(&extra_inline_qos, 0, sizeof(extra_inline_qos));
  memset(&empty_buffer, 0, sizeof(empty_buffer));

  length = strlen(SAMPLE_TEST_DATA) + 1;
  plain_buffer._length = plain_buffer._maximum = (uint32_t) length;
  plain_buffer._buffer = DDS_Security_OctetSeq_allocbuf((uint32_t) length);
  memcpy((char *)plain_buffer._buffer, SAMPLE_TEST_DATA, length);

  /* unknown writer crypto handle specified */
  result = crypto->crypto_transform->encode_serialized_payload(
      crypto->crypto_transform,
      &encoded_buffer,
      &extra_inline_qos,
      &plain_buffer,
      1,
      &exception);

  if (!result)
  {
    printf("encode_serialized_payload: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* incorrect writer crypto handle specified */
  result = crypto->crypto_transform->encode_serialized_payload(
      crypto->crypto_transform,
      &encoded_buffer,
      &extra_inline_qos,
      &plain_buffer,
      reader_crypto,
      &exception);

  if (!result)
  {
    printf("encode_serialized_payload: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  unregister_remote_datareader(reader_crypto);
  unregister_local_datawriter(writer_crypto);

  DDS_Security_OctetSeq_deinit(&plain_buffer);
}

