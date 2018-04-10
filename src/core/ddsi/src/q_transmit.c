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
#include <assert.h>
#include <math.h>

#include "os/os.h"

#include "util/ut_avl.h"
#include "ddsi/q_whc.h"
#include "ddsi/q_entity.h"
#include "ddsi/q_addrset.h"
#include "ddsi/q_xmsg.h"
#include "ddsi/q_bswap.h"
#include "ddsi/q_misc.h"
#include "ddsi/q_thread.h"
#include "ddsi/q_xevent.h"
#include "ddsi/q_time.h"
#include "ddsi/q_config.h"
#include "ddsi/q_globals.h"
#include "ddsi/q_error.h"
#include "ddsi/q_transmit.h"
#include "ddsi/q_entity.h"
#include "ddsi/q_unused.h"
#include "ddsi/q_hbcontrol.h"
#include "ddsi/q_static_assert.h"

#include "ddsi/ddsi_ser.h"

#include "ddsi/sysdeps.h"

#if __STDC_VERSION__ >= 199901L
#define POS_INFINITY_DOUBLE INFINITY
#elif defined HUGE_VAL
/* Hope for the best -- the only consequence of getting this wrong is
   that T_NEVER may be printed as a fugly value instead of as +inf. */
#define POS_INFINITY_DOUBLE (HUGE_VAL + HUGE_VAL)
#else
#define POS_INFINITY_DOUBLE 1e1000
#endif

static const struct wr_prd_match *root_rdmatch (const struct writer *wr)
{
  return ut_avlRoot (&wr_readers_treedef, &wr->readers);
}

static int have_reliable_subs (const struct writer *wr)
{
  if (ut_avlIsEmpty (&wr->readers) || root_rdmatch (wr)->min_seq == MAX_SEQ_NUMBER)
    return 0;
  else
    return 1;
}

void writer_hbcontrol_init (struct hbcontrol *hbc)
{
  hbc->t_of_last_write.v = 0;
  hbc->t_of_last_hb.v = 0;
  hbc->t_of_last_ackhb.v = 0;
  hbc->tsched.v = T_NEVER;
  hbc->hbs_since_last_write = 0;
  hbc->last_packetid = 0;
}

static void writer_hbcontrol_note_hb (struct writer *wr, nn_mtime_t tnow, int ansreq)
{
  struct hbcontrol * const hbc = &wr->hbcontrol;

  if (ansreq)
    hbc->t_of_last_ackhb = tnow;
  hbc->t_of_last_hb = tnow;

  /* Count number of heartbeats since last write, used to lower the
     heartbeat rate.  Overflow doesn't matter, it'll just revert to a
     highish rate for a short while. */
  hbc->hbs_since_last_write++;
}

int64_t writer_hbcontrol_intv (const struct writer *wr, UNUSED_ARG (nn_mtime_t tnow))
{
  struct hbcontrol const * const hbc = &wr->hbcontrol;
  int64_t ret = config.const_hb_intv_sched;
  size_t n_unacked;

  if (hbc->hbs_since_last_write > 2)
  {
    unsigned cnt = hbc->hbs_since_last_write;
    while (cnt-- > 2 && 2 * ret < config.const_hb_intv_sched_max)
      ret *= 2;
  }

  n_unacked = whc_unacked_bytes (wr->whc);
  if (n_unacked >= wr->whc_low + 3 * (wr->whc_high - wr->whc_low) / 4)
    ret /= 2;
  if (n_unacked >= wr->whc_low + (wr->whc_high - wr->whc_low) / 2)
    ret /= 2;
  if (wr->throttling)
    ret /= 2;
  if (ret < config.const_hb_intv_sched_min)
    ret = config.const_hb_intv_sched_min;
  return ret;
}

void writer_hbcontrol_note_asyncwrite (struct writer *wr, nn_mtime_t tnow)
{
  struct hbcontrol * const hbc = &wr->hbcontrol;
  nn_mtime_t tnext;

  /* Reset number of heartbeats since last write: that means the
     heartbeat rate will go back up to the default */
  hbc->hbs_since_last_write = 0;

  /* We know this is new data, so we want a heartbeat event after one
     base interval */
  tnext.v = tnow.v + config.const_hb_intv_sched;
  if (tnext.v < hbc->tsched.v)
  {
    /* Insertion of a message with WHC locked => must now have at
       least one unacked msg if there are reliable readers, so must
       have a heartbeat scheduled.  Do so now */
    hbc->tsched = tnext;
    resched_xevent_if_earlier (wr->heartbeat_xevent, tnext);
  }
}

int writer_hbcontrol_must_send (const struct writer *wr, nn_mtime_t tnow /* monotonic */)
{
  struct hbcontrol const * const hbc = &wr->hbcontrol;
  return (tnow.v >= hbc->t_of_last_hb.v + writer_hbcontrol_intv (wr, tnow));
}

struct nn_xmsg *writer_hbcontrol_create_heartbeat (struct writer *wr, nn_mtime_t tnow, int hbansreq, int issync)
{
  struct nn_xmsg *msg;
  const nn_guid_t *prd_guid;

  ASSERT_MUTEX_HELD (&wr->e.lock);
  assert (wr->reliable);
  assert (hbansreq >= 0);

  if ((msg = nn_xmsg_new (gv.xmsgpool, &wr->e.guid.prefix, sizeof (InfoTS_t) + sizeof (Heartbeat_t), NN_XMSG_KIND_CONTROL)) == NULL)
    /* out of memory at worst slows down traffic */
    return NULL;

