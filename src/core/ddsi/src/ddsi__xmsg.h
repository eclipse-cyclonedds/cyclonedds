/*
 * Copyright(c) 2006 to 2021 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSI__XMSG_H
#define DDSI__XMSG_H

#include <stddef.h>

#include "dds/ddsrt/bswap.h"
#include "dds/ddsi/ddsi_protocol.h"
#include "dds/ddsi/ddsi_tran.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_xmsg.h"
#include "dds/features.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_serdata;
struct ddsi_addrset;
struct ddsi_proxy_reader;
struct ddsi_proxy_writer;
struct ddsi_writer;
struct ddsi_participant;
struct ddsi_xmsgpool;
struct ddsi_xmsg;
struct ddsi_xpack;
struct ddsi_plist_sample;

struct ddsi_xmsg_marker {
  size_t offset;
};

enum ddsi_xmsg_kind {
  DDSI_XMSG_KIND_CONTROL,
  DDSI_XMSG_KIND_DATA,
  DDSI_XMSG_KIND_DATA_REXMIT,
  DDSI_XMSG_KIND_DATA_REXMIT_NOMERGE
};

/* XMSGPOOL */
struct ddsi_xmsgpool *ddsi_xmsgpool_new (void);
void ddsi_xmsgpool_free (struct ddsi_xmsgpool *pool);


/* XMSG */

/* To allocate a new xmsg from the pool; if expected_size is NOT
   exceeded, no reallocs will be performed, else the address of the
   xmsg may change because of reallocing when appending to it. */
struct ddsi_xmsg *ddsi_xmsg_new (struct ddsi_xmsgpool *pool, const ddsi_guid_t *src_guid, struct ddsi_participant *pp, size_t expected_size, enum ddsi_xmsg_kind kind);

/* For sending to a particular destination (participant) */
void ddsi_xmsg_setdst1 (struct ddsi_domaingv *gv, struct ddsi_xmsg *m, const ddsi_guid_prefix_t *gp, const ddsi_xlocator_t *addr);
bool ddsi_xmsg_getdst1_prefix (struct ddsi_xmsg *m, ddsi_guid_prefix_t *gp);

/* For sending to a particular proxy reader; this is a convenience
   routine that extracts a suitable address from the proxy reader's
   address sets and calls setdst1. */
void ddsi_xmsg_setdst_prd (struct ddsi_xmsg *m, const struct ddsi_proxy_reader *prd);
void ddsi_xmsg_setdst_pwr (struct ddsi_xmsg *m, const struct ddsi_proxy_writer *pwr);

/* For sending to all in the address set AS -- typically, the writer's
   address set to multicast to all matched readers */
void ddsi_xmsg_setdst_n (struct ddsi_xmsg *msg, struct ddsi_addrset *as);

int ddsi_xmsg_setmaxdelay (struct ddsi_xmsg *msg, int64_t maxdelay);

#ifdef DDS_HAS_NETWORK_PARTITIONS
int ddsi_xmsg_setencoderid (struct ddsi_xmsg *msg, uint32_t encoderid);
#endif

/* Sets the location of the destination readerId within the message
   (address changes because of reallocations are handled correctly).
   M must be a rexmit, and for all rexmits this must be called.  It is
   a separate function because the location may only become known at a
   late-ish stage in the construction of the message. */
void ddsi_xmsg_set_data_reader_id (struct ddsi_xmsg *m, ddsi_entityid_t *readerId);

/* If M and MADD are both xmsg's containing the same retransmit
   message, this will merge the destination embedded in MADD into M.
   Typically, this will cause the readerId of M to be cleared and the
   destination to change to the writer's address set.

   M and MADD *must* contain the same sample/fragment of a sample.

   Returns 1 if merge was successful, else 0.  On failure, neither
   message will have been changed and both should be sent as if there
   had been no merging. */
int ddsi_xmsg_merge_rexmit_destinations_wrlock_held (struct ddsi_domaingv *gv, struct ddsi_xmsg *m, const struct ddsi_xmsg *madd);

/* To set writer ids for updating last transmitted sequence number;
   wrfragid is 0 based, unlike DDSI but like other places where
   fragment numbers are handled internally. */
void ddsi_xmsg_setwriterseq (struct ddsi_xmsg *msg, const ddsi_guid_t *wrguid, ddsi_seqno_t wrseq);
void ddsi_xmsg_setwriterseq_fragid (struct ddsi_xmsg *msg, const ddsi_guid_t *wrguid, ddsi_seqno_t wrseq, ddsi_fragment_number_t wrfragid);

