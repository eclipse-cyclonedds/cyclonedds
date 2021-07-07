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
#ifndef _DDS_BUILTIN_H_
#define _DDS_BUILTIN_H_

#include "dds/ddsi/ddsi_builtin_topic_if.h"
#include "dds__types.h"

#if defined (__cplusplus)
extern "C"
{
#endif

/* Get name and typename based from a builtin-topic pseudo-handle; returns an DDS_RETCODE_BAD_PARAMETER if pseudo_handle is invalid */
dds_return_t dds__get_builtin_topic_name_typename (dds_entity_t pseudo_handle, const char **name, const char **typename);

/* Returns the pseudo handle for the given typename, returns DDS_RETCODE_BAD_PARAMETER if typename isn't one of the built-in topics */
dds_entity_t dds__get_builtin_topic_pseudo_handle_from_typename (const char *typename);

/* Get actual topic in related participant related to topic 'id'. */
dds_entity_t dds__get_builtin_topic (dds_entity_t e, dds_entity_t topic);

/* Constructs the QoS object for a built-in topic QoS */
dds_qos_t *dds__create_builtin_qos (void);

/* Subscriber singleton within related participant. */
dds_entity_t dds__get_builtin_subscriber (dds_entity_t e);

/* Checks whether the reader QoS is valid for use with built-in topic TOPIC */
bool dds__validate_builtin_reader_qos (const dds_domain *dom, dds_entity_t topic, const dds_qos_t *qos);

void dds__builtin_init (struct dds_domain *dom);
void dds__builtin_fini (struct dds_domain *dom);

struct entity_common;
struct proxy_topic;
struct ddsi_serdata *dds__builtin_make_sample_endpoint (const struct entity_common *e, ddsrt_wctime_t timestamp, bool alive);
struct ddsi_serdata *dds__builtin_make_sample_topic (const struct entity_common *e, ddsrt_wctime_t timestamp, bool alive);
struct ddsi_serdata *dds__builtin_make_sample_proxy_topic (const struct proxy_topic *proxytp, ddsrt_wctime_t timestamp, bool alive);

#if defined (__cplusplus)
}
#endif

#endif /* _DDS_BUILTIN_H_ */

