// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS_H
#define DDS_H

/**
 * @file
 * @brief Eclipse Cyclone DDS C header
 * Main header of the Cyclone DDS C library, containing everything you need
 * for your DDS application.
 */

/**
 * @defgroup dds (DDS Functionality)
 */
/**
 * @defgroup deprecated (Deprecated functionality)
 */

#include "dds/export.h"
#include "dds/features.h"

#include "dds/ddsc/dds_basic_types.h"

/* Sub components */

#include "dds/ddsrt/time.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsc/dds_public_impl.h"
#include "dds/ddsc/dds_public_alloc.h"
#include "dds/ddsc/dds_public_qos.h"
#include "dds/ddsc/dds_public_error.h"
#include "dds/ddsc/dds_public_status.h"
#include "dds/ddsc/dds_public_listener.h"
#include "dds/ddsc/dds_public_dynamic_type.h"
#include "dds/ddsc/dds_public_loan_api.h"
#include "dds/ddsc/dds_public_qos_provider.h"

#if defined (__cplusplus)
extern "C" {
#endif


/**
 * @brief DDS Type Identifier (XTypes)
 * @ingroup dds
 * DOC_TODO
 */
typedef struct ddsi_typeid dds_typeid_t;

/**
 * @brief DDS Type Information (XTypes)
 * @ingroup dds
 * DOC_TODO
 */
typedef struct ddsi_typeinfo dds_typeinfo_t;

/**
 * @brief DDS Type Object (XTypes)
 * @ingroup dds
 * DOC_TODO
 */
typedef struct ddsi_typeobj dds_typeobj_t;

/**
 * @brief Reader History Cache
 * @ingroup dds
 * DOC_TODO
 */
struct dds_rhc;

/**
 * @brief DDSI parameter list
 * @ingroup dds
 * DOC_TODO
 */
struct ddsi_plist;

/**
 * @anchor ddsi_sertype
 * @brief DDSI sertype
 * @ingroup dds
 * DOC_TODO
 */
struct ddsi_sertype;

/**
 * @anchor ddsi_serdata
 * @brief DDSI Serdata
 * @ingroup dds
 * DOC_TODO
 */
struct ddsi_serdata;

/**
 * @brief DDSI Config
 * @ingroup dds
 * DOC_TODO
 */
struct ddsi_config;

/**
 * @brief Indicates that the library uses ddsi_sertype instead of ddsi_sertopic
 * @ingroup dds
 */
#define DDS_HAS_DDSI_SERTYPE 1

/**
 * @defgroup builtintopic (Builtin Topic Support)
 * @ingroup dds
 */
/**
 * @defgroup builtintopic_constants (Constants)
 * @ingroup builtintopic
 * @brief Convenience constants for referring to builtin topics
 * These constants can be used in place of an actual topic handle when creating
 * readers for built-in topics.
 */
/**
 * @def DDS_BUILTIN_TOPIC_DCPSPARTICIPANT
 * @ingroup builtintopic_constants
 * Pseudo topic handle for the built-in topic DCPSParticipant. Samples from this topic are
 * @ref dds_builtintopic_participant structs.
 */
/**
 * @def DDS_BUILTIN_TOPIC_DCPSTOPIC
 * @ingroup builtintopic_constants
 * Pseudo topic handle for the built-in topic DCPSTopic. Samples from this topic are
 * @ref dds_builtintopic_topic structs. Note that this only works if Cyclone DDS
 * was built with topic discovery support.
 */
/**
 * @def DDS_BUILTIN_TOPIC_DCPSPUBLICATION
 * @ingroup builtintopic_constants
 * Pseudo topic handle for the built-in topic DCPSPublication. Samples from this topic are
 * @ref dds_builtintopic_endpoint structs.
 */
/**
 * @def DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION
 * @ingroup builtintopic_constants
 * Pseudo topic handle for the built-in topic DCPSSubscription. Samples from this topic are
 * @ref dds_builtintopic_endpoint structs.
 */

#define DDS_BUILTIN_TOPIC_DCPSPARTICIPANT  ((dds_entity_t) (DDS_MIN_PSEUDO_HANDLE + 1))
#define DDS_BUILTIN_TOPIC_DCPSTOPIC        ((dds_entity_t) (DDS_MIN_PSEUDO_HANDLE + 2))
#define DDS_BUILTIN_TOPIC_DCPSPUBLICATION  ((dds_entity_t) (DDS_MIN_PSEUDO_HANDLE + 3))
#define DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION ((dds_entity_t) (DDS_MIN_PSEUDO_HANDLE + 4))


/**
 * @defgroup entity_status (Entity Status)
 * @ingroup entity
 * All entities have a set of "status conditions"
 * (following the DCPS spec), read peeks, take reads & resets (analogously to read &
 * take operations on reader). The "mask" allows operating only on a subset of the statuses.
 * Enabled status analogously to DCPS spec.
 * @{
 */
/**
 * @brief These identifiers are used to generate the bitshifted identifiers.
 * By using bitflags instead of these IDs the process of building status masks is
 * simplified to using simple binary OR operations.
 * DOC_TODO fix the refs
 */
typedef enum dds_status_id {
  DDS_INCONSISTENT_TOPIC_STATUS_ID,         /**< See @ref DDS_INCONSISTENT_TOPIC_STATUS */
  DDS_OFFERED_DEADLINE_MISSED_STATUS_ID,    /**< See @ref DDS_OFFERED_DEADLINE_MISSED_STATUS */
  DDS_REQUESTED_DEADLINE_MISSED_STATUS_ID,  /**< See @ref DDS_REQUESTED_DEADLINE_MISSED_STATUS */
  DDS_OFFERED_INCOMPATIBLE_QOS_STATUS_ID,   /**< See @ref DDS_OFFERED_INCOMPATIBLE_QOS_STATUS */
  DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS_ID, /**< See @ref DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS */
  DDS_SAMPLE_LOST_STATUS_ID,                /**< See @ref DDS_SAMPLE_LOST_STATUS */
  DDS_SAMPLE_REJECTED_STATUS_ID,            /**< See @ref DDS_SAMPLE_REJECTED_STATUS */
  DDS_DATA_ON_READERS_STATUS_ID,            /**< See @ref DDS_DATA_ON_READERS_STATUS */
  DDS_DATA_AVAILABLE_STATUS_ID,             /**< See @ref DDS_DATA_AVAILABLE_STATUS */
  DDS_LIVELINESS_LOST_STATUS_ID,            /**< See @ref DDS_LIVELINESS_LOST_STATUS */
  DDS_LIVELINESS_CHANGED_STATUS_ID,         /**< See @ref DDS_LIVELINESS_CHANGED_STATUS */
  DDS_PUBLICATION_MATCHED_STATUS_ID,        /**< See @ref DDS_PUBLICATION_MATCHED_STATUS */
  DDS_SUBSCRIPTION_MATCHED_STATUS_ID        /**< See @ref DDS_SUBSCRIPTION_MATCHED_STATUS */
} dds_status_id_t;

/** Helper value to indicate the highest bit that can be set in a status mask. */
#define DDS_STATUS_ID_MAX (DDS_SUBSCRIPTION_MATCHED_STATUS_ID)

/**
 * @anchor DDS_INCONSISTENT_TOPIC_STATUS
 * Another topic exists with the same name but with different characteristics.
 */
#define DDS_INCONSISTENT_TOPIC_STATUS          (1u << DDS_INCONSISTENT_TOPIC_STATUS_ID)
/**
 * @anchor DDS_OFFERED_DEADLINE_MISSED_STATUS
 * The deadline that the writer has committed through its deadline QoS policy was not respected for a specific instance. */
#define DDS_OFFERED_DEADLINE_MISSED_STATUS     (1u << DDS_OFFERED_DEADLINE_MISSED_STATUS_ID)
/**
 * @anchor DDS_REQUESTED_DEADLINE_MISSED_STATUS
 * The deadline that the reader was expecting through its deadline QoS policy was not respected for a specific instance. */
#define DDS_REQUESTED_DEADLINE_MISSED_STATUS   (1u << DDS_REQUESTED_DEADLINE_MISSED_STATUS_ID)
/**
 * @anchor DDS_OFFERED_INCOMPATIBLE_QOS_STATUS
 * A QoS policy setting was incompatible with what was requested. */
#define DDS_OFFERED_INCOMPATIBLE_QOS_STATUS    (1u << DDS_OFFERED_INCOMPATIBLE_QOS_STATUS_ID)
/**
 * @anchor DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS
 * A QoS policy setting was incompatible with what is offered. */
#define DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS  (1u << DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS_ID)
/**
 * @anchor DDS_SAMPLE_LOST_STATUS
 * A sample has been lost (never received). */
#define DDS_SAMPLE_LOST_STATUS                 (1u << DDS_SAMPLE_LOST_STATUS_ID)
/**
 * @anchor DDS_SAMPLE_REJECTED_STATUS
 * A (received) sample has been rejected. */
#define DDS_SAMPLE_REJECTED_STATUS             (1u << DDS_SAMPLE_REJECTED_STATUS_ID)
/**
 * @anchor DDS_DATA_ON_READERS_STATUS
 * New information is available in some of the data readers of a subscriber.
 * (See also the remark on @ref DDS_DATA_AVAILABLE_STATUS) */
#define DDS_DATA_ON_READERS_STATUS             (1u << DDS_DATA_ON_READERS_STATUS_ID)
/**
 * @anchor DDS_DATA_AVAILABLE_STATUS
 * New information is available in a data reader. When a data-available status is
 * triggered or a data-available listener is invoked, it's possible that a sample
 * has already been taken from the reader history cache (RHC), leaving no sample
 * available for reading. This can occur because updating the RHC with a newly
 * arrived sample and triggering the status update or listener callback are not
 * performed atomically. As a result, another thread may have consumed the data
 * from the RHC before the status update or listener invocation has occurred. */
#define DDS_DATA_AVAILABLE_STATUS              (1u << DDS_DATA_AVAILABLE_STATUS_ID)
/**
 * @anchor DDS_LIVELINESS_LOST_STATUS
 * The liveliness that the DDS_DataWriter has committed through its liveliness QoS policy was not respected; thus readers will consider the writer as no longer "alive". */
#define DDS_LIVELINESS_LOST_STATUS             (1u << DDS_LIVELINESS_LOST_STATUS_ID)
/**
 * @anchor DDS_LIVELINESS_CHANGED_STATUS
 * The liveliness of one or more writers, that were writing instances read through the readers has changed. Some writers have become "alive" or "not alive". */
#define DDS_LIVELINESS_CHANGED_STATUS          (1u << DDS_LIVELINESS_CHANGED_STATUS_ID)
/**
 * @anchor DDS_PUBLICATION_MATCHED_STATUS
 * The writer has found a reader that matches the topic and has a compatible QoS. */
#define DDS_PUBLICATION_MATCHED_STATUS         (1u << DDS_PUBLICATION_MATCHED_STATUS_ID)
/**
 * @anchor DDS_SUBSCRIPTION_MATCHED_STATUS
 * The reader has found a writer that matches the topic and has a compatible QoS. */
#define DDS_SUBSCRIPTION_MATCHED_STATUS        (1u << DDS_SUBSCRIPTION_MATCHED_STATUS_ID)
/** @}*/ // end group entity_status

/**
 * @defgroup subscription (Subscription)
 * @ingroup dds
 * DOC_TODO This contains the definitions regarding subscribing to data.
 */

/**
 * @defgroup subdata (Data access)
 * @ingroup subscription
 * Every sample you read from DDS comes with some metadata, which you can inspect and filter on.
 * @{
 */

/** Read state for a data value */
typedef enum dds_sample_state
{
  DDS_SST_READ = DDS_READ_SAMPLE_STATE, /**<DataReader has already accessed the sample by read */
  DDS_SST_NOT_READ = DDS_NOT_READ_SAMPLE_STATE /**<DataReader has not accessed the sample before */
}
dds_sample_state_t;

/** View state of an instance relative to the samples */
typedef enum dds_view_state
{
  /** DataReader is accessing the sample for the first time when the instance is alive */
  DDS_VST_NEW = DDS_NEW_VIEW_STATE,
  /** DataReader accessed the sample before */
  DDS_VST_OLD = DDS_NOT_NEW_VIEW_STATE
}
dds_view_state_t;

/** Defines the state of the instance */
typedef enum dds_instance_state
{
  /** Samples received for the instance from the live data writers */
  DDS_IST_ALIVE = DDS_ALIVE_INSTANCE_STATE,
  /** Instance was explicitly disposed by the data writer */
  DDS_IST_NOT_ALIVE_DISPOSED = DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE,
  /** Instance has been declared as not alive by data reader as there are no live data writers writing that instance */
  DDS_IST_NOT_ALIVE_NO_WRITERS = DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE
}
dds_instance_state_t;

/** Contains information about the associated data value */
typedef struct dds_sample_info
{
  /** Sample state */
  dds_sample_state_t sample_state;
  /** View state */
  dds_view_state_t view_state;
  /** Instance state */
  dds_instance_state_t instance_state;
  /** Indicates whether there is a data associated with a sample
   *  - true, indicates the data is valid
   *  - false, indicates the data is invalid, no data to read */
  bool valid_data;
  /** timestamp of a data instance when it is written */
  dds_time_t source_timestamp;
  /** handle to the data instance */
  dds_instance_handle_t instance_handle;
  /** handle to the publisher */
  dds_instance_handle_t publication_handle;
  /** count of instance state change from NOT_ALIVE_DISPOSED to ALIVE */
  uint32_t disposed_generation_count;
  /** count of instance state change from NOT_ALIVE_NO_WRITERS to ALIVE */
  uint32_t no_writers_generation_count;
  /** indicates the number of samples of the same instance that follow the current one in the collection */
  uint32_t sample_rank;
  /** difference in generations between the sample and most recent sample of the same instance that appears in the returned collection */
  uint32_t generation_rank;
  /** difference in generations between the sample and most recent sample of the same instance when read/take was called */
  uint32_t absolute_generation_rank;
}
dds_sample_info_t;

/** @}*/ // end group subdata

/**
 * @brief Sample structure of the Builtin topic DcpsParticipant.
 * @ingroup builtintopic
 */
typedef struct dds_builtintopic_participant
{
  dds_guid_t key; /**< The GUID that uniquely identifies the participant on the network */
  dds_qos_t *qos; /**< The QoS of the participant */
}
dds_builtintopic_participant_t;

/**
 * @brief Structure of a key in the Builtin topic DcpsTopic.
 * @ingroup builtintopic
 */
typedef struct dds_builtintopic_topic_key {
  unsigned char d[16]; /**< 16-byte unique identifier */
} dds_builtintopic_topic_key_t;

/**
 * @brief Sample structure of the Builtin topic DcpsTopic.
 * @ingroup builtintopic
 */
typedef struct dds_builtintopic_topic
{
  dds_builtintopic_topic_key_t key; /**< The GUID that uniquely identifies the topic on the network */
  char *topic_name; /**< The name of the topic, potentially unicode. */
  char *type_name; /**< The name of the type, potentially unicode. */
  dds_qos_t *qos; /**< The QoS of the topic */
}
dds_builtintopic_topic_t;

/**
 * @brief Sample structure of the Builtin topic DcpsPublication and DcpsSubscription.
 * @ingroup builtintopic
 */
typedef struct dds_builtintopic_endpoint
{
  dds_guid_t key; /**< The GUID that uniquely identifies the endpoint on the network */
  dds_guid_t participant_key; /**< The GUID of the participant this endpoint belongs to. */
  dds_instance_handle_t participant_instance_handle; /**< The instance handle the participant assigned to this enpoint. */
  char *topic_name; /**< The name of the topic, potentially unicode. */
  char *type_name; /**< The name of the type, potentially unicode. */
  dds_qos_t *qos; /**< The QoS of the endpoint */
}
dds_builtintopic_endpoint_t;

/**
 * @defgroup entity (Entities)
 * @ingroup dds
 * @brief Every DDS object in the library is an Entity.
 * Each entity has a process-private handle and is enabled on creation.
 *
 * @section entity_handle_errors Entity handle errors
 * Operations that look up an existing entity ordinarily return
 * DDS_RETCODE_PRECONDITION_NOT_MET if the library is uninitialized and
 * DDS_RETCODE_BAD_PARAMETER if the handle is invalid or closed. Operations that
 * require a particular entity kind ordinarily return DDS_RETCODE_ILLEGAL_OPERATION
 * for a live entity of another kind. Individual functions document exceptions.
 */

/**
 * @brief Enable an entity.
 * @ingroup entity
 * @component generic_entity
 *
 * Cyclone DDS creates entities enabled. Calling this function on an enabled
 * entity succeeds and has no effect.
 *
 * @param[in] entity Entity to enable.
 *
 * @retval DDS_RETCODE_OK The entity is enabled.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_return_t
dds_enable(dds_entity_t entity);

/*
  All entities are represented by a process-private handle, with one
  call to delete an entity and all entities it logically contains.
  That is, it is equivalent to combination of
  delete_contained_entities and delete_xxx in the DCPS API.
*/

/**
 * @brief Delete an entity and its descendants.
 * @ingroup entity
 * @component generic_entity
 *
 * Deleting an entity recursively deletes its children. A topic referenced
 * by a reader or writer stays alive until its last reference is released.
 * The initial delete request succeeds even when destruction is deferred.
 *
 * An implicitly created domain, publisher or subscriber is also deleted
 * when its last child is deleted. An explicitly created entity with no
 * children remains alive until it or one of its ancestors is deleted.
 *
 * @param[in] entity Entity to delete.
 *
 * @retval DDS_RETCODE_OK The delete request was accepted.
 * @retval DDS_RETCODE_ALREADY_DELETED A previous delete request was deferred
 *         and the entity still has outstanding references.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_return_t
dds_delete(dds_entity_t entity);

/**
 * @brief Get the publisher of a writer.
 * @ingroup entity
 * @component entity_relations
 *
 * If the writer was created directly on a participant, this returns its
 * implicitly created publisher.
 *
 * @param[in] writer Writer whose publisher to return.
 *
 * @retval >0 The publisher handle.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION The entity is not a writer.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_entity_t
dds_get_publisher(dds_entity_t writer);

/**
 * @brief Get the subscriber of a reader or one of its conditions.
 * @ingroup entity
 * @component entity_relations
 *
 * For a reader created directly on a participant, the subscriber is the
 * implicitly created subscriber for an ordinary topic or the participant's
 * built-in subscriber for a built-in topic.
 *
 * @param[in] entity Reader, read condition or query condition.
 *
 * @retval >0 The subscriber handle.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION The entity is not a reader, read
 *         condition or query condition.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_entity_t
dds_get_subscriber(dds_entity_t entity);

/**
 * @brief Get the reader that owns a read or query condition.
 * @ingroup entity
 * @component entity_relations
 *
 * @param[in] entity Read condition or query condition.
 *
 * @retval >0 The reader handle.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION The entity is not a read or query condition.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_entity_t
dds_get_datareader(dds_entity_t entity);

/**
 * @defgroup condition (Conditions)
 * @ingroup dds
 * @brief Conditions allow you to express conditional interest in samples,
 * to be used in read/take operations or attach to Waitsets.
 */

/**
 * @brief Get the state mask of a read or query condition.
 * @ingroup condition
 * @component entity_status
 *
 * The result combines the sample, view and instance state bits selected
 * when the condition was created. Other bits in the creation mask are ignored.
 *
 * @param[in]  condition Read condition or query condition.
 * @param[out] mask      Where to store the combined state mask.
 *
 * @retval DDS_RETCODE_OK The mask was returned.
 * @retval DDS_RETCODE_BAD_PARAMETER The @p mask pointer is NULL.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION The entity is not a read or query condition.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_return_t
dds_get_mask(dds_entity_t condition, uint32_t *mask);

/**
 * @brief Get the instance handle that identifies an entity.
 * @ingroup entity
 * @component generic_entity
 *
 * @param[in]  entity Entity whose instance handle to return.
 * @param[out] ihdl   Where to store the instance handle.
 *
 * @retval DDS_RETCODE_OK The instance handle was returned.
 * @retval DDS_RETCODE_BAD_PARAMETER The @p ihdl pointer is NULL.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_return_t
dds_get_instance_handle(dds_entity_t entity, dds_instance_handle_t *ihdl);

/**
 * @brief Get the GUID that identifies a participant, reader or writer on the network.
 * @ingroup entity
 * @component generic_entity
 *
 * @param[in]  entity Participant, reader or writer whose GUID to return.
 * @param[out] guid   Where to store the GUID.
 *
 * @retval DDS_RETCODE_OK The GUID was returned.
 * @retval DDS_RETCODE_BAD_PARAMETER The @p guid pointer is NULL.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION The entity is not a participant, reader or writer.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_return_t
dds_get_guid (dds_entity_t entity, dds_guid_t *guid);

/**
 * @brief Read the selected status flags without clearing them.
 * @ingroup entity_status
 * @component entity_status
 *
 * The selection is independent of the mask enabled for waitset triggering.
 *
 * @param[in]  entity Entity whose status to read.
 * @param[out] status Currently set flags selected by @p mask.
 * @param[in]  mask   Status flags to read, or zero for all statuses.
 *
 * @retval DDS_RETCODE_OK The status flags were returned.
 * @retval DDS_RETCODE_BAD_PARAMETER The @p status pointer is NULL or @p mask
 *         contains an unknown status or one inapplicable to this entity.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION The entity does not support statuses.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_return_t
dds_read_status(dds_entity_t entity, uint32_t *status, uint32_t mask);

/**
 * @brief Read and clear the selected status flags.
 * @ingroup entity_status
 * @component entity_status
 *
 * The selection is independent of the mask enabled for waitset triggering.
 * A subscriber can compute DDS_DATA_ON_READERS_STATUS from its readers'
 * DDS_DATA_AVAILABLE_STATUS flags. Taking the subscriber status leaves those
 * reader flags set, so successive takes can return DDS_DATA_ON_READERS_STATUS.
 *
 * @param[in]  entity Entity whose status to take.
 * @param[out] status Flags selected by @p mask that were set when taken.
 * @param[in]  mask   Status flags to take, or zero for all statuses.
 *
 * @retval DDS_RETCODE_OK The status flags were returned.
 * @retval DDS_RETCODE_BAD_PARAMETER The @p status pointer is NULL or @p mask
 *         contains an unknown status or one inapplicable to this entity.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION The entity does not support statuses.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_return_t
dds_take_status(dds_entity_t entity, uint32_t *status, uint32_t mask);

/**
 * @brief Read all currently set status flags.
 * @ingroup entity_status
 * @component entity_status
 *
 * Equivalent to calling @ref dds_read_status with a mask of zero.
 * Reading the flags leaves them set.
 *
 * @param[in]  entity Entity whose status to read.
 * @param[out] status Currently set status flags.
 *
 * @retval DDS_RETCODE_OK The status flags were returned.
 * @retval DDS_RETCODE_BAD_PARAMETER The @p status pointer is NULL.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION The entity does not support statuses.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_return_t
dds_get_status_changes(dds_entity_t entity, uint32_t *status);

/**
 * @anchor dds_get_status_mask
 * @brief Get the status flags enabled for waitset triggering.
 * @ingroup entity_status
 * @component entity_status
 *
 * @param[in]  entity Entity whose enabled status mask to read.
 * @param[out] mask   Where to store the enabled status mask.
 *
 * @retval DDS_RETCODE_OK The mask was returned.
 * @retval DDS_RETCODE_BAD_PARAMETER The @p mask pointer is NULL.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION The entity does not support statuses.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_return_t
dds_get_status_mask(dds_entity_t entity, uint32_t *mask);

/**
 * @anchor dds_set_status_mask
 * @brief Replace the status flags enabled for waitset triggering.
 * @ingroup entity_status
 * @component entity_status
 *
 * Every status omitted from the new mask is disabled. A mask of zero
 * disables all statuses for triggering.
 *
 * @param[in] entity Entity whose enabled status mask to replace.
 * @param[in] mask   Status flags to enable.
 *
 * @retval DDS_RETCODE_OK The mask was set.
 * @retval DDS_RETCODE_BAD_PARAMETER The mask contains an unknown status or
 *         one inapplicable to this entity.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION The entity does not support statuses.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET The entity was closed during the operation.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_return_t
dds_set_status_mask(dds_entity_t entity, uint32_t mask);

/**
 * @defgroup entity_qos (Entity QoS)
 * @ingroup entity
 * @brief Get and set QoS policies on DDS entities.
 */

/**
 * @brief Get entity QoS policies.
 * @ingroup entity_qos
 * @component entity_qos
 *
 * On success, replaces the contents of @p qos with the policies stored for
 * the entity. An ordinary topic can have sparse QoS. For example, a
 * default topic reports its data representation but omits reliability,
 * allowing readers and writers to apply their own reliability defaults. Built-in
 * topic handles return the policies configured for built-in topics.
 *
 * @param[in]  entity  Participant, publisher, subscriber, topic, reader or writer.
 * @param[out] qos     QoS object to receive the policies.
 *
 * @retval DDS_RETCODE_OK The policies were copied into @p qos.
 * @retval DDS_RETCODE_BAD_PARAMETER The @p qos pointer is NULL.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION The entity does not support QoS.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_return_t
dds_get_qos(dds_entity_t entity, dds_qos_t *qos);

/**
 * @brief Update the applicable QoS policies supplied for an entity.
 * @ingroup entity_qos
 * @component entity_qos
 *
 * Merges the supplied policies with the current QoS. Policies omitted from
 * @p qos retain their current values. Policies inapplicable to the entity
 * are ignored. Topic handles for the same name in a participant share QoS.
 *
 * @param[in] entity Participant, publisher, subscriber, topic, reader or writer.
 * @param[in] qos    QoS object containing the policy values to apply.
 *
 * @retval DDS_RETCODE_OK The update succeeded.
 * @retval DDS_RETCODE_BAD_PARAMETER The @p qos pointer is NULL or a policy value is invalid.
 * @retval DDS_RETCODE_INCONSISTENT_POLICY Policies in the resulting QoS are inconsistent.
 * @retval DDS_RETCODE_IMMUTABLE_POLICY The update would change an immutable policy.
 * @retval DDS_RETCODE_UNSUPPORTED The update would change deadline, latency
 *         budget or partition. Cyclone DDS does not support changing these
 *         policies on an enabled entity because they affect endpoint matching.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION The entity does not support QoS.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_return_t
dds_set_qos(dds_entity_t entity, const dds_qos_t * qos);

/**
 * @defgroup entity_listener (Entity Listener)
 * @ingroup entity
 * @brief Get and set the listener callbacks associated with an entity.
 */

/**
 * @brief Copy the effective listener of an entity.
 * @ingroup entity_listener
 * @component entity_listener
 *
 * The result includes callbacks set on the entity and inherited callbacks.
 *
 * @param[in]  entity   Entity whose listener to retrieve.
 * @param[out] listener Listener object to receive the callbacks and their settings.
 *
 * @retval DDS_RETCODE_OK The listener was copied.
 * @retval DDS_RETCODE_BAD_PARAMETER The @p listener pointer is NULL.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_return_t
dds_get_listener(dds_entity_t entity, dds_listener_t * listener);

/**
 * @brief Replace the listener callbacks configured on an entity.
 * @ingroup entity_listener
 * @component entity_listener
 *
 * Copies the callbacks and their settings from @p listener. For a callback
 * left unset, the entity inherits the nearest ancestor's applicable
 * callback. Passing NULL clears all locally configured callbacks and
 * allows them to be inherited. The change also updates callbacks that
 * descendants inherit from this entity.
 *
 * Callbacks for pending statuses can run before this function returns.
 * This may result in repeated notifications, particularly for
 * DDS_DATA_ON_READERS_STATUS. Each callback's reset_on_invoke setting
 * controls whether its status is reset before invocation. The entity's
 * enabled status mask governs waitset triggering.
 *
 * @param[in] entity   Entity whose listener to replace.
 * @param[in] listener Listener to copy, or NULL to clear the local callbacks.
 *
 * @retval DDS_RETCODE_OK The listener was updated.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_return_t
dds_set_listener(dds_entity_t entity, const dds_listener_t * listener);

/*
  Creation functions for various entities. Creating a subscriber or
  publisher is optional: if one creates a reader or writer directly
  as a descendant of a participant for a normal topic, an implicit
  subscriber or publisher is created as the parent of that reader or
  writer.

  QoS default values are those of the DDS specification, but the
  inheritance rules are different:

    * publishers and subscribers inherit from the participant QoS
    * readers and writers always inherit from the topic QoS
    * the QoS's present in the "qos" parameter override the inherited values
*/

/**
 * @defgroup domain (Domain)
 * @ingroup dds
 */

/**
 * @defgroup domain_participant (DomainParticipant)
 * @ingroup domain
 */

/**
 * @brief Creates a new instance of a DDS participant in a domain
 * @ingroup domain_participant
 * @component participant
 *
 * This function creates a new participant in the specified domain id,
 * implicitly creating a domain entity if one doesn't exist for the specified
 * domain id. The domain entity can exist as a consequence of the existence of
 * another participant in it, or because it was explicitly created using
 * @ref dds_create_domain or @ref dds_create_domain_with_rawconfig. If the domain
 * entity has not been created yet, a new domain entity is created using a
 * configuration taken from the CYCLONEDDS_URI environment variable.
 *
 * A domain implicitly created by this function is deleted automatically when
 * its last child is deleted. A domain-owned guard condition or waitset can
 * therefore keep an implicit domain alive after its last participant is
 * deleted. An explicitly created domain remains until it or the library
 * entity is deleted.
 *
 * The domain id may be specified as DDS_DOMAIN_DEFAULT, in which case the behaviour
 * depends on whether some domain entity already exists or not.  If there is at least one,
 * the one with the lowest id will be used.  If there are none, one is created with the
 * domain id taken from the first domain id specified in the configuration file (i.e.,
 * with a Domain element with the id attribute not "any"), and if no domain id is
 * specified in the configuration file, domain id 0 is used.
 *
 * The domain configuration is constructed by amending the default configuration with the
 * Domain configuration (fragments) for which the domain id is specified as "any" and those
 * for which the domain id matches the specified domain id (not DDS_DOMAIN_DEFAULT) in the
 * order in which they are encountered in the configuration.
 *
 * @param[in]  domain The domain in which to create the participant.
 * @param[in]  qos The QoS to set on the new participant (can be NULL).
 * @param[in]  listener Any listener functions associated with the new participant (can be NULL).
 *
 * @returns A valid participant handle or an error code.
 *
 * @retval >0
 *             A valid participant handle.
 * @retval DDS_RETCODE_BAD_PARAMETER A participant QoS value is invalid,
 *         including a liveliness kind other than DDS_LIVELINESS_AUTOMATIC.
 * @retval DDS_RETCODE_ERROR Domain initialization or participant creation
 *         failed. This includes invalid configuration, security failures
 *         and reaching the configured maximum number of participants.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES A required domain resource or entity
 *         handle could not be allocated.
 * @retval DDS_RETCODE_UNSUPPORTED The new domain requests more PSMX
 *         instances than the implementation supports.
 */
DDS_EXPORT dds_entity_t
dds_create_participant(
  const dds_domainid_t domain,
  const dds_qos_t *qos,
  const dds_listener_t *listener);

/**
 * @brief Creates a domain with a given configuration
 * @ingroup domain
 * @component domain
 *
 * Creates an explicit domain. Its lifetime ends when @ref dds_delete is
 * called on the domain or on DDS_CYCLONEDDS_HANDLE.
 *
 * It will not be created if a domain with the given domain id already exists.
 * The existing domain could have been created implicitly by
 * @ref dds_create_participant, or explicitly by a previous call to this
 * function or @ref dds_create_domain_with_rawconfig.
 *
 * The domain configuration is constructed by amending the default configuration with the
 * Domain configuration (fragments) for which the domain id is specified as "any" and those
 * for which the domain id matches the specified domain id in the order in which they are
 * encountered in the configuration.
 *
 * Using NULL or "" as config will create a domain with default settings.
 *
 * @param[in]  domain The domain to be created. DDS_DOMAIN_DEFAULT is not allowed.
 * @param[in]  config A configuration string containing file names and/or XML fragments representing the configuration.
 *
 * @returns A valid entity handle or an error code.
 *
 * @retval >0 The domain handle.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The domain ID is DDS_DOMAIN_DEFAULT.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The domain already existed and cannot be created again.
 * @retval DDS_RETCODE_ERROR
 *             The domain configuration could not be applied or domain
 *             initialization failed.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES A required domain resource or entity
 *         handle could not be allocated.
 * @retval DDS_RETCODE_UNSUPPORTED The configuration requests more PSMX
 *         instances than the implementation supports.
 */
DDS_EXPORT dds_entity_t
dds_create_domain(const dds_domainid_t domain, const char *config);

/**
 * @brief Creates a domain with a given configuration, specified as an
 * initializer (unstable interface)
 * @ingroup domain
 * @component domain
 * @unstable
 *
 * Creates an explicit domain from a configuration structure.
 * ddsi_config_init_default() in dds/ddsi/ddsi_config.h initializes that
 * structure with the defaults. The domain's lifetime ends when
 * @ref dds_delete is called on the domain or on DDS_CYCLONEDDS_HANDLE.
 *
 * It will not be created if a domain with the given domain id already exists.
 * The existing domain could have been created implicitly by
 * @ref dds_create_participant, or explicitly by a previous call to this
 * function or @ref dds_create_domain.
 *
 * The supplied domain ID takes precedence over the ID in the configuration.
 *
 * @param[in]  domain The domain to be created. DDS_DOMAIN_DEFAULT is not allowed.
 * @param[in]  config A configuration initializer.  The lifetime of any pointers
 *             in config must be at least that of the lifetime of the domain.
 *
 * @returns A valid entity handle or an error code.
 *
 * @retval >0 The domain handle.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The domain ID is DDS_DOMAIN_DEFAULT or @p config is NULL.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The domain already existed and cannot be created again.
 * @retval DDS_RETCODE_ERROR
 *             The domain configuration could not be applied or domain
 *             initialization failed.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES A required domain resource or entity
 *         handle could not be allocated.
 * @retval DDS_RETCODE_UNSUPPORTED The configuration requests more PSMX
 *         instances than the implementation supports.
 */
DDS_EXPORT dds_entity_t
dds_create_domain_with_rawconfig(const dds_domainid_t domain, const struct ddsi_config *config);

/**
 * @brief Get the direct parent of an entity.
 * @ingroup entity
 * @component entity_relations
 *
 * A participant's parent is its domain. A domain's parent is
 * DDS_CYCLONEDDS_HANDLE. For a reader or writer created directly on a
 * participant, this function returns the subscriber or publisher created
 * for it. A reader of a built-in topic belongs to the built-in subscriber.
 *
 * @param[in] entity Entity whose parent to return.
 *
 * @retval >0 The direct parent handle.
 * @retval DDS_ENTITY_NIL The entity is DDS_CYCLONEDDS_HANDLE.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_entity_t
dds_get_parent(dds_entity_t entity);

/**
 * @brief Get the participant to which an entity belongs.
 * @ingroup entity
 * @component entity_relations
 *
 * @param[in] entity Participant or entity whose participant to return.
 *
 * @retval >0 The participant handle. For a participant this is @p entity itself.
 * @retval DDS_ENTITY_NIL The entity has no participant ancestor, for example,
 *         a domain or a waitset created on a domain.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_entity_t
dds_get_participant(dds_entity_t entity);

/**
 * @brief Get the direct children of an entity.
 * @ingroup entity
 * @component entity_relations
 *
 * Includes implicit publishers and subscribers and the participant's built-in
 * subscriber once created. Internal built-in topic entities are omitted.
 *
 * The return value counts all children, including those that do not fit
 * in the supplied array. In that case, the call fills the array with the
 * handles of @p size arbitrarily selected children. Unused array elements
 * retain their previous values.
 *
 * @param[in]  entity   Entity whose children to enumerate.
 * @param[out] children Array to receive handles, or NULL to obtain only the count.
 * @param[in]  size     Capacity of the array in handles, or zero if @p children is NULL.
 *
 * @retval >=0 Total number of children found.
 * @retval DDS_RETCODE_BAD_PARAMETER The array is NULL with a nonzero size, or
 *         the array is non-NULL with a size of zero or greater than INT32_MAX.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_return_t
dds_get_children(dds_entity_t entity, dds_entity_t *children, size_t size);

/**
 * @brief Get the domain ID associated with an entity.
 * @ingroup entity
 * @component entity_relations
 *
 * @param[in]  entity Entity whose domain ID to return.
 * @param[out] id     Where to store the domain ID. Entities outside a domain,
 *                    including library-owned waitsets, return DDS_DOMAIN_DEFAULT here.
 *
 * @retval DDS_RETCODE_OK The domain ID was returned.
 * @retval DDS_RETCODE_BAD_PARAMETER The @p id pointer is NULL.
 * @see @ref entity_handle_errors
 */
DDS_EXPORT dds_return_t
dds_get_domainid(dds_entity_t entity, dds_domainid_t *id);

/**
 * @brief Get participants of a domain.
 * @ingroup domain
 * @component participant
 *
 * Returns the total number of local participants in the given domain,
 * including those that do not fit in the supplied array.
 *
 * If more participants are found than fit in the supplied array, the call
 * fills it with the handles of @p size arbitrarily selected participants.
 * If none are found and an array is supplied, its first element is set to
 * zero. All other unused elements retain their previous values.
 * On error, all array elements retain their previous values.
 *
 * @param[in]  domain_id    The actual domain ID. An unknown ID or DDS_DOMAIN_DEFAULT
 *                          yields an empty result.
 * @param[out] participants Array that receives participant handles, or NULL to obtain the count.
 * @param[in]  size         Size of the pre-allocated participant's list.
 *
 * @returns Number of participants found or a negative error code.
 *
 * @retval >=0
 *             Number of participants found.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The array is NULL with a nonzero size, or the array is non-NULL
 *             with a size of zero or at least INT32_MAX.
 */
DDS_EXPORT dds_return_t
dds_lookup_participant(
  dds_domainid_t domain_id,
  dds_entity_t *participants,
  size_t size);

/**
 * @defgroup topic (Topic)
 * @ingroup dds
 */

/**
 * @brief Creates a topic using an IDL-generated descriptor.
 * @ingroup topic
 * @component topic
 *
 * Topics of the same name in one participant share QoS and may have different types.
 * Each successful call creates a distinct topic handle.
 *
 * @param[in] participant Participant in which to create the topic.
 * @param[in] descriptor Topic descriptor supplying the type name and serialization metadata.
 * @param[in] name Topic name.
 * @param[in] qos Topic QoS, or NULL for defaults.
 * @param[in] listener Listener, or NULL.
 *
 * @retval >0 A new topic handle.
 * @retval DDS_RETCODE_BAD_PARAMETER Invalid name, type description or QoS policy value,
 *         including an invalid data representation.
 * @retval DDS_RETCODE_INCONSISTENT_POLICY Inconsistent policies, or different QoS
 *         on an existing topic of the same name in this participant.
 * @retval DDS_RETCODE_UNSUPPORTED XML data representation was requested.
 * @retval DDS_RETCODE_NOT_ALLOWED_BY_SECURITY Access control denied topic creation.
 * @retval DDS_RETCODE_ERROR A PSMX instance failed to create the topic.
 * @return Entity lookup errors as described in @ref entity_handle_errors.
 */
DDS_EXPORT dds_entity_t
dds_create_topic(
  dds_entity_t participant,
  const dds_topic_descriptor_t *descriptor,
  const char *name,
  const dds_qos_t *qos,
  const dds_listener_t *listener);


/**
 * @brief Indicates that the library defines the dds_create_topic_sertype function
 * @ingroup topic
 * Introduced to help with the change from sertopic to sertype. If you are using
 * a modern CycloneDDS version you will not need this.
 */
#define DDS_HAS_CREATE_TOPIC_SERTYPE 1

/**
 * @brief Creates a topic using a caller-supplied serialization type.
 * @ingroup topic
 * @component topic
 *
 * On success this function consumes one reference to the supplied sertype and
 * sets `*sertype` to the type used by the topic. This is a borrowed pointer,
 * valid while the topic exists. It can point to an equivalent type already
 * registered in the domain. On error the caller keeps its reference and
 * `*sertype` is unchanged. A late failure can leave the supplied type
 * registered in this domain. Retry in the same domain or release the
 * reference before deleting the domain.
 *
 * Topics of the same name in one participant share QoS and may have different types.
 *
 * @param[in] participant Participant in which to create the topic.
 * @param[in] name Topic name.
 * @param[in,out] sertype Reference to the supplied serialization type.
 * @param[in] qos Topic QoS, or NULL for defaults.
 * @param[in] listener Listener, or NULL.
 * @param[in] sedp_plist Reserved. Pass NULL.
 *
 * @retval >0 A new topic handle.
 * @retval DDS_RETCODE_BAD_PARAMETER Invalid name, type description or QoS policy value,
 *         including an invalid data representation.
 * @retval DDS_RETCODE_INCONSISTENT_POLICY Inconsistent policies, or different QoS
 *         on an existing topic of the same name in this participant.
 * @retval DDS_RETCODE_UNSUPPORTED XML data representation was requested.
 * @retval DDS_RETCODE_NOT_ALLOWED_BY_SECURITY Access control denied topic creation.
 * @retval DDS_RETCODE_ERROR A PSMX instance failed to create the topic.
 * @return Entity lookup errors as described in @ref entity_handle_errors.
 */
DDS_EXPORT dds_entity_t
dds_create_topic_sertype (
  dds_entity_t participant,
  const char *name,
  struct ddsi_sertype **sertype,
  const dds_qos_t *qos,
  const dds_listener_t *listener,
  const struct ddsi_plist *sedp_plist);

/**
 * @brief Find a topic by name and optional type information.
 * @ingroup topic
 * @component topic
 *
 * Searches the given participant, all local participants in its domain, or
 * the local domain and discovered remote topics, according to @p scope.
 * Local searches do not send type lookup requests. A global search can
 * request the selected remote type and its dependencies. If @p type_info
 * is NULL, any matching definition may be selected.
 *
 * On success, the caller owns a new topic handle in @p participant and must
 * release it with @ref dds_delete. Errors from creating that topic or
 * resolving its descriptor are passed through.
 *
 * @param[in] scope       DDS_FIND_SCOPE_PARTICIPANT, DDS_FIND_SCOPE_LOCAL_DOMAIN
 *                        or DDS_FIND_SCOPE_GLOBAL.
 * @param[in] participant Participant in which to create the found topic.
 * @param[in] name        Name of the topic to find.
 * @param[in] type_info   Type to match, or NULL for a name-only search.
 * @param[in] timeout     Maximum duration to wait for the topic and its type.
 *                        Pass zero to poll or DDS_INFINITY to wait indefinitely.
 *
 * @retval >0 The new topic handle.
 * @retval 0 A zero-timeout search found no matching topic definition.
 * @retval DDS_RETCODE_TIMEOUT The wait expired, or a zero-timeout search
 *         found a remote topic whose type was still unresolved.
 * @retval DDS_RETCODE_BAD_PARAMETER The name, scope or type information is
 *         invalid, the entity is not a participant, or the timeout is negative.
 *         Global scope and non-NULL type information require topic discovery
 *         support in the build.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET The selected remote definition has
 *         no complete type ID, its type is no longer known, or a type lookup
 *         request could not be sent.
 * @see @ref entity_handle_errors, @ref dds_create_topic, @ref dds_create_topic_descriptor
 */
DDS_EXPORT dds_entity_t
dds_find_topic (dds_find_scope_t scope, dds_entity_t participant, const char *name, const dds_typeinfo_t *type_info, dds_duration_t timeout);

/**
 * @brief Find a topic by name.
 * @component topic
 * @ingroup deprecated
 * @deprecated Use @ref dds_find_topic with NULL type information.
 *
 * Equivalent to calling @ref dds_find_topic with the same scope, participant,
 * name and timeout, and NULL for the type information. It has the same return
 * codes and transfers the same ownership of the returned topic handle.
 *
 * @param[in] scope       Participant, local-domain or global search scope.
 * @param[in] participant Participant in which to create the found topic.
 * @param[in] name        Name of the topic to find.
 * @param[in] timeout     Maximum duration to wait, or zero to poll.
 *
 * @returns A new topic handle, zero for a poll with no match, or a negative
 *          error code as described by @ref dds_find_topic.
 */
DDS_DEPRECATED_EXPORT dds_entity_t
dds_find_topic_scoped (dds_find_scope_t scope, dds_entity_t participant, const char *name, dds_duration_t timeout);


/**
 * @ingroup topic
 * @component topic
 * @brief Creates topic descriptor for the provided type_info
 *
 * @param[in]  scope        The scope used to find the type: DDS_FIND_SCOPE_LOCAL_DOMAIN or DDS_FIND_SCOPE_GLOBAL. In case DDS_FIND_SCOPE_GLOBAL is used, a type lookup request will be sent to other nodes.
 * @param[in]  participant  The handle of the participant.
 * @param[in]  type_info    The type (dds_typeinfo_t) of the topic to find.
 * @param[in]  timeout      The timeout for waiting for the type to become available
 * @param[out] descriptor   Receives the newly allocated descriptor on success. On error, `*descriptor` is unchanged.
 *                          Use @ref dds_delete_topic_descriptor to free the descriptor.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The topic descriptor has been succesfully created.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             Type_info or descriptor parameter not provided, invalid entity (not a participant) or scope invalid.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The participant or the type_id was not found.
 * @retval DDS_RETCODE_TIMEOUT
 *             Type was not resolved within the provided timeout
 * @retval DDS_RETCODE_UNSUPPORTED
 *             Cyclone DDS built without type discovery
 *             (cf. DDS_HAS_TYPE_DISCOVERY)
 */
DDS_EXPORT dds_return_t
dds_create_topic_descriptor (dds_find_scope_t scope, dds_entity_t participant, const dds_typeinfo_t *type_info, dds_duration_t timeout, dds_topic_descriptor_t **descriptor);

/**
 * @ingroup topic
 * @component topic
 * @brief Delete memory allocated to the provided topic descriptor
 *
 * @param[in] descriptor - Pointer to a dds_topic_descriptor_t
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The topic descriptor has been succesfully deleted.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             No descriptor provided
 * @retval DDS_RETCODE_UNSUPPORTED
 *             Cyclone DDS built without type discovery
 *             (cf. DDS_HAS_TYPE_DISCOVERY)
 */
DDS_EXPORT dds_return_t
dds_delete_topic_descriptor (dds_topic_descriptor_t *descriptor);

/**
 * @brief Returns the name of a given topic.
 * @ingroup topic
 * @component topic
 *
 * @param[in]  topic  The topic.
 * @param[out] name   Buffer for the topic name. On error, the buffer is unchanged.
 * @param[in]  size   Number of bytes available in the buffer.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @return Actual length of topic name (name is truncated if return value >= size) or error
 */
DDS_EXPORT dds_return_t
dds_get_name(dds_entity_t topic, char *name, size_t size);

/**
 * @brief Returns the type name of a given topic.
 * @ingroup topic
 * @component topic
 *
 * @param[in]  topic  The topic.
 * @param[out] name   Buffer for the topic type name. On error, the buffer is unchanged.
 * @param[in]  size   Number of bytes available in the buffer.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @return Actual length of type name (name is truncated if return value >= size) or error
 */
DDS_EXPORT dds_return_t
dds_get_type_name(dds_entity_t topic, char *name, size_t size);

/**
 * @defgroup topic_filter (Topic filters)
 * @ingroup topic
 * Topic filter functions.
 * @warning part of the Unstable API: no guarantee that any
 *          of this will be maintained for backwards compatibility.
 *
 * Sampleinfo is all zero when filtering in a write call (i.e., writer created
 * using a filtered topic, which one perhaps shouldn't be doing), otherwise it
 * has as much filled in correctly as is possible given the context and the rest
 * fixed:
 *    - sample_state         DDS_NOT_READ_SAMPLE_STATE;
 *    - publication_handle   set to writer's instance handle
 *    - source_timestamp     set to source timestamp of sample
 *    - ranks                0
 *    - valid_data           true
 *    - instance_handle      set to instance handle of existing instance if the
 *                           sample matches an existing instance, otherwise to what
 *                           the instance handle will be if it passes the filter
 *    - view_state           set to instance view state if sample being filtered
 *                           matches an existing instance, NEW if not
 *    - instance_state       set to instance state if sample being filtered
 *                           matches an existing instance, NEW if not
 *    - generation counts    set to instance's generation counts if the sample
 *                           matches an existing instance instance, 0 if not
 */

/**
 * @anchor dds_topic_filter_sample_fn
 * @brief Topic filter function that only needs to look at the sample.
 * @ingroup topic_filter
 * @warning Unstable API
 * @unstable
 */
typedef bool (*dds_topic_filter_sample_fn) (const void * sample);

/**
 * @anchor dds_topic_filter_sample_arg_fn
 * @brief Topic filter function that only needs to look at the sample and a custom argument.
 * @ingroup topic_filter
 * @warning Unstable API
 */
typedef bool (*dds_topic_filter_sample_arg_fn) (const void * sample, void * arg);

/**
 * @anchor dds_topic_filter_sampleinfo_arg_fn
 * @brief Topic filter function that only needs to look at the sampleinfo and a custom argument.
 * @ingroup topic_filter
 * @warning Unstable API
 */
typedef bool (*dds_topic_filter_sampleinfo_arg_fn) (const dds_sample_info_t * sampleinfo, void * arg);

/**
 * @anchor dds_topic_filter_sample_sampleinfo_arg_fn
 * @brief Topic filter function that needs to look at the sample, the sampleinfo and a custom argument.
 * @ingroup topic_filter
 * @warning Unstable API
 */
typedef bool (*dds_topic_filter_sample_sampleinfo_arg_fn) (const void * sample, const dds_sample_info_t * sampleinfo, void * arg);

/**
 * @anchor dds_topic_filter_fn
 * @brief See \ref dds_topic_filter_sample_fn
 * @ingroup topic_filter
 * @warning Unstable API
 */
typedef dds_topic_filter_sample_fn dds_topic_filter_fn;

/**
 * @anchor dds_topic_filter_arg_fn
 * @brief See \ref dds_topic_filter_sample_arg_fn
 * @ingroup topic_filter
 * @warning Unstable API
 */
typedef dds_topic_filter_sample_arg_fn dds_topic_filter_arg_fn;

/**
 * @brief Topic filter mode;
 * @ingroup topic_filter
 * @warning Unstable API
 */
enum dds_topic_filter_mode {
  DDS_TOPIC_FILTER_NONE,                  /**< Can be used to reset topic filter */
  DDS_TOPIC_FILTER_SAMPLE,                /**< Use with \ref dds_topic_filter_sample_fn */
  DDS_TOPIC_FILTER_SAMPLE_ARG,            /**< Use with \ref dds_topic_filter_sample_arg_fn */
  DDS_TOPIC_FILTER_SAMPLEINFO_ARG,        /**< Use with \ref dds_topic_filter_sampleinfo_arg_fn */
  DDS_TOPIC_FILTER_SAMPLE_SAMPLEINFO_ARG, /**< Use with \ref dds_topic_filter_sample_sampleinfo_arg_fn */
};

/**
 * @brief Union of all filter function types;
 * @ingroup topic_filter
 * @warning Unstable API
*/
union dds_topic_filter_function_union {
  dds_topic_filter_sample_fn sample; /**< Use with mode dds_topic_filter_mode::DDS_TOPIC_FILTER_SAMPLE */
  dds_topic_filter_sample_arg_fn sample_arg; /**< Use with mode dds_topic_filter_mode::DDS_TOPIC_FILTER_SAMPLE_ARG */
  dds_topic_filter_sampleinfo_arg_fn sampleinfo_arg; /**< Use with mode dds_topic_filter_mode::DDS_TOPIC_FILTER_SAMPLEINFO_ARG */
  dds_topic_filter_sample_sampleinfo_arg_fn sample_sampleinfo_arg; /**< Use with mode dds_topic_filter_mode::DDS_TOPIC_FILTER_SAMPLE_SAMPLEINFO_ARG */
};

/**
 * @brief Full topic filter container;
 * @ingroup topic_filter
 * @warning Unstable API
 */
struct dds_topic_filter {
  enum dds_topic_filter_mode mode;         /**< Provide a mode */
  union dds_topic_filter_function_union f; /**< Provide a filter function */
  void *arg;                               /**< Provide an argument, can be NULL */
};

/**
 * @anchor dds_set_topic_filter_and_arg
 * @brief Sets a filter and filter argument on a topic.
 * @ingroup topic_filter
 * @component topic
 * @warning Unstable API
 * To be replaced by proper filtering on readers.
 *
 * Not thread-safe with respect to data being read/written using readers/writers
 * using this topic.  Be sure to create a topic entity specific to the reader you
 * want to filter, then set the filter function, and only then create the reader.
 * And don't change it unless you know there are no concurrent writes.
 *
 * @param[in]  topic   The topic on which the content filter is set.
 * @param[in]  filter  The filter function used to filter topic samples.
 * @param[in]  arg     Argument for the filter function.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK  Filter set successfully
 * @retval DDS_RETCODE_BAD_PARAMETER  The topic handle is invalid
*/
DDS_EXPORT dds_return_t
dds_set_topic_filter_and_arg(
  dds_entity_t topic,
  dds_topic_filter_arg_fn filter,
  void *arg);

/**
 * @anchor dds_set_topic_filter_extended
 * @brief Sets a filter and filter argument on a topic.
 * @ingroup topic_filter
 * @component topic
 * @warning Unstable API
 * To be replaced by proper filtering on readers.
 *
 * Not thread-safe with respect to data being read/written using readers/writers
 * using this topic.  Be sure to create a topic entity specific to the reader you
 * want to filter, then set the filter function, and only then create the reader.
 * And don't change it unless you know there are no concurrent writes.
 *
 * @param[in]  topic   The topic on which the content filter is set.
 * @param[in]  filter  The filter specification.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK  Filter set successfully
 * @retval DDS_RETCODE_BAD_PARAMETER  The topic handle is invalid
*/
DDS_EXPORT dds_return_t
dds_set_topic_filter_extended(
  dds_entity_t topic,
  const struct dds_topic_filter *filter);

/**
 * @brief Gets the filter for a topic.
 * @ingroup topic_filter
 * @component topic
 * @warning Unstable API
 *
 * To be replaced by proper filtering on readers
 *
 * @param[in]  topic  The topic from which to get the filter.
 * @param[out] fn     The topic filter function (fn may be NULL).
 * @param[out] arg    Filter function argument (arg may be NULL).
 *
 * @retval DDS_RETCODE_OK  Filter set successfully
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET  Filter is not of "none" or "sample_arg"
 * @retval DDS_RETCODE_BAD_PARAMETER  The topic handle is invalid
 */
DDS_EXPORT dds_return_t
dds_get_topic_filter_and_arg (
  dds_entity_t topic,
  dds_topic_filter_arg_fn *fn,
  void **arg);

/**
 * @brief Gets the filter for a topic.
 * @ingroup topic_filter
 * @component topic
 * @warning Unstable API
 *
 * To be replaced by proper filtering on readers
 *
 * @param[in]  topic  The topic from which to get the filter.
 * @param[out] filter The topic filter specification.
 *
 * @retval DDS_RETCODE_OK  Filter set successfully
 * @retval DDS_RETCODE_BAD_PARAMETER  The topic handle is invalid
 */
DDS_EXPORT dds_return_t
dds_get_topic_filter_extended (
  dds_entity_t topic,
  struct dds_topic_filter *filter);

/**
 * @defgroup subscriber (Subscriber)
 * @ingroup subscription
 * DOC_TODO The Subscriber is a DDS Entity
 */

/**
 * @brief Creates a new instance of a DDS subscriber
 * @ingroup subscriber
 * @component subscriber
 *
 * The subscriber created by this function is explicit. It is not deleted
 * automatically merely because it has no readers; it must be deleted
 * explicitly, or by deleting the participant that contains it.
 *
 * @param[in]  participant The participant on which the subscriber is being created.
 * @param[in]  qos         The QoS to set on the new subscriber (can be NULL).
 * @param[in]  listener    Any listener functions associated with the new subscriber (can be NULL).
 *
 * @returns A valid subscriber handle or an error code.
 *
 * @retval >0
 *             A valid subscriber handle.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the parameters is invalid.
 */
DDS_EXPORT dds_entity_t
dds_create_subscriber(
  dds_entity_t participant,
  const dds_qos_t *qos,
  const dds_listener_t *listener);


/**
 * @defgroup publication (Publication)
 * @ingroup dds
 * DOC_TODO This contains the definitions regarding publication of data.
 */

/**
 * @defgroup publisher (Publisher)
 * @ingroup publication
 * DOC_TODO The Publisher is a DDS Entity
 */

/**
 * @brief Creates a new instance of a DDS publisher
 * @ingroup publisher
 * @component publisher
 *
 * The publisher created by this function is explicit. It is not deleted
 * automatically merely because it has no writers; it must be deleted
 * explicitly, or by deleting the participant that contains it.
 *
 * @param[in]  participant The participant to create a publisher for.
 * @param[in]  qos         The QoS to set on the new publisher (can be NULL).
 * @param[in]  listener    Any listener functions associated with the new publisher (can be NULL).
 *
 * @returns A valid publisher handle or an error code.
 *
 * @retval >0
 *            A valid publisher handle.
 * @retval DDS_RETCODE_ERROR
 *            An internal error has occurred.
 */
/* TODO: Check list of error codes is complete. */
DDS_EXPORT dds_entity_t
dds_create_publisher(
  dds_entity_t participant,
  const dds_qos_t *qos,
  const dds_listener_t *listener);

/**
 * @brief Suspends the publications of the Publisher
 * @ingroup publisher
 * @component publisher
 *
 * This operation is a hint to the Service so it can optimize its performance by e.g., collecting
 * modifications to DDS writers and then batching them. The Service is not required to use the hint.
 *
 * Every invocation of this operation must be matched by a corresponding call to @see dds_resume
 * indicating that the set of modifications has completed.
 *
 * @param[in]  publisher The publisher for which all publications will be suspended.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             Publications suspended successfully.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The pub parameter is not a valid publisher.
 * @retval DDS_RETCODE_UNSUPPORTED
 *             Operation is not supported.
 */
DDS_EXPORT dds_return_t
dds_suspend(dds_entity_t publisher);

/**
 * @brief Resumes the publications of the Publisher
 * @ingroup publisher
 * @component publisher
 *
 * This operation is a hint to the Service to indicate that the application has
 * completed changes initiated by a previous dds_suspend(). The Service is not
 * required to use the hint.
 *
 * The call to resume_publications must match a previous call to @see suspend_publications.
 *
 * @param[in]  publisher The publisher for which all publications will be resumed.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             Publications resumed successfully.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The pub parameter is not a valid publisher.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             No previous matching dds_suspend().
 * @retval DDS_RETCODE_UNSUPPORTED
 *             Operation is not supported.
 */
DDS_EXPORT dds_return_t
dds_resume(dds_entity_t publisher);

/**
 * @brief Waits at most for the duration timeout for acks for data in the publisher or writer.
 * @ingroup publication
 * @component publisher
 *
 * This operation blocks the calling thread until either all data written by the publisher
 * or writer is acknowledged by all matched reliable reader entities, or else the duration
 * specified by the timeout parameter elapses, whichever happens first.
 *
 * @param[in]  publisher_or_writer Publisher or writer whose acknowledgments must be waited for
 * @param[in]  timeout             How long to wait for acknowledgments before time out
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             All acknowledgments successfully received with the timeout.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The publisher_or_writer is not a valid publisher or writer.
 * @retval DDS_RETCODE_TIMEOUT
 *             Timeout expired before all acknowledgments from reliable reader entities were received.
 * @retval DDS_RETCODE_UNSUPPORTED
 *             Operation is not supported.
 */
DDS_EXPORT dds_return_t
dds_wait_for_acks(dds_entity_t publisher_or_writer, dds_duration_t timeout);


/**
 * @defgroup reader (Reader)
 * @ingroup subscription
 * DOC_TODO The reader is a DDS Entity
 */

/**
 * @brief Creates a new instance of a DDS reader.
 * @ingroup reader
 * @component reader
 *
 * When a participant is used to create a reader for a normal topic, an
 * implicit subscriber is created as the reader's parent. This implicit
 * subscriber is deleted automatically when its last child is deleted, which is
 * when the created reader is deleted unless additional children were added to
 * the implicit subscriber. It is also deleted as part of the recursive deletion
 * of the participant.
 *
 * The topic may be a normal topic handle or one of the built-in topic pseudo
 * handles:
 * - @ref DDS_BUILTIN_TOPIC_DCPSPARTICIPANT reads @ref dds_builtintopic_participant_t samples.
 * - @ref DDS_BUILTIN_TOPIC_DCPSTOPIC reads @ref dds_builtintopic_topic_t samples.
 * - @ref DDS_BUILTIN_TOPIC_DCPSPUBLICATION reads @ref dds_builtintopic_endpoint_t samples for writers.
 * - @ref DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION reads @ref dds_builtintopic_endpoint_t samples for readers.
 *
 * Creating a reader for a built-in topic uses the participant's built-in
 * subscriber. If a subscriber is provided, its participant is used to locate
 * that built-in subscriber; the provided subscriber itself does not become the
 * parent of the reader. The built-in subscriber is shared by built-in topic
 * readers for that participant and is deleted when the participant is deleted,
 * not when an individual built-in topic reader is deleted. The built-in topic
 * handles are pseudo handles, not normal topic entities: @ref dds_get_topic on
 * such a reader returns the same pseudo handle, and the internal topic entity
 * cannot be used directly to create another reader.
 *
 * Reader QoS for built-in topics must be compatible with the corresponding
 * built-in topic writer and must not impose resource limits, because otherwise
 * discovery data could be rejected. When @p qos is NULL, Cyclone DDS supplies a
 * compatible default built-in topic QoS. The DCPSTopic built-in topic is only
 * available when Cyclone DDS was built with topic discovery support; otherwise
 * creating a reader for @ref DDS_BUILTIN_TOPIC_DCPSTOPIC returns
 * @ref DDS_RETCODE_UNSUPPORTED.
 *
 * @param[in]  participant_or_subscriber The participant or subscriber on which the reader is being created.
 * @param[in]  topic                     The topic to read, or a built-in topic pseudo handle.
 * @param[in]  qos                       The QoS to set on the new reader (can be NULL).
 * @param[in]  listener                  Any listener functions associated with the new reader (can be NULL).
 *
 * @returns A valid reader handle or an error code.
 *
 * @retval >0
 *            A valid reader handle.
 * @retval DDS_RETCODE_ERROR
 *            An internal error occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            One of the parameters is invalid, or the participant and topic do
 *            not belong to the same domain participant.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *            participant_or_subscriber is neither a participant nor a subscriber,
 *            or the operation is otherwise inappropriate for the supplied entity.
 * @retval DDS_RETCODE_INCONSISTENT_POLICY
 *            The requested QoS is inconsistent, including a built-in topic reader
 *            QoS that is incompatible with the built-in topic writer or has
 *            finite resource limits.
 * @retval DDS_RETCODE_UNSUPPORTED
 *            The requested built-in topic requires a feature that is not enabled
 *            in this build, such as topic discovery for
 *            @ref DDS_BUILTIN_TOPIC_DCPSTOPIC.
 * @retval DDS_RETCODE_NOT_ALLOWED_BY_SECURITY
 *            The configured DDS Security access control plugin denied creation
 *            of the reader.
 */
DDS_EXPORT dds_entity_t
dds_create_reader(
  dds_entity_t participant_or_subscriber,
  dds_entity_t topic,
  const dds_qos_t *qos,
  const dds_listener_t *listener);

/**
 * @brief Creates a new instance of a DDS reader with a custom history cache.
 * @ingroup reader
 * @component reader
 *
 * This function follows the same parent-selection rules as @ref dds_create_reader.
 * When a participant is used to create a reader for a normal topic, an implicit
 * subscriber is created as the reader's parent and is deleted automatically
 * when its last child is deleted, or as part of the recursive deletion of the
 * participant. A reader for a built-in topic uses the participant's shared
 * built-in subscriber instead; that subscriber is deleted when the participant
 * is deleted.
 *
 * @param[in]  participant_or_subscriber The participant or subscriber on which the reader is being created.
 * @param[in]  topic                     The topic to read, or a built-in topic pseudo handle.
 * @param[in]  qos                       The QoS to set on the new reader (can be NULL).
 * @param[in]  listener                  Any listener functions associated with the new reader (can be NULL).
 * @param[in]  rhc                       Reader history cache to use, reader becomes the owner
 *
 * @returns A valid reader handle or an error code.
 *
 * @retval >0
 *            A valid reader handle.
 * @retval DDS_RETCODE_ERROR
 *            An internal error occurred.
 * @see dds_create_reader for the full set of possible return codes.
 */
DDS_EXPORT dds_entity_t
dds_create_reader_rhc(
  dds_entity_t participant_or_subscriber,
  dds_entity_t topic,
  const dds_qos_t *qos,
  const dds_listener_t *listener,
  struct dds_rhc *rhc);

/**
 * @brief Wait until reader receives all historic data
 * @ingroup reader
 * @component reader
 *
 * The operation blocks the calling thread until either all "historical" data is
 * received, or else the duration specified by the max_wait parameter elapses, whichever happens
 * first. A return value of 0 indicates that all the "historical" data was received; a return
 * value of TIMEOUT indicates that max_wait elapsed before all the data was received.
 *
 * @param[in]  reader    The reader on which to wait for historical data.
 * @param[in]  max_wait  How long to wait for historical data before time out.
 *
 * @returns a status, 0 on success, TIMEOUT on timeout or a negative value to indicate error.
 *
 * DOC_TODO: Complete list of error codes
 */
DDS_EXPORT dds_return_t
dds_reader_wait_for_historical_data(
  dds_entity_t reader,
  dds_duration_t max_wait);

/**
 * @defgroup writer (Writer)
 * @ingroup publication
 * DOC_TODO The writer is a DDS Entity
 */

/**
 * @brief Creates a new instance of a DDS writer.
 * @ingroup writer
 * @component writer
 *
 * When a participant is used to create a writer, an implicit publisher is
 * created as the writer's parent. This implicit publisher is deleted
 * automatically when its last child is deleted, which is when the created
 * writer is deleted unless additional children were added to the implicit
 * publisher. It is also deleted as part of the recursive deletion of the
 * participant.
 *
 * @param[in]  participant_or_publisher The participant or publisher on which the writer is being created.
 * @param[in]  topic The topic to write.
 * @param[in]  qos The QoS to set on the new writer (can be NULL).
 * @param[in]  listener Any listener functions associated with the new writer (can be NULL).
 *
 * @returns A valid writer handle or an error code.
 *
 * @retval >0
 *              A valid writer handle.
 * @retval DDS_RETCODE_ERROR
 *              An internal error occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *              One of the parameters is invalid, or the participant and topic do
 *              not belong to the same domain participant.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *              participant_or_publisher is neither a participant nor a publisher,
 *              or the operation is otherwise inappropriate for the supplied entity.
 * @retval DDS_RETCODE_INCONSISTENT_POLICY
 *              The requested QoS is inconsistent.
 * @retval DDS_RETCODE_NOT_ALLOWED_BY_SECURITY
 *              The configured DDS Security access control plugin denied creation
 *              of the writer.
 */
DDS_EXPORT dds_entity_t
dds_create_writer(
  dds_entity_t participant_or_publisher,
  dds_entity_t topic,
  const dds_qos_t *qos,
  const dds_listener_t *listener);


/**
 * @defgroup writing (Writing data)
 * @ingroup writer
 * Writing data (and variants of it) is straightforward. The first set
 * is equivalent to the second set with -1 passed for "timestamp",
 * meaning, substitute the result of a call to time(). The dispose
 * and unregister operations take an object of the topic's type, but
 * only touch the key fields; the remained may be undefined.
*/

/**
 * @brief Registers an instance
 * @ingroup writing
 * @component data_instance
 *
 * This operation registers an instance with a key value to the data writer and
 * returns an instance handle that could be used for successive write & dispose
 * operations. When the handle is not allocated, the function will return an
 * error and the handle will be un-touched.
 *
 * @param[in]  writer  The writer to which instance has be associated.
 * @param[out] handle  The instance handle.
 * @param[in]  data    The instance with the key value.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *            The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *            The operation is invoked on an inappropriate object.
 */
DDS_EXPORT dds_return_t
dds_register_instance(
  dds_entity_t writer,
  dds_instance_handle_t *handle,
  const void *data);

/**
 * @brief Unregisters an instance by instance
 * @ingroup writing
 * @component data_instance
 *
 * This operation reverses the action of register instance, removes all information regarding
 * the instance and unregisters an instance with a key value from the data writer.
 *
 * @param[in]  writer  The writer to which instance is associated.
 * @param[in]  data    The instance with the key value.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 */
DDS_EXPORT dds_return_t
dds_unregister_instance(dds_entity_t writer, const void *data);

/**
 * @brief Unregisters an instance by instance handle
 * @ingroup writing
 * @component data_instance
 *
 * This operation unregisters the instance which is identified by the key fields of the given
 * typed instance handle.
 *
 * @param[in]  writer  The writer to which instance is associated.
 * @param[in]  handle  The instance handle.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 */
DDS_EXPORT dds_return_t
dds_unregister_instance_ih(dds_entity_t writer, dds_instance_handle_t handle);

/**
 * @brief Unregisters an instance by instance with timestamp
 * @ingroup writing
 * @component data_instance
 *
 * This operation reverses the action of register instance, removes all information regarding
 * the instance and unregisters an instance with a key value from the data writer. It also
 * provides a value for the timestamp explicitly.
 *
 * @param[in]  writer    The writer to which instance is associated.
 * @param[in]  data      The instance with the key value.
 * @param[in]  timestamp The timestamp for the unregistration (>= 0).
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 */
DDS_EXPORT dds_return_t
dds_unregister_instance_ts(
  dds_entity_t writer,
  const void *data,
  dds_time_t timestamp);

/**
 * @brief Unregisters an instance by instance handle with timestamp
 * @ingroup writing
 * @component data_instance
 *
 * This operation unregisters an instance with a key value from the handle. Instance can be identified
 * from instance handle. If an unregistered key ID is passed as an instance data, an error is logged and
 * not flagged as return value.
 *
 * @param[in]  writer    The writer to which instance is associated.
 * @param[in]  handle    The instance handle.
 * @param[in]  timestamp The timestamp for the unregistration (>= 0).
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object
 */
DDS_EXPORT dds_return_t
dds_unregister_instance_ih_ts(
  dds_entity_t writer,
  dds_instance_handle_t handle,
  dds_time_t timestamp);

/**
 * @brief This operation modifies and disposes a data instance.
 * @ingroup writing
 * @component write_data
 *
 * This operation requests the Data Distribution Service to modify the instance and
 * mark it for deletion. Copies of the instance and its corresponding samples, which are
 * stored in every connected reader and, dependent on the QoS policy settings (also in
 * the Transient and Persistent stores) will be modified and marked for deletion by
 * setting their dds_instance_state_t to DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE.
 *
 * @par Blocking
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
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The sample is written and the instance is marked for deletion.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             At least one of the arguments is invalid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_TIMEOUT
 *             Either the current action overflowed the available resources as
 *             specified by the combination of the reliability QoS policy,
 *             history QoS policy and resource_limits QoS policy, or the
 *             current action was waiting for data delivery acknowledgement
 *             by synchronous readers. This caused blocking of this operation,
 *             which could not be resolved before max_blocking_time of the
 *             reliability QoS policy elapsed.
 */
DDS_EXPORT dds_return_t
dds_writedispose(dds_entity_t writer, const void *data);

/**
 * @brief This operation modifies and disposes a data instance with a specific
 *        timestamp.
 * @ingroup writing
 * @component write_data
 *
 * This operation performs the same functions as dds_writedispose() except that
 * the application provides the value for the source_timestamp that is made
 * available to connected reader objects. This timestamp is important for the
 * interpretation of the destination_order QoS policy.
 *
 * @param[in]  writer    The writer to dispose the data instance from.
 * @param[in]  data      The data to be written and disposed.
 * @param[in]  timestamp The timestamp used as source timestamp (>= 0).
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The sample is written and the instance is marked for deletion.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             At least one of the arguments is invalid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_TIMEOUT
 *             Either the current action overflowed the available resources as
 *             specified by the combination of the reliability QoS policy,
 *             history QoS policy and resource_limits QoS policy, or the
 *             current action was waiting for data delivery acknowledgement
 *             by synchronous readers. This caused blocking of this operation,
 *             which could not be resolved before max_blocking_time of the
 *             reliability QoS policy elapsed.
 */
DDS_EXPORT dds_return_t
dds_writedispose_ts(
  dds_entity_t writer,
  const void *data,
  dds_time_t timestamp);

/**
 * @brief This operation disposes an instance, identified by the data sample.
 * @ingroup writing
 * @component write_data
 *
 * This operation requests the Data Distribution Service to modify the instance and
 * mark it for deletion. Copies of the instance and its corresponding samples, which are
 * stored in every connected reader and, dependent on the QoS policy settings (also in
 * the Transient and Persistent stores) will be modified and marked for deletion by
 * setting their dds_instance_state_t to DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE.
 *
 * @par Blocking
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
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The sample is written and the instance is marked for deletion.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             At least one of the arguments is invalid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_TIMEOUT
 *             Either the current action overflowed the available resources as
 *             specified by the combination of the reliability QoS policy,
 *             history QoS policy and resource_limits QoS policy, or the
 *             current action was waiting for data delivery acknowledgement
 *             by synchronous readers. This caused blocking of this operation,
 *             which could not be resolved before max_blocking_time of the
 *             reliability QoS policy elapsed.
 */
DDS_EXPORT dds_return_t
dds_dispose(dds_entity_t writer, const void *data);

/**
 * @brief This operation disposes an instance with a specific timestamp, identified by the data sample.
 * @ingroup writing
 * @component write_data
 *
 * This operation performs the same functions as dds_dispose() except that
 * the application provides the value for the source_timestamp that is made
 * available to connected reader objects. This timestamp is important for the
 * interpretation of the destination_order QoS policy.
 *
 * @param[in]  writer    The writer to dispose the data instance from.
 * @param[in]  data      The data sample that identifies the instance
 *                       to be disposed.
 * @param[in]  timestamp The timestamp used as source timestamp (>= 0).
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The sample is written and the instance is marked for deletion
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             At least one of the arguments is invalid
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted
 * @retval DDS_RETCODE_TIMEOUT
 *             Either the current action overflowed the available resources as
 *             specified by the combination of the reliability QoS policy,
 *             history QoS policy and resource_limits QoS policy, or the
 *             current action was waiting for data delivery acknowledgment
 *             by synchronous readers. This caused blocking of this operation,
 *             which could not be resolved before max_blocking_time of the
 *             reliability QoS policy elapsed.
 */
DDS_EXPORT dds_return_t
dds_dispose_ts(
  dds_entity_t writer,
  const void *data,
  dds_time_t timestamp);

/**
 * @brief This operation disposes an instance, identified by the instance handle.
 * @ingroup writing
 * @component write_data
 *
 * This operation requests the Data Distribution Service to modify the instance and
 * mark it for deletion. Copies of the instance and its corresponding samples, which are
 * stored in every connected reader and, dependent on the QoS policy settings (also in
 * the Transient and Persistent stores) will be modified and marked for deletion by
 * setting their dds_instance_state_t to DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE.
 *
 * @par Instance Handle
 * The given instance handle must correspond to the value that was returned by either
 * the dds_register_instance operation, dds_register_instance_ts or dds_lookup_instance.
 * If there is no correspondence, then the result of the operation is unspecified.
 *
 * @param[in]  writer The writer to dispose the data instance from.
 * @param[in]  handle The handle to identify an instance.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The sample is written and the instance is marked for deletion.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             At least one of the arguments is invalid
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The instance handle has not been registered with this writer
 */
DDS_EXPORT dds_return_t
dds_dispose_ih(dds_entity_t writer, dds_instance_handle_t handle);

/**
 * @brief This operation disposes an instance with a specific timestamp, identified by the instance handle.
 * @ingroup writing
 * @component write_data
 *
 * This operation performs the same functions as dds_dispose_ih() except that
 * the application provides the value for the source_timestamp that is made
 * available to connected reader objects. This timestamp is important for the
 * interpretation of the destination_order QoS policy.
 *
 * @param[in]  writer    The writer to dispose the data instance from.
 * @param[in]  handle    The handle to identify an instance.
 * @param[in]  timestamp The timestamp used as source timestamp (>= 0).
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The sample is written and the instance is marked for deletion.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             At least one of the arguments is invalid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The instance handle has not been registered with this writer.
 */
DDS_EXPORT dds_return_t
dds_dispose_ih_ts(
  dds_entity_t writer,
  dds_instance_handle_t handle,
  dds_time_t timestamp);

/**
 * @brief Write the value of a data instance
 * @ingroup writing
 * @component write_data
 *
 * With this API, the value of the source timestamp is automatically made
 * available to the data reader by the service.
 *
 * @param[in]  writer The writer entity.
 * @param[in]  data Value to be written.
 *
 * @returns dds_return_t indicating success or failure.
 */
DDS_EXPORT dds_return_t
dds_write(dds_entity_t writer, const void *data);

/**
 * @brief Flush a writers batched writes
 * @ingroup writing
 * @component write_data
 *
 * When using write batching you can manually batch small writes into larger
 * datapackets for network efficiency. The normal `dds_write()` no longer
 * guarantee that data is sent on the network automatically.
 *
 * @param[in]  entity The writer entity.

 * @returns A dds_return_t indicating success or failure.
 * @retval DDS_RETCODE_OK
 *             The writer successfully forwarded to the network.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_write_flush(dds_entity_t entity);

/**
 * @brief Write a serialized value of a data instance
 * @ingroup writing
 * @component write_data
 *
 * This call causes the writer to write the serialized value that is provided
 * in the serdata argument.  Timestamp and statusinfo fields are set to the
 * current time and 0 (indicating a regular write), respectively.
 *
 * @param[in]  writer The writer entity.
 * @param[in]  serdata Serialized value to be written. Its type pointer must be non-null.
 *             If non-null, the call consumes one reference on both success and error.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The writer successfully wrote the serialized value.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_TIMEOUT
 *             The writer failed to write the serialized value reliably within the specified max_blocking_time.
 */
DDS_EXPORT dds_return_t
dds_writecdr(dds_entity_t writer, struct ddsi_serdata *serdata);

/**
 * @brief Write a serialized value of a data instance
 * @ingroup writing
 * @component write_data
 *
 * This call causes the writer to write the serialized value that is provided
 * in the serdata argument.  Timestamp and statusinfo are used as is.
 *
 * @param[in]  writer The writer entity.
 * @param[in]  serdata Serialized value to be written. Its type pointer must be non-null.
 *             If non-null, the call consumes one reference on both success and error.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The writer successfully wrote the serialized value.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_TIMEOUT
 *             The writer failed to write the serialized value reliably within the specified max_blocking_time.
 */
DDS_EXPORT dds_return_t
dds_forwardcdr(dds_entity_t writer, struct ddsi_serdata *serdata);

/**
 * @brief Write the value of a data instance along with the source timestamp passed.
 * @ingroup writing
 * @component write_data
 *
 * @param[in]  writer The writer entity.
 * @param[in]  data Value to be written.
 * @param[in]  timestamp Source timestamp (>= 0).
 *
 * @returns A dds_return_t indicating success or failure.
 */
DDS_EXPORT dds_return_t
dds_write_ts(
  dds_entity_t writer,
  const void *data,
  dds_time_t timestamp);

/**
 * @defgroup readcondition (ReadCondition)
 * @ingroup condition
 */
/**
 * @defgroup querycondition (QueryCondition)
 * @ingroup condition
 */
/**
 * @defgroup guardcondition (GuardCondition)
 * @ingroup condition
 */

/**
 * @brief Creates a readcondition associated to the given reader.
 * @ingroup readcondition
 * @component data_query
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
 * @returns A valid condition handle or an error code.
 *
 * @retval >0
 *             A valid condition handle
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_EXPORT dds_entity_t
dds_create_readcondition(dds_entity_t reader, uint32_t mask);

/**
 * @brief Function signature for a querycondition filter
 * @ingroup querycondition
 */
typedef bool (*dds_querycondition_filter_fn) (const void * sample);

/**
 * @brief Creates a queryondition associated to the given reader.
 * @ingroup querycondition
 * @component data_query
 *
 * The queryondition allows specifying which samples are of interest in
 * a data reader's history, by means of a mask and a filter. The mask is
 * or'd with the flags that are dds_sample_state_t, dds_view_state_t and
 * dds_instance_state_t.
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
 * @param[in]  reader  Reader to associate the condition to.
 * @param[in]  mask    Interest (dds_sample_state_t|dds_view_state_t|dds_instance_state_t).
 * @param[in]  filter  Callback that the application can use to filter specific samples.
 *
 * @returns A valid condition handle or an error code
 *
 * @retval >=0
 *             A valid condition handle.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_EXPORT dds_entity_t
dds_create_querycondition(
  dds_entity_t reader,
  uint32_t mask,
  dds_querycondition_filter_fn filter);

/**
 * @brief Creates a guardcondition.
 * @ingroup guardcondition
 * @component guard_condition
 *
 * Waitsets allow waiting for an event on some of any set of entities.
 * This means that the guardcondition can be used to wake up a waitset when
 * data is in the reader history with states that matches the given mask.
 *
 * @param[in]   owner  Participant on which to create the guardcondition.
 *
 * @returns A valid condition handle or an error code.
 *
 * @retval >0
 *             A valid condition handle
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_EXPORT dds_entity_t
dds_create_guardcondition(dds_entity_t owner);

/**
 * @brief Sets the trigger status of a guardcondition.
 * @ingroup guardcondition
 * @component guard_condition
 *
 * @param[in]   guardcond  Guard condition to set the trigger status of.
 * @param[in]   triggered  The triggered status to set.
 *
 * @retval DDS_RETCODE_OK
 *             Operation successful
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_set_guardcondition(dds_entity_t guardcond, bool triggered);

/**
 * @brief Reads the trigger status of a guardcondition.
 * @ingroup guardcondition
 * @component guard_condition
 *
 * @param[in]   guardcond  Guard condition to read the trigger status of.
 * @param[out]  triggered  Receives the trigger status on success. On error, `*triggered` is unchanged.
 *
 * @retval DDS_RETCODE_OK
 *             Operation successful
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_read_guardcondition(dds_entity_t guardcond, bool *triggered);

/**
 * @brief Reads and resets the trigger status of a guardcondition.
 * @ingroup guardcondition
 * @component guard_condition
 *
 * @param[in]   guardcond  Guard condition to read and reset the trigger status of.
 * @param[out]  triggered  Receives the trigger status before the reset. On error, `*triggered` is unchanged.
 *
 * @retval DDS_RETCODE_OK
 *             Operation successful
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_take_guardcondition(dds_entity_t guardcond, bool *triggered);

/**
 * @defgroup waitset (WaitSet)
 * @ingroup dds
 */

/**
 * @brief Waitset attachment argument.
 * @ingroup waitset
 *
 * Every entity that is attached to the waitset can be accompanied by such
 * an attachment argument. When the waitset wait is unblocked because of an
 * entity that triggered, then the returning array will be populated with
 * these attachment arguments that are related to the triggered entity.
 */
typedef intptr_t dds_attach_t;

/**
 * @brief Create a waitset and allocate the resources required
 * @ingroup waitset
 * @component waitset
 *
 * A WaitSet object allows an application to wait until one or more of the
 * conditions of the attached entities evaluates to TRUE or until the timeout
 * expires.
 *
 * @param[in]  owner  Domain participant which the WaitSet contains.
 *
 * @returns A valid waitset handle or an error code.
 *
 * @retval >=0
 *             A valid waitset handle.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_EXPORT dds_entity_t
dds_create_waitset(dds_entity_t owner);

/**
 * @brief Acquire previously attached entities.
 * @ingroup waitset
 * @component waitset
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
 * @returns A dds_return_t with the number of children or an error code.
 *
 * @retval >=0
 *             Number of children found (can be larger than 'size').
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The entities parameter is NULL, while a size is provided.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The waitset has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_waitset_get_entities(
  dds_entity_t waitset,
  dds_entity_t *entities,
  size_t size);

/**
 * @brief This operation attaches an Entity to the WaitSet.
 * @ingroup waitset
 * @component waitset
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
 *                      triggered by the given entity.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             Entity attached.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The given waitset or entity are not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The waitset has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The entity was already attached.
 */
DDS_EXPORT dds_return_t
dds_waitset_attach(
  dds_entity_t waitset,
  dds_entity_t entity,
  dds_attach_t x);

/**
 * @brief This operation detaches an Entity from the WaitSet.
 * @ingroup waitset
 * @component waitset
 *
 * @param[in]  waitset  The waitset to detach the given entity from.
 * @param[in]  entity   The entity to detach.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             Entity detached.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The given waitset or entity are not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The waitset has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The entity is not attached.
 */
DDS_EXPORT dds_return_t
dds_waitset_detach(
  dds_entity_t waitset,
  dds_entity_t entity);

/**
 * @brief Sets the trigger_value associated with a waitset.
 * @ingroup waitset
 * @component waitset
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
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             Trigger value set.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The given waitset is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The waitset has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_waitset_set_trigger(
  dds_entity_t waitset,
  bool trigger);

/**
 * @brief This operation allows an application thread to wait for the a status
 *        change or other trigger on (one of) the entities that are attached to
 *        the WaitSet.
 * @ingroup waitset
 * @component waitset
 *
 * The dds_waitset_wait() operation blocks until the some of the attached
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
 * @returns A dds_return_t with the number of entities triggered or an error code
 *
 * @retval >0
 *             Number of entities triggered.
 * @retval  0
 *             Time out (no entities were triggered).
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The given waitset is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The waitset has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_waitset_wait(
  dds_entity_t waitset,
  dds_attach_t *xs,
  size_t nxs,
  dds_duration_t reltimeout);

/**
 * @brief This operation allows an application thread to wait for the a status
 *        change or other trigger on (one of) the entities that are attached to
 *        the WaitSet.
 * @ingroup waitset
 * @component waitset
 *
 * The dds_waitset_wait() operation blocks until the some of the attached
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
 * @returns A dds_return_t with the number of entities triggered or an error code.
 *
 * @retval >0
 *             Number of entities triggered.
 * @retval  0
 *             Time out (no entities were triggered).
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The given waitset is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The waitset has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_waitset_wait_until(
  dds_entity_t waitset,
  dds_attach_t *xs,
  size_t nxs,
  dds_time_t abstimeout);

/**
 * @defgroup reading (Reading Data)
 * @ingroup reader
 */

/**
 * @brief Read data from the data reader, read or query condition without updating state
 * @ingroup reading
 * @component read_data
 *
 * Reads samples from the reader history cache without marking these samples as "read". It starts
 * with an arbitrary (matching) instance, reading (matching) samples from the oldest to
 * the most recent, then continues with another arbitrarily selected (matching) instance,
 * etc. This continues until it has traversed the entire history cache or has gathered
 * `maxs` samples.
 *
 * The @ref dds_read operation can be used to mark the returned samples as "read"; the
 * @ref dds_take operation can be used to also remove the returned samples from the
 * history cache.
 *
 * For the plain `dds_peek` operation, all instances and samples match. This is different
 * for the more selective variants, where the documentation refers to this function and
 * only gives detailed information where it differs.
 *
 * The `buf` parameter is used as follows:
 * - If `buf[0]` on entry is a null pointer:
 *   - on return `buf[0]` .. `buf[k-1]` will point to middleware-owned memory (a.k.a. loans); and
 *   - `buf[k]` will be a null pointer if `0 <= k < bufsz`; and
 *   - `0 <= k <= maxs` is the number of samples read (a.k.a. the return value).
 *
 * - If `buf[0]` on entry is an outstanding sample loan (i.e., resulting from a previous call to, e.g., read), then:
 *   - all of `buf[0]` .. `buf[k-1]` must be pointers to outstanding loans; and
 *   - `k` = `bufsz` or `buf[k]` is a null pointer; where
 *   - `1 <= k < bufsz`; and
 *   - all these outstanding loans are returned as-if through @ref dds_return_loan; and
 *   - the result will be as if `buf[0]` had been a null pointer on entry.
 *
 * - If `buf[0]` on entry is any other address, then:
 *   - all of `buf[0]` .. `buf[bufsz-1]` must point to memory suitable for storing samples; and
 *   - the C binding requires that this memory must be initialized such that all embedded strings, externals,
 *     optionals and sequences are initialized (null pointers are ok, sequences may also be all-0); and
 *   - for the C binding, @ref dds_sequence_t and @ref dds_sample_free describe the ownership rules for
 *     sequence fields when application-owned sample buffers are used.
 *
 * The loans returned by `dds_peek` operation are potentially shared copies of the data and the contents
 * may not be modified. If a private copy is required, pass in non-null pointers to memory as in the third case
 * above.
 *
 * The `si` array is filled with sample information on all returned samples. If the
 * `valid_data` flag is set in the sample info for a particular sample, all fields of that
 * sample are valid. Otherwise, only the key value is valid. For the C binding, all other
 * fields will be set to 0.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of  `bufsz` pointers to samples (see above).
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] bufsz The size of buffer provided.
 * @param[in] maxs Maximum number of samples to read.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_peek(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs);

/**
 * @brief Read data matching sample/view/instance states from the data reader, read or query condition without updating state
 * @ingroup reading
 * @component read_data
 *
 * See @ref dds_peek. The matching criterion referred to there is that the
 * sample/view/instance states must match the specification in the `mask` parameter.
 *
 * If the sample/view/instance state component in the mask is 0 and `reader_or_condition`
 * references a data reader (as opposed to a read or query condition), it is treated as
 * equivalent to any sample/view/instance state. If `reader_or_condition` references a
 * read or query condition, the matching states are the union of `mask` and the
 * condition's mask.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of `bufsz` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] bufsz The size of buffer provided.
 * @param[in] maxs Maximum number of samples to read.
 * @param[in] mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_peek_mask(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs,
  uint32_t mask);

/**
 * @brief Read data for a specific instance from the data reader, read or query condition without updating state
 * @ingroup reading
 * @component read_data
 *
 * See @ref dds_peek. The matching criterion referred to there is that the instance
 * handle must equal the `handle` parameter.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of `bufsz` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] bufsz The size of buffer provided.
 * @param[in] maxs Maximum number of samples to read.
 * @param[in] handle Instance handle related to the samples to read.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The instance handle has not been registered with this reader.
 */
DDS_EXPORT dds_return_t
dds_peek_instance(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs,
  dds_instance_handle_t handle);

/**
 * @brief Read data for a specific instance matching sample/view/instance states from the data reader, read or query condition without updating state
 * @ingroup reading
 * @component read_data
 *
 * See @ref dds_peek. The matching criterion referred to there is that:
 * - the instance handle must equal the `handle` parameter; and
 * - the sample/view/instance states must match the specification in the `mask` parameter.
 *
 * If the sample/view/instance state component in the mask is 0 and `reader_or_condition`
 * references a data reader (as opposed to a read or query condition), it is treated as
 * equivalent to any sample/view/instance state. If `reader_or_condition` references a
 * read or query condition, the matching states are the union of `mask` and the
 * condition's mask.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of  `bufsz` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] bufsz The size of buffer provided.
 * @param[in] maxs Maximum number of samples to read.
 * @param[in] handle Instance handle related to the samples to read.
 * @param[in] mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The instance handle has not been registered with this reader.
 */
DDS_EXPORT dds_return_t
dds_peek_instance_mask(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs,
  dds_instance_handle_t handle,
  uint32_t mask);

/**
 * @brief Take data for a specific instance from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * See @ref dds_take. The matching criterion referred to there is that the instance
 * handle must equal the `handle` parameter.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of `bufsz` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] bufsz The size of buffer provided.
 * @param[in] maxs Maximum number of samples to read.
 * @param[in] previous_handle Instance handle right before the next instance from which the samples will be read.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_NO_DATA
 *             The entity has no more data to be read.
 */
DDS_EXPORT dds_return_t
dds_peek_next_instance(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs,
  dds_instance_handle_t previous_handle);

/**
 * @brief Take data for a specific instance matching sample/view/instance states from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * See @ref dds_take. The matching criterion referred to there is that:
 * - the instance handle must equal the `handle` parameter; and
 * - the sample/view/instance states must match the specification in the `mask` parameter.
 *
 * If the sample/view/instance state component in the mask is 0 and `reader_or_condition`
 * references a data reader (as opposed to a read or query condition), it is treated as
 * equivalent to any sample/view/instance state. If `reader_or_condition` references a
 * read or query condition, the matching states are the union of `mask` and the
 * condition's mask.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of  `bufsz` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] bufsz The size of buffer provided.
 * @param[in] maxs Maximum number of samples to read.
 * @param[in] previous_handle Instance handle related to the samples to read.
 * @param[in] mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_NO_DATA
 *             The entity has no more data to be read 
 */
DDS_EXPORT dds_return_t
dds_peek_next_instance_mask(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs,
  dds_instance_handle_t previous_handle,
  uint32_t mask);

/**
 * @brief Read the first unread sample without updating state
 * @ingroup reading
 * @component read_data
 *
 * Equivalent to `dds_peek_mask(reader, buf, si, 1, 1, DDS_NOT_READ_SAMPLE_STATE)`.
 *
 * @param[in] reader The reader entity.
 * @param[in,out] buf A pointer to a sample.
 * @param[out] si The pointer to @ref dds_sample_info_t returned for a data value.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The entity parameter is not a valid parameter.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_peek_next(
  dds_entity_t reader,
  void **buf,
  dds_sample_info_t *si);

/**
 * @brief Read data from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * Reads samples from the reader history cache, marking these samples as "read". It starts
 * with an arbitrary (matching) instance, reading (matching) samples from the oldest to
 * the most recent, then continues with another arbitrarily selected (matching) instance,
 * etc. This continues until it has traversed the entire history cache or has gathered
 * `maxs` samples.
 *
 * The @ref dds_peek operation can be read samples without marking them as "read"; the
 * @ref dds_take operation can be used to also remove the returned samples from the
 * history cache.
 *
 * For the plain `dds_read` operation, all instances and samples match. This is different
 * for the more selective variants, where the documentation refers to this function and
 * only gives detailed information where it differs.
 *
 * The `buf` parameter is used as follows:
 * - If `buf[0]` on entry is a null pointer:
 *   - on return `buf[0]` .. `buf[k-1]` will point to middleware-owned memory (a.k.a. loans); and
 *   - `buf[k]` will be a null pointer if `0 <= k < bufsz`; and
 *   - `0 <= k <= maxs` is the number of samples read (a.k.a. the return value).
 *
 * - If `buf[0]` on entry is an outstanding sample loan (i.e., resulting from a previous call to, e.g., read), then:
 *   - all of `buf[0]` .. `buf[k-1]` must be pointers to outstanding loans; and
 *   - `k` = `bufsz` or `buf[k]` is a null pointer; where
 *   - `1 <= k < bufsz`; and
 *   - all these outstanding loans are returned as-if through @ref dds_return_loan; and
 *   - the result will be as if `buf[0]` had been a null pointer on entry.
 *
 * - If `buf[0]` on entry is any other address, then:
 *   - all of `buf[0]` .. `buf[bufsz-1]` must point to memory suitable for storing samples; and
 *   - the C binding requires that this memory must be initialized such that all embedded strings, externals,
 *     optionals and sequences are initialized (null pointers are ok, sequences may also be all-0); and
 *   - for the C binding, @ref dds_sequence_t and @ref dds_sample_free describe the ownership rules for
 *     sequence fields when application-owned sample buffers are used.
 *
 * The loans returned by `dds_read` operation are potentially shared copies of the data and the contents
 * may not be modified. If a private copy is required, pass in non-null pointers to memory as in the third case
 * above.
 *
 * The `si` array is filled with sample information on all returned samples. If the
 * `valid_data` flag is set in the sample info for a particular sample, all fields of that
 * sample are valid. Otherwise, only the key value is valid. For the C binding, all other
 * fields will be set to 0.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of  `bufsz` pointers to samples (see above).
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] bufsz The size of buffer provided.
 * @param[in] maxs Maximum number of samples to read.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_read(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs);

/**
 * @brief Read data from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * @deprecated Alias for `dds_read` where `bufsz` = `maxs`.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of `maxs` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] maxs Maximum number of samples to read.
 *
 * @returns A dds_return_t with the number of samples read or an error code
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_DEPRECATED_EXPORT dds_return_t
dds_read_wl(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  uint32_t maxs);

/**
 * @brief Read data matching sample/view/instance states from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * See @ref dds_read. The matching criterion referred to there is that the
 * sample/view/instance states must match the specification in the `mask` parameter.
 *
 * If the sample/view/instance state component in the mask is 0 and `reader_or_condition`
 * references a data reader (as opposed to a read or query condition), it is treated as
 * equivalent to any sample/view/instance state. If `reader_or_condition` references a
 * read or query condition, the matching states are the union of `mask` and the
 * condition's mask.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of `bufsz` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] bufsz The size of buffer provided.
 * @param[in] maxs Maximum number of samples to read.
 * @param[in] mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_read_mask(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs,
  uint32_t mask);

/**
 * @brief Read data matching sample/view/instance states from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * @deprecated Alias for @ref dds_read_mask where `bufsz` = `maxs`.
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of `maxs` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in]  maxs Maximum number of samples to read.
 * @param[in]  mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_DEPRECATED_EXPORT dds_return_t
dds_read_mask_wl(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  uint32_t maxs,
  uint32_t mask);

/**
 * @brief Read data for a specific instance from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * See @ref dds_read. The matching criterion referred to there is that the instance
 * handle must equal the `handle` parameter.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of `bufsz` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] bufsz The size of buffer provided.
 * @param[in] maxs Maximum number of samples to read.
 * @param[in] handle Instance handle related to the samples to read.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The instance handle has not been registered with this reader.
 */
DDS_EXPORT dds_return_t
dds_read_instance(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs,
  dds_instance_handle_t handle);

/**
 * @brief Read data for a specific instance from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * @deprecated Alias for @ref dds_read_instance where `bufsz` = `maxs`.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of `maxs` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] maxs Maximum number of samples to read.
 * @param[in] handle Instance handle related to the samples to read.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The instance handle has not been registered with this reader.
 */
DDS_DEPRECATED_EXPORT dds_return_t
dds_read_instance_wl(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  uint32_t maxs,
  dds_instance_handle_t handle);

/**
 * @brief Read data for a specific instance matching sample/view/instance states from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * See @ref dds_read. The matching criterion referred to there is that:
 * - the instance handle must equal the `handle` parameter; and
 * - the sample/view/instance states must match the specification in the `mask` parameter.
 *
 * If the sample/view/instance state component in the mask is 0 and `reader_or_condition`
 * references a data reader (as opposed to a read or query condition), it is treated as
 * equivalent to any sample/view/instance state. If `reader_or_condition` references a
 * read or query condition, the matching states are the union of `mask` and the
 * condition's mask.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of  `bufsz` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] bufsz The size of buffer provided.
 * @param[in] maxs Maximum number of samples to read.
 * @param[in] handle Instance handle related to the samples to read.
 * @param[in] mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The instance handle has not been registered with this reader.
 */
DDS_EXPORT dds_return_t
dds_read_instance_mask(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs,
  dds_instance_handle_t handle,
  uint32_t mask);

/**
 * @brief Read data for a specific instance matching sample/view/instance states from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * @deprecated Alias for @ref dds_read_instance_mask where `bufsz` = `maxs`.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of `maxs` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] maxs Maximum number of samples to read.
 * @param[in] handle Instance handle related to the samples to read.
 * @param[in] mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The instance handle has not been registered with this reader.
 */
DDS_DEPRECATED_EXPORT dds_return_t
dds_read_instance_mask_wl(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  uint32_t maxs,
  dds_instance_handle_t handle,
  uint32_t mask);

/**
 * @brief Read data for a specific instance from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * See @ref dds_read. The matching criterion referred to there is that the instance
 * handle must equal the `handle` parameter.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of `bufsz` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] bufsz The size of buffer provided.
 * @param[in] maxs Maximum number of samples to read.
 * @param[in] previous_handle Instance handle before the instance for which to read the samples from.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_NO_DATA
 *             The entity has no more data to be read 
 */
DDS_EXPORT dds_return_t
dds_read_next_instance(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs,
  dds_instance_handle_t previous_handle);

/**
 * @brief Read data for a specific instance matching sample/view/instance states from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * See @ref dds_read. The matching criterion referred to there is that:
 * - the instance handle must equal the `handle` parameter; and
 * - the sample/view/instance states must match the specification in the `mask` parameter.
 *
 * If the sample/view/instance state component in the mask is 0 and `reader_or_condition`
 * references a data reader (as opposed to a read or query condition), it is treated as
 * equivalent to any sample/view/instance state. If `reader_or_condition` references a
 * read or query condition, the matching states are the union of `mask` and the
 * condition's mask.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of  `bufsz` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] bufsz The size of buffer provided.
 * @param[in] maxs Maximum number of samples to read.
 * @param[in] previous_handle Instance handle related to the samples to read.
 * @param[in] mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_NO_DATA
 *             The entity has no more data to be read 
 */
DDS_EXPORT dds_return_t
dds_read_next_instance_mask(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs,
  dds_instance_handle_t previous_handle,
  uint32_t mask);

/**
 * @brief Read the first unread sample
 * @ingroup reading
 * @component read_data
 *
 * Equivalent to `dds_read_mask(reader, buf, si, 1, 1, DDS_NOT_READ_SAMPLE_STATE)`.
 *
 * @param[in] reader The reader entity.
 * @param[in,out] buf A pointer to a sample.
 * @param[out] si The pointer to @ref dds_sample_info_t returned for a data value.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The entity parameter is not a valid parameter.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_read_next(
  dds_entity_t reader,
  void **buf,
  dds_sample_info_t *si);

/**
 * @brief Read the first unread sample
 * @ingroup reading
 * @component read_data
 *
 * @deprecated Alias for @ref dds_read_next.
 *
 * @param[in] reader The reader entity.
 * @param[in,out] buf A pointer to a sample.
 * @param[out] si The pointer to @ref dds_sample_info_t returned for a data value.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The entity parameter is not a valid parameter.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_DEPRECATED_EXPORT dds_return_t
dds_read_next_wl(
  dds_entity_t reader,
  void **buf,
  dds_sample_info_t *si);

/**
 * @brief Take data from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * Reads and removes samples from the reader history cache. It starts with an arbitrary
 * (matching) instance, reading (matching) samples from the oldest to the most recent,
 * then continues with another arbitrarily selected (matching) instance, etc. This
 * continues until it has traversed the entire history cache or has gathered `maxs`
 * samples.
 *
 * The @ref dds_read operation can be used to read samples without removing them from
 * the history cache but marking them as "read"; the @ref dds_peek operation can be used
 * to read samples from the cache without changing any internal state.
 *
 * For the plain `dds_take` operation, all instances and samples match. This is different
 * for the more selective variants, where the documentation refers to this function and
 * only gives detailed information where it differs.
 *
 * The `buf` parameter is used as follows:
 * - If `buf[0]` on entry is a null pointer:
 *   - on return `buf[0]` .. `buf[k-1]` will point to middleware-owned memory (a.k.a. loans); and
 *   - `buf[k]` will be a null pointer if `0 <= k < bufsz`; and
 *   - `0 <= k <= maxs` is the number of samples read (a.k.a. the return value).
 *
 * - If `buf[0]` on entry is an outstanding sample loan (i.e., resulting from a previous call to, e.g., read), then:
 *   - all of `buf[0]` .. `buf[k-1]` must be pointers to outstanding loans; and
 *   - `k` = `bufsz` or `buf[k]` is a null pointer; where
 *   - `1 <= k < bufsz`; and
 *   - all these outstanding loans are returned as-if through @ref dds_return_loan; and
 *   - the result will be as if `buf[0]` had been a null pointer on entry.
 *
 * - If `buf[0]` on entry is any other address, then:
 *   - all of `buf[0]` .. `buf[bufsz-1]` must point to memory suitable for storing samples; and
 *   - the C binding requires that this memory must be initialized such that all embedded strings, externals,
 *     optionals and sequences are initialized (null pointers are ok, sequences may also be all-0); and
 *   - for the C binding, @ref dds_sequence_t and @ref dds_sample_free describe the ownership rules for
 *     sequence fields when application-owned sample buffers are used.
 *
 * The loans returned by `dds_take` operation are potentially shared copies of the data and the contents
 * may not be modified. If a private copy is required, pass in non-null pointers to memory as in the third case
 * above.
 *
 * The `si` array is filled with sample information on all returned samples. If the
 * `valid_data` flag is set in the sample info for a particular sample, all fields of that
 * sample are valid. Otherwise, only the key value is valid. For the C binding, all other
 * fields will be set to 0.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of  `bufsz` pointers to samples (see above).
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] bufsz The size of buffer provided.
 * @param[in] maxs Maximum number of samples to read.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_take(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs);

/**
 * @brief Take data from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * @deprecated Alias for @ref dds_take where `bufsz` = `maxs`.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of `maxs` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] maxs Maximum number of samples to read.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_DEPRECATED_EXPORT dds_return_t
dds_take_wl(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  uint32_t maxs);

/**
 * @brief Take data matching sample/view/instance states from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * See @ref dds_take. The matching criterion referred to there is that the
 * sample/view/instance states must match the specification in the `mask` parameter.
 *
 * If the sample/view/instance state component in the mask is 0 and `reader_or_condition`
 * references a data reader (as opposed to a read or query condition), it is treated as
 * equivalent to any sample/view/instance state. If `reader_or_condition` references a
 * read or query condition, the matching states are the union of `mask` and the
 * condition's mask.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of `bufsz` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] bufsz The size of buffer provided.
 * @param[in] maxs Maximum number of samples to read.
 * @param[in] mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_take_mask(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs,
  uint32_t mask);

/**
 * @brief Take data matching sample/view/instance states from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * @deprecated Alias for `dds_take_mask` where `bufsz` = `maxs`.
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of `maxs` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in]  maxs Maximum number of samples to read.
 * @param[in]  mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_DEPRECATED_EXPORT dds_return_t
dds_take_mask_wl(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  uint32_t maxs,
  uint32_t mask);

/**
 * @brief Take data for a specific instance from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * See @ref dds_take. The matching criterion referred to there is that the instance
 * handle must equal the `handle` parameter.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of `bufsz` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] bufsz The size of buffer provided.
 * @param[in] maxs Maximum number of samples to read.
 * @param[in] handle Instance handle related to the samples to read.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The instance handle has not been registered with this reader.
 */
DDS_EXPORT dds_return_t
dds_take_instance(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs,
  dds_instance_handle_t handle);

/**
 * @brief Take data for a specific instance from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * @deprecated Alias for @ref dds_take_instance where `bufsz` = `maxs`.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of `maxs` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] maxs Maximum number of samples to read.
 * @param[in] handle Instance handle related to the samples to read.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The instance handle has not been registered with this reader.
 */
DDS_DEPRECATED_EXPORT dds_return_t
dds_take_instance_wl(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  uint32_t maxs,
  dds_instance_handle_t handle);

/**
 * @brief Take data for a specific instance matching sample/view/instance states from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * See @ref dds_take. The matching criterion referred to there is that:
 * - the instance handle must equal the `handle` parameter; and
 * - the sample/view/instance states must match the specification in the `mask` parameter.
 *
 * If the sample/view/instance state component in the mask is 0 and `reader_or_condition`
 * references a data reader (as opposed to a read or query condition), it is treated as
 * equivalent to any sample/view/instance state. If `reader_or_condition` references a
 * read or query condition, the matching states are the union of `mask` and the
 * condition's mask.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of  `bufsz` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] bufsz The size of buffer provided.
 * @param[in] maxs Maximum number of samples to read.
 * @param[in] handle Instance handle related to the samples to read.
 * @param[in] mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The instance handle has not been registered with this reader.
 */
DDS_EXPORT dds_return_t
dds_take_instance_mask(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs,
  dds_instance_handle_t handle,
  uint32_t mask);

/**
 * @brief Take data for a specific instance matching sample/view/instance states from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * @deprecated Alias for @ref dds_take_instance_mask where `bufsz` = `maxs`.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of `maxs` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] maxs Maximum number of samples to read.
 * @param[in] handle Instance handle related to the samples to read.
 * @param[in] mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns A dds_return_t with the number of samples or an error code.
 *
 * @retval >= 0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The instance handle has not been registered with this reader.
 */
DDS_DEPRECATED_EXPORT dds_return_t
dds_take_instance_mask_wl(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  uint32_t maxs,
  dds_instance_handle_t handle,
  uint32_t mask);

/**
 * @brief Take data for a specific instance from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * See @ref dds_take. The matching criterion referred to there is that the instance
 * handle must equal the `handle` parameter.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of `bufsz` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] bufsz The size of buffer provided.
 * @param[in] maxs Maximum number of samples to read.
 * @param[in] previous_handle Instance handle right before the next instance from which the samples will be read.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_NO_DATA
 *             The entity has no more data to be read.
 */
DDS_EXPORT dds_return_t
dds_take_next_instance(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs,
  dds_instance_handle_t previous_handle);

/**
 * @brief Take data for a specific instance matching sample/view/instance states from the data reader, read or query condition
 * @ingroup reading
 * @component read_data
 *
 * See @ref dds_take. The matching criterion referred to there is that:
 * - the instance handle must equal the `handle` parameter; and
 * - the sample/view/instance states must match the specification in the `mask` parameter.
 *
 * If the sample/view/instance state component in the mask is 0 and `reader_or_condition`
 * references a data reader (as opposed to a read or query condition), it is treated as
 * equivalent to any sample/view/instance state. If `reader_or_condition` references a
 * read or query condition, the matching states are the union of `mask` and the
 * condition's mask.
 *
 * @param[in] reader_or_condition Reader, readcondition or querycondition entity.
 * @param[in,out] buf An array of  `bufsz` pointers to samples.
 * @param[out] si Pointer to an array of @ref dds_sample_info_t returned for each data value.
 * @param[in] bufsz The size of buffer provided.
 * @param[in] maxs Maximum number of samples to read.
 * @param[in] previous_handle Instance handle related to the samples to read.
 * @param[in] mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_NO_DATA
 *             The entity has no more data to be read 
 */
DDS_EXPORT dds_return_t
dds_take_next_instance_mask(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs,
  dds_instance_handle_t previous_handle,
  uint32_t mask);

/**
 * @brief Take the first unread sample
 * @ingroup reading
 * @component read_data
 *
 * Equivalent to `dds_take_mask(reader, buf, si, 1, 1, DDS_NOT_READ_SAMPLE_STATE)`.
 *
 * @param[in] reader The reader entity.
 * @param[in,out] buf A pointer to a sample.
 * @param[out] si The pointer to @ref dds_sample_info_t returned for a data value.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The entity parameter is not a valid parameter.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_take_next(
  dds_entity_t reader,
  void **buf,
  dds_sample_info_t *si);

/**
 * @brief Take the first unread sample
 * @ingroup reading
 * @component read_data
 *
 * @deprecated Alias for @ref dds_take_next.
 *
 * @param[in] reader The reader entity.
 * @param[in,out] buf A pointer to a sample.
 * @param[out] si The pointer to @ref dds_sample_info_t returned for a data value.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The entity parameter is not a valid parameter.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_DEPRECATED_EXPORT dds_return_t
dds_take_next_wl(
  dds_entity_t reader,
  void **buf,
  dds_sample_info_t *si);

/**
 * @brief Function type for sample collector argument in read/take-with-collector
 * @ingroup reading
 * @component read_data
 *
 * This defines the function type used by @ref dds_read_with_collector and @ref
 * dds_take_with_collector for passing samples to be included in the result to an
 * application defined function for collecting them in whatever way it needs.  The
 * function is called for each sample while the RHC is locked, and so it is advisable to
 * only perform a small amount of work.  Calling Cyclone DDS API functions is not
 * supported.
 *
 * It is called for the samples in the order compatible with the requirements of the DDS
 * specification, in particular that means instances are contiguous.
 *
 * @param[in] arg A pointer to the application-defined argument passed to read/take
 * @param[in] si A fully initialized sample info object
 * @param[in] st The underlying ddsi_sertype (needed only if si.valid_data is false)
 * @param[in] sd The sample, if si.valid_data is false, the type has been erased (hence the "st" argument)
 *
 * @return An indication of success or failure
 * @retval DDS_RETCODE_OK
 *           the sample was successfully handled and reading can continue
 * @retval otherwise
 *           an error, this will propagated to the caller if it occurs on the first call
 *           otherwise the return valu eof read/take will be the number of samples
 *           successfully collected
 */
typedef dds_return_t (*dds_read_with_collector_fn_t) (
  void *arg,
  const dds_sample_info_t *si,
  const struct ddsi_sertype *st,
  struct ddsi_serdata *sd);

/**
 * @brief Read samples while collecting result in an application-defined way without updating state
 * @ingroup reading
 * @component read_data
 *
 * When using a readcondition or querycondition, their masks are or'd with the given mask.
 *
 * If the sample/view/instance state component in the mask is 0 and there is no read or query condition,
 * to combine it with, it is treated as equivalent to any sample/view/instance state.
 *
 * Collected samples are not marked as read.
 *
 * @param[in] reader_or_condition Handle of a reader or a read/query condition
 * @param[in] maxs Maximum number of samples (1 .. INT32_MAX)
 * @param[in] handle Instance handle or 0 if not reading a specific instance
 * @param[in] mask Sample/view/instance state mask
 * @param[in] collect_sample Function be called for each sample in the result
 * @param[in] collect_sample_arg Arbitrary argument passed to "collect_sample"
 * @return The number of returned samples or an error code
 * @retval > 0 number of samples passed successfully collected by collect_sample
 * @retval 0   success, no matching data
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval < 0 Return value of failing collect_sample on first invocation
 */
DDS_EXPORT dds_return_t
dds_peek_with_collector (
  dds_entity_t reader_or_condition,
  uint32_t maxs,
  dds_instance_handle_t handle,
  uint32_t mask,
  dds_read_with_collector_fn_t collect_sample,
  void *collect_sample_arg);

/**
 * @brief Take samples from the next non-empty instance while collecting result in an application-defined way
 * @ingroup reading
 * @component read_data
 * 
 * See @ref dds_take_with_collector. The matching criterion referred to there is that the instance
 * handle must equal the `handle` parameter.
 *
 * @param[in] reader_or_condition Handle of a reader or a read/query condition
 * @param[in] maxs Maximum number of samples (1 .. INT32_MAX)
 * @param[in] previous_handle Instance handle before the instance for which to read the samples from.
 * @param[in] mask Sample/view/instance state mask
 * @param[in] collect_sample Function be called for each sample in the result
 * @param[in] collect_sample_arg Arbitrary argument passed to "collect_sample"
 * @return The number of returned samples or an error code
 * @retval > 0 number of samples passed successfully collected by collect_sample
 * @retval 0   success, no matching data
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval < 0 Return value of failing collect_sample on first invocation
 */
DDS_EXPORT dds_return_t
dds_peek_next_instance_with_collector (
  dds_entity_t reader_or_condition,
  uint32_t maxs,
  dds_instance_handle_t previous_handle,
  uint32_t mask,
  dds_read_with_collector_fn_t collect_sample,
  void *collect_sample_arg);

/**
 * @brief Read samples while collecting result in an application-defined way
 * @ingroup reading
 * @component read_data
 *
 * When using a readcondition or querycondition, their masks are or'd with the given mask.
 *
 * If the sample/view/instance state component in the mask is 0 and there is no read or query condition,
 * to combine it with, it is treated as equivalent to any sample/view/instance state.
 *
 * Collected samples are marked as read.
 *
 * @param[in] reader_or_condition Handle of a reader or a read/query condition
 * @param[in] maxs Maximum number of samples (1 .. INT32_MAX)
 * @param[in] handle Instance handle or 0 if not reading a specific instance
 * @param[in] mask Sample/view/instance state mask
 * @param[in] collect_sample Function be called for each sample in the result
 * @param[in] collect_sample_arg Arbitrary argument passed to "collect_sample"
 * @return The number of returned samples or an error code
 * @retval > 0 number of samples passed successfully collected by collect_sample
 * @retval 0   success, no matching data
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval < 0 Return value of failing collect_sample on first invocation
 */
DDS_EXPORT dds_return_t
dds_read_with_collector (
  dds_entity_t reader_or_condition,
  uint32_t maxs,
  dds_instance_handle_t handle,
  uint32_t mask,
  dds_read_with_collector_fn_t collect_sample,
  void *collect_sample_arg);

/**
 * @brief Read samples from the next instance while collecting result in an application-defined way
 * @ingroup reading
 * @component read_data
 *
 * See @ref dds_take_with_collector. The matching criterion referred to there is that the instance
 * handle must equal the `handle` parameter.
 *
 * @param[in] reader_or_condition Handle of a reader or a read/query condition
 * @param[in] maxs Maximum number of samples (1 .. INT32_MAX)
 * @param[in] previous_handle Instance handle before the instance for which to read the samples from.
 * @param[in] mask Sample/view/instance state mask
 * @param[in] collect_sample Function be called for each sample in the result
 * @param[in] collect_sample_arg Arbitrary argument passed to "collect_sample"
 * @return The number of returned samples or an error code
 * @retval > 0 number of samples passed successfully collected by collect_sample
 * @retval 0   success, no matching data
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval < 0 Return value of failing collect_sample on first invocation
 */
DDS_EXPORT dds_return_t
dds_read_next_instance_with_collector (
  dds_entity_t reader_or_condition,
  uint32_t maxs,
  dds_instance_handle_t previous_handle,
  uint32_t mask,
  dds_read_with_collector_fn_t collect_sample,
  void *collect_sample_arg);

/**
 * @brief Take samples while collecting result in an application-defined way
 * @ingroup reading
 * @component read_data
 *
 * When using a readcondition or querycondition, their masks are or'd with the given mask.
 *
 * If the sample/view/instance state component in the mask is 0 and there is no read or query condition,
 * to combine it with, it is treated as equivalent to any sample/view/instance state.
 *
 * Collected samples are removed from the history cache.
 *
 * @param[in] reader_or_condition Handle of a reader or a read/query condition
 * @param[in] maxs Maximum number of samples (1 .. INT32_MAX)
 * @param[in] handle Instance handle or 0 if not taking from a specific instance
 * @param[in] mask Sample/view/instance state mask
 * @param[in] collect_sample Function be called for each sample in the result
 * @param[in] collect_sample_arg Arbitrary argument passed to "collect_sample"
 * @return The number of returned samples or an error code
 * @retval > 0 number of samples passed successfully collected by collect_sample
 * @retval 0   success, no matching data
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval < 0 Return value of failing collect_sample on first invocation
 */
DDS_EXPORT dds_return_t
dds_take_with_collector (
  dds_entity_t reader_or_condition,
  uint32_t maxs,
  dds_instance_handle_t handle,
  uint32_t mask,
  dds_read_with_collector_fn_t collect_sample,
  void *collect_sample_arg);

/**
 * @brief Take samples from the next non-empty instance while collecting result in an application-defined way
 * @ingroup reading
 * @component read_data
 * 
 * See @ref dds_take_with_collector. The matching criterion referred to there is that the instance
 * handle must equal the `handle` parameter.
 *
 * @param[in] reader_or_condition Handle of a reader or a read/query condition
 * @param[in] maxs Maximum number of samples (1 .. INT32_MAX)
 * @param[in] previous_handle Instance handle before the instance for which to read the samples from.
 * @param[in] mask Sample/view/instance state mask
 * @param[in] collect_sample Function be called for each sample in the result
 * @param[in] collect_sample_arg Arbitrary argument passed to "collect_sample"
 * @return The number of returned samples or an error code
 * @retval > 0 number of samples passed successfully collected by collect_sample
 * @retval 0   success, no matching data
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval < 0 Return value of failing collect_sample on first invocation
 */
DDS_EXPORT dds_return_t
dds_take_next_instance_with_collector (
  dds_entity_t reader_or_condition,
  uint32_t maxs,
  dds_instance_handle_t previous_handle,
  uint32_t mask,
  dds_read_with_collector_fn_t collect_sample,
  void *collect_sample_arg);

/**
 * @anchor DDS_HAS_READCDR
 * @ingroup reading
 * @brief Set when function dds_has_readcdr is defined.
 */
#define DDS_HAS_READCDR 1

/**
 * @brief Get references to a representation of the samples in a reader history cache and their accompanying sample infodata values (of same type) without updating state
 * @ingroup reading
 * @component read_data
 *
 * This operation returns references to the internal representation of samples (`struct ddsi_serdata`), which can
 * then be used in a variety of ways. Examples are converting it to application representation and obtaining a copy or a
 * reference of the serialized representation. If the underlying implementation (`struct ddsi_sertype`) is known to
 * the application, other options may exist as well.
 *
 * The data is left in the reader history cache and the sample state and view state of the returned samples and their
 * instances are not updated; @ref dds_readcdr updates these states; @ref dds_takecdr removes the data
 * from the history cache.
 *
 * The returned references must eventually be released by calling @ref ddsi_serdata_unref. There is no guarantee
 * the type pointer survives beyond the existence of the reader from which the references were read.
 *
 * When using a readcondition or querycondition, their masks are or'd with the given mask.
 *
 * If the sample/view/instance state component in the mask is 0 and there is no read or query condition,
 * to combine it with, it is treated as equivalent to any sample/view/instance state.
 *
 * Note that this is a simple wrapper around @ref dds_peek_with_collector.
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity.
 * @param[out] buf Filled with references @ref ddsi_serdata structures.
 * @param[in]  maxs Maximum number of samples to read.
 * @param[out] si Filled with sample info.
 * @param[in]  mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The precondition for this operation is not met.
 */
DDS_EXPORT dds_return_t
dds_peekcdr(
  dds_entity_t reader_or_condition,
  struct ddsi_serdata **buf,
  uint32_t maxs,
  dds_sample_info_t *si,
  uint32_t mask);

/**
 * @brief Get references to a representation of the samples of a specific instance in a reader history cache and their accompanying sample infodata values (of same type) without updating state
 * @ingroup reading
 * @component read_data
 *
 * This operation returns references to the internal representation of samples (`struct ddsi_serdata`), which can
 * then be used in a variety of ways. Examples are converting it to application representation and obtaining a copy or a
 * reference of the serialized representation. If the underlying implementation (`struct ddsi_sertype`) is known to
 * the application, other options may exist as well.
 *
 * The data is left in the reader history cache and the sample state and view state of the returned samples and their
 * instances are not updated; @ref dds_readcdr updates these states; @ref dds_takecdr removes the data
 * from the history cache.
 *
 * The returned references must eventually be released by calling @ref ddsi_serdata_unref. There is no guarantee
 * the type pointer survives beyond the existence of the reader from which the references were read.
 *
 * When using a readcondition or querycondition, their masks are or'd with the given mask.
 *
 * If the sample/view/instance state component in the mask is 0 and there is no read or query condition,
 * to combine it with, it is treated as equivalent to any sample/view/instance state.
 *
 * Note that this is a simple wrapper around @ref dds_peek_with_collector.
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity.
 * @param[out] buf Filled with references @ref ddsi_serdata structures.
 * @param[in]  maxs Maximum number of samples to read.
 * @param[out] si Filled with sample info.
 * @param[in]  handle Handle of instance from which to read samples.
 * @param[in]  mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The instance handle has not been registered with this reader.
 */
DDS_EXPORT dds_return_t
dds_peekcdr_instance (
    dds_entity_t reader_or_condition,
    struct ddsi_serdata **buf,
    uint32_t maxs,
    dds_sample_info_t *si,
    dds_instance_handle_t handle,
    uint32_t mask);

/**
 * @brief Get references to a representation of the samples in a reader history cache and their accompanying sample infodata values (of same type) and marking them as read
 * @ingroup reading
 * @component read_data
 *
 * This operation returns references to the internal representation of samples (`struct ddsi_serdata`), which can
 * then be used in a variety of ways. Examples are converting it to application representation and obtaining a copy or a
 * reference of the serialized representation. If the underlying implementation (`struct ddsi_sertype`) is known to
 * the application, other options may exist as well.
 *
 * The data is left in the reader history cache and the sample state and view state of the returned samples and their
 * instances are updated; @ref dds_peekcdr returns the data without updating these states; @ref dds_takecdr
 * removes the data from the history cache.
 *
 * The returned references must eventually be released by calling @ref ddsi_serdata_unref. There is no guarantee
 * the type pointer survives beyond the existence of the reader from which the references were read.
 *
 * When using a readcondition or querycondition, their masks are or'd with the given mask.
 *
 * If the sample/view/instance state component in the mask is 0 and there is no read or query condition,
 * to combine it with, it is treated as equivalent to any sample/view/instance state.
 *
 * Note that this is a simple wrapper around @ref dds_read_with_collector.
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity.
 * @param[out] buf Filled with references @ref ddsi_serdata structures.
 * @param[in]  maxs Maximum number of samples to read.
 * @param[out] si Filled with sample info.
 * @param[in]  mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The precondition for this operation is not met.
 */
DDS_EXPORT dds_return_t
dds_readcdr(
  dds_entity_t reader_or_condition,
  struct ddsi_serdata **buf,
  uint32_t maxs,
  dds_sample_info_t *si,
  uint32_t mask);

/**
 * @brief Get references to a representation of the samples of a specific instance in a reader history cache and their accompanying sample infodata values (of same type) and marking them as read
 * @ingroup reading
 * @component read_data
 *
 * This operation returns references to the internal representation of samples (`struct ddsi_serdata`), which can
 * then be used in a variety of ways. Examples are converting it to application representation and obtaining a copy or a
 * reference of the serialized representation. If the underlying implementation (`struct ddsi_sertype`) is known to
 * the application, other options may exist as well.
 *
 * The data is left in the reader history cache and the sample state and view state of the returned samples and their
 * instances are updated; @ref dds_peekcdr returns the data without updating these states; @ref dds_takecdr
 * removes the data from the history cache.
 *
 * The returned references must eventually be released by calling @ref ddsi_serdata_unref. There is no guarantee
 * the type pointer survives beyond the existence of the reader from which the references were read.
 *
 * When using a readcondition or querycondition, their masks are or'd with the given mask.
 *
 * If the sample/view/instance state component in the mask is 0 and there is no read or query condition,
 * to combine it with, it is treated as equivalent to any sample/view/instance state.
 *
 * Note that this is a simple wrapper around @ref dds_read_with_collector.
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity.
 * @param[out] buf Filled with references @ref ddsi_serdata structures.
 * @param[in]  maxs Maximum number of samples to read.
 * @param[out] si Filled with sample info.
 * @param[in]  handle Handle of instance from which to read samples.
 * @param[in]  mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The instance handle has not been registered with this reader.
 */
DDS_EXPORT dds_return_t
dds_readcdr_instance (
    dds_entity_t reader_or_condition,
    struct ddsi_serdata **buf,
    uint32_t maxs,
    dds_sample_info_t *si,
    dds_instance_handle_t handle,
    uint32_t mask);

/**
 * @brief Get references to a representation of the samples in a reader history cache and their accompanying sample infodata values (of same type) and remove them from the cache
 * @ingroup reading
 * @component read_data
 *
 * This operation returns references to the internal representation of samples (`struct ddsi_serdata`), which can
 * then be used in a variety of ways. Examples are converting it to application representation and obtaining a copy or a
 * reference of the serialized representation. If the underlying implementation (`struct ddsi_sertype`) is known to
 * the application, other options may exist as well.
 *
 * The data is removed from the reader history cache; @ref dds_peekcdr leaves them in and leaves the sample and
 * view states unchanged; @ref dds_readcdr leaves the data in the cache but does update the sample and view
 * states.
 *
 * The returned references must eventually be released by calling @ref ddsi_serdata_unref. There is no guarantee
 * the type pointer survives beyond the existence of the reader from which the references were read.
 *
 * When using a readcondition or querycondition, their masks are or'd with the given mask.
 *
 * If the sample/view/instance state component in the mask is 0 and there is no read or query condition,
 * to combine it with, it is treated as equivalent to any sample/view/instance state.
 *
 * Note that this is a simple wrapper around @ref dds_take_with_collector.
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity.
 * @param[out] buf Filled with references @ref ddsi_serdata structures.
 * @param[in]  maxs Maximum number of samples to read.
 * @param[out] si Filled with sample info.
 * @param[in]  mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The precondition for this operation is not met.
 */
DDS_EXPORT dds_return_t
dds_takecdr(
  dds_entity_t reader_or_condition,
  struct ddsi_serdata **buf,
  uint32_t maxs,
  dds_sample_info_t *si,
  uint32_t mask);

/**
 * @brief Get references to a representation of the samples of a specific instance in a reader history cache and their accompanying sample infodata values (of same type) and remove them from the cache
 * @ingroup reading
 * @component read_data
 *
 * This operation returns references to the internal representation of samples (`struct ddsi_serdata`), which can
 * then be used in a variety of ways. Examples are converting it to application representation and obtaining a copy or a
 * reference of the serialized representation. If the underlying implementation (`struct ddsi_sertype`) is known to
 * the application, other options may exist as well.
 *
 * The data is removed from the reader history cache; @ref dds_peekcdr leaves them in and leaves the sample and
 * view states unchanged; @ref dds_readcdr leaves the data in the cache but does update the sample and view
 * states.
 *
 * The returned references must eventually be released by calling @ref ddsi_serdata_unref. There is no guarantee
 * the type pointer survives beyond the existence of the reader from which the references were read.
 *
 * When using a readcondition or querycondition, their masks are or'd with the given mask.
 *
 * If the sample/view/instance state component in the mask is 0 and there is no read or query condition,
 * to combine it with, it is treated as equivalent to any sample/view/instance state.
 *
 * Note that this is a simple wrapper around @ref dds_take_with_collector.
 *
 * @param[in]  reader_or_condition Reader, readcondition or querycondition entity.
 * @param[out] buf Filled with references @ref ddsi_serdata structures.
 * @param[in]  maxs Maximum number of samples to read.
 * @param[out] si Filled with sample info.
 * @param[in]  handle Handle of instance from which to read samples.
 * @param[in]  mask Filter the data based on dds_sample_state_t|dds_view_state_t|dds_instance_state_t.
 *
 * @returns A dds_return_t with the number of samples read or an error code.
 *
 * @retval >=0
 *             Number of samples read.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the given arguments is not valid.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             The instance handle has not been registered with this reader.
 */
DDS_EXPORT dds_return_t
dds_takecdr_instance (
    dds_entity_t reader_or_condition,
    struct ddsi_serdata **buf,
    uint32_t maxs,
    dds_sample_info_t *si,
    dds_instance_handle_t handle,
    uint32_t mask);

/**
 * @defgroup instance_handle (Instance Handles)
 * @ingroup dds
 * Instance handle <=> key value mapping.
 * Functions exactly as read w.r.t. treatment of data
 * parameter. On output, only key values set.
 * @code{c}
 * T x = { ... };
 * T y;
 * dds_instance_handle_t ih;
 * ih = dds_lookup_instance (e, &x);
 * dds_instance_get_key (e, ih, &y);
 * @endcode
*/

/**
 * @brief This operation takes a sample and returns an instance handle to be used for subsequent operations.
 * @ingroup instance_handle
 * @component data_instance
 *
 * @param[in]  entity Reader or Writer entity.
 * @param[in]  data   Sample with a key fields set.
 *
 * @returns instance handle or DDS_HANDLE_NIL if instance could not be found from key.
 */
DDS_EXPORT dds_instance_handle_t
dds_lookup_instance(dds_entity_t entity, const void *data);

/**
 * @brief This operation takes an instance handle and return a key-value corresponding to it.
 * @ingroup instance_handle
 * @component data_instance
 *
 * @param[in]  entity Reader, writer, readcondition or querycondition entity.
 * @param[in]  ih     Instance handle.
 * @param[out] data   pointer to an instance, to which the key ID corresponding to the instance handle will be
 *    returned, the sample in the instance should be ignored.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One of the parameters was invalid or the topic does not exist.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 *
 * DOC_TODO: Check return codes for completeness
 */
DDS_EXPORT dds_return_t
dds_instance_get_key(
  dds_entity_t entity,
  dds_instance_handle_t ih,
  void *data);

/**
 * @brief Begin coherent publishing or begin accessing a coherent set in a subscriber
 * @ingroup publication
 * @component coherent_sets
 *
 * Invoking on a Writer or Reader behaves as if dds_begin_coherent was invoked on its parent
 * Publisher or Subscriber respectively.
 *
 * @param[in]  entity The entity that is prepared for coherent access.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_ERROR
 *             An internal error has occurred.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The provided entity is invalid or not supported.
 */
DDS_EXPORT dds_return_t
dds_begin_coherent(dds_entity_t entity);

/**
 * @brief End coherent publishing or end accessing a coherent set in a subscriber
 * @ingroup publication
 * @component coherent_sets
 *
 * Invoking on a Writer or Reader behaves as if dds_end_coherent was invoked on its parent
 * Publisher or Subscriber respectively.
 *
 * @param[in] entity The entity on which coherent access is finished.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The provided entity is invalid or not supported.
 */
DDS_EXPORT dds_return_t
dds_end_coherent(dds_entity_t entity);

/**
 * @brief Trigger DATA_AVAILABLE event on contained readers
 * @ingroup subscriber
 * @component subscriber
 *
 * The DATA_AVAILABLE event is broadcast to all readers owned by this subscriber that currently
 * have new data available. Any on_data_available listener callbacks attached to respective
 * readers are invoked.
 *
 * @param[in] subscriber A valid subscriber handle.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The provided subscriber is invalid.
 */
DDS_EXPORT dds_return_t
dds_notify_readers(dds_entity_t subscriber);

/**
 * @brief Checks whether the entity has one of its enabled statuses triggered.
 * @ingroup entity
 * @component entity_status
 *
 * @param[in]  entity  Entity for which to check for triggered status.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The entity parameter is not a valid parameter.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_EXPORT dds_return_t
dds_triggered(dds_entity_t entity);

/**
 * @brief Get the topic
 * @ingroup entity
 * @component entity_relations
 *
 * This operation returns a topic (handle) when the function call is done
 * with reader, writer, read condition or query condition. For instance, it
 * will return the topic when it is used for creating the reader or writer.
 * For the conditions, it returns the topic that is used for creating the reader
 * which was used to create the condition.
 *
 * @param[in] entity The entity.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The entity parameter is not a valid parameter.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_ALREADY_DELETED
 *             The entity has already been deleted.
 */
DDS_EXPORT dds_entity_t
dds_get_topic(dds_entity_t entity);

/**
 * @brief Get instance handles of the data readers matching a writer
 * @ingroup builtintopic
 * @component writer
 *
 * This operation fills the provided array with the instance handles
 * of the data readers that match the writer.  On successful output,
 * the number of entries of "rds" set is the minimum of the return
 * value and the value of "nrds".
 *
 * @param[in] writer   The writer.
 * @param[in] rds      The array to be filled.
 * @param[in] nrds     The size of the rds array, at most the first
 *                     nrds entries will be filled.  rds = NULL and nrds = 0
 *                     is a valid way of determining the number of matched
 *                     readers, but inefficient compared to relying on the
 *                     matched publication status.
 *
 * @returns A dds_return_t indicating the number of matched readers
 *             or failure.  The return value may be larger than nrds
 *             if there are more matching readers than the array can
 *             hold.
 *
 * @retval >=0
 *             The number of matching readers.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The entity parameter is not valid or rds = NULL and
 *             nrds > 0.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 */
DDS_EXPORT dds_return_t
dds_get_matched_subscriptions (
  dds_entity_t writer,
  dds_instance_handle_t *rds,
  size_t nrds);

/**
 * @brief Get a description of a reader matched with the provided writer
 * @ingroup builtintopic
 * @component writer
 *
 * This operation looks up the reader instance handle in the set of
 * readers matched with the specified writer, returning a freshly
 * allocated sample of the DCPSSubscription built-in topic if found,
 * and NULL if not.  The caller is responsible for freeing the
 * memory allocated, e.g. using dds_builtintopic_free_endpoint.
 *
 * This operation is similar to performing a read of the given
 * instance handle on a reader of the DCPSSubscription built-in
 * topic, but this operation additionally filters on whether the
 * reader is matched by the provided writer.
 *
 * @param[in] writer   The writer.
 * @param[in] ih       The instance handle of a reader.
 *
 * @returns A newly allocated sample containing the information on the
 *             reader, or a NULL pointer for any kind of failure.
 *
 * @retval != NULL
 *             The requested data
 * @retval NULL
 *             The writer is not valid or ih is not an instance handle
 *             of a matched reader.
 */
DDS_EXPORT dds_builtintopic_endpoint_t *
dds_get_matched_subscription_data (
  dds_entity_t writer,
  dds_instance_handle_t ih);

/**
 * @brief Get instance handles of the data writers matching a reader
 * @ingroup builtintopic
 * @component reader
 *
 * This operation fills the provided array with the instance handles
 * of the data writers that match the reader.  On successful output,
 * the number of entries of "wrs" set is the minimum of the return
 * value and the value of "nwrs".
 *
 * @param[in] reader   The reader.
 * @param[in] wrs      The array to be filled.
 * @param[in] nwrs     The size of the wrs array, at most the first
 *             nwrs entries will be filled.  wrs = NULL and wrds = 0
 *             is a valid way of determining the number of matched
 *             readers, but inefficient compared to relying on the
 *             matched publication status.
 *
 * @returns A dds_return_t indicating the number of matched writers
 *             or failure.  The return value may be larger than nwrs
 *             if there are more matching writers than the array can
 *             hold.
 *
 * @retval >=0
 *             The number of matching writers.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The entity parameter is not valid or wrs = NULL and
 *             nwrs > 0.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 */
DDS_EXPORT dds_return_t
dds_get_matched_publications (
  dds_entity_t reader,
  dds_instance_handle_t *wrs,
  size_t nwrs);

/**
 * @brief Get a description of a writer matched with the provided reader
 * @ingroup builtintopic
 * @component reader
 *
 * This operation looks up the writer instance handle in the set of
 * writers matched with the specified reader, returning a freshly
 * allocated sample of the DCPSPublication built-in topic if found,
 * and NULL if not.  The caller is responsible for freeing the
 * memory allocated, e.g. using dds_builtintopic_free_endpoint.
 *
 * This operation is similar to performing a read of the given
 * instance handle on a reader of the DCPSPublication built-in
 * topic, but this operation additionally filters on whether the
 * writer is matched by the provided reader.
 *
 * @param[in] reader   The reader.
 * @param[in] ih       The instance handle of a writer.
 *
 * @returns A newly allocated sample containing the information on the
 *             writer, or a NULL pointer for any kind of failure.
 *
 * @retval != NULL
 *             The requested data
 * @retval NULL
 *             The reader is not valid or ih is not an instance handle
 *             of a matched writer.
 */
DDS_EXPORT dds_builtintopic_endpoint_t *
dds_get_matched_publication_data (
  dds_entity_t reader,
  dds_instance_handle_t ih);

#ifdef DDS_HAS_TYPELIB
/**
 * @brief Gets the type information from endpoint information that was
 *        retrieved by dds_get_matched_subscription_data or
 *        dds_get_matched_publication_data
 * @ingroup builtintopic
 * @component builtin_topic
 *
 * @param[in] builtintopic_endpoint  The builtintopic endpoint struct
 * @param[out] type_info             Type information that will be allocated by this function in case of success.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             One or more parameters are invalid
 */
DDS_EXPORT dds_return_t
dds_builtintopic_get_endpoint_type_info (
  dds_builtintopic_endpoint_t * builtintopic_endpoint,
  const dds_typeinfo_t ** type_info);

#endif /* DDS_HAS_TYPELIB */

/**
 * @brief Free the endpoint information that was retrieved by
 *        dds_get_matched_subscription_data or dds_get_matched_publication_data
 * @ingroup builtintopic
 * @component builtin_topic
 *
 * This operation deallocates the memory of the fields in a
 * dds_builtintopic_endpoint_t struct and deallocates the
 * struct itself.
 *
 * @param[in] builtintopic_endpoint  The builtintopic endpoint struct
 */
DDS_EXPORT void
dds_builtintopic_free_endpoint (
  dds_builtintopic_endpoint_t * builtintopic_endpoint);

/**
 * @brief Free the provided topic information
 * @ingroup builtintopic
 * @component builtin_topic
 *
 * This operation deallocates the memory of the fields in a
 * dds_builtintopic_topic_t struct and deallocates the
 * struct itself.
 *
 * @param[in] builtintopic_topic  The builtintopic topic struct
 */
DDS_EXPORT void
dds_builtintopic_free_topic (
  dds_builtintopic_topic_t * builtintopic_topic);

/**
 * @brief Free the provided participant information
 * @ingroup builtintopic
 * @component builtin_topic
 *
 * This operation deallocates the memory of the fields in a
 * dds_builtintopic_participant_t struct and deallocates the
 * struct itself.
 *
 * @param[in] builtintopic_participant  The builtintopic participant struct
 */
DDS_EXPORT void
dds_builtintopic_free_participant (
  dds_builtintopic_participant_t * builtintopic_participant);

/**
 * @brief This operation manually asserts the liveliness of a writer
 *        or domain participant.
 * @ingroup entity
 * @component participant
 *
 * This operation manually asserts the liveliness of a writer
 * or domain participant. This is used in combination with the Liveliness
 * QoS policy to indicate that the entity remains active. This operation need
 * only be used if the liveliness kind in the QoS is either
 * DDS_LIVELINESS_MANUAL_BY_PARTICIPANT or DDS_LIVELINESS_MANUAL_BY_TOPIC.
 *
 * @param[in] entity  A domain participant or writer
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 */
DDS_EXPORT dds_return_t
dds_assert_liveliness (
  dds_entity_t entity);


/**
 * @defgroup internal (Internal)
 * @ingroup dds
 */

/**
 * @defgroup testing (Testing tools)
 * @ingroup internal
 */

/**
 *
 * @brief This operation allows making the domain's network stack temporarily deaf and/or mute.
 * @ingroup testing
 * @component domain
 * @warning Unstable API, for testing
 * @unstable
 *
 * This is a support function for testing and, other special uses and is subject to change.
 *
 * @param[in] entity  A domain entity or an entity bound to a domain, such
 *                    as a participant, reader or writer.
 * @param[in] deaf    Whether to network stack should pretend to be deaf and
 *                    ignore any incoming packets.
 * @param[in] mute    Whether to network stack should pretend to be mute and
 *                    discard any outgoing packets where it normally would.
 *                    pass them to the operating system kernel for transmission.
 * @param[in] reset_after  Any value less than INFINITY will cause it to
 *                    set deaf = mute = false after reset_after ns have passed.
 *                    This is done by an event scheduled for the appropriate
 *                    time and otherwise forgotten. These events are not
 *                    affected by subsequent calls to this function.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The entity parameter is not a valid parameter.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
*/
DDS_EXPORT dds_return_t
dds_domain_set_deafmute (
  dds_entity_t entity,
  bool deaf,
  bool mute,
  dds_duration_t reset_after);


/**
 * @defgroup xtypes (XTypes)
 * @ingroup dds
 *
 * CycloneDDS supports XTypes, but most of that functionality outside the new IDL constructs
 * happens behind the scenes. However, some API functionality is added that allows inspecting
 * types at runtime. Using it in C is not very ergonomic, but dynamic languages like Python can
 * make good use of it.
 */

/**
 * @brief This function resolves the type for the provided type identifier,
 * which can e.g. be retrieved from endpoint or topic discovery data.
 * @ingroup xtypes
 * @component type_metadata
 *
 * @param[in]   entity              A domain entity or an entity bound to a domain, such
 *                                  as a participant, reader or writer.
 * @param[in]   type_id             Type identifier
 * @param[in]   timeout             Timeout for waiting for requested type information to be available
 * @param[out]  type_obj            The type information, untouched if type is not resolved
 *
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The entity parameter is not a valid parameter, type_id or type name
 *             is not provided, or the sertype out parameter is NULL
 * @retval DDS_RETCODE_NOT_FOUND
 *             A type with the provided type_id and type_name was not found
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_UNSUPPORTED
 *             Cyclone DDS built without type discovery
 *             (cf. DDS_HAS_TYPE_DISCOVERY)
*/
DDS_EXPORT dds_return_t
dds_get_typeobj (
  dds_entity_t entity,
  const dds_typeid_t *type_id,
  dds_duration_t timeout,
  dds_typeobj_t **type_obj);

/**
 * @brief Free the type object that was retrieved using dds_get_typeobj
 * @ingroup xtypes
 * @component type_metadata
 *
 * @param[in]  type_obj     The type object
 *
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The type_obj parameter is NULL
 * @retval DDS_RETCODE_UNSUPPORTED
 *             Cyclone DDS built without type discovery
 *             (cf. DDS_HAS_TYPE_DISCOVERY)
*/
DDS_EXPORT dds_return_t
dds_free_typeobj (
  dds_typeobj_t *type_obj);

/**
 * @brief This function gets the type information from the
 * provided topic, reader or writer
 * @ingroup xtypes
 * @component type_metadata
 *
 * @param[in]   entity          A topic/reader/writer entity
 * @param[out]  type_info       Receives the type information on success. On error, `*type_info` is unchanged.
 *                              Use @ref dds_free_typeinfo to free the type information.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The type_info parameter is null
 * @retval DDS_RETCODE_NOT_FOUND
 *             The entity does not have type information set
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             The operation is invoked on an inappropriate object.
 * @retval DDS_RETCODE_UNSUPPORTED
 *             Cyclone DDS built without the type library
 *             (cf. DDS_HAS_TYPELIB)
*/
DDS_EXPORT dds_return_t
dds_get_typeinfo (
  dds_entity_t entity,
  dds_typeinfo_t **type_info);

/**
 * @brief Free the type information that was retrieved using dds_get_typeinfo
 * @ingroup xtypes
 * @component type_metadata
 *
 * @param[in]  type_info     The type information
 *
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The type_info parameter is NULL
 * @retval DDS_RETCODE_UNSUPPORTED
 *             Cyclone DDS built without type discovery
 *             (cf. DDS_HAS_TYPELIB)
*/
DDS_EXPORT dds_return_t
dds_free_typeinfo (
  dds_typeinfo_t *type_info);


/**
 * @brief Gets the sertype of an entity
 *
 * The provided entity must be a topic or endpoint. This function returns a pointer to
 * the sertype of the entity. The refcount of the sertype is not incremented. The lifetime
 * of the returned sertype pointer is at least that of the lifetime of the entity on which
 * it was invoked.
 *
 * @param[in] entity A topic, reader or writer entity
 * @param[out] sertype A pointer to the entity's sertype is stored in this parameter (see note above on lifetime of this pointer)
 *
 * @returns A dds_return_t indicating success or failure.
 * @retval DDS_RETCODE_OK
 *             The operation was successful.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             The sertype parameter is NULL
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             Not a topic, reader or writer entity
 */
DDS_EXPORT dds_return_t
dds_get_entity_sertype (
  dds_entity_t entity,
  const struct ddsi_sertype **sertype);

#if defined (__cplusplus)
}
#endif
#endif /* DDS_H */
