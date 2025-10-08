// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdlib.h>
#include <string.h>

#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/security/openssl_support.h"
#include "CUnit/Test.h"
#include "cert_utils.h"

#define MAX_EMAIL 255
#define EMAIL_HOST "cycloneddssecurity.adlinktech.com"

static X509 * get_x509(int not_valid_before, int not_valid_after, const char * cn, const char * email)
{
  X509 * cert = X509_new ();
  CU_ASSERT_NEQ_FATAL (cert, NULL);
  ASN1_INTEGER_set (X509_get_serialNumber (cert), 1);
  X509_gmtime_adj (X509_getm_notBefore (cert), not_valid_before);
  X509_gmtime_adj (X509_getm_notAfter (cert), not_valid_after);

  X509_NAME * name = X509_get_subject_name (cert);
  X509_NAME_add_entry_by_txt (name, "C",  MBSTRING_ASC, (unsigned char *) "NL", -1, -1, 0);
  X509_NAME_add_entry_by_txt (name, "O",  MBSTRING_ASC, (unsigned char *) "Example Organization", -1, -1, 0);
  X509_NAME_add_entry_by_txt (name, "CN", MBSTRING_ASC, (unsigned char *) cn, -1, -1, 0);
  X509_NAME_add_entry_by_txt (name, "emailAddress", MBSTRING_ASC, (unsigned char *) email, -1, -1, 0);
  return cert;
}

static char * get_x509_data(X509 * cert)
{
  // Create BIO for writing output
  BIO *output_bio = BIO_new (BIO_s_mem ());
  if (!PEM_write_bio_X509 (output_bio, cert)) {
    printf ("Error writing certificate\n");
    ERR_print_errors_fp (stderr);
    CU_FAIL ("oops");
  }

  // Get string
  char *output_tmp = NULL;
  size_t output_sz = (size_t) BIO_get_mem_data (output_bio, &output_tmp);
  char * output = ddsrt_malloc (output_sz + 1);
  memcpy (output, output_tmp, output_sz);
  output[output_sz] = 0;
  BIO_free (output_bio);

  return output;
}

static EVP_PKEY * get_priv_key(const char * priv_key_str)
{
  BIO *pkey_bio = BIO_new_mem_buf (priv_key_str, -1);
  EVP_PKEY *priv_key = PEM_read_bio_PrivateKey (pkey_bio, NULL, NULL, 0);
  CU_ASSERT_NEQ_FATAL (priv_key, NULL);
  BIO_free (pkey_bio);
  return priv_key;
}

static X509 * get_cert(const char * cert_str)
{
  BIO *cert_bio = BIO_new_mem_buf (cert_str, -1);
  X509 *cert = PEM_read_bio_X509 (cert_bio, NULL, NULL, 0);
  CU_ASSERT_NEQ_FATAL (cert, NULL);
  BIO_free (cert_bio);
  return cert;
}

static char * generate_ca_internal(const char *ca_name, EVP_PKEY *ca_priv_key, int not_valid_before, int not_valid_after)
{
  char * email = malloc (MAX_EMAIL);
  snprintf(email, MAX_EMAIL, "%s@%s" , ca_name, EMAIL_HOST);
  X509 * ca_cert = get_x509 (not_valid_before, not_valid_after, ca_name, email);
  ddsrt_free (email);

  X509_set_pubkey (ca_cert, ca_priv_key);
  X509_set_issuer_name (ca_cert, X509_get_subject_name (ca_cert)); /* self-signed */
  X509_sign (ca_cert, ca_priv_key, EVP_sha256 ());
  char * output = get_x509_data (ca_cert);

  EVP_PKEY_free (ca_priv_key);
  X509_free (ca_cert);

  return output;
}

char * generate_ca(const char *ca_name, const char * ca_priv_key_str, int not_valid_before, int not_valid_after)
{
  EVP_PKEY *ca_priv_key = get_priv_key (ca_priv_key_str);
  return generate_ca_internal(ca_name, ca_priv_key, not_valid_before, not_valid_after);
}

