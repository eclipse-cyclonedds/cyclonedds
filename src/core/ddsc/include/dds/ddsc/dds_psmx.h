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
 *
 * OVERVIEW
 * ========
 *
 * The Publish Subscribe Message Exchange (PSMX) interface provides support for
 * off-loading data communication from the network stack of Cyclone to arbitrary pub-sub
 * transport implementations.[^1]  This section provides an overview of the structure and
 * introduces some terminology, details on specific operations are provided at the
 * definitions of those operations.
 *
 * A Cyclone DDS Domain consists of a plurality of DDS Domain Entities, which are the
 * representations of the DDS Domain in a specific process.
 *
 * A PSMX Plugin provides an implementation of the PSMX interface, allowing the
 * instantiation of a PSMX Instance to establish a connection between a DDS Domain Entity
 * and a PSMX Domain.  The PSMX Plugin is specified as a library and a PSMX Instance Name.
 * The library is loaded in the process and a constructor function provided by the library
 * is invoked to create and initialize a PSMX Instance given the PSMX Instance Name (and a
 * configuration string).  In principle a specific library may be configured multiple
 * times in a single DDS Domain.
 *
 * The PSMX Instance Name is assumed to uniquely identify the PSMX Domain in the DDS
 * Domain.  From the PSMX Instance Name, a numeric PSMX Instance ID is derived that
 * uniquely identifies the PSMX Domain within the DDS Domain Entity and is assumed to
 * uniquely identify the PSMX Domain in the DDS Domain.[^2]
 *
 * Each PSMX Instance chooses a 16-byte PSMX Locator[^3] such that any pair of instances
 * with the same PSMX Locator communicate, and any pair with different locators do not
 * communicate.[^4]
 *
 * DDS Topics, DDS Readers and DDS Writers are mapped to corresponding objects in PSMX
 * Instances.  For DDS Readers and DDS Writers, the application can restrict the set of
 * PSMX Instances for which the mapping is created using the "PSMX Instances" QoS setting,
 * and the PSMX Instances can refuse mapping based on type and QoS information.
 *
 * DDS Topic Entities are representations of the topics in the DDS Domain, such that two
 * identical definitions of a topic in a DDS Domain Entity give rise to two
 * application-level DDS Topic Entities, but only to a single topic in the DDS Domain
 * Entity and thus also only one PSMX Topic object per PSMX Instance.
 *
 * Each DDS Reader/Writer is mapped to a set of PSMX Reader/Writer Endpoints, one for each
 * PSMX Instance in the "PSMX Instances" QoS that accepts the type and reader/writer QoS.
 * An associated set of PSMX Domains consisting of the PSMX Domains for which PSMX
 * Reader/Writer Endpoints have been created is assumed to exist.
 *
 * The PSMX Domain is assumed to deliver data published by the PSMX Writer associated with
 * DDS Writer X to all PSMX Readers associated with the DDS Readers Ys that match X[^5],
 * optionally excluding DDS Readers in the same Domain Entity as X.  It is assumed to not
 * deliver data to other DDS Readers in the DDS Domain.  It is assumed to do this with a
 * quality of service compatible with the DDS QoS.
 *
 * Readers associated with DDS Readers in the same DDS Domain Entity.
 *
 * If the intersection of the sets of PSMX Domains of a DDS Reader and a DDS Writer in a
 * DDS Domain:
 *
 * - is empty, off-loading data transfer to PSMX (for this pair) is not possible;
 *
 * - contains one instance, that PSMX Domain is eligible for off-loading data transfer;
 *
 * - contains multiple instances, the configuration is invalid.
 *
 * If an eligible PSMX Domain exists and the PSMX Locators for the corresponding two PSMX
 * Instances are the same, then PSMX is used to transfer data.
 *
 * The PSMX objects are represented in the interface as pointers to "dds_psmx",
 * "dds_psmx_topic", "dds_psmx_endpoint".  The PSMX Plugin is responsible for allocating
 * and freeing these.  It is expected that the PSMX Plugin internally uses an extended
 * version of these types to store any additional data it needs.  E.g., a hypothetical
 * "weed" PSMX Plugin could do:
 *
 *     struct psmx_weed {
 *       struct dds_psmx c;
 *       weed_root *x;
 *     };
 *
 * The creator function mentioned above is required to be called NAME_create_psmx, where
 * NAME is the value of the "name" attribute of the PubSubMessageExchange interface
 * configuration element.  It must have the following signature:
 *
 *     dds_return_t NAME_create_psmx (
 *       struct dds_psmx **psmx_instance,
 *       dds_psmx_instance_id_t identifier,
 *       const char *config)
 *
 * Where
 *
 *   *psmx_instance must be set point to a new PSMX Instance on success and may be left
 *                  undefined on error
 *   identifier     contains the numeric PSMX Instance ID
 *   config         the PSMX configuration from the "config" attribute of the
 *                  PubSubMessageExchange interface configuration element.
 *
 * The "config" argument is a contiguous sequence of characters terminated by the first
 * double-\0.  Each \0-terminated character sequence is a string that consists of
 * KEY=VALUE pairs, where each K-V pair is terminated by a semicolon.
 *
 * If the configuration string as set in Cyclone DDS configuration contains a
 * "INSTANCE_NAME" key, its value is used as the PSMX Instance Name.  If the key is not
 * included, the value of the "name" attribute of the corresponding PubSubMessageExchange
 * element in configuration is used as the PSMX Instance Name.  In all cases, looking up
 * the "INSTANCE_NAME" key in the configuration string using @ref
 * dds_psmx_get_config_option_value will return the PSMX Instance Name as its value.
 *
 * The behaviour of the constructor function is dependent on the interface version it
 * implements:
 *
 * - For version 0, it is responsible for setting:
 *
 *   - ops           to the addresses of the various functions implementing the operations
 *   - instance_name to a "dds_alloc" allocated string
 *   - instance_id   to the "identifier" argument
 *
 *   and for zero-initializing the other fields.  At some point after this initialization,
 *   and once it is prepared to handle the "get_node_id" operation, it must invoke the
 *   "dds_psmx_init_generic" to complete the initialization.
 *
 * - For version 1, it is responsible for setting:
 *
 *   - ops
 *
 *   All other fields will be initialized by the Cyclone DDS after succesful return and
 *   the "get_node_id" operation also will be invoked after the constructor returned.
 *
 * Whether the plugin implements version 0 or version 1 of the interface is controlled by
 * the function pointers in "dds_psmx_ops_t".  If "create_topic" and "deinit" are
 * non-null, it is version 0; if both are null it is version 1.  Neither
 * "create_topic_type" nor "delete_psmx" is touched by Cyclone DDS if the interface is
 * version 0, allowing for binary backwards compatibility.
 *
 * -- Footnotes: --
 *
 * [^1]: In particular including shared-memory based mechanisms.
 *
 * [^2]: Internally, the name is not used for anything other than the generation of the
 * numeric id.
 *
 * [^3]: Confusingly named "node identifier" in the interface, even though it has nothing
 * to do with the numeric PSMX Domain identifier.
 *
 * [^4]: This typically matches a machine when the transport is shared memory.
 *
 * [^5]: That is, the matching rules between Readers and Writers defined in the DDS
 * specification.
 */
