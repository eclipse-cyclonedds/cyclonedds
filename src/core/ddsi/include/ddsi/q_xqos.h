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
#ifndef NN_XQOS_H
#define NN_XQOS_H

/*XXX*/
#include "ddsi/q_protocol.h"
#include "ddsi/q_rtps.h"
/*XXX*/
#include "ddsi/q_log.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define NN_DDS_LENGTH_UNLIMITED -1

typedef struct nn_octetseq {
  uint32_t length;
  unsigned char *value;
} nn_octetseq_t;

typedef nn_octetseq_t nn_userdata_qospolicy_t;
typedef nn_octetseq_t nn_topicdata_qospolicy_t;
typedef nn_octetseq_t nn_groupdata_qospolicy_t;

typedef struct nn_property {
  char *name;
  char *value;
  bool propagate;
} nn_property_t;

typedef struct nn_propertyseq {
  uint32_t n;
  nn_property_t *props;
} nn_propertyseq_t;

typedef struct nn_binaryproperty {
  char *name;
  nn_octetseq_t value;
  bool propagate;
} nn_binaryproperty_t;

typedef struct nn_binarypropertyseq {
  uint32_t n;
  nn_binaryproperty_t *props;
} nn_binarypropertyseq_t;

typedef struct nn_property_qospolicy {
  nn_propertyseq_t value;
  nn_binarypropertyseq_t binary_value;
} nn_property_qospolicy_t;

typedef enum nn_durability_kind {
  NN_VOLATILE_DURABILITY_QOS,
  NN_TRANSIENT_LOCAL_DURABILITY_QOS,
  NN_TRANSIENT_DURABILITY_QOS,
  NN_PERSISTENT_DURABILITY_QOS
} nn_durability_kind_t;

typedef struct nn_durability_qospolicy {
  nn_durability_kind_t kind;
} nn_durability_qospolicy_t;

typedef enum nn_history_kind {
  NN_KEEP_LAST_HISTORY_QOS,
  NN_KEEP_ALL_HISTORY_QOS
} nn_history_kind_t;

typedef struct nn_history_qospolicy {
  nn_history_kind_t kind;
  int32_t depth;
} nn_history_qospolicy_t;

typedef struct nn_resource_limits_qospolicy {
  int32_t max_samples;
  int32_t max_instances;
  int32_t max_samples_per_instance;
} nn_resource_limits_qospolicy_t;

typedef struct nn_durability_service_qospolicy {
  nn_duration_t service_cleanup_delay;
  nn_history_qospolicy_t history;
  nn_resource_limits_qospolicy_t resource_limits;
} nn_durability_service_qospolicy_t;

typedef enum nn_presentation_access_scope_kind {
  NN_INSTANCE_PRESENTATION_QOS,
  NN_TOPIC_PRESENTATION_QOS,
  NN_GROUP_PRESENTATION_QOS
} nn_presentation_access_scope_kind_t;

typedef struct nn_presentation_qospolicy {
  nn_presentation_access_scope_kind_t access_scope;
  unsigned char coherent_access;
  unsigned char ordered_access;
} nn_presentation_qospolicy_t;

typedef struct nn_deadline_qospolicy {
  nn_duration_t deadline;
} nn_deadline_qospolicy_t;

typedef struct nn_latency_budget_qospolicy {
  nn_duration_t duration;
} nn_latency_budget_qospolicy_t;

typedef enum nn_ownership_kind {
  NN_SHARED_OWNERSHIP_QOS,
  NN_EXCLUSIVE_OWNERSHIP_QOS
} nn_ownership_kind_t;

typedef struct nn_ownership_qospolicy {
  nn_ownership_kind_t kind;
} nn_ownership_qospolicy_t;

typedef struct nn_ownership_strength_qospolicy {
  int32_t value;
} nn_ownership_strength_qospolicy_t;

typedef enum nn_liveliness_kind {
  NN_AUTOMATIC_LIVELINESS_QOS,
  NN_MANUAL_BY_PARTICIPANT_LIVELINESS_QOS,
  NN_MANUAL_BY_TOPIC_LIVELINESS_QOS
} nn_liveliness_kind_t;

