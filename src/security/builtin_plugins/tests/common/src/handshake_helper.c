// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "dds/ddsrt/string.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/misc.h"
#include "dds/security/core/dds_security_shared_secret.h"
#include "dds/security/openssl_support.h"
#include "dds/security/core/dds_security_serialize.h"
#include "dds/security/core/dds_security_utils.h"
#include "CUnit/CUnit.h"
#include "CUnit/Test.h"
#include "handshake_helper.h"

const BIGNUM *
dh_get_public_key(
     DH *dhkey)
{
#ifdef AUTH_INCLUDE_DH_ACCESSORS
    const BIGNUM *pubkey, *privkey;
    DH_get0_key(dhkey, &pubkey, &privkey);
    return pubkey;
#else
    return dhkey->pub_key;
#endif
}

int
dh_set_public_key(
     DH *dhkey,
     BIGNUM *pubkey)
{
#ifdef AUTH_INCLUDE_DH_ACCESSORS
    return DH_set0_key(dhkey, pubkey, NULL);
#else
    dhkey->pub_key = pubkey;
#endif
    return 1;
}


/* for DEBUG purposes */
void print_binary_test( char* name, unsigned char *value, uint32_t size){
    uint32_t i;
    printf("%s: ",name );
    for( i=0; i<  size; i++)
    {
        printf("%x",value[i]);
    }
    printf("\n");
}

DDS_Security_BinaryProperty_t *
print_binary_properties_test(
    DDS_Security_DataHolder *token)
{
    DDS_Security_BinaryProperty_t *result = NULL;
    uint32_t i;
    for (i = 0; i < token->binary_properties._length && !result; i++) {
        print_binary_test( token->binary_properties._buffer[i].name, token->binary_properties._buffer[i].value._buffer, token->binary_properties._buffer[i].value._length);
    }

    return result;
}

DDS_Security_ValidationResult_t
create_signature_for_test(
    EVP_PKEY *pkey,
    const DDS_Security_BinaryProperty_t **binary_properties,
    const uint32_t binary_properties_length,
    unsigned char **signature,
    size_t *signatureLen,
    DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result;
    DDS_Security_Serializer serializer;
    unsigned char *buffer;
    size_t size;

    serializer = DDS_Security_Serializer_new(4096, 4096);

    DDS_Security_Serialize_BinaryPropertyArray(serializer,binary_properties, binary_properties_length);
    DDS_Security_Serializer_buffer(serializer, &buffer, &size);

    result = create_asymmetrical_signature_for_test(pkey, buffer, size, signature, signatureLen, ex);

    ddsrt_free(buffer);
    DDS_Security_Serializer_free(serializer);

    return result;
}

#if( AC_TESTS_IMPLEMENTED )
static DDS_Security_ValidationResult_t
screate_asymmetrical_signature_for_test(
     EVP_PKEY *pkey,
     void *data,
     size_t dataLen,
     unsigned char **signature,
     size_t *signatureLen,
     DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result = DDS_SECURITY_VALIDATION_OK;
    EVP_MD_CTX *mdctx = NULL;
    EVP_PKEY_CTX *kctx = NULL;

    if (!(mdctx = EVP_MD_CTX_create())) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, result, "Failed to create signing context: %s", msg);
        ddsrt_free(msg);
        goto err_create_ctx;
    }

    if (EVP_DigestSignInit(mdctx, &kctx, EVP_sha256(), NULL, pkey) != 1) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, result, "Failed to initialize signing context: %s", msg);
        ddsrt_free(msg);
        goto err_sign;
    }

    if (EVP_PKEY_CTX_set_rsa_padding(kctx, RSA_PKCS1_PSS_PADDING) < 1) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, result, "Failed to initialize signing context: %s", msg);
        ddsrt_free(msg);
        goto err_sign;
    }

    if (EVP_DigestSignUpdate(mdctx, data, dataLen) != 1) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, result, "Failed to update signing context: %s", msg);
        ddsrt_free(msg);
        goto err_sign;
    }

    if (EVP_DigestSignFinal(mdctx, NULL, signatureLen) != 1) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, result, "Failed to finalize signing context: %s", msg);
        ddsrt_free(msg);
        goto err_sign;
    }

    *signature = ddsrt_malloc(*signatureLen);
    if (EVP_DigestSignFinal(mdctx, *signature, signatureLen) != 1) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, result, "Failed to finalize signing context: %s", msg);
        ddsrt_free(msg);
        ddsrt_free(signature);
    }

