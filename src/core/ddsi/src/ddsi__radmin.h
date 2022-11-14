/*
 * Copyright(c) 2006 to 2021 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSI__RADMIN_H
#define DDSI__RADMIN_H

#include <stddef.h>

#include "dds/ddsrt/time.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/align.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsi/ddsi_locator.h"
#include "dds/ddsi/q_rtps.h"
#include "dds/ddsi/ddsi_protocol.h"
#include "dds/ddsi/ddsi_radmin.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_rbufpool;
struct ddsi_rbuf;
struct ddsi_rmsg;
struct ddsi_rdata;
struct ddsi_rsample;
struct ddsi_rsample_chain;
struct ddsi_rsample_info;
struct ddsi_defrag;
struct ddsi_reorder;
struct ddsi_dqueue;
struct ddsi_guid;
struct ddsi_tran_conn;
struct ddsi_proxy_writer;
struct ddsrt_log_cfg;
struct ddsi_fragment_number_set_header;
struct ddsi_sequence_number_set_header;

/* Allocated inside a chunk of memory by a custom allocator and requires >= 8-byte alignment */
#define DDSI_ALIGNOF_RMSG (dds_alignof(struct ddsi_rmsg) > 8 ? dds_alignof(struct ddsi_rmsg) : 8)

typedef int (*ddsi_dqueue_handler_t) (const struct ddsi_rsample_info *sampleinfo, const struct ddsi_rdata *fragchain, const struct ddsi_guid *rdguid, void *qarg);

struct ddsi_receiver_state {
  ddsi_guid_prefix_t src_guid_prefix;     /* 12 */
  ddsi_guid_prefix_t dst_guid_prefix;     /* 12 */
  struct ddsi_addrset *reply_locators;         /* 4/8 */
  uint32_t forme:1;                       /* 4 */
  uint32_t rtps_encoded:1;                /* - */
  ddsi_vendorid_t vendor;                   /* 2 */
  nn_protocol_version_t protocol_version; /* 2 => 44/48 */
  struct ddsi_tran_conn *conn;            /* Connection for request */
  ddsi_locator_t srcloc;
  struct ddsi_domaingv *gv;
};

struct ddsi_rsample_info {
  seqno_t seq;
  struct ddsi_receiver_state *rst;
  struct ddsi_proxy_writer *pwr;
  uint32_t size;
  uint32_t fragsize;
  ddsrt_wctime_t timestamp;
  ddsrt_wctime_t reception_timestamp; /* OpenSplice extension -- but we get it essentially for free, so why not? */
  unsigned statusinfo: 2;       /* just the two defined bits from the status info */
  unsigned bswap: 1;            /* so we can extract well formatted writer info quicker */
  unsigned complex_qos: 1;      /* includes QoS other than keyhash, 2-bit statusinfo, PT writer info */
};

struct ddsi_rsample_chain_elem {
  /* FIXME: evidently smaller than a defrag_iv, but maybe better to
     merge it with defrag_iv in a union anyway. */
  struct ddsi_rdata *fragchain;
  struct ddsi_rsample_chain_elem *next;
  /* Gaps have sampleinfo = NULL, but nonetheless a fragchain with 1
     rdata with min=maxp1 (length 0) and valid rmsg pointer.  And (see
     DQUEUE) its lsb gets abused so we can queue "bubbles" in addition
     to data). */
  struct ddsi_rsample_info *sampleinfo;
};

struct ddsi_rsample_chain {
  struct ddsi_rsample_chain_elem *first;
  struct ddsi_rsample_chain_elem *last;
};

enum ddsi_reorder_mode {
  DDSI_REORDER_MODE_NORMAL,
  DDSI_REORDER_MODE_MONOTONICALLY_INCREASING,
  DDSI_REORDER_MODE_ALWAYS_DELIVER
};

enum ddsi_defrag_drop_mode {
  DDSI_DEFRAG_DROP_OLDEST,        /* (believed to be) best for unreliable */
  DDSI_DEFRAG_DROP_LATEST         /* (...) best for reliable  */
};

typedef int32_t ddsi_reorder_result_t;
/* typedef of reorder result serves as a warning that it is to be
   interpreted as follows: */
/* REORDER_DELIVER > 0 -- number of samples in sample chain */
#define DDSI_REORDER_ACCEPT        0 /* accepted/stored (for gap: also adjusted next_expected) */
#define DDSI_REORDER_TOO_OLD      -1 /* discarded because it was too old */
#define DDSI_REORDER_REJECT       -2 /* caller may reuse memory ("real" reject for data, "fake" for gap) */

typedef void (*ddsi_dqueue_callback_t) (void *arg);

enum ddsi_defrag_nackmap_result {
  DDSI_DEFRAG_NACKMAP_UNKNOWN_SAMPLE,
  DDSI_DEFRAG_NACKMAP_ALL_ADVERTISED_FRAGMENTS_KNOWN,
  DDSI_DEFRAG_NACKMAP_FRAGMENTS_MISSING
};

struct ddsi_rbufpool *ddsi_rbufpool_new (const struct ddsrt_log_cfg *logcfg, uint32_t rbuf_size, uint32_t max_rmsg_size);
void ddsi_rbufpool_setowner (struct ddsi_rbufpool *rbp, ddsrt_thread_t tid);
void ddsi_rbufpool_free (struct ddsi_rbufpool *rbp);