#ifndef DDS_PSMX_H
#define DDS_PSMX_H

#include "dds/export.h"
#include "dds/dds.h"
#include "dds/ddsc/dds_loaned_sample.h"
#include "dds/ddsc/dds_data_type_properties.h"
#include "dds/ddsrt/dynlib.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define DDS_MAX_PSMX_INSTANCES 8 // No particular reason for this specific number, except that it's probably enough, usually.

struct dds_psmx;
struct dds_psmx_topic;
struct dds_psmx_endpoint;
struct ddsi_type;

/** @brief Type of the PSMX endpoint
 * @ingroup psmx
 *
 * The PSMX interface distinguishes between readers and writers, and in some cases needs
 * to specify that an operation applies to both or neither (depending on context).
 * "Reader" and "writer" here are used as in DDS, outside the DDS context these often are
 * known as subscribers and publishers.
 */
typedef enum dds_psmx_endpoint_type {
  DDS_PSMX_ENDPOINT_TYPE_UNSET,  /**< inapplicable or applies to both */
  DDS_PSMX_ENDPOINT_TYPE_READER, /**< applies to reader only */
  DDS_PSMX_ENDPOINT_TYPE_WRITER  /**< applies to writer only */
} dds_psmx_endpoint_type_t;

/** @brief The numeric PSMX Instance ID
 * @ingroup psmx
 */
