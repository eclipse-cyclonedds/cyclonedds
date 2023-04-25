// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef ACCESS_CONTROL_H
#define ACCESS_CONTROL_H

#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/export.h"

SECURITY_EXPORT int init_access_control(const char *argument, void **context, struct ddsi_domaingv *gv);
SECURITY_EXPORT int finalize_access_control(void *context);

#endif /* ACCESS_CONTROL_H */
