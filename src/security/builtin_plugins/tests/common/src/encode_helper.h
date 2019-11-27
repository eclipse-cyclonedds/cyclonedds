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
#ifndef DDS_SECURITY_BUITIN_TEST_ENCODE_HELPER_H
#define DDS_SECURITY_BUITIN_TEST_ENCODE_HELPER_H

#include "dds/ddsrt/types.h"
#include "dds/security/dds_security_api.h"
#include "crypto_objects.h"

bool
crypto_calculate_session_key_test(
    crypto_key_t *session_key,
    uint32_t session_id,
    const crypto_salt_t *master_salt,
    const crypto_key_t *master_key,
    DDS_Security_CryptoTransformKind_Enum transformation_kind);

bool calculate_receiver_specific_key_test(
    unsigned char *session_key,
    uint32_t session_id,
    const crypto_salt_t *master_salt,
    const crypto_key_t *master_key,
    DDS_Security_CryptoTransformKind_Enum transformation_kind);

#endif /* DDS_SECURITY_BUITIN_TEST_ENCODE_HELPER_H */