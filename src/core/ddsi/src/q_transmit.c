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
#include <string.h>
#include <math.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/static_assert.h"

#include "dds/ddsrt/avl.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_addrset.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/q_xevent.h"
#include "dds/ddsi/ddsi_config_impl.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/q_transmit.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_hbcontrol.h"
#include "dds/ddsi/q_receive.h"
#include "dds/ddsi/q_lease.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_security_omg.h"

#include "dds/ddsi/sysdeps.h"
#include "dds__whc.h"

static const struct wr_prd_match *root_rdmatch (const struct writer *wr)
{
  return ddsrt_avl_root (&wr_readers_treedef, &wr->readers);
}

static int have_reliable_subs (const struct writer *wr)
{
  if (ddsrt_avl_is_empty (&wr->readers) || root_rdmatch (wr)->min_seq == MAX_SEQ_NUMBER)
    return 0;
  else
    return 1;
}

void writer_hbcontrol_init (struct hbcontrol *hbc)
{
  hbc->t_of_last_write.v = 0;
  hbc->t_of_last_hb.v = 0;
  hbc->t_of_last_ackhb.v = 0;
  hbc->tsched = DDSRT_MTIME_NEVER;
  hbc->hbs_since_last_write = 0;
  hbc->last_packetid = 0;
}

static void writer_hbcontrol_note_hb (struct writer *wr, ddsrt_mtime_t tnow, int ansreq)
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

int64_t writer_hbcontrol_intv (const struct writer *wr, const struct whc_state *whcst, UNUSED_ARG (ddsrt_mtime_t tnow))
{
  struct ddsi_domaingv const * const gv = wr->e.gv;
  struct hbcontrol const * const hbc = &wr->hbcontrol;
  int64_t ret = gv->config.const_hb_intv_sched;
  size_t n_unacked;

  if (hbc->hbs_since_last_write > 5)
  {
    unsigned cnt = (hbc->hbs_since_last_write - 5) / 2;
    while (cnt-- != 0 && 2 * ret < gv->config.const_hb_intv_sched_max)
      ret *= 2;
  }

  n_unacked = whcst->unacked_bytes;
  if (n_unacked >= wr->whc_low + 3 * (wr->whc_high - wr->whc_low) / 4)
    ret /= 2;
  if (n_unacked >= wr->whc_low + (wr->whc_high - wr->whc_low) / 2)
    ret /= 2;
  if (wr->throttling)
    ret /= 2;
  if (ret < gv->config.const_hb_intv_sched_min)
    ret = gv->config.const_hb_intv_sched_min;
  return ret;
}

void writer_hbcontrol_note_asyncwrite (struct writer *wr, ddsrt_mtime_t tnow)
{
  struct ddsi_domaingv const * const gv = wr->e.gv;
  struct hbcontrol * const hbc = &wr->hbcontrol;
  ddsrt_mtime_t tnext;

  /* Reset number of heartbeats since last write: that means the
     heartbeat rate will go back up to the default */
  hbc->hbs_since_last_write = 0;

  /* We know this is new data, so we want a heartbeat event after one
     base interval */
  tnext.v = tnow.v + gv->config.const_hb_intv_sched;
  if (tnext.v < hbc->tsched.v)
  {
    /* Insertion of a message with WHC locked => must now have at
       least one unacked msg if there are reliable readers, so must
       have a heartbeat scheduled.  Do so now */
    hbc->tsched = tnext;
    (void) resched_xevent_if_earlier (wr->heartbeat_xevent, tnext);
  }
}

int writer_hbcontrol_must_send (const struct writer *wr, const struct whc_state *whcst, ddsrt_mtime_t tnow /* monotonic */)
{
  struct hbcontrol const * const hbc = &wr->hbcontrol;
  return (tnow.v >= hbc->t_of_last_hb.v + writer_hbcontrol_intv (wr, whcst, tnow));
}

struct nn_xmsg *writer_hbcontrol_create_heartbeat (struct writer *wr, const struct whc_state *whcst, ddsrt_mtime_t tnow, int hbansreq, int issync)
{
  struct ddsi_domaingv const * const gv = wr->e.gv;
  struct nn_xmsg *msg;
  const ddsi_guid_t *prd_guid;

  ASSERT_MUTEX_HELD (&wr->e.lock);
  assert (wr->reliable);
  assert (hbansreq >= 0);

  if ((msg = nn_xmsg_new (gv->xmsgpool, &wr->e.guid, wr->c.pp, sizeof (InfoTS_t) + sizeof (Heartbeat_t), NN_XMSG_KIND_CONTROL)) == NULL)
    /* out of memory at worst slows down traffic */
    return NULL;

  if (ddsrt_avl_is_empty (&wr->readers) || wr->num_reliable_readers == 0)
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
    const uint32_t n_unacked = wr->num_reliable_readers - root_rdmatch (wr)->num_reliable_readers_where_seq_equals_max;
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

  ETRACE (wr, "writer_hbcontrol: wr "PGUIDFMT" ", PGUID (wr->e.guid));
  if (prd_guid == NULL)
    ETRACE (wr, "multicasting ");
  else
    ETRACE (wr, "unicasting to prd "PGUIDFMT" ", PGUID (*prd_guid));
  if (ddsrt_avl_is_empty (&wr->readers))
  {
    ETRACE (wr, "(rel-prd %"PRId32" seq-eq-max [none] seq %"PRId64" maxseq %"PRId64")\n",
            wr->num_reliable_readers,
            wr->seq,
            root_rdmatch (wr)->max_seq);
  }
  else
  {
    ETRACE (wr, "(rel-prd %"PRId32" seq-eq-max %"PRId32" seq %"PRIu64" maxseq %"PRIu64")\n",
            wr->num_reliable_readers,
            (int32_t) root_rdmatch (wr)->num_reliable_readers_where_seq_equals_max,
            wr->seq,
            root_rdmatch (wr)->max_seq);
  }

  if (prd_guid == NULL)
  {
    nn_xmsg_setdstN (msg, wr->as, wr->as_group);
    add_Heartbeat (msg, wr, whcst, hbansreq, 0, to_entityid (NN_ENTITYID_UNKNOWN), issync);
  }
  else
  {
    struct proxy_reader *prd;
    if ((prd = entidx_lookup_proxy_reader_guid (gv->entity_index, prd_guid)) == NULL)
    {
      ETRACE (wr, "writer_hbcontrol: wr "PGUIDFMT" unknown prd "PGUIDFMT"\n", PGUID (wr->e.guid), PGUID (*prd_guid));
      nn_xmsg_free (msg);
      return NULL;
    }
    /* set the destination explicitly to the unicast destination and the fourth
       param of add_Heartbeat needs to be the guid of the reader */
    nn_xmsg_setdstPRD (msg, prd);
    // send to all readers in the participant: whether or not the entityid is set affects
    // the retransmit requests
    add_Heartbeat (msg, wr, whcst, hbansreq, 0, to_entityid (NN_ENTITYID_UNKNOWN), issync);
  }

