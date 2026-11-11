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
 * @brief Builds a topic descriptor from a complete type and its dependencies.
 * @ingroup topic
 * @component topic
 *
 * @param[in] scope DDS_FIND_SCOPE_LOCAL_DOMAIN waits for local type information.
 *            DDS_FIND_SCOPE_GLOBAL may request missing types from other participants
 *            and requires DDS_HAS_TYPE_DISCOVERY.
 * @param[in] participant Participant whose domain supplies the type library.
 * @param[in] type_info Type information containing the complete type identifier.
 * @param[in] timeout Maximum wait in nanoseconds. Zero polls and DDS_INFINITY waits indefinitely.
 * @param[out] descriptor Receives the allocated descriptor on success. On error,
 *             `*descriptor` is unchanged. Free the result with @ref dds_delete_topic_descriptor.
 *
 * @retval DDS_RETCODE_OK The descriptor is ready to use.
 * @retval DDS_RETCODE_BAD_PARAMETER Invalid scope, participant, type identifier,
 *         negative timeout, or null type information or output pointer.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET The library is uninitialized, the type
 *         identifier is unknown in the domain, or a type request could not be issued.
 * @retval DDS_RETCODE_TIMEOUT The known type or a dependency is still unresolved.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES Descriptor allocation failed.
 * @retval DDS_RETCODE_UNSUPPORTED The build lacks DDS_HAS_TYPELIB or the type
 *         uses a feature unsupported by the descriptor builder.
 */
DDS_EXPORT dds_return_t
dds_create_topic_descriptor (dds_find_scope_t scope, dds_entity_t participant, const dds_typeinfo_t *type_info, dds_duration_t timeout, dds_topic_descriptor_t **descriptor);

/**
 * @brief Frees a descriptor returned by @ref dds_create_topic_descriptor.
 * @ingroup topic
 * @component topic
 *
 * @param[in] descriptor Caller-owned descriptor.
 *
 * @retval DDS_RETCODE_OK The descriptor was freed.
 * @retval DDS_RETCODE_BAD_PARAMETER The descriptor is NULL.
 * @retval DDS_RETCODE_UNSUPPORTED The build lacks DDS_HAS_TYPELIB.
 */
DDS_EXPORT dds_return_t
dds_delete_topic_descriptor (dds_topic_descriptor_t *descriptor);

/**
 * @brief Copies the topic name into a caller-supplied buffer.
 * @ingroup topic
 * @component topic
 *
 * @param[in] topic Topic handle or built-in topic pseudo-handle.
 * @param[out] name Buffer for the NUL-terminated topic name. On error the buffer is unchanged.
 * @param[in] size Buffer capacity in bytes, including space for the terminator.
 *
 * @return The full length in bytes excluding the terminator. A result greater
 *         than or equal to `size` means that the name was truncated.
 * @retval DDS_RETCODE_BAD_PARAMETER The buffer is NULL, the capacity is zero,
 *         or the topic handle is invalid.
 * @return Other errors as described in @ref entity_handle_errors.
 */
DDS_EXPORT dds_return_t
dds_get_name(dds_entity_t topic, char *name, size_t size);

/**
 * @brief Copies the type name into a caller-supplied buffer.
 * @ingroup topic
 * @component topic
 *
 * @param[in] topic Topic handle or built-in topic pseudo-handle.
 * @param[out] name Buffer for the NUL-terminated type name. On error the buffer is unchanged.
 * @param[in] size Buffer capacity in bytes, including space for the terminator.
 *
 * @return The full length in bytes excluding the terminator. A result greater
 *         than or equal to `size` means that the name was truncated.
 * @retval DDS_RETCODE_BAD_PARAMETER The buffer is NULL, the capacity is zero,
 *         or the topic handle is invalid.
 * @return Other errors as described in @ref entity_handle_errors.
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
 * A writer applies the filter to data samples and bypasses it for key-only
 * operations. DDS_TOPIC_FILTER_SAMPLEINFO_ARG is ignored by writers.
 * DDS_TOPIC_FILTER_SAMPLE_SAMPLEINFO_ARG receives zeroed sample information
 * on the writer.
 *
 * In the default reader history the filter sees valid, unread data with the
 * writer's publication handle, the source timestamp, the instance handle and
 * zero ranks. For an existing instance it sees the current view state,
 * instance state and generation counts. For a new instance it sees
 * DDS_NEW_VIEW_STATE, DDS_ALIVE_INSTANCE_STATE and zero generation counts.
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
 * @brief Sets or clears a sample filter on a topic.
 * @anchor dds_set_topic_filter_and_arg
 * @ingroup topic_filter
 * @component topic
 * @warning Unstable API. Set the filter before creating readers or writers using
 *          this topic. To change it later, ensure that no writer calls or
 *          incoming deliveries to readers can use the filter during the change.
 *
 * @param[in] topic Topic to filter.
 * @param[in] filter Sample callback, or NULL to clear the filter.
 * @param[in] arg Argument passed to the callback.
 *
 * @retval DDS_RETCODE_OK The filter was set or cleared.
 * @return Other errors as described in @ref entity_handle_errors.
 */
DDS_EXPORT dds_return_t
dds_set_topic_filter_and_arg(
  dds_entity_t topic,
  dds_topic_filter_arg_fn filter,
  void *arg);

/**
 * @brief Sets the topic filter mode, callback and argument.
 * @anchor dds_set_topic_filter_extended
 * @ingroup topic_filter
 * @component topic
 * @warning Unstable API. Set the filter before creating readers or writers using
 *          this topic. To change it later, ensure that no writer calls or
 *          incoming deliveries to readers can use the filter during the change.
 *
 * @param[in] topic Topic to filter.
 * @param[in] filter Filter specification. DDS_TOPIC_FILTER_NONE clears the filter.
 *
 * @retval DDS_RETCODE_OK The filter was set or cleared.
 * @retval DDS_RETCODE_BAD_PARAMETER The specification is NULL, the mode is
 *         invalid, or an active mode has no callback.
 * @return Other errors as described in @ref entity_handle_errors.
 */
DDS_EXPORT dds_return_t
dds_set_topic_filter_extended(
  dds_entity_t topic,
  const struct dds_topic_filter *filter);

/**
 * @brief Retrieves a sample-and-argument filter.
 * @ingroup topic_filter
 * @component topic
 * @warning Unstable API.
 *
 * @param[in] topic Topic to query.
 * @param[out] fn Receives the callback, or NULL if filtering is disabled. Optional.
 * @param[out] arg Receives the callback argument, or NULL if filtering is disabled. Optional.
 *
 * @retval DDS_RETCODE_OK The requested outputs were set.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET The filter uses a mode other than
 *         DDS_TOPIC_FILTER_NONE or DDS_TOPIC_FILTER_SAMPLE_ARG.
 * @return Other errors as described in @ref entity_handle_errors.
 */
DDS_EXPORT dds_return_t
dds_get_topic_filter_and_arg (
  dds_entity_t topic,
  dds_topic_filter_arg_fn *fn,
  void **arg);

/**
 * @brief Retrieves the topic filter mode, callback and argument.
 * @ingroup topic_filter
 * @component topic
 * @warning Unstable API.
 *
 * @param[in] topic Topic to query.
 * @param[out] filter Receives the filter specification on success.
 *
 * @retval DDS_RETCODE_OK The specification was copied.
 * @retval DDS_RETCODE_BAD_PARAMETER The output pointer is NULL or the topic handle is invalid.
 * @return Other errors as described in @ref entity_handle_errors.
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
 * @retval DDS_RETCODE_BAD_PARAMETER  The participant handle or a subscriber QoS value is invalid.
 *
 * @see @ref entity_handle_errors for errors validating the participant handle.
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
 * @retval DDS_RETCODE_BAD_PARAMETER  The participant handle or a publisher QoS value is invalid.
 *
 * @see @ref entity_handle_errors for errors validating the participant handle.
 */
DDS_EXPORT dds_entity_t
dds_create_publisher(
  dds_entity_t participant,
  const dds_qos_t *qos,
  const dds_listener_t *listener);

/**
 * @brief Suspending publications is unsupported.
 * @ingroup publisher
 * @component publisher
 *
 * @param[in] publisher The publisher.
 *
 * @retval DDS_RETCODE_UNSUPPORTED  The handle identifies a publisher.
 * @see @ref entity_handle_errors for errors validating the publisher handle.
 */
DDS_EXPORT dds_return_t
dds_suspend(dds_entity_t publisher);

/**
 * @brief Resuming publications is unsupported.
 * @ingroup publisher
 * @component publisher
 *
 * @param[in] publisher The publisher.
 *
 * @retval DDS_RETCODE_UNSUPPORTED  The handle identifies a publisher.
 * @see @ref entity_handle_errors for errors validating the publisher handle.
 */
DDS_EXPORT dds_return_t
dds_resume(dds_entity_t publisher);

