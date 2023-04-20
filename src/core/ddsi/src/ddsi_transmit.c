// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <string.h>
#include <math.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "ddsi__entity.h"
#include "ddsi__participant.h"
#include "ddsi__entity_index.h"
#include "ddsi__addrset.h"
#include "ddsi__xmsg.h"
#include "ddsi__misc.h"
#include "ddsi__thread.h"
#include "ddsi__xevent.h"
#include "ddsi__transmit.h"
#include "ddsi__hbcontrol.h"
#include "ddsi__receive.h"
#include "ddsi__lease.h"
#include "ddsi__security_omg.h"
#include "ddsi__sysdeps.h"
#include "ddsi__endpoint.h"
#include "ddsi__endpoint_match.h"
#include "ddsi__protocol.h"
#include "ddsi__vendor.h"
#include "dds__whc.h"

static const struct ddsi_wr_prd_match *root_rdmatch (const struct ddsi_writer *wr)
{
  return ddsrt_avl_root (&ddsi_wr_readers_treedef, &wr->readers);
}

static int have_reliable_subs (const struct ddsi_writer *wr)
{
  if (ddsrt_avl_is_empty (&wr->readers) || root_rdmatch (wr)->min_seq == DDSI_MAX_SEQ_NUMBER)
    return 0;
  else
    return 1;
}

