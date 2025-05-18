// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__PSMX_H
#define DDS__PSMX_H

#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsc/dds_psmx.h"
#include "dds/ddsc/dds_public_impl.h"

struct ddsi_domaingv;
struct dds_domain;
struct dds_ktopic;
struct dds_endpoint;

struct dds_psmx_int;
struct dds_psmx_topic_int;
struct dds_psmx_endpoint_int;
struct dds_psmx_set;
struct dds_psmx_topics_set;

/**
 * @brief Table of pointers to functions operating on a PSMX Instance (for binary backwards compatibility)
 * @ingroup psmx
 */
typedef struct dds_psmx_ops_v0 {
  dds_psmx_type_qos_supported_fn type_qos_supported;
  dds_psmx_create_topic_fn create_topic;
  dds_psmx_delete_topic_fn delete_topic;
  dds_psmx_deinit_fn deinit;
  dds_psmx_get_node_identifier_fn get_node_id;
  dds_psmx_supported_features_fn supported_features;
} dds_psmx_ops_v0_t;

/**
 * @brief Table of pointers to functions that are used on a PSMX Endpoint (for binary backwards compatibility)
 * @ingroup psmx
 */
typedef struct dds_psmx_endpoint_ops_v0 {
  dds_psmx_endpoint_request_loan_fn request_loan;
  dds_psmx_endpoint_write_fn write;
  dds_psmx_endpoint_take_fn take;
  dds_psmx_endpoint_on_data_available_fn on_data_available;
} dds_psmx_endpoint_ops_v0_t;

/**
 * @brief Type representing a PSMX Instance in a DDS Domain Entity (for binary backwards compatibility)
 * @ingroup psmx
 *
 * Each PSMX Instance is represented in a DDS Domain Entity by a pointer to a `struct dds_psmx`.
 * The PSMX Plugin is responsible for allocating and constructing it on initialization in a constructor
 * as described in the overview.
 */
typedef struct dds_psmx_v0 {
  dds_psmx_ops_v0_t ops;              /**< operations on the PSMX Instance */
  const char *instance_name;          /**< name of this PSMX Instance */
  int32_t priority;                   /**< priority for this interface */
  const struct ddsi_locator *locator; /**< PSMX Locator for this PSMX Instance */
  dds_psmx_instance_id_t instance_id; /**< Numeric PSMX Instance ID for this PSMX Instance */
  void *psmx_topics;                  /**< Reserved, must be 0 */
} dds_psmx_v0_t;

/**
 * @brief Type representing a PSMX Endpoint in a DDS Domain Entity (for binary backwards compatibility)
 * @ingroup psmx
 */
typedef struct dds_psmx_endpoint_v0 {
  dds_psmx_endpoint_ops_v0_t ops;         /**< operations on the PSMX Endpoint */
  struct dds_psmx_topic *psmx_topic;      /**< PSMX Topic for this endpoint */
  dds_psmx_endpoint_type_t endpoint_type; /**< Type of endpoint (READER or WRITER) */
} dds_psmx_endpoint_v0_t;

/**
 * @brief Definition of the function for destructing and freeing a PSMX Instance (interface version 1)
 * @ingroup psmx
 *
 * Function called on shutdown of a DDS Domain Entity, before unloading the PSMX Plugin
 * and after all other operations have completed and all objects created in the PSMX
 * Instance have been destructed using the various "delete" functions.
 *
 * @param[in] psmx    The PSMX Instance to de-initialize
 */
typedef void (*dds_psmx_delete_int_fn) (struct dds_psmx_int *psmx);

/**
 * @brief Definition of the function for creating a PSMX Topic (interface version 1)
 * @ingroup psmx
 *
 * Definition for a function that is called to construct a new PSMX Topic in a PSMX Instance
 * representing a new topic in the DDS Domain Entity.
 *
 * The PSMX Plugin is expected to represent a PSMX Topic using an extended version of the
 * `dds_psmx_topic` structure.  It is required to initialize the generic fields using
 * `dds_psmx_topic_init`.
 *
 * If `type_definition` is not a null pointer, it points into the Cyclone type library. A
 * (default, C) serializer can be constructed using `ddsi_topic_descriptor_from_type`,
 * an XTypes TypeObject using `ddsi_type_get_typeobj`.
 *
 * If the function returns failure, creation of the DDS Entity fails.
 *
 * @param[in] psmx    The PSMX instance.
 * @param[in] ktp       The ktopic in the DDS Domain
 * @param[in] sertype        The sertype this topic
 * @param[in] type  Optional pointer to type definition in type library.
 * @returns A new PSMX Topic structure, or NULL on error
 */
