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
#ifndef Q_ENTITY_H
#define Q_ENTITY_H

#include "dds/export.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/fibheap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsi/q_rtps.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/q_protocol.h"
#include "dds/ddsi/q_lat_estim.h"
#include "dds/ddsi/q_hbcontrol.h"
#include "dds/ddsi/q_feature_check.h"
#include "dds/ddsi/q_inverse_uint32_set.h"
#include "dds/ddsi/ddsi_serdata_default.h"
#include "dds/ddsi/ddsi_handshake.h"

#include "dds/ddsi/ddsi_tran.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct xevent;
struct nn_reorder;
struct nn_defrag;
struct nn_dqueue;
struct nn_rsample_info;
struct nn_rdata;
struct addrset;
struct ddsi_sertopic;
struct whc;
struct dds_qos;
struct ddsi_plist;
struct lease;
struct participant_sec_attributes;
struct proxy_participant_sec_attributes;
struct writer_sec_attributes;
struct reader_sec_attributes;

struct proxy_group;
struct proxy_endpoint_common;
typedef void (*ddsi2direct_directread_cb_t) (const struct nn_rsample_info *sampleinfo, const struct nn_rdata *fragchain, void *arg);

enum entity_kind {
  EK_PARTICIPANT,
  EK_PROXY_PARTICIPANT,
  EK_WRITER,
  EK_PROXY_WRITER,
  EK_READER,
  EK_PROXY_READER
};

/* Liveliness changed is more complicated than just add/remove. Encode the event
   in status_cb_data_t::extra and ignore status_cb_data_t::add */
enum liveliness_changed_data_extra {
  LIVELINESS_CHANGED_ADD_ALIVE,
  LIVELINESS_CHANGED_ADD_NOT_ALIVE,
  LIVELINESS_CHANGED_REMOVE_NOT_ALIVE,
  LIVELINESS_CHANGED_REMOVE_ALIVE,
  LIVELINESS_CHANGED_ALIVE_TO_NOT_ALIVE,
  LIVELINESS_CHANGED_NOT_ALIVE_TO_ALIVE
};

typedef struct status_cb_data
{
  int raw_status_id;
  uint32_t extra;
  uint64_t handle;
  bool add;
} status_cb_data_t;

typedef void (*status_cb_t) (void *entity, const status_cb_data_t *data);

struct prd_wr_match {
  ddsrt_avl_node_t avlnode;
  ddsi_guid_t wr_guid;
#ifdef DDSI_INCLUDE_SECURITY
  int64_t crypto_handle;
#endif
};

struct rd_pwr_match {
  ddsrt_avl_node_t avlnode;
  ddsi_guid_t pwr_guid;
  unsigned pwr_alive: 1; /* tracks pwr's alive state */
  uint32_t pwr_alive_vclock; /* used to ensure progress */
#ifdef DDSI_INCLUDE_SSM
  nn_locator_t ssm_mc_loc;
  nn_locator_t ssm_src_loc;
#endif
#ifdef DDSI_INCLUDE_SECURITY
  int64_t crypto_handle;
#endif
};

struct wr_rd_match {
  ddsrt_avl_node_t avlnode;
  ddsi_guid_t rd_guid;
};

struct rd_wr_match {
  ddsrt_avl_node_t avlnode;
  ddsi_guid_t wr_guid;
  unsigned wr_alive: 1; /* tracks wr's alive state */
  uint32_t wr_alive_vclock; /* used to ensure progress */
};

struct wr_prd_match {
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
#ifdef DDSI_INCLUDE_SECURITY
  int64_t crypto_handle;
#endif
};

enum pwr_rd_match_syncstate {
  PRMSS_SYNC, /* in sync with proxy writer, has caught up with historical data */
  PRMSS_TLCATCHUP, /* in sync with proxy writer, pwr + readers still catching up on historical data */
  PRMSS_OUT_OF_SYNC /* not in sync with proxy writer */
};

struct last_nack_summary {
  seqno_t seq_end_p1; /* last seq for which we requested a retransmit */
  seqno_t seq_base;
  uint32_t frag_end_p1; /* last fragnum of seq_last_nack for which requested a retransmit */
  uint32_t frag_base;
};

