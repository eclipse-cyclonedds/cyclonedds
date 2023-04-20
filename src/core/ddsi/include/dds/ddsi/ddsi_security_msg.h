// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_SECURITY_MSG_H
#define DDSI_SECURITY_MSG_H

#include "dds/features.h"

#ifdef DDS_HAS_SECURITY

#include "dds/ddsrt/misc.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define DDS_SECURITY_AUTH_REQUEST                     "dds.sec.auth_request"
#define DDS_SECURITY_AUTH_HANDSHAKE                   "dds.sec.auth"

#define DDS_SECURITY_AUTH_VERSION_MAJOR 1
#define DDS_SECURITY_AUTH_VERSION_MINOR 0

#define DDS_SECURITY_AUTH_TOKEN_CLASS_ID_BASE         "DDS:Auth:PKI-DH:"
#define DDS_SECURITY_AUTH_TOKEN_CLASS_ID              DDS_SECURITY_AUTH_TOKEN_CLASS_ID_BASE DDSRT_STRINGIFY(DDS_SECURITY_AUTH_VERSION_MAJOR) "." DDSRT_STRINGIFY(DDS_SECURITY_AUTH_VERSION_MINOR)

#define DDS_SECURITY_AUTH_REQUEST_TOKEN_CLASS_ID      DDS_SECURITY_AUTH_TOKEN_CLASS_ID "+AuthReq"
#define DDS_SECURITY_AUTH_HANDSHAKE_REQUEST_TOKEN_ID  DDS_SECURITY_AUTH_TOKEN_CLASS_ID "+Req"
#define DDS_SECURITY_AUTH_HANDSHAKE_REPLY_TOKEN_ID    DDS_SECURITY_AUTH_TOKEN_CLASS_ID "+Reply"
#define DDS_SECURITY_AUTH_HANDSHAKE_FINAL_TOKEN_ID    DDS_SECURITY_AUTH_TOKEN_CLASS_ID "+Final"

#if defined (__cplusplus)
}
#endif

#endif /* DDS_HAS_SECURITY */

#endif /* DDSI_SECURITY_MSG_H */
