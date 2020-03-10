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
#ifndef DDSI_XQOS_H
#define DDSI_XQOS_H

#include "dds/ddsc/dds_public_qosdefs.h"
/*XXX*/
#include "dds/ddsi/q_protocol.h"
#include "dds/ddsi/q_rtps.h"
/*XXX*/
#include "dds/ddsi/q_log.h"

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct ddsi_octetseq {
  uint32_t length;
  unsigned char *value;
} ddsi_octetseq_t;

typedef ddsi_octetseq_t dds_userdata_qospolicy_t;
typedef ddsi_octetseq_t dds_topicdata_qospolicy_t;
typedef ddsi_octetseq_t dds_groupdata_qospolicy_t;

typedef struct dds_property {
  /* The propagate boolean will not be send over the wire.
   * When the value is 'false', the complete struct shouldn't be send.
   * It has to be the first variable within the structure because it
   * is mapped to XbPROP in the serialiser. */
  unsigned char propagate;
  char *name;
  char *value;
} dds_property_t;

typedef struct dds_propertyseq {
  uint32_t n;
  dds_property_t *props;
} dds_propertyseq_t;

typedef struct dds_binaryproperty {
  /* The propagate boolean will not be send over the wire.
   * When the value is 'false', the complete struct shouldn't be send.
   * It has to be the first variable within the structure because it
   * is mapped to XbPROP in the serialiser. */
  unsigned char propagate;
  char *name;
  ddsi_octetseq_t value;
} dds_binaryproperty_t;

typedef struct dds_binarypropertyseq {
  uint32_t n;
  dds_binaryproperty_t *props;
} dds_binarypropertyseq_t;

typedef struct dds_property_qospolicy {
  dds_propertyseq_t value;
  dds_binarypropertyseq_t binary_value;
} dds_property_qospolicy_t;

typedef struct dds_durability_qospolicy {
  dds_durability_kind_t kind;
} dds_durability_qospolicy_t;

typedef struct dds_history_qospolicy {
  dds_history_kind_t kind;
  int32_t depth;
} dds_history_qospolicy_t;

typedef struct dds_resource_limits_qospolicy {
  int32_t max_samples;
  int32_t max_instances;
  int32_t max_samples_per_instance;
} dds_resource_limits_qospolicy_t;

typedef struct dds_durability_service_qospolicy {
  dds_duration_t service_cleanup_delay;
  dds_history_qospolicy_t history;
  dds_resource_limits_qospolicy_t resource_limits;
} dds_durability_service_qospolicy_t;

typedef struct dds_external_durability_service_qospolicy {
  ddsi_duration_t service_cleanup_delay;
  dds_history_qospolicy_t history;
  dds_resource_limits_qospolicy_t resource_limits;
} dds_external_durability_service_qospolicy_t;

typedef struct dds_presentation_qospolicy {
  dds_presentation_access_scope_kind_t access_scope;
  unsigned char coherent_access;
  unsigned char ordered_access;
} dds_presentation_qospolicy_t;

typedef struct dds_deadline_qospolicy {
  dds_duration_t deadline;
} dds_deadline_qospolicy_t;

typedef struct dds_external_deadline_qospolicy {
  ddsi_duration_t deadline;
} dds_external_deadline_qospolicy_t;

typedef struct dds_latency_budget_qospolicy {
  dds_duration_t duration;
} dds_latency_budget_qospolicy_t;

typedef struct dds_external_latency_budget_qospolicy {
  ddsi_duration_t duration;
} dds_external_latency_budget_qospolicy_t;

typedef struct dds_ownership_qospolicy {
  dds_ownership_kind_t kind;
} dds_ownership_qospolicy_t;

typedef struct dds_ownership_strength_qospolicy {
  int32_t value;
} dds_ownership_strength_qospolicy_t;

typedef struct dds_liveliness_qospolicy {
  dds_liveliness_kind_t kind;
  dds_duration_t lease_duration;
} dds_liveliness_qospolicy_t;

typedef struct dds_external_liveliness_qospolicy {
  dds_liveliness_kind_t kind;
  ddsi_duration_t lease_duration;
} dds_external_liveliness_qospolicy_t;

typedef struct dds_time_based_filter_qospolicy {
  dds_duration_t minimum_separation;
} dds_time_based_filter_qospolicy_t;

typedef struct dds_external_time_based_filter_qospolicy {
  ddsi_duration_t minimum_separation;
} dds_external_time_based_filter_qospolicy_t;

