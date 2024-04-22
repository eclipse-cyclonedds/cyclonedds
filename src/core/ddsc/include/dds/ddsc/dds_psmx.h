// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

/**
 * @defgroup psmx (Publish Subscribe Message Exchange)
 * @ingroup dds
 * The Publish Subscribe Message Exchange (PSMX) interface allows implementing additional methods of pub-sub data-exchange
 * as an alternative to Cyclone's networked transport. The PSMX interface allows to load implementations as plugins,
 * so there is no need for compile-time linking.
 */
#ifndef DDS_PSMX_H
#define DDS_PSMX_H

#include "dds/export.h"
#include "dds/dds.h"
#include "dds/ddsc/dds_loaned_sample.h"
#include "dds/ddsc/dds_data_type_properties.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define DDS_MAX_PSMX_INSTANCES 1

struct dds_psmx;
struct dds_psmx_topic;
struct dds_psmx_topic_list_elem;
struct dds_psmx_endpoint;
struct dds_psmx_endpoint_list_elem;

/**
 * @brief Type of the PSMX endpoint
 * @ingroup psmx
 */
typedef enum dds_psmx_endpoint_type {
  DDS_PSMX_ENDPOINT_TYPE_UNSET,
  DDS_PSMX_ENDPOINT_TYPE_READER,
  DDS_PSMX_ENDPOINT_TYPE_WRITER
} dds_psmx_endpoint_type_t;

/**
 * @brief Identifier for the PSMX instance
 * @ingroup psmx
 */
typedef uint32_t dds_psmx_instance_id_t;

/**
 * @brief Defines the features that can be supported by PSMX implementation
 */
#define DDS_PSMX_FEATURE_SHARED_MEMORY  1ul << 0
#define DDS_PSMX_FEATURE_ZERO_COPY      1ul << 1
typedef uint32_t dds_psmx_features_t;

/**
 * @brief describes the data which is transferred in addition to just the sample
 * @ingroup psmx
 */
typedef struct dds_psmx_metadata {
  dds_loaned_sample_state_t sample_state;
  dds_loan_data_type_t data_type;
  dds_psmx_instance_id_t instance_id;
  uint32_t sample_size;
  dds_guid_t guid;
  dds_time_t timestamp;
  uint32_t statusinfo;
  uint16_t cdr_identifier;
  uint16_t cdr_options;
} dds_psmx_metadata_t;

/**
 * @brief identifier used to distinguish between PSMX instances on nodes
 * @ingroup psmx
 */
typedef struct dds_psmx_node_identifier
{
  uint8_t x[16];
} dds_psmx_node_identifier_t;

/**
 * @brief Definition for function that checks QoS support
 * @ingroup psmx
 *
 * Definition for function that checks whether the provided QoS
 * is supported by the PSMX implementation.
 *
 * @param[in] psmx_instance The PSMX instance.
 * @param[in] forwhat whether for a topic/writer/reader (UNSET if topic)
 * @param[in] data_type_props Data type properties
 * @param[in] qos  The QoS.
 * @returns true if the QoS is supported, false otherwise
 */
typedef bool (*dds_psmx_type_qos_supported_fn) (struct dds_psmx *psmx_instance, dds_psmx_endpoint_type_t forwhat, dds_data_type_properties_t data_type_props, const struct dds_qos *qos);

/**
 * @brief Definition for function to create a topic
 * @ingroup psmx
 * Definition for a function that is called to create a new topic
 * for a PSMX instance.
 *
 * @param[in] psmx_instance  The PSMX instance.
 * @param[in] topic_name  The name of the topic to create
 * @param[in] type_name The name of the DDS data type for this topic
 * @param[in] data_type_props  The data type properties for the topic's data type.
 * @returns a PSMX topic structure
 */
typedef struct dds_psmx_topic * (* dds_psmx_create_topic_fn) (
    struct dds_psmx * psmx_instance,
    const char * topic_name,
    const char * type_name,
    dds_data_type_properties_t data_type_props);

/**
 * @brief Definition for function to destruct a topic
 * @ingroup psmx
 * Definition for a function that is called on topic destruction.
 *
 * @param[in] psmx_topic  The PSMX topic to destruct
 * @returns a DDS return code
 *
 */
typedef dds_return_t (*dds_psmx_delete_topic_fn) (struct dds_psmx_topic *psmx_topic);

/**
 * @brief Function definition for pubsub message exchange cleanup
 * @ingroup psmx
 * @param[in] psmx_instance  the psmx instance to de-initialize
 * @returns a DDS return code
 */
typedef dds_return_t (* dds_psmx_deinit_fn) (struct dds_psmx *psmx_instance);

/**
 * @brief Definition for PSMX locator generation function
 * @ingroup psmx
 * Returns a locator which is unique between nodes, but identical for instances on
 * the same node
 *
 * @param[in] psmx_instance  a PSMX instance
 * @returns a unique node identifier (locator)
 */
typedef dds_psmx_node_identifier_t (* dds_psmx_get_node_identifier_fn) (const struct dds_psmx *psmx_instance);