typedef uint32_t dds_psmx_instance_id_t;

/**
 * @brief Flags for identifying features supported by a PSMX implementation
 */
#define DDS_PSMX_FEATURE_SHARED_MEMORY  1ul << 0 /**< Uses shared memory */
#define DDS_PSMX_FEATURE_ZERO_COPY      1ul << 1 /**< Supports zero-copy */
typedef uint32_t dds_psmx_features_t;

/**
 * @brief Describes the data which is transferred in addition to the application data
 * @ingroup psmx
 *
 * All fields are filled in by Cyclone DDS during the write prior to handing it to the
 * PSMX plug-in.  The plug-in is only required to pass the contents to Cyclone DDS on the
 * receiving side.
 */
typedef struct dds_psmx_metadata {
  dds_loaned_sample_state_t sample_state; /**< Representation of this sample/key */
  dds_loan_data_type_t data_type;         /**< Reserved, must be 0 */
  dds_psmx_instance_id_t instance_id;     /**< PSMX instance id for this sample */
  uint32_t sample_size;    /**< Size of payload */
  dds_guid_t guid;         /**< GUID of original writer */
  dds_time_t timestamp;    /**< Source timestamp of DDS sample */
  uint32_t statusinfo;     /**< DDSI status info (write/dispose/unregister) */
  uint16_t cdr_identifier; /**< CDR encoding (DDSI_RTPS_SAMPLE_NATIVE if not serialized) */
  uint16_t cdr_options;    /**< CDR options field (0 if not serialized) */
} dds_psmx_metadata_t;

/**
 * @brief PSMX Locator
 * @ingroup psmx
 */
typedef struct dds_psmx_node_identifier {
  uint8_t x[16];
} dds_psmx_node_identifier_t;

/**
 * @brief Definition of the function checking support for type and QoS
 * @ingroup psmx
 *
 * A PSMX Instance in the "PSMX Instances" QoS may accept or reject a given DDS
 * Topic/Reader/Writer based on information on the type and the QoS settings.  PSMX
 * Instances not in the "PSMX Instances" QoS setting are not asked.
 *
 * @param[in] psmx_instance    The PSMX Instance
 * @param[in] forwhat          Whether for a topic/writer/reader (UNSET if topic)
 * @param[in] data_type_props  Data type properties (see there)
 * @param[in] qos              QoS of the DDS topic
 * @returns True if the PSMX Instance is willing to handle this DDS entity, false otherwise
 */
typedef bool (*dds_psmx_type_qos_supported_fn) (struct dds_psmx *psmx_instance, dds_psmx_endpoint_type_t forwhat, dds_data_type_properties_t data_type_props, const struct dds_qos *qos);

