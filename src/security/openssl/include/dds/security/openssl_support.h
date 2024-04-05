// Copyright(c) 2020 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS_OPENSSL_SUPPORT_H
#define DDS_OPENSSL_SUPPORT_H

#include "dds/security/dds_security_api_types.h"

#ifdef _WIN32
/* WinSock2 must be included before openssl 1.0.2 headers otherwise winsock will be used */
#include <WinSock2.h>
#endif

/* Setting this macro to 30000 specifies that the code will be compatible with openssl version 3 and lower like version 1.1 */
#define OPENSSL_API_COMPAT 30000

#include <openssl/opensslv.h>
#include <openssl/opensslconf.h>
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/conf.h>

#include <openssl/ec.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/core_names.h>
#endif
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/x509_vfy.h>

void dds_openssl_init (void);

void DDS_Security_Exception_set_with_openssl_error (DDS_Security_SecurityException *ex, const char *context, int code, int minor_code, const char *error_area)
  ddsrt_nonnull_all;

#endif
