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
#ifdef DDSI_INCLUDE_SECURITY

#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/md5.h"

#include "dds/ddsi/q_globals.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_security_exchange.h"
#include "dds/ddsi/ddsi_serdata_default.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsi/q_ddsi_discovery.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsi/q_transmit.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/q_bswap.h"

#ifdef HANDSHAKE_IMPLEMENTED

static void nn_property_seq_log(struct q_globals *gv, const dds_propertyseq_t *seq)
{
  uint32_t i;

  GVTRACE("{");
  for (i = 0; i < seq->n; i++) {
    GVTRACE("n=%s,v=%s", seq->props[i].name, seq->props[i].value);
  }
  GVTRACE("}");
}

static void nn_binary_property_seq_log(struct q_globals *gv, const dds_binarypropertyseq_t *seq)
{
  uint32_t i;

  GVTRACE("{");
  for (i = 0; i < seq->n; i++) {
    uint32_t j;
    GVTRACE("n=%s,v={", seq->props[i].name);
    for (j = 0; j < seq->props[i].value.length; j++) {
      GVTRACE("%02x", seq->props[i].value.value[j]);
    }
    GVTRACE("}");
  }
  GVTRACE("}");
}

static void nn_dataholder_seq_log(struct q_globals *gv, const char *prefix, const nn_dataholderseq_t *dhseq)
{
  uint32_t i;

  GVTRACE("%s={", prefix);
  for (i = 0; i < dhseq->n; i++) {
    GVTRACE("cid=%s,", dhseq->tags[i].class_id);
    nn_property_seq_log(gv, &dhseq->tags[i].properties);
    GVTRACE(",");
    nn_binary_property_seq_log(gv, &dhseq->tags[i].binary_properties);
  }
  GVTRACE("}");
}

static void nn_participant_generic_message_log(struct q_globals *gv, const struct nn_participant_generic_message *msg, int conv)
{
  ddsi_guid_t spguid = conv ? nn_ntoh_guid(msg->message_identity.source_guid ): msg->message_identity.source_guid;
  ddsi_guid_t rmguid = conv ? nn_ntoh_guid(msg->related_message_identity.source_guid) : msg->related_message_identity.source_guid;
  ddsi_guid_t dpguid = conv ? nn_ntoh_guid(msg->destination_participant_guid) : msg->destination_participant_guid;
  ddsi_guid_t deguid = conv ? nn_ntoh_guid(msg->destination_endpoint_guid) : msg->destination_endpoint_guid;
  ddsi_guid_t seguid = conv ? nn_ntoh_guid(msg->source_endpoint_guid) : msg->source_endpoint_guid;

  GVTRACE("mi=" PGUIDFMT "#%" PRIdSIZE ",", PGUID(spguid), msg->message_identity.sequence_number);
  GVTRACE("rmi=" PGUIDFMT "#%" PRIdSIZE ",", PGUID(rmguid), msg->related_message_identity.sequence_number);
  GVTRACE("dpg=" PGUIDFMT ",", PGUID(dpguid));
  GVTRACE("deg=" PGUIDFMT ",", PGUID(deguid));
  GVTRACE("seg=" PGUIDFMT ",", PGUID(seguid));
  GVTRACE("cid=%s,", msg->message_class_id);
  nn_dataholder_seq_log(gv, "mdata", &msg->message_data);
}
#endif

bool write_auth_handshake_message(const struct participant *pp, const struct proxy_participant *proxypp, const nn_dataholder_t *hmsg, bool request, const nn_message_identity_t *related_message_id)
{
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);
  DDSRT_UNUSED_ARG(hmsg);
  DDSRT_UNUSED_ARG(request);
  DDSRT_UNUSED_ARG(related_message_id);

