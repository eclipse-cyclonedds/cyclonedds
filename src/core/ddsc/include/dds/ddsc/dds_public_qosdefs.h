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

/** @file
 *
 * @brief DDS C QoS API
 *
 * This header file defines the public API of QoS and Policies in the
 * Eclipse Cyclone DDS C language binding.
 */
#ifndef DDS_QOSDEFS_H
#define DDS_QOSDEFS_H

#if defined (__cplusplus)
extern "C" {
#endif

#define DDS_LENGTH_UNLIMITED -1

/** QoS identifiers */
typedef enum dds_qos_policy_id {
  DDS_INVALID_QOS_POLICY_ID,
  DDS_USERDATA_QOS_POLICY_ID,
  DDS_DURABILITY_QOS_POLICY_ID,
  DDS_PRESENTATION_QOS_POLICY_ID,
  DDS_DEADLINE_QOS_POLICY_ID,
  DDS_LATENCYBUDGET_QOS_POLICY_ID,
  DDS_OWNERSHIP_QOS_POLICY_ID,
  DDS_OWNERSHIPSTRENGTH_QOS_POLICY_ID,
  DDS_LIVELINESS_QOS_POLICY_ID,
  DDS_TIMEBASEDFILTER_QOS_POLICY_ID,
  DDS_PARTITION_QOS_POLICY_ID,
  DDS_RELIABILITY_QOS_POLICY_ID,
  DDS_DESTINATIONORDER_QOS_POLICY_ID,
  DDS_HISTORY_QOS_POLICY_ID,
  DDS_RESOURCELIMITS_QOS_POLICY_ID,
  DDS_ENTITYFACTORY_QOS_POLICY_ID,
  DDS_WRITERDATALIFECYCLE_QOS_POLICY_ID,
  DDS_READERDATALIFECYCLE_QOS_POLICY_ID,
  DDS_TOPICDATA_QOS_POLICY_ID,
  DDS_GROUPDATA_QOS_POLICY_ID,
  DDS_TRANSPORTPRIORITY_QOS_POLICY_ID,
  DDS_LIFESPAN_QOS_POLICY_ID,
  DDS_DURABILITYSERVICE_QOS_POLICY_ID,
  DDS_PROPERTY_QOS_POLICY_ID,
} dds_qos_policy_id_t;

/* QoS structure is opaque */
/** QoS structure */
typedef struct dds_qos dds_qos_t;

/** Durability QoS: Applies to Topic, DataReader, DataWriter */
typedef enum dds_durability_kind
{
    DDS_DURABILITY_VOLATILE,
    DDS_DURABILITY_TRANSIENT_LOCAL,
    DDS_DURABILITY_TRANSIENT,
    DDS_DURABILITY_PERSISTENT
}
dds_durability_kind_t;

/** History QoS: Applies to Topic, DataReader, DataWriter */
typedef enum dds_history_kind
{
    DDS_HISTORY_KEEP_LAST,
    DDS_HISTORY_KEEP_ALL
}
dds_history_kind_t;

/** Ownership QoS: Applies to Topic, DataReader, DataWriter */
typedef enum dds_ownership_kind
{
    DDS_OWNERSHIP_SHARED,
    DDS_OWNERSHIP_EXCLUSIVE
}
dds_ownership_kind_t;

/** Liveliness QoS: Applies to Topic, DataReader, DataWriter */
typedef enum dds_liveliness_kind
{
    DDS_LIVELINESS_AUTOMATIC,
    DDS_LIVELINESS_MANUAL_BY_PARTICIPANT,
    DDS_LIVELINESS_MANUAL_BY_TOPIC
}
dds_liveliness_kind_t;

/** Reliability QoS: Applies to Topic, DataReader, DataWriter */
typedef enum dds_reliability_kind
{
    DDS_RELIABILITY_BEST_EFFORT,
    DDS_RELIABILITY_RELIABLE
}
dds_reliability_kind_t;

/** DestinationOrder QoS: Applies to Topic, DataReader, DataWriter */
typedef enum dds_destination_order_kind
{
    DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP,
    DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP
}
dds_destination_order_kind_t;

/** Presentation QoS: Applies to Publisher, Subscriber */
typedef enum dds_presentation_access_scope_kind
{
    DDS_PRESENTATION_INSTANCE,
    DDS_PRESENTATION_TOPIC,
    DDS_PRESENTATION_GROUP
}
dds_presentation_access_scope_kind_t;

/** Ignore-local QoS: Applies to DataReader, DataWriter */
typedef enum dds_ignorelocal_kind
{
    DDS_IGNORELOCAL_NONE,
    DDS_IGNORELOCAL_PARTICIPANT,
    DDS_IGNORELOCAL_PROCESS
}
dds_ignorelocal_kind_t;

#if defined (__cplusplus)
}
#endif
#endif