  /* It is possible that the encoding removed the submessage(s). */
  if (nn_xmsg_size(msg) == 0)
  {
    nn_xmsg_free (msg);
    msg = NULL;
  }

  writer_hbcontrol_note_hb (wr, tnow, hbansreq);
  return msg;
}

static int writer_hbcontrol_ack_required_generic (const struct writer *wr, const struct whc_state *whcst, ddsrt_mtime_t tlast, ddsrt_mtime_t tnow, int piggyback)
{
  struct ddsi_domaingv const * const gv = wr->e.gv;
  struct hbcontrol const * const hbc = &wr->hbcontrol;
  const int64_t hb_intv_ack = gv->config.const_hb_intv_sched;
  assert(wr->heartbeat_xevent != NULL && whcst != NULL);

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

  if (whcst->unacked_bytes >= wr->whc_low + (wr->whc_high - wr->whc_low) / 2)
  {
    if (tnow.v >= hbc->t_of_last_ackhb.v + gv->config.const_hb_intv_sched_min)
      return 2;
    else if (tnow.v >= hbc->t_of_last_ackhb.v + gv->config.const_hb_intv_min)
      return 1;
  }

  return 0;
}

int writer_hbcontrol_ack_required (const struct writer *wr, const struct whc_state *whcst, ddsrt_mtime_t tnow)
{
  struct hbcontrol const * const hbc = &wr->hbcontrol;
  return writer_hbcontrol_ack_required_generic (wr, whcst, hbc->t_of_last_write, tnow, 0);
}

struct nn_xmsg *writer_hbcontrol_piggyback (struct writer *wr, const struct whc_state *whcst, ddsrt_mtime_t tnow, uint32_t packetid, int *hbansreq)
{
  struct hbcontrol * const hbc = &wr->hbcontrol;
  uint32_t last_packetid;
  ddsrt_mtime_t tlast;
  ddsrt_mtime_t t_of_last_hb;
  struct nn_xmsg *msg;

  tlast = hbc->t_of_last_write;
  last_packetid = hbc->last_packetid;
  t_of_last_hb = hbc->t_of_last_hb;

  hbc->t_of_last_write = tnow;
  hbc->last_packetid = packetid;

  /* Update statistics, intervals, scheduling of heartbeat event,
     &c. -- there's no real difference between async and sync so we
     reuse the async version. */
  writer_hbcontrol_note_asyncwrite (wr, tnow);

  *hbansreq = writer_hbcontrol_ack_required_generic (wr, whcst, tlast, tnow, 1);
  if (*hbansreq >= 2) {
    /* So we force a heartbeat in - but we also rely on our caller to
       send the packet out */
    msg = writer_hbcontrol_create_heartbeat (wr, whcst, tnow, *hbansreq, 1);
  } else if (last_packetid != packetid && tnow.v - t_of_last_hb.v > DDS_USECS (100)) {
    /* If we crossed a packet boundary since the previous write,
       piggyback a heartbeat, with *hbansreq determining whether or
       not an ACK is needed.  We don't force the packet out either:
       this is just to ensure a regular flow of ACKs for cleaning up
       the WHC & for allowing readers to NACK missing samples.

       Still rate-limit: if there are new readers that haven't sent an
       an ACK yet, the FINAL flag will be cleared and so we get an ACK
       storm if writing at a high rate without batching which eats up
       a *large* amount of time because there are out-of-order readers
       present. */
    msg = writer_hbcontrol_create_heartbeat (wr, whcst, tnow, *hbansreq, 1);
  } else {
    *hbansreq = 0;
    msg = NULL;
  }

  if (msg)
  {
    if (ddsrt_avl_is_empty (&wr->readers))
    {
      ETRACE (wr, "heartbeat(wr "PGUIDFMT"%s) piggybacked, resched in %g s (min-ack [none], avail-seq %"PRIu64", xmit %"PRIu64")\n",
              PGUID (wr->e.guid),
              *hbansreq ? "" : " final",
              (hbc->tsched.v == DDS_NEVER) ? INFINITY : (double) (hbc->tsched.v - tnow.v) / 1e9,
              whcst->max_seq, writer_read_seq_xmit(wr));
    }
    else
    {
      ETRACE (wr, "heartbeat(wr "PGUIDFMT"%s) piggybacked, resched in %g s (min-ack %"PRIu64"%s, avail-seq %"PRIu64", xmit %"PRIu64")\n",
              PGUID (wr->e.guid),
              *hbansreq ? "" : " final",
              (hbc->tsched.v == DDS_NEVER) ? INFINITY : (double) (hbc->tsched.v - tnow.v) / 1e9,
              root_rdmatch (wr)->min_seq,
              root_rdmatch (wr)->all_have_replied_to_hb ? "" : "!",
              whcst->max_seq, writer_read_seq_xmit(wr));
    }
  }

  return msg;
}

#ifdef DDS_HAS_SECURITY
struct nn_xmsg *writer_hbcontrol_p2p(struct writer *wr, const struct whc_state *whcst, int hbansreq, struct proxy_reader *prd)
{
  struct ddsi_domaingv const * const gv = wr->e.gv;
  struct nn_xmsg *msg;

  ASSERT_MUTEX_HELD (&wr->e.lock);
  assert (wr->reliable);

  if ((msg = nn_xmsg_new (gv->xmsgpool, &wr->e.guid, wr->c.pp, sizeof (InfoTS_t) + sizeof (Heartbeat_t), NN_XMSG_KIND_CONTROL)) == NULL)
    return NULL;

  ETRACE (wr, "writer_hbcontrol_p2p: wr "PGUIDFMT" unicasting to prd "PGUIDFMT" ", PGUID (wr->e.guid), PGUID (prd->e.guid));
  if (ddsrt_avl_is_empty (&wr->readers))
  {
    ETRACE (wr, "(rel-prd %d seq-eq-max [none] seq %"PRIu64")\n", wr->num_reliable_readers, wr->seq);
  }
  else
  {
    ETRACE (wr, "(rel-prd %d seq-eq-max %d seq %"PRIu64" maxseq %"PRIu64")\n",
            wr->num_reliable_readers,
            (int32_t) root_rdmatch (wr)->num_reliable_readers_where_seq_equals_max,
            wr->seq,
            root_rdmatch (wr)->max_seq);
  }

  /* set the destination explicitly to the unicast destination and the fourth
     param of add_Heartbeat needs to be the guid of the reader */
  nn_xmsg_setdstPRD (msg, prd);
  add_Heartbeat (msg, wr, whcst, hbansreq, 0, prd->e.guid.entityid, 1);

  if (nn_xmsg_size(msg) == 0)
  {
    nn_xmsg_free (msg);
    msg = NULL;
  }

  return msg;
}
#endif

