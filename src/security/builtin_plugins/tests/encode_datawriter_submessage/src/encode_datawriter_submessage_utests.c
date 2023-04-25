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
#include "common/src/crypto_helper.h"
#include "crypto_objects.h"
#include "crypto_utils.h"

#define TEST_SHARED_SECRET_SIZE 32

static struct plugins_hdl *plugins = NULL;
static dds_security_cryptography *crypto = NULL;

static DDS_Security_IdentityHandle local_participant_identity = 1;
static DDS_Security_IdentityHandle remote_participant_identity = 2;

static DDS_Security_ParticipantCryptoHandle local_particpant_crypto = 0;
static DDS_Security_ParticipantCryptoHandle remote_particpant_crypto = 0;

static DDS_Security_SharedSecretHandleImpl *shared_secret_handle_impl = NULL;
static DDS_Security_SharedSecretHandle shared_secret_handle;

static const char *sample_test_data =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxy";

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
  uint32_t length;
};

#if 0
struct receiver_specific_mac
{
  DDS_Security_CryptoTransformKeyId receiver_mac_key_id;
  unsigned char receiver_mac[CRYPTO_HMAC_SIZE];
};
#endif

struct encrypted_data
{
  uint32_t length;
  unsigned char data[];
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

static int register_local_participant(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PermissionsHandle participant_permissions = 3; //valid dummy value
  DDS_Security_PropertySeq participant_properties;
  DDS_Security_ParticipantSecurityAttributes participant_security_attributes;

  memset(&participant_properties, 0, sizeof(participant_properties));
  memset(&participant_security_attributes, 0, sizeof(participant_security_attributes));

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

static void unregister_local_participant(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  if (local_particpant_crypto)
  {
    crypto->crypto_key_factory->unregister_participant(crypto->crypto_key_factory, local_particpant_crypto, &exception);
    reset_exception(&exception);
  }
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

static void prepare_endpoint_security_attributes_and_properties(DDS_Security_EndpointSecurityAttributes *attributes,
                                                                DDS_Security_PropertySeq *properties,
                                                                DDS_Security_CryptoTransformKind_Enum transformation_kind,
                                                                bool is_origin_authenticated)
{
  memset(attributes, 0, sizeof(DDS_Security_EndpointSecurityAttributes));
  memset(properties, 0, sizeof(DDS_Security_PropertySeq));

  attributes->is_discovery_protected = true;

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

static void unregister_remote_participant(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  if (remote_particpant_crypto)
  {
    crypto->crypto_key_factory->unregister_participant(crypto->crypto_key_factory, remote_particpant_crypto, &exception);
    reset_exception(&exception);
  }
}

static DDS_Security_DatawriterCryptoHandle register_local_datawriter(DDS_Security_EndpointSecurityAttributes *datawriter_security_attributes, DDS_Security_PropertySeq *datawriter_properties)
{
  DDS_Security_DatawriterCryptoHandle writer_crypto;
  DDS_Security_SecurityException exception = {NULL, 0, 0};

  writer_crypto =
      crypto->crypto_key_factory->register_local_datawriter(
          crypto->crypto_key_factory,
          local_particpant_crypto,
          datawriter_properties,
          datawriter_security_attributes,
          &exception);

  if (writer_crypto == 0)
  {
    printf("register_local_datawriter: %s\n", exception.message ? exception.message : "Error message missing");
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
          remote_particpant_crypto,
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

static bool read_prefix(unsigned char **ptr, uint32_t *remain)
{
  struct submsg_header *prefix;
  uint32_t hlen;
  int swap;

  if (*remain < sizeof(struct submsg_header))
  {
    printf("check_encoded_data: prefix missing\n");
    return false;
  }

  prefix = (struct submsg_header *)(*ptr);

  if (prefix->id != DDSI_RTPS_SMID_SEC_PREFIX)
  {
    printf("check_encoded_data: prefix incorrect smid 0x%x02\n", prefix->id);
    return false;
  }

  DDSRT_WARNING_MSVC_OFF(6326)
  if (prefix->flags & 0x01)
    swap = (DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN);
  else
    swap = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN);
  DDSRT_WARNING_MSVC_ON(6326)

  hlen = swap ? ddsrt_bswap2u(prefix->length) : prefix->length;

  if (hlen != sizeof(struct crypto_header))
  {
    printf("check_encoded_data: crypto_header missing\n");
    return false;
  }

  *ptr += sizeof(struct submsg_header);
  *remain -= (uint32_t)sizeof(struct submsg_header);

  return true;
}

static bool read_header(struct crypto_header **header, unsigned char **ptr, uint32_t *remain)
{
  if (*remain < sizeof(struct crypto_header))
  {
    printf("check_encoded_data: crypto_header too short\n");
    return false;
  }

  *header = ddsrt_malloc(sizeof(struct crypto_header));
  memcpy(*header, *ptr, sizeof(struct crypto_header));

  *ptr += sizeof(struct crypto_header);
  *remain -= (uint32_t)sizeof(struct crypto_header);

  return true;
}

static bool read_body(DDS_Security_OctetSeq *contents, bool encrypted, unsigned char **ptr, uint32_t *remain)
{
  struct submsg_header *body;
  uint32_t hlen, clen;
  int swap;

  if (*remain < sizeof(struct submsg_header))
  {
    return false;
  }

  body = (struct submsg_header *)(*ptr);

  DDSRT_WARNING_MSVC_OFF(6326)
  if (body->flags & 0x01)
    swap = (DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN);
  else
    swap = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN);
  DDSRT_WARNING_MSVC_ON(6326)

  hlen = swap ? ddsrt_bswap2u(body->length) : body->length;

  if (encrypted)
  {
    struct encrypted_data *enc;

    if (body->id != DDSI_RTPS_SMID_SEC_BODY)
    {
      printf("check_encoded_data: submessage SEC_BODY missing\n");
      return false;
    }
    enc = (struct encrypted_data *)(body + 1);
    clen = ddsrt_fromBE4u(enc->length);

    contents->_length = contents->_maximum = clen;
    contents->_buffer = &enc->data[0];
  }
  else
  {
    if (body->id == DDSI_RTPS_SMID_SEC_BODY)
    {
      printf("check_encoded_data: submessage SEC_BODY not expected\n");
      return false;
    }
    clen = swap ? ddsrt_bswap2u(body->length) : body->length;
    clen += (uint32_t)sizeof(struct submsg_header);

    contents->_length = contents->_maximum = clen;
    contents->_buffer = *ptr;
  }
  *ptr += sizeof(struct submsg_header) + hlen;
  *remain -= (uint32_t)sizeof(struct submsg_header) + hlen;

  return true;
}

static bool read_postfix(unsigned char **ptr,uint32_t *remain)
{
  struct submsg_header *postfix;

  if (*remain < sizeof(struct submsg_header))
  {
    printf("check_encoded_data: postfix missing\n");
    return false;
  }

  postfix = (struct submsg_header *)(*ptr);

  if (postfix->id != DDSI_RTPS_SMID_SEC_POSTFIX)
  {
    printf("check_encoded_data: postfix invalid smid\n");
    return false;
  }

  *ptr += sizeof(struct submsg_header);
  *remain -= (uint32_t)sizeof(struct submsg_header);

  return true;
}

static bool read_footer(struct crypto_footer **footer, unsigned char **ptr, uint32_t *remain)
{
  if (*remain < CRYPTO_HMAC_SIZE + sizeof(uint32_t))
  {
    printf("check_encoded_data: crypto_footer incorrect size\n");
    return false;
  }

  *footer = ddsrt_malloc(*remain);
  memcpy(*footer, *ptr, *remain);

  /* length of reader specific macs is in BIG-ENDIAN format */
  (*footer)->length = ddsrt_fromBE4u((*footer)->length);

  return true;
}

static bool check_encoded_data(DDS_Security_OctetSeq *data, bool encrypted, struct crypto_header **header, struct crypto_footer **footer, DDS_Security_OctetSeq *contents)
{
  bool result = true;
  unsigned char *ptr = data->_buffer;
  uint32_t remain = data->_length;

  result = read_prefix(&ptr, &remain);
  if (result)
    result = read_header(header, &ptr, &remain);
  if (result)
    result = read_body(contents, encrypted, &ptr, &remain);
  if (result)
    result = read_postfix(&ptr, &remain);
  if (result)
    result = read_footer(footer, &ptr, &remain);

  return result;
}

static bool cipher_sign_data(const unsigned char *session_key, uint32_t key_size, const unsigned char *iv, const unsigned char *data, uint32_t data_len, unsigned char *tag)
{
  EVP_CIPHER_CTX *ctx;
  unsigned char temp[32];
  int len;

  /* create the cipher context */
  ctx = EVP_CIPHER_CTX_new();
  if (!ctx)
  {
    ERR_print_errors_fp(stderr);
    goto fail_ctx_new;
  }

  /* initialize the cipher and set to AES GCM */
  if (key_size == 128)
  {
    if (!EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL))
    {
      ERR_print_errors_fp(stderr);
      goto fail_encrypt;
    }
  }
  else if (key_size == 256)
  {
    if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
    {
      ERR_print_errors_fp(stderr);
      goto fail_encrypt;
    }
  }

  /* Initialise key and IV */
  if (!EVP_EncryptInit_ex(ctx, NULL, NULL, session_key, iv))
  {
    ERR_print_errors_fp(stderr);
    goto fail_encrypt;
  }

  if (!EVP_EncryptUpdate(ctx, NULL, &len, data, (int)data_len))
  {
    ERR_print_errors_fp(stderr);
    goto fail_encrypt;
  }

  if (!EVP_EncryptFinal_ex(ctx, temp, &len))
  {
    ERR_print_errors_fp(stderr);
    goto fail_encrypt;
  }

  /* get the tag */
  if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, CRYPTO_HMAC_SIZE, tag))
  {
    ERR_print_errors_fp(stderr);
    goto fail_encrypt;
  }

  /* clean up */
  EVP_CIPHER_CTX_free(ctx);

  return true;

fail_encrypt:
  EVP_CIPHER_CTX_free(ctx);
fail_ctx_new:
  return false;
}

static bool crypto_decrypt_data(uint32_t session_id, unsigned char *iv, DDS_Security_CryptoTransformKind transformation_kind, master_key_material *key_material, DDS_Security_OctetSeq *encrypted, DDS_Security_OctetSeq *decoded, unsigned char *tag)
{
  EVP_CIPHER_CTX *ctx;
  crypto_session_key_t session_key;
  uint32_t key_size = crypto_get_key_size(CRYPTO_TRANSFORM_KIND(transformation_kind));
  assert (key_size);
  int len = 0;

  if (!key_size)
    return false;

  if (!crypto_calculate_session_key_test(&session_key, session_id, key_material->master_salt, key_material->master_sender_key, key_material->transformation_kind))
    return false;

  /* create the cipher context */
  ctx = EVP_CIPHER_CTX_new();
  if (!ctx)
  {
    ERR_print_errors_fp(stderr);
    return false;
  }

  if (key_size == 128 && !EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL))
  {
    ERR_print_errors_fp(stderr);
    goto err;
  }
  if (key_size == 256 && !EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
  {
    ERR_print_errors_fp(stderr);
    goto err;
  }
  if (!EVP_DecryptInit_ex(ctx, NULL, NULL, session_key.data, iv))
  {
    ERR_print_errors_fp(stderr);
    goto err;
  }

  if (decoded)
  {
    if (!EVP_DecryptUpdate(ctx, decoded->_buffer, &len, encrypted->_buffer, (int)encrypted->_length))
    {
      ERR_print_errors_fp(stderr);
      goto err;
    }
    decoded->_length = (uint32_t) len;
  }
  else if (!EVP_DecryptUpdate(ctx, NULL, &len, encrypted->_buffer, (int) encrypted->_length))
  {
    ERR_print_errors_fp(stderr);
    goto err;
  }

  if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, CRYPTO_HMAC_SIZE, tag))
  {
    ERR_print_errors_fp(stderr);
    goto err;
  }

  if (decoded)
  {
    if (!EVP_DecryptFinal_ex(ctx, decoded->_buffer + len, &len))
    {
      ERR_print_errors_fp(stderr);
      goto err;
    }
    decoded->_length += (uint32_t) len;
  }
  else
  {
    unsigned char temp[32];
    if (!EVP_DecryptFinal_ex(ctx, temp, &len))
    {
      ERR_print_errors_fp(stderr);
      goto err;
    }
  }
  EVP_CIPHER_CTX_free(ctx);
  return true;

