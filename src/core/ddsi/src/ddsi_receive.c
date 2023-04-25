// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_gc.h"
#include "dds/ddsi/ddsi_proxy_participant.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "ddsi__log.h"
#include "ddsi__protocol.h"
#include "ddsi__misc.h"
#include "ddsi__bswap.h"
#include "ddsi__lat_estim.h"
#include "ddsi__bitset.h"
#include "ddsi__xevent.h"
#include "ddsi__addrset.h"
#include "ddsi__discovery.h"
#include "ddsi__radmin.h"
#include "ddsi__thread.h"
#include "ddsi__entity_index.h"
#include "ddsi__lease.h"
#include "ddsi__entity.h"
#include "ddsi__participant.h"
#include "ddsi__xmsg.h"
#include "ddsi__receive.h"
#include "ddsi__rhc.h"
#include "ddsi__transmit.h"
#include "ddsi__mcgroup.h"
#include "ddsi__security_omg.h"
#include "ddsi__acknack.h"
#include "ddsi__sysdeps.h"
#include "ddsi__deliver_locally.h"
#include "ddsi__endpoint.h"
#include "ddsi__endpoint_match.h"
#include "ddsi__plist.h"
#include "ddsi__proxy_endpoint.h"
#include "ddsi__tran.h"
#include "ddsi__vendor.h"
#include "ddsi__hbcontrol.h"
#include "ddsi__sockwaitset.h"

#include "dds/cdr/dds_cdrstream.h"
#include "dds__whc.h"

/*
Notes:

- for now, the safer option is usually chosen: hold a lock even if it
  isn't strictly necessary in the particular configuration we have
  (such as one receive thread vs. multiple receive threads)

- ddsi_dqueue_enqueue may be called with pwr->e.lock held

- deliver_user_data_synchronously may be called with pwr->e.lock held,
  which is needed if IN-ORDER synchronous delivery is desired when
  there are also multiple receive threads

- deliver_user_data gets passed in whether pwr->e.lock is held on entry

*/

static void deliver_user_data_synchronously (struct ddsi_rsample_chain *sc, const ddsi_guid_t *rdguid);

static void maybe_set_reader_in_sync (struct ddsi_proxy_writer *pwr, struct ddsi_pwr_rd_match *wn, ddsi_seqno_t last_deliv_seq)
{
  switch (wn->in_sync)
  {
    case PRMSS_SYNC:
      assert(0);
      break;
    case PRMSS_TLCATCHUP:
      if (last_deliv_seq >= wn->u.not_in_sync.end_of_tl_seq)
      {
        wn->in_sync = PRMSS_SYNC;
        if (--pwr->n_readers_out_of_sync == 0)
          ddsi_local_reader_ary_setfastpath_ok (&pwr->rdary, true);
      }
      break;
    case PRMSS_OUT_OF_SYNC:
      if (!wn->filtered)
      {
        if (pwr->have_seen_heartbeat && ddsi_reorder_next_seq (wn->u.not_in_sync.reorder) == ddsi_reorder_next_seq (pwr->reorder))
        {
          ETRACE (pwr, " msr_in_sync("PGUIDFMT" out-of-sync to tlcatchup)", PGUID (wn->rd_guid));
          wn->in_sync = PRMSS_TLCATCHUP;
          maybe_set_reader_in_sync (pwr, wn, last_deliv_seq);
        }
      }
      break;
  }
}

static bool valid_sequence_number_set (const ddsi_sequence_number_set_header_t *snset, ddsi_seqno_t *start)
{
  // reject sets that imply sequence numbers beyond the range of valid sequence numbers
  // (not a spec'd requirement)
  return (ddsi_validating_from_seqno (snset->bitmap_base, start) && snset->numbits <= 256 && snset->numbits <= DDSI_MAX_SEQ_NUMBER - *start);
}

static bool valid_fragment_number_set (const ddsi_fragment_number_set_header_t *fnset)
{
  // reject sets that imply fragment numbers beyond the range of valid fragment numbers
  // (not a spec'd requirement)
  return (fnset->bitmap_base > 0 && fnset->numbits <= 256 && fnset->numbits <= UINT32_MAX - fnset->bitmap_base);
}

enum validation_result {
  VR_MALFORMED,
  VR_NOT_UNDERSTOOD,
  VR_ACCEPT
};

static enum validation_result validate_writer_and_reader_entityid (ddsi_entityid_t wrid, ddsi_entityid_t rdid)
{
  if (ddsi_is_writer_entityid (wrid) && ddsi_is_reader_entityid (rdid))
    return VR_ACCEPT;
  else // vendor-specific entity kinds means ignoring it is better than saying "malformed"
    return VR_NOT_UNDERSTOOD;
}

static enum validation_result validate_writer_and_reader_or_null_entityid (ddsi_entityid_t wrid, ddsi_entityid_t rdid)
{
  // the official term is "unknown entity id" but that's too close for comfort
  // to "unknown entity" in the message validation code
  if (ddsi_is_writer_entityid (wrid) && (rdid.u == DDSI_ENTITYID_UNKNOWN || ddsi_is_reader_entityid (rdid)))
    return VR_ACCEPT;
  else // see validate_writer_and_reader_entityid
    return VR_NOT_UNDERSTOOD;
}

static enum validation_result validate_AckNack (const struct ddsi_receiver_state *rst, ddsi_rtps_acknack_t *msg, size_t size, int byteswap)
{
  ddsi_count_t *count; /* this should've preceded the bitmap */
  if (size < DDSI_ACKNACK_SIZE (0))
    return VR_MALFORMED;
  if (byteswap)
  {
    ddsi_bswap_sequence_number_set_hdr (&msg->readerSNState);
    /* bits[], count deferred until validation of fixed part */
  }
  msg->readerId = ddsi_ntoh_entityid (msg->readerId);
  msg->writerId = ddsi_ntoh_entityid (msg->writerId);
  /* Validation following 8.3.7.1.3 + 8.3.5.5 */
  ddsi_seqno_t ackseq;
  if (!valid_sequence_number_set (&msg->readerSNState, &ackseq))
  {
    /* FastRTPS, Connext send invalid pre-emptive ACKs -- patch the message to
       make it well-formed and process it as normal */
    if (! DDSI_SC_STRICT_P (rst->gv->config) &&
        (ackseq == 0 && msg->readerSNState.numbits == 0) &&
        (ddsi_vendor_is_eprosima (rst->vendor) || ddsi_vendor_is_rti (rst->vendor) || ddsi_vendor_is_rti_micro (rst->vendor)))
      msg->readerSNState.bitmap_base = ddsi_to_seqno (1);
    else
      return VR_MALFORMED;
  }
  /* Given the number of bits, we can compute the size of the AckNack
     submessage, and verify that the submessage is large enough */
  if (size < DDSI_ACKNACK_SIZE (msg->readerSNState.numbits))
    return VR_MALFORMED;
  count = (ddsi_count_t *) ((char *) &msg->bits + DDSI_SEQUENCE_NUMBER_SET_BITS_SIZE (msg->readerSNState.numbits));
  if (byteswap)
  {
    ddsi_bswap_sequence_number_set_bitmap (&msg->readerSNState, msg->bits);
    *count = ddsrt_bswap4u (*count);
  }
  // do reader/writer entity id validation last: if it returns "NOT_UNDERSTOOD" for an
  // otherwise malformed message, we still need to discard the message in its entirety
  //
  // unspecified reader makes no sense in the context of an ACKNACK
  return validate_writer_and_reader_entityid (msg->writerId, msg->readerId);
}

static enum validation_result validate_Gap (ddsi_rtps_gap_t *msg, size_t size, int byteswap)
{
  if (size < DDSI_GAP_SIZE (0))
    return VR_MALFORMED;
  if (byteswap)
  {
    ddsi_bswap_sequence_number (&msg->gapStart);
    ddsi_bswap_sequence_number_set_hdr (&msg->gapList);
  }
  msg->readerId = ddsi_ntoh_entityid (msg->readerId);
  msg->writerId = ddsi_ntoh_entityid (msg->writerId);
  ddsi_seqno_t gapstart;
  if (!ddsi_validating_from_seqno (msg->gapStart, &gapstart))
    return VR_MALFORMED;
  ddsi_seqno_t gapend;
  if (!valid_sequence_number_set (&msg->gapList, &gapend))
    return VR_MALFORMED;
  // gapstart >= gapend is not listed as malformed in spec but it really makes no sense
  // the only plausible interpretation is that the interval is empty and that only the
  // bitmap matters (which could then be all-0 in which case the message is roughly
  // equivalent to a heartbeat that says 1 .. N ...  Rewrite so at least end >= start
  if (gapend < gapstart)
    msg->gapStart = msg->gapList.bitmap_base;
  if (size < DDSI_GAP_SIZE (msg->gapList.numbits))
    return VR_MALFORMED;
  if (byteswap)
    ddsi_bswap_sequence_number_set_bitmap (&msg->gapList, msg->bits);
  // do reader/writer entity id validation last: if it returns "NOT_UNDERSTOOD" for an
  // otherwise malformed message, we still need to discard the message in its entirety
  return validate_writer_and_reader_or_null_entityid (msg->writerId, msg->readerId);
}

static enum validation_result validate_InfoDST (ddsi_rtps_info_dst_t *msg, size_t size, UNUSED_ARG (int byteswap))
{
  if (size < sizeof (*msg))
    return VR_MALFORMED;
  return VR_ACCEPT;
}

static enum validation_result validate_InfoSRC (ddsi_rtps_info_src_t *msg, size_t size, UNUSED_ARG (int byteswap))
{
  if (size < sizeof (*msg))
    return VR_MALFORMED;
  return VR_ACCEPT;
}

static enum validation_result validate_InfoTS (ddsi_rtps_info_ts_t *msg, size_t size, int byteswap)
{
  assert (sizeof (ddsi_rtps_submessage_header_t) <= size);
  if (msg->smhdr.flags & DDSI_INFOTS_INVALIDATE_FLAG)
    return VR_ACCEPT;
  else if (size < sizeof (ddsi_rtps_info_ts_t))
    return VR_MALFORMED;
  else
  {
    if (byteswap)
    {
      msg->time.seconds = ddsrt_bswap4 (msg->time.seconds);
      msg->time.fraction = ddsrt_bswap4u (msg->time.fraction);
    }
    return ddsi_is_valid_timestamp (msg->time) ? VR_ACCEPT : VR_MALFORMED;
  }
}

static enum validation_result validate_Heartbeat (ddsi_rtps_heartbeat_t *msg, size_t size, int byteswap)
{
  if (size < sizeof (*msg))
    return VR_MALFORMED;
  if (byteswap)
  {
    ddsi_bswap_sequence_number (&msg->firstSN);
    ddsi_bswap_sequence_number (&msg->lastSN);
    msg->count = ddsrt_bswap4u (msg->count);
  }
  msg->readerId = ddsi_ntoh_entityid (msg->readerId);
  msg->writerId = ddsi_ntoh_entityid (msg->writerId);
  /* Validation following 8.3.7.5.3; lastSN + 1 == firstSN: no data; test using
     firstSN-1 because lastSN+1 can overflow and we already know firstSN-1 >= 0 */
  if (ddsi_from_seqno (msg->firstSN) <= 0 || ddsi_from_seqno (msg->lastSN) < ddsi_from_seqno (msg->firstSN) - 1)
    return VR_MALFORMED;
  // do reader/writer entity id validation last: if it returns "NOT_UNDERSTOOD" for an
  // otherwise malformed message, we still need to discard the message in its entirety
  return validate_writer_and_reader_or_null_entityid (msg->writerId, msg->readerId);
}

static enum validation_result validate_HeartbeatFrag (ddsi_rtps_heartbeatfrag_t *msg, size_t size, int byteswap)
{
  if (size < sizeof (*msg))
    return VR_MALFORMED;
  if (byteswap)
  {
    ddsi_bswap_sequence_number (&msg->writerSN);
    msg->lastFragmentNum = ddsrt_bswap4u (msg->lastFragmentNum);
    msg->count = ddsrt_bswap4u (msg->count);
  }
  msg->readerId = ddsi_ntoh_entityid (msg->readerId);
  msg->writerId = ddsi_ntoh_entityid (msg->writerId);
  if (ddsi_from_seqno (msg->writerSN) <= 0 || msg->lastFragmentNum == 0)
    return VR_MALFORMED;
  // do reader/writer entity id validation last: if it returns "NOT_UNDERSTOOD" for an
  // otherwise malformed message, we still need to discard the message in its entirety
  return validate_writer_and_reader_or_null_entityid (msg->writerId, msg->readerId);
}

static enum validation_result validate_NackFrag (ddsi_rtps_nackfrag_t *msg, size_t size, int byteswap)
{
  ddsi_count_t *count; /* this should've preceded the bitmap */
  if (size < DDSI_NACKFRAG_SIZE (0))
    return VR_MALFORMED;
  if (byteswap)
  {
    ddsi_bswap_sequence_number (&msg->writerSN);
    ddsi_bswap_fragment_number_set_hdr (&msg->fragmentNumberState);
    /* bits[], count deferred until validation of fixed part */
  }
  msg->readerId = ddsi_ntoh_entityid (msg->readerId);
  msg->writerId = ddsi_ntoh_entityid (msg->writerId);
  /* Validation following 8.3.7.1.3 + 8.3.5.5 */
  if (!valid_fragment_number_set (&msg->fragmentNumberState))
    return VR_MALFORMED;
  /* Given the number of bits, we can compute the size of the Nackfrag
     submessage, and verify that the submessage is large enough */
  if (size < DDSI_NACKFRAG_SIZE (msg->fragmentNumberState.numbits))
    return VR_MALFORMED;
  count = (ddsi_count_t *) ((char *) &msg->bits + DDSI_FRAGMENT_NUMBER_SET_BITS_SIZE (msg->fragmentNumberState.numbits));
  if (byteswap)
  {
    ddsi_bswap_fragment_number_set_bitmap (&msg->fragmentNumberState, msg->bits);
    *count = ddsrt_bswap4u (*count);
  }
  // do reader/writer entity id validation last: if it returns "NOT_UNDERSTOOD" for an
  // otherwise malformed message, we still need to discard the message in its entirety
  //
  // unspecified reader makes no sense in the context of NACKFRAG
  return validate_writer_and_reader_entityid (msg->writerId, msg->readerId);
}

static void set_sampleinfo_proxy_writer (struct ddsi_rsample_info *sampleinfo, ddsi_guid_t *pwr_guid)
{
  struct ddsi_proxy_writer * pwr = ddsi_entidx_lookup_proxy_writer_guid (sampleinfo->rst->gv->entity_index, pwr_guid);
  sampleinfo->pwr = pwr;
}

static bool set_sampleinfo_bswap (struct ddsi_rsample_info *sampleinfo, struct dds_cdr_header *hdr)
{
  if (hdr)
  {
    if (!DDSI_RTPS_CDR_ENC_IS_VALID(hdr->identifier))
      return false;
    sampleinfo->bswap = !DDSI_RTPS_CDR_ENC_IS_NATIVE(hdr->identifier);
  }
  return true;
}

static enum validation_result validate_Data (const struct ddsi_receiver_state *rst, ddsi_rtps_data_t *msg, size_t size, int byteswap, struct ddsi_rsample_info *sampleinfo, const ddsi_keyhash_t **keyhashp, unsigned char **payloadp, uint32_t *payloadsz)
{
  /* on success: sampleinfo->{seq,rst,statusinfo,bswap,complex_qos} all set */
  ddsi_guid_t pwr_guid;
  unsigned char *ptr;

  if (size < sizeof (*msg))
    return VR_MALFORMED; /* too small even for fixed fields */
  /* D=1 && K=1 is invalid in this version of the protocol */
  if ((msg->x.smhdr.flags & (DDSI_DATA_FLAG_DATAFLAG | DDSI_DATA_FLAG_KEYFLAG)) ==
      (DDSI_DATA_FLAG_DATAFLAG | DDSI_DATA_FLAG_KEYFLAG))
    return VR_MALFORMED;
  if (byteswap)
  {
    msg->x.extraFlags = ddsrt_bswap2u (msg->x.extraFlags);
    msg->x.octetsToInlineQos = ddsrt_bswap2u (msg->x.octetsToInlineQos);
    ddsi_bswap_sequence_number (&msg->x.writerSN);
  }
  if ((msg->x.octetsToInlineQos % 4) != 0) {
    // Not quite clear whether this is actually required
    return VR_MALFORMED;
  }
  msg->x.readerId = ddsi_ntoh_entityid (msg->x.readerId);
  msg->x.writerId = ddsi_ntoh_entityid (msg->x.writerId);
  pwr_guid.prefix = rst->src_guid_prefix;
  pwr_guid.entityid = msg->x.writerId;

  sampleinfo->rst = (struct ddsi_receiver_state *) rst; /* drop const */
  if (!ddsi_validating_from_seqno (msg->x.writerSN, &sampleinfo->seq))
    return VR_MALFORMED;
  sampleinfo->fragsize = 0; /* for unfragmented data, fragsize = 0 works swell */

  if ((msg->x.smhdr.flags & (DDSI_DATA_FLAG_INLINE_QOS | DDSI_DATA_FLAG_DATAFLAG | DDSI_DATA_FLAG_KEYFLAG)) == 0)
  {
    /* no QoS, no payload, so octetsToInlineQos will never be used
       though one would expect octetsToInlineQos and size to be in
       agreement or octetsToInlineQos to be 0 or so */
    *payloadp = NULL;
    *keyhashp = NULL;
    sampleinfo->size = 0; /* size is full payload size, no payload & unfragmented => size = 0 */
    sampleinfo->statusinfo = 0;
    sampleinfo->complex_qos = 0;
    goto accept;
  }

  /* QoS and/or payload, so octetsToInlineQos must be within the
     msg; since the serialized data and serialized parameter lists
     have a 4 byte header, that one, too must fit */
  if (offsetof (ddsi_rtps_data_datafrag_common_t, octetsToInlineQos) + sizeof (msg->x.octetsToInlineQos) + msg->x.octetsToInlineQos + 4 > size)
    return VR_MALFORMED;

  ptr = (unsigned char *) msg + offsetof (ddsi_rtps_data_datafrag_common_t, octetsToInlineQos) + sizeof (msg->x.octetsToInlineQos) + msg->x.octetsToInlineQos;
  if (msg->x.smhdr.flags & DDSI_DATA_FLAG_INLINE_QOS)
  {
    ddsi_plist_src_t src;
    src.protocol_version = rst->protocol_version;
    src.vendorid = rst->vendor;
    src.encoding = (msg->x.smhdr.flags & DDSI_RTPS_SUBMESSAGE_FLAG_ENDIANNESS) ? DDSI_RTPS_PL_CDR_LE : DDSI_RTPS_PL_CDR_BE;
    src.buf = ptr;
    src.bufsz = (unsigned) ((unsigned char *) msg + size - src.buf); /* end of message, that's all we know */
    /* just a quick scan, gathering only what we _really_ need */
    if ((ptr = ddsi_plist_quickscan (sampleinfo, keyhashp, &src, rst->gv)) == NULL)
      return VR_MALFORMED;
  }
  else
  {
    sampleinfo->statusinfo = 0;
    sampleinfo->complex_qos = 0;
    *keyhashp = NULL;
  }

  if (!(msg->x.smhdr.flags & (DDSI_DATA_FLAG_DATAFLAG | DDSI_DATA_FLAG_KEYFLAG)))
  {
    /*TRACE (("no payload\n"));*/
    *payloadp = NULL;
    *payloadsz = 0;
    sampleinfo->size = 0;
  }
  else if ((size_t) ((char *) ptr + 4 - (char *) msg) > size)
  {
    /* no space for the header */
    return VR_MALFORMED;
  }
  else
  {
    sampleinfo->size = (uint32_t) ((char *) msg + size - (char *) ptr);
    *payloadsz = sampleinfo->size;
    *payloadp = ptr;
  }
accept:
  ;
  // do reader/writer entity id validation last: if it returns "NOT_UNDERSTOOD" for an
  // otherwise malformed message, we still need to discard the message in its entirety
  enum validation_result vr = validate_writer_and_reader_or_null_entityid (msg->x.writerId, msg->x.readerId);
  if (vr == VR_ACCEPT)
    set_sampleinfo_proxy_writer (sampleinfo, &pwr_guid);
  return vr;
}

static enum validation_result validate_DataFrag (const struct ddsi_receiver_state *rst, ddsi_rtps_datafrag_t *msg, size_t size, int byteswap, struct ddsi_rsample_info *sampleinfo, const ddsi_keyhash_t **keyhashp, unsigned char **payloadp, uint32_t *payloadsz)
{
  ddsi_guid_t pwr_guid;
  unsigned char *ptr;

  if (size < sizeof (*msg))
    return VR_MALFORMED; /* too small even for fixed fields */

  if (byteswap)
  {
    msg->x.extraFlags = ddsrt_bswap2u (msg->x.extraFlags);
    msg->x.octetsToInlineQos = ddsrt_bswap2u (msg->x.octetsToInlineQos);
    ddsi_bswap_sequence_number (&msg->x.writerSN);
    msg->fragmentStartingNum = ddsrt_bswap4u (msg->fragmentStartingNum);
    msg->fragmentsInSubmessage = ddsrt_bswap2u (msg->fragmentsInSubmessage);
    msg->fragmentSize = ddsrt_bswap2u (msg->fragmentSize);
    msg->sampleSize = ddsrt_bswap4u (msg->sampleSize);
  }
  if ((msg->x.octetsToInlineQos % 4) != 0) {
    // Not quite clear whether this is actually required
    return VR_MALFORMED;
  }
  msg->x.readerId = ddsi_ntoh_entityid (msg->x.readerId);
  msg->x.writerId = ddsi_ntoh_entityid (msg->x.writerId);
  pwr_guid.prefix = rst->src_guid_prefix;
  pwr_guid.entityid = msg->x.writerId;

