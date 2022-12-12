/*
 * Copyright(c) 2006 to 2021 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSI__DISCOVERY_H
#define DDSI__DISCOVERY_H

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

struct ddsi_participant_builtin_topic_data_locators {
  struct ddsi_locators_one def_uni[MAX_XMIT_CONNS], meta_uni[MAX_XMIT_CONNS];
  struct ddsi_locators_one def_multi, meta_multi;
};

/** @component discovery */
void ddsi_get_participant_builtin_topic_data (const struct ddsi_participant *pp, ddsi_plist_t *dst, struct ddsi_participant_builtin_topic_data_locators *locs);

/** @component discovery */
struct ddsi_addrset *ddsi_get_endpoint_addrset (const struct ddsi_domaingv *gv, const ddsi_plist_t *datap, struct ddsi_addrset *proxypp_as_default, const ddsi_locator_t *rst_srcloc);

/** @component discovery */
int ddsi_spdp_write (struct ddsi_participant *pp);

/** @component discovery */
int ddsi_spdp_dispose_unregister (struct ddsi_participant *pp);

/** @component discovery */
int ddsi_sedp_write_topic (struct ddsi_topic *tp, bool alive);

/** @component discovery */
int ddsi_sedp_write_writer (struct ddsi_writer *wr);

/** @component discovery */
int ddsi_sedp_write_reader (struct ddsi_reader *rd);

/** @component discovery */
int ddsi_sedp_dispose_unregister_writer (struct ddsi_writer *wr);

/** @component discovery */
int ddsi_sedp_dispose_unregister_reader (struct ddsi_reader *rd);

/** @component discovery */
int ddsi_builtins_dqueue_handler (const struct ddsi_rsample_info *sampleinfo, const struct ddsi_rdata *fragchain, const ddsi_guid_t *rdguid, void *qarg);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__DISCOVERY_H */
