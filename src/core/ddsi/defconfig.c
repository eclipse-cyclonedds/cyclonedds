#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include "dds/ddsi/ddsi_config.h"

void ddsi_config_init_default (struct ddsi_config *cfg)
{
  memset (cfg, 0, sizeof (*cfg));
  static char *networkRecvAddressStrings_init_[] = {
    "preferred",
    NULL
  };
  cfg->networkRecvAddressStrings = networkRecvAddressStrings_init_;
  cfg->externalMaskString = "0.0.0.0";
  cfg->allowMulticast = UINT32_C (2147483648);
  cfg->multicast_ttl = INT32_C (32);
  cfg->transport_selector = INT32_C (1);
  cfg->enableMulticastLoopback = INT32_C (1);
  cfg->max_msg_size = UINT32_C (14720);
  cfg->max_rexmit_msg_size = UINT32_C (1456);
  cfg->fragment_size = UINT16_C (1344);
#ifdef DDS_HAS_SECURITY
#endif /* DDS_HAS_SECURITY */
#ifdef DDS_HAS_NETWORK_PARTITIONS
#endif /* DDS_HAS_NETWORK_PARTITIONS */
  cfg->rbuf_size = UINT32_C (1048576);
  cfg->rmsg_chunk_size = UINT32_C (131072);
  cfg->standards_conformance = INT32_C (2);
  cfg->many_sockets_mode = INT32_C (1);
  cfg->domainTag = "";
  cfg->extDomainId.isdefault = 1;
  cfg->ds_grace_period = INT64_C (30000000000);
  cfg->participantIndex = INT32_C (-3);
  cfg->maxAutoParticipantIndex = INT32_C (9);
  cfg->spdpMulticastAddressString = "239.255.0.1";
  cfg->spdp_interval.isdefault = 1;
  cfg->ports.base = UINT32_C (7400);
  cfg->ports.dg = UINT32_C (250);
  cfg->ports.pg = UINT32_C (2);
  cfg->ports.d1 = UINT32_C (10);
  cfg->ports.d2 = UINT32_C (1);
  cfg->ports.d3 = UINT32_C (11);
#ifdef DDS_HAS_TOPIC_DISCOVERY
#endif /* DDS_HAS_TOPIC_DISCOVERY */
  cfg->lease_duration = INT64_C (10000000000);
  cfg->tracefile = "cyclonedds.log";
  cfg->pcap_file = "";
  cfg->delivery_queue_maxsamples = UINT32_C (256);
  cfg->primary_reorder_maxsamples = UINT32_C (128);
  cfg->secondary_reorder_maxsamples = UINT32_C (128);
  cfg->defrag_unreliable_maxsamples = UINT32_C (4);
  cfg->defrag_reliable_maxsamples = UINT32_C (16);
  cfg->besmode = INT32_C (1);
  cfg->synchronous_delivery_latency_bound = INT64_C (9223372036854775807);
  cfg->retransmit_merging_period = INT64_C (5000000);
  cfg->const_hb_intv_sched = INT64_C (100000000);
  cfg->const_hb_intv_min = INT64_C (5000000);
  cfg->const_hb_intv_sched_min = INT64_C (20000000);
  cfg->const_hb_intv_sched_max = INT64_C (8000000000);
  cfg->max_queued_rexmit_bytes = UINT32_C (524288);
  cfg->max_queued_rexmit_msgs = UINT32_C (200);
  cfg->writer_linger_duration = INT64_C (1000000000);
  cfg->socket_rcvbuf_size.min.isdefault = 1;
  cfg->socket_rcvbuf_size.max.isdefault = 1;
  cfg->socket_sndbuf_size.min.isdefault = 0;
  cfg->socket_sndbuf_size.min.value = UINT32_C (65536);
  cfg->socket_sndbuf_size.max.isdefault = 1;
  cfg->nack_delay = INT64_C (100000000);
  cfg->ack_delay = INT64_C (10000000);
  cfg->auto_resched_nack_delay = INT64_C (3000000000);
  cfg->preemptive_ack_delay = INT64_C (10000000);
  cfg->max_sample_size = UINT32_C (2147483647);
  cfg->noprogress_log_stacktraces = INT32_C (1);
  cfg->liveliness_monitoring_interval = INT64_C (1000000000);
  cfg->monitor_port = INT32_C (-1);
  cfg->prioritize_retransmit = INT32_C (1);
  cfg->recv_thread_stop_maxretries = UINT32_C (4294967295);
  cfg->whc_lowwater_mark = UINT32_C (1024);
  cfg->whc_highwater_mark = UINT32_C (512000);
  cfg->whc_init_highwater_mark.isdefault = 0;
  cfg->whc_init_highwater_mark.value = UINT32_C (30720);
  cfg->whc_adaptive = INT32_C (1);
  cfg->max_rexmit_burst_size = UINT32_C (1048576);
  cfg->init_transmit_extra_pct = UINT32_C (4294967295);
  cfg->max_frags_in_rexmit_of_sample = UINT32_C (1);
  cfg->extended_packet_info = INT32_C (1);
  cfg->tcp_nodelay = INT32_C (1);
  cfg->tcp_port = INT32_C (-1);
  cfg->tcp_read_timeout = INT64_C (2000000000);
  cfg->tcp_write_timeout = INT64_C (2000000000);
#ifdef DDS_HAS_TCP_TLS
  cfg->ssl_verify = INT32_C (1);
  cfg->ssl_verify_client = INT32_C (1);
  cfg->ssl_keystore = "keystore";
  cfg->ssl_key_pass = "secret";
  cfg->ssl_ciphers = "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH";
  cfg->ssl_rand_file = "";
  cfg->ssl_min_version.major = 1;
  cfg->ssl_min_version.minor = 3;
#endif /* DDS_HAS_TCP_TLS */
}
/* generated from ddsi_config.h[007a7968df8cbc42a122109bd139ac85bab0f6c9] */
/* generated from ddsi__cfgunits.h[bd22f0c0ed210501d0ecd3b07c992eca549ef5aa] */
/* generated from ddsi__cfgelems.h[607a8f573eb5d87d6f93b1d9bce2947f29da56dc] */
/* generated from ddsi_config.c[d4ef67f90737b1bf8ae94ad932774fa015e3a2cf] */
/* generated from _confgen.h[e32eabfc35e9f3a7dcb63b19ed148c0d17c6e5fc] */
/* generated from _confgen.c[237308acd53897a34e8c643e16e05a61d73ffd65] */
/* generated from generate_rnc.c[b50e4b7ab1d04b2bc1d361a0811247c337b74934] */
/* generated from generate_md.c[789b92e422631684352909cfb8bf43f6ceb16a01] */
/* generated from generate_rst.c[3c4b523fbb57c8e4a7e247379d06a8021ccc21c4] */
/* generated from generate_xsd.c[6b6818d7f17a35d56c376c04ec1410427f34c0f0] */
/* generated from generate_defconfig.c[63ca9d8ae2f1ce2e761c9d4c0510a45eb062d830] */