static dds_return_t ddsi_create_fragment_message_simple (struct ddsi_writer *wr, ddsi_seqno_t seq, struct ddsi_serdata *serdata, struct ddsi_xmsg **pmsg)
{
#define TEST_KEYHASH 0
  /* actual expected_inline_qos_size is typically 0, but always claiming 32 bytes won't make
     a difference, so no point in being precise */
  const size_t expected_inline_qos_size = /* statusinfo */ 8 + /* keyhash */ 20 + /* sentinel */ 4;
  struct ddsi_domaingv const * const gv = wr->e.gv;
  struct ddsi_xmsg_marker sm_marker;
  unsigned char contentflag = 0;
  ddsi_rtps_data_t *data;

  switch (serdata->kind)
  {
    case SDK_EMPTY:
      break;
    case SDK_KEY:
#if TEST_KEYHASH
      contentflag = wr->include_keyhash ? 0 : DDSI_DATA_FLAG_KEYFLAG;
#else
      contentflag = DDSI_DATA_FLAG_KEYFLAG;
#endif
      break;
    case SDK_DATA:
      contentflag = DDSI_DATA_FLAG_DATAFLAG;
      break;
  }

  ASSERT_MUTEX_HELD (&wr->e.lock);

  /* INFO_TS: 12 bytes, ddsi_rtps_data_t: 24 bytes, expected inline QoS: 32 => should be single chunk */
  if ((*pmsg = ddsi_xmsg_new (gv->xmsgpool, &wr->e.guid, wr->c.pp, sizeof (ddsi_rtps_info_ts_t) + sizeof (ddsi_rtps_data_t) + expected_inline_qos_size, DDSI_XMSG_KIND_DATA)) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;

  ddsi_xmsg_setdst_addrset (*pmsg, wr->as);
  ddsi_xmsg_setmaxdelay (*pmsg, wr->xqos->latency_budget.duration);
  ddsi_xmsg_add_timestamp (*pmsg, serdata->timestamp);
  data = ddsi_xmsg_append (*pmsg, &sm_marker, sizeof (ddsi_rtps_data_t));

  ddsi_xmsg_submsg_init (*pmsg, sm_marker, DDSI_RTPS_SMID_DATA);
  data->x.smhdr.flags = (unsigned char) (data->x.smhdr.flags | contentflag);
  data->x.extraFlags = 0;
  data->x.readerId = ddsi_to_entityid (DDSI_ENTITYID_UNKNOWN);
  data->x.writerId = ddsi_hton_entityid (wr->e.guid.entityid);
  data->x.writerSN = ddsi_to_seqno (seq);
  data->x.octetsToInlineQos = (unsigned short) ((char*) (data+1) - ((char*) &data->x.octetsToInlineQos + 2));

  if (wr->reliable)
    ddsi_xmsg_setwriterseq (*pmsg, &wr->e.guid, seq);

  /* Adding parameters means potential reallocing, so sm, ddcmn now likely become invalid */
  if (wr->num_readers_requesting_keyhash > 0)
    ddsi_xmsg_addpar_keyhash (*pmsg, serdata, wr->force_md5_keyhash);
  if (serdata->statusinfo)
    ddsi_xmsg_addpar_statusinfo (*pmsg, serdata->statusinfo);
  if (ddsi_xmsg_addpar_sentinel_ifparam (*pmsg) > 0)
  {
    data = ddsi_xmsg_submsg_from_marker (*pmsg, sm_marker);
    data->x.smhdr.flags |= DDSI_DATAFRAG_FLAG_INLINE_QOS;
  }

#if TEST_KEYHASH
  if (serdata->kind != SDK_KEY || !wr->include_keyhash)
    ddsi_xmsg_serdata (*pmsg, serdata, 0, ddsi_serdata_size (serdata), wr);
#else
  ddsi_xmsg_serdata (*pmsg, serdata, 0, ddsi_serdata_size (serdata), wr);
#endif
  ddsi_xmsg_submsg_setnext (*pmsg, sm_marker);
  return 0;
}

dds_return_t ddsi_create_fragment_message (struct ddsi_writer *wr, ddsi_seqno_t seq, struct ddsi_serdata *serdata, uint32_t fragnum, uint16_t nfrags, struct ddsi_proxy_reader *prd, struct ddsi_xmsg **pmsg, int isnew, uint32_t advertised_fragnum)
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
  struct ddsi_xmsg_marker sm_marker;
  void *sm;
  ddsi_rtps_data_datafrag_common_t *ddcmn;
  int fragging;
  uint32_t fragstart, fraglen;
  enum ddsi_xmsg_kind xmsg_kind = isnew ? DDSI_XMSG_KIND_DATA : DDSI_XMSG_KIND_DATA_REXMIT;
  const uint32_t size = ddsi_serdata_size (serdata);
  dds_return_t ret = 0;

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

  /* INFO_TS: 12 bytes, ddsi_rtps_datafrag_t: 36 bytes, expected inline QoS: 32 => should be single chunk */
  if ((*pmsg = ddsi_xmsg_new (gv->xmsgpool, &wr->e.guid, wr->c.pp, sizeof (ddsi_rtps_info_ts_t) + sizeof (ddsi_rtps_datafrag_t) + expected_inline_qos_size, xmsg_kind)) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;

  if (prd)
  {
    ddsi_xmsg_setdst_prd (*pmsg, prd);
    /* retransmits: latency budget doesn't apply */
  }
  else
  {
    ddsi_xmsg_setdst_addrset (*pmsg, wr->as);
    ddsi_xmsg_setmaxdelay (*pmsg, wr->xqos->latency_budget.duration);
  }

  /* Timestamp only needed once, for the first fragment */
  if (fragnum == 0)
  {
    ddsi_xmsg_add_timestamp (*pmsg, serdata->timestamp);
  }

  sm = ddsi_xmsg_append (*pmsg, &sm_marker, fragging ? sizeof (ddsi_rtps_datafrag_t) : sizeof (ddsi_rtps_data_t));
  ddcmn = sm;

  if (!fragging)
  {
    unsigned char contentflag = 0;
    ddsi_rtps_data_t *data = sm;
    switch (serdata->kind)
    {
      case SDK_EMPTY: contentflag = 0; break;
      case SDK_KEY:   contentflag = DDSI_DATA_FLAG_KEYFLAG; break;
      case SDK_DATA:  contentflag = DDSI_DATA_FLAG_DATAFLAG; break;
    }
    ddsi_xmsg_submsg_init (*pmsg, sm_marker, DDSI_RTPS_SMID_DATA);
    ddcmn->smhdr.flags = (unsigned char) (ddcmn->smhdr.flags | contentflag);

    fragstart = 0;
    fraglen = size;
    ddcmn->octetsToInlineQos = (unsigned short) ((char*) (data+1) - ((char*) &ddcmn->octetsToInlineQos + 2));

    if (wr->reliable)
      ddsi_xmsg_setwriterseq (*pmsg, &wr->e.guid, seq);
  }
  else
  {
    const unsigned char contentflag = (serdata->kind == SDK_KEY ? DDSI_DATAFRAG_FLAG_KEYFLAG : 0);
    ddsi_rtps_datafrag_t *frag = sm;
    /* empty means size = 0, which means it never needs fragmenting */
    assert (serdata->kind != SDK_EMPTY);
    ddsi_xmsg_submsg_init (*pmsg, sm_marker, DDSI_RTPS_SMID_DATA_FRAG);
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
      ddsi_xmsg_setwriterseq_fragid (*pmsg, &wr->e.guid, seq, isnew ? advertised_fragnum : fragnum + frag->fragmentsInSubmessage - 1);
    }
  }

  ddcmn->extraFlags = 0;
  ddcmn->readerId = ddsi_hton_entityid (prd ? prd->e.guid.entityid : ddsi_to_entityid (DDSI_ENTITYID_UNKNOWN));
  ddcmn->writerId = ddsi_hton_entityid (wr->e.guid.entityid);
  ddcmn->writerSN = ddsi_to_seqno (seq);

  if (xmsg_kind == DDSI_XMSG_KIND_DATA_REXMIT)
    ddsi_xmsg_set_data_reader_id (*pmsg, &ddcmn->readerId);

  DDSRT_STATIC_ASSERT_CODE (DDSI_DATA_FLAG_INLINE_QOS == DDSI_DATAFRAG_FLAG_INLINE_QOS);
  assert (!(ddcmn->smhdr.flags & DDSI_DATAFRAG_FLAG_INLINE_QOS));

  if (fragnum == 0)
  {
    int rc;
    /* Adding parameters means potential reallocing, so sm, ddcmn now likely become invalid */
    if (wr->num_readers_requesting_keyhash > 0)
    {
      ddsi_xmsg_addpar_keyhash (*pmsg, serdata, wr->force_md5_keyhash);
    }
    if (serdata->statusinfo)
    {
      ddsi_xmsg_addpar_statusinfo (*pmsg, serdata->statusinfo);
    }
    rc = ddsi_xmsg_addpar_sentinel_ifparam (*pmsg);
    if (rc > 0)
    {
      ddcmn = ddsi_xmsg_submsg_from_marker (*pmsg, sm_marker);
      ddcmn->smhdr.flags |= DDSI_DATAFRAG_FLAG_INLINE_QOS;
    }
  }

  ddsi_xmsg_serdata (*pmsg, serdata, fragstart, fraglen, wr);
  ddsi_xmsg_submsg_setnext (*pmsg, sm_marker);
#if 0
  GVTRACE ("queue data%s "PGUIDFMT" #%"PRId64"/%"PRIu32"[%"PRIu32"..%"PRIu32")\n",
           fragging ? "frag" : "", PGUID (wr->e.guid),
           seq, fragnum+1, fragstart, fragstart + fraglen);
#endif

  ddsi_security_encode_datawriter_submsg(*pmsg, sm_marker, wr);

  /* It is possible that the encoding removed the submessage.
   * If there is no content, free the message. */
  if (ddsi_xmsg_size(*pmsg) == 0) {
      ddsi_xmsg_free (*pmsg);
      *pmsg = NULL;
  }

  return ret;
}

