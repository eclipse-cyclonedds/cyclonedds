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
#include "dds/ddsrt/endian.h"
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

#define TEST_SHARED_SECRET_SIZE 32

static struct plugins_hdl *plugins = NULL;
static dds_security_cryptography *crypto = NULL;

static DDS_Security_IdentityHandle local_participantA_identity = 1;
static DDS_Security_IdentityHandle local_participantB_identity = 2;
static DDS_Security_IdentityHandle remote_identities[] = {2, 3, 4, 5};

static DDS_Security_ParticipantCryptoHandle local_participantA_crypto = 0;
static DDS_Security_ParticipantCryptoHandle local_participantB_crypto = 0;
static DDS_Security_ParticipantCryptoHandle remote_particpantA_crypto;
static DDS_Security_ParticipantCryptoHandle remote_cryptos[4];

static DDS_Security_SharedSecretHandleImpl *shared_secret_handle_impl = NULL;
static DDS_Security_SharedSecretHandle shared_secret_handle;

static const char *sample_test_data =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxy";

static const char *RTPS_HEADER = "RTPS abcdefghijklmno";

struct submsg_header
{
  unsigned char id;
  unsigned char flags;
  uint16_t length;
};

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

#if 0
struct receiver_specific_mac
{
  DDS_Security_CryptoTransformKeyId receiver_mac_key_id;
  unsigned char receiver_mac[CRYPTO_HMAC_SIZE];
};
#endif

static void reset_exception(DDS_Security_SecurityException *ex)
{
  ex->code = 0;
  ex->minor_code = 0;
  ddsrt_free(ex->message);
  ex->message = NULL;
}

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

static void print_octets(const char *msg, const unsigned char *data, size_t sz)
{
  size_t i;
  printf("%s: ", msg);
  for (i = 0; i < sz; i++)
  {
    printf("%02x", data[i]);
  }
  printf("\n");
}

static void prepare_participant_security_attributes_and_properties(DDS_Security_ParticipantSecurityAttributes *attributes,
                                                                   DDS_Security_PropertySeq *properties,
                                                                   DDS_Security_CryptoTransformKind_Enum transformation_kind,
                                                                   bool is_origin_authenticated)
{
  memset(attributes, 0, sizeof(DDS_Security_EndpointSecurityAttributes));

  attributes->is_discovery_protected = true;

  if (properties != NULL)
  {
    memset(properties, 0, sizeof(DDS_Security_PropertySeq));
    properties->_maximum = properties->_length = 1;
    properties->_buffer = ddsrt_malloc(sizeof(DDS_Security_Property_t));

    properties->_buffer[0].name = ddsrt_strdup(DDS_SEC_PROP_CRYPTO_KEYSIZE);

    if (transformation_kind == CRYPTO_TRANSFORMATION_KIND_AES128_GCM || transformation_kind == CRYPTO_TRANSFORMATION_KIND_AES128_GMAC)
    {
      properties->_buffer[0].value = ddsrt_strdup("128");
    }
    else
    {
      properties->_buffer[0].value = ddsrt_strdup("256");
    }
  }

  switch (transformation_kind)
  {
  case CRYPTO_TRANSFORMATION_KIND_AES128_GCM:
  case CRYPTO_TRANSFORMATION_KIND_AES256_GCM:
    attributes->plugin_participant_attributes = DDS_SECURITY_PARTICIPANT_ATTRIBUTES_FLAG_IS_VALID;
    attributes->is_rtps_protected = true;
    attributes->plugin_participant_attributes |= DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_ENCRYPTED;
    if (is_origin_authenticated)
    {
      attributes->plugin_participant_attributes |= DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_AUTHENTICATED;
    }
    break;

  case CRYPTO_TRANSFORMATION_KIND_AES256_GMAC:
  case CRYPTO_TRANSFORMATION_KIND_AES128_GMAC:
    attributes->plugin_participant_attributes = DDS_SECURITY_PARTICIPANT_ATTRIBUTES_FLAG_IS_VALID;
    attributes->is_rtps_protected = true;
    if (is_origin_authenticated)
    {
      attributes->plugin_participant_attributes |= DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_AUTHENTICATED;
    }
    break;

  default:
    assert(0);
    break;
  }
}

static int register_local_participants(DDS_Security_ParticipantSecurityAttributes *participant_security_attributes, DDS_Security_PropertySeq *participant_properties)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PermissionsHandle participant_permissions = 3; //valid dummy value

  local_participantA_crypto =
      crypto->crypto_key_factory->register_local_participant(
          crypto->crypto_key_factory,
          local_participantA_identity,
          participant_permissions,
          participant_properties,
          participant_security_attributes,
          &exception);

  if (local_participantA_crypto == 0)
  {
    printf("register_local_participant: %s\n", exception.message ? exception.message : "Error message missing");
  }

  local_participantB_crypto =
      crypto->crypto_key_factory->register_local_participant(
          crypto->crypto_key_factory,
          local_participantB_identity,
          participant_permissions,
          participant_properties,
          participant_security_attributes,
          &exception);

  if (local_participantA_crypto == 0 || local_participantB_crypto == 0)
  {
    printf("register_local_participant: %s\n", exception.message ? exception.message : "Error message missing");
  }

  return local_participantA_crypto && local_participantB_crypto ? 0 : -1;
}

