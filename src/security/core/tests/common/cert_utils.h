// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef SECURITY_CORE_TEST_CERT_UTILS_H_
#define SECURITY_CORE_TEST_CERT_UTILS_H_

#include <stdlib.h>
#include "dds/security/openssl_support.h"

char * generate_ca(const char *ca_name, const char * ca_priv_key_str, int not_valid_before, int not_valid_after);
char * generate_identity(const char * ca_cert_str, const char * ca_priv_key_str, const char * name, const char * priv_key_str, int not_valid_before, int not_valid_after, char ** subject);
char * generate_pkcs11_private_key(const char *token, const char *name, uint32_t id, const char *pin);
char * generate_ca_pkcs11(const char *ca_name, const char * ca_priv_key_uri, int not_valid_before, int not_valid_after);
char * generate_identity_pkcs11(const char * ca_cert_str, const char * ca_priv_key_uri, const char * name, const char * priv_key_uri, int not_valid_before, int not_valid_after, char ** subject);
EVP_PKEY * get_priv_key_pkcs11(const char *uri);
X509 * get_certificate_pkcs11(const char *uri);
bool check_pkcs11_provider(void);

#endif /* SECURITY_CORE_TEST_CERT_UTILS_H_ */