/**
 * @brief Waits for reliable readers to acknowledge data already written by a writer.
 * @ingroup publication
 * @component publisher
 *
 * The wait covers the writer's sequence number at the start of the wait.
 * It returns immediately if no acknowledgments are outstanding.
 *
 * @param[in] publisher_or_writer A writer or publisher handle.
 * @param[in] timeout Maximum duration to wait, or DDS_INFINITY.
 *
 * @retval DDS_RETCODE_OK  All required acknowledgments have been received.
 * @retval DDS_RETCODE_TIMEOUT  Acknowledgments are still outstanding when the wait ends.
 * @retval DDS_RETCODE_UNSUPPORTED  The handle identifies a publisher.
 * @retval DDS_RETCODE_BAD_PARAMETER  The timeout is negative or the handle is invalid.
 * @see @ref entity_handle_errors for other errors validating the entity handle.
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
 * @returns A valid reader handle or an error code. A PSMX endpoint can also
 * return an error from registering its data availability callback.
 *
 * @retval >0
 *            A valid reader handle.
 * @retval DDS_RETCODE_ERROR
 *            Creation of a PSMX endpoint failed.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            A handle or QoS value is invalid, the selected data representation
 *            or PSMX instance is incompatible with the topic, or the parent
 *            and topic belong to different participants.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *            participant_or_subscriber is neither a participant nor a subscriber,
 *            or the operation is otherwise inappropriate for the supplied entity.
 * @retval DDS_RETCODE_INCONSISTENT_POLICY
 *            The requested QoS is inconsistent, including a built-in topic reader
 *            QoS that is incompatible with the built-in topic writer or has
 *            finite resource limits.
 * @retval DDS_RETCODE_UNSUPPORTED
 *            The requested data representation is XML, or topic discovery is
 *            disabled in this build and @p topic is @ref DDS_BUILTIN_TOPIC_DCPSTOPIC.
 * @retval DDS_RETCODE_NOT_ALLOWED_BY_SECURITY
 *            The configured DDS Security access control plugin denied creation
 *            of the reader.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES
 *            The participant has no available endpoint IDs.
 * @see @ref entity_handle_errors for errors validating the parent and topic handles.
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
 * Parent selection, supported topics and QoS follow @ref dds_create_reader.
 * Argument validation and security checks precede the call to the cache's
 * associate callback. Until that call, the caller owns the cache. Once
 * associated, the cache belongs to the reader, even if a subsequent endpoint
 * setup step returns an error. To determine who owns the cache after an
 * error, have the callback record the association in caller-owned state
 * outside the cache. The caller may free the cache only if the callback was
 * never invoked. A negative result from the callback aborts the process.
 *
 * @param[in]  participant_or_subscriber The participant or subscriber on which the reader is being created.
 * @param[in]  topic                     The topic to read, or a built-in topic pseudo handle.
 * @param[in]  qos                       The QoS to set on the new reader (can be NULL).
 * @param[in]  listener                  Any listener functions associated with the new reader (can be NULL).
 * @param[in]  rhc                       Custom reader history cache. Must be non-null.
 *
 * @returns A valid reader handle or an error code.
 *
 * @retval >0
 *            A valid reader handle.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            The custom history cache is null or an argument is invalid.
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
 *            A valid writer handle.
 * @retval DDS_RETCODE_ERROR
 *            Creation of a PSMX endpoint failed.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *            A handle or QoS value is invalid, the selected data representation
 *            or PSMX instance is incompatible with the topic, or the parent
 *            and topic belong to different participants.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *            participant_or_publisher is neither a participant nor a publisher,
 *            or the operation is otherwise inappropriate for the supplied entity.
 * @retval DDS_RETCODE_INCONSISTENT_POLICY
 *            The requested QoS is inconsistent.
 * @retval DDS_RETCODE_UNSUPPORTED
 *            The requested data representation is XML.
 * @retval DDS_RETCODE_NOT_ALLOWED_BY_SECURITY
 *            The configured DDS Security access control plugin denied creation
 *            of the writer.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES
 *            The participant has no available endpoint IDs.
 * @see @ref entity_handle_errors for errors validating the parent and topic handles.
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
 *
 * @anchor write_source_timestamp
 * Operations without an explicit source timestamp use `dds_time`. A
 * timestamped operation accepts DDS_TIME_INVALID or a nonnegative time in
 * nanoseconds since the Unix epoch. The largest representable time is one
 * nanosecond before 2^31 seconds for RTPS 2.2 and earlier, or one nanosecond
 * before 2^32 seconds for RTPS 2.3 and later. Passing -1 returns
 * DDS_RETCODE_BAD_PARAMETER.
 *
 * @anchor write_sample_loan
 * When the sample is an outstanding loan from this writer, a write,
 * write-dispose, dispose or unregister operation takes ownership after
 * validating the writer and source timestamp. A rejected timestamp leaves
 * the loan with the caller. A topic filter discarding the sample, or a later
 * serialization or delivery failure, consumes the loan. Caller-owned samples
 * remain the caller's responsibility.
 *
 * @anchor write_delivery_errors
 * A write or instance lifecycle update may block while waiting for space in
 * the writer history or in a local reader's history. DDS_RETCODE_TIMEOUT
 * indicates that the write exceeded the reliability QoS max_blocking_time.
 * Failure to obtain a required PSMX loan can return DDS_RETCODE_OUT_OF_RESOURCES.
 * Other delivery failures can return DDS_RETCODE_ERROR or an error from
 * a PSMX endpoint. A destination may already have accepted the update when
 * another destination fails.
 */

/**
 * @brief Registers an instance and returns its handle.
 * @ingroup writing
 * @component data_instance
 *
 * The instance is identified by its key fields. Use the handle with
 * @ref dds_dispose_ih or @ref dds_unregister_instance_ih.
 *
 * @param[in] writer The writer.
 * @param[out] handle Receives the instance handle on success. Preserved on error.
 * @param[in] data A sample containing the key.
 *
 * @retval DDS_RETCODE_OK  The instance handle was stored.
 * @retval DDS_RETCODE_BAD_PARAMETER  A pointer is null, the key could not be
 *         registered, or the writer handle is invalid.
 * @see @ref entity_handle_errors for errors validating the writer handle.
 */
DDS_EXPORT dds_return_t
dds_register_instance(
  dds_entity_t writer,
  dds_instance_handle_t *handle,
  const void *data);

/**
 * @brief Unregisters the instance identified by a sample using the current time.
 * @ingroup writing
 * @component data_instance
 *
 * The writer sends a key-only unregister update, bypassing its topic filter.
 * If the writer data lifecycle QoS enables autodispose, the update also
 * disposes the instance.
 *
 * @param[in] writer The writer.
 * @param[in] data A sample containing the key. See @ref write_sample_loan "Writer loan ownership".
 *
 * @retval DDS_RETCODE_OK  The writer accepted the unregister update.
 * @retval DDS_RETCODE_BAD_PARAMETER  The writer handle is invalid, @p data is
 *         null, or the sample could not be serialized.
 * @retval DDS_RETCODE_TIMEOUT  The write exceeded max_blocking_time.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES  A required PSMX loan was unavailable.
 * @retval DDS_RETCODE_ERROR  Delivery failed.
 * @see @ref write_delivery_errors "Delivery errors" and @ref entity_handle_errors.
 */
DDS_EXPORT dds_return_t
dds_unregister_instance(dds_entity_t writer, const void *data);

/**
 * @brief Unregisters an instance by handle using the current time.
 * @ingroup writing
 * @component data_instance
 *
 * The writer looks up the handle in its domain and sends a key-only unregister
 * update, bypassing the topic filter. If the writer data lifecycle QoS enables
 * autodispose, the update also disposes the instance.
 *
 * @param[in] writer The writer.
 * @param[in] handle An instance handle for the writer's topic.
 *
 * @retval DDS_RETCODE_OK  The writer accepted the update.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET  The instance handle is unknown in this domain.
 * @retval DDS_RETCODE_BAD_PARAMETER  The writer handle is invalid or the key could not be serialized.
 * @retval DDS_RETCODE_ERROR  The key could not be reconstructed or delivery failed.
 * @retval DDS_RETCODE_TIMEOUT  The write exceeded max_blocking_time.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES  A required PSMX loan was unavailable.
 * @see @ref write_delivery_errors "Delivery errors" and @ref entity_handle_errors.
 */
DDS_EXPORT dds_return_t
dds_unregister_instance_ih(dds_entity_t writer, dds_instance_handle_t handle);

/**
 * @brief Unregisters the instance identified by a sample using a supplied timestamp.
 * @ingroup writing
 * @component data_instance
 *
 * The operation, loan ownership and return values follow @ref dds_unregister_instance.
 *
 * @param[in] writer The writer.
 * @param[in] data A sample containing the key. See @ref write_sample_loan "Writer loan ownership".
 * @param[in] timestamp Source timestamp. See @ref write_source_timestamp "Source timestamps".
 *
 * @returns The result of the write, as for @ref dds_unregister_instance.
 * @retval DDS_RETCODE_BAD_PARAMETER  The source timestamp is outside the supported range.
 */
DDS_EXPORT dds_return_t
dds_unregister_instance_ts(
  dds_entity_t writer,
  const void *data,
  dds_time_t timestamp);

/**
 * @brief Unregisters an instance by handle using a supplied timestamp.
 * @ingroup writing
 * @component data_instance
 *
 * The operation and return values follow @ref dds_unregister_instance_ih.
 *
 * @param[in] writer The writer.
 * @param[in] handle An instance handle for the writer's topic.
 * @param[in] timestamp Source timestamp. See @ref write_source_timestamp "Source timestamps".
 *
 * @returns The result of the lifecycle update, as for @ref dds_unregister_instance_ih.
 * @retval DDS_RETCODE_BAD_PARAMETER  The source timestamp is outside the supported range.
 */
DDS_EXPORT dds_return_t
dds_unregister_instance_ih_ts(
  dds_entity_t writer,
  dds_instance_handle_t handle,
  dds_time_t timestamp);

/**
 * @brief Writes a data sample and disposes the instance using the current time.
 * @ingroup writing
 * @component write_data
 *
 * The writer sends the complete sample with the disposed instance state.
 * If its topic filter rejects the sample, the writer also skips the dispose.
 *
 * @param[in] writer The writer.
 * @param[in] data The sample to write and dispose. See @ref write_sample_loan "Writer loan ownership".
 *
 * @retval DDS_RETCODE_OK  The writer accepted the update or its topic filter discarded it.
 * @retval DDS_RETCODE_BAD_PARAMETER  The writer handle is invalid, @p data is
 *         null, or the sample could not be serialized.
 * @retval DDS_RETCODE_TIMEOUT  The write exceeded max_blocking_time.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES  A required PSMX loan was unavailable.
 * @retval DDS_RETCODE_ERROR  Delivery failed.
 * @see @ref write_delivery_errors "Delivery errors" and @ref entity_handle_errors.
 */
DDS_EXPORT dds_return_t
dds_writedispose(dds_entity_t writer, const void *data);

/**
 * @brief Writes a data sample and disposes the instance using a supplied timestamp.
 * @ingroup writing
 * @component write_data
 *
 * The operation, loan ownership and return values follow @ref dds_writedispose.
 *
 * @param[in] writer The writer.
 * @param[in] data The sample to write and dispose. See @ref write_sample_loan "Writer loan ownership".
 * @param[in] timestamp Source timestamp. See @ref write_source_timestamp "Source timestamps".
 *
 * @returns The result of the write, as for @ref dds_writedispose.
 * @retval DDS_RETCODE_BAD_PARAMETER  The source timestamp is outside the supported range.
 */
DDS_EXPORT dds_return_t
dds_writedispose_ts(
  dds_entity_t writer,
  const void *data,
  dds_time_t timestamp);

