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
#ifndef NN_CONFIG_H
#define NN_CONFIG_H

#include "dds/ddsi/q_log.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsi/q_feature_check.h"
#include "dds/ddsi/ddsi_portmapping.h"

#if defined (__cplusplus)
extern "C" {
#endif

enum nn_standards_conformance {
  NN_SC_PEDANTIC,
  NN_SC_STRICT,
  NN_SC_LAX
};

#define NN_PEDANTIC_P(config) ((config).standards_conformance <= NN_SC_PEDANTIC)
#define NN_STRICT_P(config) ((config).standards_conformance <= NN_SC_STRICT)

enum besmode {
  BESMODE_FULL,
  BESMODE_WRITERS,
  BESMODE_MINIMAL
};

enum retransmit_merging {
  REXMIT_MERGE_NEVER,
  REXMIT_MERGE_ADAPTIVE,
  REXMIT_MERGE_ALWAYS
};

enum boolean_default {
  BOOLDEF_DEFAULT,
  BOOLDEF_FALSE,
  BOOLDEF_TRUE
};

#define PARTICIPANT_INDEX_AUTO -1
#define PARTICIPANT_INDEX_NONE -2

/* config_listelem must be an overlay for all used listelem types */
struct config_listelem {
  struct config_listelem *next;
};

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
struct config_networkpartition_listelem {
  struct config_networkpartition_listelem *next;
  char *name;
  char *address_string;
  struct addrset *as;
  int connected;
  uint32_t partitionId;
};

struct config_ignoredpartition_listelem {
  struct config_ignoredpartition_listelem *next;
  char *DCPSPartitionTopic;
};

struct config_partitionmapping_listelem {
  struct config_partitionmapping_listelem *next;
  char *networkPartition;
  char *DCPSPartitionTopic;
  struct config_networkpartition_listelem *partition;
};
#endif /* DDSI_INCLUDE_NETWORK_PARTITIONS */

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
struct config_channel_listelem {
  struct config_channel_listelem *next;
  char   *name;
  int    priority;
  int64_t resolution;
#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
  uint32_t data_bandwidth_limit;
  uint32_t auxiliary_bandwidth_limit;
#endif
  int    diffserv_field;
  struct thread_state1 *channel_reader_ts;  /* keeping an handle to the running thread for this channel */
  struct nn_dqueue *dqueue; /* The handle of teh delivery queue servicing incoming data for this channel*/
  struct xeventq *evq; /* The handle of the event queue servicing this channel*/
  uint32_t queueId; /* the index of the networkqueue serviced by this channel*/
  struct ddsi_tran_conn * transmit_conn; /* the connection used for sending data out via this channel */
};
#endif /* DDSI_INCLUDE_NETWORK_CHANNELS */

struct config_maybe_int32 {
  int isdefault;
  int32_t value;
};

struct config_maybe_uint32 {
  int isdefault;
  uint32_t value;
};

struct config_maybe_int64 {
  int isdefault;
  int64_t value;
};

struct config_thread_properties_listelem {
  struct config_thread_properties_listelem *next;
  char *name;
  ddsrt_sched_t sched_class;
  struct config_maybe_int32 sched_priority;
  struct config_maybe_uint32 stack_size;
};

struct config_peer_listelem
{
  struct config_peer_listelem *next;
  char *peer;
};

struct prune_deleted_ppant {
  int64_t delay;
  int enforce_delay;
};

/* allow multicast bits (default depends on network type): */
#define AMC_FALSE 0u
#define AMC_SPDP 1u
#define AMC_ASM 2u
#ifdef DDSI_INCLUDE_SSM
#define AMC_SSM 4u
#define AMC_TRUE (AMC_SPDP | AMC_ASM | AMC_SSM)
#else
#define AMC_TRUE (AMC_SPDP | AMC_ASM)
#endif
#define AMC_DEFAULT 0x80000000u

/* FIXME: this should be fully dynamic ... but this is easier for a quick hack */
enum transport_selector {
  TRANS_DEFAULT, /* actually UDP, but this is so we can tell what has been set */
  TRANS_UDP,
  TRANS_UDP6,
  TRANS_TCP,
  TRANS_TCP6,
  TRANS_RAWETH
};

enum many_sockets_mode {
  MSM_NO_UNICAST,
  MSM_SINGLE_UNICAST,
  MSM_MANY_UNICAST
};

#ifdef DDSI_INCLUDE_SECURITY
typedef struct plugin_library_properties_type{
  char *library_path;
  char *library_init;
  char *library_finalize;
} plugin_library_properties_type;

typedef struct authentication_properties_type{
  char *identity_certificate;
  char *identity_ca;
  char *private_key;
  char *password;
  char *trusted_ca_dir;
  int include_optional_fields;
} authentication_properties_type;

typedef struct access_control_properties_type{
  char *permissions;
  char *permissions_ca;
  char *governance;
} access_control_properties_type;

typedef struct omg_security_configuration_type {
  authentication_properties_type authentication_properties;
  access_control_properties_type access_control_properties;
  plugin_library_properties_type authentication_plugin;
  plugin_library_properties_type access_control_plugin;
  plugin_library_properties_type cryptography_plugin;
} omg_security_configuration_type;

struct config_omg_security_listelem {
  struct config_omg_security_listelem *next;
  omg_security_configuration_type cfg;
};
#endif /* DDSI_INCLUDE_SECURITY */

#ifdef DDSI_INCLUDE_SSL
struct ssl_min_version {
  int major;
  int minor;
};
#endif

/* Expensive checks (compiled in when NDEBUG not defined, enabled only if flag set in xchecks) */
#define DDS_XCHECK_WHC 1u
#define DDS_XCHECK_RHC 2u

struct config
{
  int valid;
  uint32_t tracemask;
  uint32_t enabled_xchecks;
  char *servicename;
  char *pcap_file;

  char *networkAddressString;
  char **networkRecvAddressStrings;
  char *externalAddressString;
  char *externalMaskString;
  FILE *tracefp;
  char *tracefile;
  int tracingTimestamps;
  int tracingAppendToFile;
  uint32_t allowMulticast;
  int prefer_multicast;
  enum transport_selector transport_selector;
  enum boolean_default compat_use_ipv6;
  enum boolean_default compat_tcp_enable;
  int dontRoute;
  int enableMulticastLoopback;
  uint32_t domainId;
  struct config_maybe_uint32 extDomainId; // domain id advertised in discovery
  char *domainTag;
  int participantIndex;
  int maxAutoParticipantIndex;
  char *spdpMulticastAddressString;
  char *defaultMulticastAddressString;
  char *assumeMulticastCapable;
  int64_t spdp_interval;
  int64_t spdp_response_delay_max;
  int64_t lease_duration;
  int64_t const_hb_intv_sched;
  int64_t const_hb_intv_sched_min;
  int64_t const_hb_intv_sched_max;
  int64_t const_hb_intv_min;
  enum retransmit_merging retransmit_merging;
  int64_t retransmit_merging_period;
  int squash_participants;
  int liveliness_monitoring;
  int noprogress_log_stacktraces;
  int64_t liveliness_monitoring_interval;
  int prioritize_retransmit;
  int xpack_send_async;
  enum boolean_default multiple_recv_threads;
  unsigned recv_thread_stop_maxretries;

  unsigned primary_reorder_maxsamples;
  unsigned secondary_reorder_maxsamples;

  unsigned delivery_queue_maxsamples;

  uint16_t fragment_size;
  uint32_t max_msg_size;
  uint32_t max_rexmit_msg_size;
  uint32_t init_transmit_extra_pct;
  uint32_t max_rexmit_burst_size;

  int publish_uc_locators; /* Publish discovery unicast locators */
  int enable_uc_locators; /* If false, don't even try to create a unicast socket */

  /* TCP transport configuration */
  int tcp_nodelay;
  int tcp_port;
  int64_t tcp_read_timeout;
  int64_t tcp_write_timeout;
  int tcp_use_peeraddr_for_unicast;

#ifdef DDSI_INCLUDE_SSL
  /* SSL support for TCP */
  int ssl_enable;
  int ssl_verify;
  int ssl_verify_client;
  int ssl_self_signed;
  char * ssl_keystore;
  char * ssl_rand_file;
  char * ssl_key_pass;
  char * ssl_ciphers;
  struct ssl_min_version ssl_min_version;
#endif

  /* Thread pool configuration */
  int tp_enable;
  uint32_t tp_threads;
  uint32_t tp_max_threads;

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  struct config_channel_listelem *channels;
  struct config_channel_listelem *max_channel; /* channel with highest prio; always computed */
#endif /* DDSI_INCLUDE_NETWORK_CHANNELS */
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  struct config_networkpartition_listelem *networkPartitions;
  unsigned nof_networkPartitions;
  struct config_ignoredpartition_listelem *ignoredPartitions;
  struct config_partitionmapping_listelem *partitionMappings;
#endif /* DDSI_INCLUDE_NETWORK_PARTITIONS */
  struct config_peer_listelem *peers;
  struct config_peer_listelem *peers_group;
  struct config_thread_properties_listelem *thread_properties;

  /* debug/test/undoc features: */
  int xmit_lossiness;           /**<< fraction of packets to drop on xmit, in units of 1e-3 */
  uint32_t rmsg_chunk_size;          /**<< size of a chunk in the receive buffer */
  uint32_t rbuf_size;                /* << size of a single receiver buffer */
  enum besmode besmode;
  int meas_hb_to_ack_latency;
  int unicast_response_to_spdp_messages;
  int synchronous_delivery_priority_threshold;
  int64_t synchronous_delivery_latency_bound;

  /* Write cache */

  int whc_batch;
  uint32_t whc_lowwater_mark;
  uint32_t whc_highwater_mark;
  struct config_maybe_uint32 whc_init_highwater_mark;
  int whc_adaptive;

  unsigned defrag_unreliable_maxsamples;
  unsigned defrag_reliable_maxsamples;
  unsigned accelerate_rexmit_block_size;
  int64_t responsiveness_timeout;
  uint32_t max_participants;
  int64_t writer_linger_duration;
  int multicast_ttl;
  struct config_maybe_uint32 socket_min_rcvbuf_size;
  uint32_t socket_min_sndbuf_size;
  int64_t ack_delay;
  int64_t nack_delay;
  int64_t preemptive_ack_delay;
  int64_t schedule_time_rounding;
  int64_t auto_resched_nack_delay;
  int64_t ds_grace_period;
#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
  uint32_t auxiliary_bandwidth_limit; /* bytes/second */
#endif
  uint32_t max_queued_rexmit_bytes;
  unsigned max_queued_rexmit_msgs;
  unsigned ddsi2direct_max_threads;
  int late_ack_mode;
  int retry_on_reject_besteffort;
  int generate_keyhash;
  uint32_t max_sample_size;

  /* compability options */
  enum nn_standards_conformance standards_conformance;
  int explicitly_publish_qos_set_to_default;
  enum many_sockets_mode many_sockets_mode;
  int assume_rti_has_pmd_endpoints;

  struct ddsi_portmapping ports;

  int monitor_port;

  int enable_control_topic;
  int initial_deaf;
  int initial_mute;
  int64_t initial_deaf_mute_reset;

  int use_multicast_if_mreqn;
  struct prune_deleted_ppant prune_deleted_ppant;

#ifdef DDSI_INCLUDE_SECURITY
  struct config_omg_security_listelem *omg_security_configuration;
#endif
};

struct cfgst;

struct cfgst *config_init (const char *config, struct config *cfg, uint32_t domid) ddsrt_nonnull((1,2));
void config_print_cfgst (struct cfgst *cfgst, const struct ddsrt_log_cfg *logcfg);
void config_free_source_info (struct cfgst *cfgst);
void config_fini (struct cfgst *cfgst);

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
struct config_partitionmapping_listelem *find_partitionmapping (const struct config *cfg, const char *partition, const char *topic);
struct config_networkpartition_listelem *find_networkpartition_by_id (const struct config *cfg, uint32_t id);
int is_ignored_partition (const struct config *cfg, const char *partition, const char *topic);
#endif
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
struct config_channel_listelem *find_channel (const struct config *cfg, nn_transport_priority_qospolicy_t transport_priority);
#endif

#if defined (__cplusplus)
}
#endif

#endif /* NN_CONFIG_H */
