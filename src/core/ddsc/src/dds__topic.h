// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__TOPIC_H
#define DDS__TOPIC_H

#include "dds__types.h"
#include "dds__entity.h"

#if defined (__cplusplus)
extern "C" {
#endif

DEFINE_ENTITY_LOCK_UNLOCK(dds_topic, DDS_KIND_TOPIC, topic)

/** @component topic */
dds_return_t dds_topic_pin_with_origin (dds_entity_t handle, bool from_user, struct dds_topic **tp) ddsrt_nonnull_all;

/** @component topic */
dds_return_t dds_topic_pin (dds_entity_t handle, struct dds_topic **tp) ddsrt_nonnull_all;

/** @component topic */
void dds_topic_unpin (struct dds_topic *tp) ddsrt_nonnull_all;

/** @component topic */
void dds_topic_defer_set_qos (struct dds_topic *tp) ddsrt_nonnull_all;

/** @component topic */
void dds_topic_allow_set_qos (struct dds_topic *tp) ddsrt_nonnull_all;

/** @component topic */
dds_entity_t dds_create_topic_impl (
    dds_entity_t participant,
    const char * name,
    bool allow_dcps,
    struct ddsi_sertype **sertype,
    const dds_qos_t *qos,
    const dds_listener_t *listener,
    bool is_builtin);

#if defined (__cplusplus)
}
#endif

#endif /* DDS__TOPIC_H */
