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

void ddsi_proxy_writer_set_alive_may_unlock (struct ddsi_proxy_writer *pwr, bool notify);
int ddsi_proxy_writer_set_notalive (struct ddsi_proxy_writer *pwr, bool notify);
void ddsi_proxy_writer_get_alive_state (struct ddsi_proxy_writer *pwr, struct ddsi_alive_state *st);
struct ddsi_entity_common *ddsi_entity_common_from_proxy_endpoint_common (const struct ddsi_proxy_endpoint_common *c);
bool ddsi_is_proxy_endpoint (const struct ddsi_entity_common *e);


/* To create a new proxy writer or reader; the proxy participant is
   determined from the GUID and must exist. */
int ddsi_new_proxy_writer (struct ddsi_domaingv *gv, const struct ddsi_guid *ppguid, const struct ddsi_guid *guid, struct ddsi_addrset *as, const struct ddsi_plist *plist, struct nn_dqueue *dqueue, struct xeventq *evq, ddsrt_wctime_t timestamp, seqno_t seq);
int ddsi_new_proxy_reader (struct ddsi_domaingv *gv, const struct ddsi_guid *ppguid, const struct ddsi_guid *guid, struct ddsi_addrset *as, const struct ddsi_plist *plist, ddsrt_wctime_t timestamp, seqno_t seq
#ifdef DDS_HAS_SSM
, int favours_ssm
#endif
);

/* To delete a proxy writer or reader; these synchronously hide it
   from the outside world, preventing it from being matched to a
   reader or writer. Actual deletion is scheduled in the future, when
   no outstanding references may still exist (determined by checking
   thread progress, &c.). */
int ddsi_delete_proxy_writer (struct ddsi_domaingv *gv, const struct ddsi_guid *guid, ddsrt_wctime_t timestamp, int isimplicit);
int ddsi_delete_proxy_reader (struct ddsi_domaingv *gv, const struct ddsi_guid *guid, ddsrt_wctime_t timestamp, int isimplicit);

void ddsi_update_proxy_reader (struct ddsi_proxy_reader *prd, seqno_t seq, struct ddsi_addrset *as, const struct dds_qos *xqos, ddsrt_wctime_t timestamp);
void ddsi_update_proxy_writer (struct ddsi_proxy_writer *pwr, seqno_t seq, struct ddsi_addrset *as, const struct dds_qos *xqos, ddsrt_wctime_t timestamp);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__PROXY_ENDPOINT_H */