struct pwr_rd_match {
  ddsrt_avl_node_t avlnode;
  ddsi_guid_t rd_guid;
  ddsrt_mtime_t tcreate;
  nn_count_t count; /* most recent acknack sequence number */
  nn_count_t prev_heartbeat; /* latest accepted heartbeat (see also add_proxy_writer_to_reader) */
  ddsrt_wctime_t hb_timestamp; /* time of most recent heartbeat that rescheduled the ack event */
  ddsrt_etime_t t_heartbeat_accepted; /* (local) time a heartbeat was last accepted */
  ddsrt_mtime_t t_last_nack; /* (local) time we last sent a NACK */
  ddsrt_mtime_t t_last_ack; /* (local) time we last sent any ACKNACK */
  seqno_t last_seq; /* last known sequence number from this writer */
  struct last_nack_summary last_nack;
  struct xevent *acknack_xevent; /* entry in xevent queue for sending acknacks */
  enum pwr_rd_match_syncstate in_sync; /* whether in sync with the proxy writer */
  unsigned ack_requested : 1; /* set on receipt of HEARTBEAT with FINAL clear, cleared on sending an ACKNACK */
  unsigned heartbeat_since_ack : 1; /* set when a HEARTBEAT has been received since the last ACKNACK */
  unsigned heartbeatfrag_since_ack : 1; /* set when a HEARTBEATFRAG has been received since the last ACKNACK */
  unsigned directed_heartbeat : 1; /* set on receipt of a directed heartbeat, cleared on sending an ACKNACK */
  unsigned nack_sent_on_nackdelay : 1; /* set when the most recent NACK sent was because of the NackDelay  */
  unsigned filtered : 1;
  union {
    struct {
      seqno_t end_of_tl_seq; /* when seq >= end_of_tl_seq, it's in sync, =0 when not tl */
      struct nn_reorder *reorder; /* can be done (mostly) per proxy writer, but that is harder; only when state=OUT_OF_SYNC */
    } not_in_sync;
  } u;
#ifdef DDSI_INCLUDE_SECURITY
  int64_t crypto_handle;
#endif
};

struct nn_rsample_info;
struct nn_rdata;
struct ddsi_tkmap_instance;

struct entity_common {
  enum entity_kind kind;
  ddsi_guid_t guid;
  ddsrt_wctime_t tupdate; /* timestamp of last update */
  char *name;
  uint64_t iid;
  struct ddsi_tkmap_instance *tk;
  ddsrt_mutex_t lock;
  bool onlylocal;
  struct ddsi_domaingv *gv;
  ddsrt_avl_node_t all_entities_avlnode;

  /* QoS changes always lock the entity itself, and additionally
     (and within the scope of the entity lock) acquire qos_lock
     while manipulating the QoS.  So any thread that needs to read
     the QoS without acquiring the entity's lock can still do so
     (e.g., the materialisation of samples for built-in topics
     when connecting a reader to a writer for a built-in topic).

     qos_lock lock order across entities in is in increasing
     order of entity addresses cast to uintptr_t. */
  ddsrt_mutex_t qos_lock;
};

struct local_reader_ary {
  ddsrt_mutex_t rdary_lock;
  unsigned valid: 1; /* always true until (proxy-)writer is being deleted; !valid => !fastpath_ok */
  unsigned fastpath_ok: 1; /* if not ok, fall back to using GUIDs (gives access to the reader-writer match data for handling readers that bumped into resource limits, hence can flip-flop, unlike "valid") */
  uint32_t n_readers;
  struct reader **rdary; /* for efficient delivery, null-pointer terminated, grouped by topic */
};

struct avail_entityid_set {
  struct inverse_uint32_set x;
};

struct participant
{
  struct entity_common e;
  dds_duration_t lease_duration; /* constant */
  uint32_t bes; /* built-in endpoint set */
  unsigned is_ddsi2_pp: 1; /* true for the "federation leader", the ddsi2 participant itself in OSPL; FIXME: probably should use this for broker mode as well ... */
  struct ddsi_plist *plist; /* settings/QoS for this participant */
  struct xevent *spdp_xevent; /* timed event for periodically publishing SPDP */
  struct xevent *pmd_update_xevent; /* timed event for periodically publishing ParticipantMessageData */
  nn_locator_t m_locator;
  ddsi_tran_conn_t m_conn;
  struct avail_entityid_set avail_entityids; /* available entity ids [e.lock] */
  ddsrt_mutex_t refc_lock;
  int32_t user_refc; /* number of non-built-in endpoints in this participant [refc_lock] */
  int32_t builtin_refc; /* number of built-in endpoints in this participant [refc_lock] */
  int builtins_deleted; /* whether deletion of built-in endpoints has been initiated [refc_lock] */
  ddsrt_fibheap_t ldur_auto_wr; /* Heap that contains lease duration for writers with automatic liveliness in this participant */
  ddsrt_atomic_voidp_t minl_man; /* clone of min(leaseheap_man) */
  ddsrt_fibheap_t leaseheap_man; /* keeps leases for this participant's writers (with liveliness manual-by-participant) */
#ifdef DDSI_INCLUDE_SECURITY
  struct participant_sec_attributes *sec_attr;
  nn_security_info_t security_info;
#endif
};

struct endpoint_common {
  struct participant *pp;
  ddsi_guid_t group_guid;
};

struct generic_endpoint { /* FIXME: currently only local endpoints; proxies use entity_common + proxy_endpoint common */
  struct entity_common e;
  struct endpoint_common c;
};

