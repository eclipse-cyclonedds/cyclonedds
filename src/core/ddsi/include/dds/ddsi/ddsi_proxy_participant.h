// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_PROXY_PARTICIPANT_H
#define DDSI_PROXY_PARTICIPANT_H

#include "dds/export.h"
#include "dds/features.h"

#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/fibheap.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_topic.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_lease;
struct ddsi_plist;
struct ddsi_addrset;
struct ddsi_proxy_endpoint_common;

struct ddsi_proxy_participant
{
  struct ddsi_entity_common e;
  uint32_t refc; /* number of proxy endpoints (both user & built-in; not groups, they don't have a life of their own) */
  ddsi_vendorid_t vendor; /* vendor code from discovery */
  unsigned bes; /* built-in endpoint set */
  ddsi_guid_t privileged_pp_guid; /* if this PP depends on another PP for its SEDP writing */
  struct ddsi_plist *plist; /* settings/QoS for this participant */
  ddsrt_atomic_voidp_t minl_auto; /* clone of min(leaseheap_auto) */
  ddsrt_fibheap_t leaseheap_auto; /* keeps leases for this proxypp and leases for pwrs (with liveliness automatic) */
  ddsrt_atomic_voidp_t minl_man; /* clone of min(leaseheap_man) */
  ddsrt_fibheap_t leaseheap_man; /* keeps leases for this proxypp and leases for pwrs (with liveliness manual-by-participant) */
  struct ddsi_lease *lease; /* lease for this proxypp */
  struct ddsi_addrset *as_default; /* default address set to use for user data traffic */
  struct ddsi_addrset *as_meta; /* default address set to use for discovery traffic */
  struct ddsi_proxy_endpoint_common *endpoints; /* all proxy endpoints can be reached from here */
#ifdef DDS_HAS_TOPIC_DISCOVERY
  ddsrt_avl_tree_t topics;
#endif
  ddsi_seqno_t seq; /* sequence number of most recent SPDP message */
  uint32_t receive_buffer_size; /* assumed size of receive buffer, used to limit bursts involving this proxypp */
  unsigned implicitly_created : 1; /* participants are implicitly created for Cloud/Fog discovered endpoints */
  unsigned is_ddsi2_pp: 1; /* if this is the federation-leader on the remote node */
  unsigned minimal_bes_mode: 1;
  unsigned lease_expired: 1;
  unsigned deleting: 1;
  unsigned proxypp_have_spdp: 1;
  unsigned owns_lease: 1;
  unsigned redundant_networking: 1; /* 1 iff requests receiving data on all advertised interfaces */
#ifdef DDS_HAS_SECURITY
  ddsi_security_info_t security_info;
  struct ddsi_proxy_participant_sec_attributes *sec_attr;
#endif
};

#ifdef DDS_HAS_TOPIC_DISCOVERY
extern const ddsrt_avl_treedef_t ddsi_proxypp_proxytp_treedef;
#endif

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_PROXY_PARTICIPANT_H */