  if (ut_avlIsEmpty (&wr->readers) || wr->num_reliable_readers == 0)
  {
    /* Not really supposed to come here, at least not for the first
       case. Secondly, there really seems to be little use for
       optimising reliable writers with only best-effort readers. And
       in any case, it is always legal to multicast a heartbeat from a
       reliable writer. */
    prd_guid = NULL;
  }
  else if (wr->seq != root_rdmatch (wr)->max_seq)
  {
    /* If the writer is ahead of its readers, multicast. Couldn't care
       less about the pessimal cases such as multicasting when there
       is one reliable reader & multiple best-effort readers. See
       comment above. */
    prd_guid = NULL;
  }
  else
  {
    const int n_unacked = wr->num_reliable_readers - root_rdmatch (wr)->num_reliable_readers_where_seq_equals_max;
    assert (n_unacked >= 0);
    if (n_unacked == 0)
      prd_guid = NULL;
    else
    {
      assert (root_rdmatch (wr)->arbitrary_unacked_reader.entityid.u != NN_ENTITYID_UNKNOWN);
      if (n_unacked > 1)
        prd_guid = NULL;
      else
        prd_guid = &(root_rdmatch (wr)->arbitrary_unacked_reader);
    }
  }

  TRACE (("writer_hbcontrol: wr %x:%x:%x:%x ", PGUID (wr->e.guid)));
  if (prd_guid == NULL)
    TRACE (("multicasting "));
  else
    TRACE (("unicasting to prd %x:%x:%x:%x ", PGUID (*prd_guid)));
  TRACE (("(rel-prd %d seq-eq-max %d seq %"PRId64" maxseq %"PRId64")\n",
          wr->num_reliable_readers,
          ut_avlIsEmpty (&wr->readers) ? -1 : root_rdmatch (wr)->num_reliable_readers_where_seq_equals_max,
          wr->seq,
          ut_avlIsEmpty (&wr->readers) ? (seqno_t) -1 : root_rdmatch (wr)->max_seq));

  if (prd_guid == NULL)
  {
    nn_xmsg_setdstN (msg, wr->as, wr->as_group);
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
    nn_xmsg_setencoderid (msg, wr->partition_id);
#endif
    add_Heartbeat (msg, wr, hbansreq, to_entityid (NN_ENTITYID_UNKNOWN), issync);
  }
  else
  {
    struct proxy_reader *prd;
    if ((prd = ephash_lookup_proxy_reader_guid (prd_guid)) == NULL)
    {
      TRACE (("writer_hbcontrol: wr %x:%x:%x:%x unknown prd %x:%x:%x:%x\n", PGUID (wr->e.guid), PGUID (*prd_guid)));
      nn_xmsg_free (msg);
      return NULL;
    }
    /* set the destination explicitly to the unicast destination and the fourth
       param of add_Heartbeat needs to be the guid of the reader */
    if (nn_xmsg_setdstPRD (msg, prd) < 0)
    {
      nn_xmsg_free (msg);
      return NULL;
    }
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
    nn_xmsg_setencoderid (msg, wr->partition_id);
#endif
    add_Heartbeat (msg, wr, hbansreq, prd_guid->entityid, issync);
  }

  writer_hbcontrol_note_hb (wr, tnow, hbansreq);
  return msg;
}

static int writer_hbcontrol_ack_required_generic (const struct writer *wr, nn_mtime_t tlast, nn_mtime_t tnow, int piggyback)
{
  struct hbcontrol const * const hbc = &wr->hbcontrol;
  const int64_t hb_intv_ack = config.const_hb_intv_sched;

  if (piggyback)
  {
    /* If it is likely that a heartbeat requiring an ack will go out
       shortly after the sample was written, it is better to piggyback
       it onto the sample.  The current idea is that a write shortly
       before the next heartbeat will go out should have one
       piggybacked onto it, so that the scheduled heartbeat can be
       suppressed. */
    if (tnow.v >= tlast.v + 4 * hb_intv_ack / 5)
      return 2;
  }
  else
  {
    /* For heartbeat events use a slightly longer interval */
    if (tnow.v >= tlast.v + hb_intv_ack)
      return 2;
  }

  if (whc_unacked_bytes (wr->whc) >= wr->whc_low + (wr->whc_high - wr->whc_low) / 2)
  {
    if (tnow.v >= hbc->t_of_last_ackhb.v + config.const_hb_intv_sched_min)
      return 2;
    else if (tnow.v >= hbc->t_of_last_ackhb.v + config.const_hb_intv_min)
      return 1;
  }

  return 0;
}

int writer_hbcontrol_ack_required (const struct writer *wr, nn_mtime_t tnow)
{
  struct hbcontrol const * const hbc = &wr->hbcontrol;
  return writer_hbcontrol_ack_required_generic (wr, hbc->t_of_last_write, tnow, 0);
}

struct nn_xmsg *writer_hbcontrol_piggyback (struct writer *wr, nn_mtime_t tnow, unsigned packetid, int *hbansreq)
{
  struct hbcontrol * const hbc = &wr->hbcontrol;
  unsigned last_packetid;
  nn_mtime_t tlast;
  struct nn_xmsg *msg;

  tlast = hbc->t_of_last_write;
  last_packetid = hbc->last_packetid;

  hbc->t_of_last_write = tnow;
  hbc->last_packetid = packetid;

  /* Update statistics, intervals, scheduling of heartbeat event,
     &c. -- there's no real difference between async and sync so we
     reuse the async version. */
  writer_hbcontrol_note_asyncwrite (wr, tnow);

  *hbansreq = writer_hbcontrol_ack_required_generic (wr, tlast, tnow, 1);
  if (*hbansreq >= 2) {
    /* So we force a heartbeat in - but we also rely on our caller to
       send the packet out */
    msg = writer_hbcontrol_create_heartbeat (wr, tnow, *hbansreq, 1);
  } else if (last_packetid != packetid) {
    /* If we crossed a packet boundary since the previous write,
       piggyback a heartbeat, with *hbansreq determining whether or
       not an ACK is needed.  We don't force the packet out either:
       this is just to ensure a regular flow of ACKs for cleaning up
       the WHC & for allowing readers to NACK missing samples. */
    msg = writer_hbcontrol_create_heartbeat (wr, tnow, *hbansreq, 1);
  } else {
    *hbansreq = 0;
    msg = NULL;
  }

