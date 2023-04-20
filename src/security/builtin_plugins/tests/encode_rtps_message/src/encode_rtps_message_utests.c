// Copyright(c) 2006 to 2022 ZettaScale Technology and others
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
#include "crypto_key_factory.h"
#include "crypto_objects.h"
#include "crypto_utils.h"

#define TEST_SHARED_SECRET_SIZE 32

static struct plugins_hdl *plugins = NULL;
static dds_security_cryptography *crypto = NULL;

static DDS_Security_IdentityHandle local_participant_identity = 1;
static DDS_Security_IdentityHandle remote_participant_identities[] = {2, 3, 4, 5};

static DDS_Security_ParticipantCryptoHandle local_particpant_crypto = 0;
static DDS_Security_ParticipantCryptoHandle remote_particpant_cryptos[sizeof(remote_participant_identities) / sizeof(remote_participant_identities[0])];

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
  unsigned char session_id[sizeof(remote_participant_identities) / sizeof(remote_participant_identities[0])];
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

static void print_octets(const char *msg, const unsigned char *data, uint32_t sz)
{
  uint32_t i;
  printf("%s: ", msg);
  for (i = 0; i < sz; i++)
  {
    printf("%02x", data[i]);
  }
  printf("\n");
}

static int register_local_participant(DDS_Security_ParticipantSecurityAttributes *participant_security_attributes, DDS_Security_PropertySeq *participant_properties)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PermissionsHandle participant_permissions = 2; /* dummy but valid */

  local_particpant_crypto =
      crypto->crypto_key_factory->register_local_participant(
          crypto->crypto_key_factory,
          local_participant_identity,
          participant_permissions,
          participant_properties,
          participant_security_attributes,
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

static int register_remote_participants(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PermissionsHandle remote_participant_permissions = 5;

  unsigned i;
  int result = 0;

  for (i = 0; i < sizeof(remote_particpant_cryptos) / sizeof(remote_particpant_cryptos[0]); ++i)
  {
    remote_particpant_cryptos[i] =
        crypto->crypto_key_factory->register_matched_remote_participant(
            crypto->crypto_key_factory,
            local_particpant_crypto,
            remote_participant_identities[i],
            remote_participant_permissions,
            shared_secret_handle,
            &exception);

    if (remote_particpant_cryptos[i] == 0)
    {
      printf("register_matched_remote_participant: %s\n", exception.message ? exception.message : "Error message missing");
      result = 1;
      break;
    }
  }

  return result;
}

static void
unregister_remote_participants(void)
{
  unsigned i;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  for (i = 0; i < sizeof(remote_particpant_cryptos) / sizeof(remote_particpant_cryptos[0]); ++i)
  {
    if (remote_particpant_cryptos[i])
    {
      crypto->crypto_key_factory->unregister_participant(crypto->crypto_key_factory, remote_particpant_cryptos[i], &exception);
      reset_exception(&exception);
    }
  }
}

static bool check_encoded_data(DDS_Security_OctetSeq *data, bool encrypted, struct crypto_header **header, struct crypto_footer **footer, DDS_Security_OctetSeq *contents)
{
  struct submsg_header *prefix;
  struct submsg_header *postfix;
  struct submsg_header *body;
  unsigned char *ptr = data->_buffer;
  uint32_t remain = data->_length;
  uint32_t hlen, clen, dlen;
  int swap;

  if (remain < 20)
  {
    printf("check_encoded_data: RTPS header missing\n");
    goto fail_prefix;
  }

  /* rtps header first */
  if (memcmp(ptr, RTPS_HEADER, strlen(RTPS_HEADER)) != 0)
  {
    printf("check_encoded_data: RTPS header invalid\n");
    goto fail_prefix;
  }

  if (remain < sizeof(struct submsg_header))
  {
    printf("check_encoded_data: prefix missing\n");
    goto fail_prefix;
  }

  remain -= 20; /* rtps header */
  ptr += 20;

  prefix = (struct submsg_header *)ptr;

  if (prefix->id != DDSI_RTPS_SMID_SRTPS_PREFIX)
  {
    printf("check_encoded_data: prefix incorrect smid 0x%x02\n", prefix->id);
    goto fail_prefix;
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
    goto fail_prefix;
  }

  ptr += sizeof(struct submsg_header);
  remain -= (uint32_t)sizeof(struct submsg_header);

  if (remain < sizeof(struct crypto_header))
  {
    printf("check_encoded_data: crypto_header too short\n");
    goto fail_prefix;
  }

  *header = ddsrt_malloc(sizeof(struct crypto_header));
  memcpy(*header, ptr, sizeof(struct crypto_header));

  ptr += sizeof(struct crypto_header);
  remain -= (uint32_t)sizeof(struct crypto_header);

  if (remain < sizeof(struct submsg_header))
  {
    goto fail_body;
  }

  if (encrypted)
  {
    body = (struct submsg_header *)ptr;
    if (body->id != DDSI_RTPS_SMID_SEC_BODY)
    {
      printf("check_encoded_data: submessage SEC_BODY missing\n");
      goto fail_body;
    }
    ptr += sizeof(struct submsg_header);
    remain -= (uint32_t)sizeof(struct submsg_header);

    dlen = ddsrt_fromBE4u(*(uint32_t *)ptr);

    clen = swap ? ddsrt_bswap2u(body->length) : body->length;
    if (dlen > clen)
    {
      printf("check_encoded_data: encrypted body length incorrect\n");
      goto fail_body;
    }

    ptr += sizeof(uint32_t);
    remain -= (uint32_t)sizeof(uint32_t);

    contents->_length = contents->_maximum = dlen;
    contents->_buffer = ptr;

    ptr += clen - sizeof(uint32_t);
  }
  else
  {
    body = (struct submsg_header *)(ptr + 24); /* header after info_src */
    if (body->id == DDSI_RTPS_SMID_SEC_BODY)
    {
      printf("check_encoded_data: submessage SEC_BODY not expected\n");
      goto fail_body;
    }
    clen = swap ? ddsrt_bswap2u(body->length) : body->length;
    clen += (uint32_t)sizeof(struct submsg_header) + 24;

    contents->_length = contents->_maximum = clen;
    contents->_buffer = ptr;

    ptr += clen;
  }

  if (clen > remain)
  {
    printf("check_encoded_data: payload invalid size\n");
    goto fail_body;
  }

  remain -= contents->_length;

  if (remain < sizeof(struct submsg_header))
  {
    printf("check_encoded_data: postfix missing\n");
    goto fail_postfix;
  }

  postfix = (struct submsg_header *)ptr;

  if (postfix->id != DDSI_RTPS_SMID_SRTPS_POSTFIX)
  {
    printf("check_encoded_data: postfix invalid smid\n");
    goto fail_postfix;
  }

  ptr += sizeof(struct submsg_header);
  remain -= (uint32_t)sizeof(struct submsg_header);

  if (remain < CRYPTO_HMAC_SIZE + sizeof(uint32_t))
  {
    printf("check_encoded_data: crypto_footer incorrect size\n");
    goto fail_postfix;
  }

  *footer = ddsrt_malloc(remain);
  memcpy(*footer, ptr, remain);

  /* length of reader specific macs is in BIG-ENDIAN format */
  (*footer)->length = ddsrt_fromBE4u((*footer)->length);

  return true;

fail_postfix:
fail_body:
  ddsrt_free(*header);
fail_prefix:
  return false;
}

static bool
cipher_sign_data(
    const unsigned char *session_key,
    uint32_t key_size,
    const unsigned char *iv,
    const unsigned char *data,
    uint32_t data_len,
    unsigned char *tag)
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
  else
  {
    assert (0);
    goto fail_encrypt;
  }


  /* Initialise key and IV */
  if (!EVP_EncryptInit_ex(ctx, NULL, NULL, session_key, iv))
  {
    ERR_print_errors_fp(stderr);
    goto fail_encrypt;
  }

  if (!EVP_EncryptUpdate(ctx, NULL, &len, data, (int) data_len))
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

static bool
crypto_decrypt_data(
    uint32_t session_id,
    unsigned char *iv,
    DDS_Security_CryptoTransformKind transformation_kind,
    master_key_material *key_material,
    DDS_Security_OctetSeq *encrypted,
    DDS_Security_OctetSeq *decoded,
    unsigned char *tag)
{
  bool result = true;
  EVP_CIPHER_CTX *ctx;
  crypto_session_key_t session_key = {.data = {0} };
  uint32_t key_size = crypto_get_key_size(CRYPTO_TRANSFORM_KIND(transformation_kind));
  int len = 0;

  if (!crypto_calculate_session_key_test(&session_key, session_id, key_material->master_salt, key_material->master_sender_key, key_material->transformation_kind))
    return false;

  printf("SessionId: %08x\n", session_id);
  print_octets("SessionKey", (const unsigned char *)session_key.data, key_size >> 3);

  /* create the cipher context */
  ctx = EVP_CIPHER_CTX_new();
  if (ctx)
  {
    if (key_size == 128)
    {
      if (!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL))
      {
        ERR_print_errors_fp(stderr);
        result = false;
      }
    }
    else if (key_size == 256)
    {
      if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
      {
        ERR_print_errors_fp(stderr);
        result = false;
      }
    }
    else
    {
      assert (0);
      result = false;
    }
  }
  else
  {
    result = false;
  }

  if (result)
  {
    if (!EVP_DecryptInit_ex(ctx, NULL, NULL, session_key.data, iv))
    {
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
        ERR_print_errors_fp(stderr);
        result = false;
      }
    }
    else
    {
      if (!EVP_DecryptUpdate(ctx, NULL, &len, encrypted->_buffer, (int) encrypted->_length))
      {
        ERR_print_errors_fp(stderr);
        result = false;
      }
    }
  }

  if (result)
  {
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, CRYPTO_HMAC_SIZE, tag))
    {
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
        ERR_print_errors_fp(stderr);
        result = false;
      }
    }
    else
    {
      unsigned char temp[32];
      if (!EVP_DecryptFinal_ex(ctx, temp, &len))
      {
        ERR_print_errors_fp(stderr);
        result = false;
      }
    }
  }

  if (ctx)
    EVP_CIPHER_CTX_free(ctx);

  return result;
}

