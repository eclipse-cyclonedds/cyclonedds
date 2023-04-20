// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

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

/** @brief removes an event from the event queue and frees it
 * @component timed_events
 *
 * @remark: may be called from inside the event handler
 * @remark: if the event was created with the `sync_on_delete` flag set, it blocks until
 *   the system guarantees that it is not and will not be executing anymore
 *
 * @param[in] ev the event to be deleted
 */
void ddsi_delete_xevent (struct ddsi_xevent *ev)
  ddsrt_nonnull_all;

/** @brief reschedules the event iff `tsched` precedes the time for which it is currently scheduled
 * @component timed_events
 *
 * @remark: may be called from inside the event handler
 *
 * @param[in] ev the event to be rescheduled
 * @param[in] tsched new scheduled time
 *
 * @return whether the event was actually rescheduled, i.e., whether `tsched` preceded the
 *   time for which it was scheduled before
 *
 * @retval 0 not rescheduled
 * @retval 1 rescheduled
 */
int ddsi_resched_xevent_if_earlier (struct ddsi_xevent *ev, ddsrt_mtime_t tsched)
  ddsrt_nonnull_all;

/** @brief creates a new event object on the given event queue for invoking `cb` in the
 *   future on the thread handling this queue
 * @component timed_events
 *
 * @remark: delete does not block until an ongoing callback finishes, unless
 *   `sync_on_delete` is set
 * @remark: just before invoking the callback, the schedule time is set to NEVER
 * @remark: cb will be called with now = NEVER if the event is still enqueued when
 *   `ddsi_xeventq_free` starts cleaning up (a consequence of the combination of
 *   this and being allowed to delete the event from within the callback is that
 *   one need not always store the returned event pointer)
 *
 * @param[in] evq       event queue
 * @param[in] tsched    timestamp scheduled, may be NEVER
 * @param[in] cb        callback function pointer
 * @param[in] arg       argument (copied into ddsi_xevent, may be a null pointer
 *                      when arg_size = 0)
 * @param[in] arg_size  size of argument in bytes
 * @param[in] sync_on_delete    whether `ddsi_delete_xevent` must block until the
 *                      system guarantees that `cb` is not and will not be executing
 *                      anymore
 *
 * @return the new event object
 */
struct ddsi_xevent *ddsi_qxev_callback (struct ddsi_xeventq *evq, ddsrt_mtime_t tsched, ddsi_xevent_cb_t cb, const void *arg, size_t arg_size, bool sync_on_delete)
  ddsrt_nonnull((1,3));

#if defined (__cplusplus)
}
#endif
#endif /* DDSI_XEVENT_H */
