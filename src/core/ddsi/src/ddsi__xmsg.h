// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__XMSG_H
#define DDSI__XMSG_H

#include <stddef.h>

#include "dds/features.h"
#include "dds/ddsrt/bswap.h"
#include "dds/ddsi/ddsi_tran.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_xmsg.h"
#include "ddsi__protocol.h"

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

/** @component rtps_submsg */
struct ddsi_xmsgpool *ddsi_xmsgpool_new (void);

/** @component rtps_submsg */
void ddsi_xmsgpool_free (struct ddsi_xmsgpool *pool);

/**
 * @brief Allocates a new xmsg from the pool
 * @component rtps_submsg
 *
 * if expected_size is NOT exceeded, no reallocs will be performed,
 * else the address of the xmsg may change because of reallocing
 * when appending to it.
 *
 * @param pool          message pool
 * @param src_guid      source guid
 * @param pp            participant
 * @param expected_size expected message size
 * @param kind          the xmsg kind
 * @return struct ddsi_xmsg*
 */
struct ddsi_xmsg *ddsi_xmsg_new (struct ddsi_xmsgpool *pool, const ddsi_guid_t *src_guid, struct ddsi_participant *pp, size_t expected_size, enum ddsi_xmsg_kind kind);

/**
 * @brief For sending to a particular destination (participant)
 * @component rtps_submsg
 *
 * @param gv    domain globals
 * @param m     xmsg
 * @param gp    guid prefix
 * @param addr  destination locator
 */
void ddsi_xmsg_setdst1 (struct ddsi_domaingv *gv, struct ddsi_xmsg *m, const ddsi_guid_prefix_t *gp, const ddsi_xlocator_t *addr);

/** @component rtps_submsg */
bool ddsi_xmsg_getdst1_prefix (struct ddsi_xmsg *m, ddsi_guid_prefix_t *gp);

/**
 * @brief For sending to a particular proxy reader
 * @component rtps_submsg
 *
 * This is a convenience routine that extracts a suitable address from the
 * proxy reader's address sets and calls setdst1.
 *
 * @param m   xmsg
 * @param prd destination proxy reader
 */
void ddsi_xmsg_setdst_prd (struct ddsi_xmsg *m, const struct ddsi_proxy_reader *prd);

/** @component rtps_submsg */
void ddsi_xmsg_setdst_pwr (struct ddsi_xmsg *m, const struct ddsi_proxy_writer *pwr);

/**
 * @brief For sending to all in the address set AS
 * @component rtps_submsg
 *
 * Typically, the writer's address set to multicast to all matched readers
 *
 * @param msg xmsg
 * @param as  address set
 */
void ddsi_xmsg_setdst_addrset (struct ddsi_xmsg *msg, struct ddsi_addrset *as);

/** @component rtps_submsg */
int ddsi_xmsg_setmaxdelay (struct ddsi_xmsg *msg, int64_t maxdelay);

/**
 * @brief Sets the location of the destination readerId within the message
 * @component rtps_submsg
 *
 * (address changes because of reallocations are handled correctly).
 * M must be a rexmit, and for all rexmits this must be called. It is a
 * separate function because the location may only become known at a late-ish
 * stage in the construction of the message.
 *
 * @param m         xmsg
 * @param readerId  reader entity id
 */
void ddsi_xmsg_set_data_reader_id (struct ddsi_xmsg *m, ddsi_entityid_t *readerId);

/**
 * @component rtps_submsg
 *
 * If M and MADD are both xmsg's containing the same retransmit message, this will
 * merge the destination embedded in MADD into M. Typically, this will cause the
 * readerId of M to be cleared and the destination to change to the writer's
 * address set.
 *
 * M and MADD *must* contain the same sample/fragment of a sample. Returns 1 if
 * merge was successful, else 0. On failure, neither message will have been changed
 * and both should be sent as if there had been no merging.
 *
 * @param gv    domain globals
 * @param m     xmsg
 * @param madd  xmsg to add
 * @returns Returns 1 if merge was successful, else 0.
 */
int ddsi_xmsg_merge_rexmit_destinations_wrlock_held (struct ddsi_domaingv *gv, struct ddsi_xmsg *m, const struct ddsi_xmsg *madd);

/**
 * @brief To set writer ids for updating last transmitted sequence number
 * @component rtps_submsg
 *
 * wrfragid is 0 based, unlike DDSI but like other places where
   fragment numbers are handled internally.

 * @param msg     xmsg
 * @param wrguid  writer guid
 * @param wrseq   write sequence number
 */
void ddsi_xmsg_setwriterseq (struct ddsi_xmsg *msg, const ddsi_guid_t *wrguid, ddsi_seqno_t wrseq);

/** @component rtps_submsg */
void ddsi_xmsg_setwriterseq_fragid (struct ddsi_xmsg *msg, const ddsi_guid_t *wrguid, ddsi_seqno_t wrseq, ddsi_fragment_number_t wrfragid);

/**
 * @brief Comparison function for retransmits
 * @component rtps_submsg
 *
 * Orders messages on writer guid, sequence number and fragment id
 *
 * @param a   xmsg
 * @param b   xmsg to compare with
 * @return int
 */