static char * generate_identity_internal(const char * ca_cert_str, EVP_PKEY * ca_key_pkey, const char * name, EVP_PKEY * id_pkey, int not_valid_before, int not_valid_after, char ** subject)
{
  X509 *ca_cert = get_cert (ca_cert_str);
  EVP_PKEY *ca_cert_pkey = X509_get_pubkey (ca_cert);
  X509_REQ *csr =  X509_REQ_new ();
  X509_REQ_set_pubkey (csr, id_pkey);
  X509_REQ_sign (csr, id_pkey, EVP_sha256 ());

  char * email = malloc (MAX_EMAIL);
  snprintf(email, MAX_EMAIL, "%s@%s" , name, EMAIL_HOST);
  X509 * cert = get_x509 (not_valid_before, not_valid_after, name, email);
  ddsrt_free (email);

  EVP_PKEY *csr_pkey = X509_REQ_get_pubkey (csr);
  X509_set_pubkey (cert, csr_pkey);
  X509_set_issuer_name (cert, X509_get_subject_name (ca_cert));
  X509_sign (cert, ca_key_pkey, EVP_sha256 ());
  char * output = get_x509_data (cert);

  if (subject)
  {
    X509_NAME *subj_name = X509_get_subject_name (cert);
    char * subj_openssl = X509_NAME_oneline (subj_name, NULL, 0);
    *subject = ddsrt_strdup (subj_openssl);
    OPENSSL_free (subj_openssl);
  }

  X509_REQ_free (csr);
  EVP_PKEY_free (id_pkey);
  EVP_PKEY_free (ca_cert_pkey);
  EVP_PKEY_free (ca_key_pkey);
  EVP_PKEY_free (csr_pkey);
  X509_free (cert);
  X509_free (ca_cert);

  return output;
}

char * generate_identity(const char * ca_cert_str, const char * ca_priv_key_str, const char * name, const char * priv_key_str, int not_valid_before, int not_valid_after, char ** subject)
{
  EVP_PKEY *ca_key_pkey = get_priv_key (ca_priv_key_str);
  EVP_PKEY *id_pkey = get_priv_key (priv_key_str);
  return generate_identity_internal(ca_cert_str, ca_key_pkey, name, id_pkey, not_valid_before, not_valid_after, subject);
}

bool check_pkcs11_provider(void)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  if (OSSL_PROVIDER_available(NULL, "pkcs11"))
    return true;
  else if (OSSL_PROVIDER_try_load(NULL, "pkcs11", 1))
    return true;
#endif
  return false;
}

char * generate_pkcs11_private_key(const char *token, const char *name, uint32_t id, const char *pin)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  const char template[] = "pkcs11:token=%s;object=%s;id=%u?pin-value=%s";
  size_t len = sizeof (template) + strlen (token) + strlen (name) + 10 + strlen (pin);
  char *uri = malloc (len);
  snprintf(uri, len, template, token, name, id, pin);
  EVP_PKEY *pkey = NULL;

  /* Create keygen context using PKCS#11 URI */
  EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", "provider=pkcs11");
  CU_ASSERT_NEQ_FATAL (ctx, NULL);

  if (EVP_PKEY_keygen_init(ctx) <= 0)
  {
    ERR_print_errors_fp (stderr);
    CU_FAIL ("oops");
  }

  OSSL_PARAM params[3];
  params[0] = OSSL_PARAM_construct_utf8_string("ec_paramgen_curve", "P-256", 0);
  params[1] = OSSL_PARAM_construct_utf8_string("pkcs11_uri", uri, 0);
  params[2] = OSSL_PARAM_construct_end();

  if (EVP_PKEY_CTX_set_params(ctx, params) <= 0)
  {
    ERR_print_errors_fp (stderr);
    CU_FAIL ("oops");
  }

  /* Generate the key */
  if (EVP_PKEY_generate(ctx, &pkey) <= 0)
  {
    ERR_print_errors_fp (stderr);
    CU_FAIL ("oops");
  }
  EVP_PKEY_free(pkey);
  printf("PRIV_KEY_URI=%s\n", uri);
  return uri;
