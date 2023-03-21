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
#ifndef DDSI_XEVENT_H
#define DDSI_XEVENT_H

#include "dds/ddsrt/retcode.h"
#include "dds/ddsi/ddsi_guid.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_xevent;
struct ddsi_xeventq;

struct ddsi_domaingv;
struct ddsi_xpack;

typedef void (*ddsi_xevent_cb_t) (struct ddsi_domaingv *gv, struct ddsi_xevent *ev, struct ddsi_xpack *xp, void *arg, ddsrt_mtime_t tnow);

/**
 * @component timed_events
 * @remark: locks EVQ for the duration of the operation
 * @param ev the event
 */
void ddsi_delete_xevent (struct ddsi_xevent *ev);

/**
 * @component timed_events
 * @remark: locks EVQ for the duration of the operation
 * @param ev the event
 */
int ddsi_resched_xevent_if_earlier (struct ddsi_xevent *ev, ddsrt_mtime_t tsched);

/**
 * @component timed_events
 *
 * @remark: delete does not block until an ongoing callback finishes
 * @remark: cb will be called with now = NEVER if the event is still enqueued when when ddsi_xeventq_free starts cleaning up
 *
 * @param evq       event queue
 * @param tsched    timestamp scheduled
 * @param cb        callback function pointer
 * @param arg       argument (copied into ddsi_xevent)
 * @param arg_size  size of argument
 * @return struct ddsi_xevent*
 */
struct ddsi_xevent *ddsi_qxev_callback (struct ddsi_xeventq *evq, ddsrt_mtime_t tsched, ddsi_xevent_cb_t cb, const void *arg, size_t arg_size, bool sync_on_delete);

#if defined (__cplusplus)
}
#endif
#endif /* DDSI_XEVENT_H */
