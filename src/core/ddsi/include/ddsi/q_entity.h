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

#include "os/os.h"
#include "util/ut_avl.h"
#include "ddsi/q_rtps.h"
#include "ddsi/q_protocol.h"
#include "ddsi/q_lat_estim.h"
#include "ddsi/q_ephash.h"
#include "ddsi/q_hbcontrol.h"
#include "ddsi/q_feature_check.h"
#include "ddsi/q_inverse_uint32_set.h"

#include "ddsi/ddsi_tran.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct xevent;
struct nn_reorder;
struct nn_defrag;
struct nn_dqueue;
struct addrset;
struct ddsi_sertopic;
struct whc;
struct nn_xqos;
struct nn_plist;

struct proxy_group;
struct proxy_endpoint_common;
typedef void (*ddsi2direct_directread_cb_t) (const struct nn_rsample_info *sampleinfo, const struct nn_rdata *fragchain, void *arg);

typedef struct status_cb_data
{
  int raw_status_id;
  uint32_t extra;
  uint64_t handle;
  bool add;
}
status_cb_data_t;

typedef void (*status_cb_t) (void *entity, const status_cb_data_t *data);

struct prd_wr_match {
  ut_avlNode_t avlnode;
  nn_guid_t wr_guid;
};

struct rd_pwr_match {
  ut_avlNode_t avlnode;
  nn_guid_t pwr_guid;
#ifdef DDSI_INCLUDE_SSM
  nn_locator_t ssm_mc_loc;
  nn_locator_t ssm_src_loc;
#endif
};

struct wr_rd_match {
  ut_avlNode_t avlnode;
  nn_guid_t rd_guid;
};

struct rd_wr_match {
  ut_avlNode_t avlnode;
  nn_guid_t wr_guid;
};

struct wr_prd_match {
  ut_avlNode_t avlnode;
  nn_guid_t prd_guid; /* guid of the proxy reader */
  unsigned assumed_in_sync: 1; /* set to 1 upon receipt of ack not nack'ing msgs */
  unsigned has_replied_to_hb: 1; /* we must keep sending HBs until all readers have this set */
  unsigned all_have_replied_to_hb: 1; /* true iff 'has_replied_to_hb' for all readers in subtree */
  unsigned is_reliable: 1; /* true iff reliable proxy reader */
  seqno_t min_seq; /* smallest ack'd seq nr in subtree */
  seqno_t max_seq; /* sort-of highest ack'd seq nr in subtree (see augment function) */
  seqno_t seq; /* highest acknowledged seq nr */
  int num_reliable_readers_where_seq_equals_max;
  nn_guid_t arbitrary_unacked_reader;
  nn_count_t next_acknack; /* next acceptable acknack sequence number */
  nn_count_t next_nackfrag; /* next acceptable nackfrag sequence number */
  nn_etime_t t_acknack_accepted; /* (local) time an acknack was last accepted */
  struct nn_lat_estim hb_to_ack_latency;
  nn_wctime_t hb_to_ack_latency_tlastlog;
  uint32_t non_responsive_count;
  uint32_t rexmit_requests;
};

enum pwr_rd_match_syncstate {
  PRMSS_SYNC, /* in sync with proxy writer, has caught up with historical data */
  PRMSS_TLCATCHUP, /* in sync with proxy writer, pwr + readers still catching up on historical data */
  PRMSS_OUT_OF_SYNC /* not in sync with proxy writer */
};