static session_key_material * get_local_participant_session(DDS_Security_ParticipantCryptoHandle participant_crypto)
{
  local_participant_crypto *participant_crypto_impl = (local_participant_crypto *)participant_crypto;
  assert(participant_crypto_impl);
  return participant_crypto_impl->session;
}

static master_key_material * get_remote_participant_key_material(DDS_Security_ParticipantCryptoHandle participant_crypto)
{
  return crypto_factory_get_master_key_material_for_test(crypto->crypto_key_factory, local_particpant_crypto, participant_crypto);
}

static void set_protection_kind(DDS_Security_ParticipantCryptoHandle participant_crypto, DDS_Security_ProtectionKind protection_kind)
{
  local_participant_crypto *paricipant_crypto_impl = (local_participant_crypto *)participant_crypto;
  paricipant_crypto_impl->rtps_protection_kind = protection_kind;
}

static void set_remote_participant_protection_kind(DDS_Security_ParticipantCryptoHandle participant_crypto, DDS_Security_ProtectionKind protection_kind)
{
  remote_participant_crypto *paricipant_crypto_impl = (remote_participant_crypto *)participant_crypto;
  paricipant_crypto_impl->rtps_protection_kind = protection_kind;
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
  header->length = ddsrt_toBO2u(bo, (uint16_t) length);

  ptr = (unsigned char *)(header + 1);
  memcpy((char *)ptr, sample_test_data, length);

  submsg->_length = submsg->_maximum = (uint32_t) (20 + length + sizeof(struct submsg_header));
  submsg->_buffer = buffer;
}