/**
 * @brief Disposes the instance identified by a sample using the current time.
 * @ingroup writing
 * @component write_data
 *
 * The writer sends a key-only dispose update, bypassing its topic filter.
 * Readers use this update to mark the instance as disposed.
 *
 * @param[in] writer The writer.
 * @param[in] data A sample containing the key. See @ref write_sample_loan "Writer loan ownership".
 *
 * @retval DDS_RETCODE_OK  The writer accepted the dispose update.
 * @retval DDS_RETCODE_BAD_PARAMETER  The writer handle is invalid, @p data is
 *         null, or the sample could not be serialized.
 * @retval DDS_RETCODE_TIMEOUT  The write exceeded max_blocking_time.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES  A required PSMX loan was unavailable.
 * @retval DDS_RETCODE_ERROR  Delivery failed.
 * @see @ref write_delivery_errors "Delivery errors" and @ref entity_handle_errors.
 */
DDS_EXPORT dds_return_t
dds_dispose(dds_entity_t writer, const void *data);

/**
 * @brief Disposes the instance identified by a sample using a supplied timestamp.
 * @ingroup writing
 * @component write_data
 *
 * The operation, loan ownership and return values follow @ref dds_dispose.
 *
 * @param[in] writer The writer.
 * @param[in] data A sample containing the key. See @ref write_sample_loan "Writer loan ownership".
 * @param[in] timestamp Source timestamp. See @ref write_source_timestamp "Source timestamps".
 *
 * @returns The result of the write, as for @ref dds_dispose.
 * @retval DDS_RETCODE_BAD_PARAMETER  The source timestamp is outside the supported range.
 */
DDS_EXPORT dds_return_t
dds_dispose_ts(
  dds_entity_t writer,
  const void *data,
  dds_time_t timestamp);

/**
 * @brief Disposes an instance by handle using the current time.
 * @ingroup writing
 * @component write_data
 *
 * The writer looks up the handle in its domain and sends a key-only dispose
 * update, bypassing the topic filter.
 *
 * @param[in] writer The writer.
 * @param[in] handle An instance handle for the writer's topic.
 *
 * @retval DDS_RETCODE_OK  The writer accepted the update.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET  The instance handle is unknown in this domain.
 * @retval DDS_RETCODE_BAD_PARAMETER  The writer handle is invalid or the key could not be serialized.
 * @retval DDS_RETCODE_ERROR  The key could not be reconstructed or delivery failed.
 * @retval DDS_RETCODE_TIMEOUT  The write exceeded max_blocking_time.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES  A required PSMX loan was unavailable.
 * @see @ref write_delivery_errors "Delivery errors" and @ref entity_handle_errors.
 */
DDS_EXPORT dds_return_t
dds_dispose_ih(dds_entity_t writer, dds_instance_handle_t handle);

/**
 * @brief Disposes an instance by handle using a supplied timestamp.
 * @ingroup writing
 * @component write_data
 *
 * The operation and return values follow @ref dds_dispose_ih.
 *
 * @param[in] writer The writer.
 * @param[in] handle An instance handle for the writer's topic.
 * @param[in] timestamp Source timestamp. See @ref write_source_timestamp "Source timestamps".
 *
 * @returns The result of the lifecycle update, as for @ref dds_dispose_ih.
 * @retval DDS_RETCODE_BAD_PARAMETER  The source timestamp is outside the supported range.
 */
DDS_EXPORT dds_return_t
dds_dispose_ih_ts(
  dds_entity_t writer,
  dds_instance_handle_t handle,
  dds_time_t timestamp);

/**
 * @brief Writes a data sample using the current time.
 * @ingroup writing
 * @component write_data
 *
 * The writer applies its topic filter before publishing the sample.
 *
 * @param[in] writer The writer.
 * @param[in] data The sample to write. See @ref write_sample_loan "Writer loan ownership".
 *
 * @retval DDS_RETCODE_OK  The writer accepted the sample or its topic filter discarded it.
 * @retval DDS_RETCODE_BAD_PARAMETER  The writer handle is invalid, @p data is
 *         null, or the sample could not be serialized.
 * @retval DDS_RETCODE_TIMEOUT  The write exceeded max_blocking_time.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES  A required PSMX loan was unavailable.
 * @retval DDS_RETCODE_ERROR  Delivery failed.
 * @see @ref write_delivery_errors "Delivery errors" and @ref entity_handle_errors.
 */
DDS_EXPORT dds_return_t
dds_write(dds_entity_t writer, const void *data);

/**
 * @brief Flushes batched network writes.
 * @ingroup writing
 * @component write_data
 *
 * A writer flushes its current batch. For a publisher, participant or domain,
 * the call flushes its descendant writers.
 *
 * @param[in] entity A writer, publisher, participant or domain.
 *
 * @retval DDS_RETCODE_OK  The flush request completed, including when there
 *         were no batched writes. Individual network send results are not reported.
 * @see @ref entity_handle_errors for errors validating the entity handle.
 */
DDS_EXPORT dds_return_t
dds_write_flush(dds_entity_t entity);

/**
 * @brief Writes a serialized sample using the current time.
 * @ingroup writing
 * @component write_data
 *
 * After validating the serdata type and writer and checking for a topic
 * filter, this call sets the timestamp in @p serdata to the current time and
 * its statusinfo to zero. Other references see these values even if a
 * subsequent step fails.
 *
 * @param[in] writer The writer.
 * @param[in] serdata Serialized data with a non-null type pointer.
 *            If @p serdata is non-null, the call consumes one reference on
 *            every return path.
 *
 * @retval DDS_RETCODE_OK  The writer accepted the serialized data.
 * @retval DDS_RETCODE_BAD_PARAMETER  The writer handle is invalid, @p serdata
 *         or its type pointer is null, the source timestamp is outside the
 *         supported range, or an attached PSMX loan has incompatible origin
 *         or metadata.
 * @retval DDS_RETCODE_ERROR  The writer's topic has a configured filter,
 *         conversion to the writer's serialized type failed, or delivery failed.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES  A required PSMX loan was unavailable.
 * @retval DDS_RETCODE_TIMEOUT  The write exceeded max_blocking_time.
 * @see @ref write_delivery_errors "Delivery errors" and @ref entity_handle_errors.
 */
DDS_EXPORT dds_return_t
dds_writecdr(dds_entity_t writer, struct ddsi_serdata *serdata);

/**
 * @brief Forwards serialized data with its source timestamp and statusinfo.
 * @ingroup writing
 * @component write_data
 *
 * The writer preserves the timestamp and statusinfo from @p serdata. The
 * timestamp must satisfy the @ref write_source_timestamp "source timestamp limits".
 *
 * @param[in] writer The writer.
 * @param[in] serdata Serialized data with a non-null type pointer.
 *            If @p serdata is non-null, the call consumes one reference on
 *            every return path.
 *
 * @retval DDS_RETCODE_OK  The writer accepted the serialized data.
 * @retval DDS_RETCODE_BAD_PARAMETER  The writer handle is invalid, @p serdata
 *         or its type pointer is null, the source timestamp is outside the
 *         supported range, or an attached PSMX loan has incompatible origin
 *         or metadata.
 * @retval DDS_RETCODE_ERROR  The writer's topic has a configured filter,
 *         conversion to the writer's serialized type failed, or delivery failed.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES  A required PSMX loan was unavailable.
 * @retval DDS_RETCODE_TIMEOUT  The write exceeded max_blocking_time.
 * @see @ref write_delivery_errors "Delivery errors" and @ref entity_handle_errors.
 */
DDS_EXPORT dds_return_t
dds_forwardcdr(dds_entity_t writer, struct ddsi_serdata *serdata);

/**
 * @brief Writes a data sample using a supplied timestamp.
 * @ingroup writing
 * @component write_data
 *
 * The operation, loan ownership and return values follow @ref dds_write.
 *
 * @param[in] writer The writer.
 * @param[in] data The sample to write. See @ref write_sample_loan "Writer loan ownership".
 * @param[in] timestamp Source timestamp. See @ref write_source_timestamp "Source timestamps".
 *
 * @returns The result of the write, as for @ref dds_write.
 * @retval DDS_RETCODE_BAD_PARAMETER  The source timestamp is outside the supported range.
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
 * @brief Create a read condition on a reader.
 * @ingroup readcondition
 * @component data_query
 *
 * The condition triggers while the reader history contains samples that match
 * the state mask. Reads and takes through the reader or any of its
 * conditions affect the same history and can change which conditions trigger.
 *
 * @param[in] reader Reader that owns the condition.
 * @param[in] mask Bitwise OR of sample, view and instance states. Omitting a
 *            state category matches any state in that category. Other bits are ignored.
 *
 * @returns A positive condition handle on success, or a negative error code.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES The reader history could not add the condition.
 * @see @ref entity_handle_errors for errors looking up the reader.
 */
DDS_EXPORT dds_entity_t
dds_create_readcondition(dds_entity_t reader, uint32_t mask);

/**
 * @brief Test whether a sample matches a query condition.
 * @ingroup querycondition
 *
 * The reader evaluates the predicate on existing samples when the condition is
 * created and as it processes incoming data. The predicate runs with the reader
 * history locked. Calling DDS functions that access that history can deadlock.
 *
 * @param[in] sample Temporary sample in application representation, valid only
 *            during the call. Copy any values to be retained. For an invalid-data
 *           notification only the key is valid.
 * @returns True if the sample matches the query.
 */
typedef bool (*dds_querycondition_filter_fn) (const void * sample);

/**
 * @brief Create a query condition on a reader.
 * @ingroup querycondition
 * @component data_query
 *
 * The condition triggers while the reader history contains samples that match
 * the state mask and pass the filter. Reads and takes through the reader or any of its
 * conditions affect the same history and can change which conditions trigger.
 *
 * @param[in] reader Reader that owns the condition.
 * @param[in] mask Bitwise OR of sample, view and instance states. Omitting a
 *            state category matches any state in that category. Other bits are ignored.
 * @param[in] filter Non-null predicate applied to the sample.
 *
 * @returns A positive condition handle on success, or a negative error code.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES The reader history could not add the condition.
 *         The default reader history supports up to 32 query conditions per reader.
 * @see @ref entity_handle_errors for errors looking up the reader.
 */
DDS_EXPORT dds_entity_t
dds_create_querycondition(
  dds_entity_t reader,
  uint32_t mask,
  dds_querycondition_filter_fn filter);

/**
 * @brief Create a guard condition.
 * @ingroup guardcondition
 * @component guard_condition
 *
 * The application controls its trigger value. It starts false and retains the
 * value set by @ref dds_set_guardcondition until it is set again or taken.
 *
 * @param[in] owner Participant, domain or `DDS_CYCLONEDDS_HANDLE`.
 *
 * @returns A positive guard condition handle on success, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER The owner handle is invalid or has been deleted.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION The owner is of an unsupported entity kind.
 * @retval DDS_RETCODE_ERROR The library could not be initialized.
 */