  if (msg)
  {
    TRACE (("heartbeat(wr %x:%x:%x:%x%s) piggybacked, resched in %g s (min-ack %"PRId64"%s, avail-seq %"PRId64", xmit %"PRId64")\n",
            PGUID (wr->e.guid),
            *hbansreq ? "" : " final",
            (hbc->tsched.v == T_NEVER) ? POS_INFINITY_DOUBLE : (double) (hbc->tsched.v - tnow.v) / 1e9,
            ut_avlIsEmpty (&wr->readers) ? -1 : root_rdmatch (wr)->min_seq,
            ut_avlIsEmpty (&wr->readers) || root_rdmatch (wr)->all_have_replied_to_hb ? "" : "!",
            whc_empty (wr->whc) ? -1 : whc_max_seq (wr->whc), READ_SEQ_XMIT(wr)));
  }

  return msg;
}

void add_Heartbeat (struct nn_xmsg *msg, struct writer *wr, int hbansreq, nn_entityid_t dst, int issync)
{
  struct nn_xmsg_marker sm_marker;
  Heartbeat_t * hb;
  seqno_t max = 0, min = 1;

  ASSERT_MUTEX_HELD (&wr->e.lock);

  assert (wr->reliable);
  assert (hbansreq >= 0);

  if (config.meas_hb_to_ack_latency)
  {
    /* If configured to measure heartbeat-to-ack latency, we must add
       a timestamp.  No big deal if it fails. */
    nn_xmsg_add_timestamp (msg, now ());
  }

  hb = nn_xmsg_append (msg, &sm_marker, sizeof (Heartbeat_t));
  nn_xmsg_submsg_init (msg, sm_marker, SMID_HEARTBEAT);

  if (!hbansreq)
    hb->smhdr.flags |= HEARTBEAT_FLAG_FINAL;

  hb->readerId = nn_hton_entityid (dst);
  hb->writerId = nn_hton_entityid (wr->e.guid.entityid);
  if (whc_empty (wr->whc))
  {
    /* Really don't have data.  Fake one at the current wr->seq.
       We're not really allowed to generate heartbeats when the WHC is
       empty, but it appears RTI sort-of needs them ...  Now we use
       GAPs, and allocate a sequence number specially for that. */
    assert (config.respond_to_rti_init_zero_ack_with_invalid_heartbeat || wr->seq >= 1);
    max = wr->seq;
    min = max;
    if (config.respond_to_rti_init_zero_ack_with_invalid_heartbeat)
    {
      min += 1;
    }
  }
  else
  {
    seqno_t seq_xmit;
    min = whc_min_seq (wr->whc);
    max = wr->seq;
    seq_xmit = READ_SEQ_XMIT(wr);
    assert (min <= max);
    /* Informing readers of samples that haven't even been transmitted makes little sense,
       but for transient-local data, we let the first heartbeat determine the time at which
       we trigger wait_for_historical_data, so it had better be correct */
    if (!issync && seq_xmit < max && !wr->handle_as_transient_local)
    {
      /* When: queue data ; queue heartbeat ; transmit data ; update
         seq_xmit, max may be < min.  But we must never advertise the
         minimum available sequence number incorrectly! */
      if (seq_xmit >= min) {
        /* Advertise some but not all data */
        max = seq_xmit;
      } else if (config.respond_to_rti_init_zero_ack_with_invalid_heartbeat) {
        /* if we can generate an empty heartbeat => do so. */
        max = min - 1;
      } else {
        /* claim the existence of a sample we possibly haven't set
           yet, at worst this causes a retransmission (but the
           NackDelay usually takes care of that). */
        max = min;
      }
    }
  }
  hb->firstSN = toSN (min);
  hb->lastSN = toSN (max);

  hb->count = ++wr->hbcount;

  nn_xmsg_submsg_setnext (msg, sm_marker);
}

static int create_fragment_message_simple (struct writer *wr, seqno_t seq, struct serdata *serdata, struct nn_xmsg **pmsg)
{
  const size_t expected_inline_qos_size = 4+8+20+4 + 32;
  struct nn_xmsg_marker sm_marker;
  const unsigned char contentflag = (ddsi_serdata_is_empty (serdata) ? 0 : ddsi_serdata_is_key (serdata) ? DATA_FLAG_KEYFLAG : DATA_FLAG_DATAFLAG);
  Data_t *data;

  ASSERT_MUTEX_HELD (&wr->e.lock);

  if ((*pmsg = nn_xmsg_new (gv.xmsgpool, &wr->e.guid.prefix, sizeof (InfoTimestamp_t) + sizeof (Data_t) + expected_inline_qos_size, NN_XMSG_KIND_DATA)) == NULL)
    return ERR_OUT_OF_MEMORY;

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  /* use the partition_id from the writer to select the proper encoder */
  nn_xmsg_setencoderid (*pmsg, wr->partition_id);
#endif

  nn_xmsg_setdstN (*pmsg, wr->as, wr->as_group);
  nn_xmsg_setmaxdelay (*pmsg, nn_from_ddsi_duration (wr->xqos->latency_budget.duration));
  nn_xmsg_add_timestamp (*pmsg, serdata->v.msginfo.timestamp);
  data = nn_xmsg_append (*pmsg, &sm_marker, sizeof (Data_t));

  nn_xmsg_submsg_init (*pmsg, sm_marker, SMID_DATA);
  data->x.smhdr.flags = (unsigned char) (data->x.smhdr.flags | contentflag);
  data->x.extraFlags = 0;
  data->x.readerId = to_entityid (NN_ENTITYID_UNKNOWN);
  data->x.writerId = nn_hton_entityid (wr->e.guid.entityid);
  data->x.writerSN = toSN (seq);
  data->x.octetsToInlineQos = (unsigned short) ((char*) (data+1) - ((char*) &data->x.octetsToInlineQos + 2));

  if (wr->reliable)
    nn_xmsg_setwriterseq (*pmsg, &wr->e.guid, seq);

  /* Adding parameters means potential reallocing, so sm, ddcmn now likely become invalid */
  if (wr->include_keyhash)
    nn_xmsg_addpar_keyhash (*pmsg, serdata);
  if (serdata->v.msginfo.statusinfo)
    nn_xmsg_addpar_statusinfo (*pmsg, serdata->v.msginfo.statusinfo);
  if (nn_xmsg_addpar_sentinel_ifparam (*pmsg) > 0)
  {
    data = nn_xmsg_submsg_from_marker (*pmsg, sm_marker);
    data->x.smhdr.flags |= DATAFRAG_FLAG_INLINE_QOS;
  }

  nn_xmsg_serdata (*pmsg, serdata, 0, ddsi_serdata_size (serdata));
  nn_xmsg_submsg_setnext (*pmsg, sm_marker);
  return 0;
}

