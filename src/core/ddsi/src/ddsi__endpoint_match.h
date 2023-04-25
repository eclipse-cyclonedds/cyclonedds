// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__ENDPOINT_MATCH_H
#define DDSI__ENDPOINT_MATCH_H

#include "dds/export.h"
#include "dds/features.h"

#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_endpoint_match.h"
#include "ddsi__handshake.h"
#include "ddsi__addrset.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_participant;
struct ddsi_proxy_participant;
struct ddsi_writer;
struct ddsi_reader;
struct ddsi_proxy_writer;
struct ddsi_proxy_reader;
struct ddsi_alive_state;
struct ddsi_generic_proxy_endpoint;

struct ddsi_bestab {
  unsigned besflag;
  unsigned entityid;
  const char *topic_name;
};

#ifdef DDS_HAS_SECURITY
struct ddsi_setab {
  enum ddsi_entity_kind kind;
  uint32_t id;
};
#endif

enum ddsi_pwr_rd_match_syncstate {
  PRMSS_SYNC, /* in sync with proxy writer, has caught up with historical data */
  PRMSS_TLCATCHUP, /* in sync with proxy writer, pwr + readers still catching up on historical data */
  PRMSS_OUT_OF_SYNC /* not in sync with proxy writer */
};

struct ddsi_last_nack_summary {
  ddsi_seqno_t seq_end_p1; /* last seq for which we requested a retransmit */
  ddsi_seqno_t seq_base;
  uint32_t frag_end_p1; /* last fragnum of seq_last_nack for which requested a retransmit */
  uint32_t frag_base;
};

struct ddsi_rd_pwr_match {
  ddsrt_avl_node_t avlnode;
  ddsi_guid_t pwr_guid;
  unsigned pwr_alive: 1; /* tracks pwr's alive state */
  uint32_t pwr_alive_vclock; /* used to ensure progress */
#ifdef DDS_HAS_SSM
  ddsi_xlocator_t ssm_mc_loc;
  ddsi_xlocator_t ssm_src_loc;
#endif
#ifdef DDS_HAS_SECURITY
  int64_t crypto_handle;
#endif
};

struct ddsi_wr_rd_match {
  ddsrt_avl_node_t avlnode;
  ddsi_guid_t rd_guid;
};

struct ddsi_rd_wr_match {
  ddsrt_avl_node_t avlnode;
  ddsi_guid_t wr_guid;
  unsigned wr_alive: 1; /* tracks wr's alive state */
  uint32_t wr_alive_vclock; /* used to ensure progress */
};

struct ddsi_wr_prd_match {
  ddsrt_avl_node_t avlnode;
  ddsi_guid_t prd_guid; /* guid of the proxy reader */
  unsigned assumed_in_sync: 1; /* set to 1 upon receipt of ack not nack'ing msgs */
  unsigned has_replied_to_hb: 1; /* we must keep sending HBs until all readers have this set */
  unsigned all_have_replied_to_hb: 1; /* true iff 'has_replied_to_hb' for all readers in subtree */
  unsigned is_reliable: 1; /* true iff reliable proxy reader */
  ddsi_seqno_t min_seq; /* smallest ack'd seq nr in subtree */
  ddsi_seqno_t max_seq; /* sort-of highest ack'd seq nr in subtree (see augment function) */
  ddsi_seqno_t seq; /* highest acknowledged seq nr */
  ddsi_seqno_t last_seq; /* highest seq send to this reader used when filter is applied */
  uint32_t num_reliable_readers_where_seq_equals_max;
  ddsi_guid_t arbitrary_unacked_reader;
  ddsi_count_t prev_acknack; /* latest accepted acknack sequence number */
  ddsi_count_t prev_nackfrag; /* latest accepted nackfrag sequence number */
  ddsrt_etime_t t_acknack_accepted; /* (local) time an acknack was last accepted */
  ddsrt_etime_t t_nackfrag_accepted; /* (local) time a nackfrag was last accepted */
  struct ddsi_lat_estim hb_to_ack_latency;
  ddsrt_wctime_t hb_to_ack_latency_tlastlog;
  uint32_t non_responsive_count;
  uint32_t rexmit_requests;
#ifdef DDS_HAS_SECURITY
  int64_t crypto_handle;
#endif
};

struct ddsi_prd_wr_match {
  ddsrt_avl_node_t avlnode;
  ddsi_guid_t wr_guid;
#ifdef DDS_HAS_SECURITY
  int64_t crypto_handle;
#endif
};