typedef struct ddsi_stringseq {
  uint32_t n;
  char **strs;
} ddsi_stringseq_t;

typedef ddsi_stringseq_t dds_partition_qospolicy_t;

typedef struct dds_reliability_qospolicy {
  dds_reliability_kind_t kind;
  dds_duration_t max_blocking_time;
} dds_reliability_qospolicy_t;

typedef enum dds_external_reliability_kind {
  DDS_EXTERNAL_RELIABILITY_BEST_EFFORT = 1,
  DDS_EXTERNAL_RELIABILITY_RELIABLE = 2
} dds_external_reliability_kind_t;

typedef struct dds_external_reliability_qospolicy {
  dds_external_reliability_kind_t kind;
  ddsi_duration_t max_blocking_time;
} dds_external_reliability_qospolicy_t;

typedef struct dds_transport_priority_qospolicy {
  int32_t value;
} dds_transport_priority_qospolicy_t;

typedef struct dds_lifespan_qospolicy {
  dds_duration_t duration;
} dds_lifespan_qospolicy_t;

typedef struct dds_external_lifespan_qospolicy {
  ddsi_duration_t duration;
} dds_external_lifespan_qospolicy_t;

typedef struct dds_destination_order_qospolicy {
  dds_destination_order_kind_t kind;
} dds_destination_order_qospolicy_t;

typedef struct dds_entity_factory_qospolicy {
  unsigned char autoenable_created_entities;
} dds_entity_factory_qospolicy_t;

typedef struct dds_writer_data_lifecycle_qospolicy {
  unsigned char autodispose_unregistered_instances;
} dds_writer_data_lifecycle_qospolicy_t;

typedef struct dds_reader_data_lifecycle_qospolicy {
  dds_duration_t autopurge_nowriter_samples_delay;
  dds_duration_t autopurge_disposed_samples_delay;
} dds_reader_data_lifecycle_qospolicy_t;

typedef struct dds_external_reader_data_lifecycle_qospolicy {
  ddsi_duration_t autopurge_nowriter_samples_delay;
  ddsi_duration_t autopurge_disposed_samples_delay;
} dds_external_reader_data_lifecycle_qospolicy_t;

typedef struct dds_subscription_keys_qospolicy {
  unsigned char use_key_list;
  ddsi_stringseq_t key_list;
} dds_subscription_keys_qospolicy_t;

typedef struct dds_reader_lifespan_qospolicy {
  unsigned char use_lifespan;
  dds_duration_t duration;
} dds_reader_lifespan_qospolicy_t;

typedef struct dds_external_reader_lifespan_qospolicy {
  unsigned char use_lifespan;
  ddsi_duration_t duration;
} dds_external_reader_lifespan_qospolicy_t;

typedef struct dds_ignorelocal_qospolicy {
  dds_ignorelocal_kind_t value;
} dds_ignorelocal_qospolicy_t;

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
#define QP_ADLINK_WRITER_DATA_LIFECYCLE   ((uint64_t)1 << 21)
#define QP_ADLINK_READER_DATA_LIFECYCLE   ((uint64_t)1 << 22)
#define QP_ADLINK_READER_LIFESPAN         ((uint64_t)1 << 24)
#define QP_ADLINK_SUBSCRIPTION_KEYS       ((uint64_t)1 << 25)
#define QP_ADLINK_ENTITY_FACTORY          ((uint64_t)1 << 27)
#define QP_CYCLONE_IGNORELOCAL               ((uint64_t)1 << 30)
#define QP_PROPERTY_LIST                     ((uint64_t)1 << 31)

/* Partition QoS is not RxO according to the specification (DDS 1.2,
   section 7.1.3), but communication will not take place unless it
   matches. Same for topic and type.  Relaxed qos matching is a bit of
   a weird one, but it affects matching, so ... */
#define QP_RXO_MASK (QP_DURABILITY | QP_PRESENTATION | QP_DEADLINE | QP_LATENCY_BUDGET | QP_OWNERSHIP | QP_LIVELINESS | QP_RELIABILITY | QP_DESTINATION_ORDER)
#define QP_CHANGEABLE_MASK (QP_USER_DATA | QP_TOPIC_DATA | QP_GROUP_DATA | QP_DEADLINE | QP_LATENCY_BUDGET | QP_OWNERSHIP_STRENGTH | QP_TIME_BASED_FILTER | QP_PARTITION | QP_TRANSPORT_PRIORITY | QP_LIFESPAN | QP_ADLINK_ENTITY_FACTORY | QP_ADLINK_WRITER_DATA_LIFECYCLE | QP_ADLINK_READER_DATA_LIFECYCLE)
#define QP_UNRECOGNIZED_INCOMPATIBLE_MASK ((uint64_t) 0)

