// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_XQOS_H
#define DDSI_XQOS_H

#include "dds/features.h"

#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsc/dds_public_qosdefs.h"
#include "dds/ddsi/ddsi_protocol.h"
#include "dds/ddsi/ddsi_log.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_typeinfo;

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

typedef struct dds_presentation_qospolicy {
  dds_presentation_access_scope_kind_t access_scope;
  unsigned char coherent_access;
  unsigned char ordered_access;
} dds_presentation_qospolicy_t;

typedef struct dds_deadline_qospolicy {
  dds_duration_t deadline;
} dds_deadline_qospolicy_t;

typedef struct dds_latency_budget_qospolicy {
  dds_duration_t duration;
} dds_latency_budget_qospolicy_t;

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

typedef struct dds_time_based_filter_qospolicy {
  dds_duration_t minimum_separation;
} dds_time_based_filter_qospolicy_t;

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

typedef struct dds_transport_priority_qospolicy {
  int32_t value;
} dds_transport_priority_qospolicy_t;

typedef struct dds_lifespan_qospolicy {
  dds_duration_t duration;
} dds_lifespan_qospolicy_t;

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

typedef struct dds_reader_lifespan_qospolicy {
  unsigned char use_lifespan;
  dds_duration_t duration;
} dds_reader_lifespan_qospolicy_t;

typedef struct dds_writer_batching_qospolicy {
  unsigned char batch_updates;
} dds_writer_batching_qospolicy_t;

typedef struct dds_ignorelocal_qospolicy {
  dds_ignorelocal_kind_t value;
} dds_ignorelocal_qospolicy_t;

typedef struct dds_type_consistency_enforcement_qospolicy {
  dds_type_consistency_kind_t kind;
  bool ignore_sequence_bounds;
  bool ignore_string_bounds;
  bool ignore_member_names;
  bool prevent_type_widening;
  bool force_type_validation;
} dds_type_consistency_enforcement_qospolicy_t;

typedef uint32_t dds_locator_mask_t;

typedef struct dds_data_representation_id_seq {
  uint32_t n;
  dds_data_representation_id_t *ids;
} dds_data_representation_id_seq_t;

typedef struct dds_data_representation_qospolicy {
  dds_data_representation_id_seq_t value;
} dds_data_representation_qospolicy_t;


/***/

/* Qos Present bit indices */
#define DDSI_QP_TOPIC_NAME                        ((uint64_t)1 <<  0)
#define DDSI_QP_TYPE_NAME                         ((uint64_t)1 <<  1)
#define DDSI_QP_PRESENTATION                      ((uint64_t)1 <<  2)
#define DDSI_QP_PARTITION                         ((uint64_t)1 <<  3)
#define DDSI_QP_GROUP_DATA                        ((uint64_t)1 <<  4)
#define DDSI_QP_TOPIC_DATA                        ((uint64_t)1 <<  5)
#define DDSI_QP_DURABILITY                        ((uint64_t)1 <<  6)
#define DDSI_QP_DURABILITY_SERVICE                ((uint64_t)1 <<  7)
#define DDSI_QP_DEADLINE                          ((uint64_t)1 <<  8)
#define DDSI_QP_LATENCY_BUDGET                    ((uint64_t)1 <<  9)
#define DDSI_QP_LIVELINESS                        ((uint64_t)1 << 10)
#define DDSI_QP_RELIABILITY                       ((uint64_t)1 << 11)
#define DDSI_QP_DESTINATION_ORDER                 ((uint64_t)1 << 12)
#define DDSI_QP_HISTORY                           ((uint64_t)1 << 13)
#define DDSI_QP_RESOURCE_LIMITS                   ((uint64_t)1 << 14)
#define DDSI_QP_TRANSPORT_PRIORITY                ((uint64_t)1 << 15)
#define DDSI_QP_LIFESPAN                          ((uint64_t)1 << 16)
#define DDSI_QP_USER_DATA                         ((uint64_t)1 << 17)
#define DDSI_QP_OWNERSHIP                         ((uint64_t)1 << 18)
#define DDSI_QP_OWNERSHIP_STRENGTH                ((uint64_t)1 << 19)
#define DDSI_QP_TIME_BASED_FILTER                 ((uint64_t)1 << 20)
#define DDSI_QP_ADLINK_WRITER_DATA_LIFECYCLE      ((uint64_t)1 << 21)
#define DDSI_QP_ADLINK_READER_DATA_LIFECYCLE      ((uint64_t)1 << 22)
#define DDSI_QP_ADLINK_READER_LIFESPAN            ((uint64_t)1 << 24)
#define DDSI_QP_CYCLONE_WRITER_BATCHING           ((uint64_t)1 << 25)
#define DDSI_QP_ADLINK_ENTITY_FACTORY             ((uint64_t)1 << 27)
#define DDSI_QP_CYCLONE_IGNORELOCAL               ((uint64_t)1 << 30)
#define DDSI_QP_PROPERTY_LIST                     ((uint64_t)1 << 31)
#define DDSI_QP_TYPE_CONSISTENCY_ENFORCEMENT      ((uint64_t)1 << 32)
#define DDSI_QP_TYPE_INFORMATION                  ((uint64_t)1 << 33)
#define DDSI_QP_LOCATOR_MASK                      ((uint64_t)1 << 34)
#define DDSI_QP_DATA_REPRESENTATION               ((uint64_t)1 << 35)
#define DDSI_QP_ENTITY_NAME                       ((uint64_t)1 << 36)