static bool check_sign(DDS_Security_ParticipantCryptoHandle participant_crypto, uint32_t session_id, uint32_t key_id, uint32_t key_size, unsigned char *init_vector, unsigned char *common_mac, unsigned char *hmac)
{
  master_key_material *keymat;
  crypto_session_key_t key;
  unsigned char md[CRYPTO_HMAC_SIZE];

  memset(md, 0, CRYPTO_HMAC_SIZE);

  keymat = get_remote_participant_key_material(participant_crypto);
  if (key_id != keymat->receiver_specific_key_id)
  {
    printf("check_sign: key_id(%d) does not match key_mat(%d)\n", (int)key_id, (int)keymat->receiver_specific_key_id);
    return false;
  }
  else if (!calculate_receiver_specific_key_test(&key, session_id, keymat->master_salt, keymat->master_receiver_specific_key, keymat->transformation_kind))
  {
    printf("check_sign: calculate key failed\n");
    return false;
  }
  else if (!cipher_sign_data(key.data, key_size, init_vector, common_mac, CRYPTO_HMAC_SIZE, md))
  {
    return false;
  }
  else if (memcmp(hmac, md, CRYPTO_HMAC_SIZE) != 0)
  {
    printf("check_sign: hmac incorrect\n");

    //print_octets("Reader Specific Key:", key, CRYPTO_KEY_SIZE);
    //print_octets("Common:", common_mac, CRYPTO_HMAC_SIZE);
    //print_octets("Hmac:", hmac, CRYPTO_HMAC_SIZE);
    //print_octets("MD:", md, CRYPTO_HMAC_SIZE);
    return false;
  }

  return true;
}

