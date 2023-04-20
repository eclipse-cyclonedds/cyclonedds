// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_ENTITY_INDEX_H
#define DDSI_ENTITY_INDEX_H

#include "dds/ddsi/ddsi_thread.h"
#include "dds/ddsi/ddsi_topic.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/ddsi/ddsi_proxy_endpoint.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_entity_index;
struct ddsi_guid;
struct ddsi_domaingv;

struct ddsi_entity_enum
{
  struct ddsi_entity_index *entidx;
  enum ddsi_entity_kind kind;
  struct ddsi_entity_common *cur;
#ifndef NDEBUG
  ddsi_vtime_t vtime;
#endif
};

/* Readers & writers are both in a GUID- and in a GID-keyed table. If
   they are in the GID-based one, they are also in the GUID-based one,
   but not the way around, for two reasons:

   - firstly, there are readers & writers that do not have a GID
     (built-in endpoints, fictitious transient data readers),

   - secondly, they are inserted first in the GUID-keyed one, and then
     in the GID-keyed one.

   The GID is used solely for the interface with the OpenSplice
   kernel, all internal state and protocol handling is done using the
   GUID. So all this means is that, e.g., a writer being deleted
   becomes invisible to the network reader slightly before it
   disappears in the protocol handling, or that a writer might exist
   at the protocol level slightly before the network reader can use it
   to transmit data. */


/** @component entity_index */
void *ddsi_entidx_lookup_guid_untyped (const struct ddsi_entity_index *ei, const struct ddsi_guid *guid) ddsrt_nonnull_all;

/** @component entity_index */
void *ddsi_entidx_lookup_guid (const struct ddsi_entity_index *ei, const struct ddsi_guid *guid, enum ddsi_entity_kind kind) ddsrt_nonnull_all;

/** @component entity_index */
struct ddsi_participant *ddsi_entidx_lookup_participant_guid (const struct ddsi_entity_index *ei, const struct ddsi_guid *guid) ddsrt_nonnull_all;

/** @component entity_index */
struct ddsi_writer *ddsi_entidx_lookup_writer_guid (const struct ddsi_entity_index *ei, const struct ddsi_guid *guid) ddsrt_nonnull_all;

/** @component entity_index */
struct ddsi_reader *ddsi_entidx_lookup_reader_guid (const struct ddsi_entity_index *ei, const struct ddsi_guid *guid) ddsrt_nonnull_all;

/** @component entity_index */
struct ddsi_proxy_participant *ddsi_entidx_lookup_proxy_participant_guid (const struct ddsi_entity_index *ei, const struct ddsi_guid *guid) ddsrt_nonnull_all;

/** @component entity_index */
struct ddsi_proxy_writer *ddsi_entidx_lookup_proxy_writer_guid (const struct ddsi_entity_index *ei, const struct ddsi_guid *guid) ddsrt_nonnull_all;

/** @component entity_index */
struct ddsi_proxy_reader *ddsi_entidx_lookup_proxy_reader_guid (const struct ddsi_entity_index *ei, const struct ddsi_guid *guid) ddsrt_nonnull_all;

/* Enumeration of entries in the hash table:

   - "next" visits at least all entries that were in the hash table at
     the time of calling init and that have not subsequently been
     removed;

   - "next" may visit an entry more than once, but will do so only
     because of rare events (i.e., resize or so);

   - the order in which entries are visited is arbitrary;

   - the caller must call init() before it may call next(); it must
     call fini() before it may call init() again. */

/** @component entity_index */
void ddsi_entidx_enum_init (struct ddsi_entity_enum *st, const struct ddsi_entity_index *ei, enum ddsi_entity_kind kind) ddsrt_nonnull_all;

/** @component entity_index */
void *ddsi_entidx_enum_next (struct ddsi_entity_enum *st) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_enum_fini (struct ddsi_entity_enum *st) ddsrt_nonnull_all;

#ifdef DDS_HAS_TOPIC_DISCOVERY
/** @component entity_index */
struct ddsi_topic *ddsi_entidx_lookup_topic_guid (const struct ddsi_entity_index *ei, const struct ddsi_guid *guid);
#endif

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_ENTITY_INDEX_H */
