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

/* Set when this proxy participant is created implicitly and has to be deleted upon disappearance
   of its last endpoint.  FIXME: Currently there is a potential race with adding a new endpoint
   in parallel to deleting the last remaining one. The endpoint will then be created, added to the
   proxy participant and then both are deleted. With the current single-threaded discovery
   this can only happen when it is all triggered by lease expiry. */
#define DDSI_CF_IMPLICITLY_CREATED_PROXYPP          (1 << 0)
/* Set when this proxy participant is a DDSI2 participant, to help Cloud figure out whom to send
   discovery data when used in conjunction with the networking bridge */
#define DDSI_CF_PARTICIPANT_IS_DDSI2                (1 << 1)
/* Set when this proxy participant is not to be announced on the built-in topics yet */
#define DDSI_CF_PROXYPP_NO_SPDP                     (1 << 2)

/** @component ddsi_proxy_participant */
int ddsi_update_proxy_participant_plist_locked (struct ddsi_proxy_participant *proxypp, ddsi_seqno_t seq, const struct ddsi_plist *datap, ddsrt_wctime_t timestamp);

/** @component ddsi_proxy_participant */
void ddsi_proxy_participant_reassign_lease (struct ddsi_proxy_participant *proxypp, struct ddsi_lease *newlease);

/** @component ddsi_proxy_participant */
void ddsi_purge_proxy_participants (struct ddsi_domaingv *gv, const ddsi_xlocator_t *loc, bool delete_from_as_disc);

/** @component ddsi_proxy_participant */
int ddsi_ref_proxy_participant (struct ddsi_proxy_participant *proxypp, struct ddsi_proxy_endpoint_common *c);

/** @component ddsi_proxy_participant */
void ddsi_unref_proxy_participant (struct ddsi_proxy_participant *proxypp, struct ddsi_proxy_endpoint_common *c);

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
bool ddsi_new_proxy_participant (struct ddsi_domaingv *gv, const struct ddsi_guid *guid, uint32_t bes, const struct ddsi_guid *privileged_pp_guid, struct ddsi_addrset *as_default, struct ddsi_addrset *as_meta, const struct ddsi_plist *plist, dds_duration_t tlease_dur, ddsi_vendorid_t vendor, unsigned custom_flags, ddsrt_wctime_t timestamp, ddsi_seqno_t seq);

/** @component ddsi_proxy_participant */
int ddsi_delete_proxy_participant_by_guid (struct ddsi_domaingv *gv, const struct ddsi_guid *guid, ddsrt_wctime_t timestamp, int isimplicit);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__PROXY_PARTICIPANT_H */