  if (DDSI_SC_STRICT_P (rst->gv->config) && msg->fragmentSize <= 1024 && msg->fragmentSize < rst->gv->config.fragment_size)
  {
    /* Spec says fragments must > 1kB; not allowing 1024 bytes is IMHO
       totally ridiculous; and I really don't care how small the
       fragments anyway. And we're certainly not going too fail the
       message if it is as least as large as the configured fragment
       size. */
    return VR_MALFORMED;
  }
  if (msg->fragmentSize == 0 || msg->fragmentStartingNum == 0 || msg->fragmentsInSubmessage == 0)
    return VR_MALFORMED;
  if (msg->fragmentsInSubmessage > UINT32_MAX - msg->fragmentStartingNum)
    return VR_MALFORMED;
  if (DDSI_SC_STRICT_P (rst->gv->config) && msg->fragmentSize >= msg->sampleSize)
    /* may not fragment if not needed -- but I don't care */
    return VR_MALFORMED;
  if ((uint64_t) (msg->fragmentStartingNum + msg->fragmentsInSubmessage - 2) * msg->fragmentSize >= msg->sampleSize)
    /* starting offset of last fragment must be within sample, note:
       fragment numbers are 1-based */
    return VR_MALFORMED;

  sampleinfo->rst = (struct ddsi_receiver_state *) rst; /* drop const */
  if (!ddsi_validating_from_seqno (msg->x.writerSN, &sampleinfo->seq))
    return VR_MALFORMED;
  sampleinfo->fragsize = msg->fragmentSize;
  sampleinfo->size = msg->sampleSize;

  /* QoS and/or payload, so octetsToInlineQos must be within the msg;
     since the serialized data and serialized parameter lists have a 4
     byte header, that one, too must fit */
  if (offsetof (ddsi_rtps_data_datafrag_common_t, octetsToInlineQos) + sizeof (msg->x.octetsToInlineQos) + msg->x.octetsToInlineQos + 4 > size)
    return VR_MALFORMED;

  /* Quick check inline QoS if present, collecting a little bit of
     information on it.  The only way to find the payload offset if
     inline QoSs are present. */
  ptr = (unsigned char *) msg + offsetof (ddsi_rtps_data_datafrag_common_t, octetsToInlineQos) + sizeof (msg->x.octetsToInlineQos) + msg->x.octetsToInlineQos;
  if (msg->x.smhdr.flags & DDSI_DATAFRAG_FLAG_INLINE_QOS)
  {
    ddsi_plist_src_t src;
    src.protocol_version = rst->protocol_version;
    src.vendorid = rst->vendor;
    src.encoding = (msg->x.smhdr.flags & DDSI_RTPS_SUBMESSAGE_FLAG_ENDIANNESS) ? DDSI_RTPS_PL_CDR_LE : DDSI_RTPS_PL_CDR_BE;
    src.buf = ptr;
    src.bufsz = (unsigned) ((unsigned char *) msg + size - src.buf); /* end of message, that's all we know */
    /* just a quick scan, gathering only what we _really_ need */
    if ((ptr = ddsi_plist_quickscan (sampleinfo, keyhashp, &src, rst->gv)) == NULL)
      return VR_MALFORMED;
  }
  else
  {
    sampleinfo->statusinfo = 0;
    sampleinfo->complex_qos = 0;
    *keyhashp = NULL;
  }

  *payloadp = ptr;
  *payloadsz = (uint32_t) ((char *) msg + size - (char *) ptr);
  if ((uint32_t) msg->fragmentsInSubmessage * msg->fragmentSize <= (*payloadsz))
    ; /* all spec'd fragments fit in payload */
  else if ((uint32_t) (msg->fragmentsInSubmessage - 1) * msg->fragmentSize >= (*payloadsz))
    return VR_MALFORMED; /* I can live with a short final fragment, but _only_ the final one */
  else if ((uint32_t) (msg->fragmentStartingNum - 1) * msg->fragmentSize + (*payloadsz) >= msg->sampleSize)
    ; /* final fragment is long enough to cover rest of sample */
  else
    return VR_MALFORMED;
  if (msg->fragmentStartingNum == 1)
  {
    if ((size_t) ((char *) ptr + 4 - (char *) msg) > size)
    {
      /* no space for the header -- technically, allowing small
         fragments would also mean allowing a partial header, but I
         prefer this */
      return VR_MALFORMED;
    }
  }
  enum validation_result vr = validate_writer_and_reader_or_null_entityid (msg->x.writerId, msg->x.readerId);
  if (vr == VR_ACCEPT)
    set_sampleinfo_proxy_writer (sampleinfo, &pwr_guid);
  return vr;
}

int ddsi_add_gap (struct ddsi_xmsg *msg, struct ddsi_writer *wr, struct ddsi_proxy_reader *prd, ddsi_seqno_t start, ddsi_seqno_t base, uint32_t numbits, const uint32_t *bits)
{
  struct ddsi_xmsg_marker sm_marker;
  ddsi_rtps_gap_t *gap;
  ASSERT_MUTEX_HELD (wr->e.lock);
  gap = ddsi_xmsg_append (msg, &sm_marker, DDSI_GAP_SIZE (numbits));
  ddsi_xmsg_submsg_init (msg, sm_marker, DDSI_RTPS_SMID_GAP);
  gap->readerId = ddsi_hton_entityid (prd->e.guid.entityid);
  gap->writerId = ddsi_hton_entityid (wr->e.guid.entityid);
  gap->gapStart = ddsi_to_seqno (start);
  gap->gapList.bitmap_base = ddsi_to_seqno (base);
  gap->gapList.numbits = numbits;
  memcpy (gap->bits, bits, DDSI_SEQUENCE_NUMBER_SET_BITS_SIZE (numbits));
  ddsi_xmsg_submsg_setnext (msg, sm_marker);
  ddsi_security_encode_datawriter_submsg(msg, sm_marker, wr);
  return 0;
}

static ddsi_seqno_t grow_gap_to_next_seq (const struct ddsi_writer *wr, ddsi_seqno_t seq)
{
  ddsi_seqno_t next_seq = ddsi_whc_next_seq (wr->whc, seq - 1);
  ddsi_seqno_t seq_xmit = ddsi_writer_read_seq_xmit (wr);
  if (next_seq == DDSI_MAX_SEQ_NUMBER) /* no next sample */
    return seq_xmit + 1;
  else if (next_seq > seq_xmit)  /* next is beyond last actually transmitted */
    return seq_xmit;
  else /* next one is already visible in the outside world */
    return next_seq;
}

static int acknack_is_nack (const ddsi_rtps_acknack_t *msg)
{
  unsigned x = 0, mask;
  int i;
  if (msg->readerSNState.numbits == 0)
    /* Disallowed by the spec, but RTI appears to require them (and so
       even we generate them) */
    return 0;
  for (i = 0; i < (int) DDSI_SEQUENCE_NUMBER_SET_BITS_SIZE (msg->readerSNState.numbits) / 4 - 1; i++)
    x |= msg->bits[i];
  if ((msg->readerSNState.numbits % 32) == 0)
    mask = ~0u;
  else
    mask = ~(~0u >> (msg->readerSNState.numbits % 32));
  x |= msg->bits[i] & mask;
  return x != 0;
}

static int accept_ack_or_hb_w_timeout (ddsi_count_t new_count, ddsi_count_t *prev_count, ddsrt_etime_t tnow, ddsrt_etime_t *t_last_accepted, int force_accept)
{
  /* AckNacks and Heartbeats with a sequence number (called "count"
     for some reason) equal to or less than the highest one received
     so far must be dropped.  However, we provide an override
     (force_accept) for pre-emptive acks and we accept ones regardless
     of the sequence number after a few seconds.

     This allows continuing after an asymmetrical disconnection if the
     re-connecting side jumps back in its sequence numbering.  DDSI2.1
     8.4.15.7 says: "New HEARTBEATS should have Counts greater than
     all older HEARTBEATs. Then, received HEARTBEATs with Counts not
     greater than any previously received can be ignored."  But it
     isn't clear whether that is about connections or entities.

     The type is defined in the spec as signed but without limiting
     them to, e.g., positive numbers.  Instead of implementing them as
     spec'd, we implement it as unsigned to avoid integer overflow (and
     the consequence undefined behaviour).  Serial number arithmetic
     deals with the wrap-around after 2**31-1.

     Cyclone pre-emptive heartbeats have "count" bitmap_base = 1, NACK
     nothing, have count set to 0.  They're never sent more often than
     once per second, so the 500ms timeout allows them to pass through.

     This combined procedure should give the best of all worlds, and
     is not more expensive in the common case. */
  const int64_t timeout = DDS_MSECS (500);

  if ((int32_t) (new_count - *prev_count) <= 0 && tnow.v - t_last_accepted->v < timeout && !force_accept)
    return 0;

  *prev_count = new_count;
  *t_last_accepted = tnow;
  return 1;
}

void ddsi_gap_info_init(struct ddsi_gap_info *gi)
{
  gi->gapstart = 0;
  gi->gapend = 0;
  gi->gapnumbits = 0;
  memset(gi->gapbits, 0, sizeof(gi->gapbits));
}

void ddsi_gap_info_update(struct ddsi_domaingv *gv, struct ddsi_gap_info *gi, ddsi_seqno_t seqnr)
{
  assert (gi->gapend >= gi->gapstart);
  assert (seqnr >= gi->gapend);

  if (gi->gapstart == 0)
  {
    GVTRACE (" M%"PRIu64, seqnr);
    gi->gapstart = seqnr;
    gi->gapend = gi->gapstart + 1;
  }
  else if (seqnr == gi->gapend)
  {
    GVTRACE (" M%"PRIu64, seqnr);
    gi->gapend = seqnr + 1;
  }
  else if (seqnr - gi->gapend < 256)
  {
    uint32_t idx = (uint32_t) (seqnr - gi->gapend);
    GVTRACE (" M%"PRIu64, seqnr);
    gi->gapnumbits = idx + 1;
    ddsi_bitset_set (gi->gapnumbits, gi->gapbits, idx);
  }
}

struct ddsi_xmsg * ddsi_gap_info_create_gap(struct ddsi_writer *wr, struct ddsi_proxy_reader *prd, struct ddsi_gap_info *gi)
{
  struct ddsi_xmsg *m;

  if (gi->gapstart == 0)
    return NULL;

  m = ddsi_xmsg_new (wr->e.gv->xmsgpool, &wr->e.guid, wr->c.pp, 0, DDSI_XMSG_KIND_CONTROL);

  ddsi_xmsg_setdst_prd (m, prd);
  ddsi_add_gap (m, wr, prd, gi->gapstart, gi->gapend, gi->gapnumbits, gi->gapbits);
  if (ddsi_xmsg_size(m) == 0)
  {
    ddsi_xmsg_free (m);
    m = NULL;
  }
  else
  {
    ETRACE (wr, " FXGAP%"PRIu64"..%"PRIu64"/%"PRIu32":", gi->gapstart, gi->gapend, gi->gapnumbits);
    for (uint32_t i = 0; i < gi->gapnumbits; i++)
      ETRACE (wr, "%c", ddsi_bitset_isset (gi->gapnumbits, gi->gapbits, i) ? '1' : '0');
  }

  return m;
}

struct defer_hb_state {
  struct ddsi_xmsg *m;
  struct ddsi_xeventq *evq;
  int hbansreq;
  uint64_t wr_iid;
  uint64_t prd_iid;
};

static void defer_heartbeat_to_peer (struct ddsi_writer *wr, const struct ddsi_whc_state *whcst, struct ddsi_proxy_reader *prd, int hbansreq, struct defer_hb_state *defer_hb_state)
{
  ETRACE (wr, "defer_heartbeat_to_peer: "PGUIDFMT" -> "PGUIDFMT" - queue for transmit\n", PGUID (wr->e.guid), PGUID (prd->e.guid));

  if (defer_hb_state->m != NULL)
  {
    if (wr->e.iid == defer_hb_state->wr_iid && prd->e.iid == defer_hb_state->prd_iid)
    {
      if (hbansreq <= defer_hb_state->hbansreq)
        return;
      else
        ddsi_xmsg_free (defer_hb_state->m);
    }
    else
    {
      ddsi_qxev_msg (wr->evq, defer_hb_state->m);
    }
  }

  ASSERT_MUTEX_HELD (&wr->e.lock);
  assert (wr->reliable);

  defer_hb_state->m = ddsi_xmsg_new (wr->e.gv->xmsgpool, &wr->e.guid, wr->c.pp, 0, DDSI_XMSG_KIND_CONTROL);
  ddsi_xmsg_setdst_prd (defer_hb_state->m, prd);
  ddsi_add_heartbeat (defer_hb_state->m, wr, whcst, hbansreq, 0, prd->e.guid.entityid, 0);
  defer_hb_state->evq = wr->evq;
  defer_hb_state->hbansreq = hbansreq;
  defer_hb_state->wr_iid = wr->e.iid;
  defer_hb_state->prd_iid = prd->e.iid;
}

static void force_heartbeat_to_peer (struct ddsi_writer *wr, const struct ddsi_whc_state *whcst, struct ddsi_proxy_reader *prd, int hbansreq, struct defer_hb_state *defer_hb_state)
{
  defer_heartbeat_to_peer (wr, whcst, prd, hbansreq, defer_hb_state);
  ddsi_qxev_msg (wr->evq, defer_hb_state->m);
  defer_hb_state->m = NULL;
}

static void defer_hb_state_init (struct defer_hb_state *defer_hb_state)
{
  defer_hb_state->m = NULL;
}

static void defer_hb_state_fini (struct ddsi_domaingv * const gv, struct defer_hb_state *defer_hb_state)
{
  if (defer_hb_state->m)
  {
    GVTRACE ("send_deferred_heartbeat: %"PRIx64" -> %"PRIx64" - queue for transmit\n", defer_hb_state->wr_iid, defer_hb_state->prd_iid);
    ddsi_qxev_msg (defer_hb_state->evq, defer_hb_state->m);
    defer_hb_state->m = NULL;
  }
}

