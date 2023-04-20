// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__TOPIC_H
#define DDSI__TOPIC_H

#include "dds/export.h"
#include "dds/features.h"

#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_topic.h"

#if defined (__cplusplus)
extern "C" {
#endif

/** @component ddsi_topic */
int ddsi_is_topic_entityid (ddsi_entityid_t id);

#ifdef DDS_HAS_TOPIC_DISCOVERY

/** @component ddsi_topic */
int ddsi_topic_definition_equal (const struct ddsi_topic_definition *tpd_a, const struct ddsi_topic_definition *tpd_b);

/** @component ddsi_topic */
uint32_t ddsi_topic_definition_hash (const struct ddsi_topic_definition *tpd);

/** @component ddsi_topic */
dds_return_t ddsi_new_proxy_topic (struct ddsi_proxy_participant *proxypp, ddsi_seqno_t seq, const ddsi_guid_t *guid, const ddsi_typeid_t *type_id_minimal, const ddsi_typeid_t *type_id, struct dds_qos *qos, ddsrt_wctime_t timestamp);

/** @component ddsi_topic */
struct ddsi_proxy_topic *ddsi_lookup_proxy_topic (struct ddsi_proxy_participant *proxypp, const ddsi_guid_t *guid);

/** @component ddsi_topic */
void ddsi_update_proxy_topic (struct ddsi_proxy_participant *proxypp, struct ddsi_proxy_topic *proxytp, ddsi_seqno_t seq, struct dds_qos *xqos, ddsrt_wctime_t timestamp);

/** @component ddsi_topic */
int ddsi_delete_proxy_topic_locked (struct ddsi_proxy_participant *proxypp, struct ddsi_proxy_topic *proxytp, ddsrt_wctime_t timestamp);

#endif /* DDS_HAS_TOPIC_DISCOVERY */

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__TOPIC_H */
