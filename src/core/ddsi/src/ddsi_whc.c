// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/ddsi/ddsi_protocol.h"
#include "ddsi__whc.h"

extern inline ddsi_seqno_t ddsi_whc_next_seq (const struct ddsi_whc *whc, ddsi_seqno_t seq);
extern inline void ddsi_whc_get_state (const struct ddsi_whc *whc, struct ddsi_whc_state *st);
extern inline bool ddsi_whc_borrow_sample (const struct ddsi_whc *whc, ddsi_seqno_t seq, struct ddsi_whc_borrowed_sample *sample);
extern inline bool ddsi_whc_borrow_sample_key (const struct ddsi_whc *whc, const struct ddsi_serdata *serdata_key, struct ddsi_whc_borrowed_sample *sample);
extern inline void ddsi_whc_return_sample (struct ddsi_whc *whc, struct ddsi_whc_borrowed_sample *sample, bool update_retransmit_info);
extern inline void ddsi_whc_sample_iter_init (const struct ddsi_whc *whc, struct ddsi_whc_sample_iter *it);
extern inline bool ddsi_whc_sample_iter_borrow_next (struct ddsi_whc_sample_iter *it, struct ddsi_whc_borrowed_sample *sample);
extern inline void ddsi_whc_free (struct ddsi_whc *whc);
extern int ddsi_whc_insert (struct ddsi_whc *whc, ddsi_seqno_t max_drop_seq, ddsi_seqno_t seq, ddsrt_mtime_t exp, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk);
extern unsigned ddsi_whc_remove_acked_messages (struct ddsi_whc *whc, ddsi_seqno_t max_drop_seq, struct ddsi_whc_state *whcst, struct ddsi_whc_node **deferred_free_list);
extern void ddsi_whc_free_deferred_free_list (struct ddsi_whc *whc, struct ddsi_whc_node *deferred_free_list);
