/*
 * Copyright(c) 2006 to 2022 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSI__ENTITY_H
#define DDSI__ENTITY_H

#include "dds/export.h"
#include "dds/features.h"

#include "dds/ddsrt/atomics.h"
#include "dds/ddsi/ddsi_protocol.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_guid.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_alive_state {
  bool alive;
  uint32_t vclock;
};

bool ddsi_is_null_guid (const ddsi_guid_t *guid);
int ddsi_is_builtin_entityid (ddsi_entityid_t id, ddsi_vendorid_t vendorid);
bool ddsi_update_qos_locked (struct ddsi_entity_common *e, dds_qos_t *ent_qos, const dds_qos_t *xqos, ddsrt_wctime_t timestamp);
int ddsi_set_topic_type_name (dds_qos_t *xqos, const char * topic_name, const char * type_name);
int ddsi_compare_entityid (const void *a, const void *b);

int ddsi_compare_guid (const void *va, const void *vb);

void ddsi_entity_common_init (struct ddsi_entity_common *e, struct ddsi_domaingv *gv, const struct ddsi_guid *guid, enum ddsi_entity_kind kind, ddsrt_wctime_t tcreate, ddsi_vendorid_t vendorid, bool onlylocal);
void ddsi_entity_common_fini (struct ddsi_entity_common *e);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__ENTITY_H */
