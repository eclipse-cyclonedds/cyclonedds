/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef Q_LEASE_H
#define Q_LEASE_H

#include "ddsi/q_time.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct receiver_state;
struct participant;
struct lease;
struct entity_common;
struct thread_state1;

void lease_management_init (void);
void lease_management_term (void);
struct lease *lease_new (nn_etime_t texpire, int64_t tdur, struct entity_common *e);
void lease_register (struct lease *l);
void lease_free (struct lease *l);
void lease_renew (struct lease *l, nn_etime_t tnow);
void lease_set_expiry (struct lease *l, nn_etime_t when);
int64_t check_and_handle_lease_expiration (struct thread_state1 *self, nn_etime_t tnow);

void handle_PMD (const struct receiver_state *rst, nn_wctime_t timestamp, unsigned statusinfo, const void *vdata, unsigned len);

#if defined (__cplusplus)
}
#endif

#endif /* Q_LEASE_H */
