// Copyright(c) 2024 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#ifndef DDS__SYSDEF_MODEL_H
#define DDS__SYSDEF_MODEL_H

#include "dds/dds.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsrt/sockets.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define TYPE_HASH_LENGTH 14

struct dds_sysdef_type_metadata {
  unsigned char *type_hash;
  size_t type_info_cdr_sz;
  unsigned char *type_info_cdr;
  size_t type_map_cdr_sz;
  unsigned char *type_map_cdr;
};

struct dds_sysdef_type_metadata_admin {
  struct ddsrt_hh *m;
};

struct xml_element;
struct dds_sysdef_type;
struct dds_sysdef_type_lib;
struct dds_sysdef_qos;
struct dds_sysdef_qos_profile;
struct dds_sysdef_qos_lib;
struct dds_sysdef_domain;
struct dds_sysdef_domain_lib;
struct dds_sysdef_participant;
struct dds_sysdef_participant_lib;
struct dds_sysdef_publisher;
struct dds_sysdef_subscriber;
struct dds_sysdef_application;
struct dds_sysdef_application_lib;
struct dds_sysdef_node_lib;
struct parse_sysdef_state;

enum element_kind
{
  ELEMENT_KIND_UNDEFINED,
  ELEMENT_KIND_DDS,

  ELEMENT_KIND_TYPE_LIB,
  ELEMENT_KIND_TYPE,

  ELEMENT_KIND_QOS_LIB,
  ELEMENT_KIND_QOS_PROFILE,
  ELEMENT_KIND_QOS_PARTICIPANT,
  ELEMENT_KIND_QOS_PUBLISHER,
  ELEMENT_KIND_QOS_SUBSCRIBER,
  ELEMENT_KIND_QOS_TOPIC,
  ELEMENT_KIND_QOS_WRITER,
  ELEMENT_KIND_QOS_READER,

  ELEMENT_KIND_QOS_DURATION_SEC,
  ELEMENT_KIND_QOS_DURATION_NSEC,

