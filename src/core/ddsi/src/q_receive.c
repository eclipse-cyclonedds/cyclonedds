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
#include "dds__stream.h"
#include "dds/ddsi/q_protocol.h"
#include "dds/ddsi/q_rtps.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/q_plist.h"
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_lat_estim.h"
#include "dds/ddsi/q_bitset.h"
#include "dds/ddsi/q_xevent.h"
#include "dds/ddsi/q_addrset.h"
#include "dds/ddsi/q_ddsi_discovery.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/q_ephash.h"
#include "dds/ddsi/q_lease.h"
#include "dds/ddsi/q_gc.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsi/q_receive.h"
#include "dds/ddsi/ddsi_rhc.h"

#include "dds/ddsi/q_transmit.h"
#include "dds/ddsi/q_globals.h"
#include "dds/ddsi/q_init.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_mcgroup.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_serdata_default.h" /* FIXME: get rid of this */

#include "dds/ddsi/sysdeps.h"
#include "dds__whc.h"

/*
Notes:

- for now, the safer option is usually chosen: hold a lock even if it
  isn't strictly necessary in the particular configuration we have
  (such as one receive thread vs. multiple receive threads)

- nn_dqueue_enqueue may be called with pwr->e.lock held

- deliver_user_data_synchronously may be called with pwr->e.lock held,
  which is needed if IN-ORDER synchronous delivery is desired when
  there are also multiple receive threads

- deliver_user_data gets passed in whether pwr->e.lock is held on entry

*/

static void deliver_user_data_synchronously (struct nn_rsample_chain *sc, const ddsi_guid_t *rdguid);

static void maybe_set_reader_in_sync (struct proxy_writer *pwr, struct pwr_rd_match *wn, seqno_t last_deliv_seq)
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
          local_reader_ary_setfastpath_ok (&pwr->rdary, true);
      }
      break;
    case PRMSS_OUT_OF_SYNC:
      assert (nn_reorder_next_seq (wn->u.not_in_sync.reorder) <= nn_reorder_next_seq (pwr->reorder));
      if (pwr->have_seen_heartbeat && nn_reorder_next_seq (wn->u.not_in_sync.reorder) == nn_reorder_next_seq (pwr->reorder))
      {
        ETRACE (pwr, " msr_in_sync("PGUIDFMT" out-of-sync to tlcatchup)", PGUID (wn->rd_guid));
        wn->in_sync = PRMSS_TLCATCHUP;
        maybe_set_reader_in_sync (pwr, wn, last_deliv_seq);
      }
      break;
  }
}

static int valid_sequence_number_set (const nn_sequence_number_set_header_t *snset)
{
  return (fromSN (snset->bitmap_base) > 0 && snset->numbits <= 256);
}

static int valid_fragment_number_set (const nn_fragment_number_set_header_t *fnset)
{
  return (fnset->bitmap_base > 0 && fnset->numbits <= 256);
}

static int valid_AckNack (const struct receiver_state *rst, AckNack_t *msg, size_t size, int byteswap)
{
  nn_count_t *count; /* this should've preceded the bitmap */
  if (size < ACKNACK_SIZE (0))
    /* note: sizeof(*msg) is not sufficient verification, but it does
       suffice for verifying all fixed header fields exist */
    return 0;
  if (byteswap)
  {
    bswap_sequence_number_set_hdr (&msg->readerSNState);
    /* bits[], count deferred until validation of fixed part */
  }
  msg->readerId = nn_ntoh_entityid (msg->readerId);
  msg->writerId = nn_ntoh_entityid (msg->writerId);
  /* Validation following 8.3.7.1.3 + 8.3.5.5 */
  if (!valid_sequence_number_set (&msg->readerSNState))
  {
    /* FastRTPS sends invalid pre-emptive ACKs -- patch the message so we can process it */
    if (! NN_STRICT_P (rst->gv->config) && vendor_is_eprosima (rst->vendor) &&
        fromSN (msg->readerSNState.bitmap_base) == 0 && msg->readerSNState.numbits == 0)
      msg->readerSNState.bitmap_base = toSN (1);
    else
      return 0;
  }
  /* Given the number of bits, we can compute the size of the AckNack
     submessage, and verify that the submessage is large enough */
  if (size < ACKNACK_SIZE (msg->readerSNState.numbits))
    return 0;
  count = (nn_count_t *) ((char *) &msg->bits + NN_SEQUENCE_NUMBER_SET_BITS_SIZE (msg->readerSNState.numbits));
  if (byteswap)
  {
    bswap_sequence_number_set_bitmap (&msg->readerSNState, msg->bits);
    *count = ddsrt_bswap4 (*count);
  }
  return 1;
}

static int valid_Gap (Gap_t *msg, size_t size, int byteswap)
{
  if (size < GAP_SIZE (0))
    return 0;
  if (byteswap)
  {
    bswapSN (&msg->gapStart);
    bswap_sequence_number_set_hdr (&msg->gapList);
  }
  msg->readerId = nn_ntoh_entityid (msg->readerId);
  msg->writerId = nn_ntoh_entityid (msg->writerId);
  if (fromSN (msg->gapStart) <= 0)
    return 0;
  if (!valid_sequence_number_set (&msg->gapList))
    return 0;
  /* One would expect gapStart < gapList.base, but it is not required by
     the spec for the GAP to valid. */
  if (size < GAP_SIZE (msg->gapList.numbits))
    return 0;
  if (byteswap)
    bswap_sequence_number_set_bitmap (&msg->gapList, msg->bits);
  return 1;
}

static int valid_InfoDST (InfoDST_t *msg, size_t size, UNUSED_ARG (int byteswap))
{
  if (size < sizeof (*msg))
    return 0;
  return 1;
}

static int valid_InfoSRC (InfoSRC_t *msg, size_t size, UNUSED_ARG (int byteswap))
{
  if (size < sizeof (*msg))
    return 0;
  return 1;
}

static int valid_InfoTS (InfoTS_t *msg, size_t size, int byteswap)
{
  assert (sizeof (SubmessageHeader_t) <= size);
  if (msg->smhdr.flags & INFOTS_INVALIDATE_FLAG)
    return 1;
  else if (size < sizeof (InfoTS_t))
    return 0;
  else
  {
    if (byteswap)
    {
      msg->time.seconds = ddsrt_bswap4 (msg->time.seconds);
      msg->time.fraction = ddsrt_bswap4u (msg->time.fraction);
    }
    return valid_ddsi_timestamp (msg->time);
  }
}

static int valid_Heartbeat (Heartbeat_t *msg, size_t size, int byteswap)
{
  if (size < sizeof (*msg))
    return 0;
  if (byteswap)
  {
    bswapSN (&msg->firstSN);
    bswapSN (&msg->lastSN);
    msg->count = ddsrt_bswap4 (msg->count);
  }
  msg->readerId = nn_ntoh_entityid (msg->readerId);
  msg->writerId = nn_ntoh_entityid (msg->writerId);
  /* Validation following 8.3.7.5.3; lastSN + 1 == firstSN: no data */
  if (fromSN (msg->firstSN) <= 0 || fromSN (msg->lastSN) + 1 < fromSN (msg->firstSN))
    return 0;
  return 1;
}

static int valid_HeartbeatFrag (HeartbeatFrag_t *msg, size_t size, int byteswap)
{
  if (size < sizeof (*msg))
    return 0;
  if (byteswap)
  {
    bswapSN (&msg->writerSN);
    msg->lastFragmentNum = ddsrt_bswap4u (msg->lastFragmentNum);
    msg->count = ddsrt_bswap4 (msg->count);
  }
  msg->readerId = nn_ntoh_entityid (msg->readerId);
  msg->writerId = nn_ntoh_entityid (msg->writerId);
  if (fromSN (msg->writerSN) <= 0 || msg->lastFragmentNum == 0)
    return 0;
  return 1;
}

static int valid_NackFrag (NackFrag_t *msg, size_t size, int byteswap)
{
  nn_count_t *count; /* this should've preceded the bitmap */
  if (size < NACKFRAG_SIZE (0))
    /* note: sizeof(*msg) is not sufficient verification, but it does
       suffice for verifying all fixed header fields exist */
    return 0;
  if (byteswap)
  {
    bswapSN (&msg->writerSN);
    bswap_fragment_number_set_hdr (&msg->fragmentNumberState);
    /* bits[], count deferred until validation of fixed part */
  }
  msg->readerId = nn_ntoh_entityid (msg->readerId);
  msg->writerId = nn_ntoh_entityid (msg->writerId);
  /* Validation following 8.3.7.1.3 + 8.3.5.5 */
  if (!valid_fragment_number_set (&msg->fragmentNumberState))
    return 0;
  /* Given the number of bits, we can compute the size of the Nackfrag
     submessage, and verify that the submessage is large enough */
  if (size < NACKFRAG_SIZE (msg->fragmentNumberState.numbits))
    return 0;
  count = (nn_count_t *) ((char *) &msg->bits + NN_FRAGMENT_NUMBER_SET_BITS_SIZE (msg->fragmentNumberState.numbits));
  if (byteswap)
  {
    bswap_fragment_number_set_bitmap (&msg->fragmentNumberState, msg->bits);
    *count = ddsrt_bswap4 (*count);
  }
  return 1;
}

static void set_sampleinfo_proxy_writer (struct nn_rsample_info *sampleinfo, ddsi_guid_t *pwr_guid)
{
  struct proxy_writer * pwr = ephash_lookup_proxy_writer_guid (sampleinfo->rst->gv->guid_hash, pwr_guid);
  sampleinfo->pwr = pwr;
}

static int valid_Data (const struct receiver_state *rst, struct nn_rmsg *rmsg, Data_t *msg, size_t size, int byteswap, struct nn_rsample_info *sampleinfo, unsigned char **payloadp)
{
  /* on success: sampleinfo->{seq,rst,statusinfo,pt_wr_info_zoff,bswap,complex_qos} all set */
  ddsi_guid_t pwr_guid;
  unsigned char *ptr;

  if (size < sizeof (*msg))
    return 0; /* too small even for fixed fields */
  /* D=1 && K=1 is invalid in this version of the protocol */
  if ((msg->x.smhdr.flags & (DATA_FLAG_DATAFLAG | DATA_FLAG_KEYFLAG)) ==
      (DATA_FLAG_DATAFLAG | DATA_FLAG_KEYFLAG))
    return 0;
  if (byteswap)
  {
    msg->x.extraFlags = ddsrt_bswap2u (msg->x.extraFlags);
    msg->x.octetsToInlineQos = ddsrt_bswap2u (msg->x.octetsToInlineQos);
    bswapSN (&msg->x.writerSN);
  }
  msg->x.readerId = nn_ntoh_entityid (msg->x.readerId);
  msg->x.writerId = nn_ntoh_entityid (msg->x.writerId);
  pwr_guid.prefix = rst->src_guid_prefix;
  pwr_guid.entityid = msg->x.writerId;

  sampleinfo->rst = (struct receiver_state *) rst; /* drop const */
  set_sampleinfo_proxy_writer (sampleinfo, &pwr_guid);
  sampleinfo->seq = fromSN (msg->x.writerSN);
  sampleinfo->fragsize = 0; /* for unfragmented data, fragsize = 0 works swell */

  if (sampleinfo->seq <= 0 && sampleinfo->seq != NN_SEQUENCE_NUMBER_UNKNOWN)
    return 0;

  if ((msg->x.smhdr.flags & (DATA_FLAG_INLINE_QOS | DATA_FLAG_DATAFLAG | DATA_FLAG_KEYFLAG)) == 0)
  {
    /* no QoS, no payload, so octetsToInlineQos will never be used
       though one would expect octetsToInlineQos and size to be in
       agreement or octetsToInlineQos to be 0 or so */
    *payloadp = NULL;
    sampleinfo->size = 0; /* size is full payload size, no payload & unfragmented => size = 0 */
    sampleinfo->statusinfo = 0;
    sampleinfo->pt_wr_info_zoff = NN_OFF_TO_ZOFF (0);
    sampleinfo->complex_qos = 0;
    return 1;
  }

  /* QoS and/or payload, so octetsToInlineQos must be within the
     msg; since the serialized data and serialized parameter lists
     have a 4 byte header, that one, too must fit */
  if (offsetof (Data_DataFrag_common_t, octetsToInlineQos) + sizeof (msg->x.octetsToInlineQos) + msg->x.octetsToInlineQos + 4 > size)
    return 0;

  ptr = (unsigned char *) msg + offsetof (Data_DataFrag_common_t, octetsToInlineQos) + sizeof (msg->x.octetsToInlineQos) + msg->x.octetsToInlineQos;
  if (msg->x.smhdr.flags & DATA_FLAG_INLINE_QOS)
  {
    nn_plist_src_t src;
    src.protocol_version = rst->protocol_version;
    src.vendorid = rst->vendor;
    src.encoding = (msg->x.smhdr.flags & SMFLAG_ENDIANNESS) ? PL_CDR_LE : PL_CDR_BE;
    src.buf = ptr;
    src.bufsz = (unsigned) ((unsigned char *) msg + size - src.buf); /* end of message, that's all we know */
    src.factory = NULL;
    src.logconfig = &rst->gv->logconfig;
    /* just a quick scan, gathering only what we _really_ need */
    if ((ptr = nn_plist_quickscan (sampleinfo, rmsg, &src)) == NULL)
      return 0;
  }
  else
  {
    sampleinfo->statusinfo = 0;
    sampleinfo->pt_wr_info_zoff = NN_OFF_TO_ZOFF (0);
    sampleinfo->complex_qos = 0;
  }

  if (!(msg->x.smhdr.flags & (DATA_FLAG_DATAFLAG | DATA_FLAG_KEYFLAG)))
  {
    /*TRACE (("no payload\n"));*/
    *payloadp = NULL;
    sampleinfo->size = 0;
  }
  else if ((size_t) ((char *) ptr + 4 - (char *) msg) > size)
  {
    /* no space for the header */
    return 0;
  }
  else
  {
    struct CDRHeader *hdr;
    sampleinfo->size = (uint32_t) ((char *) msg + size - (char *) ptr);
    *payloadp = ptr;
    hdr = (struct CDRHeader *) ptr;
    switch (hdr->identifier)
    {
      case CDR_BE:
      case PL_CDR_BE:
      {
        sampleinfo->bswap = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN ? 1 : 0);
        break;
      }
      case CDR_LE:
      case PL_CDR_LE:
      {
        sampleinfo->bswap = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN ? 0 : 1);
        break;
      }
      default:
        return 0;
    }
  }
  return 1;
}

