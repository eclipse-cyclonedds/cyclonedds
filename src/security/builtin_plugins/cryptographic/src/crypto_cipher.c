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
#include <stddef.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/types.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/security/openssl_support.h"
#include "crypto_defs.h"
#include "crypto_utils.h"
#include "crypto_cipher.h"

#define SSLERROR(lab_, operstr_) do { \
  DDS_Security_Exception_set_with_openssl_error (ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, operstr_ "failed: "); \
  goto lab_; \
} while (0)

#ifndef NDEBUG
static bool check_buffer_sizes (const size_t num, const const_tainted_crypto_data_t *inp, const tainted_crypto_data_t *outp)
  ddsrt_nonnull((2)) ddsrt_attribute_warn_unused_result;

static bool check_buffer_sizes (const size_t num, const const_tainted_crypto_data_t *inp, const tainted_crypto_data_t *outp)
{
	size_t total = 0;
	for (size_t i = 0; i < num; i++)
  {
    if (inp[i].length > INT_MAX) // OpenSSL wants "int"
      return false;
    if (inp[i].length > SIZE_MAX - total)
      return false;
		total += inp[i].length;
	}
  return outp == NULL || outp->length == total;
}

static bool trusted_check_buffer_sizes (const size_t num, const trusted_crypto_data_t *inp, const trusted_crypto_data_t *outp)
  ddsrt_nonnull((2)) ddsrt_attribute_warn_unused_result;

static bool trusted_check_buffer_sizes (const size_t num, const trusted_crypto_data_t *inp, const trusted_crypto_data_t *outp)
{
  DDSRT_STATIC_ASSERT (sizeof (trusted_crypto_data_t) == sizeof (tainted_crypto_data_t));
  DDSRT_STATIC_ASSERT (offsetof (trusted_crypto_data_t, x.base) == offsetof (tainted_crypto_data_t, base));
  DDSRT_STATIC_ASSERT (offsetof (trusted_crypto_data_t, x.length) == offsetof (tainted_crypto_data_t, length));
  return check_buffer_sizes (num, (const const_tainted_crypto_data_t *) inp, outp ? &outp->x : NULL);
}
#endif

bool crypto_cipher_encrypt_data (const crypto_session_key_t *session_key, uint32_t key_size, const struct init_vector *iv, const size_t num_inp, const trusted_crypto_data_t *inpdata, trusted_crypto_data_t *outpdata, crypto_hmac_t *tag, DDS_Security_SecurityException *ex)
{
  assert (session_key);
  assert (iv);
  assert (num_inp > 0);
  assert (inpdata);
  assert (key_size == 128 || key_size == 256);
  assert (trusted_check_buffer_sizes (num_inp, inpdata, outpdata));

  EVP_CIPHER const * const evp = (key_size != 256) ? EVP_aes_128_gcm () : EVP_aes_256_gcm ();
  EVP_CIPHER_CTX *ctx;
  unsigned char *ptr = outpdata ? outpdata->x.base : NULL;

  if ((ctx = EVP_CIPHER_CTX_new ()) == NULL)
    SSLERROR (fail_context_new, "EVP_CIPHER_CTX_new");
  if (!EVP_EncryptInit_ex (ctx, evp, NULL, NULL, NULL))
    SSLERROR (fail_encrypt, "EVP_EncryptInit_ex to set aes_128_gcm/aes_256_gcm");
  if (!EVP_EncryptInit_ex (ctx, NULL, NULL, session_key->data, iv->u))
    SSLERROR (fail_encrypt, "EVP_EncryptInit_ex to set key and IV");

  for (size_t i = 0; i < num_inp; i++)
  {
    assert (inpdata[i].x.length <= INT_MAX);
    int len;
    if (!EVP_EncryptUpdate (ctx, ptr, &len, inpdata[i].x.base, (int) inpdata[i].x.length))
      SSLERROR (fail_encrypt, "EVP_EncryptUpdate update data");
    assert (len >= 0); /* conform openssl spec */
    if (ptr)
      ptr += len;
  }

  if (outpdata)
  {
    int len;
    if (!EVP_EncryptFinal_ex (ctx, ptr, &len))
      SSLERROR (fail_encrypt, "EVP_EncryptFinal_ex to finalize encryption");
    assert (len >= 0); /* conform openssl spec */
    outpdata->x.length = (size_t) (ptr + len - outpdata->x.base);
  }
  else
  {
    unsigned char temp[32];
    int len;
    if (!EVP_EncryptFinal_ex (ctx, temp, &len))
      SSLERROR (fail_encrypt, "EVP_EncryptFinal_ex to finalize aad");
  }

  /* get the tag */
  if (!EVP_CIPHER_CTX_ctrl (ctx, EVP_CTRL_GCM_GET_TAG, CRYPTO_HMAC_SIZE, tag->data))
    SSLERROR (fail_encrypt, "EVP_CIPHER_CTX_ctrl to get the tag");

  EVP_CIPHER_CTX_free (ctx);
  return true;

fail_encrypt:
  EVP_CIPHER_CTX_free (ctx);
fail_context_new:
  return false;
}