static void create_HeartbeatFrag (struct ddsi_writer *wr, ddsi_seqno_t seq, unsigned fragnum, struct ddsi_proxy_reader *prd, struct ddsi_xmsg **pmsg)
{
  struct ddsi_domaingv const * const gv = wr->e.gv;
  struct ddsi_xmsg_marker sm_marker;
  ddsi_rtps_heartbeatfrag_t *hbf;
  ASSERT_MUTEX_HELD (&wr->e.lock);
  if ((*pmsg = ddsi_xmsg_new (gv->xmsgpool, &wr->e.guid, wr->c.pp, sizeof (ddsi_rtps_heartbeatfrag_t), DDSI_XMSG_KIND_CONTROL)) == NULL)
    return; /* ignore out-of-memory: HeartbeatFrag is only advisory anyway */
  if (prd)
    ddsi_xmsg_setdst_prd (*pmsg, prd);
  else
    ddsi_xmsg_setdst_addrset (*pmsg, wr->as);
  hbf = ddsi_xmsg_append (*pmsg, &sm_marker, sizeof (ddsi_rtps_heartbeatfrag_t));
  ddsi_xmsg_submsg_init (*pmsg, sm_marker, DDSI_RTPS_SMID_HEARTBEAT_FRAG);
  hbf->readerId = ddsi_hton_entityid (prd ? prd->e.guid.entityid : ddsi_to_entityid (DDSI_ENTITYID_UNKNOWN));
  hbf->writerId = ddsi_hton_entityid (wr->e.guid.entityid);
  hbf->writerSN = ddsi_to_seqno (seq);
  hbf->lastFragmentNum = fragnum + 1; /* network format is 1 based */

  hbf->count = wr->hbfragcount++;

  ddsi_xmsg_submsg_setnext (*pmsg, sm_marker);
  ddsi_security_encode_datawriter_submsg(*pmsg, sm_marker, wr);

  /* It is possible that the encoding removed the submessage.
   * If there is no content, free the message. */
  if (ddsi_xmsg_size(*pmsg) == 0)
  {
    ddsi_xmsg_free(*pmsg);
    *pmsg = NULL;
  }
}

