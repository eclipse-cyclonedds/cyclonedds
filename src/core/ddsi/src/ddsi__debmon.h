// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__DEBMON_H
#define DDSI__DEBMON_H

#include "dds/ddsi/ddsi_locator.h"
#include "dds/ddsi/ddsi_tran.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_domaingv;
struct ddsi_debug_monitor;

typedef int (*ddsi_debug_monitor_cpf_t) (struct ddsi_tran_conn * conn, const char *fmt, ...);
typedef int (*ddsi_debug_monitor_plugin_t) (struct ddsi_tran_conn * conn, ddsi_debug_monitor_cpf_t cpf, void *arg);

/** @component debug_support */
struct ddsi_debug_monitor *ddsi_new_debug_monitor (struct ddsi_domaingv *gv, int32_t port);

/** @component debug_support */
bool ddsi_get_debug_monitor_locator (struct ddsi_debug_monitor *dm, ddsi_locator_t *locator);

/** @component debug_support */
void ddsi_free_debug_monitor (struct ddsi_debug_monitor *dm);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__DEBMON_H */
