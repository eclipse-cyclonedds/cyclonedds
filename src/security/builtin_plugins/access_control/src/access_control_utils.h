// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef ACCESS_CONTROL_UTILS_H
#define ACCESS_CONTROL_UTILS_H

#include "dds/ddsrt/types.h"
#include "dds/security/export.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/openssl_support.h"

#define DDS_ACCESS_CONTROL_PLUGIN_CONTEXT "Access Control"

bool ac_X509_certificate_read(const char *data, X509 **x509Cert, DDS_Security_SecurityException *ex);
bool ac_X509_certificate_from_data(const char *data, int len, X509 **x509Cert, DDS_Security_SecurityException *ex);
char *ac_get_certificate_subject_name(X509 *cert, DDS_Security_SecurityException *ex);
bool ac_PKCS7_document_check(const char *data, size_t len, X509 *cert, char **document, DDS_Security_SecurityException *ex);
bool ac_check_subjects_are_equal(const char *permissions_sn, const char *identity_sn);
size_t ac_regular_file_size(const char *filename);
SECURITY_EXPORT bool ac_fnmatch(const char* pattern, const char* string);

#endif /* ACCESS_CONTROL_UTILS_H */