static bool check_signing(
    DDS_Security_ParticipantCryptoHandleSeq *list,
    struct crypto_footer *footer,
    uint32_t session_id,
    unsigned char *init_vector,
    uint32_t key_size)
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
    if (!check_sign(list->_buffer[i], session_id, key_id, key_size, init_vector, footer->common_mac, rmac[i].receiver_mac.data))
    {
      return false;
    }
  }

  return true;
}

static void suite_encode_rtps_message_init(void)
{
  allocate_shared_secret();
  CU_ASSERT_FATAL ((plugins = load_plugins(
                      NULL    /* Access Control */,
                      NULL    /* Authentication */,
                      &crypto /* Cryptograpy    */,
                      NULL)) != NULL);
}

static void suite_encode_rtps_message_fini(void)
{
  deallocate_shared_secret();
  unload_plugins(plugins);
}

static void prepare_participant_security_attributes_and_properties(
    DDS_Security_ParticipantSecurityAttributes *attributes,
    DDS_Security_PropertySeq *properties,
    DDS_Security_CryptoTransformKind_Enum transformation_kind,
    bool is_origin_authenticated)
{
  memset(attributes, 0, sizeof(DDS_Security_ParticipantSecurityAttributes));
  memset(properties, 0, sizeof(DDS_Security_PropertySeq));

  attributes->allow_unauthenticated_participants = false;
  attributes->is_access_protected = false;
  attributes->is_discovery_protected = false;
  attributes->is_liveliness_protected = false;

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
    attributes->is_rtps_protected = true;
    attributes->plugin_participant_attributes = DDS_SECURITY_PARTICIPANT_ATTRIBUTES_FLAG_IS_VALID;
    attributes->plugin_participant_attributes |= DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_ENCRYPTED;
    if (is_origin_authenticated)
    {
      attributes->plugin_participant_attributes |= DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_AUTHENTICATED;
    }
    break;

  case CRYPTO_TRANSFORMATION_KIND_AES256_GMAC:
  case CRYPTO_TRANSFORMATION_KIND_AES128_GMAC:
    attributes->is_rtps_protected = true;
    if (is_origin_authenticated)
    {
      attributes->plugin_participant_attributes = DDS_SECURITY_PARTICIPANT_ATTRIBUTES_FLAG_IS_VALID;
      attributes->plugin_participant_attributes |= DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_AUTHENTICATED;
    }
    break;

  default:
    assert(0);
    break;
  }
}

