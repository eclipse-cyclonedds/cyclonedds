// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef CRYPTO_TRANSFORM_H
#define CRYPTO_TRANSFORM_H

#include "dds/security/dds_security_api.h"

/**
 * @brief Allocation function for implementer structure (with internal variables) transparently.
 */
dds_security_crypto_transform *
dds_security_crypto_transform__alloc(
    const dds_security_cryptography *crypto);

void
dds_security_crypto_transform__dealloc(
    dds_security_crypto_transform *instance);

#endif /* CRYPTO_TRANSFORM_H */