static int valid_DataFrag (const struct receiver_state *rst, struct nn_rmsg *rmsg, DataFrag_t *msg, size_t size, int byteswap, struct nn_rsample_info *sampleinfo, unsigned char **payloadp)
{
  /* on success: sampleinfo->{rst,statusinfo,pt_wr_info_zoff,bswap,complex_qos} all set */
  uint32_t payloadsz;
  ddsi_guid_t pwr_guid;
  unsigned char *ptr;

  if (size < sizeof (*msg))
    return 0; /* too small even for fixed fields */

  if (byteswap)
  {
    msg->x.extraFlags = ddsrt_bswap2u (msg->x.extraFlags);
    msg->x.octetsToInlineQos = ddsrt_bswap2u (msg->x.octetsToInlineQos);
    bswapSN (&msg->x.writerSN);
    msg->fragmentStartingNum = ddsrt_bswap4u (msg->fragmentStartingNum);
    msg->fragmentsInSubmessage = ddsrt_bswap2u (msg->fragmentsInSubmessage);
    msg->fragmentSize = ddsrt_bswap2u (msg->fragmentSize);
    msg->sampleSize = ddsrt_bswap4u (msg->sampleSize);
  }
  msg->x.readerId = nn_ntoh_entityid (msg->x.readerId);
  msg->x.writerId = nn_ntoh_entityid (msg->x.writerId);
  pwr_guid.prefix = rst->src_guid_prefix;
  pwr_guid.entityid = msg->x.writerId;

  if (NN_STRICT_P (rst->gv->config) && msg->fragmentSize <= 1024 && msg->fragmentSize < rst->gv->config.fragment_size)
  {
    /* Spec says fragments must > 1kB; not allowing 1024 bytes is IMHO
       totally ridiculous; and I really don't care how small the
       fragments anyway. And we're certainly not going too fail the
       message if it is as least as large as the configured fragment
       size. */
    return 0;
  }
  if (msg->fragmentSize == 0 || msg->fragmentStartingNum == 0 || msg->fragmentsInSubmessage == 0)
    return 0;
  if (NN_STRICT_P (rst->gv->config) && msg->fragmentSize >= msg->sampleSize)
    /* may not fragment if not needed -- but I don't care */
    return 0;
  if ((msg->fragmentStartingNum + msg->fragmentsInSubmessage - 2) * msg->fragmentSize >= msg->sampleSize)
    /* starting offset of last fragment must be within sample, note:
       fragment numbers are 1-based */
    return 0;

  sampleinfo->rst = (struct receiver_state *) rst; /* drop const */
  set_sampleinfo_proxy_writer (sampleinfo, &pwr_guid);
  sampleinfo->seq = fromSN (msg->x.writerSN);
  sampleinfo->fragsize = msg->fragmentSize;
  sampleinfo->size = msg->sampleSize;

  if (sampleinfo->seq <= 0 && sampleinfo->seq != NN_SEQUENCE_NUMBER_UNKNOWN)
    return 0;

  /* QoS and/or payload, so octetsToInlineQos must be within the msg;
     since the serialized data and serialized parameter lists have a 4
     byte header, that one, too must fit */
  if (offsetof (Data_DataFrag_common_t, octetsToInlineQos) + sizeof (msg->x.octetsToInlineQos) + msg->x.octetsToInlineQos + 4 > size)
    return 0;

  /* Quick check inline QoS if present, collecting a little bit of
     information on it.  The only way to find the payload offset if
     inline QoSs are present. */
  ptr = (unsigned char *) msg + offsetof (Data_DataFrag_common_t, octetsToInlineQos) + sizeof (msg->x.octetsToInlineQos) + msg->x.octetsToInlineQos;
  if (msg->x.smhdr.flags & DATAFRAG_FLAG_INLINE_QOS)
  {
    nn_plist_src_t src;
    src.protocol_version = rst->protocol_version;
    src.vendorid = rst->vendor;
    src.encoding = (msg->x.smhdr.flags & SMFLAG_ENDIANNESS) ? PL_CDR_LE : PL_CDR_BE;
    src.buf = ptr;
    src.bufsz = (unsigned) ((unsigned char *) msg + size - src.buf); /* end of message, that's all we know */
    src.factory = NULL;
    src.logconfig = &rst->gv->logconfig;
    /* just a quick scan, gathering only what we _really_ need */
    if ((ptr = nn_plist_quickscan (sampleinfo, rmsg, &src)) == NULL)
      return 0;
  }
  else
  {
    sampleinfo->statusinfo = 0;
    sampleinfo->pt_wr_info_zoff = NN_OFF_TO_ZOFF (0);
    sampleinfo->complex_qos = 0;
  }

  *payloadp = ptr;
  payloadsz = (uint32_t) ((char *) msg + size - (char *) ptr);
  if ((uint32_t) msg->fragmentsInSubmessage * msg->fragmentSize <= payloadsz)
    ; /* all spec'd fragments fit in payload */
  else if ((uint32_t) (msg->fragmentsInSubmessage - 1) * msg->fragmentSize >= payloadsz)
    return 0; /* I can live with a short final fragment, but _only_ the final one */
  else if ((uint32_t) (msg->fragmentStartingNum - 1) * msg->fragmentSize + payloadsz >= msg->sampleSize)
    ; /* final fragment is long enough to cover rest of sample */
  else
    return 0;
  if (msg->fragmentStartingNum == 1)
  {
    struct CDRHeader *hdr = (struct CDRHeader *) ptr;
    if ((size_t) ((char *) ptr + 4 - (char *) msg) > size)
    {
      /* no space for the header -- technically, allowing small
         fragments would also mean allowing a partial header, but I
         prefer this */
      return 0;
    }
    switch (hdr->identifier)
    {
      case CDR_BE:
      case PL_CDR_BE:
      {
        sampleinfo->bswap = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN ? 1 : 0);
        break;
      }
      case CDR_LE:
      case PL_CDR_LE:
      {
        sampleinfo->bswap = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN ? 0 : 1);
        break;
      }
      default:
        return 0;
    }
  }
  return 1;
}

static int add_Gap (struct nn_xmsg *msg, struct writer *wr, struct proxy_reader *prd, seqno_t start, seqno_t base, uint32_t numbits, const uint32_t *bits)
{
  struct nn_xmsg_marker sm_marker;
  Gap_t *gap;
  ASSERT_MUTEX_HELD (wr->e.lock);
  gap = nn_xmsg_append (msg, &sm_marker, GAP_SIZE (numbits));
  nn_xmsg_submsg_init (msg, sm_marker, SMID_GAP);
  gap->readerId = nn_hton_entityid (prd->e.guid.entityid);
  gap->writerId = nn_hton_entityid (wr->e.guid.entityid);
  gap->gapStart = toSN (start);
  gap->gapList.bitmap_base = toSN (base);
  gap->gapList.numbits = numbits;
  memcpy (gap->bits, bits, NN_SEQUENCE_NUMBER_SET_BITS_SIZE (numbits));
  nn_xmsg_submsg_setnext (msg, sm_marker);
  return 0;
}

static void force_heartbeat_to_peer (struct writer *wr, const struct whc_state *whcst, struct proxy_reader *prd, int hbansreq)
{
  struct nn_xmsg *m;

  ASSERT_MUTEX_HELD (&wr->e.lock);
  assert (wr->reliable);

  m = nn_xmsg_new (wr->e.gv->xmsgpool, &wr->e.guid.prefix, 0, NN_XMSG_KIND_CONTROL);
  if (nn_xmsg_setdstPRD (m, prd) < 0)
  {
    /* If we don't have an address, give up immediately */
    nn_xmsg_free (m);
    return;
  }

  /* Send a Heartbeat just to this peer */
  add_Heartbeat (m, wr, whcst, hbansreq, prd->e.guid.entityid, 0);
  ETRACE (wr, "force_heartbeat_to_peer: "PGUIDFMT" -> "PGUIDFMT" - queue for transmit\n",
          PGUID (wr->e.guid), PGUID (prd->e.guid));
  qxev_msg (wr->evq, m);
}

static seqno_t grow_gap_to_next_seq (const struct writer *wr, seqno_t seq)
{
  seqno_t next_seq = whc_next_seq (wr->whc, seq - 1);
  seqno_t seq_xmit = writer_read_seq_xmit (wr);
  if (next_seq == MAX_SEQ_NUMBER) /* no next sample */
    return seq_xmit + 1;
  else if (next_seq > seq_xmit)  /* next is beyond last actually transmitted */
    return seq_xmit;
  else /* next one is already visible in the outside world */
    return next_seq;
}

static int acknack_is_nack (const AckNack_t *msg)
{
  unsigned x = 0, mask;
  int i;
  if (msg->readerSNState.numbits == 0)
    /* Disallowed by the spec, but RTI appears to require them (and so
       even we generate them) */
    return 0;
  for (i = 0; i < (int) NN_SEQUENCE_NUMBER_SET_BITS_SIZE (msg->readerSNState.numbits) / 4 - 1; i++)
    x |= msg->bits[i];
  if ((msg->readerSNState.numbits % 32) == 0)
    mask = ~0u;
  else
    mask = ~(~0u >> (msg->readerSNState.numbits % 32));
  x |= msg->bits[i] & mask;
  return x != 0;
}

static int accept_ack_or_hb_w_timeout (nn_count_t new_count, nn_count_t *exp_count, nn_etime_t tnow, nn_etime_t *t_last_accepted, int force_accept)
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
     isn't clear whether that is about connections or entities, and
     besides there is an issue with the wrap around after 2**31-1.

     This combined procedure should give the best of all worlds, and
     is not more expensive in the common case. */
  const int64_t timeout = 2 * T_SECOND;

  if (new_count < *exp_count && tnow.v - t_last_accepted->v < timeout && !force_accept)
    return 0;

  *exp_count = new_count + 1;
  *t_last_accepted = tnow;
  return 1;
}

