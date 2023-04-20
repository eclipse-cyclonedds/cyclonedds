// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef AUTH_UTILS_H
#define AUTH_UTILS_H

#ifdef _WIN32
/* supposedly WinSock2 must be included before openssl 1.0.2 headers otherwise winsock will be used */
#include <WinSock2.h>
#endif
#include <openssl/x509.h>
#include <openssl/evp.h>

#include "dds/security/dds_security_api.h"
#include "dds/ddsrt/time.h"

#define DDS_AUTH_PLUGIN_CONTEXT "Authentication"

typedef enum {
    AUTH_ALGO_KIND_UNKNOWN,
    AUTH_ALGO_KIND_RSA_2048,
    AUTH_ALGO_KIND_EC_PRIME256V1
} AuthenticationAlgoKind_t;

typedef struct AuthenticationChallenge {
    unsigned char value[DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE];
} AuthenticationChallenge;

typedef struct {
    uint32_t length;
    X509 **buffer;
} X509Seq;

/* Return the subject name of contained in a X509 certificate
 * Note that the returned string should be freed.
 */
char * get_certificate_subject_name(X509 *cert, DDS_Security_SecurityException *ex) ddsrt_nonnull_all;

/* Return the expiry date of contained in a X509 certificate */
dds_time_t get_certificate_expiry(const X509 *cert);

/* Return the subject name of a X509 certificate DER
 * encoded. The DER encoded subject name is returned in
 * the provided buffer. The length of the allocated
 * buffer is returned
 *
 * return length of allocated buffer or -1 on error
 */
DDS_Security_ValidationResult_t get_subject_name_DER_encoded(const X509 *cert, unsigned char **buffer, size_t *size, DDS_Security_SecurityException *ex);

/* Load a X509 certificate for the provided data (PEM format) */
DDS_Security_ValidationResult_t load_X509_certificate_from_data(const char *data, int len, X509 **x509Cert, DDS_Security_SecurityException *ex);

/* Load a X509 certificate for the provided data (certificate uri) */
DDS_Security_ValidationResult_t load_X509_certificate(const char *data, X509 **x509Cert, DDS_Security_SecurityException *ex);

/* Load a X509 certificate for the provided file */
DDS_Security_ValidationResult_t load_X509_certificate_from_file(const char *filename, X509 **x509Cert, DDS_Security_SecurityException *ex);

/* Load a Private Key for the provided data (private key uri) */
DDS_Security_ValidationResult_t load_X509_private_key(const char *data, const char *password, EVP_PKEY **privateKey, DDS_Security_SecurityException *ex);

/* Load a Certificate Revocation List (CRL) for the provided data (CRL uri) */
DDS_Security_ValidationResult_t load_X509_CRL(const char *data, X509_CRL **crl, DDS_Security_SecurityException *ex);

/* Validate an identity certificate against the identityCA
 * The provided identity certificate is checked if it is
 * signed by the identity corresponding to the identityCA.
 *
 * Note: Currently only a self signed CA is supported
 *       The function does not yet check OCSP
 *       for expiry of identity certificate.
 */
DDS_Security_ValidationResult_t verify_certificate(X509 *identityCert, X509 *identityCa, X509_CRL *crl, DDS_Security_SecurityException *ex);

DDS_Security_ValidationResult_t check_certificate_expiry(const X509 *cert, DDS_Security_SecurityException *ex);
AuthenticationAlgoKind_t get_authentication_algo_kind(X509 *cert);
AuthenticationChallenge *generate_challenge(DDS_Security_SecurityException *ex);
DDS_Security_ValidationResult_t get_certificate_contents(X509 *cert, unsigned char **data, uint32_t *size, DDS_Security_SecurityException *ex);
DDS_Security_ValidationResult_t generate_dh_keys(EVP_PKEY **dhkey, AuthenticationAlgoKind_t authKind, DDS_Security_SecurityException *ex);
DDS_Security_ValidationResult_t dh_public_key_to_oct(EVP_PKEY *pkey, AuthenticationAlgoKind_t algo, unsigned char **buffer, uint32_t *length, DDS_Security_SecurityException *ex);
DDS_Security_ValidationResult_t dh_oct_to_public_key(EVP_PKEY **data, AuthenticationAlgoKind_t algo, const unsigned char *str, uint32_t size, DDS_Security_SecurityException *ex);
void free_ca_list_contents(X509Seq *ca_list);
DDS_Security_ValidationResult_t get_trusted_ca_list(const char* trusted_ca_dir, X509Seq *ca_list, DDS_Security_SecurityException *ex);
char * string_from_data(const unsigned char *data, uint32_t size);
DDS_Security_ValidationResult_t create_validate_asymmetrical_signature(bool create, EVP_PKEY *pkey, const unsigned char *data, const size_t dataLen,
    unsigned char **signature, size_t *signatureLen, DDS_Security_SecurityException *ex);

#endif /* AUTH_UTILS_H */
