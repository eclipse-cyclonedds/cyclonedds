// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_WHC_H
#define DDSI_WHC_H

#include <stddef.h>
#include "dds/ddsrt/time.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_serdata;
struct ddsi_plist;
struct ddsi_tkmap_instance;
struct ddsi_whc;

/**
 * @brief Base type for whc node
 */
struct ddsi_whc_node {
  ddsi_seqno_t seq;
};

struct ddsi_whc_borrowed_sample {
  ddsi_seqno_t seq;
  struct ddsi_serdata *serdata;
  bool unacked;
  ddsrt_mtime_t last_rexmit_ts;
  unsigned rexmit_count;
};

struct ddsi_whc_state {
  ddsi_seqno_t min_seq; /* 0 if WHC empty, else > 0 */
  ddsi_seqno_t max_seq; /* 0 if WHC empty, else >= min_seq */
  size_t unacked_bytes;
};
#define DDSI_WHCST_ISEMPTY(whcst) ((whcst)->max_seq == 0)

/* Adjust SIZE and alignment stuff as needed: they are here simply so we can allocate
   an iter on the stack without specifying an implementation. If future changes or
   implementations require more, these can be adjusted.  An implementation should check
   things fit at compile time. */
#define DDSI_WHC_SAMPLE_ITER_SIZE (8 * sizeof(void *))
struct ddsi_whc_sample_iter_base {
  struct ddsi_whc *whc;
};
struct ddsi_whc_sample_iter {
  struct ddsi_whc_sample_iter_base c;
  union {
    char opaque[DDSI_WHC_SAMPLE_ITER_SIZE];
    /* cover alignment requirements: */
    uint64_t x;
    double y;
    void *p;
  } opaque;
};

typedef ddsi_seqno_t (*ddsi_whc_next_seq_t)(const struct ddsi_whc *whc, ddsi_seqno_t seq);
typedef void (*ddsi_whc_get_state_t)(const struct ddsi_whc *whc, struct ddsi_whc_state *st);
typedef bool (*ddsi_whc_borrow_sample_t)(const struct ddsi_whc *whc, ddsi_seqno_t seq, struct ddsi_whc_borrowed_sample *sample);
typedef bool (*ddsi_whc_borrow_sample_key_t)(const struct ddsi_whc *whc, const struct ddsi_serdata *serdata_key, struct ddsi_whc_borrowed_sample *sample);
typedef void (*ddsi_whc_return_sample_t)(struct ddsi_whc *whc, struct ddsi_whc_borrowed_sample *sample, bool update_retransmit_info);
typedef void (*ddsi_whc_sample_iter_init_t)(const struct ddsi_whc *whc, struct ddsi_whc_sample_iter *it);
typedef bool (*ddsi_whc_sample_iter_borrow_next_t)(struct ddsi_whc_sample_iter *it, struct ddsi_whc_borrowed_sample *sample);
typedef void (*whc_free_t)(struct ddsi_whc *whc);

/* min_seq is lowest sequence number that must be retained because of
   reliable readers that have not acknowledged all data */
/* max_drop_seq must go soon, it's way too ugly. */
/* plist may be NULL or ddsrt_malloc'd, WHC takes ownership of plist */
typedef int (*ddsi_whc_insert_t)(struct ddsi_whc *whc, ddsi_seqno_t max_drop_seq, ddsi_seqno_t seq, ddsrt_mtime_t exp, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk);
typedef uint32_t (*ddsi_whc_remove_acked_messages_t)(struct ddsi_whc *whc, ddsi_seqno_t max_drop_seq, struct ddsi_whc_state *whcst, struct ddsi_whc_node **deferred_free_list);
typedef void (*ddsi_whc_free_deferred_free_list_t)(struct ddsi_whc *whc, struct ddsi_whc_node *deferred_free_list);

struct ddsi_whc_ops {
  ddsi_whc_insert_t insert;
  ddsi_whc_remove_acked_messages_t remove_acked_messages;
  ddsi_whc_free_deferred_free_list_t free_deferred_free_list;
  ddsi_whc_get_state_t get_state;
  ddsi_whc_next_seq_t next_seq;
  ddsi_whc_borrow_sample_t borrow_sample;
  ddsi_whc_borrow_sample_key_t borrow_sample_key;
  ddsi_whc_return_sample_t return_sample;
  ddsi_whc_sample_iter_init_t sample_iter_init;
  ddsi_whc_sample_iter_borrow_next_t sample_iter_borrow_next;
  whc_free_t free;
};

struct ddsi_whc {
  const struct ddsi_whc_ops *ops;
};

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_WHC_H */