DDS_EXPORT dds_entity_t
dds_create_guardcondition(dds_entity_t owner);

/**
 * @brief Set the trigger value of a guard condition.
 * @ingroup guardcondition
 * @component guard_condition
 *
 * The transition from false to true wakes waitsets observing the condition.
 *
 * @param[in] guardcond Guard condition.
 * @param[in] triggered New trigger value.
 *
 * @retval DDS_RETCODE_OK Success.
 * @see @ref entity_handle_errors for errors looking up the guard condition.
 */
DDS_EXPORT dds_return_t
dds_set_guardcondition(dds_entity_t guardcond, bool triggered);

/**
 * @brief Read the trigger value of a guard condition.
 * @ingroup guardcondition
 * @component guard_condition
 *
 * Reading preserves the trigger value.
 *
 * @param[in] guardcond Guard condition.
 * @param[out] triggered Receives the trigger value on success. Unchanged on error.
 *
 * @retval DDS_RETCODE_OK Success.
 * @retval DDS_RETCODE_BAD_PARAMETER `triggered` is null.
 * @see @ref entity_handle_errors for errors looking up the guard condition.
 */
DDS_EXPORT dds_return_t
dds_read_guardcondition(dds_entity_t guardcond, bool *triggered);

/**
 * @brief Read and clear the trigger value of a guard condition.
 * @ingroup guardcondition
 * @component guard_condition
 *
 * The operation atomically exchanges the current trigger value for false.
 *
 * @param[in] guardcond Guard condition.
 * @param[out] triggered Receives the previous trigger value on success. Unchanged on error.
 *
 * @retval DDS_RETCODE_OK Success.
 * @retval DDS_RETCODE_BAD_PARAMETER `triggered` is null.
 * @see @ref entity_handle_errors for errors looking up the guard condition.
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
 * @brief Create a waitset.
 * @ingroup waitset
 * @component waitset
 *
 * A waitset starts empty. Its owner determines which entities can be attached
 * to it with @ref dds_waitset_attach.
 *
 * @param[in] owner Participant, domain or `DDS_CYCLONEDDS_HANDLE`.
 *
 * @returns A positive waitset handle on success, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER The owner handle is invalid or has been deleted.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION The owner is of an unsupported entity kind.
 * @retval DDS_RETCODE_ERROR The library could not be initialized.
 */
DDS_EXPORT dds_entity_t
dds_create_waitset(dds_entity_t owner);

/**
 * @brief Get the entities attached to a waitset.
 * @ingroup waitset
 * @component waitset
 *
 * The call writes the attached entity handles in unspecified order and leaves
 * any unused slots unchanged. If there are more attachments than slots, it
 * fills the array with the handles of @p size arbitrarily selected entities.
 * Pass a null array to get just the count, regardless of @p size.
 * The count includes the waitset itself if it is attached to itself.
 *
 * @param[in] waitset Waitset to inspect.
 * @param[out] entities Array of entity handles, or NULL.
 * @param[in] size Capacity of the array.
 *
 * @returns The total number of attachments, even if it exceeds `size`, or a
 *          negative error code.
 * @see @ref entity_handle_errors for errors looking up the waitset.
 */
DDS_EXPORT dds_return_t
dds_waitset_get_entities(
  dds_entity_t waitset,
  dds_entity_t *entities,
  size_t size);

/**
 * @brief Attach an entity and its application-supplied value to a waitset.
 * @ingroup waitset
 * @component waitset
 *
 * The entity must be the owner of the waitset or a descendant of that owner.
 * A waitset can be attached to itself. An entity may be attached to multiple
 * waitsets, once per waitset.
 *
 * A read or query condition triggers while matching samples are available.
 * A guard condition or waitset triggers while its application-controlled
 * trigger value is true. Other entities trigger when an enabled status is set.
 *
 * @param[in] waitset Waitset to observe the entity.
 * @param[in] entity Entity to attach.
 * @param[in] x Value returned by a wait on this waitset when the entity triggers.
 *
 * @retval DDS_RETCODE_OK The entity was attached.
 * @retval DDS_RETCODE_BAD_PARAMETER The entity is outside the owner's scope.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET The entity is already attached.
 * @see @ref entity_handle_errors for errors looking up either entity.
 */
DDS_EXPORT dds_return_t
dds_waitset_attach(
  dds_entity_t waitset,
  dds_entity_t entity,
  dds_attach_t x);

/**
 * @brief Detach an entity from a waitset.
 * @ingroup waitset
 * @component waitset
 *
 * @param[in] waitset Waitset observing the entity.
 * @param[in] entity Entity to detach.
 *
 * @retval DDS_RETCODE_OK The entity was detached.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET The entity is not attached to this waitset.
 * @see @ref entity_handle_errors for errors looking up either entity.
 */
DDS_EXPORT dds_return_t
dds_waitset_detach(
  dds_entity_t waitset,
  dds_entity_t entity);

/**
 * @brief Set the trigger value of the waitset itself.
 * @ingroup waitset
 * @component waitset
 *
 * Setting the trigger to true wakes waitsets observing this waitset, including
 * the waitset itself if it is self-attached. The value persists until reset
 * by the application.
 *
 * @param[in] waitset Waitset whose trigger value to set.
 * @param[in] trigger New trigger value.
 *
 * @retval DDS_RETCODE_OK The trigger value was set.
 * @see @ref entity_handle_errors for errors looking up the waitset.
 */
DDS_EXPORT dds_return_t
dds_waitset_set_trigger(
  dds_entity_t waitset,
  bool trigger);

/**
 * @brief Wait for attached entities to trigger, with a relative timeout.
 * @ingroup waitset
 * @component waitset
 *
 * An empty waitset returns zero immediately. Detaching the last entity or
 * deleting the waitset can also wake a blocked call with a count of zero.
 * Multiple threads may wait independently. Waiting leaves the triggers set.
 *
 * If more than @p nxs entities trigger, the call fills the supplied array with
 * the attachment values of @p nxs arbitrarily selected triggered entities.
 * Otherwise it writes all triggered entities' attachment values and leaves any
 * remaining slots unchanged. Pass a null array and zero capacity to obtain
 * just the count.
 *
 * @param[in] waitset Waitset to wait on.
 * @param[out] xs Array for the values supplied to @ref dds_waitset_attach.
 * @param[in] nxs Capacity of the array.
 * @param[in] reltimeout Maximum duration in nanoseconds. Use zero to poll or `DDS_INFINITY` to wait indefinitely.
 *
 * @returns The total number of triggered entities, or a negative error code.
 * @retval 0 No entities triggered before the wait ended.
 * @retval DDS_RETCODE_BAD_PARAMETER The array and capacity do not agree, or the duration is negative.
 * @see @ref entity_handle_errors for errors looking up the waitset.
 */
DDS_EXPORT dds_return_t
dds_waitset_wait(
  dds_entity_t waitset,
  dds_attach_t *xs,
  size_t nxs,
  dds_duration_t reltimeout);

/**
 * @brief Wait for attached entities to trigger, with an absolute deadline.
 * @ingroup waitset
 * @component waitset
 *
 * An empty waitset returns zero immediately. Detaching the last entity or
 * deleting the waitset can also wake a blocked call with a count of zero.
 * Multiple threads may wait independently. Waiting leaves the triggers set.
 *
 * If more than @p nxs entities trigger, the call fills the supplied array with
 * the attachment values of @p nxs arbitrarily selected triggered entities.
 * Otherwise it writes all triggered entities' attachment values and leaves any
 * remaining slots unchanged. Pass a null array and zero capacity to obtain
 * just the count.
 *
 * @param[in] waitset Waitset to wait on.
 * @param[out] xs Array for the values supplied to @ref dds_waitset_attach.
 * @param[in] nxs Capacity of the array.
 * @param[in] abstimeout Absolute wall-clock time in nanoseconds. A past deadline polls immediately.
 *
 * @returns The total number of triggered entities, or a negative error code.
 * @retval 0 No entities triggered before the wait ended.
 * @retval DDS_RETCODE_BAD_PARAMETER The array and capacity do not agree.
 * @see @ref entity_handle_errors for errors looking up the waitset.
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
 *
 * @section read_sample_selection Sample selection and status changes
 *
 * With a reader, all cached samples are eligible unless the operation specifies
 * a mask or an instance. With a read condition, its sample, view and instance
 * states select the samples. With a query condition, its predicate also applies.
 *
 * Build the explicit mask from sample, view and instance state bits. A mask
 * of zero uses the condition's state mask, or all states for a reader. A
 * nonzero mask selects the indicated states in each category and
 * includes all states in any category it omits. When a condition is supplied,
 * the allowed states in each category are the union of those selected by the
 * explicit mask and those selected by the condition. The query predicate still
 * applies. For example, specifying only `DDS_NEW_VIEW_STATE` permits both read
 * and unread samples, even if the condition specifies only unread samples.
 *
 * Samples from an instance are returned together, oldest first. By default the
 * order of instances is unspecified. The next-instance operations order them
 * by instance handle.
 *
 * The default reader history computes `sample_rank` and `generation_rank`
 * from the samples selected for each instance before collecting them. If
 * collection stops early, those ranks can include samples beyond the
 * returned count.
 *
 * Peek, read and take clear the reader's `DDS_DATA_AVAILABLE_STATUS` and, when
 * tracked on the subscriber, its `DDS_DATA_ON_READERS_STATUS` before searching
 * the history, even if no samples match. Peek preserves sample and view states.
 * Read marks returned samples as read and their instances as viewed. Take
 * removes returned samples and marks their instances as viewed.
 *
 * @section read_sample_buffers Typed sample buffers and loans
 *
 * The first pointer in `buf` determines how peek, read and take return samples.
 *
 * - If `buf[0]` is null on entry, the reader lends samples.
 *   - On success, the return value `n` is the number of samples, with `0 <= n <= maxs`.
 *   - The first `n` entries in `buf` point to read-only samples owned by the reader.
 *   - If `n < bufsz`, `buf[n]` is null.
 *   - Return the loans with @ref dds_return_loan or by reusing `buf` with the same reader.
 *
 * - If `buf[0]` is an outstanding loan from this reader on entry, the call reuses the loan buffer.
 *   - The first `k` entries in `buf` must point to outstanding loans from this reader,
 *     with `1 <= k <= bufsz`.
 *   - If `k < bufsz`, `buf[k]` must be null.
 *   - The reader reclaims all `k` loans before collecting new samples, even if collection later fails.
 *   - Collection proceeds as if `buf[0]` had been null on entry.
 *
 * - If `buf[0]` is any other address on entry, the reader copies samples into application-owned memory.
 *   - All `bufsz` entries in `buf` must point to writable sample storage.
 *   - Initialize each sample's embedded strings, externals, optionals and sequences before calling.
 *     Embedded pointers may be null and sequences may be zero-initialized.
 *   - See @ref dds_sequence_t and @ref dds_sample_free for the ownership of sequence fields.
 *
 * The sample information in `si[i]` describes `buf[i]`. When `valid_data` is
 * false, only the key is valid. In application-owned C samples the remaining
 * fields are zeroed.
 *
 * A conversion failure on the first sample returns `DDS_RETCODE_ERROR`. Failure
 * to obtain or track a loan for the first sample returns
 * `DDS_RETCODE_OUT_OF_RESOURCES`. If a later sample fails, the call returns the
 * positive count already collected. A mixture of loans and application-owned
 * samples is invalid. If such a mixture is detected while returning loans,
 * the call returns `DDS_RETCODE_BAD_PARAMETER`.
 */

