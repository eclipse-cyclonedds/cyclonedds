/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSI_ENTITY_INDEX_H
#define DDSI_ENTITY_INDEX_H

#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsi/q_entity.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct entity_index;
struct ddsi_guid;
struct ddsi_domaingv;

struct match_entities_range_key {
  union {
    struct writer wr;
    struct reader rd;
    struct proxy_writer pwr;
    struct proxy_reader prd;
    struct entity_common e;
    struct generic_proxy_endpoint gpe;
  } entity;
  struct dds_qos xqos;
};

struct entidx_enum
{
  struct entity_index *entidx;
  enum entity_kind kind;
  struct entity_common *cur;
#ifndef NDEBUG
  vtime_t vtime;
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

struct entity_index *entity_index_new (struct ddsi_domaingv *gv) ddsrt_nonnull_all;
void entity_index_free (struct entity_index *ei) ddsrt_nonnull_all;

void entidx_insert_participant_guid (struct entity_index *ei, struct participant *pp) ddsrt_nonnull_all;
void entidx_insert_proxy_participant_guid (struct entity_index *ei, struct proxy_participant *proxypp) ddsrt_nonnull_all;
void entidx_insert_writer_guid (struct entity_index *ei, struct writer *wr) ddsrt_nonnull_all;
void entidx_insert_reader_guid (struct entity_index *ei, struct reader *rd) ddsrt_nonnull_all;
void entidx_insert_proxy_writer_guid (struct entity_index *ei, struct proxy_writer *pwr) ddsrt_nonnull_all;
void entidx_insert_proxy_reader_guid (struct entity_index *ei, struct proxy_reader *prd) ddsrt_nonnull_all;

void entidx_remove_participant_guid (struct entity_index *ei, struct participant *pp) ddsrt_nonnull_all;
void entidx_remove_proxy_participant_guid (struct entity_index *ei, struct proxy_participant *proxypp) ddsrt_nonnull_all;
void entidx_remove_writer_guid (struct entity_index *ei, struct writer *wr) ddsrt_nonnull_all;
void entidx_remove_reader_guid (struct entity_index *ei, struct reader *rd) ddsrt_nonnull_all;
void entidx_remove_proxy_writer_guid (struct entity_index *ei, struct proxy_writer *pwr) ddsrt_nonnull_all;
void entidx_remove_proxy_reader_guid (struct entity_index *ei, struct proxy_reader *prd) ddsrt_nonnull_all;

DDS_EXPORT void *entidx_lookup_guid_untyped (const struct entity_index *ei, const struct ddsi_guid *guid) ddsrt_nonnull_all;
DDS_EXPORT void *entidx_lookup_guid (const struct entity_index *ei, const struct ddsi_guid *guid, enum entity_kind kind) ddsrt_nonnull_all;

DDS_EXPORT struct participant *entidx_lookup_participant_guid (const struct entity_index *ei, const struct ddsi_guid *guid) ddsrt_nonnull_all;
DDS_EXPORT struct proxy_participant *entidx_lookup_proxy_participant_guid (const struct entity_index *ei, const struct ddsi_guid *guid) ddsrt_nonnull_all;
DDS_EXPORT struct writer *entidx_lookup_writer_guid (const struct entity_index *ei, const struct ddsi_guid *guid) ddsrt_nonnull_all;
DDS_EXPORT struct reader *entidx_lookup_reader_guid (const struct entity_index *ei, const struct ddsi_guid *guid) ddsrt_nonnull_all;
DDS_EXPORT struct proxy_writer *entidx_lookup_proxy_writer_guid (const struct entity_index *ei, const struct ddsi_guid *guid) ddsrt_nonnull_all;
DDS_EXPORT struct proxy_reader *entidx_lookup_proxy_reader_guid (const struct entity_index *ei, const struct ddsi_guid *guid) ddsrt_nonnull_all;

/* Enumeration of entries in the hash table:

   - "next" visits at least all entries that were in the hash table at
     the time of calling init and that have not subsequently been
     removed;

   - "next" may visit an entry more than once, but will do so only
     because of rare events (i.e., resize or so);

   - the order in which entries are visited is arbitrary;

   - the caller must call init() before it may call next(); it must
     call fini() before it may call init() again. */
struct entidx_enum_participant { struct entidx_enum st; };
struct entidx_enum_writer { struct entidx_enum st; };
struct entidx_enum_reader { struct entidx_enum st; };
struct entidx_enum_proxy_participant { struct entidx_enum st; };
struct entidx_enum_proxy_writer { struct entidx_enum st; };
struct entidx_enum_proxy_reader { struct entidx_enum st; };

void entidx_enum_init (struct entidx_enum *st, const struct entity_index *ei, enum entity_kind kind) ddsrt_nonnull_all;
void entidx_enum_init_topic (struct entidx_enum *st, const struct entity_index *gh, enum entity_kind kind, const char *topic, struct match_entities_range_key *max) ddsrt_nonnull_all;
void entidx_enum_init_topic_w_prefix (struct entidx_enum *st, const struct entity_index *ei, enum entity_kind kind, const char *topic, const ddsi_guid_prefix_t *prefix, struct match_entities_range_key *max) ddsrt_nonnull_all;
void *entidx_enum_next_max (struct entidx_enum *st, const struct match_entities_range_key *max) ddsrt_nonnull_all;
void *entidx_enum_next (struct entidx_enum *st) ddsrt_nonnull_all;
void entidx_enum_fini (struct entidx_enum *st) ddsrt_nonnull_all;

void entidx_enum_writer_init (struct entidx_enum_writer *st, const struct entity_index *ei) ddsrt_nonnull_all;
void entidx_enum_reader_init (struct entidx_enum_reader *st, const struct entity_index *ei) ddsrt_nonnull_all;
void entidx_enum_proxy_writer_init (struct entidx_enum_proxy_writer *st, const struct entity_index *ei) ddsrt_nonnull_all;
void entidx_enum_proxy_reader_init (struct entidx_enum_proxy_reader *st, const struct entity_index *ei) ddsrt_nonnull_all;
void entidx_enum_participant_init (struct entidx_enum_participant *st, const struct entity_index *ei) ddsrt_nonnull_all;
void entidx_enum_proxy_participant_init (struct entidx_enum_proxy_participant *st, const struct entity_index *ei) ddsrt_nonnull_all;

struct writer *entidx_enum_writer_next (struct entidx_enum_writer *st) ddsrt_nonnull_all;
struct reader *entidx_enum_reader_next (struct entidx_enum_reader *st) ddsrt_nonnull_all;
struct proxy_writer *entidx_enum_proxy_writer_next (struct entidx_enum_proxy_writer *st) ddsrt_nonnull_all;
struct proxy_reader *entidx_enum_proxy_reader_next (struct entidx_enum_proxy_reader *st) ddsrt_nonnull_all;
struct participant *entidx_enum_participant_next (struct entidx_enum_participant *st) ddsrt_nonnull_all;
struct proxy_participant *entidx_enum_proxy_participant_next (struct entidx_enum_proxy_participant *st) ddsrt_nonnull_all;

void entidx_enum_writer_fini (struct entidx_enum_writer *st) ddsrt_nonnull_all;
void entidx_enum_reader_fini (struct entidx_enum_reader *st) ddsrt_nonnull_all;
void entidx_enum_proxy_writer_fini (struct entidx_enum_proxy_writer *st) ddsrt_nonnull_all;
void entidx_enum_proxy_reader_fini (struct entidx_enum_proxy_reader *st) ddsrt_nonnull_all;
void entidx_enum_participant_fini (struct entidx_enum_participant *st) ddsrt_nonnull_all;
void entidx_enum_proxy_participant_fini (struct entidx_enum_proxy_participant *st) ddsrt_nonnull_all;

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_ENTITY_INDEX_H */