  ELEMENT_KIND_QOS_POLICY_DEADLINE,
  ELEMENT_KIND_QOS_POLICY_DEADLINE_PERIOD,
  ELEMENT_KIND_QOS_POLICY_DESTINATIONORDER,
  ELEMENT_KIND_QOS_POLICY_DESTINATIONORDER_KIND,
  ELEMENT_KIND_QOS_POLICY_DURABILITY,
  ELEMENT_KIND_QOS_POLICY_DURABILITY_KIND,
  ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE,
  ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE_SERVICE_CLEANUP_DELAY,
  ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE_HISTORY_KIND,
  ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE_HISTORY_DEPTH,
  ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE_RESOURCE_LIMIT_MAX_SAMPLES,
  ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE_RESOURCE_LIMIT_MAX_INSTANCES,
  ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE_RESOURCE_LIMIT_MAX_SAMPLES_PER_INSTANCE,
  ELEMENT_KIND_QOS_POLICY_ENTITYFACTORY,
  ELEMENT_KIND_QOS_POLICY_ENTITYFACTORY_AUTOENABLE_CREATED_ENTITIES,
  ELEMENT_KIND_QOS_POLICY_GROUPDATA,
  ELEMENT_KIND_QOS_POLICY_GROUPDATA_VALUE,
  ELEMENT_KIND_QOS_POLICY_HISTORY,
  ELEMENT_KIND_QOS_POLICY_HISTORY_KIND,
  ELEMENT_KIND_QOS_POLICY_HISTORY_DEPTH,
  ELEMENT_KIND_QOS_POLICY_LATENCYBUDGET,
  ELEMENT_KIND_QOS_POLICY_LATENCYBUDGET_DURATION,
  ELEMENT_KIND_QOS_POLICY_LIFESPAN,
  ELEMENT_KIND_QOS_POLICY_LIFESPAN_DURATION,
  ELEMENT_KIND_QOS_POLICY_LIVELINESS,
  ELEMENT_KIND_QOS_POLICY_LIVELINESS_KIND,
  ELEMENT_KIND_QOS_POLICY_LIVELINESS_LEASE_DURATION,
  ELEMENT_KIND_QOS_POLICY_OWNERSHIP,
  ELEMENT_KIND_QOS_POLICY_OWNERSHIP_KIND,
  ELEMENT_KIND_QOS_POLICY_OWNERSHIPSTRENGTH,
  ELEMENT_KIND_QOS_POLICY_OWNERSHIPSTRENGTH_VALUE,
  ELEMENT_KIND_QOS_POLICY_PARTITION,
  ELEMENT_KIND_QOS_POLICY_PARTITION_NAME,
  ELEMENT_KIND_QOS_POLICY_PARTITION_NAME_ELEMENT,
  ELEMENT_KIND_QOS_POLICY_PRESENTATION,
  ELEMENT_KIND_QOS_POLICY_PRESENTATION_ACCESS_SCOPE,
  ELEMENT_KIND_QOS_POLICY_PRESENTATION_COHERENT_ACCESS,
  ELEMENT_KIND_QOS_POLICY_PRESENTATION_ORDERED_ACCESS,
  ELEMENT_KIND_QOS_POLICY_READERDATALIFECYCLE,
  ELEMENT_KIND_QOS_POLICY_READERDATALIFECYCLE_AUTOPURGE_NOWRITER_SAMPLES_DELAY,
  ELEMENT_KIND_QOS_POLICY_READERDATALIFECYCLE_AUTOPURGE_DISPOSED_SAMPLES_DELAY,
  ELEMENT_KIND_QOS_POLICY_RELIABILITY,
  ELEMENT_KIND_QOS_POLICY_RELIABILITY_KIND,
  ELEMENT_KIND_QOS_POLICY_RELIABILITY_MAX_BLOCKING_DELAY,
  ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS,
  ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS_INITIAL_INSTANCES,
  ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS_INITIAL_SAMPLES,
  ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS_MAX_SAMPLES,
  ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS_MAX_INSTANCES,
  ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS_MAX_SAMPLES_PER_INSTANCE,
  ELEMENT_KIND_QOS_POLICY_TIMEBASEDFILTER,
  ELEMENT_KIND_QOS_POLICY_TIMEBASEDFILTER_MINIMUM_SEPARATION,
  ELEMENT_KIND_QOS_POLICY_TOPICDATA,
  ELEMENT_KIND_QOS_POLICY_TOPICDATA_VALUE,
  ELEMENT_KIND_QOS_POLICY_TRANSPORTPRIORITY,
  ELEMENT_KIND_QOS_POLICY_TRANSPORTPRIORITY_VALUE,
  ELEMENT_KIND_QOS_POLICY_USERDATA,
  ELEMENT_KIND_QOS_POLICY_USERDATA_VALUE,
  ELEMENT_KIND_QOS_POLICY_WRITERDATALIFECYCLE,
  ELEMENT_KIND_QOS_POLICY_WRITERDATALIFECYCLE_AUTODISPOSE_UNREGISTERED_INSTANCES,

  ELEMENT_KIND_DOMAIN_LIB,
  ELEMENT_KIND_DOMAIN,
  ELEMENT_KIND_REGISTER_TYPE,
  ELEMENT_KIND_TOPIC,

  ELEMENT_KIND_PARTICIPANT_LIB,
  ELEMENT_KIND_PARTICIPANT,
  ELEMENT_KIND_PUBLISHER,
  ELEMENT_KIND_SUBSCRIBER,
  ELEMENT_KIND_WRITER,
  ELEMENT_KIND_READER,

  ELEMENT_KIND_APPLICATION_LIB,
  ELEMENT_KIND_APPLICATION,

  ELEMENT_KIND_NODE_LIB,
  ELEMENT_KIND_NODE,
  ELEMENT_KIND_NODE_HOSTNAME,
  ELEMENT_KIND_NODE_IPV4_ADDRESS,
  ELEMENT_KIND_NODE_IPV6_ADDRESS,
  ELEMENT_KIND_NODE_MAC_ADDRESS,

  ELEMENT_KIND_DEPLOYMENT_LIB,
  ELEMENT_KIND_DEPLOYMENT,
  ELEMENT_KIND_DEPLOYMENT_NODE_REF,
  ELEMENT_KIND_DEPLOYMENT_APPLICATION_LIST,
  ELEMENT_KIND_DEPLOYMENT_APPLICATION_REF,
  ELEMENT_KIND_DEPLOYMENT_CONF,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE_SOURCE_IP_ADDRESS,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE_DESTINATION_IP_ADDRESS,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE_DSCP,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE_PROTOCOL,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE_SOURCE_PORT,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE_DESTINATION_PORT,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE_SOURCE_IP_ADDRESS,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE_DESTINATION_IP_ADDRESS,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE_DSCP,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE_PROTOCOL,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE_SOURCE_PORT,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE_DESTINATION_PORT,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_PERIODICITY,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_SAMPLES_PER_PERIOD,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_MAX_BYTES_PER_SAMPLE,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_TRANSMISSION_SELECTION,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_TIME_AWARE,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_TIME_AWARE_EARLIEST_TRANSMIT_OFFSET,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_TIME_AWARE_LATEST_TRANSMIT_OFFSET,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_TIME_AWARE_JITTER,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_NETWORK_REQUIREMENTS,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_NETWORK_REQUIREMENTS_NUM_SEAMLESS_TREES,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_NETWORK_REQUIREMENTS_MAX_LATENCY,

  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_LISTENER,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_LISTENER_NETWORK_REQUIREMENTS,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_LISTENER_NETWORK_REQUIREMENTS_NUM_SEAMLESS_TREES,
  ELEMENT_KIND_DEPLOYMENT_CONF_TSN_LISTENER_NETWORK_REQUIREMENTS_MAX_LATENCY
};

