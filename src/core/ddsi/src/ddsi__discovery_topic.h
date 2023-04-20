// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__DISCOVERY_TOPIC_H
#define DDSI__DISCOVERY_TOPIC_H

#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_domaingv.h"

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
int ddsi_sedp_write_topic (struct ddsi_topic *tp, bool alive) ddsrt_nonnull_all;

/** @component discovery */
void ddsi_handle_sedp_alive_topic (const struct ddsi_receiver_state *rst, ddsi_seqno_t seq, ddsi_plist_t *datap /* note: potentially modifies datap */, const ddsi_guid_prefix_t *src_guid_prefix, ddsi_vendorid_t vendorid, ddsrt_wctime_t timestamp)
  ddsrt_nonnull_all;

/** @component discovery */
void ddsi_handle_sedp_dead_topic (const struct ddsi_receiver_state *rst, ddsi_plist_t *datap, ddsrt_wctime_t timestamp)
  ddsrt_nonnull_all;

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__DISCOVERY_TOPIC_H */