/**
 * @brief Definition of the function for creating a PSMX Topic (interface version 1)
 * @ingroup psmx
 *
 * Definition for a function that is called to construct a new PSMX Topic in a PSMX Instance
 * representing a new topic in the DDS Domain Entity.
 *
 * The PSMX Plugin is expected to represent a PSMX Topic using an extended version of the
 * `dds_psmx_topic` structure.
 *
 * If `type_definition` is not a null pointer, it points into the Cyclone type library. A
 * (default, C) serializer can be constructed using `ddsi_topic_descriptor_from_type`,
 * an XTypes TypeObject using `ddsi_type_get_typeobj`.
 *
 * If the function returns failure, creation of the DDS Entity fails.
 *
 * @param[in] psmx_instance    The PSMX instance.
 * @param[in] topic_name       The name of the topic to create
 * @param[in] type_name        The name of the DDS data type for this topic
 * @param[in] data_type_props  The data type properties for the topic's data type.
 * @param[in] type_definition  Optional pointer to type definition in type library.
 * @param[in] sizeof_type      In-memory size of a single instance of the type, 0 if unknown.
 * @returns A new PSMX Topic structure, or NULL on error
 */
typedef struct dds_psmx_topic * (*dds_psmx_create_topic_with_type_fn) (
    struct dds_psmx *psmx_instance,
    const char *topic_name,
    const char *type_name,
    dds_data_type_properties_t data_type_props,
    const struct ddsi_type *type_definition,
    uint32_t sizeof_type);

/**
 * @brief Definition of the function for destructing and freeing a PSMX Topic
 * @ingroup psmx
 *
 * Definition for a function that is called on deleting the topic in the DDS Domain.
 * Called exactly once for each successful invocation of `dds_psmx_create_topic`/
 * `dds_psmx_create_topic_with_type`, all PSMX Endpoints related to this PSMX Topic will have
 * been destructed prior to calling this function.
 *
 * If the PSMX Topic was created using `dds_psmx_create_topic`, the PSMX Plugin is
 * required to call `dds_psmx_cleanup_topic_generic` and to do so prior to invalidating
 * the memory associated with the PSMX Topic and releasing any memory allocated for it
 * during construction.
 *
 * @param[in] psmx_topic       The PSMX Topic to destruct
 * @returns A DDS return code, should be DDS_RETCODE_OK.
 */
typedef dds_return_t (*dds_psmx_delete_topic_fn) (struct dds_psmx_topic *psmx_topic);

/**
 * @brief Definition of the function for creating a PSMX Topic (interface version 0)
 * @ingroup psmx
 *
 * Equivalent to `dds_psmx_create_topic_type` with `type_definition` and `sizeof_type`
 * a null pointer and 0, respectively. It is required to initialize the generic fields using
 * `dds_psmx_topic_init_generic`.
 *
 * @param[in] psmx_instance    The PSMX instance.
 * @param[in] topic_name       The name of the topic to create
 * @param[in] type_name        The name of the DDS data type for this topic
 * @param[in] data_type_props  The data type properties for the topic's data type.
 * @returns A new PSMX Topic structure, or NULL on error
 */
typedef struct dds_psmx_topic * (*dds_psmx_create_topic_fn) (
    struct dds_psmx * psmx_instance,
    const char * topic_name,
    const char * type_name,
    dds_data_type_properties_t data_type_props);

/**
 * @brief Definition of the function for destructing and freeing a PSMX Instance (interface version 1)
 * @ingroup psmx
 *
 * Function called on shutdown of a DDS Domain Entity, before unloading the PSMX Plugin
 * and after all other operations have completed and all objects created in the PSMX
 * Instance have been destructed using the various "delete" functions.
 *
 * The PSMX Plugin is required to call `dds_psmx_fini` and to do so prior to
 * invalidating the memory associated with the PSMX Instance and releasing any memory
 * allocated for it during construction.
 *
 * @param[in] psmx_instance    The PSMX Instance to de-initialize
 */
typedef void (*dds_psmx_delete_fn) (struct dds_psmx *psmx_instance);