err:
  EVP_CIPHER_CTX_free(ctx);
  return false;
}

static session_key_material * get_datawriter_session(DDS_Security_DatawriterCryptoHandle writer_crypto)
{
  local_datawriter_crypto *writer_crypto_impl = (local_datawriter_crypto *)writer_crypto;
  return writer_crypto_impl->writer_session_message;
}

static master_key_material * get_datareader_key_material(DDS_Security_DatareaderCryptoHandle reader_crypto)
{
  remote_datareader_crypto *reader_crypto_impl = (remote_datareader_crypto *)reader_crypto;
  return reader_crypto_impl->writer2reader_key_material_message;
}

static unsigned char submsg_header_endianness_flag (enum ddsrt_byte_order_selector bo)
{
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
  return (unsigned char) ((bo == DDSRT_BOSEL_BE) ? 0 : DDSI_RTPS_SUBMESSAGE_FLAG_ENDIANNESS);
#else
  return (unsigned char) ((bo == DDSRT_BO_LE) ? DDSI_RTPS_SUBMESSAGE_FLAG_ENDIANNESS : 0);
#endif
}

static void initialize_data_submessage(DDS_Security_OctetSeq *submsg, enum ddsrt_byte_order_selector bo)
{
  size_t length = strlen(sample_test_data) + 1;
  struct submsg_header *header;
  unsigned char *buffer, *ptr;

  buffer = ddsrt_malloc(length + sizeof(struct submsg_header));
  header = (struct submsg_header *)buffer;
  header->id = 0x15;
  header->flags = submsg_header_endianness_flag(bo);
  header->length = ddsrt_toBO2u(bo, (uint16_t)length);
  ptr = (unsigned char *)(header + 1);

  memcpy((char *)ptr, sample_test_data, length);

  submsg->_length = submsg->_maximum = (uint32_t)(length + sizeof(struct submsg_header));
  submsg->_buffer = buffer;
}

