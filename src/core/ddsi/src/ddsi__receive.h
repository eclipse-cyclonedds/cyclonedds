// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__RECEIVE_H
#define DDSI__RECEIVE_H

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_rbufpool;
struct ddsi_rsample_info;
struct ddsi_rdata;
struct ddsi_rmsg;
struct ddsi_tran_listener;
struct ddsi_recv_thread_arg;
struct ddsi_writer;
struct ddsi_proxy_reader;

struct ddsi_gap_info {
  ddsi_seqno_t gapstart; // == 0 on init, indicating no gap recorded yet
  ddsi_seqno_t gapend;   // >= gapstart
  uint32_t gapnumbits;
  uint32_t gapbits[256 / 32];
};

/** @component incoming_rtps */
void ddsi_gap_info_init(struct ddsi_gap_info *gi);

/** @component incoming_rtps */
void ddsi_gap_info_update(struct ddsi_domaingv *gv, struct ddsi_gap_info *gi, ddsi_seqno_t seqnr);

/** @component incoming_rtps */
struct ddsi_xmsg * ddsi_gap_info_create_gap(struct ddsi_writer *wr, struct ddsi_proxy_reader *prd, struct ddsi_gap_info *gi);

/** @component incoming_rtps */
void ddsi_trigger_recv_threads (const struct ddsi_domaingv *gv);

/** @component incoming_rtps */
uint32_t ddsi_recv_thread (void *vrecv_thread_arg);

/** @component incoming_rtps */
uint32_t ddsi_listen_thread (struct ddsi_tran_listener * listener);

/** @component incoming_rtps */
int ddsi_user_dqueue_handler (const struct ddsi_rsample_info *sampleinfo, const struct ddsi_rdata *fragchain, const ddsi_guid_t *rdguid, void *qarg);

/** @component incoming_rtps */
int ddsi_add_gap (struct ddsi_xmsg *msg, struct ddsi_writer *wr, struct ddsi_proxy_reader *prd, ddsi_seqno_t start, ddsi_seqno_t base, uint32_t numbits, const uint32_t *bits);

/** @component incoming_rtps */
void ddsi_handle_rtps_message (struct ddsi_thread_state * const thrst, struct ddsi_domaingv *gv, struct ddsi_tran_conn * conn, const ddsi_guid_prefix_t *guidprefix, struct ddsi_rbufpool *rbpool, struct ddsi_rmsg *rmsg, size_t sz, unsigned char *msg, const ddsi_locator_t *srcloc);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__RECEIVE_H */