void add_Heartbeat (struct nn_xmsg *msg, struct writer *wr, const struct whc_state *whcst, int hbansreq, int hbliveliness, ddsi_entityid_t dst, int issync)
{
  struct ddsi_domaingv const * const gv = wr->e.gv;
  struct nn_xmsg_marker sm_marker;
  Heartbeat_t * hb;
  seqno_t max, min;

  ASSERT_MUTEX_HELD (&wr->e.lock);

  assert (wr->reliable);
  assert (hbansreq >= 0);
  assert (hbliveliness >= 0);

  if (gv->config.meas_hb_to_ack_latency)
  {
    /* If configured to measure heartbeat-to-ack latency, we must add
       a timestamp.  No big deal if it fails. */
    nn_xmsg_add_timestamp (msg, ddsrt_time_wallclock ());
  }

  hb = nn_xmsg_append (msg, &sm_marker, sizeof (Heartbeat_t));
  nn_xmsg_submsg_init (msg, sm_marker, SMID_HEARTBEAT);

  if (!hbansreq)
    hb->smhdr.flags |= HEARTBEAT_FLAG_FINAL;
  if (hbliveliness)
    hb->smhdr.flags |= HEARTBEAT_FLAG_LIVELINESS;

  hb->readerId = nn_hton_entityid (dst);
  hb->writerId = nn_hton_entityid (wr->e.guid.entityid);
  if (WHCST_ISEMPTY(whcst))
  {
    max = wr->seq;
    min = max + 1;
  }
  else
  {
    /* If data present in WHC, wr->seq > 0, but xmit_seq possibly still 0 */
    min = whcst->min_seq;
    max = wr->seq;
    const seqno_t seq_xmit = writer_read_seq_xmit (wr);
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
      } else {
        /* Advertise no data yet */
        max = min - 1;
      }
    }
  }
  hb->firstSN = toSN (min);
  hb->lastSN = toSN (max);

  hb->count = wr->hbcount++;

  nn_xmsg_submsg_setnext (msg, sm_marker);
  encode_datawriter_submsg(msg, sm_marker, wr);
}