dds_return_t ddsi_write_hb_liveliness (struct ddsi_domaingv * const gv, struct ddsi_guid *wr_guid, struct ddsi_xpack *xp)
{
  struct ddsi_xmsg *msg = NULL;
  struct ddsi_whc_state whcst;
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  struct ddsi_lease *lease;

  ddsi_thread_state_awake (thrst, gv);
  struct ddsi_writer *wr = ddsi_entidx_lookup_writer_guid (gv->entity_index, wr_guid);
  if (wr == NULL)
  {
    GVTRACE ("ddsi_write_hb_liveliness("PGUIDFMT") - writer not found\n", PGUID (*wr_guid));
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  }

  if (wr->xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_PARTICIPANT && ((lease = ddsrt_atomic_ldvoidp (&wr->c.pp->minl_man)) != NULL))
    ddsi_lease_renew (lease, ddsrt_time_elapsed());
  else if (wr->xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_TOPIC && wr->lease != NULL)
    ddsi_lease_renew (wr->lease, ddsrt_time_elapsed());

  if ((msg = ddsi_xmsg_new (gv->xmsgpool, &wr->e.guid, wr->c.pp, sizeof (ddsi_rtps_info_ts_t) + sizeof (ddsi_rtps_heartbeat_t), DDSI_XMSG_KIND_CONTROL)) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  ddsrt_mutex_lock (&wr->e.lock);
  ddsi_xmsg_setdst_addrset (msg, wr->as);
  ddsi_whc_get_state (wr->whc, &whcst);
  ddsi_add_heartbeat (msg, wr, &whcst, 0, 1, ddsi_to_entityid (DDSI_ENTITYID_UNKNOWN), 1);
  ddsrt_mutex_unlock (&wr->e.lock);
  ddsi_xpack_addmsg (xp, msg, 0);
  ddsi_xpack_send (xp, true);
  ddsi_thread_state_asleep (thrst);
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

static void transmit_sample_lgmsg_unlocks_wr (struct ddsi_xpack *xp, struct ddsi_writer *wr, ddsi_seqno_t seq, struct ddsi_serdata *serdata, struct ddsi_proxy_reader *prd, int isnew, uint32_t nfrags, uint32_t nfrags_lim)
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
    struct ddsi_xmsg *fmsg = NULL;
    struct ddsi_xmsg *hmsg = NULL;
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
    ret = ddsi_create_fragment_message (wr, seq, serdata, i, (uint16_t) nf_in_submsg, prd, &fmsg, isnew, i + nf_in_submsg == nfrags_lim ? nfrags - 1 : UINT32_MAX);
    if (ret >= 0 && i + nf_in_submsg < nfrags_lim && wr->heartbeat_xevent)
    {
      // more fragment messages to come
      create_HeartbeatFrag (wr, seq, i + nf_in_submsg - 1, prd, &hmsg);
    }
    ddsrt_mutex_unlock (&wr->e.lock);

    if(fmsg) ddsi_xpack_addmsg (xp, fmsg, 0);
    if(hmsg) ddsi_xpack_addmsg (xp, hmsg, 0);

    ddsrt_mutex_lock (&wr->e.lock);
  }
}