#else
  DDSRT_UNUSED_ARG(token);
  DDSRT_UNUSED_ARG(name);
  DDSRT_UNUSED_ARG(id);
  DDSRT_UNUSED_ARG(pin);
  return NULL;
#endif
}

EVP_PKEY * get_priv_key_pkcs11(const char *uri)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  OSSL_STORE_CTX *store_ctx = NULL;
  OSSL_STORE_INFO *store_info = NULL;
  EVP_PKEY *pkey = NULL;

  if (!(store_ctx = OSSL_STORE_open(uri, NULL, NULL, NULL, NULL)))
  {
    ERR_print_errors_fp (stderr);
    CU_FAIL ("oops");
  }

  while (!pkey)
  {
    if ((store_info = OSSL_STORE_load(store_ctx)))
    {
      if (OSSL_STORE_INFO_get_type(store_info) == OSSL_STORE_INFO_PKEY)
        pkey = OSSL_STORE_INFO_get1_PKEY(store_info);
      OSSL_STORE_INFO_free(store_info);
    }
    else if (OSSL_STORE_error(store_ctx))
    {
      ERR_print_errors_fp (stderr);
      CU_FAIL ("oops");
    }
    else
      CU_FAIL ("oops");
  }
  OSSL_STORE_close(store_ctx);

  return pkey;
#else
  DDSRT_UNUSED_ARG(uri);
  return NULL;
#endif
}

X509 * get_certificate_pkcs11(const char *uri)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  OSSL_STORE_CTX *store_ctx = NULL;
  OSSL_STORE_INFO *store_info = NULL;
  X509 *cert = NULL;

  if (!(store_ctx = OSSL_STORE_open(uri, NULL, NULL, NULL, NULL)))
  {
    ERR_print_errors_fp (stderr);
    CU_FAIL ("oops");
  }

  while (!cert)
  {
    if ((store_info = OSSL_STORE_load(store_ctx)))
    {
      if (OSSL_STORE_INFO_get_type(store_info) == OSSL_STORE_INFO_CERT)
        cert = OSSL_STORE_INFO_get1_CERT(store_info);
      OSSL_STORE_INFO_free(store_info);
    }
    else if (OSSL_STORE_error(store_ctx))
    {
      ERR_print_errors_fp (stderr);
      CU_FAIL ("oops");
    }
    else
      CU_FAIL ("oops");
  }
  OSSL_STORE_close(store_ctx);

  return cert;
#else
  DDSRT_UNUSED_ARG(uri);
  return NULL;
#endif
}

char * generate_ca_pkcs11(const char *ca_name, const char * ca_priv_key_uri, int not_valid_before, int not_valid_after)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  EVP_PKEY *ca_priv_key = get_priv_key_pkcs11(ca_priv_key_uri);
  return generate_ca_internal(ca_name, ca_priv_key, not_valid_before, not_valid_after);
#else
  DDSRT_UNUSED_ARG(ca_name);
  DDSRT_UNUSED_ARG(ca_priv_key_uri);
  DDSRT_UNUSED_ARG(not_valid_before);
  DDSRT_UNUSED_ARG(not_valid_after);
  return NULL;
#endif
}

char * generate_identity_pkcs11(const char * ca_cert_str, const char * ca_priv_key_uri, const char * name, const char * priv_key_uri, int not_valid_before, int not_valid_after, char ** subject)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  EVP_PKEY *ca_key_pkey = get_priv_key_pkcs11 (ca_priv_key_uri);
  EVP_PKEY *id_pkey = get_priv_key_pkcs11 (priv_key_uri);
  return generate_identity_internal(ca_cert_str, ca_key_pkey, name, id_pkey, not_valid_before, not_valid_after, subject);
#else
  DDSRT_UNUSED_ARG(ca_cert_str);
  DDSRT_UNUSED_ARG(ca_priv_key_uri);
  DDSRT_UNUSED_ARG(name);
  DDSRT_UNUSED_ARG(priv_key_uri);
  DDSRT_UNUSED_ARG(not_valid_before);
  DDSRT_UNUSED_ARG(not_valid_after);
  DDSRT_UNUSED_ARG(subject);
  return NULL;
#endif
}


