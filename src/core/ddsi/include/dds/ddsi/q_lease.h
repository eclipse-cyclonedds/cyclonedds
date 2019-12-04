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

#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/fibheap.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsi/q_time.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct receiver_state;
struct participant;
struct entity_common;
struct q_globals; /* FIXME: make a special for the lease admin */

struct lease {
  ddsrt_fibheap_node_t heapnode;
  ddsrt_fibheap_node_t pp_heapnode;
  nn_etime_t tsched;            /* access guarded by leaseheap_lock */
  ddsrt_atomic_uint64_t tend;   /* really an nn_etime_t */
  dds_duration_t tdur;          /* constant (renew depends on it) */
  struct entity_common *entity; /* constant */
};

int compare_lease_tsched (const void *va, const void *vb);
int compare_lease_tdur (const void *va, const void *vb);
void lease_management_init (struct q_globals *gv);
void lease_management_term (struct q_globals *gv);
struct lease *lease_new (nn_etime_t texpire, int64_t tdur, struct entity_common *e);
struct lease *lease_clone (const struct lease *l);
void lease_register (struct lease *l);
void lease_unregister (struct lease *l);
void lease_free (struct lease *l);
void lease_renew (struct lease *l, nn_etime_t tnow);
void lease_set_expiry (struct lease *l, nn_etime_t when);
int64_t check_and_handle_lease_expiration (struct q_globals *gv, nn_etime_t tnow);

#if defined (__cplusplus)
}
#endif

#endif /* Q_LEASE_H */