/**
 * @brief Peek at samples in the reader history.
 * @ingroup reading
 * @component read_data
 *
 * Returned samples stay in the history with their sample and view states unchanged.
 * With a reader, all samples are eligible. A condition applies its state mask
 * and, for a query condition, its predicate.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `bufsz` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] bufsz Capacity of `buf`, at least `maxs`.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 *
 * @returns The number of samples returned, 0 if none match, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or the sizes are outside the stated bounds.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
 */
DDS_EXPORT dds_return_t
dds_peek(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs);

/**
 * @brief Peek at samples matching a state mask.
 * @ingroup reading
 * @component read_data
 *
 * Returned samples stay in the history with their sample and view states unchanged.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `bufsz` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] bufsz Capacity of `buf`, at least `maxs`.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] mask Sample, view and instance states (see @ref read_sample_selection).
 *
 * @returns The number of samples returned, 0 if none match, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or the sizes are outside the stated bounds.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Peek at matching samples from a specified instance.
 * @ingroup reading
 * @component read_data
 *
 * Returned samples stay in the history with their sample and view states unchanged.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `bufsz` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] bufsz Capacity of `buf`, at least `maxs`.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] handle Nonzero handle of an instance known to this reader.
 *
 * @returns The number of samples returned, 0 if none match, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or the sizes are outside the stated bounds.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET `handle` is nil or is unknown to this reader.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Peek at matching samples from a specified instance.
 * @ingroup reading
 * @component read_data
 *
 * Returned samples stay in the history with their sample and view states unchanged.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `bufsz` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] bufsz Capacity of `buf`, at least `maxs`.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] handle Nonzero handle of an instance known to this reader.
 * @param[in] mask Sample, view and instance states (see @ref read_sample_selection).
 *
 * @returns The number of samples returned, 0 if none match, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or the sizes are outside the stated bounds.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET `handle` is nil or is unknown to this reader.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Peek at matching samples from the next instance in handle order.
 * @ingroup reading
 * @component read_data
 *
 * Selects the lowest instance handle greater than `previous_handle` with at
 * least one matching sample. Pass `DDS_HANDLE_NIL` to start, then use the
 * `instance_handle` in the returned sample information to advance.
 * Returned samples stay in the history with their sample and view states unchanged.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `bufsz` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] bufsz Capacity of `buf`, at least `maxs`.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] previous_handle Exclusive lower bound on the instance handle.
 *
 * @returns The number of samples returned from one instance, 0 if no matching
 *          instance follows the cursor, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or the sizes are outside the stated bounds.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Peek at matching samples from the next instance in handle order.
 * @ingroup reading
 * @component read_data
 *
 * Selects the lowest instance handle greater than `previous_handle` with at
 * least one matching sample. Pass `DDS_HANDLE_NIL` to start, then use the
 * `instance_handle` in the returned sample information to advance.
 * Returned samples stay in the history with their sample and view states unchanged.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `bufsz` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] bufsz Capacity of `buf`, at least `maxs`.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] previous_handle Exclusive lower bound on the instance handle.
 * @param[in] mask Sample, view and instance states (see @ref read_sample_selection).
 *
 * @returns The number of samples returned from one instance, 0 if no matching
 *          instance follows the cursor, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or the sizes are outside the stated bounds.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Peek at the first unread sample.
 * @ingroup reading
 * @component read_data
 *
 * Returned samples stay in the history with their sample and view states unchanged.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader Reader to search.
 * @param[in,out] buf Address of a sample pointer, as in @ref read_sample_buffers.
 * @param[out] si Sample information for the returned sample.
 *
 * @returns 1 if an unread sample was returned, 0 if none was available, or a
 *          negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
 */
DDS_EXPORT dds_return_t
dds_peek_next(
  dds_entity_t reader,
  void **buf,
  dds_sample_info_t *si);

/**
 * @brief Read samples in the reader history.
 * @ingroup reading
 * @component read_data
 *
 * Returned samples stay in the history and are marked as read.
 * With a reader, all samples are eligible. A condition applies its state mask
 * and, for a query condition, its predicate.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `bufsz` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] bufsz Capacity of `buf`, at least `maxs`.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 *
 * @returns The number of samples returned, 0 if none match, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or the sizes are outside the stated bounds.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
 */
DDS_EXPORT dds_return_t
dds_read(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs);

/**
 * @brief Read samples in the reader history.
 * @ingroup reading
 * @component read_data
 *
 * @deprecated Use @ref dds_read with `bufsz` set to `maxs`. The same buffer ownership and error rules apply.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `maxs` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 *
 * @returns The number of samples returned, 0 if none match, or a negative error code.
 * @see dds_read for possible errors.
 */
DDS_DEPRECATED_EXPORT dds_return_t
dds_read_wl(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  uint32_t maxs);

/**
 * @brief Read samples matching a state mask.
 * @ingroup reading
 * @component read_data
 *
 * Returned samples stay in the history and are marked as read.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `bufsz` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] bufsz Capacity of `buf`, at least `maxs`.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] mask Sample, view and instance states (see @ref read_sample_selection).
 *
 * @returns The number of samples returned, 0 if none match, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or the sizes are outside the stated bounds.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Read samples matching a state mask.
 * @ingroup reading
 * @component read_data
 *
 * @deprecated Use @ref dds_read_mask with `bufsz` set to `maxs`. The same buffer ownership and error rules apply.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `maxs` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] mask Sample, view and instance states (see @ref read_sample_selection).
 *
 * @returns The number of samples returned, 0 if none match, or a negative error code.
 * @see dds_read_mask for possible errors.
 */
DDS_DEPRECATED_EXPORT dds_return_t
dds_read_mask_wl(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  uint32_t maxs,
  uint32_t mask);

/**
 * @brief Read matching samples from a specified instance.
 * @ingroup reading
 * @component read_data
 *
 * Returned samples stay in the history and are marked as read.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `bufsz` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] bufsz Capacity of `buf`, at least `maxs`.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] handle Nonzero handle of an instance known to this reader.
 *
 * @returns The number of samples returned, 0 if none match, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or the sizes are outside the stated bounds.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET `handle` is nil or is unknown to this reader.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Read matching samples from a specified instance.
 * @ingroup reading
 * @component read_data
 *
 * @deprecated Use @ref dds_read_instance with `bufsz` set to `maxs`. The same buffer ownership and error rules apply.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `maxs` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] handle Nonzero handle of an instance known to this reader.
 *
 * @returns The number of samples returned, 0 if none match, or a negative error code.
 * @see dds_read_instance for possible errors.
 */
DDS_DEPRECATED_EXPORT dds_return_t
dds_read_instance_wl(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  uint32_t maxs,
  dds_instance_handle_t handle);

/**
 * @brief Read matching samples from a specified instance.
 * @ingroup reading
 * @component read_data
 *
 * Returned samples stay in the history and are marked as read.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `bufsz` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] bufsz Capacity of `buf`, at least `maxs`.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] handle Nonzero handle of an instance known to this reader.
 * @param[in] mask Sample, view and instance states (see @ref read_sample_selection).
 *
 * @returns The number of samples returned, 0 if none match, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or the sizes are outside the stated bounds.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET `handle` is nil or is unknown to this reader.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Read matching samples from a specified instance.
 * @ingroup reading
 * @component read_data
 *
 * @deprecated Use @ref dds_read_instance_mask with `bufsz` set to `maxs`. The same buffer ownership and error rules apply.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `maxs` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] handle Nonzero handle of an instance known to this reader.
 * @param[in] mask Sample, view and instance states (see @ref read_sample_selection).
 *
 * @returns The number of samples returned, 0 if none match, or a negative error code.
 * @see dds_read_instance_mask for possible errors.
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
 * @brief Read matching samples from the next instance in handle order.
 * @ingroup reading
 * @component read_data
 *
 * Selects the lowest instance handle greater than `previous_handle` with at
 * least one matching sample. Pass `DDS_HANDLE_NIL` to start, then use the
 * `instance_handle` in the returned sample information to advance.
 * Returned samples stay in the history and are marked as read.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `bufsz` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] bufsz Capacity of `buf`, at least `maxs`.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] previous_handle Exclusive lower bound on the instance handle.
 *
 * @returns The number of samples returned from one instance, 0 if no matching
 *          instance follows the cursor, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or the sizes are outside the stated bounds.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Read matching samples from the next instance in handle order.
 * @ingroup reading
 * @component read_data
 *
 * Selects the lowest instance handle greater than `previous_handle` with at
 * least one matching sample. Pass `DDS_HANDLE_NIL` to start, then use the
 * `instance_handle` in the returned sample information to advance.
 * Returned samples stay in the history and are marked as read.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `bufsz` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] bufsz Capacity of `buf`, at least `maxs`.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] previous_handle Exclusive lower bound on the instance handle.
 * @param[in] mask Sample, view and instance states (see @ref read_sample_selection).
 *
 * @returns The number of samples returned from one instance, 0 if no matching
 *          instance follows the cursor, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or the sizes are outside the stated bounds.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Read the first unread sample.
 * @ingroup reading
 * @component read_data
 *
 * Returned samples stay in the history and are marked as read.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader Reader to search.
 * @param[in,out] buf Address of a sample pointer, as in @ref read_sample_buffers.
 * @param[out] si Sample information for the returned sample.
 *
 * @returns 1 if an unread sample was returned, 0 if none was available, or a
 *          negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
 */
DDS_EXPORT dds_return_t
dds_read_next(
  dds_entity_t reader,
  void **buf,
  dds_sample_info_t *si);

