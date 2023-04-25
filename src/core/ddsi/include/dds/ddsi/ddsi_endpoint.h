// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_ENDPOINT_H
#define DDSI_ENDPOINT_H

#include "dds/export.h"
#include "dds/features.h"

#include "dds/ddsrt/fibheap.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_hbcontrol.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_participant;
struct ddsi_type_pair;
struct ddsi_writer_info;
struct ddsi_entity_common;
struct ddsi_endpoint_common;
struct ddsi_ldur_fhnode;
struct ddsi_entity_index;
struct dds_qos;

/* Liveliness changed is more complicated than just add/remove. Encode the event
   in ddsi_status_cb_data_t::extra and ignore ddsi_status_cb_data_t::add */
enum ddsi_liveliness_changed_data_extra {
  DDSI_LIVELINESS_CHANGED_ADD_ALIVE,
  DDSI_LIVELINESS_CHANGED_ADD_NOT_ALIVE,
  DDSI_LIVELINESS_CHANGED_REMOVE_NOT_ALIVE,
  DDSI_LIVELINESS_CHANGED_REMOVE_ALIVE,
  DDSI_LIVELINESS_CHANGED_ALIVE_TO_NOT_ALIVE,
  DDSI_LIVELINESS_CHANGED_NOT_ALIVE_TO_ALIVE
};

struct ddsi_endpoint_common {
  struct ddsi_participant *pp;
  ddsi_guid_t group_guid;
#ifdef DDS_HAS_TYPE_DISCOVERY
  struct ddsi_type_pair *type_pair;
#endif
};

enum ddsi_writer_state {
  WRST_OPERATIONAL, /* normal situation */
  WRST_INTERRUPT, /* will be deleted, unblock throttle_writer but do not do anything further */
  WRST_LINGERING, /* writer deletion has been requested but still has unack'd data */
  WRST_DELETING /* writer is actually being deleted (removed from hash table) */
};

typedef ddsrt_atomic_uint64_t seq_xmit_t;

