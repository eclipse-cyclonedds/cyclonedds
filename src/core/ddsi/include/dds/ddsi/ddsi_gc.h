/*
 * Copyright(c) 2006 to 2021 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSI_GC_H
#define DDSI_GC_H

#include "dds/export.h"
#include "dds/ddsi/q_thread.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_gcreq;
struct ddsi_gcreq_queue;
struct ddsi_domaingv;

struct ddsi_writer;
struct ddsi_reader;
struct ddsi_proxy_writer;
struct ddsi_proxy_reader;

typedef void (*ddsi_gcreq_cb_t) (struct ddsi_gcreq *gcreq);

struct ddsi_idx_vtime {
  struct thread_state *thrst;
  vtime_t vtime;
};

struct ddsi_gcreq {
  struct ddsi_gcreq *next;
  struct ddsi_gcreq_queue *queue;
  ddsi_gcreq_cb_t cb;
  void *arg;
  uint32_t nvtimes;
  struct ddsi_idx_vtime vtimes[];
};

DDS_EXPORT struct ddsi_gcreq_queue *ddsi_gcreq_queue_new (struct ddsi_domaingv *gv);
DDS_EXPORT void ddsi_gcreq_queue_drain (struct ddsi_gcreq_queue *q);
DDS_EXPORT void ddsi_gcreq_queue_free (struct ddsi_gcreq_queue *q);

DDS_EXPORT struct ddsi_gcreq *ddsi_gcreq_new (struct ddsi_gcreq_queue *gcreq_queue, ddsi_gcreq_cb_t cb);
DDS_EXPORT bool ddsi_gcreq_queue_start (struct ddsi_gcreq_queue *q);
DDS_EXPORT void ddsi_gcreq_free (struct ddsi_gcreq *gcreq);
DDS_EXPORT void ddsi_gcreq_enqueue (struct ddsi_gcreq *gcreq);
DDS_EXPORT int ddsi_gcreq_requeue (struct ddsi_gcreq *gcreq, ddsi_gcreq_cb_t cb);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_GC_H */