/**
 * @brief Definition of the function for destructing and freeing a PSMX Instance (interface version 0)
 * @ingroup psmx
 *
 * Function called on shutdown of a DDS Domain Entity, before unloading the PSMX Plugin
 * and after all other operations have completed and all objects created in the PSMX
 * Instance have been destructed using the various "delete" functions.
 *
 * The PSMX Plugin is required to call `dds_psmx_cleanup_generic` and to do so prior to
 * invalidating the memory associated with the PSMX Instance and releasing any memory
 * allocated for it during construction.
 *
 * @param[in] psmx_instance    The PSMX Instance to de-initialize
 * @returns A DDS return code, should be DDS_RETCODE_OK.
 */
typedef dds_return_t (*dds_psmx_deinit_fn) (struct dds_psmx *psmx_instance);

/**
 * @brief Definition of the function returning the PSMX Locator
 * @ingroup psmx
 *
 * All PSMX Instances that can communicate must return the same PSMX Locator, PSMX
 * Instances that do not communicate must return different PSMX Locators.
 *
 * The function may be called by `dds_psmx_init_generic`.
 *
 * @param[in] psmx_instance    A PSMX Instance
 * @returns The PSMX Locator for this PSMX Instance
 */
typedef dds_psmx_node_identifier_t (*dds_psmx_get_node_identifier_fn) (const struct dds_psmx *psmx_instance);

/**
 * @brief Definition of the function to query the features supported by the PSMX Instance
 * @ingroup psmx
 *
 * Returns an integer with the flags set for the features that are supported by the
 * provided PSMX Instance.
 *
 * @param[in] psmx_instance    A PSMX instance
 * @returns The set of features supported by this PSMX instance
 */
typedef dds_psmx_features_t (*dds_psmx_supported_features_fn) (const struct dds_psmx *psmx_instance);

/**
 * @brief Table of pointers to functions operating on a PSMX Instance
 * @ingroup psmx
 */
typedef struct dds_psmx_ops {
  dds_psmx_type_qos_supported_fn type_qos_supported;
  dds_psmx_create_topic_fn create_topic; /**< non-null for interface version 0, null for version 1 */
  dds_psmx_delete_topic_fn delete_topic;
  dds_psmx_deinit_fn deinit; /**< non-null for interface version 0, null for version 1 */
  dds_psmx_get_node_identifier_fn get_node_id;
  dds_psmx_supported_features_fn supported_features;
  dds_psmx_create_topic_with_type_fn create_topic_with_type; /**< undefined for interface version 0, non-null for version 1 */
  dds_psmx_delete_fn delete_psmx; /**< undefined for interface version 0, non-null for version 1 */
} dds_psmx_ops_t;

/**
 * @brief Definition of the function for constructing a PSMX Endpoint for a PSMX Topic
 * @ingroup psmx
 *
 * @param[in] psmx_topic       The PSMX Topic to create the PSMX Endpoint for
 * @param[in] qos              QoS of the corresponding DDS endpoint
 * @param[in] endpoint_type    The type of endpoint to create, either READER or WRITER
 * @returns A new PSMX Endpoint or a null pointer on error
 */
typedef struct dds_psmx_endpoint * (*dds_psmx_create_endpoint_fn) (struct dds_psmx_topic *psmx_topic, const struct dds_qos *qos, dds_psmx_endpoint_type_t endpoint_type);

/**
 * @brief Definition of the function for destructing a PSMX Endpoint
 * @ingroup psmx
 *
 * Called after the last operation on the PSMX Endpoint has completed and no further
 * operations will be invoked; called prior to destructing the PSMX Topic.
 *
 * @param[in] psmx_endpoint    The PSMX Endpoint to be destructed
 * @returns a DDS return code, should be DDS_RETCODE_OK
 */
typedef dds_return_t (*dds_psmx_delete_endpoint_fn) (struct dds_psmx_endpoint *psmx_endpoint);

/**
 * @brief Table of pointers to functions operating on a PSMX Topic
 * @ingroup psmx
 */
typedef struct dds_psmx_topic_ops {
  dds_psmx_create_endpoint_fn create_endpoint;
  dds_psmx_delete_endpoint_fn delete_endpoint;
} dds_psmx_topic_ops_t;

