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

#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"

#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsi/ddsi_sertopic.h"

static bool
q_omg_writer_is_payload_protected(
  const struct writer *wr);

static bool endpoint_is_DCPSParticipantSecure(const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER) ||
          (guid->entityid.u == NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER) );
}

static bool endpoint_is_DCPSPublicationsSecure(const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER) ||
          (guid->entityid.u == NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER) );
}

static bool endpoint_is_DCPSSubscriptionsSecure(const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER) ||
          (guid->entityid.u == NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER) );
}

static bool endpoint_is_DCPSParticipantStatelessMessage(const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER) ||
          (guid->entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER) );
}

static bool endpoint_is_DCPSParticipantMessageSecure(const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER) ||
          (guid->entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER) );
}

static bool endpoint_is_DCPSParticipantVolatileMessageSecure(const ddsi_guid_t *guid)
{
#if 1
  /* TODO: volatile endpoint. */
  DDSRT_UNUSED_ARG(guid);
  return false;
#else
  return ((guid->entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER) ||
          (guid->entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER) );
#endif
}


bool
q_omg_participant_is_secure(
  const struct participant *pp)
{
  /* TODO: Register local participant. */
  DDSRT_UNUSED_ARG(pp);
  return false;
}

static bool
q_omg_writer_is_discovery_protected(
  const struct writer *wr)
{
  /* TODO: Register local writer. */
  DDSRT_UNUSED_ARG(wr);
  return false;
}

static bool
q_omg_reader_is_discovery_protected(
  const struct reader *rd)
{
  /* TODO: Register local reader. */
  DDSRT_UNUSED_ARG(rd);
  return false;
}

bool
q_omg_get_writer_security_info(
  const struct writer *wr,
  nn_security_info_t *info)
{
  assert(wr);
  assert(info);
  /* TODO: Register local writer. */
  DDSRT_UNUSED_ARG(wr);

  info->plugin_security_attributes = 0;
  if (q_omg_writer_is_payload_protected(wr))
  {
    info->security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID|
                                NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_PAYLOAD_PROTECTED;
  }
  else
  {
    info->security_attributes = 0;
  }
  return true;
}

bool
q_omg_get_reader_security_info(
  const struct reader *rd,
  nn_security_info_t *info)
{
  assert(rd);
  assert(info);
  /* TODO: Register local reader. */
  DDSRT_UNUSED_ARG(rd);
  info->plugin_security_attributes = 0;
  info->security_attributes = 0;
  return false;
}

static bool
q_omg_proxyparticipant_is_authenticated(
  struct proxy_participant *proxypp)
{
  /* TODO: Handshake */
  DDSRT_UNUSED_ARG(proxypp);
  return false;
}

unsigned
determine_subscription_writer(
  const struct reader *rd)
{
  if (q_omg_reader_is_discovery_protected(rd))
  {
    return NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER;
  }
  return NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER;
}

unsigned
determine_publication_writer(
  const struct writer *wr)
{
  if (q_omg_writer_is_discovery_protected(wr))
  {
    return NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER;
  }
  return NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER;
}

bool
allow_proxy_participant_deletion(
  struct q_globals * const gv,
  const struct ddsi_guid *guid,
  const ddsi_entityid_t pwr_entityid)
{
  struct proxy_participant *proxypp;

  assert(gv);
  assert(guid);

  /* Always allow deletion from a secure proxy writer. */
  if (pwr_entityid.u == NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER)
    return true;

  /* Not from a secure proxy writer.
   * Only allow deletion when proxy participant is not authenticated. */
  proxypp = ephash_lookup_proxy_participant_guid(gv->guid_hash, guid);
  if (!proxypp)
  {
    GVLOGDISC (" unknown");
    return false;
  }
  return (!q_omg_proxyparticipant_is_authenticated(proxypp));
}

void
set_proxy_participant_security_info(
  struct proxy_participant *proxypp,
  const nn_plist_t *plist)
{
  assert(proxypp);
  assert(plist);
  if (plist->present & PP_PARTICIPANT_SECURITY_INFO) {
    proxypp->security_info.security_attributes = plist->participant_security_info.security_attributes;
    proxypp->security_info.plugin_security_attributes = plist->participant_security_info.plugin_security_attributes;
  } else {
    proxypp->security_info.security_attributes = 0;
    proxypp->security_info.plugin_security_attributes = 0;
  }
}