typedef struct nn_liveliness_qospolicy {
  nn_liveliness_kind_t kind;
  nn_duration_t lease_duration;
} nn_liveliness_qospolicy_t;

typedef struct nn_time_based_filter_qospolicy {
  nn_duration_t minimum_separation;
} nn_time_based_filter_qospolicy_t;

typedef struct nn_stringseq {
  uint32_t n;
  char **strs;
} nn_stringseq_t;

typedef nn_stringseq_t nn_partition_qospolicy_t;

typedef enum nn_reliability_kind {
  NN_BEST_EFFORT_RELIABILITY_QOS,
  NN_RELIABLE_RELIABILITY_QOS
} nn_reliability_kind_t;

typedef struct nn_reliability_qospolicy {
  nn_reliability_kind_t kind;
  nn_duration_t max_blocking_time;
} nn_reliability_qospolicy_t;

typedef struct nn_external_reliability_qospolicy {
  uint32_t kind;
  nn_duration_t max_blocking_time;
} nn_external_reliability_qospolicy_t;

#define NN_PEDANTIC_BEST_EFFORT_RELIABILITY_QOS 1
#define NN_PEDANTIC_RELIABLE_RELIABILITY_QOS    3 /* <= see DDSI 2.1, table 9.4 */
#define NN_INTEROP_BEST_EFFORT_RELIABILITY_QOS  1
#define NN_INTEROP_RELIABLE_RELIABILITY_QOS     2

typedef struct nn_transport_priority_qospolicy {
  int32_t value;
} nn_transport_priority_qospolicy_t;

typedef struct nn_lifespan_qospolicy {
  nn_duration_t duration;
} nn_lifespan_qospolicy_t;

typedef enum nn_destination_order_kind {
  NN_BY_RECEPTION_TIMESTAMP_DESTINATIONORDER_QOS,
  NN_BY_SOURCE_TIMESTAMP_DESTINATIONORDER_QOS
} nn_destination_order_kind_t;

typedef struct nn_destination_order_qospolicy {
  nn_destination_order_kind_t kind;
} nn_destination_order_qospolicy_t;

typedef struct nn_entity_factory_qospolicy {
  unsigned char autoenable_created_entities;
} nn_entity_factory_qospolicy_t;

typedef struct nn_writer_data_lifecycle_qospolicy {
  unsigned char autodispose_unregistered_instances;
  nn_duration_t autopurge_suspended_samples_delay; /* OpenSplice extension */
  nn_duration_t autounregister_instance_delay; /* OpenSplice extension */
} nn_writer_data_lifecycle_qospolicy_t;

typedef enum nn_invalid_sample_visibility_kind {
  NN_NO_INVALID_SAMPLE_VISIBILITY_QOS,
  NN_MINIMUM_INVALID_SAMPLE_VISIBILITY_QOS,
  NN_ALL_INVALID_SAMPLE_VISIBILITY_QOS
} nn_invalid_sample_visibility_kind_t;

typedef struct nn_reader_data_lifecycle_qospolicy {
  nn_duration_t autopurge_nowriter_samples_delay;
  nn_duration_t autopurge_disposed_samples_delay;
  unsigned char autopurge_dispose_all; /* OpenSplice extension */
  unsigned char enable_invalid_samples; /* OpenSplice extension */
  nn_invalid_sample_visibility_kind_t invalid_sample_visibility; /* OpenSplice extension */
} nn_reader_data_lifecycle_qospolicy_t;

typedef struct nn_synchronous_endpoint_qospolicy {
  unsigned char value;
} nn_synchronous_endpoint_qospolicy_t;

typedef struct nn_relaxed_qos_matching_qospolicy {
  unsigned char value;
} nn_relaxed_qos_matching_qospolicy_t;

typedef struct nn_subscription_keys_qospolicy {
  unsigned char use_key_list;
  nn_stringseq_t key_list;
} nn_subscription_keys_qospolicy_t;

typedef struct nn_reader_lifespan_qospolicy {
  unsigned char use_lifespan;
  nn_duration_t duration;
} nn_reader_lifespan_qospolicy_t;

typedef struct nn_share_qospolicy {
  unsigned char enable;
  char *name;
} nn_share_qospolicy_t;

/***/