static int handle_AckNack (struct receiver_state *rst, nn_etime_t tnow, const AckNack_t *msg, nn_wctime_t timestamp)
{
  struct proxy_reader *prd;
  struct wr_prd_match *rn;
  struct writer *wr;
  ddsi_guid_t src, dst;
  seqno_t seqbase;
  seqno_t seq_xmit;
  nn_count_t *countp;
  seqno_t gapstart = -1, gapend = -1;
  unsigned gapnumbits = 0;
  uint32_t gapbits[256 / 32];
  int accelerate_rexmit = 0;
  int is_pure_ack;
  int is_pure_nonhist_ack;
  int is_preemptive_ack;
  int enqueued;
  unsigned numbits;
  uint32_t msgs_sent, msgs_lost;
  seqno_t max_seq_in_reply;
  struct whc_node *deferred_free_list = NULL;
  struct whc_state whcst;
  int hb_sent_in_response = 0;
  memset (gapbits, 0, sizeof (gapbits));
  countp = (nn_count_t *) ((char *) msg + offsetof (AckNack_t, bits) + NN_SEQUENCE_NUMBER_SET_BITS_SIZE (msg->readerSNState.numbits));
  src.prefix = rst->src_guid_prefix;
  src.entityid = msg->readerId;
  dst.prefix = rst->dst_guid_prefix;
  dst.entityid = msg->writerId;
  RSTTRACE ("ACKNACK(%s#%"PRId32":%"PRId64"/%"PRIu32":", msg->smhdr.flags & ACKNACK_FLAG_FINAL ? "F" : "",
            *countp, fromSN (msg->readerSNState.bitmap_base), msg->readerSNState.numbits);
  for (uint32_t i = 0; i < msg->readerSNState.numbits; i++)
    RSTTRACE ("%c", nn_bitset_isset (msg->readerSNState.numbits, msg->bits, i) ? '1' : '0');
  seqbase = fromSN (msg->readerSNState.bitmap_base);

  if (!rst->forme)
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT" not-for-me)", PGUID (src), PGUID (dst));
    return 1;
  }

  if ((wr = ephash_lookup_writer_guid (rst->gv->guid_hash, &dst)) == NULL)
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT"?)", PGUID (src), PGUID (dst));
    return 1;
  }
  /* Always look up the proxy reader -- even though we don't need for
     the normal pure ack steady state. If (a big "if"!) this shows up
     as a significant portion of the time, we can always rewrite it to
     only retrieve it when needed. */
  if ((prd = ephash_lookup_proxy_reader_guid (rst->gv->guid_hash, &src)) == NULL)
  {
    RSTTRACE (" "PGUIDFMT"? -> "PGUIDFMT")", PGUID (src), PGUID (dst));
    return 1;
  }

  /* liveliness is still only implemented partially (with all set to AUTOMATIC, BY_PARTICIPANT, &c.), so we simply renew the proxy participant's lease. */
  lease_renew (ddsrt_atomic_ldvoidp (&prd->c.proxypp->lease), tnow);

  if (!wr->reliable) /* note: reliability can't be changed */
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT" not a reliable writer!)", PGUID (src), PGUID (dst));
    return 1;
  }

  ddsrt_mutex_lock (&wr->e.lock);
  if ((rn = ddsrt_avl_lookup (&wr_readers_treedef, &wr->readers, &src)) == NULL)
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT" not a connection)", PGUID (src), PGUID (dst));
    goto out;
  }

  /* is_pure_nonhist ack differs from is_pure_ack in that it doesn't
     get set when only historical data is being acked, which is
     relevant to setting "has_replied_to_hb" and "assumed_in_sync". */
  is_pure_ack = !acknack_is_nack (msg);
  is_pure_nonhist_ack = is_pure_ack && seqbase - 1 >= rn->seq;
  is_preemptive_ack = seqbase <= 1 && is_pure_ack;

  wr->num_acks_received++;
  if (!is_pure_ack)
  {
    wr->num_nacks_received++;
    rn->rexmit_requests++;
  }

  if (!accept_ack_or_hb_w_timeout (*countp, &rn->next_acknack, tnow, &rn->t_acknack_accepted, is_preemptive_ack))
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
    nn_wctime_t tstamp_now = now ();
    nn_lat_estim_update (&rn->hb_to_ack_latency, tstamp_now.v - timestamp.v);
    if ((rst->gv->logconfig.c.mask & DDS_LC_TRACE) && tstamp_now.v > rn->hb_to_ack_latency_tlastlog.v + 10 * T_SECOND)
    {
      nn_lat_estim_log (DDS_LC_TRACE, &rst->gv->logconfig, NULL, &rn->hb_to_ack_latency);
      rn->hb_to_ack_latency_tlastlog = tstamp_now;
    }
  }

  /* First, the ACK part: if the AckNack advances the highest sequence
     number ack'd by the remote reader, update state & try dropping
     some messages */
  if (seqbase - 1 > rn->seq)
  {
    int64_t n_ack = (seqbase - 1) - rn->seq;
    unsigned n;
    rn->seq = seqbase - 1;
    if (rn->seq > wr->seq) {
      /* Prevent a reader from ACKing future samples (is only malicious because we require
         that rn->seq <= wr->seq) */
      rn->seq = wr->seq;
    }
    ddsrt_avl_augment_update (&wr_readers_treedef, rn);
    n = remove_acked_messages (wr, &whcst, &deferred_free_list);
    RSTTRACE (" ACK%"PRId64" RM%u", n_ack, n);
  }
  else
  {
    /* There's actually no guarantee that we need this information */
    whc_get_state(wr->whc, &whcst);
  }

  /* If this reader was marked as "non-responsive" in the past, it's now responding again,
     so update its status */
  if (rn->seq == MAX_SEQ_NUMBER && prd->c.xqos->reliability.kind == DDS_RELIABILITY_RELIABLE)
  {
    seqno_t oldest_seq;
    oldest_seq = WHCST_ISEMPTY(&whcst) ? wr->seq : whcst.max_seq;
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
    ddsrt_avl_augment_update (&wr_readers_treedef, rn);
    DDS_CLOG (DDS_LC_THROTTLE, &rst->gv->logconfig, "writer "PGUIDFMT" considering reader "PGUIDFMT" responsive again\n", PGUID (wr->e.guid), PGUID (rn->prd_guid));
  }

  /* Second, the NACK bits (literally, that is). To do so, attempt to
     classify the AckNack for reverse-engineered compatibility with
     RTI's invalid acks and sometimes slightly odd behaviour. */
  numbits = msg->readerSNState.numbits;
  msgs_sent = 0;
  msgs_lost = 0;
  max_seq_in_reply = 0;
  if (!rn->has_replied_to_hb && seqbase > 1 && is_pure_nonhist_ack)
  {
    RSTTRACE (" setting-has-replied-to-hb");
    rn->has_replied_to_hb = 1;
    /* walk the whole tree to ensure all proxy readers for this writer
       have their unack'ed info updated */
    ddsrt_avl_augment_update (&wr_readers_treedef, rn);
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
    if (WHCST_ISEMPTY(&whcst))
    {
      RSTTRACE (" whc-empty ");
      force_heartbeat_to_peer (wr, &whcst, prd, 0);
      hb_sent_in_response = 1;
    }
    else
    {
      RSTTRACE (" rebase ");
      force_heartbeat_to_peer (wr, &whcst, prd, 0);
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
  enqueued = 1;
  seq_xmit = writer_read_seq_xmit (wr);
  const bool gap_for_already_acked = vendor_is_eclipse (rst->vendor) && prd->c.xqos->durability.kind == DDS_DURABILITY_VOLATILE && seqbase <= rn->seq;
  const seqno_t min_seq_to_rexmit = gap_for_already_acked ? rn->seq + 1 : 0;
  for (uint32_t i = 0; i < numbits && seqbase + i <= seq_xmit && enqueued; i++)
  {
    /* Accelerated schedule may run ahead of sequence number set
       contained in the acknack, and assumes all messages beyond the
       set are NACK'd -- don't feel like tracking where exactly we
       left off ... */
    if (i >= msg->readerSNState.numbits || nn_bitset_isset (numbits, msg->bits, i))
    {
      seqno_t seq = seqbase + i;
      struct whc_borrowed_sample sample;
      if (seqbase + i >= min_seq_to_rexmit && whc_borrow_sample (wr->whc, seq, &sample))
      {
        if (!wr->retransmitting && sample.unacked)
          writer_set_retransmitting (wr);

        if (rst->gv->config.retransmit_merging != REXMIT_MERGE_NEVER && rn->assumed_in_sync)
        {
          /* send retransmit to all receivers, but skip if recently done */
          nn_mtime_t tstamp = now_mt ();
          if (tstamp.v > sample.last_rexmit_ts.v + rst->gv->config.retransmit_merging_period)
          {
            RSTTRACE (" RX%"PRId64, seqbase + i);
            enqueued = (enqueue_sample_wrlock_held (wr, seq, sample.plist, sample.serdata, NULL, 0) >= 0);
            if (enqueued)
            {
              max_seq_in_reply = seqbase + i;
              msgs_sent++;
              sample.last_rexmit_ts = tstamp;
            }
          }
          else
          {
            RSTTRACE (" RX%"PRId64" (merged)", seqbase + i);
          }
        }
        else
        {
          /* no merging, send directed retransmit */
          RSTTRACE (" RX%"PRId64"", seqbase + i);
          enqueued = (enqueue_sample_wrlock_held (wr, seq, sample.plist, sample.serdata, prd, 0) >= 0);
          if (enqueued)
          {
            max_seq_in_reply = seqbase + i;
            msgs_sent++;
            sample.rexmit_count++;
          }
        }

        whc_return_sample(wr->whc, &sample, true);
      }
      else if (gapstart == -1)
      {
        RSTTRACE (" M%"PRId64, seqbase + i);
        gapstart = seqbase + i;
        gapend = gapstart + 1;
        msgs_lost++;
      }
      else if (seqbase + i == gapend)
      {
        RSTTRACE (" M%"PRId64, seqbase + i);
        gapend = seqbase + i + 1;
        msgs_lost++;
      }
      else if (seqbase + i - gapend < 256)
      {
        uint32_t idx = (uint32_t) (seqbase + i - gapend);
        RSTTRACE (" M%"PRId64, seqbase + i);
        gapnumbits = idx + 1;
        nn_bitset_set (gapnumbits, gapbits, idx);
        msgs_lost++;
      }
    }
  }
  if (!enqueued)
    RSTTRACE (" rexmit-limit-hit");
  /* Generate a Gap message if some of the sequence is missing */
  if (gapstart > 0)
  {
    struct nn_xmsg *m;
    if (gapend == seqbase + msg->readerSNState.numbits)
    {
      /* We automatically grow a gap as far as we can -- can't
         retransmit those messages anyway, so no need for round-trip
         to the remote reader. */
      gapend = grow_gap_to_next_seq (wr, gapend);
    }
    /* The non-bitmap part of a gap message says everything <=
       gapend-1 is no more (so the maximum sequence number it informs
       the peer of is gapend-1); each bit adds one sequence number to
       that. */
    if (gapend-1 + gapnumbits > max_seq_in_reply)
      max_seq_in_reply = gapend-1 + gapnumbits;
    RSTTRACE (" XGAP%"PRId64"..%"PRId64"/%u:", gapstart, gapend, gapnumbits);
    for (uint32_t i = 0; i < gapnumbits; i++)
      RSTTRACE ("%c", nn_bitset_isset (gapnumbits, gapbits, i) ? '1' : '0');
    m = nn_xmsg_new (rst->gv->xmsgpool, &wr->e.guid.prefix, 0, NN_XMSG_KIND_CONTROL);
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
    nn_xmsg_setencoderid (m, wr->partition_id);
#endif
    if (nn_xmsg_setdstPRD (m, prd) < 0)
      nn_xmsg_free (m);
    else
    {
      add_Gap (m, wr, prd, gapstart, gapend, gapnumbits, gapbits);
      qxev_msg (wr->evq, m);
    }
    msgs_sent++;
  }
  wr->rexmit_count += msgs_sent;
  wr->rexmit_lost_count += msgs_lost;
  /* If rexmits and/or a gap message were sent, and if the last
     sequence number that we're informing the NACK'ing peer about is
     less than the last sequence number transmitted by the writer,
     tell the peer to acknowledge quickly. Not sure if that helps, but
     it might ... [NB writer->seq is the last msg sent so far] */
  if (msgs_sent && max_seq_in_reply < seq_xmit)
  {
    RSTTRACE (" rexmit#%"PRIu32" maxseq:%"PRId64"<%"PRId64"<=%"PRId64"", msgs_sent, max_seq_in_reply, seq_xmit, wr->seq);
    force_heartbeat_to_peer (wr, &whcst, prd, 1);
    hb_sent_in_response = 1;

    /* The primary purpose of hbcontrol_note_asyncwrite is to ensure
       heartbeats will go out at the "normal" rate again, instead of a
       gradually lowering rate.  If we just got a request for a
       retransmit, and there is more to be retransmitted, surely the
       rate should be kept up for now */
    writer_hbcontrol_note_asyncwrite (wr, now_mt ());
  }
  /* If "final" flag not set, we must respond with a heartbeat. Do it
     now if we haven't done so already */
  if (!(msg->smhdr.flags & ACKNACK_FLAG_FINAL) && !hb_sent_in_response)
    force_heartbeat_to_peer (wr, &whcst, prd, 0);
  RSTTRACE (")");
 out:
  ddsrt_mutex_unlock (&wr->e.lock);
  whc_free_deferred_free_list (wr->whc, deferred_free_list);
  return 1;
}

static void handle_forall_destinations (const ddsi_guid_t *dst, struct proxy_writer *pwr, ddsrt_avl_walk_t fun, void *arg)
{
  /* prefix:  id:   to:
     0        0     all matched readers
     0        !=0   all matched readers with entityid id
     !=0      0     to all matched readers in addressed participant
     !=0      !=0   to the one addressed reader
  */
  const int haveprefix =
    !(dst->prefix.u[0] == 0 && dst->prefix.u[1] == 0 && dst->prefix.u[2] == 0);
  const int haveid = !(dst->entityid.u == NN_ENTITYID_UNKNOWN);

  /* must have pwr->e.lock held for safely iterating over readers */
  ASSERT_MUTEX_HELD (&pwr->e.lock);

  switch ((haveprefix << 1) | haveid)
  {
    case (0 << 1) | 0: /* all: full treewalk */
      ddsrt_avl_walk (&pwr_readers_treedef, &pwr->readers, fun, arg);
      break;
    case (0 << 1) | 1: /* all with correct entityid: special filtering treewalk */
      {
        struct pwr_rd_match *wn;
        for (wn = ddsrt_avl_find_min (&pwr_readers_treedef, &pwr->readers); wn; wn = ddsrt_avl_find_succ (&pwr_readers_treedef, &pwr->readers, wn))
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
        ddsrt_avl_walk_range (&pwr_readers_treedef, &pwr->readers, &a, &b, fun, arg);
      }
      break;
    case (1 << 1) | 1: /* fully addressed: dst should exist (but for removal) */
      {
        struct pwr_rd_match *wn;
        if ((wn = ddsrt_avl_lookup (&pwr_readers_treedef, &pwr->readers, dst)) != NULL)
          fun (wn, arg);
      }
      break;
  }
}

struct handle_Heartbeat_helper_arg {
  struct receiver_state *rst;
  const Heartbeat_t *msg;
  struct proxy_writer *pwr;
  nn_wctime_t timestamp;
  nn_etime_t tnow;
  nn_mtime_t tnow_mt;
};

static void handle_Heartbeat_helper (struct pwr_rd_match * const wn, struct handle_Heartbeat_helper_arg * const arg)
{
  struct receiver_state * const rst = arg->rst;
  Heartbeat_t const * const msg = arg->msg;
  struct proxy_writer * const pwr = arg->pwr;
  seqno_t refseq;

  ASSERT_MUTEX_HELD (&pwr->e.lock);

  /* Not supposed to respond to repeats and old heartbeats. */
  if (!accept_ack_or_hb_w_timeout (msg->count, &wn->next_heartbeat, arg->tnow, &wn->t_heartbeat_accepted, 0))
  {
    RSTTRACE (" ("PGUIDFMT")", PGUID (wn->rd_guid));
    return;
  }

  /* Reference sequence number for determining whether or not to
     Ack/Nack unfortunately depends on whether the reader is in
     sync. */
  if (wn->in_sync != PRMSS_OUT_OF_SYNC)
    refseq = nn_reorder_next_seq (pwr->reorder) - 1;
  else
    refseq = nn_reorder_next_seq (wn->u.not_in_sync.reorder) - 1;
  RSTTRACE (" "PGUIDFMT"@%"PRId64"%s", PGUID (wn->rd_guid), refseq, (wn->in_sync == PRMSS_SYNC) ? "(sync)" : (wn->in_sync == PRMSS_TLCATCHUP) ? "(tlcatchup)" : "");

  /* Reschedule AckNack transmit if deemed appropriate; unreliable
     readers have acknack_xevent == NULL and can't do this.

     There is no real need to send a nack from each reader that is in
     sync -- indeed, we could simply ignore the destination address in
     the messages we receive and only ever nack each sequence number
     once, regardless of which readers care about it. */
  if (wn->acknack_xevent)
  {
    nn_mtime_t tsched;
    tsched.v = T_NEVER;
    if (pwr->last_seq > refseq)
    {
      RSTTRACE ("/NAK");
      if (arg->tnow_mt.v >= wn->t_last_nack.v + rst->gv->config.nack_delay || refseq >= wn->seq_last_nack)
        tsched = arg->tnow_mt;
      else
      {
        tsched.v = arg->tnow_mt.v + rst->gv->config.nack_delay;
        RSTTRACE ("d");
      }
    }
    else if (!(msg->smhdr.flags & HEARTBEAT_FLAG_FINAL))
    {
      tsched = arg->tnow_mt;
    }
    if (resched_xevent_if_earlier (wn->acknack_xevent, tsched))
    {
      if (rst->gv->config.meas_hb_to_ack_latency && arg->timestamp.v)
        wn->hb_timestamp = arg->timestamp;
    }
  }
}

static int handle_Heartbeat (struct receiver_state *rst, nn_etime_t tnow, struct nn_rmsg *rmsg, const Heartbeat_t *msg, nn_wctime_t timestamp)
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
  const seqno_t firstseq = fromSN (msg->firstSN);
  const seqno_t lastseq = fromSN (msg->lastSN);
  struct handle_Heartbeat_helper_arg arg;
  struct proxy_writer *pwr;
  ddsi_guid_t src, dst;

  src.prefix = rst->src_guid_prefix;
  src.entityid = msg->writerId;
  dst.prefix = rst->dst_guid_prefix;
  dst.entityid = msg->readerId;

  RSTTRACE ("HEARTBEAT(%s#%"PRId32":%"PRId64"..%"PRId64" ", msg->smhdr.flags & HEARTBEAT_FLAG_FINAL ? "F" : "", msg->count, firstseq, lastseq);

  if (!rst->forme)
  {
    RSTTRACE (PGUIDFMT" -> "PGUIDFMT" not-for-me)", PGUID (src), PGUID (dst));
    return 1;
  }

  if ((pwr = ephash_lookup_proxy_writer_guid (rst->gv->guid_hash, &src)) == NULL)
  {
    RSTTRACE (PGUIDFMT"? -> "PGUIDFMT")", PGUID (src), PGUID (dst));
    return 1;
  }

  /* liveliness is still only implemented partially (with all set to AUTOMATIC,
     BY_PARTICIPANT, &c.), so we simply renew the proxy participant's lease. */
  lease_renew (ddsrt_atomic_ldvoidp (&pwr->c.proxypp->lease), tnow);

  RSTTRACE (PGUIDFMT" -> "PGUIDFMT":", PGUID (src), PGUID (dst));

  ddsrt_mutex_lock (&pwr->e.lock);

  if (pwr->n_reliable_readers == 0)
  {
    RSTTRACE (PGUIDFMT" -> "PGUIDFMT" no-reliable-readers)", PGUID (src), PGUID (dst));
    ddsrt_mutex_unlock (&pwr->e.lock);
    return 1;
  }

  if (!pwr->have_seen_heartbeat)
  {
    struct nn_rdata *gap;
    struct nn_rsample_chain sc;
    int refc_adjust = 0;
    nn_reorder_result_t res;
    nn_defrag_notegap (pwr->defrag, 1, lastseq + 1);
    gap = nn_rdata_newgap (rmsg);
    res = nn_reorder_gap (&sc, pwr->reorder, gap, 1, lastseq + 1, &refc_adjust);
    /* proxy writer is not accepting data until it has received a heartbeat, so
       there can't be any data to deliver */
    assert (res <= 0);
    (void) res;
    nn_fragchain_adjust_refcount (gap, refc_adjust);
    pwr->have_seen_heartbeat = 1;
  }

  if (lastseq > pwr->last_seq)
  {
    pwr->last_seq = lastseq;
    pwr->last_fragnum = ~0u;
    pwr->last_fragnum_reset = 0;
  }
  else if (pwr->last_fragnum != ~0u && lastseq == pwr->last_seq)
  {
    if (!pwr->last_fragnum_reset)
      pwr->last_fragnum_reset = 1;
    else
    {
      pwr->last_fragnum = ~0u;
      pwr->last_fragnum_reset = 0;
    }
  }

  nn_defrag_notegap (pwr->defrag, 1, firstseq);

  {
    struct nn_rdata *gap;
    struct pwr_rd_match *wn;
    struct nn_rsample_chain sc;
    int refc_adjust = 0;
    nn_reorder_result_t res;
    gap = nn_rdata_newgap (rmsg);
    if ((res = nn_reorder_gap (&sc, pwr->reorder, gap, 1, firstseq, &refc_adjust)) > 0)
    {
      if (pwr->deliver_synchronously)
        deliver_user_data_synchronously (&sc, NULL);
      else
        nn_dqueue_enqueue (pwr->dqueue, &sc, res);
    }
    for (wn = ddsrt_avl_find_min (&pwr_readers_treedef, &pwr->readers); wn; wn = ddsrt_avl_find_succ (&pwr_readers_treedef, &pwr->readers, wn))
      if (wn->in_sync != PRMSS_SYNC)
      {
        seqno_t last_deliv_seq = 0;
        switch (wn->in_sync)
        {
          case PRMSS_SYNC:
            assert(0);
            break;
          case PRMSS_TLCATCHUP:
            last_deliv_seq = nn_reorder_next_seq (pwr->reorder) - 1;
            break;
          case PRMSS_OUT_OF_SYNC: {
            struct nn_reorder *ro = wn->u.not_in_sync.reorder;
            if ((res = nn_reorder_gap (&sc, ro, gap, 1, firstseq, &refc_adjust)) > 0)
            {
              if (pwr->deliver_synchronously)
                deliver_user_data_synchronously (&sc, &wn->rd_guid);
              else
                nn_dqueue_enqueue1 (pwr->dqueue, &wn->rd_guid, &sc, res);
            }
            last_deliv_seq = nn_reorder_next_seq (wn->u.not_in_sync.reorder) - 1;
          }
        }
        if (wn->u.not_in_sync.end_of_tl_seq == MAX_SEQ_NUMBER)
        {
          wn->u.not_in_sync.end_of_tl_seq = fromSN (msg->lastSN);
          RSTTRACE (" end-of-tl-seq(rd "PGUIDFMT" #%"PRId64")", PGUID(wn->rd_guid), wn->u.not_in_sync.end_of_tl_seq);
        }
        maybe_set_reader_in_sync (pwr, wn, last_deliv_seq);
      }
    nn_fragchain_adjust_refcount (gap, refc_adjust);
  }

  arg.rst = rst;
  arg.msg = msg;
  arg.pwr = pwr;
  arg.timestamp = timestamp;
  arg.tnow = tnow;
  arg.tnow_mt = now_mt ();
  handle_forall_destinations (&dst, pwr, (ddsrt_avl_walk_t) handle_Heartbeat_helper, &arg);
  RSTTRACE (")");

  ddsrt_mutex_unlock (&pwr->e.lock);
  return 1;
}

