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

#include "util/ut_avl.h"
#include "util/ut_hopscotch.h"
#include "ddsi/q_time.h"
#include "ddsi/q_rtps.h"
#include "ddsi/q_freelist.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct serdata;
struct nn_plist;
struct whc_idxnode;

#define USE_EHH 0

struct whc_node {
  struct whc_node *next_seq; /* next in this interval */
  struct whc_node *prev_seq; /* prev in this interval */
  struct whc_idxnode *idxnode; /* NULL if not in index */
  unsigned idxnode_pos; /* index in idxnode.hist */
  seqno_t seq;
  uint64_t total_bytes; /* cumulative number of bytes up to and including this node */
  size_t size;
  struct nn_plist *plist; /* 0 if nothing special */
  unsigned unacked: 1; /* counted in whc::unacked_bytes iff 1 */
  nn_mtime_t last_rexmit_ts;
  unsigned rexmit_count;
  struct serdata *serdata;
};

struct whc_intvnode {
  ut_avlNode_t avlnode;
  seqno_t min;
  seqno_t maxp1;
  struct whc_node *first; /* linked list of seqs with contiguous sequence numbers [min,maxp1) */
  struct whc_node *last; /* valid iff first != NULL */
};

struct whc_idxnode {
  int64_t iid;
  seqno_t prune_seq;
  struct tkmap_instance *tk;
  unsigned headidx;
#if __STDC_VERSION__ >= 199901L
  struct whc_node *hist[];
#else
  struct whc_node *hist[1];
#endif
};

#if USE_EHH
struct whc_seq_entry {
  seqno_t seq;
  struct whc_node *whcn;
};
#endif

struct whc {
  unsigned seq_size;
  size_t unacked_bytes;
  size_t sample_overhead;
  uint64_t total_bytes; /* total number of bytes pushed in */
  unsigned is_transient_local: 1;
  unsigned hdepth; /* 0 = unlimited */
  unsigned tldepth; /* 0 = disabled/unlimited (no need to maintain an index if KEEP_ALL <=> is_transient_local + tldepth=0) */
  unsigned idxdepth; /* = max(hdepth, tldepth) */
  seqno_t max_drop_seq; /* samples in whc with seq <= max_drop_seq => transient-local */
  struct whc_intvnode *open_intv; /* interval where next sample will go (usually) */
  struct whc_node *maxseq_node; /* NULL if empty; if not in open_intv, open_intv is empty */
  struct nn_freelist freelist; /* struct whc_node *; linked via whc_node::next_seq */
#if USE_EHH
  struct ut_ehh *seq_hash;
#else
  struct ut_hh *seq_hash;
#endif
  struct ut_hh *idx_hash;
  ut_avlTree_t seq;
};

struct whc *whc_new (int is_transient_local, unsigned hdepth, unsigned tldepth, size_t sample_overhead);
void whc_free (struct whc *whc);
int whc_empty (const struct whc *whc);
seqno_t whc_min_seq (const struct whc *whc);
seqno_t whc_max_seq (const struct whc *whc);
seqno_t whc_next_seq (const struct whc *whc, seqno_t seq);
size_t whc_unacked_bytes (struct whc *whc);

struct whc_node *whc_findseq (const struct whc *whc, seqno_t seq);
struct whc_node *whc_findmax (const struct whc *whc);
struct whc_node *whc_findkey (const struct whc *whc, const struct serdata *serdata_key);

struct whc_node *whc_next_node (const struct whc *whc, seqno_t seq);

/* min_seq is lowest sequence number that must be retained because of
   reliable readers that have not acknowledged all data */
/* max_drop_seq must go soon, it's way too ugly. */
/* plist may be NULL or os_malloc'd, WHC takes ownership of plist */
int whc_insert (struct whc *whc, seqno_t max_drop_seq, seqno_t seq, struct nn_plist *plist, struct serdata *serdata, struct tkmap_instance *tk);
void whc_downgrade_to_volatile (struct whc *whc);
unsigned whc_remove_acked_messages (struct whc *whc, seqno_t max_drop_seq, struct whc_node **deferred_free_list);
void whc_free_deferred_free_list (struct whc *whc, struct whc_node *deferred_free_list);

#if defined (__cplusplus)
}
#endif

#endif /* Q_WHC_H */
