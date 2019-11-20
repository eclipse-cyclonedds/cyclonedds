/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#ifndef SECURITY_CRYPTO_MISSING_H_
#define SECURITY_CRYPTO_MISSING_H_

#include "dds/security/cryptography_missing_function_export.h"
#include "dds/security/dds_security_api.h"

SECURITY_EXPORT int32_t
init_crypto(const char *argument, void **context);

SECURITY_EXPORT int32_t
finalize_crypto(void *context);


#endif /* SECURITY_CRYPTO_MISSING_H_ */