static int handle_HeartbeatFrag (struct receiver_state *rst, UNUSED_ARG(nn_etime_t tnow), const HeartbeatFrag_t *msg)
{
  const seqno_t seq = fromSN (msg->writerSN);
  const nn_fragment_number_t fragnum = msg->lastFragmentNum - 1; /* we do 0-based */
  ddsi_guid_t src, dst;
  struct proxy_writer *pwr;

  src.prefix = rst->src_guid_prefix;
  src.entityid = msg->writerId;
  dst.prefix = rst->dst_guid_prefix;
  dst.entityid = msg->readerId;

  RSTTRACE ("HEARTBEATFRAG(#%"PRId32":%"PRId64"/[1,%u]", msg->count, seq, fragnum+1);
  if (!rst->forme)
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT" not-for-me)", PGUID (src), PGUID (dst));
    return 1;
  }

  if ((pwr = ephash_lookup_proxy_writer_guid (rst->gv->guid_hash, &src)) == NULL)
  {
    RSTTRACE (" "PGUIDFMT"? -> "PGUIDFMT")", PGUID (src), PGUID (dst));
    return 1;
  }

  /* liveliness is still only implemented partially (with all set to AUTOMATIC, BY_PARTICIPANT, &c.), so we simply renew the proxy participant's lease. */
  lease_renew (ddsrt_atomic_ldvoidp (&pwr->c.proxypp->lease), tnow);

  RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT"", PGUID (src), PGUID (dst));
  ddsrt_mutex_lock (&pwr->e.lock);

  if (seq > pwr->last_seq)
  {
    pwr->last_seq = seq;
    pwr->last_fragnum = fragnum;
    pwr->last_fragnum_reset = 0;
  }
  else if (seq == pwr->last_seq && fragnum > pwr->last_fragnum)
  {
    pwr->last_fragnum = fragnum;
    pwr->last_fragnum_reset = 0;
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
    struct pwr_rd_match *m = NULL;

    if (nn_reorder_wantsample (pwr->reorder, seq))
    {
      /* Pick an arbitrary reliable reader's guid for the response --
         assuming a reliable writer -> unreliable reader is rare, and
         so scanning the readers is acceptable if the first guess
         fails */
      m = ddsrt_avl_root_non_empty (&pwr_readers_treedef, &pwr->readers);
      if (m->acknack_xevent == NULL)
      {
        m = ddsrt_avl_find_min (&pwr_readers_treedef, &pwr->readers);
        while (m && m->acknack_xevent == NULL)
          m = ddsrt_avl_find_succ (&pwr_readers_treedef, &pwr->readers, m);
      }
    }
    else if (seq < nn_reorder_next_seq (pwr->reorder))
    {
      /* Check out-of-sync readers -- should add a bit to cheaply test
         whether there are any (usually there aren't) */
      m = ddsrt_avl_find_min (&pwr_readers_treedef, &pwr->readers);
      while (m)
      {
        if ((m->in_sync == PRMSS_OUT_OF_SYNC) && m->acknack_xevent != NULL && nn_reorder_wantsample (m->u.not_in_sync.reorder, seq))
        {
          /* If reader is out-of-sync, and reader is realiable, and
             reader still wants this particular sample, then use this
             reader to decide which fragments to nack */
          break;
        }
        m = ddsrt_avl_find_succ (&pwr_readers_treedef, &pwr->readers, m);
      }
    }

    if (m == NULL)
      RSTTRACE (" no interested reliable readers");
    else
    {
      /* Check if we are missing something */
      DDSRT_STATIC_ASSERT ((NN_FRAGMENT_NUMBER_SET_MAX_BITS % 32) == 0);
      struct {
        struct nn_fragment_number_set_header set;
        uint32_t bits[NN_FRAGMENT_NUMBER_SET_MAX_BITS / 32];
      } nackfrag;
      if (nn_defrag_nackmap (pwr->defrag, seq, fragnum, &nackfrag.set, nackfrag.bits, NN_FRAGMENT_NUMBER_SET_MAX_BITS) > 0)
      {
        /* Yes we are (note that this potentially also happens for
           samples we no longer care about) */
        int64_t delay = rst->gv->config.nack_delay;
        RSTTRACE ("/nackfrag");
        resched_xevent_if_earlier (m->acknack_xevent, add_duration_to_mtime (now_mt(), delay));
      }
    }
  }
  RSTTRACE (")");
  ddsrt_mutex_unlock (&pwr->e.lock);
  return 1;
}

static int handle_NackFrag (struct receiver_state *rst, nn_etime_t tnow, const NackFrag_t *msg)
{
  struct proxy_reader *prd;
  struct wr_prd_match *rn;
  struct writer *wr;
  struct whc_borrowed_sample sample;
  ddsi_guid_t src, dst;
  nn_count_t *countp;
  seqno_t seq = fromSN (msg->writerSN);

  countp = (nn_count_t *) ((char *) msg + offsetof (NackFrag_t, bits) + NN_FRAGMENT_NUMBER_SET_BITS_SIZE (msg->fragmentNumberState.numbits));
  src.prefix = rst->src_guid_prefix;
  src.entityid = msg->readerId;
  dst.prefix = rst->dst_guid_prefix;
  dst.entityid = msg->writerId;

  RSTTRACE ("NACKFRAG(#%"PRId32":%"PRId64"/%u/%"PRIu32":", *countp, seq, msg->fragmentNumberState.bitmap_base, msg->fragmentNumberState.numbits);
  for (uint32_t i = 0; i < msg->fragmentNumberState.numbits; i++)
    RSTTRACE ("%c", nn_bitset_isset (msg->fragmentNumberState.numbits, msg->bits, i) ? '1' : '0');

  if (!rst->forme)
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT" not-for-me)", PGUID (src), PGUID (dst));
    return 1;
  }

  if ((wr = ephash_lookup_writer_guid (rst->gv->guid_hash, &dst)) == NULL)
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT"?)", PGUID (src), PGUID (dst));
    return 1;
  }
  /* Always look up the proxy reader -- even though we don't need for
     the normal pure ack steady state. If (a big "if"!) this shows up
     as a significant portion of the time, we can always rewrite it to
     only retrieve it when needed. */
  if ((prd = ephash_lookup_proxy_reader_guid (rst->gv->guid_hash, &src)) == NULL)
  {
    RSTTRACE (" "PGUIDFMT"? -> "PGUIDFMT")", PGUID (src), PGUID (dst));
    return 1;
  }

  /* liveliness is still only implemented partially (with all set to AUTOMATIC, BY_PARTICIPANT, &c.), so we simply renew the proxy participant's lease. */
  lease_renew (ddsrt_atomic_ldvoidp (&prd->c.proxypp->lease), tnow);

  if (!wr->reliable) /* note: reliability can't be changed */
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT" not a reliable writer)", PGUID (src), PGUID (dst));
    return 1;
  }

  ddsrt_mutex_lock (&wr->e.lock);
  if ((rn = ddsrt_avl_lookup (&wr_readers_treedef, &wr->readers, &src)) == NULL)
  {
    RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT" not a connection", PGUID (src), PGUID (dst));
    goto out;
  }

  /* Ignore old NackFrags (see also handle_AckNack) */
  if (*countp < rn->next_nackfrag)
  {
    RSTTRACE (" ["PGUIDFMT" -> "PGUIDFMT"]", PGUID (src), PGUID (dst));
    goto out;
  }
  rn->next_nackfrag = *countp + 1;
  RSTTRACE (" "PGUIDFMT" -> "PGUIDFMT"", PGUID (src), PGUID (dst));

  /* Resend the requested fragments if we still have the sample, send
     a Gap if we don't have them anymore. */
  if (whc_borrow_sample (wr->whc, seq, &sample))
  {
    const unsigned base = msg->fragmentNumberState.bitmap_base - 1;
    int enqueued = 1;
    RSTTRACE (" scheduling requested frags ...\n");
    for (uint32_t i = 0; i < msg->fragmentNumberState.numbits && enqueued; i++)
    {
      if (nn_bitset_isset (msg->fragmentNumberState.numbits, msg->bits, i))
      {
        struct nn_xmsg *reply;
        if (create_fragment_message (wr, seq, sample.plist, sample.serdata, base + i, prd, &reply, 0) < 0)
          enqueued = 0;
        else
          enqueued = qxev_msg_rexmit_wrlock_held (wr->evq, reply, 0);
      }
    }
    whc_return_sample (wr->whc, &sample, false);
  }
  else
  {
    static uint32_t zero = 0;
    struct nn_xmsg *m;
    RSTTRACE (" msg not available: scheduling Gap\n");
    m = nn_xmsg_new (rst->gv->xmsgpool, &wr->e.guid.prefix, 0, NN_XMSG_KIND_CONTROL);
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
    nn_xmsg_setencoderid (m, wr->partition_id);
#endif
    if (nn_xmsg_setdstPRD (m, prd) < 0)
      nn_xmsg_free (m);
    else
    {
      /* length-1 bitmap with the bit clear avoids the illegal case of a
       length-0 bitmap */
      add_Gap (m, wr, prd, seq, seq+1, 1, &zero);
      qxev_msg (wr->evq, m);
    }
  }
  if (seq < writer_read_seq_xmit (wr))
  {
    /* Not everything was retransmitted yet, so force a heartbeat out
       to give the reader a chance to nack the rest and make sure
       hearbeats will go out at a reasonably high rate for a while */
    struct whc_state whcst;
    whc_get_state(wr->whc, &whcst);
    force_heartbeat_to_peer (wr, &whcst, prd, 1);
    writer_hbcontrol_note_asyncwrite (wr, now_mt ());
  }

 out:
  ddsrt_mutex_unlock (&wr->e.lock);
  RSTTRACE (")");
  return 1;
}

static int handle_InfoDST (struct receiver_state *rst, const InfoDST_t *msg, const ddsi_guid_prefix_t *dst_prefix)
{
  rst->dst_guid_prefix = nn_ntoh_guid_prefix (msg->guid_prefix);
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
    dst.entityid = to_entityid(NN_ENTITYID_PARTICIPANT);
    rst->forme = (ephash_lookup_participant_guid (rst->gv->guid_hash, &dst) != NULL ||
                  is_deleted_participant_guid (rst->gv->deleted_participants, &dst, DPG_LOCAL));
  }
  return 1;
}

static int handle_InfoSRC (struct receiver_state *rst, const InfoSRC_t *msg)
{
  rst->src_guid_prefix = nn_ntoh_guid_prefix (msg->guid_prefix);
  rst->protocol_version = msg->version;
  rst->vendor = msg->vendorid;
  RSTTRACE ("INFOSRC(%"PRIx32":%"PRIx32":%"PRIx32" vendor %u.%u)",
          PGUIDPREFIX (rst->src_guid_prefix), rst->vendor.id[0], rst->vendor.id[1]);
  return 1;
}

static int handle_InfoTS (const struct receiver_state *rst, const InfoTS_t *msg, nn_wctime_t *timestamp)
{
  RSTTRACE ("INFOTS(");
  if (msg->smhdr.flags & INFOTS_INVALIDATE_FLAG)
  {
    *timestamp = NN_WCTIME_INVALID;
    RSTTRACE ("invalidate");
  }
  else
  {
    *timestamp = nn_wctime_from_ddsi_time (msg->time);
    if (rst->gv->logconfig.c.mask & DDS_LC_TRACE)
      RSTTRACE ("%d.%09d", (int) (timestamp->v / 1000000000), (int) (timestamp->v % 1000000000));
  }
  RSTTRACE (")");
  return 1;
}

static int handle_one_gap (struct proxy_writer *pwr, struct pwr_rd_match *wn, seqno_t a, seqno_t b, struct nn_rdata *gap, int *refc_adjust)
{
  struct nn_rsample_chain sc;
  nn_reorder_result_t res;
  int gap_was_valuable = 0;
  ASSERT_MUTEX_HELD (&pwr->e.lock);

  /* Clean up the defrag admin: no fragments of a missing sample will
     be arriving in the future */
  nn_defrag_notegap (pwr->defrag, a, b);

  /* Primary reorder: the gap message may cause some samples to become
     deliverable. */
  if ((res = nn_reorder_gap (&sc, pwr->reorder, gap, a, b, refc_adjust)) > 0)
  {
    if (pwr->deliver_synchronously)
      deliver_user_data_synchronously (&sc, NULL);
    else
      nn_dqueue_enqueue (pwr->dqueue, &sc, res);
  }

  /* If the result was REJECT or TOO_OLD, then this gap didn't add
     anything useful, or there was insufficient memory to store it.
     When the result is either ACCEPT or a sample chain, it clearly
     meant something. */
  DDSRT_STATIC_ASSERT_CODE (NN_REORDER_ACCEPT == 0);
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
        if ((res = nn_reorder_gap (&sc, wn->u.not_in_sync.reorder, gap, a, b, refc_adjust)) > 0)
        {
          if (pwr->deliver_synchronously)
            deliver_user_data_synchronously (&sc, &wn->rd_guid);
          else
            nn_dqueue_enqueue1 (pwr->dqueue, &wn->rd_guid, &sc, res);
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

static int handle_Gap (struct receiver_state *rst, nn_etime_t tnow, struct nn_rmsg *rmsg, const Gap_t *msg)
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

  struct proxy_writer *pwr;
  struct pwr_rd_match *wn;
  ddsi_guid_t src, dst;
  seqno_t gapstart, listbase;
  int32_t last_included_rel;
  uint32_t listidx;

  src.prefix = rst->src_guid_prefix;
  src.entityid = msg->writerId;
  dst.prefix = rst->dst_guid_prefix;
  dst.entityid = msg->readerId;
  gapstart = fromSN (msg->gapStart);
  listbase = fromSN (msg->gapList.bitmap_base);
  RSTTRACE ("GAP(%"PRId64"..%"PRId64"/%"PRIu32" ", gapstart, listbase, msg->gapList.numbits);

  /* There is no _good_ reason for a writer to start the bitmap with a
     1 bit, but check for it just in case, to reduce the number of
     sequence number gaps to be processed. */
  for (listidx = 0; listidx < msg->gapList.numbits; listidx++)
    if (!nn_bitset_isset (msg->gapList.numbits, msg->bits, listidx))
      break;
  last_included_rel = (int32_t) listidx - 1;

  if (!rst->forme)
  {
    RSTTRACE (""PGUIDFMT" -> "PGUIDFMT" not-for-me)", PGUID (src), PGUID (dst));
    return 1;
  }

  if ((pwr = ephash_lookup_proxy_writer_guid (rst->gv->guid_hash, &src)) == NULL)
  {
    RSTTRACE (""PGUIDFMT"? -> "PGUIDFMT")", PGUID (src), PGUID (dst));
    return 1;
  }

  /* liveliness is still only implemented partially (with all set to AUTOMATIC, BY_PARTICIPANT, &c.), so we simply renew the proxy participant's lease. */
  lease_renew (ddsrt_atomic_ldvoidp (&pwr->c.proxypp->lease), tnow);

  ddsrt_mutex_lock (&pwr->e.lock);
  if ((wn = ddsrt_avl_lookup (&pwr_readers_treedef, &pwr->readers, &dst)) == NULL)
  {
    RSTTRACE (PGUIDFMT" -> "PGUIDFMT" not a connection)", PGUID (src), PGUID (dst));
    ddsrt_mutex_unlock (&pwr->e.lock);
    return 1;
  }
  RSTTRACE (PGUIDFMT" -> "PGUIDFMT, PGUID (src), PGUID (dst));

  if (!pwr->have_seen_heartbeat && pwr->n_reliable_readers > 0)
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
    struct nn_rdata *gap;
    gap = nn_rdata_newgap (rmsg);
    if (gapstart < listbase + listidx)
    {
      /* sanity check on sequence numbers because a GAP message is not invalid even
         if start >= listbase (DDSI 2.1 sect 8.3.7.4.3), but only handle non-empty
         intervals */
      (void) handle_one_gap (pwr, wn, gapstart, listbase + listidx, gap, &refc_adjust);
    }
    while (listidx < msg->gapList.numbits)
    {
      if (!nn_bitset_isset (msg->gapList.numbits, msg->bits, listidx))
        listidx++;
      else
      {
        uint32_t j;
        for (j = listidx + 1; j < msg->gapList.numbits; j++)
          if (!nn_bitset_isset (msg->gapList.numbits, msg->bits, j))
            break;
        /* spec says gapList (2) identifies an additional list of sequence numbers that
           are invalid (8.3.7.4.2), so by that rule an insane start would simply mean the
           initial interval is to be ignored and the bitmap to be applied */
        (void) handle_one_gap (pwr, wn, listbase + listidx, listbase + j, gap, &refc_adjust);
        assert(j >= 1);
        last_included_rel = (int32_t) j - 1;
        listidx = j;
      }
    }
    nn_fragchain_adjust_refcount (gap, refc_adjust);
  }

  /* If the last sequence number explicitly included in the set is
     beyond the last sequence number we know exists, update the
     latter.  Note that a sequence number _not_ included in the set
     doesn't tell us anything (which is something that RTI apparently
     got wrong in its interpetation of pure acks that do include a
     bitmap).  */
  if (listbase + last_included_rel > pwr->last_seq)
  {
    pwr->last_seq = listbase + last_included_rel;
    pwr->last_fragnum = ~0u;
    pwr->last_fragnum_reset = 0;
  }
  RSTTRACE (")");
  ddsrt_mutex_unlock (&pwr->e.lock);
  return 1;
}