err_sign:
    EVP_MD_CTX_destroy(mdctx);
err_create_ctx:
    return result;
}
#endif


char *
get_openssl_error_message_for_test(
        void)
{
    BIO *bio = BIO_new(BIO_s_mem());
    char *msg;
    char *buf = NULL;
    size_t len;

    if (bio) {
        ERR_print_errors(bio);
        len = (size_t)BIO_get_mem_data (bio, &buf);
        msg = ddsrt_malloc(len + 1);
        memset(msg, 0, len+1);
        memcpy(msg, buf, len);
        BIO_free(bio);
    } else {
        msg = ddsrt_strdup("BIO_new failed");
    }

    return msg;
}

DDS_Security_ValidationResult_t
validate_asymmetrical_signature_for_test(
    EVP_PKEY *pkey,
    void *data,
    size_t dataLen,
    unsigned char *signature,
    size_t signatureLen,
    DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result = DDS_SECURITY_VALIDATION_OK;
    EVP_MD_CTX *mdctx = NULL;

    if (!(mdctx = EVP_MD_CTX_create())) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "Failed to create verify context: %s", msg);
        ddsrt_free(msg);
        goto err_create_ctx;
    }

    if (EVP_DigestVerifyInit(mdctx, NULL, EVP_sha256(), NULL, pkey) != 1) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "Failed to initialize verify context: %s", msg);
        ddsrt_free(msg);
        goto err_verify;
    }

    if (EVP_DigestVerifyUpdate(mdctx, data, dataLen) != 1) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "Failed to update verify context: %s", msg);
        ddsrt_free(msg);
        goto err_verify;
    }

    if (EVP_DigestVerifyFinal(mdctx, signature, signatureLen) != 1) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "Failed to finalize verify context: %s", msg);
        ddsrt_free(msg);
        goto err_verify;
    }

err_verify:
    EVP_MD_CTX_destroy(mdctx);
err_create_ctx:
    return result;
}

DDS_Security_ValidationResult_t
get_public_key(
    EVP_PKEY *pkey,
    unsigned char **buffer,
    size_t *length,
    DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result = DDS_SECURITY_VALIDATION_OK;
    BIO *bio = NULL;
    char *ptr = NULL;
    size_t sz;

    assert(pkey);
    assert(buffer);

    *length = 0;

    bio = BIO_new(BIO_s_mem());

    if ( bio == NULL) {
      result = DDS_SECURITY_VALIDATION_FAILED;
      DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE,  (int)result, "Failed to get public key: BIO_new_mem_buf failed");
    } else if (!PEM_write_bio_PUBKEY(bio, pkey)) {
      char *msg = get_openssl_error_message_for_test();
      result = DDS_SECURITY_VALIDATION_FAILED;
      DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE,  (int)result, "Failed to get public key: PEM_write_bio_PUBKEY failed: %s", msg);
      ddsrt_free(msg);
    } else {
      sz = (size_t)BIO_get_mem_data(bio, &ptr);
      *buffer = ddsrt_malloc(sz +1);
      memcpy(*buffer, ptr, sz);
      *length = sz;
    }

    if (bio) BIO_free(bio);

    return result;
}

