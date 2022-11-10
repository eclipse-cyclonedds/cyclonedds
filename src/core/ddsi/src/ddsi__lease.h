/*
 * Copyright(c) 2006 to 2022 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSI__LEASE_H
#define DDSI__LEASE_H

#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/fibheap.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsi/ddsi_lease.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_entity_common;
struct ddsi_domaingv; /* FIXME: make a special for the lease admin */

int ddsi_compare_lease_tsched (const void *va, const void *vb);
int ddsi_compare_lease_tdur (const void *va, const void *vb);
void ddsi_lease_management_init (struct ddsi_domaingv *gv);
void ddsi_lease_management_term (struct ddsi_domaingv *gv);
struct ddsi_lease *ddsi_lease_new (ddsrt_etime_t texpire, int64_t tdur, struct ddsi_entity_common *e);
struct ddsi_lease *ddsi_lease_clone (const struct ddsi_lease *l);
void ddsi_lease_register (struct ddsi_lease *l);
void ddsi_lease_unregister (struct ddsi_lease *l);
void ddsi_lease_free (struct ddsi_lease *l);
void ddsi_lease_renew (struct ddsi_lease *l, ddsrt_etime_t tnow);
void ddsi_lease_set_expiry (struct ddsi_lease *l, ddsrt_etime_t when);
int64_t ddsi_check_and_handle_lease_expiration (struct ddsi_domaingv *gv, ddsrt_etime_t tnow);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__LEASE_H */
