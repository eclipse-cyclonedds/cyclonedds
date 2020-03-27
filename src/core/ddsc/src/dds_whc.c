/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * sdaSPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include "dds/dds.h"
#include "dds/ddsc/dds_whc.h"

extern inline seqno_t dds_whc_next_seq (const struct dds_whc *whc, seqno_t seq);
extern inline void dds_whc_get_state (const struct dds_whc *whc, struct dds_whc_state *st);
extern inline bool dds_whc_borrow_sample (const struct dds_whc *whc, seqno_t seq, struct dds_whc_borrowed_sample *sample);
extern inline bool dds_whc_borrow_sample_key (const struct dds_whc *whc, const struct ddsi_serdata *serdata_key, struct dds_whc_borrowed_sample *sample);
extern inline void dds_whc_return_sample (struct dds_whc *whc, struct dds_whc_borrowed_sample *sample, bool update_retransmit_info);
extern inline void dds_whc_sample_iter_init (const struct dds_whc *whc, struct dds_whc_sample_iter *it);
extern inline bool dds_whc_sample_iter_borrow_next (struct dds_whc_sample_iter *it, struct dds_whc_borrowed_sample *sample);
extern inline void dds_whc_free (struct dds_whc *whc);
extern int dds_whc_insert (struct dds_whc *whc, seqno_t max_drop_seq, seqno_t seq, ddsrt_mtime_t exp, struct ddsi_plist *plist, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk);
extern unsigned dds_whc_downgrade_to_volatile (struct dds_whc *whc, struct dds_whc_state *st);
extern unsigned dds_whc_remove_acked_messages (struct dds_whc *whc, seqno_t max_drop_seq, struct dds_whc_state *whcst, struct whc_node **deferred_free_list);
extern void dds_whc_free_deferred_free_list (struct dds_whc *whc, struct whc_node *deferred_free_list);