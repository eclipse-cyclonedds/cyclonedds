// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

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
struct ddsi_xevent;
struct ddsi_xpack;
struct ddsi_domaingv;

typedef enum ddsi_sedp_kind {
  SEDP_KIND_READER,
  SEDP_KIND_WRITER,
  SEDP_KIND_TOPIC
} ddsi_sedp_kind_t;

/** @component discovery */
struct ddsi_writer *ddsi_get_sedp_writer (const struct ddsi_participant *pp, unsigned entityid)
  ddsrt_nonnull_all;

/** @component discovery */
struct ddsi_proxy_participant *ddsi_implicitly_create_proxypp (struct ddsi_domaingv *gv, const ddsi_guid_t *ppguid, ddsi_plist_t *datap /* note: potentially modifies datap */, const ddsi_guid_prefix_t *src_guid_prefix, ddsi_vendorid_t vendorid, ddsrt_wctime_t timestamp, ddsi_seqno_t seq)
  ddsrt_nonnull_all;

/** @component discovery */
bool ddsi_check_sedp_kind_and_guid (ddsi_sedp_kind_t sedp_kind, const ddsi_guid_t *entity_guid)
  ddsrt_nonnull_all;

/** @component discovery */
bool ddsi_handle_sedp_checks (struct ddsi_domaingv * const gv, ddsi_sedp_kind_t sedp_kind, ddsi_guid_t *entity_guid, ddsi_plist_t *datap,
    const ddsi_guid_prefix_t *src_guid_prefix, ddsi_vendorid_t vendorid, ddsrt_wctime_t timestamp,
    struct ddsi_proxy_participant **proxypp, ddsi_guid_t *ppguid)
  ddsrt_nonnull_all;

/** @component discovery */
int ddsi_builtins_dqueue_handler (const struct ddsi_rsample_info *sampleinfo, const struct ddsi_rdata *fragchain, const ddsi_guid_t *rdguid, void *qarg);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__DISCOVERY_H */
