// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS_SECURITY_AC_TOKENS_H
#define DDS_SECURITY_AC_TOKENS_H

// FIXME: move token names into separate file
#include "dds/ddsi/ddsi_security_msg.h"

// PermissionsCredentialToken
#define DDS_ACTOKEN_PERMISSIONS_CREDENTIAL_CLASS_ID "DDS:Access:PermissionsCredential"
#define DDS_ACTOKEN_PROP_PERM_CERT "dds.perm.cert"

// PermissionsToken
#define DDS_ACTOKEN_PERMISSIONS_CLASS_ID "DDS:Access:Permissions:1.0"
#define DDS_ACTOKEN_PROP_C_ID "c.id"
#define DDS_ACTOKEN_PROP_C_PERM "c.perm"
#define DDS_ACTOKEN_PROP_PERM_CA_SN "dds.perm_ca.sn"
#define DDS_ACTOKEN_PROP_PERM_CA_ALGO "dds.perm_ca.algo"

#endif
