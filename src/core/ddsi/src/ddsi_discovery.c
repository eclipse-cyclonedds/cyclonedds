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
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "dds/version.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_log.h"
#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_feature_check.h"
#include "ddsi__protocol.h"
#include "ddsi__misc.h"
#include "ddsi__xevent.h"
#include "ddsi__discovery.h"
#include "ddsi__discovery_addrset.h"
#include "ddsi__discovery_spdp.h"
#include "ddsi__discovery_endpoint.h"
#ifdef DDS_HAS_TOPIC_DISCOVERY
#include "ddsi__discovery_topic.h"
#endif
#include "ddsi__serdata_plist.h"
#include "ddsi__radmin.h"
#include "ddsi__entity_index.h"
#include "ddsi__entity.h"
#include "ddsi__participant.h"
#include "ddsi__xmsg.h"
#include "ddsi__transmit.h"
#include "ddsi__lease.h"
#include "ddsi__security_omg.h"
#include "ddsi__pmd.h"
#include "ddsi__endpoint.h"
#include "ddsi__plist.h"
#include "ddsi__proxy_endpoint.h"
#include "ddsi__proxy_participant.h"
#include "ddsi__topic.h"
#include "ddsi__tran.h"
#include "ddsi__typelib.h"
#include "ddsi__vendor.h"
#include "ddsi__xqos.h"
#include "ddsi__addrset.h"
#ifdef DDS_HAS_TYPE_DISCOVERY
#include "ddsi__typelookup.h"
#endif

#ifdef DDS_HAS_SECURITY
#include "ddsi__security_exchange.h"
#endif

struct ddsi_writer *ddsi_get_sedp_writer (const struct ddsi_participant *pp, unsigned entityid)
{
  struct ddsi_writer *sedp_wr;
  dds_return_t ret = ddsi_get_builtin_writer (pp, entityid, &sedp_wr);
  if (ret != DDS_RETCODE_OK)
    DDS_FATAL ("sedp_write_writer: no SEDP builtin writer %x for "PGUIDFMT"\n", entityid, PGUID (pp->e.guid));
  return sedp_wr;
}

bool ddsi_check_sedp_kind_and_guid (ddsi_sedp_kind_t sedp_kind, const ddsi_guid_t *entity_guid)
{
  switch (sedp_kind)
  {
    case SEDP_KIND_TOPIC:
      return ddsi_is_topic_entityid (entity_guid->entityid);
    case SEDP_KIND_WRITER:
      return ddsi_is_writer_entityid (entity_guid->entityid);
    case SEDP_KIND_READER:
      return ddsi_is_reader_entityid (entity_guid->entityid);
  }
  assert (0);
  return false;
}

bool ddsi_handle_sedp_checks (struct ddsi_domaingv * const gv, ddsi_sedp_kind_t sedp_kind, ddsi_guid_t *entity_guid, ddsi_plist_t *datap, ddsi_vendorid_t vendorid, struct ddsi_proxy_participant **proxypp, ddsi_guid_t *ppguid)
{
#define E(msg, lbl) do { GVLOGDISC (msg); return false; } while (0)
  if (!ddsi_check_sedp_kind_and_guid (sedp_kind, entity_guid))
    E (" SEDP topic/GUID entity kind mismatch\n", err);
  ppguid->prefix = entity_guid->prefix;
  ppguid->entityid.u = DDSI_ENTITYID_PARTICIPANT;
  // Accept the presence of a participant GUID, but only if it matches
  if ((datap->present & PP_PARTICIPANT_GUID) && memcmp (&datap->participant_guid, ppguid, sizeof (*ppguid)) != 0)
    E (" endpoint/participant GUID mismatch", err);
  if (ddsi_is_deleted_participant_guid (gv->deleted_participants, ppguid))
    E (" local dead pp?\n", err);
  if (ddsi_entidx_lookup_participant_guid (gv->entity_index, ppguid) != NULL)
    E (" local pp?\n", err);
  if (ddsi_is_builtin_entityid (entity_guid->entityid, vendorid))
    E (" built-in\n", err);
  if (!(datap->qos.present & DDSI_QP_TOPIC_NAME))
    E (" no topic?\n", err);
  if (!(datap->qos.present & DDSI_QP_TYPE_NAME))
    E (" no typename?\n", err);
  if ((*proxypp = ddsi_entidx_lookup_proxy_participant_guid (gv->entity_index, ppguid)) == NULL)
    E (" unknown-proxypp", err);
  return true;
#undef E
}

