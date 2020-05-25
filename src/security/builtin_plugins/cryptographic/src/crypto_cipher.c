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

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/types.h"
#include "dds/security/openssl_support.h"
#include "crypto_defs.h"
#include "crypto_utils.h"
#include "crypto_cipher.h"

bool crypto_cipher_encrypt_data(
  const crypto_session_key_t *session_key,
  uint32_t key_size,
  const unsigned char *iv,
  const unsigned char *data,
  uint32_t data_len,
  const unsigned char *aad,
  uint32_t aad_len,
  unsigned char *encrypted,
  uint32_t *encrypted_len,
  crypto_hmac_t *tag,
  DDS_Security_SecurityException *ex)
{
  EVP_CIPHER_CTX *ctx;
  int len = 0;

  /* create the cipher context */
  ctx = EVP_CIPHER_CTX_new();
  if (!ctx)
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_CIPHER_CTX_new failed: ");
    goto fail_ctx_new;
  }

  /* initialize the cipher and set to AES GCM */
  if (key_size == 128)
  {
    if (!EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL))
    {
      DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_EncryptInit_ex to set aes_128_gcm failed: ");
      goto fail_encrypt;
    }
  }
  else if (key_size == 256)
  {
    if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL))
    {
      DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_EncryptInit_ex to set aes_256_gcm failed: ");
      goto fail_encrypt;
    }
  }
  else
  {
    assert(0);
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_EncryptInit_ex invalid key size: %u", key_size);
    goto fail_encrypt;
  }

  /* Initialise key and IV */
  if (!EVP_EncryptInit_ex(ctx, NULL, NULL, session_key->data, iv))
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_EncryptInit_ex failed: ");
    goto fail_encrypt;
  }

  if (aad)
  {
    if (aad_len > INT_MAX)
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_EncryptUpdate to update aad failed: aad_len exceeds INT_MAX");
      goto fail_encrypt;
    }

    /* Provide any AAD data */
    if (!EVP_EncryptUpdate(ctx, NULL, &len, aad, (int) aad_len))
    {
      DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_EncryptUpdate to update aad failed: %s");
      goto fail_encrypt;
    }
  }

  if (data)
  {
    if (data_len > INT_MAX)
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_EncryptUpdate to update data failed: data_len exceeds INT_MAX");
      goto fail_encrypt;
    }

    /* encrypt the message */
    if (!EVP_EncryptUpdate(ctx, encrypted, &len, data, (int) data_len))
    {
      DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_EncryptUpdate update data failed: ");
      goto fail_encrypt;
    }
    assert (len >= 0); /* conform openssl spec */
    *encrypted_len = (uint32_t) len;
  }

  /* finalize the encryption */
  if (data)
  {
    if (!EVP_EncryptFinal_ex(ctx, encrypted + len, &len))
    {
      DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_EncryptFinal_ex to finalize encryption failed: ");
      goto fail_encrypt;
    }
    assert (len >= 0); /* conform openssl spec */
    *encrypted_len += (uint32_t) len;
  }
  else
  {
    unsigned char temp[32];
    if (!EVP_EncryptFinal_ex(ctx, temp, &len))
    {
      DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_EncryptFinal_ex to finalize aad failed: ");
      goto fail_encrypt;
    }
  }

  /* get the tag */
  if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, CRYPTO_HMAC_SIZE, tag->data))
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_CIPHER_CTX_ctrl to get the tag failed: ");
    goto fail_encrypt;
  }

  EVP_CIPHER_CTX_free(ctx);
  return true;

fail_encrypt:
  EVP_CIPHER_CTX_free(ctx);
fail_ctx_new:
  return false;
}

bool crypto_cipher_decrypt_data(
  const remote_session_info *session,
  const unsigned char *iv,
  const unsigned char *encrypted,
  uint32_t encrypted_len,
  const unsigned char *aad,
  uint32_t aad_len,
  unsigned char *data,
  uint32_t *data_len,
  crypto_hmac_t *tag,
  DDS_Security_SecurityException *ex)
{
  EVP_CIPHER_CTX *ctx;
  int len = 0;

  /* create the cipher context */
  ctx = EVP_CIPHER_CTX_new();
  if (!ctx)
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_CIPHER_CTX_new failed: ");
    goto fail_ctx_new;
  }

  /* initialize the cipher and set to AES GCM */
  if (session->key_size == 128)
  {
    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL) != 1)
    {
      DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_DecryptInit_ex to set aes_128_gcm failed: ");
      goto fail_decrypt;
    }
  }
  else if (session->key_size == 256)
  {
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1)
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_DecryptInit_ex to set aes_256_gcm failed: ");
      goto fail_decrypt;
    }
  }
  else
  {
    assert(0);
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "Internal key_size is not correct: %u", session->key_size);
    goto fail_decrypt;
  }

  /* Initialise key and IV */
  if (EVP_DecryptInit_ex(ctx, NULL, NULL, session->key.data, iv) != 1)
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_DecryptInit_ex to set aes_256_gcm failed: %s");
    goto fail_decrypt;
  }

  if (aad)
  {
    assert (aad_len <= INT32_MAX);
    /* Provide any AAD data for signature */
    if (EVP_DecryptUpdate(ctx, NULL, &len, aad, (int) aad_len) != 1)
    {
      DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_DecryptUpdate to update aad failed: ");
      goto fail_decrypt;
    }
  }

  /* Set expected tag value. */
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, CRYPTO_HMAC_SIZE, tag->data) != 1)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_CIPHER_CTX_ctrl to get tag failed: ");
    goto fail_decrypt;
  }

  if (data)
  {
    /* decrypt the message */
    if (EVP_DecryptUpdate(ctx, data, &len, encrypted, (int) encrypted_len) != 1)
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_DecryptUpdate update data failed: ");
      goto fail_decrypt;
    }
    assert (len >= 0);
    *data_len = (uint32_t)len;
  }

  if (data)
  {
    if (EVP_DecryptFinal_ex(ctx, data + len, &len) != 1)
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_DecryptFinal_ex to finalize decryption failed: ");
      goto fail_decrypt;
    }
    assert (len >= 0);
    *data_len += (uint32_t)len;
  }
  else
  {
    unsigned char temp[32];
    if (EVP_DecryptFinal_ex(ctx, temp, &len) != 1)
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_DecryptFinal_ex to finalize signature check failed: ");
      goto fail_decrypt;
    }
  }

  EVP_CIPHER_CTX_free(ctx);
  return true;

fail_decrypt:
  EVP_CIPHER_CTX_free(ctx);
fail_ctx_new:
  return false;
}
