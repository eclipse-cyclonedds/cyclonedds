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

#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_security_exchange.h"
#include "dds/ddsi/ddsi_serdata_default.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsi/ddsi_handshake.h"
#include "dds/ddsi/ddsi_serdata_pserop.h"
#include "dds/ddsi/q_ddsi_discovery.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsi/q_transmit.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/q_bswap.h"

bool write_auth_handshake_message(const struct participant *pp, const struct proxy_participant *proxypp, nn_dataholderseq_t *mdata, bool request, const nn_message_identity_t *related_message_id)
{
  struct ddsi_domaingv *gv = pp->e.gv;
  struct nn_participant_generic_message pmg;
  struct ddsi_serdata *serdata;
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
    nn_participant_generic_message_init(&pmg, &wr->e.guid, seq, &proxypp->e.guid, NULL, NULL, DDS_SECURITY_AUTH_REQUEST, mdata, NULL);
  } else {
    nn_participant_generic_message_init(&pmg, &wr->e.guid, seq, &proxypp->e.guid, NULL, NULL, DDS_SECURITY_AUTH_HANDSHAKE, mdata, related_message_id);
  }

  serdata = ddsi_serdata_from_sample (wr->topic, SDK_DATA, &pmg);
  serdata->timestamp = ddsrt_time_wallclock ();
  result = enqueue_sample_wrlock_held (wr, seq, NULL, serdata, prd, 1) == 0;
  ddsi_serdata_unref (serdata);
  ddsrt_mutex_unlock (&wr->e.lock);
  nn_participant_generic_message_deinit(&pmg);

  return result;
}

void auth_get_serialized_participant_data(struct participant *pp, ddsi_octetseq_t *seq)
{
  struct nn_xmsg *mpayload;
  ddsi_plist_t ps;
  struct participant_builtin_topic_data_locators locs;
  size_t sz;
  char *payload;
  mpayload = nn_xmsg_new (pp->e.gv->xmsgpool, &pp->e.guid, pp, 0, NN_XMSG_KIND_DATA);
  get_participant_builtin_topic_data (pp, &ps, &locs);
  ddsi_plist_addtomsg_bo (mpayload, &ps, ~(uint64_t)0, ~(uint64_t)0, true);
  nn_xmsg_addpar_sentinel_bo (mpayload, true);
  ddsi_plist_fini (&ps);
  payload = nn_xmsg_payload (&sz, mpayload);
  seq->length = (uint32_t) sz;
  seq->value = ddsrt_malloc (sz);
  memcpy (seq->value, payload, sz);
  nn_xmsg_free (mpayload);
}

void handle_auth_handshake_message(const struct receiver_state *rst, ddsi_entityid_t wr_entity_id, struct ddsi_serdata *sample_common)
{
  const struct ddsi_serdata_pserop *sample = (const struct ddsi_serdata_pserop *) sample_common;
  const struct nn_participant_generic_message *msg = sample->sample;
  struct participant *pp = NULL;
  struct proxy_writer *pwr = NULL;
  ddsi_guid_t guid;
  const ddsi_guid_t *pwr_guid;
  struct ddsi_handshake *handshake;

  DDSRT_UNUSED_ARG(wr_entity_id);

  if (msg->message_identity.source_guid.entityid.u == NN_ENTITYID_PARTICIPANT)
  {
    guid = msg->message_identity.source_guid;
    guid.entityid.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER;
    pwr_guid = &guid;
  }
  else if (msg->message_identity.source_guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER)
  {
    pwr_guid = &msg->message_identity.source_guid;
  }
  else
  {
    RSTTRACE ("invalid source entity id\n");
    return;
  }

  if ((pp = entidx_lookup_participant_guid(rst->gv->entity_index, &msg->destination_participant_guid)) == NULL)
  {
    RSTTRACE ("destination participant ("PGUIDFMT") not found\n", PGUID (msg->destination_participant_guid));
  }
  else if ((pwr = entidx_lookup_proxy_writer_guid(rst->gv->entity_index, pwr_guid)) == NULL)
  {
    RSTTRACE ("proxy writer ("PGUIDFMT") not found\n", PGUID(*pwr_guid));
  }
  else if ((handshake = ddsi_handshake_find(pp, pwr->c.proxypp)) == NULL)
  {
    RSTTRACE ("handshake not found ("PGUIDFMT" --> "PGUIDFMT")\n", PGUID (pwr->c.proxypp->e.guid), PGUID(pp->e.guid));
  }
  else
  {
    //RSTTRACE (" ("PGUIDFMT" --> "PGUIDFMT")\n", PGUID (pwr->c.proxypp->e.guid), PGUID (pp->e.guid));
    ddsi_handshake_handle_message (handshake, pp, pwr->c.proxypp, msg);
    ddsi_handshake_release (handshake);
  }
}

