// Copyright(c) 2024 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#ifndef DDS_QOS_PROVIDER_H
#define DDS_QOS_PROVIDER_H

#include "dds/export.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsc/dds_public_qosdefs.h"

#if defined (__cplusplus)
extern "C" {
#endif

#ifdef DDS_HAS_QOS_PROVIDER

/**
 * @defgroup qos_provider (QosProvider)
 * @ingroup dds
 * The Qos Provider API.
 */

/**
 * @brief All kind of entities for which qos can be stored in Profile.
 * @ingroup qos_provider
 * @component qos_provider_api
 */
enum dds_qos_kind
{
  DDS_PARTICIPANT_QOS,
  DDS_PUBLISHER_QOS,
  DDS_SUBSCRIBER_QOS,
  DDS_TOPIC_QOS,
  DDS_READER_QOS,
  DDS_WRITER_QOS
};
typedef enum dds_qos_kind dds_qos_kind_t;

/**
 * @brief Sample structure of the Qos Provider.
 * @ingroup qos_provider
 * @component qos_provider_api
 */
struct dds_qos_provider;
typedef struct dds_qos_provider dds_qos_provider_t;

/**
 * @brief Initialize Qos Provider.
 * @ingroup qos_provider
 * @component qos_provider_api
 *
 * Create dds_qos_provider with provided system definition file path.
 *
 * @param[in] path - String that contains system definition inself or path to system defenition file.
 * @param[in,out] provider - Pointer to the Qos Provider structure.
 *
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t
dds_create_qos_provider (const char *path, dds_qos_provider_t **provider);

/**
 * @brief Initialize Qos Provider with certain scope.
 * @ingroup qos_provider
 * @component qos_provider_api
 *
 * Create dds_qos_provider with provided system definition file path and scope.
 *
 * @param[in] path - String that contains system definition inself or path to system defenition file.
 * @param[in,out] provider - Pointer to the Qos Provider structure.
 * @param[in] key - String that contains pattern of interested qos from `path` in format '<library name>::<profile name>::<entity name>'.
 *
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t
dds_create_qos_provider_scope (const char *path, dds_qos_provider_t **provider,
                               const char *key);

/**
 * @brief Get Qos from Qos Provider.
 * @ingroup qos_provider
 * @component qos_provider_api
 *
 * Provide access to dds_qos_t from dds_qos_provider by full key and type of qos entity.
 *
 * @param[in] provider - Pointer to the Qos Provider structure.
 * @param[in] type - Type of entity which Qos to get.
 * @param[in] key - Full qualify name of Qos to get in format '<library name>::<profile name>::<entity name>'.
 * @param[in,out] qos - Pointer to the Qos structure.
 *
 * @return a DDS return code
 */
DDS_EXPORT dds_return_t
dds_qos_provider_get_qos (const dds_qos_provider_t *provider, dds_qos_kind_t type,
                          const char *key, const dds_qos_t **qos);

/**
 * @brief Finalize Qos Provider.
 * @ingroup qos_provider
 * @component qos_provider_api
 *
 * Release resources allocated by dds_qos_provider.
 *
 * @param[in] provider - Pointer to the Qos Provider structure.
 */
DDS_EXPORT void
dds_delete_qos_provider (dds_qos_provider_t *provider);

#endif /* DDS_HAS_QOS_PROVIDER */

#if defined (__cplusplus)
}
#endif

#endif // DDS_QOS_PROVIDER_H