int create_fragment_message (struct writer *wr, seqno_t seq, const struct nn_plist *plist, struct serdata *serdata, unsigned fragnum, struct proxy_reader *prd, struct nn_xmsg **pmsg, int isnew)
{
  /* We always fragment into FRAGMENT_SIZEd fragments, which are near
     the smallest allowed fragment size & can't be bothered (yet) to
     put multiple fragments into one DataFrag submessage if it makes
     sense to send large messages, as it would e.g. on GigE with jumbo
     frames.  If the sample is small enough to fit into one Data
     submessage, we require fragnum = 0 & generate a Data instead of a
     DataFrag.

     Note: fragnum is 0-based here, 1-based in DDSI. But 0-based is
     much easier ...

     Expected inline QoS size: header(4) + statusinfo(8) + keyhash(20)
     + sentinel(4). Plus some spare cos I can't be bothered. */
  const int set_smhdr_flags_asif_data = config.buggy_datafrag_flags_mode;
  const size_t expected_inline_qos_size = 4+8+20+4 + 32;
  struct nn_xmsg_marker sm_marker;
  void *sm;
  Data_DataFrag_common_t *ddcmn;
  int fragging;
  unsigned fragstart, fraglen;
  enum nn_xmsg_kind xmsg_kind = isnew ? NN_XMSG_KIND_DATA : NN_XMSG_KIND_DATA_REXMIT;
  int ret = 0;

  ASSERT_MUTEX_HELD (&wr->e.lock);

  if (fragnum * config.fragment_size >= ddsi_serdata_size (serdata) && ddsi_serdata_size (serdata) > 0)
  {
    /* This is the first chance to detect an attempt at retransmitting
       an non-existent fragment, which a malicious (or buggy) remote
       reader can trigger.  So we return an error instead of asserting
       as we used to. */
    return ERR_INVALID;
  }

  fragging = (config.fragment_size < ddsi_serdata_size (serdata));

  if ((*pmsg = nn_xmsg_new (gv.xmsgpool, &wr->e.guid.prefix, sizeof (InfoTimestamp_t) + sizeof (DataFrag_t) + expected_inline_qos_size, xmsg_kind)) == NULL)
    return ERR_OUT_OF_MEMORY;

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  /* use the partition_id from the writer to select the proper encoder */
  nn_xmsg_setencoderid (*pmsg, wr->partition_id);
#endif

  if (prd)
  {
    if (nn_xmsg_setdstPRD (*pmsg, prd) < 0)
    {
      nn_xmsg_free (*pmsg);
      *pmsg = NULL;
      return ERR_NO_ADDRESS;
    }
    /* retransmits: latency budget doesn't apply */
  }
  else
  {
    nn_xmsg_setdstN (*pmsg, wr->as, wr->as_group);
    nn_xmsg_setmaxdelay (*pmsg, nn_from_ddsi_duration (wr->xqos->latency_budget.duration));
  }

  /* Timestamp only needed once, for the first fragment */
  if (fragnum == 0)
  {
    nn_xmsg_add_timestamp (*pmsg, serdata->v.msginfo.timestamp);
  }

  sm = nn_xmsg_append (*pmsg, &sm_marker, fragging ? sizeof (DataFrag_t) : sizeof (Data_t));
  ddcmn = sm;

  if (!fragging)
  {
    const unsigned char contentflag = (ddsi_serdata_is_empty (serdata) ? 0 : ddsi_serdata_is_key (serdata) ? DATA_FLAG_KEYFLAG : DATA_FLAG_DATAFLAG);
    Data_t *data = sm;
    nn_xmsg_submsg_init (*pmsg, sm_marker, SMID_DATA);
    ddcmn->smhdr.flags = (unsigned char) (ddcmn->smhdr.flags | contentflag);

    fragstart = 0;
    fraglen = ddsi_serdata_size (serdata);
    ddcmn->octetsToInlineQos = (unsigned short) ((char*) (data+1) - ((char*) &ddcmn->octetsToInlineQos + 2));

    if (wr->reliable)
      nn_xmsg_setwriterseq (*pmsg, &wr->e.guid, seq);
  }
  else
  {
    const unsigned char contentflag =
      set_smhdr_flags_asif_data
      ? (ddsi_serdata_is_key (serdata) ? DATA_FLAG_KEYFLAG : DATA_FLAG_DATAFLAG)
      : (ddsi_serdata_is_key (serdata) ? DATAFRAG_FLAG_KEYFLAG : 0);
    DataFrag_t *frag = sm;
    /* empty means size = 0, which means it never needs fragmenting */
    assert (!ddsi_serdata_is_empty (serdata));
    nn_xmsg_submsg_init (*pmsg, sm_marker, SMID_DATA_FRAG);
    ddcmn->smhdr.flags = (unsigned char) (ddcmn->smhdr.flags | contentflag);

    frag->fragmentStartingNum = fragnum + 1;
    frag->fragmentsInSubmessage = 1;
    frag->fragmentSize = (unsigned short) config.fragment_size;
    frag->sampleSize = ddsi_serdata_size (serdata);

    fragstart = fragnum * config.fragment_size;
#if MULTIPLE_FRAGS_IN_SUBMSG /* ugly hack for testing only */
    if (fragstart + config.fragment_size < ddsi_serdata_size (serdata) &&
        fragstart + 2 * config.fragment_size >= ddsi_serdata_size (serdata))
      frag->fragmentsInSubmessage++;
    ret = frag->fragmentsInSubmessage;
#endif

    fraglen = config.fragment_size * frag->fragmentsInSubmessage;
    if (fragstart + fraglen > ddsi_serdata_size (serdata))
      fraglen = ddsi_serdata_size (serdata) - fragstart;
    ddcmn->octetsToInlineQos = (unsigned short) ((char*) (frag+1) - ((char*) &ddcmn->octetsToInlineQos + 2));

    if (wr->reliable && (!isnew || fragstart + fraglen == ddsi_serdata_size (serdata)))
    {
      /* only set for final fragment for new messages; for rexmits we
         want it set for all so we can do merging. FIXME: I guess the
         writer should track both seq_xmit and the fragment number
         ... */
      nn_xmsg_setwriterseq_fragid (*pmsg, &wr->e.guid, seq, fragnum + frag->fragmentsInSubmessage - 1);
    }
  }

  ddcmn->extraFlags = 0;
  ddcmn->readerId = nn_hton_entityid (prd ? prd->e.guid.entityid : to_entityid (NN_ENTITYID_UNKNOWN));
  ddcmn->writerId = nn_hton_entityid (wr->e.guid.entityid);
  ddcmn->writerSN = toSN (seq);

  if (xmsg_kind == NN_XMSG_KIND_DATA_REXMIT)
    nn_xmsg_set_data_readerId (*pmsg, &ddcmn->readerId);

  Q_STATIC_ASSERT_CODE (DATA_FLAG_INLINE_QOS == DATAFRAG_FLAG_INLINE_QOS);
  assert (!(ddcmn->smhdr.flags & DATAFRAG_FLAG_INLINE_QOS));

  if (fragnum == 0)
  {
    int rc;
    /* Adding parameters means potential reallocing, so sm, ddcmn now likely become invalid */
    if (wr->include_keyhash)
    {
      nn_xmsg_addpar_keyhash (*pmsg, serdata);
    }
    if (serdata->v.msginfo.statusinfo)
    {
      nn_xmsg_addpar_statusinfo (*pmsg, serdata->v.msginfo.statusinfo);
    }
    rc = nn_xmsg_addpar_sentinel_ifparam (*pmsg);
    if (rc > 0)
    {
      ddcmn = nn_xmsg_submsg_from_marker (*pmsg, sm_marker);
      ddcmn->smhdr.flags |= DATAFRAG_FLAG_INLINE_QOS;
    }
  }

  nn_xmsg_serdata (*pmsg, serdata, fragstart, fraglen);
  nn_xmsg_submsg_setnext (*pmsg, sm_marker);
#if 0
  TRACE (("queue data%s %x:%x:%x:%x #%lld/%u[%u..%u)\n",
          fragging ? "frag" : "", PGUID (wr->e.guid),
          seq, fragnum+1, fragstart, fragstart + fraglen));
#endif

  return ret;
}

