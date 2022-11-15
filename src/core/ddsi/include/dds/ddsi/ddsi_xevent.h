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

/* All of the following lock EVQ for the duration of the operation */
void ddsi_delete_xevent (struct ddsi_xevent *ev);
void ddsi_delete_xevent_callback (struct ddsi_xevent *ev);
int ddsi_resched_xevent_if_earlier (struct ddsi_xevent *ev, ddsrt_mtime_t tsched);

/* cb will be called with now = NEVER if the event is still enqueued when when ddsi_xeventq_free starts cleaning up */
struct ddsi_xevent *ddsi_qxev_callback (struct ddsi_xeventq *evq, ddsrt_mtime_t tsched, void (*cb) (struct ddsi_xevent *xev, void *arg, ddsrt_mtime_t now), void *arg);

#if defined (__cplusplus)
}
#endif
#endif /* DDSI_XEVENT_H */