static void transmit_sample_unlocks_wr (struct ddsi_xpack *xp, struct ddsi_writer *wr, const struct ddsi_whc_state *whcst, ddsi_seqno_t seq, struct ddsi_serdata *serdata, struct ddsi_proxy_reader *prd, int isnew)
{
  /* on entry: &wr->e.lock held; on exit: lock no longer held */
  struct ddsi_domaingv const * const gv = wr->e.gv;
  struct ddsi_xmsg *hmsg = NULL;
  int hbansreq = 0;
  uint32_t sz;
  assert(xp);
  assert((wr->heartbeat_xevent != NULL) == (whcst != NULL));

  sz = ddsi_serdata_size (serdata);
  if (sz > gv->config.fragment_size || !isnew || prd != NULL || ddsi_omg_writer_is_submessage_protected (wr))
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

    transmit_sample_lgmsg_unlocks_wr (xp, wr, seq, serdata, prd, isnew, nfrags, nfrags_lim);
  }
  else
  {
    struct ddsi_xmsg *fmsg;
    if (ddsi_create_fragment_message_simple (wr, seq, serdata, &fmsg) >= 0)
      ddsi_xpack_addmsg (xp, fmsg, 0);
  }

  if (wr->heartbeat_xevent)
    hmsg = ddsi_writer_hbcontrol_piggyback (wr, whcst, serdata->twrite, ddsi_xpack_packetid (xp), &hbansreq);
  ddsrt_mutex_unlock (&wr->e.lock);

  if(hmsg)
    ddsi_xpack_addmsg (xp, hmsg, 0);
  if (hbansreq >= 2)
    ddsi_xpack_send (xp, true);
}

void ddsi_enqueue_spdp_sample_wrlock_held (struct ddsi_writer *wr, ddsi_seqno_t seq, struct ddsi_serdata *serdata, struct ddsi_proxy_reader *prd)
{
  assert (wr->e.guid.entityid.u == DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER);
  struct ddsi_xmsg *msg = NULL;
  if (ddsi_create_fragment_message(wr, seq, serdata, 0, UINT16_MAX, prd, &msg, 1, UINT32_MAX) >= 0)
    ddsi_qxev_msg (wr->evq, msg);
}

int ddsi_enqueue_sample_wrlock_held (struct ddsi_writer *wr, ddsi_seqno_t seq, struct ddsi_serdata *serdata, struct ddsi_proxy_reader *prd, int isnew)
{
  struct ddsi_domaingv const * const gv = wr->e.gv;
  uint32_t i, sz, nfrags;
  enum ddsi_qxev_msg_rexmit_result enqueued = DDSI_QXEV_MSG_REXMIT_QUEUED;

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
  for (i = 0; i < nfrags && enqueued != DDSI_QXEV_MSG_REXMIT_DROPPED; i++)
  {
    struct ddsi_xmsg *fmsg = NULL;
    struct ddsi_xmsg *hmsg = NULL;
    /* Ignore out-of-memory errors: we can't do anything about it, and
       eventually we'll have to retry.  But if a packet went out and
       we haven't yet completed transmitting a fragmented message, add
       a HeartbeatFrag. */
    if (ddsi_create_fragment_message (wr, seq, serdata, i, 1, prd, &fmsg, isnew, (i+1) == nfrags ? i : UINT32_MAX) >= 0)
    {
      if (nfrags > 1 && i + 1 < nfrags)
        create_HeartbeatFrag (wr, seq, i, prd, &hmsg);
    }
    if (isnew)
    {
      if(fmsg) ddsi_qxev_msg (wr->evq, fmsg);
      if(hmsg) ddsi_qxev_msg (wr->evq, hmsg);
    }
    else
    {
      /* Implementations that never use NACKFRAG are allowed by the specification, and for such a peer, we must always force out the full sample on a retransmit request. I am not aware of any such implementations so leaving the override flag in, but not actually using it at the moment. Should set force = (i != 0) for "known bad" implementations. */
      const int force = 0;
      if(fmsg)
      {
        enqueued = ddsi_qxev_msg_rexmit_wrlock_held (wr->evq, fmsg, force);
      }
      /* Functioning of the system is not dependent on getting the
         HeartbeatFrags out, so never force them into the queue. */
      if(hmsg)
      {
        switch (enqueued)
        {
          case DDSI_QXEV_MSG_REXMIT_DROPPED:
          case DDSI_QXEV_MSG_REXMIT_MERGED:
            ddsi_xmsg_free (hmsg);
            break;
          case DDSI_QXEV_MSG_REXMIT_QUEUED:
            ddsi_qxev_msg (wr->evq, hmsg);
            break;
        }
      }
    }
  }
  return (enqueued != DDSI_QXEV_MSG_REXMIT_DROPPED) ? 0 : -1;
}

