/*
 * Copyright(c) 2006 to 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef SECURITY_CORE_TEST_CRYPTO_WRAPPER_H_
#define SECURITY_CORE_TEST_CRYPTO_WRAPPER_H_

#include "dds/security/dds_security_api.h"
#include "dds/security/dds_security_api_defs.h"
#include "dds/security/cryptography_wrapper_export.h"

struct dds_security_cryptography_impl;

SECURITY_EXPORT void set_protection_kinds(
  struct dds_security_cryptography_impl * impl,
  DDS_Security_ProtectionKind rtps_protection_kind,
  DDS_Security_ProtectionKind metadata_protection_kind,
  DDS_Security_BasicProtectionKind payload_protection_kind);

SECURITY_EXPORT void set_encrypted_secret(struct dds_security_cryptography_impl * impl, const char * secret);

/* Init in all-ok mode: all functions return success without calling the actual plugin */
SECURITY_EXPORT int32_t init_test_cryptography_all_ok(const char *argument, void **context);
SECURITY_EXPORT int32_t finalize_test_cryptography_all_ok(void *context);

/* Init in missing function mode: one of the function pointers is null */
SECURITY_EXPORT int32_t init_test_cryptography_missing_func(const char *argument, void **context);
SECURITY_EXPORT int32_t finalize_test_cryptography_missing_func(void *context);

/* Init in wrapper mode */
SECURITY_EXPORT int32_t init_test_cryptography_wrapped(const char *argument, void **context);
SECURITY_EXPORT int32_t finalize_test_cryptography_wrapped(void *context);

#endif /* SECURITY_CORE_TEST_CRYPTO_WRAPPER_H_ */
