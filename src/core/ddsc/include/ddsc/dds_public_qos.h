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
#ifndef DDS_QOS_H
#define DDS_QOS_H

#include "os/os_public.h"
#include "ddsc/dds_export.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* QoS identifiers */
/** @name QoS identifiers
  @{**/
#define DDS_INVALID_QOS_POLICY_ID 0
#define DDS_USERDATA_QOS_POLICY_ID 1
#define DDS_DURABILITY_QOS_POLICY_ID 2
#define DDS_PRESENTATION_QOS_POLICY_ID 3
#define DDS_DEADLINE_QOS_POLICY_ID 4
#define DDS_LATENCYBUDGET_QOS_POLICY_ID 5
#define DDS_OWNERSHIP_QOS_POLICY_ID 6
#define DDS_OWNERSHIPSTRENGTH_QOS_POLICY_ID 7
#define DDS_LIVELINESS_QOS_POLICY_ID 8
#define DDS_TIMEBASEDFILTER_QOS_POLICY_ID 9
#define DDS_PARTITION_QOS_POLICY_ID 10
#define DDS_RELIABILITY_QOS_POLICY_ID 11
#define DDS_DESTINATIONORDER_QOS_POLICY_ID 12
#define DDS_HISTORY_QOS_POLICY_ID 13
#define DDS_RESOURCELIMITS_QOS_POLICY_ID 14
#define DDS_ENTITYFACTORY_QOS_POLICY_ID 15
#define DDS_WRITERDATALIFECYCLE_QOS_POLICY_ID 16
#define DDS_READERDATALIFECYCLE_QOS_POLICY_ID 17
#define DDS_TOPICDATA_QOS_POLICY_ID 18
#define DDS_GROUPDATA_QOS_POLICY_ID 19
#define DDS_TRANSPORTPRIORITY_QOS_POLICY_ID 20
#define DDS_LIFESPAN_QOS_POLICY_ID 21
#define DDS_DURABILITYSERVICE_QOS_POLICY_ID 22
/** @}*/


/* QoS structure is opaque */
/** QoS structure */
typedef struct nn_xqos dds_qos_t;

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

/** History QoS: Applies to Topic, DataReader, DataWriter */
typedef struct dds_history_qospolicy
{
    dds_history_kind_t kind;
    int32_t depth;
}
dds_history_qospolicy_t;

/** ResourceLimits QoS: Applies to Topic, DataReader, DataWriter */
typedef struct dds_resource_limits_qospolicy
{
    int32_t max_samples;
    int32_t max_instances;
    int32_t max_samples_per_instance;
}
dds_resource_limits_qospolicy_t;

/** Presentation QoS: Applies to Publisher, Subscriber */
typedef enum dds_presentation_access_scope_kind
{
    DDS_PRESENTATION_INSTANCE,
    DDS_PRESENTATION_TOPIC,
    DDS_PRESENTATION_GROUP
}
dds_presentation_access_scope_kind_t;

/**
 * @brief Allocate memory and initialize default QoS-policies
 *
 * @returns - Pointer to the initialized dds_qos_t structure, NULL if unsuccessful.
 */
_Ret_notnull_
DDS_EXPORT
dds_qos_t * dds_create_qos (void);
_Ret_notnull_
DDS_DEPRECATED_EXPORT
dds_qos_t * dds_qos_create (void);

/**
 * @brief Delete memory allocated to QoS-policies structure
 *
 * @param[in] qos - Pointer to dds_qos_t structure
 */
DDS_EXPORT
void dds_delete_qos (
    _In_ _Post_invalid_ dds_qos_t * __restrict qos
);
DDS_DEPRECATED_EXPORT
void dds_qos_delete (
    _In_ _Post_invalid_ dds_qos_t * __restrict qos
);

/**
 * @brief Reset a QoS-policies structure to default values
 *
 * @param[in,out] qos - Pointer to the dds_qos_t structure
 */
DDS_EXPORT void
dds_reset_qos(
    _Out_ dds_qos_t * __restrict qos);