/**
 * @brief Definition of the function for requesting a loan from a PSMX Writer
 * @ingroup psmx
 *
 * The PSMX Instance is the owner of the loan and implements the allocation and the
 * freeing of the memory for the `dds_loaned_sample_t`, the `dds_psmx_metadata_t` it
 * references and the payload it references.
 *
 * The `dds_loaned_sample_t` is reference counted and the free function referenced through
 * the "ops" table is invoked once the reference count drops to 0.
 *
 * The PSMX Plugin is required to properly initialize all fields:
 *
 * - ops
 * - metadata, the contents of the memory it points to may be undefined
 * - sample_ptr, the contents of the memory it points to may be undefined
 *
 * The loan_origin and refc fields are set by Cyclone DDS following a succesful return.
 *
 * The `sample_ptr` in the Loaned Sample must point to a unique memory block of at least
 * `size_requested` bytes and be suitably aligned to directly store any of the types in
 * application-representation to which the various supported IDL types map.
 *
 * `size_requested` is equal to `sizeof_type` in the PSMX create topic call if:
 * - the loan is to be used for a raw sample (implies `IS_MEMCPY_SAFE` set in the type properties)
 * - `sizeof_type` was non-0 in the call to create the PSMX Topic
 *
 * @param[in] psmx_endpoint   The PSMX Writer to borrow from
 * @param[in] size_requested  The size of the loan requested in bytes
 * @returns A pointer to the Loaned Sample on success or a null pointer on failure
 */
typedef dds_loaned_sample_t * (*dds_psmx_endpoint_request_loan_fn) (struct dds_psmx_endpoint *psmx_endpoint, uint32_t size_requested);

/**
 * @brief Definition of function for writing data via a PSMX Writer
 * @ingroup psmx
 *
 * The PSMX Plugin is expected to publish the payload and the metadata of the Loaned
 * Sample via the PSMX Instance to its subscribers as described in the overview.  The
 * Loaned Sample:
 *
 * - originated in a successful request for a loan from this PSMX Instance;
 * - has the metadata and payload set correctly;
 * - has a metadata state that is not `UNINITIALIZED`.
 *
 * Cyclone DDS assumes:
 *
 * - that the `dds_loaned_sample_t` outlives the PSMX write operation
 * - that the `sample_ptr` and `metadata` are invalidated by the PSMX write operation
 *
 * and will consequently not touch the `sample_ptr` and `metadata` after the operation and
 * only invoke the Loaned Sample's `free` function.
 *
 * If the operations fails and returns an error, the DDS write (or write-like) operation
 * that caused it will report an error to the application.  It is unspecified whether or
 * not any other deliver paths may or may not have been provided with the data when this
 * happens.
 *
 * @param[in] psmx_endpoint    The PSMX Writer to publish the data on
 * @param[in] data             The Loaned Sample containing the data to publish
 * @param[in] keysz            The size of the serialized key blob, 0 for keyless topics
 * @param[in] key              The serialized key, a null pointer for keyless topics
 * @returns A DDS return code
 */
typedef dds_return_t (*dds_psmx_endpoint_write_with_key_fn) (struct dds_psmx_endpoint *psmx_endpoint, dds_loaned_sample_t *data, size_t keysz, const void *key);

/**
 * @brief Definition of function for writing data via a PSMX Writer
 * @ingroup psmx
 *
 * Equivalent to `dds_psmx_endpoint_write_with_key_fn` with `keysz` = 0 and `key` = NULL
 */
typedef dds_return_t (*dds_psmx_endpoint_write_fn) (struct dds_psmx_endpoint *psmx_endpoint, dds_loaned_sample_t *data);