#ifdef HANDSHAKE_IMPLEMENTED
  struct q_globals *gv = pp->e.gv;
  const nn_dataholderseq_t mdata = {1, (nn_dataholder_t *)hmsg};
  struct nn_participant_generic_message pmg;
  struct ddsi_serdata *serdata;
  unsigned char *blob;
  size_t len;
  struct writer *wr;
  int64_t seq;
  struct proxy_reader *prd;
  ddsi_guid_t prd_guid;
  bool result = false;

  if ((wr = get_builtin_writer (pp, NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER)) == NULL) {
    GVTRACE ("write_handshake("PGUIDFMT") - builtin stateless message writer not found", PGUID (pp->e.guid));
    return false;
  }

  prd_guid.prefix = proxypp->e.guid.prefix;
  prd_guid.entityid.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER;
  if ((prd = entidx_lookup_proxy_reader_guid (gv->entity_index, &prd_guid)) == NULL) {
    GVTRACE ("write_handshake("PGUIDFMT") - builtin stateless message proxy reader not found", PGUID (prd_guid));
    return false;
  }

  ddsrt_mutex_lock (&wr->e.lock);
  seq = ++wr->seq;

  if (request) {
    nn_participant_generic_message_init(&pmg, &wr->e.guid, seq, &proxypp->e.guid, NULL, NULL, GMCLASSID_SECURITY_AUTH_HANDSHAKE, &mdata, NULL);
  } else {
    nn_participant_generic_message_init(&pmg, &wr->e.guid, seq, &proxypp->e.guid, NULL, NULL, GMCLASSID_SECURITY_AUTH_HANDSHAKE, &mdata, related_message_id);
  }

  if (nn_participant_generic_message_serialize(&pmg, &blob, &len) == DDS_RETCODE_OK)
  {
    GVTRACE("write_handshake("PGUIDFMT" --> "PGUIDFMT")(lguid="PGUIDFMT" rguid="PGUIDFMT") ",
        PGUID (wr->e.guid), PGUID (prd_guid),
        PGUID (pp->e.guid), PGUID (proxypp->e.guid));
    nn_participant_generic_message_log(gv, &pmg, 1);

    struct ddsi_rawcdr_sample raw = {
        .blob = blob,
        .size = len,
        .key = NULL,
        .keysize = 0
    };
    serdata = ddsi_serdata_from_sample (gv->rawcdr_topic, SDK_DATA, &raw);
    serdata->timestamp = now ();

    result = enqueue_sample_wrlock_held (wr, seq, NULL, serdata, prd, 1) == 0;
  }
  else
    GVERROR("Failed to serialize handshake message");

  ddsrt_mutex_unlock (&wr->e.lock);
  nn_participant_generic_message_deinit(&pmg);

  return result;
#else
  return true;
#endif

}

void auth_get_serialized_participant_data(struct participant *pp, ddsi_octetseq_t *seq)
{
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(seq);

#ifdef HANDSHAKE_IMPLEMENTED
  struct nn_xmsg *mpayload;
  size_t sz;
  char *payload;

  mpayload = nn_xmsg_new (pp->e.gv->xmsgpool, &pp->e.guid, pp, 0, NN_XMSG_KIND_DATA);

  get_participant_builtin_topic_data(pp, mpayload);
  payload = nn_xmsg_payload (&sz, mpayload);

  seq->length = (uint32_t)sz;
  seq->value = ddsrt_malloc(sz);
  memcpy(seq->value, payload, sz);
  nn_xmsg_free (mpayload);
#endif
}

