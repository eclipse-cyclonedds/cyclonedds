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

#ifndef AUTH_UTILS_H
#define AUTH_UTILS_H

#include "dds/security/dds_security_api.h"
#include "dds/ddsrt/time.h"

#define DDS_AUTH_PLUGIN_CONTEXT "Authentication"

typedef enum {
    AUTH_ALGO_KIND_UNKNOWN,
    AUTH_ALGO_KIND_RSA_2048,
    AUTH_ALGO_KIND_EC_PRIME256V1
} AuthenticationAlgoKind_t;

typedef enum {
    AUTH_CONF_ITEM_PREFIX_UNKNOWN,
    AUTH_CONF_ITEM_PREFIX_FILE,
    AUTH_CONF_ITEM_PREFIX_DATA,
    AUTH_CONF_ITEM_PREFIX_PKCS11
} AuthConfItemPrefix_t;


typedef struct AuthenticationChallenge {
    unsigned char value[DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE];
} AuthenticationChallenge;

typedef struct {
    uint32_t length;
    X509 **buffer;
} X509Seq;

typedef unsigned char HashValue_t[SHA256_DIGEST_LENGTH];
/*typedef struct HashValue {
    unsigned char value[SHA256_DIGEST_LENGTH];
} HashValue_t;
*/

/* Return a string that contains an openssl error description
 * When a openssl function returns an error this function can be
 * used to retrieve a descriptive error string.
 * Note that the returned string should be freed.
 */
char *
get_openssl_error_message(
        void);

/* Return the subject name of contained in a X509 certificate
 * Note that the returned string should be freed.
 */
char*
get_certificate_subject_name(
        X509 *cert,
        DDS_Security_SecurityException *ex);

/* Return the expiry date of contained in a X509 certificate
 *
 */
dds_time_t
get_certificate_expiry(
    const X509 *cert);

/* Return the subject name of a X509 certificate DER
 * encoded. The DER encoded subject name is returned in
 * the provided buffer. The length of the allocated
 * buffer is returned
 *
 * return length of allocated buffer or -1 on error
 */
DDS_Security_ValidationResult_t
get_subject_name_DER_encoded(
        const X509 *cert,
        unsigned char **buffer,
        size_t *size,
        DDS_Security_SecurityException *ex);


/* Load a X509 certificate for the provided data.
 *
 * data     : certificate in PEM format
 * x509Cert : the openssl X509 return value
 */
DDS_Security_ValidationResult_t
load_X509_certificate_from_data(
        const char *data,
        int len,
        X509 **x509Cert,
        DDS_Security_SecurityException *ex);


/* Load a X509 certificate for the provided data.
 *
 * data     : URI of the certificate. URI format is defined in DDS Security spec 9.3.1

 * x509Cert : the openssl X509 return value
 */
DDS_Security_ValidationResult_t
load_X509_certificate(
        const char *data,
        X509 **x509Cert,
        DDS_Security_SecurityException *ex);


/* Load a X509 certificate for the provided file.
 *
 * filename : path of the file that contains PEM formatted certificate
 * x509Cert : the openssl X509 return value
 */
DDS_Security_ValidationResult_t
load_X509_certificate_from_file(
        const char *filename,
        X509 **x509Cert,
        DDS_Security_SecurityException *ex);

/* Load a Private Key for the provided data.
 *
 * data       : URI of the private key. URI format is defined in DDS Security spec 9.3.1
 * privateKey : the openssl EVP_PKEY return value
 */
DDS_Security_ValidationResult_t
load_X509_private_key(
        const char *data,
        const char *password,
        EVP_PKEY **privateKey,
        DDS_Security_SecurityException *ex);


/* Validate an identity certificate against the identityCA
 * The provided identity certificate is checked if it is
 * signed by the identity corresponding to the identityCA.
 *
 * Note: Currently only a self signed CA is supported
 *       The function does not yet check a CLR or ocsp
 *       for expiry of identity certificate.
 */
DDS_Security_ValidationResult_t
verify_certificate(
        X509 *identityCert,
        X509 *identityCa,
        DDS_Security_SecurityException *ex);

DDS_Security_ValidationResult_t
check_certificate_expiry(
    const X509 *cert,
    DDS_Security_SecurityException *ex);

AuthenticationAlgoKind_t
get_auhentication_algo_kind(
        X509 *cert);

AuthenticationChallenge *
generate_challenge(
        DDS_Security_SecurityException *ex);

DDS_Security_ValidationResult_t
get_certificate_contents(
        X509 *cert,
        unsigned char **data,
        uint32_t *size,
        DDS_Security_SecurityException *ex);

DDS_Security_ValidationResult_t
generate_dh_keys(
    EVP_PKEY **dhkey,
    AuthenticationAlgoKind_t authKind,
    DDS_Security_SecurityException *ex);

DDS_Security_ValidationResult_t
dh_public_key_to_oct(
    EVP_PKEY *pkey,
    AuthenticationAlgoKind_t algo,
    unsigned char **buffer,
    uint32_t *length,
    DDS_Security_SecurityException *ex);

DDS_Security_ValidationResult_t
dh_oct_to_public_key(
    EVP_PKEY **data,
    AuthenticationAlgoKind_t algo,
    const unsigned char *str,
    uint32_t size,
    DDS_Security_SecurityException *ex);


AuthConfItemPrefix_t
get_conf_item_type(
        const char *str,
        char **data);

/*
 * Frees the contents of theCA list.
 */
void
free_ca_list_contents(
     X509Seq *ca_list);

DDS_Security_ValidationResult_t
get_trusted_ca_list (
    const char* trusted_ca_dir,
    X509Seq *ca_list,
    DDS_Security_SecurityException *ex);

char *
string_from_data(
    const unsigned char *data,
    uint32_t size);


DDS_Security_ValidationResult_t
create_asymmetrical_signature(
        EVP_PKEY *pkey,
        const unsigned char *data,
        const size_t dataLen,
        unsigned char **signature,
        size_t *signatureLen,
        DDS_Security_SecurityException *ex);

DDS_Security_ValidationResult_t
validate_asymmetrical_signature(
        EVP_PKEY *pkey,
        const unsigned char *data,
        const size_t dataLen,
        const unsigned char *signature,
        const size_t signatureLen,
        DDS_Security_SecurityException *ex);

#endif /* AUTH_UTILS_H */