static bool check_reader_sign(
    DDS_Security_DatareaderCryptoHandle reader_crypto,
    uint32_t session_id,
    uint32_t key_id,
    uint32_t key_size,
    unsigned char *init_vector,
    unsigned char *common_mac,
    unsigned char *hmac)
{
  master_key_material *keymat;
  crypto_session_key_t key;
  unsigned char md[CRYPTO_HMAC_SIZE];

  keymat = get_datareader_key_material(reader_crypto);

  if (key_id != keymat->receiver_specific_key_id)
  {
    printf("check_reader_sign: key_id does not match\n");
    return false;
  }
  else if (!calculate_receiver_specific_key_test(&key, session_id, keymat->master_salt, keymat->master_receiver_specific_key, keymat->transformation_kind))
  {
    printf("check_reader_sign: calculate key failed\n");
    return false;
  }
  else if (!cipher_sign_data(key.data, key_size, init_vector, common_mac, CRYPTO_HMAC_SIZE, md))
  {
    return false;
  }
  else if (memcmp(hmac, md, CRYPTO_HMAC_SIZE) != 0)
  {
    printf("check_reader_sign: hmac incorrect\n");
    return false;
  }

  return true;
}

static bool check_reader_signing(DDS_Security_DatareaderCryptoHandleSeq *list, struct crypto_footer *footer, uint32_t session_id, unsigned char *init_vector, uint32_t key_size)
{
  struct receiver_specific_mac *rmac;
  uint32_t key_id;
  uint32_t i;

  if (footer->length != list->_length)
  {
    return false;
  }

  rmac = (struct receiver_specific_mac *)(footer + 1);

  for (i = 0; i < list->_length; i++)
  {
    key_id = ddsrt_bswap4u(*(uint32_t *)rmac[i].receiver_mac_key_id);
    if (!check_reader_sign(list->_buffer[i], session_id, key_id, key_size, init_vector, footer->common_mac, rmac[i].receiver_mac.data))
    {
      return false;
    }
  }

  return true;
}

