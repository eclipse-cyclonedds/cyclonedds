// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__ENTITY_INDEX_H
#define DDSI__ENTITY_INDEX_H

#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsi/ddsi_topic.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/ddsi/ddsi_proxy_endpoint.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_entity.h"
#include "ddsi__thread.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_entity_index;
struct ddsi_guid;
struct ddsi_domaingv;

struct ddsi_match_entities_range_key {
  union {
#ifdef DDS_HAS_TOPIC_DISCOVERY
    struct ddsi_topic tp;
#endif
    struct ddsi_writer wr;
    struct ddsi_reader rd;
    struct ddsi_entity_common e;
    struct ddsi_generic_proxy_endpoint gpe;
  } entity;
  struct dds_qos xqos;
#ifdef DDS_HAS_TOPIC_DISCOVERY
  struct ddsi_topic_definition tpdef;
#endif
};

struct ddsi_entity_enum_participant { struct ddsi_entity_enum st; };
struct ddsi_entity_enum_writer { struct ddsi_entity_enum st; };
struct ddsi_entity_enum_reader { struct ddsi_entity_enum st; };
struct ddsi_entity_enum_proxy_participant { struct ddsi_entity_enum st; };
struct ddsi_entity_enum_proxy_writer { struct ddsi_entity_enum st; };
struct ddsi_entity_enum_proxy_reader { struct ddsi_entity_enum st; };

/** @component entity_index */
struct ddsi_entity_index *ddsi_entity_index_new (struct ddsi_domaingv *gv) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entity_index_free (struct ddsi_entity_index *ei) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_insert_participant_guid (struct ddsi_entity_index *ei, struct ddsi_participant *pp) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_insert_proxy_participant_guid (struct ddsi_entity_index *ei, struct ddsi_proxy_participant *proxypp) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_insert_writer_guid (struct ddsi_entity_index *ei, struct ddsi_writer *wr) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_insert_reader_guid (struct ddsi_entity_index *ei, struct ddsi_reader *rd) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_insert_proxy_writer_guid (struct ddsi_entity_index *ei, struct ddsi_proxy_writer *pwr) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_insert_proxy_reader_guid (struct ddsi_entity_index *ei, struct ddsi_proxy_reader *prd) ddsrt_nonnull_all;


/** @component entity_index */
void ddsi_entidx_remove_participant_guid (struct ddsi_entity_index *ei, struct ddsi_participant *pp) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_remove_proxy_participant_guid (struct ddsi_entity_index *ei, struct ddsi_proxy_participant *proxypp) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_remove_writer_guid (struct ddsi_entity_index *ei, struct ddsi_writer *wr) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_remove_reader_guid (struct ddsi_entity_index *ei, struct ddsi_reader *rd) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_remove_proxy_writer_guid (struct ddsi_entity_index *ei, struct ddsi_proxy_writer *pwr) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_remove_proxy_reader_guid (struct ddsi_entity_index *ei, struct ddsi_proxy_reader *prd) ddsrt_nonnull_all;


/** @component entity_index */
void ddsi_entidx_enum_init_topic (struct ddsi_entity_enum *st, const struct ddsi_entity_index *gh, enum ddsi_entity_kind kind, const char *topic, struct ddsi_match_entities_range_key *max) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_enum_init_topic_w_prefix (struct ddsi_entity_enum *st, const struct ddsi_entity_index *ei, enum ddsi_entity_kind kind, const char *topic, const ddsi_guid_prefix_t *prefix, struct ddsi_match_entities_range_key *max) ddsrt_nonnull_all;

/** @component entity_index */
void *ddsi_entidx_enum_next_max (struct ddsi_entity_enum *st, const struct ddsi_match_entities_range_key *max) ddsrt_nonnull_all;


/** @component entity_index */
void ddsi_entidx_enum_writer_init (struct ddsi_entity_enum_writer *st, const struct ddsi_entity_index *ei) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_enum_reader_init (struct ddsi_entity_enum_reader *st, const struct ddsi_entity_index *ei) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_enum_proxy_writer_init (struct ddsi_entity_enum_proxy_writer *st, const struct ddsi_entity_index *ei) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_enum_proxy_reader_init (struct ddsi_entity_enum_proxy_reader *st, const struct ddsi_entity_index *ei) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_enum_participant_init (struct ddsi_entity_enum_participant *st, const struct ddsi_entity_index *ei) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_enum_proxy_participant_init (struct ddsi_entity_enum_proxy_participant *st, const struct ddsi_entity_index *ei) ddsrt_nonnull_all;


/** @component entity_index */
struct ddsi_writer *ddsi_entidx_enum_writer_next (struct ddsi_entity_enum_writer *st) ddsrt_nonnull_all;

/** @component entity_index */
struct ddsi_reader *ddsi_entidx_enum_reader_next (struct ddsi_entity_enum_reader *st) ddsrt_nonnull_all;

/** @component entity_index */
struct ddsi_proxy_writer *ddsi_entidx_enum_proxy_writer_next (struct ddsi_entity_enum_proxy_writer *st) ddsrt_nonnull_all;

/** @component entity_index */
struct ddsi_proxy_reader *ddsi_entidx_enum_proxy_reader_next (struct ddsi_entity_enum_proxy_reader *st) ddsrt_nonnull_all;

/** @component entity_index */
struct ddsi_participant *ddsi_entidx_enum_participant_next (struct ddsi_entity_enum_participant *st) ddsrt_nonnull_all;

/** @component entity_index */
struct ddsi_proxy_participant *ddsi_entidx_enum_proxy_participant_next (struct ddsi_entity_enum_proxy_participant *st) ddsrt_nonnull_all;


/** @component entity_index */
void ddsi_entidx_enum_writer_fini (struct ddsi_entity_enum_writer *st) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_enum_reader_fini (struct ddsi_entity_enum_reader *st) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_enum_proxy_writer_fini (struct ddsi_entity_enum_proxy_writer *st) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_enum_proxy_reader_fini (struct ddsi_entity_enum_proxy_reader *st) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_enum_participant_fini (struct ddsi_entity_enum_participant *st) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_enum_proxy_participant_fini (struct ddsi_entity_enum_proxy_participant *st) ddsrt_nonnull_all;

#ifdef DDS_HAS_TOPIC_DISCOVERY

/** @component entity_index */
void ddsi_entidx_insert_topic_guid (struct ddsi_entity_index *ei, struct ddsi_topic *tp) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_remove_topic_guid (struct ddsi_entity_index *ei, struct ddsi_topic *tp) ddsrt_nonnull_all;

struct ddsi_entity_enum_topic { struct ddsi_entity_enum st; };

/** @component entity_index */
void ddsi_entidx_enum_topic_init (struct ddsi_entity_enum_topic *st, const struct ddsi_entity_index *ei) ddsrt_nonnull_all;

/** @component entity_index */
struct ddsi_topic *ddsi_entidx_enum_topic_next (struct ddsi_entity_enum_topic *st) ddsrt_nonnull_all;

/** @component entity_index */
void ddsi_entidx_enum_topic_fini (struct ddsi_entity_enum_topic *st) ddsrt_nonnull_all;

#endif /* DDS_HAS_TOPIC_DISCOVERY */

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__ENTITY_INDEX_H */