static EVP_PKEY *
modp_data_to_pubkey(
    const unsigned char *data,
    uint32_t size)
{
    EVP_PKEY *pkey= NULL;
    DH *dhkey = NULL;
    ASN1_INTEGER *asni;
    BIGNUM *bn = NULL;

    if (!(asni = d2i_ASN1_INTEGER(NULL, &data, (long)size))) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to decode DH public key: %s", msg);
        ddsrt_free(msg);
        goto fail_asni;
    }

    if (!(bn = ASN1_INTEGER_to_BN(asni, NULL))) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to convert to BIGNU<: %s", msg);
        ddsrt_free(msg);
        goto fail_bn;
    }

    if (!(dhkey = DH_get_2048_256())) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to allocate dhkey: %s", msg);
        ddsrt_free(msg);
        goto fail_dhkey;
    }

    dh_set_public_key(dhkey,bn);

    if (!(pkey = EVP_PKEY_new())) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to allocate pkey: %s", msg);
        ddsrt_free(msg);
        goto fail_pkey;
    }

    if (!EVP_PKEY_set1_DH(pkey, dhkey)) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to set public key: %s", msg);
        ddsrt_free(msg);
        EVP_PKEY_free(pkey);
        pkey = NULL;
    }

    ASN1_INTEGER_free(asni);
    DH_free(dhkey);

    return pkey;

fail_pkey:
    DH_free(dhkey);
fail_dhkey:
    BN_free(bn);
fail_bn:
    ASN1_INTEGER_free(asni);
fail_asni:
    return NULL;
}

static EVP_PKEY *
ecdh_data_to_pubkey(
    const unsigned char *data,
    uint32_t size)
{
    EVP_PKEY *pkey = NULL;
    EC_KEY *eckey = NULL;
    EC_GROUP *group = NULL;
    EC_POINT *point = NULL;

    if (!(group = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1))) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to allocate EC group: %s", msg);
        ddsrt_free(msg);
    } else if (!(point = EC_POINT_new(group))) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to allocate EC point: %s", msg);
        ddsrt_free(msg);
    } else if (EC_POINT_oct2point(group, point, data, size, NULL) != 1) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to deserialize EC public key to EC point: %s", msg);
        ddsrt_free(msg);
    } else if (!(eckey = EC_KEY_new())) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to allocate EC KEY: %s", msg);
        ddsrt_free(msg);
    } else if (EC_KEY_set_group(eckey, group) != 1) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to convert octet sequence to ASN1 integer: %s", msg);
        ddsrt_free(msg);
    } else if (EC_KEY_set_public_key(eckey, point) != 1) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to set EC public key: %s", msg);
        ddsrt_free(msg);
    } else if (!(pkey = EVP_PKEY_new())) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to allocate EVP key: %s", msg);
        ddsrt_free(msg);
    } else if (EVP_PKEY_set1_EC_KEY(pkey, eckey) != 1) {
        char *msg = get_openssl_error_message_for_test();
        printf("Failed to set EVP key to EC public key: %s", msg);
        ddsrt_free(msg);
        EVP_PKEY_free(pkey);
        pkey = NULL;
    }

    if (eckey) EC_KEY_free(eckey);
    if (point) EC_POINT_free(point);
    if (group) EC_GROUP_free(group);

    return pkey;
}

int
check_shared_secret(
    dds_security_authentication *auth,
    int use_ecdh,
    const DDS_Security_BinaryProperty_t *dh_remote,
    EVP_PKEY *dh_local_private,

    DDS_Security_HandshakeHandle handshake_handle)

