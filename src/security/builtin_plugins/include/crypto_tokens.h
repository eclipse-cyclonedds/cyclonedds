// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS_SECURITY_CRYPTO_TOKENS_H
#define DDS_SECURITY_CRYPTO_TOKENS_H

// FIXME: move token names into separate file
#include "dds/ddsi/ddsi_security_msg.h"

// CryptoToken
#define DDS_CRYPTOTOKEN_CLASS_ID "DDS:Crypto:AES_GCM_GMAC"
#define DDS_CRYPTOTOKEN_PROP_KEYMAT "dds.cryp.keymat"

#endif
