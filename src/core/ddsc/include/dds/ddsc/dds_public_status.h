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
 * @brief DDS C Communication Status API
 *
 * This header file defines the public API of the Communication Status in the
 * Eclipse Cyclone DDS C language binding.
 */
#ifndef DDS_STATUS_H
#define DDS_STATUS_H

#include "dds/export.h"

#if defined (__cplusplus)
extern "C" {
#endif

/*
  Listeners implemented as structs containing callback functions
  that take listener status types as arguments.
*/

/* Listener status types */
/**
 * \ref DCPS_Status_OfferedDeadlineMissed
 */
typedef struct dds_offered_deadline_missed_status
{
  uint32_t total_count;
  int32_t total_count_change;
  dds_instance_handle_t last_instance_handle;
}
dds_offered_deadline_missed_status_t;

/**
 * \ref DCPS_Status_OfferedIncompatibleQoS
 */
typedef struct dds_offered_incompatible_qos_status
{
  uint32_t total_count;
  int32_t total_count_change;
  uint32_t last_policy_id;
}
dds_offered_incompatible_qos_status_t;

/**
 * \ref DCPS_Status_PublicationMatched
 */
typedef struct dds_publication_matched_status
{
  uint32_t total_count;
  int32_t total_count_change;
  uint32_t current_count;
  int32_t current_count_change;
  dds_instance_handle_t last_subscription_handle;
}
dds_publication_matched_status_t;

/**
 * \ref DCPS_Status_LivelinessLost
 */
typedef struct dds_liveliness_lost_status
{
  uint32_t total_count;
  int32_t total_count_change;
}
dds_liveliness_lost_status_t;

/**
 * \ref DCPS_Status_SubscriptionMatched
 */
typedef struct dds_subscription_matched_status
{
  uint32_t total_count;
  int32_t total_count_change;
  uint32_t current_count;
  int32_t current_count_change;
  dds_instance_handle_t last_publication_handle;
}
dds_subscription_matched_status_t;

/**
 * dds_sample_rejected_status_kind
 */
typedef enum
{
  DDS_NOT_REJECTED,
  DDS_REJECTED_BY_INSTANCES_LIMIT,
  DDS_REJECTED_BY_SAMPLES_LIMIT,
  DDS_REJECTED_BY_SAMPLES_PER_INSTANCE_LIMIT
}
dds_sample_rejected_status_kind;

/**
 * \ref DCPS_Status_SampleRejected
 */
typedef struct dds_sample_rejected_status
{
  uint32_t total_count;
  int32_t total_count_change;
  dds_sample_rejected_status_kind last_reason;
  dds_instance_handle_t last_instance_handle;
}
dds_sample_rejected_status_t;

/**
 * \ref DCPS_Status_LivelinessChanged
 */
typedef struct dds_liveliness_changed_status
{
  uint32_t alive_count;
  uint32_t not_alive_count;
  int32_t alive_count_change;
  int32_t not_alive_count_change;
  dds_instance_handle_t last_publication_handle;
}
dds_liveliness_changed_status_t;

/**
 * \ref DCPS_Status_RequestedDeadlineMissed
 */
typedef struct dds_requested_deadline_missed_status
{
  uint32_t total_count;
  int32_t total_count_change;
  dds_instance_handle_t last_instance_handle;
}
dds_requested_deadline_missed_status_t;

/**
 * \ref DCPS_Status_RequestedIncompatibleQoS
 */
typedef struct dds_requested_incompatible_qos_status
{
  uint32_t total_count;
  int32_t total_count_change;
  uint32_t last_policy_id;
}
dds_requested_incompatible_qos_status_t;

/**
 * \ref DCPS_Status_SampleLost
 */
typedef struct dds_sample_lost_status
{
  uint32_t total_count;
  int32_t total_count_change;
}
dds_sample_lost_status_t;

/**
 * \ref DCPS_Status_InconsistentTopic
 */
typedef struct dds_inconsistent_topic_status
{
  uint32_t total_count;
  int32_t total_count_change;
}
dds_inconsistent_topic_status_t;


/*
  get_<status> APIs return the status of an entity and resets the status
*/

/**
 * @brief Get INCONSISTENT_TOPIC status
 *
 * This operation gets the status value corresponding to INCONSISTENT_TOPIC
 * and reset the status. The value can be obtained, only if the status is enabled for an entity.
 * NULL value for status is allowed and it will reset the trigger value when status is enabled.
 *
 * @param[in]  topic  The entity to get the status
 * @param[out] status The pointer to \ref DCPS_Status_InconsistentTopic to get the status
 *
 * @returns  0 - Success
 * @returns <0 - Failure
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_get_inconsistent_topic_status (
  dds_entity_t topic,
  dds_inconsistent_topic_status_t * status);

/**
 * @brief Get PUBLICATION_MATCHED status
 *
 * This operation gets the status value corresponding to PUBLICATION_MATCHED
 * and reset the status. The value can be obtained, only if the status is enabled for an entity.
 * NULL value for status is allowed and it will reset the trigger value when status is enabled.
 *
 * @param[in]  writer  The entity to get the status
 * @param[out] status  The pointer to \ref DCPS_Status_PublicationMatched to get the status
 *
 * @returns  0 - Success
 * @returns <0 - Failure
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_get_publication_matched_status (
  dds_entity_t writer,
  dds_publication_matched_status_t * status);

/**
 * @brief Get LIVELINESS_LOST status
 *
 * This operation gets the status value corresponding to LIVELINESS_LOST
 * and reset the status. The value can be obtained, only if the status is enabled for an entity.
 * NULL value for status is allowed and it will reset the trigger value when status is enabled.
 *
 * @param[in]  writer  The entity to get the status
 * @param[out] status  The pointer to \ref DCPS_Status_LivelinessLost to get the status
 *
 * @returns  0 - Success
 * @returns <0 - Failure
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_get_liveliness_lost_status (
  dds_entity_t writer,
  dds_liveliness_lost_status_t * status);

/**
 * @brief Get OFFERED_DEADLINE_MISSED status
 *
 * This operation gets the status value corresponding to OFFERED_DEADLINE_MISSED
 * and reset the status. The value can be obtained, only if the status is enabled for an entity.
 * NULL value for status is allowed and it will reset the trigger value when status is enabled.
 *
 * @param[in]  writer  The entity to get the status
 * @param[out] status  The pointer to \ref DCPS_Status_OfferedDeadlineMissed to get the status
 *
 * @returns  0 - Success
 * @returns <0 - Failure
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_get_offered_deadline_missed_status(
  dds_entity_t writer,
  dds_offered_deadline_missed_status_t *status);

/**
 * @brief Get OFFERED_INCOMPATIBLE_QOS status
 *
 * This operation gets the status value corresponding to OFFERED_INCOMPATIBLE_QOS
 * and reset the status. The value can be obtained, only if the status is enabled for an entity.
 * NULL value for status is allowed and it will reset the trigger value when status is enabled.
 *
 * @param[in]  writer  The writer entity to get the status
 * @param[out] status  The pointer to \ref DCPS_Status_OfferedIncompatibleQoS to get the status
 *
 * @returns  0 - Success
 * @returns <0 - Failure
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_get_offered_incompatible_qos_status (
  dds_entity_t writer,
  dds_offered_incompatible_qos_status_t * status);

/**
 * @brief Get SUBSCRIPTION_MATCHED status
 *
 * This operation gets the status value corresponding to SUBSCRIPTION_MATCHED
 * and reset the status. The value can be obtained, only if the status is enabled for an entity.
 * NULL value for status is allowed and it will reset the trigger value when status is enabled.
 *
 * @param[in]  reader  The reader entity to get the status
 * @param[out] status  The pointer to \ref DCPS_Status_SubscriptionMatched to get the status
 *
 * @returns  0 - Success
 * @returns <0 - Failure
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_get_subscription_matched_status (
  dds_entity_t reader,
  dds_subscription_matched_status_t * status);

/**
 * @brief Get LIVELINESS_CHANGED status
 *
 * This operation gets the status value corresponding to LIVELINESS_CHANGED
 * and reset the status. The value can be obtained, only if the status is enabled for an entity.
 * NULL value for status is allowed and it will reset the trigger value when status is enabled.
 *
 * @param[in]  reader  The entity to get the status
 * @param[out] status  The pointer to \ref DCPS_Status_LivelinessChanged to get the status
 *
 * @returns  0 - Success
 * @returns <0 - Failure
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_get_liveliness_changed_status (
  dds_entity_t reader,
  dds_liveliness_changed_status_t * status);

/**
 * @brief Get SAMPLE_REJECTED status
 *
 * This operation gets the status value corresponding to SAMPLE_REJECTED
 * and reset the status. The value can be obtained, only if the status is enabled for an entity.
 * NULL value for status is allowed and it will reset the trigger value when status is enabled.
 *
 * @param[in]  reader  The entity to get the status
 * @param[out] status  The pointer to \ref DCPS_Status_SampleRejected to get the status
 *
 * @returns  0 - Success
 * @returns <0 - Failure
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_get_sample_rejected_status (
  dds_entity_t reader,
  dds_sample_rejected_status_t * status);

/**
 * @brief Get SAMPLE_LOST status
 *
 * This operation gets the status value corresponding to SAMPLE_LOST
 * and reset the status. The value can be obtained, only if the status is enabled for an entity.
 * NULL value for status is allowed and it will reset the trigger value when status is enabled.
 *
 * @param[in]  reader  The entity to get the status
 * @param[out] status  The pointer to \ref DCPS_Status_SampleLost to get the status
 *
 * @returns A dds_return_t indicating success or failure
 *
 * @retval DDS_RETCODE_OK
 *            Success
 * @retval DDS_RETCODE_ERROR
 *            An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *            The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *            The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_get_sample_lost_status (
  dds_entity_t reader,
  dds_sample_lost_status_t * status);

/**
 * @brief Get REQUESTED_DEADLINE_MISSED status
 *
 * This operation gets the status value corresponding to REQUESTED_DEADLINE_MISSED
 * and reset the status. The value can be obtained, only if the status is enabled for an entity.
 * NULL value for status is allowed and it will reset the trigger value when status is enabled.
 *
 * @param[in]  reader  The entity to get the status
 * @param[out] status  The pointer to \ref DCPS_Status_RequestedDeadlineMissed to get the status
 *
 * @returns A dds_return_t indicating success or failure
 *
 * @retval DDS_RETCODE_OK
 *            Success
 * @retval DDS_RETCODE_ERROR
 *            An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *            The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *            The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_get_requested_deadline_missed_status (
  dds_entity_t reader,
  dds_requested_deadline_missed_status_t * status);

/**
 * @brief Get REQUESTED_INCOMPATIBLE_QOS status
 *
 * This operation gets the status value corresponding to REQUESTED_INCOMPATIBLE_QOS
 * and reset the status. The value can be obtained, only if the status is enabled for an entity.
 * NULL value for status is allowed and it will reset the trigger value when status is enabled.
 *
 * @param[in]  reader  The entity to get the status
 * @param[out] status  The pointer to \ref DCPS_Status_RequestedIncompatibleQoS to get the status
 *
 * @returns A dds_return_t indicating success or failure
 *
 * @retval DDS_RETCODE_OK
 *            Success
 * @retval DDS_RETCODE_ERROR
 *            An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *            The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *            The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_get_requested_incompatible_qos_status (
  dds_entity_t reader,
  dds_requested_incompatible_qos_status_t * status);

#if defined (__cplusplus)
}
#endif
#endif