struct pwr_rd_match {
  ut_avlNode_t avlnode;
  nn_guid_t rd_guid;
  nn_mtime_t tcreate;
  nn_count_t count; /* most recent acknack sequence number */
  nn_count_t next_heartbeat; /* next acceptable heartbeat (see also add_proxy_writer_to_reader) */
  nn_wctime_t hb_timestamp; /* time of most recent heartbeat that rescheduled the ack event */
  nn_etime_t t_heartbeat_accepted; /* (local) time a heartbeat was last accepted */
  nn_mtime_t t_last_nack; /* (local) time we last sent a NACK */  /* FIXME: probably elapsed time is better */
  seqno_t seq_last_nack; /* last seq for which we requested a retransmit */
  struct xevent *acknack_xevent; /* entry in xevent queue for sending acknacks */
  enum pwr_rd_match_syncstate in_sync; /* whether in sync with the proxy writer */
  union {
    struct {
      seqno_t end_of_tl_seq; /* when seq >= end_of_tl_seq, it's in sync, =0 when not tl */
      seqno_t end_of_out_of_sync_seq; /* when seq >= end_of_tl_seq, it's in sync, =0 when not tl */
      struct nn_reorder *reorder; /* can be done (mostly) per proxy writer, but that is harder; only when state=OUT_OF_SYNC */
    } not_in_sync;
  } u;
};

struct nn_rsample_info;
struct nn_rdata;
struct ddsi_tkmap_instance;

struct entity_common {
  enum entity_kind kind;
  nn_guid_t guid;
  nn_wctime_t tupdate; /* timestamp of last update */
  char *name;
  uint64_t iid;
  struct ddsi_tkmap_instance *tk;
  os_mutex lock;
  bool onlylocal;
};

struct local_reader_ary {
  os_mutex rdary_lock;
  unsigned valid: 1; /* always true until (proxy-)writer is being deleted; !valid => !fastpath_ok */
  unsigned fastpath_ok: 1; /* if not ok, fall back to using GUIDs (gives access to the reader-writer match data for handling readers that bumped into resource limits, hence can flip-flop, unlike "valid") */
  unsigned n_readers;
  struct reader **rdary; /* for efficient delivery, null-pointer terminated */
};

struct avail_entityid_set {
  struct inverse_uint32_set x;
};

struct participant
{
  struct entity_common e;
  long long lease_duration; /* constant */
  unsigned bes; /* built-in endpoint set */
  unsigned prismtech_bes; /* prismtech-specific extension of built-in endpoints set */
  unsigned is_ddsi2_pp: 1; /* true for the "federation leader", the ddsi2 participant itself in OSPL; FIXME: probably should use this for broker mode as well ... */
  nn_plist_t *plist; /* settings/QoS for this participant */
  struct xevent *spdp_xevent; /* timed event for periodically publishing SPDP */
  struct xevent *pmd_update_xevent; /* timed event for periodically publishing ParticipantMessageData */
  nn_locator_t m_locator;
  ddsi_tran_conn_t m_conn;
  struct avail_entityid_set avail_entityids; /* available entity ids [e.lock] */
  os_mutex refc_lock;
  int32_t user_refc; /* number of non-built-in endpoints in this participant [refc_lock] */
  int32_t builtin_refc; /* number of built-in endpoints in this participant [refc_lock] */
  int builtins_deleted; /* whether deletion of built-in endpoints has been initiated [refc_lock] */
};

struct endpoint_common {
  struct participant *pp;
  nn_guid_t group_guid;
};

struct generic_endpoint { /* FIXME: currently only local endpoints; proxies use entity_common + proxy_endpoint common */
  struct entity_common e;
  struct endpoint_common c;
};

enum writer_state {
  WRST_OPERATIONAL, /* normal situation */
  WRST_LINGERING, /* writer deletion has been requested but still has unack'd data */
  WRST_DELETING /* writer is actually being deleted (removed from hash table) */
};