{
/* calculate shared secret with the other side */
    EVP_PKEY_CTX *ctx = NULL;
    size_t skeylen;
    EVP_PKEY *dh_remote_public = NULL;
    DDS_Security_SharedSecretHandle shared_secret_local_handle;
    DDS_Security_SecurityException exception;
    DDS_Security_octet *shared_secret_local;
    DDS_Security_octet shared_secret_remote[SHA256_DIGEST_LENGTH];
    DDS_Security_octet *secret;
    int result;

    if (use_ecdh) {
        dh_remote_public = ecdh_data_to_pubkey(dh_remote->value._buffer, dh_remote->value._length);
    } else {
        dh_remote_public = modp_data_to_pubkey(dh_remote->value._buffer, dh_remote->value._length);
    }

    if (!dh_remote_public) {
        CU_FAIL("Coud not decode DH public key");
    }

    ctx = EVP_PKEY_CTX_new(dh_local_private, NULL /* no engine */);
    if (!ctx)
    {
           /* Error occurred */
        CU_FAIL("Coud not allocate CTX");
    }

    if (EVP_PKEY_derive_init(ctx) <= 0)
    {
           /* Error */
        CU_FAIL("Coud not init");
    }
    result = EVP_PKEY_derive_set_peer(ctx, dh_remote_public) ;
    if (result<= 0)
    {
           /* Error */
        char *msg = get_openssl_error_message_for_test();
        printf("DH remote public: %s\n",dh_remote->value._buffer);
        printf("SSL Error: %s\n", msg);
        ddsrt_free(msg);
        CU_FAIL("Could not set peer");
    }

    /* Determine buffer length */
    result = EVP_PKEY_derive(ctx, NULL, &skeylen);
    if (result <= 0)
    {
           /* Error */
        CU_FAIL("Could not set derive");
    }

    secret = ddsrt_malloc(skeylen+1);

    if (EVP_PKEY_derive(ctx, secret, &skeylen) <= 0)
    {
           /* Error */
        CU_FAIL("Could not set derive");
    }

    SHA256(secret, skeylen, shared_secret_remote);

    ddsrt_free(secret);

    /* get the secret handle */
    shared_secret_local_handle = auth->get_shared_secret( auth, handshake_handle, &exception);

    /* convert handle to object */
    shared_secret_local = ((DDS_Security_SharedSecretHandleImpl *)(shared_secret_local_handle))->shared_secret;
    /*compare with remote. They should be same */

    if (ctx) {
        EVP_PKEY_CTX_free(ctx);
    }

    if (dh_remote_public) {
        EVP_PKEY_free(dh_remote_public);
    }

    return memcmp(shared_secret_local, shared_secret_remote, SHA256_DIGEST_LENGTH);
}



DDS_Security_ValidationResult_t
create_asymmetrical_signature_for_test(
        EVP_PKEY *pkey,
        void *data,
        size_t dataLen,
        unsigned char **signature,
        size_t *signatureLen,
        DDS_Security_SecurityException *ex)
{
    DDS_Security_ValidationResult_t result = DDS_SECURITY_VALIDATION_OK;
    EVP_MD_CTX *mdctx = NULL;
    EVP_PKEY_CTX *kctx = NULL;

    if (!(mdctx = EVP_MD_CTX_create())) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "Failed to create signing context: %s", msg);
        ddsrt_free(msg);
        goto err_create_ctx;
    }

    if (EVP_DigestSignInit(mdctx, &kctx, EVP_sha256(), NULL, pkey) != 1) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "Failed to initialize signing context: %s", msg);
        ddsrt_free(msg);
        goto err_sign;
    }

    if (EVP_PKEY_CTX_set_rsa_padding(kctx, RSA_PKCS1_PSS_PADDING) < 1) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "Failed to initialize signing context: %s", msg);
        ddsrt_free(msg);
        goto err_sign;
    }

    if (EVP_DigestSignUpdate(mdctx, data, dataLen) != 1) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "Failed to update signing context: %s", msg);
        ddsrt_free(msg);
        goto err_sign;
    }

    if (EVP_DigestSignFinal(mdctx, NULL, signatureLen) != 1) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "Failed to finalize signing context: %s", msg);
        ddsrt_free(msg);
        goto err_sign;
    }

    *signature = ddsrt_malloc(*signatureLen);
    if (EVP_DigestSignFinal(mdctx, *signature, signatureLen) != 1) {
        char *msg = get_openssl_error_message_for_test();
        result = DDS_SECURITY_VALIDATION_FAILED;
        DDS_Security_Exception_set(ex, "Authentication", DDS_SECURITY_ERR_UNDEFINED_CODE, (int)result, "Failed to finalize signing context: %s", msg);
        ddsrt_free(msg);
        ddsrt_free(*signature);
    }

    err_sign:
    EVP_MD_CTX_destroy(mdctx);
    err_create_ctx:
    return result;
}