static void
q_omg_get_proxy_endpoint_security_info(
  const struct entity_common *entity,
  nn_security_info_t *proxypp_sec_info,
  const nn_plist_t *plist,
  nn_security_info_t *info)
{
  bool proxypp_info_available;

  proxypp_info_available = (proxypp_sec_info->security_attributes != 0) ||
                           (proxypp_sec_info->plugin_security_attributes != 0);

  /*
   * If Security info is present, use that.
   * Otherwise, use the specified values for the secure builtin endpoints.
   *      (Table 20 – EndpointSecurityAttributes for all "Builtin Security Endpoints")
   * Otherwise, reset.
   */
  if (plist->present & PP_ENDPOINT_SECURITY_INFO)
  {
    info->security_attributes = plist->endpoint_security_info.security_attributes;
    info->plugin_security_attributes = plist->endpoint_security_info.plugin_security_attributes;
  }
  else if (endpoint_is_DCPSParticipantSecure(&(entity->guid)) ||
           endpoint_is_DCPSPublicationsSecure(&(entity->guid)) ||
           endpoint_is_DCPSSubscriptionsSecure(&(entity->guid)) )
  {
    info->plugin_security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    info->security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    if (proxypp_info_available)
    {
      if (proxypp_sec_info->security_attributes & NN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_DISCOVERY_PROTECTED)
      {
        info->security_attributes |= NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;
      }
      if (proxypp_sec_info->plugin_security_attributes & NN_PLUGIN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_DISCOVERY_ENCRYPTED)
      {
        info->plugin_security_attributes |= NN_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
      }
      if (proxypp_sec_info->plugin_security_attributes & NN_PLUGIN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_DISCOVERY_AUTHENTICATED)
      {
        info->plugin_security_attributes |= NN_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ORIGIN_AUTHENTICATED;
      }
    }
    else
    {
      /* No participant info: assume hardcoded OpenSplice V6.10.0 values. */
      info->security_attributes |= NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;
      info->plugin_security_attributes |= NN_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
    }
  }
  else if (endpoint_is_DCPSParticipantMessageSecure(&(entity->guid)))
  {
    info->plugin_security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    info->security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    if (proxypp_info_available)
    {
      if (proxypp_sec_info->security_attributes & NN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_LIVELINESS_PROTECTED)
      {
        info->security_attributes |= NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;
      }
      if (proxypp_sec_info->plugin_security_attributes & NN_PLUGIN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_LIVELINESS_ENCRYPTED)
      {
        info->plugin_security_attributes |= NN_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
      }
      if (proxypp_sec_info->plugin_security_attributes & NN_PLUGIN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_LIVELINESS_AUTHENTICATED)
      {
        info->plugin_security_attributes |= NN_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ORIGIN_AUTHENTICATED;
      }
    }
    else
    {
      /* No participant info: assume hardcoded OpenSplice V6.10.0 values. */
      info->security_attributes |= NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;
      info->plugin_security_attributes |= NN_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
    }
  }
  else if (endpoint_is_DCPSParticipantStatelessMessage(&(entity->guid)))
  {
    info->security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    info->plugin_security_attributes = 0;
  }
  else if (endpoint_is_DCPSParticipantVolatileMessageSecure(&(entity->guid)))
  {
    info->security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID |
                                NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;
    info->plugin_security_attributes = 0;
  }
  else
  {
    info->security_attributes = 0;
    info->plugin_security_attributes = 0;
  }
}

void
set_proxy_reader_security_info(
  struct proxy_reader *prd,
  const nn_plist_t *plist)
{
  assert(prd);
  q_omg_get_proxy_endpoint_security_info(&(prd->e),
                                         &(prd->c.proxypp->security_info),
                                         plist,
                                         &(prd->security_info));
}

void
set_proxy_writer_security_info(
  struct proxy_writer *pwr,
  const nn_plist_t *plist)
{
  assert(pwr);
  q_omg_get_proxy_endpoint_security_info(&(pwr->e),
                                         &(pwr->c.proxypp->security_info),
                                         plist,
                                         &(pwr->security_info));
}

static bool
q_omg_security_encode_serialized_payload(
  const struct writer *wr,
  const unsigned char *src_buf,
  const unsigned int   src_len,
  unsigned char     **dst_buf,
  unsigned int       *dst_len)
{
  /* TODO: Use proper keys to actually encode (need key-exchange). */
  DDSRT_UNUSED_ARG(wr);
  DDSRT_UNUSED_ARG(src_buf);
  DDSRT_UNUSED_ARG(src_len);
  DDSRT_UNUSED_ARG(dst_buf);
  DDSRT_UNUSED_ARG(dst_len);
  return false;
}

static bool
q_omg_security_decode_serialized_payload(
  struct proxy_writer *pwr,
  const unsigned char *src_buf,
  const unsigned int   src_len,
  unsigned char     **dst_buf,
  unsigned int       *dst_len)
{
  /* TODO: Use proper keys to actually decode (need key-exchange). */
  DDSRT_UNUSED_ARG(pwr);
  DDSRT_UNUSED_ARG(src_buf);
  DDSRT_UNUSED_ARG(src_len);
  DDSRT_UNUSED_ARG(dst_buf);
  DDSRT_UNUSED_ARG(dst_len);
  return false;
}

static bool
q_omg_writer_is_payload_protected(
  const struct writer *wr)
{
  /* TODO: Local registration. */
  DDSRT_UNUSED_ARG(wr);
  return false;
}