struct ddsi_writer
{
  struct ddsi_entity_common e;
  struct ddsi_endpoint_common c;
  ddsi_status_cb_t status_cb;
  void * status_cb_entity;
  ddsrt_cond_t throttle_cond; /* used to trigger a transmit thread blocked in throttle_writer() or wait_for_acks() */
  ddsi_seqno_t seq; /* last sequence number (transmitted seqs are 1 ... seq, 0 when nothing published yet) */
  seq_xmit_t seq_xmit; /* last sequence number actually transmitted */
  ddsi_seqno_t min_local_readers_reject_seq; /* mimum of local_readers->last_deliv_seq */
  ddsi_count_t hbcount; /* last hb seq number */
  ddsi_count_t hbfragcount; /* last hb frag seq number */
  int throttling; /* non-zero when some thread is waiting for the WHC to shrink */
  struct ddsi_hbcontrol hbcontrol; /* controls heartbeat timing, piggybacking */
  struct dds_qos *xqos;
  enum ddsi_writer_state state;
  unsigned reliable: 1; /* iff 1, writer is reliable <=> heartbeat_xevent != NULL */
  unsigned handle_as_transient_local: 1; /* controls whether data is retained in WHC */
  unsigned force_md5_keyhash: 1; /* iff 1, when keyhash has to be hashed, no matter the size */
  unsigned retransmitting: 1; /* iff 1, this writer is currently retransmitting */
  unsigned alive: 1; /* iff 1, the writer is alive (lease for this writer is not expired); field may be modified only when holding both wr->e.lock and wr->c.pp->e.lock */
  unsigned test_ignore_acknack : 1; /* iff 1, the writer ignores all arriving ACKNACK messages */
  unsigned test_suppress_retransmit : 1; /* iff 1, the writer does not respond to retransmit requests */
  unsigned test_suppress_heartbeat : 1; /* iff 1, the writer suppresses all periodic heartbeats */
  unsigned test_drop_outgoing_data : 1; /* iff 1, the writer drops outgoing data, forcing the readers to request a retransmit */
#ifdef DDS_HAS_SHM
  unsigned has_iceoryx : 1;
#endif
#ifdef DDS_HAS_SSM
  unsigned supports_ssm: 1;
  struct ddsi_addrset *ssm_as;
#endif
  uint32_t alive_vclock; /* virtual clock counting transitions between alive/not-alive */
  const struct ddsi_sertype * type; /* type of the data written by this writer */
  struct ddsi_addrset *as; /* set of addresses to publish to */
  struct ddsi_xevent *heartbeat_xevent; /* timed event for "periodically" publishing heartbeats when unack'd data present, NULL <=> unreliable */
  struct ddsi_ldur_fhnode *lease_duration; /* fibheap node to keep lease duration for this writer, NULL in case of automatic liveliness with inifite duration  */
  struct ddsi_whc *whc; /* WHC tracking history, T-L durability service history + samples by sequence number for retransmit */
  uint32_t whc_low, whc_high; /* watermarks for WHC in bytes (counting only unack'd data) */
  ddsrt_etime_t t_rexmit_start;
  ddsrt_etime_t t_rexmit_end; /* time of last 1->0 transition of "retransmitting" */
  ddsrt_etime_t t_whc_high_upd; /* time "whc_high" was last updated for controlled ramp-up of throughput */
  uint32_t init_burst_size_limit; /* derived from reader's receive_buffer_size */
  uint32_t rexmit_burst_size_limit; /* derived from reader's receive_buffer_size */
  uint32_t num_readers; /* total number of matching PROXY readers */
  uint32_t num_reliable_readers; /* number of matching reliable PROXY readers */
  uint32_t num_readers_requesting_keyhash; /* also +1 for protected keys and config override for generating keyhash */
  ddsrt_avl_tree_t readers; /* all matching PROXY readers, see struct ddsi_wr_prd_match */
  ddsrt_avl_tree_t local_readers; /* all matching LOCAL readers, see struct ddsi_wr_rd_match */
#ifdef DDS_HAS_NETWORK_PARTITIONS
  const struct ddsi_config_networkpartition_listelem *network_partition;
#endif
  uint32_t num_acks_received; /* cum received ACKNACKs with no request for retransmission */
  uint32_t num_nacks_received; /* cum received ACKNACKs that did request retransmission */
  uint32_t throttle_count; /* cum times transmitting was throttled (whc hitting high-level mark) */
  uint32_t throttle_tracing;
  uint32_t rexmit_count; /* cum samples retransmitted (counting events; 1 sample can be counted many times) */
  uint32_t rexmit_lost_count; /* cum samples lost but retransmit requested (also counting events) */
  uint64_t rexmit_bytes; /* cum bytes queued for retransmit */
  uint64_t time_throttled; /* cum time in throttled state */
  uint64_t time_retransmit; /* cum time in retransmitting state */
  struct ddsi_xeventq *evq; /* timed event queue to be used by this writer */
  struct ddsi_local_reader_ary rdary; /* LOCAL readers for fast-pathing; if not fast-pathed, fall back to scanning local_readers */
  struct ddsi_lease *lease; /* for liveliness administration (writer can only become inactive when using manual liveliness) */
#ifdef DDS_HAS_SECURITY
  struct ddsi_writer_sec_attributes *sec_attr;
#endif
};

struct ddsi_local_orphan_writer {
  struct ddsi_writer wr;
};

struct ddsi_reader
{
  struct ddsi_entity_common e;
  struct ddsi_endpoint_common c;
  ddsi_status_cb_t status_cb;
  void * status_cb_entity;
  struct ddsi_rhc * rhc; /* reader history, tracks registrations and data */
  struct dds_qos *xqos;
  unsigned reliable: 1; /* 1 iff reader is reliable */
  unsigned handle_as_transient_local: 1; /* 1 iff reader wants historical data from proxy writers */
  unsigned request_keyhash: 1; /* really controlled by the sertype */
#ifdef DDS_HAS_SSM
  unsigned favours_ssm: 1; /* iff 1, this reader favours SSM */
#endif
#ifdef DDS_HAS_SHM
  unsigned has_iceoryx : 1;
#endif
  ddsi_count_t init_acknack_count; /* initial value for "count" (i.e. ACK seq num) for newly matched proxy writers */
#ifdef DDS_HAS_NETWORK_PARTITIONS
  struct ddsi_networkpartition_address *uc_as;
  struct ddsi_networkpartition_address *mc_as;
#endif
  const struct ddsi_sertype * type; /* type of the data read by this reader */
  uint32_t num_writers; /* total number of matching PROXY writers */
  ddsrt_avl_tree_t writers; /* all matching PROXY writers, see struct ddsi_rd_pwr_match */
  ddsrt_avl_tree_t local_writers; /* all matching LOCAL writers, see struct ddsi_rd_wr_match */
#ifdef DDS_HAS_SECURITY
  struct ddsi_reader_sec_attributes *sec_attr;
#endif
};