static void suite_encode_datawriter_submessage_init(void)
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

static void suite_encode_datawriter_submessage_fini(void)
{
  unregister_remote_participant();
  unregister_local_participant();
  deallocate_shared_secret();
  unload_plugins(plugins);
}

static void encode_datawriter_submessage_not_signed(DDS_Security_CryptoTransformKind_Enum transformation_kind)
{
  DDS_Security_boolean result;
  DDS_Security_DatawriterCryptoHandle writer_crypto;
  DDS_Security_DatareaderCryptoHandle reader_crypto;
  DDS_Security_DatareaderCryptoHandleSeq reader_list;
  int32_t index;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_OctetSeq plain_buffer;
  DDS_Security_OctetSeq encoded_buffer;
  DDS_Security_OctetSeq decoded_buffer;
  DDS_Security_OctetSeq data;

  session_key_material *session_keys;
  struct crypto_header *header = NULL;
  struct crypto_footer *footer = NULL;
  uint32_t session_id;
  DDS_Security_PropertySeq datawriter_properties;
  DDS_Security_EndpointSecurityAttributes datawriter_security_attributes;
  bool is_encrypted;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform != NULL);
  assert(crypto->crypto_transform != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->encode_datawriter_submessage != NULL);
  assert(crypto->crypto_transform->encode_datawriter_submessage != 0);

  if (transformation_kind == CRYPTO_TRANSFORMATION_KIND_AES128_GCM || transformation_kind == CRYPTO_TRANSFORMATION_KIND_AES256_GCM)
  {
    is_encrypted = true;
  }
  else
  {
    is_encrypted = false;
  }

  prepare_endpoint_security_attributes_and_properties(&datawriter_security_attributes, &datawriter_properties, transformation_kind, false);

  initialize_data_submessage(&plain_buffer, DDSRT_BOSEL_NATIVE);

  writer_crypto = register_local_datawriter(&datawriter_security_attributes, &datawriter_properties);
  CU_ASSERT_FATAL(writer_crypto != 0);
  assert(writer_crypto != 0); // for Clang's static analyzer

  session_keys = get_datawriter_session(writer_crypto);

  reader_crypto = register_remote_datareader(writer_crypto);
  CU_ASSERT_FATAL(reader_crypto != 0);
  assert(reader_crypto != 0); // for Clang's static analyzer

  reader_list._length = reader_list._maximum = 1;
  reader_list._buffer = DDS_Security_DatareaderCryptoHandleSeq_allocbuf(1);
  reader_list._buffer[0] = reader_crypto;
  index = 0;

  /* Now call the function. */
  result = crypto->crypto_transform->encode_datawriter_submessage(
      crypto->crypto_transform,
      &encoded_buffer,
      &plain_buffer,
      writer_crypto,
      &reader_list,
      &index,
      &exception);

  if (!result)
  {
    printf("encode_datawriter_submessage: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT_FATAL(result);
  assert(result != 0); // for Clang's static analyzer

  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);

  reset_exception(&exception);

  result = check_encoded_data(&encoded_buffer, is_encrypted, &header, &footer, &data);
  CU_ASSERT_FATAL(result);
  assert(result); // for Clang's static analyzer

  CU_ASSERT(header->transform_identifier.transformation_kind[3] == transformation_kind);

  session_id = ddsrt_bswap4u(*(uint32_t *)header->session_id);

  if (is_encrypted)
  {
    decoded_buffer._buffer = ddsrt_malloc(plain_buffer._length);
    decoded_buffer._length = 0;
    decoded_buffer._maximum = plain_buffer._length;

    result = crypto_decrypt_data(session_id, &header->session_id[0], header->transform_identifier.transformation_kind,
                                 session_keys->master_key_material, &data, &decoded_buffer, footer->common_mac);

    if (!result)
    {
      printf("Decode failed\n");
    }

    CU_ASSERT_FATAL(result);

    CU_ASSERT(memcmp(plain_buffer._buffer, decoded_buffer._buffer, plain_buffer._length) == 0);

    DDS_Security_OctetSeq_deinit((&decoded_buffer));
  }
  else
  {
    result = crypto_decrypt_data(session_id, &header->session_id[0], header->transform_identifier.transformation_kind,
                                 session_keys->master_key_material, &data, NULL, footer->common_mac);
    if (!result)
    {
      printf("Decode failed\n");
    }

    CU_ASSERT_FATAL(result);

    CU_ASSERT(memcmp(plain_buffer._buffer, data._buffer, plain_buffer._length) == 0);
  }

  unregister_datareader(reader_list._buffer[0]);
  reader_list._buffer[0] = 0;

  unregister_datawriter(writer_crypto);

  DDS_Security_OctetSeq_deinit((&plain_buffer));
  DDS_Security_OctetSeq_deinit((&encoded_buffer));
  DDS_Security_DatareaderCryptoHandleSeq_deinit(&reader_list);

  ddsrt_free(datawriter_properties._buffer[0].name);
  ddsrt_free(datawriter_properties._buffer[0].value);
  ddsrt_free(datawriter_properties._buffer);
  ddsrt_free(footer);
  ddsrt_free(header);
}