DDS_DEPRECATED_EXPORT
void dds_qos_reset (
    _Out_ dds_qos_t * __restrict qos
);

/**
 * @brief Copy all QoS-policies from one structure to another
 *
 * @param[in,out] dst - Pointer to the destination dds_qos_t structure
 * @param[in] src - Pointer to the source dds_qos_t structure
 *
 * @returns - Return-code indicating success or failure
 */
DDS_EXPORT
dds_return_t dds_copy_qos (
    _Out_ dds_qos_t * __restrict dst,
    _In_ const dds_qos_t * __restrict src
);
DDS_DEPRECATED_EXPORT
dds_return_t dds_qos_copy (
    _Out_ dds_qos_t * __restrict dst,
    _In_ const dds_qos_t * __restrict src
);

/**
 * @brief Copy all QoS-policies from one structure to another, unless already set
 *
 * Policies are copied from src to dst, unless src already has the policy set to a non-default value.
 *
 * @param[in,out] dst - Pointer to the destination qos structure
 * @param[in] src - Pointer to the source qos structure
 */
DDS_EXPORT
void dds_merge_qos
(
    _Inout_ dds_qos_t * __restrict dst,
    _In_ const dds_qos_t * __restrict src
);
DDS_DEPRECATED_EXPORT
void dds_qos_merge
(
    _Inout_ dds_qos_t * __restrict dst,
    _In_ const dds_qos_t * __restrict src
);

/**
 * @brief Copy all QoS-policies from one structure to another, unless already set
 *
 * Policies are copied from src to dst, unless src already has the policy set to a non-default value.
 *
 * @param[in,out] dst - Pointer to the destination qos structure
 * @param[in] src - Pointer to the source qos structure
 */
DDS_EXPORT
bool dds_qos_equal
(
    _In_ const dds_qos_t * __restrict a,
    _In_ const dds_qos_t * __restrict b
);

/**
 * @brief Set the userdata of a qos structure.
 *
 * @param[in,out] qos - Pointer to a dds_qos_t structure that will store the userdata
 * @param[in] value - Pointer to the userdata
 * @param[in] sz - Size of userdata stored in value
 */
DDS_EXPORT
void dds_qset_userdata
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_reads_bytes_opt_(sz) const void * __restrict value,
    _In_ size_t sz
);

/**
 * @brief Set the topicdata of a qos structure.
 *
 * @param[in,out] qos - Pointer to a dds_qos_t structure that will store the topicdata
 * @param[in] value - Pointer to the topicdata
 * @param[in] sz - Size of the topicdata stored in value
 */
DDS_EXPORT
void dds_qset_topicdata
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_reads_bytes_opt_(sz) const void * __restrict value,
    _In_ size_t sz
);

/**
 * @brief Set the groupdata of a qos structure.
 *
 * @param[in,out] qos - Pointer to a dds_qos_t structure that will store the groupdata
 * @param[in] value - Pointer to the group data
 * @param[in] sz - Size of groupdata stored in value
 */
DDS_EXPORT
void dds_qset_groupdata
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_reads_bytes_opt_(sz) const void * __restrict value,
    _In_ size_t sz
);

/**
 * @brief Set the durability policy of a qos structure.
 *
 * @param[in,out] qos - Pointer to a dds_qos_t structure that will store the policy
 * @param[in] kind - Durability kind value \ref DCPS_QoS_Durability
 */
DDS_EXPORT
void dds_qset_durability
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(DDS_DURABILITY_VOLATILE, DDS_DURABILITY_PERSISTENT) dds_durability_kind_t kind
);

/**
 * @brief Set the history policy of a qos structure.
 *
 * @param[in,out] qos - Pointer to a dds_qos_t structure that will store the policy
 * @param[in] kind - History kind value \ref DCPS_QoS_History
 * @param[in] depth - History depth value \ref DCPS_QoS_History
 */