/* Qos Present bit indices */
#define QP_TOPIC_NAME                        ((uint64_t)1 <<  0)
#define QP_TYPE_NAME                         ((uint64_t)1 <<  1)
#define QP_PRESENTATION                      ((uint64_t)1 <<  2)
#define QP_PARTITION                         ((uint64_t)1 <<  3)
#define QP_GROUP_DATA                        ((uint64_t)1 <<  4)
#define QP_TOPIC_DATA                        ((uint64_t)1 <<  5)
#define QP_DURABILITY                        ((uint64_t)1 <<  6)
#define QP_DURABILITY_SERVICE                ((uint64_t)1 <<  7)
#define QP_DEADLINE                          ((uint64_t)1 <<  8)
#define QP_LATENCY_BUDGET                    ((uint64_t)1 <<  9)
#define QP_LIVELINESS                        ((uint64_t)1 << 10)
#define QP_RELIABILITY                       ((uint64_t)1 << 11)
#define QP_DESTINATION_ORDER                 ((uint64_t)1 << 12)
#define QP_HISTORY                           ((uint64_t)1 << 13)
#define QP_RESOURCE_LIMITS                   ((uint64_t)1 << 14)
#define QP_TRANSPORT_PRIORITY                ((uint64_t)1 << 15)
#define QP_LIFESPAN                          ((uint64_t)1 << 16)
#define QP_USER_DATA                         ((uint64_t)1 << 17)
#define QP_OWNERSHIP                         ((uint64_t)1 << 18)
#define QP_OWNERSHIP_STRENGTH                ((uint64_t)1 << 19)
#define QP_TIME_BASED_FILTER                 ((uint64_t)1 << 20)
#define QP_PRISMTECH_WRITER_DATA_LIFECYCLE   ((uint64_t)1 << 21)
#define QP_PRISMTECH_READER_DATA_LIFECYCLE   ((uint64_t)1 << 22)
#define QP_PRISMTECH_RELAXED_QOS_MATCHING    ((uint64_t)1 << 23)
#define QP_PRISMTECH_READER_LIFESPAN         ((uint64_t)1 << 24)
#define QP_PRISMTECH_SUBSCRIPTION_KEYS       ((uint64_t)1 << 25)
#define QP_PRISMTECH_ENTITY_FACTORY          ((uint64_t)1 << 27)
#define QP_PRISMTECH_SYNCHRONOUS_ENDPOINT    ((uint64_t)1 << 28)
#define QP_RTI_TYPECODE                      ((uint64_t)1 << 29)
#define QP_PROPERTY                          ((uint64_t)1 << 30)

/* Partition QoS is not RxO according to the specification (DDS 1.2,
   section 7.1.3), but communication will not take place unless it
   matches. Same for topic and type.  Relaxed qos matching is a bit of
   a weird one, but it affects matching, so ... */
#define QP_RXO_MASK (QP_DURABILITY | QP_PRESENTATION | QP_DEADLINE | QP_LATENCY_BUDGET | QP_OWNERSHIP | QP_LIVELINESS | QP_RELIABILITY | QP_DESTINATION_ORDER | QP_PRISMTECH_RELAXED_QOS_MATCHING | QP_PRISMTECH_SYNCHRONOUS_ENDPOINT)
#define QP_CHANGEABLE_MASK (QP_USER_DATA | QP_TOPIC_DATA | QP_GROUP_DATA | QP_DEADLINE | QP_LATENCY_BUDGET | QP_OWNERSHIP_STRENGTH | QP_TIME_BASED_FILTER | QP_PARTITION | QP_TRANSPORT_PRIORITY | QP_LIFESPAN | QP_PRISMTECH_ENTITY_FACTORY | QP_PRISMTECH_WRITER_DATA_LIFECYCLE | QP_PRISMTECH_READER_DATA_LIFECYCLE)
#define QP_UNRECOGNIZED_INCOMPATIBLE_MASK (QP_PRISMTECH_RELAXED_QOS_MATCHING)

/* readers & writers have an extended qos, hence why it is a separate
   type */
