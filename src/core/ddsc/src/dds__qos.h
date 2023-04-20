// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__QOS_H
#define DDS__QOS_H

#include "dds/ddsi/ddsi_xqos.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define DDS_TOPIC_QOS_MASK                                                                  \
  (DDSI_QP_TOPIC_DATA | DDSI_QP_DURABILITY | DDSI_QP_DURABILITY_SERVICE |                   \
   DDSI_QP_DEADLINE | DDSI_QP_LATENCY_BUDGET | DDSI_QP_OWNERSHIP | DDSI_QP_LIVELINESS |     \
   DDSI_QP_RELIABILITY | DDSI_QP_TRANSPORT_PRIORITY | DDSI_QP_LIFESPAN |                    \
   DDSI_QP_DESTINATION_ORDER | DDSI_QP_HISTORY | DDSI_QP_RESOURCE_LIMITS |                  \
   DDSI_QP_DATA_REPRESENTATION | DDSI_QP_ENTITY_NAME)

#define DDS_PARTICIPANT_QOS_MASK                                                            \
  (DDSI_QP_USER_DATA | DDSI_QP_ADLINK_ENTITY_FACTORY | DDSI_QP_CYCLONE_IGNORELOCAL |        \
   DDSI_QP_PROPERTY_LIST | DDSI_QP_LIVELINESS | DDSI_QP_ENTITY_NAME) // liveliness is a Cyclone DDS special

#define DDS_PUBLISHER_QOS_MASK                                                              \
  (DDSI_QP_PARTITION | DDSI_QP_PRESENTATION | DDSI_QP_GROUP_DATA |                          \
   DDSI_QP_ADLINK_ENTITY_FACTORY | DDSI_QP_CYCLONE_IGNORELOCAL | DDSI_QP_ENTITY_NAME)

#define DDS_READER_QOS_MASK                                                                 \
  (DDSI_QP_USER_DATA | DDSI_QP_DURABILITY | DDSI_QP_DEADLINE | DDSI_QP_LATENCY_BUDGET |     \
   DDSI_QP_OWNERSHIP | DDSI_QP_LIVELINESS | DDSI_QP_TIME_BASED_FILTER |                     \
   DDSI_QP_RELIABILITY | DDSI_QP_DESTINATION_ORDER | DDSI_QP_HISTORY |                      \
   DDSI_QP_RESOURCE_LIMITS | DDSI_QP_ADLINK_READER_DATA_LIFECYCLE |                         \
   DDSI_QP_CYCLONE_IGNORELOCAL | DDSI_QP_PROPERTY_LIST |                                    \
   DDSI_QP_TYPE_CONSISTENCY_ENFORCEMENT | DDSI_QP_DATA_REPRESENTATION |                     \
   DDSI_QP_ENTITY_NAME)

#define DDS_SUBSCRIBER_QOS_MASK                                                             \
  (DDSI_QP_PARTITION | DDSI_QP_PRESENTATION | DDSI_QP_GROUP_DATA |                          \
   DDSI_QP_ADLINK_ENTITY_FACTORY | DDSI_QP_CYCLONE_IGNORELOCAL | DDSI_QP_ENTITY_NAME)

#define DDS_WRITER_QOS_MASK                                                                 \
  (DDSI_QP_USER_DATA | DDSI_QP_DURABILITY | DDSI_QP_DURABILITY_SERVICE | DDSI_QP_DEADLINE | \
   DDSI_QP_LATENCY_BUDGET | DDSI_QP_OWNERSHIP | DDSI_QP_OWNERSHIP_STRENGTH |                \
   DDSI_QP_LIVELINESS | DDSI_QP_RELIABILITY | DDSI_QP_TRANSPORT_PRIORITY |                  \
   DDSI_QP_LIFESPAN | DDSI_QP_DESTINATION_ORDER | DDSI_QP_HISTORY |                         \
   DDSI_QP_RESOURCE_LIMITS | DDSI_QP_ADLINK_WRITER_DATA_LIFECYCLE |                         \
   DDSI_QP_CYCLONE_IGNORELOCAL | DDSI_QP_PROPERTY_LIST | DDSI_QP_DATA_REPRESENTATION |      \
   DDSI_QP_ENTITY_NAME | DDSI_QP_CYCLONE_WRITER_BATCHING)

/** @component qos_obj */
dds_return_t dds_ensure_valid_data_representation (dds_qos_t *qos, uint32_t allowed_data_representations, bool topicqos);

/** @component qos_obj */
void dds_apply_entity_naming(dds_qos_t *qos, /* optional */ dds_qos_t *parent_qos, struct ddsi_domaingv *gv);

#if defined (__cplusplus)
}
#endif
#endif /* DDS__QOS_H */