static void ddsi_handle_sedp (const struct ddsi_receiver_state *rst, ddsi_seqno_t seq, struct ddsi_serdata *serdata, ddsi_sedp_kind_t sedp_kind)
{
  ddsi_plist_t decoded_data;
  if (ddsi_serdata_to_sample (serdata, &decoded_data, NULL, NULL))
  {
    struct ddsi_domaingv * const gv = rst->gv;
    GVLOGDISC ("SEDP ST%"PRIx32, serdata->statusinfo);
    switch (serdata->statusinfo & (DDSI_STATUSINFO_DISPOSE | DDSI_STATUSINFO_UNREGISTER))
    {
      case 0:
        switch (sedp_kind)
        {
          case SEDP_KIND_TOPIC:
#ifdef DDS_HAS_TOPIC_DISCOVERY
            ddsi_handle_sedp_alive_topic (rst, seq, &decoded_data, rst->vendor, serdata->timestamp);
#endif
            break;
          case SEDP_KIND_READER:
          case SEDP_KIND_WRITER:
            ddsi_handle_sedp_alive_endpoint (rst, seq, &decoded_data, sedp_kind, rst->vendor, serdata->timestamp);
            break;
        }
        break;
      case DDSI_STATUSINFO_DISPOSE:
      case DDSI_STATUSINFO_UNREGISTER:
      case (DDSI_STATUSINFO_DISPOSE | DDSI_STATUSINFO_UNREGISTER):
        switch (sedp_kind)
        {
          case SEDP_KIND_TOPIC:
#ifdef DDS_HAS_TOPIC_DISCOVERY
            ddsi_handle_sedp_dead_topic (rst, &decoded_data, serdata->timestamp);
#endif
            break;
          case SEDP_KIND_READER:
          case SEDP_KIND_WRITER:
            ddsi_handle_sedp_dead_endpoint (rst, &decoded_data, sedp_kind, serdata->timestamp);
            break;
        }
        break;
    }
    ddsi_plist_fini (&decoded_data);
  }
}

#ifdef DDS_HAS_TYPE_DISCOVERY
static void handle_typelookup (const struct ddsi_receiver_state *rst, ddsi_entityid_t wr_entity_id, struct ddsi_serdata *serdata)
{
  if (!(serdata->statusinfo & (DDSI_STATUSINFO_DISPOSE | DDSI_STATUSINFO_UNREGISTER)))
  {
    struct ddsi_domaingv * const gv = rst->gv;
    if (wr_entity_id.u == DDSI_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER)
      ddsi_tl_handle_request (gv, serdata);
    else if (wr_entity_id.u == DDSI_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER)
      ddsi_tl_handle_reply (gv, serdata);
    else
      assert (false);
  }
}
#endif