#if OS_ATOMIC64_SUPPORT
typedef os_atomic_uint64_t seq_xmit_t;
#define INIT_SEQ_XMIT(wr, v) os_atomic_st64(&(wr)->seq_xmit, (uint64_t) (v))
#define READ_SEQ_XMIT(wr) ((seqno_t) os_atomic_ld64(&(wr)->seq_xmit))
#define UPDATE_SEQ_XMIT_LOCKED(wr, nv) do { uint64_t ov_; do { \
  ov_ = os_atomic_ld64(&(wr)->seq_xmit); \
  if ((uint64_t) nv <= ov_) break; \
} while (!os_atomic_cas64(&(wr)->seq_xmit, ov_, (uint64_t) nv)); } while (0)
#define UPDATE_SEQ_XMIT_UNLOCKED(sx, nv) UPDATE_SEQ_XMIT_LOCKED(sx, nv)
#else
typedef seqno_t seq_xmit_t;
#define INIT_SEQ_XMIT(wr, v) ((wr)->seq_xmit = (v))
#define READ_SEQ_XMIT(wr) ((wr)->seq_xmit)
#define UPDATE_SEQ_XMIT_LOCKED(wr, nv) do { \
  if ((nv) > (wr)->seq_xmit) { (wr)->seq_xmit = (nv); } \
} while (0)
#define UPDATE_SEQ_XMIT_UNLOCKED(wr, nv) do { \
  os_mutexLock (&(wr)->e.lock); \
  if ((nv) > (wr)->seq_xmit) { (wr)->seq_xmit = (nv); } \
  os_mutexUnlock (&(wr)->e.lock); \
} while (0)
#endif

struct writer
{
  struct entity_common e;
  struct endpoint_common c;
  status_cb_t status_cb;
  void * status_cb_entity;
  os_cond throttle_cond; /* used to trigger a transmit thread blocked in throttle_writer() */
  seqno_t seq; /* last sequence number (transmitted seqs are 1 ... seq) */
  seqno_t cs_seq; /* 1st seq in coherent set (or 0) */
  seq_xmit_t seq_xmit; /* last sequence number actually transmitted */
  seqno_t min_local_readers_reject_seq; /* mimum of local_readers->last_deliv_seq */
  nn_count_t hbcount; /* last hb seq number */
  nn_count_t hbfragcount; /* last hb frag seq number */
  int throttling; /* non-zero when some thread is waiting for the WHC to shrink */
  struct hbcontrol hbcontrol; /* controls heartbeat timing, piggybacking */
  struct nn_xqos *xqos;
  enum writer_state state;
  unsigned reliable: 1; /* iff 1, writer is reliable <=> heartbeat_xevent != NULL */
  unsigned handle_as_transient_local: 1; /* controls whether data is retained in WHC */
  unsigned aggressive_keep_last: 1; /* controls whether KEEP_LAST will overwrite samples that haven't been ACK'd yet */
  unsigned startup_mode: 1; /* causes data to be treated as T-L for a while */
  unsigned include_keyhash: 1; /* iff 1, this writer includes a keyhash; keyless topics => include_keyhash = 0 */
  unsigned retransmitting: 1; /* iff 1, this writer is currently retransmitting */
#ifdef DDSI_INCLUDE_SSM
  unsigned supports_ssm: 1;
  struct addrset *ssm_as;
#endif
  const struct ddsi_sertopic * topic; /* topic, but may be NULL for built-ins */
  struct addrset *as; /* set of addresses to publish to */
  struct addrset *as_group; /* alternate case, used for SPDP, when using Cloud with multiple bootstrap locators */
  struct xevent *heartbeat_xevent; /* timed event for "periodically" publishing heartbeats when unack'd data present, NULL <=> unreliable */
  long long lease_duration;
  struct whc *whc; /* WHC tracking history, T-L durability service history + samples by sequence number for retransmit */
  uint32_t whc_low, whc_high; /* watermarks for WHC in bytes (counting only unack'd data) */
  nn_etime_t t_rexmit_end; /* time of last 1->0 transition of "retransmitting" */
  nn_etime_t t_whc_high_upd; /* time "whc_high" was last updated for controlled ramp-up of throughput */
  int num_reliable_readers; /* number of matching reliable PROXY readers */
  ut_avlTree_t readers; /* all matching PROXY readers, see struct wr_prd_match */
  ut_avlTree_t local_readers; /* all matching LOCAL readers, see struct wr_rd_match */
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  uint32_t partition_id;
#endif
  uint32_t num_acks_received; /* cum received ACKNACKs with no request for retransmission */
  uint32_t num_nacks_received; /* cum received ACKNACKs that did request retransmission */
  uint32_t throttle_count; /* cum times transmitting was throttled (whc hitting high-level mark) */
  uint32_t throttle_tracing;
  uint32_t rexmit_count; /* cum samples retransmitted (counting events; 1 sample can be counted many times) */
  uint32_t rexmit_lost_count; /* cum samples lost but retransmit requested (also counting events) */
  struct xeventq *evq; /* timed event queue to be used by this writer */
  struct local_reader_ary rdary; /* LOCAL readers for fast-pathing; if not fast-pathed, fall back to scanning local_readers */
};