/**
 * @brief Read the first unread sample.
 * @ingroup reading
 * @component read_data
 *
 * @deprecated Use @ref dds_read_next. The same buffer ownership and error rules apply.
 *
 * @param[in] reader Reader to search.
 * @param[in,out] buf Address of a sample pointer, as in @ref read_sample_buffers.
 * @param[out] si Sample information for the returned sample.
 *
 * @returns 1 if an unread sample was returned, 0 if none was available, or a
 *          negative error code.
 * @see dds_read_next for possible errors.
 */
DDS_DEPRECATED_EXPORT dds_return_t
dds_read_next_wl(
  dds_entity_t reader,
  void **buf,
  dds_sample_info_t *si);

/**
 * @brief Take samples in the reader history.
 * @ingroup reading
 * @component read_data
 *
 * Returned samples are removed from the reader history.
 * With a reader, all samples are eligible. A condition applies its state mask
 * and, for a query condition, its predicate.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `bufsz` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] bufsz Capacity of `buf`, at least `maxs`.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 *
 * @returns The number of samples returned, 0 if none match, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or the sizes are outside the stated bounds.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
 */
DDS_EXPORT dds_return_t
dds_take(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  size_t bufsz,
  uint32_t maxs);

/**
 * @brief Take samples in the reader history.
 * @ingroup reading
 * @component read_data
 *
 * @deprecated Use @ref dds_take with `bufsz` set to `maxs`. The same buffer ownership and error rules apply.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `maxs` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 *
 * @returns The number of samples returned, 0 if none match, or a negative error code.
 * @see dds_take for possible errors.
 */
DDS_DEPRECATED_EXPORT dds_return_t
dds_take_wl(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  uint32_t maxs);

/**
 * @brief Take samples matching a state mask.
 * @ingroup reading
 * @component read_data
 *
 * Returned samples are removed from the reader history.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `bufsz` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] bufsz Capacity of `buf`, at least `maxs`.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] mask Sample, view and instance states (see @ref read_sample_selection).
 *
 * @returns The number of samples returned, 0 if none match, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or the sizes are outside the stated bounds.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Take samples matching a state mask.
 * @ingroup reading
 * @component read_data
 *
 * @deprecated Use @ref dds_take_mask with `bufsz` set to `maxs`. The same buffer ownership and error rules apply.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `maxs` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] mask Sample, view and instance states (see @ref read_sample_selection).
 *
 * @returns The number of samples returned, 0 if none match, or a negative error code.
 * @see dds_take_mask for possible errors.
 */
DDS_DEPRECATED_EXPORT dds_return_t
dds_take_mask_wl(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  uint32_t maxs,
  uint32_t mask);

/**
 * @brief Take matching samples from a specified instance.
 * @ingroup reading
 * @component read_data
 *
 * Returned samples are removed from the reader history.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `bufsz` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] bufsz Capacity of `buf`, at least `maxs`.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] handle Nonzero handle of an instance known to this reader.
 *
 * @returns The number of samples returned, 0 if none match, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or the sizes are outside the stated bounds.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET `handle` is nil or is unknown to this reader.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Take matching samples from a specified instance.
 * @ingroup reading
 * @component read_data
 *
 * @deprecated Use @ref dds_take_instance with `bufsz` set to `maxs`. The same buffer ownership and error rules apply.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `maxs` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] handle Nonzero handle of an instance known to this reader.
 *
 * @returns The number of samples returned, 0 if none match, or a negative error code.
 * @see dds_take_instance for possible errors.
 */
DDS_DEPRECATED_EXPORT dds_return_t
dds_take_instance_wl(
  dds_entity_t reader_or_condition,
  void **buf,
  dds_sample_info_t *si,
  uint32_t maxs,
  dds_instance_handle_t handle);

/**
 * @brief Take matching samples from a specified instance.
 * @ingroup reading
 * @component read_data
 *
 * Returned samples are removed from the reader history.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `bufsz` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] bufsz Capacity of `buf`, at least `maxs`.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] handle Nonzero handle of an instance known to this reader.
 * @param[in] mask Sample, view and instance states (see @ref read_sample_selection).
 *
 * @returns The number of samples returned, 0 if none match, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or the sizes are outside the stated bounds.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET `handle` is nil or is unknown to this reader.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Take matching samples from a specified instance.
 * @ingroup reading
 * @component read_data
 *
 * @deprecated Use @ref dds_take_instance_mask with `bufsz` set to `maxs`. The same buffer ownership and error rules apply.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `maxs` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] handle Nonzero handle of an instance known to this reader.
 * @param[in] mask Sample, view and instance states (see @ref read_sample_selection).
 *
 * @returns The number of samples returned, 0 if none match, or a negative error code.
 * @see dds_take_instance_mask for possible errors.
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
 * @brief Take matching samples from the next instance in handle order.
 * @ingroup reading
 * @component read_data
 *
 * Selects the lowest instance handle greater than `previous_handle` with at
 * least one matching sample. Pass `DDS_HANDLE_NIL` to start, then use the
 * `instance_handle` in the returned sample information to advance.
 * Returned samples are removed from the reader history.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `bufsz` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] bufsz Capacity of `buf`, at least `maxs`.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] previous_handle Exclusive lower bound on the instance handle.
 *
 * @returns The number of samples returned from one instance, 0 if no matching
 *          instance follows the cursor, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or the sizes are outside the stated bounds.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Take matching samples from the next instance in handle order.
 * @ingroup reading
 * @component read_data
 *
 * Selects the lowest instance handle greater than `previous_handle` with at
 * least one matching sample. Pass `DDS_HANDLE_NIL` to start, then use the
 * `instance_handle` in the returned sample information to advance.
 * Returned samples are removed from the reader history.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in,out] buf Array of `bufsz` sample pointers, as in @ref read_sample_buffers.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] bufsz Capacity of `buf`, at least `maxs`.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] previous_handle Exclusive lower bound on the instance handle.
 * @param[in] mask Sample, view and instance states (see @ref read_sample_selection).
 *
 * @returns The number of samples returned from one instance, 0 if no matching
 *          instance follows the cursor, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or the sizes are outside the stated bounds.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Take the first unread sample.
 * @ingroup reading
 * @component read_data
 *
 * Returned samples are removed from the reader history.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader Reader to search.
 * @param[in,out] buf Address of a sample pointer, as in @ref read_sample_buffers.
 * @param[out] si Sample information for the returned sample.
 *
 * @returns 1 if an unread sample was returned, 0 if none was available, or a
 *          negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null.
 * @see @ref read_sample_buffers for conversion and loan errors, and
 *      @ref entity_handle_errors for errors looking up the entity.
 */
DDS_EXPORT dds_return_t
dds_take_next(
  dds_entity_t reader,
  void **buf,
  dds_sample_info_t *si);

/**
 * @brief Take the first unread sample.
 * @ingroup reading
 * @component read_data
 *
 * @deprecated Use @ref dds_take_next. The same buffer ownership and error rules apply.
 *
 * @param[in] reader Reader to search.
 * @param[in,out] buf Address of a sample pointer, as in @ref read_sample_buffers.
 * @param[out] si Sample information for the returned sample.
 *
 * @returns 1 if an unread sample was returned, 0 if none was available, or a
 *          negative error code.
 * @see dds_take_next for possible errors.
 */
DDS_DEPRECATED_EXPORT dds_return_t
dds_take_next_wl(
  dds_entity_t reader,
  void **buf,
  dds_sample_info_t *si);

/**
 * @brief Collect one sample during a peek, read or take.
 * @ingroup reading
 * @component read_data
 *
 * The callback runs once per matching sample, with samples of the same instance
 * grouped together. It runs under the reader history lock. Keep it brief and
 * avoid calling back into the DDS API.
 *
 * @param[in] arg Application argument passed to the collector operation.
 * @param[in] si Sample information valid for this invocation. Copy it to retain it.
 *            If a callback stops collection, the ranks can include later samples
 *            selected for the same instance (see @ref read_sample_selection).
 * @param[in] st Reader's serialization type, needed to interpret an untyped key.
 * @param[in] sd Borrowed serdata. When `si->valid_data` is false this is an
 *            untyped instance key. Take a reference with `ddsi_serdata_ref`
 *            during the callback to retain it. A take can release the history's
 *            reference as soon as the callback returns. Release retained
 *            references with `ddsi_serdata_unref` while the originating
 *            domain is still alive. Keep the serialization type alive
 *            separately when using it after reader deletion.
 *
 * @returns Return `DDS_RETCODE_OK` to accept the sample and continue. The
 *          default reader history accepts any nonnegative value. A negative
 *          value stops collection. The caller receives that error on the first
 *          sample, or the number already collected if a later callback fails.
 */
typedef dds_return_t (*dds_read_with_collector_fn_t) (
  void *arg,
  const dds_sample_info_t *si,
  const struct ddsi_sertype *st,
  struct ddsi_serdata *sd);

