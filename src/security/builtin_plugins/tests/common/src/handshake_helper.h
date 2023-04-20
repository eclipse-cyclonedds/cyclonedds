// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS_SECURITY_BUITIN_TEST_HANDSHAKE_HELPER_H
#define DDS_SECURITY_BUITIN_TEST_HANDSHAKE_HELPER_H

#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_serialize.h"
#include "dds/security/openssl_support.h"

const BIGNUM *
dh_get_public_key(
     DH *dhkey);

int
dh_set_public_key(
    DH *dhkey,
    BIGNUM *pubkey);

DDS_Security_ValidationResult_t
create_signature_for_test(
    EVP_PKEY *pkey,
    const DDS_Security_BinaryProperty_t **binary_properties,
    const uint32_t binary_properties_length,
    unsigned char **signature,
    size_t *signatureLen,
    DDS_Security_SecurityException *ex);

DDS_Security_ValidationResult_t
create_asymmetrical_signature_for_test(
     EVP_PKEY *pkey,
     void *data,
     size_t dataLen,
     unsigned char **signature,
     size_t *signatureLen,
     DDS_Security_SecurityException *ex);

char *
get_openssl_error_message_for_test(
        void);

DDS_Security_ValidationResult_t
validate_asymmetrical_signature_for_test(
    EVP_PKEY *pkey,
    void *data,
    size_t dataLen,
    unsigned char *signature,
    size_t signatureLen,
    DDS_Security_SecurityException *ex);

DDS_Security_ValidationResult_t
get_public_key(
     EVP_PKEY *pkey,
     unsigned char **buffer,
     size_t *length,
    DDS_Security_SecurityException *ex);

/* for DEBUG purposes */
void print_binary_test( char* name, unsigned char *value, uint32_t size);

DDS_Security_BinaryProperty_t *
print_binary_properties_test(
    DDS_Security_DataHolder *token);

int
check_shared_secret(
    dds_security_authentication *auth,
    int use_ecdh,
    const DDS_Security_BinaryProperty_t *dh_remote,
    EVP_PKEY *dh_local_private,
    DDS_Security_HandshakeHandle handshake_handle);

#endif