struct reader
{
  struct entity_common e;
  struct endpoint_common c;
  status_cb_t status_cb;
  void * status_cb_entity;
  struct rhc * rhc; /* reader history, tracks registrations and data */
  struct nn_xqos *xqos;
  unsigned reliable: 1; /* 1 iff reader is reliable */
  unsigned handle_as_transient_local: 1; /* 1 iff reader wants historical data from proxy writers */
#ifdef DDSI_INCLUDE_SSM
  unsigned favours_ssm: 1; /* iff 1, this reader favours SSM */
#endif
  nn_count_t init_acknack_count; /* initial value for "count" (i.e. ACK seq num) for newly matched proxy writers */
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  struct addrset *as;
#endif
  const struct ddsi_sertopic * topic; /* topic is NULL for built-in readers */
  ut_avlTree_t writers; /* all matching PROXY writers, see struct rd_pwr_match */
  ut_avlTree_t local_writers; /* all matching LOCAL writers, see struct rd_wr_match */
  ddsi2direct_directread_cb_t ddsi2direct_cb;
  void *ddsi2direct_cbarg;
};

struct proxy_participant
{
  struct entity_common e;
  uint32_t refc; /* number of proxy endpoints (both user & built-in; not groups, they don't have a life of their own) */
  nn_vendorid_t vendor; /* vendor code from discovery */
  unsigned bes; /* built-in endpoint set */
  unsigned prismtech_bes; /* prismtech-specific extension of built-in endpoints set */
  nn_guid_t privileged_pp_guid; /* if this PP depends on another PP for its SEDP writing */
  nn_plist_t *plist; /* settings/QoS for this participant */
  os_atomic_voidp_t lease; /* lease object for this participant, for automatic leases */
  struct addrset *as_default; /* default address set to use for user data traffic */
  struct addrset *as_meta; /* default address set to use for discovery traffic */
  struct proxy_endpoint_common *endpoints; /* all proxy endpoints can be reached from here */
  ut_avlTree_t groups; /* table of all groups (publisher, subscriber), see struct proxy_group */
  unsigned kernel_sequence_numbers : 1; /* whether this proxy participant generates OSPL kernel sequence numbers */
  unsigned implicitly_created : 1; /* participants are implicitly created for Cloud/Fog discovered endpoints */
  unsigned is_ddsi2_pp: 1; /* if this is the federation-leader on the remote node */
  unsigned minimal_bes_mode: 1;
  unsigned lease_expired: 1;
  unsigned proxypp_have_spdp: 1;
  unsigned proxypp_have_cm: 1;
  unsigned owns_lease: 1;
};

/* Representing proxy subscriber & publishers as "groups": until DDSI2
   gets a reason to care about these other than for the generation of
   CM topics, there's little value in distinguishing between the two.
   In another way, they're secondly-class citizens, too: "real"
   entities are garbage collected and found using lock-free hash
   tables, but "groups" only live in the context of a proxy
   participant. */
struct proxy_group {
  ut_avlNode_t avlnode;
  nn_guid_t guid;
  char *name;
  struct proxy_participant *proxypp; /* uncounted backref to proxy participant */
  struct nn_xqos *xqos; /* publisher/subscriber QoS */
};