static dds_return_t create_fragment_message_simple (struct writer *wr, seqno_t seq, struct ddsi_serdata *serdata, struct nn_xmsg **pmsg)
{
#define TEST_KEYHASH 0
  /* actual expected_inline_qos_size is typically 0, but always claiming 32 bytes won't make
     a difference, so no point in being precise */
  const size_t expected_inline_qos_size = /* statusinfo */ 8 + /* keyhash */ 20 + /* sentinel */ 4;
  struct ddsi_domaingv const * const gv = wr->e.gv;
  struct nn_xmsg_marker sm_marker;
  unsigned char contentflag = 0;
  Data_t *data;

  switch (serdata->kind)
  {
    case SDK_EMPTY:
      break;
    case SDK_KEY:
#if TEST_KEYHASH
      contentflag = wr->include_keyhash ? 0 : DATA_FLAG_KEYFLAG;
#else
      contentflag = DATA_FLAG_KEYFLAG;
#endif
      break;
    case SDK_DATA:
      contentflag = DATA_FLAG_DATAFLAG;
      break;
  }

  ASSERT_MUTEX_HELD (&wr->e.lock);

  /* INFO_TS: 12 bytes, Data_t: 24 bytes, expected inline QoS: 32 => should be single chunk */
  if ((*pmsg = nn_xmsg_new (gv->xmsgpool, &wr->e.guid, wr->c.pp, sizeof (InfoTimestamp_t) + sizeof (Data_t) + expected_inline_qos_size, NN_XMSG_KIND_DATA)) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;

  nn_xmsg_setdstN (*pmsg, wr->as, wr->as_group);
  nn_xmsg_setmaxdelay (*pmsg, wr->xqos->latency_budget.duration);
  nn_xmsg_add_timestamp (*pmsg, serdata->timestamp);
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
  if (wr->num_readers_requesting_keyhash > 0)
    nn_xmsg_addpar_keyhash (*pmsg, serdata, wr->force_md5_keyhash);
  if (serdata->statusinfo)
    nn_xmsg_addpar_statusinfo (*pmsg, serdata->statusinfo);
  if (nn_xmsg_addpar_sentinel_ifparam (*pmsg) > 0)
  {
    data = nn_xmsg_submsg_from_marker (*pmsg, sm_marker);
    data->x.smhdr.flags |= DATAFRAG_FLAG_INLINE_QOS;
  }

#if TEST_KEYHASH
  if (serdata->kind != SDK_KEY || !wr->include_keyhash)
    nn_xmsg_serdata (*pmsg, serdata, 0, ddsi_serdata_size (serdata), wr);
#else
  nn_xmsg_serdata (*pmsg, serdata, 0, ddsi_serdata_size (serdata), wr);
#endif
  nn_xmsg_submsg_setnext (*pmsg, sm_marker);
  return 0;
}

dds_return_t create_fragment_message (struct writer *wr, seqno_t seq, const struct ddsi_plist *plist, struct ddsi_serdata *serdata, uint32_t fragnum, uint16_t nfrags, struct proxy_reader *prd, struct nn_xmsg **pmsg, int isnew, uint32_t advertised_fragnum)
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

     actual expected_inline_qos_size is typically 0, but always claiming 32 bytes won't make
     a difference, so no point in being precise */
  const size_t expected_inline_qos_size = /* statusinfo */ 8 + /* keyhash */ 20 + /* sentinel */ 4;
  struct ddsi_domaingv const * const gv = wr->e.gv;
  struct nn_xmsg_marker sm_marker;
  void *sm;
  Data_DataFrag_common_t *ddcmn;
  int fragging;
  uint32_t fragstart, fraglen;
  enum nn_xmsg_kind xmsg_kind = isnew ? NN_XMSG_KIND_DATA : NN_XMSG_KIND_DATA_REXMIT;
  const uint32_t size = ddsi_serdata_size (serdata);
  dds_return_t ret = 0;
  (void)plist;

  ASSERT_MUTEX_HELD (&wr->e.lock);

  if (fragnum * (uint32_t) gv->config.fragment_size >= size && size > 0)
  {
    /* This is the first chance to detect an attempt at retransmitting
       an non-existent fragment, which a malicious (or buggy) remote
       reader can trigger.  So we return an error instead of asserting
       as we used to. */
    return DDS_RETCODE_BAD_PARAMETER;
  }

  fragging = (nfrags * (uint32_t) gv->config.fragment_size < size);

  /* INFO_TS: 12 bytes, DataFrag_t: 36 bytes, expected inline QoS: 32 => should be single chunk */
  if ((*pmsg = nn_xmsg_new (gv->xmsgpool, &wr->e.guid, wr->c.pp, sizeof (InfoTimestamp_t) + sizeof (DataFrag_t) + expected_inline_qos_size, xmsg_kind)) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;

  if (prd)
  {
    nn_xmsg_setdstPRD (*pmsg, prd);
    /* retransmits: latency budget doesn't apply */
  }
  else
  {
    nn_xmsg_setdstN (*pmsg, wr->as, wr->as_group);
    nn_xmsg_setmaxdelay (*pmsg, wr->xqos->latency_budget.duration);
  }

  /* Timestamp only needed once, for the first fragment */
  if (fragnum == 0)
  {
    nn_xmsg_add_timestamp (*pmsg, serdata->timestamp);
  }

  sm = nn_xmsg_append (*pmsg, &sm_marker, fragging ? sizeof (DataFrag_t) : sizeof (Data_t));
  ddcmn = sm;

  if (!fragging)
  {
    unsigned char contentflag = 0;
    Data_t *data = sm;
    switch (serdata->kind)
    {
      case SDK_EMPTY: contentflag = 0; break;
      case SDK_KEY:   contentflag = DATA_FLAG_KEYFLAG; break;
      case SDK_DATA:  contentflag = DATA_FLAG_DATAFLAG; break;
    }
    nn_xmsg_submsg_init (*pmsg, sm_marker, SMID_DATA);
    ddcmn->smhdr.flags = (unsigned char) (ddcmn->smhdr.flags | contentflag);

    fragstart = 0;
    fraglen = size;
    ddcmn->octetsToInlineQos = (unsigned short) ((char*) (data+1) - ((char*) &ddcmn->octetsToInlineQos + 2));

    if (wr->reliable)
      nn_xmsg_setwriterseq (*pmsg, &wr->e.guid, seq);
  }
  else
  {
    const unsigned char contentflag = (serdata->kind == SDK_KEY ? DATAFRAG_FLAG_KEYFLAG : 0);
    DataFrag_t *frag = sm;
    /* empty means size = 0, which means it never needs fragmenting */
    assert (serdata->kind != SDK_EMPTY);
    nn_xmsg_submsg_init (*pmsg, sm_marker, SMID_DATA_FRAG);
    ddcmn->smhdr.flags = (unsigned char) (ddcmn->smhdr.flags | contentflag);

    frag->fragmentStartingNum = fragnum + 1;
    frag->fragmentsInSubmessage = nfrags;
    frag->fragmentSize = gv->config.fragment_size;
    frag->sampleSize = (uint32_t) size;

    fragstart = fragnum * (uint32_t) gv->config.fragment_size;
    fraglen = (uint32_t) gv->config.fragment_size * (uint32_t) frag->fragmentsInSubmessage;
    if (fragstart + fraglen > size)
      fraglen = (uint32_t) (size - fragstart);
    ddcmn->octetsToInlineQos = (unsigned short) ((char*) (frag+1) - ((char*) &ddcmn->octetsToInlineQos + 2));

    if (wr->reliable && (!isnew || advertised_fragnum != UINT32_MAX))
    {
      /* only set for final fragment for new messages; for rexmits we
         want it set for all so we can do merging. FIXME: I guess the
         writer should track both seq_xmit and the fragment number
         ... */
      nn_xmsg_setwriterseq_fragid (*pmsg, &wr->e.guid, seq, isnew ? advertised_fragnum : fragnum + frag->fragmentsInSubmessage - 1);
    }
  }

  ddcmn->extraFlags = 0;
  ddcmn->readerId = nn_hton_entityid (prd ? prd->e.guid.entityid : to_entityid (NN_ENTITYID_UNKNOWN));
  ddcmn->writerId = nn_hton_entityid (wr->e.guid.entityid);
  ddcmn->writerSN = toSN (seq);

  if (xmsg_kind == NN_XMSG_KIND_DATA_REXMIT)
    nn_xmsg_set_data_readerId (*pmsg, &ddcmn->readerId);

  DDSRT_STATIC_ASSERT_CODE (DATA_FLAG_INLINE_QOS == DATAFRAG_FLAG_INLINE_QOS);
  assert (!(ddcmn->smhdr.flags & DATAFRAG_FLAG_INLINE_QOS));

  if (fragnum == 0)
  {
    int rc;
    /* Adding parameters means potential reallocing, so sm, ddcmn now likely become invalid */
    if (wr->num_readers_requesting_keyhash > 0)
    {
      nn_xmsg_addpar_keyhash (*pmsg, serdata, wr->force_md5_keyhash);
    }
    if (serdata->statusinfo)
    {
      nn_xmsg_addpar_statusinfo (*pmsg, serdata->statusinfo);
    }
    rc = nn_xmsg_addpar_sentinel_ifparam (*pmsg);
    if (rc > 0)
    {
      ddcmn = nn_xmsg_submsg_from_marker (*pmsg, sm_marker);
      ddcmn->smhdr.flags |= DATAFRAG_FLAG_INLINE_QOS;
    }
  }

  nn_xmsg_serdata (*pmsg, serdata, fragstart, fraglen, wr);
  nn_xmsg_submsg_setnext (*pmsg, sm_marker);
#if 0
  GVTRACE ("queue data%s "PGUIDFMT" #%"PRId64"/%"PRIu32"[%"PRIu32"..%"PRIu32")\n",
           fragging ? "frag" : "", PGUID (wr->e.guid),
           seq, fragnum+1, fragstart, fragstart + fraglen);
#endif

  encode_datawriter_submsg(*pmsg, sm_marker, wr);

  /* It is possible that the encoding removed the submessage.
   * If there is no content, free the message. */
  if (nn_xmsg_size(*pmsg) == 0) {
      nn_xmsg_free (*pmsg);
      *pmsg = NULL;
  }

  return ret;
}

static void create_HeartbeatFrag (struct writer *wr, seqno_t seq, unsigned fragnum, struct proxy_reader *prd, struct nn_xmsg **pmsg)
{
  struct ddsi_domaingv const * const gv = wr->e.gv;
  struct nn_xmsg_marker sm_marker;
  HeartbeatFrag_t *hbf;
  ASSERT_MUTEX_HELD (&wr->e.lock);
  if ((*pmsg = nn_xmsg_new (gv->xmsgpool, &wr->e.guid, wr->c.pp, sizeof (HeartbeatFrag_t), NN_XMSG_KIND_CONTROL)) == NULL)
    return; /* ignore out-of-memory: HeartbeatFrag is only advisory anyway */
  if (prd)
    nn_xmsg_setdstPRD (*pmsg, prd);
  else
    nn_xmsg_setdstN (*pmsg, wr->as, wr->as_group);
  hbf = nn_xmsg_append (*pmsg, &sm_marker, sizeof (HeartbeatFrag_t));
  nn_xmsg_submsg_init (*pmsg, sm_marker, SMID_HEARTBEAT_FRAG);
  hbf->readerId = nn_hton_entityid (prd ? prd->e.guid.entityid : to_entityid (NN_ENTITYID_UNKNOWN));
  hbf->writerId = nn_hton_entityid (wr->e.guid.entityid);
  hbf->writerSN = toSN (seq);
  hbf->lastFragmentNum = fragnum + 1; /* network format is 1 based */

  hbf->count = wr->hbfragcount++;

  nn_xmsg_submsg_setnext (*pmsg, sm_marker);
  encode_datawriter_submsg(*pmsg, sm_marker, wr);

  /* It is possible that the encoding removed the submessage.
   * If there is no content, free the message. */
  if (nn_xmsg_size(*pmsg) == 0)
  {
    nn_xmsg_free(*pmsg);
    *pmsg = NULL;
  }
}

dds_return_t write_hb_liveliness (struct ddsi_domaingv * const gv, struct ddsi_guid *wr_guid, struct nn_xpack *xp)
{
  struct nn_xmsg *msg = NULL;
  struct whc_state whcst;
  struct thread_state1 * const ts1 = lookup_thread_state ();
  struct lease *lease;

  thread_state_awake (ts1, gv);
  struct writer *wr = entidx_lookup_writer_guid (gv->entity_index, wr_guid);
  if (wr == NULL)
  {
    GVTRACE ("write_hb_liveliness("PGUIDFMT") - writer not found\n", PGUID (*wr_guid));
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  }

  if (wr->xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_PARTICIPANT && ((lease = ddsrt_atomic_ldvoidp (&wr->c.pp->minl_man)) != NULL))
    lease_renew (lease, ddsrt_time_elapsed());
  else if (wr->xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_TOPIC && wr->lease != NULL)
    lease_renew (wr->lease, ddsrt_time_elapsed());

  if ((msg = nn_xmsg_new (gv->xmsgpool, &wr->e.guid, wr->c.pp, sizeof (InfoTS_t) + sizeof (Heartbeat_t), NN_XMSG_KIND_CONTROL)) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  ddsrt_mutex_lock (&wr->e.lock);
  nn_xmsg_setdstN (msg, wr->as, wr->as_group);
  whc_get_state (wr->whc, &whcst);
  add_Heartbeat (msg, wr, &whcst, 0, 1, to_entityid (NN_ENTITYID_UNKNOWN), 1);
  ddsrt_mutex_unlock (&wr->e.lock);
  nn_xpack_addmsg (xp, msg, 0);
  nn_xpack_send (xp, true);
  thread_state_asleep (ts1);
  return DDS_RETCODE_OK;
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

static void transmit_sample_lgmsg_unlocks_wr (struct nn_xpack *xp, struct writer *wr, seqno_t seq, const struct ddsi_plist *plist, struct ddsi_serdata *serdata, struct proxy_reader *prd, int isnew, uint32_t nfrags, uint32_t nfrags_lim)
{
#if 0
  const char *frags_to_skip = getenv ("SKIPFRAGS");
#endif
  assert(xp);
  assert(0 < nfrags_lim && nfrags_lim <= nfrags);
  uint32_t nf_in_submsg = isnew ? (wr->e.gv->config.max_msg_size / wr->e.gv->config.fragment_size) : 1;
  if (nf_in_submsg == 0)
    nf_in_submsg = 1;
  else if (nf_in_submsg > UINT16_MAX)
    nf_in_submsg = UINT16_MAX;
  for (uint32_t i = 0; i < nfrags_lim; i += nf_in_submsg)
  {
    struct nn_xmsg *fmsg = NULL;
    struct nn_xmsg *hmsg = NULL;
    int ret;
#if 0
    if (must_skip_frag (frags_to_skip, i))
      continue;
#endif

    if (nf_in_submsg > nfrags_lim - i)
      nf_in_submsg = nfrags_lim - i;

    /* Ignore out-of-memory errors: we can't do anything about it, and
       eventually we'll have to retry.  But if a packet went out and
       we haven't yet completed transmitting a fragmented message, add
       a HeartbeatFrag. */
    ret = create_fragment_message (wr, seq, plist, serdata, i, (uint16_t) nf_in_submsg, prd, &fmsg, isnew, i + nf_in_submsg == nfrags_lim ? nfrags - 1 : UINT32_MAX);
    if (ret >= 0 && i + nf_in_submsg < nfrags_lim && wr->heartbeat_xevent)
    {
      // more fragment messages to come
      create_HeartbeatFrag (wr, seq, i + nf_in_submsg - 1, prd, &hmsg);
    }
    ddsrt_mutex_unlock (&wr->e.lock);

    if(fmsg) nn_xpack_addmsg (xp, fmsg, 0);
    if(hmsg) nn_xpack_addmsg (xp, hmsg, 0);

    ddsrt_mutex_lock (&wr->e.lock);
  }
}

static void transmit_sample_unlocks_wr (struct nn_xpack *xp, struct writer *wr, const struct whc_state *whcst, seqno_t seq, const struct ddsi_plist *plist, struct ddsi_serdata *serdata, struct proxy_reader *prd, int isnew)
{
  /* on entry: &wr->e.lock held; on exit: lock no longer held */
  struct ddsi_domaingv const * const gv = wr->e.gv;
  struct nn_xmsg *hmsg = NULL;
  int hbansreq = 0;
  uint32_t sz;
  assert(xp);
  assert((wr->heartbeat_xevent != NULL) == (whcst != NULL));

  sz = ddsi_serdata_size (serdata);
  if (sz > gv->config.fragment_size || !isnew || plist != NULL || prd != NULL || q_omg_writer_is_submessage_protected(wr))
  {
    assert (wr->init_burst_size_limit <= UINT32_MAX - UINT16_MAX);
    assert (wr->rexmit_burst_size_limit <= UINT32_MAX - UINT16_MAX);
    const uint32_t max_burst_size = isnew ? wr->init_burst_size_limit : wr->rexmit_burst_size_limit;
    const uint32_t nfrags = (sz + gv->config.fragment_size - 1) / gv->config.fragment_size;
    uint32_t nfrags_lim;
    if (sz <= max_burst_size || wr->num_reliable_readers != wr->num_readers)
      nfrags_lim = nfrags; // if it fits or if there are best-effort readers, send it in its entirety
    else
      nfrags_lim = (max_burst_size + gv->config.fragment_size - 1) / gv->config.fragment_size;

    transmit_sample_lgmsg_unlocks_wr (xp, wr, seq, plist, serdata, prd, isnew, nfrags, nfrags_lim);
  }
  else
  {
    struct nn_xmsg *fmsg;
    if (create_fragment_message_simple (wr, seq, serdata, &fmsg) >= 0)
      nn_xpack_addmsg (xp, fmsg, 0);
  }

  if (wr->heartbeat_xevent)
    hmsg = writer_hbcontrol_piggyback (wr, whcst, serdata->twrite, nn_xpack_packetid (xp), &hbansreq);
  ddsrt_mutex_unlock (&wr->e.lock);

  if(hmsg)
    nn_xpack_addmsg (xp, hmsg, 0);
  if (hbansreq >= 2)
    nn_xpack_send (xp, true);
}

void enqueue_spdp_sample_wrlock_held (struct writer *wr, seqno_t seq, struct ddsi_serdata *serdata, struct proxy_reader *prd)
{
  assert (wr->e.guid.entityid.u == NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER);
  struct nn_xmsg *msg = NULL;
  if (create_fragment_message(wr, seq, NULL, serdata, 0, UINT16_MAX, prd, &msg, 1, UINT32_MAX) >= 0)
    qxev_msg (wr->evq, msg);
}

int enqueue_sample_wrlock_held (struct writer *wr, seqno_t seq, const struct ddsi_plist *plist, struct ddsi_serdata *serdata, struct proxy_reader *prd, int isnew)
{
  struct ddsi_domaingv const * const gv = wr->e.gv;
  uint32_t i, sz, nfrags;
  enum qxev_msg_rexmit_result enqueued = QXEV_MSG_REXMIT_QUEUED;

  ASSERT_MUTEX_HELD (&wr->e.lock);

  sz = ddsi_serdata_size (serdata);
  nfrags = (sz + gv->config.fragment_size - 1) / gv->config.fragment_size;
  if (nfrags == 0)
  {
    /* end-of-transaction messages are empty, but still need to be sent */
    nfrags = 1;
  }
  if (!isnew && nfrags > 1)
    nfrags = 1;
  for (i = 0; i < nfrags && enqueued != QXEV_MSG_REXMIT_DROPPED; i++)
  {
    struct nn_xmsg *fmsg = NULL;
    struct nn_xmsg *hmsg = NULL;
    /* Ignore out-of-memory errors: we can't do anything about it, and
       eventually we'll have to retry.  But if a packet went out and
       we haven't yet completed transmitting a fragmented message, add
       a HeartbeatFrag. */
    if (create_fragment_message (wr, seq, plist, serdata, i, 1, prd, &fmsg, isnew, (i+1) == nfrags ? i : UINT32_MAX) >= 0)
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
        switch (enqueued)
        {
          case QXEV_MSG_REXMIT_DROPPED:
          case QXEV_MSG_REXMIT_MERGED:
            nn_xmsg_free (hmsg);
            break;
          case QXEV_MSG_REXMIT_QUEUED:
            qxev_msg (wr->evq, hmsg);
            break;
        }
      }
    }
  }
  return (enqueued != QXEV_MSG_REXMIT_DROPPED) ? 0 : -1;
}

static int insert_sample_in_whc (struct writer *wr, seqno_t seq, struct ddsi_plist *plist, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk)
{
  /* returns: < 0 on error, 0 if no need to insert in whc, > 0 if inserted */
  int insres, res = 0;
  bool wr_deadline = false;

  ASSERT_MUTEX_HELD (&wr->e.lock);

  if (wr->e.gv->logconfig.c.mask & DDS_LC_TRACE)
  {
    char ppbuf[1024];
    int tmp;
    ppbuf[0] = '\0';
    tmp = sizeof (ppbuf) - 1;
    if (wr->e.gv->logconfig.c.mask & DDS_LC_CONTENT)
      ddsi_serdata_print (serdata, ppbuf, sizeof (ppbuf));
    ETRACE (wr, "write_sample "PGUIDFMT" #%"PRIu64, PGUID (wr->e.guid), seq);
    if (plist != 0 && (plist->present & PP_COHERENT_SET))
      ETRACE (wr, " C#%"PRIu64"", fromSN (plist->coherent_set_seqno));
    ETRACE (wr, ": ST%"PRIu32" %s/%s:%s%s\n", serdata->statusinfo, wr->xqos->topic_name, wr->type->type_name, ppbuf, tmp < (int) sizeof (ppbuf) ? "" : " (trunc)");
  }

  assert (wr->reliable || have_reliable_subs (wr) == 0);
#ifdef DDS_HAS_DEADLINE_MISSED
  /* If deadline missed duration is not infinite, the sample is inserted in
     the whc so that the instance is created (or renewed) in the whc and the deadline
     missed event is registered. The sample is removed immediately after inserting it
     as we don't want to store it. */
  wr_deadline = wr->xqos->deadline.deadline != DDS_INFINITY;
#endif

  if ((wr->reliable && have_reliable_subs (wr)) || wr_deadline || wr->handle_as_transient_local)
  {
    ddsrt_mtime_t exp = DDSRT_MTIME_NEVER;
#ifdef DDS_HAS_LIFESPAN
    /* Don't set expiry for samples with flags unregister or dispose, because these are required
     * for sample lifecycle and should always be delivered to the reader so that is can clean up
     * its history cache. */
    if (wr->xqos->lifespan.duration != DDS_INFINITY && (serdata->statusinfo & (NN_STATUSINFO_UNREGISTER | NN_STATUSINFO_DISPOSE)) == 0)
      exp = ddsrt_mtime_add_duration(serdata->twrite, wr->xqos->lifespan.duration);
#endif
    res = ((insres = whc_insert (wr->whc, writer_max_drop_seq (wr), seq, exp, plist, serdata, tk)) < 0) ? insres : 1;

#ifdef DDS_HAS_DEADLINE_MISSED
    if (!(wr->reliable && have_reliable_subs (wr)) && !wr->handle_as_transient_local)
    {
      /* Sample was inserted only because writer has deadline, so we'll remove the sample from whc */
      struct whc_node *deferred_free_list = NULL;
      struct whc_state whcst;
      uint32_t n = whc_remove_acked_messages (wr->whc, seq, &whcst, &deferred_free_list);
      (void)n;
      assert (n <= 1);
      assert (whcst.min_seq == 0 && whcst.max_seq == 0);
      whc_free_deferred_free_list (wr->whc, deferred_free_list);
    }
#endif
  }

#ifndef NDEBUG
  if (((wr->e.guid.entityid.u == NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER) ||
       (wr->e.guid.entityid.u == NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER)) &&
       !is_local_orphan_endpoint (&wr->e))
  {
    struct whc_state whcst;
    whc_get_state(wr->whc, &whcst);
    if (WHCST_ISEMPTY(&whcst))
      assert (wr->c.pp->state >= PARTICIPANT_STATE_DELETING_BUILTINS);
  }
#endif
  return res;
}

static int writer_may_continue (const struct writer *wr, const struct whc_state *whcst)
{
  return (whcst->unacked_bytes <= wr->whc_low && !wr->retransmitting) || (wr->state != WRST_OPERATIONAL);
}

static dds_return_t throttle_writer (struct thread_state1 * const ts1, struct nn_xpack *xp, struct writer *wr)
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
  struct ddsi_domaingv const * const gv = wr->e.gv;
  dds_return_t result = DDS_RETCODE_OK;
  const ddsrt_mtime_t throttle_start = ddsrt_time_monotonic ();
  const ddsrt_mtime_t abstimeout = ddsrt_mtime_add_duration (throttle_start, wr->xqos->reliability.max_blocking_time);
  ddsrt_mtime_t tnow = throttle_start;
  struct whc_state whcst;
  whc_get_state (wr->whc, &whcst);

  {
    ASSERT_MUTEX_HELD (&wr->e.lock);
    assert (wr->throttling == 0);
    assert (thread_is_awake ());
    assert (!is_builtin_entityid(wr->e.guid.entityid, NN_VENDORID_ECLIPSE));
  }

  GVLOG (DDS_LC_THROTTLE,
         "writer "PGUIDFMT" waiting for whc to shrink below low-water mark (whc %"PRIuSIZE" low=%"PRIu32" high=%"PRIu32")\n",
         PGUID (wr->e.guid), whcst.unacked_bytes, wr->whc_low, wr->whc_high);
  wr->throttling++;
  wr->throttle_count++;

  /* Force any outstanding packet out: there will be a heartbeat
     requesting an answer in it.  FIXME: obviously, this is doing
     things the wrong way round ... */
  if (xp)
  {
    struct nn_xmsg *hbmsg = writer_hbcontrol_create_heartbeat (wr, &whcst, tnow, 1, 1);
    ddsrt_mutex_unlock (&wr->e.lock);
    if (hbmsg)
    {
      nn_xpack_addmsg (xp, hbmsg, 0);
    }
    nn_xpack_send (xp, true);
    ddsrt_mutex_lock (&wr->e.lock);
    whc_get_state (wr->whc, &whcst);
  }

  while (ddsrt_atomic_ld32 (&gv->rtps_keepgoing) && !writer_may_continue (wr, &whcst))
  {
    int64_t reltimeout;
    tnow = ddsrt_time_monotonic ();
    reltimeout = abstimeout.v - tnow.v;
    result = DDS_RETCODE_TIMEOUT;
    if (reltimeout > 0)
    {
      thread_state_asleep (ts1);
      if (ddsrt_cond_waitfor (&wr->throttle_cond, &wr->e.lock, reltimeout))
        result = DDS_RETCODE_OK;
      thread_state_awake_domain_ok (ts1);
      whc_get_state(wr->whc, &whcst);
    }
    if (result == DDS_RETCODE_TIMEOUT)
    {
      break;
    }
  }

  wr->throttling--;
  wr->time_throttled += (uint64_t) (ddsrt_time_monotonic().v - throttle_start.v);
  if (wr->state != WRST_OPERATIONAL)
  {
    /* gc_delete_writer may be waiting */
    ddsrt_cond_broadcast (&wr->throttle_cond);
  }

  GVLOG (DDS_LC_THROTTLE,
         "writer "PGUIDFMT" done waiting for whc to shrink below low-water mark (whc %"PRIuSIZE" low=%"PRIu32" high=%"PRIu32")\n",
         PGUID (wr->e.guid), whcst.unacked_bytes, wr->whc_low, wr->whc_high);
  return result;
}