DDS_EXPORT
void dds_qset_history
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(DDS_HISTORY_KEEP_LAST, DDS_HISTORY_KEEP_ALL) dds_history_kind_t kind,
    _In_range_(>=, DDS_LENGTH_UNLIMITED) int32_t depth
);

/**
 * @brief Set the resource limits policy of a qos structure.
 *
 * @param[in,out] qos - Pointer to a dds_qos_t structure that will store the policy
 * @param[in] max_samples - Number of samples resource-limit value
 * @param[in] max_instances - Number of instances resource-limit value
 * @param[in] max_samples_per_instance - Number of samples per instance resource-limit value
 */
DDS_EXPORT
void dds_qset_resource_limits
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(>=, DDS_LENGTH_UNLIMITED) int32_t max_samples,
    _In_range_(>=, DDS_LENGTH_UNLIMITED) int32_t max_instances,
    _In_range_(>=, DDS_LENGTH_UNLIMITED) int32_t max_samples_per_instance
);


/**
 * @brief Set the presentation policy of a qos structure.
 *
 * @param[in,out] qos - Pointer to a dds_qos_t structure that will store the policy
 * @param[in] access_scope - Access-scope kind
 * @param[in] coherent_access - Coherent access enable value
 * @param[in] ordered_access - Ordered access enable value
 */
DDS_EXPORT void dds_qset_presentation
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(DDS_PRESENTATION_INSTANCE, DDS_PRESENTATION_GROUP) dds_presentation_access_scope_kind_t access_scope,
    _In_ bool coherent_access,
    _In_ bool ordered_access
);

/**
 * @brief Set the lifespan policy of a qos structure.
 *
 * @param[in,out] qos - Pointer to a dds_qos_t structure that will store the policy
 * @param[in] lifespan - Lifespan duration (expiration time relative to source timestamp of a sample)
 */
DDS_EXPORT
void dds_qset_lifespan
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(0, DDS_INFINITY) dds_duration_t lifespan
);

/**
 * @brief Set the deadline policy of a qos structure.
 *
 * @param[in,out] qos - Pointer to a dds_qos_t structure that will store the policy
 * @param[in] deadline - Deadline duration
 */
DDS_EXPORT
void dds_qset_deadline
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(0, DDS_INFINITY) dds_duration_t deadline
);

/**
 * @brief Set the latency-budget policy of a qos structure
 *
 * @param[in,out] qos - Pointer to a dds_qos_t structure that will store the policy
 * @param[in] duration - Latency budget duration
 */
DDS_EXPORT
void dds_qset_latency_budget
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(0, DDS_INFINITY) dds_duration_t duration
);

/**
 * @brief Set the ownership policy of a qos structure
 *
 * @param[in,out] qos - Pointer to a dds_qos_t structure that will store the policy
 * @param[in] kind - Ownership kind
 */
DDS_EXPORT
void dds_qset_ownership
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(DDS_OWNERSHIP_SHARED, DDS_OWNERSHIP_EXCLUSIVE) dds_ownership_kind_t kind
);

/**
 * @brief Set the ownership strength policy of a qos structure
 *
 * param[in,out] qos - Pointer to a dds_qos_t structure that will store the policy
 * param[in] value - Ownership strength value
 */
DDS_EXPORT
void dds_qset_ownership_strength
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_ int32_t value
);

/**
 * @brief Set the liveliness policy of a qos structure
 *
 * param[in,out] qos - Pointer to a dds_qos_t structure that will store the policy
 * param[in] kind - Liveliness kind
 * param[in[ lease_duration - Lease duration
 */
DDS_EXPORT
void dds_qset_liveliness
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(DDS_LIVELINESS_AUTOMATIC, DDS_LIVELINESS_MANUAL_BY_TOPIC) dds_liveliness_kind_t kind,
    _In_range_(0, DDS_INFINITY) dds_duration_t lease_duration
);

/**
 * @brief Set the time-based filter policy of a qos structure
 *
 * @param[in,out] qos - Pointer to a dds_qos_t structure that will store the policy
 * @param[in] minimum_separation - Minimum duration between sample delivery for an instance
 */