typedef struct dds_psmx_topic_int * (*dds_psmx_create_topic_with_type_int_fn) (
    struct dds_psmx_int *psmx,
    struct dds_ktopic *ktp,
    struct ddsi_sertype *sertype,
    struct ddsi_type *type);

/**
 * @brief Definition of the function for destructing and freeing a PSMX Topic
 * @ingroup psmx
 *
 * Definition for a function that is called on deleting the topic in the DDS Domain.
 * Called exactly once for each successful invocation of `dds_psmx_create_topic_with_type`, all
 * PSMX Endpoints related to this PSMX Topic will have been destructed prior to calling
 * this function.
 *
 * If it was created using `dds_psmx_create_topic` it is allowed to call
 * `dds_psmx_topic_cleanup_generic` instead of `dds_psmx_topic_fini`
 * for backwards compatibility.
 *
 * @param[in] psmx_topic       The PSMX Topic to destruct
 */
typedef void (*dds_psmx_delete_topic_int_fn) (struct dds_psmx_topic_int *psmx_topic);

/**
 * @brief Table of pointers to functions operating on a PSMX Instance
 * @ingroup psmx
 */
typedef struct dds_psmx_int_ops {
  dds_psmx_type_qos_supported_fn type_qos_supported;
  dds_psmx_delete_topic_int_fn delete_topic;
  dds_psmx_get_node_identifier_fn get_node_id;
  dds_psmx_supported_features_fn supported_features;
  dds_psmx_create_topic_with_type_int_fn create_topic_with_type; /**< undefined for interface version 0, non-null for version 1 */
  dds_psmx_delete_int_fn delete_psmx; /**< undefined for interface version 0, non-null for version 1 */
} dds_psmx_int_ops_t;

/**
 * @brief Type representing a PSMX Instance in a DDS Domain Entity (for binary backwards compatibility)
 * @ingroup psmx
 *
 * Each PSMX Instance is represented in a DDS Domain Entity by a pointer to a `struct dds_psmx`.
 * The PSMX Plugin is responsible for allocating and constructing it on initialization in a constructor
 * as described in the overview.
 */
typedef struct dds_psmx_int {
  dds_psmx_int_ops_t ops;             /**< operations on the PSMX Instance */
  struct dds_psmx *ext;               /**< object owned by plugin, we do alias things in there */
  const char *instance_name;          /**< name of this PSMX Instance */
  int32_t priority;                   /**< priority for this interface */
  struct ddsi_locator locator;        /**< PSMX Locator for this PSMX Instance */
  dds_psmx_instance_id_t instance_id; /**< Numeric PSMX Instance ID for this PSMX Instance */
} dds_psmx_int_t;

/**
 * @brief Definition of the function for constructing a PSMX Endpoint for a PSMX Topic
 * @ingroup psmx
 *
 * @param[in] psmx_topic       The PSMX Topic to create the PSMX Endpoint for
 * @param[in] qos              QoS of the corresponding DDS endpoint
 * @param[in] endpoint_type    The type of endpoint to create, either READER or WRITER
 * @returns A new PSMX Endpoint or a null pointer on error
 */
typedef struct dds_psmx_endpoint_int * (*dds_psmx_create_endpoint_int_fn) (const struct dds_psmx_topic_int *psmx_topic, const struct dds_qos *qos, dds_psmx_endpoint_type_t endpoint_type);

/**
 * @brief Table of pointers to functions operating on an internal PSMX Topic
 * @ingroup psmx
 */
typedef struct dds_psmx_topic_int_ops {
  dds_psmx_create_endpoint_int_fn create_endpoint;
  dds_psmx_delete_endpoint_fn delete_endpoint;
} dds_psmx_topic_int_ops_t;

/**
 * @brief Type representing a PSMX Topic in a DDS Domain Entity
 * @ingroup psmx
 */
typedef struct dds_psmx_topic_int {
  dds_psmx_topic_int_ops_t ops;       /**< operations on the PSMX Topic */
  struct dds_psmx_topic *ext;         /**< object owned by plugin, we do alias things in there */
  struct dds_psmx_int *psmx_instance; /**< PSMX Instance which created this PSMX Topic */
  dds_data_type_properties_t data_type_props; /**< Properties of the data type associated with this topic */
} dds_psmx_topic_int_t;

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
typedef dds_loaned_sample_t * (*dds_psmx_endpoint_request_loan_int_fn) (const struct dds_psmx_endpoint_int *psmx_endpoint, uint32_t size_requested);

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
typedef dds_return_t (*dds_psmx_endpoint_write_with_key_int_fn) (const struct dds_psmx_endpoint_int *psmx_endpoint, dds_loaned_sample_t *data, size_t keysz, const void *key);

