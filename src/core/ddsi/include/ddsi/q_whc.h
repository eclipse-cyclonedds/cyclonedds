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
#ifndef Q_WHC_H
#define Q_WHC_H

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_serdata;
struct nn_plist;
struct tkmap_instance;
struct whc_node; /* opaque, but currently used for deferred free lists */
struct whc;

struct whc_borrowed_sample {
  seqno_t seq;
  struct ddsi_serdata *serdata;
  struct nn_plist *plist;
  bool unacked;
  nn_mtime_t last_rexmit_ts;
  unsigned rexmit_count;
};

struct whc_state {
  seqno_t min_seq; /* -1 if WHC empty, else > 0 */
  seqno_t max_seq; /* -1 if WHC empty, else >= min_seq */
  size_t unacked_bytes;
};
#define WHCST_ISEMPTY(whcst) ((whcst)->max_seq == -1)

/* Adjust SIZE and alignment stuff as needed: they are here simply so we can allocate
   an iter on the stack without specifying an implementation. If future changes or
   implementations require more, these can be adjusted.  An implementation should check
   things fit at compile time. */
#define WHC_SAMPLE_ITER_SIZE (2*sizeof(void *))
struct whc_sample_iter {
  union {
    char opaque[WHC_SAMPLE_ITER_SIZE];
    /* cover alignment requirements: */
    uint64_t x;
    double y;
    void *p;
  } opaque;
};

typedef seqno_t (*whc_next_seq_t)(const struct whc *whc, seqno_t seq);
typedef void (*whc_get_state_t)(const struct whc *whc, struct whc_state *st);
typedef bool (*whc_borrow_sample_t)(const struct whc *whc, seqno_t seq, struct whc_borrowed_sample *sample);
typedef bool (*whc_borrow_sample_key_t)(const struct whc *whc, const struct ddsi_serdata *serdata_key, struct whc_borrowed_sample *sample);
typedef void (*whc_return_sample_t)(struct whc *whc, struct whc_borrowed_sample *sample, bool update_retransmit_info);
typedef void (*whc_sample_iter_init_t)(const struct whc *whc, struct whc_sample_iter *it);
typedef bool (*whc_sample_iter_borrow_next_t)(struct whc_sample_iter *it, struct whc_borrowed_sample *sample);
typedef void (*whc_free_t)(struct whc *whc);

/* min_seq is lowest sequence number that must be retained because of
   reliable readers that have not acknowledged all data */
/* max_drop_seq must go soon, it's way too ugly. */
/* plist may be NULL or os_malloc'd, WHC takes ownership of plist */
typedef int (*whc_insert_t)(struct whc *whc, seqno_t max_drop_seq, seqno_t seq, struct nn_plist *plist, struct ddsi_serdata *serdata, struct tkmap_instance *tk);
typedef unsigned (*whc_downgrade_to_volatile_t)(struct whc *whc, struct whc_state *st);
typedef unsigned (*whc_remove_acked_messages_t)(struct whc *whc, seqno_t max_drop_seq, struct whc_state *whcst, struct whc_node **deferred_free_list);
typedef void (*whc_free_deferred_free_list_t)(struct whc *whc, struct whc_node *deferred_free_list);

struct whc_ops {
  whc_insert_t insert;
  whc_remove_acked_messages_t remove_acked_messages;
  whc_free_deferred_free_list_t free_deferred_free_list;
  whc_get_state_t get_state;
  whc_next_seq_t next_seq;
  whc_borrow_sample_t borrow_sample;
  whc_borrow_sample_key_t borrow_sample_key;
  whc_return_sample_t return_sample;
  whc_sample_iter_init_t sample_iter_init;
  whc_sample_iter_borrow_next_t sample_iter_borrow_next;
  whc_downgrade_to_volatile_t downgrade_to_volatile;
  whc_free_t free;
};

struct whc {
  const struct whc_ops *ops;
};

#if defined (__cplusplus)
}
#endif

#endif /* Q_WHC_H */