static int handle_AckNack (struct ddsi_receiver_state *rst, ddsrt_etime_t tnow, const ddsi_rtps_acknack_t *msg, ddsrt_wctime_t timestamp, ddsi_rtps_submessage_kind_t prev_smid, struct defer_hb_state *defer_hb_state)
{
  struct ddsi_proxy_reader *prd;
  struct ddsi_wr_prd_match *rn;
  struct ddsi_writer *wr;
  struct ddsi_lease *lease;
  ddsi_guid_t src, dst;
  ddsi_seqno_t seqbase;
  ddsi_seqno_t seq_xmit;
  ddsi_count_t *countp;
  struct ddsi_gap_info gi;
  int accelerate_rexmit = 0;
  int is_pure_ack;
  int is_pure_nonhist_ack;
  int is_preemptive_ack;
  int enqueued;
  unsigned numbits;
  uint32_t msgs_sent, msgs_lost;
  ddsi_seqno_t max_seq_in_reply;
  struct ddsi_whc_node *deferred_free_list = NULL;
  struct ddsi_whc_state whcst;
  int hb_sent_in_response = 0;
  countp = (ddsi_count_t *) ((char *) msg + offsetof (ddsi_rtps_acknack_t, bits) + DDSI_SEQUENCE_NUMBER_SET_BITS_SIZE (msg->readerSNState.numbits));
  src.prefix = rst->src_guid_prefix;
  src.entityid = msg->readerId;
  dst.prefix = rst->dst_guid_prefix;
  dst.entityid = msg->writerId;
  RSTTRACE ("ACKNACK(%s#%"PRId32":%"PRIu64"/%"PRIu32":", msg->smhdr.flags & DDSI_ACKNACK_FLAG_FINAL ? "F" : "",
            *countp, ddsi_from_seqno (msg->readerSNState.bitmap_base), msg->readerSNState.numbits);
  for (uint32_t i = 0; i < msg->readerSNState.numbits; i++)
    RSTTRACE ("%c", ddsi_bitset_isset (msg->readerSNState.numbits, msg->bits, i) ? '1' : '0');
  seqbase = ddsi_from_seqno (msg->readerSNState.bitmap_base);
  assert (seqbase > 0); // documentation, really, to make it obvious that subtracting 1 is ok

  if (!rst->forme)
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT" not-for-me)", PGUID (src), PGUID (dst));
    return 1;
  }

  if ((wr = ddsi_entidx_lookup_writer_guid (rst->gv->entity_index, &dst)) == NULL)
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT"?)", PGUID (src), PGUID (dst));
    return 1;
  }
  /* Always look up the proxy reader -- even though we don't need for
     the normal pure ack steady state. If (a big "if"!) this shows up
     as a significant portion of the time, we can always rewrite it to
     only retrieve it when needed. */
  if ((prd = ddsi_entidx_lookup_proxy_reader_guid (rst->gv->entity_index, &src)) == NULL)
  {
    RSTTRACE (" "PGUIDFMT"? -> "PGUIDFMT")", PGUID (src), PGUID (dst));
    return 1;
  }

  if (!ddsi_security_validate_msg_decoding(&(prd->e), &(prd->c), prd->c.proxypp, rst, prev_smid))
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT" clear submsg from protected src)", PGUID (src), PGUID (dst));
    return 1;
  }

  if ((lease = ddsrt_atomic_ldvoidp (&prd->c.proxypp->minl_auto)) != NULL)
    ddsi_lease_renew (lease, tnow);

  if (!wr->reliable) /* note: reliability can't be changed */
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT" not a reliable writer!)", PGUID (src), PGUID (dst));
    return 1;
  }

  ddsrt_mutex_lock (&wr->e.lock);
  if (wr->test_ignore_acknack)
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT" test_ignore_acknack)", PGUID (src), PGUID (dst));
    goto out;
  }

  if ((rn = ddsrt_avl_lookup (&ddsi_wr_readers_treedef, &wr->readers, &src)) == NULL)
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT" not a connection)", PGUID (src), PGUID (dst));
    goto out;
  }

  /* is_pure_nonhist ack differs from is_pure_ack in that it doesn't
     get set when only historical data is being acked, which is
     relevant to setting "has_replied_to_hb" and "assumed_in_sync". */
  is_pure_ack = !acknack_is_nack (msg);
  is_pure_nonhist_ack = is_pure_ack && seqbase - 1 >= rn->seq;
  is_preemptive_ack = seqbase < 1 || (seqbase == 1 && *countp == 0);

  wr->num_acks_received++;
  if (!is_pure_ack)
  {
    wr->num_nacks_received++;
    rn->rexmit_requests++;
  }

  if (!accept_ack_or_hb_w_timeout (*countp, &rn->prev_acknack, tnow, &rn->t_acknack_accepted, is_preemptive_ack))
  {
    RSTTRACE (" ["PGUIDFMT" -> "PGUIDFMT"])", PGUID (src), PGUID (dst));
    goto out;
  }
  RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT"", PGUID (src), PGUID (dst));

  /* Update latency estimates if we have a timestamp -- won't actually
     work so well if the timestamp can be a left over from some other
     submessage -- but then, it is no more than a quick hack at the
     moment. */
  if (rst->gv->config.meas_hb_to_ack_latency && timestamp.v)
  {
    ddsrt_wctime_t tstamp_now = ddsrt_time_wallclock ();
    ddsi_lat_estim_update (&rn->hb_to_ack_latency, tstamp_now.v - timestamp.v);
    if ((rst->gv->logconfig.c.mask & DDS_LC_TRACE) && tstamp_now.v > rn->hb_to_ack_latency_tlastlog.v + DDS_SECS (10))
    {
      ddsi_lat_estim_log (DDS_LC_TRACE, &rst->gv->logconfig, NULL, &rn->hb_to_ack_latency);
      rn->hb_to_ack_latency_tlastlog = tstamp_now;
    }
  }

  /* First, the ACK part: if the AckNack advances the highest sequence
     number ack'd by the remote reader, update state & try dropping
     some messages */
  if (seqbase - 1 > rn->seq)
  {
    const uint64_t n_ack = (seqbase - 1) - rn->seq;
    rn->seq = seqbase - 1;
    if (rn->seq > wr->seq) {
      /* Prevent a reader from ACKing future samples (is only malicious because we require
         that rn->seq <= wr->seq) */
      rn->seq = wr->seq;
    }
    ddsrt_avl_augment_update (&ddsi_wr_readers_treedef, rn);
    const unsigned n = ddsi_remove_acked_messages (wr, &whcst, &deferred_free_list);
    RSTTRACE (" ACK%"PRIu64" RM%u", n_ack, n);
  }
  else
  {
    /* There's actually no guarantee that we need this information */
    ddsi_whc_get_state(wr->whc, &whcst);
  }

  /* If this reader was marked as "non-responsive" in the past, it's now responding again,
     so update its status */
  if (rn->seq == DDSI_MAX_SEQ_NUMBER && prd->c.xqos->reliability.kind == DDS_RELIABILITY_RELIABLE)
  {
    ddsi_seqno_t oldest_seq;
    oldest_seq = DDSI_WHCST_ISEMPTY(&whcst) ? wr->seq : whcst.max_seq;
    rn->has_replied_to_hb = 1; /* was temporarily cleared to ensure heartbeats went out */
    rn->seq = seqbase - 1;
    if (oldest_seq > rn->seq) {
      /* Prevent a malicious reader from lowering the min. sequence number retained in the WHC. */
      rn->seq = oldest_seq;
    }
    if (rn->seq > wr->seq) {
      /* Prevent a reader from ACKing future samples (is only malicious because we require
         that rn->seq <= wr->seq) */
      rn->seq = wr->seq;
    }
    ddsrt_avl_augment_update (&ddsi_wr_readers_treedef, rn);
    DDS_CLOG (DDS_LC_THROTTLE, &rst->gv->logconfig, "writer "PGUIDFMT" considering reader "PGUIDFMT" responsive again\n", PGUID (wr->e.guid), PGUID (rn->prd_guid));
  }

  /* Second, the NACK bits (literally, that is). To do so, attempt to
     classify the AckNack for reverse-engineered compatibility with
     RTI's invalid acks and sometimes slightly odd behaviour. */
  numbits = msg->readerSNState.numbits;
  msgs_sent = 0;
  msgs_lost = 0;
  max_seq_in_reply = 0;
  if (!rn->has_replied_to_hb && is_pure_nonhist_ack)
  {
    RSTTRACE (" setting-has-replied-to-hb");
    rn->has_replied_to_hb = 1;
    /* walk the whole tree to ensure all proxy readers for this writer
       have their unack'ed info updated */
    ddsrt_avl_augment_update (&ddsi_wr_readers_treedef, rn);
  }
  if (is_preemptive_ack)
  {
    /* Pre-emptive nack: RTI uses (seqbase = 0, numbits = 0), we use
       (seqbase = 1, numbits = 1, bits = {0}).  Seqbase <= 1 and not a
       NACK covers both and is otherwise a useless message.  Sent on
       reader start-up and we respond with a heartbeat and, if we have
       data in our WHC, we start sending it regardless of whether the
       remote reader asked for it */
    RSTTRACE (" preemptive-nack");
    if (DDSI_WHCST_ISEMPTY(&whcst))
    {
      RSTTRACE (" whc-empty ");
      force_heartbeat_to_peer (wr, &whcst, prd, 1, defer_hb_state);
      hb_sent_in_response = 1;
    }
    else
    {
      RSTTRACE (" rebase ");
      force_heartbeat_to_peer (wr, &whcst, prd, 1, defer_hb_state);
      hb_sent_in_response = 1;
      numbits = rst->gv->config.accelerate_rexmit_block_size;
      seqbase = whcst.min_seq;
    }
  }
  else if (!rn->assumed_in_sync)
  {
    /* We assume a remote reader that hasn't ever sent a pure Ack --
       an AckNack that doesn't NACK a thing -- is still trying to
       catch up, so we try to accelerate its attempts at catching up
       by a configurable amount. FIXME: what about a pulling reader?
       that doesn't play too nicely with this. */
    if (is_pure_nonhist_ack)
    {
      RSTTRACE (" happy-now");
      rn->assumed_in_sync = 1;
    }
    else if (msg->readerSNState.numbits < rst->gv->config.accelerate_rexmit_block_size)
    {
      RSTTRACE (" accelerating");
      accelerate_rexmit = 1;
      if (accelerate_rexmit && numbits < rst->gv->config.accelerate_rexmit_block_size)
        numbits = rst->gv->config.accelerate_rexmit_block_size;
    }
    else
    {
      RSTTRACE (" complying");
    }
  }
  /* Retransmit requested messages, including whatever we decided to
     retransmit that the remote reader didn't ask for. While doing so,
     note any gaps in the sequence: if there are some, we transmit a
     Gap message as well.

     Note: ignoring retransmit requests for samples beyond the one we
     last transmitted, even though we may have more available.  If it
     hasn't been transmitted ever, the initial transmit should solve
     that issue; if it has, then the timing is terribly unlucky, but
     a future request'll fix it. */
  if (wr->test_suppress_retransmit && numbits > 0)
  {
    RSTTRACE (" test_suppress_retransmit");
    numbits = 0;
  }
  enqueued = 1;
  seq_xmit = ddsi_writer_read_seq_xmit (wr);
  ddsi_gap_info_init(&gi);
  const bool gap_for_already_acked = ddsi_vendor_is_eclipse (rst->vendor) && prd->c.xqos->durability.kind == DDS_DURABILITY_VOLATILE && seqbase <= rn->seq;
  const ddsi_seqno_t min_seq_to_rexmit = gap_for_already_acked ? rn->seq + 1 : 0;
  uint32_t limit = wr->rexmit_burst_size_limit;
  for (uint32_t i = 0; i < numbits && seqbase + i <= seq_xmit && enqueued && limit > 0; i++)
  {
    /* Accelerated schedule may run ahead of sequence number set
       contained in the acknack, and assumes all messages beyond the
       set are NACK'd -- don't feel like tracking where exactly we
       left off ... */
    if (i >= msg->readerSNState.numbits || ddsi_bitset_isset (numbits, msg->bits, i))
    {
      ddsi_seqno_t seq = seqbase + i;
      struct ddsi_whc_borrowed_sample sample;
      if (seqbase + i >= min_seq_to_rexmit && ddsi_whc_borrow_sample (wr->whc, seq, &sample))
      {
        if (!wr->retransmitting && sample.unacked)
          ddsi_writer_set_retransmitting (wr);

        if (rst->gv->config.retransmit_merging != DDSI_REXMIT_MERGE_NEVER && rn->assumed_in_sync && !prd->filter)
        {
          /* send retransmit to all receivers, but skip if recently done */
          ddsrt_mtime_t tstamp = ddsrt_time_monotonic ();
          if (tstamp.v > sample.last_rexmit_ts.v + rst->gv->config.retransmit_merging_period)
          {
            RSTTRACE (" RX%"PRIu64, seqbase + i);
            enqueued = (ddsi_enqueue_sample_wrlock_held (wr, seq, sample.serdata, NULL, 0) >= 0);
            if (enqueued)
            {
              max_seq_in_reply = seqbase + i;
              msgs_sent++;
              sample.last_rexmit_ts = tstamp;
              // FIXME: now ddsi_enqueue_sample_wrlock_held limits retransmit requests of a large sample to 1 fragment
              // thus we can easily figure out how much was sent, but we shouldn't have that knowledge here:
              // it should return how much it queued instead
              uint32_t sent = ddsi_serdata_size (sample.serdata);
              if (sent > wr->e.gv->config.fragment_size)
                sent = wr->e.gv->config.fragment_size;
              wr->rexmit_bytes += sent;
              limit = (sent > limit) ? 0 : limit - sent;
            }
          }
          else
          {
            RSTTRACE (" RX%"PRIu64" (merged)", seqbase + i);
          }
        }
        else
        {
          /* Is this a volatile reader with a filter?
           * If so, call the filter to see if we should re-arrange the sequence gap when needed. */
          if (prd->filter && !prd->filter (wr, prd, sample.serdata))
            ddsi_gap_info_update (rst->gv, &gi, seqbase + i);
          else
          {
            /* no merging, send directed retransmit */
            RSTTRACE (" RX%"PRIu64"", seqbase + i);
            enqueued = (ddsi_enqueue_sample_wrlock_held (wr, seq, sample.serdata, prd, 0) >= 0);
            if (enqueued)
            {
              max_seq_in_reply = seqbase + i;
              msgs_sent++;
              sample.rexmit_count++;
              // FIXME: now ddsi_enqueue_sample_wrlock_held limits retransmit requests of a large sample to 1 fragment
              // thus we can easily figure out how much was sent, but we shouldn't have that knowledge here:
              // it should return how much it queued instead
              uint32_t sent = ddsi_serdata_size (sample.serdata);
              if (sent > wr->e.gv->config.fragment_size)
                sent = wr->e.gv->config.fragment_size;
              wr->rexmit_bytes += sent;
              limit = (sent > limit) ? 0 : limit - sent;
            }
          }
        }
        ddsi_whc_return_sample(wr->whc, &sample, true);
      }
      else
      {
        ddsi_gap_info_update (rst->gv, &gi, seqbase + i);
        msgs_lost++;
      }
    }
  }

  if (!enqueued)
    RSTTRACE (" rexmit-limit-hit");
  /* Generate a Gap message if some of the sequence is missing */
  if (gi.gapstart > 0)
  {
    struct ddsi_xmsg *gap;

    if (gi.gapend == seqbase + msg->readerSNState.numbits)
      gi.gapend = grow_gap_to_next_seq (wr, gi.gapend);

    if (gi.gapend-1 + gi.gapnumbits > max_seq_in_reply)
      max_seq_in_reply = gi.gapend-1 + gi.gapnumbits;

    gap = ddsi_gap_info_create_gap (wr, prd, &gi);
    if (gap)
    {
      ddsi_qxev_msg (wr->evq, gap);
      msgs_sent++;
    }
  }

  wr->rexmit_count += msgs_sent;
  wr->rexmit_lost_count += msgs_lost;
  if (msgs_sent)
  {
    RSTTRACE (" rexmit#%"PRIu32" maxseq:%"PRIu64"<%"PRIu64"<=%"PRIu64"", msgs_sent, max_seq_in_reply, seq_xmit, wr->seq);

    defer_heartbeat_to_peer (wr, &whcst, prd, 1, defer_hb_state);
    hb_sent_in_response = 1;

    /* The primary purpose of hbcontrol_note_asyncwrite is to ensure
       heartbeats will go out at the "normal" rate again, instead of a
       gradually lowering rate.  If we just got a request for a
       retransmit, and there is more to be retransmitted, surely the
       rate should be kept up for now */
    ddsi_writer_hbcontrol_note_asyncwrite (wr, ddsrt_time_monotonic ());
  }
  /* If "final" flag not set, we must respond with a heartbeat. Do it
     now if we haven't done so already */
  if (!(msg->smhdr.flags & DDSI_ACKNACK_FLAG_FINAL) && !hb_sent_in_response)
  {
    defer_heartbeat_to_peer (wr, &whcst, prd, 0, defer_hb_state);
  }
  RSTTRACE (")");
 out:
  ddsrt_mutex_unlock (&wr->e.lock);
  ddsi_whc_free_deferred_free_list (wr->whc, deferred_free_list);
  return 1;
}

static void handle_forall_destinations (const ddsi_guid_t *dst, struct ddsi_proxy_writer *pwr, ddsrt_avl_walk_t fun, void *arg)
{
  /* prefix:  id:   to:
     0        0     all matched readers
     0        !=0   all matched readers with entityid id
     !=0      0     to all matched readers in addressed participant
     !=0      !=0   to the one addressed reader
  */
  const int haveprefix =
    !(dst->prefix.u[0] == 0 && dst->prefix.u[1] == 0 && dst->prefix.u[2] == 0);
  const int haveid = !(dst->entityid.u == DDSI_ENTITYID_UNKNOWN);

  /* must have pwr->e.lock held for safely iterating over readers */
  ASSERT_MUTEX_HELD (&pwr->e.lock);

  switch ((haveprefix << 1) | haveid)
  {
    case (0 << 1) | 0: /* all: full treewalk */
      ddsrt_avl_walk (&ddsi_pwr_readers_treedef, &pwr->readers, fun, arg);
      break;
    case (0 << 1) | 1: /* all with correct entityid: special filtering treewalk */
      {
        struct ddsi_pwr_rd_match *wn;
        for (wn = ddsrt_avl_find_min (&ddsi_pwr_readers_treedef, &pwr->readers); wn; wn = ddsrt_avl_find_succ (&ddsi_pwr_readers_treedef, &pwr->readers, wn))
        {
          if (wn->rd_guid.entityid.u == dst->entityid.u)
            fun (wn, arg);
        }
      }
      break;
    case (1 << 1) | 0: /* all within one participant: walk a range of keyvalues */
      {
        ddsi_guid_t a, b;
        a = *dst; a.entityid.u = 0;
        b = *dst; b.entityid.u = ~0u;
        ddsrt_avl_walk_range (&ddsi_pwr_readers_treedef, &pwr->readers, &a, &b, fun, arg);
      }
      break;
    case (1 << 1) | 1: /* fully addressed: dst should exist (but for removal) */
      {
        struct ddsi_pwr_rd_match *wn;
        if ((wn = ddsrt_avl_lookup (&ddsi_pwr_readers_treedef, &pwr->readers, dst)) != NULL)
          fun (wn, arg);
      }
      break;
  }
}

struct handle_Heartbeat_helper_arg {
  struct ddsi_receiver_state *rst;
  const ddsi_rtps_heartbeat_t *msg;
  struct ddsi_proxy_writer *pwr;
  ddsrt_wctime_t timestamp;
  ddsrt_etime_t tnow;
  ddsrt_mtime_t tnow_mt;
  bool directed_heartbeat;
};

static void handle_Heartbeat_helper (struct ddsi_pwr_rd_match * const wn, struct handle_Heartbeat_helper_arg * const arg)
{
  struct ddsi_receiver_state * const rst = arg->rst;
  ddsi_rtps_heartbeat_t const * const msg = arg->msg;
  struct ddsi_proxy_writer * const pwr = arg->pwr;

  ASSERT_MUTEX_HELD (&pwr->e.lock);

  if (wn->acknack_xevent == NULL)
  {
    // Ignore best-effort readers
    return;
  }

  if (!accept_ack_or_hb_w_timeout (msg->count, &wn->prev_heartbeat, arg->tnow, &wn->t_heartbeat_accepted, 0))
  {
    RSTTRACE (" ("PGUIDFMT")", PGUID (wn->rd_guid));
    return;
  }

  if (rst->gv->logconfig.c.mask & DDS_LC_TRACE)
  {
    ddsi_seqno_t refseq;
    if (wn->in_sync != PRMSS_OUT_OF_SYNC && !wn->filtered)
      refseq = ddsi_reorder_next_seq (pwr->reorder);
    else
      refseq = ddsi_reorder_next_seq (wn->u.not_in_sync.reorder);
    RSTTRACE (" "PGUIDFMT"@%"PRIu64"%s", PGUID (wn->rd_guid), refseq - 1, (wn->in_sync == PRMSS_SYNC) ? "(sync)" : (wn->in_sync == PRMSS_TLCATCHUP) ? "(tlcatchup)" : "");
  }

  wn->heartbeat_since_ack = 1;
  if (!(msg->smhdr.flags & DDSI_HEARTBEAT_FLAG_FINAL))
    wn->ack_requested = 1;
  if (arg->directed_heartbeat)
    wn->directed_heartbeat = 1;

  ddsi_sched_acknack_if_needed (wn->acknack_xevent, pwr, wn, arg->tnow_mt, true);
}

static int handle_Heartbeat (struct ddsi_receiver_state *rst, ddsrt_etime_t tnow, struct ddsi_rmsg *rmsg, const ddsi_rtps_heartbeat_t *msg, ddsrt_wctime_t timestamp, ddsi_rtps_submessage_kind_t prev_smid)
{
  /* We now cheat: and process the heartbeat for _all_ readers,
     always, regardless of the destination address in the Heartbeat
     sub-message. This is to take care of the samples with sequence
     numbers that become deliverable because of the heartbeat.

     We do play by the book with respect to generating AckNacks in
     response -- done by handle_Heartbeat_helper.

     A heartbeat that states [a,b] is the smallest interval in which
     the range of available sequence numbers is is interpreted here as
     a gap [1,a). See also handle_Gap.  */
  const ddsi_seqno_t firstseq = ddsi_from_seqno (msg->firstSN);
  const ddsi_seqno_t lastseq = ddsi_from_seqno (msg->lastSN);
  struct handle_Heartbeat_helper_arg arg;
  struct ddsi_proxy_writer *pwr;
  struct ddsi_lease *lease;
  ddsi_guid_t src, dst;

  src.prefix = rst->src_guid_prefix;
  src.entityid = msg->writerId;
  dst.prefix = rst->dst_guid_prefix;
  dst.entityid = msg->readerId;

  RSTTRACE ("HEARTBEAT(%s%s#%"PRId32":%"PRIu64"..%"PRIu64" ", msg->smhdr.flags & DDSI_HEARTBEAT_FLAG_FINAL ? "F" : "",
    msg->smhdr.flags & DDSI_HEARTBEAT_FLAG_LIVELINESS ? "L" : "", msg->count, firstseq, lastseq);

  if (!rst->forme)
  {
    RSTTRACE (PGUIDFMT" -> "PGUIDFMT" not-for-me)", PGUID (src), PGUID (dst));
    return 1;
  }

  if ((pwr = ddsi_entidx_lookup_proxy_writer_guid (rst->gv->entity_index, &src)) == NULL)
  {
    RSTTRACE (PGUIDFMT"? -> "PGUIDFMT")", PGUID (src), PGUID (dst));
    return 1;
  }

  if (!ddsi_security_validate_msg_decoding(&(pwr->e), &(pwr->c), pwr->c.proxypp, rst, prev_smid))
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT" clear submsg from protected src)", PGUID (src), PGUID (dst));
    return 1;
  }

  if ((lease = ddsrt_atomic_ldvoidp (&pwr->c.proxypp->minl_auto)) != NULL)
    ddsi_lease_renew (lease, tnow);

  RSTTRACE (PGUIDFMT" -> "PGUIDFMT":", PGUID (src), PGUID (dst));
  ddsrt_mutex_lock (&pwr->e.lock);
  if (msg->smhdr.flags & DDSI_HEARTBEAT_FLAG_LIVELINESS &&
      pwr->c.xqos->liveliness.kind != DDS_LIVELINESS_AUTOMATIC &&
      pwr->c.xqos->liveliness.lease_duration != DDS_INFINITY)
  {
    if ((lease = ddsrt_atomic_ldvoidp (&pwr->c.proxypp->minl_man)) != NULL)
      ddsi_lease_renew (lease, tnow);
    ddsi_lease_renew (pwr->lease, tnow);
  }
  if (pwr->n_reliable_readers == 0)
  {
    RSTTRACE (PGUIDFMT" -> "PGUIDFMT" no-reliable-readers)", PGUID (src), PGUID (dst));
    ddsrt_mutex_unlock (&pwr->e.lock);
    return 1;
  }

  // Inserting a GAP for [1..gap_end_seq) is our way of implementing the processing of
  // a heartbeat that indicates some data we're still waiting for is no longer available.
  // (A no-op GAP is thrown away very quickly.)
  //
  // By definition that means we need gap_end_seq = firstseq, but the first heartbeat has
  // to be treated specially because the spec doesn't define anything for a full handshake
  // establishing a well-defined start point for a reliable session *and* it also defines
  // that one may have a transient-local writer with a volatile reader, and so the last
  // sequence number is the only one that can be used to start up a volatile reader ...
  ddsi_seqno_t gap_end_seq = firstseq;
  if (!pwr->have_seen_heartbeat)
  {
    // Note: if the writer is Cyclone DDS, there will not be any data, for other implementations
    // anything goes.
    gap_end_seq = lastseq + 1;
    // validate_Heartbeat requires that 0 < firstseq <= lastseq+1 (lastseq = firstseq - 1
    // is the encoding for an empty WHC), this matters here because it guarantees changing
    // gap_end_seq doesn't lower sequence number.
    assert (gap_end_seq >= firstseq);
    pwr->have_seen_heartbeat = 1;
  }

  if (lastseq > pwr->last_seq)
  {
    pwr->last_seq = lastseq;
    pwr->last_fragnum = UINT32_MAX;
  }
  else if (pwr->last_fragnum != UINT32_MAX && lastseq == pwr->last_seq)
  {
    pwr->last_fragnum = UINT32_MAX;
  }

  ddsi_defrag_notegap (pwr->defrag, 1, gap_end_seq);

  {
    struct ddsi_rdata *gap;
    struct ddsi_pwr_rd_match *wn;
    struct ddsi_rsample_chain sc;
    int refc_adjust = 0;
    ddsi_reorder_result_t res;
    gap = ddsi_rdata_newgap (rmsg);
    int filtered = 0;

    if (pwr->filtered && !ddsi_is_null_guid(&dst))
    {
      for (wn = ddsrt_avl_find_min (&ddsi_pwr_readers_treedef, &pwr->readers); wn; wn = ddsrt_avl_find_succ (&ddsi_pwr_readers_treedef, &pwr->readers, wn))
      {
        if (ddsi_guid_eq(&wn->rd_guid, &dst))
        {
          if (wn->filtered)
          {
            // Content filtering on reader GUID, and the HEARTBEAT destination GUID is
            // just that one reader, so it makes sense to "trust" the heartbeat and
            // use the advertised first sequence number in the WHC
            struct ddsi_reorder *ro = wn->u.not_in_sync.reorder;
            if ((res = ddsi_reorder_gap (&sc, ro, gap, 1, firstseq, &refc_adjust)) > 0)
              ddsi_dqueue_enqueue1 (pwr->dqueue, &wn->rd_guid, &sc, res);
            if (ddsi_from_seqno (msg->lastSN) > wn->last_seq)
            {
              wn->last_seq = ddsi_from_seqno (msg->lastSN);
            }
            filtered = 1;
          }
          break;
        }
      }
    }

    if (!filtered)
    {
      if ((res = ddsi_reorder_gap (&sc, pwr->reorder, gap, 1, gap_end_seq, &refc_adjust)) > 0)
      {
        if (pwr->deliver_synchronously)
          deliver_user_data_synchronously (&sc, NULL);
        else
          ddsi_dqueue_enqueue (pwr->dqueue, &sc, res);
      }
      for (wn = ddsrt_avl_find_min (&ddsi_pwr_readers_treedef, &pwr->readers); wn; wn = ddsrt_avl_find_succ (&ddsi_pwr_readers_treedef, &pwr->readers, wn))
      {
        if (wn->in_sync == PRMSS_SYNC)
          continue;
        if (wn->u.not_in_sync.end_of_tl_seq == DDSI_MAX_SEQ_NUMBER)
        {
          wn->u.not_in_sync.end_of_tl_seq = ddsi_from_seqno (msg->lastSN);
          RSTTRACE (" end-of-tl-seq(rd "PGUIDFMT" #%"PRIu64")", PGUID(wn->rd_guid), wn->u.not_in_sync.end_of_tl_seq);
        }
        switch (wn->in_sync)
        {
          case PRMSS_SYNC:
            assert(0);
            break;
          case PRMSS_TLCATCHUP:
            assert (ddsi_reorder_next_seq (pwr->reorder) > 0);
            maybe_set_reader_in_sync (pwr, wn, ddsi_reorder_next_seq (pwr->reorder) - 1);
            break;
          case PRMSS_OUT_OF_SYNC: {
            struct ddsi_reorder *ro = wn->u.not_in_sync.reorder;
            // per-reader "out-of-sync" reorder admins need to use firstseq: they are used
            // to retrieve transient-local data, hence fast-forwarding to lastseq would
            // mean they would never need to retrieve any historical data
            if ((res = ddsi_reorder_gap (&sc, ro, gap, 1, firstseq, &refc_adjust)) > 0)
            {
              if (pwr->deliver_synchronously)
                deliver_user_data_synchronously (&sc, &wn->rd_guid);
              else
                ddsi_dqueue_enqueue1 (pwr->dqueue, &wn->rd_guid, &sc, res);
            }
            assert (ddsi_reorder_next_seq (wn->u.not_in_sync.reorder) > 0);
            maybe_set_reader_in_sync (pwr, wn, ddsi_reorder_next_seq (wn->u.not_in_sync.reorder) - 1);
          }
        }
      }
    }
    ddsi_fragchain_adjust_refcount (gap, refc_adjust);
  }

  arg.rst = rst;
  arg.msg = msg;
  arg.pwr = pwr;
  arg.timestamp = timestamp;
  arg.tnow = tnow;
  arg.tnow_mt = ddsrt_time_monotonic ();
  arg.directed_heartbeat = (dst.entityid.u != DDSI_ENTITYID_UNKNOWN && ddsi_vendor_is_eclipse (rst->vendor));
  handle_forall_destinations (&dst, pwr, (ddsrt_avl_walk_t) handle_Heartbeat_helper, &arg);
  RSTTRACE (")");

  ddsrt_mutex_unlock (&pwr->e.lock);
  return 1;
}