static bool write_crypto_exchange_message(const struct participant *pp, const ddsi_guid_t *dst_pguid, const ddsi_guid_t *src_eguid, const ddsi_guid_t *dst_eguid, const char *classid, const nn_dataholderseq_t *tokens)
{
  struct ddsi_domaingv * const gv = pp->e.gv;
  struct nn_participant_generic_message pmg;
  struct ddsi_tkmap_instance *tk;
  struct ddsi_serdata *serdata;
  struct proxy_reader *prd;
  ddsi_guid_t prd_guid;
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
  serdata = ddsi_serdata_from_sample (wr->topic, SDK_DATA, &pmg);
  serdata->timestamp = ddsrt_time_wallclock ();
  tk = ddsi_tkmap_lookup_instance_ref (gv->m_tkmap, serdata);
  r = write_sample_p2p_wrlock_held(wr, seq, NULL, serdata, tk, prd);
  ddsrt_mutex_unlock (&wr->e.lock);
  ddsi_tkmap_instance_unref (gv->m_tkmap, tk);
  ddsi_serdata_unref (serdata);

  nn_participant_generic_message_deinit(&pmg);

  return (r < 0 ? false : true);
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

void handle_crypto_exchange_message(const struct receiver_state *rst, struct ddsi_serdata *sample_common)
{
  struct ddsi_domaingv * const gv = rst->gv;
  const struct ddsi_serdata_pserop *sample = (const struct ddsi_serdata_pserop *) sample_common;
  const struct nn_participant_generic_message *msg = sample->sample;
  ddsi_guid_t proxypp_guid;

  proxypp_guid.prefix = msg->message_identity.source_guid.prefix;
  proxypp_guid.entityid.u = NN_ENTITYID_PARTICIPANT;

  if (strcmp(GMCLASSID_SECURITY_PARTICIPANT_CRYPTO_TOKENS, msg->message_class_id) == 0)
  {
    struct participant * const pp = entidx_lookup_participant_guid(gv->entity_index, &msg->destination_participant_guid);
    if (!pp)
    {
      GVWARNING("received a crypto exchange message from "PGUIDFMT" with participant unknown "PGUIDFMT, PGUID(proxypp_guid), PGUID(msg->destination_participant_guid));
      return;
    }
    struct proxy_participant *proxypp = entidx_lookup_proxy_participant_guid(gv->entity_index, &proxypp_guid);
    if (!proxypp)
    {
      GVWARNING("received a crypto exchange message from "PGUIDFMT" with proxy participant unknown "PGUIDFMT, PGUID(proxypp_guid), PGUID(msg->destination_participant_guid));
      return;
    }
    q_omg_security_set_participant_crypto_tokens(pp, proxypp, &msg->message_data);
  }
  else if (strcmp(GMCLASSID_SECURITY_DATAWRITER_CRYPTO_TOKENS, msg->message_class_id) == 0)
  {
    struct reader * const rd = entidx_lookup_reader_guid(gv->entity_index, &msg->destination_endpoint_guid);
    if (!rd)
    {
      GVWARNING("received a crypto exchange message from "PGUIDFMT" with reader unknown "PGUIDFMT, PGUID(proxypp_guid), PGUID(msg->destination_participant_guid));
      return;
    }
    q_omg_security_set_remote_writer_crypto_tokens(rd, &msg->source_endpoint_guid, &msg->message_data);
  }
  else if (strcmp(GMCLASSID_SECURITY_DATAREADER_CRYPTO_TOKENS, msg->message_class_id) == 0)
  {
    struct writer * const wr = entidx_lookup_writer_guid(gv->entity_index, &msg->destination_endpoint_guid);
    if (!wr)
    {
      GVWARNING("received a crypto exchange message from "PGUIDFMT" with writer unknown "PGUIDFMT, PGUID(proxypp_guid), PGUID(msg->destination_participant_guid));
      return;
    }
    q_omg_security_set_remote_reader_crypto_tokens(wr, &msg->source_endpoint_guid, &msg->message_data);
  }
  else
  {
    ddsi_guid_t guid;
    guid.prefix = rst->dst_guid_prefix;
    guid.entityid.u = NN_ENTITYID_PARTICIPANT;
    GVWARNING("participant "PGUIDFMT" received a crypto exchange message with unknown class_id", PGUID(guid));
  }
}

#endif /* DDSI_INCLUDE_SECURITY */