static int maybe_grow_whc (struct writer *wr)
{
  struct ddsi_domaingv const * const gv = wr->e.gv;
  if (!wr->retransmitting && gv->config.whc_adaptive && wr->whc_high < gv->config.whc_highwater_mark)
  {
    ddsrt_etime_t tnow = ddsrt_time_elapsed();
    ddsrt_etime_t tgrow = ddsrt_etime_add_duration (wr->t_whc_high_upd, DDS_MSECS (10));
    if (tnow.v >= tgrow.v)
    {
      uint32_t m = (gv->config.whc_highwater_mark - wr->whc_high) / 32;
      wr->whc_high = (m == 0) ? gv->config.whc_highwater_mark : wr->whc_high + m;
      wr->t_whc_high_upd = tnow;
      return 1;
    }
  }
  return 0;
}

int write_sample_p2p_wrlock_held(struct writer *wr, seqno_t seq, struct ddsi_plist *plist, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk, struct proxy_reader *prd)
{
  struct ddsi_domaingv * const gv = wr->e.gv;
  int r = 0;
  ddsrt_mtime_t tnow;
  int rexmit = 1;
  struct wr_prd_match *wprd = NULL;
  seqno_t gseq;
  struct nn_xmsg *gap = NULL;

  tnow = ddsrt_time_monotonic ();
  serdata->twrite = tnow;
  serdata->timestamp = ddsrt_time_wallclock ();


  if (prd->filter)
  {
    if ((wprd = ddsrt_avl_lookup (&wr_readers_treedef, &wr->readers, &prd->e.guid)) != NULL)
    {
      if (wprd->seq == MAX_SEQ_NUMBER)
        goto prd_is_deleting;

      rexmit = prd->filter(wr, prd, serdata);
      /* determine if gap has to added */
      if (rexmit)
      {
        struct nn_gap_info gi;

        GVLOG (DDS_LC_DISCOVERY, "send filtered "PGUIDFMT" last_seq=%"PRIu64" seq=%"PRIu64"\n", PGUID (wr->e.guid), wprd->seq, seq);

        nn_gap_info_init(&gi);
        for (gseq = wprd->seq + 1; gseq < seq; gseq++)
        {
          struct whc_borrowed_sample sample;
          if (whc_borrow_sample (wr->whc, seq, &sample))
          {
            if (prd->filter(wr, prd, sample.serdata) == 0)
            {
              nn_gap_info_update(wr->e.gv, &gi, gseq);
            }
            whc_return_sample (wr->whc, &sample, false);
          }
        }
        gap = nn_gap_info_create_gap(wr, prd, &gi);
      }
      wprd->last_seq = seq;
    }
  }

  if ((r = insert_sample_in_whc (wr, seq, plist, serdata, tk)) >= 0)
  {
    enqueue_sample_wrlock_held (wr, seq, plist, serdata, prd, 1);

    if (gap)
      qxev_msg (wr->evq, gap);

    if (wr->heartbeat_xevent)
      writer_hbcontrol_note_asyncwrite(wr, tnow);
  }
  else if (gap)
  {
    nn_xmsg_free (gap);
  }

prd_is_deleting:
  return r;
}