static struct ddsi_serdata *get_serdata (struct ddsi_sertopic const * const topic, const struct nn_rdata *fragchain, uint32_t sz, int justkey, unsigned statusinfo, nn_wctime_t tstamp)
{
  struct ddsi_serdata *sd = ddsi_serdata_from_ser (topic, justkey ? SDK_KEY : SDK_DATA, fragchain, sz);
  if (sd)
  {
    sd->statusinfo = statusinfo;
    sd->timestamp = tstamp;
  }
  return sd;
}

static struct ddsi_serdata *new_sample_from_data (struct ddsi_tkmap_instance **tk1, struct q_globals *gv, const struct nn_rsample_info *sampleinfo, unsigned char data_smhdr_flags, const nn_plist_t *qos, const struct nn_rdata *fragchain, unsigned statusinfo, nn_wctime_t tstamp, struct ddsi_sertopic const * const topic)
{
  const char *failmsg = NULL;
  struct ddsi_serdata *sample = NULL;

  if (statusinfo == 0)
  {
    /* normal write */
    if (!(data_smhdr_flags & DATA_FLAG_DATAFLAG) || sampleinfo->size == 0)
    {
      const struct proxy_writer *pwr = sampleinfo->pwr;
      ddsi_guid_t guid;
      /* pwr can't currently be null, but that might change some day, and this being
         an error path, it doesn't hurt to survive that */
      if (pwr) guid = pwr->e.guid; else memset (&guid, 0, sizeof (guid));
      DDS_CTRACE (&gv->logconfig,
                  "data(application, vendor %u.%u): "PGUIDFMT" #%"PRId64": write without proper payload (data_smhdr_flags 0x%x size %"PRIu32")\n",
                  sampleinfo->rst->vendor.id[0], sampleinfo->rst->vendor.id[1],
                  PGUID (guid), sampleinfo->seq,
                  data_smhdr_flags, sampleinfo->size);
      return NULL;
    }
    sample = get_serdata (topic, fragchain, sampleinfo->size, 0, statusinfo, tstamp);
  }
  else if (sampleinfo->size)
  {
    /* dispose or unregister with included serialized key or data
       (data is a PrismTech extension) -- i.e., dispose or unregister
       as one would expect to receive */
    if (data_smhdr_flags & DATA_FLAG_KEYFLAG)
    {
      sample = get_serdata (topic, fragchain, sampleinfo->size, 1, statusinfo, tstamp);
    }
    else
    {
      assert (data_smhdr_flags & DATA_FLAG_DATAFLAG);
      sample = get_serdata (topic, fragchain, sampleinfo->size, 0, statusinfo, tstamp);
    }
  }
  else if (data_smhdr_flags & DATA_FLAG_INLINE_QOS)
  {
    /* RTI always tries to make us survive on the keyhash. RTI must
       mend its ways. */
    if (NN_STRICT_P (gv->config))
      failmsg = "no content";
    else if (!(qos->present & PP_KEYHASH))
      failmsg = "qos present but without keyhash";
    else if ((sample = ddsi_serdata_from_keyhash (topic, &qos->keyhash)) == NULL)
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
    const struct proxy_writer *pwr = sampleinfo->pwr;
    ddsi_guid_t guid;
    if (pwr) guid = pwr->e.guid; else memset (&guid, 0, sizeof (guid));
    DDS_CWARNING (&gv->logconfig,
                  "data(application, vendor %u.%u): "PGUIDFMT" #%"PRId64": deserialization %s/%s failed (%s)\n",
                  sampleinfo->rst->vendor.id[0], sampleinfo->rst->vendor.id[1],
                  PGUID (guid), sampleinfo->seq,
                  topic->name, topic->type_name,
                  failmsg ? failmsg : "for reasons unknown");
  }
  else
  {
    if ((*tk1 = ddsi_tkmap_lookup_instance_ref (gv->m_tkmap, sample)) == NULL)
    {
      ddsi_serdata_unref (sample);
      sample = NULL;
    }
    else if (gv->logconfig.c.mask & DDS_LC_TRACE)
    {
      const struct proxy_writer *pwr = sampleinfo->pwr;
      ddsi_guid_t guid;
      char tmp[1024];
      size_t res = 0;
      tmp[0] = 0;
      if (gv->logconfig.c.mask & DDS_LC_CONTENT)
        res = ddsi_serdata_print (sample, tmp, sizeof (tmp));
      if (pwr) guid = pwr->e.guid; else memset (&guid, 0, sizeof (guid));
      GVTRACE ("data(application, vendor %u.%u): "PGUIDFMT" #%"PRId64": ST%x %s/%s:%s%s",
               sampleinfo->rst->vendor.id[0], sampleinfo->rst->vendor.id[1],
               PGUID (guid), sampleinfo->seq, statusinfo, topic->name, topic->type_name,
               tmp, res < sizeof (tmp) ? "" : "(trunc)");
    }
  }
  return sample;
}

static void free_sample_after_store (struct q_globals *gv, struct ddsi_serdata *sample, struct ddsi_tkmap_instance *tk)
{
  ddsi_tkmap_instance_unref (gv->m_tkmap, tk);
  ddsi_serdata_unref (sample);
}

unsigned char normalize_data_datafrag_flags (const SubmessageHeader_t *smhdr)
{
  switch ((SubmessageKind_t) smhdr->submessageId)
  {
    case SMID_DATA:
      return smhdr->flags;
    case SMID_DATA_FRAG:
      {
        unsigned char common = smhdr->flags & DATA_FLAG_INLINE_QOS;
        DDSRT_STATIC_ASSERT_CODE (DATA_FLAG_INLINE_QOS == DATAFRAG_FLAG_INLINE_QOS);
        if (smhdr->flags & DATAFRAG_FLAG_KEYFLAG)
          return common | DATA_FLAG_KEYFLAG;
        else
          return common | DATA_FLAG_DATAFLAG;
      }
    default:
      assert (0);
      return 0;
  }
}

static struct reader *proxy_writer_first_in_sync_reader (struct proxy_writer *pwr, ddsrt_avl_iter_t *it)
{
  struct pwr_rd_match *m;
  struct reader *rd;
  for (m = ddsrt_avl_iter_first (&pwr_readers_treedef, &pwr->readers, it); m != NULL; m = ddsrt_avl_iter_next (it))
    if (m->in_sync == PRMSS_SYNC && (rd = ephash_lookup_reader_guid (pwr->e.gv->guid_hash, &m->rd_guid)) != NULL)
      return rd;
  return NULL;
}

static struct reader *proxy_writer_next_in_sync_reader (struct proxy_writer *pwr, ddsrt_avl_iter_t *it)
{
  struct pwr_rd_match *m;
  struct reader *rd;
  for (m = ddsrt_avl_iter_next (it); m != NULL; m = ddsrt_avl_iter_next (it))
    if (m->in_sync == PRMSS_SYNC && (rd = ephash_lookup_reader_guid (pwr->e.gv->guid_hash, &m->rd_guid)) != NULL)
      return rd;
  return NULL;
}

static int deliver_user_data (const struct nn_rsample_info *sampleinfo, const struct nn_rdata *fragchain, const ddsi_guid_t *rdguid, int pwr_locked)
{
  struct receiver_state const * const rst = sampleinfo->rst;
  struct q_globals * const gv = rst->gv;
  struct proxy_writer * const pwr = sampleinfo->pwr;
  unsigned statusinfo;
  Data_DataFrag_common_t *msg;
  unsigned char data_smhdr_flags;
  nn_plist_t qos;
  int need_keyhash;

  if (pwr->ddsi2direct_cb)
  {
    pwr->ddsi2direct_cb (sampleinfo, fragchain, pwr->ddsi2direct_cbarg);
    return 0;
  }

  /* FIXME: fragments are now handled by copying the message to
     freshly malloced memory (see defragment()) ... that'll have to
     change eventually */
  assert (fragchain->min == 0);
  assert (!is_builtin_entityid (pwr->e.guid.entityid, pwr->c.vendor));

  /* Luckily, the Data header (up to inline QoS) is a prefix of the
     DataFrag header, so for the fixed-position things that we're
     interested in here, both can be treated as Data submessages. */
  msg = (Data_DataFrag_common_t *) NN_RMSG_PAYLOADOFF (fragchain->rmsg, NN_RDATA_SUBMSG_OFF (fragchain));
  data_smhdr_flags = normalize_data_datafrag_flags (&msg->smhdr);

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
  need_keyhash = (sampleinfo->size == 0 || (data_smhdr_flags & (DATA_FLAG_KEYFLAG | DATA_FLAG_DATAFLAG)) == 0);
  if (!(sampleinfo->complex_qos || need_keyhash) || !(data_smhdr_flags & DATA_FLAG_INLINE_QOS))
  {
    nn_plist_init_empty (&qos);
    statusinfo = sampleinfo->statusinfo;
  }
  else
  {
    nn_plist_src_t src;
    size_t qos_offset = NN_RDATA_SUBMSG_OFF (fragchain) + offsetof (Data_DataFrag_common_t, octetsToInlineQos) + sizeof (msg->octetsToInlineQos) + msg->octetsToInlineQos;
    dds_return_t plist_ret;
    src.protocol_version = rst->protocol_version;
    src.vendorid = rst->vendor;
    src.encoding = (msg->smhdr.flags & SMFLAG_ENDIANNESS) ? PL_CDR_LE : PL_CDR_BE;
    src.buf = NN_RMSG_PAYLOADOFF (fragchain->rmsg, qos_offset);
    src.bufsz = NN_RDATA_PAYLOAD_OFF (fragchain) - qos_offset;
    src.strict = NN_STRICT_P (gv->config);
    src.factory = gv->m_factory;
    src.logconfig = &gv->logconfig;
    if ((plist_ret = nn_plist_init_frommsg (&qos, NULL, PP_STATUSINFO | PP_KEYHASH | PP_COHERENT_SET, 0, &src)) < 0)
    {
      if (plist_ret != DDS_RETCODE_UNSUPPORTED)
        GVWARNING ("data(application, vendor %u.%u): "PGUIDFMT" #%"PRId64": invalid inline qos\n",
                   src.vendorid.id[0], src.vendorid.id[1], PGUID (pwr->e.guid), sampleinfo->seq);
      return 0;
    }
    statusinfo = (qos.present & PP_STATUSINFO) ? qos.statusinfo : 0;
  }

  /* FIXME: should it be 0, local wall clock time or INVALID? */
  const nn_wctime_t tstamp = (sampleinfo->timestamp.v != NN_WCTIME_INVALID.v) ? sampleinfo->timestamp : ((nn_wctime_t) {0});
  struct ddsi_writer_info wrinfo;
  ddsi_make_writer_info (&wrinfo, &pwr->e, pwr->c.xqos);

  if (rdguid == NULL)
  {
    /* FIXME: Retry loop, for re-delivery of rejected reliable samples. Is a
       temporary hack till throttling back of writer is implemented (with late
       acknowledgement of sample and nack). */
  retry:
    ddsrt_mutex_lock (&pwr->rdary.rdary_lock);
    if (pwr->rdary.fastpath_ok)
    {
      struct reader ** const rdary = pwr->rdary.rdary;
      if (rdary[0])
      {
        struct ddsi_serdata *payload;
        struct ddsi_tkmap_instance *tk;
        if ((payload = new_sample_from_data (&tk, gv, sampleinfo, data_smhdr_flags, &qos, fragchain, statusinfo, tstamp, rdary[0]->topic)) != NULL)
        {
          ETRACE (pwr, " => EVERYONE\n");
          uint32_t i = 0;
          do {
            if (!ddsi_rhc_store (rdary[i]->rhc, &wrinfo, payload, tk))
            {
              if (pwr_locked) ddsrt_mutex_unlock (&pwr->e.lock);
              ddsrt_mutex_unlock (&pwr->rdary.rdary_lock);
              /* It is painful to drop the sample, but there is no guarantee that the readers
                 will still be there after unlocking; indeed, it is even possible that the
                 topic definition got replaced in the meantime.  Fortunately, this is in
                 the midst of a FIXME for many other reasons. */
              free_sample_after_store (gv, payload, tk);
              dds_sleepfor (DDS_MSECS (10));
              if (pwr_locked) ddsrt_mutex_lock (&pwr->e.lock);
              goto retry;
            }
          } while (rdary[++i]);
          free_sample_after_store (gv, payload, tk);
        }
      }
      ddsrt_mutex_unlock (&pwr->rdary.rdary_lock);
    }
    else
    {
      /* When deleting, pwr is no longer accessible via the hash
         tables, and consequently, a reader may be deleted without
         it being possible to remove it from rdary. The primary
         reason rdary exists is to avoid locking the proxy writer
         but this is less of an issue when we are deleting it, so
         we fall back to using the GUIDs so that we can deliver all
         samples we received from it. As writer being deleted any
         reliable samples that are rejected are simply discarded. */
      ddsrt_avl_iter_t it;
      struct reader *rd;
      ddsrt_mutex_unlock (&pwr->rdary.rdary_lock);
      if (!pwr_locked) ddsrt_mutex_lock (&pwr->e.lock);
      if ((rd = proxy_writer_first_in_sync_reader (pwr, &it)) != NULL)
      {
        struct ddsi_serdata *payload;
        struct ddsi_tkmap_instance *tk;
        if ((payload = new_sample_from_data (&tk, gv, sampleinfo, data_smhdr_flags, &qos, fragchain, statusinfo, tstamp, rd->topic)) != NULL)
        {
          ETRACE (pwr, " =>");
          do {
            ETRACE (pwr, " "PGUIDFMT, PGUID (rd->e.guid));
            (void) ddsi_rhc_store (rd->rhc, &wrinfo, payload, tk);
            rd = proxy_writer_next_in_sync_reader (pwr, &it);
          } while (rd != NULL);
          free_sample_after_store (gv, payload, tk);
          ETRACE (pwr, "\n");
        }
      }
      if (!pwr_locked) ddsrt_mutex_unlock (&pwr->e.lock);
    }

    ddsrt_atomic_st32 (&pwr->next_deliv_seq_lowword, (uint32_t) (sampleinfo->seq + 1));
  }
  else
  {
    struct reader *rd = ephash_lookup_reader_guid (gv->guid_hash, rdguid);
    if (rd != NULL)
    {
      struct ddsi_serdata *payload;
      struct ddsi_tkmap_instance *tk;
      if ((payload = new_sample_from_data (&tk, gv, sampleinfo, data_smhdr_flags, &qos, fragchain, statusinfo, tstamp, rd->topic)) != NULL)
      {
        ETRACE (pwr, " =>"PGUIDFMT"\n", PGUID (*rdguid));
        /* FIXME: why look up rd,pwr again? Their states remains valid while the thread stays
           "awake" (although a delete can be initiated), and blocking like this is a stopgap
           anyway -- quite possibly to abort once either is deleted */
        while (!ddsi_rhc_store (rd->rhc, &wrinfo, payload, tk))
        {
          if (pwr_locked) ddsrt_mutex_unlock (&pwr->e.lock);
          dds_sleepfor (DDS_MSECS (1));
          if (pwr_locked) ddsrt_mutex_lock (&pwr->e.lock);
          if (ephash_lookup_reader_guid (gv->guid_hash, rdguid) == NULL ||
              ephash_lookup_proxy_writer_guid (gv->guid_hash, &pwr->e.guid) == NULL)
          {
            /* give up when reader or proxy writer no longer accessible */
            break;
          }
        }
        free_sample_after_store (gv, payload, tk);
      }
    }
  }
  nn_plist_fini (&qos);
  return 0;
}