enum element_data_type {
  ELEMENT_DATA_TYPE_GENERIC,
  ELEMENT_DATA_TYPE_DURATION
};

typedef int (* init_fn) (struct parse_sysdef_state * const pstate, struct xml_element *element);
typedef void (* fini_fn) (struct xml_element *element);

struct xml_element
{
  struct xml_element *parent;
  enum element_kind kind;
  enum element_data_type data_type;
  bool retain;
  bool handle_close;
  struct xml_element *next;
  fini_fn fini;
};

/* Type library */
struct dds_sysdef_type {
  struct xml_element xmlnode;
  char *name;
  unsigned char *identifier;
  struct dds_sysdef_type_lib *parent;
};

struct dds_sysdef_type_lib {
  struct xml_element xmlnode;
  struct dds_sysdef_type *types;
};

/* QoS library */
enum dds_sysdef_qos_kind {
  DDS_SYSDEF_TOPIC_QOS,
  DDS_SYSDEF_READER_QOS,
  DDS_SYSDEF_WRITER_QOS,
  DDS_SYSDEF_SUBSCRIBER_QOS,
  DDS_SYSDEF_PUBLISHER_QOS,
  DDS_SYSDEF_PARTICIPANT_QOS
};

#define QOS_POLICY_SYSDEF_STRUCT(p,t) \
  struct dds_sysdef_ ## p { \
    struct xml_element xmlnode; \
    t values; \
    uint32_t populated; \
  };

#define QOS_POLICY_DEADLINE_PARAM_PERIOD (1 << 0u)
#define QOS_POLICY_DEADLINE_PARAMS (QOS_POLICY_DEADLINE_PARAM_PERIOD)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_DEADLINE, dds_deadline_qospolicy_t)

#define QOS_POLICY_DESTINATIONORDER_PARAM_KIND (1 << 0u)
#define QOS_POLICY_DESTINATIONORDER_PARAMS (QOS_POLICY_DESTINATIONORDER_PARAM_KIND)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_DESTINATIONORDER, dds_destination_order_qospolicy_t)

#define QOS_POLICY_DURABILITY_PARAM_KIND (1 << 0u)
#define QOS_POLICY_DURABILITY_PARAMS (QOS_POLICY_DURABILITY_PARAM_KIND)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_DURABILITY, dds_durability_qospolicy_t)

#define QOS_POLICY_DURABILITYSERVICE_PARAM_SERVICE_CLEANUP_DELAY (1 << 0u)
#define QOS_POLICY_DURABILITYSERVICE_PARAM_HISTORY_KIND (1 << 1u)
#define QOS_POLICY_DURABILITYSERVICE_PARAM_HISTORY_DEPTH (1 << 2u)
#define QOS_POLICY_DURABILITYSERVICE_PARAM_RESOURCE_LIMIT_MAX_SAMPLES (1 << 3u)
#define QOS_POLICY_DURABILITYSERVICE_PARAM_RESOURCE_LIMIT_MAX_INSTANCES (1 << 4u)
#define QOS_POLICY_DURABILITYSERVICE_PARAM_RESOURCE_LIMIT_MAX_SAMPLES_PER_INSTANCE (1 << 5u)
#define QOS_POLICY_DURABILITYSERVICE_PARAMS (\
    QOS_POLICY_DURABILITYSERVICE_PARAM_SERVICE_CLEANUP_DELAY \
    | QOS_POLICY_DURABILITYSERVICE_PARAM_HISTORY_KIND \
    | QOS_POLICY_DURABILITYSERVICE_PARAM_HISTORY_DEPTH \
    | QOS_POLICY_DURABILITYSERVICE_PARAM_RESOURCE_LIMIT_MAX_SAMPLES \
    | QOS_POLICY_DURABILITYSERVICE_PARAM_RESOURCE_LIMIT_MAX_INSTANCES \
    | QOS_POLICY_DURABILITYSERVICE_PARAM_RESOURCE_LIMIT_MAX_SAMPLES_PER_INSTANCE)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_DURABILITYSERVICE, dds_durability_service_qospolicy_t)