struct proxy_endpoint_common
{
  struct proxy_participant *proxypp; /* counted backref to proxy participant */
  struct proxy_endpoint_common *next_ep; /* next \ endpoint belonging to this proxy participant */
  struct proxy_endpoint_common *prev_ep; /* prev / -- this is in arbitrary ordering */
  struct nn_xqos *xqos; /* proxy endpoint QoS lives here; FIXME: local ones should have it moved to common as well */
  struct ddsi_sertopic * topic; /* topic may be NULL: for built-ins, but also for never-yet matched proxies (so we don't have to know the topic; when we match, we certainly do know) */
  struct addrset *as; /* address set to use for communicating with this endpoint */
  nn_guid_t group_guid; /* 0:0:0:0 if not available */
  nn_vendorid_t vendor; /* cached from proxypp->vendor */
};

struct proxy_writer {
  struct entity_common e;
  struct proxy_endpoint_common c;
  ut_avlTree_t readers; /* matching LOCAL readers, see pwr_rd_match */
  int n_reliable_readers; /* number of those that are reliable */
  int n_readers_out_of_sync; /* number of those that require special handling (accepting historical data, waiting for historical data set to become complete) */
  seqno_t last_seq; /* highest known seq published by the writer, not last delivered */
  uint32_t last_fragnum; /* last known frag for last_seq, or ~0u if last_seq not partial */
  nn_count_t nackfragcount; /* last nackfrag seq number */
  os_atomic_uint32_t next_deliv_seq_lowword; /* lower 32-bits for next sequence number that will be delivered; for generating acks; 32-bit so atomic reads on all supported platforms */
  unsigned last_fragnum_reset: 1; /* iff set, heartbeat advertising last_seq as highest seq resets last_fragnum */
  unsigned deliver_synchronously: 1; /* iff 1, delivery happens straight from receive thread for non-historical data; else through delivery queue "dqueue" */
  unsigned have_seen_heartbeat: 1; /* iff 1, we have received at least on heartbeat from this proxy writer */
  unsigned assert_pp_lease: 1; /* iff 1, renew the proxy-participant's lease when data comes in */
  unsigned local_matching_inprogress: 1; /* iff 1, we are still busy matching local readers; this is so we don't deliver incoming data to some but not all readers initially */
#ifdef DDSI_INCLUDE_SSM
  unsigned supports_ssm: 1; /* iff 1, this proxy writer supports SSM */
#endif
  struct nn_defrag *defrag; /* defragmenter for this proxy writer; FIXME: perhaps shouldn't be for historical data */
  struct nn_reorder *reorder; /* message reordering for this proxy writer, out-of-sync readers can have their own, see pwr_rd_match */
  struct nn_dqueue *dqueue; /* delivery queue for asynchronous delivery (historical data is always delivered asynchronously) */
  struct xeventq *evq; /* timed event queue to be used for ACK generation */
  struct local_reader_ary rdary; /* LOCAL readers for fast-pathing; if not fast-pathed, fall back to scanning local_readers */
  ddsi2direct_directread_cb_t ddsi2direct_cb;
  void *ddsi2direct_cbarg;
};

struct proxy_reader {
  struct entity_common e;
  struct proxy_endpoint_common c;
  unsigned deleting: 1; /* set when being deleted */
  unsigned is_fict_trans_reader: 1; /* only true when it is certain that is a fictitious transient data reader (affects built-in topic generation) */
  unsigned assert_pp_lease: 1; /* iff 1, renew the proxy-participant's lease when data comes in */
#ifdef DDSI_INCLUDE_SSM
  unsigned favours_ssm: 1; /* iff 1, this proxy reader favours SSM when available */
#endif
  ut_avlTree_t writers; /* matching LOCAL writers */
};

extern const ut_avlTreedef_t wr_readers_treedef;
extern const ut_avlTreedef_t wr_local_readers_treedef;
extern const ut_avlTreedef_t rd_writers_treedef;
extern const ut_avlTreedef_t rd_local_writers_treedef;
extern const ut_avlTreedef_t pwr_readers_treedef;
extern const ut_avlTreedef_t prd_writers_treedef;
extern const ut_avlTreedef_t deleted_participants_treedef;

#define DPG_LOCAL 1
#define DPG_REMOTE 2

