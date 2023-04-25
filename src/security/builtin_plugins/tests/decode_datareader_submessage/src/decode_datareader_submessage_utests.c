// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

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

#define TEST_SHARED_SECRET_SIZE 32

static struct plugins_hdl *plugins = NULL;
static dds_security_cryptography *crypto = NULL;

static DDS_Security_IdentityHandle local_participant_identity = 1;
static DDS_Security_IdentityHandle remote_participant_identity = 2;

static DDS_Security_ParticipantCryptoHandle local_participant_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_ParticipantCryptoHandle remote_participant_handle = DDS_SECURITY_HANDLE_NIL;

static DDS_Security_SharedSecretHandleImpl *shared_secret_handle_impl = NULL;
static DDS_Security_SharedSecretHandle shared_secret_handle;

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

struct seq_number
{
  int high;
  unsigned low;
};

struct heartbeat
{
  struct submsg_header smhdr;
  uint32_t readerId;
  uint32_t writerId;
  struct seq_number firstSN;
  struct seq_number lastSN;
  int count;
};

static struct heartbeat heartbeat;

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

static void initialize_heartbeat(void)
{
  heartbeat.smhdr.id = 0x07;
  heartbeat.smhdr.flags = 1;
  heartbeat.smhdr.length = sizeof(heartbeat) - sizeof(struct submsg_header);
  heartbeat.readerId = 0xA1B2C3D4;
  heartbeat.writerId = 0xE5F6A7B0;
  heartbeat.firstSN.high = 0;
  heartbeat.firstSN.low = 1;
  heartbeat.lastSN.high = 20;
  heartbeat.lastSN.low = 500;
  heartbeat.count = 1021;
}

static int register_local_participant(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PermissionsHandle participant_permissions = 3; //valid dummy value
  DDS_Security_PropertySeq participant_properties;
  DDS_Security_ParticipantSecurityAttributes participant_security_attributes;

  memset(&participant_properties, 0, sizeof(participant_properties));
  memset(&participant_security_attributes, 0, sizeof(participant_security_attributes));

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
    local_participant_handle = DDS_SECURITY_HANDLE_NIL;
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
    remote_participant_handle = DDS_SECURITY_HANDLE_NIL;
    reset_exception(&exception);
  }
}

static void prepare_endpoint_security_attributes_and_properties(DDS_Security_EndpointSecurityAttributes *attributes,
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
    attributes->is_submessage_protected = true;
    attributes->plugin_endpoint_attributes |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
    if (is_origin_authenticated)
    {
      attributes->plugin_endpoint_attributes |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ORIGIN_AUTHENTICATED;
    }
    break;

  case CRYPTO_TRANSFORMATION_KIND_AES256_GMAC:
  case CRYPTO_TRANSFORMATION_KIND_AES128_GMAC:
    attributes->is_submessage_protected = true;
    if (is_origin_authenticated)
    {
      attributes->plugin_endpoint_attributes |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ORIGIN_AUTHENTICATED;
    }
    break;

  default:
    assert(0);
    break;
  }
}