/**
 * @brief Peek at matching samples using an application-defined collector.
 * @ingroup reading
 * @component read_data
 *
 * Successfully collected samples keep their sample and view states.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] handle Instance to select, or `DDS_HANDLE_NIL` for all matching instances.
 * @param[in] mask Sample, view and instance states (see @ref read_sample_selection).
 * @param[in] collect_sample Callback to invoke for each matching sample.
 *            See @ref dds_read_with_collector_fn_t for serdata ownership.
 * @param[in] collect_sample_arg Application argument passed to the callback.
 *
 * @returns The number of samples successfully collected, 0 if no samples match,
 *          or a negative error code. A callback error is returned if the first
 *          invocation fails. A later error yields the number already collected.
 * @retval DDS_RETCODE_BAD_PARAMETER The callback is null or `maxs` is outside the stated range.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET A nonzero `handle` is unknown to this reader.
 * @see @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Peek at samples from the next matching instance using an application-defined collector.
 * @ingroup reading
 * @component read_data
 *
 * Selects the lowest instance handle greater than `previous_handle` with at
 * least one matching sample. Pass `DDS_HANDLE_NIL` to start and use the instance
 * handle supplied to the callback to advance.
 * Successfully collected samples keep their sample and view states.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] previous_handle Exclusive lower bound on the instance handle.
 * @param[in] mask Sample, view and instance states (see @ref read_sample_selection).
 * @param[in] collect_sample Callback to invoke for each matching sample.
 *            See @ref dds_read_with_collector_fn_t for serdata ownership.
 * @param[in] collect_sample_arg Application argument passed to the callback.
 *
 * @returns The number of samples successfully collected, 0 if no matching instance follows the cursor,
 *          or a negative error code. A callback error is returned if the first
 *          invocation fails. A later error yields the number already collected.
 * @retval DDS_RETCODE_BAD_PARAMETER The callback is null or `maxs` is outside the stated range.
 * @see @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Read matching samples using an application-defined collector.
 * @ingroup reading
 * @component read_data
 *
 * Successfully collected samples remain in the history and are marked as read.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] handle Instance to select, or `DDS_HANDLE_NIL` for all matching instances.
 * @param[in] mask Sample, view and instance states (see @ref read_sample_selection).
 * @param[in] collect_sample Callback to invoke for each matching sample.
 *            See @ref dds_read_with_collector_fn_t for serdata ownership.
 * @param[in] collect_sample_arg Application argument passed to the callback.
 *
 * @returns The number of samples successfully collected, 0 if no samples match,
 *          or a negative error code. A callback error is returned if the first
 *          invocation fails. A later error yields the number already collected.
 * @retval DDS_RETCODE_BAD_PARAMETER The callback is null or `maxs` is outside the stated range.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET A nonzero `handle` is unknown to this reader.
 * @see @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Read samples from the next matching instance using an application-defined collector.
 * @ingroup reading
 * @component read_data
 *
 * Selects the lowest instance handle greater than `previous_handle` with at
 * least one matching sample. Pass `DDS_HANDLE_NIL` to start and use the instance
 * handle supplied to the callback to advance.
 * Successfully collected samples remain in the history and are marked as read.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] previous_handle Exclusive lower bound on the instance handle.
 * @param[in] mask Sample, view and instance states (see @ref read_sample_selection).
 * @param[in] collect_sample Callback to invoke for each matching sample.
 *            See @ref dds_read_with_collector_fn_t for serdata ownership.
 * @param[in] collect_sample_arg Application argument passed to the callback.
 *
 * @returns The number of samples successfully collected, 0 if no matching instance follows the cursor,
 *          or a negative error code. A callback error is returned if the first
 *          invocation fails. A later error yields the number already collected.
 * @retval DDS_RETCODE_BAD_PARAMETER The callback is null or `maxs` is outside the stated range.
 * @see @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Take matching samples using an application-defined collector.
 * @ingroup reading
 * @component read_data
 *
 * Successfully collected samples are removed from the reader history.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] handle Instance to select, or `DDS_HANDLE_NIL` for all matching instances.
 * @param[in] mask Sample, view and instance states (see @ref read_sample_selection).
 * @param[in] collect_sample Callback to invoke for each matching sample.
 *            See @ref dds_read_with_collector_fn_t for serdata ownership.
 * @param[in] collect_sample_arg Application argument passed to the callback.
 *
 * @returns The number of samples successfully collected, 0 if no samples match,
 *          or a negative error code. A callback error is returned if the first
 *          invocation fails. A later error yields the number already collected.
 * @retval DDS_RETCODE_BAD_PARAMETER The callback is null or `maxs` is outside the stated range.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET A nonzero `handle` is unknown to this reader.
 * @see @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Take samples from the next matching instance using an application-defined collector.
 * @ingroup reading
 * @component read_data
 *
 * Selects the lowest instance handle greater than `previous_handle` with at
 * least one matching sample. Pass `DDS_HANDLE_NIL` to start and use the instance
 * handle supplied to the callback to advance.
 * Successfully collected samples are removed from the reader history.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * @param[in] reader_or_condition Reader, read condition or query condition.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[in] previous_handle Exclusive lower bound on the instance handle.
 * @param[in] mask Sample, view and instance states (see @ref read_sample_selection).
 * @param[in] collect_sample Callback to invoke for each matching sample.
 *            See @ref dds_read_with_collector_fn_t for serdata ownership.
 * @param[in] collect_sample_arg Application argument passed to the callback.
 *
 * @returns The number of samples successfully collected, 0 if no matching instance follows the cursor,
 *          or a negative error code. A callback error is returned if the first
 *          invocation fails. A later error yields the number already collected.
 * @retval DDS_RETCODE_BAD_PARAMETER The callback is null or `maxs` is outside the stated range.
 * @see @ref entity_handle_errors for errors looking up the entity.
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
 * @brief Defined when the serialized read API (@ref dds_readcdr) is available.
 */
#define DDS_HAS_READCDR 1

/**
 * @brief Peek at matching samples as serdata references.
 * @ingroup reading
 * @component read_data
 *
 * Successfully collected samples keep their sample and view states.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * The caller owns one reference to each returned serdata. Release it with
 * `ddsi_serdata_unref` while the originating domain is still alive. Deleting
 * the last participant can also delete an implicit domain. The serdata
 * reference alone does not keep its type alive after reader deletion.
 * When `si[i].valid_data` is false, the default reader history returns an
 * untyped instance key in `buf[i]`. Its type pointer is null in the default
 * serialization implementation.
 * Use the reader's serialization type with `ddsi_serdata_untyped_to_sample`
 * to convert such a key.
 *
 * @param[in] reader_or_condition Reader handle. Read and query conditions are rejected.
 * @param[out] buf Array with space for `maxs` serdata pointers.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] mask Sample, view and instance states. Omitted categories match any state.
 *
 * @returns The number of references returned, 0 if no samples match, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or `maxs` is outside the stated range.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION The entity is of a kind other than reader.
 * @see @ref entity_handle_errors for errors looking up the reader.
 */
DDS_EXPORT dds_return_t
dds_peekcdr(
  dds_entity_t reader_or_condition,
  struct ddsi_serdata **buf,
  uint32_t maxs,
  dds_sample_info_t *si,
  uint32_t mask);

/**
 * @brief Peek at matching samples of one instance as serdata references.
 * @ingroup reading
 * @component read_data
 *
 * Successfully collected samples keep their sample and view states.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * The caller owns one reference to each returned serdata. Release it with
 * `ddsi_serdata_unref` while the originating domain is still alive. Deleting
 * the last participant can also delete an implicit domain. The serdata
 * reference alone does not keep its type alive after reader deletion.
 * When `si[i].valid_data` is false, the default reader history returns an
 * untyped instance key in `buf[i]`. Its type pointer is null in the default
 * serialization implementation.
 * Use the reader's serialization type with `ddsi_serdata_untyped_to_sample`
 * to convert such a key.
 *
 * @param[in] reader_or_condition Reader handle. Read and query conditions are rejected.
 * @param[out] buf Array with space for `maxs` serdata pointers.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] handle Nonzero handle of an instance known to this reader.
 * @param[in] mask Sample, view and instance states. Omitted categories match any state.
 *
 * @returns The number of references returned, 0 if no samples match, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or `maxs` is outside the stated range.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION The entity is of a kind other than reader.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET `handle` is nil or is unknown to this reader.
 * @see @ref entity_handle_errors for errors looking up the reader.
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
 * @brief Read matching samples as serdata references.
 * @ingroup reading
 * @component read_data
 *
 * Successfully collected samples remain in the history and are marked as read.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * The caller owns one reference to each returned serdata. Release it with
 * `ddsi_serdata_unref` while the originating domain is still alive. Deleting
 * the last participant can also delete an implicit domain. The serdata
 * reference alone does not keep its type alive after reader deletion.
 * When `si[i].valid_data` is false, the default reader history returns an
 * untyped instance key in `buf[i]`. Its type pointer is null in the default
 * serialization implementation.
 * Use the reader's serialization type with `ddsi_serdata_untyped_to_sample`
 * to convert such a key.
 *
 * @param[in] reader_or_condition Reader handle. Read and query conditions are rejected.
 * @param[out] buf Array with space for `maxs` serdata pointers.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] mask Sample, view and instance states. Omitted categories match any state.
 *
 * @returns The number of references returned, 0 if no samples match, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or `maxs` is outside the stated range.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION The entity is of a kind other than reader.
 * @see @ref entity_handle_errors for errors looking up the reader.
 */
DDS_EXPORT dds_return_t
dds_readcdr(
  dds_entity_t reader_or_condition,
  struct ddsi_serdata **buf,
  uint32_t maxs,
  dds_sample_info_t *si,
  uint32_t mask);

/**
 * @brief Read matching samples of one instance as serdata references.
 * @ingroup reading
 * @component read_data
 *
 * Successfully collected samples remain in the history and are marked as read.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * The caller owns one reference to each returned serdata. Release it with
 * `ddsi_serdata_unref` while the originating domain is still alive. Deleting
 * the last participant can also delete an implicit domain. The serdata
 * reference alone does not keep its type alive after reader deletion.
 * When `si[i].valid_data` is false, the default reader history returns an
 * untyped instance key in `buf[i]`. Its type pointer is null in the default
 * serialization implementation.
 * Use the reader's serialization type with `ddsi_serdata_untyped_to_sample`
 * to convert such a key.
 *
 * @param[in] reader_or_condition Reader handle. Read and query conditions are rejected.
 * @param[out] buf Array with space for `maxs` serdata pointers.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] handle Nonzero handle of an instance known to this reader.
 * @param[in] mask Sample, view and instance states. Omitted categories match any state.
 *
 * @returns The number of references returned, 0 if no samples match, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or `maxs` is outside the stated range.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION The entity is of a kind other than reader.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET `handle` is nil or is unknown to this reader.
 * @see @ref entity_handle_errors for errors looking up the reader.
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
 * @brief Take matching samples as serdata references.
 * @ingroup reading
 * @component read_data
 *
 * Successfully collected samples are removed from the reader history.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * The caller owns one reference to each returned serdata. Release it with
 * `ddsi_serdata_unref` while the originating domain is still alive. Deleting
 * the last participant can also delete an implicit domain. The serdata
 * reference alone does not keep its type alive after reader deletion.
 * When `si[i].valid_data` is false, the default reader history returns an
 * untyped instance key in `buf[i]`. Its type pointer is null in the default
 * serialization implementation.
 * Use the reader's serialization type with `ddsi_serdata_untyped_to_sample`
 * to convert such a key.
 *
 * @param[in] reader_or_condition Reader handle. Read and query conditions are rejected.
 * @param[out] buf Array with space for `maxs` serdata pointers.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] mask Sample, view and instance states. Omitted categories match any state.
 *
 * @returns The number of references returned, 0 if no samples match, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or `maxs` is outside the stated range.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION The entity is of a kind other than reader.
 * @see @ref entity_handle_errors for errors looking up the reader.
 */
DDS_EXPORT dds_return_t
dds_takecdr(
  dds_entity_t reader_or_condition,
  struct ddsi_serdata **buf,
  uint32_t maxs,
  dds_sample_info_t *si,
  uint32_t mask);