enum writer_state {
  WRST_OPERATIONAL, /* normal situation */
  WRST_INTERRUPT, /* will be deleted, unblock throttle_writer but do not do anything further */
  WRST_LINGERING, /* writer deletion has been requested but still has unack'd data */
  WRST_DELETING /* writer is actually being deleted (removed from hash table) */
};

typedef ddsrt_atomic_uint64_t seq_xmit_t;

struct ldur_fhnode {
  ddsrt_fibheap_node_t heapnode;
  dds_duration_t ldur;
};

struct writer
{
  struct entity_common e;
  struct endpoint_common c;
  status_cb_t status_cb;
  void * status_cb_entity;
  ddsrt_cond_t throttle_cond; /* used to trigger a transmit thread blocked in throttle_writer() or wait_for_acks() */
  seqno_t seq; /* last sequence number (transmitted seqs are 1 ... seq) */
  seqno_t cs_seq; /* 1st seq in coherent set (or 0) */
  seq_xmit_t seq_xmit; /* last sequence number actually transmitted */
  seqno_t min_local_readers_reject_seq; /* mimum of local_readers->last_deliv_seq */
  nn_count_t hbcount; /* last hb seq number */
  nn_count_t hbfragcount; /* last hb frag seq number */
  int throttling; /* non-zero when some thread is waiting for the WHC to shrink */
  struct hbcontrol hbcontrol; /* controls heartbeat timing, piggybacking */
  struct dds_qos *xqos;
  enum writer_state state;
  unsigned reliable: 1; /* iff 1, writer is reliable <=> heartbeat_xevent != NULL */
  unsigned handle_as_transient_local: 1; /* controls whether data is retained in WHC */
  unsigned include_keyhash: 1; /* iff 1, this writer includes a keyhash; keyless topics => include_keyhash = 0 */
  unsigned force_md5_keyhash: 1; /* iff 1, when keyhash has to be hashed, no matter the size */
  unsigned retransmitting: 1; /* iff 1, this writer is currently retransmitting */
  unsigned alive: 1; /* iff 1, the writer is alive (lease for this writer is not expired); field may be modified only when holding both wr->e.lock and wr->c.pp->e.lock */
  unsigned test_ignore_acknack : 1; /* iff 1, the writer ignores all arriving ACKNACK messages */
  unsigned test_suppress_retransmit : 1; /* iff 1, the writer does not respond to retransmit requests */
  unsigned test_suppress_heartbeat : 1; /* iff 1, the writer suppresses all periodic heartbeats */
  unsigned test_drop_outgoing_data : 1; /* iff 1, the writer drops outgoing data, forcing the readers to request a retransmit */
#ifdef DDSI_INCLUDE_SSM
  unsigned supports_ssm: 1;
  struct addrset *ssm_as;
#endif
  uint32_t alive_vclock; /* virtual clock counting transitions between alive/not-alive */
  const struct ddsi_sertopic * topic; /* topic */
  struct addrset *as; /* set of addresses to publish to */
  struct addrset *as_group; /* alternate case, used for SPDP, when using Cloud with multiple bootstrap locators */
  struct xevent *heartbeat_xevent; /* timed event for "periodically" publishing heartbeats when unack'd data present, NULL <=> unreliable */
  struct ldur_fhnode *lease_duration; /* fibheap node to keep lease duration for this writer, NULL in case of automatic liveliness with inifite duration  */
  struct whc *whc; /* WHC tracking history, T-L durability service history + samples by sequence number for retransmit */
  uint32_t whc_low, whc_high; /* watermarks for WHC in bytes (counting only unack'd data) */
  ddsrt_etime_t t_rexmit_start;
  ddsrt_etime_t t_rexmit_end; /* time of last 1->0 transition of "retransmitting" */
  ddsrt_etime_t t_whc_high_upd; /* time "whc_high" was last updated for controlled ramp-up of throughput */
  uint32_t init_burst_size_limit; /* derived from reader's receive_buffer_size */
  uint32_t rexmit_burst_size_limit; /* derived from reader's receive_buffer_size */
  uint32_t num_readers; /* total number of matching PROXY readers */
  uint32_t num_reliable_readers; /* number of matching reliable PROXY readers */
  ddsrt_avl_tree_t readers; /* all matching PROXY readers, see struct wr_prd_match */
  ddsrt_avl_tree_t local_readers; /* all matching LOCAL readers, see struct wr_rd_match */
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  uint32_t partition_id;
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
  struct xeventq *evq; /* timed event queue to be used by this writer */
  struct local_reader_ary rdary; /* LOCAL readers for fast-pathing; if not fast-pathed, fall back to scanning local_readers */
  struct lease *lease; /* for liveliness administration (writer can only become inactive when using manual liveliness) */
#ifdef DDSI_INCLUDE_SECURITY
  struct writer_sec_attributes *sec_attr;
#endif
};

inline seqno_t writer_read_seq_xmit (const struct writer *wr) {
  return (seqno_t) ddsrt_atomic_ld64 (&wr->seq_xmit);
}

