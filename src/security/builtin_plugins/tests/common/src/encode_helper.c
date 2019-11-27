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
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/endian.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/types.h"
#include "dds/ddsrt/environ.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_serialize.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/core/shared_secret.h"
#include "CUnit/CUnit.h"
#include "CUnit/Test.h"
#include "encode_helper.h"
#include "crypto_utils.h"

bool
crypto_calculate_session_key_test(
    crypto_key_t *session_key,
    uint32_t session_id,
    const crypto_salt_t *master_salt,
    const crypto_key_t *master_key,
    DDS_Security_CryptoTransformKind_Enum transformation_kind)
{
  bool result = true;
  uint32_t salt_bytes = CRYPTO_SALT_SIZE_BYTES(transformation_kind);
  uint32_t key_bytes = CRYPTO_KEY_SIZE_BYTES(transformation_kind);
  uint32_t id = ddsrt_toBE4u(session_id);
  char prefix[] = "SessionKey";
  size_t sz = strlen(prefix) + salt_bytes + sizeof(id);
  unsigned char *buffer = ddsrt_malloc (sz);
  memcpy(buffer, prefix, strlen(prefix));
  memcpy(&buffer[strlen(prefix)], &master_salt->data, salt_bytes);
  memcpy(&buffer[strlen(prefix) + salt_bytes], &id, sizeof(id));
  if (HMAC(EVP_sha256(), master_key->data, (int)key_bytes, buffer, sz, session_key->data, NULL) == NULL)
  {
    ERR_print_errors_fp(stderr);
    result = false;
  }
  ddsrt_free (buffer);
  return result;
}

bool calculate_receiver_specific_key_test(
    unsigned char *session_key,
    uint32_t session_id,
    const crypto_salt_t *master_salt,
    const crypto_key_t *master_key,
    DDS_Security_CryptoTransformKind_Enum transformation_kind)
{
  bool result = true;
  uint32_t salt_bytes = CRYPTO_SALT_SIZE_BYTES(transformation_kind);
  uint32_t key_bytes = CRYPTO_KEY_SIZE_BYTES(transformation_kind);
  uint32_t id = ddsrt_toBE4u(session_id);
  char prefix[] = "SessionReceiverKey";
  size_t sz = strlen(prefix) + salt_bytes + sizeof(id);
  unsigned char *buffer = ddsrt_malloc (sz);
  memcpy(buffer, prefix, strlen(prefix));
  memcpy(&buffer[strlen(prefix)], &master_salt->data, salt_bytes);
  memcpy(&buffer[strlen(prefix) + salt_bytes], &id, sizeof(id));
  if (HMAC(EVP_sha256(), master_key->data, (int)key_bytes, buffer, sz, session_key, NULL) == NULL)
  {
    ERR_print_errors_fp(stderr);
    result = false;
  }
  ddsrt_free (buffer);
  return result;
}