int deleted_participants_admin_init (void);
void deleted_participants_admin_fini (void);
int is_deleted_participant_guid (const struct nn_guid *guid, unsigned for_what);

nn_entityid_t to_entityid (unsigned u);
int is_builtin_entityid (nn_entityid_t id, nn_vendorid_t vendorid);
int is_builtin_endpoint (nn_entityid_t id, nn_vendorid_t vendorid);
bool is_local_orphan_endpoint (const struct entity_common *e);
int is_writer_entityid (nn_entityid_t id);
int is_reader_entityid (nn_entityid_t id);
nn_vendorid_t get_entity_vendorid (const struct entity_common *e);

int pp_allocate_entityid (nn_entityid_t *id, unsigned kind, struct participant *pp);
void pp_release_entityid(struct participant *pp, nn_entityid_t id);

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

/* To create a DDSI participant given a GUID. May return ERR_OUT_OF_IDS
   (a.o.) */
int new_participant_guid (const nn_guid_t *ppguid, unsigned flags, const struct nn_plist *plist);

int new_participant (struct nn_guid *ppguid, unsigned flags, const struct nn_plist *plist);

/* To delete a DDSI participant: this only removes the participant
   from the hash tables and schedules the actual delete operation,
   which will start doing scary things once all but the DDSI built-in
   endpoints are gone.  It is acceptable to call delete_participant()
   before all its readers and writers have been deleted (which also
   fits nicely with model where the glue calls merely schedules
   garbage-collection). */
int delete_participant (const struct nn_guid *ppguid);

/* To obtain the builtin writer to be used for publishing SPDP, SEDP,
   PMD stuff for PP and its endpoints, given the entityid.  If PP has
   its own writer, use it; else use the privileged participant. */
struct writer *get_builtin_writer (const struct participant *pp, unsigned entityid);

/* To create a new DDSI writer or reader belonging to participant with
   GUID "ppguid". May return NULL if participant unknown or
   writer/reader already known. */

struct writer *new_writer (struct nn_guid *wrguid, const struct nn_guid *group_guid, const struct nn_guid *ppguid, const struct ddsi_sertopic *topic, const struct nn_xqos *xqos, struct whc * whc, status_cb_t status_cb, void *status_cb_arg);

struct reader *new_reader (struct nn_guid *rdguid, const struct nn_guid *group_guid, const struct nn_guid *ppguid, const struct ddsi_sertopic *topic, const struct nn_xqos *xqos, struct rhc * rhc, status_cb_t status_cb, void *status_cb_arg);

struct whc_node;
struct whc_state;
unsigned remove_acked_messages (struct writer *wr, struct whc_state *whcst, struct whc_node **deferred_free_list);
seqno_t writer_max_drop_seq (const struct writer *wr);
int writer_must_have_hb_scheduled (const struct writer *wr, const struct whc_state *whcst);
void writer_set_retransmitting (struct writer *wr);
void writer_clear_retransmitting (struct writer *wr);

int delete_writer (const struct nn_guid *guid);
int delete_writer_nolinger (const struct nn_guid *guid);
int delete_writer_nolinger_locked (struct writer *wr);

int delete_reader (const struct nn_guid *guid);
uint64_t reader_instance_id (const struct nn_guid *guid);

struct local_orphan_writer {
  struct writer wr;
};
struct local_orphan_writer *new_local_orphan_writer (nn_entityid_t entityid, struct ddsi_sertopic *topic, const struct nn_xqos *xqos, struct whc *whc);
void delete_local_orphan_writer (struct local_orphan_writer *wr);

/* To create or delete a new proxy participant: "guid" MUST have the
   pre-defined participant entity id. Unlike delete_participant(),
   deleting a proxy participant will automatically delete all its
   readers & writers. Delete removes the participant from a hash table
   and schedules the actual deletion.

      -- XX what about proxy participants without built-in endpoints?
      XX --
*/