CU_Test(ddssec_builtin_encode_datawriter_submessage, encode_256, .init = suite_encode_datawriter_submessage_init, .fini = suite_encode_datawriter_submessage_fini)
{
  encode_datawriter_submessage_not_signed(CRYPTO_TRANSFORMATION_KIND_AES256_GCM);
}

CU_Test(ddssec_builtin_encode_datawriter_submessage, encode_128, .init = suite_encode_datawriter_submessage_init, .fini = suite_encode_datawriter_submessage_fini)
{
  encode_datawriter_submessage_not_signed(CRYPTO_TRANSFORMATION_KIND_AES128_GCM);
}

CU_Test(ddssec_builtin_encode_datawriter_submessage, no_encode_256, .init = suite_encode_datawriter_submessage_init, .fini = suite_encode_datawriter_submessage_fini)
{
  encode_datawriter_submessage_not_signed(CRYPTO_TRANSFORMATION_KIND_AES256_GMAC);
}

CU_Test(ddssec_builtin_encode_datawriter_submessage, no_encode_128, .init = suite_encode_datawriter_submessage_init, .fini = suite_encode_datawriter_submessage_fini)
{
  encode_datawriter_submessage_not_signed(CRYPTO_TRANSFORMATION_KIND_AES128_GMAC);
}