/**
 * @brief Definition for PSMX function to get supported features
 * @ingroup psmx
 * Returns an integer with the flags set for the features that are
 * supported by the provided PSMX instance.
 *
 * @param[in] psmx_instance  a PSMX instance
 * @returns the set of features supported by this PSMX instance
 */
typedef dds_psmx_features_t (* dds_psmx_supported_features_fn) (const struct dds_psmx *psmx_instance);


/**
 * @brief functions which are used on a PSMX instance
 * @ingroup psmx
 */
typedef struct dds_psmx_ops {
  dds_psmx_type_qos_supported_fn   type_qos_supported;
  dds_psmx_create_topic_fn         create_topic;
  dds_psmx_delete_topic_fn         delete_topic;
  dds_psmx_deinit_fn               deinit;
  dds_psmx_get_node_identifier_fn  get_node_id;
  dds_psmx_supported_features_fn   supported_features;
} dds_psmx_ops_t;

/**
 * @brief Definition of function to create an endpoint for a topic
 * @ingroup psmx
 *
 * @param[in] psmx_topic  The PSMX topic to create the endpoint for
 * @param[in] endpoint_type  The type of endpoint to create (publisher or subscriber)
 * @returns A PSMX endpoint struct
 */
typedef struct dds_psmx_endpoint * (* dds_psmx_create_endpoint_fn) (struct dds_psmx_topic *psmx_topic, const struct dds_qos *qos, dds_psmx_endpoint_type_t endpoint_type);

/**
 * @brief Definition of function to delete an PSMX endpoint
 * @ingroup psmx
 *
 * @param[in] psmx_endpoint  The endpoint to be deleted
 * @returns a DDS return code
 */
typedef dds_return_t (* dds_psmx_delete_endpoint_fn) (struct dds_psmx_endpoint *psmx_endpoint);

/**
 * @brief functions which are used on a PSMX topic
 * @ingroup psmx
 */
typedef struct dds_psmx_topic_ops {
  dds_psmx_create_endpoint_fn        create_endpoint;
  dds_psmx_delete_endpoint_fn        delete_endpoint;
} dds_psmx_topic_ops_t;


/**
 * @brief Definition for function to requests a loan from the PSMX
 * @ingroup psmx
 *
 * @param[in] psmx_endpoint   the endpoint to loan from
 * @param[in] size_requested  the size of the loan requested
 * @returns a pointer to the loaned block on success
 */
typedef dds_loaned_sample_t * (* dds_psmx_endpoint_request_loan_fn) (struct dds_psmx_endpoint *psmx_endpoint, uint32_t size_requested);

/**
 * @brief Definition of function to write data on a PSMX endpoint
 * @ingroup psmx
 *
 * @param[in] psmx_endpoint    The endpoint to publish the data on
 * @param[in] data    The data to publish
 * @returns a DDS return code
 */
typedef dds_return_t (* dds_psmx_endpoint_write_fn) (struct dds_psmx_endpoint *psmx_endpoint, dds_loaned_sample_t *data);

/**
 * @brief Definition of function to take data from an PSMX endpoint
 * @ingroup psmx
 *
 * Used in a poll based implementation.
 *
 * @param[in] psmx_endpoint The endpoint to take the data from
 * @returns the oldest unread received block of memory
 */
typedef dds_loaned_sample_t * (* dds_psmx_endpoint_take_fn) (struct dds_psmx_endpoint *psmx_endpoint);

/**
 * @brief Definition of function to set the a callback function on an PSMX endpoint
 * @ingroup psmx
 *
 * @param[in] psmx_endpoint the endpoint to set the callback function on
 * @param[in] reader        the DDS reader associated with the endpoint
 * @returns a DDS return code
 */
typedef dds_return_t (* dds_psmx_endpoint_on_data_available_fn) (struct dds_psmx_endpoint *psmx_endpoint, dds_entity_t reader);

/**
 * @brief Functions that are used on a PSMX endpoint
 * @ingroup psmx
 */
typedef struct dds_psmx_endpoint_ops {
  dds_psmx_endpoint_request_loan_fn       request_loan;
  dds_psmx_endpoint_write_fn              write;
  dds_psmx_endpoint_take_fn               take;
  dds_psmx_endpoint_on_data_available_fn  on_data_available;
} dds_psmx_endpoint_ops_t;

/**
 * @brief the top-level entry point on the PSMX is bound to a specific implementation of a PSMX
 * @ingroup psmx
 */
typedef struct dds_psmx {
  dds_psmx_ops_t ops; //!< associated functions
  const char *instance_name; //!< name of this PSMX instance
  int32_t priority; //!< priority of choosing this interface
  const struct ddsi_locator *locator; //!< the locator for this PSMX instance
  dds_psmx_instance_id_t instance_id; //!< the identifier of this PSMX instance
  struct dds_psmx_topic_list_elem *psmx_topics; //!< associated topics
} dds_psmx_t;

