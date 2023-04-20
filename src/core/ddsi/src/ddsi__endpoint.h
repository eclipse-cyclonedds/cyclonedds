// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__ENDPOINT_H
#define DDSI__ENDPOINT_H

#include "dds/export.h"
#include "dds/features.h"

#include "dds/ddsrt/fibheap.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "ddsi__whc.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_participant;
struct ddsi_type_pair;
struct ddsi_entity_common;
struct ddsi_endpoint_common;
struct ddsi_alive_state;
struct dds_qos;

struct ddsi_ldur_fhnode {
  ddsrt_fibheap_node_t heapnode;
  dds_duration_t ldur;
};

/** @component ddsi_endpoint */
inline ddsi_seqno_t ddsi_writer_read_seq_xmit (const struct ddsi_writer *wr)
{
  return ddsrt_atomic_ld64 (&wr->seq_xmit);
}

/** @component ddsi_endpoint */
inline void ddsi_writer_update_seq_xmit (struct ddsi_writer *wr, ddsi_seqno_t nv)
{
  uint64_t ov;
  do {
    ov = ddsrt_atomic_ld64 (&wr->seq_xmit);
    if (nv <= ov) break;
  } while (!ddsrt_atomic_cas64 (&wr->seq_xmit, ov, nv));
}

/** @component ddsi_endpoint */
bool ddsi_is_local_orphan_endpoint (const struct ddsi_entity_common *e);

/** @component ddsi_endpoint */
int ddsi_is_keyed_endpoint_entityid (ddsi_entityid_t id);

/** @component ddsi_endpoint */
int ddsi_is_builtin_volatile_endpoint (ddsi_entityid_t id);


// writer

/** @component ddsi_endpoint */
dds_return_t ddsi_new_writer_guid (struct ddsi_writer **wr_out, const struct ddsi_guid *guid, const struct ddsi_guid *group_guid, struct ddsi_participant *pp, const char *topic_name, const struct ddsi_sertype *type, const struct dds_qos *xqos, struct ddsi_whc *whc, ddsi_status_cb_t status_cb, void *status_entity);

/** @component ddsi_endpoint */
int ddsi_is_writer_entityid (ddsi_entityid_t id);

/** @component ddsi_endpoint */
void ddsi_deliver_historical_data (const struct ddsi_writer *wr, const struct ddsi_reader *rd);

/** @component ddsi_endpoint */
unsigned ddsi_remove_acked_messages (struct ddsi_writer *wr, struct ddsi_whc_state *whcst, struct ddsi_whc_node **deferred_free_list);

/** @component ddsi_endpoint */
ddsi_seqno_t ddsi_writer_max_drop_seq (const struct ddsi_writer *wr);

/** @component ddsi_endpoint */
int ddsi_writer_must_have_hb_scheduled (const struct ddsi_writer *wr, const struct ddsi_whc_state *whcst);

/** @component ddsi_endpoint */
void ddsi_writer_set_retransmitting (struct ddsi_writer *wr);

/** @component ddsi_endpoint */
void ddsi_writer_clear_retransmitting (struct ddsi_writer *wr);

/** @component ddsi_endpoint */
dds_return_t ddsi_delete_writer_nolinger (struct ddsi_domaingv *gv, const struct ddsi_guid *guid);

/** @component ddsi_endpoint */
void ddsi_writer_get_alive_state (struct ddsi_writer *wr, struct ddsi_alive_state *st);

/** @component ddsi_endpoint */
void ddsi_rebuild_writer_addrset (struct ddsi_writer *wr);

/** @component ddsi_endpoint */
void ddsi_writer_set_alive_may_unlock (struct ddsi_writer *wr, bool notify);

/** @component ddsi_endpoint */
int ddsi_writer_set_notalive (struct ddsi_writer *wr, bool notify);

/** @component ddsi_endpoint */
dds_return_t ddsi_new_reader_guid (struct ddsi_reader **rd_out, const struct ddsi_guid *guid, const struct ddsi_guid *group_guid, struct ddsi_participant *pp, const char *topic_name, const struct ddsi_sertype *type, const struct dds_qos *xqos, struct ddsi_rhc *rhc, ddsi_status_cb_t status_cb, void * status_entity);


// reader

/** @component ddsi_endpoint */
int ddsi_is_reader_entityid (ddsi_entityid_t id);

/** @component ddsi_endpoint */
void ddsi_reader_update_notify_wr_alive_state (struct ddsi_reader *rd, const struct ddsi_writer *wr, const struct ddsi_alive_state *alive_state);

/** @component ddsi_endpoint */
void ddsi_reader_update_notify_pwr_alive_state (struct ddsi_reader *rd, const struct ddsi_proxy_writer *pwr, const struct ddsi_alive_state *alive_state);

/** @component ddsi_endpoint */
void ddsi_reader_update_notify_pwr_alive_state_guid (const struct ddsi_guid *rd_guid, const struct ddsi_proxy_writer *pwr, const struct ddsi_alive_state *alive_state);

/** @component ddsi_endpoint */
void ddsi_update_reader_init_acknack_count (const ddsrt_log_cfg_t *logcfg, const struct ddsi_entity_index *entidx, const struct ddsi_guid *rd_guid, ddsi_count_t count);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__ENDPOINT_H */
