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
  CU_ASSERT_FATAL (cert != NULL);
  ASN1_INTEGER_set (X509_get_serialNumber (cert), 1);
  X509_gmtime_adj (X509_get_notBefore (cert), not_valid_before);
  X509_gmtime_adj (X509_get_notAfter (cert), not_valid_after);

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
    CU_ASSERT_FATAL (false);
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
  CU_ASSERT_FATAL (priv_key != NULL);
  BIO_free (pkey_bio);
  return priv_key;
}

static X509 * get_cert(const char * cert_str)
{
  BIO *cert_bio = BIO_new_mem_buf (cert_str, -1);
  X509 *cert = PEM_read_bio_X509 (cert_bio, NULL, NULL, 0);
  CU_ASSERT_FATAL (cert != NULL);
  BIO_free (cert_bio);
  return cert;
}

char * generate_ca(const char *ca_name, const char * ca_priv_key_str, int not_valid_before, int not_valid_after)
{
  EVP_PKEY *ca_priv_key = get_priv_key (ca_priv_key_str);

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

char * generate_identity(const char * ca_cert_str, const char * ca_priv_key_str, const char * name, const char * priv_key_str, int not_valid_before, int not_valid_after, char ** subject)
{
  X509 *ca_cert = get_cert (ca_cert_str);
  EVP_PKEY *ca_key_pkey = get_priv_key (ca_priv_key_str);
  EVP_PKEY *id_pkey = get_priv_key (priv_key_str);
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