#define QOS_POLICY_ENTITYFACTORY_PARAM_AUTOENABLE_CREATED_ENTITIES (1 << 0u)
#define QOS_POLICY_ENTITYFACTORY_PARAMS (QOS_POLICY_ENTITYFACTORY_PARAM_AUTOENABLE_CREATED_ENTITIES)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_ENTITYFACTORY, dds_entity_factory_qospolicy_t)

#define QOS_POLICY_HISTORY_PARAM_KIND (1 << 0u)
#define QOS_POLICY_HISTORY_PARAM_DEPTH (1 << 1u)
#define QOS_POLICY_HISTORY_PARAMS (QOS_POLICY_HISTORY_PARAM_KIND | QOS_POLICY_HISTORY_PARAM_DEPTH)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_HISTORY, dds_history_qospolicy_t)

#define QOS_POLICY_LATENCYBUDGET_PARAM_DURATION (1 << 0u)
#define QOS_POLICY_LATENCYBUDGET_PARAMS (QOS_POLICY_LATENCYBUDGET_PARAM_DURATION)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_LATENCYBUDGET, dds_latency_budget_qospolicy_t)

#define QOS_POLICY_LIFESPAN_PARAM_DURATION (1 << 0u)
#define QOS_POLICY_LIFESPAN_PARAMS (QOS_POLICY_LIFESPAN_PARAM_DURATION)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_LIFESPAN, dds_lifespan_qospolicy_t)

#define QOS_POLICY_LIVELINESS_PARAM_KIND (1 << 0u)
#define QOS_POLICY_LIVELINESS_PARAM_LEASE_DURATION (1 << 1u)
#define QOS_POLICY_LIVELINESS_PARAMS (QOS_POLICY_LIVELINESS_PARAM_KIND | QOS_POLICY_LIVELINESS_PARAM_LEASE_DURATION)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_LIVELINESS, dds_liveliness_qospolicy_t)

#define QOS_POLICY_OWNERSHIP_PARAM_KIND (1 << 0u)
#define QOS_POLICY_OWNERSHIP_PARAMS (QOS_POLICY_OWNERSHIP_PARAM_KIND)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_OWNERSHIP, dds_ownership_qospolicy_t)

#define QOS_POLICY_OWNERSHIPSTRENGTH_PARAM_VALUE (1 << 0u)
#define QOS_POLICY_OWNERSHIPSTRENGTH_PARAMS (QOS_POLICY_OWNERSHIPSTRENGTH_PARAM_VALUE)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_OWNERSHIPSTRENGTH, dds_ownership_strength_qospolicy_t)

#define QOS_POLICY_PARTITION_PARAM_NAME (1 << 0u)
#define QOS_POLICY_PARTITION_PARAMS (QOS_POLICY_PARTITION_PARAM_NAME)
struct dds_sysdef_QOS_POLICY_PARTITION_NAME_ELEMENT {
  struct xml_element xmlnode;
  char *element;
};

struct dds_sysdef_QOS_POLICY_PARTITION_NAME {
  struct xml_element xmlnode;
  struct dds_sysdef_QOS_POLICY_PARTITION_NAME_ELEMENT *elements;
};

struct dds_sysdef_QOS_POLICY_PARTITION {
  struct xml_element xmlnode;
  struct dds_sysdef_QOS_POLICY_PARTITION_NAME *name;
  uint32_t populated;
};

#define QOS_POLICY_PRESENTATION_PARAM_ACCESS_SCOPE (1 << 0u)
#define QOS_POLICY_PRESENTATION_PARAM_COHERENT_ACCESS (1 << 1u)
#define QOS_POLICY_PRESENTATION_PARAM_ORDERED_ACCESS (1 << 2u)
#define QOS_POLICY_PRESENTATION_PARAMS (QOS_POLICY_PRESENTATION_PARAM_ACCESS_SCOPE | QOS_POLICY_PRESENTATION_PARAM_COHERENT_ACCESS | QOS_POLICY_PRESENTATION_PARAM_ORDERED_ACCESS)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_PRESENTATION, dds_presentation_qospolicy_t)

