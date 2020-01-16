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
#ifndef Q_GLOBALS_H
#define Q_GLOBALS_H

#include <stdio.h>

#include "dds/export.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/fibheap.h"

#include "dds/ddsi/q_plist.h"
#include "dds/ddsi/q_protocol.h"
#include "dds/ddsi/q_nwif.h"
#include "dds/ddsi/q_sockwaitset.h"
#include "dds/ddsi/q_config.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct nn_xmsgpool;
struct serdatapool;
struct nn_dqueue;
struct nn_reorder;
struct nn_defrag;
struct addrset;
struct xeventq;
struct gcreq_queue;
struct entity_index;
struct lease;
struct ddsi_tran_conn;
struct ddsi_tran_listener;
struct ddsi_tran_factory;
struct ddsrt_thread_pool_s;
struct debug_monitor;
struct ddsi_tkmap;
struct dds_security_context;

typedef struct config_in_addr_node {
   nn_locator_t loc;
   struct config_in_addr_node *next;
} config_in_addr_node;

enum recvips_mode {
  RECVIPS_MODE_ALL,             /* all MC capable interfaces */
  RECVIPS_MODE_ANY,             /* kernel-default interface */
  RECVIPS_MODE_PREFERRED,       /* selected interface only */
  RECVIPS_MODE_NONE,            /* no interfaces at all */
  RECVIPS_MODE_SOME             /* explicit list of interfaces; only one requiring recvips */
};

enum recv_thread_mode {
  RTM_SINGLE,
  RTM_MANY
};

struct recv_thread_arg {
  enum recv_thread_mode mode;
  struct nn_rbufpool *rbpool;
  struct q_globals *gv;
  union {
    struct {
      const nn_locator_t *loc;
      struct ddsi_tran_conn *conn;
    } single;
    struct {
      os_sockWaitset ws;
    } many;
  } u;
};

struct deleted_participants_admin;

struct q_globals {
  volatile int terminate;
  volatile int deaf;
  volatile int mute;

  struct ddsrt_log_cfg logconfig;
  struct config config;

  struct ddsi_tkmap * m_tkmap;

  /* Hash tables for participants, readers, writers, proxy
     participants, proxy readers and proxy writers by GUID. */
  struct entity_index *entity_index;

  /* Timed events admin */
  struct xeventq *xevents;

  /* Queue for garbage collection requests */
  struct gcreq_queue *gcreq_queue;

  /* Lease junk */
  ddsrt_mutex_t leaseheap_lock;
  ddsrt_fibheap_t leaseheap;

  /* Transport factories & selected factory */
  struct ddsi_tran_factory *ddsi_tran_factories;
  struct ddsi_tran_factory *m_factory;

  /* Connections for multicast discovery & data, and those that correspond
     to the one DDSI participant index that the DDSI2 service uses. The
     DCPS participant of DDSI2 itself will be mirrored in a DDSI
     participant, and in multi-socket mode that one gets its own
     socket. */

  struct ddsi_tran_conn * disc_conn_mc;
  struct ddsi_tran_conn * data_conn_mc;
  struct ddsi_tran_conn * disc_conn_uc;
  struct ddsi_tran_conn * data_conn_uc;

  /* TCP listener */

  struct ddsi_tran_listener * listener;

  /* Thread pool */

  struct ddsrt_thread_pool_s * thread_pool;

  /* In many sockets mode, the receive threads maintain a local array
     with participant GUIDs and sockets, participant_set_generation is
     used to notify them. */
  ddsrt_atomic_uint32_t participant_set_generation;

  /* nparticipants is used primarily for limiting the number of active
     participants, but also during shutdown to determine when it is
     safe to stop the GC thread. */
  ddsrt_mutex_t participant_set_lock;
  ddsrt_cond_t participant_set_cond;
  uint32_t nparticipants;

  /* For participants without (some) built-in writers, we fall back to
     this participant, which is the first one created with all
     built-in writers present.  It MUST be created before any in need
     of it pops up! */
  struct participant *privileged_pp;
  ddsrt_mutex_t privileged_pp_lock;

  /* For tracking (recently) deleted participants */
  struct deleted_participants_admin *deleted_participants;

  /* GUID to be used in next call to new_participant; also protected
     by privileged_pp_lock */
  struct ddsi_guid ppguid_base;

  /* number of up, non-loopback, IPv4/IPv6 interfaces, the index of
     the selected/preferred one, and the discovered interfaces. */
  int n_interfaces;
  int selected_interface;
  struct nn_interface interfaces[MAX_INTERFACES];

#if DDSRT_HAVE_IPV6
  /* whether we're using an IPv6 link-local address (and therefore
     only listening to multicasts on that interface) */
  int ipv6_link_local;
#endif

  /* Addressing: actual own (preferred) IP address, IP address
     advertised in discovery messages (so that an external IP address on
     a NAT may be advertised), and the DDSI multi-cast address. */
  enum recvips_mode recvips_mode;
  struct config_in_addr_node *recvips;
  nn_locator_t extmask;

