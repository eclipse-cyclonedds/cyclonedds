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

char * generate_ca(const char *ca_name, const char * ca_priv_key_str, int not_valid_before, int not_valid_after);
char * generate_identity(const char * ca_cert_str, const char * ca_priv_key_str, const char * name, const char * priv_key_str, int not_valid_before, int not_valid_after, char ** subject);

#endif /* SECURITY_CORE_TEST_CERT_UTILS_H_ */