DDS_EXPORT extern const ddsrt_avl_treedef_t ddsi_wr_readers_treedef;
DDS_EXPORT extern const ddsrt_avl_treedef_t ddsi_wr_local_readers_treedef;
DDS_EXPORT extern const ddsrt_avl_treedef_t ddsi_rd_writers_treedef;
DDS_EXPORT extern const ddsrt_avl_treedef_t ddsi_rd_local_writers_treedef;

/** @component ddsi_endpoint */
int ddsi_is_builtin_endpoint (ddsi_entityid_t id, ddsi_vendorid_t vendorid);


// writer

/** @component ddsi_endpoint */
struct ddsi_local_orphan_writer *ddsi_new_local_orphan_writer (struct ddsi_domaingv *gv, ddsi_entityid_t entityid, const char *topic_name, struct ddsi_sertype *type, const struct dds_qos *xqos, struct ddsi_whc *whc);

/** @component ddsi_endpoint */
void ddsi_delete_local_orphan_writer (struct ddsi_local_orphan_writer *wr);

/** @component ddsi_endpoint */
dds_return_t ddsi_new_writer (struct ddsi_writer **wr_out, struct ddsi_guid *wrguid, const struct ddsi_guid *group_guid, struct ddsi_participant *pp, const char *topic_name, const struct ddsi_sertype *type, const struct dds_qos *xqos, struct ddsi_whc * whc, ddsi_status_cb_t status_cb, void *status_cb_arg);

/** @component ddsi_endpoint */
void ddsi_update_writer_qos (struct ddsi_writer *wr, const struct dds_qos *xqos);

/** @component ddsi_endpoint */
void ddsi_make_writer_info(struct ddsi_writer_info *wrinfo, const struct ddsi_entity_common *e, const struct dds_qos *xqos, uint32_t statusinfo);

/** @component ddsi_endpoint */
dds_return_t ddsi_writer_wait_for_acks (struct ddsi_writer *wr, const ddsi_guid_t *rdguid, dds_time_t abstimeout);

/** @component ddsi_endpoint */
dds_return_t ddsi_unblock_throttled_writer (struct ddsi_domaingv *gv, const struct ddsi_guid *guid);

/** @component ddsi_endpoint */
dds_return_t ddsi_delete_writer (struct ddsi_domaingv *gv, const struct ddsi_guid *guid);

/** @component ddsi_endpoint */
struct ddsi_reader *ddsi_writer_first_in_sync_reader (struct ddsi_entity_index *entity_index, struct ddsi_entity_common *wrcmn, ddsrt_avl_iter_t *it);

/** @component ddsi_endpoint */
struct ddsi_reader *ddsi_writer_next_in_sync_reader (struct ddsi_entity_index *entity_index, ddsrt_avl_iter_t *it);


// reader

/** @component ddsi_endpoint */
dds_return_t ddsi_new_reader (struct ddsi_reader **rd_out, struct ddsi_guid *rdguid, const struct ddsi_guid *group_guid, struct ddsi_participant *pp, const char *topic_name, const struct ddsi_sertype *type, const struct dds_qos *xqos, struct ddsi_rhc * rhc, ddsi_status_cb_t status_cb, void *status_cb_arg);

/** @component ddsi_endpoint */
void ddsi_update_reader_qos (struct ddsi_reader *rd, const struct dds_qos *xqos);

/** @component ddsi_endpoint */
dds_return_t ddsi_delete_reader (struct ddsi_domaingv *gv, const struct ddsi_guid *guid);


#if defined (__cplusplus)
}
#endif

#endif /* DDSI_ENDPOINT_H */