int user_dqueue_handler (const struct nn_rsample_info *sampleinfo, const struct nn_rdata *fragchain, const ddsi_guid_t *rdguid, UNUSED_ARG (void *qarg))
{
  int res;
  res = deliver_user_data (sampleinfo, fragchain, rdguid, 0);
  return res;
}

static void deliver_user_data_synchronously (struct nn_rsample_chain *sc, const ddsi_guid_t *rdguid)
{
  while (sc->first)
  {
    struct nn_rsample_chain_elem *e = sc->first;
    sc->first = e->next;
    if (e->sampleinfo != NULL)
    {
      /* Must not try to deliver a gap -- possibly a FIXME for
         sample_lost events. Also note that the synchronous path is
         _never_ used for historical data, and therefore never has the
         GUID of a reader to deliver to */
      deliver_user_data (e->sampleinfo, e->fragchain, rdguid, 1);
    }
    nn_fragchain_unref (e->fragchain);
  }
}

static void clean_defrag (struct proxy_writer *pwr)
{
  seqno_t seq = nn_reorder_next_seq (pwr->reorder);
  if (pwr->n_readers_out_of_sync > 0)
  {
    struct pwr_rd_match *wn;
    for (wn = ddsrt_avl_find_min (&pwr_readers_treedef, &pwr->readers); wn != NULL; wn = ddsrt_avl_find_succ (&pwr_readers_treedef, &pwr->readers, wn))
    {
      if (wn->in_sync == PRMSS_OUT_OF_SYNC)
      {
        seqno_t seq1 = nn_reorder_next_seq (wn->u.not_in_sync.reorder);
        if (seq1 < seq)
          seq = seq1;
      }
    }
  }
  nn_defrag_notegap (pwr->defrag, 1, seq);
}

static void handle_regular (struct receiver_state *rst, nn_etime_t tnow, struct nn_rmsg *rmsg, const Data_DataFrag_common_t *msg, const struct nn_rsample_info *sampleinfo, uint32_t fragnum, struct nn_rdata *rdata, struct nn_dqueue **deferred_wakeup)
{
  struct proxy_writer *pwr;
  struct nn_rsample *rsample;
  ddsi_guid_t dst;

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

  /* liveliness is still only implemented partially (with all set to
     AUTOMATIC, BY_PARTICIPANT, &c.), so we simply renew the proxy
     participant's lease. */
  lease_renew (ddsrt_atomic_ldvoidp (&pwr->c.proxypp->lease), tnow);

  /* Shouldn't lock the full writer, but will do so for now */
  ddsrt_mutex_lock (&pwr->e.lock);

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
  if (!pwr->have_seen_heartbeat && pwr->n_reliable_readers > 0)
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
    pwr->last_fragnum = fragnum;
    pwr->last_fragnum_reset = 0;
  }
  else if (sampleinfo->seq == pwr->last_seq && fragnum > pwr->last_fragnum)
  {
    pwr->last_fragnum = fragnum;
    pwr->last_fragnum_reset = 0;
  }

  clean_defrag (pwr);

  if ((rsample = nn_defrag_rsample (pwr->defrag, rdata, sampleinfo)) != NULL)
  {
    int refc_adjust = 0;
    struct nn_rsample_chain sc;
    struct nn_rdata *fragchain = nn_rsample_fragchain (rsample);
    nn_reorder_result_t rres;

    rres = nn_reorder_rsample (&sc, pwr->reorder, rsample, &refc_adjust, 0); // nn_dqueue_is_full (pwr->dqueue));

    if (rres == NN_REORDER_ACCEPT && pwr->n_reliable_readers == 0)
    {
      /* If no reliable readers but the reorder buffer accepted the
         sample, it must be a reliable proxy writer with only
         unreliable readers.  "Inserting" a Gap [1, sampleinfo->seq)
         will force delivery of this sample, and not cause the gap to
         be added to the reorder admin. */
      int gap_refc_adjust = 0;
      rres = nn_reorder_gap (&sc, pwr->reorder, rdata, 1, sampleinfo->seq, &gap_refc_adjust);
      assert (rres > 0);
      assert (gap_refc_adjust == 0);
    }

    if (rres > 0)
    {
      /* Enqueue or deliver with pwr->e.lock held: to ensure no other
         receive thread's data gets interleaved -- arguably delivery
         needn't be exactly in-order, which would allow us to do this
         without pwr->e.lock held. */
      if (pwr->deliver_synchronously)
      {
        /* FIXME: just in case the synchronous delivery runs into a delay caused
           by the current mishandling of resource limits */
        if (*deferred_wakeup)
          dd_dqueue_enqueue_trigger (*deferred_wakeup);
        deliver_user_data_synchronously (&sc, NULL);
      }
      else
      {
        if (nn_dqueue_enqueue_deferred_wakeup (pwr->dqueue, &sc, rres))
        {
          if (*deferred_wakeup && *deferred_wakeup != pwr->dqueue)
            dd_dqueue_enqueue_trigger (*deferred_wakeup);
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
      struct pwr_rd_match *wn;
      struct nn_rsample *rsample_dup = NULL;
      int reuse_rsample_dup = 0;
      for (wn = ddsrt_avl_iter_first (&pwr_readers_treedef, &pwr->readers, &it); wn != NULL; wn = ddsrt_avl_iter_next (&it))
      {
        nn_reorder_result_t rres2;
        if (wn->in_sync == PRMSS_SYNC)
          continue;
        /* only need to get a copy of the first sample, because that's the one
           that triggered delivery */
        if (!reuse_rsample_dup)
          rsample_dup = nn_reorder_rsample_dup_first (rmsg, rsample);
        rres2 = nn_reorder_rsample (&sc, wn->u.not_in_sync.reorder, rsample_dup, &refc_adjust, nn_dqueue_is_full (pwr->dqueue));
        switch (rres2)
        {
          case NN_REORDER_TOO_OLD:
          case NN_REORDER_REJECT:
            reuse_rsample_dup = 1;
            break;
          case NN_REORDER_ACCEPT:
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
               NN_REORDER_DELIVER case in outer switch. */
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
                dd_dqueue_enqueue_trigger (*deferred_wakeup);
                *deferred_wakeup = NULL;
              }
              nn_dqueue_enqueue1 (pwr->dqueue, &wn->rd_guid, &sc, rres2);
            }
            break;
        }
      }
    }

    nn_fragchain_adjust_refcount (fragchain, refc_adjust);
  }
  ddsrt_mutex_unlock (&pwr->e.lock);
  nn_dqueue_wait_until_empty_if_full (pwr->dqueue);
}

static int handle_SPDP (const struct nn_rsample_info *sampleinfo, struct nn_rdata *rdata)
{
  struct q_globals * const gv = sampleinfo->rst->gv;
  struct nn_rsample *rsample;
  struct nn_rsample_chain sc;
  struct nn_rdata *fragchain;
  nn_reorder_result_t rres;
  int refc_adjust = 0;
  ddsrt_mutex_lock (&gv->spdp_lock);
  rsample = nn_defrag_rsample (gv->spdp_defrag, rdata, sampleinfo);
  fragchain = nn_rsample_fragchain (rsample);
  if ((rres = nn_reorder_rsample (&sc, gv->spdp_reorder, rsample, &refc_adjust, nn_dqueue_is_full (gv->builtins_dqueue))) > 0)
    nn_dqueue_enqueue (gv->builtins_dqueue, &sc, rres);
  nn_fragchain_adjust_refcount (fragchain, refc_adjust);
  ddsrt_mutex_unlock (&gv->spdp_lock);
  return 0;
}