inline void writer_update_seq_xmit (struct writer *wr, seqno_t nv) {
  uint64_t ov;
  do {
    ov = ddsrt_atomic_ld64 (&wr->seq_xmit);
    if ((uint64_t) nv <= ov) break;
  } while (!ddsrt_atomic_cas64 (&wr->seq_xmit, ov, (uint64_t) nv));
}

struct reader
{
  struct entity_common e;
  struct endpoint_common c;
  status_cb_t status_cb;
  void * status_cb_entity;
  struct ddsi_rhc * rhc; /* reader history, tracks registrations and data */
  struct dds_qos *xqos;
  unsigned reliable: 1; /* 1 iff reader is reliable */
  unsigned handle_as_transient_local: 1; /* 1 iff reader wants historical data from proxy writers */
#ifdef DDSI_INCLUDE_SSM
  unsigned favours_ssm: 1; /* iff 1, this reader favours SSM */
#endif
  nn_count_t init_acknack_count; /* initial value for "count" (i.e. ACK seq num) for newly matched proxy writers */
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  struct addrset *as;
#endif
  const struct ddsi_sertopic * topic; /* topic */
  uint32_t num_writers; /* total number of matching PROXY writers */
  ddsrt_avl_tree_t writers; /* all matching PROXY writers, see struct rd_pwr_match */
  ddsrt_avl_tree_t local_writers; /* all matching LOCAL writers, see struct rd_wr_match */
  ddsi2direct_directread_cb_t ddsi2direct_cb;
  void *ddsi2direct_cbarg;
#ifdef DDSI_INCLUDE_SECURITY
  struct reader_sec_attributes *sec_attr;
#endif
};

struct proxy_participant
{
  struct entity_common e;
  uint32_t refc; /* number of proxy endpoints (both user & built-in; not groups, they don't have a life of their own) */
  nn_vendorid_t vendor; /* vendor code from discovery */
  unsigned bes; /* built-in endpoint set */
  ddsi_guid_t privileged_pp_guid; /* if this PP depends on another PP for its SEDP writing */
  struct ddsi_plist *plist; /* settings/QoS for this participant */
  ddsrt_atomic_voidp_t minl_auto; /* clone of min(leaseheap_auto) */
  ddsrt_fibheap_t leaseheap_auto; /* keeps leases for this proxypp and leases for pwrs (with liveliness automatic) */
  ddsrt_atomic_voidp_t minl_man; /* clone of min(leaseheap_man) */
  ddsrt_fibheap_t leaseheap_man; /* keeps leases for this proxypp and leases for pwrs (with liveliness manual-by-participant) */
  struct lease *lease; /* lease for this proxypp */
  struct addrset *as_default; /* default address set to use for user data traffic */
  struct addrset *as_meta; /* default address set to use for discovery traffic */
  struct proxy_endpoint_common *endpoints; /* all proxy endpoints can be reached from here */
  ddsrt_avl_tree_t groups; /* table of all groups (publisher, subscriber), see struct proxy_group */
  seqno_t seq; /* sequence number of most recent SPDP message */
  uint32_t receive_buffer_size; /* assumed size of receive buffer, used to limit bursts involving this proxypp */
  unsigned implicitly_created : 1; /* participants are implicitly created for Cloud/Fog discovered endpoints */
  unsigned is_ddsi2_pp: 1; /* if this is the federation-leader on the remote node */
  unsigned minimal_bes_mode: 1;
  unsigned lease_expired: 1;
  unsigned deleting: 1;
  unsigned proxypp_have_spdp: 1;
  unsigned owns_lease: 1;
#ifdef DDSI_INCLUDE_SECURITY
  nn_security_info_t security_info;
  struct proxy_participant_sec_attributes *sec_attr;
#endif
};

/* Representing proxy subscriber & publishers as "groups": until DDSI2
   gets a reason to care about these other than for the generation of
   CM topics, there's little value in distinguishing between the two.
   In another way, they're secondly-class citizens, too: "real"
   entities are garbage collected and found using lock-free hash
   tables, but "groups" only live in the context of a proxy
   participant. */
struct proxy_group {
  ddsrt_avl_node_t avlnode;
  ddsi_guid_t guid;
  char *name;
  struct proxy_participant *proxypp; /* uncounted backref to proxy participant */
  struct dds_qos *xqos; /* publisher/subscriber QoS */
};

struct proxy_endpoint_common
{
  struct proxy_participant *proxypp; /* counted backref to proxy participant */
  struct proxy_endpoint_common *next_ep; /* next \ endpoint belonging to this proxy participant */
  struct proxy_endpoint_common *prev_ep; /* prev / -- this is in arbitrary ordering */
  struct dds_qos *xqos; /* proxy endpoint QoS lives here; FIXME: local ones should have it moved to common as well */
  struct addrset *as; /* address set to use for communicating with this endpoint */
  ddsi_guid_t group_guid; /* 0:0:0:0 if not available */
  nn_vendorid_t vendor; /* cached from proxypp->vendor */
  seqno_t seq; /* sequence number of most recent SEDP message */
#ifdef DDSI_INCLUDE_SECURITY
  nn_security_info_t security_info;
#endif
};