static void create_HeartbeatFrag (struct writer *wr, seqno_t seq, unsigned fragnum, struct proxy_reader *prd, struct nn_xmsg **pmsg)
{
  struct nn_xmsg_marker sm_marker;
  HeartbeatFrag_t *hbf;
  ASSERT_MUTEX_HELD (&wr->e.lock);
  if ((*pmsg = nn_xmsg_new (gv.xmsgpool, &wr->e.guid.prefix, sizeof (HeartbeatFrag_t), NN_XMSG_KIND_CONTROL)) == NULL)
    return; /* ignore out-of-memory: HeartbeatFrag is only advisory anyway */
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  nn_xmsg_setencoderid (*pmsg, wr->partition_id);
#endif
  if (prd)
  {
    if (nn_xmsg_setdstPRD (*pmsg, prd) < 0)
    {
      /* HeartbeatFrag is only advisory anyway */
      nn_xmsg_free (*pmsg);
      *pmsg = NULL;
      return;
    }
  }
  else
  {
    nn_xmsg_setdstN (*pmsg, wr->as, wr->as_group);
  }
  hbf = nn_xmsg_append (*pmsg, &sm_marker, sizeof (HeartbeatFrag_t));
  nn_xmsg_submsg_init (*pmsg, sm_marker, SMID_HEARTBEAT_FRAG);
  hbf->readerId = nn_hton_entityid (prd ? prd->e.guid.entityid : to_entityid (NN_ENTITYID_UNKNOWN));
  hbf->writerId = nn_hton_entityid (wr->e.guid.entityid);
  hbf->writerSN = toSN (seq);
  hbf->lastFragmentNum = fragnum + 1; /* network format is 1 based */

  hbf->count = ++wr->hbfragcount;

  nn_xmsg_submsg_setnext (*pmsg, sm_marker);
}

#if 0
static int must_skip_frag (const char *frags_to_skip, unsigned frag)
{
  /* one based, for easier reading of logs */
  char str[14];
  int n, m;
  if (frags_to_skip == NULL)
    return 0;
  n = snprintf (str, sizeof (str), ",%u,", frag + 1);
  if (strstr (frags_to_skip, str))
    return 1; /* somewhere in middle */
  if (strncmp (frags_to_skip, str+1, (size_t)n-1) == 0)
    return 1; /* first in list */
  str[--n] = 0; /* drop trailing comma */
  if (strcmp (frags_to_skip, str+1) == 0)
    return 1; /* only one */
  m = (int)strlen (frags_to_skip);
  if (m >= n && strcmp (frags_to_skip + m - n, str) == 0)
    return 1; /* last one in list */
  return 0;
}
#endif

