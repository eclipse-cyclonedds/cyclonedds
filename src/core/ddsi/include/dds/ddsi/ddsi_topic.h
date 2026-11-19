// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_TOPIC_H
#define DDSI_TOPIC_H

#include "dds/export.h"
#include "dds/features.h"

#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_entity.h"

#ifdef DDS_HAS_TOPIC_DISCOVERY
#include "dds/ddsi/ddsi_typelib.h"
#endif

#if defined (__cplusplus)
extern "C" {
#endif

/** @component ddsi_topic */
int ddsi_is_builtin_topic (ddsi_entityid_t id, ddsi_vendorid_t vendorid);

#ifdef DDS_HAS_TOPIC_DISCOVERY

struct ddsi_proxy_participant;
struct ddsi_type_pair;
struct dds_qos;

struct ddsi_topic_definition {
  unsigned char key[16]; /* key for this topic definition (MD5 hash of the type_id and qos) */
  struct ddsi_type_pair *type_pair; /* has a ddsi_type object for the minimal and complete type, which contains the XTypes type identifiers */
  struct dds_qos *xqos; /* contains also the topic name and type name */
  uint32_t refc;
  struct ddsi_domaingv *gv;
};

struct ddsi_topic {
  struct ddsi_entity_common e;
  struct ddsi_topic_definition *definition; /* ref to (shared) topic definition, protected by e.qos_lock */
  struct ddsi_participant *pp; /* backref to the participant */
};

struct ddsi_proxy_topic
{
  ddsi_entityid_t entityid;
  struct ddsi_topic_definition *definition; /* ref to (shared) topic definition */
  ddsrt_wctime_t tupdate; /* timestamp of last update */
  ddsi_seqno_t seq; /* sequence number of most recent SEDP message */
  ddsrt_avl_node_t avlnode; /* entry in proxypp->topics */
  unsigned deleted: 1;
};

/** @component ddsi_topic */
dds_return_t ddsi_new_topic (struct ddsi_topic **tp_out, struct ddsi_guid *tpguid, struct ddsi_participant *pp, const char *topic_name, const struct ddsi_sertype *type, const struct dds_qos *xqos, bool is_builtin);

/** @component ddsi_topic */
void ddsi_update_topic_qos (struct ddsi_topic *tp, const dds_qos_t *xqos);

/** @component ddsi_topic */
dds_return_t ddsi_delete_topic (struct ddsi_domaingv *gv, const struct ddsi_guid *guid);

/** @component ddsi_topic
 * If type_id is none, select the first definition named topic_name. Otherwise
 * its TypeInformation must also match the requested complete type ID.
 * Return a copy of the QoS and store the definition's complete type ID in
 * *matched_type_id (NULL if unknown). The caller owns both copies.
 * If no definition matches, return NULL and leave *matched_type_id unchanged.
 */
dds_qos_t *ddsi_lookup_topic_definition_qos (struct ddsi_domaingv *gv, const char *topic_name, const ddsi_typeid_t *type_id, ddsi_typeid_t **matched_type_id);

#endif /* DDS_HAS_TOPIC_DISCOVERY */

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_TOPIC_H */
