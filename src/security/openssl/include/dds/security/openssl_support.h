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

/* There's OpenSSL 1.1.x and there's OpenSSL 1.0.2 and the difference is like
   night and day: 1.1.0 deprecated all the initialization and cleanup routines
   and so any library can link with OpenSSL and use it safely without breaking
   the application code or some other library in the same process.

   OpenSSL 1.0.2h deprecated the cleanup functions such as EVP_cleanup because
   calling the initialisation functions multiple times was survivable, but an
   premature invocation of the cleanup functions deadly. It still has the per-
   thread error state that one ought to clean up, but that firstly requires
   keeping track of which threads make OpenSSL calls, and secondly we do
   perform OpenSSL calls on the applications main-thread and so cleaning up
   might interfere with the application code.

   Compatibility with 1.0.2 exists merely as a courtesy to those who insist on
   using it with that problematic piece of code. We only initialise it, and we
   don't clean up thread state. If Cyclone DDS is the only part of the process
   that uses OpenSSL, it should be ok (just some some minor leaks at the end),
   if the application code or another library also uses it, it'll probably be
   fine too. */

#ifdef _WIN32
/* WinSock2 must be included before openssl 1.0.2 headers otherwise winsock will be used */
#include <WinSock2.h>
#endif

#define OPENSSL_API_COMPAT 10101

#include <openssl/opensslv.h>
#include <openssl/opensslconf.h>
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/conf.h>

#if OPENSSL_VERSION_NUMBER >= 0x1000200fL
#define AUTH_INCLUDE_EC
#include <openssl/ec.h>
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
#define AUTH_INCLUDE_DH_ACCESSORS
#endif
#else
#error "OpenSSL version is not supported"
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

#if OPENSSL_VERSION_NUMBER < 0x10100000L
/* 1.1.0 has it as a supported API. 1.0.2 has it in practice and since that has been
   obsolete for ages, chances are that we can safely use it */
struct tm *OPENSSL_gmtime(const time_t *timer, struct tm *result);
#endif

void DDS_Security_Exception_set_with_openssl_error (DDS_Security_SecurityException *ex, const char *context, int code, int minor_code, const char *error_area)
  ddsrt_nonnull_all;

#endif
