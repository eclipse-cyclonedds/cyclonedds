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
#ifndef SECURITY_CORE_TEST_ACCESS_CONTROL_WRAPPER_H_
#define SECURITY_CORE_TEST_ACCESS_CONTROL_WRAPPER_H_

#include "dds/security/dds_security_api.h"
#include "dds/security/access_control_wrapper_export.h"

/* Init in all-ok mode: all functions return success without calling the actual plugin */
SECURITY_EXPORT int32_t init_test_access_control_all_ok(const char *argument, void **context);
SECURITY_EXPORT int32_t finalize_test_access_control_all_ok(void *context);

/* Init in missing function mode: one of the function pointers is null */
SECURITY_EXPORT int32_t init_test_access_control_missing_func(const char *argument, void **context);
SECURITY_EXPORT int32_t finalize_test_access_control_missing_func(void *context);

#endif /* SECURITY_CORE_TEST_ACCESS_CONTROL_WRAPPER_H_ */