void handle_auth_handshake_message(const struct receiver_state *rst, ddsi_entityid_t wr_entity_id, nn_wctime_t timestamp, unsigned statusinfo, const void *vdata, size_t len)
{
  DDSRT_UNUSED_ARG(rst);
  DDSRT_UNUSED_ARG(wr_entity_id);
  DDSRT_UNUSED_ARG(timestamp);
  DDSRT_UNUSED_ARG(statusinfo);
  DDSRT_UNUSED_ARG(vdata);
  DDSRT_UNUSED_ARG(len);

#ifdef HANDSHAKE_IMPLEMENTED
  const struct CDRHeader *hdr = vdata; /* built-ins not deserialized (yet) */
  const bool bswap = (hdr->identifier == CDR_LE) ^ DDSRT_LITTLE_ENDIAN;
  const void *data = (void *) (hdr + 1);
  size_t size = (len - sizeof(struct CDRHeader));
  struct nn_participant_generic_message msg;
  struct participant *pp = NULL;
  struct proxy_writer *pwr = NULL;
  ddsi_guid_t guid;
  ddsi_guid_t *pwr_guid;
  struct ddsi_handshake *handshake;

  DDSRT_UNUSED_ARG(wr_entity_id);
  DDSRT_UNUSED_ARG(timestamp);

  RSTTRACE ("recv_handshake ST%x", statusinfo);
  if ((hdr->identifier != CDR_LE) && (hdr->identifier != PL_CDR_LE) &&
      (hdr->identifier != CDR_BE) && (hdr->identifier != PL_CDR_BE))
  {
    RSTTRACE (" data->identifier %d !?\n", ntohs (hdr->identifier));
    return;
  }

  if (nn_participant_generic_message_deseralize(&msg, data, size, bswap) < 0)
  {
    RSTTRACE (" deserialize failed\n");
    goto err_deser;
  }

  RSTTRACE ("msg=");
  nn_participant_generic_message_log(rst->gv, &msg, 0);
  RSTTRACE ("\n");

  if (msg.message_identity.source_guid.entityid.u == NN_ENTITYID_PARTICIPANT)
  {
    guid = msg.message_identity.source_guid;
    guid.entityid.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER;
    pwr_guid= &guid;
  }
  else if (msg.message_identity.source_guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER)
  {
    pwr_guid= &msg.message_identity.source_guid;
  }
  else
  {
    RSTTRACE (" invalid source entity id\n");
    goto invalid_source;
  }

  if ((pp = entidx_lookup_participant_guid(rst->gv->entity_index, &msg.destination_participant_guid)) == NULL)
  {
    RSTTRACE (" destination participant ("PGUIDFMT") not found\n", PGUID(msg.destination_participant_guid));
  }
  else if ((pwr = entidx_lookup_proxy_writer_guid(rst->gv->entity_index, pwr_guid)) == NULL)
  {
    RSTTRACE (" proxy writer ("PGUIDFMT") not found\n", PGUID(*pwr_guid));
  }
  else if ((handshake = ddsi_handshake_find(pp, pwr->c.proxypp)) == NULL)
  {
    RSTTRACE (" handshake not found ("PGUIDFMT" --> "PGUIDFMT")\n", PGUID (pwr->c.proxypp->e.guid), PGUID(pp->e.guid));
  }
  else
  {
    RSTTRACE (" ("PGUIDFMT" --> "PGUIDFMT")\n", PGUID (pwr->c.proxypp->e.guid), PGUID (pp->e.guid));
    ddsi_handshake_handle_message(handshake, pp, pwr->c.proxypp, &msg);
    ddsi_handshake_release(handshake);
  }

invalid_source:
  nn_participant_generic_message_deinit(&msg);
err_deser:
  return;
#endif
}