static void unregister_local_participants(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  if (local_participantA_crypto)
  {
    crypto->crypto_key_factory->unregister_participant(crypto->crypto_key_factory, local_participantA_crypto, &exception);
    reset_exception(&exception);
  }
  if (local_participantB_crypto)
  {
    crypto->crypto_key_factory->unregister_participant(crypto->crypto_key_factory, local_participantB_crypto, &exception);
    reset_exception(&exception);
  }
}

static int register_remote_participants(DDS_Security_ParticipantCryptoHandle local_id,
                             DDS_Security_IdentityHandle remote_ids[4],
                             DDS_Security_ParticipantCryptoHandle participant_cryptos[4])
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PermissionsHandle remote_participant_permissions = 5;

  unsigned i;
  int result = 0;

  for (i = 0; i < 4; ++i)
  {
    participant_cryptos[i] =
        crypto->crypto_key_factory->register_matched_remote_participant(
            crypto->crypto_key_factory,
            local_id,
            remote_ids[i],
            remote_participant_permissions,
            shared_secret_handle,
            &exception);

    if (participant_cryptos[i] == 0)
    {
      printf("register_matched_remote_participant: %s\n", exception.message ? exception.message : "Error message missing");
      result = 1;
      break;
    }
  }

  return result;
}

static void unregister_remote_participants(void)
{
  unsigned i;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  for (i = 0; i < 4; ++i)
  {
    if (remote_cryptos[i])
    {
      crypto->crypto_key_factory->unregister_participant(crypto->crypto_key_factory, remote_cryptos[i], &exception);
      reset_exception(&exception);
    }
  }
}

static int register_remote_participant_for_participantB(
    DDS_Security_ParticipantCryptoHandle local_id,
    DDS_Security_IdentityHandle remote_identity,
    DDS_Security_ParticipantCryptoHandle *remote_participant_crypto)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PermissionsHandle remote_participant_permissions = 5;

  int result = 0;

  *remote_participant_crypto = crypto->crypto_key_factory->register_matched_remote_participant(
      crypto->crypto_key_factory, local_id, remote_identity,
      remote_participant_permissions, shared_secret_handle, &exception);

  if (*remote_participant_crypto == 0)
  {
    printf("register_matched_remote_participant: %s\n",
           exception.message ? exception.message : "Error message missing");
    result = 1;
  }

  return result;
}

static void unregister_remote_participant_of_participantB(DDS_Security_ParticipantCryptoHandle remote_participant_crypto)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  if (remote_participant_crypto)
  {
    crypto->crypto_key_factory->unregister_participant(crypto->crypto_key_factory, remote_participant_crypto, &exception);
    reset_exception(&exception);
  }
}

static void set_protection_kind(DDS_Security_ParticipantCryptoHandle participant_crypto, DDS_Security_ProtectionKind protection_kind)
{
  local_participant_crypto *paricipant_crypto_impl = (local_participant_crypto *)participant_crypto;

  paricipant_crypto_impl->rtps_protection_kind = protection_kind;
}

static bool set_remote_participant_tokens(
    DDS_Security_ParticipantCryptoHandle local_participantA_crypto_handle,
    DDS_Security_ParticipantCryptoHandle remote_participantB_crypto_handle,
    DDS_Security_ParticipantCryptoHandle local_participantB_crypto_handle,
    DDS_Security_ParticipantCryptoHandle remote_participantA_crypto_handle)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatawriterCryptoTokenSeq tokens;

  memset(&tokens, 0, sizeof(tokens));

  /* Now call the function. */

  result = crypto->crypto_key_exchange->create_local_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      &tokens,
      local_participantA_crypto_handle,
      remote_participantB_crypto_handle,
      &exception);

  if (result)
  {
    result = crypto->crypto_key_exchange->set_remote_participant_crypto_tokens(
        crypto->crypto_key_exchange,
        local_participantB_crypto_handle,
        remote_participantA_crypto_handle,
        &tokens,
        &exception);
  }

  DDS_Security_DataHolderSeq_deinit(&tokens);

  return (bool)result;
}

static session_key_material * get_local_participant_session(DDS_Security_ParticipantCryptoHandle participant_crypto)
{
  local_participant_crypto *participant_crypto_impl = (local_participant_crypto *)participant_crypto;
  return participant_crypto_impl->session;
}

static void suite_decode_rtps_message_init(void)
{
  allocate_shared_secret();

  CU_ASSERT_FATAL ((plugins = load_plugins(
                      NULL    /* Access Control */,
                      NULL    /* Authentication */,
                      &crypto /* Cryptograpy    */,
                      NULL)) != NULL);
}

static void suite_decode_rtps_message_fini(void)
{
  deallocate_shared_secret();
  unload_plugins(plugins);
}

static unsigned char submsg_header_endianness_flag (enum ddsrt_byte_order_selector bo)
{
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
  return (unsigned char) ((bo == DDSRT_BOSEL_BE) ? 0 : DDSI_RTPS_SUBMESSAGE_FLAG_ENDIANNESS);
#else
  return (unsigned char) ((bo == DDSRT_BO_LE) ? DDSI_RTPS_SUBMESSAGE_FLAG_ENDIANNESS : 0);
#endif
}

