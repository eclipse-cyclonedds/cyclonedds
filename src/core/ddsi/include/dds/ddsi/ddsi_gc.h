// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_GC_H
#define DDSI_GC_H

#include "dds/export.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_gcreq;
struct ddsi_gcreq_queue;

typedef void (*ddsi_gcreq_cb_t) (struct ddsi_gcreq *gcreq);

/** @component garbage_collector */
DDS_EXPORT struct ddsi_gcreq *ddsi_gcreq_new (struct ddsi_gcreq_queue *gcreq_queue, ddsi_gcreq_cb_t cb);

/** @component garbage_collector */
DDS_EXPORT void ddsi_gcreq_free (struct ddsi_gcreq *gcreq);

/** @component garbage_collector */
DDS_EXPORT void ddsi_gcreq_enqueue (struct ddsi_gcreq *gcreq);

/** @component garbage_collector */
DDS_EXPORT void * ddsi_gcreq_get_arg (struct ddsi_gcreq *gcreq);

/** @component garbage_collector */
DDS_EXPORT void ddsi_gcreq_set_arg (struct ddsi_gcreq *gcreq, void *arg);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_GC_H */
