// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__DISCOVERY_SPDP_H
#define DDSI__DISCOVERY_SPDP_H

#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_domaingv.h" // FIXME: MAX_XMIT_CONNS

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
struct ddsi_serdata;

struct ddsi_participant_builtin_topic_data_locators {
  struct ddsi_locators_one def_uni[MAX_XMIT_CONNS], meta_uni[MAX_XMIT_CONNS];
  struct ddsi_locators_one def_multi, meta_multi;
};

/** @component discovery */
void ddsi_get_participant_builtin_topic_data (const struct ddsi_participant *pp, ddsi_plist_t *dst, struct ddsi_participant_builtin_topic_data_locators *locs)
  ddsrt_nonnull_all;

/** @component discovery */
int ddsi_spdp_write (struct ddsi_participant *pp);

/** @component discovery */
int ddsi_spdp_dispose_unregister (struct ddsi_participant *pp);

struct ddsi_spdp_broadcast_xevent_cb_arg {
  ddsi_guid_t pp_guid;
};

/** @component discovery */
void ddsi_spdp_broadcast_xevent_cb (struct ddsi_domaingv *gv, struct ddsi_xevent *ev, UNUSED_ARG (struct ddsi_xpack *xp), void *varg, ddsrt_mtime_t tnow);

/** @component discovery */
void ddsi_handle_spdp (const struct ddsi_receiver_state *rst, ddsi_entityid_t pwr_entityid, ddsi_seqno_t seq, const struct ddsi_serdata *serdata);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__DISCOVERY_SPDP_H */