static DDS_Security_DatawriterCryptoHandle register_local_datawriter(DDS_Security_EndpointSecurityAttributes *datawriter_security_attributes, DDS_Security_PropertySeq *datawriter_properties)
{
  DDS_Security_DatawriterCryptoHandle writer_crypto;
  DDS_Security_SecurityException exception = {NULL, 0, 0};

  writer_crypto =
      crypto->crypto_key_factory->register_local_datawriter(
          crypto->crypto_key_factory,
          local_participant_handle,
          datawriter_properties,
          datawriter_security_attributes,
          &exception);

  if (writer_crypto == 0)
  {
    printf("register_local_datawriter: %s\n", exception.message ? exception.message : "Error message missing");
  }

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

static DDS_Security_DatareaderCryptoHandle register_local_datareader(DDS_Security_EndpointSecurityAttributes *datareader_security_attributes, DDS_Security_PropertySeq *datareader_properties)
{
  DDS_Security_DatareaderCryptoHandle reader_crypto;
  DDS_Security_SecurityException exception = {NULL, 0, 0};

  reader_crypto =
      crypto->crypto_key_factory->register_local_datareader(
          crypto->crypto_key_factory,
          local_participant_handle,
          datareader_properties,
          datareader_security_attributes,
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

static bool set_remote_datareader_tokens(
    DDS_Security_DatareaderCryptoHandle nodeA_local_reader_crypto,
    DDS_Security_DatawriterCryptoHandle nodeA_remote_writer_crypto,
    DDS_Security_DatawriterCryptoHandle nodeB_local_writer_crypto,
    DDS_Security_DatareaderCryptoHandle nodeB_remote_reader_crypto)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatareaderCryptoTokenSeq tokens;

  memset(&tokens, 0, sizeof(tokens));

  /* Now call the function. */
  result = crypto->crypto_key_exchange->create_local_datareader_crypto_tokens(
      crypto->crypto_key_exchange,
      &tokens,
      nodeA_local_reader_crypto,
      nodeA_remote_writer_crypto,
      &exception);

  if (result)
  {
    result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
        crypto->crypto_key_exchange,
        nodeB_local_writer_crypto,
        nodeB_remote_reader_crypto,
        &tokens,
        &exception);
  }

  DDS_Security_DataHolderSeq_deinit(&tokens);

  if (result)
  {
    result = crypto->crypto_key_exchange->create_local_datawriter_crypto_tokens(
        crypto->crypto_key_exchange,
        &tokens,
        nodeB_local_writer_crypto,
        nodeB_remote_reader_crypto,
        &exception);
  }

  if (result)
  {
    result = crypto->crypto_key_exchange->set_remote_datawriter_crypto_tokens(
        crypto->crypto_key_exchange,
        nodeA_local_reader_crypto,
        nodeA_remote_writer_crypto,
        &tokens,
        &exception);
  }

  DDS_Security_DataHolderSeq_deinit(&tokens);

  return (bool)result;
}

static void suite_decode_datareader_submessage_init(void)
{
  allocate_shared_secret();
  initialize_heartbeat();

  CU_ASSERT_FATAL ((plugins = load_plugins(
                      NULL    /* Access Control */,
                      NULL    /* Authentication */,
                      &crypto /* Cryptograpy    */,
                      NULL)) != NULL);

  CU_ASSERT_EQUAL_FATAL (register_local_participant(), 0);
  CU_ASSERT_EQUAL_FATAL (register_remote_participant(), 0);
}

static void suite_decode_datareader_submessage_fini(void)
{
  unregister_local_participant();
  unregister_remote_participant();
  deallocate_shared_secret();
  unload_plugins(plugins);
}

static void initialize_data_submessage(DDS_Security_OctetSeq *submsg)
{
  uint32_t length = sizeof(struct heartbeat);
  unsigned char *buffer;

  buffer = ddsrt_malloc(length);

  memcpy(buffer, &heartbeat, length);

  submsg->_length = submsg->_maximum = length;
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
  submsg = get_submsg(data, 3);
  return (struct crypto_footer *)(submsg + 1);
}

static void decode_datareader_submessage_not_signed(
    DDS_Security_CryptoTransformKind_Enum transformation_kind)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatareaderCryptoHandle nodeA_local_reader_crypto;
  DDS_Security_DatawriterCryptoHandle nodeB_local_writer_crypto;
  DDS_Security_DatareaderCryptoHandle nodeB_remote_reader_crypto;
  DDS_Security_DatawriterCryptoHandle nodeA_remote_writer_crypto;
  DDS_Security_DatawriterCryptoHandleSeq writer_list;
  DDS_Security_OctetSeq plain_buffer;
  DDS_Security_OctetSeq encoded_buffer;
  DDS_Security_OctetSeq decoded_buffer;
  DDS_Security_EndpointSecurityAttributes datawriter_security_attributes;
  DDS_Security_PropertySeq datawriter_properties;
  DDS_Security_EndpointSecurityAttributes datareader_security_attributes;
  DDS_Security_PropertySeq datareader_properties;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform != NULL);
  assert(crypto->crypto_transform != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->encode_datareader_submessage != NULL);
  assert(crypto->crypto_transform->encode_datareader_submessage != 0);
  CU_ASSERT_FATAL(crypto->crypto_transform->decode_datareader_submessage != NULL);
  assert(crypto->crypto_transform->decode_datareader_submessage != 0);

  initialize_data_submessage(&plain_buffer);

  prepare_endpoint_security_attributes_and_properties(&datareader_security_attributes, &datareader_properties, transformation_kind, false);
  prepare_endpoint_security_attributes_and_properties(&datawriter_security_attributes, &datawriter_properties, transformation_kind, false);

  nodeA_local_reader_crypto = register_local_datareader(&datareader_security_attributes, &datareader_properties);
  CU_ASSERT_FATAL(nodeA_local_reader_crypto != 0);

  nodeB_local_writer_crypto = register_local_datawriter(&datawriter_security_attributes, &datawriter_properties);
  CU_ASSERT_FATAL(nodeB_local_writer_crypto != 0);

  nodeA_remote_writer_crypto = register_remote_datawriter(nodeA_local_reader_crypto);
  CU_ASSERT_FATAL(nodeA_remote_writer_crypto != 0);

  nodeB_remote_reader_crypto = register_remote_datareader(nodeB_local_writer_crypto);
  CU_ASSERT_FATAL(nodeB_remote_reader_crypto != 0);

  result = set_remote_datareader_tokens(nodeA_local_reader_crypto, nodeA_remote_writer_crypto, nodeB_local_writer_crypto, nodeB_remote_reader_crypto);
  CU_ASSERT_FATAL(result);

  writer_list._length = writer_list._maximum = 1;
  writer_list._buffer = DDS_Security_DatawriterCryptoHandleSeq_allocbuf(1);
  writer_list._buffer[0] = nodeA_remote_writer_crypto;

  /* Encrypt the datawriter submessage. */
  result = crypto->crypto_transform->encode_datareader_submessage(
      crypto->crypto_transform,
      &encoded_buffer,
      &plain_buffer,
      nodeA_local_reader_crypto,
      &writer_list,
      &exception);

  if (!result)
  {
    printf("encode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT_FATAL(result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);

  reset_exception(&exception);

  /* Decrypt the datareader submessage */
  result = crypto->crypto_transform->decode_datareader_submessage(
      crypto->crypto_transform,
      &decoded_buffer,
      &encoded_buffer,
      nodeB_local_writer_crypto,
      nodeB_remote_reader_crypto,
      &exception);

  if (!result)
  {
    printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);
  if (exception.message)
  {
    printf("Decoding failed: %s\n", exception.message);
  }
  CU_ASSERT_FATAL(result);
  CU_ASSERT_FATAL(decoded_buffer._length == plain_buffer._length);

  reset_exception(&exception);

  if (memcmp(decoded_buffer._buffer, plain_buffer._buffer, plain_buffer._length) != 0)
  {
    CU_FAIL("decode submessage is not equal to original");
  }

  unregister_datawriter(nodeA_remote_writer_crypto);
  unregister_datareader(nodeB_remote_reader_crypto);
  unregister_datareader(nodeA_local_reader_crypto);
  unregister_datawriter(nodeB_local_writer_crypto);

  DDS_Security_DatareaderCryptoHandleSeq_deinit(&writer_list);

  DDS_Security_OctetSeq_deinit(&plain_buffer);
  DDS_Security_OctetSeq_deinit(&encoded_buffer);
  DDS_Security_OctetSeq_deinit(&decoded_buffer);

  DDS_Security_PropertySeq_deinit(&datareader_properties);
  DDS_Security_PropertySeq_deinit(&datawriter_properties);
}

CU_Test(ddssec_builtin_decode_datareader_submessage, encoded_256, .init = suite_decode_datareader_submessage_init, .fini = suite_decode_datareader_submessage_fini)
{
  decode_datareader_submessage_not_signed(CRYPTO_TRANSFORMATION_KIND_AES256_GCM);
}

CU_Test(ddssec_builtin_decode_datareader_submessage, encoded_128, .init = suite_decode_datareader_submessage_init, .fini = suite_decode_datareader_submessage_fini)
{
  decode_datareader_submessage_not_signed(CRYPTO_TRANSFORMATION_KIND_AES128_GCM);
}

CU_Test(ddssec_builtin_decode_datareader_submessage, not_encoded_256, .init = suite_decode_datareader_submessage_init, .fini = suite_decode_datareader_submessage_fini)
{
  decode_datareader_submessage_not_signed(CRYPTO_TRANSFORMATION_KIND_AES256_GMAC);
}

CU_Test(ddssec_builtin_decode_datareader_submessage, not_encoded_128, .init = suite_decode_datareader_submessage_init, .fini = suite_decode_datareader_submessage_fini)
{
  decode_datareader_submessage_not_signed(CRYPTO_TRANSFORMATION_KIND_AES128_GMAC);
}

static void decode_datareader_submessage_signed(
    DDS_Security_CryptoTransformKind_Enum transformation_kind)
{
  const uint32_t LIST_SIZE = 4u;
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatareaderCryptoHandle local_reader_crypto;
  DDS_Security_DatawriterCryptoHandleSeq local_writer_list;
  DDS_Security_DatareaderCryptoHandleSeq remote_reader_list;
  DDS_Security_DatawriterCryptoHandleSeq remote_writer_list;
  DDS_Security_OctetSeq plain_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq encoded_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq decoded_buffer = {0, 0, NULL};
  uint32_t i;
  DDS_Security_EndpointSecurityAttributes datawriter_security_attributes;
  DDS_Security_PropertySeq datawriter_properties;
  DDS_Security_EndpointSecurityAttributes datareader_security_attributes;
  DDS_Security_PropertySeq datareader_properties;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform != NULL);
  assert(crypto->crypto_transform != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->encode_datareader_submessage != NULL);
  assert(crypto->crypto_transform->encode_datareader_submessage != 0);
  CU_ASSERT_FATAL(crypto->crypto_transform->decode_datareader_submessage != NULL);
  assert(crypto->crypto_transform->decode_datareader_submessage != 0);

  initialize_data_submessage(&plain_buffer);

  prepare_endpoint_security_attributes_and_properties(&datareader_security_attributes, &datareader_properties, transformation_kind, true);
  prepare_endpoint_security_attributes_and_properties(&datawriter_security_attributes, &datawriter_properties, transformation_kind, true);

  local_reader_crypto = register_local_datareader(&datareader_security_attributes, &datareader_properties);
  CU_ASSERT_FATAL(local_reader_crypto != 0);

  local_writer_list._length = local_writer_list._maximum = LIST_SIZE;
  local_writer_list._buffer = DDS_Security_DatawriterCryptoHandleSeq_allocbuf(LIST_SIZE);

  remote_reader_list._length = remote_reader_list._maximum = LIST_SIZE;
  remote_reader_list._buffer = DDS_Security_DatareaderCryptoHandleSeq_allocbuf(LIST_SIZE);

  remote_writer_list._length = remote_writer_list._maximum = LIST_SIZE;
  remote_writer_list._buffer = DDS_Security_DatawriterCryptoHandleSeq_allocbuf(LIST_SIZE);

  for (i = 0; i < LIST_SIZE; i++)
  {
    DDS_Security_DatawriterCryptoHandle local_writer_crypto;
    DDS_Security_DatareaderCryptoHandle remote_reader_crypto;
    DDS_Security_DatawriterCryptoHandle remote_writer_crypto;

    local_writer_crypto = register_local_datawriter(&datawriter_security_attributes, &datawriter_properties);
    CU_ASSERT_FATAL(local_writer_crypto != 0);

    remote_writer_crypto = register_remote_datawriter(local_reader_crypto);
    CU_ASSERT_FATAL(remote_writer_crypto != 0);

    remote_reader_crypto = register_remote_datareader(local_writer_crypto);
    CU_ASSERT_FATAL(remote_reader_crypto != 0);

    result = set_remote_datareader_tokens(local_reader_crypto, remote_writer_crypto, local_writer_crypto, remote_reader_crypto);
    CU_ASSERT_FATAL(result);

    local_writer_list._buffer[i] = local_writer_crypto;
    remote_reader_list._buffer[i] = remote_reader_crypto;
    remote_writer_list._buffer[i] = remote_writer_crypto;
  }

  /* Encrypt the datareader submessage. */
  result = crypto->crypto_transform->encode_datareader_submessage(
      crypto->crypto_transform,
      &encoded_buffer,
      &plain_buffer,
      local_reader_crypto,
      &remote_writer_list,
      &exception);

  if (!result)
  {
    printf("encode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT_FATAL(result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);

  reset_exception(&exception);

  /* Decrypt the datareader submessage */
  for (i = 0; i < LIST_SIZE; i++)
  {
    result = crypto->crypto_transform->decode_datareader_submessage(
        crypto->crypto_transform,
        &decoded_buffer,
        &encoded_buffer,
        local_writer_list._buffer[i],
        remote_reader_list._buffer[i],
        &exception);

    if (!result)
    {
      printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL(result);
    CU_ASSERT(exception.code == 0);
    CU_ASSERT(exception.message == NULL);
    CU_ASSERT_FATAL(decoded_buffer._length == plain_buffer._length);

    if (memcmp(decoded_buffer._buffer, plain_buffer._buffer, plain_buffer._length) != 0)
    {
      CU_FAIL("decode submessage is not equal to original");
    }

    reset_exception(&exception);
    DDS_Security_OctetSeq_deinit(&decoded_buffer);
  }

  for (i = 0; i < LIST_SIZE; i++)
  {
    unregister_datareader(remote_reader_list._buffer[i]);
    unregister_datawriter(remote_writer_list._buffer[i]);
    unregister_datawriter(local_writer_list._buffer[i]);
    local_writer_list._buffer[i] = 0;
    remote_reader_list._buffer[i] = 0;
    remote_writer_list._buffer[i] = 0;
  }
  unregister_datareader(local_reader_crypto);

  DDS_Security_DatareaderCryptoHandleSeq_deinit(&local_writer_list);
  DDS_Security_DatareaderCryptoHandleSeq_deinit(&remote_reader_list);
  DDS_Security_DatawriterCryptoHandleSeq_deinit(&remote_writer_list);

  DDS_Security_OctetSeq_deinit(&plain_buffer);
  DDS_Security_OctetSeq_deinit(&encoded_buffer);

  DDS_Security_PropertySeq_deinit(&datareader_properties);
  DDS_Security_PropertySeq_deinit(&datawriter_properties);
}

CU_Test(ddssec_builtin_decode_datareader_submessage, signed_256, .init = suite_decode_datareader_submessage_init, .fini = suite_decode_datareader_submessage_fini)
{
  decode_datareader_submessage_signed(CRYPTO_TRANSFORMATION_KIND_AES256_GCM);
}

CU_Test(ddssec_builtin_decode_datareader_submessage, signed_128, .init = suite_decode_datareader_submessage_init, .fini = suite_decode_datareader_submessage_fini)
{
  decode_datareader_submessage_signed(CRYPTO_TRANSFORMATION_KIND_AES128_GCM);
}

CU_Test(ddssec_builtin_decode_datareader_submessage, only_signed_256, .init = suite_decode_datareader_submessage_init, .fini = suite_decode_datareader_submessage_fini)
{
  decode_datareader_submessage_signed(CRYPTO_TRANSFORMATION_KIND_AES256_GMAC);
}

CU_Test(ddssec_builtin_decode_datareader_submessage, only_signed_128, .init = suite_decode_datareader_submessage_init, .fini = suite_decode_datareader_submessage_fini)
{
  decode_datareader_submessage_signed(CRYPTO_TRANSFORMATION_KIND_AES128_GMAC);
}

CU_Test(ddssec_builtin_decode_datareader_submessage, invalid_args, .init = suite_decode_datareader_submessage_init, .fini = suite_decode_datareader_submessage_fini)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatareaderCryptoHandle local_reader_crypto;
  DDS_Security_DatawriterCryptoHandle local_writer_crypto;
  DDS_Security_DatareaderCryptoHandle remote_reader_crypto;
  DDS_Security_DatawriterCryptoHandle remote_writer_crypto;
  DDS_Security_DatawriterCryptoHandleSeq writer_list;
  DDS_Security_OctetSeq plain_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq empty_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq encoded_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq decoded_buffer = {0, 0, NULL};
  DDS_Security_EndpointSecurityAttributes datawriter_security_attributes;
  DDS_Security_PropertySeq datawriter_properties;
  DDS_Security_EndpointSecurityAttributes datareader_security_attributes;
  DDS_Security_PropertySeq datareader_properties;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform != NULL);
  assert(crypto->crypto_transform != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->encode_datareader_submessage != NULL);
  assert(crypto->crypto_transform->encode_datareader_submessage != 0);
  CU_ASSERT_FATAL(crypto->crypto_transform->decode_datareader_submessage != NULL);
  assert(crypto->crypto_transform->decode_datareader_submessage != 0);

  initialize_data_submessage(&plain_buffer);

  prepare_endpoint_security_attributes_and_properties(&datareader_security_attributes, &datareader_properties, CRYPTO_TRANSFORMATION_KIND_AES256_GMAC, false);
  prepare_endpoint_security_attributes_and_properties(&datawriter_security_attributes, &datawriter_properties, CRYPTO_TRANSFORMATION_KIND_AES256_GMAC, false);

  memset(&empty_buffer, 0, sizeof(empty_buffer));

  local_reader_crypto = register_local_datareader(&datareader_security_attributes, &datareader_properties);
  CU_ASSERT_FATAL(local_reader_crypto != 0);

  local_writer_crypto = register_local_datawriter(&datawriter_security_attributes, &datawriter_properties);
  CU_ASSERT_FATAL(local_writer_crypto != 0);

  remote_writer_crypto = register_remote_datawriter(local_reader_crypto);
  CU_ASSERT_FATAL(remote_writer_crypto != 0);

  remote_reader_crypto = register_remote_datareader(local_writer_crypto);
  CU_ASSERT_FATAL(remote_reader_crypto != 0);

  result = set_remote_datareader_tokens(local_reader_crypto, remote_writer_crypto, local_writer_crypto, remote_reader_crypto);
  CU_ASSERT_FATAL(result);

  writer_list._length = writer_list._maximum = 1;
  writer_list._buffer = DDS_Security_DatawriterCryptoHandleSeq_allocbuf(1);
  writer_list._buffer[0] = remote_writer_crypto;

  /* Encrypt the datawriter submessage. */
  result = crypto->crypto_transform->encode_datareader_submessage(
      crypto->crypto_transform,
      &encoded_buffer,
      &plain_buffer,
      local_reader_crypto,
      &writer_list,
      &exception);

  if (!result)
  {
    printf("encode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT_FATAL(result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);

  reset_exception(&exception);

  /* local writer crypto 0 */
  result = crypto->crypto_transform->decode_datareader_submessage(
      crypto->crypto_transform,
      &decoded_buffer,
      &encoded_buffer,
      0,
      remote_reader_crypto,
      &exception);

  if (!result)
  {
    printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* local writer crypto unknown */
  result = crypto->crypto_transform->decode_datareader_submessage(
      crypto->crypto_transform,
      &decoded_buffer,
      &encoded_buffer,
      1,
      remote_reader_crypto,
      &exception);

  if (!result)
  {
    printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* remote reader crypto 0 */
  result = crypto->crypto_transform->decode_datareader_submessage(
      crypto->crypto_transform,
      &decoded_buffer,
      &encoded_buffer,
      local_writer_crypto,
      0,
      &exception);

  if (!result)
  {
    printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* remote reader crypto unknown */
  result = crypto->crypto_transform->decode_datareader_submessage(
      crypto->crypto_transform,
      &decoded_buffer,
      &encoded_buffer,
      local_writer_crypto,
      1,
      &exception);

  if (!result)
  {
    printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  unregister_datawriter(writer_list._buffer[0]);
  writer_list._buffer[0] = 0;
  unregister_datareader(remote_reader_crypto);
  unregister_datareader(local_reader_crypto);
  unregister_datawriter(local_writer_crypto);

  reset_exception(&exception);

  DDS_Security_DatareaderCryptoHandleSeq_deinit(&writer_list);

  DDS_Security_OctetSeq_deinit(&plain_buffer);
  DDS_Security_OctetSeq_deinit(&empty_buffer);
  DDS_Security_OctetSeq_deinit(&encoded_buffer);
  DDS_Security_OctetSeq_deinit(&decoded_buffer);

  DDS_Security_PropertySeq_deinit(&datareader_properties);
  DDS_Security_PropertySeq_deinit(&datawriter_properties);
}

CU_Test(ddssec_builtin_decode_datareader_submessage, invalid_data, .init = suite_decode_datareader_submessage_init, .fini = suite_decode_datareader_submessage_fini)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatareaderCryptoHandle local_reader_crypto;
  DDS_Security_DatawriterCryptoHandle local_writer_crypto;
  DDS_Security_DatareaderCryptoHandle remote_reader_crypto;
  DDS_Security_DatawriterCryptoHandle remote_writer_crypto;
  DDS_Security_DatawriterCryptoHandleSeq writer_list;
  DDS_Security_OctetSeq plain_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq encoded_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq decoded_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq corrupt_buffer = {0, 0, NULL};
  DDS_Security_EndpointSecurityAttributes datawriter_security_attributes;
  DDS_Security_PropertySeq datawriter_properties;
  DDS_Security_EndpointSecurityAttributes datareader_security_attributes;
  DDS_Security_PropertySeq datareader_properties;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform != NULL);
  assert(crypto->crypto_transform != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->encode_datareader_submessage != NULL);
  assert(crypto->crypto_transform->encode_datareader_submessage != 0);
  CU_ASSERT_FATAL(crypto->crypto_transform->decode_datareader_submessage != NULL);
  assert(crypto->crypto_transform->decode_datareader_submessage != 0);

  initialize_data_submessage(&plain_buffer);

  prepare_endpoint_security_attributes_and_properties(&datareader_security_attributes, &datareader_properties, CRYPTO_TRANSFORMATION_KIND_AES256_GCM, true);
  prepare_endpoint_security_attributes_and_properties(&datawriter_security_attributes, &datawriter_properties, CRYPTO_TRANSFORMATION_KIND_AES256_GCM, true);

  local_reader_crypto = register_local_datareader(&datareader_security_attributes, &datareader_properties);
  CU_ASSERT_FATAL(local_reader_crypto != 0);

  local_writer_crypto = register_local_datawriter(&datawriter_security_attributes, &datawriter_properties);
  CU_ASSERT_FATAL(local_writer_crypto != 0);

  remote_writer_crypto = register_remote_datawriter(local_reader_crypto);
  CU_ASSERT_FATAL(remote_writer_crypto != 0);

  remote_reader_crypto = register_remote_datareader(local_writer_crypto);
  CU_ASSERT_FATAL(remote_reader_crypto != 0);

  result = set_remote_datareader_tokens(local_reader_crypto, remote_writer_crypto, local_writer_crypto, remote_reader_crypto);
  CU_ASSERT_FATAL(result);

  writer_list._length = writer_list._maximum = 1;
  writer_list._buffer = DDS_Security_DatawriterCryptoHandleSeq_allocbuf(1);
  writer_list._buffer[0] = remote_writer_crypto;

  /* Encrypt the datareader submessage. */
  result = crypto->crypto_transform->encode_datareader_submessage(
      crypto->crypto_transform,
      &encoded_buffer,
      &plain_buffer,
      local_reader_crypto,
      &writer_list,
      &exception);

  if (!result)
  {
    printf("encode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
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

    prefix = get_submsg(corrupt_buffer._buffer, 1);

    set_submsg_header(prefix, 0x15, prefix->flags, prefix->length);

    result = crypto->crypto_transform->decode_datareader_submessage(
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_writer_crypto,
        remote_reader_crypto,
        &exception);

    if (!result)
    {
      printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
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

    prefix = get_submsg(corrupt_buffer._buffer, 1);

    set_submsg_header(prefix, prefix->id, 0, prefix->length);

    result = crypto->crypto_transform->decode_datareader_submessage(
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_writer_crypto,
        remote_reader_crypto,
        &exception);

    if (!result)
    {
      printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
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

    body = get_submsg(corrupt_buffer._buffer, 2);

    set_submsg_header(body, 0x15, body->flags, body->length);

    result = crypto->crypto_transform->decode_datareader_submessage(
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_writer_crypto,
        remote_reader_crypto,
        &exception);

    if (!result)
    {
      printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
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

    body = get_submsg(corrupt_buffer._buffer, 2);

    set_submsg_header(body, body->id, body->flags, 1000);

    result = crypto->crypto_transform->decode_datareader_submessage(
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_writer_crypto,
        remote_reader_crypto,
        &exception);

    if (!result)
    {
      printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
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

    postfix = get_submsg(corrupt_buffer._buffer, 3);

    set_submsg_header(postfix, 0x15, postfix->flags, postfix->length);

    result = crypto->crypto_transform->decode_datareader_submessage(
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_writer_crypto,
        remote_reader_crypto,
        &exception);

    if (!result)
    {
      printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
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

    postfix = get_submsg(corrupt_buffer._buffer, 3);

    set_submsg_header(postfix, postfix->id, postfix->flags, 1000);

    result = crypto->crypto_transform->decode_datareader_submessage(
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_writer_crypto,
        remote_reader_crypto,
        &exception);

    if (!result)
    {
      printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
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

    postfix = get_submsg(corrupt_buffer._buffer, 3);

    set_submsg_header(postfix, postfix->id, postfix->flags, (uint16_t)(postfix->length - 20));

    result = crypto->crypto_transform->decode_datareader_submessage(
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_writer_crypto,
        remote_reader_crypto,
        &exception);

    if (!result)
    {
      printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
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

    header = get_crypto_header(corrupt_buffer._buffer);
    header->transform_identifier.transformation_kind[3] = CRYPTO_TRANSFORMATION_KIND_AES256_GMAC;

    result = crypto->crypto_transform->decode_datareader_submessage(
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_writer_crypto,
        remote_reader_crypto,
        &exception);

    if (!result)
    {
      printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
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

    header = get_crypto_header(corrupt_buffer._buffer);
    header->session_id[0] = (unsigned char)(header->session_id[0] + 1);

    result = crypto->crypto_transform->decode_datareader_submessage(
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_writer_crypto,
        remote_reader_crypto,
        &exception);

    if (!result)
    {
      printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
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

    header = get_crypto_header(corrupt_buffer._buffer);
    header->init_vector_suffix[0] = (unsigned char)(header->init_vector_suffix[0] + 1);

    result = crypto->crypto_transform->decode_datareader_submessage(
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_writer_crypto,
        remote_reader_crypto,
        &exception);

    if (!result)
    {
      printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
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

    body = get_submsg(corrupt_buffer._buffer, 2);
    data = (unsigned char *)(body + 1);
    data[0] = (unsigned char)(data[0] + 1);

    result = crypto->crypto_transform->decode_datareader_submessage(
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_writer_crypto,
        remote_reader_crypto,
        &exception);

    if (!result)
    {
      printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
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
    footer->common_mac[0]++;

    result = crypto->crypto_transform->decode_datareader_submessage(
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_writer_crypto,
        remote_reader_crypto,
        &exception);

    if (!result)
    {
      printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
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

    result = crypto->crypto_transform->decode_datareader_submessage(
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_writer_crypto,
        remote_reader_crypto,
        &exception);

    if (!result)
    {
      printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    DDS_Security_OctetSeq_deinit(&corrupt_buffer);
  }

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
    rmac->receiver_mac_key_id[0] = (unsigned char)(rmac->receiver_mac_key_id[0] + 1);

    result = crypto->crypto_transform->decode_datareader_submessage(
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_writer_crypto,
        remote_reader_crypto,
        &exception);

    if (!result)
    {
      printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
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
    rmac->receiver_mac.data[0] = (unsigned char)(rmac->receiver_mac.data[0] + 1);

    result = crypto->crypto_transform->decode_datareader_submessage(
        crypto->crypto_transform,
        &decoded_buffer,
        &corrupt_buffer,
        local_writer_crypto,
        remote_reader_crypto,
        &exception);

    if (!result)
    {
      printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);

    reset_exception(&exception);

    DDS_Security_OctetSeq_deinit(&corrupt_buffer);
  }

  unregister_datawriter(writer_list._buffer[0]);
  writer_list._buffer[0] = 0;
  unregister_datareader(remote_reader_crypto);
  unregister_datareader(local_reader_crypto);
  unregister_datawriter(local_writer_crypto);

  reset_exception(&exception);

  DDS_Security_DatareaderCryptoHandleSeq_deinit(&writer_list);

  DDS_Security_OctetSeq_deinit(&plain_buffer);
  DDS_Security_OctetSeq_deinit(&corrupt_buffer);
  DDS_Security_OctetSeq_deinit(&encoded_buffer);
  DDS_Security_OctetSeq_deinit(&decoded_buffer);

  DDS_Security_PropertySeq_deinit(&datareader_properties);
  DDS_Security_PropertySeq_deinit(&datawriter_properties);
}

CU_Test(ddssec_builtin_decode_datareader_submessage, volatile_sec, .init = suite_decode_datareader_submessage_init, .fini = suite_decode_datareader_submessage_fini)
{
  DDS_Security_boolean result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_DatareaderCryptoHandle local_reader_crypto;
  DDS_Security_DatawriterCryptoHandle local_writer_crypto;
  DDS_Security_DatareaderCryptoHandle remote_reader_crypto;
  DDS_Security_DatawriterCryptoHandle remote_writer_crypto;
  DDS_Security_DatawriterCryptoHandleSeq writer_list;
  DDS_Security_EndpointSecurityAttributes datawriter_security_attributes;
  DDS_Security_PropertySeq datawriter_properties;
  DDS_Security_EndpointSecurityAttributes datareader_security_attributes;
  DDS_Security_PropertySeq datareader_properties;
  DDS_Security_OctetSeq plain_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq encoded_buffer = {0, 0, NULL};
  DDS_Security_OctetSeq decoded_buffer = {0, 0, NULL};

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform != NULL);
  assert(crypto->crypto_transform != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->encode_datareader_submessage != NULL);
  assert(crypto->crypto_transform->encode_datareader_submessage != 0);
  CU_ASSERT_FATAL(crypto->crypto_transform->decode_datareader_submessage != NULL);
  assert(crypto->crypto_transform->decode_datareader_submessage != 0);

  initialize_data_submessage(&plain_buffer);

  prepare_endpoint_security_attributes_and_properties(&datareader_security_attributes, NULL, CRYPTO_TRANSFORMATION_KIND_AES256_GCM, false);
  prepare_endpoint_security_attributes_and_properties(&datawriter_security_attributes, NULL, CRYPTO_TRANSFORMATION_KIND_AES256_GCM, false);

  datareader_properties._length = datareader_properties._maximum = 1;
  datareader_properties._buffer = DDS_Security_PropertySeq_allocbuf(1);
  datareader_properties._buffer[0].name = ddsrt_strdup(DDS_SEC_PROP_BUILTIN_ENDPOINT_NAME);
  datareader_properties._buffer[0].value = ddsrt_strdup("BuiltinParticipantVolatileMessageSecureReader");
  datareader_properties._buffer[0].propagate = false;

  datawriter_properties._length = datawriter_properties._maximum = 1;
  datawriter_properties._buffer = DDS_Security_PropertySeq_allocbuf(1);
  datawriter_properties._buffer[0].name = ddsrt_strdup(DDS_SEC_PROP_BUILTIN_ENDPOINT_NAME);
  datawriter_properties._buffer[0].value = ddsrt_strdup("BuiltinParticipantVolatileMessageSecureWriter");
  datawriter_properties._buffer[0].propagate = false;

  local_writer_crypto =
      crypto->crypto_key_factory->register_local_datawriter(
          crypto->crypto_key_factory,
          local_participant_handle,
          &datawriter_properties,
          &datawriter_security_attributes,
          &exception);
  CU_ASSERT_FATAL(local_writer_crypto != 0);

  local_reader_crypto =
      crypto->crypto_key_factory->register_local_datareader(
          crypto->crypto_key_factory,
          local_participant_handle,
          &datareader_properties,
          &datareader_security_attributes,
          &exception);
  CU_ASSERT_FATAL(local_reader_crypto != 0);

  remote_writer_crypto =
      crypto->crypto_key_factory->register_matched_remote_datawriter(
          crypto->crypto_key_factory,
          local_reader_crypto,
          remote_participant_handle,
          shared_secret_handle,
          &exception);
  CU_ASSERT_FATAL(remote_writer_crypto != 0);

  remote_reader_crypto =
      crypto->crypto_key_factory->register_matched_remote_datareader(
          crypto->crypto_key_factory,
          local_writer_crypto,
          remote_participant_handle,
          shared_secret_handle,
          true,
          &exception);
  CU_ASSERT_FATAL(remote_reader_crypto != 0);

  writer_list._length = writer_list._maximum = 1;
  writer_list._buffer = DDS_Security_DatawriterCryptoHandleSeq_allocbuf(1);
  writer_list._buffer[0] = remote_writer_crypto;

  /* Encrypt the datawriter submessage. */
  result = crypto->crypto_transform->encode_datareader_submessage(
      crypto->crypto_transform,
      &encoded_buffer,
      &plain_buffer,
      local_reader_crypto,
      &writer_list,
      &exception);

  if (!result)
  {
    printf("encode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT_FATAL(result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);

  reset_exception(&exception);

  /* Decrypt the datareader submessage */
  result = crypto->crypto_transform->decode_datareader_submessage(
      crypto->crypto_transform,
      &decoded_buffer,
      &encoded_buffer,
      local_writer_crypto,
      remote_reader_crypto,
      &exception);

  if (!result)
  {
    printf("decode_datareader_submessage: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT_FATAL(result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);
  CU_ASSERT_FATAL(decoded_buffer._length == plain_buffer._length);

  reset_exception(&exception);

  if (memcmp(decoded_buffer._buffer, plain_buffer._buffer, plain_buffer._length) != 0)
  {
    CU_FAIL("decode submessage is not equal to original");
  }

  unregister_datawriter(writer_list._buffer[0]);
  writer_list._buffer[0] = 0;
  unregister_datareader(remote_reader_crypto);
  unregister_datareader(local_reader_crypto);
  unregister_datawriter(local_writer_crypto);

  reset_exception(&exception);

  DDS_Security_DatareaderCryptoHandleSeq_deinit(&writer_list);

  DDS_Security_OctetSeq_deinit(&plain_buffer);
  DDS_Security_OctetSeq_deinit(&encoded_buffer);
  DDS_Security_OctetSeq_deinit(&decoded_buffer);

  DDS_Security_PropertySeq_deinit(&datawriter_properties);
  DDS_Security_PropertySeq_deinit(&datareader_properties);
}