static int handle_HeartbeatFrag (struct ddsi_receiver_state *rst, UNUSED_ARG(ddsrt_etime_t tnow), const ddsi_rtps_heartbeatfrag_t *msg, ddsi_rtps_submessage_kind_t prev_smid)
{
  const ddsi_seqno_t seq = ddsi_from_seqno (msg->writerSN);
  const ddsi_fragment_number_t fragnum = msg->lastFragmentNum - 1; /* we do 0-based */
  ddsi_guid_t src, dst;
  struct ddsi_proxy_writer *pwr;
  struct ddsi_lease *lease;

  src.prefix = rst->src_guid_prefix;
  src.entityid = msg->writerId;
  dst.prefix = rst->dst_guid_prefix;
  dst.entityid = msg->readerId;
  const bool directed_heartbeat = (dst.entityid.u != DDSI_ENTITYID_UNKNOWN && ddsi_vendor_is_eclipse (rst->vendor));

  RSTTRACE ("HEARTBEATFRAG(#%"PRId32":%"PRIu64"/[1,%"PRIu32"]", msg->count, seq, fragnum+1);
  if (!rst->forme)
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT" not-for-me)", PGUID (src), PGUID (dst));
    return 1;
  }

  if ((pwr = ddsi_entidx_lookup_proxy_writer_guid (rst->gv->entity_index, &src)) == NULL)
  {
    RSTTRACE (" "PGUIDFMT"? -> "PGUIDFMT")", PGUID (src), PGUID (dst));
    return 1;
  }

  if (!ddsi_security_validate_msg_decoding(&(pwr->e), &(pwr->c), pwr->c.proxypp, rst, prev_smid))
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT" clear submsg from protected src)", PGUID (src), PGUID (dst));
    return 1;
  }

  if ((lease = ddsrt_atomic_ldvoidp (&pwr->c.proxypp->minl_auto)) != NULL)
    ddsi_lease_renew (lease, tnow);

  RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT"", PGUID (src), PGUID (dst));
  ddsrt_mutex_lock (&pwr->e.lock);

  if (seq > pwr->last_seq)
  {
    pwr->last_seq = seq;
    pwr->last_fragnum = fragnum;
  }
  else if (seq == pwr->last_seq && fragnum > pwr->last_fragnum)
  {
    pwr->last_fragnum = fragnum;
  }

  if (!pwr->have_seen_heartbeat)
  {
    ddsrt_mutex_unlock(&pwr->e.lock);
    return 1;
  }

  /* Defragmenting happens at the proxy writer, readers have nothing
     to do with it.  Here we immediately respond with a NackFrag if we
     discover a missing fragment, which differs significantly from
     handle_Heartbeat's scheduling of an AckNack event when it must
     respond.  Why?  Just because. */
  if (ddsrt_avl_is_empty (&pwr->readers) || pwr->local_matching_inprogress)
    RSTTRACE (" no readers");
  else
  {
    struct ddsi_pwr_rd_match *m = NULL;

    if (ddsi_reorder_wantsample (pwr->reorder, seq))
    {
      if (directed_heartbeat)
      {
        /* Cyclone currently only ever sends a HEARTBEAT(FRAG) with the
           destination entity id set AFTER retransmitting any samples
           that reader requested.  So it makes sense to only interpret
           those for that reader, and to suppress the NackDelay in a
           response to it.  But it better be a reliable reader! */
        m = ddsrt_avl_lookup (&ddsi_pwr_readers_treedef, &pwr->readers, &dst);
        if (m && m->acknack_xevent == NULL)
          m = NULL;
      }
      else
      {
        /* Pick an arbitrary reliable reader's guid for the response --
           assuming a reliable writer -> unreliable reader is rare, and
           so scanning the readers is acceptable if the first guess
           fails */
        m = ddsrt_avl_root_non_empty (&ddsi_pwr_readers_treedef, &pwr->readers);
        if (m->acknack_xevent == NULL)
        {
          m = ddsrt_avl_find_min (&ddsi_pwr_readers_treedef, &pwr->readers);
          while (m && m->acknack_xevent == NULL)
            m = ddsrt_avl_find_succ (&ddsi_pwr_readers_treedef, &pwr->readers, m);
        }
      }
    }
    else if (seq < ddsi_reorder_next_seq (pwr->reorder))
    {
      if (directed_heartbeat)
      {
        m = ddsrt_avl_lookup (&ddsi_pwr_readers_treedef, &pwr->readers, &dst);
        if (m && !(m->in_sync == PRMSS_OUT_OF_SYNC && m->acknack_xevent != NULL && ddsi_reorder_wantsample (m->u.not_in_sync.reorder, seq)))
        {
          /* Ignore if reader is happy or not best-effort */
          m = NULL;
        }
      }
      else
      {
        /* Check out-of-sync readers -- should add a bit to cheaply test
         whether there are any (usually there aren't) */
        m = ddsrt_avl_find_min (&ddsi_pwr_readers_treedef, &pwr->readers);
        while (m)
        {
          if (m->in_sync == PRMSS_OUT_OF_SYNC && m->acknack_xevent != NULL && ddsi_reorder_wantsample (m->u.not_in_sync.reorder, seq))
          {
            /* If reader is out-of-sync, and reader is realiable, and
             reader still wants this particular sample, then use this
             reader to decide which fragments to nack */
            break;
          }
          m = ddsrt_avl_find_succ (&ddsi_pwr_readers_treedef, &pwr->readers, m);
        }
      }
    }

    if (m == NULL)
      RSTTRACE (" no interested reliable readers");
    else
    {
      if (directed_heartbeat)
        m->directed_heartbeat = 1;
      m->heartbeatfrag_since_ack = 1;

      DDSRT_STATIC_ASSERT ((DDSI_FRAGMENT_NUMBER_SET_MAX_BITS % 32) == 0);
      struct {
        struct ddsi_fragment_number_set_header set;
        uint32_t bits[DDSI_FRAGMENT_NUMBER_SET_MAX_BITS / 32];
      } nackfrag;
      const ddsi_seqno_t last_seq = m->filtered ? m->last_seq : pwr->last_seq;
      if (seq == last_seq && ddsi_defrag_nackmap (pwr->defrag, seq, fragnum, &nackfrag.set, nackfrag.bits, DDSI_FRAGMENT_NUMBER_SET_MAX_BITS) == DDSI_DEFRAG_NACKMAP_FRAGMENTS_MISSING)
      {
        // don't rush it ...
        ddsi_resched_xevent_if_earlier (m->acknack_xevent, ddsrt_mtime_add_duration (ddsrt_time_monotonic (), pwr->e.gv->config.nack_delay));
      }
    }
  }
  RSTTRACE (")");
  ddsrt_mutex_unlock (&pwr->e.lock);
  return 1;
}

static int handle_NackFrag (struct ddsi_receiver_state *rst, ddsrt_etime_t tnow, const ddsi_rtps_nackfrag_t *msg, ddsi_rtps_submessage_kind_t prev_smid, struct defer_hb_state *defer_hb_state)
{
  struct ddsi_proxy_reader *prd;
  struct ddsi_wr_prd_match *rn;
  struct ddsi_writer *wr;
  struct ddsi_lease *lease;
  struct ddsi_whc_borrowed_sample sample;
  ddsi_guid_t src, dst;
  ddsi_count_t *countp;
  ddsi_seqno_t seq = ddsi_from_seqno (msg->writerSN);

  countp = (ddsi_count_t *) ((char *) msg + offsetof (ddsi_rtps_nackfrag_t, bits) + DDSI_FRAGMENT_NUMBER_SET_BITS_SIZE (msg->fragmentNumberState.numbits));
  src.prefix = rst->src_guid_prefix;
  src.entityid = msg->readerId;
  dst.prefix = rst->dst_guid_prefix;
  dst.entityid = msg->writerId;

  RSTTRACE ("NACKFRAG(#%"PRId32":%"PRIu64"/%"PRIu32"/%"PRIu32":", *countp, seq, msg->fragmentNumberState.bitmap_base, msg->fragmentNumberState.numbits);
  for (uint32_t i = 0; i < msg->fragmentNumberState.numbits; i++)
    RSTTRACE ("%c", ddsi_bitset_isset (msg->fragmentNumberState.numbits, msg->bits, i) ? '1' : '0');

  if (!rst->forme)
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT" not-for-me)", PGUID (src), PGUID (dst));
    return 1;
  }

  if ((wr = ddsi_entidx_lookup_writer_guid (rst->gv->entity_index, &dst)) == NULL)
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT"?)", PGUID (src), PGUID (dst));
    return 1;
  }
  /* Always look up the proxy reader -- even though we don't need for
     the normal pure ack steady state. If (a big "if"!) this shows up
     as a significant portion of the time, we can always rewrite it to
     only retrieve it when needed. */
  if ((prd = ddsi_entidx_lookup_proxy_reader_guid (rst->gv->entity_index, &src)) == NULL)
  {
    RSTTRACE (" "PGUIDFMT"? -> "PGUIDFMT")", PGUID (src), PGUID (dst));
    return 1;
  }

  if (!ddsi_security_validate_msg_decoding(&(prd->e), &(prd->c), prd->c.proxypp, rst, prev_smid))
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT" clear submsg from protected src)", PGUID (src), PGUID (dst));
    return 1;
  }

  if ((lease = ddsrt_atomic_ldvoidp (&prd->c.proxypp->minl_auto)) != NULL)
    ddsi_lease_renew (lease, tnow);

  if (!wr->reliable) /* note: reliability can't be changed */
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT" not a reliable writer)", PGUID (src), PGUID (dst));
    return 1;
  }

  ddsrt_mutex_lock (&wr->e.lock);
  if ((rn = ddsrt_avl_lookup (&ddsi_wr_readers_treedef, &wr->readers, &src)) == NULL)
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT" not a connection", PGUID (src), PGUID (dst));
    goto out;
  }

  /* Ignore old NackFrags (see also handle_AckNack) */
  if (!accept_ack_or_hb_w_timeout (*countp, &rn->prev_nackfrag, tnow, &rn->t_nackfrag_accepted, false))
  {
    RSTTRACE (" ["PGUIDFMT" -> "PGUIDFMT"]", PGUID (src), PGUID (dst));
    goto out;
  }
  RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT"", PGUID (src), PGUID (dst));

  /* Resend the requested fragments if we still have the sample, send
     a Gap if we don't have them anymore. */
  if (ddsi_whc_borrow_sample (wr->whc, seq, &sample))
  {
    const uint32_t base = msg->fragmentNumberState.bitmap_base - 1;
    assert (wr->rexmit_burst_size_limit <= UINT32_MAX - UINT16_MAX);
    uint32_t nfrags_lim = (wr->rexmit_burst_size_limit + wr->e.gv->config.fragment_size - 1) / wr->e.gv->config.fragment_size;
    bool sent = false;
    RSTTRACE (" scheduling requested frags ...\n");
    for (uint32_t i = 0; i < msg->fragmentNumberState.numbits && nfrags_lim > 0; i++)
    {
      if (ddsi_bitset_isset (msg->fragmentNumberState.numbits, msg->bits, i))
      {
        struct ddsi_xmsg *reply;
        if (ddsi_create_fragment_message (wr, seq, sample.serdata, base + i, 1, prd, &reply, 0, 0) < 0)
          nfrags_lim = 0;
        else if (ddsi_qxev_msg_rexmit_wrlock_held (wr->evq, reply, 0) == DDSI_QXEV_MSG_REXMIT_DROPPED)
          nfrags_lim = 0;
        else
        {
          sent = true;
          nfrags_lim--;
          wr->rexmit_bytes += wr->e.gv->config.fragment_size;
        }
      }
    }
    if (sent && sample.unacked)
    {
      if (!wr->retransmitting)
        ddsi_writer_set_retransmitting (wr);
    }
    ddsi_whc_return_sample (wr->whc, &sample, false);
  }
  else
  {
    static uint32_t zero = 0;
    struct ddsi_xmsg *m;
    RSTTRACE (" msg not available: scheduling Gap\n");
    m = ddsi_xmsg_new (rst->gv->xmsgpool, &wr->e.guid, wr->c.pp, 0, DDSI_XMSG_KIND_CONTROL);
    ddsi_xmsg_setdst_prd (m, prd);
    /* length-1 bitmap with the bit clear avoids the illegal case of a length-0 bitmap */
    ddsi_add_gap (m, wr, prd, seq, seq+1, 0, &zero);
    ddsi_qxev_msg (wr->evq, m);
  }
  if (seq <= ddsi_writer_read_seq_xmit (wr))
  {
    /* Not everything was retransmitted yet, so force a heartbeat out
       to give the reader a chance to nack the rest and make sure
       hearbeats will go out at a reasonably high rate for a while */
    struct ddsi_whc_state whcst;
    ddsi_whc_get_state(wr->whc, &whcst);
    defer_heartbeat_to_peer (wr, &whcst, prd, 1, defer_hb_state);
    ddsi_writer_hbcontrol_note_asyncwrite (wr, ddsrt_time_monotonic ());
  }

 out:
  ddsrt_mutex_unlock (&wr->e.lock);
  RSTTRACE (")");
  return 1;
}

static int handle_InfoDST (struct ddsi_receiver_state *rst, const ddsi_rtps_info_dst_t *msg, const ddsi_guid_prefix_t *dst_prefix)
{
  rst->dst_guid_prefix = ddsi_ntoh_guid_prefix (msg->guid_prefix);
  RSTTRACE ("INFODST(%"PRIx32":%"PRIx32":%"PRIx32")", PGUIDPREFIX (rst->dst_guid_prefix));
  if (rst->dst_guid_prefix.u[0] == 0 && rst->dst_guid_prefix.u[1] == 0 && rst->dst_guid_prefix.u[2] == 0)
  {
    if (dst_prefix)
      rst->dst_guid_prefix = *dst_prefix;
    rst->forme = 1;
  }
  else
  {
    ddsi_guid_t dst;
    dst.prefix = rst->dst_guid_prefix;
    dst.entityid = ddsi_to_entityid(DDSI_ENTITYID_PARTICIPANT);
    rst->forme = (ddsi_entidx_lookup_participant_guid (rst->gv->entity_index, &dst) != NULL ||
                  ddsi_is_deleted_participant_guid (rst->gv->deleted_participants, &dst, DDSI_DELETED_PPGUID_LOCAL));
  }
  return 1;
}

static int handle_InfoSRC (struct ddsi_receiver_state *rst, const ddsi_rtps_info_src_t *msg)
{
  rst->src_guid_prefix = ddsi_ntoh_guid_prefix (msg->guid_prefix);
  rst->protocol_version = msg->version;
  rst->vendor = msg->vendorid;
  RSTTRACE ("INFOSRC(%"PRIx32":%"PRIx32":%"PRIx32" vendor %u.%u)",
          PGUIDPREFIX (rst->src_guid_prefix), rst->vendor.id[0], rst->vendor.id[1]);
  return 1;
}

static int handle_InfoTS (const struct ddsi_receiver_state *rst, const ddsi_rtps_info_ts_t *msg, ddsrt_wctime_t *timestamp)
{
  RSTTRACE ("INFOTS(");
  if (msg->smhdr.flags & DDSI_INFOTS_INVALIDATE_FLAG)
  {
    *timestamp = DDSRT_WCTIME_INVALID;
    RSTTRACE ("invalidate");
  }
  else
  {
    *timestamp = ddsi_wctime_from_ddsi_time (msg->time);
    if (rst->gv->logconfig.c.mask & DDS_LC_TRACE)
      RSTTRACE ("%d.%09d", (int) (timestamp->v / 1000000000), (int) (timestamp->v % 1000000000));
  }
  RSTTRACE (")");
  return 1;
}

static int handle_one_gap (struct ddsi_proxy_writer *pwr, struct ddsi_pwr_rd_match *wn, ddsi_seqno_t a, ddsi_seqno_t b, struct ddsi_rdata *gap, int *refc_adjust)
{
  struct ddsi_rsample_chain sc;
  ddsi_reorder_result_t res = 0;
  int gap_was_valuable = 0;
  ASSERT_MUTEX_HELD (&pwr->e.lock);
  assert (a > 0 && b >= a);

  /* Clean up the defrag admin: no fragments of a missing sample will
     be arriving in the future */
  if (!(wn && wn->filtered))
  {
    ddsi_defrag_notegap (pwr->defrag, a, b);

    /* Primary reorder: the gap message may cause some samples to become
     deliverable. */

    if ((res = ddsi_reorder_gap (&sc, pwr->reorder, gap, a, b, refc_adjust)) > 0)
    {
      if (pwr->deliver_synchronously)
        deliver_user_data_synchronously (&sc, NULL);
      else
        ddsi_dqueue_enqueue (pwr->dqueue, &sc, res);
    }
  }

  /* If the result was REJECT or TOO_OLD, then this gap didn't add
     anything useful, or there was insufficient memory to store it.
     When the result is either ACCEPT or a sample chain, it clearly
     meant something. */
  DDSRT_STATIC_ASSERT_CODE (DDSI_REORDER_ACCEPT == 0);
  if (res >= 0)
    gap_was_valuable = 1;

  /* Out-of-sync readers never deal with samples with a sequence
     number beyond end_of_tl_seq -- and so it needn't be bothered
     with gaps that start beyond that number */
  if (wn != NULL && wn->in_sync != PRMSS_SYNC)
  {
    switch (wn->in_sync)
    {
      case PRMSS_SYNC:
        assert(0);
        break;
      case PRMSS_TLCATCHUP:
        break;
      case PRMSS_OUT_OF_SYNC:
        if ((res = ddsi_reorder_gap (&sc, wn->u.not_in_sync.reorder, gap, a, b, refc_adjust)) > 0)
        {
          if (pwr->deliver_synchronously)
            deliver_user_data_synchronously (&sc, &wn->rd_guid);
          else
            ddsi_dqueue_enqueue1 (pwr->dqueue, &wn->rd_guid, &sc, res);
        }
        if (res >= 0)
          gap_was_valuable = 1;
        break;
    }

    /* Upon receipt of data a reader can only become in-sync if there
       is something to deliver; for missing data, you just don't know.
       The return value of reorder_gap _is_ sufficiently precise, but
       why not simply check?  It isn't a very expensive test. */
    maybe_set_reader_in_sync (pwr, wn, b-1);
  }

  return gap_was_valuable;
}

