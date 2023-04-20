// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_LEASE_H
#define DDSI_LEASE_H

#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/fibheap.h"
#include "dds/ddsrt/time.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_entity_common;

struct ddsi_lease {
  ddsrt_fibheap_node_t heapnode;
  ddsrt_fibheap_node_t pp_heapnode;
  ddsrt_etime_t tsched;         /* access guarded by leaseheap_lock */
  ddsrt_atomic_uint64_t tend;   /* really an ddsrt_etime_t */
  dds_duration_t tdur;          /* constant (renew depends on it) */
  struct ddsi_entity_common *entity; /* constant */
};

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_LEASE_H */
