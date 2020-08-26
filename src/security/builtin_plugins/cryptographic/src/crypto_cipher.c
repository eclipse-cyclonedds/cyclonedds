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

#ifndef NDEBUG
static bool check_buffer_sizes(const size_t num, const crypto_data_t *inp, const crypto_data_t *outp)
{
	size_t i, total = 0;

	for (i = 0; i < num; i++) {
		total += inp[i].length;
	}
	if (total >= INT_MAX)
	  return false;

	if (outp && outp->length != total)
	  return false;

	return true;
}
#endif

bool crypto_cipher_encrypt_data(const crypto_session_key_t *session_key, uint32_t key_size, const struct init_vector *iv, const size_t num_inp, const crypto_data_t *inpdata, crypto_data_t *outpdata, crypto_hmac_t *tag, DDS_Security_SecurityException *ex)
{
  const EVP_CIPHER *evp = (key_size != 256) ? EVP_aes_128_gcm() : EVP_aes_256_gcm();
  bool result = false;
  EVP_CIPHER_CTX *ctx;
  unsigned char *ptr = outpdata ? outpdata->base : NULL;
  size_t i, total_size = 0;
  int len = 0;

  assert(iv);
  assert(num_inp > 0);
  assert(inpdata);
  assert(key_size == 128 || key_size == 256);
  assert(check_buffer_sizes(num_inp, inpdata, outpdata));

  /* create the cipher context */
  if ((ctx = EVP_CIPHER_CTX_new()) == NULL)
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_CIPHER_CTX_new failed: ");
    return false;
  }

  /* initialize the cipher and set to AES GCM */
  if (!EVP_EncryptInit_ex(ctx, evp, NULL, NULL, NULL))
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_EncryptInit_ex to set aes_128_gcm failed: ");
    goto fail_encrypt;
  }

  /* Initialise key and IV */
  if (!EVP_EncryptInit_ex(ctx, NULL, NULL, session_key->data, iv->u))
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_EncryptInit_ex failed: ");
    goto fail_encrypt;
  }

  /* encrypt the message */
  for (i = 0; i < num_inp; i++)
  {
    if (!EVP_EncryptUpdate(ctx, ptr, &len, inpdata[i].base, (int) inpdata[i].length))
    {
      DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_EncryptUpdate update data failed: ");
      goto fail_encrypt;
    }
    assert (len >= 0); /* conform openssl spec */
    total_size += (size_t)len;
    ptr = ptr ? ptr+len : NULL;
  }

  /* finalize the encryption */
  if (outpdata)
  {
    outpdata->length = total_size;

    if (!EVP_EncryptFinal_ex(ctx, ptr, &len))
    {
      DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_EncryptFinal_ex to finalize encryption failed: ");
      goto fail_encrypt;
    }
    assert (len >= 0); /* conform openssl spec */
    outpdata->length += (size_t)len;
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

  result = true;

fail_encrypt:
  EVP_CIPHER_CTX_free(ctx);
  return result;
}

bool crypto_cipher_decrypt_data(const remote_session_info *session, const struct init_vector *iv, const size_t num_inp, const crypto_data_t *inpdata, crypto_data_t *outpdata, crypto_hmac_t *tag, DDS_Security_SecurityException *ex)
{
  bool result = false;
  const EVP_CIPHER *evp = (session->key_size != 256) ? EVP_aes_128_gcm() : EVP_aes_256_gcm();
  unsigned char *ptr = outpdata ? outpdata->base : NULL;
  EVP_CIPHER_CTX *ctx;
  size_t i, total_size = 0;
  int len = 0;

  assert(iv);
  assert(num_inp > 0);
  assert(inpdata);
  assert(session->key_size == 128 || session->key_size == 256);
  assert(check_buffer_sizes(num_inp, inpdata, outpdata));

  /* create the cipher context */
  ctx = EVP_CIPHER_CTX_new();
  if (!ctx)
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_CIPHER_CTX_new failed: ");
    return false;
  }

  /* initialize the cipher and set to AES GCM */
  if (EVP_DecryptInit_ex(ctx, evp, NULL, NULL, NULL) != 1)
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_DecryptInit_ex to set aes_128_gcm failed: ");
    goto fail_decrypt;
  }

  /* Initialise key and IV */
  if (EVP_DecryptInit_ex(ctx, NULL, NULL, session->key.data, iv->u) != 1)
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_DecryptInit_ex to set aes_256_gcm failed: %s");
    goto fail_decrypt;
  }

  /* Set expected tag value. */
  if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, CRYPTO_HMAC_SIZE, tag->data) != 1)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_CIPHER_CTX_ctrl to get tag failed: ");
    goto fail_decrypt;
  }

  /* decrypt the message */
  for (i = 0; i < num_inp; i++)
  {
    if (!EVP_DecryptUpdate(ctx, ptr, &len, inpdata[i].base, (int) inpdata[i].length))
    {
      DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_DecryptUpdate update data failed: ");
      goto fail_decrypt;
    }
    assert (len >= 0); /* conform openssl spec */
    total_size += (size_t)len;
    ptr = ptr ? ptr+len : NULL;
  }

  /* finalize the decryption */
  if (outpdata)
  {
    outpdata->length = total_size;

    if (!EVP_DecryptFinal_ex(ctx, ptr, &len))
    {
      DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_DecryptFinal_ex to finalize decryption failed: ");
      goto fail_decrypt;
    }
    assert (len >= 0); /* conform openssl spec */
    outpdata->length += (size_t)len;
  }
  else
  {
    unsigned char temp[32];
    if (!EVP_DecryptFinal_ex(ctx, temp, &len))
    {
      DDS_Security_Exception_set_with_openssl_error(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "EVP_DecryptFinal_ex to signature check failed: ");
      goto fail_decrypt;
    }
  }

  result = true;

fail_decrypt:
  EVP_CIPHER_CTX_free(ctx);
  return result;
}