static int handle_Gap (struct ddsi_receiver_state *rst, ddsrt_etime_t tnow, struct ddsi_rmsg *rmsg, const ddsi_rtps_gap_t *msg, ddsi_rtps_submessage_kind_t prev_smid)
{
  /* Option 1: Process the Gap for the proxy writer and all
     out-of-sync readers: what do I care which reader is being
     addressed?  Either the sample can still be reproduced by the
     writer, or it can't be anymore.

     Option 2: Process the Gap for the proxy writer and for the
     addressed reader if it happens to be out-of-sync.

     Obviously, both options differ from the specification, but we
     don't have much choice: there is no way of addressing just a
     single in-sync reader, and if that's impossible than we might as
     well ignore the destination completely.

     Option 1 can be fairly expensive if there are many readers, so we
     do option 2. */

  struct ddsi_proxy_writer *pwr;
  struct ddsi_pwr_rd_match *wn;
  struct ddsi_lease *lease;
  ddsi_guid_t src, dst;
  ddsi_seqno_t gapstart, listbase;
  uint32_t first_excluded_rel;
  uint32_t listidx;

  src.prefix = rst->src_guid_prefix;
  src.entityid = msg->writerId;
  dst.prefix = rst->dst_guid_prefix;
  dst.entityid = msg->readerId;
  gapstart = ddsi_from_seqno (msg->gapStart);
  listbase = ddsi_from_seqno (msg->gapList.bitmap_base);
  RSTTRACE ("GAP(%"PRIu64"..%"PRIu64"/%"PRIu32" ", gapstart, listbase, msg->gapList.numbits);

  // valid_Gap guarantees this, but as we are doing sequence number
  // calculations it doesn't hurt to document it here again
  assert (listbase >= gapstart && gapstart >= 1);

  /* There is no _good_ reason for a writer to start the bitmap with a
     1 bit, but check for it just in case, to reduce the number of
     sequence number gaps to be processed. */
  for (listidx = 0; listidx < msg->gapList.numbits; listidx++)
    if (!ddsi_bitset_isset (msg->gapList.numbits, msg->bits, listidx))
      break;
  first_excluded_rel = listidx;

  if (!rst->forme)
  {
    RSTTRACE (""PGUIDFMT" -> "PGUIDFMT" not-for-me)", PGUID (src), PGUID (dst));
    return 1;
  }

  if ((pwr = ddsi_entidx_lookup_proxy_writer_guid (rst->gv->entity_index, &src)) == NULL)
  {
    RSTTRACE (""PGUIDFMT"? -> "PGUIDFMT")", PGUID (src), PGUID (dst));
    return 1;
  }

  if (!ddsi_security_validate_msg_decoding(&(pwr->e), &(pwr->c), pwr->c.proxypp, rst, prev_smid))
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT" clear submsg from protected src)", PGUID (src), PGUID (dst));
    return 1;
  }

  if ((lease = ddsrt_atomic_ldvoidp (&pwr->c.proxypp->minl_auto)) != NULL)
    ddsi_lease_renew (lease, tnow);

  ddsrt_mutex_lock (&pwr->e.lock);
  if ((wn = ddsrt_avl_lookup (&ddsi_pwr_readers_treedef, &pwr->readers, &dst)) == NULL)
  {
    RSTTRACE (PGUIDFMT" -> "PGUIDFMT" not a connection)", PGUID (src), PGUID (dst));
    ddsrt_mutex_unlock (&pwr->e.lock);
    return 1;
  }
  RSTTRACE (PGUIDFMT" -> "PGUIDFMT, PGUID (src), PGUID (dst));

  if (!pwr->have_seen_heartbeat && pwr->n_reliable_readers > 0 && ddsi_vendor_is_eclipse (rst->vendor))
  {
    RSTTRACE (": no heartbeat seen yet");
    ddsrt_mutex_unlock (&pwr->e.lock);
    return 1;
  }

  /* Notify reordering in proxy writer & and the addressed reader (if
     it is out-of-sync, &c.), while delivering samples that become
     available because preceding ones are now known to be missing. */
  {
    int refc_adjust = 0;
    struct ddsi_rdata *gap;
    gap = ddsi_rdata_newgap (rmsg);
    if (gapstart < listbase + listidx)
    {
      /* sanity check on sequence numbers because a GAP message is not invalid even
         if start >= listbase (DDSI 2.1 sect 8.3.7.4.3), but only handle non-empty
         intervals */
      (void) handle_one_gap (pwr, wn, gapstart, listbase + listidx, gap, &refc_adjust);
    }
    while (listidx < msg->gapList.numbits)
    {
      if (!ddsi_bitset_isset (msg->gapList.numbits, msg->bits, listidx))
        listidx++;
      else
      {
        uint32_t j;
        for (j = listidx + 1; j < msg->gapList.numbits; j++)
          if (!ddsi_bitset_isset (msg->gapList.numbits, msg->bits, j))
            break;
        /* spec says gapList (2) identifies an additional list of sequence numbers that
           are invalid (8.3.7.4.2), so by that rule an insane start would simply mean the
           initial interval is to be ignored and the bitmap to be applied */
        (void) handle_one_gap (pwr, wn, listbase + listidx, listbase + j, gap, &refc_adjust);
        assert(j >= 1);
        first_excluded_rel = j;
        listidx = j;
      }
    }
    ddsi_fragchain_adjust_refcount (gap, refc_adjust);
  }

  /* If the last sequence number explicitly included in the set is
     beyond the last sequence number we know exists, update the
     latter.  Note that a sequence number _not_ included in the set
     doesn't tell us anything (which is something that RTI apparently
     got wrong in its interpetation of pure acks that do include a
     bitmap).  */
  const ddsi_seqno_t lastseq = { listbase + first_excluded_rel - 1 };
  if (lastseq > pwr->last_seq)
  {
    pwr->last_seq = lastseq;
    pwr->last_fragnum = UINT32_MAX;
  }

  if (wn && wn->filtered)
  {
    if (lastseq > wn->last_seq)
      wn->last_seq = lastseq;
  }
  RSTTRACE (")");
  ddsrt_mutex_unlock (&pwr->e.lock);
  return 1;
}

static struct ddsi_serdata *get_serdata (struct ddsi_sertype const * const type, const struct ddsi_rdata *fragchain, uint32_t sz, int justkey, unsigned statusinfo, ddsrt_wctime_t tstamp)
{
  struct ddsi_serdata *sd = ddsi_serdata_from_ser (type, justkey ? SDK_KEY : SDK_DATA, fragchain, sz);
  if (sd)
  {
    sd->statusinfo = statusinfo;
    sd->timestamp = tstamp;
  }
  return sd;
}

struct remote_sourceinfo {
  const struct ddsi_rsample_info *sampleinfo;
  unsigned char data_smhdr_flags;
  const ddsi_plist_t *qos;
  const struct ddsi_rdata *fragchain;
  unsigned statusinfo;
  ddsrt_wctime_t tstamp;
};

static struct ddsi_serdata *remote_make_sample (struct ddsi_tkmap_instance **tk, struct ddsi_domaingv *gv, struct ddsi_sertype const * const type, void *vsourceinfo)
{
  /* hopefully the compiler figures out that these are just aliases and doesn't reload them
     unnecessarily from memory */
  const struct remote_sourceinfo * __restrict si = vsourceinfo;
  const struct ddsi_rsample_info * __restrict sampleinfo = si->sampleinfo;
  const struct ddsi_rdata * __restrict fragchain = si->fragchain;
  const uint32_t statusinfo = si->statusinfo;
  const unsigned char data_smhdr_flags = si->data_smhdr_flags;
  const ddsrt_wctime_t tstamp = si->tstamp;
  const ddsi_plist_t * __restrict qos = si->qos;
  const char *failmsg = NULL;
  struct ddsi_serdata *sample = NULL;

  if (si->statusinfo == 0)
  {
    /* normal write */
    if (!(data_smhdr_flags & DDSI_DATA_FLAG_DATAFLAG) || sampleinfo->size == 0)
    {
      const struct ddsi_proxy_writer *pwr = sampleinfo->pwr;
      ddsi_guid_t guid;
      /* pwr can't currently be null, but that might change some day, and this being
         an error path, it doesn't hurt to survive that */
      if (pwr) guid = pwr->e.guid; else memset (&guid, 0, sizeof (guid));
      DDS_CTRACE (&gv->logconfig,
                  "data(application, vendor %u.%u): "PGUIDFMT" #%"PRIu64": write without proper payload (data_smhdr_flags 0x%x size %"PRIu32")\n",
                  sampleinfo->rst->vendor.id[0], sampleinfo->rst->vendor.id[1],
                  PGUID (guid), sampleinfo->seq,
                  si->data_smhdr_flags, sampleinfo->size);
      return NULL;
    }
    sample = get_serdata (type, fragchain, sampleinfo->size, 0, statusinfo, tstamp);
  }
  else if (sampleinfo->size)
  {
    /* dispose or unregister with included serialized key or data
       (data is a Adlink extension) -- i.e., dispose or unregister
       as one would expect to receive */
    if (data_smhdr_flags & DDSI_DATA_FLAG_KEYFLAG)
    {
      sample = get_serdata (type, fragchain, sampleinfo->size, 1, statusinfo, tstamp);
    }
    else
    {
      assert (data_smhdr_flags & DDSI_DATA_FLAG_DATAFLAG);
      sample = get_serdata (type, fragchain, sampleinfo->size, 0, statusinfo, tstamp);
    }
  }
  else if (data_smhdr_flags & DDSI_DATA_FLAG_INLINE_QOS)
  {
    /* RTI always tries to make us survive on the keyhash. RTI must
       mend its ways. */
    if (DDSI_SC_STRICT_P (gv->config))
      failmsg = "no content";
    else if (!(qos->present & PP_KEYHASH))
      failmsg = "qos present but without keyhash";
    else if (ddsi_omg_plist_keyhash_is_protected (qos))
    {
      /* If the keyhash is protected, then it is forced to be an actual MD5
       * hash. This means the keyhash can't be decoded into a sample. */
      failmsg = "keyhash is protected";
    }
    else if ((sample = ddsi_serdata_from_keyhash (type, &qos->keyhash)) == NULL)
      failmsg = "keyhash is MD5 and can't be converted to key value";
    else
    {
      sample->statusinfo = statusinfo;
      sample->timestamp = tstamp;
    }
  }
  else
  {
    failmsg = "no content whatsoever";
  }
  if (sample == NULL)
  {
    /* No message => error out */
    const struct ddsi_proxy_writer *pwr = sampleinfo->pwr;
    ddsi_guid_t guid;
    if (pwr) guid = pwr->e.guid; else memset (&guid, 0, sizeof (guid));
    DDS_CWARNING (&gv->logconfig,
                  "data(application, vendor %u.%u): "PGUIDFMT" #%"PRIu64": deserialization %s/%s failed (%s)\n",
                  sampleinfo->rst->vendor.id[0], sampleinfo->rst->vendor.id[1],
                  PGUID (guid), sampleinfo->seq,
                  pwr && (pwr->c.xqos->present & DDSI_QP_TOPIC_NAME) ? pwr->c.xqos->topic_name : "", type->type_name,
                  failmsg ? failmsg : "for reasons unknown");
  }
  else
  {
    if ((*tk = ddsi_tkmap_lookup_instance_ref (gv->m_tkmap, sample)) == NULL)
    {
      ddsi_serdata_unref (sample);
      sample = NULL;
    }
    else if (gv->logconfig.c.mask & DDS_LC_TRACE)
    {
      const struct ddsi_proxy_writer *pwr = sampleinfo->pwr;
      ddsi_guid_t guid;
      char tmp[1024];
      size_t res = 0;
      tmp[0] = 0;
      if (gv->logconfig.c.mask & DDS_LC_CONTENT)
        res = ddsi_serdata_print (sample, tmp, sizeof (tmp));
      if (pwr) guid = pwr->e.guid; else memset (&guid, 0, sizeof (guid));
      GVTRACE ("data(application, vendor %u.%u): "PGUIDFMT" #%"PRIu64": ST%"PRIx32" %s/%s:%s%s",
               sampleinfo->rst->vendor.id[0], sampleinfo->rst->vendor.id[1],
               PGUID (guid), sampleinfo->seq, statusinfo,
               pwr && (pwr->c.xqos->present & DDSI_QP_TOPIC_NAME) ? pwr->c.xqos->topic_name : "", type->type_name,
               tmp, res < sizeof (tmp) - 1 ? "" : "(trunc)");
    }
  }
  return sample;
}

unsigned char ddsi_normalize_data_datafrag_flags (const ddsi_rtps_submessage_header_t *smhdr)
{
  switch ((ddsi_rtps_submessage_kind_t) smhdr->submessageId)
  {
    case DDSI_RTPS_SMID_DATA:
      return smhdr->flags;
    case DDSI_RTPS_SMID_DATA_FRAG:
      {
        unsigned char common = smhdr->flags & DDSI_DATA_FLAG_INLINE_QOS;
        DDSRT_STATIC_ASSERT_CODE (DDSI_DATA_FLAG_INLINE_QOS == DDSI_DATAFRAG_FLAG_INLINE_QOS);
        if (smhdr->flags & DDSI_DATAFRAG_FLAG_KEYFLAG)
          return common | DDSI_DATA_FLAG_KEYFLAG;
        else
          return common | DDSI_DATA_FLAG_DATAFLAG;
      }
    default:
      assert (0);
      return 0;
  }
}

static struct ddsi_reader *proxy_writer_first_in_sync_reader (struct ddsi_entity_index *entity_index, struct ddsi_entity_common *pwrcmn, ddsrt_avl_iter_t *it)
{
  assert (pwrcmn->kind == DDSI_EK_PROXY_WRITER);
  struct ddsi_proxy_writer *pwr = (struct ddsi_proxy_writer *) pwrcmn;
  struct ddsi_pwr_rd_match *m;
  struct ddsi_reader *rd;
  for (m = ddsrt_avl_iter_first (&ddsi_pwr_readers_treedef, &pwr->readers, it); m != NULL; m = ddsrt_avl_iter_next (it))
    if (m->in_sync == PRMSS_SYNC && (rd = ddsi_entidx_lookup_reader_guid (entity_index, &m->rd_guid)) != NULL)
      return rd;
  return NULL;
}

static struct ddsi_reader *proxy_writer_next_in_sync_reader (struct ddsi_entity_index *entity_index, ddsrt_avl_iter_t *it)
{
  struct ddsi_pwr_rd_match *m;
  struct ddsi_reader *rd;
  for (m = ddsrt_avl_iter_next (it); m != NULL; m = ddsrt_avl_iter_next (it))
    if (m->in_sync == PRMSS_SYNC && (rd = ddsi_entidx_lookup_reader_guid (entity_index, &m->rd_guid)) != NULL)
      return rd;
  return NULL;
}

static dds_return_t remote_on_delivery_failure_fastpath (struct ddsi_entity_common *source_entity, bool source_entity_locked, struct ddsi_local_reader_ary *fastpath_rdary, void *vsourceinfo)
{
  (void) vsourceinfo;
  ddsrt_mutex_unlock (&fastpath_rdary->rdary_lock);
  if (source_entity_locked)
    ddsrt_mutex_unlock (&source_entity->lock);

  dds_sleepfor (DDS_MSECS (10));

  if (source_entity_locked)
    ddsrt_mutex_lock (&source_entity->lock);
  ddsrt_mutex_lock (&fastpath_rdary->rdary_lock);
  return DDS_RETCODE_TRY_AGAIN;
}

static int deliver_user_data (const struct ddsi_rsample_info *sampleinfo, const struct ddsi_rdata *fragchain, const ddsi_guid_t *rdguid, int pwr_locked)
{
  static const struct ddsi_deliver_locally_ops deliver_locally_ops = {
    .makesample = remote_make_sample,
    .first_reader = proxy_writer_first_in_sync_reader,
    .next_reader = proxy_writer_next_in_sync_reader,
    .on_failure_fastpath = remote_on_delivery_failure_fastpath
  };
  struct ddsi_receiver_state const * const rst = sampleinfo->rst;
  struct ddsi_domaingv * const gv = rst->gv;
  struct ddsi_proxy_writer * const pwr = sampleinfo->pwr;
  unsigned statusinfo;
  ddsi_rtps_data_datafrag_common_t *msg;
  unsigned char data_smhdr_flags;
  ddsi_plist_t qos;
  int need_keyhash;

  /* FIXME: fragments are now handled by copying the message to
     freshly malloced memory (see defragment()) ... that'll have to
     change eventually */
  assert (fragchain->min == 0);
  assert (!ddsi_is_builtin_entityid (pwr->e.guid.entityid, pwr->c.vendor));

  /* Luckily, the Data header (up to inline QoS) is a prefix of the
     DataFrag header, so for the fixed-position things that we're
     interested in here, both can be treated as Data submessages. */
  msg = (ddsi_rtps_data_datafrag_common_t *) DDSI_RMSG_PAYLOADOFF (fragchain->rmsg, DDSI_RDATA_SUBMSG_OFF (fragchain));
  data_smhdr_flags = ddsi_normalize_data_datafrag_flags (&msg->smhdr);

  /* Extract QoS's to the extent necessary.  The expected case has all
     we need predecoded into a few bits in the sample info.

     If there is no payload, it is either a completely invalid message
     or a dispose/unregister in RTI style.  We assume the latter,
     consequently expect to need the keyhash.  Then, if sampleinfo
     says it is a complex qos, or the keyhash is required, extract all
     we need from the inline qos.

     Complex qos bit also gets set when statusinfo bits other than
     dispose/unregister are set.  They are not currently defined, but
     this may save us if they do get defined one day.  */
  need_keyhash = (sampleinfo->size == 0 || (data_smhdr_flags & (DDSI_DATA_FLAG_KEYFLAG | DDSI_DATA_FLAG_DATAFLAG)) == 0);
  if (!(sampleinfo->complex_qos || need_keyhash) || !(data_smhdr_flags & DDSI_DATA_FLAG_INLINE_QOS))
  {
    ddsi_plist_init_empty (&qos);
    statusinfo = sampleinfo->statusinfo;
  }
  else
  {
    ddsi_plist_src_t src;
    size_t qos_offset = DDSI_RDATA_SUBMSG_OFF (fragchain) + offsetof (ddsi_rtps_data_datafrag_common_t, octetsToInlineQos) + sizeof (msg->octetsToInlineQos) + msg->octetsToInlineQos;
    dds_return_t plist_ret;
    src.protocol_version = rst->protocol_version;
    src.vendorid = rst->vendor;
    src.encoding = (msg->smhdr.flags & DDSI_RTPS_SUBMESSAGE_FLAG_ENDIANNESS) ? DDSI_RTPS_PL_CDR_LE : DDSI_RTPS_PL_CDR_BE;
    src.buf = DDSI_RMSG_PAYLOADOFF (fragchain->rmsg, qos_offset);
    src.bufsz = DDSI_RDATA_PAYLOAD_OFF (fragchain) - qos_offset;
    src.strict = DDSI_SC_STRICT_P (gv->config);
    if ((plist_ret = ddsi_plist_init_frommsg (&qos, NULL, PP_STATUSINFO | PP_KEYHASH, 0, &src, gv, DDSI_PLIST_CONTEXT_INLINE_QOS)) < 0)
    {
      if (plist_ret != DDS_RETCODE_UNSUPPORTED)
        GVWARNING ("data(application, vendor %u.%u): "PGUIDFMT" #%"PRIu64": invalid inline qos\n",
                   src.vendorid.id[0], src.vendorid.id[1], PGUID (pwr->e.guid), sampleinfo->seq);
      return 0;
    }
    statusinfo = (qos.present & PP_STATUSINFO) ? qos.statusinfo : 0;
  }

  /* FIXME: should it be 0, local wall clock time or INVALID? */
  const ddsrt_wctime_t tstamp = (sampleinfo->timestamp.v != DDSRT_WCTIME_INVALID.v) ? sampleinfo->timestamp : ((ddsrt_wctime_t) {0});
  struct ddsi_writer_info wrinfo;
  ddsi_make_writer_info (&wrinfo, &pwr->e, pwr->c.xqos, statusinfo);

  struct remote_sourceinfo sourceinfo = {
    .sampleinfo = sampleinfo,
    .data_smhdr_flags = data_smhdr_flags,
    .qos = &qos,
    .fragchain = fragchain,
    .statusinfo = statusinfo,
    .tstamp = tstamp
  };
  if (rdguid)
    (void) ddsi_deliver_locally_one (gv, &pwr->e, pwr_locked != 0, rdguid, &wrinfo, &deliver_locally_ops, &sourceinfo);
  else
  {
    (void) ddsi_deliver_locally_allinsync (gv, &pwr->e, pwr_locked != 0, &pwr->rdary, &wrinfo, &deliver_locally_ops, &sourceinfo);
    ddsrt_atomic_st32 (&pwr->next_deliv_seq_lowword, (uint32_t) (sampleinfo->seq + 1));
  }

  ddsi_plist_fini (&qos);
  return 0;
}

int ddsi_user_dqueue_handler (const struct ddsi_rsample_info *sampleinfo, const struct ddsi_rdata *fragchain, const ddsi_guid_t *rdguid, UNUSED_ARG (void *qarg))
{
  int res;
  res = deliver_user_data (sampleinfo, fragchain, rdguid, 0);
  return res;
}

static void deliver_user_data_synchronously (struct ddsi_rsample_chain *sc, const ddsi_guid_t *rdguid)
{
  while (sc->first)
  {
    struct ddsi_rsample_chain_elem *e = sc->first;
    sc->first = e->next;
    if (e->sampleinfo != NULL)
    {
      /* Must not try to deliver a gap -- possibly a FIXME for
         sample_lost events. Also note that the synchronous path is
         _never_ used for historical data, and therefore never has the
         GUID of a reader to deliver to */
      deliver_user_data (e->sampleinfo, e->fragchain, rdguid, 1);
    }
    ddsi_fragchain_unref (e->fragchain);
  }
}

static void clean_defrag (struct ddsi_proxy_writer *pwr)
{
  ddsi_seqno_t seq = ddsi_reorder_next_seq (pwr->reorder);
  if (pwr->n_readers_out_of_sync > 0)
  {
    struct ddsi_pwr_rd_match *wn;
    for (wn = ddsrt_avl_find_min (&ddsi_pwr_readers_treedef, &pwr->readers); wn != NULL; wn = ddsrt_avl_find_succ (&ddsi_pwr_readers_treedef, &pwr->readers, wn))
    {
      if (wn->in_sync == PRMSS_OUT_OF_SYNC)
      {
        ddsi_seqno_t seq1 = ddsi_reorder_next_seq (wn->u.not_in_sync.reorder);
        if (seq1 < seq)
          seq = seq1;
      }
    }
  }
  ddsi_defrag_notegap (pwr->defrag, 1, seq);
}