/* readers & writers have an extended qos, hence why it is a separate
   type */
struct dds_qos {
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
  /*xxx */dds_presentation_qospolicy_t presentation;
  /*xxx */dds_partition_qospolicy_t partition;
  /*xxx */dds_groupdata_qospolicy_t group_data;
  /*x xX*/dds_entity_factory_qospolicy_t entity_factory;
  /*      TopicQos: */
  /*xxx */dds_topicdata_qospolicy_t topic_data;
  /*      DataWriterQos, DataReaderQos: */
  /*xxx */dds_durability_qospolicy_t durability;
  /*xxx */dds_durability_service_qospolicy_t durability_service;
  /*xxx */dds_deadline_qospolicy_t deadline;
  /*xxx */dds_latency_budget_qospolicy_t latency_budget;
  /*xxx */dds_liveliness_qospolicy_t liveliness;
  /*xxx */dds_reliability_qospolicy_t reliability;
  /*xxx */dds_destination_order_qospolicy_t destination_order;
  /*x x */dds_history_qospolicy_t history;
  /*x x */dds_resource_limits_qospolicy_t resource_limits;
  /*x x */dds_transport_priority_qospolicy_t transport_priority;
  /*xxx */dds_lifespan_qospolicy_t lifespan;
  /*xxx */dds_userdata_qospolicy_t user_data;
  /*xxx */dds_ownership_qospolicy_t ownership;
  /*xxxW*/dds_ownership_strength_qospolicy_t ownership_strength;
  /*xxxR*/dds_time_based_filter_qospolicy_t time_based_filter;
  /*x  W*/dds_writer_data_lifecycle_qospolicy_t writer_data_lifecycle;
  /*x xR*/dds_reader_data_lifecycle_qospolicy_t reader_data_lifecycle;
  /*x xR*/dds_subscription_keys_qospolicy_t subscription_keys;
  /*x xR*/dds_reader_lifespan_qospolicy_t reader_lifespan;
  /* x  */dds_ignorelocal_qospolicy_t ignorelocal;
  /*xxx */dds_property_qospolicy_t property;
};

struct nn_xmsg;

DDS_EXPORT void ddsi_xqos_init_empty (dds_qos_t *xqos);
DDS_EXPORT void ddsi_xqos_init_default_reader (dds_qos_t *xqos);
DDS_EXPORT void ddsi_xqos_init_default_writer (dds_qos_t *xqos);
DDS_EXPORT void ddsi_xqos_init_default_writer_noautodispose (dds_qos_t *xqos);
DDS_EXPORT void ddsi_xqos_init_default_subscriber (dds_qos_t *xqos);
DDS_EXPORT void ddsi_xqos_init_default_publisher (dds_qos_t *xqos);
DDS_EXPORT void ddsi_xqos_init_default_topic (dds_qos_t *xqos);
DDS_EXPORT void ddsi_xqos_copy (dds_qos_t *dst, const dds_qos_t *src);
DDS_EXPORT void ddsi_xqos_unalias (dds_qos_t *xqos);
DDS_EXPORT void ddsi_xqos_fini (dds_qos_t *xqos);
DDS_EXPORT void ddsi_xqos_fini_mask (dds_qos_t *xqos, uint64_t mask);
DDS_EXPORT dds_return_t ddsi_xqos_valid (const struct ddsrt_log_cfg *logcfg, const dds_qos_t *xqos);
DDS_EXPORT void ddsi_xqos_mergein_missing (dds_qos_t *a, const dds_qos_t *b, uint64_t mask);
DDS_EXPORT uint64_t ddsi_xqos_delta (const dds_qos_t *a, const dds_qos_t *b, uint64_t mask);
DDS_EXPORT void ddsi_xqos_addtomsg (struct nn_xmsg *m, const dds_qos_t *xqos, uint64_t wanted);
DDS_EXPORT void ddsi_xqos_log (uint32_t cat, const struct ddsrt_log_cfg *logcfg, const dds_qos_t *xqos);
DDS_EXPORT size_t ddsi_xqos_print (char * __restrict buf, size_t bufsize, const dds_qos_t *xqos);
DDS_EXPORT dds_qos_t *ddsi_xqos_dup (const dds_qos_t *src);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_XQOS_H */
