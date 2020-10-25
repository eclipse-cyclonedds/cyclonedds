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
#ifndef CRYPTO_CIPHER_H
#define CRYPTO_CIPHER_H

#include "dds/ddsrt/types.h"
#include "crypto_objects.h"

/**
 * @brief Encodes the provide data using the provided key
 *
 * This function encodes the provide data using the provided key, key_size
 * and initialization_vector. It also computes the common_mac from the provided data.
 * The data parameter contains the data that has to encoded. The aad parameter contains
 * the data that is not encoded but is used in the computation of the common_mac
 * On return the encrypted parameter contains the encoded data and the tag parameter
 * the common mac.
 * This function will be used either to encode the provided data in that case the
 * data parameter should be set to the data to be encoded and the aad parameter should
 * be NULL. Also the encrypted parameter should be set and contain a buffer large enough
 * to contain the encoded data.
 * This function is also used to only computer the common_mac. In that case the
 * data parameter should be NULL and the aad parameter should point to the data on
 * which the common_mac has to be computed. The encryped parameter is not relevant
 * in this case.
 *
 * @param[in]     session_key   The session key used to encode the provided data
 * @param[in]     key_size      The size of the session key (128 or 256 bit)
 * @param[in]     iv            The init vector used by the encoding
 * @param[in]     data          The data to be encoded
 * @param[in]     data_len      The size of the data to be encoded
 * @param[in]     aad           The additional data not be encoded but only used in the computation of the mac
 * @param[in]     aad_len       The size of the additional data
 * @param[in,out] encrypted     The buffer to hold on return the encoded data
 * @param[in,out] encrypted_len The size of the encrypted data buffer
 * @param[in,out] tag           Contains on return the mac value calculated over the provided data
 * @param[in,out] ex            Security exception (optional)
 */
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
    DDS_Security_SecurityException *ex);

/**
 * @brief Decodes the provided data using the session key and key_size
 *
 * This function decodes the provided data using the session key and key_size provided
 * by by the session parameter. The iv parameter contains the initialization_vector used
 * by the decode operation which is the concatination of received session_id and init_vector_suffix.
 * The function checks if the common_mac (tag parameter) is corresponds with the provided data.
 * This function will be used either to decode the provided data in that case the
 * encrypted parameter should be set to the data to be decoded and the aad parameter should
 * be NULL. Also the data parameter should be set and contain a buffer large enough
 * to contain the decoded data.
 * This function is also used to only verify the common_mac. In that case the
 * data and the encrypted parameter should be NULL and the aad parameter should point to
 * the data for which the common_mac has to be verified.
 *
 * @param[in]     session       Contains the session key and key size used of the decoding
 * @param[in]     iv            The init vector used by the decoding
 * @param[in]     encrypted     The encoded data
 * @param[in]     encrypted_len The size of the encoded data
 * @param[in]     aad           The not encoded data used in the verification of the provided mac
 * @param[in]     aad_len       The size of the additional data
 * @param[in,out] data          The buffer to hold on return the decoded data
 * @param[in,out] data_len      The size of the decoded data buffer
 * @param[in,out] tag           The mac value which has to be verified
 * @param[in,out] ex            Security exception (optional)
 */
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
    DDS_Security_SecurityException *ex);

#endif /* CRYPTO_CIPHER_H */
