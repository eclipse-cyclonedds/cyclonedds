// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__XEVENT_H
#define DDSI__XEVENT_H

#include "dds/ddsrt/retcode.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_xevent.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* NOTE: xevents scheduled with the same tsched used to always be
   executed in the order of scheduling, but that is no longer true.
   With the messages now via the "untimed" path, that should not
   introduce any issues. */

struct ddsi_xevent;
struct ddsi_xeventq;
struct ddsi_proxy_writer;
struct ddsi_proxy_reader;
struct ddsi_domaingv;
struct ddsi_xmsg;

/** @component timed_events */
struct ddsi_xeventq *ddsi_xeventq_new (struct ddsi_domaingv *gv, size_t max_queued_rexmit_bytes, size_t max_queued_rexmit_msgs);


/**
 * @component timed_events
 *
 * ddsi_xeventq_free calls callback handlers with t = NEVER, at which point
 * they are required to free whatever memory is claimed for the argument
 * and call ddsi_delete_xevent.
 *
 * @param evq the event queue
 */
void ddsi_xeventq_free (struct ddsi_xeventq *evq);

/** @component timed_events */
dds_return_t ddsi_xeventq_start (struct ddsi_xeventq *evq, const char *name); /* <0 => error, =0 => ok */

/** @component timed_events */
void ddsi_xeventq_stop (struct ddsi_xeventq *evq);

/** @component timed_events */
void ddsi_qxev_msg (struct ddsi_xeventq *evq, struct ddsi_xmsg *msg);

/** @component timed_events */
void ddsi_qxev_nt_callback (struct ddsi_xeventq *evq, void (*cb) (void *arg), void *arg);

enum ddsi_qxev_msg_rexmit_result {
  DDSI_QXEV_MSG_REXMIT_DROPPED,
  DDSI_QXEV_MSG_REXMIT_MERGED,
  DDSI_QXEV_MSG_REXMIT_QUEUED
};

/** @component timed_events */
enum ddsi_qxev_msg_rexmit_result ddsi_qxev_msg_rexmit_wrlock_held (struct ddsi_xeventq *evq, struct ddsi_xmsg *msg, int force);

#ifndef NDEBUG
/**
 * @component timed_events
 * @remark: locks EVQ for the duration of the operation
 * @param ev the event
 */
bool ddsi_delete_xevent_pending (struct ddsi_xevent *ev);
#endif

#if defined (__cplusplus)
}
#endif
#endif /* DDSI__XEVENT_H */