static void transmit_sample_lgmsg_unlocked (struct nn_xpack *xp, struct writer *wr, seqno_t seq, const struct nn_plist *plist, serdata_t serdata, struct proxy_reader *prd, int isnew, unsigned nfrags)
{
  unsigned i;
#if 0
  const char *frags_to_skip = getenv ("SKIPFRAGS");
#endif
  assert(xp);

  for (i = 0; i < nfrags; i++)
  {
    struct nn_xmsg *fmsg = NULL;
    struct nn_xmsg *hmsg = NULL;
    int ret;
#if 0
    if (must_skip_frag (frags_to_skip, i))
      continue;
#endif
    /* Ignore out-of-memory errors: we can't do anything about it, and
       eventually we'll have to retry.  But if a packet went out and
       we haven't yet completed transmitting a fragmented message, add
       a HeartbeatFrag. */
    os_mutexLock (&wr->e.lock);
    ret = create_fragment_message (wr, seq, plist, serdata, i, prd, &fmsg, isnew);
    if (ret >= 0)
    {
      if (nfrags > 1 && i + 1 < nfrags)
        create_HeartbeatFrag (wr, seq, i, prd, &hmsg);
    }
    os_mutexUnlock (&wr->e.lock);

    if(fmsg) nn_xpack_addmsg (xp, fmsg, 0);
    if(hmsg) nn_xpack_addmsg (xp, hmsg, 0);

#if MULTIPLE_FRAGS_IN_SUBMSG /* ugly hack for testing only */
    if (ret > 1)
      i += ret-1;
#endif
  }

  /* Note: wr->heartbeat_xevent != NULL <=> wr is reliable */
  if (wr->heartbeat_xevent)
  {
    struct nn_xmsg *msg = NULL;
    int hbansreq;
    os_mutexLock (&wr->e.lock);
    msg = writer_hbcontrol_piggyback
      (wr, ddsi_serdata_twrite (serdata), nn_xpack_packetid (xp), &hbansreq);
    os_mutexUnlock (&wr->e.lock);
    if (msg)
    {
      nn_xpack_addmsg (xp, msg, 0);
      if (hbansreq >= 2)
        nn_xpack_send (xp, true);
    }
  }
}

static void transmit_sample_unlocks_wr (struct nn_xpack *xp, struct writer *wr, seqno_t seq, const struct nn_plist *plist, serdata_t serdata, struct proxy_reader *prd, int isnew)
{
  /* on entry: &wr->e.lock held; on exit: lock no longer held */
  struct nn_xmsg *fmsg;
  unsigned sz;
  assert(xp);

  sz = ddsi_serdata_size (serdata);
  if (sz > config.fragment_size || !isnew || plist != NULL || prd != NULL)
  {
    unsigned nfrags;
    os_mutexUnlock (&wr->e.lock);
    nfrags = (sz + config.fragment_size - 1) / config.fragment_size;
    transmit_sample_lgmsg_unlocked (xp, wr, seq, plist, serdata, prd, isnew, nfrags);
    return;
  }
  else if (create_fragment_message_simple (wr, seq, serdata, &fmsg) < 0)
  {
    os_mutexUnlock (&wr->e.lock);
    return;
  }
  else
  {
    int hbansreq = 0;
    struct nn_xmsg *hmsg;

    /* Note: wr->heartbeat_xevent != NULL <=> wr is reliable */
    if (wr->heartbeat_xevent)
      hmsg = writer_hbcontrol_piggyback (wr, ddsi_serdata_twrite (serdata), nn_xpack_packetid (xp), &hbansreq);
    else
      hmsg = NULL;

    os_mutexUnlock (&wr->e.lock);
    nn_xpack_addmsg (xp, fmsg, 0);
    if(hmsg)
      nn_xpack_addmsg (xp, hmsg, 0);
    if (hbansreq >= 2)
      nn_xpack_send (xp, true);
  }
}

int enqueue_sample_wrlock_held (struct writer *wr, seqno_t seq, const struct nn_plist *plist, serdata_t serdata, struct proxy_reader *prd, int isnew)
{
  unsigned i, sz, nfrags;
  int enqueued = 1;

  ASSERT_MUTEX_HELD (&wr->e.lock);

  sz = ddsi_serdata_size (serdata);
  nfrags = (sz + config.fragment_size - 1) / config.fragment_size;
  if (nfrags == 0)
  {
    /* end-of-transaction messages are empty, but still need to be sent */
    nfrags = 1;
  }
  for (i = 0; i < nfrags && enqueued; i++)
  {
    struct nn_xmsg *fmsg = NULL;
    struct nn_xmsg *hmsg = NULL;
    /* Ignore out-of-memory errors: we can't do anything about it, and
       eventually we'll have to retry.  But if a packet went out and
       we haven't yet completed transmitting a fragmented message, add
       a HeartbeatFrag. */
    if (create_fragment_message (wr, seq, plist, serdata, i, prd, &fmsg, isnew) >= 0)
    {
      if (nfrags > 1 && i + 1 < nfrags)
        create_HeartbeatFrag (wr, seq, i, prd, &hmsg);
    }
    if (isnew)
    {
      if(fmsg) qxev_msg (wr->evq, fmsg);
      if(hmsg) qxev_msg (wr->evq, hmsg);
    }
    else
    {
      /* Implementations that never use NACKFRAG are allowed by the specification, and for such a peer, we must always force out the full sample on a retransmit request. I am not aware of any such implementations so leaving the override flag in, but not actually using it at the moment. Should set force = (i != 0) for "known bad" implementations. */
      const int force = 0;
      if(fmsg)
      {
        enqueued = qxev_msg_rexmit_wrlock_held (wr->evq, fmsg, force);
      }
      /* Functioning of the system is not dependent on getting the
         HeartbeatFrags out, so never force them into the queue. */
      if(hmsg)
      {
        if (enqueued > 1)
          qxev_msg (wr->evq, hmsg);
        else
          nn_xmsg_free (hmsg);
      }
    }
  }
  return enqueued ? 0 : -1;
}

