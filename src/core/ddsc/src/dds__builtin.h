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

#include "ddsi/q_time.h"
#include "ddsi/ddsi_serdata_builtin.h"


#if defined (__cplusplus)
extern "C"
{
#endif


/* Get actual topic in related participant related to topic 'id'. */
_Must_inspect_result_ dds_entity_t
dds__get_builtin_topic(
    _In_ dds_entity_t e,
    _In_ dds_entity_t topic);

/* Global publisher singleton (publishes only locally). */
_Must_inspect_result_ dds_entity_t
dds__get_builtin_publisher(
    void);

/* Subscriber singleton within related participant. */
_Must_inspect_result_ dds_entity_t
dds__get_builtin_subscriber(
    _In_ dds_entity_t e);

/* Checks whether the reader QoS is valid for use with built-in topic TOPIC */
bool dds__validate_builtin_reader_qos(dds_entity_t topic, const dds_qos_t *qos);

/* Initialization and finalize functions. */
void
dds__builtin_init(
        void);

void
dds__builtin_fini(
        void);

void
dds__builtin_write(
    _In_ enum ddsi_sertopic_builtin_type type,
    _In_ const nn_guid_t *guid,
    _In_ nn_wctime_t timestamp,
    _In_ bool alive);

#if defined (__cplusplus)
}
#endif

#endif /* _DDS_BUILTIN_H_ */