bool crypto_cipher_calc_hmac (const crypto_session_key_t *session_key, uint32_t key_size, const struct init_vector *iv, const tainted_crypto_data_t *inpdata, crypto_hmac_t *tag, DDS_Security_SecurityException *ex)
{
  const trusted_crypto_data_t inpdata_wrapper = { *inpdata };
  if (inpdata_wrapper.x.length > INT_MAX)
  {
    DDS_Security_Exception_set (ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "oversize data fragment");
    return false;
  }
  return crypto_cipher_encrypt_data (session_key, key_size, iv, 1, &inpdata_wrapper, NULL, tag, ex);
}

bool crypto_cipher_decrypt_data (const remote_session_info *session, const struct init_vector *iv, const size_t num_inp, const const_tainted_crypto_data_t *inpdata, tainted_crypto_data_t *outpdata, crypto_hmac_t *tag, DDS_Security_SecurityException *ex)
{
  assert (session);
  assert (iv);
  assert (num_inp > 0);
  assert (inpdata);
  assert (session->key_size == 128 || session->key_size == 256);
  assert (check_buffer_sizes (num_inp, inpdata, outpdata));

  EVP_CIPHER const * const evp = (session->key_size != 256) ? EVP_aes_128_gcm () : EVP_aes_256_gcm ();
  unsigned char *ptr = outpdata ? outpdata->base : NULL;
  EVP_CIPHER_CTX *ctx;

  if ((ctx = EVP_CIPHER_CTX_new ()) == NULL)
    SSLERROR (fail_context_new, "EVP_CIPHER_CTX_new");
  if (!EVP_DecryptInit_ex (ctx, evp, NULL, NULL, NULL))
    SSLERROR (fail_decrypt, "EVP_DecryptInit_ex to set aes_128_gcm/aes_256_gcm");
  if (!EVP_DecryptInit_ex (ctx, NULL, NULL, session->key.data, iv->u))
    SSLERROR (fail_decrypt, "EVP_DecryptInit_ex to set key and IV");

  /* Set expected tag value. */
  if (!EVP_CIPHER_CTX_ctrl (ctx, EVP_CTRL_GCM_SET_TAG, CRYPTO_HMAC_SIZE, tag->data))
    SSLERROR (fail_decrypt, "EVP_CIPHER_CTX_ctrl to set expected tag");

  for (size_t i = 0; i < num_inp; i++)
  {
    if (inpdata[i].length > INT_MAX)
    {
      DDS_Security_Exception_set (ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "oversize data fragment");
      goto fail_decrypt;
    }

    int len;
    if (!EVP_DecryptUpdate (ctx, ptr, &len, inpdata[i].base, (int) inpdata[i].length))
      SSLERROR (fail_decrypt, "EVP_DecryptUpdate update data");
    assert (len >= 0); /* conform openssl spec */
    if (ptr)
      ptr += len;
  }

  if (outpdata)
  {
    int len;
    if (!EVP_DecryptFinal_ex (ctx, ptr, &len))
      SSLERROR (fail_decrypt, "EVP_DecryptFinal_ex to finalize decryption");
    assert (len >= 0); /* conform openssl spec */
    outpdata->length = (size_t) (ptr + len - outpdata->base);
  }
  else
  {
    unsigned char temp[32];
    int len;
    if (!EVP_DecryptFinal_ex (ctx, temp, &len))
      SSLERROR (fail_decrypt, "EVP_EncryptFinal_ex to finalize signature check");
  }

  EVP_CIPHER_CTX_free(ctx);
  return true;

fail_decrypt:
  EVP_CIPHER_CTX_free(ctx);
fail_context_new:
  return false;
}
