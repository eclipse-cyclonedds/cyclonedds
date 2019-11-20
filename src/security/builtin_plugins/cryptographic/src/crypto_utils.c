/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <assert.h>
#include <string.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/types.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_utils.h"
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

bool
crypto_calculate_session_key(
    crypto_key_t *session_key,
    uint32_t session_id,
    const crypto_salt_t *master_salt,
    const crypto_key_t *master_key,
    DDS_Security_SecurityException *ex)
{
#define BUFFER_LEN 10 + CRYPTO_SALT_SIZE + 4
  uint32_t id = ddsrt_toBE4u(session_id);
  char prefix[] = "SessionKey";
  unsigned char buffer[BUFFER_LEN];
  memcpy(buffer, prefix, strlen(prefix));
  memcpy(&buffer[strlen(prefix)], &master_salt->data, CRYPTO_SALT_SIZE);
  memcpy(&buffer[strlen(prefix) + CRYPTO_SALT_SIZE], &id, 4);
  if (HMAC(EVP_sha256(), master_key->data, CRYPTO_KEY_SIZE, buffer, BUFFER_LEN, session_key->data, NULL) == NULL)
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "HMAC failed: ");
    return false;
  }
  return true;
#undef BUFFER_LEN
}

bool
crypto_calculate_receiver_specific_key(
    crypto_key_t *session_key,
    uint32_t session_id,
    const crypto_salt_t *master_salt,
    const crypto_key_t *master_key,
    DDS_Security_SecurityException *ex)
{
#define BUFFER_LEN 18 + CRYPTO_SALT_SIZE + 4
  uint32_t id = ddsrt_toBE4u(session_id);
  char prefix[] = "SessionReceiverKey";
  unsigned char buffer[BUFFER_LEN];
  memcpy(buffer, prefix, strlen(prefix));
  memcpy(&buffer[strlen(prefix)], &master_salt->data, CRYPTO_SALT_SIZE);
  memcpy(&buffer[strlen(prefix) + CRYPTO_SALT_SIZE], &id, 4);
  if (HMAC(EVP_sha256(), master_key->data, CRYPTO_KEY_SIZE, buffer, BUFFER_LEN, session_key->data, NULL) == NULL)
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "HMAC failed: ");
    return false;
  }
  return true;
#undef BUFFER_LEN
}

uint32_t
crypto_get_key_size(
    DDS_Security_CryptoTransformKind_Enum kind)
{
  uint32_t size = 0;

  switch (kind)
  {
  case CRYPTO_TRANSFORMATION_KIND_AES128_GMAC:
  case CRYPTO_TRANSFORMATION_KIND_AES128_GCM:
    size = 128;
    break;
  case CRYPTO_TRANSFORMATION_KIND_AES256_GMAC:
  case CRYPTO_TRANSFORMATION_KIND_AES256_GCM:
    size = 256;
    break;
  default:
    assert(0);
    break;
  }

  return size;
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
  unsigned char *result;

  if (key_size > INT32_MAX)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, 0, "Failed to init hashing context: invalid key size");
    return NULL;
  }

  result = ddsrt_malloc(key_size);
  if (HMAC(EVP_sha256(), key, (int) key_size, data, data_size, result, NULL) == NULL)
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, 0, "Failed to init hashing context: ");
    ddsrt_free(result);
    return NULL;
  }
  return result;
}
