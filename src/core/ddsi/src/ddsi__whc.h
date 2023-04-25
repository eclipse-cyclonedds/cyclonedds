// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__WHC_H
#define DDSI__WHC_H

#include <stddef.h>
#include "dds/ddsrt/time.h"
#include "dds/ddsi/ddsi_whc.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_serdata;
struct ddsi_tkmap_instance;
struct ddsi_whc_node; /* opaque, but currently used for deferred free lists */

/** @component whc_if */
inline ddsi_seqno_t ddsi_whc_next_seq (const struct ddsi_whc *whc, ddsi_seqno_t seq) {
  return whc->ops->next_seq (whc, seq);
}

/** @component whc_if */
inline void ddsi_whc_get_state (const struct ddsi_whc *whc, struct ddsi_whc_state *st) {
  whc->ops->get_state (whc, st);
}

/** @component whc_if */
inline bool ddsi_whc_borrow_sample (const struct ddsi_whc *whc, ddsi_seqno_t seq, struct ddsi_whc_borrowed_sample *sample) {
  return whc->ops->borrow_sample (whc, seq, sample);
}

/** @component whc_if */
inline bool ddsi_whc_borrow_sample_key (const struct ddsi_whc *whc, const struct ddsi_serdata *serdata_key, struct ddsi_whc_borrowed_sample *sample) {
  return whc->ops->borrow_sample_key (whc, serdata_key, sample);
}

/** @component whc_if */
inline void ddsi_whc_return_sample (struct ddsi_whc *whc, struct ddsi_whc_borrowed_sample *sample, bool update_retransmit_info) {
  whc->ops->return_sample (whc, sample, update_retransmit_info);
}

/** @component whc_if */
inline void ddsi_whc_sample_iter_init (const struct ddsi_whc *whc, struct ddsi_whc_sample_iter *it) {
  whc->ops->sample_iter_init (whc, it);
}

/** @component whc_if */
inline bool ddsi_whc_sample_iter_borrow_next (struct ddsi_whc_sample_iter *it, struct ddsi_whc_borrowed_sample *sample) {
  return it->c.whc->ops->sample_iter_borrow_next (it, sample);
}

/** @component whc_if */
inline void ddsi_whc_free (struct ddsi_whc *whc) {
  whc->ops->free (whc);
}

/** @component whc_if */
inline int ddsi_whc_insert (struct ddsi_whc *whc, ddsi_seqno_t max_drop_seq, ddsi_seqno_t seq, ddsrt_mtime_t exp, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk) {
  return whc->ops->insert (whc, max_drop_seq, seq, exp, serdata, tk);
}

/** @component whc_if */
inline unsigned ddsi_whc_remove_acked_messages (struct ddsi_whc *whc, ddsi_seqno_t max_drop_seq, struct ddsi_whc_state *whcst, struct ddsi_whc_node **deferred_free_list) {
  return whc->ops->remove_acked_messages (whc, max_drop_seq, whcst, deferred_free_list);
}

/** @component whc_if */
inline void ddsi_whc_free_deferred_free_list (struct ddsi_whc *whc, struct ddsi_whc_node *deferred_free_list) {
  whc->ops->free_deferred_free_list (whc, deferred_free_list);
}

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__WHC_H */