struct generic_proxy_endpoint {
  struct entity_common e;
  struct proxy_endpoint_common c;
};

struct proxy_writer {
  struct entity_common e;
  struct proxy_endpoint_common c;
  ddsrt_avl_tree_t readers; /* matching LOCAL readers, see pwr_rd_match */
  int32_t n_reliable_readers; /* number of those that are reliable */
  int32_t n_readers_out_of_sync; /* number of those that require special handling (accepting historical data, waiting for historical data set to become complete) */
  seqno_t last_seq; /* highest known seq published by the writer, not last delivered */
  uint32_t last_fragnum; /* last known frag for last_seq, or UINT32_MAX if last_seq not partial */
  nn_count_t nackfragcount; /* last nackfrag seq number */
  ddsrt_atomic_uint32_t next_deliv_seq_lowword; /* lower 32-bits for next sequence number that will be delivered; for generating acks; 32-bit so atomic reads on all supported platforms */
  unsigned deliver_synchronously: 1; /* iff 1, delivery happens straight from receive thread for non-historical data; else through delivery queue "dqueue" */
  unsigned have_seen_heartbeat: 1; /* iff 1, we have received at least on heartbeat from this proxy writer */
  unsigned local_matching_inprogress: 1; /* iff 1, we are still busy matching local readers; this is so we don't deliver incoming data to some but not all readers initially */
  unsigned alive: 1; /* iff 1, the proxy writer is alive (lease for this proxy writer is not expired); field may be modified only when holding both pwr->e.lock and pwr->c.proxypp->e.lock */
  unsigned filtered: 1; /* iff 1, builtin proxy writer uses content filter, which affects heartbeats and gaps. */
#ifdef DDSI_INCLUDE_SSM
  unsigned supports_ssm: 1; /* iff 1, this proxy writer supports SSM */
#endif
  uint32_t alive_vclock; /* virtual clock counting transitions between alive/not-alive */
  struct nn_defrag *defrag; /* defragmenter for this proxy writer; FIXME: perhaps shouldn't be for historical data */
  struct nn_reorder *reorder; /* message reordering for this proxy writer, out-of-sync readers can have their own, see pwr_rd_match */
  struct nn_dqueue *dqueue; /* delivery queue for asynchronous delivery (historical data is always delivered asynchronously) */
  struct xeventq *evq; /* timed event queue to be used for ACK generation */
  struct local_reader_ary rdary; /* LOCAL readers for fast-pathing; if not fast-pathed, fall back to scanning local_readers */
  ddsi2direct_directread_cb_t ddsi2direct_cb;
  void *ddsi2direct_cbarg;
  struct lease *lease;
};


typedef int (*filter_fn_t)(struct writer *wr, struct proxy_reader *prd, struct ddsi_serdata *serdata);

struct proxy_reader {
  struct entity_common e;
  struct proxy_endpoint_common c;
  unsigned deleting: 1; /* set when being deleted */
  unsigned is_fict_trans_reader: 1; /* only true when it is certain that is a fictitious transient data reader (affects built-in topic generation) */
#ifdef DDSI_INCLUDE_SSM
  unsigned favours_ssm: 1; /* iff 1, this proxy reader favours SSM when available */
#endif
  ddsrt_avl_tree_t writers; /* matching LOCAL writers */
  uint32_t receive_buffer_size; /* assumed receive buffer size inherited from proxypp */
  filter_fn_t filter;
};

DDS_EXPORT extern const ddsrt_avl_treedef_t wr_readers_treedef;
DDS_EXPORT extern const ddsrt_avl_treedef_t wr_local_readers_treedef;
DDS_EXPORT extern const ddsrt_avl_treedef_t rd_writers_treedef;
DDS_EXPORT extern const ddsrt_avl_treedef_t rd_local_writers_treedef;
DDS_EXPORT extern const ddsrt_avl_treedef_t pwr_readers_treedef;
DDS_EXPORT extern const ddsrt_avl_treedef_t prd_writers_treedef;
extern const ddsrt_avl_treedef_t deleted_participants_treedef;

#define DPG_LOCAL 1
#define DPG_REMOTE 2
struct deleted_participants_admin;
struct deleted_participants_admin *deleted_participants_admin_new (const ddsrt_log_cfg_t *logcfg, int64_t delay);
void deleted_participants_admin_free (struct deleted_participants_admin *admin);
int is_deleted_participant_guid (struct deleted_participants_admin *admin, const struct ddsi_guid *guid, unsigned for_what);

