// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__PROXY_ENDPOINT_H
#define DDSI__PROXY_ENDPOINT_H

#include "dds/export.h"
#include "dds/features.h"

#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_lease.h"
#include "dds/ddsi/ddsi_proxy_endpoint.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_alive_state;
struct dds_qos;
struct ddsi_addrset;

extern const ddsrt_avl_treedef_t ddsi_pwr_readers_treedef;
extern const ddsrt_avl_treedef_t ddsi_prd_writers_treedef;


/** @component ddsi_proxy_endpoint */
void ddsi_proxy_writer_set_alive_may_unlock (struct ddsi_proxy_writer *pwr, bool notify);

/** @component ddsi_proxy_endpoint */
int ddsi_proxy_writer_set_notalive (struct ddsi_proxy_writer *pwr, bool notify);

/** @component ddsi_proxy_endpoint */
void ddsi_proxy_writer_get_alive_state (struct ddsi_proxy_writer *pwr, struct ddsi_alive_state *st);

/** @component ddsi_proxy_endpoint */
struct ddsi_entity_common *ddsi_entity_common_from_proxy_endpoint_common (const struct ddsi_proxy_endpoint_common *c);

/** @component ddsi_proxy_endpoint */
bool ddsi_is_proxy_endpoint (const struct ddsi_entity_common *e);

/** @component ddsi_proxy_endpoint */
void ddsi_send_entityid_to_pwr (struct ddsi_proxy_writer *pwr, const ddsi_guid_t *guid);

/** @component ddsi_proxy_endpoint */
void ddsi_send_entityid_to_prd (struct ddsi_proxy_reader *prd, const ddsi_guid_t *guid);

/**
 * @brief Delete a proxy writer
 * @component ddsi_proxy_endpoint
 *
 * These synchronously hide it from the outside world, preventing it from being matched to a
 * reader. Actual deletion is scheduled in the future, when no outstanding references may
 * still exist (determined by checking thread progress, &c.)
 *
 * @param gv            domain globals
 * @param guid          guid of the proxy writer to delete
 * @param timestamp     deletion timestamp
 * @param lease_expired    if false, evidence of deletion; if true, circumstantial evidence only (typically lease expiration)
 * @return int
 */
int ddsi_delete_proxy_writer (struct ddsi_domaingv *gv, const struct ddsi_guid *guid, ddsrt_wctime_t timestamp, bool lease_expired);

/**
 * @brief Delete a proxy reader
 * @component ddsi_proxy_endpoint
 *
 * These synchronously hide it from the outside world, preventing it from being matched to a
 * writer. Actual deletion is scheduled in the future, when no outstanding references may still
 * exist (determined by checking thread progress, &c.)
 *
 * @param gv            domain globals
 * @param guid          guid of the proxy reader to delete
 * @param timestamp     deletion timestamp
 * @param lease_expired    if false, evidence of deletion; if true, circumstantial evidence only (typically lease expiration)
 * @return int
 */
int ddsi_delete_proxy_reader (struct ddsi_domaingv *gv, const struct ddsi_guid *guid, ddsrt_wctime_t timestamp, bool lease_expired);

/** @component ddsi_proxy_endpoint */
void ddsi_update_proxy_reader (struct ddsi_proxy_reader *prd, ddsi_seqno_t seq, struct ddsi_addrset *as, const struct dds_qos *xqos, ddsrt_wctime_t timestamp);

/** @component ddsi_proxy_endpoint */
void ddsi_update_proxy_writer (struct ddsi_proxy_writer *pwr, ddsi_seqno_t seq, struct ddsi_addrset *as, const struct dds_qos *xqos, ddsrt_wctime_t timestamp);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__PROXY_ENDPOINT_H */
