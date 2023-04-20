// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS_SECURITY_AUTH_TOKENS_H
#define DDS_SECURITY_AUTH_TOKENS_H

// FIXME: move token names into separate file
#include "dds/ddsi/ddsi_security_msg.h"

// IdentityToken
#define DDS_AUTHTOKEN_CLASS_ID "DDS:Auth:PKI-DH:1.0"
#define DDS_AUTHTOKEN_PROP_CERT_SN "dds.cert.sn"
#define DDS_AUTHTOKEN_PROP_CERT_ALGO "dds.cert.algo"
#define DDS_AUTHTOKEN_PROP_CA_SN "dds.ca.sn"
#define DDS_AUTHTOKEN_PROP_CA_ALGO "dds.ca.algo"

// IdentityStatusToken
//#define DDS_AUTHTOKEN_CLASS_ID "DDS:Auth:PKI-DH:1.0"
#define DDS_AUTHTOKEN_PROP_OCSP_STATUS "ocsp_status"

// AuthenticatedPeerCredentialToken
//#define DDS_AUTHTOKEN_CLASS_ID "DDS:Auth:PKI-DH:1.0"
#define DDS_AUTHTOKEN_PROP_C_ID "c.id"
#define DDS_AUTHTOKEN_PROP_C_PERM "c.perm"

// AuthRequestMessageToken
#define DDS_AUTHTOKEN_AUTHREQ_CLASS_ID "DDS:Auth:PKI-DH:1.0+AuthReq"
#define DDS_AUTHTOKEN_PROP_FUTURE_CHALLENGE "future_challenge"

// HandshakeRequestMessageToken
#define DDS_AUTHTOKEN_REQ_CLASS_ID "DDS:Auth:PKI-DH:1.0+Req"
//#define DDS_AUTHTOKEN_PROP_C_ID "c.id"
//#define DDS_AUTHTOKEN_PROP_C_PERM "c.perm"
#define DDS_AUTHTOKEN_PROP_C_PDATA "c.pdata"
#define DDS_AUTHTOKEN_PROP_C_DSIGN_ALGO "c.dsign_algo"
#define DDS_AUTHTOKEN_PROP_C_KAGREE_ALGO "c.kagree_algo"
#define DDS_AUTHTOKEN_PROP_HASH_C1 "hash_c1"
#define DDS_AUTHTOKEN_PROP_DH1 "dh1"
#define DDS_AUTHTOKEN_PROP_CHALLENGE1 "challenge1"
//#define DDS_AUTHTOKEN_PROP_OCSP_STATUS "ocsp_status"

// HandshakeReplyMessageToken
#define DDS_AUTHTOKEN_REPLY_CLASS_ID "DDS:Auth:PKI-DH:1.0+Reply"
//#define DDS_AUTHTOKEN_PROP_C_ID "c.id"
//#define DDS_AUTHTOKEN_PROP_C_PERM "c.perm"
//#define DDS_AUTHTOKEN_PROP_C_PDATA "c.pdata"
//#define DDS_AUTHTOKEN_PROP_C_DSIGN_ALGO "c.dsign_algo"
//#define DDS_AUTHTOKEN_PROP_C_KAGREE_ALGO "c.kagree_algo"
#define DDS_AUTHTOKEN_PROP_HASH_C2 "hash_c2"
#define DDS_AUTHTOKEN_PROP_DH2 "dh2"
//#define DDS_AUTHTOKEN_PROP_HASH_C1 "hash_c1"
//#define DDS_AUTHTOKEN_PROP_DH1 "dh1"
//#define DDS_AUTHTOKEN_PROP_CHALLENGE1 "challenge1"
#define DDS_AUTHTOKEN_PROP_CHALLENGE2 "challenge2"
//#define DDS_AUTHTOKEN_PROP_OCSP_STATUS "ocsp_status"
#define DDS_AUTHTOKEN_PROP_SIGNATURE "signature"

// HandshakeFinalMessageToken
#define DDS_AUTHTOKEN_FINAL_CLASS_ID "DDS:Auth:PKI-DH:1.0+Final"
//#define DDS_AUTHTOKEN_PROP_HASH_C1 "hash_c1"
//#define DDS_AUTHTOKEN_PROP_HASH_C2 "hash_c2"
//#define DDS_AUTHTOKEN_PROP_DH1 "dh1"
//#define DDS_AUTHTOKEN_PROP_DH2 "dh2"
//#define DDS_AUTHTOKEN_PROP_CHALLENGE1 "challenge1"
//#define DDS_AUTHTOKEN_PROP_CHALLENGE2 "challenge2"
//#define DDS_AUTHTOKEN_PROP_SIGNATURE "signature"

#endif
