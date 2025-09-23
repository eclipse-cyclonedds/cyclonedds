// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__PROXY_PARTICIPANT_H
#define DDSI__PROXY_PARTICIPANT_H

#include "dds/export.h"
#include "dds/features.h"

#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/fibheap.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_topic.h"
#include "dds/ddsi/ddsi_proxy_participant.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_lease;
struct ddsi_plist;
struct ddsi_addrset;
struct ddsi_proxy_endpoint_common;
struct ddsi_proxy_writer;

/** @component ddsi_proxy_participant */
int ddsi_update_proxy_participant_plist_locked (struct ddsi_proxy_participant *proxypp, ddsi_seqno_t seq, const struct ddsi_plist *datap, ddsrt_wctime_t timestamp);

/** @component ddsi_proxy_participant */
void ddsi_purge_proxy_participants (struct ddsi_domaingv *gv, const ddsi_xlocator_t *loc);

/** @component ddsi_proxy_participant */
dds_return_t ddsi_ref_proxy_participant_begin (struct ddsi_proxy_participant *proxypp, struct ddsi_proxy_endpoint_common *c)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

/** @component ddsi_proxy_participant */
dds_return_t ddsi_ref_proxy_participant_complete (struct ddsi_proxy_participant *proxypp)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

/** @component ddsi_proxy_participant */
void ddsi_unref_proxy_participant (struct ddsi_proxy_participant *proxypp, struct ddsi_proxy_endpoint_common *c)
  ddsrt_nonnull ((1));

/** @component ddsi_proxy_participant */
void ddsi_proxy_participant_add_pwr_lease_locked (struct ddsi_proxy_participant * proxypp, const struct ddsi_proxy_writer * pwr);

/** @component ddsi_proxy_participant */
void ddsi_proxy_participant_remove_pwr_lease_locked (struct ddsi_proxy_participant * proxypp, struct ddsi_proxy_writer * pwr);

/* To create or delete a new proxy participant: "guid" MUST have the
   pre-defined participant entity id. Unlike ddsi_delete_participant (),
   deleting a proxy participant will automatically delete all its
   readers & writers. Delete removes the participant from a hash table
   and schedules the actual deletion.

   TODO: what about proxy participants without built-in endpoints?
*/


/** @component ddsi_proxy_participant */
int ddsi_delete_proxy_participant_by_guid (struct ddsi_domaingv *gv, const struct ddsi_guid *guid, ddsrt_wctime_t timestamp, bool lease_expired);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__PROXY_PARTICIPANT_H */
