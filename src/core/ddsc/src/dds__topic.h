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
#ifndef _DDS_TOPIC_H_
#define _DDS_TOPIC_H_

#include "dds__types.h"
#include "dds__entity.h"

#if defined (__cplusplus)
extern "C" {
#endif

DEFINE_ENTITY_LOCK_UNLOCK(inline, dds_topic, DDS_KIND_TOPIC)

DDS_EXPORT void dds_topic_free (dds_domainid_t domainid, struct ddsi_sertopic * st) ddsrt_nonnull_all;

DDS_EXPORT dds_return_t dds_topic_pin (dds_entity_t handle, struct dds_topic **tp) ddsrt_nonnull_all;
DDS_EXPORT void dds_topic_unpin (struct dds_topic *tp) ddsrt_nonnull_all;
DDS_EXPORT void dds_topic_defer_set_qos (struct dds_topic *tp) ddsrt_nonnull_all;
DDS_EXPORT void dds_topic_allow_set_qos (struct dds_topic *tp) ddsrt_nonnull_all;

DDS_EXPORT dds_entity_t dds_create_topic_impl (dds_entity_t participant, struct ddsi_sertopic **sertopic, const dds_qos_t *qos, const dds_listener_t *listener, const ddsi_plist_t *sedp_plist);

#if defined (__cplusplus)
}
#endif
#endif
