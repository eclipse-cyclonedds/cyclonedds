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
#include "crypto_helper.h"
#include "crypto_utils.h"


static bool
crypto_calculate_session_key_impl_test(
    const char *prefix,
    crypto_session_key_t *session_key,
    uint32_t session_id,
    const unsigned char *master_salt,
    const unsigned char *master_key,
    DDS_Security_CryptoTransformKind_Enum transformation_kind)
{
  bool result = true;
  uint32_t key_bytes = CRYPTO_KEY_SIZE_BYTES(transformation_kind);
  uint32_t id = ddsrt_toBE4u(session_id);
  size_t sz = strlen(prefix) + key_bytes + sizeof(id);
  unsigned char *buffer = ddsrt_malloc (sz);
  memcpy(buffer, prefix, strlen(prefix));
  memcpy(&buffer[strlen(prefix)], master_salt, key_bytes);
  memcpy(&buffer[strlen(prefix) + key_bytes], &id, sizeof(id));
  if (HMAC(EVP_sha256(), master_key, (int)key_bytes, buffer, sz, session_key->data, NULL) == NULL)
  {
    ERR_print_errors_fp(stderr);
    result = false;
  }
  ddsrt_free (buffer);
  return result;
}

bool
crypto_calculate_session_key_test(
    crypto_session_key_t *session_key,
    uint32_t session_id,
    const unsigned char *master_salt,
    const unsigned char *master_key,
    DDS_Security_CryptoTransformKind_Enum transformation_kind)
{
  return crypto_calculate_session_key_impl_test ("SessionKey", session_key, session_id, master_salt, master_key, transformation_kind);
}

bool calculate_receiver_specific_key_test(
    crypto_session_key_t *session_key,
    uint32_t session_id,
    const unsigned char *master_salt,
    const unsigned char *master_key,
    DDS_Security_CryptoTransformKind_Enum transformation_kind)
{
  return crypto_calculate_session_key_impl_test ("SessionReceiverKey", session_key, session_id, master_salt, master_key, transformation_kind);
}

int master_salt_not_empty(master_key_material *keymat)
{
  uint32_t key_bytes = CRYPTO_KEY_SIZE_BYTES(keymat->transformation_kind);
  for (uint32_t i = 0; i < key_bytes; i++)
  {
    if (keymat->master_salt[i])
      return 1;
  }
  return 0;
}

int master_key_not_empty(master_key_material *keymat)
{
  uint32_t key_bytes = CRYPTO_KEY_SIZE_BYTES(keymat->transformation_kind);
  for (uint32_t i = 0; i < key_bytes; i++)
  {
    if (keymat->master_sender_key[i])
      return 1;
  }
  return 0;
}

int master_receiver_specific_key_not_empty(master_key_material *keymat)
{
  uint32_t key_bytes = CRYPTO_KEY_SIZE_BYTES(keymat->transformation_kind);
  for (uint32_t i = 0; i < key_bytes; i++)
  {
    if (keymat->master_receiver_specific_key[i])
      return 1;
  }
  return 0;
}