/**
 * @brief Definition of function for taking data from a PSMX Reader
 * @ingroup psmx
 *
 * Return an unread sample available to the PSMX Reader and remove it from the set of
 * samples available to the PSMX Reader.  It should ensure, firstly, that a sample for
 * each key value for which data is available shall eventually be returned, and, secondly,
 * that samples for a given key value are returned in the order of publication.
 *
 * The Loaned Sample shall be fully initialized on return, with a metadata sample state
 * different from `UNINITIALIZED`.
 *
 * (Currently not called.)
 *
 * @param[in] psmx_endpoint    The PSMX Reader to take the data from
 * @returns a Loaned Sample for an unread sample, or a null pointer if none is available
 */
typedef dds_loaned_sample_t * (*dds_psmx_endpoint_take_fn) (struct dds_psmx_endpoint *psmx_endpoint);

/**
 * @brief Definition of function to request asynchronous delivery of new data by a PSMX Reader
 * @ingroup psmx
 *
 * The PSMX Plugin is requested to arrange for data arriving at the PSMX Reader to be
 * delivered to Cyclone DDS as if a background thread exists that contains a loop body:
 * ```
 *   dds_loaned_sample_t *ls = dds_psmx_endpoint_ops.take(psmx_endpoint)
 *   if (ls != NULL) {
 *     dds_reader_store_loaned_sample(reader, ls);
 *   }
 *   dds_loaned_sample_unref(data)
 * ```
 * (alternatively calling `dds_reader_store_loaned_sample_wr_metadata` for each sample)
 *
 * @param[in] psmx_endpoint    The PSMX Reader on which to enable asynchronous delivery
 * @param[in] reader           The DDS Reader to which the data is to be delivered
 * @returns a DDS return code
 */
typedef dds_return_t (*dds_psmx_endpoint_on_data_available_fn) (struct dds_psmx_endpoint *psmx_endpoint, dds_entity_t reader);

/**
 * @brief Table of pointers to functions that are used on a PSMX Endpoint
 * @ingroup psmx
 */
typedef struct dds_psmx_endpoint_ops {
  dds_psmx_endpoint_request_loan_fn request_loan;
  dds_psmx_endpoint_write_fn write; /**< write, not required if `write_key` is defined */
  dds_psmx_endpoint_take_fn take;
  dds_psmx_endpoint_on_data_available_fn on_data_available;
  dds_psmx_endpoint_write_with_key_fn write_with_key; /**< may be null for backwards compatibility if `write` is defined */
} dds_psmx_endpoint_ops_t;

/**
 * @brief Type representing a PSMX Instance in a DDS Domain Entity
 * @ingroup psmx
 *
 * Each PSMX Instance is represented in a DDS Domain Entity by a pointer to a `struct dds_psmx`.
 * The PSMX Plugin is responsible for allocating and constructing it on initialization in a constructor
 * as described in the overview.
 */
typedef struct dds_psmx {
  dds_psmx_ops_t ops;                 /**< operations on the PSMX Instance */
  const char *instance_name;          /**< name of this PSMX Instance */
  int32_t priority;                   /**< priority for this interface */
  const struct ddsi_locator *locator; /**< PSMX Locator for this PSMX Instance */
  dds_psmx_instance_id_t instance_id; /**< Numeric PSMX Instance ID for this PSMX Instance */
  void *psmx_topics;                  /**< Reserved, must be 0 */
} dds_psmx_t;

/**
 * @brief Type representing a PSMX Topic in a DDS Domain Entity
 * @ingroup psmx
 */
typedef struct dds_psmx_topic {
  dds_psmx_topic_ops_t ops;       /**< operations on the PSMX Topic */
  struct dds_psmx *psmx_instance; /**< PSMX Instance which created this PSMX Topic */
  const char *topic_name;         /**< Topic name */
  const char *type_name;          /**< Type name */
  dds_loan_data_type_t data_type; /**< Reserved, must be 0 */
  void *psmx_endpoints;           /**< Reserved, must be 0 */
  dds_data_type_properties_t data_type_props; /**< Properties of the data type associated with this topic */
} dds_psmx_topic_t;

/**
 * @brief Type representing a PSMX Endpoint in a DDS Domain Entity
 * @ingroup psmx
 */
