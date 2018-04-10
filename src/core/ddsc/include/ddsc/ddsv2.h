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
#ifndef DDS_H
#define DDS_H

/** @file
 *
 *  @brief C DDS header
 */

#if defined (__cplusplus)
#define restrict
#endif

#include "os/os_public.h"
#include "ddsc/dds_export.h"

/* TODO: Move to appropriate location */
typedef _Return_type_success_(return >= 0) int32_t dds_return_t;
typedef _Return_type_success_(return >  0) int32_t dds_entity_t;

/* Sub components */

#include "ddsc/dds_public_stream.h"
#include "ddsc/dds_public_impl.h"
#include "ddsc/dds_public_alloc.h"
#include "ddsc/dds_public_time.h"
#include "ddsc/dds_public_qos.h"
#include "ddsc/dds_public_error.h"
#include "ddsc/dds_public_status.h"
#include "ddsc/dds_public_listener.h"
#include "ddsc/dds_public_log.h"

#if defined (__cplusplus)
extern "C" {
#endif

/**
 * Description : Returns the default DDS domain id. This can be configured
 * in xml or set as an evironment variable ({DDSC_PROJECT_NAME_NOSPACE_CAPS}_DOMAIN).
 *
 * Arguments :
 *   -# None
 *   -# Returns the default domain id
 */
DDS_EXPORT dds_domainid_t dds_get_default_domainid (void);

/** @name Communication Status definitions
  @{**/
#define DDS_INCONSISTENT_TOPIC_STATUS          1u
#define DDS_OFFERED_DEADLINE_MISSED_STATUS     2u
#define DDS_REQUESTED_DEADLINE_MISSED_STATUS   4u
#define DDS_OFFERED_INCOMPATIBLE_QOS_STATUS    32u
#define DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS  64u
#define DDS_SAMPLE_LOST_STATUS                 128u
#define DDS_SAMPLE_REJECTED_STATUS             256u
#define DDS_DATA_ON_READERS_STATUS             512u
#define DDS_DATA_AVAILABLE_STATUS              1024u
#define DDS_LIVELINESS_LOST_STATUS             2048u
#define DDS_LIVELINESS_CHANGED_STATUS          4096u
#define DDS_PUBLICATION_MATCHED_STATUS         8192u
#define DDS_SUBSCRIPTION_MATCHED_STATUS        16384u
/** @}*/

/**
 * dds_sample_state_t
 * \brief defines the state for a data value
 * -# DDS_SST_READ - DataReader has already accessed the sample by read
 * -# DDS_SST_NOT_READ - DataReader has not accessed that sample before
 */
typedef enum dds_sample_state
{
  DDS_SST_READ = DDS_READ_SAMPLE_STATE,
  DDS_SST_NOT_READ = DDS_NOT_READ_SAMPLE_STATE
}
dds_sample_state_t;

/**
 * dds_view_state_t
 * \brief defines the view state of an instance relative to the samples
 * -# DDS_VST_NEW - DataReader is accessing the sample for the first time when the
 *                  instance is alive
 * -# DDS_VST_OLD - DataReader has accessed the sample before
 */
typedef enum dds_view_state
{
  DDS_VST_NEW = DDS_NEW_VIEW_STATE,
  DDS_VST_OLD = DDS_NOT_NEW_VIEW_STATE
}
dds_view_state_t;

/**
 * dds_instance_state_t
 * \brief defines the state of the instance
 * -# DDS_IST_ALIVE - Samples received for the instance from the live data writers
 * -# DDS_IST_NOT_ALIVE_DISPOSED - Instance was explicitly disposed by the data writer
 * -# DDS_IST_NOT_ALIVE_NO_WRITERS - Instance has been declared as not alive by data reader
 *                                   as there are no live data writers writing that instance
 */
typedef enum dds_instance_state
{
  DDS_IST_ALIVE = DDS_ALIVE_INSTANCE_STATE,
  DDS_IST_NOT_ALIVE_DISPOSED = DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE,
  DDS_IST_NOT_ALIVE_NO_WRITERS = DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE
}
dds_instance_state_t;

/**
 * Structure dds_sample_info_t - contains information about the associated data value
 * -# sample_state - \ref dds_sample_state_t
 * -# view_state - \ref dds_view_state_t
 * -# instance_state - \ref dds_instance_state_t
 * -# valid_data - indicates whether there is a data associated with a sample
 *    - true, indicates the data is valid
 *    - false, indicates the data is invalid, no data to read
 * -# source_timestamp - timestamp of a data instance when it is written
 * -# instance_handle - handle to the data instance
 * -# publication_handle - handle to the publisher
 * -# disposed_generation_count - count of instance state change from
 *    NOT_ALIVE_DISPOSED to ALIVE
 * -# no_writers_generation_count - count of instance state change from
 *    NOT_ALIVE_NO_WRITERS to ALIVE
 * -# sample_rank - indicates the number of samples of the same instance
 *    that follow the current one in the collection
 * -# generation_rank - difference in generations between the sample and most recent sample
 *    of the same instance that appears in the returned collection
 * -# absolute_generation_rank - difference in generations between the sample and most recent sample
 *    of the same instance when read/take was called
 * -# reception_timestamp - timestamp of a data instance when it is added to a read queue
 */
typedef struct dds_sample_info
{
  dds_sample_state_t sample_state;
  dds_view_state_t view_state;
  dds_instance_state_t instance_state;
  bool valid_data;
  dds_time_t source_timestamp;
  dds_instance_handle_t instance_handle;
  dds_instance_handle_t publication_handle;
  uint32_t disposed_generation_count;
  uint32_t no_writers_generation_count;
  uint32_t sample_rank;
  uint32_t generation_rank;
  uint32_t absolute_generation_rank;
  dds_time_t reception_timestamp; /* NOTE: VLite extension */
}
dds_sample_info_t;

/*
  All entities are represented by a process-private handle, with one
  call to enable an entity when it was created disabled.
  An entity is created enabled by default.
  Note: disabled creation is currently not supported.
*/

/**
 * @brief Enable entity.
 *
 * @note Delayed entity enabling is not supported yet (CHAM-96).
 *
 * This operation enables the dds_entity_t. Created dds_entity_t objects can start in
 * either an enabled or disabled state. This is controlled by the value of the
 * entityfactory policy on the corresponding parent entity for the given
 * entity. Enabled entities are immediately activated at creation time meaning
 * all their immutable QoS settings can no longer be changed. Disabled Entities are not
 * yet activated, so it is still possible to change their immutable QoS settings. However,
 * once activated the immutable QoS settings can no longer be changed.
 * Creating disabled entities can make sense when the creator of the DDS_Entity
 * does not yet know which QoS settings to apply, thus allowing another piece of code
 * to set the QoS later on.
 *
 * The default setting of DDS_EntityFactoryQosPolicy is such that, by default,
 * entities are created in an enabled state so that it is not necessary to explicitly call
 * dds_enable on newly-created entities.
 *
 * The dds_enable operation produces the same results no matter how
 * many times it is performed. Calling dds_enable on an already
 * enabled DDS_Entity returns DDS_RETCODE_OK and has no effect.
 *
 * If an Entity has not yet been enabled, the only operations that can be invoked
 * on it are: the ones to set, get or copy the QosPolicy settings, the ones that set
 * (or get) the Listener, the ones that get the Status and the dds_get_status_changes
 * operation (although the status of a disabled entity never changes). Other operations
 * will return the error DDS_RETCODE_NOT_ENABLED.
 *
 * Entities created with a parent that is disabled, are created disabled regardless of
 * the setting of the entityfactory policy.
 *
 * Calling dds_enable on an Entity whose parent is not enabled
 * will fail and return DDS_RETCODE_PRECONDITION_NOT_MET.
 *
 * If the entityfactory policy has autoenable_created_entities
 * set to TRUE, the dds_enable operation on the parent will
 * automatically enable all child entities created with the parent.
 *
 * The Listeners associated with an Entity are not called until the
 * Entity is enabled. Conditions associated with an Entity that
 * is not enabled are "inactive", that is, have a trigger_value which is FALSE.
 *
 * @param[in]  e        The entity to enable.
 *
 * @returns  0 - Success (DDS_RETCODE_OK).
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_OK
 *                  The listeners of to the entity have been successfully been
 *                  copied into the specified listener parameter.
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *                  The parent of the given Entity is not enabled.
 */
_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
DDS_EXPORT _Check_return_ dds_return_t
dds_enable(
        _In_ dds_entity_t entity);

/*
  All entities are represented by a process-private handle, with one
  call to delete an entity and all entities it logically contains.
  That is, it is equivalent to combination of
  delete_contained_entities and delete_xxx in the DCPS API.
*/

/**
 * @brief Delete given entity.
 *
 * This operation will delete the given entity. It will also automatically
 * delete all its children, childrens' children, etc entities.
 *
 * TODO: Link to generic dds entity relations documentation.
 *
 * @param[in]  entity  Entity from which to get its parent.
 *
 * @returns  0 - Success (DDS_RETCODE_OK).
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  The entity and its children (recursive are deleted).
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
DDS_EXPORT dds_return_t
dds_delete(
        _In_ dds_entity_t entity);


/**
 * @brief Get entity publisher.
 *
 * This operation returns the publisher to which the given entity belongs.
 * For instance, it will return the Publisher that was used when
 * creating a DataWriter (when that DataWriter was provided here).
 *
 * TODO: Link to generic dds entity relations documentation.
 *
 * @param[in]  entity  Entity from which to get its publisher.
 *
 * @returns >0 - Success (valid entity handle).
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
_Pre_satisfies_(((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER))
DDS_EXPORT dds_entity_t
dds_get_publisher(
        _In_ dds_entity_t writer);


/**
 * @brief Get entity subscriber.
 *
 * This operation returns the subscriber to which the given entity belongs.
 * For instance, it will return the Subscriber that was used when
 * creating a DataReader (when that DataReader was provided here).
 *
 * TODO: Link to generic dds entity relations documentation.
 *
 * @param[in]  entity  Entity from which to get its subscriber.
 *
 * @returns >0 - Success (valid entity handle).
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
_Pre_satisfies_(((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER    ) || \
                ((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY) )
DDS_EXPORT dds_entity_t
dds_get_subscriber(
        _In_ dds_entity_t entity);


/**
 * @brief Get entity datareader.
 *
 * This operation returns the datareader to which the given entity belongs.
 * For instance, it will return the DataReader that was used when
 * creating a ReadCondition (when that ReadCondition was provided here).
 *
 * TODO: Link to generic dds entity relations documentation.
 *
 * @param[in]  entity  Entity from which to get its datareader.
 *
 * @returns >0 - Success (valid entity handle).
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
_Pre_satisfies_(((condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY) )
DDS_EXPORT dds_entity_t
dds_get_datareader(
        _In_ dds_entity_t condition);



/**
 * @brief Get the mask of a condition.
 *
 * This operation returns the mask that was used to create the given
 * condition.
 *
 * @param[in]  condition  Read or Query condition that has a mask.
 *
 * @returns  0 - Success (given mask is set).
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  The mask arg is NULL.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
_Pre_satisfies_(((condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY) )
DDS_EXPORT _Check_return_ dds_return_t
dds_get_mask(
        _In_ dds_entity_t condition,
        _Out_ uint32_t   *mask);

/* TODO: document. */
_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
DDS_EXPORT _Check_return_ dds_return_t
dds_instancehandle_get(
        _In_  dds_entity_t entity,
        _Out_ dds_instance_handle_t *ihdl);

/*
  All entities have a set of "status conditions" (following the DCPS
  spec), read peeks, take reads & resets (analogously to read & take
  operations on reader). The "mask" allows operating only on a subset
  of the statuses. Enabled status analogously to DCPS spec.
*/


/**
 * Description : Read the status(es) set for the entity based on the enabled
 * status and mask set. This operation does not clear the read status(es).
 *
 * Arguments :
 *   -# e Entity on which the status has to be read
 *   -# status Returns the status set on the entity, based on the enabled status
 *   -# mask Filter the status condition to be read (can be NULL)
 *   -# Returns 0 on success, or a non-zero error value if the mask does not
 *      correspond to the entity
 */
_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
DDS_EXPORT _Check_return_ dds_return_t
dds_read_status(
        _In_  dds_entity_t entity,
        _Out_ uint32_t *status,
        _In_  uint32_t mask);

/**
 * Description : Read the status(es) set for the entity based on the enabled
 * status and mask set. This operation clears the status set after reading.
 *
 * Arguments :
 *   -# e Entity on which the status has to be read
 *   -# status Returns the status set on the entity, based on the enabled status
 *   -# mask Filter the status condition to be read (can be NULL)
 *   -# Returns 0 on success, or a non-zero error value if the mask does not
 *      correspond to the entity
 */
_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
DDS_EXPORT _Check_return_ dds_return_t
dds_take_status(
        _In_  dds_entity_t entity,
        _Out_ uint32_t *status,
        _In_  uint32_t mask);

/**
 * Description : Returns the status changes since they were last read.
 *
 * Arguments :
 *   -# e Entity on which the statuses are read
 *   -# Returns the curent set of triggered statuses.
 */
_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
DDS_EXPORT _Check_return_ dds_return_t
dds_get_status_changes(
        _In_  dds_entity_t entity,
        _Out_ uint32_t *status);

/**
 * Description : This operation returns the status enabled on the entity
 *
 * Arguments :
 *   -# e Entity to get the status
 *   -# Returns the status that are enabled for the entity
 */
_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
DDS_EXPORT _Check_return_ dds_return_t
dds_get_enabled_status(
        _In_  dds_entity_t entity,
        _Out_ uint32_t *status);


/**
 * Description : This operation enables the status(es) based on the mask set
 *
 * Arguments :
 *   -# e Entity to enable the status
 *   -# mask Status value that indicates the status to be enabled
 *   -# Returns 0 on success, or a non-zero error value indicating failure if the mask
 *      does not correspond to the entity.
 */
_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
DDS_EXPORT dds_return_t
dds_set_enabled_status(
        _In_ dds_entity_t entity,
        _In_ uint32_t mask);

/*
  Almost all entities have get/set qos operations defined on them,
  again following the DCPS spec. But unlike the DCPS spec, the
  "present" field in qos_t allows one to initialize just the one QoS
  one wants to set & pass it to set_qos.
*/

/**
 * @brief Get entity QoS policies.
 *
 * This operation allows access to the existing set of QoS policies
 * for the entity.
 *
 * TODO: Link to generic QoS information documentation.
 *
 * @param[in]  e    Entity on which to get qos
 * @param[out] qos  Pointer to the qos structure that returns the set policies
 *
 * @returns  0 - Success (DDS_RETCODE_OK).
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_OK
 *                  The existing set of QoS policy values applied to the
 *                  entity has successfully been copied into the specified
 *                  qos parameter.
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  The qos parameter is NULL.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
DDS_EXPORT _Check_return_ dds_return_t
dds_get_qos(
        _In_  dds_entity_t entity,
        _Out_ dds_qos_t *qos);


/**
 * @brief Set entity QoS policies.
 *
 * This operation replaces the existing set of Qos Policy settings for an
 * entity. The parameter qos must contain the struct with the QosPolicy
 * settings which is checked for self-consistency.
 *
 * The set of QosPolicy settings specified by the qos parameter are applied on
 * top of the existing QoS, replacing the values of any policies previously set
 * (provided, the operation returned DDS_RETCODE_OK).
 *
 * Not all policies are changeable when the entity is enabled.
 *
 * TODO: Link to generic QoS information documentation.
 *
 * @note Currently only Latency Budget and Ownership Strength are changeable QoS
 *       that can be set.
 *
 * @param[in]  e    Entity from which to get qos
 * @param[in]  qos  Pointer to the qos structure that provides the policies
 *
 * @returns  0 - Success (DDS_RETCODE_OK).
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_OK
 *                  The new QoS policies are set.
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  The qos parameter is NULL.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 * @retval DDS_RETCODE_IMMUTABLE_POLICY
 *                  The entity is enabled and one or more of the policies of
 *                  the QoS are immutable.
 * @retval DDS_RETCODE_INCONSISTENT_POLICY
 *                  A few policies within the QoS are not consistent with
 *                  each other.
 */
_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
DDS_EXPORT _Check_return_ dds_return_t
dds_set_qos(
        _In_ dds_entity_t entity,
        _In_ const dds_qos_t * qos);

/*
  Get or set listener associated with an entity, type of listener
  provided much match type of entity.
*/

/**
 * @brief Get entity listeners.
 *
 * This operation allows access to the existing listeners attached to
 * the entity.
 *
 * TODO: Link to (generic) Listener and status information.
 *
 * @param[in]  e        Entity on which to get the listeners
 * @param[out] listener Pointer to the listener structure that returns the
 *                      set of listener callbacks.
 *
 * @returns  0 - Success (DDS_RETCODE_OK).
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_OK
 *                  The listeners of to the entity have been successfully been
 *                  copied into the specified listener parameter.
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  The listener parameter is NULL.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
DDS_EXPORT _Check_return_ dds_return_t
dds_get_listener(
        _In_  dds_entity_t entity,
        _Out_ dds_listener_t * listener);


/**
 * @brief Set entity listeners.
 *
 * This operation attaches a dds_listener_t to the dds_entity_t. Only one
 * Listener can be attached to each Entity. If a Listener was already
 * attached, this operation will replace it with the new one. In other
 * words, all related callbacks are replaced (possibly with NULL).
 *
 * When listener parameter is NULL, all listener callbacks that were possibly
 * set on the Entity will be removed.
 *
 * @note Not all listener callbacks are related to all entities.
 *
 * TODO: Link to (generic) Listener and status information.
 *
 * <b><i>Communication Status</i></b><br>
 * For each communication status, the StatusChangedFlag flag is initially set to
 * FALSE. It becomes TRUE whenever that plain communication status changes. For
 * each plain communication status activated in the mask, the associated
 * Listener callback is invoked and the communication status is reset
 * to FALSE, as the listener implicitly accesses the status which is passed as a
 * parameter to that operation.
 * The status is reset prior to calling the listener, so if the application calls
 * the get_<status_name> from inside the listener it will see the
 * status already reset.
 *
 * <b><i>Status Propagation</i></b><br>
 * In case a related callback within the Listener is not set, the Listener of
 * the Parent entity is called recursively, until a Listener with the appropriate
 * callback set has been found and called. This allows the application to set
 * (for instance) a default behaviour in the Listener of the containing Publisher
 * and a DataWriter specific behaviour when needed. In case the callback is not
 * set in the Publishers' Listener either, the communication status will be
 * propagated to the Listener of the DomainParticipant of the containing
 * DomainParticipant. In case the callback is not set in the DomainParticipants'
 * Listener either, the Communication Status flag will be set, resulting in a
 * possible WaitSet trigger.
 *
 * @param[in]  e        Entity on which to get the listeners
 * @param[in] listener  Pointer to the listener structure that contains the
 *                      set of listener callbacks (maybe NULL).
 *
 * @returns  0 - Success (DDS_RETCODE_OK).
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_OK
 *                  The listeners of to the entity have been successfully been
 *                  copied into the specified listener parameter.
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
DDS_EXPORT _Check_return_ dds_return_t
dds_set_listener(
        _In_     dds_entity_t entity,
        _In_opt_ const dds_listener_t * listener);

/*
  Creation functions for various entities. Creating a subscriber or
  publisher is optional: if one creates a reader as a descendant of a
  participant, it is as if a subscriber is created specially for
  that reader.

  QoS default values are those of the DDS specification, but the
  inheritance rules are different:

    * publishers and subscribers inherit from the participant QoS
    * readers and writers always inherit from the topic QoS
    * the QoS's present in the "qos" parameter override the inherited values
*/

/**
 * @brief Creates a new instance of a DDS participant in a domain
 *
 * If domain is set (not DDS_DOMAIN_DEFAULT) then it must match if the domain has also
 * been configured or an error status will be returned. Currently only a single domain
 * can be configured by setting the environment variable {DDSC_PROJECT_NAME_NOSPACE_CAPS}_DOMAIN,
 * if this is not set the the default domain is 0. Valid values for domain id are between 0 and 230.
 *
 *
 * @param[in]  domain - The domain in which to create the participant (can be DDS_DOMAIN_DEFAULT)
 * @param[in]  qos - The QoS to set on the new participant (can be NULL)
 * @param[in]  listener - Any listener functions associated with the new participant (can be NULL)
 *
 * @returns >0 - Success (valid handle of a participant entity).
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 */
DDS_EXPORT _Must_inspect_result_ dds_entity_t
dds_create_participant(
        _In_     const dds_domainid_t domain,
        _In_opt_ const dds_qos_t *qos,
        _In_opt_ const dds_listener_t *listener);



/**
 * @brief Get entity parent.
 *
 * This operation returns the parent to which the given entity belongs.
 * For instance, it will return the Participant that was used when
 * creating a Publisher (when that Publisher was provided here).
 *
 * TODO: Link to generic dds entity relations documentation.
 *
 * @param[in]  entity  Entity from which to get its parent.
 *
 * @returns >0 - Success (valid entity handle).
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
DDS_EXPORT _Check_return_ dds_entity_t
dds_get_parent(
        _In_ dds_entity_t entity);


/**
 * @brief Get entity participant.
 *
 * This operation returns the participant to which the given entity belongs.
 * For instance, it will return the Participant that was used when
 * creating a Publisher that was used to create a DataWriter (when that
 * DataWriter was provided here).
 *
 * TODO: Link to generic dds entity relations documentation.
 *
 * @param[in]  entity  Entity from which to get its participant.
 *
 * @returns >0 - Success (valid entity handle).
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
DDS_EXPORT _Check_return_ dds_entity_t
dds_get_participant (
        _In_ dds_entity_t entity);


/**
 * @brief Get entity children.
 *
 * This operation returns the children that the entity contains.
 * For instance, it will return all the Topics, Publishers and Subscribers
 * of the Participant that was used to create those entities (when that
 * Participant is provided here).
 *
 * This functions takes a pre-allocated list to put the children in and
 * will return the number of found children. It is possible that the given
 * size of the list is not the same as the number of found children. If
 * less children are found, then the last few entries in the list are
 * untouched. When more children are found, then only 'size' number of
 * entries are inserted into the list, but still complete count of the
 * found children is returned. Which children are returned in the latter
 * case is undefined.
 *
 * When supplying NULL as list and 0 as size, you can use this to acquire
 * the number of children without having to pre-allocate a list.
 *
 * TODO: Link to generic dds entity relations documentation.
 *
 * @param[in]  entity   Entity from which to get its children.
 * @param[out] children Pre-allocated array to contain the found children.
 * @param[in]  size     Size of the pre-allocated children's list.
 *
 * @returns >=0 - Success (number of found children, can be larger than 'size').
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  The children parameter is NULL, while a size is provided.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
DDS_EXPORT _Check_return_ dds_return_t
dds_get_children(
        _In_        dds_entity_t entity,
        _Out_opt_   dds_entity_t *children,
        _In_        size_t size);


/**
 * @brief Get the domain id to which this entity is attached.
 *
 * When creating a participant entity, it is attached to a certain domain.
 * All the children (like Publishers) and childrens' children (like
 * DataReaders), etc are also attached to that domain.
 *
 * This function will return the original domain ID when called on
 * any of the entities within that hierarchy.
 *
 * @param[in]  entity   Entity from which to get its children.
 * @param[out] id       Pointer to put the domain ID in.
 *
 * @returns  0 - Success (DDS_RETCODE_OK).
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_OK
 *                  Domain ID was returned.
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  The id parameter is NULL.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
DDS_EXPORT _Check_return_ dds_return_t
dds_get_domainid(
        _In_  dds_entity_t entity,
        _Out_ dds_domainid_t *id);

/**
 * @brief Get participants of a domain.
 *
 * This operation acquires the participants created on a domain and returns
 * the number of found participants.
 *
 * This function takes a domain id with the size of pre-allocated participant's
 * list in and will return the number of found participants. It is possible that
 * the given size of the list is not the same as the number of found participants.
 * If less participants are found, then the last few entries in an array stay
 * untouched. If more participants are found and the array is too small, then the
 * participants returned are undefined.
 *
 * @param[in]  domain_id    The domain id
 * @param[out] participants The participant for domain
 * @param[in]  size         Size of the pre-allocated participant's list.
 *
 * @returns >=0 - Success (number of found participants).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  The participant parameter is NULL, while a size is provided.
 */
DDS_EXPORT _Check_return_ dds_return_t
dds_lookup_participant(
        _In_        dds_domainid_t domain_id,
        _Out_opt_   dds_entity_t *participants,
        _In_        size_t size);

/**
 * Description : Creates a new DDS topic. The type name for the topic
 * is taken from the generated descriptor. Topic matching is done on a
 * combination of topic name and type name.
 *
 * Arguments :
 *   -# pp The participant on which the topic is being created
 *   -# descriptor The IDL generated topic descriptor
 *   -# name The name of the created topic
 *   -# qos The QoS to set on the new topic (can be NULL)
 *   -# listener Any listener functions associated with the new topic (can be NULL)
 *   -# Returns a status, 0 on success or non-zero value to indicate an error
 */
_Pre_satisfies_((participant & DDS_ENTITY_KIND_MASK) == DDS_KIND_PARTICIPANT)
DDS_EXPORT dds_entity_t
dds_create_topic(
        _In_ dds_entity_t participant,
        _In_ const dds_topic_descriptor_t *descriptor,
        _In_z_ const char *name,
        _In_opt_ const dds_qos_t *qos,
        _In_opt_ const dds_listener_t *listener);

/**
 * Description : Finds a named topic. Returns NULL if does not exist.
 * The returned topic should be released with dds_delete.
 *
 * Arguments :
 *   -# pp The participant on which to find the topic
 *   -# name The name of the topic to find
 *   -# Returns a topic, NULL if could not be found or error
 */
_Pre_satisfies_((participant & DDS_ENTITY_KIND_MASK) == DDS_KIND_PARTICIPANT)
DDS_EXPORT dds_entity_t
dds_find_topic(
        _In_ dds_entity_t participant,
        _In_z_ const char *name);

/**
 * Description : Returns a topic name.
 *
 * Arguments :
 *   -# topic The topic
 *   -# Returns The topic name or NULL to indicate an error
 */
/* TODO: do we need a convenience version as well that allocates and add a _s suffix to this one? */
/* TODO: Check annotation. Could be _Out_writes_to_(size, return + 1) as well. */
_Pre_satisfies_((topic & DDS_ENTITY_KIND_MASK) == DDS_KIND_TOPIC)
DDS_EXPORT dds_return_t
dds_get_name(
        _In_                 dds_entity_t topic,
        _Out_writes_z_(size) char *name,
        _In_                 size_t size);


/**
 * Description : Returns a topic type name.
 *
 * Arguments :
 *   -# topic The topic
 *   -# Returns The topic type name or NULL to indicate an error
 */
_Pre_satisfies_((topic & DDS_ENTITY_KIND_MASK) == DDS_KIND_TOPIC)
DDS_EXPORT dds_return_t
dds_get_type_name(
        _In_ dds_entity_t topic,
        _Out_writes_z_(size) char *name,
        _In_ size_t size);

/**
 * @brief Creates a new instance of a DDS subscriber
 *
 * @param[in]  participant The participant on which the subscriber is being created
 * @param[in]  qos         The QoS to set on the new subscriber (can be NULL)
 * @param[in]  listener    Any listener functions associated with the new subscriber (can be NULL)

 * @returns >0 - Success (valid handle of a subscriber entity).
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 *         DDS_RETCODE_BAD_PARAMETER
 *                  One of the parameters is invalid
 */
_Pre_satisfies_((participant & DDS_ENTITY_KIND_MASK) == DDS_KIND_PARTICIPANT)
DDS_EXPORT _Must_inspect_result_ dds_entity_t
dds_create_subscriber(
        _In_     dds_entity_t participant,
        _In_opt_ const dds_qos_t *qos,
        _In_opt_ const dds_listener_t *listener);


/**
 * @brief Creates a new instance of a DDS publisher
 *
 * @param[in]  participant The participant to create a publisher for
 * @param[in]  qos         The QoS to set on the new publisher (can be NULL)
 * @param[in]  listener    Any listener functions associated with the new publisher (can be NULL)
 *
 * @returns >0 - Success (valid handle of a publisher entity).
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 */
_Pre_satisfies_((participant & DDS_ENTITY_KIND_MASK) == DDS_KIND_PARTICIPANT)
DDS_EXPORT _Must_inspect_result_ dds_entity_t
dds_create_publisher(
        _In_     dds_entity_t participant,
        _In_opt_ const dds_qos_t *qos,
        _In_opt_ const dds_listener_t *listener);


/**
 * @brief Suspends the publications of the Publisher
 *
 * This operation is a hint to the Service so it can optimize its performance by e.g., collecting
 * modifications to DDS writers and then batching them. The Service is not required to use the hint.
 *
 * Every invocation of this operation must be matched by a corresponding call to @see dds_resume
 * indicating that the set of modifications has completed.
 *
 * @param[in]  publisher The publisher for which all publications will be suspended
 *
 * @returns >0 - Success.
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_OK
 *                Publications suspended successfully.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                The pub parameter is not a valid publisher.
 * @retval DDS_RETCODE_UNSUPPORTED
 *                Operation is not supported
 */
_Pre_satisfies_((publisher & DDS_ENTITY_KIND_MASK) == DDS_KIND_PUBLISHER)
DDS_EXPORT dds_return_t
dds_suspend(
        _In_ dds_entity_t publisher);


/**
 * @brief Resumes the publications of the Publisher
 *
 * This operation is a hint to the Service to indicate that the application has completed changes
 * initiated by a previous @see suspend. The Service is not required to use the hint.
 *
 * The call to resume_publications must match a previous call to @see suspend_publications.
 *
 * @param[in]  publisher The publisher for which all publications will be resumed
 *
 * @returns >0 - Success.
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_OK
 *                Publications resumed successfully.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                The pub parameter is not a valid publisher.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *                No previous matching @see dds_suspend.
 * @retval DDS_RETCODE_UNSUPPORTED
 *                Operation is not supported.
 */
_Pre_satisfies_((publisher & DDS_ENTITY_KIND_MASK) == DDS_KIND_PUBLISHER)
DDS_EXPORT dds_return_t
dds_resume(
        _In_ dds_entity_t publisher);


/**
 * @brief Waits at most for the duration timeout for acks for data in the publisher or writer.
 *
 * This operation blocks the calling thread until either all data written by the publisher
 * or writer is acknowledged by all matched reliable reader entities, or else the duration
 * specified by the timeout parameter elapses, whichever happens first.
 *
 * @param[in]  pub_or_w   The publisher or writer whose acknowledgements must be waited for.
 *
 * @returns >0 - Success.
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_OK
 *                All acknowledgements successfully received with the timeout.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                The pub_or_w parameter is not a valid publisher or writer.
 * @retval DDS_RETCODE_TIMEOUT
 *                Timeout expired before all acknowledgements from reliable reader entities were received.
 * @retval DDS_RETCODE_UNSUPPORTED
 *                Operation is not supported.
 */
_Pre_satisfies_(((publisher_or_writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER   ) ||\
                ((publisher_or_writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_PUBLISHER) )
DDS_EXPORT dds_return_t
dds_wait_for_acks(
        _In_ dds_entity_t publisher_or_writer,
        _In_ dds_duration_t timeout);


/**
 * @brief Creates a new instance of a DDS reader
 *
 * @param[in]  participant_or_subscriber The participant or subscriber on which the reader is being created
 *
 * @param[in]  topic The topic to read
 *
 * @param[in]  qos The QoS to set on the new reader (can be NULL)
 *
 * @param[in]  listener Any listener functions associated with the new reader (can be NULL)
 *
 * @returns >0 - Success (valid handle of a reader entity)
 * @returns <0 - Failure (use dds_err_nr() to get error value)
 *
 */
_Pre_satisfies_(((participant_or_subscriber & DDS_ENTITY_KIND_MASK) == DDS_KIND_SUBSCRIBER ) ||\
                ((participant_or_subscriber & DDS_ENTITY_KIND_MASK) == DDS_KIND_PARTICIPANT) )
_Pre_satisfies_( (topic & DDS_ENTITY_KIND_MASK) == DDS_KIND_TOPIC )
DDS_EXPORT dds_entity_t
dds_create_reader(
        _In_ dds_entity_t participant_or_subscriber,
        _In_ dds_entity_t topic,
        _In_opt_ const dds_qos_t *qos,
        _In_opt_ const dds_listener_t *listener);

/**
 * Description : The operation blocks the calling thread until either all "historical" data is
 * received, or else the duration specified by the max_wait parameter elapses, whichever happens
 * first. A return value of 0 indicates that all the "historical" data was received; a return
 * value of TIMEOUT indicates that max_wait elapsed before all the data was received.
 *
 * Arguments :
 *   -# reader The reader on which to wait for historical data
 *   -# max_wait How long to wait for historical data before time out
 *   -# Returns a status, 0 on success, TIMEOUT on timeout or a  negative value to indicate error
 */
/* TODO: SAL-annotate TIMEOUT as a succesfull return as well? */
_Pre_satisfies_((reader & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER)
DDS_EXPORT dds_return_t
dds_wait_for_historical_data(
        _In_ dds_entity_t reader,
        _In_ dds_duration_t max_wait);

/**
 * @brief Creates a new instance of a DDS writer
 *
 * @param[in]  participant_or_publisher The participant or publisher on which the writer is being created
 * @param[in]  topic The topic to write
 * @param[in]  qos The QoS to set on the new writer (can be NULL)
 * @param[in]  listener Any listener functions associated with the new writer (can be NULL)
 *
 * @returns >0 - Success (valid handle of a writer entity)
 * @returns <0 - Failure (use dds_err_nr() to get error value)
 */
_Pre_satisfies_(((participant_or_publisher & DDS_ENTITY_KIND_MASK) == DDS_KIND_PUBLISHER  ) ||\
                ((participant_or_publisher & DDS_ENTITY_KIND_MASK) == DDS_KIND_PARTICIPANT) )
_Pre_satisfies_( (topic & DDS_ENTITY_KIND_MASK) == DDS_KIND_TOPIC )
DDS_EXPORT dds_entity_t
dds_create_writer(
        _In_     dds_entity_t participant_or_publisher,
        _In_     dds_entity_t topic,
        _In_opt_ const dds_qos_t *qos,
        _In_opt_ const dds_listener_t *listener);


/*
  Writing data (and variants of it) is straightforward. The first set
  is equivalent to the second set with -1 passed for "timestamp",
  meaning, substitute the result of a call to time(). The dispose
  and unregister operations take an object of the topic's type, but
  only touch the key fields; the remained may be undefined.
*/
/**
 * Description : Registers an instance with a key value to the data writer
 *
 * Arguments :
 *   -# wr The writer to which instance has be associated
 *   -# data Instance with the key value
 *   -# Returns an instance handle that could be used for successive write & dispose operations or
 *      NULL, if handle is not allocated
 */
_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
DDS_EXPORT dds_return_t
dds_register_instance (
        _In_ dds_entity_t writer,
        _Out_ dds_instance_handle_t *handle,
        _In_ const void *data);

/**
 * Description : Unregisters an instance with a key value from the data writer. Instance can be identified
 *               either from data sample or from instance handle (at least one must be provided).
 *
 * Arguments :
 *   -# wr The writer to which instance is associated
 *   -# data Instance with the key value (can be NULL if handle set)
 *   -# handle Instance handle (can be DDS_HANDLE_NIL if data set)
 *   -# Returns 0 on success, or non-zero value to indicate an error
 *
 * Note : If an unregistered key ID is passed as instance data, an error is logged and not flagged as return value
 */
_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
DDS_EXPORT dds_return_t
dds_unregister_instance(
        _In_ dds_entity_t writer,
        _In_ const void *data);

_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
DDS_EXPORT dds_return_t
dds_unregister_instance_ih(
       _In_ dds_entity_t writer,
       _In_ dds_instance_handle_t handle);

  /**
 * Description : Unregisters an instance with a key value from the data writer. Instance can be identified
 *               either from data sample or from instance handle (at least one must be provided).
 *
 * Arguments :
 *   -# wr The writer to which instance is associated
 *   -# data Instance with the key value (can be NULL if handle set)
 *   -# handle Instance handle (can be DDS_HANDLE_NIL if data set)
 *   -# timestamp used at registration.
 *   -# Returns 0 on success, or non-zero value to indicate an error
 *
 * Note : If an unregistered key ID is passed as instance data, an error is logged and not flagged as return value
 */
_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
DDS_EXPORT dds_return_t
dds_unregister_instance_ts(
       _In_ dds_entity_t writer,
       _In_ const void *data,
       _In_ dds_time_t timestamp);

_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
DDS_EXPORT dds_return_t
dds_unregister_instance_ih_ts(
       _In_ dds_entity_t writer,
       _In_ dds_instance_handle_t handle,
       _In_ dds_time_t timestamp);

/**
 * @brief This operation modifies and disposes a data instance.
 *
 * This operation requests the Data Distribution Service to modify the instance and
 * mark it for deletion. Copies of the instance and its corresponding samples, which are
 * stored in every connected reader and, dependent on the QoS policy settings (also in
 * the Transient and Persistent stores) will be modified and marked for deletion by
 * setting their dds_instance_state_t to DDS_IST_NOT_ALIVE_DISPOSED.
 *
 * <b><i>Blocking</i></b><br>
 * If the history QoS policy is set to DDS_HISTORY_KEEP_ALL, the
 * dds_writedispose operation on the writer may block if the modification
 * would cause data to be lost because one of the limits, specified in the
 * resource_limits QoS policy, to be exceeded. In case the synchronous
 * attribute value of the reliability Qos policy is set to true for
 * communicating writers and readers then the writer will wait until
 * all synchronous readers have acknowledged the data. Under these
 * circumstances, the max_blocking_time attribute of the reliability
 * QoS policy configures the maximum time the dds_writedispose operation
 * may block.
 * If max_blocking_time elapses before the writer is able to store the
 * modification without exceeding the limits and all expected acknowledgements
 * are received, the dds_writedispose operation will fail and returns
 * DDS_RETCODE_TIMEOUT.
 *
 * @param[in]  writer The writer to dispose the data instance from.
 * @param[in]  data   The data to be written and disposed.
 *
 * @returns  0 - Success.
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_OK
 *                The sample is written and the instance is marked for deletion.
 * @retval DDS_RETCODE_ERROR
 *                An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                At least one of the arguments is invalid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                The entity has already been deleted.
 * @retval DDS_RETCODE_TIMEOUT
 *                Either the current action overflowed the available resources
 *                as specified by the combination of the reliability QoS policy,
 *                history QoS policy and resource_limits QoS policy, or the
 *                current action was waiting for data delivery acknowledgement
 *                by synchronous readers. This caused blocking of this operation,
 *                which could not be resolved before max_blocking_time of the
 *                reliability QoS policy elapsed.
 */
_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
DDS_EXPORT dds_return_t
dds_writedispose(
       _In_ dds_entity_t writer,
       _In_ const void *data);

/**
 * Description : This operation modifies and disposes a data instance with
 *               a specific timestamp.
 *
 * This operation performs the same functions as dds_writedispose except that
 * the application provides the value for the source_timestamp that is made
 * available to connected reader objects. This timestamp is important for the
 * interpretation of the destination_order QoS policy.
 *
 * @param[in]  writer    The writer to dispose the data instance from.
 * @param[in]  data      The data to be written and disposed.
 * @param[in]  timestamp The timestamp used as source timestamp.
 *
 * @returns  0 - Success.
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_OK
 *                The sample is written and the instance is marked for deletion.
 * @retval DDS_RETCODE_ERROR
 *                An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                At least one of the arguments is invalid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                The entity has already been deleted.
 * @retval DDS_RETCODE_TIMEOUT
 *                Either the current action overflowed the available resources
 *                as specified by the combination of the reliability QoS policy,
 *                history QoS policy and resource_limits QoS policy, or the
 *                current action was waiting for data delivery acknowledgement
 *                by synchronous readers. This caused blocking of this operation,
 *                which could not be resolved before max_blocking_time of the
 *                reliability QoS policy elapsed.
 */
_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
DDS_EXPORT dds_return_t
dds_writedispose_ts(
       _In_ dds_entity_t writer,
       _In_ const void *data,
       _In_ dds_time_t timestamp);

/**
 * @brief This operation disposes an instance, identified by the data sample.
 *
 * This operation requests the Data Distribution Service to modify the instance and
 * mark it for deletion. Copies of the instance and its corresponding samples, which are
 * stored in every connected reader and, dependent on the QoS policy settings (also in
 * the Transient and Persistent stores) will be modified and marked for deletion by
 * setting their dds_instance_state_t to DDS_IST_NOT_ALIVE_DISPOSED.
 *
 * <b><i>Blocking</i></b><br>
 * If the history QoS policy is set to DDS_HISTORY_KEEP_ALL, the
 * dds_writedispose operation on the writer may block if the modification
 * would cause data to be lost because one of the limits, specified in the
 * resource_limits QoS policy, to be exceeded. In case the synchronous
 * attribute value of the reliability Qos policy is set to true for
 * communicating writers and readers then the writer will wait until
 * all synchronous readers have acknowledged the data. Under these
 * circumstances, the max_blocking_time attribute of the reliability
 * QoS policy configures the maximum time the dds_writedispose operation
 * may block.
 * If max_blocking_time elapses before the writer is able to store the
 * modification without exceeding the limits and all expected acknowledgements
 * are received, the dds_writedispose operation will fail and returns
 * DDS_RETCODE_TIMEOUT.
 *
 * @param[in]  writer The writer to dispose the data instance from.
 * @param[in]  data   The data sample that identifies the instance
 *                    to be disposed.
 *
 * @returns  0 - Success.
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_OK
 *                The sample is written and the instance is marked for deletion.
 * @retval DDS_RETCODE_ERROR
 *                An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                At least one of the arguments is invalid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                The entity has already been deleted.
 * @retval DDS_RETCODE_TIMEOUT
 *                Either the current action overflowed the available resources
 *                as specified by the combination of the reliability QoS policy,
 *                history QoS policy and resource_limits QoS policy, or the
 *                current action was waiting for data delivery acknowledgement
 *                by synchronous readers. This caused blocking of this operation,
 *                which could not be resolved before max_blocking_time of the
 *                reliability QoS policy elapsed.
 */
_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
DDS_EXPORT dds_return_t
dds_dispose(
       _In_ dds_entity_t writer,
       _In_ const void *data);

/**
 * Description : This operation disposes an instance with a specific timestamp,
 *               identified by the data sample.
 *
 * This operation performs the same functions as dds_dispose except that
 * the application provides the value for the source_timestamp that is made
 * available to connected reader objects. This timestamp is important for the
 * interpretation of the destination_order QoS policy.
 *
 * @param[in]  writer    The writer to dispose the data instance from.
 * @param[in]  data      The data sample that identifies the instance
 *                       to be disposed.
 * @param[in]  timestamp The timestamp used as source timestamp.
 *
 * @returns  0 - Success.
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_OK
 *                The sample is written and the instance is marked for deletion.
 * @retval DDS_RETCODE_ERROR
 *                An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                At least one of the arguments is invalid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                The entity has already been deleted.
 * @retval DDS_RETCODE_TIMEOUT
 *                Either the current action overflowed the available resources
 *                as specified by the combination of the reliability QoS policy,
 *                history QoS policy and resource_limits QoS policy, or the
 *                current action was waiting for data delivery acknowledgement
 *                by synchronous readers. This caused blocking of this operation,
 *                which could not be resolved before max_blocking_time of the
 *                reliability QoS policy elapsed.
 */
_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
DDS_EXPORT dds_return_t
dds_dispose_ts(
       _In_ dds_entity_t writer,
       _In_ const void *data,
       _In_ dds_time_t timestamp);

/**
 * @brief This operation disposes an instance, identified by the instance handle.
 *
 * This operation requests the Data Distribution Service to modify the instance and
 * mark it for deletion. Copies of the instance and its corresponding samples, which are
 * stored in every connected reader and, dependent on the QoS policy settings (also in
 * the Transient and Persistent stores) will be modified and marked for deletion by
 * setting their dds_instance_state_t to DDS_IST_NOT_ALIVE_DISPOSED.
 *
 * <b><i>Instance Handle</i></b><br>
 * The given instance handle must correspond to the value that was returned by either
 * the dds_register_instance operation, dds_register_instance_ts or dds_instance_lookup.
 * If there is no correspondence, then the result of the operation is unspecified.
 *
 * @param[in]  writer The writer to dispose the data instance from.
 * @param[in]  handle The handle to identify an instance.
 *
 * @returns  0 - Success.
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_OK
 *                The sample is written and the instance is marked for deletion.
 * @retval DDS_RETCODE_ERROR
 *                An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                At least one of the arguments is invalid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *                The instance handle has not been registered with this writer.
 */
_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
DDS_EXPORT dds_return_t
dds_dispose_ih(
       _In_ dds_entity_t writer,
       _In_ dds_instance_handle_t handle);

/**
 * Description : This operation disposes an instance with a specific timestamp,
 *               identified by the instance handle.
 *
 * This operation performs the same functions as dds_dispose_ih except that
 * the application provides the value for the source_timestamp that is made
 * available to connected reader objects. This timestamp is important for the
 * interpretation of the destination_order QoS policy.
 *
 * @param[in]  writer    The writer to dispose the data instance from.
 * @param[in]  handle    The handle to identify an instance.
 * @param[in]  timestamp The timestamp used as source timestamp.
 *
 * @returns  0 - Success.
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_OK
 *                The sample is written and the instance is marked for deletion.
 * @retval DDS_RETCODE_ERROR
 *                An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                At least one of the arguments is invalid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *                The instance handle has not been registered with this writer.
 */
_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
DDS_EXPORT dds_return_t
dds_dispose_ih_ts(
       _In_ dds_entity_t writer,
       _In_ dds_instance_handle_t handle,
       _In_ dds_time_t timestamp);

/**
 * @brief Write the value of a data instance
 *
 * With this API, the value of the source timestamp is automatically made
 * available to the data reader by the service.
 *
 * @param[in]  writer The writer entity
 * @param[in]  data Value to be written
 *
 * @returns - dds_return_t indicating success or failure
 */
_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
DDS_EXPORT dds_return_t
dds_write(
       _In_ dds_entity_t writer,
       _In_ const void *data);


/**
 * @brief Write a CDR serialized value of a data instance
 *
 * Untyped API, which take serialized blobs now.
 * Whether they remain exposed like this with X-types isn't entirely clear yet.
 * TODO: make a decide about dds_takecdr
 *
 * @param[in]  writer The writer entity
 * @param[in]  cdr CDR serialized value to be written
 * @param[in]  size Size (in bytes) of CDR encoded data to be written
 *
 * @returns - A dds_return_t indicating success or failure
 */
_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
DDS_EXPORT dds_return_t
dds_writecdr(
       _In_ dds_entity_t writer,
       _In_reads_bytes_(size) const void *cdr,
       _In_ size_t size);

/**
 * @brief Write the value of a data instance along with the source timestamp passed.
 *
 * @param[in]  writer The writer entity
 * @param[in]  data Value to be written
 * @param[in]  timestamp Source timestamp
 *
 * @returns - A dds_return_t indicating success or failure
 */
_Pre_satisfies_((writer & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER)
DDS_EXPORT dds_return_t
dds_write_ts(
       _In_ dds_entity_t writer,
       _In_ const void *data,
       _In_ dds_time_t timestamp);

/**
 * @brief Creates a readcondition associated to the given reader.
 *
 * The readcondition allows specifying which samples are of interest in
 * a data reader's history, by means of a mask. The mask is or'd with
 * the flags that are dds_sample_state_t, dds_view_state_t and
 * dds_instance_state_t.
 *
 * Based on the mask value set, the readcondition gets triggered when
 * data is available on the reader.
 *
 * Waitsets allow waiting for an event on some of any set of entities.
 * This means that the readcondition can be used to wake up a waitset when
 * data is in the reader history with states that matches the given mask.
 *
 * @note The parent reader and every of its associated conditions (whether
 *       they are readconditions or queryconditions) share the same resources.
 *       This means that one of these entities reads or takes data, the states
 *       of the data will change for other entities automatically. For instance,
 *       if one reads a sample, then the sample state will become 'read' for all
 *       associated reader/conditions. Or if one takes a sample, then it's not
 *       available to any other associated reader/condition.
 *
 * @param[in]  reader  Reader to associate the condition to.
 * @param[in]  mask    Interest (dds_sample_state_t|dds_view_state_t|dds_instance_state_t).
 *
 * @returns >0 - Success (valid condition).
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
_Pre_satisfies_((reader & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER)
DDS_EXPORT _Must_inspect_result_ dds_entity_t
dds_create_readcondition(
        _In_ dds_entity_t reader,
        _In_ uint32_t mask);

/**
 * @brief Creates a queryondition associated to the given reader.
 *
 * The queryondition allows specifying which samples are of interest in
 * a data reader's history, by means of a mask and a filter. The mask is
 * or'd with the flags that are dds_sample_state_t, dds_view_state_t and
 * dds_instance_state_t.
 *
 * TODO: Explain the filter (aka expression & parameters) of the (to be
 *       implemented) new querycondition implementation.
 *
 * Based on the mask value set and data that matches the filter, the
 * querycondition gets triggered when data is available on the reader.
 *
 * Waitsets allow waiting for an event on some of any set of entities.
 * This means that the querycondition can be used to wake up a waitset when
 * data is in the reader history with states that matches the given mask
 * and filter.
 *
 * @note The parent reader and every of its associated conditions (whether
 *       they are readconditions or queryconditions) share the same resources.
 *       This means that one of these entities reads or takes data, the states
 *       of the data will change for other entities automatically. For instance,
 *       if one reads a sample, then the sample state will become 'read' for all
 *       associated reader/conditions. Or if one takes a sample, then it's not
 *       available to any other associated reader/condition.
 *
 * TODO: Update parameters when new querycondition is introduced.
 *
 * @param[in]  reader  Reader to associate the condition to.
 * @param[in]  mask    Interest (dds_sample_state_t|dds_view_state_t|dds_instance_state_t).
 * @param[in]  filter  Callback that the application can use to filter specific samples.
 *
 * @returns >0 - Success (valid condition).
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
typedef bool (*dds_querycondition_filter_fn) (const void * sample);
_Pre_satisfies_((reader & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER)
DDS_EXPORT dds_entity_t
dds_create_querycondition(
        _In_ dds_entity_t reader,
        _In_ uint32_t mask,
        _In_ dds_querycondition_filter_fn filter);


/**
 * @brief Waitset attachment argument.
 *
 * Every entity that is attached to the waitset can be accompanied by such
 * an attachment argument. When the waitset wait is unblocked because of an
 * entity that triggered, then the returning array will be populated with
 * these attachment arguments that are related to the triggered entity.
*/
typedef void * dds_attach_t;

/**
 * @brief Create a waitset and allocate the resources required
 *
 * A WaitSet object allows an application to wait until one or more of the
 * conditions of the attached entities evaluates to TRUE or until the timeout
 * expires.
 *
 * @param[in]  participant  Domain participant which the WaitSet contains.
 *
 * @returns >0 - Success (valid waitset).
 * @returns <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 */
_Pre_satisfies_((participant & DDS_ENTITY_KIND_MASK) == DDS_KIND_PARTICIPANT)
DDS_EXPORT _Must_inspect_result_ dds_entity_t
dds_create_waitset(
        _In_ dds_entity_t participant);


/**
 * @brief Acquire previously attached entities.
 *
 * This functions takes a pre-allocated list to put the entities in and
 * will return the number of found entities. It is possible that the given
 * size of the list is not the same as the number of found entities. If
 * less entities are found, then the last few entries in the list are
 * untouched. When more entities are found, then only 'size' number of
 * entries are inserted into the list, but still the complete count of the
 * found entities is returned. Which entities are returned in the latter
 * case is undefined.
 *
 * @param[in]  waitset  Waitset from which to get its attached entities.
 * @param[out] entities Pre-allocated array to contain the found entities.
 * @param[in]  size     Size of the pre-allocated entities' list.
 *
 * @returns >=0 - Success (number of found children, can be larger than 'size').
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  The entities parameter is NULL, while a size is provided.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The waitset has already been deleted.
 */
_Pre_satisfies_((waitset & DDS_ENTITY_KIND_MASK) == DDS_KIND_WAITSET)
DDS_EXPORT dds_return_t
dds_waitset_get_entities(
        _In_ dds_entity_t waitset,
        _Out_writes_to_(size, return < 0 ? 0 : return) dds_entity_t *entities,
        _In_ size_t size);


/**
 * @brief This operation attaches an Entity to the WaitSet.
 *
 * This operation attaches an Entity to the WaitSet. The dds_waitset_wait()
 * will block when none of the attached entities are triggered. 'Triggered'
 * (dds_triggered()) doesn't mean the same for every entity:
 *  - Reader/Writer/Publisher/Subscriber/Topic/Participant
 *      - These are triggered when their status changed.
 *  - WaitSet
 *      - Triggered when trigger value was set to true by the application.
 *        It stays triggered until application sets the trigger value to
 *        false (dds_waitset_set_trigger()). This can be used to wake up an
 *        waitset for different reasons (f.i. termination) than the 'normal'
 *        status change (like new data).
 *  - ReadCondition/QueryCondition
 *      - Triggered when data is available on the related Reader that matches
 *        the Condition.
 *
 * Multiple entities can be attached to a single waitset. A particular entity
 * can be attached to multiple waitsets. However, a particular entity can not
 * be attached to a particular waitset multiple times.
 *
 * @param[in]  waitset  The waitset to attach the given entity to.
 * @param[in]  entity   The entity to attach.
 * @param[in]  x        Blob that will be supplied when the waitset wait is
 *                      triggerd by the given entity.
 *
 * @returns   0 - Success (entity attached).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  The given waitset or entity are not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The waitset has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *                  The entity was already attached.
 */
_Pre_satisfies_((waitset & DDS_ENTITY_KIND_MASK) == DDS_KIND_WAITSET)
DDS_EXPORT dds_return_t
dds_waitset_attach(
        _In_ dds_entity_t waitset,
        _In_ dds_entity_t entity,
        _In_ dds_attach_t x);


/**
 * @brief This operation detaches an Entity to the WaitSet.
 *
 * @param[in]  waitset  The waitset to detach the given entity from.
 * @param[in]  entity   The entity to detach.
 *
 * @returns   0 - Success (entity attached).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  The given waitset or entity are not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The waitset has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *                  The entity is not attached.
 */
_Pre_satisfies_((waitset & DDS_ENTITY_KIND_MASK) == DDS_KIND_WAITSET)
DDS_EXPORT dds_return_t
dds_waitset_detach(
        _In_ dds_entity_t waitset,
        _In_ dds_entity_t entity);

/**
 * @brief Sets the trigger_value associated with a waitset.
 *
 * When the waitset is attached to itself and the trigger value is
 * set to 'true', then the waitset will wake up just like with an
 * other status change of the attached entities.
 *
 * This can be used to forcefully wake up a waitset, for instance
 * when the application wants to shut down. So, when the trigger
 * value is true, the waitset will wake up or not wait at all.
 *
 * The trigger value will remain true until the application sets it
 * false again deliberately.
 *
 * @param[in]  waitset  The waitset to set the trigger value on.
 * @param[in]  trigger  The trigger value to set.
 *
 * @returns   0 - Success (entity attached).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  The given waitset is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The waitset has already been deleted.
 */
_Pre_satisfies_((waitset & DDS_ENTITY_KIND_MASK) == DDS_KIND_WAITSET)
DDS_EXPORT dds_return_t
dds_waitset_set_trigger(
        _In_ dds_entity_t waitset,
        _In_ bool trigger);

/**
 * @brief This operation allows an application thread to wait for the a status
 *        change or other trigger on (one of) the entities that are attached to
 *        the WaitSet.
 *
 * The "dds_waitset_wait" operation blocks until the some of the attached
 * entities have triggered or "reltimeout" has elapsed.
 * 'Triggered' (dds_triggered()) doesn't mean the same for every entity:
 *  - Reader/Writer/Publisher/Subscriber/Topic/Participant
 *      - These are triggered when their status changed.
 *  - WaitSet
 *      - Triggered when trigger value was set to true by the application.
 *        It stays triggered until application sets the trigger value to
 *        false (dds_waitset_set_trigger()). This can be used to wake up an
 *        waitset for different reasons (f.i. termination) than the 'normal'
 *        status change (like new data).
 *  - ReadCondition/QueryCondition
 *      - Triggered when data is available on the related Reader that matches
 *        the Condition.
 *
 * This functions takes a pre-allocated list to put the "xs" blobs in (that
 * were provided during the attach of the related entities) and will return
 * the number of triggered entities. It is possible that the given size
 * of the list is not the same as the number of triggered entities. If less
 * entities were triggered, then the last few entries in the list are
 * untouched. When more entities are triggered, then only 'size' number of
 * entries are inserted into the list, but still the complete count of the
 * triggered entities is returned. Which "xs" blobs are returned in the
 * latter case is undefined.
 *
 * In case of a time out, the return value is 0.
 *
 * Deleting the waitset while the application is blocked results in an
 * error code (i.e. < 0) returned by "wait".
 *
 * Multiple threads may block on a single waitset at the same time;
 * the calls are entirely independent.
 *
 * An empty waitset never triggers (i.e., dds_waitset_wait on an empty
 * waitset is essentially equivalent to a sleep).
 *
 * The "dds_waitset_wait_until" operation is the same as the
 * "dds_waitset_wait" except that it takes an absolute timeout.
 *
 * @param[in]  waitset    The waitset to set the trigger value on.
 * @param[out] xs         Pre-allocated list to store the 'blobs' that were
 *                        provided during the attach of the triggered entities.
 * @param[in]  nxs        The size of the pre-allocated blobs list.
 * @param[in]  reltimeout Relative timeout
 *
 * @returns  >0 - Success (number of entities triggered).
 * @returns   0 - Time out (no entities were triggered).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  The given waitset is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The waitset has already been deleted.
 */
_Pre_satisfies_((waitset & DDS_ENTITY_KIND_MASK) == DDS_KIND_WAITSET)
DDS_EXPORT dds_return_t
dds_waitset_wait(
        _In_ dds_entity_t waitset,
        _Out_writes_to_(nxs, return < 0 ? 0 : return) dds_attach_t *xs,
        _In_ size_t nxs,
        _In_ dds_duration_t reltimeout);

/**
 * @brief This operation allows an application thread to wait for the a status
 *        change or other trigger on (one of) the entities that are attached to
 *        the WaitSet.
 *
 * The "dds_waitset_wait" operation blocks until the some of the attached
 * entities have triggered or "abstimeout" has been reached.
 * 'Triggered' (dds_triggered()) doesn't mean the same for every entity:
 *  - Reader/Writer/Publisher/Subscriber/Topic/Participant
 *      - These are triggered when their status changed.
 *  - WaitSet
 *      - Triggered when trigger value was set to true by the application.
 *        It stays triggered until application sets the trigger value to
 *        false (dds_waitset_set_trigger()). This can be used to wake up an
 *        waitset for different reasons (f.i. termination) than the 'normal'
 *        status change (like new data).
 *  - ReadCondition/QueryCondition
 *      - Triggered when data is available on the related Reader that matches
 *        the Condition.
 *
 * This functions takes a pre-allocated list to put the "xs" blobs in (that
 * were provided during the attach of the related entities) and will return
 * the number of triggered entities. It is possible that the given size
 * of the list is not the same as the number of triggered entities. If less
 * entities were triggered, then the last few entries in the list are
 * untouched. When more entities are triggered, then only 'size' number of
 * entries are inserted into the list, but still the complete count of the
 * triggered entities is returned. Which "xs" blobs are returned in the
 * latter case is undefined.
 *
 * In case of a time out, the return value is 0.
 *
 * Deleting the waitset while the application is blocked results in an
 * error code (i.e. < 0) returned by "wait".
 *
 * Multiple threads may block on a single waitset at the same time;
 * the calls are entirely independent.
 *
 * An empty waitset never triggers (i.e., dds_waitset_wait on an empty
 * waitset is essentially equivalent to a sleep).
 *
 * The "dds_waitset_wait" operation is the same as the
 * "dds_waitset_wait_until" except that it takes an relative timeout.
 *
 * The "dds_waitset_wait" operation is the same as the "dds_wait"
 * except that it takes an absolute timeout.
 *
 * @param[in]  waitset    The waitset to set the trigger value on.
 * @param[out] xs         Pre-allocated list to store the 'blobs' that were
 *                        provided during the attach of the triggered entities.
 * @param[in]  nxs        The size of the pre-allocated blobs list.
 * @param[in]  abstimeout Absolute timeout
 *
 * @returns  >0 - Success (number of entities triggered).
 * @returns   0 - Time out (no entities were triggered).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  The given waitset is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The waitset has already been deleted.
 */
_Pre_satisfies_((waitset & DDS_ENTITY_KIND_MASK) == DDS_KIND_WAITSET)
DDS_EXPORT dds_return_t
dds_waitset_wait_until(
        _In_ dds_entity_t waitset,
        _Out_writes_to_(nxs, return < 0 ? 0 : return) dds_attach_t *xs,
        _In_ size_t nxs,
        _In_ dds_time_t abstimeout);

/*
  There are a number of read and take variations.

  Return value is the number of elements returned. "max_samples"
  should have the same type, as one can't return more than MAX_INT
  this way, anyway. X, Y, CX, CY return to the various filtering
  options, see the DCPS spec.

  O ::= read | take

  X             => CX
  (empty)          (empty)
  _next_instance   instance_handle_t prev

  Y             => CY
  (empty)          uint32_t mask
  _cond            cond_t cond -- refers to a read condition (or query if implemented)
*/

/**
 * @brief Access and read the collection of data values (of same type) and sample info from the
 *        data reader, readcondition or querycondition.
 *
 * Return value provides information about number of samples read, which will
 * be <= maxs. Based on the count, the buffer will contain data to be read only
 * when valid_data bit in sample info structure is set.
 * The buffer required for data values, could be allocated explicitly or can
 * use the memory from data reader to prevent copy. In the latter case, buffer and
 * sample_info should be returned back, once it is no longer using the Data.
 * Data values once read will remain in the buffer with the sample_state set to READ
 * and view_state set to NOT_NEW.
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity
 * @param[out] buf An array of pointers to samples into which data is read (pointers can be NULL)
 * @param[out] si Pointer to an array of \ref dds_sample_info_t returned for each data value
 * @param[in]  bufsz The size of buffer provided
 * @param[in]  maxs Maximum number of samples to read
 *
 * @returns >=0 - Success (number of samples read).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
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
_Pre_satisfies_(((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
DDS_EXPORT dds_return_t
dds_read(
        _In_ dds_entity_t reader_or_condition,
        _Out_ void **buf,
        _Out_ dds_sample_info_t *si,
        _In_ size_t bufsz,
        _In_ uint32_t maxs);

/**
 * @brief Access and read loaned samples of data reader, readcondition or querycondition.
 *
 * After dds_read_wl function is being called and the data has been handled, dds_return_loan function must be called to possibly free memory
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity
 * @param[out] buf An array of pointers to samples into which data is read (pointers can be NULL)
 * @param[out] si Pointer to an array of \ref dds_sample_info_t returned for each data value
 * @param[in]  maxs Maximum number of samples to read
 *
 * @returns >=0 - Success (number of samples read).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
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
_Pre_satisfies_(((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
DDS_EXPORT dds_return_t
dds_read_wl(
        _In_ dds_entity_t reader_or_condition,
        _Out_ void **buf,
        _Out_ dds_sample_info_t *si,
        _In_ uint32_t maxs);

/**
 * @brief Read the collection of data values and sample info from the data reader, readcondition
 *        or querycondition based on mask.
 *
 * When using a readcondition or querycondition, their masks are or'd with the given mask.
 *
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity
 * @param[out] buf An array of pointers to samples into which data is read (pointers can be NULL)
 * @param[out] si Pointer to an array of \ref dds_sample_info_t returned for each data value
 * @param[in]  bufsz The size of buffer provided
 * @param[in]  maxs Maximum number of samples to read
 * @param[in]  mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns >=0 - Success (number of samples read).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
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
_Pre_satisfies_(((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
DDS_EXPORT dds_return_t
dds_read_mask(
        _In_ dds_entity_t reader_or_condition,
        _Out_ void **buf,
        _Out_ dds_sample_info_t *si,
        _In_ size_t bufsz,
        _In_ uint32_t maxs,
        _In_ uint32_t mask /* In case of ReadCondition, both masks are applied (OR'd) */);

/**
 * @brief Access and read loaned samples of data reader, readcondition
 *        or querycondition based on mask
 *
 * When using a readcondition or querycondition, their masks are or'd with the given mask.
 *
 * After dds_read_mask_wl function is being called and the data has been handled, dds_return_loan function must be called to possibly free memory
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity
 * @param[out] buf An array of pointers to samples into which data is read (pointers can be NULL)
 * @param[out] si Pointer to an array of \ref dds_sample_info_t returned for each data value
 * @param[in]  maxs Maximum number of samples to read
 * @param[in]  mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns >=0 - Success (number of samples read).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
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
_Pre_satisfies_(((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
DDS_EXPORT dds_return_t
dds_read_mask_wl(
        _In_ dds_entity_t reader_or_condition,
        _Out_ void **buf,
        _Out_ dds_sample_info_t *si,
        _In_ uint32_t maxs,
        _In_ uint32_t mask /* In case of ReadCondition, both masks are applied (OR'd) */);


/**
 * @brief Access and read the collection of data values (of same type) and sample info from the
 *        data reader, readcondition or querycondition, coped by the provided instance handle.
 *
 * This operation implements the same functionality as dds_read, except that only data scoped to
 * the provided instance handle is read.
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity
 * @param[out] buf An array of pointers to samples into which data is read (pointers can be NULL)
 * @param[out] si Pointer to an array of \ref dds_sample_info_t returned for each data value
 * @param[in]  bufsz The size of buffer provided
 * @param[in]  maxs Maximum number of samples to read
 * @param[in]  handle Instance handle related to the samples to read
 *
 * @returns >=0 - Success (number of samples read).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *                  The instance handle has not been registered with this reader.
 */
_Pre_satisfies_(((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
DDS_EXPORT dds_return_t
dds_read_instance(
        _In_ dds_entity_t reader_or_condition,
        _Out_ void **buf,
        _Out_ dds_sample_info_t *si,
        _In_ size_t bufsz,
        _In_ uint32_t maxs,
        _In_ dds_instance_handle_t handle);

/**
 * @brief Access and read loaned samples of data reader, readcondition or querycondition,
 *        scoped by the provided instance handle.
 *
 * This operation implements the same functionality as dds_read_wl, except that only data
 *        scoped to the provided instance handle is read.
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity
 * @param[out] buf An array of pointers to samples into which data is read (pointers can be NULL)
 * @param[out] si Pointer to an array of \ref dds_sample_info_t returned for each data value
 * @param[in]  maxs Maximum number of samples to read
 * @param[in]  handle Instance handle related to the samples to read
 *
 * @returns >=0 - Success (number of samples read).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *                  The instance handle has not been registered with this reader.
 */
_Pre_satisfies_(((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
DDS_EXPORT dds_return_t
dds_read_instance_wl(
        _In_ dds_entity_t reader_or_condition,
        _Out_ void **buf,
        _Out_ dds_sample_info_t *si,
        _In_ uint32_t maxs,
        _In_ dds_instance_handle_t handle);

/**
 * @brief Read the collection of data values and sample info from the data reader, readcondition
 *        or querycondition based on mask and scoped by the provided instance handle.
 *
 * This operation implements the same functionality as dds_read_mask, except that only data
 *        scoped to the provided instance handle is read.
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity
 * @param[out] buf An array of pointers to samples into which data is read (pointers can be NULL)
 * @param[out] si Pointer to an array of \ref dds_sample_info_t returned for each data value
 * @param[in]  bufsz The size of buffer provided
 * @param[in]  maxs Maximum number of samples to read
 * @param[in]  handle Instance handle related to the samples to read
 * @param[in]  mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns >=0 - Success (number of samples read).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *                  The instance handle has not been registered with this reader.
 */
_Pre_satisfies_(((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
DDS_EXPORT dds_return_t
dds_read_instance_mask(
        _In_ dds_entity_t reader_or_condition,
        _Out_ void **buf,
        _Out_ dds_sample_info_t *si,
        _In_ size_t bufsz,
        _In_ uint32_t maxs,
        _In_ dds_instance_handle_t handle,
        _In_ uint32_t mask);

/**
 * @brief Access and read loaned samples of data reader, readcondition or
 *        querycondition based on mask, scoped by the provided instance handle.
 *
 * This operation implements the same functionality as dds_read_mask_wl, except that
 * only data scoped to the provided instance handle is read.
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity
 * @param[out] buf An array of pointers to samples into which data is read (pointers can be NULL)
 * @param[out] si Pointer to an array of \ref dds_sample_info_t returned for each data value
 * @param[in]  maxs Maximum number of samples to read
 * @param[in]  handle Instance handle related to the samples to read
 * @param[in]  mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns >=0 - Success (number of samples read).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *                  The instance handle has not been registered with this reader.
 */
_Pre_satisfies_(((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
DDS_EXPORT dds_return_t
dds_read_instance_mask_wl(
        _In_ dds_entity_t reader_or_condition,
        _Out_ void **buf,
        _Out_ dds_sample_info_t *si,
        _In_ uint32_t maxs,
        _In_ dds_instance_handle_t handle,
        _In_ uint32_t mask);

/**
 * Description : Access the collection of data values (of same type) and sample info from the data reader
 *               based on the criteria specified in the read condition.
 *               Read condition must be attached to the data reader before associating with data read.
 *               Return value provides information about number of samples read, which will
 *               be <= maxs. Based on the count, the buffer will contain data to be read only
 *               when valid_data bit in sample info structure is set.
 *               The buffer required for data values, could be allocated explicitly or can
 *               use the memory from data reader to prevent copy. In the latter case, buffer and
 *               sample_info should be returned back, once it is no longer using the Data.
 *               Data values once read will remain in the buffer with the sample_state set to READ
 *               and view_state set to NOT_NEW.
 *
 * Arguments :
 *   -# rd Reader entity
 *   -# buf an array of pointers to samples into which data is read (pointers can be NULL)
 *   -# maxs maximum number of samples to read
 *   -# si pointer to an array of \ref dds_sample_info_t returned for each data value
 *   -# cond read condition to filter the data samples based on the content
 *   -# Returns the number of samples read, 0 indicates no data to read.
 */

/**
 * @brief Access the collection of data values (of same type) and sample info from the
 *        data reader, readcondition or querycondition.
 *
 * Data value once read is removed from the Data Reader cannot to
 * 'read' or 'taken' again.
 * Return value provides information about number of samples read, which will
 * be <= maxs. Based on the count, the buffer will contain data to be read only
 * when valid_data bit in sample info structure is set.
 * The buffer required for data values, could be allocated explicitly or can
 * use the memory from data reader to prevent copy. In the latter case, buffer and
 * sample_info should be returned back, once it is no longer using the Data.
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity
 * @param[out] buf An array of pointers to samples into which data is read (pointers can be NULL)
 * @param[out] si Pointer to an array of \ref dds_sample_info_t returned for each data value
 * @param[in]  bufsz The size of buffer provided
 * @param[in]  maxs Maximum number of samples to read
 *
 * @returns >=0 - Success (number of samples read).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
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
_Pre_satisfies_(((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
DDS_EXPORT dds_return_t
dds_take(
        _In_ dds_entity_t reader_or_condition,
        _Out_ void **buf, /* _Out_writes_to_ annotation would be nice, however we don't know the size of the elements. Solution for that? Is there a better annotation? */
        _Out_ dds_sample_info_t *si,
        _In_ size_t bufsz,
        _In_ uint32_t maxs);

/**
 * @brief Access loaned samples of data reader, readcondition or querycondition.
 *
 * After dds_take_wl function is being called and the data has been handled, dds_return_loan function must be called to possibly free memory
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity
 * @param[out] buf An array of pointers to samples into which data is read (pointers can be NULL)
 * @param[out] si Pointer to an array of \ref dds_sample_info_t returned for each data value
 * @param[in]  maxs Maximum number of samples to read
 *
 * @returns >=0 - Success (number of samples read).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
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
_Pre_satisfies_(((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
DDS_EXPORT dds_return_t
dds_take_wl(
        _In_ dds_entity_t reader_or_condition,
        _Out_ void **buf, /* _Out_writes_to_ annotation would be nice, however we don't know the size of the elements. Solution for that? Is there a better annotation? */
        _Out_ dds_sample_info_t *si,
        _In_ uint32_t maxs);

/**
 * @brief Take the collection of data values (of same type) and sample info from the
 *        data reader, readcondition or querycondition based on mask
 *
 * When using a readcondition or querycondition, their masks are or'd with the given mask.
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity
 * @param[out] buf An array of pointers to samples into which data is read (pointers can be NULL)
 * @param[out] si Pointer to an array of \ref dds_sample_info_t returned for each data value
 * @param[in]  bufsz The size of buffer provided
 * @param[in]  maxs Maximum number of samples to read
 * @param[in]  mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns >=0 - Success (number of samples read).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
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
_Pre_satisfies_(((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
DDS_EXPORT dds_return_t
dds_take_mask(
        _In_ dds_entity_t reader_or_condition,
        _Out_ void **buf, /* _Out_writes_to_ annotation would be nice, however we don't know the size of the elements. Solution for that? Is there a better annotation? */
        _Out_ dds_sample_info_t *si,
        _In_ size_t bufsz,
        _In_ uint32_t maxs,
        _In_ uint32_t mask);

/**
 * @brief  Access loaned samples of data reader, readcondition or querycondition based on mask.
 *
 * When using a readcondition or querycondition, their masks are or'd with the given mask.
 *
 * After dds_take_mask_wl function is being called and the data has been handled, dds_return_loan function must be called to possibly free memory
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity
 * @param[out] buf An array of pointers to samples into which data is read (pointers can be NULL)
 * @param[out] si Pointer to an array of \ref dds_sample_info_t returned for each data value
 * @param[in]  maxs Maximum number of samples to read
 * @param[in]  mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns >=0 - Success (number of samples read).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
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
_Pre_satisfies_(((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
DDS_EXPORT dds_return_t
dds_take_mask_wl(
        _In_ dds_entity_t reader_or_condition,
        _Out_ void **buf, /* _Out_writes_to_ annotation would be nice, however we don't know the size of the elements. Solution for that? Is there a better annotation? */
        _Out_ dds_sample_info_t *si,
        _In_ uint32_t maxs,
        _In_ uint32_t mask);

/*
 * Untyped API, which take serialized blobs now.
 * Whether they remain exposed like this with X-types isn't entirely clear yet.
 * TODO: make a decide about dds_takecdr
 * If we want dds_takecdr(), shouldn't there be a dds_readcdr()?
 */
struct serdata;

DDS_EXPORT int
dds_takecdr(
        dds_entity_t reader_or_condition,
        struct serdata **buf,
        uint32_t maxs,
        dds_sample_info_t *si,
        uint32_t mask);


/**
 * @brief Access the collection of data values (of same type) and sample info from the
 *        data reader, readcondition or querycondition but scoped by the given
 *        instance handle.
 *
 * This operation mplements the same functionality as dds_take, except that only data
 *        scoped to the provided instance handle is taken.
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity
 * @param[out] buf An array of pointers to samples into which data is read (pointers can be NULL)
 * @param[out] si Pointer to an array of \ref dds_sample_info_t returned for each data value
 * @param[in]  bufsz The size of buffer provided
 * @param[in]  maxs Maximum number of samples to read
 * @param[in]  handle Instance handle related to the samples to read
 *
 * @returns >=0 - Success (number of samples read).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *                  The instance handle has not been registered with this reader.
 */
_Pre_satisfies_(((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
DDS_EXPORT dds_return_t
dds_take_instance(
        _In_ dds_entity_t reader_or_condition,
        _Out_ void **buf,
        _Out_ dds_sample_info_t *si,
        _In_ size_t bufsz,
        _In_ uint32_t maxs,
        _In_ dds_instance_handle_t handle);

/**
 * @brief Access loaned samples of data reader, readcondition or querycondition,
 *        scoped by the given instance handle.
 *
 * This operation implements the same functionality as dds_take_wl, except that
 * only data scoped to the provided instance handle is read.
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity
 * @param[out] buf An array of pointers to samples into which data is read (pointers can be NULL)
 * @param[out] si Pointer to an array of \ref dds_sample_info_t returned for each data value
 * @param[in]  maxs Maximum number of samples to read
 * @param[in]  handle Instance handle related to the samples to read
 *
 * @returns >=0 - Success (number of samples read).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *                  The instance handle has not been registered with this reader.
 */
_Pre_satisfies_(((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
DDS_EXPORT dds_return_t
dds_take_instance_wl(
        _In_ dds_entity_t reader_or_condition,
        _Out_ void **buf,
        _Out_ dds_sample_info_t *si,
        _In_ uint32_t maxs,
        _In_ dds_instance_handle_t handle);

/**
 * @brief Take the collection of data values (of same type) and sample info from the
 *        data reader, readcondition or querycondition based on mask and scoped
 *        by the given instance handle.
 *
 * This operation implements the same functionality as dds_take_mask, except that only
 * data scoped to the provided instance handle is read.
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity
 * @param[out] buf An array of pointers to samples into which data is read (pointers can be NULL)
 * @param[out] si Pointer to an array of \ref dds_sample_info_t returned for each data value
 * @param[in]  bufsz The size of buffer provided
 * @param[in]  maxs Maximum number of samples to read
 * @param[in]  handle Instance handle related to the samples to read
 * @param[in]  mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns >=0 - Success (number of samples read).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *                  The instance handle has not been registered with this reader.
 */
_Pre_satisfies_(((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
DDS_EXPORT dds_return_t
dds_take_instance_mask(
        _In_ dds_entity_t reader_or_condition,
        _Out_ void **buf,
        _Out_ dds_sample_info_t *si,
        _In_ size_t bufsz,
        _In_ uint32_t maxs,
        _In_ dds_instance_handle_t handle,
        _In_ uint32_t mask);

/**
 * @brief  Access loaned samples of data reader, readcondition or querycondition based
 *         on mask and scoped by the given intance handle.
 *
 * This operation implements the same functionality as dds_take_mask_wl, except that
 * only data scoped to the provided instance handle is read.
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity
 * @param[out] buf An array of pointers to samples into which data is read (pointers can be NULL)
 * @param[out] si Pointer to an array of \ref dds_sample_info_t returned for each data value
 * @param[in]  maxs Maximum number of samples to read
 * @param[in]  handle Instance handle related to the samples to read
 * @param[in]  mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns >=0 - Success (number of samples read).
 * @returns  <0 - Failure (use dds_err_nr() to get error value).
 *
 * @retval DDS_RETCODE_ERROR
 *                  An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *                  One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *                  The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *                  The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *                  The instance handle has not been registered with this reader.
 */
_Pre_satisfies_(((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
DDS_EXPORT dds_return_t
dds_take_instance_mask_wl(
        _In_ dds_entity_t reader_or_condition,
        _Out_ void **buf,
        _Out_ dds_sample_info_t *si,
        _In_ uint32_t maxs,
        _In_ dds_instance_handle_t handle,
        _In_ uint32_t mask);


/*
  The read/take next functions return a single sample. The returned sample
  has a sample state of NOT_READ, a view state of ANY_VIEW_STATE and an
  instance state of ANY_INSTANCE_STATE.
*/

/**
 * Description : This operation copies the next, non-previously accessed data value and corresponding
 *               sample info and removes from the data reader.
 *
 * Arguments :
 * -# rd Reader entity
 * -# buf an array of pointers to samples into which data is read (pointers can be NULL)
 * -# si pointer to \ref dds_sample_info_t returned for a data value
 * -# Returns 1 on successful operation, else 0 if there is no data to be read.
 */
DDS_EXPORT dds_return_t
dds_take_next(
        _In_ dds_entity_t reader_or_condition,
        _Out_ void **buf,
        _Out_ dds_sample_info_t *si);

DDS_EXPORT dds_return_t
dds_take_next_wl(
        _In_ dds_entity_t reader_or_condition,
        _Out_ void **buf,
        _Out_ dds_sample_info_t *si);

/**
 * Description : This operation copies the next, non-previously accessed data value and corresponding
 *               sample info.
 *
 * Arguments :
 * -# rd Reader entity
 * -# buf an array of pointers to samples into which data is read (pointers can be NULL)
 * -# si pointer to \ref dds_sample_info_t returned for a data value
 * -# Returns 1 on successful operation, else 0 if there is no data to be read.
 */
DDS_EXPORT dds_return_t
dds_read_next(
        _In_ dds_entity_t reader_or_condition,
        _Out_ void **buf,
        _Out_ dds_sample_info_t *si);

DDS_EXPORT dds_return_t
dds_read_next_wl(
        _In_ dds_entity_t reader_or_condition,
        _Out_ void **buf,
        _Out_ dds_sample_info_t *si);

/**
 * @brief Return loaned samples to data-reader or condition associated with a data-reader
 *
 * Used to release sample buffers returned by a read/take operation. When the application
 * provides an empty buffer, memory is allocated and managed by DDS. By calling dds_return_loan,
 * the memory is released so that the buffer can be reused during a successive read/take operation.
 * When a condition is provided, the reader to which the condition belongs is looked up.
 *
 * @param[in] rd_or_cnd Reader or condition that belongs to a reader
 * @param[in] buf An array of (pointers to) samples
 * @param[in] bufsz The number of (pointers to) samples stored in buf
 *
 * @returns A dds_return_t indicating success or failure
 */
_Pre_satisfies_(((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER ) ||\
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_READ ) || \
                ((reader_or_condition & DDS_ENTITY_KIND_MASK) == DDS_KIND_COND_QUERY ))
DDS_EXPORT _Must_inspect_result_ dds_return_t
dds_return_loan(
        _In_ dds_entity_t reader_or_condition,
        _Inout_updates_(bufsz) void **buf,
        _In_ size_t bufsz);

/*
  Instance handle <=> key value mapping.
  Functions exactly as read w.r.t. treatment of data
  parameter. On output, only key values set.

    T x = { ... };
    T y;
    dds_instance_handle_t ih;
    ih = dds_instance_lookup (e, &x);
    dds_instance_get_key (e, ih, &y);
*/

/**
 * Description : This operation takes a sample and returns an instance handle to be used for
 * subsequent operations.
 *
 * Arguments :
 * -# e Reader or Writer entity
 * -# data sample with a key fields set
 * -# Returns instance handle or DDS_HANDLE_NIL if instance could not be found from key
 */
DDS_EXPORT dds_return_t
dds_lookup_instance(
        _In_ dds_entity_t entity,
        _Out_ dds_instance_handle_t *handle,
        _In_ const void *data);

/**
 * Description : This operation takes an instance handle and return a key-value corresponding to it.
 *
 * Arguments :
 * -# e Reader or Writer entity
 * -# inst Instance handle
 * -# data pointer to an instance, to which the key ID corresponding to the instance handle will be
 *    returned, the sample in the instance should be ignored.
 * -# Returns 0 on successful operation, or a non-zero value to indicate an error if the instance
 *    passed doesn't have a key-value
 */
DDS_EXPORT dds_return_t
dds_instance_get_key(
        _In_ dds_entity_t entity,
        _In_ dds_instance_handle_t handle,
        _Out_ void *data);

/**
 * @brief Begin coherent publishing or begin accessing a coherent set in a subscriber
 *
 * Invoking on a Writer or Reader behaves as if dds_begin_coherent was invoked on its parent
 * Publisher or Subscriber respectively.
 *
 * @param[in]  e - The entity that is prepared for coherent access
 *
 * @returns - A dds_return_t indicating success or failure
 *
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 *         DDS_RETCODE_BAD_PARAMETER
 *             The provided entity is invalid or not supported
 */
_Pre_satisfies_(((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER    ) || \
                ((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_SUBSCRIBER) || \
                ((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER    ) || \
                ((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_SUBSCRIBER) )
DDS_EXPORT dds_return_t
dds_begin_coherent(
        _In_ dds_entity_t entity);

/**
 * @brief End coherent publishing or end accessing a coherent set in a subscriber
 *
 * Invoking on a Writer or Reader behaves as if dds_end_coherent was invoked on its parent
 * Publisher or Subscriber respectively.
 *
 * @param[in] e - The entity on which coherent access is finished
 *
 * @returns - A dds_return_t indicating success or failure
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful
 *         DDS_RETCODE_BAD_PARAMETER
 *             The provided entity is invalid or not supported
 */
_Pre_satisfies_(((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER    ) || \
                ((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_SUBSCRIBER) || \
                ((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_WRITER    ) || \
                ((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_SUBSCRIBER) )
DDS_EXPORT dds_return_t
dds_end_coherent(
        _In_ dds_entity_t entity);

/**
 * @brief Trigger DATA_AVAILABLE event on contained readers
 *
 * The DATA_AVAILABLE event is broadcast to all readers owned by this subscriber that currently
 * have new data available. Any on_data_available listener callbacks attached to respective
 * readers are invoked.
 *
 * @param[in] sub A subscriber
 *
 * @returns - A dds_return_t indicating success or failure
 *
 * @reval DDS_RETCODE_OK
 *            The operation was successful
 *        DDS_RETCODE_BAD_PARAMETER
 *            The provided subscriber is invalid
 */
_Pre_satisfies_((subscriber & DDS_ENTITY_KIND_MASK) == DDS_KIND_SUBSCRIBER)
DDS_EXPORT dds_return_t
dds_notify_readers(
        _In_ dds_entity_t subscriber);


/**
 * Description : Resolves the domain-entity identified by id if it exists
 *
 * Arguments :
 * -# id
 */
DDS_EXPORT dds_entity_t dds_get_domain(_In_ dds_domainid_t id);

/**
 * Description : Retrieves the matched publications (for a given Reader) or subscriptions (for a given Writer)
 *
 * Arguments :
 * -# wr_or_r Writer or Reader
 * -# handles Array of size nofHandles
 * -# nofHandles Number of elements that can be written to in handles
 *
 * -# Returns the number of available matched publications or subscriptions. If return > nofHandles
 *    the resulting set is truncated. Handles are only initialized up to min(return, nofHandles).
 */
DDS_EXPORT dds_return_t dds_get_matched(_In_ dds_entity_t wr_or_r, _Out_writes_to_(nofHandles, return) dds_instance_handle_t *handles, _In_ size_t nofHandles);

/**
 * Description : Asserts the liveliness of the entity
 *
 * Arguments :
 * -# e Entity
 */
DDS_EXPORT dds_return_t dds_assert_liveliness(_In_ dds_entity_t e);

/**
 * Description : Checks whether entity c is contained in entity e
 *
 * Containment is defined as follows: TODO
 *
 * Arguments :
 * -# e Entity for which has to be determined whether c is contained within it
 * -# c Entity to check for being contained in e
 */
DDS_EXPORT dds_return_t dds_contains(_In_ dds_entity_t e, _In_ dds_entity_t c);

/**
 * Description : Returns the current wall-clock as used for timestamps
 */
DDS_EXPORT dds_time_t dds_time(void);

/**
 * Description : Checks whether the entity has one of its enabled statuses triggered.
 *
 * Arguments :
 * -# e Entity for which to check for triggered status
 */
_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
DDS_EXPORT dds_return_t
dds_triggered(
        _In_ dds_entity_t entity);

/* TODO: dds_create_contentfilteredtopic -> dds_create_topic_w_query and use dds_get_query and the like. */
DDS_EXPORT dds_entity_t dds_create_contentfilteredtopic(_In_ dds_entity_t pp, _In_z_ const char * name, _In_ dds_entity_t related_topic, _In_z_ const char *expression, _In_reads_opt_z_(npars) const char ** parameters, _In_ size_t npars);

/**
 * Description : Tries to find the topic with the supplied name.
 *
 * Arguments :
 * -# pp Participant
 * -# name Topic-name to look for
 */
DDS_EXPORT dds_entity_t dds_lookup_topic(_In_ dds_entity_t pp, _In_z_ const char * name);

/**
 * Description : Ignore the entity described by handle.
 *
 * Arguments :
 * -# pp Participant
 * -# handle Instance-handle of entity to be ignored.
 */
DDS_EXPORT dds_return_t dds_ignore(_In_ dds_entity_t pp, _In_ dds_instance_handle_t handle);

/**
 * Description : Retrieve the topic on which the content-filtered-topic is based
 *
 * TODO: Refactor CFT
 *
 * Arguments :
 * -# cft ContentFilteredTopic
 */
DDS_EXPORT dds_entity_t dds_get_related_topic(_In_ dds_entity_t cft);
/* DDS_EXPORT dds_return_t dds_contentfilteredtopic_get_parameters(...) see dds_get_query_patameters(...) */;
/* DDS_EXPORT dds_return_t dds_contentfilteredtopic_set_parameters(...) see dds_set_query_patameters(...) */;

/**
 * Description : Retrieve the query underlying the entity
 *
 *
 * Arguments :
 * -# top_mt_qc Topic, MultiTopic, QueryConditon
 */
DDS_EXPORT dds_entity_t dds_get_query(_In_ dds_entity_t top_mt_qc);

/**
 * Description : Retrieve the query-parameters
 *
 *
 * Arguments :
 * -# top_mt_qc Topic, MultiTopic, QueryConditon
 */
DDS_EXPORT dds_return_t dds_get_query_parameters(_In_ dds_entity_t e, _Out_writes_to_(npars, return) const char ** params, _In_ size_t npars);

/**
 * Description : Set the query-parameters
 *
 * Arguments :
 * -# top_mt_qc Topic, MultiTopic, QueryConditon
 */
DDS_EXPORT dds_return_t dds_set_query_parameters(_In_ dds_entity_t e, _In_reads_opt_z_(npars) const char ** parameters, _In_ size_t npars);

DDS_EXPORT dds_entity_t dds_get_topic(_In_ dds_entity_t e); /* Convenience-wrapper for (multiple) get_parent on Writer or Reader or their children*/

#if defined (__cplusplus)
}
#endif
#endif /* DDS_H */
