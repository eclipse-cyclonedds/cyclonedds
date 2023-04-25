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

#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/types.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/openssl_support.h"
#include "crypto_defs.h"
#include "crypto_utils.h"

char *crypto_openssl_error_message(void)
{
  BIO *bio = BIO_new(BIO_s_mem());
  char *msg;
  char *buf = NULL;
  size_t len;

  if (!bio)
    return ddsrt_strdup ("BIO_new failed");

  ERR_print_errors(bio);
  len = (size_t) BIO_get_mem_data(bio, &buf);
  msg = ddsrt_malloc(len + 1);
  memset(msg, 0, len + 1);
  memcpy(msg, buf, len);
  BIO_free(bio);
  return msg;
}

static bool
crypto_calculate_key_impl(
    const char *prefix,
    crypto_session_key_t *session_key,
    uint32_t session_id,
    const unsigned char *master_salt,
    const unsigned char *master_key,
    DDS_Security_CryptoTransformKind_Enum transformation_kind,
    DDS_Security_SecurityException *ex)
{
  uint32_t key_bytes = CRYPTO_KEY_SIZE_BYTES(transformation_kind);
  uint32_t id = ddsrt_toBE4u(session_id);
  size_t sz = strlen(prefix) + key_bytes + sizeof(id);
  unsigned char *buffer = ddsrt_malloc (sz);
  unsigned char md[EVP_MAX_MD_SIZE];

  memcpy(buffer, prefix, strlen(prefix));
  memcpy(&buffer[strlen(prefix)], master_salt, key_bytes);
  memcpy(&buffer[strlen(prefix) + key_bytes], &id, sizeof(id));

  if (HMAC(EVP_sha256(), master_key, (int)key_bytes, buffer, sz, md, NULL) == NULL)
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "HMAC failed: ");
    ddsrt_free (buffer);
    return false;
  }
  memcpy (session_key->data, md, key_bytes);
  ddsrt_free (buffer);
  return true;
}

bool
crypto_calculate_session_key(
    crypto_session_key_t *session_key,
    uint32_t session_id,
    const unsigned char *master_salt,
    const unsigned char *master_key,
    DDS_Security_CryptoTransformKind_Enum transformation_kind,
    DDS_Security_SecurityException *ex)
{
  return crypto_calculate_key_impl("SessionKey", session_key, session_id, master_salt, master_key, transformation_kind, ex);
}

bool
crypto_calculate_receiver_specific_key(
    crypto_session_key_t *session_key,
    uint32_t session_id,
    const unsigned char *master_salt,
    const unsigned char *master_key,
    DDS_Security_CryptoTransformKind_Enum transformation_kind,
    DDS_Security_SecurityException *ex)
{
  return crypto_calculate_key_impl("SessionReceiverKey", session_key, session_id, master_salt, master_key, transformation_kind, ex);
}

uint32_t
crypto_get_key_size(
    DDS_Security_CryptoTransformKind_Enum kind)
{
  switch (kind)
  {
  case CRYPTO_TRANSFORMATION_KIND_AES128_GMAC:
  case CRYPTO_TRANSFORMATION_KIND_AES128_GCM:
    return 128;
  case CRYPTO_TRANSFORMATION_KIND_AES256_GMAC:
  case CRYPTO_TRANSFORMATION_KIND_AES256_GCM:
    return 256;
  default:
    return 0;
  }
}

uint32_t
crypto_get_random_uint32(void)
{
  uint32_t val;
  RAND_bytes((unsigned char *)&val, sizeof(uint32_t));
  return val;
}

uint64_t
crypto_get_random_uint64(void)
{
  uint64_t val;
  RAND_bytes((unsigned char *)&val, sizeof(uint64_t));
  return val;
}

unsigned char *
crypto_hmac256(
    const unsigned char *key,
    uint32_t key_size,
    const unsigned char *data,
    uint32_t data_size,
    DDS_Security_SecurityException *ex)
{
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned char *result;

  DDSRT_STATIC_ASSERT (EVP_MAX_MD_SIZE <= INT32_MAX);
  assert (key_size <= EVP_MAX_MD_SIZE);
  if (HMAC(EVP_sha256(), key, (int) key_size, data, data_size, md, NULL) == NULL)
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, 0, "Failed to init hashing context: ");
    return NULL;
  }
  result = ddsrt_malloc(key_size);
  memcpy (result, md, key_size);
  return result;
}