/* Comparison function for retransmits: orders messages on writer
   guid, sequence number and fragment id */
int ddsi_xmsg_compare_fragid (const struct ddsi_xmsg *a, const struct ddsi_xmsg *b);

void ddsi_xmsg_free (struct ddsi_xmsg *msg);
size_t ddsi_xmsg_size (const struct ddsi_xmsg *m);
void *ddsi_xmsg_payload (size_t *sz, struct ddsi_xmsg *m);
void ddsi_xmsg_payload_to_plistsample (struct ddsi_plist_sample *dst, ddsi_parameterid_t keyparam, const struct ddsi_xmsg *m);
enum ddsi_xmsg_kind ddsi_xmsg_kind (const struct ddsi_xmsg *m);
void ddsi_xmsg_guid_seq_fragid (const struct ddsi_xmsg *m, ddsi_guid_t *wrguid, ddsi_seqno_t *wrseq, ddsi_fragment_number_t *wrfragid);

void *ddsi_xmsg_submsg_from_marker (struct ddsi_xmsg *msg, struct ddsi_xmsg_marker marker);
void *ddsi_xmsg_append (struct ddsi_xmsg *m, struct ddsi_xmsg_marker *marker, size_t sz);
void ddsi_xmsg_shrink (struct ddsi_xmsg *m, struct ddsi_xmsg_marker marker, size_t sz);
void ddsi_xmsg_serdata (struct ddsi_xmsg *m, struct ddsi_serdata *serdata, size_t off, size_t len, struct ddsi_writer *wr);
#ifdef DDS_HAS_SECURITY
size_t ddsi_xmsg_submsg_size (struct ddsi_xmsg *msg, struct ddsi_xmsg_marker marker);
void ddsi_xmsg_submsg_remove (struct ddsi_xmsg *msg, struct ddsi_xmsg_marker sm_marker);
void ddsi_xmsg_submsg_replace (struct ddsi_xmsg *msg, struct ddsi_xmsg_marker sm_marker, unsigned char *new_submsg, size_t new_len);
void ddsi_xmsg_submsg_append_refd_payload (struct ddsi_xmsg *msg, struct ddsi_xmsg_marker sm_marker);
#endif
void ddsi_xmsg_submsg_setnext (struct ddsi_xmsg *msg, struct ddsi_xmsg_marker marker);
void ddsi_xmsg_submsg_init (struct ddsi_xmsg *msg, struct ddsi_xmsg_marker marker, ddsi_rtps_submessage_kind_t smkind);
void ddsi_xmsg_add_timestamp (struct ddsi_xmsg *m, ddsrt_wctime_t t);
void ddsi_xmsg_add_entityid (struct ddsi_xmsg * m);
void *ddsi_xmsg_addpar_bo (struct ddsi_xmsg *m, ddsi_parameterid_t pid, size_t len, enum ddsrt_byte_order_selector bo);
void *ddsi_xmsg_addpar (struct ddsi_xmsg *m, ddsi_parameterid_t pid, size_t len);
void ddsi_xmsg_addpar_keyhash (struct ddsi_xmsg *m, const struct ddsi_serdata *serdata, bool force_md5);
void ddsi_xmsg_addpar_statusinfo (struct ddsi_xmsg *m, unsigned statusinfo);
void ddsi_xmsg_addpar_sentinel (struct ddsi_xmsg *m);
void ddsi_xmsg_addpar_sentinel_bo (struct ddsi_xmsg * m, enum ddsrt_byte_order_selector bo);
int ddsi_xmsg_addpar_sentinel_ifparam (struct ddsi_xmsg *m);

/* XPACK */
int ddsi_xpack_addmsg (struct ddsi_xpack *xp, struct ddsi_xmsg *m, const uint32_t flags);
int64_t ddsi_xpack_maxdelay (const struct ddsi_xpack *xp);
unsigned ddsi_xpack_packetid (const struct ddsi_xpack *xp);

/* SENDQ */
void ddsi_xpack_sendq_stop (struct ddsi_domaingv *gv);
void ddsi_xpack_sendq_fini (struct ddsi_domaingv *gv);

#if defined (__cplusplus)
}
#endif
#endif /* DDSI__XMSG_H */