static int insert_sample_in_whc (struct writer *wr, seqno_t seq, struct nn_plist *plist, serdata_t serdata, struct tkmap_instance *tk)
{
  /* returns: < 0 on error, 0 if no need to insert in whc, > 0 if inserted */
  int do_insert, insres, res;

  ASSERT_MUTEX_HELD (&wr->e.lock);

  if (config.enabled_logcats & LC_TRACE)
  {
    char ppbuf[1024];
    int tmp;
    const char *tname = wr->topic ? wr->topic->name : "(null)";
    const char *ttname = wr->topic ? wr->topic->typename : "(null)";
    ppbuf[0] = '\0';
    tmp = sizeof (ppbuf) - 1;
    nn_log (LC_TRACE, "write_sample %x:%x:%x:%x #%"PRId64"", PGUID (wr->e.guid), seq);
    if (plist != 0 && (plist->present & PP_COHERENT_SET))
      nn_log (LC_TRACE, " C#%"PRId64"", fromSN (plist->coherent_set_seqno));
    nn_log (LC_TRACE, ": ST%u %s/%s:%s%s\n",
            serdata->v.msginfo.statusinfo, tname, ttname,
            ppbuf, tmp < (int) sizeof (ppbuf) ? "" : " (trunc)");
  }

  assert (wr->reliable || have_reliable_subs (wr) == 0);

  if (wr->reliable && have_reliable_subs (wr))
    do_insert = 1;
  else if (wr->handle_as_transient_local || wr->startup_mode)
    do_insert = 1;
  else
    do_insert = 0;

  if (!do_insert)
    res = 0;
  else if ((insres = whc_insert (wr->whc, writer_max_drop_seq (wr), seq, plist, serdata, tk)) < 0)
    res = insres;
  else
    res = 1;

#ifndef NDEBUG
  if (wr->e.guid.entityid.u == NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER)
  {
    if (whc_findmax (wr->whc) == NULL)
      assert (wr->c.pp->builtins_deleted);
  }
#endif
  return res;
}

static int writer_may_continue (const struct writer *wr)
{
  return (whc_unacked_bytes (wr->whc) <= wr->whc_low && !wr->retransmitting) || (wr->state != WRST_OPERATIONAL);
}


static os_result throttle_writer (struct nn_xpack *xp, struct writer *wr)
{
  /* Sleep (cond_wait) without updating the thread's vtime: the
     garbage collector won't free the writer while we leave it
     unchanged.  Alternatively, we could decide to go back to sleep,
     allow garbage collection and check the writers existence every
     time we get woken up.  That would preclude the use of a condition
     variable embedded in "struct writer", of course.

     For normal data that would be okay, because the thread forwarding
     data from the network queue to rtps_write() simply uses the gid
     and doesn't mind if the writer is freed halfway through (although
     we would have to specify it may do so it!); but for internal
     data, it would be absolutely unacceptable if they were ever to
     take the path that would increase vtime.

     Currently, rtps_write/throttle_writer are used only by the normal
     data forwarding path, the internal ones use write_sample().  Not
     worth the bother right now.

     Therefore, we don't check the writer is still there after waking
     up.

     Used to block on a combination of |xeventq| and |whc|, but that
     is hard now that we use a per-writer condition variable.  So
     instead, wait until |whc| is small enough, then wait for
     |xeventq|.  The reasoning is that the WHC won't grow
     spontaneously the way the xevent queue does.

     If the |whc| is dropping with in a configurable timeframe
     (default 1 second) all connected readers that still haven't acked
     all data, are considered "non-responsive" and data is no longer
     resent to them, until a ACKNACK is received from that
     reader. This implicitly clears the whc and unblocks the
     writer. */

  os_result result = os_resultSuccess;
  nn_mtime_t tnow = now_mt ();
  const nn_mtime_t abstimeout = add_duration_to_mtime (tnow, nn_from_ddsi_duration (wr->xqos->reliability.max_blocking_time));
  size_t n_unacked = whc_unacked_bytes (wr->whc);

  {
    nn_vendorid_t ownvendorid = MY_VENDOR_ID;
    ASSERT_MUTEX_HELD (&wr->e.lock);
    assert (wr->throttling == 0);
    assert (vtime_awake_p (lookup_thread_state ()->vtime));
    assert (!is_builtin_entityid(wr->e.guid.entityid, ownvendorid));
  }

  nn_log (LC_THROTTLE, "writer %x:%x:%x:%x waiting for whc to shrink below low-water mark (whc %"PRIuSIZE" low=%u high=%u)\n", PGUID (wr->e.guid), n_unacked, wr->whc_low, wr->whc_high);
  wr->throttling = 1;
  wr->throttle_count++;

  /* Force any outstanding packet out: there will be a heartbeat
     requesting an answer in it.  FIXME: obviously, this is doing
     things the wrong way round ... */
  if (xp)
  {
    struct nn_xmsg *hbmsg = writer_hbcontrol_create_heartbeat (wr, tnow, 1, 1);
    os_mutexUnlock (&wr->e.lock);
    if (hbmsg)
    {
      nn_xpack_addmsg (xp, hbmsg, 0);
    }
    nn_xpack_send (xp, true);
    os_mutexLock (&wr->e.lock);
  }

  while (gv.rtps_keepgoing && !writer_may_continue (wr))
  {
    int64_t reltimeout;
    tnow = now_mt ();
    reltimeout = abstimeout.v - tnow.v;
    result = os_resultTimeout;
    if (reltimeout > 0)
    {
      os_time timeout;
      timeout.tv_sec = (int32_t) (reltimeout / T_SECOND);
      timeout.tv_nsec = (int32_t) (reltimeout % T_SECOND);
      thread_state_asleep (lookup_thread_state());
      result = os_condTimedWait (&wr->throttle_cond, &wr->e.lock, &timeout);
      thread_state_awake (lookup_thread_state());
    }
    if (result == os_resultTimeout)
    {
      break;
    }
  }

  wr->throttling = 0;
  if (wr->state != WRST_OPERATIONAL)
  {
    /* gc_delete_writer may be waiting */
    os_condBroadcast (&wr->throttle_cond);
  }

  n_unacked = whc_unacked_bytes (wr->whc);
  nn_log (LC_THROTTLE, "writer %x:%x:%x:%x done waiting for whc to shrink below low-water mark (whc %"PRIuSIZE" low=%u high=%u)\n", PGUID (wr->e.guid), n_unacked, wr->whc_low, wr->whc_high);
  return result;
}

static int maybe_grow_whc (struct writer *wr)
{
  if (!wr->retransmitting && config.whc_adaptive && wr->whc_high < config.whc_highwater_mark)
  {
    nn_etime_t tnow = now_et();
    nn_etime_t tgrow = add_duration_to_etime (wr->t_whc_high_upd, 10 * T_MILLISECOND);
    if (tnow.v >= tgrow.v)
    {
      uint32_t m = (config.whc_highwater_mark - wr->whc_high) / 32;
      wr->whc_high = (m == 0) ? config.whc_highwater_mark : wr->whc_high + m;
      wr->t_whc_high_upd = tnow;
      return 1;
    }
  }
  return 0;
}