static bool write_crypto_exchange_message(const struct participant *pp, const ddsi_guid_t *dst_pguid, const ddsi_guid_t *src_eguid, const ddsi_guid_t *dst_eguid, const char *classid, const nn_dataholderseq_t *tokens)
{
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(dst_pguid);
  DDSRT_UNUSED_ARG(src_eguid);
  DDSRT_UNUSED_ARG(dst_eguid);
  DDSRT_UNUSED_ARG(classid);
  DDSRT_UNUSED_ARG(tokens);

#ifdef HANDSHAKE_IMPLEMENTED
  struct q_globals * const gv = pp->e.gv;
  struct nn_participant_generic_message pmg;
  struct ddsi_tkmap_instance *tk;
  struct ddsi_serdata *serdata;
  struct proxy_reader *prd;
  ddsi_guid_t prd_guid;
  unsigned char *data;
  size_t len;
  struct writer *wr;
  seqno_t seq;
  int r;

  if ((wr = get_builtin_writer (pp, NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER)) == NULL)
  {
    GVLOG (DDS_LC_DISCOVERY, "write_crypto_exchange_message("PGUIDFMT") - builtin volatile secure writer not found\n", PGUID (pp->e.guid));
    return false;
  }

  prd_guid.prefix = dst_pguid->prefix;
  prd_guid.entityid.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER;
  if ((prd = entidx_lookup_proxy_reader_guid (gv->entity_index, &prd_guid)) == NULL)
    return false;

  GVLOG (DDS_LC_DISCOVERY, "send crypto tokens("PGUIDFMT" --> "PGUIDFMT")\n", PGUID (wr->e.guid), PGUID (prd_guid));

  ddsrt_mutex_lock (&wr->e.lock);
  seq = ++wr->seq;

  /* Get serialized message. */
  nn_participant_generic_message_init(&pmg, &wr->e.guid, seq, dst_pguid, dst_eguid, src_eguid, classid, tokens, NULL);
  nn_participant_generic_message_serialize(&pmg, &data, &len);

  /* Get the key value. */
  ddsrt_md5_state_t md5st;
  ddsrt_md5_byte_t digest[16];
  ddsrt_md5_init (&md5st);
  ddsrt_md5_append (&md5st, (const ddsrt_md5_byte_t *)data, sizeof (nn_message_identity_t));
  ddsrt_md5_finish (&md5st, digest);

  /* Write the sample. */
  struct ddsi_rawcdr_sample raw = {
    .blob = data,
    .size = len,
    .key = digest,
    .keysize = 16
  };
  serdata = ddsi_serdata_from_sample (gv->rawcdr_topic, SDK_DATA, &raw);
  tk = ddsi_tkmap_lookup_instance_ref (gv->m_tkmap, serdata);
  r = write_sample_p2p_wrlock_held(wr, seq, NULL, serdata, tk, prd);
  ddsi_tkmap_instance_unref (gv->m_tkmap, tk);
  ddsi_serdata_unref (serdata);

  nn_participant_generic_message_deinit(&pmg);

  ddsrt_mutex_unlock (&wr->e.lock);

  return (r < 0 ? false : true);
#else
  return true;
#endif
}

bool write_crypto_participant_tokens(const struct participant *pp, const struct proxy_participant *proxypp, const nn_dataholderseq_t *tokens)
{
  return write_crypto_exchange_message(pp, &proxypp->e.guid, NULL, NULL, GMCLASSID_SECURITY_PARTICIPANT_CRYPTO_TOKENS, tokens);
}

bool write_crypto_writer_tokens(const struct writer *wr, const struct proxy_reader *prd, const nn_dataholderseq_t *tokens)
{
  struct participant *pp = wr->c.pp;
  struct proxy_participant *proxypp = prd->c.proxypp;

  return write_crypto_exchange_message(pp, &proxypp->e.guid, &wr->e.guid, &prd->e.guid, GMCLASSID_SECURITY_DATAWRITER_CRYPTO_TOKENS, tokens);
}

bool write_crypto_reader_tokens(const struct reader *rd, const struct proxy_writer *pwr, const nn_dataholderseq_t *tokens)
{
  struct participant *pp = rd->c.pp;
  struct proxy_participant *proxypp = pwr->c.proxypp;

  return write_crypto_exchange_message(pp, &proxypp->e.guid, &rd->e.guid, &pwr->e.guid, GMCLASSID_SECURITY_DATAREADER_CRYPTO_TOKENS, tokens);
}

