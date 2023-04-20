// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef SECURITY_CORE_TEST_ACCESS_CONTROL_WRAPPER_H_
#define SECURITY_CORE_TEST_ACCESS_CONTROL_WRAPPER_H_

#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/access_control_wrapper_export.h"

/* Topic name prefix expected by this wrapper when running in not-allowed
   mode. This prefix is used to exclude built-in topics from being disallowed. */
#define AC_WRAPPER_TOPIC_PREFIX "ddssec_access_control_"

struct dds_security_access_control_impl;

/* Init in all-ok mode: all functions return success without calling the actual plugin */
SECURITY_EXPORT int init_test_access_control_all_ok(const char *argument, void **context, struct ddsi_domaingv *gv);
SECURITY_EXPORT int finalize_test_access_control_all_ok(void *context);

/* Init in missing function mode: one of the function pointers is null */
SECURITY_EXPORT int init_test_access_control_missing_func(const char *argument, void **context, struct ddsi_domaingv *gv);
SECURITY_EXPORT int finalize_test_access_control_missing_func(void *context);

SECURITY_EXPORT int init_test_access_control_wrapped(const char *argument, void **context, struct ddsi_domaingv *gv);
SECURITY_EXPORT int finalize_test_access_control_wrapped(void *context);

/* Init functions for not-allowed modes */
#define INIT_NOT_ALLOWED_DECL(name_) \
  SECURITY_EXPORT int init_test_access_control_##name_ (const char *argument, void **context, struct ddsi_domaingv *gv);

INIT_NOT_ALLOWED_DECL(local_participant_not_allowed)
INIT_NOT_ALLOWED_DECL(local_topic_not_allowed)
INIT_NOT_ALLOWED_DECL(local_writer_not_allowed)
INIT_NOT_ALLOWED_DECL(local_reader_not_allowed)
INIT_NOT_ALLOWED_DECL(local_permissions_not_allowed)
INIT_NOT_ALLOWED_DECL(remote_participant_not_allowed)
INIT_NOT_ALLOWED_DECL(remote_topic_not_allowed)
INIT_NOT_ALLOWED_DECL(remote_writer_not_allowed)
INIT_NOT_ALLOWED_DECL(remote_reader_not_allowed)
INIT_NOT_ALLOWED_DECL(remote_reader_relay_only)
INIT_NOT_ALLOWED_DECL(remote_permissions_not_allowed)

SECURITY_EXPORT int finalize_test_access_control_not_allowed(void *context);

#endif /* SECURITY_CORE_TEST_ACCESS_CONTROL_WRAPPER_H_ */