static int write_sample_eot (struct nn_xpack *xp, struct writer *wr, struct nn_plist *plist, serdata_t serdata, struct tkmap_instance *tk, int end_of_txn, int gc_allowed)
{
  int r;
  seqno_t seq;
  nn_mtime_t tnow;

  /* If GC not allowed, we must be sure to never block when writing.  That is only the case for (true, aggressive) KEEP_LAST writers, and also only if there is no limit to how much unacknowledged data the WHC may contain. */
  assert(gc_allowed || (wr->xqos->history.kind == NN_KEEP_LAST_HISTORY_QOS && wr->aggressive_keep_last && wr->whc_low == INT32_MAX));

  if (ddsi_serdata_size (serdata) > config.max_sample_size)
  {
    char ppbuf[1024];
    int tmp;
    const char *tname = wr->topic ? wr->topic->name : "(null)";
    const char *ttname = wr->topic ? wr->topic->typename : "(null)";
    ppbuf[0] = '\0';
    tmp = sizeof (ppbuf) - 1;
    NN_WARNING ("dropping oversize (%u > %u) sample from local writer %x:%x:%x:%x %s/%s:%s%s\n",
                 ddsi_serdata_size (serdata), config.max_sample_size,
                 PGUID (wr->e.guid), tname, ttname, ppbuf,
                 tmp < (int) sizeof (ppbuf) ? "" : " (trunc)");
    r = ERR_INVALID_DATA;
    goto drop;
  }

  os_mutexLock (&wr->e.lock);

  if (end_of_txn)
  {
    wr->cs_seq = 0;
  }

  /* If WHC overfull, block. */
  {
    size_t unacked_bytes = whc_unacked_bytes (wr->whc);
    if (unacked_bytes > wr->whc_high)
    {
      os_result ores;
      assert(gc_allowed); /* also see beginning of the function */
      if (config.prioritize_retransmit && wr->retransmitting)
        ores = throttle_writer (xp, wr);
      else
      {
        maybe_grow_whc (wr);
        if (unacked_bytes <= wr->whc_high)
          ores = os_resultSuccess;
        else
          ores = throttle_writer (xp, wr);
      }
      if (ores == os_resultTimeout)
      {
        os_mutexUnlock (&wr->e.lock);
        r = ERR_TIMEOUT;
        goto drop;
      }
    }
  }

  /* Always use the current monotonic time */
  tnow = now_mt ();
  ddsi_serdata_set_twrite (serdata, tnow);

  seq = ++wr->seq;
  if (wr->cs_seq != 0)
  {
    if (plist == NULL)
    {
      plist = os_malloc (sizeof (*plist));
      nn_plist_init_empty (plist);
    }
    assert (!(plist->present & PP_COHERENT_SET));
    plist->present |= PP_COHERENT_SET;
    plist->coherent_set_seqno = toSN (wr->cs_seq);
  }

  if ((r = insert_sample_in_whc (wr, seq, plist, serdata, tk)) < 0)
  {
    /* Failure of some kind */
    os_mutexUnlock (&wr->e.lock);
    if (plist != NULL)
    {
      nn_plist_fini (plist);
      os_free (plist);
    }
  }
  else
  {
    /* Note the subtlety of enqueueing with the lock held but
       transmitting without holding the lock. Still working on
       cleaning that up. */
    if (xp)
    {
      /* If all reliable readers disappear between unlocking the writer and
       * creating the message, the WHC will free the plist (if any). Currently,
       * plist's are only used for coherent sets, which is assumed to be rare,
       * which in turn means that an extra copy doesn't hurt too badly ... */
      nn_plist_t plist_stk, *plist_copy;
      if (plist == NULL)
        plist_copy = NULL;
      else
      {
        plist_copy = &plist_stk;
        nn_plist_copy (plist_copy, plist);
      }
      transmit_sample_unlocks_wr (xp, wr, seq, plist_copy, serdata, NULL, 1);
      if (plist_copy)
        nn_plist_fini (plist_copy);
    }
    else
    {
      if (wr->heartbeat_xevent)
        writer_hbcontrol_note_asyncwrite (wr, tnow);
      enqueue_sample_wrlock_held (wr, seq, plist, serdata, NULL, 1);
      os_mutexUnlock (&wr->e.lock);
    }

    /* If not actually inserted, WHC didn't take ownership of plist */
    if (r == 0 && plist != NULL)
    {
      nn_plist_fini (plist);
      os_free (plist);
    }
  }

drop:
  /* FIXME: shouldn't I move the ddsi_serdata_unref call to the callers? */
  ddsi_serdata_unref (serdata);
  return r;
}

int write_sample_gc (struct nn_xpack *xp, struct writer *wr, serdata_t serdata, struct tkmap_instance *tk)
{
  return write_sample_eot (xp, wr, NULL, serdata, tk, 0, 1);
}

int write_sample_nogc (struct nn_xpack *xp, struct writer *wr, serdata_t serdata, struct tkmap_instance *tk)
{
  return write_sample_eot (xp, wr, NULL, serdata, tk, 0, 0);
}

int write_sample_gc_notk (struct nn_xpack *xp, struct writer *wr, serdata_t serdata)
{
  struct tkmap_instance *tk;
  int res;
  tk = (ddsi_plugin.rhc_lookup_fn) (serdata);
  res = write_sample_eot (xp, wr, NULL, serdata, tk, 0, 1);
  (ddsi_plugin.rhc_unref_fn) (tk);
  return res;
}

int write_sample_nogc_notk (struct nn_xpack *xp, struct writer *wr, serdata_t serdata)
{
  struct tkmap_instance *tk;
  int res;
  tk = (ddsi_plugin.rhc_lookup_fn) (serdata);
  res = write_sample_eot (xp, wr, NULL, serdata, tk, 0, 0);
  (ddsi_plugin.rhc_unref_fn) (tk);
  return res;
}