static void encode_rtps_message_not_authenticated(DDS_Security_CryptoTransformKind_Enum transformation_kind, uint32_t key_size, bool encrypted)
{
  DDS_Security_boolean result;
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
  DDS_Security_ParticipantSecurityAttributes attributes;
  DDS_Security_PropertySeq properties;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform != NULL);
  assert(crypto->crypto_transform != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->encode_rtps_message != NULL);
  assert(crypto->crypto_transform->encode_rtps_message != 0);

  prepare_participant_security_attributes_and_properties(&attributes, &properties, transformation_kind, false);

  register_local_participant(&attributes, &properties);

  initialize_rtps_message(&plain_buffer, DDSRT_BOSEL_NATIVE);

  session_keys = get_local_participant_session(local_particpant_crypto);

  session_keys->master_key_material->transformation_kind = transformation_kind;
  session_keys->key_size = key_size;

  reader_list._length = reader_list._maximum = 1;
  reader_list._buffer = DDS_Security_ParticipantCryptoHandleSeq_allocbuf(1);
  reader_list._buffer[0] = remote_particpant_cryptos[0];
  index = 0;

  /* Now call the function. */
  result = crypto->crypto_transform->encode_rtps_message(
      crypto->crypto_transform,
      &encoded_buffer,
      &plain_buffer,
      local_particpant_crypto,
      &reader_list,
      &index,
      &exception);

  if (!result)
  {
    printf("encode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT_FATAL(result);
  assert(result); // for Clang's static analyzer
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);

  reset_exception(&exception);

  result = check_encoded_data(&encoded_buffer, encrypted, &header, &footer, &data);
  CU_ASSERT_FATAL(result);
  assert(result); // for Clang's static analyzer

  CU_ASSERT(header->transform_identifier.transformation_kind[3] == transformation_kind);

  session_id = ddsrt_bswap4u(*(uint32_t *)header->session_id);

  if (encrypted)
  {
    decoded_buffer._buffer = ddsrt_malloc(plain_buffer._length + 24); /* info_src is added */
    decoded_buffer._length = 0;
    decoded_buffer._maximum = plain_buffer._length + 24; /* info_src is added */

    result = crypto_decrypt_data(session_id, &header->session_id[0], header->transform_identifier.transformation_kind,
                                 session_keys->master_key_material, &data, &decoded_buffer, footer->common_mac);

    if (!result)
    {
      printf("Decode failed\n");
    }

    CU_ASSERT_FATAL(result);

    //print_octets( "PLAIN RTPS:",plain_buffer._buffer+4, plain_buffer._length-4);
    //print_octets( "DECODED RTPS:",decoded_buffer._buffer+8, decoded_buffer._length-8);

    CU_ASSERT_FATAL(memcmp(plain_buffer._buffer + 4, decoded_buffer._buffer + 8, plain_buffer._length - 4) == 0);

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

    CU_ASSERT_FATAL(memcmp(plain_buffer._buffer + 4, data._buffer + 8, plain_buffer._length - 4) == 0);
  }

  DDS_Security_OctetSeq_deinit((&plain_buffer));
  DDS_Security_OctetSeq_deinit((&encoded_buffer));
  DDS_Security_DatareaderCryptoHandleSeq_deinit(&reader_list);

  ddsrt_free(footer);
  ddsrt_free(header);

  DDS_Security_PropertySeq_deinit(&properties);
  unregister_local_participant();
}

CU_Test(ddssec_builtin_encode_rtps_message, encrypt_256, .init = suite_encode_rtps_message_init, .fini = suite_encode_rtps_message_fini)
{
  encode_rtps_message_not_authenticated(CRYPTO_TRANSFORMATION_KIND_AES256_GCM, 256, true);
}

CU_Test(ddssec_builtin_encode_rtps_message, encrypt_128, .init = suite_encode_rtps_message_init, .fini = suite_encode_rtps_message_fini)
{
  encode_rtps_message_not_authenticated(CRYPTO_TRANSFORMATION_KIND_AES128_GCM, 128, true);
}

CU_Test(ddssec_builtin_encode_rtps_message, no_encrypt_256, .init = suite_encode_rtps_message_init, .fini = suite_encode_rtps_message_fini)
{
  encode_rtps_message_not_authenticated(CRYPTO_TRANSFORMATION_KIND_AES256_GMAC, 256, false);
}

CU_Test(ddssec_builtin_encode_rtps_message, no_encrypt_128, .init = suite_encode_rtps_message_init, .fini = suite_encode_rtps_message_fini)
{
  encode_rtps_message_not_authenticated(CRYPTO_TRANSFORMATION_KIND_AES128_GMAC, 128, false);
}