static int insert_sample_in_whc (struct ddsi_writer *wr, ddsi_seqno_t seq, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk)
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
    if (wr->xqos->lifespan.duration != DDS_INFINITY && (serdata->statusinfo & (DDSI_STATUSINFO_UNREGISTER | DDSI_STATUSINFO_DISPOSE)) == 0)
      exp = ddsrt_mtime_add_duration(serdata->twrite, wr->xqos->lifespan.duration);
#endif
    res = ((insres = ddsi_whc_insert (wr->whc, ddsi_writer_max_drop_seq (wr), seq, exp, serdata, tk)) < 0) ? insres : 1;

#ifdef DDS_HAS_DEADLINE_MISSED
    if (!(wr->reliable && have_reliable_subs (wr)) && !wr->handle_as_transient_local)
    {
      /* Sample was inserted only because writer has deadline, so we'll remove the sample from whc */
      struct ddsi_whc_node *deferred_free_list = NULL;
      struct ddsi_whc_state whcst;
      uint32_t n = ddsi_whc_remove_acked_messages (wr->whc, seq, &whcst, &deferred_free_list);
      (void)n;
      assert (n <= 1);
      assert (whcst.min_seq == 0 && whcst.max_seq == 0);
      ddsi_whc_free_deferred_free_list (wr->whc, deferred_free_list);
    }
#endif
  }

#ifndef NDEBUG
  if (((wr->e.guid.entityid.u == DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER) ||
       (wr->e.guid.entityid.u == DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER)) &&
       !ddsi_is_local_orphan_endpoint (&wr->e))
  {
    struct ddsi_whc_state whcst;
    ddsi_whc_get_state(wr->whc, &whcst);
    if (DDSI_WHCST_ISEMPTY(&whcst))
      assert (wr->c.pp->state >= DDSI_PARTICIPANT_STATE_DELETING_BUILTINS);
  }
#endif
  return res;
}

static int writer_may_continue (const struct ddsi_writer *wr, const struct ddsi_whc_state *whcst)
{
  return (whcst->unacked_bytes <= wr->whc_low && !wr->retransmitting) || (wr->state != WRST_OPERATIONAL);
}

