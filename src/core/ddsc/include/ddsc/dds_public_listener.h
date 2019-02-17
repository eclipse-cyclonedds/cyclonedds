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
 * @brief DDS C Listener API
 *
 * This header file defines the public API of listeners in the
 * Eclipse Cyclone DDS C language binding.
 */
#ifndef _DDS_PUBLIC_LISTENER_H_
#define _DDS_PUBLIC_LISTENER_H_

#include "ddsc/dds_export.h"
#include "ddsc/dds_public_impl.h"
#include "ddsc/dds_public_status.h"
#include "os/os_public.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* Listener callbacks */
typedef void (*dds_on_inconsistent_topic_fn) (dds_entity_t topic, const dds_inconsistent_topic_status_t status, void* arg);
typedef void (*dds_on_liveliness_lost_fn) (dds_entity_t writer, const dds_liveliness_lost_status_t status, void* arg);
typedef void (*dds_on_offered_deadline_missed_fn) (dds_entity_t writer, const dds_offered_deadline_missed_status_t status, void* arg);
typedef void (*dds_on_offered_incompatible_qos_fn) (dds_entity_t writer, const dds_offered_incompatible_qos_status_t status, void* arg);
typedef void (*dds_on_data_on_readers_fn) (dds_entity_t subscriber, void* arg);
typedef void (*dds_on_sample_lost_fn) (dds_entity_t reader, const dds_sample_lost_status_t status, void* arg);
typedef void (*dds_on_data_available_fn) (dds_entity_t reader, void* arg);
typedef void (*dds_on_sample_rejected_fn) (dds_entity_t reader, const dds_sample_rejected_status_t status, void* arg);
typedef void (*dds_on_liveliness_changed_fn) (dds_entity_t reader, const dds_liveliness_changed_status_t status, void* arg);
typedef void (*dds_on_requested_deadline_missed_fn) (dds_entity_t reader, const dds_requested_deadline_missed_status_t status, void* arg);
typedef void (*dds_on_requested_incompatible_qos_fn) (dds_entity_t reader, const dds_requested_incompatible_qos_status_t status, void* arg);
typedef void (*dds_on_publication_matched_fn) (dds_entity_t writer, const dds_publication_matched_status_t  status, void* arg);
typedef void (*dds_on_subscription_matched_fn) (dds_entity_t reader, const dds_subscription_matched_status_t  status, void* arg);

#define DDS_LUNSET 0
struct dds_listener;
typedef struct dds_listener dds_listener_t;

/**
 * @brief Allocate memory and initializes to default values (::DDS_LUNSET) of a listener
 *
 * @param[in] arg optional pointer that will be passed on to the listener callbacks
 *
 * @return Returns a pointer to the allocated memory for dds_listener_t structure.
 */
_Ret_notnull_
DDS_EXPORT dds_listener_t* dds_create_listener (_In_opt_ void* arg);
_Ret_notnull_
DDS_DEPRECATED_EXPORT dds_listener_t* dds_listener_create (_In_opt_ void* arg);

/**
 * @brief Delete the memory allocated to listener structure
 *
 * @param[in] listener pointer to the listener struct to delete
 */
DDS_EXPORT void dds_delete_listener (_In_ _Post_invalid_ dds_listener_t * __restrict listener);
DDS_DEPRECATED_EXPORT void dds_listener_delete (_In_ _Post_invalid_ dds_listener_t * __restrict listener);

/**
 * @brief Reset the listener structure contents to ::DDS_LUNSET
 *
 * @param[in,out] listener pointer to the listener struct to reset
 */
DDS_EXPORT void dds_reset_listener (_Out_ dds_listener_t * __restrict listener);
DDS_DEPRECATED_EXPORT void dds_listener_reset (_Out_ dds_listener_t * __restrict listener);

/**
 * @brief Copy the listener callbacks from source to destination
 *
 * @param[in,out] dst The pointer to the destination listener structure, where the content is to copied
 * @param[in] src The pointer to the source listener structure to be copied
 */
DDS_EXPORT void dds_copy_listener (_Out_ dds_listener_t * __restrict dst, _In_ const dds_listener_t * __restrict src);
DDS_DEPRECATED_EXPORT void dds_listener_copy (_Out_ dds_listener_t * __restrict dst, _In_ const dds_listener_t * __restrict src);