static void encode_rtps_message_sign(DDS_Security_CryptoTransformKind_Enum transformation_kind, uint32_t key_size, DDS_Security_ProtectionKind protection_kind, bool encoded)
{
  DDS_Security_boolean result;
  DDS_Security_DatareaderCryptoHandleSeq reader_list;
  int32_t index;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_OctetSeq plain_buffer;
  DDS_Security_OctetSeq encoded_buffer;
  DDS_Security_OctetSeq decoded_buffer;
  DDS_Security_OctetSeq data;
  DDS_Security_OctetSeq *buffer;
  session_key_material *session_keys;
  DDS_Security_ParticipantSecurityAttributes attributes;
  DDS_Security_PropertySeq properties;
  struct crypto_header *header = NULL;
  struct crypto_footer *footer = NULL;
  uint32_t session_id;
  size_t i;

  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform != NULL);
  assert(crypto->crypto_transform != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->encode_rtps_message != NULL);
  assert(crypto->crypto_transform->encode_rtps_message != 0);

  prepare_participant_security_attributes_and_properties(&attributes, &properties, transformation_kind, true);

  register_local_participant(&attributes, &properties);

  initialize_rtps_message(&plain_buffer, DDSRT_BOSEL_NATIVE);

  CU_ASSERT_FATAL(local_particpant_crypto != 0);

  session_keys = get_local_participant_session(local_particpant_crypto);
  session_keys->master_key_material->transformation_kind = transformation_kind;
  session_keys->key_size = key_size;
  set_protection_kind(local_particpant_crypto, protection_kind);

  register_remote_participants();

  reader_list._length = reader_list._maximum = (uint32_t) (sizeof(remote_particpant_cryptos) / sizeof(remote_particpant_cryptos[0]));
  reader_list._buffer = DDS_Security_ParticipantCryptoHandleSeq_allocbuf(reader_list._maximum);
  for (i = 0; i < sizeof(remote_particpant_cryptos) / sizeof(remote_particpant_cryptos[0]); i++)
  {
    set_remote_participant_protection_kind(remote_particpant_cryptos[i], protection_kind);
    reader_list._buffer[i] = remote_particpant_cryptos[i];
  }
  index = 0;

  /* Now call the function. */

  buffer = &plain_buffer;
  encoded_buffer._buffer = NULL;
  while ((uint32_t) index != reader_list._length)
  {
    result = crypto->crypto_transform->encode_rtps_message(
        crypto->crypto_transform,
        &encoded_buffer,
        buffer,
        local_particpant_crypto,
        &reader_list,
        &index,
        &exception);

    if (!result)
    {
      printf("encode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
    }

    CU_ASSERT_FATAL(result);
    assert(result);
    CU_ASSERT(exception.code == 0);
    CU_ASSERT(exception.message == NULL);

    if (index == 0)
      assert (encoded_buffer._buffer != NULL);

    reset_exception(&exception);
    buffer = NULL;
  }
  assert (encoded_buffer._buffer != NULL);

  result = check_encoded_data(&encoded_buffer, encoded, &header, &footer, &data);
  CU_ASSERT_FATAL(result);
  assert(footer);

  CU_ASSERT(header->transform_identifier.transformation_kind[3] == transformation_kind);

  session_id = ddsrt_bswap4u(*(uint32_t *)header->session_id);

  if (encoded)
  {
    decoded_buffer._buffer = ddsrt_malloc(plain_buffer._length + 24); /* info_src is added */
    decoded_buffer._length = 0;
    decoded_buffer._maximum = plain_buffer._length + 24; /* info_src is added */

    result = crypto_decrypt_data(session_id, &header->session_id[0], header->transform_identifier.transformation_kind,
                                 session_keys->master_key_material, &data, &decoded_buffer, footer->common_mac);
    if (!result)
    {
      printf("Decode failed\n");
    }

    CU_ASSERT_FATAL(result);

    /*TODO: this should consider INFO_SRC */
    CU_ASSERT(memcmp(plain_buffer._buffer + 4, decoded_buffer._buffer + 8, plain_buffer._length - 4) == 0);

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
    CU_ASSERT(memcmp(plain_buffer._buffer + 4, data._buffer + 8, plain_buffer._length - 4) == 0);
  }

  printf("num hmacs = %u\n", footer->length);

  CU_ASSERT(check_signing(&reader_list, footer, session_id, header->session_id, session_keys->key_size));

  unregister_remote_participants();

  DDS_Security_OctetSeq_deinit((&plain_buffer));
  DDS_Security_OctetSeq_deinit((&encoded_buffer));
  DDS_Security_DatareaderCryptoHandleSeq_deinit(&reader_list);
  DDS_Security_PropertySeq_deinit(&properties);

  ddsrt_free(footer);
  ddsrt_free(header);

  unregister_local_participant();
}

CU_Test(ddssec_builtin_encode_rtps_message, encrypt_sign_256, .init = suite_encode_rtps_message_init, .fini = suite_encode_rtps_message_fini)
{
  encode_rtps_message_sign(CRYPTO_TRANSFORMATION_KIND_AES256_GCM, 256, DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION, true);
}

CU_Test(ddssec_builtin_encode_rtps_message, encrypt_sign_128, .init = suite_encode_rtps_message_init, .fini = suite_encode_rtps_message_fini)
{
  encode_rtps_message_sign(CRYPTO_TRANSFORMATION_KIND_AES128_GCM, 128, DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION, true);
}