#define QOS_POLICY_READERDATALIFECYCLE_PARAM_AUTOPURGE_NOWRITER_SAMPLES_DELAY (1 << 0u)
#define QOS_POLICY_READERDATALIFECYCLE_PARAM_AUTOPURGE_DISPOSED_SAMPLES_DELAY (1 << 1u)
#define QOS_POLICY_READERDATALIFECYCLE_PARAMS (QOS_POLICY_READERDATALIFECYCLE_PARAM_AUTOPURGE_NOWRITER_SAMPLES_DELAY | QOS_POLICY_READERDATALIFECYCLE_PARAM_AUTOPURGE_DISPOSED_SAMPLES_DELAY)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_READERDATALIFECYCLE, dds_reader_data_lifecycle_qospolicy_t)

#define QOS_POLICY_RELIABILITY_PARAM_KIND (1 << 0u)
#define QOS_POLICY_RELIABILITY_PARAM_MAX_BLOCKING_DELAY (1 << 1u)
#define QOS_POLICY_RELIABILITY_PARAMS (QOS_POLICY_RELIABILITY_PARAM_KIND | QOS_POLICY_RELIABILITY_PARAM_MAX_BLOCKING_DELAY)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_RELIABILITY, dds_reliability_qospolicy_t)

#define QOS_POLICY_RESOURCELIMITS_PARAM_MAX_SAMPLES (1 << 0u)
#define QOS_POLICY_RESOURCELIMITS_PARAM_MAX_INSTANCES (1 << 1u)
#define QOS_POLICY_RESOURCELIMITS_PARAM_MAX_SAMPLES_PER_INSTANCE (1 << 2u)
#define QOS_POLICY_RESOURCELIMITS_PARAMS (QOS_POLICY_RESOURCELIMITS_PARAM_MAX_SAMPLES | QOS_POLICY_RESOURCELIMITS_PARAM_MAX_INSTANCES | QOS_POLICY_RESOURCELIMITS_PARAM_MAX_SAMPLES_PER_INSTANCE)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_RESOURCELIMITS, dds_resource_limits_qospolicy_t)

#define QOS_POLICY_TIMEBASEDFILTER_PARAM_MINIMUM_SEPARATION (1 << 0u)
#define QOS_POLICY_TIMEBASEDFILTER_PARAMS (QOS_POLICY_TIMEBASEDFILTER_PARAM_MINIMUM_SEPARATION)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_TIMEBASEDFILTER, dds_time_based_filter_qospolicy_t)

#define QOS_POLICY_TRANSPORTPRIORITY_PARAM_VALUE (1 << 0u)
#define QOS_POLICY_TRANSPORTPRIORITY_PARAMS (QOS_POLICY_TRANSPORTPRIORITY_PARAM_VALUE)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_TRANSPORTPRIORITY, dds_transport_priority_qospolicy_t)

#define QOS_POLICY_WRITERDATALIFECYCLE_PARAM_AUTODISPOSE_UNREGISTERED_INSTANCES (1 << 0u)
#define QOS_POLICY_WRITERDATALIFECYCLE_PARAMS (QOS_POLICY_WRITERDATALIFECYCLE_PARAM_AUTODISPOSE_UNREGISTERED_INSTANCES)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_WRITERDATALIFECYCLE, dds_writer_data_lifecycle_qospolicy_t)

#define QOS_POLICY_GROUPDATA_PARAM_VALUE (1 << 0u)
#define QOS_POLICY_GROUPDATA_PARAMS (QOS_POLICY_GROUPDATA_PARAM_VALUE)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_GROUPDATA, dds_groupdata_qospolicy_t)

#define QOS_POLICY_TOPICDATA_PARAM_VALUE (1 << 0u)
#define QOS_POLICY_TOPICDATA_PARAMS (QOS_POLICY_TOPICDATA_PARAM_VALUE)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_TOPICDATA, dds_topicdata_qospolicy_t)

#define QOS_POLICY_USERDATA_PARAM_VALUE (1 << 0u)
#define QOS_POLICY_USERDATA_PARAMS (QOS_POLICY_USERDATA_PARAM_VALUE)
QOS_POLICY_SYSDEF_STRUCT (QOS_POLICY_USERDATA, dds_userdata_qospolicy_t)

struct dds_sysdef_qos_generic_property {
  struct xml_element xmlnode;
};