typedef struct nn_xqos {
  /* Entries present, for sparse QoS */
  uint64_t present;
  uint64_t aliased;

  /*v---- in ...Qos
     v--- in ...BuiltinTopicData
      v-- mapped in DDSI
       v- reader/writer/publisher/subscriber/participant specific */
  /*      Extras: */
  /* xx */char *topic_name;
  /* xx */char *type_name;
  /*      PublisherQos, SubscriberQos: */
  /*xxx */nn_presentation_qospolicy_t presentation;
  /*xxx */nn_partition_qospolicy_t partition;
  /*xxx */nn_groupdata_qospolicy_t group_data;
  /*x xX*/nn_entity_factory_qospolicy_t entity_factory;
  /*      TopicQos: */
  /*xxx */nn_topicdata_qospolicy_t topic_data;
  /*      DataWriterQos, DataReaderQos: */
  /*xxx */nn_durability_qospolicy_t durability;
  /*xxx */nn_durability_service_qospolicy_t durability_service;
  /*xxx */nn_deadline_qospolicy_t deadline;
  /*xxx */nn_latency_budget_qospolicy_t latency_budget;
  /*xxx */nn_liveliness_qospolicy_t liveliness;
  /*xxx */nn_reliability_qospolicy_t reliability;
  /*xxx */nn_destination_order_qospolicy_t destination_order;
  /*x x */nn_history_qospolicy_t history;
  /*x x */nn_resource_limits_qospolicy_t resource_limits;
  /*x x */nn_transport_priority_qospolicy_t transport_priority;
  /*xxx */nn_lifespan_qospolicy_t lifespan;
  /*xxx */nn_userdata_qospolicy_t user_data;
  /*xxx */nn_ownership_qospolicy_t ownership;
  /*xxxW*/nn_ownership_strength_qospolicy_t ownership_strength;
  /*xxxR*/nn_time_based_filter_qospolicy_t time_based_filter;
  /*x  W*/nn_writer_data_lifecycle_qospolicy_t writer_data_lifecycle;
  /*x xR*/nn_reader_data_lifecycle_qospolicy_t reader_data_lifecycle;
  /*x x */nn_relaxed_qos_matching_qospolicy_t relaxed_qos_matching;
  /*x xR*/nn_subscription_keys_qospolicy_t subscription_keys;
  /*x xR*/nn_reader_lifespan_qospolicy_t reader_lifespan;
  /*x xR*/nn_share_qospolicy_t share;
  /*xxx */nn_synchronous_endpoint_qospolicy_t synchronous_endpoint;

  /*xxx */nn_property_qospolicy_t property;

  /*   X*/nn_octetseq_t rti_typecode;
} nn_xqos_t;

struct nn_xmsg;

DDS_EXPORT void nn_xqos_init_empty (nn_xqos_t *xqos);
DDS_EXPORT void nn_xqos_init_default_reader (nn_xqos_t *xqos);
DDS_EXPORT void nn_xqos_init_default_writer (nn_xqos_t *xqos);
DDS_EXPORT void nn_xqos_init_default_writer_noautodispose (nn_xqos_t *xqos);
DDS_EXPORT void nn_xqos_init_default_subscriber (nn_xqos_t *xqos);
DDS_EXPORT void nn_xqos_init_default_publisher (nn_xqos_t *xqos);
DDS_EXPORT void nn_xqos_init_default_topic (nn_xqos_t *xqos);
DDS_EXPORT void nn_xqos_copy (nn_xqos_t *dst, const nn_xqos_t *src);
DDS_EXPORT void nn_xqos_unalias (nn_xqos_t *xqos);
DDS_EXPORT void nn_xqos_fini (nn_xqos_t *xqos);
DDS_EXPORT void nn_xqos_mergein_missing (nn_xqos_t *a, const nn_xqos_t *b);
DDS_EXPORT uint64_t nn_xqos_delta (const nn_xqos_t *a, const nn_xqos_t *b, uint64_t mask);
DDS_EXPORT void nn_xqos_addtomsg (struct nn_xmsg *m, const nn_xqos_t *xqos, uint64_t wanted);
DDS_EXPORT void nn_log_xqos (uint32_t cat, const nn_xqos_t *xqos);
DDS_EXPORT nn_xqos_t *nn_xqos_dup (const nn_xqos_t *src);

#if defined (__cplusplus)
}
#endif

#endif /* NN_XQOS_H */