bool is_null_guid (const ddsi_guid_t *guid);
ddsi_entityid_t to_entityid (unsigned u);
int is_builtin_entityid (ddsi_entityid_t id, nn_vendorid_t vendorid);
int is_builtin_endpoint (ddsi_entityid_t id, nn_vendorid_t vendorid);
bool is_local_orphan_endpoint (const struct entity_common *e);
int is_writer_entityid (ddsi_entityid_t id);
int is_reader_entityid (ddsi_entityid_t id);
int is_keyed_endpoint_entityid (ddsi_entityid_t id);
nn_vendorid_t get_entity_vendorid (const struct entity_common *e);

/* Interface for glue code between the OpenSplice kernel and the DDSI
   entities. These all return 0 iff successful. All GIDs supplied
   __MUST_BE_UNIQUE__. All hell may break loose if they aren't.

   All delete operations synchronously remove the entity being deleted
   from the various global hash tables on GUIDs. This ensures no new
   operations can be invoked by the glue code, discovery, protocol
   messages, &c.  The entity is then scheduled for garbage collection.

     There is one exception: a participant without built-in
     endpoints: that one synchronously reaches reference count zero
     and is then freed immediately.

     If new_writer() and/or new_reader() may be called in parallel to
     delete_participant(), trouble ensues. The current glue code
     performs all local discovery single-threaded, and can't ever get
     into that issue.

   A garbage collector thread is used to perform the actual freeing of
   an entity, but it never does so before all threads have made
   sufficient progress to guarantee they are not using that entity any
   longer, with the exception of use via internal pointers in the
   entity data structures.

   An example of the latter is that (proxy) endpoints have a pointer
   to the owning (proxy) participant, but the (proxy) participant is
   reference counted to make this safe.

   The case of a proxy writer is particularly complicated is it has to
   pass through a multiple-stage delay in the garbage collector before
   it may be freed: first there is the possibility of a parallel
   delete or protocol message, then there is still the possibility of
   data in a delivery queue.  This is dealt by requeueing garbage
   collection and sending bubbles through the delivery queue. */

/* Set this flag in new_participant to prevent the creation SPDP, SEDP
   and PMD readers for that participant.  It doesn't really need it,
   they all share the information anyway.  But you do need it once. */
#define RTPS_PF_NO_BUILTIN_READERS 1u
/* Set this flag to prevent the creation of SPDP, SEDP and PMD
   writers.  It will then rely on the "privileged participant", which
   must exist at the time of creation.  It creates a reference to that
   "privileged participant" to ensure it won't disappear too early. */
#define RTPS_PF_NO_BUILTIN_WRITERS 2u
/* Set this flag to mark the participant as the "privileged
   participant", there can only be one of these.  The privileged
   participant MUST have all builtin readers and writers. */
#define RTPS_PF_PRIVILEGED_PP 4u
  /* Set this flag to mark the participant as is_ddsi2_pp. */
#define RTPS_PF_IS_DDSI2_PP 8u
  /* Set this flag to mark the participant as an local entity only. */
#define RTPS_PF_ONLY_LOCAL 16u

/**
 * @brief Create a new participant with a given GUID in the domain.
 *
 * @param[in,out]  ppguid
 *               The GUID of the new participant, may be adjusted by security.
 * @param[in]  flags
 *               Zero or more of:
 *               - RTPS_PF_NO_BUILTIN_READERS   do not create discovery readers in new ppant
 *               - RTPS_PF_NO_BUILTIN_WRITERS   do not create discvoery writers in new ppant
 *               - RTPS_PF_PRIVILEGED_PP        FIXME: figure out how to describe this ...
 *               - RTPS_PF_IS_DDSI2_PP          FIXME: OSPL holdover - there is no DDSI2E here
 *               - RTPS_PF_ONLY_LOCAL           FIXME: not used, it seems
 * @param[in]  plist
 *               Parameters/QoS for this participant
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *               All parameters valid (or ignored), dest and *nextafterplist have been set
 *               accordingly.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *               A participant with GUID *ppguid already exists.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES
 *               The configured maximum number of participants has been reached.
 */
dds_return_t new_participant_guid (ddsi_guid_t *ppguid, struct ddsi_domaingv *gv, unsigned flags, const struct ddsi_plist *plist);

/**
 * @brief Create a new participant in the domain.  See also new_participant_guid.
 *
 * @param[out] ppguid
 *               On successful return: the GUID of the new participant;
 *               Undefined on error.
 * @param[in]  flags
 *               See new_participant_guid
 * @param[in]  plist
 *               See new_participant_guid
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *               Success, there is now a local participant with the GUID stored in
 *               *ppguid
 * @retval DDS_RETCODE_OUT_OF_RESOURCES
 *               Failed to allocate a new GUID (note: currently this will always
 *               happen after 2**24-1 successful calls to new_participant ...).
 * @retval DDS_RETCODE_OUT_OF_RESOURCES
 *               The configured maximum number of participants has been reached.
*/
dds_return_t new_participant (struct ddsi_guid *ppguid, struct ddsi_domaingv *gv, unsigned flags, const struct ddsi_plist *plist);

