// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS_SECURITY_BUITIN_TEST_CRYPTO_HELPER_H
#define DDS_SECURITY_BUITIN_TEST_CRYPTO_HELPER_H

#include "dds/ddsrt/types.h"
#include "dds/security/dds_security_api.h"
#include "crypto_objects.h"

bool
crypto_calculate_session_key_test(
    crypto_session_key_t *session_key,
    uint32_t session_id,
    const unsigned char *master_salt,
    const unsigned char *master_key,
    DDS_Security_CryptoTransformKind_Enum transformation_kind);

bool calculate_receiver_specific_key_test(
    crypto_session_key_t *session_key,
    uint32_t session_id,
    const unsigned char *master_salt,
    const unsigned char *master_key,
    DDS_Security_CryptoTransformKind_Enum transformation_kind);

int master_salt_not_empty(master_key_material *keymat);
int master_key_not_empty(master_key_material *keymat);
int master_receiver_specific_key_not_empty(master_key_material *keymat);

#endif /* DDS_SECURITY_BUITIN_TEST_CRYPTO_HELPER_H */