int ddsi_xmsg_compare_fragid (const struct ddsi_xmsg *a, const struct ddsi_xmsg *b);

/** @component rtps_submsg */
void ddsi_xmsg_free (struct ddsi_xmsg *msg);

/** @component rtps_submsg */
size_t ddsi_xmsg_size (const struct ddsi_xmsg *m);

/** @component rtps_submsg */
void *ddsi_xmsg_payload (size_t *sz, struct ddsi_xmsg *m);

/** @component rtps_submsg */
void ddsi_xmsg_payload_to_plistsample (struct ddsi_plist_sample *dst, ddsi_parameterid_t keyparam, const struct ddsi_xmsg *m);

/** @component rtps_submsg */
enum ddsi_xmsg_kind ddsi_xmsg_kind (const struct ddsi_xmsg *m);

/** @component rtps_submsg */
void ddsi_xmsg_guid_seq_fragid (const struct ddsi_xmsg *m, ddsi_guid_t *wrguid, ddsi_seqno_t *wrseq, ddsi_fragment_number_t *wrfragid);


/** @component rtps_submsg */
void *ddsi_xmsg_submsg_from_marker (struct ddsi_xmsg *msg, struct ddsi_xmsg_marker marker);

/** @component rtps_submsg */
void *ddsi_xmsg_append (struct ddsi_xmsg *m, struct ddsi_xmsg_marker *marker, size_t sz);

/** @component rtps_submsg */
void ddsi_xmsg_shrink (struct ddsi_xmsg *m, struct ddsi_xmsg_marker marker, size_t sz);

/** @component rtps_submsg */
void ddsi_xmsg_serdata (struct ddsi_xmsg *m, struct ddsi_serdata *serdata, size_t off, size_t len, struct ddsi_writer *wr);


#ifdef DDS_HAS_SECURITY
/** @component rtps_submsg */
size_t ddsi_xmsg_submsg_size (struct ddsi_xmsg *msg, struct ddsi_xmsg_marker marker);

/** @component rtps_submsg */
void ddsi_xmsg_submsg_remove (struct ddsi_xmsg *msg, struct ddsi_xmsg_marker sm_marker);

/** @component rtps_submsg */
void ddsi_xmsg_submsg_replace (struct ddsi_xmsg *msg, struct ddsi_xmsg_marker sm_marker, unsigned char *new_submsg, size_t new_len);

/** @component rtps_submsg */
void ddsi_xmsg_submsg_append_refd_payload (struct ddsi_xmsg *msg, struct ddsi_xmsg_marker sm_marker);

#endif /* DDS_HAS_SECURITY */


/** @component rtps_submsg */
void ddsi_xmsg_submsg_setnext (struct ddsi_xmsg *msg, struct ddsi_xmsg_marker marker);

/** @component rtps_submsg */
void ddsi_xmsg_submsg_init (struct ddsi_xmsg *msg, struct ddsi_xmsg_marker marker, ddsi_rtps_submessage_kind_t smkind);

/** @component rtps_submsg */
void ddsi_xmsg_add_timestamp (struct ddsi_xmsg *m, ddsrt_wctime_t t);

/** @component rtps_submsg */
void ddsi_xmsg_add_entityid (struct ddsi_xmsg * m);

/** @component rtps_submsg */
void *ddsi_xmsg_addpar_bo (struct ddsi_xmsg *m, ddsi_parameterid_t pid, size_t len, enum ddsrt_byte_order_selector bo);

/** @component rtps_submsg */
void *ddsi_xmsg_addpar (struct ddsi_xmsg *m, ddsi_parameterid_t pid, size_t len);

/** @component rtps_submsg */
void ddsi_xmsg_addpar_keyhash (struct ddsi_xmsg *m, const struct ddsi_serdata *serdata, bool force_md5);

/** @component rtps_submsg */
void ddsi_xmsg_addpar_statusinfo (struct ddsi_xmsg *m, unsigned statusinfo);

/** @component rtps_submsg */
void ddsi_xmsg_addpar_sentinel (struct ddsi_xmsg *m);
/** @component rtps_submsg */
void ddsi_xmsg_addpar_sentinel_bo (struct ddsi_xmsg * m, enum ddsrt_byte_order_selector bo);

/** @component rtps_submsg */
int ddsi_xmsg_addpar_sentinel_ifparam (struct ddsi_xmsg *m);



/** @component rtps_msg */
int ddsi_xpack_addmsg (struct ddsi_xpack *xp, struct ddsi_xmsg *m, const uint32_t flags);

/** @component rtps_msg */
int64_t ddsi_xpack_maxdelay (const struct ddsi_xpack *xp);

/** @component rtps_msg */
unsigned ddsi_xpack_packetid (const struct ddsi_xpack *xp);

/** @component rtps_msg */
void ddsi_xpack_sendq_stop (struct ddsi_domaingv *gv);

/** @component rtps_msg */
void ddsi_xpack_sendq_fini (struct ddsi_domaingv *gv);

#if defined (__cplusplus)
}
#endif
#endif /* DDSI__XMSG_H */