static dds_return_t throttle_writer (struct ddsi_thread_state * const thrst, struct ddsi_xpack *xp, struct ddsi_writer *wr)
{
  /* Sleep (cond_wait) without updating the thread's vtime: the
     garbage collector won't free the writer while we leave it
     unchanged.  Alternatively, we could decide to go back to sleep,
     allow garbage collection and check the writers existence every
     time we get woken up.  That would preclude the use of a condition
     variable embedded in "struct ddsi_writer", of course.

     For normal data that would be okay, because the thread forwarding
     data from the network queue to ddsi_write() simply uses the gid
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
  struct ddsi_whc_state whcst;
  ddsi_whc_get_state (wr->whc, &whcst);

  {
    ASSERT_MUTEX_HELD (&wr->e.lock);
    assert (wr->throttling == 0);
    assert (ddsi_thread_is_awake ());
    assert (!ddsi_is_builtin_entityid(wr->e.guid.entityid, DDSI_VENDORID_ECLIPSE));
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
    struct ddsi_xmsg *hbmsg = ddsi_writer_hbcontrol_create_heartbeat (wr, &whcst, tnow, 1, 1);
    ddsrt_mutex_unlock (&wr->e.lock);
    if (hbmsg)
    {
      ddsi_xpack_addmsg (xp, hbmsg, 0);
    }
    ddsi_xpack_send (xp, true);
    ddsrt_mutex_lock (&wr->e.lock);
    ddsi_whc_get_state (wr->whc, &whcst);
  }

  while (ddsrt_atomic_ld32 (&gv->rtps_keepgoing) && !writer_may_continue (wr, &whcst))
  {
    int64_t reltimeout;
    tnow = ddsrt_time_monotonic ();
    reltimeout = abstimeout.v - tnow.v;
    result = DDS_RETCODE_TIMEOUT;
    if (reltimeout > 0)
    {
      ddsi_thread_state_asleep (thrst);
      if (ddsrt_cond_waitfor (&wr->throttle_cond, &wr->e.lock, reltimeout))
        result = DDS_RETCODE_OK;
      ddsi_thread_state_awake_domain_ok (thrst);
      ddsi_whc_get_state(wr->whc, &whcst);
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

static int maybe_grow_whc (struct ddsi_writer *wr)
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

int ddsi_write_sample_p2p_wrlock_held(struct ddsi_writer *wr, ddsi_seqno_t seq, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk, struct ddsi_proxy_reader *prd)
{
  struct ddsi_domaingv * const gv = wr->e.gv;
  int r = 0;
  ddsrt_mtime_t tnow;
  int rexmit = 1;
  struct ddsi_wr_prd_match *wprd = NULL;
  ddsi_seqno_t gseq;
  struct ddsi_xmsg *gap = NULL;

  tnow = ddsrt_time_monotonic ();
  serdata->twrite = tnow;
  serdata->timestamp = ddsrt_time_wallclock ();


  if (prd->filter)
  {
    if ((wprd = ddsrt_avl_lookup (&ddsi_wr_readers_treedef, &wr->readers, &prd->e.guid)) != NULL)
    {
      if (wprd->seq == DDSI_MAX_SEQ_NUMBER)
        goto prd_is_deleting;

      rexmit = prd->filter(wr, prd, serdata);
      /* determine if gap has to added */
      if (rexmit)
      {
        struct ddsi_gap_info gi;

        GVLOG (DDS_LC_DISCOVERY, "send filtered "PGUIDFMT" last_seq=%"PRIu64" seq=%"PRIu64"\n", PGUID (wr->e.guid), wprd->seq, seq);

        ddsi_gap_info_init(&gi);
        for (gseq = wprd->seq + 1; gseq < seq; gseq++)
        {
          struct ddsi_whc_borrowed_sample sample;
          if (ddsi_whc_borrow_sample (wr->whc, seq, &sample))
          {
            if (prd->filter(wr, prd, sample.serdata) == 0)
            {
              ddsi_gap_info_update(wr->e.gv, &gi, gseq);
            }
            ddsi_whc_return_sample (wr->whc, &sample, false);
          }
        }
        gap = ddsi_gap_info_create_gap(wr, prd, &gi);
      }
      wprd->last_seq = seq;
    }
  }

  if ((r = insert_sample_in_whc (wr, seq, serdata, tk)) >= 0)
  {
    ddsi_enqueue_sample_wrlock_held (wr, seq, serdata, prd, 1);

    if (gap)
      ddsi_qxev_msg (wr->evq, gap);

    if (wr->heartbeat_xevent)
      ddsi_writer_hbcontrol_note_asyncwrite(wr, tnow);
  }
  else if (gap)
  {
    ddsi_xmsg_free (gap);
  }

prd_is_deleting:
  return r;
}