void handle_crypto_exchange_message(const struct receiver_state *rst, ddsi_entityid_t wr_entity_id, nn_wctime_t timestamp, unsigned statusinfo, const void *vdata, unsigned len)
{
  DDSRT_UNUSED_ARG(rst);
  DDSRT_UNUSED_ARG(wr_entity_id);
  DDSRT_UNUSED_ARG(timestamp);
  DDSRT_UNUSED_ARG(statusinfo);
  DDSRT_UNUSED_ARG(vdata);
  DDSRT_UNUSED_ARG(len);

#ifdef HANDSHAKE_IMPLEMENTED
  struct q_globals *gv = rst->gv;
  const struct CDRHeader *hdr = vdata; /* built-ins not deserialized (yet) */
  const int bswap = (hdr->identifier == CDR_LE) ^ DDSRT_LITTLE_ENDIAN;
  const void *data = (void *) (hdr + 1);
  unsigned size = (unsigned)(len - sizeof(struct CDRHeader));
  struct nn_participant_generic_message msg;
  ddsi_guid_t rd_guid;
  ddsi_guid_t pwr_guid;
  ddsi_guid_t proxypp_guid;
  struct participant *pp;
  struct proxy_participant *proxypp;

  DDSRT_UNUSED_ARG(timestamp);

  rd_guid.prefix = rst->dst_guid_prefix;
  rd_guid.entityid.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER;
  pwr_guid.prefix = rst->src_guid_prefix;
  pwr_guid.entityid = wr_entity_id;

  GVTRACE (" recv crypto tokens("PGUIDFMT" --> "PGUIDFMT") ST%x", PGUID (pwr_guid), PGUID (rd_guid), statusinfo);

  memset(&msg, 0, sizeof(msg));
  if (nn_participant_generic_message_deseralize(&msg, data, size, bswap) < 0)
    goto deser_msg_failed;

  GVTRACE (" msg=");
  nn_participant_generic_message_log(gv, &msg, 0);
  GVTRACE ("\n");

  if (!msg.message_class_id)
  {
    ddsi_guid_t guid;
    guid.prefix = rst->dst_guid_prefix;
    guid.entityid.u = NN_ENTITYID_PARTICIPANT;
    GVWARNING("participant "PGUIDFMT" received a crypto exchange message with empty class_id", PGUID(guid));
    goto invalid_msg;
  }

  proxypp_guid.prefix = msg.message_identity.source_guid.prefix;
  proxypp_guid.entityid.u = NN_ENTITYID_PARTICIPANT;

  if (strcmp(GMCLASSID_SECURITY_PARTICIPANT_CRYPTO_TOKENS, msg.message_class_id) == 0)
  {
    pp = entidx_lookup_participant_guid(gv->entity_index, &msg.destination_participant_guid);
    if (!pp)
    {
      GVWARNING("received a crypto exchange message from "PGUIDFMT" with participant unknown "PGUIDFMT, PGUID(proxypp_guid), PGUID(msg.destination_participant_guid));
      goto invalid_msg;
    }
    proxypp = entidx_lookup_proxy_participant_guid(gv->entity_index, &proxypp_guid);
    if (!proxypp)
    {
      GVWARNING("received a crypto exchange message from "PGUIDFMT" with proxy participant unknown "PGUIDFMT, PGUID(proxypp_guid), PGUID(msg.destination_participant_guid));
      goto invalid_msg;
    }
    q_omg_security_set_participant_crypto_tokens(pp, proxypp, &msg.message_data);
  }
  else if (strcmp(GMCLASSID_SECURITY_DATAWRITER_CRYPTO_TOKENS, msg.message_class_id) == 0)
  {
    struct reader *rd;

    rd = entidx_lookup_reader_guid(gv->entity_index, &msg.destination_endpoint_guid);
    if (!rd)
    {
      GVWARNING("received a crypto exchange message from "PGUIDFMT" with reader unknown "PGUIDFMT, PGUID(proxypp_guid), PGUID(msg.destination_participant_guid));
      goto invalid_msg;
    }
    q_omg_security_set_remote_writer_crypto_tokens(rd, &msg.source_endpoint_guid, &msg.message_data);
  }
  else if (strcmp(GMCLASSID_SECURITY_DATAREADER_CRYPTO_TOKENS, msg.message_class_id) == 0)
  {
    struct writer *wr;

    wr = entidx_lookup_writer_guid(gv->entity_index, &msg.destination_endpoint_guid);
    if (!wr)
    {
      GVWARNING("received a crypto exchange message from "PGUIDFMT" with writer unknown "PGUIDFMT, PGUID(proxypp_guid), PGUID(msg.destination_participant_guid));
      goto invalid_msg;
    }
    q_omg_security_set_remote_reader_crypto_tokens(wr, &msg.source_endpoint_guid, &msg.message_data);
  }
  else
  {
    ddsi_guid_t guid;
    guid.prefix = rst->dst_guid_prefix;
    guid.entityid.u = NN_ENTITYID_PARTICIPANT;
    GVWARNING("participant "PGUIDFMT" received a crypto exchange message with unknown class_id", PGUID(guid));
  }

invalid_msg:
  nn_participant_generic_message_deinit(&msg);
deser_msg_failed:
  return;
#endif
}


#endif /* DDSI_INCLUDE_SECURITY */
