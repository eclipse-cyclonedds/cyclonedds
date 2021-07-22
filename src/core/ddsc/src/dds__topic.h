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

DEFINE_ENTITY_LOCK_UNLOCK(dds_topic, DDS_KIND_TOPIC)

DDS_EXPORT void dds_topic_free (dds_domainid_t domainid, struct ddsi_sertype * st) ddsrt_nonnull_all;

DDS_EXPORT dds_return_t dds_topic_pin_with_origin (dds_entity_t handle, bool from_user, struct dds_topic **tp) ddsrt_nonnull_all;
DDS_EXPORT dds_return_t dds_topic_pin (dds_entity_t handle, struct dds_topic **tp) ddsrt_nonnull_all;
DDS_EXPORT void dds_topic_unpin (struct dds_topic *tp) ddsrt_nonnull_all;
DDS_EXPORT void dds_topic_defer_set_qos (struct dds_topic *tp) ddsrt_nonnull_all;
DDS_EXPORT void dds_topic_allow_set_qos (struct dds_topic *tp) ddsrt_nonnull_all;

#ifndef DDS_TOPIC_INTERN_FILTER_FN_DEFINED
#define DDS_TOPIC_INTERN_FILTER_FN_DEFINED
typedef bool (*dds_topic_intern_filter_fn) (const void * sample, void *ctx);
#endif

DDS_EXPORT void dds_topic_set_filter_with_ctx (dds_entity_t topic, dds_topic_intern_filter_fn filter, void *ctx);
DDS_EXPORT dds_topic_intern_filter_fn dds_topic_get_filter_with_ctx (dds_entity_t topic);
DDS_EXPORT dds_entity_t dds_create_topic_impl (
    dds_entity_t participant,
    const char * name,
    bool allow_dcps,
    struct ddsi_sertype **sertype,
    const dds_qos_t *qos,
    const dds_listener_t *listener,
    const ddsi_plist_t *sedp_plist,
    bool is_builtin);

#if defined (__cplusplus)
}
#endif
#endif