static void initialize_rtps_message(DDS_Security_OctetSeq *submsg, enum ddsrt_byte_order_selector bo)
{
  size_t length = strlen(sample_test_data) + 1;
  struct submsg_header *header;
  unsigned char *buffer, *ptr;

  buffer = ddsrt_malloc(length + 20 + sizeof(struct submsg_header));
  memcpy(buffer, RTPS_HEADER, 20);

  header = (struct submsg_header *)(buffer + 20);
  header->id = 0x15;
  header->flags = submsg_header_endianness_flag(bo);
  header->length = ddsrt_toBO2u(bo, (uint16_t)length);

  ptr = (unsigned char *)(header + 1);
  memcpy((char *)ptr, sample_test_data, length);

  submsg->_length = submsg->_maximum = (uint32_t)(20 + length + sizeof(struct submsg_header));
  submsg->_buffer = buffer;
}

static void set_submsg_header(struct submsg_header *submsg, unsigned char id, unsigned char flags, uint16_t length)
{
  submsg->id = id;
  submsg->flags = flags;
  submsg->length = length;
}

static struct submsg_header * get_submsg(unsigned char *data, int num)
{
  struct submsg_header *submsg;
  int i;

  submsg = (struct submsg_header *)data;
  for (i = 0; i < num - 1; i++)
  {
    uint32_t hlen = submsg->length;
    data += sizeof(struct submsg_header) + hlen;
    submsg = (struct submsg_header *)data;
  }

  return submsg;
}

static struct crypto_header * get_crypto_header(unsigned char *data)
{
  return (struct crypto_header *)(data + sizeof(struct submsg_header));
}

static struct crypto_footer * get_crypto_footer(unsigned char *data)
{
  struct submsg_header *submsg;
  submsg = get_submsg(data + 20, 3);
  return (struct crypto_footer *)(submsg + 1);
}

static bool check_decoded_rtps_message(const DDS_Security_OctetSeq *decoded, const DDS_Security_OctetSeq *orig)
{
  unsigned char *d_ptr, *o_ptr;
  size_t d_len = decoded->_length;
  size_t o_len = orig->_length;
  ddsi_rtps_info_src_t *info_src;

  if (d_len < DDSI_RTPS_MESSAGE_HEADER_SIZE)
  {
    CU_FAIL("decoded message does not start with an RTPS header");
    return false;
  }

  if (memcmp(decoded->_buffer, orig->_buffer, DDSI_RTPS_MESSAGE_HEADER_SIZE) != 0)
  {
    CU_FAIL("decoded message does not start with an RTPS header");
    return false;
  }
  d_ptr = decoded->_buffer + DDSI_RTPS_MESSAGE_HEADER_SIZE;
  o_ptr = orig->_buffer + DDSI_RTPS_MESSAGE_HEADER_SIZE;
  d_len -= DDSI_RTPS_MESSAGE_HEADER_SIZE;
  o_len -= DDSI_RTPS_MESSAGE_HEADER_SIZE;

  if (d_len < sizeof(ddsi_rtps_info_src_t))
  {
    CU_FAIL("decoded message does not start with an InfoSRC submessage");
    return false;
  }

  info_src = (ddsi_rtps_info_src_t *)d_ptr;
  d_ptr += info_src->smhdr.octetsToNextHeader + DDSI_RTPS_SUBMESSAGE_HEADER_SIZE;
  d_len -= info_src->smhdr.octetsToNextHeader + DDSI_RTPS_SUBMESSAGE_HEADER_SIZE;

  if (d_len != o_len)
  {
    CU_FAIL("decoded message has not the expected size");
    return false;
  }

  if (memcmp(d_ptr, o_ptr, o_len) != 0)
  {
    CU_FAIL("decode submessage is not equal to original");
    print_octets("decoded", d_ptr, d_len);
    print_octets("orig   ", o_ptr, o_len);
    return false;
  }
  return true;
}