static int write_sample_eot (struct thread_state1 * const ts1, struct nn_xpack *xp, struct writer *wr, struct ddsi_plist *plist, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk, int end_of_txn, int gc_allowed)
{
  struct ddsi_domaingv const * const gv = wr->e.gv;
  int r;
  seqno_t seq;
  ddsrt_mtime_t tnow;
  struct lease *lease;

  /* If GC not allowed, we must be sure to never block when writing.  That is only the case for (true, aggressive) KEEP_LAST writers, and also only if there is no limit to how much unacknowledged data the WHC may contain. */
  assert (gc_allowed || (wr->xqos->history.kind == DDS_HISTORY_KEEP_LAST && wr->whc_low == INT32_MAX));
  (void) gc_allowed;

  if (gv->config.max_sample_size < (uint32_t) INT32_MAX && ddsi_serdata_size (serdata) > gv->config.max_sample_size)
  {
    char ppbuf[1024];
    int tmp;
    ppbuf[0] = '\0';
    tmp = sizeof (ppbuf) - 1;
    GVWARNING ("dropping oversize (%"PRIu32" > %"PRIu32") sample from local writer "PGUIDFMT" %s/%s:%s%s\n",
               ddsi_serdata_size (serdata), gv->config.max_sample_size,
               PGUID (wr->e.guid), wr->xqos->topic_name, wr->type->type_name, ppbuf,
               tmp < (int) sizeof (ppbuf) ? "" : " (trunc)");
    r = DDS_RETCODE_BAD_PARAMETER;
    goto drop;
  }

  if (wr->xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_PARTICIPANT && ((lease = ddsrt_atomic_ldvoidp (&wr->c.pp->minl_man)) != NULL))
    lease_renew (lease, ddsrt_time_elapsed());
  else if (wr->xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_TOPIC && wr->lease != NULL)
    lease_renew (wr->lease, ddsrt_time_elapsed());

  ddsrt_mutex_lock (&wr->e.lock);

  if (!wr->alive)
    writer_set_alive_may_unlock (wr, true);

  if (end_of_txn)
  {
    wr->cs_seq = 0;
  }

  /* If WHC overfull, block. */
  {
    struct whc_state whcst;
    whc_get_state(wr->whc, &whcst);
    if (whcst.unacked_bytes > wr->whc_high)
    {
      dds_return_t ores;
      assert(gc_allowed); /* also see beginning of the function */
      if (gv->config.prioritize_retransmit && wr->retransmitting)
        ores = throttle_writer (ts1, xp, wr);
      else
      {
        maybe_grow_whc (wr);
        if (whcst.unacked_bytes <= wr->whc_high)
          ores = DDS_RETCODE_OK;
        else
          ores = throttle_writer (ts1, xp, wr);
      }
      if (ores == DDS_RETCODE_TIMEOUT)
      {
        ddsrt_mutex_unlock (&wr->e.lock);
        r = DDS_RETCODE_TIMEOUT;
        goto drop;
      }
    }
  }

  if (wr->state != WRST_OPERATIONAL)
  {
    r = DDS_RETCODE_PRECONDITION_NOT_MET;
    ddsrt_mutex_unlock (&wr->e.lock);
    goto drop;
  }

  /* Always use the current monotonic time */
  tnow = ddsrt_time_monotonic ();
  serdata->twrite = tnow;

  seq = ++wr->seq;
  if (wr->cs_seq != 0)
  {
    if (plist == NULL)
    {
      plist = ddsrt_malloc (sizeof (*plist));
      ddsi_plist_init_empty (plist);
    }
    assert (!(plist->present & PP_COHERENT_SET));
    plist->present |= PP_COHERENT_SET;
    plist->coherent_set_seqno = toSN (wr->cs_seq);
  }

  if ((r = insert_sample_in_whc (wr, seq, plist, serdata, tk)) < 0)
  {
    /* Failure of some kind */
    ddsrt_mutex_unlock (&wr->e.lock);
    if (plist != NULL)
    {
      ddsi_plist_fini (plist);
      ddsrt_free (plist);
    }
  }
  else if (wr->test_drop_outgoing_data)
  {
    GVTRACE ("test_drop_outgoing_data");
    writer_update_seq_xmit (wr, seq);
    ddsrt_mutex_unlock (&wr->e.lock);
    if (plist != NULL)
    {
      ddsi_plist_fini (plist);
      ddsrt_free (plist);
    }
  }
  else if (addrset_empty (wr->as) && (wr->as_group == NULL || addrset_empty (wr->as_group)))
  {
    /* No network destination, so no point in doing all the work involved
       in going all the way.  We do have to record that we "transmitted"
       this sample, or it might not be retransmitted on request.

      (Note that no network destination is very nearly the same as no
      matching proxy readers.  The exception is the SPDP writer.) */
    writer_update_seq_xmit (wr, seq);
    ddsrt_mutex_unlock (&wr->e.lock);
    if (plist != NULL)
    {
      ddsi_plist_fini (plist);
      ddsrt_free (plist);
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
      ddsi_plist_t plist_stk, *plist_copy;
      struct whc_state whcst, *whcstptr;
      if (plist == NULL)
        plist_copy = NULL;
      else
      {
        plist_copy = &plist_stk;
        ddsi_plist_copy (plist_copy, plist);
      }
      if (wr->heartbeat_xevent == NULL)
        whcstptr = NULL;
      else
      {
        whc_get_state(wr->whc, &whcst);
        whcstptr = &whcst;
      }
      transmit_sample_unlocks_wr (xp, wr, whcstptr, seq, plist_copy, serdata, NULL, 1);
      if (plist_copy)
        ddsi_plist_fini (plist_copy);
    }
    else
    {
      if (wr->heartbeat_xevent)
        writer_hbcontrol_note_asyncwrite (wr, tnow);
      if (wr->e.guid.entityid.u == NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER)
        enqueue_spdp_sample_wrlock_held(wr, seq, serdata, NULL);
      else
        enqueue_sample_wrlock_held (wr, seq, plist, serdata, NULL, 1);
      ddsrt_mutex_unlock (&wr->e.lock);
    }

    /* If not actually inserted, WHC didn't take ownership of plist */
    if (r == 0 && plist != NULL)
    {
      ddsi_plist_fini (plist);
      ddsrt_free (plist);
    }
  }

drop:
  /* FIXME: shouldn't I move the ddsi_serdata_unref call to the callers? */
  ddsi_serdata_unref (serdata);
  return r;
}