bool
encode_payload(
  struct writer *wr,
  ddsrt_iovec_t *vec,
  unsigned char **buf)
{
  bool ok = true;
  *buf = NULL;
  if (q_omg_writer_is_payload_protected(wr))
  {
    /* Encrypt the data. */
    unsigned char *enc_buf;
    unsigned int   enc_len;
    ok = q_omg_security_encode_serialized_payload(
                    wr,
                    vec->iov_base,
                    (unsigned int)vec->iov_len,
                    &enc_buf,
                    &enc_len);
    if (ok)
    {
      /* Replace the iov buffer, which should always be aliased. */
      vec->iov_base = (char *)enc_buf;
      vec->iov_len = enc_len;
      /* Remember the pointer to be able to free the memory. */
      *buf = enc_buf;
    }
  }
  return ok;
}


static bool
decode_payload(
  const struct q_globals *gv,
  struct nn_rsample_info *sampleinfo,
  unsigned char *payloadp,
  uint32_t *payloadsz,
  size_t *submsg_len)
{
  bool ok = true;

  assert(payloadp);
  assert(payloadsz);
  assert(*payloadsz);
  assert(submsg_len);
  assert(sampleinfo);

  if (sampleinfo->pwr == NULL)
  {
    /* No specified proxy writer means no encoding. */
    return true;
  }

  /* Only decode when the attributes tell us so. */
  if ((sampleinfo->pwr->security_info.security_attributes & NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_PAYLOAD_PROTECTED)
                                                         == NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_PAYLOAD_PROTECTED)
  {
    unsigned char *dst_buf = NULL;
    unsigned int   dst_len = 0;

    /* Decrypt the payload. */
    if (q_omg_security_decode_serialized_payload(sampleinfo->pwr, payloadp, *payloadsz, &dst_buf, &dst_len))
    {
      /* Expect result to always fit into the original buffer. */
      assert(*payloadsz >= dst_len);

      /* Reduce submessage and payload lengths. */
      *submsg_len -= (*payloadsz - dst_len);
      *payloadsz   = dst_len;

      /* Replace the encrypted payload with the decrypted. */
      memcpy(payloadp, dst_buf, dst_len);
      ddsrt_free(dst_buf);
    }
    else
    {
      GVWARNING("decode_payload: failed to decrypt data from "PGUIDFMT"", PGUID (sampleinfo->pwr->e.guid));
      ok = false;
    }
  }

  return ok;
}

bool
decode_Data(
  const struct q_globals *gv,
  struct nn_rsample_info *sampleinfo,
  unsigned char *payloadp,
  uint32_t payloadsz,
  size_t *submsg_len)
{
  int ok = true;
  /* Only decode when there's actual data. */
  if (payloadp && (payloadsz > 0))
  {
    ok = decode_payload(gv, sampleinfo, payloadp, &payloadsz, submsg_len);
    if (ok)
    {
      /* It's possible that the payload size (and thus the sample size) has been reduced. */
      sampleinfo->size = payloadsz;
    }
  }
  return ok;
}

bool
decode_DataFrag(
  const struct q_globals *gv,
  struct nn_rsample_info *sampleinfo,
  unsigned char *payloadp,
  uint32_t payloadsz,
  size_t *submsg_len)
{
  int ok = true;
  /* Only decode when there's actual data. */
  if (payloadp && (payloadsz > 0))
  {
    ok = decode_payload(gv, sampleinfo, payloadp, &payloadsz, submsg_len);
    /* Do not touch the sampleinfo->size in contradiction to decode_Data() (it has been calculated differently). */
  }
  return ok;
}

#else /* DDSI_INCLUDE_SECURITY */

#include "dds/ddsi/ddsi_security_omg.h"

extern inline bool q_omg_participant_is_secure(
  UNUSED_ARG(const struct participant *pp));

extern inline unsigned determine_subscription_writer(
  UNUSED_ARG(const struct reader *rd));

extern inline unsigned determine_publication_writer(
  UNUSED_ARG(const struct writer *wr));

extern inline bool allow_proxy_participant_deletion(
  UNUSED_ARG(struct q_globals * const gv),
  UNUSED_ARG(const struct ddsi_guid *guid),
  UNUSED_ARG(const ddsi_entityid_t pwr_entityid));

extern inline void set_proxy_participant_security_info(
  UNUSED_ARG(struct proxy_participant *prd),
  UNUSED_ARG(const nn_plist_t *plist));

extern inline void set_proxy_reader_security_info(
  UNUSED_ARG(struct proxy_reader *prd),
  UNUSED_ARG(const nn_plist_t *plist));

extern inline void set_proxy_writer_security_info(
  UNUSED_ARG(struct proxy_writer *pwr),
  UNUSED_ARG(const nn_plist_t *plist));

extern inline bool decode_Data(
  UNUSED_ARG(const struct q_globals *gv),
  UNUSED_ARG(struct nn_rsample_info *sampleinfo),
  UNUSED_ARG(unsigned char *payloadp),
  UNUSED_ARG(uint32_t payloadsz),
  UNUSED_ARG(size_t *submsg_len));

extern inline bool decode_DataFrag(
  UNUSED_ARG(const struct q_globals *gv),
  UNUSED_ARG(struct nn_rsample_info *sampleinfo),
  UNUSED_ARG(unsigned char *payloadp),
  UNUSED_ARG(uint32_t payloadsz),
  UNUSED_ARG(size_t *submsg_len));

#endif /* DDSI_INCLUDE_SECURITY */