#define QOS_LENGTH_UNLIMITED       "LENGTH_UNLIMITED"
#define QOS_DURATION_INFINITY      "DURATION_INFINITY"
#define QOS_DURATION_INFINITY_SEC  "DURATION_INFINITE_SEC"
#define QOS_DURATION_INFINITY_NSEC "DURATION_INFINITE_NSEC"

#define QOS_DURATION_PARAM_SEC (1 << 0u)
#define QOS_DURATION_PARAM_NSEC (1 << 1u)
struct dds_sysdef_qos_duration_property {
  struct xml_element xmlnode;
  dds_duration_t sec;
  dds_duration_t nsec;
  uint32_t populated;
};

struct dds_sysdef_qos {
  struct xml_element xmlnode;
  enum dds_sysdef_qos_kind kind;
  dds_qos_t *qos;
  char *name;
  struct dds_sysdef_qos_profile *base_profile;
};

struct dds_sysdef_qos_profile {
  struct xml_element xmlnode;
  char *name;
  struct dds_sysdef_qos *qos;
  struct dds_sysdef_qos_profile *base_profile;
};

struct dds_sysdef_qos_lib {
  struct xml_element xmlnode;
  char *name;
  struct dds_sysdef_qos_profile *qos_profiles;
};

/* Domain library */
struct dds_sysdef_topic {
  struct xml_element xmlnode;
  char *name;
  // TODO: registered_name?
  struct dds_sysdef_qos *qos;
  struct dds_sysdef_register_type *register_type_ref;
};

enum dds_sysdef_register_type_parent_kind {
  DDS_SYSDEF_TYPEREG_PARENT_KIND_DOMAIN,
  DDS_SYSDEF_TYPEREG_PARENT_KIND_PARTICIPANT
};

struct dds_sysdef_register_type {
  struct xml_element xmlnode;
  char *name;
  struct dds_sysdef_type *type_ref;
  enum dds_sysdef_register_type_parent_kind parent_kind;
};

#define SYSDEF_DOMAIN_DOMAIN_ID_PARAM_VALUE (1 << 0u)
#define SYSDEF_DOMAIN_PARTICIPANT_INDEX_PARAM_VALUE (1 << 1u)
#define SYSDEF_DOMAIN_PARAMS (SYSDEF_DOMAIN_DOMAIN_ID_PARAM_VALUE)
struct dds_sysdef_domain {
  struct xml_element xmlnode;
  uint32_t domain_id;
  char *name;
  int32_t participant_index;
  struct dds_sysdef_register_type *register_types;
  struct dds_sysdef_topic *topics;
  uint32_t populated;
};

struct dds_sysdef_domain_lib {
  struct xml_element xmlnode;
  char *name;
  struct dds_sysdef_domain *domains;
};

/* Participant library */
struct dds_sysdef_endpoint {
  struct xml_element xmlnode;
  char *name;
  uint32_t entity_key;
};

#define SYSDEF_WRITER_ENTITY_KEY_PARAM_VALUE (1 << 0u)
#define SYSDEF_WRITER_PARAMS (SYSDEF_WRITER_ENTITY_KEY_PARAM_VALUE)
struct dds_sysdef_writer {
  struct xml_element xmlnode;
  char *name;
  uint32_t entity_key;
  struct dds_sysdef_topic *topic;
  struct dds_sysdef_qos *qos;
  uint32_t populated;
};
DDSRT_STATIC_ASSERT (offsetof (struct dds_sysdef_writer, xmlnode) == offsetof (struct dds_sysdef_endpoint, xmlnode));
DDSRT_STATIC_ASSERT (offsetof (struct dds_sysdef_writer, name) == offsetof (struct dds_sysdef_endpoint, name));
DDSRT_STATIC_ASSERT (offsetof (struct dds_sysdef_writer, entity_key) == offsetof (struct dds_sysdef_endpoint, entity_key));

#define SYSDEF_READER_ENTITY_KEY_PARAM_VALUE (1 << 0u)
#define SYSDEF_READER_PARAMS (SYSDEF_READER_ENTITY_KEY_PARAM_VALUE)
struct dds_sysdef_reader {
  struct xml_element xmlnode;
  char *name;
  uint32_t entity_key;
  struct dds_sysdef_topic *topic;
  struct dds_sysdef_qos *qos;
  uint32_t populated;
};
DDSRT_STATIC_ASSERT (offsetof (struct dds_sysdef_reader, xmlnode) == offsetof (struct dds_sysdef_endpoint, xmlnode));
DDSRT_STATIC_ASSERT (offsetof (struct dds_sysdef_reader, name) == offsetof (struct dds_sysdef_endpoint, name));
DDSRT_STATIC_ASSERT (offsetof (struct dds_sysdef_reader, entity_key) == offsetof (struct dds_sysdef_endpoint, entity_key));