int write_sample_gc (struct thread_state1 * const ts1, struct nn_xpack *xp, struct writer *wr, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk)
{
  return write_sample_eot (ts1, xp, wr, NULL, serdata, tk, 0, 1);
}

int write_sample_nogc (struct thread_state1 * const ts1, struct nn_xpack *xp, struct writer *wr, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk)
{
  return write_sample_eot (ts1, xp, wr, NULL, serdata, tk, 0, 0);
}

int write_sample_gc_notk (struct thread_state1 * const ts1, struct nn_xpack *xp, struct writer *wr, struct ddsi_serdata *serdata)
{
  struct ddsi_tkmap_instance *tk;
  int res;
  assert (thread_is_awake ());
  tk = ddsi_tkmap_lookup_instance_ref (wr->e.gv->m_tkmap, serdata);
  res = write_sample_eot (ts1, xp, wr, NULL, serdata, tk, 0, 1);
  ddsi_tkmap_instance_unref (wr->e.gv->m_tkmap, tk);
  return res;
}

int write_sample_nogc_notk (struct thread_state1 * const ts1, struct nn_xpack *xp, struct writer *wr, struct ddsi_serdata *serdata)
{
  struct ddsi_tkmap_instance *tk;
  int res;
  assert (thread_is_awake ());
  tk = ddsi_tkmap_lookup_instance_ref (wr->e.gv->m_tkmap, serdata);
  res = write_sample_eot (ts1, xp, wr, NULL, serdata, tk, 0, 0);
  ddsi_tkmap_instance_unref (wr->e.gv->m_tkmap, tk);
  return res;
}