/**
 * @brief Take matching samples of one instance as serdata references.
 * @ingroup reading
 * @component read_data
 *
 * Successfully collected samples are removed from the reader history.
 * See @ref read_sample_selection for state masks and data-availability statuses.
 *
 * The caller owns one reference to each returned serdata. Release it with
 * `ddsi_serdata_unref` while the originating domain is still alive. Deleting
 * the last participant can also delete an implicit domain. The serdata
 * reference alone does not keep its type alive after reader deletion.
 * When `si[i].valid_data` is false, the default reader history returns an
 * untyped instance key in `buf[i]`. Its type pointer is null in the default
 * serialization implementation.
 * Use the reader's serialization type with `ddsi_serdata_untyped_to_sample`
 * to convert such a key.
 *
 * @param[in] reader_or_condition Reader handle. Read and query conditions are rejected.
 * @param[out] buf Array with space for `maxs` serdata pointers.
 * @param[in] maxs Maximum number of samples, from 1 through INT32_MAX.
 * @param[out] si Array with space for `maxs` sample information records.
 * @param[in] handle Nonzero handle of an instance known to this reader.
 * @param[in] mask Sample, view and instance states. Omitted categories match any state.
 *
 * @returns The number of references returned, 0 if no samples match, or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER `buf` or `si` is null, or `maxs` is outside the stated range.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION The entity is of a kind other than reader.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET `handle` is nil or is unknown to this reader.
 * @see @ref entity_handle_errors for errors looking up the reader.
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
 * Map between instance handles and key values.
 * @code{c}
 * T x = { ... };
 * T y;
 * dds_instance_handle_t ih;
 * ih = dds_lookup_instance (e, &x);
 * dds_instance_get_key (e, ih, &y);
 * @endcode
*/

/**
 * @brief Look up an instance handle by key in the entity's domain.
 * @ingroup instance_handle
 * @component data_instance
 *
 * @param[in] entity Reader or writer that supplies the key type and domain.
 * @param[in] data Sample with its key fields set.
 *
 * @returns The instance handle if found. `DDS_HANDLE_NIL` means that the key
 *          was unknown, `data` was null, the reader or writer handle was invalid,
 *          the entity had the wrong kind, or the key could not be serialized.
 */
DDS_EXPORT dds_instance_handle_t
dds_lookup_instance(dds_entity_t entity, const void *data);

/**
 * @brief Copy the key of a known instance into a sample.
 * @ingroup instance_handle
 * @component data_instance
 *
 * The instance handle and the output sample must be compatible with the entity's
 * type. Once the instance is found, the operation zeroes the destination before
 * converting the key. Release any dynamically allocated contents of a reused
 * sample before calling. A conversion failure may modify the destination.
 *
 * @param[in] entity Reader, writer, read condition or query condition.
 * @param[in] ih Instance handle to look up in the entity's domain.
 * @param[out] data Storage for a sample of the entity's type.
 *
 * @retval DDS_RETCODE_OK The key was copied into the sample.
 * @retval DDS_RETCODE_BAD_PARAMETER `data` is null or the domain's key map has no such handle.
 * @retval DDS_RETCODE_ERROR The key could not be converted to the sample representation.
 * @see @ref entity_handle_errors for errors looking up the entity.
 */
DDS_EXPORT dds_return_t
dds_instance_get_key(
  dds_entity_t entity,
  dds_instance_handle_t ih,
  void *data);

/**
 * @brief Request to begin coherent publishing or access.
 * @ingroup publication
 * @component coherent_sets
 *
 * @param[in] entity Publisher, subscriber, writer or reader.
 *
 * @retval DDS_RETCODE_UNSUPPORTED Cyclone DDS does not implement coherent sets.
 * @see @ref entity_handle_errors for invalid handles and other entity kinds.
 */
DDS_EXPORT dds_return_t
dds_begin_coherent(dds_entity_t entity);

/**
 * @brief Request to end coherent publishing or access.
 * @ingroup publication
 * @component coherent_sets
 *
 * @param[in] entity Publisher, subscriber, writer or reader.
 *
 * @retval DDS_RETCODE_UNSUPPORTED Cyclone DDS does not implement coherent sets.
 * @see @ref entity_handle_errors for invalid handles and other entity kinds.
 */
DDS_EXPORT dds_return_t
dds_end_coherent(dds_entity_t entity);

/**
 * @brief Request data-available notifications for the readers of a subscriber.
 * @ingroup subscriber
 * @component subscriber
 *
 * @param[in] subscriber Subscriber whose readers would receive the notification.
 *
 * @retval DDS_RETCODE_UNSUPPORTED Cyclone DDS does not implement this operation.
 *         No listener callbacks are invoked.
 * @see @ref entity_handle_errors for errors looking up the subscriber.
 */
DDS_EXPORT dds_return_t
dds_notify_readers(dds_entity_t subscriber);

/**
 * @brief Test the trigger state of an entity.
 * @ingroup entity
 * @component entity_status
 *
 * For read and query conditions this checks for matching samples. For guard
 * conditions and waitsets it checks the application-controlled trigger. For
 * status-bearing entities it checks whether any enabled status is set.
 *
 * @param[in] entity Entity to check.
 *
 * @returns 1 if triggered, 0 if clear, or a negative handle lookup error
 *          (see @ref entity_handle_errors).
 */
DDS_EXPORT dds_return_t
dds_triggered(dds_entity_t entity);

/**
 * @brief Get the topic of an endpoint or of a condition's reader.
 * @ingroup entity
 * @component entity_relations
 *
 * @param[in] entity Reader, writer, read condition or query condition.
 *
 * @returns A positive topic handle on success, or a negative error code.
 *          For a built-in reader or a condition on it, the result is the
 *          corresponding built-in topic pseudo-handle.
 * @see @ref entity_handle_errors for errors looking up the entity.
 */
DDS_EXPORT dds_entity_t
dds_get_topic(dds_entity_t entity);

/**
 * @brief Get the instance handles of readers matching a writer.
 * @ingroup builtintopic
 * @component writer
 *
 * If more than @p nrds readers match, the call fills the supplied array with
 * the handles of @p nrds arbitrarily selected matched readers. Otherwise it
 * writes all matched readers' handles and leaves any remaining slots unchanged.
 * Pass a null array and a capacity of zero to request only the count.
 * The current count is also available from
 * @ref dds_get_publication_matched_status.
 *
 * @param[in] writer Writer whose matches to inspect.
 * @param[out] rds Array of matching reader handles, or NULL for a count.
 * @param[in] nrds Array capacity, from 1 through INT32_MAX if an array is supplied.
 *
 * @returns The total number of matched readers, even when it exceeds `nrds`,
 *          or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER The capacity is invalid for the supplied array.
 * @see @ref entity_handle_errors for errors looking up the writer.
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
 * @brief Get the instance handles of writers matching a reader.
 * @ingroup builtintopic
 * @component reader
 *
 * If more than @p nwrs writers match, the call fills the supplied array with
 * the handles of @p nwrs arbitrarily selected matched writers. Otherwise it
 * writes all matched writers' handles and leaves any remaining slots unchanged.
 * Pass a null array and a capacity of zero to request only the count.
 * The current count is also available from
 * @ref dds_get_subscription_matched_status.
 *
 * @param[in] reader Reader whose matches to inspect.
 * @param[out] wrs Array of matching writer handles, or NULL for a count.
 * @param[in] nwrs Array capacity, from 1 through INT32_MAX if an array is supplied.
 *
 * @returns The total number of matched writers, even when it exceeds `nwrs`,
 *          or a negative error code.
 * @retval DDS_RETCODE_BAD_PARAMETER The capacity is invalid for the supplied array.
 * @see @ref entity_handle_errors for errors looking up the reader.
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
 * @brief Borrow type information from a built-in endpoint sample.
 * @ingroup builtintopic
 * @component builtin_topic
 *
 * The sample's QoS owns the result. Keep the sample and its QoS alive while
 * using the type information. Freeing the sample or returning its reader loan
 * ends the borrow.
 *
 * @param[in] builtintopic_endpoint Built-in endpoint sample.
 * @param[out] type_info Receives the borrowed pointer, or NULL if the sample
 *             has no type information. Unchanged on error.
 *
 * @retval DDS_RETCODE_OK The output was set, including when type information is absent.
 * @retval DDS_RETCODE_BAD_PARAMETER Either argument is null.
 */
DDS_EXPORT dds_return_t
dds_builtintopic_get_endpoint_type_info (
  dds_builtintopic_endpoint_t * builtintopic_endpoint,
  const dds_typeinfo_t ** type_info);

#endif /* DDS_HAS_TYPELIB */

/**
 * @brief Free a caller-owned built-in endpoint sample.
 * @ingroup builtintopic
 * @component builtin_topic
 *
 * Releases the fields and the sample allocation. This includes results of
 * @ref dds_get_matched_subscription_data and @ref dds_get_matched_publication_data.
 * Use @ref dds_return_loan for loaned reader samples.
 *
 * @param[in] builtintopic_endpoint Non-null, caller-owned heap allocation.
 */
DDS_EXPORT void
dds_builtintopic_free_endpoint (
  dds_builtintopic_endpoint_t * builtintopic_endpoint);

/**
 * @brief Free a caller-owned built-in topic sample.
 * @ingroup builtintopic
 * @component builtin_topic
 *
 * Releases the fields and the sample allocation.
 * Use @ref dds_return_loan for loaned reader samples.
 *
 * @param[in] builtintopic_topic Non-null, caller-owned heap allocation.
 */
DDS_EXPORT void
dds_builtintopic_free_topic (
  dds_builtintopic_topic_t * builtintopic_topic);

/**
 * @brief Free a caller-owned built-in participant sample.
 * @ingroup builtintopic
 * @component builtin_topic
 *
 * Releases the fields and the sample allocation.
 * Use @ref dds_return_loan for loaned reader samples.
 *
 * @param[in] builtintopic_participant Non-null, caller-owned heap allocation.
 */
DDS_EXPORT void
dds_builtintopic_free_participant (
  dds_builtintopic_participant_t * builtintopic_participant);

/**
 * @brief Manually assert the liveliness of a writer or participant.
 * @ingroup entity
 * @component participant
 *
 * Use this operation with the manual-by-participant or manual-by-topic
 * liveliness QoS policy to indicate continued activity.
 *
 * @param[in] entity Participant or writer.
 *
 * @retval DDS_RETCODE_OK The liveliness assertion was processed.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET The writer is no longer in the domain's writer index.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES The writer could not allocate a heartbeat message.
 * @see @ref entity_handle_errors for errors looking up the entity.
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