CU_Test(ddssec_builtin_encode_rtps_message, no_encrypt_sign_256, .init = suite_encode_rtps_message_init, .fini = suite_encode_rtps_message_fini)
{
  encode_rtps_message_sign(CRYPTO_TRANSFORMATION_KIND_AES256_GMAC, 256, DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION, false);
}

CU_Test(ddssec_builtin_encode_rtps_message, no_encrypt_sign_128, .init = suite_encode_rtps_message_init, .fini = suite_encode_rtps_message_fini)
{
  encode_rtps_message_sign(CRYPTO_TRANSFORMATION_KIND_AES128_GMAC, 128, DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION, false);
}

CU_Test(ddssec_builtin_encode_rtps_message, invalid_args, .init = suite_encode_rtps_message_init, .fini = suite_encode_rtps_message_fini)
{
  DDS_Security_boolean result;
  DDS_Security_ParticipantCryptoHandleSeq reader_list;
  DDS_Security_ParticipantCryptoHandleSeq empty_reader_list;
  int32_t index;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_OctetSeq plain_buffer;
  DDS_Security_OctetSeq encoded_buffer;
  DDS_Security_ParticipantSecurityAttributes attributes;
  DDS_Security_PropertySeq properties;
  unsigned i;

  CU_ASSERT_FATAL(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform != NULL);
  CU_ASSERT_FATAL(crypto->crypto_transform->encode_rtps_message != NULL);

  prepare_participant_security_attributes_and_properties(&attributes, &properties, CRYPTO_TRANSFORMATION_KIND_AES256_GCM, true);

  register_local_participant(&attributes, &properties);

  initialize_rtps_message(&plain_buffer, DDSRT_BOSEL_NATIVE);
  memset(&empty_reader_list, 0, sizeof(empty_reader_list));

  CU_ASSERT_FATAL(local_particpant_crypto != 0);
  assert(local_particpant_crypto != 0); // for Clang's static analyzer

  register_remote_participants();
  for (i = 0; i < sizeof (remote_particpant_cryptos) / sizeof (remote_particpant_cryptos[0]); i++)
  {
    assert (remote_particpant_cryptos[i]); // for Clang's static analyzer
    set_remote_participant_protection_kind(remote_particpant_cryptos[i], DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION);
  }

  CU_ASSERT_FATAL(remote_particpant_cryptos[0] != 0);
  assert(remote_particpant_cryptos[0] != 0); // for Clang's static analyzer

  reader_list._length = reader_list._maximum = 1;
  reader_list._buffer = DDS_Security_ParticipantCryptoHandleSeq_allocbuf(1);
  reader_list._buffer[0] = remote_particpant_cryptos[0];
  index = 0;

  /* writer crypto 0 */
  result = crypto->crypto_transform->encode_rtps_message(
      crypto->crypto_transform,
      &encoded_buffer,
      &plain_buffer,
      0,
      &reader_list,
      &index,
      &exception);

  if (!result)
  {
    printf("encode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* writer crypto unknown */
  result = crypto->crypto_transform->encode_rtps_message(
      crypto->crypto_transform,
      &encoded_buffer,
      &plain_buffer,
      1,
      &reader_list,
      &index,
      &exception);

  if (!result)
  {
    printf("encode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* reader crypto list with invalid reader crypto */
  reader_list._buffer[0] = 0;
  result = crypto->crypto_transform->encode_rtps_message(
      crypto->crypto_transform,
      &encoded_buffer,
      &plain_buffer,
      local_particpant_crypto,
      &reader_list,
      &index,
      &exception);

  if (!result)
  {
    printf("encode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* reader crypto list with unknown reader crypto*/
  reader_list._buffer[0] = 1;
  result = crypto->crypto_transform->encode_rtps_message(
      crypto->crypto_transform,
      &encoded_buffer,
      &plain_buffer,
      local_particpant_crypto,
      &reader_list,
      &index,
      &exception);

  if (!result)
  {
    printf("encode_rtps_message: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  reader_list._buffer[0] = local_particpant_crypto;

  unregister_remote_participants();

  DDS_Security_OctetSeq_deinit((&plain_buffer));
  DDS_Security_DatareaderCryptoHandleSeq_deinit(&reader_list);
  DDS_Security_PropertySeq_deinit(&properties);

  unregister_local_participant();
}

