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

#if defined (__cplusplus)
extern "C"
{
#endif

/* Get actual topic in related participant related to topic 'id'. */
dds_entity_t dds__get_builtin_topic ( dds_entity_t e, dds_entity_t topic);

/* Subscriber singleton within related participant. */
dds_entity_t dds__get_builtin_subscriber(dds_entity_t e);

/* Checks whether the reader QoS is valid for use with built-in topic TOPIC */
bool dds__validate_builtin_reader_qos(dds_entity_t topic, const dds_qos_t *qos);

struct entity_common;
struct nn_guid;
struct ddsi_tkmap_instance;

void dds__builtin_init (void);
void dds__builtin_fini (void);
bool dds__builtin_is_visible (nn_entityid_t entityid, bool onlylocal, nn_vendorid_t vendorid);
struct ddsi_tkmap_instance *dds__builtin_get_tkmap_entry (const struct nn_guid *guid);
struct ddsi_serdata *dds__builtin_make_sample (const struct entity_common *e, nn_wctime_t timestamp, bool alive);
void dds__builtin_write (const struct entity_common *e, nn_wctime_t timestamp, bool alive);

#if defined (__cplusplus)
}
#endif

#endif /* _DDS_BUILTIN_H_ */

