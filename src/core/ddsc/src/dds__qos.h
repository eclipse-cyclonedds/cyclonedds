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
#ifndef _DDS_QOS_H_
#define _DDS_QOS_H_

#include "cyclonedds/ddsi/q_xqos.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define DDS_TOPIC_QOS_MASK                                              \
  (QP_TOPIC_DATA | QP_DURABILITY | QP_DURABILITY_SERVICE |              \
   QP_DEADLINE | QP_LATENCY_BUDGET | QP_OWNERSHIP | QP_LIVELINESS |     \
   QP_RELIABILITY | QP_TRANSPORT_PRIORITY | QP_LIFESPAN |               \
   QP_DESTINATION_ORDER | QP_HISTORY | QP_RESOURCE_LIMITS)

#define DDS_PARTICIPANT_QOS_MASK                                        \
  (QP_USER_DATA | QP_PRISMTECH_ENTITY_FACTORY | QP_CYCLONE_IGNORELOCAL)

#define DDS_PUBLISHER_QOS_MASK                                          \
  (QP_PARTITION | QP_PRESENTATION | QP_GROUP_DATA |                     \
   QP_PRISMTECH_ENTITY_FACTORY | QP_CYCLONE_IGNORELOCAL)

#define DDS_READER_QOS_MASK                                             \
  (QP_USER_DATA | QP_DURABILITY | QP_DEADLINE | QP_LATENCY_BUDGET |     \
   QP_OWNERSHIP | QP_LIVELINESS | QP_TIME_BASED_FILTER |                \
   QP_RELIABILITY | QP_DESTINATION_ORDER | QP_HISTORY |                 \
   QP_RESOURCE_LIMITS | QP_PRISMTECH_READER_DATA_LIFECYCLE |            \
   QP_CYCLONE_IGNORELOCAL)

#define DDS_SUBSCRIBER_QOS_MASK                                         \
  (QP_PARTITION | QP_PRESENTATION | QP_GROUP_DATA |                     \
   QP_PRISMTECH_ENTITY_FACTORY | QP_CYCLONE_IGNORELOCAL)

#define DDS_WRITER_QOS_MASK                                             \
  (QP_USER_DATA | QP_DURABILITY | QP_DURABILITY_SERVICE | QP_DEADLINE | \
   QP_LATENCY_BUDGET | QP_OWNERSHIP | QP_OWNERSHIP_STRENGTH |           \
   QP_LIVELINESS | QP_RELIABILITY | QP_TRANSPORT_PRIORITY |             \
   QP_LIFESPAN | QP_DESTINATION_ORDER | QP_HISTORY |                    \
   QP_RESOURCE_LIMITS | QP_PRISMTECH_WRITER_DATA_LIFECYCLE |            \
   QP_CYCLONE_IGNORELOCAL)

#if defined (__cplusplus)
}
#endif
#endif