  nn_locator_t ownloc;
  nn_locator_t extloc;

  /* InterfaceNo that the OwnIP is tied to */
  unsigned interfaceNo;

  /* Locators */

  nn_locator_t loc_spdp_mc;
  nn_locator_t loc_meta_mc;
  nn_locator_t loc_meta_uc;
  nn_locator_t loc_default_mc;
  nn_locator_t loc_default_uc;

  /*
    Initial discovery address set, and the current discovery address
    set. These are the addresses that SPDP pings get sent to. The
    as_disc_group is an FT group (only use first working).
  */
  struct addrset *as_disc;
  struct addrset *as_disc_group;

  ddsrt_mutex_t lock;

  /* Receive thread. (We can only has one for now, cos of the signal
     trigger socket.) Receive buffer pool is per receive thread,
     it is only a global variable because it needs to be freed way later
     than the receive thread itself terminates */
#define MAX_RECV_THREADS 3
  uint32_t n_recv_threads;
  struct recv_thread {
    const char *name;
    struct thread_state1 *ts;
    struct recv_thread_arg arg;
  } recv_threads[MAX_RECV_THREADS];

  /* Listener thread for connection based transports */
  struct thread_state1 *listen_ts;

  /* Flag cleared when stopping (receive threads). FIXME. */
  ddsrt_atomic_uint32_t rtps_keepgoing;

  /* Start time of the DDSI2 service, for logging relative time stamps,
     should I ever so desire. */
  nn_wctime_t tstart;

  /* Default QoSs for participant, readers and writers (needed for
     eliminating default values in outgoing discovery packets, and for
     supplying values for missing QoS settings in incoming discovery
     packets); plus the actual QoSs needed for the builtin
     endpoints. */
  nn_plist_t default_plist_pp;
  nn_plist_t default_local_plist_pp;
  dds_qos_t default_xqos_rd;
  dds_qos_t default_xqos_wr;
  dds_qos_t default_xqos_wr_nad;
  dds_qos_t default_xqos_tp;
  dds_qos_t default_xqos_sub;
  dds_qos_t default_xqos_pub;
  dds_qos_t spdp_endpoint_xqos;
  dds_qos_t builtin_endpoint_xqos_rd;
  dds_qos_t builtin_endpoint_xqos_wr;
#ifdef DDSI_INCLUDE_SECURITY
  dds_qos_t builtin_volatile_xqos_rd;
  dds_qos_t builtin_volatile_xqos_wr;
  dds_qos_t builtin_stateless_xqos_rd;
  dds_qos_t builtin_stateless_xqos_wr;
#endif

  /* SPDP packets get very special treatment (they're the only packets
     we accept from writers we don't know) and have their very own
     do-nothing defragmentation and reordering thingummies, as well as a
     global mutex to in lieu of the proxy writer lock. */
  ddsrt_mutex_t spdp_lock;
  struct nn_defrag *spdp_defrag;
  struct nn_reorder *spdp_reorder;

  /* Built-in stuff other than SPDP gets funneled through the builtins
     delivery queue; currently just SEDP and PMD */
  struct nn_dqueue *builtins_dqueue;

  /* Connection used by general timed-event queue for transmitting data */

  struct ddsi_tran_conn * tev_conn;

  struct debug_monitor *debmon;

#ifndef DDSI_INCLUDE_NETWORK_CHANNELS
  uint32_t networkQueueId;
  struct thread_state1 *channel_reader_ts;

  /* Application data gets its own delivery queue */
  struct nn_dqueue *user_dqueue;
#endif

  /* Transmit side: pools for the serializer & transmit messages and a
     transmit queue*/
  struct serdatapool *serpool;
  struct nn_xmsgpool *xmsgpool;
  struct ddsi_sertopic *plist_topic; /* used for all discovery data */
  struct ddsi_sertopic *rawcdr_topic; /* used for participant message data */

  /* Network ID needed by v_groupWrite -- FIXME: might as well pass it
     to the receive thread instead of making it global (and that would
     remove the need to include kernelModule.h) */
  uint32_t myNetworkId;

  ddsrt_mutex_t sendq_lock;
  ddsrt_cond_t sendq_cond;
  unsigned sendq_length;
  struct nn_xpack *sendq_head;
  struct nn_xpack *sendq_tail;
  int sendq_stop;
  struct thread_state1 *sendq_ts;

  /* File for dumping captured packets, NULL if disabled */
  FILE *pcap_fp;
  ddsrt_mutex_t pcap_lock;

  struct ddsi_builtin_topic_interface *builtin_topic_interface;

  struct nn_group_membership *mship;

  /* security globals */
#ifdef DDSI_INCLUDE_SECURITY
  struct dds_security_context *security_context;
#endif

};

#if defined (__cplusplus)
}
#endif

#endif /* Q_GLOBALS_H */