struct dds_sysdef_publisher {
  struct xml_element xmlnode;
  char *name;
  struct dds_sysdef_qos *qos;
  struct dds_sysdef_writer *writers;
};

struct dds_sysdef_subscriber {
  struct xml_element xmlnode;
  char *name;
  struct dds_sysdef_qos *qos;
  struct dds_sysdef_reader *readers;
};

enum dds_sysdef_participant_parent_kind {
  DDS_SYSDEF_PARTICIPANT_PARENT_KIND_APPLICATION,
  DDS_SYSDEF_PARTICIPANT_PARENT_KIND_PARTICIPANTLIB
};

struct dds_sysdef_participant_guid_prefix {
  uint32_t p;
};

struct dds_sysdef_participant {
  struct xml_element xmlnode;
  char *name;
  struct dds_sysdef_qos *qos;
  struct dds_sysdef_domain *domain_ref;
  struct dds_sysdef_participant *base;
  struct dds_sysdef_participant_guid_prefix *guid_prefix;

  struct dds_sysdef_register_type *register_types;
  struct dds_sysdef_topic *topics;
  struct dds_sysdef_publisher *publishers;
  struct dds_sysdef_subscriber *subscribers;

  enum dds_sysdef_participant_parent_kind parent_kind;
  uint32_t populated;
};

struct dds_sysdef_participant_lib {
  struct xml_element xmlnode;
  char *name;
  struct dds_sysdef_participant *participants;
};

/* Application library */
struct dds_sysdef_application {
  struct xml_element xmlnode;
  char *name;
  struct dds_sysdef_participant *participants;
};

struct dds_sysdef_application_lib {
  struct xml_element xmlnode;
  char *name;
  struct dds_sysdef_application *applications;
};

/* Node library */
struct dds_sysdef_mac_addr {
  uint8_t addr[6];
};
struct dds_sysdef_ip_addr {
  struct sockaddr_storage addr;
};
struct dds_sysdef_node {
  struct xml_element xmlnode;
  char *name;
  char *hostname;
  struct dds_sysdef_ip_addr *ipv4_addr;
  struct dds_sysdef_ip_addr *ipv6_addr;
  struct dds_sysdef_mac_addr *mac_addr;
};

struct dds_sysdef_node_lib {
  struct xml_element xmlnode;
  char *name;
  struct dds_sysdef_node *nodes;
};

/* Deployment library */
#define SYSDEF_TSN_TIME_AWARE_EARLIEST_TRANSMIT_OFFSET_PARAM_VALUE (1 << 0u)
#define SYSDEF_TSN_TIME_AWARE_LATEST_TRANSMIT_OFFSET_PARAM_VALUE (1 << 1u)
#define SYSDEF_TSN_TIME_JITTER_PARAM_VALUE (1 << 2u)
struct dds_sysdef_tsn_time_aware {
  struct xml_element xmlnode;
  uint32_t earliest_transmit_offset;
  uint32_t latest_transmit_offset;
  uint32_t jitter;
  uint32_t populated;
};

enum dds_sysdef_tsn_traffic_transmission_selection {
  DDS_SYSDEF_TSN_TRAFFIC_TRANSMISSION_STRICT_PRIORITY,
  DDS_SYSDEF_TSN_TRAFFIC_TRANSMISSION_CREDIT_BASED_SHAPER,
  DDS_SYSDEF_TSN_TRAFFIC_TRANSMISSION_ENHANCED_TRANSMISSION_SELECTION,
  DDS_SYSDEF_TSN_TRAFFIC_TRANSMISSION_ATS_TRANSMISSION_SELECTION
};

#define SYSDEF_TSN_TRAFFIC_SPEC_SAMPLES_PER_PERIOD_PARAM_VALUE (1 << 0u)
#define SYSDEF_TSN_TRAFFIC_SPEC_MAX_BYTES_PER_SAMPLE_PARAM_VALUE (1 << 1u)
#define SYSDEF_TSN_TRAFFIC_SPEC_TRANSMISSION_SELECTION_PARAM_VALUE (1 << 2u)
struct dds_sysdef_tsn_traffic_specification {
  struct xml_element xmlnode;
  dds_duration_t periodicity;
  uint16_t samples_per_period;
  uint16_t max_bytes_per_sample;
  enum dds_sysdef_tsn_traffic_transmission_selection transmission_selection;
  struct dds_sysdef_tsn_time_aware *time_aware;
  uint32_t populated;
};