static int write_sample (struct ddsi_thread_state * const thrst, struct ddsi_xpack *xp, struct ddsi_writer *wr, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk, int gc_allowed)
{
  struct ddsi_domaingv const * const gv = wr->e.gv;
  int r;
  ddsi_seqno_t seq;
  ddsrt_mtime_t tnow;
  struct ddsi_lease *lease;

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
    ddsi_lease_renew (lease, ddsrt_time_elapsed());
  else if (wr->xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_TOPIC && wr->lease != NULL)
    ddsi_lease_renew (wr->lease, ddsrt_time_elapsed());

  ddsrt_mutex_lock (&wr->e.lock);

  if (!wr->alive)
    ddsi_writer_set_alive_may_unlock (wr, true);

  /* If WHC overfull, block. */
  {
    struct ddsi_whc_state whcst;
    ddsi_whc_get_state(wr->whc, &whcst);
    if (whcst.unacked_bytes > wr->whc_high)
    {
      dds_return_t ores;
      assert(gc_allowed); /* also see beginning of the function */
      if (gv->config.prioritize_retransmit && wr->retransmitting)
        ores = throttle_writer (thrst, xp, wr);
      else
      {
        maybe_grow_whc (wr);
        if (whcst.unacked_bytes <= wr->whc_high)
          ores = DDS_RETCODE_OK;
        else
          ores = throttle_writer (thrst, xp, wr);
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
  if ((r = insert_sample_in_whc (wr, seq, serdata, tk)) < 0)
  {
    /* Failure of some kind */
    ddsrt_mutex_unlock (&wr->e.lock);
  }
  else if (wr->test_drop_outgoing_data)
  {
    GVTRACE ("test_drop_outgoing_data");
    ddsi_writer_update_seq_xmit (wr, seq);
    ddsrt_mutex_unlock (&wr->e.lock);
  }
  else if (ddsi_addrset_empty (wr->as))
  {
    /* No network destination, so no point in doing all the work involved
       in going all the way.  We do have to record that we "transmitted"
       this sample, or it might not be retransmitted on request.

      (Note that no network destination is very nearly the same as no
      matching proxy readers.  The exception is the SPDP writer.) */
    ddsi_writer_update_seq_xmit (wr, seq);
    ddsrt_mutex_unlock (&wr->e.lock);
  }
  else
  {
    /* Note the subtlety of enqueueing with the lock held but
       transmitting without holding the lock. Still working on
       cleaning that up. */
    if (xp)
    {
      struct ddsi_whc_state whcst, *whcstptr;
      if (wr->heartbeat_xevent == NULL)
        whcstptr = NULL;
      else
      {
        ddsi_whc_get_state(wr->whc, &whcst);
        whcstptr = &whcst;
      }
      transmit_sample_unlocks_wr (xp, wr, whcstptr, seq, serdata, NULL, 1);
    }
    else
    {
      if (wr->heartbeat_xevent)
        ddsi_writer_hbcontrol_note_asyncwrite (wr, tnow);
      if (wr->e.guid.entityid.u == DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER)
        ddsi_enqueue_spdp_sample_wrlock_held(wr, seq, serdata, NULL);
      else
        ddsi_enqueue_sample_wrlock_held (wr, seq, serdata, NULL, 1);
      ddsrt_mutex_unlock (&wr->e.lock);
    }
  }

drop:
  /* FIXME: shouldn't I move the ddsi_serdata_unref call to the callers? */
  ddsi_serdata_unref (serdata);
  return r;
}

int ddsi_write_sample_gc (struct ddsi_thread_state * const thrst, struct ddsi_xpack *xp, struct ddsi_writer *wr, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk)
{
  return write_sample (thrst, xp, wr, serdata, tk, 1);
}

int ddsi_write_sample_nogc (struct ddsi_thread_state * const thrst, struct ddsi_xpack *xp, struct ddsi_writer *wr, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk)
{
  return write_sample (thrst, xp, wr, serdata, tk, 0);
}

int ddsi_write_sample_gc_notk (struct ddsi_thread_state * const thrst, struct ddsi_xpack *xp, struct ddsi_writer *wr, struct ddsi_serdata *serdata)
{
  struct ddsi_tkmap_instance *tk;
  int res;
  assert (ddsi_thread_is_awake ());
  tk = ddsi_tkmap_lookup_instance_ref (wr->e.gv->m_tkmap, serdata);
  res = write_sample (thrst, xp, wr, serdata, tk, 1);
  ddsi_tkmap_instance_unref (wr->e.gv->m_tkmap, tk);
  return res;
}

int ddsi_write_sample_nogc_notk (struct ddsi_thread_state * const thrst, struct ddsi_xpack *xp, struct ddsi_writer *wr, struct ddsi_serdata *serdata)
{
  struct ddsi_tkmap_instance *tk;
  int res;
  assert (ddsi_thread_is_awake ());
  tk = ddsi_tkmap_lookup_instance_ref (wr->e.gv->m_tkmap, serdata);
  res = write_sample (thrst, xp, wr, serdata, tk, 0);
  ddsi_tkmap_instance_unref (wr->e.gv->m_tkmap, tk);
  return res;
}

int ddsi_write_and_fini_plist (struct ddsi_writer *wr, ddsi_plist_t *ps, bool alive)
{
  struct ddsi_serdata *serdata = ddsi_serdata_from_sample (wr->type, alive ? SDK_DATA : SDK_KEY, ps);
  ddsi_plist_fini (ps);
  serdata->statusinfo = alive ? 0 : (DDSI_STATUSINFO_DISPOSE | DDSI_STATUSINFO_UNREGISTER);
  serdata->timestamp = ddsrt_time_wallclock ();
  return ddsi_write_sample_nogc_notk (ddsi_lookup_thread_state (), NULL, wr, serdata);
}