DDS_EXPORT
void dds_qset_time_based_filter
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(0, DDS_INFINITY) dds_duration_t minimum_separation
);

/**
 * @brief Set the partition policy of a qos structure
 *
 * @param[in,out] qos - Pointer to a dds_qos_t structure that will store the policy
 * @param[in] n - Number of partitions stored in ps
 * @param[in[ ps - Pointer to string(s) storing partition name(s)
 */
DDS_EXPORT
void dds_qset_partition
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_ uint32_t n,
    _In_count_(n) _Deref_pre_z_ const char ** __restrict ps
);

/**
 * @brief Set the reliability policy of a qos structure
 *
 * @param[in,out] qos - Pointer to a dds_qos_t structure that will store the policy
 * @param[in] kind - Reliability kind
 * @param[in] max_blocking_time - Max blocking duration applied when kind is reliable.
 */
DDS_EXPORT
void dds_qset_reliability
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(DDS_RELIABILITY_BEST_EFFORT, DDS_RELIABILITY_RELIABLE) dds_reliability_kind_t kind,
    _In_range_(0, DDS_INFINITY) dds_duration_t max_blocking_time
);

/**
 * @brief Set the transport-priority policy of a qos structure
 *
 * @param[in,out] qos - Pointer to a dds_qos_t structure that will store the policy
 * @param[in] value - Priority value
 */
DDS_EXPORT
void dds_qset_transport_priority
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_ int32_t value
);

/**
 * @brief Set the destination-order policy of a qos structure
 *
 * @param[in,out] qos - Pointer to a dds_qos_t structure that will store the policy
 * @param[in] kind - Destination-order kind
 */
DDS_EXPORT
void dds_qset_destination_order
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP,
        DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP) dds_destination_order_kind_t kind
);

/**
 * @brief Set the writer data-lifecycle policy of a qos structure
 *
 * @param[in,out] qos - Pointer to a dds_qos_t structure that will store the policy
 * @param[in] autodispose_unregistered_instances - Automatic disposal of unregistered instances
 */
DDS_EXPORT
void dds_qset_writer_data_lifecycle
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_ bool autodispose
);

/**
 * @brief Set the reader data-lifecycle policy of a qos structure
 *
 * @param[in,out] qos - Pointer to a dds_qos_t structure that will store the policy
 * @param[in] autopurge_nowriter_samples_delay - Delay for purging of samples from instances in a no-writers state
 * @param[in] autopurge_disposed_samples_delay - Delay for purging of samples from disposed instances
 */
DDS_EXPORT
void dds_qset_reader_data_lifecycle
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(0, DDS_INFINITY) dds_duration_t autopurge_nowriter_samples_delay,
    _In_range_(0, DDS_INFINITY) dds_duration_t autopurge_disposed_samples_delay
);

/**
 * @brief Set the durability-service policy of a qos structure
 *
 * @param[in,out] qos - Pointer to a dds_qos_t structure that will store the policy
 * @param[in] service_cleanup_delay - Delay for purging of abandoned instances from the durability service
 * @param[in] history_kind - History policy kind applied by the durability service
 * @param[in] history_depth - History policy depth applied by the durability service
 * @param[in] max_samples - Number of samples resource-limit policy applied by the durability service
 * @param[in] max_instances - Number of instances resource-limit policy applied by the durability service
 * @param[in] max_samples_per_instance - Number of samples per instance resource-limit policy applied by the durability service
 */
DDS_EXPORT
void dds_qset_durability_service
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(0, DDS_INFINITY) dds_duration_t service_cleanup_delay,
    _In_range_(DDS_HISTORY_KEEP_LAST, DDS_HISTORY_KEEP_ALL) dds_history_kind_t history_kind,
    _In_range_(>=, DDS_LENGTH_UNLIMITED) int32_t history_depth,
    _In_range_(>=, DDS_LENGTH_UNLIMITED) int32_t max_samples,
    _In_range_(>=, DDS_LENGTH_UNLIMITED) int32_t max_instances,
    _In_range_(>=, DDS_LENGTH_UNLIMITED) int32_t max_samples_per_instance
);