static void handle_regular (struct ddsi_receiver_state *rst, ddsrt_etime_t tnow, struct ddsi_rmsg *rmsg, const ddsi_rtps_data_datafrag_common_t *msg, const struct ddsi_rsample_info *sampleinfo,
    uint32_t max_fragnum_in_msg, struct ddsi_rdata *rdata, struct ddsi_dqueue **deferred_wakeup, bool renew_manbypp_lease)
{
  struct ddsi_proxy_writer *pwr;
  struct ddsi_rsample *rsample;
  ddsi_guid_t dst;
  struct ddsi_lease *lease;

  dst.prefix = rst->dst_guid_prefix;
  dst.entityid = msg->readerId;

  pwr = sampleinfo->pwr;
  if (pwr == NULL)
  {
    ddsi_guid_t src;
    src.prefix = rst->src_guid_prefix;
    src.entityid = msg->writerId;
    RSTTRACE (" "PGUIDFMT"? -> "PGUIDFMT, PGUID (src), PGUID (dst));
    return;
  }

  /* Proxy participant's "automatic" lease has to be renewed always, manual-by-participant one only
     for data published by the application.  If pwr->lease exists, it is in some manual lease mode,
     so check whether it is actually in manual-by-topic mode before renewing it.  As pwr->lease is
     set once (during entity creation) we can read it outside the lock, keeping all the lease
     renewals together. */
  if ((lease = ddsrt_atomic_ldvoidp (&pwr->c.proxypp->minl_auto)) != NULL)
    ddsi_lease_renew (lease, tnow);
  if ((lease = ddsrt_atomic_ldvoidp (&pwr->c.proxypp->minl_man)) != NULL && renew_manbypp_lease)
    ddsi_lease_renew (lease, tnow);
  if (pwr->lease && pwr->c.xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_TOPIC)
    ddsi_lease_renew (pwr->lease, tnow);

  /* Shouldn't lock the full writer, but will do so for now */
  ddsrt_mutex_lock (&pwr->e.lock);

  /* A change in transition from not-alive to alive is relatively complicated
     and may involve temporarily unlocking the proxy writer during the process
     (to avoid unnecessarily holding pwr->e.lock while invoking listeners on
     the reader) */
  if (!pwr->alive)
    ddsi_proxy_writer_set_alive_may_unlock (pwr, true);

  /* Don't accept data when reliable readers exist and we haven't yet seen
     a heartbeat telling us what the "current" sequence number of the writer
     is. If no reliable readers are present, we can't request a heartbeat and
     therefore must not require one.

     This should be fine except for the one case where one transitions from
     having only best-effort readers to also having a reliable reader (in
     the same process): in that case, the requirement that a heartbeat has
     been seen could potentially result in a disruption of the data flow to
     the best-effort readers.  That state should last only for a very short
     time, but it is rather inelegant.  */
  if (!pwr->have_seen_heartbeat && pwr->n_reliable_readers > 0 && ddsi_vendor_is_eclipse (rst->vendor))
  {
    ddsrt_mutex_unlock (&pwr->e.lock);
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT": no heartbeat seen yet", PGUID (pwr->e.guid), PGUID (dst));
    return;
  }

  if (ddsrt_avl_is_empty (&pwr->readers) || pwr->local_matching_inprogress)
  {
    ddsrt_mutex_unlock (&pwr->e.lock);
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT": no readers", PGUID (pwr->e.guid), PGUID (dst));
    return;
  }

  /* Track highest sequence number we know of -- we track both
     sequence number & fragment number so that the NACK generation can
     do the Right Thing. */
  if (sampleinfo->seq > pwr->last_seq)
  {
    pwr->last_seq = sampleinfo->seq;
    pwr->last_fragnum = max_fragnum_in_msg;
  }
  else if (sampleinfo->seq == pwr->last_seq && max_fragnum_in_msg > pwr->last_fragnum)
  {
    pwr->last_fragnum = max_fragnum_in_msg;
  }

  clean_defrag (pwr);

  if ((rsample = ddsi_defrag_rsample (pwr->defrag, rdata, sampleinfo)) != NULL)
  {
    int refc_adjust = 0;
    struct ddsi_rsample_chain sc;
    struct ddsi_rdata *fragchain = ddsi_rsample_fragchain (rsample);
    ddsi_reorder_result_t rres, rres2;
    struct ddsi_pwr_rd_match *wn;
    int filtered = 0;

    if (pwr->filtered && !ddsi_is_null_guid(&dst))
    {
      for (wn = ddsrt_avl_find_min (&ddsi_pwr_readers_treedef, &pwr->readers); wn != NULL; wn = ddsrt_avl_find_succ (&ddsi_pwr_readers_treedef, &pwr->readers, wn))
      {
        if (ddsi_guid_eq(&wn->rd_guid, &dst))
        {
          if (wn->filtered)
          {
            rres2 = ddsi_reorder_rsample (&sc, wn->u.not_in_sync.reorder, rsample, &refc_adjust, ddsi_dqueue_is_full (pwr->dqueue));
            if (sampleinfo->seq > wn->last_seq)
            {
              wn->last_seq = sampleinfo->seq;
            }
            if (rres2 > 0)
            {
              if (!pwr->deliver_synchronously)
                ddsi_dqueue_enqueue1 (pwr->dqueue, &wn->rd_guid, &sc, rres2);
              else
                deliver_user_data_synchronously (&sc, &wn->rd_guid);
            }
            filtered = 1;
          }
          break;
        }
      }
    }

    if (!filtered)
    {
      rres = ddsi_reorder_rsample (&sc, pwr->reorder, rsample, &refc_adjust, 0); // ddsi_dqueue_is_full (pwr->dqueue));

      if (rres == DDSI_REORDER_ACCEPT && pwr->n_reliable_readers == 0)
      {
        /* If no reliable readers but the reorder buffer accepted the
           sample, it must be a reliable proxy writer with only
           unreliable readers.  "Inserting" a Gap [1, sampleinfo->seq)
           will force delivery of this sample, and not cause the gap to
           be added to the reorder admin. */
        int gap_refc_adjust = 0;
        rres = ddsi_reorder_gap (&sc, pwr->reorder, rdata, 1, sampleinfo->seq, &gap_refc_adjust);
        assert (rres > 0);
        assert (gap_refc_adjust == 0);
      }

      if (rres > 0)
      {
        /* Enqueue or deliver with pwr->e.lock held: to ensure no other
           receive thread's data gets interleaved -- arguably delivery
           needn't be exactly in-order, which would allow us to do this
           without pwr->e.lock held.
           Note that PMD is also handled here, but the pwr for PMD does not
           use no synchronous delivery, so deliver_user_data_synchronously
           (which asserts pwr is not built-in) is not used for PMD handling. */
        if (pwr->deliver_synchronously)
        {
          /* FIXME: just in case the synchronous delivery runs into a delay caused
             by the current mishandling of resource limits */
          if (*deferred_wakeup)
            ddsi_dqueue_enqueue_trigger (*deferred_wakeup);
          deliver_user_data_synchronously (&sc, NULL);
        }
        else
        {
          if (ddsi_dqueue_enqueue_deferred_wakeup (pwr->dqueue, &sc, rres))
          {
            if (*deferred_wakeup && *deferred_wakeup != pwr->dqueue)
              ddsi_dqueue_enqueue_trigger (*deferred_wakeup);
            *deferred_wakeup = pwr->dqueue;
          }
        }
      }

      if (pwr->n_readers_out_of_sync > 0)
      {
        /* Those readers catching up with TL but in sync with the proxy
           writer may have become in sync with the proxy writer and the
           writer; those catching up with TL all by themselves go through
           the "TOO_OLD" path below. */
        ddsrt_avl_iter_t it;
        struct ddsi_rsample *rsample_dup = NULL;
        int reuse_rsample_dup = 0;
        for (wn = ddsrt_avl_iter_first (&ddsi_pwr_readers_treedef, &pwr->readers, &it); wn != NULL; wn = ddsrt_avl_iter_next (&it))
        {
          if (wn->in_sync == PRMSS_SYNC)
            continue;
          /* only need to get a copy of the first sample, because that's the one
             that triggered delivery */
          if (!reuse_rsample_dup)
            rsample_dup = ddsi_reorder_rsample_dup_first (rmsg, rsample);
          rres2 = ddsi_reorder_rsample (&sc, wn->u.not_in_sync.reorder, rsample_dup, &refc_adjust, ddsi_dqueue_is_full (pwr->dqueue));
          switch (rres2)
          {
            case DDSI_REORDER_TOO_OLD:
            case DDSI_REORDER_REJECT:
              reuse_rsample_dup = 1;
              break;
            case DDSI_REORDER_ACCEPT:
              reuse_rsample_dup = 0;
              break;
            default:
              assert (rres2 > 0);
              /* note: can't deliver to a reader, only to a group */
              maybe_set_reader_in_sync (pwr, wn, sampleinfo->seq);
              reuse_rsample_dup = 0;
              /* No need to deliver old data to out-of-sync readers
                 synchronously -- ordering guarantees don't change
                 as fresh data will be delivered anyway and hence
                 the old data will never be guaranteed to arrive
                 in-order, and those few microseconds can't hurt in
                 catching up on transient-local data.  See also
                 DDSI_REORDER_DELIVER case in outer switch. */
              if (pwr->deliver_synchronously)
              {
                /* FIXME: just in case the synchronous delivery runs into a delay caused
                   by the current mishandling of resource limits */
                deliver_user_data_synchronously (&sc, &wn->rd_guid);
              }
              else
              {
                if (*deferred_wakeup && *deferred_wakeup != pwr->dqueue)
                {
                  ddsi_dqueue_enqueue_trigger (*deferred_wakeup);
                  *deferred_wakeup = NULL;
                }
                ddsi_dqueue_enqueue1 (pwr->dqueue, &wn->rd_guid, &sc, rres2);
              }
              break;
          }
        }
      }
    }

    ddsi_fragchain_adjust_refcount (fragchain, refc_adjust);
  }
  ddsrt_mutex_unlock (&pwr->e.lock);
  ddsi_dqueue_wait_until_empty_if_full (pwr->dqueue);
}

static int handle_SPDP (const struct ddsi_rsample_info *sampleinfo, struct ddsi_rdata *rdata)
{
  struct ddsi_domaingv * const gv = sampleinfo->rst->gv;
  struct ddsi_rsample *rsample;
  struct ddsi_rsample_chain sc;
  struct ddsi_rdata *fragchain;
  ddsi_reorder_result_t rres;
  int refc_adjust = 0;
  ddsrt_mutex_lock (&gv->spdp_lock);
  rsample = ddsi_defrag_rsample (gv->spdp_defrag, rdata, sampleinfo);
  fragchain = ddsi_rsample_fragchain (rsample);
  if ((rres = ddsi_reorder_rsample (&sc, gv->spdp_reorder, rsample, &refc_adjust, ddsi_dqueue_is_full (gv->builtins_dqueue))) > 0)
    ddsi_dqueue_enqueue (gv->builtins_dqueue, &sc, rres);
  ddsi_fragchain_adjust_refcount (fragchain, refc_adjust);
  ddsrt_mutex_unlock (&gv->spdp_lock);
  return 0;
}

static void drop_oversize (struct ddsi_receiver_state *rst, struct ddsi_rmsg *rmsg, const ddsi_rtps_data_datafrag_common_t *msg, struct ddsi_rsample_info *sampleinfo)
{
  struct ddsi_proxy_writer *pwr = sampleinfo->pwr;
  if (pwr == NULL)
  {
    /* No proxy writer means nothing really gets done with, unless it
       is SPDP.  SPDP is periodic, so oversize discovery packets would
       cause periodic warnings. */
    if ((msg->writerId.u == DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER) ||
        (msg->writerId.u == DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER))
    {
      DDS_CWARNING (&rst->gv->logconfig, "dropping oversize (%"PRIu32" > %"PRIu32") SPDP sample %"PRIu64" from remote writer "PGUIDFMT"\n",
                    sampleinfo->size, rst->gv->config.max_sample_size, sampleinfo->seq,
                    PGUIDPREFIX (rst->src_guid_prefix), msg->writerId.u);
    }
  }
  else
  {
    /* Normal case: we actually do know the writer.  Dropping it is as
       easy as pushing a gap through the pipe, but trying to log the
       event only once is trickier.  Checking whether the gap had some
       effect seems a reasonable approach. */
    int refc_adjust = 0;
    struct ddsi_rdata *gap = ddsi_rdata_newgap (rmsg);
    ddsi_guid_t dst;
    struct ddsi_pwr_rd_match *wn;
    int gap_was_valuable;

    dst.prefix = rst->dst_guid_prefix;
    dst.entityid = msg->readerId;

    ddsrt_mutex_lock (&pwr->e.lock);
    wn = ddsrt_avl_lookup (&ddsi_pwr_readers_treedef, &pwr->readers, &dst);
    gap_was_valuable = handle_one_gap (pwr, wn, sampleinfo->seq, sampleinfo->seq+1, gap, &refc_adjust);
    ddsi_fragchain_adjust_refcount (gap, refc_adjust);
    ddsrt_mutex_unlock (&pwr->e.lock);

    if (gap_was_valuable)
    {
      const char *tname = (pwr->c.xqos->present & DDSI_QP_TOPIC_NAME) ? pwr->c.xqos->topic_name : "(null)";
      const char *ttname = (pwr->c.xqos->present & DDSI_QP_TYPE_NAME) ? pwr->c.xqos->type_name : "(null)";
      DDS_CWARNING (&rst->gv->logconfig, "dropping oversize (%"PRIu32" > %"PRIu32") sample %"PRIu64" from remote writer "PGUIDFMT" %s/%s\n",
                    sampleinfo->size, rst->gv->config.max_sample_size, sampleinfo->seq,
                    PGUIDPREFIX (rst->src_guid_prefix), msg->writerId.u,
                    tname, ttname);
    }
  }
}

static int handle_Data (struct ddsi_receiver_state *rst, ddsrt_etime_t tnow, struct ddsi_rmsg *rmsg, const ddsi_rtps_data_t *msg, size_t size, struct ddsi_rsample_info *sampleinfo, const ddsi_keyhash_t *keyhash, unsigned char *datap, struct ddsi_dqueue **deferred_wakeup, ddsi_rtps_submessage_kind_t prev_smid)
{
  RSTTRACE ("DATA("PGUIDFMT" -> "PGUIDFMT" #%"PRIu64,
            PGUIDPREFIX (rst->src_guid_prefix), msg->x.writerId.u,
            PGUIDPREFIX (rst->dst_guid_prefix), msg->x.readerId.u,
            ddsi_from_seqno (msg->x.writerSN));
  if (!rst->forme)
  {
    RSTTRACE (" not-for-me)");
    return 1;
  }

  if (sampleinfo->pwr)
  {
    if (!ddsi_security_validate_msg_decoding(&(sampleinfo->pwr->e), &(sampleinfo->pwr->c), sampleinfo->pwr->c.proxypp, rst, prev_smid))
    {
      RSTTRACE (" clear submsg from protected src "PGUIDFMT")", PGUID (sampleinfo->pwr->e.guid));
      return 1;
    }
  }

  if (sampleinfo->size > rst->gv->config.max_sample_size)
    drop_oversize (rst, rmsg, &msg->x, sampleinfo);
  else
  {
    struct ddsi_rdata *rdata;
    unsigned submsg_offset, payload_offset, keyhash_offset;
    submsg_offset = (unsigned) ((unsigned char *) msg - DDSI_RMSG_PAYLOAD (rmsg));
    if (datap)
      payload_offset = (unsigned) ((unsigned char *) datap - DDSI_RMSG_PAYLOAD (rmsg));
    else
      payload_offset = submsg_offset + (unsigned) size;
    if (keyhash)
      keyhash_offset = (unsigned) (keyhash->value - DDSI_RMSG_PAYLOAD (rmsg));
    else
      keyhash_offset = 0;

    rdata = ddsi_rdata_new (rmsg, 0, sampleinfo->size, submsg_offset, payload_offset, keyhash_offset);

    if ((msg->x.writerId.u & DDSI_ENTITYID_SOURCE_MASK) == DDSI_ENTITYID_SOURCE_BUILTIN)
    {
      bool renew_manbypp_lease = true;
      switch (msg->x.writerId.u)
      {
        case DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER:
        /* fall through */
        case DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER:
          /* SPDP needs special treatment: there are no proxy writers for it and we accept data from unknown sources */
          handle_SPDP (sampleinfo, rdata);
          break;
        case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER:
        /* fall through */
        case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER:
          /* Handle PMD as a regular message, but without renewing the leases on proxypp */
          renew_manbypp_lease = false;
        /* fall through */
        default:
          handle_regular (rst, tnow, rmsg, &msg->x, sampleinfo, UINT32_MAX, rdata, deferred_wakeup, renew_manbypp_lease);
      }
    }
    else
    {
      handle_regular (rst, tnow, rmsg, &msg->x, sampleinfo, UINT32_MAX, rdata, deferred_wakeup, true);
    }
  }
  RSTTRACE (")");
  return 1;
}

static int handle_DataFrag (struct ddsi_receiver_state *rst, ddsrt_etime_t tnow, struct ddsi_rmsg *rmsg, const ddsi_rtps_datafrag_t *msg, size_t size, struct ddsi_rsample_info *sampleinfo, const ddsi_keyhash_t *keyhash, unsigned char *datap, struct ddsi_dqueue **deferred_wakeup, ddsi_rtps_submessage_kind_t prev_smid)
{
  RSTTRACE ("DATAFRAG("PGUIDFMT" -> "PGUIDFMT" #%"PRIu64"/[%"PRIu32"..%"PRIu32"]",
            PGUIDPREFIX (rst->src_guid_prefix), msg->x.writerId.u,
            PGUIDPREFIX (rst->dst_guid_prefix), msg->x.readerId.u,
            ddsi_from_seqno (msg->x.writerSN),
            msg->fragmentStartingNum, (ddsi_fragment_number_t) (msg->fragmentStartingNum + msg->fragmentsInSubmessage - 1));
  if (!rst->forme)
  {
    RSTTRACE (" not-for-me)");
    return 1;
  }

  if (sampleinfo->pwr)
  {
    if (!ddsi_security_validate_msg_decoding(&(sampleinfo->pwr->e), &(sampleinfo->pwr->c), sampleinfo->pwr->c.proxypp, rst, prev_smid))
    {
      RSTTRACE (" clear submsg from protected src "PGUIDFMT")", PGUID (sampleinfo->pwr->e.guid));
      return 1;
    }
  }

  if (sampleinfo->size > rst->gv->config.max_sample_size)
    drop_oversize (rst, rmsg, &msg->x, sampleinfo);
  else
  {
    struct ddsi_rdata *rdata;
    unsigned submsg_offset, payload_offset, keyhash_offset;
    uint32_t begin, endp1;
    bool renew_manbypp_lease = true;
    if ((msg->x.writerId.u & DDSI_ENTITYID_SOURCE_MASK) == DDSI_ENTITYID_SOURCE_BUILTIN)
    {
      switch (msg->x.writerId.u)
      {
        case DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER:
        /* fall through */
        case DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER:
          DDS_CWARNING (&rst->gv->logconfig, "DATAFRAG("PGUIDFMT" #%"PRIu64" -> "PGUIDFMT") - fragmented builtin data not yet supported\n",
                        PGUIDPREFIX (rst->src_guid_prefix), msg->x.writerId.u, ddsi_from_seqno (msg->x.writerSN),
                        PGUIDPREFIX (rst->dst_guid_prefix), msg->x.readerId.u);
          return 1;
        case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER:
        /* fall through */
        case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER:
          renew_manbypp_lease = false;
      }
    }

    submsg_offset = (unsigned) ((unsigned char *) msg - DDSI_RMSG_PAYLOAD (rmsg));
    if (datap)
      payload_offset = (unsigned) ((unsigned char *) datap - DDSI_RMSG_PAYLOAD (rmsg));
    else
      payload_offset = submsg_offset + (unsigned) size;
    if (keyhash)
      keyhash_offset = (unsigned) (keyhash->value - DDSI_RMSG_PAYLOAD (rmsg));
    else
      keyhash_offset = 0;

    begin = (msg->fragmentStartingNum - 1) * msg->fragmentSize;
    if ((uint32_t) msg->fragmentSize * msg->fragmentsInSubmessage > (uint32_t) ((unsigned char *) msg + size - datap)) {
      /* this happens for the last fragment (which usually is short) --
         and is included here merely as a sanity check, because that
         would mean the computed endp1'd be larger than the sample
         size */
      endp1 = begin + (uint32_t) ((unsigned char *) msg + size - datap);
    } else {
      /* most of the time we get here, but this differs from the
         preceding only when the fragment size is not a multiple of 4
         whereas all the length of CDR data always is (and even then,
         you'd be fine as the defragmenter can deal with partially
         overlapping fragments ...) */
      endp1 = begin + (uint32_t) msg->fragmentSize * msg->fragmentsInSubmessage;
    }
    if (endp1 > msg->sampleSize)
    {
      /* the sample size need not be a multiple of 4 so we can still get
         here */
      endp1 = msg->sampleSize;
    }
    RSTTRACE ("/[%"PRIu32"..%"PRIu32") of %"PRIu32, begin, endp1, msg->sampleSize);

    rdata = ddsi_rdata_new (rmsg, begin, endp1, submsg_offset, payload_offset, keyhash_offset);

    /* Fragment numbers in DDSI2 internal representation are 0-based,
       whereas in DDSI they are 1-based.  The highest fragment number in
       the sample in internal representation is therefore START+CNT-2,
       rather than the expect START+CNT-1.  Nothing will go terribly
       wrong, it'll simply generate a request for retransmitting a
       non-existent fragment.  The other side SHOULD be capable of
       dealing with that. */
    handle_regular (rst, tnow, rmsg, &msg->x, sampleinfo, msg->fragmentStartingNum + msg->fragmentsInSubmessage - 2, rdata, deferred_wakeup, renew_manbypp_lease);
  }
  RSTTRACE (")");
  return 1;
}