struct ddsi_pwr_rd_match {
  ddsrt_avl_node_t avlnode;
  ddsi_guid_t rd_guid;
  ddsrt_mtime_t tcreate;
  ddsi_count_t count; /* most recent acknack sequence number */
  ddsi_count_t prev_heartbeat; /* latest accepted heartbeat (see also add_proxy_writer_to_reader) */
  ddsrt_wctime_t hb_timestamp; /* time of most recent heartbeat that rescheduled the ack event */
  ddsrt_etime_t t_heartbeat_accepted; /* (local) time a heartbeat was last accepted */
  ddsrt_mtime_t t_last_nack; /* (local) time we last sent a NACK */
  ddsrt_mtime_t t_last_ack; /* (local) time we last sent any ACKNACK */
  ddsi_seqno_t last_seq; /* last known sequence number from this writer */
  struct ddsi_last_nack_summary last_nack;
  struct ddsi_xevent *acknack_xevent; /* entry in xevent queue for sending acknacks */
  enum ddsi_pwr_rd_match_syncstate in_sync; /* whether in sync with the proxy writer */
  unsigned ack_requested : 1; /* set on receipt of HEARTBEAT with FINAL clear, cleared on sending an ACKNACK */
  unsigned heartbeat_since_ack : 1; /* set when a HEARTBEAT has been received since the last ACKNACK */
  unsigned heartbeatfrag_since_ack : 1; /* set when a HEARTBEATFRAG has been received since the last ACKNACK */
  unsigned directed_heartbeat : 1; /* set on receipt of a directed heartbeat, cleared on sending an ACKNACK */
  unsigned nack_sent_on_nackdelay : 1; /* set when the most recent NACK sent was because of the NackDelay  */
  unsigned filtered : 1;
  union {
    struct {
      ddsi_seqno_t end_of_tl_seq; /* when seq >= end_of_tl_seq, it's in sync, =0 when not tl */
      struct ddsi_reorder *reorder; /* can be done (mostly) per proxy writer, but that is harder; only when state=OUT_OF_SYNC */
    } not_in_sync;
  } u;
#ifdef DDS_HAS_SECURITY
  int64_t crypto_handle;
#endif
};

/** @component endpoint_matching */
void ddsi_connect_writer_with_proxy_reader_secure (struct ddsi_writer *wr, struct ddsi_proxy_reader *prd, ddsrt_mtime_t tnow, int64_t crypto_handle);

/** @component endpoint_matching */
void ddsi_connect_reader_with_proxy_writer_secure (struct ddsi_reader *rd, struct ddsi_proxy_writer *pwr, ddsrt_mtime_t tnow, int64_t crypto_handle);

/** @component endpoint_matching */
void ddsi_match_writer_with_proxy_readers (struct ddsi_writer *wr, ddsrt_mtime_t tnow);

/** @component endpoint_matching */
void ddsi_match_writer_with_local_readers (struct ddsi_writer *wr, ddsrt_mtime_t tnow);

/** @component endpoint_matching */
void ddsi_match_reader_with_proxy_writers (struct ddsi_reader *rd, ddsrt_mtime_t tnow);

/** @component endpoint_matching */
void ddsi_match_reader_with_local_writers (struct ddsi_reader *rd, ddsrt_mtime_t tnow);

/** @component endpoint_matching */
void ddsi_match_proxy_writer_with_readers (struct ddsi_proxy_writer *pwr, ddsrt_mtime_t tnow);

/** @component endpoint_matching */
void ddsi_match_proxy_reader_with_writers (struct ddsi_proxy_reader *prd, ddsrt_mtime_t tnow);

/** @component endpoint_matching */
void ddsi_free_wr_prd_match (const struct ddsi_domaingv *gv, const ddsi_guid_t *wr_guid, struct ddsi_wr_prd_match *m);

/** @component endpoint_matching */
void ddsi_free_rd_pwr_match (struct ddsi_domaingv *gv, const ddsi_guid_t *rd_guid, struct ddsi_rd_pwr_match *m);

/** @component endpoint_matching */
void ddsi_free_pwr_rd_match (struct ddsi_pwr_rd_match *m);

/** @component endpoint_matching */
void ddsi_free_prd_wr_match (struct ddsi_prd_wr_match *m);

/** @component endpoint_matching */
void ddsi_free_rd_wr_match (struct ddsi_rd_wr_match *m);