/**
 * @brief Copy the listener callbacks from source to destination, unless already set
 *
 * Any listener callbacks already set in @p dst (including NULL) are skipped, only
 * those set to DDS_LUNSET are copied from @p src.
 *
 * @param[in,out] dst The pointer to the destination listener structure, where the content is merged
 * @param[in] src The pointer to the source listener structure to be copied
 */
DDS_EXPORT void dds_merge_listener (_Inout_ dds_listener_t * __restrict dst, _In_ const dds_listener_t * __restrict src);
DDS_DEPRECATED_EXPORT void dds_listener_merge (_Inout_ dds_listener_t * __restrict dst, _In_ const dds_listener_t * __restrict src);

/************************************************************************************************
 *  Setters
 ************************************************************************************************/

/**
 * @brief Set the inconsistent_topic callback in the listener structure.
 *
 * @param listener The pointer to the listener structure, where the callback will be set
 * @param callback The callback to set in the listener, can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lset_inconsistent_topic (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_inconsistent_topic_fn callback);

/**
 * @brief Set the liveliness_lost callback in the listener structure.
 *
 * @param[out] listener The pointer to the listener structure, where the callback will be set
 * @param[in] callback The callback to set in the listener, can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lset_liveliness_lost (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_liveliness_lost_fn callback);

/**
 * @brief Set the offered_deadline_missed callback in the listener structure.
 *
 * @param[in,out] listener The pointer to the listener structure, where the callback will be set
 * @param[in] callback The callback to set in the listener, can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lset_offered_deadline_missed (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_offered_deadline_missed_fn callback);

/**
 * @brief Set the offered_incompatible_qos callback in the listener structure.
 *
 * @param[in,out] listener The pointer to the listener structure, where the callback will be set
 * @param[in] callback The callback to set in the listener, can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lset_offered_incompatible_qos (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_offered_incompatible_qos_fn callback);

/**
 * @brief Set the data_on_readers callback in the listener structure.
 *
 * @param[in,out] listener The pointer to the listener structure, where the callback will be set
 * @param[in] callback The callback to set in the listener, can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lset_data_on_readers (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_data_on_readers_fn callback);

/**
 * @brief Set the sample_lost callback in the listener structure.
 *
 * @param[in,out] listener The pointer to the listener structure, where the callback will be set
 * @param[in] callback The callback to set in the listener, can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lset_sample_lost (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_sample_lost_fn callback);

/**
 * @brief Set the data_available callback in the listener structure.
 *
 * @param[in,out] listener The pointer to the listener structure, where the callback will be set
 * @param[in] callback The callback to set in the listener, can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lset_data_available (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_data_available_fn callback);

/**
 * @brief Set the sample_rejected callback in the listener structure.
 *
 * @param[in,out] listener The pointer to the listener structure, where the callback will be set
 * @param[in] callback The callback to set in the listener, can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lset_sample_rejected (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_sample_rejected_fn callback);

/**
 * @brief Set the liveliness_changed callback in the listener structure.
 *
 * @param[in,out] listener The pointer to the listener structure, where the callback will be set
 * @param[in] callback The callback to set in the listener, can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lset_liveliness_changed (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_liveliness_changed_fn callback);

/**
 * @brief Set the requested_deadline_missed callback in the listener structure.
 *
 * @param[in,out] listener The pointer to the listener structure, where the callback will be set
 * @param[in] callback The callback to set in the listener, can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lset_requested_deadline_missed (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_requested_deadline_missed_fn callback);

/**
 * @brief Set the requested_incompatible_qos callback in the listener structure.
 *
 * @param[in,out] listener The pointer to the listener structure, where the callback will be set
 * @param[in] callback The callback to set in the listener, can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lset_requested_incompatible_qos (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_requested_incompatible_qos_fn callback);

/**
 * @brief Set the publication_matched callback in the listener structure.
 *
 * @param[in,out] listener The pointer to the listener structure, where the callback will be set
 * @param[in] callback The callback to set in the listener, can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lset_publication_matched (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_publication_matched_fn callback);

/**
 * @brief Set the subscription_matched callback in the listener structure.
 *
 * @param[in,out] listener The pointer to the listener structure, where the callback will be set
 * @param[in] callback The callback to set in the listener, can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lset_subscription_matched (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_subscription_matched_fn callback);


/************************************************************************************************
 *  Getters
 ************************************************************************************************/