struct submsg_name {
  char x[32];
};

static const char *submsg_name (ddsi_rtps_submessage_kind_t id, struct submsg_name *buffer)
{
  switch (id)
  {
    case DDSI_RTPS_SMID_PAD: return "PAD";
    case DDSI_RTPS_SMID_ACKNACK: return "ACKNACK";
    case DDSI_RTPS_SMID_HEARTBEAT: return "HEARTBEAT";
    case DDSI_RTPS_SMID_GAP: return "GAP";
    case DDSI_RTPS_SMID_INFO_TS: return "INFO_TS";
    case DDSI_RTPS_SMID_INFO_SRC: return "INFO_SRC";
    case DDSI_RTPS_SMID_INFO_REPLY_IP4: return "REPLY_IP4";
    case DDSI_RTPS_SMID_INFO_DST: return "INFO_DST";
    case DDSI_RTPS_SMID_INFO_REPLY: return "INFO_REPLY";
    case DDSI_RTPS_SMID_NACK_FRAG: return "NACK_FRAG";
    case DDSI_RTPS_SMID_HEARTBEAT_FRAG: return "HEARTBEAT_FRAG";
    case DDSI_RTPS_SMID_DATA_FRAG: return "DATA_FRAG";
    case DDSI_RTPS_SMID_DATA: return "DATA";
    case DDSI_RTPS_SMID_ADLINK_MSG_LEN: return "ADLINK_MSG_LEN";
    case DDSI_RTPS_SMID_ADLINK_ENTITY_ID: return "ADLINK_ENTITY_ID";
    case DDSI_RTPS_SMID_SEC_PREFIX: return "SEC_PREFIX";
    case DDSI_RTPS_SMID_SEC_BODY: return "SEC_BODY";
    case DDSI_RTPS_SMID_SEC_POSTFIX: return "SEC_POSTFIX";
    case DDSI_RTPS_SMID_SRTPS_PREFIX: return "SRTPS_PREFIX";
    case DDSI_RTPS_SMID_SRTPS_POSTFIX: return "SRTPS_POSTFIX";
  }
  (void) snprintf (buffer->x, sizeof (buffer->x), "UNKNOWN(%x)", (unsigned) id);
  return buffer->x;
}

static void malformed_packet_received (const struct ddsi_domaingv *gv, const unsigned char *msg, const unsigned char *submsg, size_t len, ddsi_vendorid_t vendorid)
{
  char tmp[1024];
  size_t i, pos, smsize;

  struct submsg_name submsg_name_buffer;
  ddsi_rtps_submessage_kind_t smkind;
  const char *state0;
  const char *state1;
  if (submsg == NULL || (submsg < msg || submsg >= msg + len)) {
    // outside buffer shouldn't happen, but this is for dealing with junk, so better be careful
    smkind = DDSI_RTPS_SMID_PAD;
    state0 = "";
    state1 = "header";
    submsg = msg;
  } else if ((size_t) (msg + len - submsg) < DDSI_RTPS_SUBMESSAGE_HEADER_SIZE) {
    smkind = DDSI_RTPS_SMID_PAD;
    state0 = "parse:";
    state1 = (submsg == msg) ? "init" : "shortmsg";
  } else {
    smkind = (ddsi_rtps_submessage_kind_t) *submsg;
    state0 = "parse:";
    state1 = submsg_name (smkind, &submsg_name_buffer);
  }
  assert (submsg >= msg && submsg <= msg + len);

  /* Show beginning of message and of submessage (as hex dumps) */
  pos = (size_t) snprintf (tmp, sizeof (tmp), "malformed packet received from vendor %u.%u state %s%s <", vendorid.id[0], vendorid.id[1], state0, state1);
  for (i = 0; i < 32 && i < len && msg + i < submsg && pos < sizeof (tmp); i++)
    pos += (size_t) snprintf (tmp + pos, sizeof (tmp) - pos, "%s%02x", (i > 0 && (i%4) == 0) ? " " : "", msg[i]);
  if (pos < sizeof (tmp))
    pos += (size_t) snprintf (tmp + pos, sizeof (tmp) - pos, " @0x%x ", (int) (submsg - msg));
  for (i = 0; i < 64 && i < len - (size_t) (submsg - msg) && pos < sizeof (tmp); i++)
    pos += (size_t) snprintf (tmp + pos, sizeof (tmp) - pos, "%s%02x", (i > 0 && (i%4) == 0) ? " " : "", submsg[i]);
  if (pos < sizeof (tmp))
    pos += (size_t) snprintf (tmp + pos, sizeof (tmp) - pos, "> (note: maybe partially bswap'd)");
  assert (pos < (int) sizeof (tmp));

  /* Partially decode header if we have enough bytes available */
  smsize = len - (size_t) (submsg - msg);
  if (smsize >= DDSI_RTPS_SUBMESSAGE_HEADER_SIZE && pos < sizeof (tmp)) {
    const ddsi_rtps_submessage_header_t *x = (const ddsi_rtps_submessage_header_t *) submsg;
    pos += (size_t) snprintf (tmp + pos, sizeof (tmp) - pos, " smid 0x%x flags 0x%x otnh %u", x->submessageId, x->flags, x->octetsToNextHeader);
  }
  if (pos < sizeof (tmp)) {
    switch (smkind) {
      case DDSI_RTPS_SMID_ACKNACK:
        if (smsize >= sizeof (ddsi_rtps_acknack_t)) {
          const ddsi_rtps_acknack_t *x = (const ddsi_rtps_acknack_t *) submsg;
          (void) snprintf (tmp + pos, sizeof (tmp) - pos, " rid 0x%"PRIx32" wid 0x%"PRIx32" base %"PRIu64" numbits %"PRIu32,
                           x->readerId.u, x->writerId.u, ddsi_from_seqno (x->readerSNState.bitmap_base),
                           x->readerSNState.numbits);
        }
        break;
      case DDSI_RTPS_SMID_HEARTBEAT:
        if (smsize >= sizeof (ddsi_rtps_heartbeat_t)) {
          const ddsi_rtps_heartbeat_t *x = (const ddsi_rtps_heartbeat_t *) submsg;
          (void) snprintf (tmp + pos, sizeof (tmp) - pos, " rid 0x%"PRIx32" wid 0x%"PRIx32" first %"PRIu64" last %"PRIu64,
                           x->readerId.u, x->writerId.u, ddsi_from_seqno (x->firstSN), ddsi_from_seqno (x->lastSN));
        }
        break;
      case DDSI_RTPS_SMID_GAP:
        if (smsize >= sizeof (ddsi_rtps_gap_t)) {
          const ddsi_rtps_gap_t *x = (const ddsi_rtps_gap_t *) submsg;
          (void) snprintf (tmp + pos, sizeof (tmp) - pos, " rid 0x%"PRIx32" wid 0x%"PRIx32" gapstart %"PRIu64" base %"PRIu64" numbits %"PRIu32,
                           x->readerId.u, x->writerId.u, ddsi_from_seqno (x->gapStart),
                           ddsi_from_seqno (x->gapList.bitmap_base), x->gapList.numbits);
        }
        break;
      case DDSI_RTPS_SMID_NACK_FRAG:
        if (smsize >= sizeof (ddsi_rtps_nackfrag_t)) {
          const ddsi_rtps_nackfrag_t *x = (const ddsi_rtps_nackfrag_t *) submsg;
          (void) snprintf (tmp + pos, sizeof (tmp) - pos, " rid 0x%"PRIx32" wid 0x%"PRIx32" seq# %"PRIu64" base %"PRIu32" numbits %"PRIu32,
                           x->readerId.u, x->writerId.u, ddsi_from_seqno (x->writerSN),
                           x->fragmentNumberState.bitmap_base, x->fragmentNumberState.numbits);
        }
        break;
      case DDSI_RTPS_SMID_HEARTBEAT_FRAG:
        if (smsize >= sizeof (ddsi_rtps_heartbeatfrag_t)) {
          const ddsi_rtps_heartbeatfrag_t *x = (const ddsi_rtps_heartbeatfrag_t *) submsg;
          (void) snprintf (tmp + pos, sizeof (tmp) - pos, " rid 0x%"PRIx32" wid 0x%"PRIx32" seq %"PRIu64" frag %"PRIu32,
                           x->readerId.u, x->writerId.u, ddsi_from_seqno (x->writerSN),
                           x->lastFragmentNum);
        }
        break;
      case DDSI_RTPS_SMID_DATA:
        if (smsize >= sizeof (ddsi_rtps_data_t)) {
          const ddsi_rtps_data_t *x = (const ddsi_rtps_data_t *) submsg;
          (void) snprintf (tmp + pos, sizeof (tmp) - pos, " xflags %x otiq %u rid 0x%"PRIx32" wid 0x%"PRIx32" seq %"PRIu64,
                           x->x.extraFlags, x->x.octetsToInlineQos,
                           x->x.readerId.u, x->x.writerId.u, ddsi_from_seqno (x->x.writerSN));
        }
        break;
      case DDSI_RTPS_SMID_DATA_FRAG:
        if (smsize >= sizeof (ddsi_rtps_datafrag_t)) {
          const ddsi_rtps_datafrag_t *x = (const ddsi_rtps_datafrag_t *) submsg;
          (void) snprintf (tmp + pos, sizeof (tmp) - pos, " xflags %x otiq %u rid 0x%"PRIx32" wid 0x%"PRIx32" seq %"PRIu64" frag %"PRIu32"  fragsinmsg %"PRIu16" fragsize %"PRIu16" samplesize %"PRIu32,
                           x->x.extraFlags, x->x.octetsToInlineQos,
                           x->x.readerId.u, x->x.writerId.u, ddsi_from_seqno (x->x.writerSN),
                           x->fragmentStartingNum, x->fragmentsInSubmessage, x->fragmentSize, x->sampleSize);
        }
        break;
      default:
        break;
    }
  }
  GVWARNING ("%s\n", tmp);
}

static struct ddsi_receiver_state *rst_cow_if_needed (int *rst_live, struct ddsi_rmsg *rmsg, struct ddsi_receiver_state *rst)
{
  if (! *rst_live)
    return rst;
  else
  {
    struct ddsi_receiver_state *nrst = ddsi_rmsg_alloc (rmsg, sizeof (*nrst));
    *nrst = *rst;
    *rst_live = 0;
    return nrst;
  }
}

static int handle_submsg_sequence
(
  struct ddsi_thread_state * const thrst,
  struct ddsi_domaingv *gv,
  struct ddsi_tran_conn * conn,
  const ddsi_locator_t *srcloc,
  ddsrt_wctime_t tnowWC,
  ddsrt_etime_t tnowE,
  const ddsi_guid_prefix_t * const src_prefix,
  const ddsi_guid_prefix_t * const dst_prefix,
  unsigned char * const msg /* NOT const - we may byteswap it */,
  const size_t len,
  unsigned char * submsg /* aliases somewhere in msg */,
  struct ddsi_rmsg * const rmsg,
  bool rtps_encoded /* indicate if the message was rtps encoded */
)
{
  ddsi_rtps_header_t * hdr = (ddsi_rtps_header_t *) msg;
  struct ddsi_receiver_state *rst;
  int rst_live, ts_for_latmeas;
  ddsrt_wctime_t timestamp;
  size_t submsg_size = 0;
  unsigned char * end = msg + len;
  struct ddsi_dqueue *deferred_wakeup = NULL;
  ddsi_rtps_submessage_kind_t prev_smid = DDSI_RTPS_SMID_PAD;
  struct defer_hb_state defer_hb_state;

  /* Receiver state is dynamically allocated with lifetime bound to
     the message.  Updates cause a new copy to be created if the
     current one is "live", i.e., possibly referenced by a
     submessage (for now, only Data(Frag)). */
  rst = ddsi_rmsg_alloc (rmsg, sizeof (*rst));
  memset (rst, 0, sizeof (*rst));
  rst->conn = conn;
  rst->src_guid_prefix = *src_prefix;
  if (dst_prefix)
  {
    rst->dst_guid_prefix = *dst_prefix;
  }
  /* "forme" is a whether the current submessage is intended for this
     instance of DDSI and is roughly equivalent to
       (dst_prefix == 0) ||
       (ddsi_entidx_lookup_participant_guid(dst_prefix:1c1) != 0)
     they are only roughly equivalent because the second term can become
     false at any time. That's ok: it's real purpose is to filter out
     discovery data accidentally sent by Cloud */
  rst->forme = 1;
  rst->rtps_encoded = rtps_encoded;
  rst->vendor = hdr->vendorid;
  rst->protocol_version = hdr->version;
  rst->srcloc = *srcloc;
  rst->gv = gv;
  rst_live = 0;
  ts_for_latmeas = 0;
  timestamp = DDSRT_WCTIME_INVALID;
  defer_hb_state_init (&defer_hb_state);
  assert (ddsi_thread_is_asleep ());
  ddsi_thread_state_awake_fixed_domain (thrst);
  enum validation_result vr = (len >= sizeof (ddsi_rtps_submessage_header_t)) ? VR_NOT_UNDERSTOOD : VR_MALFORMED;
  while (vr != VR_MALFORMED && submsg <= (end - sizeof (ddsi_rtps_submessage_header_t)))
  {
    ddsi_rtps_submessage_t * const sm = (ddsi_rtps_submessage_t *) submsg;
    bool byteswap;

    DDSRT_WARNING_MSVC_OFF(6326)
    if (sm->smhdr.flags & DDSI_RTPS_SUBMESSAGE_FLAG_ENDIANNESS)
      byteswap = !(DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN);
    else
      byteswap =  (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN);
    DDSRT_WARNING_MSVC_ON(6326)
    if (byteswap)
      sm->smhdr.octetsToNextHeader = ddsrt_bswap2u (sm->smhdr.octetsToNextHeader);

    const uint32_t octetsToNextHeader = sm->smhdr.octetsToNextHeader;
    if (octetsToNextHeader != 0) {
      // DDSI 2.5 9.4.1: The PSM aligns each Submessage on a 32-bit boundary
      // with respect to the start of the Message
      //
      // DDSI 2.5 9.4.5.1.3 - regular case:
      //
      // In case octetsToNextHeader > 0, it is the number of octets from the first octet
      // of the contents of the Submessage until the first octet of the header of the next
      // Submessage (in case the Submessage is not the last Submessage in the Message)
      //
      // DDSI 2.5 9.4.5.1.3 - the unnecessary complication:
      //
      // OR it is the number of octets remaining in the Message (in case the Submessage
      // is the last Submessage in the Message). An interpreter of the Message can distinguish
      // these two cases as it knows the total length of the Message.
      //
      // So what then if it is not 0 mod 4 and yet also not the number of octets remaining in
      // the Message?  The total length of the Message comes from elsewhere and is also not
      // necessarily trustworthy.  Following the tradition in Cyclone, we'll consider it
      // malformed.  (The alternative would be to *update* "end", because otherwise we'd be
      // interpreting misaligned data.)
      submsg_size = DDSI_RTPS_SUBMESSAGE_HEADER_SIZE + octetsToNextHeader;
      if (!((octetsToNextHeader % 4) == 0 || submsg_size == (size_t) (end - submsg))) {
        vr = VR_MALFORMED;
        break;
      }
    } else if (sm->smhdr.submessageId == DDSI_RTPS_SMID_PAD || sm->smhdr.submessageId == DDSI_RTPS_SMID_INFO_TS) {
      submsg_size = DDSI_RTPS_SUBMESSAGE_HEADER_SIZE;
    } else {
      submsg_size = (size_t) (end - submsg);
    }
    /*GVTRACE ("submsg_size %d\n", submsg_size);*/

    if (submsg_size > (size_t) (end - submsg))
    {
      GVTRACE (" BREAK (%u %"PRIuSIZE": %p %u)\n", (unsigned) (submsg - msg), submsg_size, (void *) msg, (unsigned) len);
      break;
    }

    ddsi_thread_state_awake_to_awake_no_nest (thrst);
    switch (sm->smhdr.submessageId)
    {
      case DDSI_RTPS_SMID_ACKNACK: {
        if ((vr = validate_AckNack (rst, &sm->acknack, submsg_size, byteswap)) == VR_ACCEPT)
          handle_AckNack (rst, tnowE, &sm->acknack, ts_for_latmeas ? timestamp : DDSRT_WCTIME_INVALID, prev_smid, &defer_hb_state);
        ts_for_latmeas = 0;
        break;
      }
      case DDSI_RTPS_SMID_HEARTBEAT: {
        if ((vr = validate_Heartbeat (&sm->heartbeat, submsg_size, byteswap)) == VR_ACCEPT)
          handle_Heartbeat (rst, tnowE, rmsg, &sm->heartbeat, ts_for_latmeas ? timestamp : DDSRT_WCTIME_INVALID, prev_smid);
        ts_for_latmeas = 0;
        break;
      }
      case DDSI_RTPS_SMID_GAP: {
        if ((vr = validate_Gap (&sm->gap, submsg_size, byteswap)) == VR_ACCEPT)
          handle_Gap (rst, tnowE, rmsg, &sm->gap, prev_smid);
        ts_for_latmeas = 0;
        break;
      }
      case DDSI_RTPS_SMID_INFO_TS: {
        if ((vr = validate_InfoTS (&sm->infots, submsg_size, byteswap)) == VR_ACCEPT) {
          handle_InfoTS (rst, &sm->infots, &timestamp);
          ts_for_latmeas = 1;
        }
        break;
      }
      case DDSI_RTPS_SMID_INFO_SRC: {
        if ((vr = validate_InfoSRC (&sm->infosrc, submsg_size, byteswap)) == VR_ACCEPT) {
          rst = rst_cow_if_needed (&rst_live, rmsg, rst);
          handle_InfoSRC (rst, &sm->infosrc);
        }
        /* no effect on ts_for_latmeas */
        break;
      }
      case DDSI_RTPS_SMID_INFO_DST: {
        if ((vr = validate_InfoDST (&sm->infodst, submsg_size, byteswap)) == VR_ACCEPT) {
          rst = rst_cow_if_needed (&rst_live, rmsg, rst);
          handle_InfoDST (rst, &sm->infodst, dst_prefix);
        }
        /* no effect on ts_for_latmeas */
        break;
      }
      case DDSI_RTPS_SMID_NACK_FRAG: {
        if ((vr = validate_NackFrag (&sm->nackfrag, submsg_size, byteswap)) == VR_ACCEPT)
          handle_NackFrag (rst, tnowE, &sm->nackfrag, prev_smid, &defer_hb_state);
        ts_for_latmeas = 0;
        break;
      }
      case DDSI_RTPS_SMID_HEARTBEAT_FRAG: {
        if ((vr = validate_HeartbeatFrag (&sm->heartbeatfrag, submsg_size, byteswap)) == VR_ACCEPT)
          handle_HeartbeatFrag (rst, tnowE, &sm->heartbeatfrag, prev_smid);
        ts_for_latmeas = 0;
        break;
      }
      case DDSI_RTPS_SMID_DATA_FRAG: {
        struct ddsi_rsample_info sampleinfo;
        uint32_t datasz = 0;
        unsigned char *datap;
        const ddsi_keyhash_t *keyhash;
        size_t submsg_len = submsg_size;
        if ((vr = validate_DataFrag (rst, &sm->datafrag, submsg_size, byteswap, &sampleinfo, &keyhash, &datap, &datasz)) != VR_ACCEPT) {
          // nothing to be done here if not accepted
        } else if (!ddsi_security_decode_datafrag (rst->gv, &sampleinfo, datap, datasz, &submsg_len)) {
          // payload decryption required but failed
          vr = VR_NOT_UNDERSTOOD;
        } else if (sm->datafrag.fragmentStartingNum == 1 && !set_sampleinfo_bswap (&sampleinfo, (struct dds_cdr_header *)datap)) {
          // first fragment has encoding header, tried to use that for setting sample bswap but failed
          vr = VR_MALFORMED;
        } else {
          sampleinfo.timestamp = timestamp;
          sampleinfo.reception_timestamp = tnowWC;
          handle_DataFrag (rst, tnowE, rmsg, &sm->datafrag, submsg_len, &sampleinfo, keyhash, datap, &deferred_wakeup, prev_smid);
          rst_live = 1;
        }
        ts_for_latmeas = 0;
        break;
      }
      case DDSI_RTPS_SMID_DATA: {
        struct ddsi_rsample_info sampleinfo;
        unsigned char *datap;
        const ddsi_keyhash_t *keyhash;
        uint32_t datasz = 0;
        size_t submsg_len = submsg_size;
        if ((vr = validate_Data (rst, &sm->data, submsg_size, byteswap, &sampleinfo, &keyhash, &datap, &datasz)) != VR_ACCEPT) {
          // nothing to be done here if not accepted
        } else if (!ddsi_security_decode_data (rst->gv, &sampleinfo, datap, datasz, &submsg_len)) {
          vr = VR_NOT_UNDERSTOOD;
        } else if (!set_sampleinfo_bswap (&sampleinfo, (struct dds_cdr_header *)datap)) {
          vr = VR_MALFORMED;
        } else {
          sampleinfo.timestamp = timestamp;
          sampleinfo.reception_timestamp = tnowWC;
          handle_Data (rst, tnowE, rmsg, &sm->data, submsg_len, &sampleinfo, keyhash, datap, &deferred_wakeup, prev_smid);
          rst_live = 1;
        }
        ts_for_latmeas = 0;
        break;
      }
      case DDSI_RTPS_SMID_SEC_PREFIX: {
        GVTRACE ("SEC_PREFIX ");
        if (!ddsi_security_decode_sec_prefix(rst, submsg, submsg_size, end, &rst->src_guid_prefix, &rst->dst_guid_prefix, byteswap))
          vr = VR_MALFORMED;
        break;
      }
      case DDSI_RTPS_SMID_PAD:
      case DDSI_RTPS_SMID_INFO_REPLY:
      case DDSI_RTPS_SMID_INFO_REPLY_IP4:
      case DDSI_RTPS_SMID_ADLINK_MSG_LEN:
      case DDSI_RTPS_SMID_ADLINK_ENTITY_ID:
      case DDSI_RTPS_SMID_SEC_BODY:
      case DDSI_RTPS_SMID_SEC_POSTFIX:
      case DDSI_RTPS_SMID_SRTPS_PREFIX:
      case DDSI_RTPS_SMID_SRTPS_POSTFIX: {
        struct submsg_name buffer;
        GVTRACE ("%s", submsg_name (sm->smhdr.submessageId, &buffer));
        break;
      }
      default: {
        GVTRACE ("UNDEFINED(%x)", sm->smhdr.submessageId);
        if (sm->smhdr.submessageId <= 0x7f) {
          /* Other submessages in the 0 .. 0x7f range may be added in
             future version of the protocol -- so an undefined code
             for the implemented version of the protocol indicates a
             malformed message. */
          if (rst->protocol_version.major < DDSI_RTPS_MAJOR ||
              (rst->protocol_version.major == DDSI_RTPS_MAJOR &&
               rst->protocol_version.minor < DDSI_RTPS_MINOR_MINIMUM))
            vr = VR_MALFORMED;
        } else {
          // Ignore vendor-specific messages, including our own ones
          // so we remain interoperable with newer versions that may
          // add vendor-specific messages.
        }
        ts_for_latmeas = 0;
        break;
      }
    }
    prev_smid = sm->smhdr.submessageId;
    submsg += submsg_size;
    GVTRACE ("\n");
  }
  if (vr != VR_MALFORMED && submsg != end)
  {
    GVTRACE ("short (size %"PRIuSIZE" exp %p act %p)", submsg_size, (void *) submsg, (void *) end);
    vr = VR_MALFORMED;
  }
  ddsi_thread_state_asleep (thrst);
  assert (ddsi_thread_is_asleep ());
  defer_hb_state_fini (gv, &defer_hb_state);
  if (deferred_wakeup)
    ddsi_dqueue_enqueue_trigger (deferred_wakeup);

  if (vr != VR_MALFORMED) {
    return 0;
  } else {
    malformed_packet_received (rst->gv, msg, submsg, len, hdr->vendorid);
    return -1;
  }
}