/** @component endpoint_matching */
void ddsi_free_wr_rd_match (struct ddsi_wr_rd_match *m);

/** @component endpoint_matching */
void ddsi_writer_add_connection (struct ddsi_writer *wr, struct ddsi_proxy_reader *prd, int64_t crypto_handle);

/** @component endpoint_matching */
void ddsi_writer_add_local_connection (struct ddsi_writer *wr, struct ddsi_reader *rd);

/** @component endpoint_matching */
void ddsi_reader_add_connection (struct ddsi_reader *rd, struct ddsi_proxy_writer *pwr, ddsi_count_t *init_count, const struct ddsi_alive_state *alive_state, int64_t crypto_handle);

/** @component endpoint_matching */
void ddsi_reader_add_local_connection (struct ddsi_reader *rd, struct ddsi_writer *wr, const struct ddsi_alive_state *alive_state);

/** @component endpoint_matching */
void ddsi_proxy_writer_add_connection (struct ddsi_proxy_writer *pwr, struct ddsi_reader *rd, ddsrt_mtime_t tnow, ddsi_count_t init_count, int64_t crypto_handle);

/** @component endpoint_matching */
void ddsi_proxy_reader_add_connection (struct ddsi_proxy_reader *prd, struct ddsi_writer *wr, int64_t crypto_handle);

/** @component endpoint_matching */
void ddsi_writer_drop_connection (const struct ddsi_guid *wr_guid, const struct ddsi_proxy_reader *prd);

/** @component endpoint_matching */
void ddsi_writer_drop_local_connection (const struct ddsi_guid *wr_guid, struct ddsi_reader *rd);

/** @component endpoint_matching */
void ddsi_reader_drop_connection (const struct ddsi_guid *rd_guid, const struct ddsi_proxy_writer *pwr);

/** @component endpoint_matching */
void ddsi_reader_drop_local_connection (const struct ddsi_guid *rd_guid, const struct ddsi_writer *wr);

/** @component endpoint_matching */
void ddsi_proxy_writer_drop_connection (const struct ddsi_guid *pwr_guid, struct ddsi_reader *rd);

/** @component endpoint_matching */
void ddsi_proxy_reader_drop_connection (const struct ddsi_guid *prd_guid, struct ddsi_writer *wr);

/** @component endpoint_matching */
void ddsi_local_reader_ary_init (struct ddsi_local_reader_ary *x);

/** @component endpoint_matching */
void ddsi_local_reader_ary_fini (struct ddsi_local_reader_ary *x);

/** @component endpoint_matching */
void ddsi_local_reader_ary_setinvalid (struct ddsi_local_reader_ary *x);

/** @component endpoint_matching */
void ddsi_local_reader_ary_insert (struct ddsi_local_reader_ary *x, struct ddsi_reader *rd);

/** @component endpoint_matching */
void ddsi_local_reader_ary_remove (struct ddsi_local_reader_ary *x, struct ddsi_reader *rd);

/** @component endpoint_matching */
void ddsi_local_reader_ary_setfastpath_ok (struct ddsi_local_reader_ary *x, bool fastpath_ok);


#ifdef DDS_HAS_SECURITY

/** @component endpoint_matching */
void ddsi_handshake_end_cb (struct ddsi_handshake *handshake, struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp, enum ddsi_handshake_state result);

/** @component endpoint_matching */
bool ddsi_proxy_participant_has_pp_match (struct ddsi_domaingv *gv, struct ddsi_proxy_participant *proxypp);

/** @component endpoint_matching */
void ddsi_proxy_participant_create_handshakes (struct ddsi_domaingv *gv, struct ddsi_proxy_participant *proxypp);

/** @component endpoint_matching */
void ddsi_disconnect_proxy_participant_secure (struct ddsi_proxy_participant *proxypp);

/** @component endpoint_matching */
void ddsi_match_volatile_secure_endpoints (struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp);

/** @component endpoint_matching */
void ddsi_update_proxy_participant_endpoint_matching (struct ddsi_proxy_participant *proxypp, struct ddsi_participant *pp);

#endif /* DDS_HAS_SECURITY */

/** @component endpoint_matching */
void ddsi_update_proxy_endpoint_matching (const struct ddsi_domaingv *gv, struct ddsi_generic_proxy_endpoint *proxy_ep);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__ENDPOINT_MATCH_H */