/**
 * @brief Get the userdata from a qos structure
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out] value - Pointer that will store the userdata
 * @param[in,out] sz - Pointer that will store the size of userdata
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
 */
DDS_EXPORT bool dds_qget_userdata (const dds_qos_t * __restrict qos, void **value, size_t *sz);

/**
 * @brief Get the topicdata from a qos structure
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out] value - Pointer that will store the topicdata
 * @param[in,out] sz - Pointer that will store the size of topicdata
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
*/
DDS_EXPORT bool dds_qget_topicdata (const dds_qos_t * __restrict qos, void **value, size_t *sz);

/**
 * @brief Get the groupdata from a qos structure
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out] value - Pointer that will store the groupdata
 * @param[in,out] sz - Pointer that will store the size of groupdata
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
 */
DDS_EXPORT bool dds_qget_groupdata (const dds_qos_t * __restrict qos, void **value, size_t *sz);

/**
 * @brief Get the durability policy from a qos structure
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out] kind - Pointer that will store the durability kind
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
 */
DDS_EXPORT bool dds_qget_durability (const dds_qos_t * __restrict qos, dds_durability_kind_t *kind);

/**
 * @brief Get the history policy from a qos structure
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out] kind - Pointer that will store the history kind (optional)
 * @param[in,out] depth - Pointer that will store the history depth (optional)
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
 */
DDS_EXPORT bool dds_qget_history (const dds_qos_t * __restrict qos, dds_history_kind_t *kind, int32_t *depth);

/**
 * @brief Get the resource-limits policy from a qos structure
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out] max_samples - Pointer that will store the number of samples resource-limit (optional)
 * @param[in,out] max_instances - Pointer that will store the number of instances resource-limit (optional)
 * @param[in,out] max_samples_per_instance - Pointer that will store the number of samples per instance resource-limit (optional)
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
 */
DDS_EXPORT bool dds_qget_resource_limits (const dds_qos_t * __restrict qos, int32_t *max_samples, int32_t *max_instances, int32_t *max_samples_per_instance);

/**
 * @brief Get the presentation policy from a qos structure
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out] access_scope - Pointer that will store access scope kind (optional)
 * @param[in,out] coherent_access - Pointer that will store coherent access enable value (optional)
 * @param[in,out] ordered_access - Pointer that will store orderede access enable value (optional)
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
 */
DDS_EXPORT bool dds_qget_presentation (const dds_qos_t * __restrict qos, dds_presentation_access_scope_kind_t *access_scope, bool *coherent_access, bool *ordered_access);

/**
 * @brief Get the lifespan policy from a qos structure
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out] lifespan - Pointer that will store lifespan duration
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
 */
DDS_EXPORT bool dds_qget_lifespan (const dds_qos_t * __restrict qos, dds_duration_t *lifespan);

/**
 * @brief Get the deadline policy from a qos structure
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out] deadline - Pointer that will store deadline duration
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
 */
DDS_EXPORT bool dds_qget_deadline (const dds_qos_t * __restrict qos, dds_duration_t *deadline);

/**
 * @brief Get the latency-budget policy from a qos structure
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out] duration - Pointer that will store latency-budget duration
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
 */
DDS_EXPORT bool dds_qget_latency_budget (const dds_qos_t * __restrict qos, dds_duration_t *duration);

/**
 * @brief Get the ownership policy from a qos structure
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out] kind - Pointer that will store the ownership kind
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
 */
DDS_EXPORT bool dds_qget_ownership (const dds_qos_t * __restrict qos, dds_ownership_kind_t *kind);

/**
 * @brief Get the ownership strength qos policy
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out] value - Pointer that will store the ownership strength value
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
 */
DDS_EXPORT bool dds_qget_ownership_strength (const dds_qos_t * __restrict qos, int32_t *value);