/* Partition QoS is not RxO according to the specification (DDS 1.2,
   section 7.1.3), but communication will not take place unless it
   matches. Same for topic and type.  Relaxed qos matching is a bit of
   a weird one, but it affects matching, so ... */
#define DDSI_QP_RXO_MASK (DDSI_QP_DURABILITY | DDSI_QP_PRESENTATION | DDSI_QP_DEADLINE | DDSI_QP_LATENCY_BUDGET | DDSI_QP_OWNERSHIP | DDSI_QP_LIVELINESS | DDSI_QP_RELIABILITY | DDSI_QP_DESTINATION_ORDER | DDSI_QP_DATA_REPRESENTATION)
#define DDSI_QP_CHANGEABLE_MASK (DDSI_QP_USER_DATA | DDSI_QP_TOPIC_DATA | DDSI_QP_GROUP_DATA | DDSI_QP_DEADLINE | DDSI_QP_LATENCY_BUDGET | DDSI_QP_OWNERSHIP_STRENGTH | DDSI_QP_TIME_BASED_FILTER | DDSI_QP_PARTITION | DDSI_QP_TRANSPORT_PRIORITY | DDSI_QP_LIFESPAN | DDSI_QP_ADLINK_ENTITY_FACTORY | DDSI_QP_ADLINK_WRITER_DATA_LIFECYCLE | DDSI_QP_ADLINK_READER_DATA_LIFECYCLE)
#define DDSI_QP_UNRECOGNIZED_INCOMPATIBLE_MASK ((uint64_t) 0)

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
  /* xx */char *entity_name;
#ifdef DDS_HAS_TYPE_DISCOVERY
  /* xx */struct ddsi_typeinfo *type_information;
#endif
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
  /*x xR*/dds_reader_lifespan_qospolicy_t reader_lifespan;
  /*x  W*/dds_writer_batching_qospolicy_t writer_batching;
  /* x  */dds_ignorelocal_qospolicy_t ignorelocal;
  /*xxx */dds_property_qospolicy_t property;
  /*xxxR*/dds_type_consistency_enforcement_qospolicy_t type_consistency;
  /*xxxX*/dds_locator_mask_t ignore_locator_type;
  /*xxx */dds_data_representation_qospolicy_t data_representation;
};

DDS_EXPORT extern const dds_qos_t ddsi_default_qos_reader;
DDS_EXPORT extern const dds_qos_t ddsi_default_qos_writer;
DDS_EXPORT extern const dds_qos_t ddsi_default_qos_topic;
DDS_EXPORT extern const dds_qos_t ddsi_default_qos_publisher_subscriber;
DDS_EXPORT extern const dds_qos_t ddsi_default_qos_participant;