typedef struct dds_psmx_endpoint {
  dds_psmx_endpoint_ops_t ops;            /**< operations on the PSMX Endpoint */
  struct dds_psmx_topic *psmx_topic;      /**< PSMX Topic for this endpoint */
  dds_psmx_endpoint_type_t endpoint_type; /**< Type of endpoint (READER or WRITER) */
} dds_psmx_endpoint_t;

/**
 * @brief nop
 * @ingroup psmx
 *
 * does nothing, exists for backwards compatibility only
 *
 * @param[in] psmx_topic       ignored
 * @param[in,out] list         ignored
 * @return `DDS_RETCODE_OK`
 */
DDS_DEPRECATED_EXPORT dds_return_t dds_add_psmx_topic_to_list (struct dds_psmx_topic *psmx_topic, void **list);

/**
 * @brief nop
 * @ingroup psmx
 *
 * does nothing, exists for backwards compatibility only
 *
 * @param[in] psmx_endpoint    ignored
 * @param[in,out] list         ignored
 * @return `DDS_RETCODE_OK`
 */
DDS_DEPRECATED_EXPORT dds_return_t dds_add_psmx_endpoint_to_list (struct dds_psmx_endpoint *psmx_endpoint, void **list);

/**
 * @brief initialization function for PSMX instance (interface version 0)
 * @ingroup psmx
 *
 * Shall be called by a constructor function for a PSMX Plugin implementing interface
 * version 0 as described in the overview.
 *
 * @param[in] psmx             The PSMX instance to initialize
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t dds_psmx_init_generic (struct dds_psmx *psmx);

/**
 * @brief cleanup function for a PSMX instance (interface version 0)
 * @ingroup psmx
 *
 * Shall be called by the destructor function of PSMX Plugin implementing interface
 * version 0 as described in the documentation of "dds_psmx_deinit_fn".
 *
 * @param[in] psmx             the PSMX instance to cleanup
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t dds_psmx_cleanup_generic (struct dds_psmx *psmx);

/**
 * @brief init function for topic (interface version 0)
 * @ingroup psmx
 *
 * Shall be called by de topic constructor function for a PSMX Plugin implementing
 * interface version 0.
 *
 * @param[in] psmx_topic       the topic to initialize
 * @param[in] ops vtable       for this psmx_topic
 * @param[in] psmx             the PSMX instance
 * @param[in] topic_name       the topic name
 * @param[in] type_name        the DDS type name for this topic
 * @param[in] data_type_props  the data type's properties
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t dds_psmx_topic_init_generic (struct dds_psmx_topic *psmx_topic, const dds_psmx_topic_ops_t *ops, const struct dds_psmx *psmx, const char *topic_name, const char *type_name, dds_data_type_properties_t data_type_props);

/**
 * @brief cleanup function for a topic (interface version 0)
 * @ingroup psmx
 *
 * Shall be called by de topic destructor function for a PSMX Plugin implementing
 * interface version 0 as described in the documentation of "dds_psmx_delete_topic_fn".
 *
 * @param[in] psmx_topic       the topic to de-initialize
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t dds_psmx_topic_cleanup_generic (struct dds_psmx_topic *psmx_topic);

/**
 * @brief Returns the string associated with the option_name in the PSMX config string.
 * @ingroup psmx
 *
 * The C string returned from this function should be freed by the user using `ddsrt_free`.
 *
 * The option "SERVICE_NAME" is treated as an alias for "INSTANCE_NAME".
 *
 * @param[in] conf             a double-\0 terminated configuration string in the
 *                             same format as that passed to the PSMX Plugin's constructor
 *                             function (see the overview).
 * @param[in] option_name      the option to look up
 * @return pointer to a newly allocated string on the heap if found, NULL if not found or out of memory.
 */
DDS_EXPORT char *dds_psmx_get_config_option_value (const char *conf, const char *option_name);

#if defined (__cplusplus)
}
#endif

#endif /* DDS_PSMX_H */