/**
 * @brief Get the liveliness qos policy
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out] kind - Pointer that will store the liveliness kind (optional)
 * @param[in,out] lease_duration - Pointer that will store the liveliness lease duration (optional)
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
 */
DDS_EXPORT bool dds_qget_liveliness (const dds_qos_t * __restrict qos, dds_liveliness_kind_t *kind, dds_duration_t *lease_duration);

/**
 * @brief Get the time-based filter qos policy
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out] minimum_separation - Pointer that will store the minimum separation duration (optional)
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
 */
DDS_EXPORT bool dds_qget_time_based_filter (const dds_qos_t * __restrict qos, dds_duration_t *minimum_separation);

/**
 * @brief Get the partition qos policy
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out] n - Pointer that will store the number of partitions (optional)
 * @param[in,out] ps - Pointer that will store the string(s) containing partition name(s) (optional)
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
 */
DDS_EXPORT bool dds_qget_partition (const dds_qos_t * __restrict qos, uint32_t *n, char ***ps);

/**
 * @brief Get the reliability qos policy
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out] kind - Pointer that will store the reliability kind (optional)
 * @param[in,out] max_blocking_time - Pointer that will store the max blocking time for reliable reliability (optional)
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
 */
DDS_EXPORT bool dds_qget_reliability (const dds_qos_t * __restrict qos, dds_reliability_kind_t *kind, dds_duration_t *max_blocking_time);

/**
 * @brief Get the transport priority qos policy
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out] value - Pointer that will store the transport priority value
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
 */
DDS_EXPORT bool dds_qget_transport_priority (const dds_qos_t * __restrict qos, int32_t *value);

/**
 * @brief Get the destination-order qos policy
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out] kind - Pointer that will store the destination-order kind
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
 */
DDS_EXPORT bool dds_qget_destination_order (const dds_qos_t * __restrict qos, dds_destination_order_kind_t *kind);

/**
 * @brief Get the writer data-lifecycle qos policy
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out] autodispose_unregistered_instances - Pointer that will store the autodispose unregistered instances enable value
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
 */
DDS_EXPORT bool dds_qget_writer_data_lifecycle (const dds_qos_t * __restrict qos, bool *autodispose);

/**
 * @brief Get the reader data-lifecycle qos policy
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out] autopurge_nowriter_samples_delay - Pointer that will store the delay for auto-purging samples from instances in a no-writer state (optional)
 * @param[in,out] autopurge_disposed_samples_delay - Pointer that will store the delay for auto-purging of disposed instances (optional)
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
 */
DDS_EXPORT bool dds_qget_reader_data_lifecycle (const dds_qos_t * __restrict qos, dds_duration_t *autopurge_nowriter_samples_delay, dds_duration_t *autopurge_disposed_samples_delay);

/**
 * @brief Get the durability-service qos policy values.
 *
 * @param[in] qos - Pointer to a dds_qos_t structure storing the policy
 * @param[in,out]  service_cleanup_delay - Pointer that will store the delay for purging of abandoned instances from the durability service (optional)
 * @param[in,out] history_kind - Pointer that will store history policy kind applied by the durability service (optional)
 * @param[in,out] history_depth - Pointer that will store history policy depth applied by the durability service (optional)
 * @param[in,out] max_samples - Pointer that will store number of samples resource-limit policy applied by the durability service (optional)
 * @param[in,out] max_instances - Pointer that will store number of instances resource-limit policy applied by the durability service (optional)
 * @param[in,out] max_samples_per_instance - Pointer that will store number of samples per instance resource-limit policy applied by the durability service (optional)
 *
 * @returns - false iff any of the arguments is invalid or the qos is not present in the qos object
 */
DDS_EXPORT bool dds_qget_durability_service (const dds_qos_t * __restrict qos, dds_duration_t *service_cleanup_delay, dds_history_kind_t *history_kind, int32_t *history_depth, int32_t *max_samples, int32_t *max_instances, int32_t *max_samples_per_instance);

#if defined (__cplusplus)
}
#endif
#endif
