// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef CRYPTO_UTILS_H
#define CRYPTO_UTILS_H

#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/endian.h"
#include "dds/ddsrt/types.h"
#include "dds/security/export.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_types.h"
#include "crypto_defs.h"

#define CRYPTO_TRANSFORM_KIND(k) ddsrt_fromBE4u((*(uint32_t *)&((k)[0])))
#define CRYPTO_TRANSFORM_ID(k) ddsrt_fromBE4u((*(uint32_t *)&((k)[0])))
#define CRYPTO_KEY_SIZE_BYTES(kind) (crypto_get_key_size(kind) >> 3)

#define ALIGN4(x) (((x) + 3) & (uint32_t)(-4))

struct init_vector_suffix
{
  uint32_t high;
  uint32_t low;
};

/**
 * Return a string that contains an openssl error description
 * When a openssl function returns an error this function can be
 * used to retrieve a descriptive error string.
 * Note that the returned string should be freed.
 */
char *crypto_openssl_error_message(void);

/**
 * @param[in,out] session_key           Session key
 * @param[in]     session_id            Session Id
 * @param[in]     master_salt           Master salt
 * @param[in]     master_key            Master key
 * @param[in]     transformation_kind   Transformation kind
 * @param[in,out] ex                    Security exception
 */
SECURITY_EXPORT bool crypto_calculate_session_key(
    crypto_session_key_t *session_key,
    uint32_t session_id,
    const unsigned char *master_salt,
    const unsigned char *master_key,
    DDS_Security_CryptoTransformKind_Enum transformation_kind,
    DDS_Security_SecurityException *ex);

/**
 * @param[in,out] session_key           Session key
 * @param[in]     session_id            Session Id
 * @param[in]     master_salt           Master salt
 * @param[in]     master_key            Master key
 * @param[in]     transformation_kind   Transformation kind
 * @param[in,out] ex                    Security exception
 */
SECURITY_EXPORT bool crypto_calculate_receiver_specific_key(
    crypto_session_key_t *session_key,
    uint32_t session_id,
    const unsigned char *master_salt,
    const unsigned char *master_key,
    DDS_Security_CryptoTransformKind_Enum transformation_kind,
    DDS_Security_SecurityException *ex);

SECURITY_EXPORT uint32_t crypto_get_key_size(DDS_Security_CryptoTransformKind_Enum kind);
SECURITY_EXPORT uint32_t crypto_get_random_uint32(void);
SECURITY_EXPORT uint64_t crypto_get_random_uint64(void);

/**
 * @brief Compute a HMAC256 on the provided data.
 *
 * @param[in]     key       The key used to compute the HMAC256 result
 * @param[in]     key_size  The size of the key (128 or 256 bits)
 * @param[in]     data      The data on which the HMAC is computed
 * @param[in]     data_size The size of the data
 * @param[in,out] ex        Security exception
 */
SECURITY_EXPORT unsigned char *crypto_hmac256(
    const unsigned char *key,
    uint32_t key_size,
    const unsigned char *data,
    uint32_t data_size,
    DDS_Security_SecurityException *ex);

#endif /* CRYPTO_UTILS_H */