static void drop_oversize (struct receiver_state *rst, struct nn_rmsg *rmsg, const Data_DataFrag_common_t *msg, struct nn_rsample_info *sampleinfo)
{
  struct proxy_writer *pwr = sampleinfo->pwr;
  if (pwr == NULL)
  {
    /* No proxy writer means nothing really gets done with, unless it
       is SPDP.  SPDP is periodic, so oversize discovery packets would
       cause periodic warnings. */
    if (msg->writerId.u == NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER)
    {
      DDS_CWARNING (&rst->gv->logconfig, "dropping oversize (%"PRIu32" > %"PRIu32") SPDP sample %"PRId64" from remote writer "PGUIDFMT"\n",
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
    struct nn_rdata *gap = nn_rdata_newgap (rmsg);
    ddsi_guid_t dst;
    struct pwr_rd_match *wn;
    int gap_was_valuable;

    dst.prefix = rst->dst_guid_prefix;
    dst.entityid = msg->readerId;

    ddsrt_mutex_lock (&pwr->e.lock);
    wn = ddsrt_avl_lookup (&pwr_readers_treedef, &pwr->readers, &dst);
    gap_was_valuable = handle_one_gap (pwr, wn, sampleinfo->seq, sampleinfo->seq+1, gap, &refc_adjust);
    nn_fragchain_adjust_refcount (gap, refc_adjust);
    ddsrt_mutex_unlock (&pwr->e.lock);

    if (gap_was_valuable)
    {
      const char *tname = (pwr->c.xqos->present & QP_TOPIC_NAME) ? pwr->c.xqos->topic_name : "(null)";
      const char *ttname = (pwr->c.xqos->present & QP_TYPE_NAME) ? pwr->c.xqos->type_name : "(null)";
      DDS_CWARNING (&rst->gv->logconfig, "dropping oversize (%"PRIu32" > %"PRIu32") sample %"PRId64" from remote writer "PGUIDFMT" %s/%s\n",
                    sampleinfo->size, rst->gv->config.max_sample_size, sampleinfo->seq,
                    PGUIDPREFIX (rst->src_guid_prefix), msg->writerId.u,
                    tname, ttname);
    }
  }
}

static int handle_Data (struct receiver_state *rst, nn_etime_t tnow, struct nn_rmsg *rmsg, const Data_t *msg, size_t size, struct nn_rsample_info *sampleinfo, unsigned char *datap, struct nn_dqueue **deferred_wakeup)
{
  RSTTRACE ("DATA("PGUIDFMT" -> "PGUIDFMT" #%"PRId64,
            PGUIDPREFIX (rst->src_guid_prefix), msg->x.writerId.u,
            PGUIDPREFIX (rst->dst_guid_prefix), msg->x.readerId.u,
            fromSN (msg->x.writerSN));
  if (!rst->forme)
  {
    RSTTRACE (" not-for-me)");
    return 1;
  }

  if (sampleinfo->size > rst->gv->config.max_sample_size)
    drop_oversize (rst, rmsg, &msg->x, sampleinfo);
  else
  {
    struct nn_rdata *rdata;
    unsigned submsg_offset, payload_offset;
    submsg_offset = (unsigned) ((unsigned char *) msg - NN_RMSG_PAYLOAD (rmsg));
    if (datap)
    {
      payload_offset = (unsigned) ((unsigned char *) datap - NN_RMSG_PAYLOAD (rmsg));
    }
    else
    {
      payload_offset = submsg_offset + (unsigned) size;
    }
    rdata = nn_rdata_new (rmsg, 0, sampleinfo->size, submsg_offset, payload_offset);

    if (msg->x.writerId.u == NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER)
      /* SPDP needs special treatment: there are no proxy writers for it
         and we accept data from unknown sources */
    {
      handle_SPDP (sampleinfo, rdata);
    }
    else
    {
      handle_regular (rst, tnow, rmsg, &msg->x, sampleinfo, ~0u, rdata, deferred_wakeup);
    }
  }
  RSTTRACE (")");
  return 1;
}

static int handle_DataFrag (struct receiver_state *rst, nn_etime_t tnow, struct nn_rmsg *rmsg, const DataFrag_t *msg, size_t size, struct nn_rsample_info *sampleinfo, unsigned char *datap, struct nn_dqueue **deferred_wakeup)
{
  RSTTRACE ("DATAFRAG("PGUIDFMT" -> "PGUIDFMT" #%"PRId64"/[%u..%u]",
            PGUIDPREFIX (rst->src_guid_prefix), msg->x.writerId.u,
            PGUIDPREFIX (rst->dst_guid_prefix), msg->x.readerId.u,
            fromSN (msg->x.writerSN),
            msg->fragmentStartingNum, msg->fragmentStartingNum + msg->fragmentsInSubmessage - 1);
  if (!rst->forme)
  {
    RSTTRACE (" not-for-me)");
    return 1;
  }

  if (sampleinfo->size > rst->gv->config.max_sample_size)
    drop_oversize (rst, rmsg, &msg->x, sampleinfo);
  else
  {
    struct nn_rdata *rdata;
    unsigned submsg_offset, payload_offset;
    uint32_t begin, endp1;
    if (msg->x.writerId.u == NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER)
    {
      DDS_CWARNING (&rst->gv->logconfig, "DATAFRAG("PGUIDFMT" #%"PRId64" -> "PGUIDFMT") - fragmented builtin data not yet supported\n",
                    PGUIDPREFIX (rst->src_guid_prefix), msg->x.writerId.u, fromSN (msg->x.writerSN),
                    PGUIDPREFIX (rst->dst_guid_prefix), msg->x.readerId.u);
      return 1;
    }

    submsg_offset = (unsigned) ((unsigned char *) msg - NN_RMSG_PAYLOAD (rmsg));
    if (datap)
      payload_offset = (unsigned) ((unsigned char *) datap - NN_RMSG_PAYLOAD (rmsg));
    else
      payload_offset = submsg_offset + (unsigned) size;

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

    rdata = nn_rdata_new (rmsg, begin, endp1, submsg_offset, payload_offset);

    /* Fragment numbers in DDSI2 internal representation are 0-based,
       whereas in DDSI they are 1-based.  The highest fragment number in
       the sample in internal representation is therefore START+CNT-2,
       rather than the expect START+CNT-1.  Nothing will go terribly
       wrong, it'll simply generate a request for retransmitting a
       non-existent fragment.  The other side SHOULD be capable of
       dealing with that. */
    handle_regular (rst, tnow, rmsg, &msg->x, sampleinfo, msg->fragmentStartingNum + msg->fragmentsInSubmessage - 2, rdata, deferred_wakeup);
  }
  RSTTRACE (")");
  return 1;
}

static void malformed_packet_received_nosubmsg (const struct q_globals *gv, const unsigned char * msg, ssize_t len, const char *state, nn_vendorid_t vendorid
)
{
  char tmp[1024];
  ssize_t i;
  size_t pos;

  /* Show beginning of message (as hex dumps) */
  pos = (size_t) snprintf (tmp, sizeof (tmp), "malformed packet received from vendor %u.%u state %s <", vendorid.id[0], vendorid.id[1], state);
  for (i = 0; i < 32 && i < len && pos < sizeof (tmp); i++)
    pos += (size_t) snprintf (tmp + pos, sizeof (tmp) - pos, "%s%02x", (i > 0 && (i%4) == 0) ? " " : "", msg[i]);
  if (pos < sizeof (tmp))
    pos += (size_t) snprintf (tmp + pos, sizeof (tmp) - pos, "> (note: maybe partially bswap'd)");
  assert (pos < sizeof (tmp));
  GVWARNING ("%s\n", tmp);
}

static void malformed_packet_received (const struct q_globals *gv, const unsigned char *msg, const unsigned char *submsg, size_t len, const char *state, SubmessageKind_t smkind, nn_vendorid_t vendorid)
{
  char tmp[1024];
  size_t i, pos, smsize;
  assert (submsg >= msg && submsg < msg + len);

  /* Show beginning of message and of submessage (as hex dumps) */
  pos = (size_t) snprintf (tmp, sizeof (tmp), "malformed packet received from vendor %u.%u state %s <", vendorid.id[0], vendorid.id[1], state);
  for (i = 0; i < 32 && i < len && msg + i < submsg && pos < sizeof (tmp); i++)
    pos += (size_t) snprintf (tmp + pos, sizeof (tmp) - pos, "%s%02x", (i > 0 && (i%4) == 0) ? " " : "", msg[i]);
  if (pos < sizeof (tmp))
    pos += (size_t) snprintf (tmp + pos, sizeof (tmp) - pos, " @0x%x ", (int) (submsg - msg));
  for (i = 0; i < 32 && i < len - (size_t) (submsg - msg) && pos < sizeof (tmp); i++)
    pos += (size_t) snprintf (tmp + pos, sizeof (tmp) - pos, "%s%02x", (i > 0 && (i%4) == 0) ? " " : "", submsg[i]);
  if (pos < sizeof (tmp))
    pos += (size_t) snprintf (tmp + pos, sizeof (tmp) - pos, "> (note: maybe partially bswap'd)");
  assert (pos < (int) sizeof (tmp));

  /* Partially decode header if we have enough bytes available */
  smsize = len - (size_t) (submsg - msg);
  switch (smkind)
  {
    case SMID_ACKNACK:
      if (smsize >= sizeof (AckNack_t))
      {
        const AckNack_t *x = (const AckNack_t *) submsg;
        (void) snprintf (tmp + pos, sizeof (tmp) - pos, " {{%x,%x,%u},%"PRIx32",%"PRIx32",%"PRId64",%"PRIu32"}",
                         x->smhdr.submessageId, x->smhdr.flags, x->smhdr.octetsToNextHeader,
                         x->readerId.u, x->writerId.u, fromSN (x->readerSNState.bitmap_base),
                         x->readerSNState.numbits);
      }
      break;
    case SMID_HEARTBEAT:
      if (smsize >= sizeof (Heartbeat_t))
      {
        const Heartbeat_t *x = (const Heartbeat_t *) submsg;
        (void) snprintf (tmp + pos, sizeof (tmp) - pos, " {{%x,%x,%u},%"PRIx32",%"PRIx32",%"PRId64",%"PRId64"}",
                         x->smhdr.submessageId, x->smhdr.flags, x->smhdr.octetsToNextHeader,
                         x->readerId.u, x->writerId.u, fromSN (x->firstSN), fromSN (x->lastSN));
      }
      break;
    case SMID_GAP:
      if (smsize >= sizeof (Gap_t))
      {
        const Gap_t *x = (const Gap_t *) submsg;
        (void) snprintf (tmp + pos, sizeof (tmp) - pos, " {{%x,%x,%u},%"PRIx32",%"PRIx32",%"PRId64",%"PRId64",%"PRIu32"}",
                         x->smhdr.submessageId, x->smhdr.flags, x->smhdr.octetsToNextHeader,
                         x->readerId.u, x->writerId.u, fromSN (x->gapStart),
                         fromSN (x->gapList.bitmap_base), x->gapList.numbits);
      }
      break;
    case SMID_NACK_FRAG:
      if (smsize >= sizeof (NackFrag_t))
      {
        const NackFrag_t *x = (const NackFrag_t *) submsg;
        (void) snprintf (tmp + pos, sizeof (tmp) - pos, " {{%x,%x,%u},%"PRIx32",%"PRIx32",%"PRId64",%u,%"PRIu32"}",
                         x->smhdr.submessageId, x->smhdr.flags, x->smhdr.octetsToNextHeader,
                         x->readerId.u, x->writerId.u, fromSN (x->writerSN),
                         x->fragmentNumberState.bitmap_base, x->fragmentNumberState.numbits);
      }
      break;
    case SMID_HEARTBEAT_FRAG:
      if (smsize >= sizeof (HeartbeatFrag_t))
      {
        const HeartbeatFrag_t *x = (const HeartbeatFrag_t *) submsg;
        (void) snprintf (tmp + pos, sizeof (tmp) - pos, " {{%x,%x,%u},%"PRIx32",%"PRIx32",%"PRId64",%u}",
                         x->smhdr.submessageId, x->smhdr.flags, x->smhdr.octetsToNextHeader,
                         x->readerId.u, x->writerId.u, fromSN (x->writerSN),
                         x->lastFragmentNum);
      }
      break;
    case SMID_DATA:
      if (smsize >= sizeof (Data_t))
      {
        const Data_t *x = (const Data_t *) submsg;
        (void) snprintf (tmp + pos, sizeof (tmp) - pos, " {{%x,%x,%u},%x,%u,%"PRIx32",%"PRIx32",%"PRId64"}",
                         x->x.smhdr.submessageId, x->x.smhdr.flags, x->x.smhdr.octetsToNextHeader,
                         x->x.extraFlags, x->x.octetsToInlineQos,
                         x->x.readerId.u, x->x.writerId.u, fromSN (x->x.writerSN));
      }
      break;
    case SMID_DATA_FRAG:
      if (smsize >= sizeof (DataFrag_t))
      {
        const DataFrag_t *x = (const DataFrag_t *) submsg;
        (void) snprintf (tmp + pos, sizeof (tmp) - pos, " {{%x,%x,%u},%x,%u,%"PRIx32",%"PRIx32",%"PRId64",%u,%u,%u,%"PRIu32"}",
                         x->x.smhdr.submessageId, x->x.smhdr.flags, x->x.smhdr.octetsToNextHeader,
                         x->x.extraFlags, x->x.octetsToInlineQos,
                         x->x.readerId.u, x->x.writerId.u, fromSN (x->x.writerSN),
                         x->fragmentStartingNum, x->fragmentsInSubmessage, x->fragmentSize, x->sampleSize);
      }
      break;
    default:
      break;
  }

  GVWARNING ("%s\n", tmp);
}

static struct receiver_state *rst_cow_if_needed (int *rst_live, struct nn_rmsg *rmsg, struct receiver_state *rst)
{
  if (! *rst_live)
    return rst;
  else
  {
    struct receiver_state *nrst = nn_rmsg_alloc (rmsg, sizeof (*nrst));
    *nrst = *rst;
    *rst_live = 0;
    return nrst;
  }
}

static int handle_submsg_sequence
(
  struct thread_state1 * const ts1,
  struct q_globals *gv,
  ddsi_tran_conn_t conn,
  const nn_locator_t *srcloc,
  nn_wctime_t tnowWC,
  nn_etime_t tnowE,
  const ddsi_guid_prefix_t * const src_prefix,
  const ddsi_guid_prefix_t * const dst_prefix,
  unsigned char * const msg /* NOT const - we may byteswap it */,
  const size_t len,
  unsigned char * submsg /* aliases somewhere in msg */,
  struct nn_rmsg * const rmsg
)
{
  const char *state;
  SubmessageKind_t state_smkind;
  Header_t * hdr = (Header_t *) msg;
  struct receiver_state *rst;
  int rst_live, ts_for_latmeas;
  nn_wctime_t timestamp;
  size_t submsg_size = 0;
  unsigned char * end = msg + len;
  struct nn_dqueue *deferred_wakeup = NULL;

  /* Receiver state is dynamically allocated with lifetime bound to
     the message.  Updates cause a new copy to be created if the
     current one is "live", i.e., possibly referenced by a
     submessage (for now, only Data(Frag)). */
  rst = nn_rmsg_alloc (rmsg, sizeof (*rst));
  memset (rst, 0, sizeof (*rst));
  rst->conn = conn;
  rst->src_guid_prefix = *src_prefix;
  if (dst_prefix)
  {
    rst->dst_guid_prefix = *dst_prefix;
  }
  /* "forme" is a whether the current submessage is intended for this
     instance of DDSI2 and is roughly equivalent to
       (dst_prefix == 0) ||
       (ephash_lookup_participant_guid(dst_prefix:1c1) != 0)
     they are only roughly equivalent because the second term can become
     false at any time. That's ok: it's real purpose is to filter out
     discovery data accidentally sent by Cloud */
  rst->forme = 1;
  rst->vendor = hdr->vendorid;
  rst->protocol_version = hdr->version;
  rst->srcloc = *srcloc;
  rst->gv = gv;
  rst_live = 0;
  ts_for_latmeas = 0;
  timestamp = NN_WCTIME_INVALID;

  assert (thread_is_asleep ());
  thread_state_awake_fixed_domain (ts1);
  while (submsg <= (end - sizeof (SubmessageHeader_t)))
  {
    Submessage_t *sm = (Submessage_t *) submsg;
    bool byteswap;
    unsigned octetsToNextHeader;

    if (sm->smhdr.flags & SMFLAG_ENDIANNESS)
    {
      byteswap = !(DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN);
    }
    else
    {
      byteswap =  (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN);
    }
    if (byteswap)
    {
      sm->smhdr.octetsToNextHeader = ddsrt_bswap2u (sm->smhdr.octetsToNextHeader);
    }

    octetsToNextHeader = sm->smhdr.octetsToNextHeader;
    if (octetsToNextHeader != 0)
    {
      submsg_size = RTPS_SUBMESSAGE_HEADER_SIZE + octetsToNextHeader;
    }
    else if (sm->smhdr.submessageId == SMID_PAD || sm->smhdr.submessageId == SMID_INFO_TS)
    {
      submsg_size = RTPS_SUBMESSAGE_HEADER_SIZE;
    }
    else
    {
      submsg_size = (unsigned) (end - submsg);
    }
    /*GVTRACE ("submsg_size %d\n", submsg_size);*/

    if (submsg + submsg_size > end)
    {
      GVTRACE (" BREAK (%u %"PRIuSIZE": %p %u)\n", (unsigned) (submsg - msg), submsg_size, (void *) msg, (unsigned) len);
      break;
    }

    thread_state_awake_to_awake_no_nest (ts1);
    state_smkind = sm->smhdr.submessageId;
    switch (sm->smhdr.submessageId)
    {
      case SMID_PAD:
        GVTRACE ("PAD");
        break;
      case SMID_ACKNACK:
        state = "parse:acknack";
        if (!valid_AckNack (rst, &sm->acknack, submsg_size, byteswap))
          goto malformed;
        handle_AckNack (rst, tnowE, &sm->acknack, ts_for_latmeas ? timestamp : NN_WCTIME_INVALID);
        ts_for_latmeas = 0;
        break;
      case SMID_HEARTBEAT:
        state = "parse:heartbeat";
        if (!valid_Heartbeat (&sm->heartbeat, submsg_size, byteswap))
          goto malformed;
        handle_Heartbeat (rst, tnowE, rmsg, &sm->heartbeat, ts_for_latmeas ? timestamp : NN_WCTIME_INVALID);
        ts_for_latmeas = 0;
        break;
      case SMID_GAP:
        state = "parse:gap";
        /* Gap is handled synchronously in principle, but may
           sometimes have to record a gap in the reorder admin.  The
           first case by definition doesn't need to set "rst_live",
           the second one avoids that because it doesn't require the
           rst after inserting the gap in the admin. */
        if (!valid_Gap (&sm->gap, submsg_size, byteswap))
          goto malformed;
        handle_Gap (rst, tnowE, rmsg, &sm->gap);
        ts_for_latmeas = 0;
        break;
      case SMID_INFO_TS:
        state = "parse:info_ts";
        if (!valid_InfoTS (&sm->infots, submsg_size, byteswap))
          goto malformed;
        handle_InfoTS (rst, &sm->infots, &timestamp);
        ts_for_latmeas = 1;
        break;
      case SMID_INFO_SRC:
        state = "parse:info_src";
        if (!valid_InfoSRC (&sm->infosrc, submsg_size, byteswap))
          goto malformed;
        rst = rst_cow_if_needed (&rst_live, rmsg, rst);
        handle_InfoSRC (rst, &sm->infosrc);
        /* no effect on ts_for_latmeas */
        break;
      case SMID_INFO_REPLY_IP4:
#if 0
        state = "parse:info_reply_ip4";
#endif
        GVTRACE ("INFO_REPLY_IP4");
        /* no effect on ts_for_latmeas */
        break;
      case SMID_INFO_DST:
        state = "parse:info_dst";
        if (!valid_InfoDST (&sm->infodst, submsg_size, byteswap))
          goto malformed;
        rst = rst_cow_if_needed (&rst_live, rmsg, rst);
        handle_InfoDST (rst, &sm->infodst, dst_prefix);
        /* no effect on ts_for_latmeas */
        break;
      case SMID_INFO_REPLY:
#if 0
        state = "parse:info_reply";
#endif
        GVTRACE ("INFO_REPLY");
        /* no effect on ts_for_latmeas */
        break;
      case SMID_NACK_FRAG:
        state = "parse:nackfrag";
        if (!valid_NackFrag (&sm->nackfrag, submsg_size, byteswap))
          goto malformed;
        handle_NackFrag (rst, tnowE, &sm->nackfrag);
        ts_for_latmeas = 0;
        break;
      case SMID_HEARTBEAT_FRAG:
        state = "parse:heartbeatfrag";
        if (!valid_HeartbeatFrag (&sm->heartbeatfrag, submsg_size, byteswap))
          goto malformed;
        handle_HeartbeatFrag (rst, tnowE, &sm->heartbeatfrag);
        ts_for_latmeas = 0;
        break;
      case SMID_DATA_FRAG:
        state = "parse:datafrag";
        {
          struct nn_rsample_info sampleinfo;
          unsigned char *datap;
          /* valid_DataFrag does not validate the payload */
          if (!valid_DataFrag (rst, rmsg, &sm->datafrag, submsg_size, byteswap, &sampleinfo, &datap))
            goto malformed;
          sampleinfo.timestamp = timestamp;
          sampleinfo.reception_timestamp = tnowWC;
          handle_DataFrag (rst, tnowE, rmsg, &sm->datafrag, submsg_size, &sampleinfo, datap, &deferred_wakeup);
          rst_live = 1;
          ts_for_latmeas = 0;
        }
        break;
      case SMID_DATA:
        state = "parse:data";
        {
          struct nn_rsample_info sampleinfo;
          unsigned char *datap;
          /* valid_Data does not validate the payload */
          if (!valid_Data (rst, rmsg, &sm->data, submsg_size, byteswap, &sampleinfo, &datap))
            goto malformed;
          sampleinfo.timestamp = timestamp;
          sampleinfo.reception_timestamp = tnowWC;
          handle_Data (rst, tnowE, rmsg, &sm->data, submsg_size, &sampleinfo, datap, &deferred_wakeup);
          rst_live = 1;
          ts_for_latmeas = 0;
        }
        break;
      case SMID_PT_MSG_LEN:
      {
#if 0
        state = "parse:msg_len";
#endif
        GVTRACE ("MSG_LEN(%"PRIu32")", ((MsgLen_t*) sm)->length);
        break;
      }
      case SMID_PT_ENTITY_ID:
      {
#if 0
        state = "parse:entity_id";
#endif
        GVTRACE ("ENTITY_ID");
        break;
      }
      default:
        state = "parse:undefined";
        GVTRACE ("UNDEFINED(%x)", sm->smhdr.submessageId);
        if (sm->smhdr.submessageId <= 0x7f)
        {
          /* Other submessages in the 0 .. 0x7f range may be added in
             future version of the protocol -- so an undefined code
             for the implemented version of the protocol indicates a
             malformed message. */
          if (rst->protocol_version.major < RTPS_MAJOR ||
              (rst->protocol_version.major == RTPS_MAJOR &&
               rst->protocol_version.minor < RTPS_MINOR_MINIMUM))
            goto malformed;
        }
        else if (vendor_is_eclipse (rst->vendor))
        {
          /* One wouldn't expect undefined stuff from ourselves,
             except that we need to be up- and backwards compatible
             with ourselves, too! */
#if 0
          goto malformed;
#endif
        }
        else
        {
          /* Ignore other vendors' private submessages */
        }
        ts_for_latmeas = 0;
        break;
    }
    submsg += submsg_size;
    GVTRACE ("\n");
  }
  if (submsg != end)
  {
    state = "parse:shortmsg";
    state_smkind = SMID_PAD;
    GVTRACE ("short (size %"PRIuSIZE" exp %p act %p)", submsg_size, (void *) submsg, (void *) end);
    goto malformed_asleep;
  }
  thread_state_asleep (ts1);
  assert (thread_is_asleep ());
  if (deferred_wakeup)
    dd_dqueue_enqueue_trigger (deferred_wakeup);
  return 0;

malformed:
  thread_state_asleep (ts1);
  assert (thread_is_asleep ());
malformed_asleep:
  assert (thread_is_asleep ());
  malformed_packet_received (rst->gv, msg, submsg, len, state, state_smkind, hdr->vendorid);
  if (deferred_wakeup)
    dd_dqueue_enqueue_trigger (deferred_wakeup);
  return -1;
}

static bool do_packet (struct thread_state1 * const ts1, struct q_globals *gv, ddsi_tran_conn_t conn, const ddsi_guid_prefix_t *guidprefix, struct nn_rbufpool *rbpool)
{
  /* UDP max packet size is 64kB */

  const size_t maxsz = gv->config.rmsg_chunk_size < 65536 ? gv->config.rmsg_chunk_size : 65536;
  const size_t ddsi_msg_len_size = 8;
  const size_t stream_hdr_size = RTPS_MESSAGE_HEADER_SIZE + ddsi_msg_len_size;
  ssize_t sz;
  struct nn_rmsg * rmsg = nn_rmsg_new (rbpool);
  unsigned char * buff;
  size_t buff_len = maxsz;
  Header_t * hdr;
  nn_locator_t srcloc;

  if (rmsg == NULL)
  {
    return false;
  }
  buff = (unsigned char *) NN_RMSG_PAYLOAD (rmsg);
  hdr = (Header_t*) buff;

  if (conn->m_stream)
  {
    MsgLen_t * ml = (MsgLen_t*) (buff + RTPS_MESSAGE_HEADER_SIZE);

    /*
      Read in packet header to get size of packet in MsgLen_t, then read in
      remainder of packet.
    */

    /* Read in DDSI header plus MSG_LEN sub message that follows it */

    sz = ddsi_conn_read (conn, buff, stream_hdr_size, true, &srcloc);
    if (sz == 0)
    {
      /* Spurious read -- which at this point is still ok */
      nn_rmsg_commit (rmsg);
      return true;
    }

    /* Read in remainder of packet */

    if (sz > 0)
    {
      int swap;

      if (ml->smhdr.flags & SMFLAG_ENDIANNESS)
      {
        swap = !(DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN);
      }
      else
      {
        swap =  (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN);
      }
      if (swap)
      {
        ml->length = ddsrt_bswap4u (ml->length);
      }

      if (ml->smhdr.submessageId != SMID_PT_MSG_LEN)
      {
        malformed_packet_received_nosubmsg (gv, buff, sz, "header", hdr->vendorid);
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
    nn_rmsg_setsize (rmsg, (uint32_t) sz);
    assert (thread_is_asleep ());

    if ((size_t)sz < RTPS_MESSAGE_HEADER_SIZE || *(uint32_t *)buff != NN_PROTOCOLID_AS_UINT32)
    {
      /* discard packets that are really too small or don't have magic cookie */
    }
    else if (hdr->version.major != RTPS_MAJOR || (hdr->version.major == RTPS_MAJOR && hdr->version.minor < RTPS_MINOR_MINIMUM))
    {
      if ((hdr->version.major == RTPS_MAJOR && hdr->version.minor < RTPS_MINOR_MINIMUM))
        GVTRACE ("HDR(%"PRIx32":%"PRIx32":%"PRIx32" vendor %d.%d) len %lu\n, version mismatch: %d.%d\n",
                 PGUIDPREFIX (hdr->guid_prefix), hdr->vendorid.id[0], hdr->vendorid.id[1], (unsigned long) sz, hdr->version.major, hdr->version.minor);
      if (NN_PEDANTIC_P (gv->config))
        malformed_packet_received_nosubmsg (gv, buff, sz, "header", hdr->vendorid);
    }
    else
    {
      hdr->guid_prefix = nn_ntoh_guid_prefix (hdr->guid_prefix);

      if (gv->logconfig.c.mask & DDS_LC_TRACE)
      {
        char addrstr[DDSI_LOCSTRLEN];
        ddsi_locator_to_string(gv, addrstr, sizeof(addrstr), &srcloc);
        GVTRACE ("HDR(%"PRIx32":%"PRIx32":%"PRIx32" vendor %d.%d) len %lu from %s\n",
                 PGUIDPREFIX (hdr->guid_prefix), hdr->vendorid.id[0], hdr->vendorid.id[1], (unsigned long) sz, addrstr);
      }

      handle_submsg_sequence (ts1, gv, conn, &srcloc, now (), now_et (), &hdr->guid_prefix, guidprefix, buff, (size_t) sz, buff + RTPS_MESSAGE_HEADER_SIZE, rmsg);
    }
  }
  nn_rmsg_commit (rmsg);
  return (sz > 0);
}

struct local_participant_desc
{
  ddsi_tran_conn_t m_conn;
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

static void rebuild_local_participant_set (struct thread_state1 * const ts1, struct q_globals *gv, struct local_participant_set *lps)
{
  struct ephash_enum_participant est;
  struct participant *pp;
  unsigned nps_alloc;
  GVTRACE ("pp set gen changed: local %"PRIu32" global %"PRIu32"\n", lps->gen, ddsrt_atomic_ld32 (&gv->participant_set_generation));
  thread_state_awake_fixed_domain (ts1);
 restart:
  lps->gen = ddsrt_atomic_ld32 (&gv->participant_set_generation);
  /* Actual local set of participants may never be older than the
     local generation count => membar to guarantee the ordering */
  ddsrt_atomic_fence_acq ();
  nps_alloc = gv->nparticipants;
  ddsrt_free (lps->ps);
  lps->nps = 0;
  lps->ps = (nps_alloc == 0) ? NULL : ddsrt_malloc (nps_alloc * sizeof (*lps->ps));
  ephash_enum_participant_init (&est, gv->guid_hash);
  while ((pp = ephash_enum_participant_next (&est)) != NULL)
  {
    if (lps->nps == nps_alloc)
    {
      /* New participants may get added while we do this (or
         existing ones removed), so we may have to restart if it
         turns out we didn't allocate enough memory [an
         alternative would be to realloc on the fly]. */
      ephash_enum_participant_fini (&est);
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
  ephash_enum_participant_fini (&est);

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
  thread_state_asleep (ts1);

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
  GVTRACE ("  nparticipants %u\n", lps->nps);
}

uint32_t listen_thread (struct ddsi_tran_listener *listener)
{
  struct q_globals *gv = listener->m_base.gv;
  ddsi_tran_conn_t conn;

  while (ddsrt_atomic_ld32 (&gv->rtps_keepgoing))
  {
    /* Accept connection from listener */

    conn = ddsi_listener_accept (listener);
    if (conn)
    {
      os_sockWaitsetAdd (gv->recv_threads[0].arg.u.many.ws, conn);
      os_sockWaitsetTrigger (gv->recv_threads[0].arg.u.many.ws);
    }
  }
  return 0;
}

static int recv_thread_waitset_add_conn (os_sockWaitset ws, ddsi_tran_conn_t conn)
{
  if (conn == NULL)
    return 0;
  else
  {
    struct q_globals *gv = conn->m_base.gv;
    for (uint32_t i = 0; i < gv->n_recv_threads; i++)
      if (gv->recv_threads[i].arg.mode == RTM_SINGLE && gv->recv_threads[i].arg.u.single.conn == conn)
        return 0;
    return os_sockWaitsetAdd (ws, conn);
  }
}

enum local_deaf_state_recover {
  LDSR_NORMAL    = 0, /* matches gv.deaf for normal operation */
  LDSR_DEAF      = 1, /* matches gv.deaf for "deaf" state */
  LDSR_REJOIN    = 2
};

struct local_deaf_state {
  enum local_deaf_state_recover state;
  nn_mtime_t tnext;
};

static int check_and_handle_deafness_recover (struct q_globals *gv, struct local_deaf_state *st, unsigned num_fixed_uc)
{
  int rebuildws = 0;
  if (now_mt().v < st->tnext.v)
  {
    GVTRACE ("check_and_handle_deafness_recover: state %d too early\n", (int)st->state);
    return 0;
  }
  switch (st->state)
  {
    case LDSR_NORMAL:
      assert(0);
      break;
    case LDSR_DEAF: {
      ddsi_tran_conn_t disc = gv->disc_conn_mc, data = gv->data_conn_mc;
      GVTRACE ("check_and_handle_deafness_recover: state %d create new sockets\n", (int) st->state);
      if (!create_multicast_sockets (gv))
        goto error;
      GVTRACE ("check_and_handle_deafness_recover: state %d transfer group membership admin\n", (int) st->state);
      ddsi_transfer_group_membership (gv->mship, disc, gv->disc_conn_mc);
      ddsi_transfer_group_membership (gv->mship, data, gv->data_conn_mc);
      GVTRACE ("check_and_handle_deafness_recover: state %d drop from waitset and add new\n", (int) st->state);
      /* see waitset construction code in recv_thread */
      os_sockWaitsetPurge (gv->recv_threads[0].arg.u.many.ws, num_fixed_uc);
      if (recv_thread_waitset_add_conn (gv->recv_threads[0].arg.u.many.ws, gv->disc_conn_mc) < 0)
        DDS_FATAL("check_and_handle_deafness_recover: failed to add disc_conn_mc to waitset\n");
      if (recv_thread_waitset_add_conn (gv->recv_threads[0].arg.u.many.ws, gv->data_conn_mc) < 0)
        DDS_FATAL("check_and_handle_deafness_recover: failed to add data_conn_mc to waitset\n");
      GVTRACE ("check_and_handle_deafness_recover: state %d close sockets\n", (int)st->state);
      ddsi_conn_free(disc);
      ddsi_conn_free(data);
      rebuildws = 1;
      st->state = LDSR_REJOIN;
    }
      /* FALLS THROUGH */
    case LDSR_REJOIN:
      GVTRACE ("check_and_handle_deafness_recover: state %d rejoin on disc socket\n", (int)st->state);
      if (ddsi_rejoin_transferred_mcgroups (gv, gv->mship, gv->disc_conn_mc) < 0)
        goto error;
      GVTRACE ("check_and_handle_deafness_recover: state %d rejoin on data socket\n", (int)st->state);
      if (ddsi_rejoin_transferred_mcgroups (gv, gv->mship, gv->data_conn_mc) < 0)
        goto error;
      GVTRACE ("check_and_handle_deafness_recover: state %d done\n", (int)st->state);
      st->state = LDSR_NORMAL;
      break;
  }
  GVTRACE ("check_and_handle_deafness_recover: state %d returning %d\n", (int)st->state, rebuildws);
  return rebuildws;
error:
  GVTRACE ("check_and_handle_deafness_recover: state %d failed, returning %d\n", (int)st->state, rebuildws);
  st->state = LDSR_DEAF;
  st->tnext = add_duration_to_mtime(now_mt(), T_SECOND);
  return rebuildws;
}

static int check_and_handle_deafness (struct q_globals *gv, struct local_deaf_state *st, unsigned num_fixed_uc)
{
  const int gv_deaf = gv->deaf;
  assert (gv_deaf == 0 || gv_deaf == 1);
  if (gv_deaf == (int)st->state)
    return 0;
  else if (gv_deaf)
  {
    GVTRACE ("check_and_handle_deafness: going deaf (%d -> %d)\n", (int)st->state, (int)LDSR_DEAF);
    st->state = LDSR_DEAF;
    st->tnext = now_mt();
    return 0;
  }
  else if (!gv->config.allowMulticast)
  {
    GVTRACE ("check_and_handle_deafness: no longer deaf (multicast disabled)\n");
    st->state = LDSR_NORMAL;
    return 0;
  }
  else
  {
    return check_and_handle_deafness_recover (gv, st, num_fixed_uc);
  }
}

void trigger_recv_threads (const struct q_globals *gv)
{
  for (uint32_t i = 0; i < gv->n_recv_threads; i++)
  {
    if (gv->recv_threads[i].ts == NULL)
      continue;
    switch (gv->recv_threads[i].arg.mode)
    {
      case RTM_SINGLE: {
        char buf[DDSI_LOCSTRLEN];
        char dummy = 0;
        const nn_locator_t *dst = gv->recv_threads[i].arg.u.single.loc;
        ddsrt_iovec_t iov;
        iov.iov_base = &dummy;
        iov.iov_len = 1;
        GVTRACE ("trigger_recv_threads: %d single %s\n", i, ddsi_locator_to_string (gv, buf, sizeof (buf), dst));
        ddsi_conn_write (gv->data_conn_uc, dst, 1, &iov, 0);
        break;
      }
      case RTM_MANY: {
        GVTRACE ("trigger_recv_threads: %d many %p\n", i, (void *) gv->recv_threads[i].arg.u.many.ws);
        os_sockWaitsetTrigger (gv->recv_threads[i].arg.u.many.ws);
        break;
      }
    }
  }
}

uint32_t recv_thread (void *vrecv_thread_arg)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  struct recv_thread_arg *recv_thread_arg = vrecv_thread_arg;
  struct q_globals * const gv = recv_thread_arg->gv;
  struct nn_rbufpool *rbpool = recv_thread_arg->rbpool;
  os_sockWaitset waitset = recv_thread_arg->mode == RTM_MANY ? recv_thread_arg->u.many.ws : NULL;
  nn_mtime_t next_thread_cputime = { 0 };

  nn_rbufpool_setowner (rbpool, ddsrt_thread_self ());
  if (waitset == NULL)
  {
    struct ddsi_tran_conn *conn = recv_thread_arg->u.single.conn;
    while (ddsrt_atomic_ld32 (&gv->rtps_keepgoing))
    {
      LOG_THREAD_CPUTIME (&gv->logconfig, next_thread_cputime);
      (void) do_packet (ts1, gv, conn, NULL, rbpool);
    }
  }
  else
  {
    struct local_participant_set lps;
    unsigned num_fixed = 0, num_fixed_uc = 0;
    os_sockWaitsetCtx ctx;
    struct local_deaf_state lds;
    lds.state = gv->deaf ? LDSR_DEAF : LDSR_NORMAL;
    lds.tnext = now_mt();
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
    }

    while (ddsrt_atomic_ld32 (&gv->rtps_keepgoing))
    {
      int rebuildws;
      LOG_THREAD_CPUTIME (&gv->logconfig, next_thread_cputime);
      rebuildws = check_and_handle_deafness (gv, &lds, num_fixed_uc);
      if (gv->config.many_sockets_mode != MSM_MANY_UNICAST)
      {
        /* no other sockets to check */
      }
      else if (ddsrt_atomic_ld32 (&gv->participant_set_generation) != lps.gen)
      {
        rebuildws = 1;
      }

      if (rebuildws && waitset && gv->config.many_sockets_mode == MSM_MANY_UNICAST)
      {
        /* first rebuild local participant set - unless someone's toggling "deafness", this
         only happens when the participant set has changed, so might as well rebuild it */
        rebuild_local_participant_set (ts1, gv, &lps);
        os_sockWaitsetPurge (waitset, num_fixed);
        for (uint32_t i = 0; i < lps.nps; i++)
        {
          if (lps.ps[i].m_conn)
            os_sockWaitsetAdd (waitset, lps.ps[i].m_conn);
        }
      }

      if ((ctx = os_sockWaitsetWait (waitset)) != NULL)
      {
        int idx;
        ddsi_tran_conn_t conn;
        while ((idx = os_sockWaitsetNextEvent (ctx, &conn)) >= 0)
        {
          const ddsi_guid_prefix_t *guid_prefix;
          if (((unsigned)idx < num_fixed) || gv->config.many_sockets_mode != MSM_MANY_UNICAST)
            guid_prefix = NULL;
          else
            guid_prefix = &lps.ps[(unsigned)idx - num_fixed].guid_prefix;
          /* Process message and clean out connection if failed or closed */
          if (!do_packet (ts1, gv, conn, guid_prefix, rbpool) && !conn->m_connless)
            ddsi_conn_free (conn);
        }
      }
    }
    local_participant_set_fini (&lps);
  }
  return 0;
}