/**
 * @brief Table of pointers to functions that are used on a PSMX Endpoint
 * @ingroup psmx
 */
typedef struct dds_psmx_endpoint_int_ops {
  dds_psmx_endpoint_request_loan_int_fn request_loan;
  dds_psmx_endpoint_take_fn take;
  dds_psmx_endpoint_on_data_available_fn on_data_available;
  dds_psmx_endpoint_write_with_key_int_fn write_with_key;
} dds_psmx_endpoint_int_ops_t;

/**
 * @brief Type representing a PSMX Endpoint in a DDS Domain Entity (for binary backwards compatibility)
 * @ingroup psmx
 */
typedef struct dds_psmx_endpoint_int {
  dds_psmx_endpoint_int_ops_t ops;        /**< operations on the PSMX Endpoint */
  struct dds_psmx_endpoint *ext;          /**< object owned by plugin, we do alias things in there */
  const struct dds_psmx_topic_int *psmx_topic; /**< PSMX Topic for this endpoint */
  dds_psmx_endpoint_type_t endpoint_type; /**< Type of endpoint (READER or WRITER) */
  bool wants_key;                         /**< Whether cares about key blob or not */
} dds_psmx_endpoint_int_t;

/**
 * @brief Definition for the function for creating a new PSMX Instance
 *
 * This function is to be exported from the PSMX Plugin library.
 *
 * The function is required to call `dds_psmx_init` to initialize the common fields prior to a successful return.
 *
 * @param[out] psmx_instance  New PSMX Instance, undefined on error
 * @param[in] identifier  Numeric PSMX Instance ID
 * @param[in] config  PSMX configuration string
 *
 * @returns a DDS return code
 */
typedef dds_return_t (*dds_psmx_create_fn) (
  struct dds_psmx **psmx_instance,
  dds_psmx_instance_id_t identifier,
  const char *config);

char *dds_pubsub_message_exchange_configstr (const char *config, const char *config_name);

dds_return_t dds_pubsub_message_exchange_init (const struct ddsi_domaingv *gv, struct dds_domain *domain);

void dds_pubsub_message_exchange_fini (struct dds_domain *domain);

dds_return_t dds_endpoint_add_psmx_endpoint (struct dds_endpoint *ep, const dds_qos_t *qos, struct dds_psmx_topics_set *psmx_topics, dds_psmx_endpoint_type_t endpoint_type);
void dds_endpoint_remove_psmx_endpoints (struct dds_endpoint *ep);

struct ddsi_psmx_locators_set *dds_get_psmx_locators_set (const dds_qos_t *qos, const struct dds_psmx_set *psmx_instances);
void dds_psmx_locators_set_free (struct ddsi_psmx_locators_set *psmx_locators_set);

/**
 * @brief Request a loan
 *
 * @param[in] psmx_endpoint  the endpoint to request a loan for
 * @param[in] sz    size of the loan
 * @return a loaned sample
 */
dds_loaned_sample_t * dds_psmx_endpoint_request_loan_v0_bincompat_wrapper (const struct dds_psmx_endpoint_int *psmx_endpoint, uint32_t sz)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

/**
 * @brief Request a loan
 *
 * @param[in] psmx_endpoint  the endpoint to request a loan for
 * @param[in] sz    size of the loan
 * @return a loaned sample
 */
dds_loaned_sample_t * dds_psmx_endpoint_request_loan_v0_wrapper (const struct dds_psmx_endpoint_int *psmx_endpoint, uint32_t sz)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

/**
 * @brief Request a loan
 *
 * @param[in] psmx_endpoint  the endpoint to request a loan for
 * @param[in] sz    size of the loan
 * @return a loaned sample
 */
dds_loaned_sample_t * dds_psmx_endpoint_request_loan_v1_wrapper (const struct dds_psmx_endpoint_int *psmx_endpoint, uint32_t sz)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

/**
 * @brief Sets the writer info parameters in a loaned sample
 *
 * @param loan The loaned sample to set the parameters for
 * @param wr_guid Writer GUID
 * @param timestamp Timestamp
 * @param statusinfo Statusinfo value
 */
void dds_psmx_set_loan_writeinfo (struct dds_loaned_sample *loan, const ddsi_guid_t *wr_guid, dds_time_t timestamp, uint32_t statusinfo)
  ddsrt_nonnull_all;

void dds_psmx_topic_init (struct dds_psmx_topic *psmx_topic, const struct dds_psmx *psmx, const char *topic_name, const char *type_name, dds_data_type_properties_t data_type_props);

#endif // DDS__PSMX_H