/**
 * @brief Initiate the deletion of the participant:
 * - dispose/unregister built-in topic
 * - list it as one of the recently deleted participants
 * - remote it from the GUID hash tables
 * - schedule the scare stuff to really delete it via the GC
 *
 * It is ok to call delete_participant without deleting all DDSI-level
 * readers/writers: those will simply be deleted.  (New ones can't be
 * created anymore because the participant can no longer be located via
 * the hash tables).
 *
 * @param[in]  ppguid
 *               GUID of the participant to be deleted.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *               Success, it is no longer visible and GC events have
 *               been scheduled for eventual deleting of all remaining
 *               readers and writers and freeing of memory
 * @retval DDS_RETCODE_BAD_PARAMETER
 *               ppguid lookup failed.
*/
dds_return_t delete_participant (struct ddsi_domaingv *gv, const struct ddsi_guid *ppguid);
void update_participant_plist (struct participant *pp, const struct ddsi_plist *plist);
uint64_t get_entity_instance_id (const struct ddsi_domaingv *gv, const struct ddsi_guid *guid);

/* Gets the interval for PMD messages, which is the minimal lease duration for writers
   with auto liveliness in this participant, or the participants lease duration if shorter */
DDS_EXPORT dds_duration_t pp_get_pmd_interval(struct participant *pp);

/* To obtain the builtin writer to be used for publishing SPDP, SEDP,
   PMD stuff for PP and its endpoints, given the entityid.  If PP has
   its own writer, use it; else use the privileged participant. */
DDS_EXPORT struct writer *get_builtin_writer (const struct participant *pp, unsigned entityid);

/* To create a new DDSI writer or reader belonging to participant with
   GUID "ppguid". May return NULL if participant unknown or
   writer/reader already known. */

dds_return_t new_writer (struct writer **wr_out, struct ddsi_guid *wrguid, const struct ddsi_guid *group_guid, struct participant *pp, const struct ddsi_sertopic *topic, const struct dds_qos *xqos, struct whc * whc, status_cb_t status_cb, void *status_cb_arg);
dds_return_t new_reader (struct reader **rd_out, struct ddsi_guid *rdguid, const struct ddsi_guid *group_guid, struct participant *pp, const struct ddsi_sertopic *topic, const struct dds_qos *xqos, struct ddsi_rhc * rhc, status_cb_t status_cb, void *status_cb_arg);

void update_reader_qos (struct reader *rd, const struct dds_qos *xqos);
void update_writer_qos (struct writer *wr, const struct dds_qos *xqos);

struct whc_node;
struct whc_state;
unsigned remove_acked_messages (struct writer *wr, struct whc_state *whcst, struct whc_node **deferred_free_list);
seqno_t writer_max_drop_seq (const struct writer *wr);
int writer_must_have_hb_scheduled (const struct writer *wr, const struct whc_state *whcst);
void writer_set_retransmitting (struct writer *wr);
void writer_clear_retransmitting (struct writer *wr);
dds_return_t writer_wait_for_acks (struct writer *wr, const ddsi_guid_t *rdguid, dds_time_t abstimeout);

dds_return_t unblock_throttled_writer (struct ddsi_domaingv *gv, const struct ddsi_guid *guid);
dds_return_t delete_writer (struct ddsi_domaingv *gv, const struct ddsi_guid *guid);
dds_return_t delete_writer_nolinger (struct ddsi_domaingv *gv, const struct ddsi_guid *guid);
dds_return_t delete_writer_nolinger_locked (struct writer *wr);

dds_return_t delete_reader (struct ddsi_domaingv *gv, const struct ddsi_guid *guid);

struct local_orphan_writer {
  struct writer wr;
};
struct local_orphan_writer *new_local_orphan_writer (struct ddsi_domaingv *gv, ddsi_entityid_t entityid, struct ddsi_sertopic *topic, const struct dds_qos *xqos, struct whc *whc);
void delete_local_orphan_writer (struct local_orphan_writer *wr);

void writer_set_alive_may_unlock (struct writer *wr, bool notify);
int writer_set_notalive (struct writer *wr, bool notify);

/* To create or delete a new proxy participant: "guid" MUST have the
   pre-defined participant entity id. Unlike delete_participant(),
   deleting a proxy participant will automatically delete all its
   readers & writers. Delete removes the participant from a hash table
   and schedules the actual deletion.

      -- XX what about proxy participants without built-in endpoints?
      XX --
*/

/* Set when this proxy participant is created implicitly and has to be deleted upon disappearance
   of its last endpoint.  FIXME: Currently there is a potential race with adding a new endpoint
   in parallel to deleting the last remaining one. The endpoint will then be created, added to the
   proxy participant and then both are deleted. With the current single-threaded discovery
   this can only happen when it is all triggered by lease expiry. */
