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
#ifndef Q_RECEIVE_H
#define Q_RECEIVE_H

#if defined (__cplusplus)
extern "C" {
#endif

struct nn_rbufpool;
struct nn_rsample_info;
struct nn_rdata;
struct ddsi_tran_listener;
struct recv_thread_arg;

void trigger_recv_threads (const struct ddsi_domaingv *gv);
uint32_t recv_thread (void *vrecv_thread_arg);
uint32_t listen_thread (struct ddsi_tran_listener * listener);
int user_dqueue_handler (const struct nn_rsample_info *sampleinfo, const struct nn_rdata *fragchain, const ddsi_guid_t *rdguid, void *qarg);

#if defined (__cplusplus)
}
#endif

#endif /* Q_RECEIVE_H */