/**
 * @brief Initialize a new empty dds_qos_t as an empty object
 * @component qos_handling
 *
 * In principle, this only clears the "present" and "aliased" bitmasks.  A debug build
 * additionally initializes all other bytes to 0x55.
 *
 * @param[out] xqos  qos object to be initialized.
 */
void ddsi_xqos_init_empty (dds_qos_t *xqos);

/**
 * @brief Copy "src" to "dst"
 * @component qos_handling
 *
 * @param[out]    dst     destination, any contents are overwritten
 * @param[in]     src     source dds_qos_t
 */
void ddsi_xqos_copy (dds_qos_t *dst, const dds_qos_t *src);

/**
 * @brief Free memory owned by "xqos"
 * @component qos_handling
 *
 * A dds_qos_t may own other allocated blocks of memory, depending on which fields are
 * set, their types and whether they are marked as "aliased".  This function releases any
 * such memory owned by "xqos", but not "xqos" itself.  Afterward, the content of "xqos"
 * is undefined and must not be used again without initialising it.
 *
 * @param[in] xqos   dds_qos_t for which to free memory
 */
void ddsi_xqos_fini (dds_qos_t *xqos);

/**
 * @brief Check whether xqos is valid according to the validation rules in the spec
 * @component qos_handling
 *
 * The checks concern the values for the individual fields as well as a few combinations
 * of fields.  Only those that are set are checked (the defaults are all valid anyway),
 * and where a combination of fields must be checked and some but not all fields are
 * specified, it uses the defaults for the missing ones.
 *
 * Invalid values get logged as category "plist" according to the specified logging
 * configuration.
 *
 * @param[in] logcfg  logging configuration
 * @param[in] xqos    qos object to check
 *
 * @returns DDS_RETCODE_OK or DDS_RETCODE_BAD_PARAMETER
 */
dds_return_t ddsi_xqos_valid (const struct ddsrt_log_cfg *logcfg, const dds_qos_t *xqos);

/**
 * @brief Extend "a" with selected entries present in "b"
 * @component qos_handling
 *
 * This copies into "a" any entries present in "b" that are included in "mask" and missing
 * in "a".  It doesn't touch any entries already present in "a".  Calling this on an empty
 * "a" with all bits set in "mask" is equivalent to copying "b" into "a"; calling this
 * with "mask" 0 copies nothing.
 *
 * @param[in,out] a       dds_qos_t to be extended
 * @param[in]     b       dds_qos_t from which to copy entries
 * @param[in]     mask    which to include (if DDSI_QP_X is set, include X)
 */
void ddsi_xqos_mergein_missing (dds_qos_t *a, const dds_qos_t *b, uint64_t mask);

/**
 * @brief Determine the set of entries in which "x" differs from "y"
 * @component qos_handling
 *
 * This computes the entries set in "x" but not set in "y", not set in "x" but set in "y",
 * or set in both "x" and "y" but to a different value.  It returns this set reduced to
 * only those included in "mask", that is, if bit X is clear in "mask", bit X will be
 * clear in the result.
 *
 * @param[in]  a         one of the two plists to compare
 * @param[in]  b         other plist to compare
 * @param[in]  mask      subset of entries to be compared
 *
 * @returns Bitmask of differences
 */
uint64_t ddsi_xqos_delta (const dds_qos_t *a, const dds_qos_t *b, uint64_t mask);

/**
 * @brief Add a property 'name' to the properties of "xqos" if it does not exists
 * @component qos_handling
 *
 * @param[in]  xqos        qos object to add property to.
 * @param[in]  propagate   whether to propagate (emit to wire) the property
 * @param[in]  name        property name
 * @param[in]  value       property value
 *
 * @returns true iff xqos was modified (property did not exist yet)
 */
bool ddsi_xqos_add_property_if_unset (dds_qos_t *xqos, bool propagate, const char *name, const char *value);

/**
 * @brief Duplicate "src"
 * @component qos_handling
 *
 * @param[in]  src       dds_qos_t to be duplicated
 *
 * @returns a new (allocated using ddsrt_malloc) dds_qos_t containing a copy of "src".
 */
dds_qos_t *ddsi_xqos_dup (const dds_qos_t *src);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_XQOS_H */
