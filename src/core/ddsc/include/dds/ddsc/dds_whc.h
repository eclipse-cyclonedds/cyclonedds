/*
 * Copyright(c) 2006 to 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDS__WHC_H
#define DDS__WHC_H

#if defined (__cplusplus)
extern "C" {
#endif

#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_tkmap.h"

struct ddsi_serdata;
struct ddsi_plist;
struct ddsi_tkmap_instance;
struct whc_node; /* opaque, but currently used for deferred free lists */
struct dds_whc;

struct dds_whc_borrowed_sample {
  seqno_t seq;
  struct ddsi_serdata *serdata;
  struct ddsi_plist *plist;
  bool unacked;
  ddsrt_mtime_t last_rexmit_ts;
  unsigned rexmit_count;
};

struct dds_whc_state {
  seqno_t min_seq; /* -1 if WHC empty, else > 0 */
  seqno_t max_seq; /* -1 if WHC empty, else >= min_seq */
  size_t unacked_bytes;
};
#define WHC_STATE_IS_EMPTY(whcst) ((whcst)->max_seq == -1)

/* Adjust SIZE and alignment stuff as needed: they are here simply so we can allocate
   an iter on the stack without specifying an implementation. If future changes or
   implementations require more, these can be adjusted.  An implementation should check
   things fit at compile time. */
#define WHC_SAMPLE_ITER_SIZE (7 * sizeof(void *))
struct dds_whc_sample_iter_base {
  struct dds_whc *whc;
};
struct dds_whc_sample_iter {
  struct dds_whc_sample_iter_base c;
  union {
    char opaque[WHC_SAMPLE_ITER_SIZE];
    /* cover alignment requirements: */
    uint64_t x;
    double y;
    void *p;
  } opaque;
};

typedef seqno_t (*dds_whc_next_seq_t)(const struct dds_whc *whc, seqno_t seq);
typedef void (*dds_whc_get_state_t)(const struct dds_whc *whc, struct dds_whc_state *st);
typedef bool (*dds_whc_borrow_sample_t)(const struct dds_whc *whc, seqno_t seq, struct dds_whc_borrowed_sample *sample);
typedef bool (*dds_whc_borrow_sample_key_t)(const struct dds_whc *whc, const struct ddsi_serdata *serdata_key, struct dds_whc_borrowed_sample *sample);
typedef void (*dds_whc_return_sample_t)(struct dds_whc *whc, struct dds_whc_borrowed_sample *sample, bool update_retransmit_info);
typedef void (*dds_whc_sample_iter_init_t)(const struct dds_whc *whc, struct dds_whc_sample_iter *it);
typedef bool (*dds_whc_sample_iter_borrow_next_t)(struct dds_whc_sample_iter *it, struct dds_whc_borrowed_sample *sample);
typedef void (*dds_whc_free_t)(struct dds_whc *whc);

/* min_seq is lowest sequence number that must be retained because of
   reliable readers that have not acknowledged all data */
/* max_drop_seq must go soon, it's way too ugly. */
/* plist may be NULL or ddsrt_malloc'd, WHC takes ownership of plist */
typedef int (*dds_whc_insert_t)(struct dds_whc *whc, seqno_t max_drop_seq, seqno_t seq, ddsrt_mtime_t exp, struct ddsi_plist *plist, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk);
typedef uint32_t (*dds_whc_downgrade_to_volatile_t)(struct dds_whc *whc, struct dds_whc_state *st);
typedef uint32_t (*dds_whc_remove_acked_messages_t)(struct dds_whc *whc, seqno_t max_drop_seq, struct dds_whc_state *whcst, struct whc_node **deferred_free_list);
typedef void (*dds_whc_free_deferred_free_list_t)(struct dds_whc *whc, struct whc_node *deferred_free_list);

struct dds_whc_ops {
  dds_whc_insert_t insert;
  dds_whc_remove_acked_messages_t remove_acked_messages;
  dds_whc_free_deferred_free_list_t free_deferred_free_list;
  dds_whc_get_state_t get_state;
  dds_whc_next_seq_t next_seq;
  dds_whc_borrow_sample_t borrow_sample;
  dds_whc_borrow_sample_key_t borrow_sample_key;
  dds_whc_return_sample_t return_sample;
  dds_whc_sample_iter_init_t sample_iter_init;
  dds_whc_sample_iter_borrow_next_t sample_iter_borrow_next;
  dds_whc_downgrade_to_volatile_t downgrade_to_volatile;
  dds_whc_free_t free;
};

struct dds_whc {
  const struct dds_whc_ops *ops;
};

inline seqno_t dds_whc_next_seq (const struct dds_whc *whc, seqno_t seq) {
  return whc->ops->next_seq (whc, seq);
}
inline void dds_whc_get_state (const struct dds_whc *whc, struct dds_whc_state *st) {
  whc->ops->get_state (whc, st);
}
inline bool dds_whc_borrow_sample (const struct dds_whc *whc, seqno_t seq, struct dds_whc_borrowed_sample *sample) {
  return whc->ops->borrow_sample (whc, seq, sample);
}
inline bool dds_whc_borrow_sample_key (const struct dds_whc *whc, const struct ddsi_serdata *serdata_key, struct dds_whc_borrowed_sample *sample) {
  return whc->ops->borrow_sample_key (whc, serdata_key, sample);
}
inline void dds_whc_return_sample (struct dds_whc *whc, struct dds_whc_borrowed_sample *sample, bool update_retransmit_info) {
  whc->ops->return_sample (whc, sample, update_retransmit_info);
}
inline void dds_whc_sample_iter_init (const struct dds_whc *whc, struct dds_whc_sample_iter *it) {
  whc->ops->sample_iter_init (whc, it);
}
inline bool dds_whc_sample_iter_borrow_next (struct dds_whc_sample_iter *it, struct dds_whc_borrowed_sample *sample) {
  return it->c.whc->ops->sample_iter_borrow_next (it, sample);
}
inline void dds_whc_free (struct dds_whc *whc) {
  whc->ops->free (whc);
}
inline int dds_whc_insert (struct dds_whc *whc, seqno_t max_drop_seq, seqno_t seq, ddsrt_mtime_t exp, struct ddsi_plist *plist, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk) {
  return whc->ops->insert (whc, max_drop_seq, seq, exp, plist, serdata, tk);
}
inline unsigned dds_whc_downgrade_to_volatile (struct dds_whc *whc, struct dds_whc_state *st) {
  return whc->ops->downgrade_to_volatile (whc, st);
}
inline unsigned dds_whc_remove_acked_messages (struct dds_whc *whc, seqno_t max_drop_seq, struct dds_whc_state *whcst, struct whc_node **deferred_free_list) {
  return whc->ops->remove_acked_messages (whc, max_drop_seq, whcst, deferred_free_list);
}
inline void dds_whc_free_deferred_free_list (struct dds_whc *whc, struct whc_node *deferred_free_list) {
  whc->ops->free_deferred_free_list (whc, deferred_free_list);
}

#if defined (__cplusplus)
}
#endif

#endif /* DDS__WHC_H */
