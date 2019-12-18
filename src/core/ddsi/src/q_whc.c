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
#include "dds/ddsi/q_rtps.h"
#include "dds/ddsi/q_time.h"
#include "dds/ddsi/q_whc.h"

extern inline seqno_t whc_next_seq (const struct whc *whc, seqno_t seq);
extern inline void whc_get_state (const struct whc *whc, struct whc_state *st);
extern inline bool whc_borrow_sample (const struct whc *whc, seqno_t seq, struct whc_borrowed_sample *sample);
extern inline bool whc_borrow_sample_key (const struct whc *whc, const struct ddsi_serdata *serdata_key, struct whc_borrowed_sample *sample);
extern inline void whc_return_sample (struct whc *whc, struct whc_borrowed_sample *sample, bool update_retransmit_info);
extern inline void whc_sample_iter_init (const struct whc *whc, struct whc_sample_iter *it);
extern inline bool whc_sample_iter_borrow_next (struct whc_sample_iter *it, struct whc_borrowed_sample *sample);
extern inline void whc_free (struct whc *whc);
extern int whc_insert (struct whc *whc, seqno_t max_drop_seq, seqno_t seq, nn_mtime_t exp, struct nn_plist *plist, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk);
extern unsigned whc_downgrade_to_volatile (struct whc *whc, struct whc_state *st);
extern unsigned whc_remove_acked_messages (struct whc *whc, seqno_t max_drop_seq, struct whc_state *whcst, struct whc_node **deferred_free_list);
extern void whc_free_deferred_free_list (struct whc *whc, struct whc_node *deferred_free_list);