/* Set this custom flag when using nn_prismtech_writer_info_t iso nn_prismtech_writer_info_old_t */
#define CF_INC_KERNEL_SEQUENCE_NUMBERS         (1 << 0)
/* Set when this proxy participant is created implicitly and has to be deleted upon disappearance
   of its last endpoint.  FIXME: Currently there is a potential race with adding a new endpoint
   in parallel to deleting the last remaining one. The endpoint will then be created, added to the
   proxy participant and then both are deleted. With the current single-threaded discovery
   this can only happen when it is all triggered by lease expiry. */
#define CF_IMPLICITLY_CREATED_PROXYPP          (1 << 1)
/* Set when this proxy participant is a DDSI2 participant, to help Cloud figure out whom to send
   discovery data when used in conjunction with the networking bridge */
#define CF_PARTICIPANT_IS_DDSI2                (1 << 2)
/* Set when this proxy participant is not to be announced on the built-in topics yet */
#define CF_PROXYPP_NO_SPDP                     (1 << 3)

void new_proxy_participant (const struct nn_guid *guid, unsigned bes, unsigned prismtech_bes, const struct nn_guid *privileged_pp_guid, struct addrset *as_default, struct addrset *as_meta, const struct nn_plist *plist, int64_t tlease_dur, nn_vendorid_t vendor, unsigned custom_flags, nn_wctime_t timestamp);
int delete_proxy_participant_by_guid (const struct nn_guid * guid, nn_wctime_t timestamp, int isimplicit);
uint64_t participant_instance_id (const struct nn_guid *guid);

enum update_proxy_participant_source {
  UPD_PROXYPP_SPDP,
  UPD_PROXYPP_CM
};

int update_proxy_participant_plist_locked (struct proxy_participant *proxypp, const struct nn_plist *datap, enum update_proxy_participant_source source, nn_wctime_t timestamp);
int update_proxy_participant_plist (struct proxy_participant *proxypp, const struct nn_plist *datap, enum update_proxy_participant_source source, nn_wctime_t timestamp);
void proxy_participant_reassign_lease (struct proxy_participant *proxypp, struct lease *newlease);

void purge_proxy_participants (const nn_locator_t *loc, bool delete_from_as_disc);

/* To create a new proxy writer or reader; the proxy participant is
   determined from the GUID and must exist. */
int new_proxy_writer (const struct nn_guid *ppguid, const struct nn_guid *guid, struct addrset *as, const struct nn_plist *plist, struct nn_dqueue *dqueue, struct xeventq *evq, nn_wctime_t timestamp);
int new_proxy_reader (const struct nn_guid *ppguid, const struct nn_guid *guid, struct addrset *as, const struct nn_plist *plist, nn_wctime_t timestamp
#ifdef DDSI_INCLUDE_SSM
                      , int favours_ssm
#endif
                      );

/* To delete a proxy writer or reader; these synchronously hide it
   from the outside world, preventing it from being matched to a
   reader or writer. Actual deletion is scheduled in the future, when
   no outstanding references may still exist (determined by checking
   thread progress, &c.). */
int delete_proxy_writer (const struct nn_guid *guid, nn_wctime_t timestamp, int isimplicit);
int delete_proxy_reader (const struct nn_guid *guid, nn_wctime_t timestamp, int isimplicit);

void update_proxy_reader (struct proxy_reader * prd, struct addrset *as);
void update_proxy_writer (struct proxy_writer * pwr, struct addrset *as);

int new_proxy_group (const struct nn_guid *guid, const char *name, const struct nn_xqos *xqos, nn_wctime_t timestamp);
void delete_proxy_group (const struct nn_guid *guid, nn_wctime_t timestamp, int isimplicit);

void writer_exit_startup_mode (struct writer *wr);
uint64_t writer_instance_id (const struct nn_guid *guid);

/* Call this to empty all address sets of all writers to stop all outgoing traffic, or to
   rebuild them all (which only makes sense after previously having emptied them all). */
void rebuild_or_clear_writer_addrsets(int rebuild);

#if defined (__cplusplus)
}
#endif

#endif /* Q_ENTITY_H */