static void encode_datawriter_submessage_sign(DDS_Security_CryptoTransformKind_Enum transformation_kind)
{
  const uint32_t READERS_CNT = 4u;
  DDS_Security_boolean result;
  DDS_Security_DatawriterCryptoHandle writer_crypto;
  DDS_Security_DatareaderCryptoHandle reader_crypto;
  DDS_Security_DatareaderCryptoHandleSeq reader_list;
  int32_t index;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_OctetSeq plain_buffer;
  DDS_Security_OctetSeq encoded_buffer;
  DDS_Security_OctetSeq decoded_buffer;
  DDS_Security_OctetSeq data;
  DDS_Security_OctetSeq *buffer;
  session_key_material *session_keys;
  struct crypto_header *header = NULL;
  struct crypto_footer *footer = NULL;
  uint32_t session_id;
  uint32_t i;
  DDS_Security_PropertySeq datawriter_properties;
  DDS_Security_EndpointSecurityAttributes datawriter_security_attributes;
  bool is_encrypted;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform != NULL);
  assert(crypto->crypto_transform != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->encode_datawriter_submessage != NULL);
  assert(crypto->crypto_transform->encode_datawriter_submessage != 0);

  if (transformation_kind == CRYPTO_TRANSFORMATION_KIND_AES128_GCM || transformation_kind == CRYPTO_TRANSFORMATION_KIND_AES256_GCM)
  {
    is_encrypted = true;
  }
  else
  {
    is_encrypted = false;
  }

  prepare_endpoint_security_attributes_and_properties(&datawriter_security_attributes, &datawriter_properties, transformation_kind, true);

  initialize_data_submessage(&plain_buffer, DDSRT_BOSEL_NATIVE);

  writer_crypto = register_local_datawriter(&datawriter_security_attributes, &datawriter_properties);
  CU_ASSERT_FATAL(writer_crypto != 0);
  assert(writer_crypto != 0); // for Clang's static analyzer

  session_keys = get_datawriter_session(writer_crypto);

  reader_list._length = reader_list._maximum = READERS_CNT;
  reader_list._buffer = DDS_Security_DatareaderCryptoHandleSeq_allocbuf(4);
  for (i = 0; i < READERS_CNT; i++)
  {
    reader_crypto = register_remote_datareader(writer_crypto);
    CU_ASSERT_FATAL(reader_crypto != 0);
    assert(reader_crypto != 0); // for Clang's static analyzer
    reader_list._buffer[i] = reader_crypto;
  }
  index = 0;

  /* Now call the function. */

  buffer = &plain_buffer;
  while (index != (int32_t)READERS_CNT)
  {
    result = crypto->crypto_transform->encode_datawriter_submessage(
        crypto->crypto_transform,
        &encoded_buffer,
        buffer,
        writer_crypto,
        &reader_list,
        &index,
        &exception);

    if (!result)
    {
      printf("encode_datawriter_submessage: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL(result);
    assert(result); // for Clang's static analyzer
    CU_ASSERT(exception.code == 0);
    CU_ASSERT(exception.message == NULL);

    reset_exception(&exception);
    buffer = NULL;
  }

  result = check_encoded_data(&encoded_buffer, is_encrypted, &header, &footer, &data);
  CU_ASSERT_FATAL(result);
  assert(result); // for Clang's static analyzer

  CU_ASSERT(header->transform_identifier.transformation_kind[3] == transformation_kind);

  session_id = ddsrt_bswap4u(*(uint32_t *)header->session_id);

  if (is_encrypted)
  {
    decoded_buffer._buffer = ddsrt_malloc(plain_buffer._length);
    decoded_buffer._length = 0;
    decoded_buffer._maximum = plain_buffer._length;

    result = crypto_decrypt_data(session_id, &header->session_id[0], header->transform_identifier.transformation_kind,
                                 session_keys->master_key_material, &data, &decoded_buffer, footer->common_mac);
    if (!result)
    {
      printf("Decode failed\n");
    }

    CU_ASSERT_FATAL(result);

    CU_ASSERT(memcmp(plain_buffer._buffer, decoded_buffer._buffer, plain_buffer._length) == 0);

    DDS_Security_OctetSeq_deinit((&decoded_buffer));
  }
  else
  {
    result = crypto_decrypt_data(session_id, header->session_id, header->transform_identifier.transformation_kind,
                                 session_keys->master_key_material, &data, NULL, footer->common_mac);

    if (!result)
    {
      printf("Decode failed\n");
    }

    CU_ASSERT_FATAL(result);
    CU_ASSERT(memcmp(plain_buffer._buffer, data._buffer, plain_buffer._length) == 0);
  }

  printf("num hmacs = %u\n", footer->length);

  CU_ASSERT(check_reader_signing(&reader_list, footer, session_id, header->session_id, session_keys->key_size));

  for (i = 0; i < READERS_CNT; i++)
  {
    unregister_datareader(reader_list._buffer[i]);
    reader_list._buffer[i] = 0;
  }

  unregister_datawriter(writer_crypto);

  DDS_Security_OctetSeq_deinit((&plain_buffer));
  DDS_Security_OctetSeq_deinit((&encoded_buffer));
  DDS_Security_DatareaderCryptoHandleSeq_deinit(&reader_list);

  ddsrt_free(datawriter_properties._buffer[0].name);
  ddsrt_free(datawriter_properties._buffer[0].value);
  ddsrt_free(datawriter_properties._buffer);
  ddsrt_free(footer);
  ddsrt_free(header);
}

CU_Test(ddssec_builtin_encode_datawriter_submessage, encode_sign_256, .init = suite_encode_datawriter_submessage_init, .fini = suite_encode_datawriter_submessage_fini)
{
  encode_datawriter_submessage_sign(CRYPTO_TRANSFORMATION_KIND_AES256_GCM);
}

CU_Test(ddssec_builtin_encode_datawriter_submessage, encode_sign_128, .init = suite_encode_datawriter_submessage_init, .fini = suite_encode_datawriter_submessage_fini)
{
  encode_datawriter_submessage_sign(CRYPTO_TRANSFORMATION_KIND_AES128_GCM);
}

CU_Test(ddssec_builtin_encode_datawriter_submessage, no_encode_sign_256, .init = suite_encode_datawriter_submessage_init, .fini = suite_encode_datawriter_submessage_fini)
{
  encode_datawriter_submessage_sign(CRYPTO_TRANSFORMATION_KIND_AES256_GMAC);
}

CU_Test(ddssec_builtin_encode_datawriter_submessage, no_encode_sign_128, .init = suite_encode_datawriter_submessage_init, .fini = suite_encode_datawriter_submessage_fini)
{
  encode_datawriter_submessage_sign(CRYPTO_TRANSFORMATION_KIND_AES128_GMAC);
}

CU_Test(ddssec_builtin_encode_datawriter_submessage, invalid_args, .init = suite_encode_datawriter_submessage_init, .fini = suite_encode_datawriter_submessage_fini)
{
  DDS_Security_boolean result;
  DDS_Security_DatawriterCryptoHandle writer_crypto;
  DDS_Security_DatareaderCryptoHandle reader_crypto;
  DDS_Security_DatareaderCryptoHandleSeq reader_list;
  DDS_Security_DatareaderCryptoHandleSeq empty_reader_list;
  int32_t index;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_OctetSeq plain_buffer;
  DDS_Security_OctetSeq encoded_buffer;
  DDS_Security_PropertySeq datawriter_properties;
  DDS_Security_EndpointSecurityAttributes datawriter_security_attributes;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform != NULL);
  assert(crypto->crypto_transform != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->encode_datawriter_submessage != NULL);
  assert(crypto->crypto_transform->encode_datawriter_submessage != 0);

  prepare_endpoint_security_attributes_and_properties(&datawriter_security_attributes, &datawriter_properties, CRYPTO_TRANSFORMATION_KIND_AES256_GCM, true);

  initialize_data_submessage(&plain_buffer, DDSRT_BOSEL_NATIVE);
  memset(&empty_reader_list, 0, sizeof(empty_reader_list));

  writer_crypto = register_local_datawriter(&datawriter_security_attributes, &datawriter_properties);
  CU_ASSERT_FATAL(writer_crypto != 0);

  //set_protection_kind(writer_crypto, DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION);

  reader_crypto = register_remote_datareader(writer_crypto);
  CU_ASSERT_FATAL(reader_crypto != 0);

  reader_list._length = reader_list._maximum = 1;
  reader_list._buffer = DDS_Security_DatareaderCryptoHandleSeq_allocbuf(1);
  reader_list._buffer[0] = reader_crypto;
  index = 0;

  /* writer crypto unknown */
  result = crypto->crypto_transform->encode_datawriter_submessage(
      crypto->crypto_transform,
      &encoded_buffer,
      &plain_buffer,
      1,
      &reader_list,
      &index,
      &exception);

  if (!result)
  {
    printf("encode_datawriter_submessage: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* reader crypto list with invalid reader crypto */
  reader_list._buffer[0] = 0;
  result = crypto->crypto_transform->encode_datawriter_submessage(
      crypto->crypto_transform,
      &encoded_buffer,
      &plain_buffer,
      writer_crypto,
      &reader_list,
      &index,
      &exception);

  if (!result)
  {
    printf("encode_datawriter_submessage: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* reader crypto list with unknown reader crypto*/
  reader_list._buffer[0] = 1;
  result = crypto->crypto_transform->encode_datawriter_submessage(
      crypto->crypto_transform,
      &encoded_buffer,
      &plain_buffer,
      writer_crypto,
      &reader_list,
      &index,
      &exception);

  if (!result)
  {
    printf("encode_datawriter_submessage: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);
  reader_list._buffer[0] = reader_crypto;

  ddsrt_free(datawriter_properties._buffer[0].name);
  ddsrt_free(datawriter_properties._buffer[0].value);
  ddsrt_free(datawriter_properties._buffer);
  DDS_Security_OctetSeq_deinit((&plain_buffer));
  DDS_Security_DatareaderCryptoHandleSeq_deinit(&reader_list);
}

