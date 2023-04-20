// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__BUILTIN_H
#define DDS__BUILTIN_H

#include "dds/ddsi/ddsi_builtin_topic_if.h"
#include "dds__types.h"

#if defined (__cplusplus)
extern "C"
{
#endif

/**
 * @brief Get name and typename based from a builtin-topic pseudo-handle
 * @component builtin_topic
 *
 * @param pseudo_handle built-in topic pseudo handle
 * @param name          gets the name of the built-in topic
 * @param typename      gets the type name of the built-in topic
 * @return DDS_RETCODE_BAD_PARAMETER if pseudo_handle is invalid
 */
dds_return_t dds__get_builtin_topic_name_typename (dds_entity_t pseudo_handle, const char **name, const char **typename);

/**
 * @brief Returns the pseudo handle for the given typename
 * @component builtin_topic
 *
 * @return DDS_RETCODE_BAD_PARAMETER if typename isn't one of the built-in topics
 */
dds_entity_t dds__get_builtin_topic_pseudo_handle_from_typename (const char *typename);

/**
 * @brief Get actual topic in related participant related to topic 'id'.
 * @component builtin_topic
 *
 * @param e         entity to get the built-in topic from
 * @param topic     pseudo handle to get the actual built-in topic for
 * @returns the built-in topic entity
 */
dds_entity_t dds__get_builtin_topic (dds_entity_t e, dds_entity_t topic);

/**
 * @brief Constructs the QoS object for a built-in topic QoS
 * @component builtin_topic
 *
 * @returns the qos for the built-in topic
 */
dds_qos_t *dds__create_builtin_qos (void);

/**
 * @brief Subscriber singleton within related participant.
 * @component builtin_topic
 *
 * @param e entity to get the participant from, from which the built-in subscriber will be retrieved
 * @returns the subscriber entity
 */
dds_entity_t dds__get_builtin_subscriber (dds_entity_t e);

/**
 * @brief Checks whether the reader QoS is valid for use with built-in topic TOPIC
 * @component builtin_topic
 *
 * @param dom the domain
 * @param topic pseudo handle of the built-in topic
 * @param qos qos to check validity for
 * @returns true iff the qos is valid
 */
bool dds__validate_builtin_reader_qos (const dds_domain *dom, dds_entity_t topic, const dds_qos_t *qos);

/** @component builtin_topic */
void dds__builtin_init (struct dds_domain *dom);

/** @component builtin_topic */
void dds__builtin_fini (struct dds_domain *dom);

struct ddsi_entity_common;
struct ddsi_proxy_topic;

/** @component builtin_topic */
struct ddsi_serdata *dds__builtin_make_sample_endpoint (const struct ddsi_entity_common *e, ddsrt_wctime_t timestamp, bool alive);

/** @component builtin_topic */
struct ddsi_serdata *dds__builtin_make_sample_topic (const struct ddsi_entity_common *e, ddsrt_wctime_t timestamp, bool alive);

/** @component builtin_topic */
struct ddsi_serdata *dds__builtin_make_sample_proxy_topic (const struct ddsi_proxy_topic *proxytp, ddsrt_wctime_t timestamp, bool alive);

#if defined (__cplusplus)
}
#endif

#endif /* DDS__BUILTIN_H */