int ddsi_builtins_dqueue_handler (const struct ddsi_rsample_info *sampleinfo, const struct ddsi_rdata *fragchain, UNUSED_ARG (const ddsi_guid_t *rdguid), UNUSED_ARG (void *qarg))
{
  struct ddsi_domaingv * const gv = sampleinfo->rst->gv;
  struct ddsi_proxy_writer *pwr;
  unsigned statusinfo;
  int need_keyhash;
  ddsi_guid_t srcguid;
  ddsi_rtps_data_datafrag_common_t *msg;
  unsigned char data_smhdr_flags;
  ddsi_plist_t qos;

  /* Luckily, most of the Data and DataFrag headers are the same - and
     in particular, all that we care about here is the same.  The
     key/data flags of DataFrag are different from those of Data, but
     DDSI used to treat them all as if they are data :( so now,
     instead of splitting out all the code, we reformat these flags
     from the submsg to always conform to that of the "Data"
     submessage regardless of the input. */
  msg = (ddsi_rtps_data_datafrag_common_t *) DDSI_RMSG_PAYLOADOFF (fragchain->rmsg, DDSI_RDATA_SUBMSG_OFF (fragchain));
  data_smhdr_flags = ddsi_normalize_data_datafrag_flags (&msg->smhdr);
  srcguid.prefix = sampleinfo->rst->src_guid_prefix;
  srcguid.entityid = msg->writerId;

  pwr = sampleinfo->pwr;
  if (pwr == NULL)
  {
    /* NULL with DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER is normal. It is possible that
     * DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER has NULL as well if there
     * is a security mismatch being handled. */
    assert ((srcguid.entityid.u == DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER) ||
            (srcguid.entityid.u == DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER));
  }
  else
  {
    assert (ddsi_is_builtin_entityid (pwr->e.guid.entityid, pwr->c.vendor));
    assert (memcmp (&pwr->e.guid, &srcguid, sizeof (srcguid)) == 0);
    assert (srcguid.entityid.u != DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER);
  }

  /* If there is no payload, it is either a completely invalid message
     or a dispose/unregister in RTI style. We assume the latter,
     consequently expect to need the keyhash.  Then, if sampleinfo
     says it is a complex qos, or the keyhash is required, extract all
     we need from the inline qos. */
  need_keyhash = (sampleinfo->size == 0 || (data_smhdr_flags & (DDSI_DATA_FLAG_KEYFLAG | DDSI_DATA_FLAG_DATAFLAG)) == 0);
  if (!(sampleinfo->complex_qos || need_keyhash))
  {
    ddsi_plist_init_empty (&qos);
    statusinfo = sampleinfo->statusinfo;
  }
  else
  {
    ddsi_plist_src_t src;
    size_t qos_offset = DDSI_RDATA_SUBMSG_OFF (fragchain) + offsetof (ddsi_rtps_data_datafrag_common_t, octetsToInlineQos) + sizeof (msg->octetsToInlineQos) + msg->octetsToInlineQos;
    dds_return_t plist_ret;
    src.protocol_version = sampleinfo->rst->protocol_version;
    src.vendorid = sampleinfo->rst->vendor;
    src.encoding = (msg->smhdr.flags & DDSI_RTPS_SUBMESSAGE_FLAG_ENDIANNESS) ? DDSI_RTPS_PL_CDR_LE : DDSI_RTPS_PL_CDR_BE;
    src.buf = DDSI_RMSG_PAYLOADOFF (fragchain->rmsg, qos_offset);
    src.bufsz = DDSI_RDATA_PAYLOAD_OFF (fragchain) - qos_offset;
    src.strict = DDSI_SC_STRICT_P (gv->config);
    if ((plist_ret = ddsi_plist_init_frommsg (&qos, NULL, PP_STATUSINFO | PP_KEYHASH, 0, &src, gv, DDSI_PLIST_CONTEXT_INLINE_QOS)) < 0)
    {
      if (plist_ret != DDS_RETCODE_UNSUPPORTED)
        GVWARNING ("data(builtin, vendor %u.%u): "PGUIDFMT" #%"PRIu64": invalid inline qos\n",
                   src.vendorid.id[0], src.vendorid.id[1], PGUID (srcguid), sampleinfo->seq);
      goto done_upd_deliv;
    }
    /* Complex qos bit also gets set when statusinfo bits other than
       dispose/unregister are set.  They are not currently defined,
       but this may save us if they do get defined one day. */
    statusinfo = (qos.present & PP_STATUSINFO) ? qos.statusinfo : 0;
  }

  if (pwr && ddsrt_avl_is_empty (&pwr->readers))
  {
    /* Wasn't empty when enqueued, but needn't still be; SPDP has no
       proxy writer, and is always accepted */
    goto done_upd_deliv;
  }

  /* proxy writers don't reference a type object, SPDP doesn't have matched readers
     but all the GUIDs are known, so be practical and map that */
  const struct ddsi_sertype *type;
  switch (srcguid.entityid.u)
  {
    case DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER:
      type = gv->spdp_type;
      break;
    case DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER:
      type = gv->sedp_writer_type;
      break;
    case DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER:
      type = gv->sedp_reader_type;
      break;
#ifdef DDS_HAS_TOPIC_DISCOVERY
    case DDSI_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER:
      type = gv->sedp_topic_type;
      break;
#endif
    case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER:
      type = gv->pmd_type;
      break;
#ifdef DDS_HAS_TYPE_DISCOVERY
    case DDSI_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER:
      type = gv->tl_svc_request_type;
      break;
    case DDSI_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER:
      type = gv->tl_svc_reply_type;
      break;
#endif
#ifdef DDS_HAS_SECURITY
    case DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER:
      type = gv->spdp_secure_type;
      break;
    case DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER:
      type = gv->sedp_writer_secure_type;
      break;
    case DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER:
      type = gv->sedp_reader_secure_type;
      break;
    case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER:
      type = gv->pmd_secure_type;
      break;
    case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER:
      type = gv->pgm_stateless_type;
      break;
    case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER:
      type = gv->pgm_volatile_type;
      break;
#endif
    default:
      type = NULL;
      break;
  }
  if (type == NULL)
  {
    /* unrecognized source entity id => ignore */
    goto done_upd_deliv;
  }

  struct ddsi_serdata *d;
  if (data_smhdr_flags & DDSI_DATA_FLAG_DATAFLAG)
    d = ddsi_serdata_from_ser (type, SDK_DATA, fragchain, sampleinfo->size);
  else if (data_smhdr_flags & DDSI_DATA_FLAG_KEYFLAG)
    d = ddsi_serdata_from_ser (type, SDK_KEY, fragchain, sampleinfo->size);
  else if ((qos.present & PP_KEYHASH) && !DDSI_SC_STRICT_P(gv->config))
    d = ddsi_serdata_from_keyhash (type, &qos.keyhash);
  else
  {
    GVLOGDISC ("data(builtin, vendor %u.%u): "PGUIDFMT" #%"PRIu64": missing payload\n",
               sampleinfo->rst->vendor.id[0], sampleinfo->rst->vendor.id[1],
               PGUID (srcguid), sampleinfo->seq);
    goto done_upd_deliv;
  }
  if (d == NULL)
  {
    GVLOG (DDS_LC_DISCOVERY | DDS_LC_WARNING, "data(builtin, vendor %u.%u): "PGUIDFMT" #%"PRIu64": deserialization failed\n",
           sampleinfo->rst->vendor.id[0], sampleinfo->rst->vendor.id[1],
           PGUID (srcguid), sampleinfo->seq);
    goto done_upd_deliv;
  }

  d->timestamp = (sampleinfo->timestamp.v != DDSRT_WCTIME_INVALID.v) ? sampleinfo->timestamp : ddsrt_time_wallclock ();
  d->statusinfo = statusinfo;
  // set protocol version & vendor id for plist types
  // FIXME: find a better way then fixing these up afterward
  if (d->ops == &ddsi_serdata_ops_plist)
  {
    struct ddsi_serdata_plist *d_plist = (struct ddsi_serdata_plist *) d;
    d_plist->protoversion = sampleinfo->rst->protocol_version;
    d_plist->vendorid = sampleinfo->rst->vendor;
  }

  if (gv->logconfig.c.mask & DDS_LC_TRACE)
  {
    ddsi_guid_t guid;
    char tmp[2048];
    size_t res = 0;
    tmp[0] = 0;
    if (gv->logconfig.c.mask & DDS_LC_CONTENT)
      res = ddsi_serdata_print (d, tmp, sizeof (tmp));
    if (pwr) guid = pwr->e.guid; else memset (&guid, 0, sizeof (guid));
    GVTRACE ("data(builtin, vendor %u.%u): "PGUIDFMT" #%"PRIu64": ST%x %s/%s:%s%s\n",
             sampleinfo->rst->vendor.id[0], sampleinfo->rst->vendor.id[1],
             PGUID (guid), sampleinfo->seq, statusinfo,
             pwr ? pwr->c.xqos->topic_name : "", d->type->type_name,
             tmp, res < sizeof (tmp) - 1 ? "" : "(trunc)");
  }

  switch (srcguid.entityid.u)
  {
    case DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER:
    case DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER:
      ddsi_handle_spdp (sampleinfo->rst, srcguid.entityid, sampleinfo->seq, d);
      break;
    case DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER:
    case DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER:
      ddsi_handle_sedp (sampleinfo->rst, sampleinfo->seq, d, SEDP_KIND_WRITER);
      break;
    case DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER:
    case DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER:
      ddsi_handle_sedp (sampleinfo->rst, sampleinfo->seq, d, SEDP_KIND_READER);
      break;
#ifdef DDS_HAS_TOPIC_DISCOVERY
    case DDSI_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER:
      ddsi_handle_sedp (sampleinfo->rst, sampleinfo->seq, d, SEDP_KIND_TOPIC);
      break;
#endif
    case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER:
    case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER:
      ddsi_handle_pmd_message (sampleinfo->rst, d);
      break;
#ifdef DDS_HAS_TYPE_DISCOVERY
    case DDSI_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER:
    case DDSI_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER:
      handle_typelookup (sampleinfo->rst, srcguid.entityid, d);
      break;
#endif
#ifdef DDS_HAS_SECURITY
    case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER:
      ddsi_handle_auth_handshake_message(sampleinfo->rst, srcguid.entityid, d);
      break;
    case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER:
      ddsi_handle_crypto_exchange_message(sampleinfo->rst, d);
      break;
#endif
    default:
      GVLOGDISC ("data(builtin, vendor %u.%u): "PGUIDFMT" #%"PRIu64": not handled\n",
                 sampleinfo->rst->vendor.id[0], sampleinfo->rst->vendor.id[1],
                 PGUID (srcguid), sampleinfo->seq);
      break;
  }

  ddsi_serdata_unref (d);

 done_upd_deliv:
  if (pwr)
  {
    /* No proxy writer for SPDP */
    ddsrt_atomic_st32 (&pwr->next_deliv_seq_lowword, (uint32_t) (sampleinfo->seq + 1));
  }
  return 0;
}