/**
 * @brief the topic-level PSMX
 * @ingroup psmx
 *
 * this will exchange data for readers and writers which are matched through discovery
 * will only exchange a single type of data
 */
typedef struct dds_psmx_topic {
  dds_psmx_topic_ops_t ops; //!< associated functions
  struct dds_psmx *psmx_instance; //!< the PSMX instance which created this topic
  char * topic_name; //!< the topic name
  char * type_name; //!< the type name
  dds_loan_data_type_t data_type; //!< the unique identifier associated with the data type of this topic
  struct dds_psmx_endpoint_list_elem *psmx_endpoints; //!< associated endpoints
  dds_data_type_properties_t data_type_props; //!< the properties of the datatype associated with this topic
} dds_psmx_topic_t;

/**
 * @brief the definition of one instance of a dds reader/writer using a PSMX instance
 * @ingroup psmx
 */
typedef struct dds_psmx_endpoint {
  dds_psmx_endpoint_ops_t ops; //!< associated functions
  struct dds_psmx_topic * psmx_topic; //!< the topic this endpoint belongs to
  dds_psmx_endpoint_type_t endpoint_type; //!< type type of endpoint
} dds_psmx_endpoint_t;


/**
 * @brief adds a topic to the list
 * @ingroup psmx
 *
 * will create the first list entry if it does not yet exist
 *
 * @param[in] psmx_topic     the topic to add
 * @param[in,out] list  list to add the topic to
 * @return DDS_RETCODE_OK on success
 */
DDS_EXPORT dds_return_t dds_add_psmx_topic_to_list (struct dds_psmx_topic *psmx_topic, struct dds_psmx_topic_list_elem **list);

/**
 * @brief removes a topic from the list
 * @ingroup psmx
 *
 * will set the pointer to the list to null if the last entry is removed
 *
 * @param[in] psmx_topic     the topic to remove
 * @param[in,out] list  list to remove the topic from
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t dds_remove_psmx_topic_from_list (struct dds_psmx_topic *psmx_topic, struct dds_psmx_topic_list_elem **list);

/**
 * @brief adds an endpoint to the list
 * @ingroup psmx
 *
 * will create the first list entry if it does not yet exist
 *
 * @param[in] psmx_endpoint   the endpoint to add
 * @param[in,out] list   list to add the endpoint to
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t dds_add_psmx_endpoint_to_list (struct dds_psmx_endpoint *psmx_endpoint, struct dds_psmx_endpoint_list_elem **list);

/**
 * @brief removes an endpoint from the list
 * @ingroup psmx
 *
 * will set the pointer to the list to null if the last entry is removed
 *
 * @param[in] psmx_endpoint  the endpoint to remove
 * @param[in,out] list  list to remove the endpoint from
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t dds_remove_psmx_endpoint_from_list (struct dds_psmx_endpoint *psmx_endpoint, struct dds_psmx_endpoint_list_elem **list);

/**
 * @brief initialization function for PSMX instance
 * @ingroup psmx
 *
 * Should be called from all constructors of class which inherit from dds_psmx_t
 *
 * @param[in] psmx  the PSMX instance to initialize
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t dds_psmx_init_generic (struct dds_psmx *psmx);

/**
 * @brief cleanup function for a PSMX instance
 * @ingroup psmx
 *
 * Should be called from all destructors of classes which inherit from dds_psmx_t
 *
 * @param[in] psmx  the PSMX instance to cleanup
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t dds_psmx_cleanup_generic (struct dds_psmx *psmx);

/**
 * @brief init function for topic
 * @ingroup psmx
 *
 * Should be called from all constructors of classes which inherit from struct dds_psmx_topic
 *
 * @param[in] psmx_topic  the topic to initialize
 * @param[in] ops vtable for this psmx_topic
 * @param[in] psmx  the PSMX instance
 * @param[in] topic_name  the topic name
 * @param[in] type_name the DDS type name for this topic
 * @param[in] data_type_props the data type's properties
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t dds_psmx_topic_init_generic (struct dds_psmx_topic *psmx_topic, const dds_psmx_topic_ops_t *ops, const struct dds_psmx *psmx, const char *topic_name, const char *type_name, dds_data_type_properties_t data_type_props);

/**
 * @brief cleanup function for a topic
 * @ingroup psmx
 *
 * Should be called from all destructors of classes which inherit from struct dds_psmx_topic
 *
 * @param[in] psmx_topic   the topic to de-initialize
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t dds_psmx_topic_cleanup_generic(struct dds_psmx_topic *psmx_topic);


/**
 * @brief Gets the supported features for a PSMX instance
 * @ingroup psmx
 *
 * Returns the set of supported features for the provided PSMX instance.
 *
 * @param[in] psmx_instance   the PSMX instance
 * @return the set of features supported by this PSMX instance
 */
DDS_EXPORT dds_psmx_features_t dds_psmx_supported_features (const struct dds_psmx *psmx_instance);


#if defined (__cplusplus)
}
#endif

#endif /* DDS_PSMX_H */
