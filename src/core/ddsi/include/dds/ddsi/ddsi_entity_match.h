/*
 * Copyright(c) 2006 to 2022 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSI_ENTITY_MATCH_H
#define DDSI_ENTITY_MATCH_H

#include "dds/export.h"
#include "dds/features.h"

#include "dds/ddsrt/avl.h"
#include "dds/ddsi/q_lat_estim.h"

#if defined (__cplusplus)
extern "C" {
#endif

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
  seqno_t min_seq; /* smallest ack'd seq nr in subtree */
  seqno_t max_seq; /* sort-of highest ack'd seq nr in subtree (see augment function) */
  seqno_t seq; /* highest acknowledged seq nr */
  seqno_t last_seq; /* highest seq send to this reader used when filter is applied */
  uint32_t num_reliable_readers_where_seq_equals_max;
  ddsi_guid_t arbitrary_unacked_reader;
  nn_count_t prev_acknack; /* latest accepted acknack sequence number */
  nn_count_t prev_nackfrag; /* latest accepted nackfrag sequence number */
  ddsrt_etime_t t_acknack_accepted; /* (local) time an acknack was last accepted */
  ddsrt_etime_t t_nackfrag_accepted; /* (local) time a nackfrag was last accepted */
  struct nn_lat_estim hb_to_ack_latency;
  ddsrt_wctime_t hb_to_ack_latency_tlastlog;
  uint32_t non_responsive_count;
  uint32_t rexmit_requests;
#ifdef DDS_HAS_SECURITY
  int64_t crypto_handle;
#endif
};

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_ENTITY_MATCH_H */