#define CF_IMPLICITLY_CREATED_PROXYPP          (1 << 0)
/* Set when this proxy participant is a DDSI2 participant, to help Cloud figure out whom to send
   discovery data when used in conjunction with the networking bridge */
#define CF_PARTICIPANT_IS_DDSI2                (1 << 1)
/* Set when this proxy participant is not to be announced on the built-in topics yet */
#define CF_PROXYPP_NO_SPDP                     (1 << 2)

bool new_proxy_participant (struct ddsi_domaingv *gv, const struct ddsi_guid *guid, uint32_t bes, const struct ddsi_guid *privileged_pp_guid, struct addrset *as_default, struct addrset *as_meta, const struct ddsi_plist *plist, dds_duration_t tlease_dur, nn_vendorid_t vendor, unsigned custom_flags, ddsrt_wctime_t timestamp, seqno_t seq);
DDS_EXPORT int delete_proxy_participant_by_guid (struct ddsi_domaingv *gv, const struct ddsi_guid *guid, ddsrt_wctime_t timestamp, int isimplicit);

int update_proxy_participant_plist_locked (struct proxy_participant *proxypp, seqno_t seq, const struct ddsi_plist *datap, ddsrt_wctime_t timestamp);
int update_proxy_participant_plist (struct proxy_participant *proxypp, seqno_t seq, const struct ddsi_plist *datap, ddsrt_wctime_t timestamp);
void proxy_participant_reassign_lease (struct proxy_participant *proxypp, struct lease *newlease);

void purge_proxy_participants (struct ddsi_domaingv *gv, const nn_locator_t *loc, bool delete_from_as_disc);


/* To create a new proxy writer or reader; the proxy participant is
   determined from the GUID and must exist. */
  int new_proxy_writer (struct ddsi_domaingv *gv, const struct ddsi_guid *ppguid, const struct ddsi_guid *guid, struct addrset *as, const struct ddsi_plist *plist, struct nn_dqueue *dqueue, struct xeventq *evq, ddsrt_wctime_t timestamp, seqno_t seq);
int new_proxy_reader (struct ddsi_domaingv *gv, const struct ddsi_guid *ppguid, const struct ddsi_guid *guid, struct addrset *as, const struct ddsi_plist *plist, ddsrt_wctime_t timestamp, seqno_t seq
#ifdef DDSI_INCLUDE_SSM
                      , int favours_ssm
#endif
                      );

/* To delete a proxy writer or reader; these synchronously hide it
   from the outside world, preventing it from being matched to a
   reader or writer. Actual deletion is scheduled in the future, when
   no outstanding references may still exist (determined by checking
   thread progress, &c.). */
int delete_proxy_writer (struct ddsi_domaingv *gv, const struct ddsi_guid *guid, ddsrt_wctime_t timestamp, int isimplicit);
int delete_proxy_reader (struct ddsi_domaingv *gv, const struct ddsi_guid *guid, ddsrt_wctime_t timestamp, int isimplicit);

void update_proxy_reader (struct proxy_reader *prd, seqno_t seq, struct addrset *as, const struct dds_qos *xqos, ddsrt_wctime_t timestamp);
void update_proxy_writer (struct proxy_writer *pwr, seqno_t seq, struct addrset *as, const struct dds_qos *xqos, ddsrt_wctime_t timestamp);

void proxy_writer_set_alive_may_unlock (struct proxy_writer *pwr, bool notify);
int proxy_writer_set_notalive (struct proxy_writer *pwr, bool notify);

int new_proxy_group (const struct ddsi_guid *guid, const char *name, const struct dds_qos *xqos, ddsrt_wctime_t timestamp);

struct entity_index;
void delete_proxy_group (struct entity_index *entidx, const struct ddsi_guid *guid, ddsrt_wctime_t timestamp, int isimplicit);

/* Call this to empty all address sets of all writers to stop all outgoing traffic, or to
   rebuild them all (which only makes sense after previously having emptied them all). */
void rebuild_or_clear_writer_addrsets(struct ddsi_domaingv *gv, int rebuild);

void local_reader_ary_setfastpath_ok (struct local_reader_ary *x, bool fastpath_ok);

void connect_writer_with_proxy_reader_secure(struct writer *wr, struct proxy_reader *prd, ddsrt_mtime_t tnow, int64_t crypto_handle);
void connect_reader_with_proxy_writer_secure(struct reader *rd, struct proxy_writer *pwr, ddsrt_mtime_t tnow, int64_t crypto_handle);

struct ddsi_writer_info;
DDS_EXPORT void ddsi_make_writer_info(struct ddsi_writer_info *wrinfo, const struct entity_common *e, const struct dds_qos *xqos, uint32_t statusinfo);

#if defined (__cplusplus)
}
#endif

#endif /* Q_ENTITY_H */