static void decode_rtps_message_not_authenticated(DDS_Security_CryptoTransformKind_Enum transformation_kind, uint32_t key_size)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatareaderCryptoHandleSeq reader_list;
  session_key_material *session_keys;
  DDS_Security_OctetSeq plain_buffer;
  DDS_Security_OctetSeq encoded_buffer;
  DDS_Security_OctetSeq decoded_buffer;
  int32_t index;
  DDS_Security_ParticipantSecurityAttributes attributes;
  DDS_Security_PropertySeq properties;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform != NULL);
  assert(crypto->crypto_transform != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->encode_rtps_message != NULL);
  assert(crypto->crypto_transform->encode_rtps_message != 0);
  CU_ASSERT_FATAL(crypto->crypto_transform->decode_rtps_message != NULL);
  assert(crypto->crypto_transform->decode_rtps_message != 0);

  prepare_participant_security_attributes_and_properties(&attributes, &properties, transformation_kind, true);
  register_local_participants(&attributes, &properties);

  initialize_rtps_message(&plain_buffer, DDSRT_BOSEL_NATIVE);

  session_keys = get_local_participant_session(local_participantA_crypto);
  session_keys->master_key_material->transformation_kind = transformation_kind;
  session_keys->key_size = key_size;

  register_remote_participants(local_participantA_crypto, remote_identities, remote_cryptos);

  /* Now remote participant cypto is in remote_cryptos[0] */

  register_remote_participant_for_participantB(local_participantB_crypto, local_participantA_identity, &remote_particpantA_crypto);

  result = set_remote_participant_tokens(local_participantA_crypto, remote_cryptos[0], local_participantB_crypto, remote_particpantA_crypto);
  CU_ASSERT_FATAL(result);

  reader_list._length = reader_list._maximum = 1;
  reader_list._buffer = DDS_Security_ParticipantCryptoHandleSeq_allocbuf(1);
  reader_list._buffer[0] = remote_cryptos[0];
  index = 0;

  /* Encrypt the datawriter submessage. */
  result = crypto->crypto_transform->encode_rtps_message(
      crypto->crypto_transform,
      &encoded_buffer,
      &plain_buffer,
      local_participantA_crypto,
      &reader_list,
      &index,
      &exception);

  if (!result)
  {
    printf("encode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT_FATAL(result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);

  reset_exception(&exception);

  /* Decrypt the datawriter submessage */
  result = crypto->crypto_transform->decode_rtps_message (
      crypto->crypto_transform,
      &decoded_buffer,
      &encoded_buffer,
      local_participantB_crypto,
      remote_particpantA_crypto,
      &exception);

  if (!result)
  {
    printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT_FATAL(result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);
  reset_exception(&exception);

  (void)check_decoded_rtps_message(&decoded_buffer, &plain_buffer);

  unregister_remote_participant_of_participantB(remote_particpantA_crypto);
  unregister_remote_participants();
  reset_exception(&exception);

  DDS_Security_OctetSeq_deinit((&plain_buffer));
  DDS_Security_OctetSeq_deinit((&decoded_buffer));
  DDS_Security_OctetSeq_deinit((&encoded_buffer));
  DDS_Security_DatareaderCryptoHandleSeq_deinit(&reader_list);
  DDS_Security_PropertySeq_deinit(&properties);

  unregister_local_participants();
}

CU_Test(ddssec_builtin_decode_rtps_message, encoded_256, .init = suite_decode_rtps_message_init, .fini = suite_decode_rtps_message_fini)
{
  decode_rtps_message_not_authenticated(CRYPTO_TRANSFORMATION_KIND_AES256_GCM, 256);
}

CU_Test(ddssec_builtin_decode_rtps_message, encoded_128, .init = suite_decode_rtps_message_init, .fini = suite_decode_rtps_message_fini)
{
  decode_rtps_message_not_authenticated(CRYPTO_TRANSFORMATION_KIND_AES128_GCM, 128);
}

CU_Test(ddssec_builtin_decode_rtps_message, not_encrypted_256, .init = suite_decode_rtps_message_init, .fini = suite_decode_rtps_message_fini)
{
  decode_rtps_message_not_authenticated(CRYPTO_TRANSFORMATION_KIND_AES256_GMAC, 256);
}

CU_Test(ddssec_builtin_decode_rtps_message, not_encrypted_128, .init = suite_decode_rtps_message_init, .fini = suite_decode_rtps_message_fini)
{
  decode_rtps_message_not_authenticated(CRYPTO_TRANSFORMATION_KIND_AES128_GMAC, 128);
}

static void decode_rtps_message_authenticated(DDS_Security_CryptoTransformKind_Enum transformation_kind, uint32_t key_size, DDS_Security_ProtectionKind protection_kind)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_ParticipantCryptoHandleSeq remote_reader_list;
  session_key_material *session_keys;
  DDS_Security_OctetSeq plain_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq encoded_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq decoded_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq *buffer;
  DDS_Security_ParticipantSecurityAttributes attributes;
  DDS_Security_PropertySeq properties;
  int i, index;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform != NULL);
  assert(crypto->crypto_transform != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->encode_rtps_message != NULL);
  assert(crypto->crypto_transform->encode_rtps_message != 0);
  CU_ASSERT_FATAL(crypto->crypto_transform->decode_rtps_message != NULL);
  assert(crypto->crypto_transform->decode_rtps_message != 0);

  prepare_participant_security_attributes_and_properties(&attributes, &properties, transformation_kind, true);
  register_local_participants(&attributes, &properties);

  initialize_rtps_message(&plain_buffer, DDSRT_BOSEL_NATIVE);

  session_keys = get_local_participant_session(local_participantA_crypto);
  session_keys->master_key_material->transformation_kind = transformation_kind;
  session_keys->key_size = key_size;

  set_protection_kind(local_participantA_crypto, protection_kind);

  register_remote_participants(local_participantA_crypto, remote_identities, remote_cryptos);

  set_protection_kind(local_participantB_crypto, protection_kind);

  register_remote_participant_for_participantB(local_participantB_crypto, local_participantA_identity, &remote_particpantA_crypto);

  result = set_remote_participant_tokens(local_participantA_crypto, remote_cryptos[0], local_participantB_crypto, remote_particpantA_crypto);
  CU_ASSERT_FATAL(result);

  remote_reader_list._length = remote_reader_list._maximum = 4;
  remote_reader_list._buffer = DDS_Security_ParticipantCryptoHandleSeq_allocbuf(4);

  for (i = 0; i < 4; i++)
  {
    remote_reader_list._buffer[i] = remote_cryptos[i];
  }

  index = 0;

  /* Encrypt the datawriter submessage. */
  buffer = &plain_buffer;
  while (index != 4)
  {
    result = crypto->crypto_transform->encode_rtps_message(
        crypto->crypto_transform,
        &encoded_buffer,
        buffer,
        local_participantA_crypto,
        &remote_reader_list,
        &index,
        &exception);

    if (!result)
    {
      printf("encode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL(result);
    CU_ASSERT(exception.code == 0);
    CU_ASSERT(exception.message == NULL);

    reset_exception(&exception);
    buffer = NULL;
  }

  /* Decrypt the datawriter submessage */

  for (i = 0; i < 4; i++)
  {
    result = crypto->crypto_transform->decode_rtps_message (
        crypto->crypto_transform,
        &decoded_buffer,
        &encoded_buffer,
        local_participantB_crypto,
        remote_particpantA_crypto,
        &exception);

    if (!result)
    {
      printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL(result);
    CU_ASSERT(exception.code == 0);
    CU_ASSERT(exception.message == NULL);

    (void)check_decoded_rtps_message(&decoded_buffer, &plain_buffer);

    reset_exception(&exception);
    DDS_Security_OctetSeq_deinit((&decoded_buffer));
  }

  unregister_remote_participant_of_participantB(remote_particpantA_crypto);
  unregister_remote_participants();
  reset_exception(&exception);

  DDS_Security_PropertySeq_deinit(&properties);
  DDS_Security_OctetSeq_deinit((&plain_buffer));
  DDS_Security_OctetSeq_deinit((&encoded_buffer));
  DDS_Security_DatareaderCryptoHandleSeq_deinit(&remote_reader_list);
}

CU_Test(ddssec_builtin_decode_rtps_message, authenticated_256, .init = suite_decode_rtps_message_init, .fini = suite_decode_rtps_message_fini)
{
  decode_rtps_message_authenticated(CRYPTO_TRANSFORMATION_KIND_AES256_GCM, 256, DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION);
}

CU_Test(ddssec_builtin_decode_rtps_message, authenticated_128, .init = suite_decode_rtps_message_init, .fini = suite_decode_rtps_message_fini)
{
  decode_rtps_message_authenticated(CRYPTO_TRANSFORMATION_KIND_AES128_GCM, 128, DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION);
}

CU_Test(ddssec_builtin_decode_rtps_message, only_authenticated_256, .init = suite_decode_rtps_message_init, .fini = suite_decode_rtps_message_fini)
{
  decode_rtps_message_authenticated(CRYPTO_TRANSFORMATION_KIND_AES256_GMAC, 256, DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION);
}

CU_Test(ddssec_builtin_decode_rtps_message, only_authenticated_128, .init = suite_decode_rtps_message_init, .fini = suite_decode_rtps_message_fini)
{
  decode_rtps_message_authenticated(CRYPTO_TRANSFORMATION_KIND_AES128_GMAC, 128, DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION);
}

CU_Test(ddssec_builtin_decode_rtps_message, invalid_args, .init = suite_decode_rtps_message_init, .fini = suite_decode_rtps_message_fini)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatareaderCryptoHandleSeq reader_list;
  DDS_Security_OctetSeq plain_buffer = {0, 0, NULL};
//  DDS_Security_OctetSeq empty_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq encoded_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq decoded_buffer = {0, 0, NULL};
  int32_t index;
  DDS_Security_ParticipantSecurityAttributes attributes;
  DDS_Security_PropertySeq properties;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform != NULL);
  assert(crypto->crypto_transform != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->encode_rtps_message != NULL);
  assert(crypto->crypto_transform->encode_rtps_message != 0);
  CU_ASSERT_FATAL(crypto->crypto_transform->decode_rtps_message != NULL);
  assert(crypto->crypto_transform->decode_rtps_message != 0);

  prepare_participant_security_attributes_and_properties(&attributes, &properties, CRYPTO_TRANSFORMATION_KIND_AES256_GMAC, false);
  register_local_participants(&attributes, &properties);

  initialize_rtps_message(&plain_buffer, DDSRT_BOSEL_NATIVE);

  register_remote_participants(local_participantA_crypto, remote_identities, remote_cryptos);

  /* Now remote participant cypto is in remote_cryptos[0] */

  register_remote_participant_for_participantB(local_participantB_crypto, local_participantA_identity, &remote_particpantA_crypto);

  result = set_remote_participant_tokens(local_participantA_crypto, remote_cryptos[0], local_participantB_crypto, remote_particpantA_crypto);
  CU_ASSERT_FATAL(result);

  reader_list._length = reader_list._maximum = 1;
  reader_list._buffer = DDS_Security_ParticipantCryptoHandleSeq_allocbuf(1);
  reader_list._buffer[0] = remote_cryptos[0];
  index = 0;

  /* Encrypt */
  result = crypto->crypto_transform->encode_rtps_message(
      crypto->crypto_transform,
      &encoded_buffer,
      &plain_buffer,
      local_participantA_crypto,
      &reader_list,
      &index,
      &exception);

  if (!result)
  {
    printf("encode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT_FATAL(result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);

  reset_exception(&exception);

  /* local reader crypto 0 */
  result = crypto->crypto_transform->decode_rtps_message (
      crypto->crypto_transform,
      &decoded_buffer,
      &encoded_buffer,
      0,
      remote_particpantA_crypto,
      &exception);

  if (!result)
  {
    printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* local reader crypto unknown */
  result = crypto->crypto_transform->decode_rtps_message (
      crypto->crypto_transform,
      &decoded_buffer,
      &encoded_buffer,
      1,
      remote_particpantA_crypto,
      &exception);

  if (!result)
  {
    printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* remote writer crypto 0 */
  result = crypto->crypto_transform->decode_rtps_message (
      crypto->crypto_transform,
      &decoded_buffer,
      &encoded_buffer,
      local_participantB_crypto,
      0,
      &exception);

  if (!result)
  {
    printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* remote writer crypto unknown */
  result = crypto->crypto_transform->decode_rtps_message (
      crypto->crypto_transform,
      &decoded_buffer,
      &encoded_buffer,
      local_participantB_crypto,
      1,
      &exception);

  if (!result)
  {
    printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  unregister_remote_participant_of_participantB(remote_particpantA_crypto);
  unregister_remote_participants();
  reset_exception(&exception);

  DDS_Security_OctetSeq_deinit((&plain_buffer));
  DDS_Security_OctetSeq_deinit((&encoded_buffer));
  DDS_Security_DatareaderCryptoHandleSeq_deinit(&reader_list);
  DDS_Security_PropertySeq_deinit(&properties);
  unregister_local_participants();
}

CU_Test(ddssec_builtin_decode_rtps_message, invalid_data, .init = suite_decode_rtps_message_init, .fini = suite_decode_rtps_message_fini)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatareaderCryptoHandleSeq reader_list;
  DDS_Security_OctetSeq plain_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq encoded_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq decoded_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq corrupt_buffer = {0, 0, NULL};
  session_key_material *session_keys;
  int32_t index;
  DDS_Security_ParticipantSecurityAttributes attributes;
  DDS_Security_PropertySeq properties;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform != NULL);
  assert(crypto->crypto_transform != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->encode_rtps_message != NULL);
  assert(crypto->crypto_transform->encode_rtps_message != 0);
  CU_ASSERT_FATAL(crypto->crypto_transform->decode_rtps_message != NULL);
  assert(crypto->crypto_transform->decode_rtps_message != 0);

  prepare_participant_security_attributes_and_properties(&attributes, &properties, CRYPTO_TRANSFORMATION_KIND_AES256_GMAC, false);
  register_local_participants(&attributes, &properties);
  initialize_rtps_message(&plain_buffer, DDSRT_BOSEL_NATIVE);

  session_keys = get_local_participant_session(local_participantA_crypto);
  session_keys->master_key_material->transformation_kind = CRYPTO_TRANSFORMATION_KIND_AES256_GCM;
  session_keys->key_size = 256;

  set_protection_kind(local_participantA_crypto, DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION);

  register_remote_participants(local_participantA_crypto, remote_identities, remote_cryptos);

  set_protection_kind(local_participantB_crypto, DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION);

  register_remote_participant_for_participantB(local_participantB_crypto, local_participantA_identity, &remote_particpantA_crypto);

  result = set_remote_participant_tokens(local_participantA_crypto, remote_cryptos[0], local_participantB_crypto, remote_particpantA_crypto);
  CU_ASSERT_FATAL(result);

  reader_list._length = reader_list._maximum = 1;
  reader_list._buffer = DDS_Security_ParticipantCryptoHandleSeq_allocbuf(1);
  reader_list._buffer[0] = remote_cryptos[0];
  index = 0;

  /* Encrypt the datawriter submessage. */
  result = crypto->crypto_transform->encode_rtps_message(
      crypto->crypto_transform,
      &encoded_buffer,
      &plain_buffer,
      local_participantA_crypto,
      &reader_list,
      &index,
      &exception);

  if (!result)
  {
    printf("encode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT_FATAL(result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);

  reset_exception(&exception);

  /* Incorrect prefix id */
  {
    struct submsg_header *prefix;

    memset(&corrupt_buffer, 0, sizeof(corrupt_buffer));
    DDS_Security_OctetSeq_copy(&corrupt_buffer, &encoded_buffer);

    prefix = get_submsg(corrupt_buffer._buffer + 20, 1);

    set_submsg_header(prefix, 0x15, prefix->flags, prefix->length);

    result = crypto->crypto_transform->decode_rtps_message (
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_participantB_crypto,
        remote_particpantA_crypto,
        &exception);

    if (!result)
    {
      printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    DDS_Security_OctetSeq_deinit(&corrupt_buffer);
  }

  /* Incorrect prefix length */
  {
    struct submsg_header *prefix;

    memset(&corrupt_buffer, 0, sizeof(corrupt_buffer));
    DDS_Security_OctetSeq_copy(&corrupt_buffer, &encoded_buffer);

    prefix = get_submsg(corrupt_buffer._buffer + 20, 1);

    set_submsg_header(prefix, prefix->id, 0, prefix->length);

    result = crypto->crypto_transform->decode_rtps_message (
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_participantB_crypto,
        remote_particpantA_crypto,
        &exception);

    if (!result)
    {
      printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    DDS_Security_OctetSeq_deinit(&corrupt_buffer);
  }

  /* Incorrect body id */
  {
    struct submsg_header *body;

    memset(&corrupt_buffer, 0, sizeof(corrupt_buffer));
    DDS_Security_OctetSeq_copy(&corrupt_buffer, &encoded_buffer);

    body = get_submsg(corrupt_buffer._buffer + 20, 2);

    set_submsg_header(body, 0x15, body->flags, body->length);

    result = crypto->crypto_transform->decode_rtps_message (
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_participantB_crypto,
        remote_particpantA_crypto,
        &exception);

    if (!result)
    {
      printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    DDS_Security_OctetSeq_deinit(&corrupt_buffer);
  }

  /* Incorrect body length */
  {
    struct submsg_header *body;

    memset(&corrupt_buffer, 0, sizeof(corrupt_buffer));
    DDS_Security_OctetSeq_copy(&corrupt_buffer, &encoded_buffer);

    body = get_submsg(corrupt_buffer._buffer + 20, 2);

    set_submsg_header(body, body->id, body->flags, 1000);

    result = crypto->crypto_transform->decode_rtps_message (
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_participantB_crypto,
        remote_particpantA_crypto,
        &exception);

    if (!result)
    {
      printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    DDS_Security_OctetSeq_deinit(&corrupt_buffer);
  }

  /* Incorrect postfix id */
  {
    struct submsg_header *postfix;

    memset(&corrupt_buffer, 0, sizeof(corrupt_buffer));
    DDS_Security_OctetSeq_copy(&corrupt_buffer, &encoded_buffer);

    postfix = get_submsg(corrupt_buffer._buffer + 20, 3);

    set_submsg_header(postfix, 0x15, postfix->flags, postfix->length);

    result = crypto->crypto_transform->decode_rtps_message (
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_participantB_crypto,
        remote_particpantA_crypto,
        &exception);

    if (!result)
    {
      printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    DDS_Security_OctetSeq_deinit(&corrupt_buffer);
  }

  /* Incorrect postfix length */
  {
    struct submsg_header *postfix;

    memset(&corrupt_buffer, 0, sizeof(corrupt_buffer));
    DDS_Security_OctetSeq_copy(&corrupt_buffer, &encoded_buffer);

    postfix = get_submsg(corrupt_buffer._buffer + 20, 3);

    set_submsg_header(postfix, postfix->id, postfix->flags, 1000);

    result = crypto->crypto_transform->decode_rtps_message (
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_participantB_crypto,
        remote_particpantA_crypto,
        &exception);

    if (!result)
    {
      printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    DDS_Security_OctetSeq_deinit(&corrupt_buffer);
  }

  /* Incorrect postfix length */
  {
    struct submsg_header *postfix;

    memset(&corrupt_buffer, 0, sizeof(corrupt_buffer));
    DDS_Security_OctetSeq_copy(&corrupt_buffer, &encoded_buffer);

    postfix = get_submsg(corrupt_buffer._buffer + 20, 3);

    set_submsg_header(postfix, postfix->id, postfix->flags, (uint16_t)(postfix->length - 20));

    result = crypto->crypto_transform->decode_rtps_message (
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_participantB_crypto,
        remote_particpantA_crypto,
        &exception);

    if (!result)
    {
      printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    DDS_Security_OctetSeq_deinit(&corrupt_buffer);
  }

  /* incorrect transformation kind */
  {
    struct crypto_header *header;

    memset(&corrupt_buffer, 0, sizeof(corrupt_buffer));
    DDS_Security_OctetSeq_copy(&corrupt_buffer, &encoded_buffer);

    header = get_crypto_header(corrupt_buffer._buffer + 20);
    header->transform_identifier.transformation_kind[3] = CRYPTO_TRANSFORMATION_KIND_AES256_GMAC;

    result = crypto->crypto_transform->decode_rtps_message (
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_participantB_crypto,
        remote_particpantA_crypto,
        &exception);

    if (!result)
    {
      printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    DDS_Security_OctetSeq_deinit(&corrupt_buffer);
  }

  /* incorrect session id */
  {
    struct crypto_header *header;

    memset(&corrupt_buffer, 0, sizeof(corrupt_buffer));
    DDS_Security_OctetSeq_copy(&corrupt_buffer, &encoded_buffer);

    header = get_crypto_header(corrupt_buffer._buffer + 20);
    header->session_id[0] = (unsigned char)(header->session_id[0] + 1);

    result = crypto->crypto_transform->decode_rtps_message (
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_participantB_crypto,
        remote_particpantA_crypto,
        &exception);

    if (!result)
    {
      printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    DDS_Security_OctetSeq_deinit(&corrupt_buffer);
  }

  /* incorrect init vector suffix */
  {
    struct crypto_header *header;

    memset(&corrupt_buffer, 0, sizeof(corrupt_buffer));
    DDS_Security_OctetSeq_copy(&corrupt_buffer, &encoded_buffer);

    header = get_crypto_header(corrupt_buffer._buffer + 20);
    header->init_vector_suffix[0] = (unsigned char)(header->init_vector_suffix[0] + 1);

    result = crypto->crypto_transform->decode_rtps_message (
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_participantB_crypto,
        remote_particpantA_crypto,
        &exception);

    if (!result)
    {
      printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    DDS_Security_OctetSeq_deinit(&corrupt_buffer);
  }

  /* incorrect encoded data */
  {
    struct submsg_header *body;
    unsigned char *data;

    memset(&corrupt_buffer, 0, sizeof(corrupt_buffer));
    DDS_Security_OctetSeq_copy(&corrupt_buffer, &encoded_buffer);

    body = get_submsg(corrupt_buffer._buffer + 20, 2);
    data = (unsigned char *)(body + 1);
    data[0] = (unsigned char)(data[0] + 1);

    result = crypto->crypto_transform->decode_rtps_message (
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_participantB_crypto,
        remote_particpantA_crypto,
        &exception);

    if (!result)
    {
      printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    DDS_Security_OctetSeq_deinit(&corrupt_buffer);
  }

  /* footer incorrect common mac */
  {
    struct crypto_footer *footer;

    memset(&corrupt_buffer, 0, sizeof(corrupt_buffer));
    DDS_Security_OctetSeq_copy(&corrupt_buffer, &encoded_buffer);

    footer = get_crypto_footer(corrupt_buffer._buffer);
    footer->common_mac[0] = (unsigned char)(footer->common_mac[0] + 1);

    result = crypto->crypto_transform->decode_rtps_message (
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_participantB_crypto,
        remote_particpantA_crypto,
        &exception);

    if (!result)
    {
      printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    DDS_Security_OctetSeq_deinit(&corrupt_buffer);
  }

  /* footer missing reader_specific mac  */
  {
    struct crypto_footer *footer;
    uint32_t len;

    memset(&corrupt_buffer, 0, sizeof(corrupt_buffer));
    DDS_Security_OctetSeq_copy(&corrupt_buffer, &encoded_buffer);

    footer = get_crypto_footer(corrupt_buffer._buffer);
    len = ddsrt_bswap4u(*(uint32_t *)footer->length);
    CU_ASSERT(len == 1);
    memset(footer->length, 0, 4);

    result = crypto->crypto_transform->decode_rtps_message (
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_participantB_crypto,
        remote_particpantA_crypto,
        &exception);

    if (!result)
    {
      printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    DDS_Security_OctetSeq_deinit(&corrupt_buffer);
  }

#if 0
    /* footer incorrect reader_specific mac id  */
    {
        struct crypto_footer *footer;
        struct receiver_specific_mac *rmac;
        uint32_t len;

        memset(&corrupt_buffer, 0, sizeof(corrupt_buffer));
        DDS_Security_OctetSeq_copy(&corrupt_buffer, &encoded_buffer);

        footer = get_crypto_footer(corrupt_buffer._buffer);
        len = ddsrt_bswap4u(*(uint32_t *)footer->length);
        CU_ASSERT(len == 1);

        rmac = (struct receiver_specific_mac *)(footer + 1);
        rmac->receiver_mac_key_id[0] += 1;

        result = crypto->crypto_transform->decode_rtps_message (
                crypto->crypto_transform,
                &decoded_buffer,
                &corrupt_buffer,
                local_participantB_crypto,
                remote_particpantA_crypto,
                &exception);

        if (!result) {
            printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
        }

        CU_ASSERT(!result);
        CU_ASSERT(exception.code != 0);
        CU_ASSERT(exception.message != NULL);

        reset_exception(&exception);

        DDS_Security_OctetSeq_deinit(&corrupt_buffer);
    }

    /* footer incorrect reader_specific mac   */
    {
        struct crypto_footer *footer;
        struct receiver_specific_mac *rmac;
        uint32_t len;

        memset(&corrupt_buffer, 0, sizeof(corrupt_buffer));
        DDS_Security_OctetSeq_copy(&corrupt_buffer, &encoded_buffer);

        footer = get_crypto_footer(corrupt_buffer._buffer);
        len = ddsrt_bswap4u(*(uint32_t *)footer->length);
        CU_ASSERT(len == 1);

        rmac = (struct receiver_specific_mac *)(footer + 1);
        rmac->receiver_mac[0] += 1;

        result = crypto->crypto_transform->decode_rtps_message (
                crypto->crypto_transform,
                &decoded_buffer,
                &corrupt_buffer,
                local_participantB_crypto,
                remote_particpantA_crypto,
                &exception);

        if (!result) {
            printf("decode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
        }

        CU_ASSERT(!result);
        CU_ASSERT(exception.code != 0);
        CU_ASSERT(exception.message != NULL);

        reset_exception(&exception);

        DDS_Security_OctetSeq_deinit(&corrupt_buffer);
    }
#endif

  unregister_remote_participant_of_participantB(remote_particpantA_crypto);
  unregister_remote_participants();
  reset_exception(&exception);

  DDS_Security_OctetSeq_deinit((&plain_buffer));
  DDS_Security_OctetSeq_deinit((&encoded_buffer));
  DDS_Security_DatareaderCryptoHandleSeq_deinit(&reader_list);
  DDS_Security_PropertySeq_deinit(&properties);
  unregister_local_participants();
}