/**
 * @brief Get the inconsistent_topic callback from the listener structure.
 *
 * @param[in] listener The pointer to the listener structure, where the callback will be retrieved from
 * @param[in,out] callback Pointer where the retrieved callback can be stored; can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lget_inconsistent_topic (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_inconsistent_topic_fn *callback);

/**
 * @brief Get the liveliness_lost callback from the listener structure.
 *
 * @param[in] listener The pointer to the listener structure, where the callback will be retrieved from
 * @param[in,out] callback Pointer where the retrieved callback can be stored; can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lget_liveliness_lost (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_liveliness_lost_fn *callback);

/**
 * @brief Get the offered_deadline_missed callback from the listener structure.
 *
 * @param[in] listener The pointer to the listener structure, where the callback will be retrieved from
 * @param[in,out] callback Pointer where the retrieved callback can be stored; can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lget_offered_deadline_missed (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_offered_deadline_missed_fn *callback);

/**
 * @brief Get the offered_incompatible_qos callback from the listener structure.
 *
 * @param[in] listener The pointer to the listener structure, where the callback will be retrieved from
 * @param[in,out] callback Pointer where the retrieved callback can be stored; can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lget_offered_incompatible_qos (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_offered_incompatible_qos_fn *callback);

/**
 * @brief Get the data_on_readers callback from the listener structure.
 *
 * @param[in] listener The pointer to the listener structure, where the callback will be retrieved from
 * @param[in,out] callback Pointer where the retrieved callback can be stored; can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lget_data_on_readers (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_data_on_readers_fn *callback);

/**
 * @brief Get the sample_lost callback from the listener structure.
 *
 * @param[in] listener The pointer to the listener structure, where the callback will be retrieved from
 * @param[in,out] callback Pointer where the retrieved callback can be stored; can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lget_sample_lost (_In_ const dds_listener_t *__restrict listener, _Outptr_result_maybenull_ dds_on_sample_lost_fn *callback);

/**
 * @brief Get the data_available callback from the listener structure.
 *
 * @param[in] listener The pointer to the listener structure, where the callback will be retrieved from
 * @param[in,out] callback Pointer where the retrieved callback can be stored; can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lget_data_available (_In_ const dds_listener_t *__restrict listener, _Outptr_result_maybenull_ dds_on_data_available_fn *callback);

/**
 * @brief Get the sample_rejected callback from the listener structure.
 *
 * @param[in] listener The pointer to the listener structure, where the callback will be retrieved from
 * @param[in,out] callback Pointer where the retrieved callback can be stored; can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lget_sample_rejected (_In_ const dds_listener_t  *__restrict listener, _Outptr_result_maybenull_ dds_on_sample_rejected_fn *callback);

/**
 * @brief Get the liveliness_changed callback from the listener structure.
 *
 * @param[in] listener The pointer to the listener structure, where the callback will be retrieved from
 * @param[in,out] callback Pointer where the retrieved callback can be stored; can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lget_liveliness_changed (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_liveliness_changed_fn *callback);

/**
 * @brief Get the requested_deadline_missed callback from the listener structure.
 *
 * @param[in] listener The pointer to the listener structure, where the callback will be retrieved from
 * @param[in,out] callback Pointer where the retrieved callback can be stored; can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lget_requested_deadline_missed (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_requested_deadline_missed_fn *callback);

/**
 * @brief Get the requested_incompatible_qos callback from the listener structure.
 *
 * @param[in] listener The pointer to the listener structure, where the callback will be retrieved from
 * @param[in,out] callback Pointer where the retrieved callback can be stored; can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lget_requested_incompatible_qos (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_requested_incompatible_qos_fn *callback);

/**
 * @brief Get the publication_matched callback from the listener structure.
 *
 * @param[in] listener The pointer to the listener structure, where the callback will be retrieved from
 * @param[in,out] callback Pointer where the retrieved callback can be stored; can be NULL, ::DDS_LUNSET or a valid callback pointer
 */
DDS_EXPORT void dds_lget_publication_matched (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_publication_matched_fn *callback);

/**
 * @brief Get the subscription_matched callback from the listener structure.
 *
 * @param[in] callback Pointer where the retrieved callback can be stored; can be NULL, ::DDS_LUNSET or a valid callback pointer
 * @param[in,out] listener The pointer to the listener structure, where the callback will be retrieved from
 */
DDS_EXPORT void dds_lget_subscription_matched (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_subscription_matched_fn *callback);

#if defined (__cplusplus)
}
#endif

#endif /*_DDS_PUBLIC_LISTENER_H_*/