struct ddsi_rmsg *ddsi_rmsg_new (struct ddsi_rbufpool *rbufpool);
void ddsi_rmsg_setsize (struct ddsi_rmsg *rmsg, uint32_t size);
void ddsi_rmsg_commit (struct ddsi_rmsg *rmsg);
void ddsi_rmsg_free (struct ddsi_rmsg *rmsg);
void *ddsi_rmsg_alloc (struct ddsi_rmsg *rmsg, uint32_t size);

struct ddsi_rdata *ddsi_rdata_new (struct ddsi_rmsg *rmsg, uint32_t start, uint32_t endp1, uint32_t submsg_offset, uint32_t payload_offset, uint32_t keyhash_offset);
struct ddsi_rdata *ddsi_rdata_newgap (struct ddsi_rmsg *rmsg);
void ddsi_fragchain_adjust_refcount (struct ddsi_rdata *frag, int adjust);
void ddsi_fragchain_unref (struct ddsi_rdata *frag);

struct ddsi_defrag *ddsi_defrag_new (const struct ddsrt_log_cfg *logcfg, enum ddsi_defrag_drop_mode drop_mode, uint32_t max_samples);
void ddsi_defrag_free (struct ddsi_defrag *defrag);
struct ddsi_rsample *ddsi_defrag_rsample (struct ddsi_defrag *defrag, struct ddsi_rdata *rdata, const struct ddsi_rsample_info *sampleinfo);
void ddsi_defrag_notegap (struct ddsi_defrag *defrag, seqno_t min, seqno_t maxp1);
enum ddsi_defrag_nackmap_result ddsi_defrag_nackmap (struct ddsi_defrag *defrag, seqno_t seq, uint32_t maxfragnum, struct ddsi_fragment_number_set_header *map, uint32_t *mapbits, uint32_t maxsz);
void ddsi_defrag_prune (struct ddsi_defrag *defrag, ddsi_guid_prefix_t *dst, seqno_t min);

struct ddsi_reorder *ddsi_reorder_new (const struct ddsrt_log_cfg *logcfg, enum ddsi_reorder_mode mode, uint32_t max_samples, bool late_ack_mode);
void ddsi_reorder_free (struct ddsi_reorder *r);
struct ddsi_rsample *ddsi_reorder_rsample_dup_first (struct ddsi_rmsg *rmsg, struct ddsi_rsample *rsampleiv);
struct ddsi_rdata *ddsi_rsample_fragchain (struct ddsi_rsample *rsample);
ddsi_reorder_result_t ddsi_reorder_rsample (struct ddsi_rsample_chain *sc, struct ddsi_reorder *reorder, struct ddsi_rsample *rsampleiv, int *refcount_adjust, int delivery_queue_full_p);
ddsi_reorder_result_t ddsi_reorder_gap (struct ddsi_rsample_chain *sc, struct ddsi_reorder *reorder, struct ddsi_rdata *rdata, seqno_t min, seqno_t maxp1, int *refcount_adjust);
void ddsi_reorder_drop_upto (struct ddsi_reorder *reorder, seqno_t maxp1); // drops [1,maxp1); next_seq' = maxp1
int ddsi_reorder_wantsample (const struct ddsi_reorder *reorder, seqno_t seq);
unsigned ddsi_reorder_nackmap (const struct ddsi_reorder *reorder, seqno_t base, seqno_t maxseq, struct ddsi_sequence_number_set_header *map, uint32_t *mapbits, uint32_t maxsz, int notail);
seqno_t ddsi_reorder_next_seq (const struct ddsi_reorder *reorder);
void ddsi_reorder_set_next_seq (struct ddsi_reorder *reorder, seqno_t seq);

struct ddsi_dqueue *ddsi_dqueue_new (const char *name, const struct ddsi_domaingv *gv, uint32_t max_samples, ddsi_dqueue_handler_t handler, void *arg);
bool ddsi_dqueue_start (struct ddsi_dqueue *q);
void ddsi_dqueue_free (struct ddsi_dqueue *q);
bool ddsi_dqueue_enqueue_deferred_wakeup (struct ddsi_dqueue *q, struct ddsi_rsample_chain *sc, ddsi_reorder_result_t rres);
void ddsi_dqueue_enqueue_trigger (struct ddsi_dqueue *q);
void ddsi_dqueue_enqueue (struct ddsi_dqueue *q, struct ddsi_rsample_chain *sc, ddsi_reorder_result_t rres);
void ddsi_dqueue_enqueue1 (struct ddsi_dqueue *q, const ddsi_guid_t *rdguid, struct ddsi_rsample_chain *sc, ddsi_reorder_result_t rres);
void ddsi_dqueue_enqueue_callback (struct ddsi_dqueue *q, ddsi_dqueue_callback_t cb, void *arg);
int ddsi_dqueue_is_full (struct ddsi_dqueue *q);
void ddsi_dqueue_wait_until_empty_if_full (struct ddsi_dqueue *q);

void ddsi_defrag_stats (struct ddsi_defrag *defrag, uint64_t *discarded_bytes);
void ddsi_reorder_stats (struct ddsi_reorder *reorder, uint64_t *discarded_bytes);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__RADMIN_H */