#define SYSDEF_TSN_NETWORK_REQ_NUM_SEAMLESS_TREES_PARAM_VALUE (1 << 0u)
#define SYSDEF_TSN_NETWORK_REQ_MAX_LATENCY_PARAM_VALUE (1 << 1u)
struct dds_sysdef_tsn_network_requirements {
  struct xml_element xmlnode;
  uint8_t num_seamless_trees;
  uint32_t max_latency;
  uint32_t populated;
};

struct dds_sysdef_tsn_ieee802_mac_addresses {
  struct xml_element xmlnode;
  char *destination_mac_address;
  char *source_mac_address;
};

struct dds_sysdef_tsn_ieee802_vlan_tag {
  struct xml_element xmlnode;
  uint8_t priority_code_point;
  uint16_t vlan_id;
  uint32_t populated;
};

#define SYSDEF_TSN_IP_TUPLE_DSCP_PARAM_VALUE (1 << 0u)
#define SYSDEF_TSN_IP_TUPLE_PROTOCOL_PARAM_VALUE (1 << 1u)
#define SYSDEF_TSN_IP_TUPLE_SOURCE_PORT_PARAM_VALUE (1 << 2u)
#define SYSDEF_TSN_IP_TUPLE_DESTINATION_PORT_PARAM_VALUE (1 << 3u)
struct dds_sysdef_tsn_ip_tuple {
  struct xml_element xmlnode;
  char *source_ip_address;
  char *destination_ip_address;
  uint8_t dscp;
  uint16_t protocol;
  uint16_t source_port;
  uint16_t destination_port;
  uint32_t populated;
};

struct dds_sysdef_tsn_data_frame_specification {
  struct xml_element xmlnode;
  struct dds_sysdef_tsn_ieee802_mac_addresses *mac_addresses;
  struct dds_sysdef_tsn_ieee802_vlan_tag *vlan_tag;
  struct dds_sysdef_tsn_ip_tuple *ipv4_tuple;
  struct dds_sysdef_tsn_ip_tuple *ipv6_tuple;
};

struct dds_sysdef_tsn_talker_configuration {
  struct xml_element xmlnode;
  char *name;
  char *stream_name;
  struct dds_sysdef_writer *writer;
  struct dds_sysdef_tsn_traffic_specification *traffic_specification;
  struct dds_sysdef_tsn_network_requirements *network_requirements;
  struct dds_sysdef_tsn_data_frame_specification *data_frame_specification;
};

struct dds_sysdef_tsn_listener_configuration {
  struct xml_element xmlnode;
  char *name;
  char *stream_name;
  struct dds_sysdef_reader *reader;
  struct dds_sysdef_tsn_network_requirements *network_requirements;
};

struct dds_sysdef_tsn_configuration {
  struct xml_element xmlnode;
  struct dds_sysdef_tsn_talker_configuration *tsn_talker_configurations;
  struct dds_sysdef_tsn_listener_configuration *tsn_listener_configurations;
};

struct dds_sysdef_configuration {
  struct xml_element xmlnode;
  struct dds_sysdef_tsn_configuration *tsn_configuration;
};

struct dds_sysdef_application_ref {
  struct xml_element xmlnode;
  struct dds_sysdef_application *application;
};

struct dds_sysdef_application_list {
  struct xml_element xmlnode;
  struct dds_sysdef_application_ref *application_refs;
};

struct dds_sysdef_deployment {
  struct xml_element xmlnode;
  char *name;
  struct dds_sysdef_node *node;
  struct dds_sysdef_application_list *application_list;
  struct dds_sysdef_configuration *configuration;
};

struct dds_sysdef_deployment_lib {
  struct xml_element xmlnode;
  char *name;
  struct dds_sysdef_deployment *deployments;
};

struct dds_sysdef_system {
  struct xml_element xmlnode;
  struct dds_sysdef_type_lib *type_libs;
  struct dds_sysdef_qos_lib *qos_libs;
  struct dds_sysdef_domain_lib *domain_libs;
  struct dds_sysdef_participant_lib *participant_libs;
  struct dds_sysdef_application_lib *application_libs;
  struct dds_sysdef_node_lib *node_libs;
  struct dds_sysdef_deployment_lib *deployment_libs;
};

#if defined (__cplusplus)
}
#endif

#endif // DDS__SYSDEF_MODEL_H
