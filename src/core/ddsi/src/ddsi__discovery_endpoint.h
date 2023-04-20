// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__DISCOVERY_ENDPOINT_H
#define DDSI__DISCOVERY_ENDPOINT_H

#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__discovery.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_participant;
struct ddsi_topic;
struct ddsi_writer;
struct ddsi_reader;
struct ddsi_rsample_info;
struct ddsi_rdata;
struct ddsi_plist;
struct ddsi_xevent;
struct ddsi_xpack;
struct ddsi_domaingv;
struct ddsi_receiver_state;

/** @component discovery */
struct ddsi_addrset *ddsi_get_endpoint_addrset (const struct ddsi_domaingv *gv, const ddsi_plist_t *datap, struct ddsi_addrset *proxypp_as_default, const ddsi_locator_t *rst_srcloc)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull((1,2,3));

/** @component discovery */
int ddsi_sedp_write_writer (struct ddsi_writer *wr) ddsrt_nonnull_all;

/** @component discovery */
int ddsi_sedp_write_reader (struct ddsi_reader *rd) ddsrt_nonnull_all;

/** @component discovery */
int ddsi_sedp_dispose_unregister_writer (struct ddsi_writer *wr) ddsrt_nonnull_all;

/** @component discovery */
int ddsi_sedp_dispose_unregister_reader (struct ddsi_reader *rd) ddsrt_nonnull_all;

/** @component discovery */
void ddsi_handle_sedp_alive_endpoint (const struct ddsi_receiver_state *rst, ddsi_seqno_t seq, ddsi_plist_t *datap /* note: potentially modifies datap */, ddsi_sedp_kind_t sedp_kind, const ddsi_guid_prefix_t *src_guid_prefix, ddsi_vendorid_t vendorid, ddsrt_wctime_t timestamp)
  ddsrt_nonnull_all;

/** @component discovery */
void ddsi_handle_sedp_dead_endpoint (const struct ddsi_receiver_state *rst, ddsi_plist_t *datap, ddsi_sedp_kind_t sedp_kind, ddsrt_wctime_t timestamp)
  ddsrt_nonnull_all;

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__DISCOVERY_ENDPOINT_H */