static void handle_rtps_message (struct ddsi_thread_state * const thrst, struct ddsi_domaingv *gv, struct ddsi_tran_conn * conn, const ddsi_guid_prefix_t *guidprefix, struct ddsi_rbufpool *rbpool, struct ddsi_rmsg *rmsg, size_t sz, unsigned char *msg, const ddsi_locator_t *srcloc)
{
  ddsi_rtps_header_t *hdr = (ddsi_rtps_header_t *) msg;
  assert (ddsi_thread_is_asleep ());
  if (sz < DDSI_RTPS_MESSAGE_HEADER_SIZE || *(uint32_t *)msg != DDSI_PROTOCOLID_AS_UINT32)
  {
    /* discard packets that are really too small or don't have magic cookie */
  }
  else if (hdr->version.major != DDSI_RTPS_MAJOR || (hdr->version.major == DDSI_RTPS_MAJOR && hdr->version.minor < DDSI_RTPS_MINOR_MINIMUM))
  {
    if ((hdr->version.major == DDSI_RTPS_MAJOR && hdr->version.minor < DDSI_RTPS_MINOR_MINIMUM))
      GVTRACE ("HDR(%"PRIx32":%"PRIx32":%"PRIx32" vendor %d.%d) len %lu\n, version mismatch: %d.%d\n",
               PGUIDPREFIX (hdr->guid_prefix), hdr->vendorid.id[0], hdr->vendorid.id[1], (unsigned long) sz, hdr->version.major, hdr->version.minor);
    if (DDSI_SC_PEDANTIC_P (gv->config))
      malformed_packet_received (gv, msg, NULL, (size_t) sz, hdr->vendorid);
  }
  else
  {
    hdr->guid_prefix = ddsi_ntoh_guid_prefix (hdr->guid_prefix);

    if (gv->logconfig.c.mask & DDS_LC_TRACE)
    {
      char addrstr[DDSI_LOCSTRLEN];
      ddsi_locator_to_string(addrstr, sizeof(addrstr), srcloc);
      GVTRACE ("HDR(%"PRIx32":%"PRIx32":%"PRIx32" vendor %d.%d) len %lu from %s\n",
               PGUIDPREFIX (hdr->guid_prefix), hdr->vendorid.id[0], hdr->vendorid.id[1], (unsigned long) sz, addrstr);
    }
    ddsi_rtps_msg_state_t res = ddsi_security_decode_rtps_message (thrst, gv, &rmsg, &hdr, &msg, &sz, rbpool, conn->m_stream);
    if (res != DDSI_RTPS_MSG_STATE_ERROR)
    {
      handle_submsg_sequence (thrst, gv, conn, srcloc, ddsrt_time_wallclock (), ddsrt_time_elapsed (), &hdr->guid_prefix, guidprefix, msg, (size_t) sz, msg + DDSI_RTPS_MESSAGE_HEADER_SIZE, rmsg, res == DDSI_RTPS_MSG_STATE_ENCODED);
    }
  }
}

void ddsi_handle_rtps_message (struct ddsi_thread_state * const thrst, struct ddsi_domaingv *gv, struct ddsi_tran_conn * conn, const ddsi_guid_prefix_t *guidprefix, struct ddsi_rbufpool *rbpool, struct ddsi_rmsg *rmsg, size_t sz, unsigned char *msg, const ddsi_locator_t *srcloc)
{
  handle_rtps_message (thrst, gv, conn, guidprefix, rbpool, rmsg, sz, msg, srcloc);
}

static bool do_packet (struct ddsi_thread_state * const thrst, struct ddsi_domaingv *gv, struct ddsi_tran_conn * conn, const ddsi_guid_prefix_t *guidprefix, struct ddsi_rbufpool *rbpool)
{
  /* UDP max packet size is 64kB */

  const size_t maxsz = gv->config.rmsg_chunk_size < 65536 ? gv->config.rmsg_chunk_size : 65536;
  const size_t ddsi_msg_len_size = 8;
  const size_t stream_hdr_size = DDSI_RTPS_MESSAGE_HEADER_SIZE + ddsi_msg_len_size;
  ssize_t sz;
  struct ddsi_rmsg * rmsg = ddsi_rmsg_new (rbpool);
  unsigned char * buff;
  size_t buff_len = maxsz;
  ddsi_rtps_header_t * hdr;
  ddsi_locator_t srcloc;

  if (rmsg == NULL)
  {
    return false;
  }

  DDSRT_STATIC_ASSERT (sizeof (struct ddsi_rmsg) == offsetof (struct ddsi_rmsg, chunk) + sizeof (struct ddsi_rmsg_chunk));
  buff = (unsigned char *) DDSI_RMSG_PAYLOAD (rmsg);
  hdr = (ddsi_rtps_header_t*) buff;

  if (conn->m_stream)
  {
    ddsi_rtps_msg_len_t * ml = (ddsi_rtps_msg_len_t*) (hdr + 1);

    /*
      Read in packet header to get size of packet in ddsi_rtps_msg_len_t, then read in
      remainder of packet.
    */

    /* Read in DDSI header plus MSG_LEN sub message that follows it */

    sz = ddsi_conn_read (conn, buff, stream_hdr_size, true, &srcloc);
    if (sz == 0)
    {
      /* Spurious read -- which at this point is still ok */
      ddsi_rmsg_commit (rmsg);
      return true;
    }

    /* Read in remainder of packet */

    if (sz > 0)
    {
      int swap;

      DDSRT_WARNING_MSVC_OFF(6326)
      if (ml->smhdr.flags & DDSI_RTPS_SUBMESSAGE_FLAG_ENDIANNESS)
      {
        swap = !(DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN);
      }
      else
      {
        swap =  (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN);
      }
      DDSRT_WARNING_MSVC_ON(6326)
      if (swap)
      {
        ml->length = ddsrt_bswap4u (ml->length);
      }

      if (ml->smhdr.submessageId != DDSI_RTPS_SMID_ADLINK_MSG_LEN)
      {
        malformed_packet_received (gv, buff, NULL, (size_t) sz, hdr->vendorid);
        sz = -1;
      }
      else
      {
        sz = ddsi_conn_read (conn, buff + stream_hdr_size, ml->length - stream_hdr_size, false, NULL);
        if (sz > 0)
        {
          sz = (ssize_t) ml->length;
        }
      }
    }
  }
  else
  {
    /* Get next packet */

    sz = ddsi_conn_read (conn, buff, buff_len, true, &srcloc);
  }

  if (sz > 0 && !gv->deaf)
  {
    ddsi_rmsg_setsize (rmsg, (uint32_t) sz);
    handle_rtps_message(thrst, gv, conn, guidprefix, rbpool, rmsg, (size_t) sz, buff, &srcloc);
  }
  ddsi_rmsg_commit (rmsg);
  return (sz > 0);
}

struct local_participant_desc
{
  struct ddsi_tran_conn * m_conn;
  ddsi_guid_prefix_t guid_prefix;
};

static int local_participant_cmp (const void *va, const void *vb)
{
  const struct local_participant_desc *a = va;
  const struct local_participant_desc *b = vb;
  ddsrt_socket_t h1 = ddsi_conn_handle (a->m_conn);
  ddsrt_socket_t h2 = ddsi_conn_handle (b->m_conn);
  return (h1 == h2) ? 0 : (h1 < h2) ? -1 : 1;
}

static size_t dedup_sorted_array (void *base, size_t nel, size_t width, int (*compar) (const void *, const void *))
{
  if (nel <= 1)
    return nel;
  else
  {
    char * const end = (char *) base + nel * width;
    char *last_unique = base;
    char *cursor = (char *) base + width;
    size_t n_unique = 1;
    while (cursor != end)
    {
      if (compar (cursor, last_unique) != 0)
      {
        n_unique++;
        last_unique += width;
        if (last_unique != cursor)
          memcpy (last_unique, cursor, width);
      }
      cursor += width;
    }
    return n_unique;
  }
}

struct local_participant_set {
  struct local_participant_desc *ps;
  uint32_t nps;
  uint32_t gen;
};

static void local_participant_set_init (struct local_participant_set *lps, ddsrt_atomic_uint32_t *ppset_generation)
{
  lps->ps = NULL;
  lps->nps = 0;
  lps->gen = ddsrt_atomic_ld32 (ppset_generation) - 1;
}

static void local_participant_set_fini (struct local_participant_set *lps)
{
  ddsrt_free (lps->ps);
}

static void rebuild_local_participant_set (struct ddsi_thread_state * const thrst, struct ddsi_domaingv *gv, struct local_participant_set *lps)
{
  struct ddsi_entity_enum_participant est;
  struct ddsi_participant *pp;
  unsigned nps_alloc;
  GVTRACE ("pp set gen changed: local %"PRIu32" global %"PRIu32"\n", lps->gen, ddsrt_atomic_ld32 (&gv->participant_set_generation));
  ddsi_thread_state_awake_fixed_domain (thrst);
 restart:
  lps->gen = ddsrt_atomic_ld32 (&gv->participant_set_generation);
  /* Actual local set of participants may never be older than the
     local generation count => membar to guarantee the ordering */
  ddsrt_atomic_fence_acq ();
  nps_alloc = gv->nparticipants;
  ddsrt_free (lps->ps);
  lps->nps = 0;
  lps->ps = (nps_alloc == 0) ? NULL : ddsrt_malloc (nps_alloc * sizeof (*lps->ps));
  ddsi_entidx_enum_participant_init (&est, gv->entity_index);
  while ((pp = ddsi_entidx_enum_participant_next (&est)) != NULL)
  {
    if (lps->nps == nps_alloc)
    {
      /* New participants may get added while we do this (or
         existing ones removed), so we may have to restart if it
         turns out we didn't allocate enough memory [an
         alternative would be to realloc on the fly]. */
      ddsi_entidx_enum_participant_fini (&est);
      GVTRACE ("  need more memory - restarting\n");
      goto restart;
    }
    else
    {
      lps->ps[lps->nps].m_conn = pp->m_conn;
      lps->ps[lps->nps].guid_prefix = pp->e.guid.prefix;
      GVTRACE ("  pp "PGUIDFMT" handle %"PRIdSOCK"\n", PGUID (pp->e.guid), ddsi_conn_handle (pp->m_conn));
      lps->nps++;
    }
  }
  ddsi_entidx_enum_participant_fini (&est);

  /* There is a (very small) probability of a participant
     disappearing and new one appearing with the same socket while
     we are enumerating, which would cause us to misinterpret the
     participant guid prefix for a directed packet without an
     explicit destination. Membar because we must have completed
     the loop before testing the generation again. */
  ddsrt_atomic_fence_acq ();
  if (lps->gen != ddsrt_atomic_ld32 (&gv->participant_set_generation))
  {
    GVTRACE ("  set changed - restarting\n");
    goto restart;
  }
  ddsi_thread_state_asleep (thrst);

  /* The definition of the hash enumeration allows visiting one
     participant multiple times, so guard against that, too.  Note
     that there's no requirement that the set be ordered on
     socket: it is merely a convenient way of finding
     duplicates. */
  if (lps->nps)
  {
    qsort (lps->ps, lps->nps, sizeof (*lps->ps), local_participant_cmp);
    lps->nps = (unsigned) dedup_sorted_array (lps->ps, lps->nps, sizeof (*lps->ps), local_participant_cmp);
  }
  GVTRACE ("  nparticipants %"PRIu32"\n", lps->nps);
}

uint32_t ddsi_listen_thread (struct ddsi_tran_listener *listener)
{
  struct ddsi_domaingv *gv = listener->m_base.gv;
  struct ddsi_tran_conn * conn;

  while (ddsrt_atomic_ld32 (&gv->rtps_keepgoing))
  {
    /* Accept connection from listener */

    conn = ddsi_listener_accept (listener);
    if (conn)
    {
      ddsi_sock_waitset_add (gv->recv_threads[0].arg.u.many.ws, conn);
      ddsi_sock_waitset_trigger (gv->recv_threads[0].arg.u.many.ws);
    }
  }
  return 0;
}

static int recv_thread_waitset_add_conn (struct ddsi_sock_waitset * ws, struct ddsi_tran_conn * conn)
{
  if (conn == NULL)
    return 0;
  else
  {
    struct ddsi_domaingv *gv = conn->m_base.gv;
    for (uint32_t i = 0; i < gv->n_recv_threads; i++)
      if (gv->recv_threads[i].arg.mode == DDSI_RTM_SINGLE && gv->recv_threads[i].arg.u.single.conn == conn)
        return 0;
    return ddsi_sock_waitset_add (ws, conn);
  }
}

void ddsi_trigger_recv_threads (const struct ddsi_domaingv *gv)
{
  for (uint32_t i = 0; i < gv->n_recv_threads; i++)
  {
    if (gv->recv_threads[i].thrst == NULL)
      continue;
    switch (gv->recv_threads[i].arg.mode)
    {
      case DDSI_RTM_SINGLE: {
        char buf[DDSI_LOCSTRLEN];
        char dummy = 0;
        const ddsi_locator_t *dst = gv->recv_threads[i].arg.u.single.loc;
        ddsrt_iovec_t iov;
        iov.iov_base = &dummy;
        iov.iov_len = 1;
        GVTRACE ("ddsi_trigger_recv_threads: %"PRIu32" single %s\n", i, ddsi_locator_to_string (buf, sizeof (buf), dst));
        // all sockets listen on at least the interfaces used for transmitting (at least for now)
        ddsi_conn_write (gv->xmit_conns[0], dst, 1, &iov, 0);
        break;
      }
      case DDSI_RTM_MANY: {
        GVTRACE ("ddsi_trigger_recv_threads: %"PRIu32" many %p\n", i, (void *) gv->recv_threads[i].arg.u.many.ws);
        ddsi_sock_waitset_trigger (gv->recv_threads[i].arg.u.many.ws);
        break;
      }
    }
  }
}

uint32_t ddsi_recv_thread (void *vrecv_thread_arg)
{
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  struct ddsi_recv_thread_arg *recv_thread_arg = vrecv_thread_arg;
  struct ddsi_domaingv * const gv = recv_thread_arg->gv;
  struct ddsi_rbufpool *rbpool = recv_thread_arg->rbpool;
  struct ddsi_sock_waitset * waitset = recv_thread_arg->mode == DDSI_RTM_MANY ? recv_thread_arg->u.many.ws : NULL;
  ddsrt_mtime_t next_thread_cputime = { 0 };

  ddsi_rbufpool_setowner (rbpool, ddsrt_thread_self ());
  if (waitset == NULL)
  {
    struct ddsi_tran_conn *conn = recv_thread_arg->u.single.conn;
    while (ddsrt_atomic_ld32 (&gv->rtps_keepgoing))
    {
      LOG_THREAD_CPUTIME (&gv->logconfig, next_thread_cputime);
      (void) do_packet (thrst, gv, conn, NULL, rbpool);
    }
  }
  else
  {
    struct local_participant_set lps;
    unsigned num_fixed = 0, num_fixed_uc = 0;
    struct ddsi_sock_waitset_ctx * ctx;
    local_participant_set_init (&lps, &gv->participant_set_generation);
    if (gv->m_factory->m_connless)
    {
      int rc;
      if ((rc = recv_thread_waitset_add_conn (waitset, gv->disc_conn_uc)) < 0)
        DDS_FATAL("recv_thread: failed to add disc_conn_uc to waitset\n");
      num_fixed_uc += (unsigned)rc;
      if ((rc = recv_thread_waitset_add_conn (waitset, gv->data_conn_uc)) < 0)
        DDS_FATAL("recv_thread: failed to add data_conn_uc to waitset\n");
      num_fixed_uc += (unsigned)rc;
      num_fixed += num_fixed_uc;
      if ((rc = recv_thread_waitset_add_conn (waitset, gv->disc_conn_mc)) < 0)
        DDS_FATAL("recv_thread: failed to add disc_conn_mc to waitset\n");
      num_fixed += (unsigned)rc;
      if ((rc = recv_thread_waitset_add_conn (waitset, gv->data_conn_mc)) < 0)
        DDS_FATAL("recv_thread: failed to add data_conn_mc to waitset\n");
      num_fixed += (unsigned)rc;

      // OpenDDS doesn't respect the locator lists and insists on sending to the
      // socket it received packets from
      for (int i = 0; i < gv->n_interfaces; i++)
      {
        // Iceoryx gets added as a pseudo-interface but there's no socket to wait
        // for input on
        if (ddsi_conn_handle (gv->xmit_conns[i]) == DDSRT_INVALID_SOCKET)
          continue;
        if ((rc = recv_thread_waitset_add_conn (waitset, gv->xmit_conns[i])) < 0)
          DDS_FATAL("recv_thread: failed to add transmit_conn[%d] to waitset\n", i);
        num_fixed += (unsigned)rc;
      }
    }

    while (ddsrt_atomic_ld32 (&gv->rtps_keepgoing))
    {
      int rebuildws = 0;
      LOG_THREAD_CPUTIME (&gv->logconfig, next_thread_cputime);
      if (gv->config.many_sockets_mode != DDSI_MSM_MANY_UNICAST)
      {
        /* no other sockets to check */
      }
      else if (ddsrt_atomic_ld32 (&gv->participant_set_generation) != lps.gen)
      {
        rebuildws = 1;
      }

      if (rebuildws && waitset && gv->config.many_sockets_mode == DDSI_MSM_MANY_UNICAST)
      {
        /* first rebuild local participant set - unless someone's toggling "deafness", this
         only happens when the participant set has changed, so might as well rebuild it */
        rebuild_local_participant_set (thrst, gv, &lps);
        ddsi_sock_waitset_purge (waitset, num_fixed);
        for (uint32_t i = 0; i < lps.nps; i++)
        {
          if (lps.ps[i].m_conn)
            ddsi_sock_waitset_add (waitset, lps.ps[i].m_conn);
        }
      }

      if ((ctx = ddsi_sock_waitset_wait (waitset)) != NULL)
      {
        int idx;
        struct ddsi_tran_conn * conn;
        while ((idx = ddsi_sock_waitset_next_event (ctx, &conn)) >= 0)
        {
          const ddsi_guid_prefix_t *guid_prefix;
          if (((unsigned)idx < num_fixed) || gv->config.many_sockets_mode != DDSI_MSM_MANY_UNICAST)
            guid_prefix = NULL;
          else
            guid_prefix = &lps.ps[(unsigned)idx - num_fixed].guid_prefix;
          /* Process message and clean out connection if failed or closed */
          if (!do_packet (thrst, gv, conn, guid_prefix, rbpool) && !conn->m_connless)
            ddsi_conn_free (conn);
        }
      }
    }
    local_participant_set_fini (&lps);
  }

  GVTRACE ("done\n");
  return 0;
}
