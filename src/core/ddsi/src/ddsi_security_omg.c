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

#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/ddsi_security_omg.h"

bool
q_omg_participant_is_secure(
  const struct participant *pp)
{
  /* TODO: Register local participant. */
  DDSRT_UNUSED_ARG(pp);
  return false;
}

bool
q_omg_proxy_participant_is_secure(
    const struct proxy_participant *proxypp)
{
  /* TODO: Register remote participant */
  DDSRT_UNUSED_ARG(proxypp);
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
  info->security_attributes = 0;
  return false;
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

void
q_omg_security_init_remote_participant(struct proxy_participant *proxypp)
{
  DDSRT_UNUSED_ARG(proxypp);
}

static bool
q_omg_proxyparticipant_is_authenticated(
  struct proxy_participant *proxypp)
{
  /* TODO: Handshake */
  DDSRT_UNUSED_ARG(proxypp);
  return false;
}

bool
q_omg_participant_allow_unauthenticated(struct participant *pp)
{
  DDSRT_UNUSED_ARG(pp);

  return true;
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

void
q_omg_security_register_remote_participant(struct participant *pp, struct proxy_participant *proxypp, int64_t shared_secret, int64_t proxy_permissions)
{
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);
  DDSRT_UNUSED_ARG(shared_secret);
  DDSRT_UNUSED_ARG(proxy_permissions);
}

void
q_omg_security_deregister_remote_participant(struct proxy_participant *proxypp)
{
  DDSRT_UNUSED_ARG(proxypp);
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

/* ask to access control security plugin for the remote participant permissions */
int64_t
q_omg_security_check_remote_participant_permissions(uint32_t domain_id, struct participant *pp, struct proxy_participant *proxypp)
{
  DDSRT_UNUSED_ARG(domain_id);
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);

  return 0;
}

bool
q_omg_is_similar_participant_security_info(struct participant *pp, struct proxy_participant *proxypp)
{
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);

  return true;
}

bool
q_omg_security_check_create_participant(struct participant *pp, uint32_t domain_id)
{
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(domain_id);
  return true;
}

void
q_omg_security_participant_send_tokens(struct participant *pp, struct proxy_participant *proxypp)
{
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(proxypp);
}

bool
q_omg_security_match_remote_writer_enabled(struct reader *rd, struct proxy_writer *pwr)
{
  DDSRT_UNUSED_ARG(rd);
  DDSRT_UNUSED_ARG(pwr);

  assert(rd);
  assert(pwr);

  return true;
}

bool
q_omg_security_match_remote_reader_enabled(struct writer *wr, struct proxy_reader *prd)
{
  DDSRT_UNUSED_ARG(wr);
  DDSRT_UNUSED_ARG(prd);

  assert(wr);
  assert(prd);

  return true;
}

bool
q_omg_security_check_remote_writer_permissions(const struct proxy_writer *pwr, uint32_t domain_id, struct participant *pp)
{
  DDSRT_UNUSED_ARG(pwr);
  DDSRT_UNUSED_ARG(domain_id);
  DDSRT_UNUSED_ARG(pp);

  assert(pwr);
  assert(pp);
  assert(pwr->c.proxypp);

  return true;
}

bool
q_omg_security_check_remote_reader_permissions(const struct proxy_reader *prd, uint32_t domain_id, struct participant *pp)
{
  DDSRT_UNUSED_ARG(prd);
  DDSRT_UNUSED_ARG(domain_id);
  DDSRT_UNUSED_ARG(pp);

  assert(prd);
  assert(pp);
  assert(prd->c.proxypp);

  return true;
}


#else /* DDSI_INCLUDE_SECURITY */

#include "dds/ddsi/ddsi_security_omg.h"

extern inline bool q_omg_participant_is_secure(UNUSED_ARG(const struct participant *pp));
extern inline bool q_omg_proxy_participant_is_secure(const struct proxy_participant *proxypp);

extern inline unsigned determine_subscription_writer(UNUSED_ARG(const struct reader *rd));
extern inline unsigned determine_publication_writer(UNUSED_ARG(const struct writer *wr));

extern inline bool q_omg_security_match_remote_writer_enabled(UNUSED_ARG(struct reader *rd), UNUSED_ARG(struct proxy_writer *pwr));
extern inline bool q_omg_security_match_remote_reader_enabled(UNUSED_ARG(struct writer *wr), UNUSED_ARG(struct proxy_reader *prd));

extern inline bool q_omg_security_check_remote_writer_permissions(UNUSED_ARG(const struct proxy_writer *pwr), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(struct participant *pp));
extern inline bool q_omg_security_check_remote_reader_permissions(UNUSED_ARG(const struct proxy_reader *prd), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(struct participant *par));



extern inline bool allow_proxy_participant_deletion(
  UNUSED_ARG(struct q_globals * const gv),
  UNUSED_ARG(const struct ddsi_guid *guid),
  UNUSED_ARG(const ddsi_entityid_t pwr_entityid));

extern inline bool q_omg_is_similar_participant_security_info(UNUSED_ARG(struct participant *pp), UNUSED_ARG(struct proxy_participant *proxypp));

extern inline bool q_omg_participant_allow_unauthenticated(UNUSED_ARG(struct participant *pp));

extern inline bool q_omg_security_check_create_participant(UNUSED_ARG(struct participant *pp), UNUSED_ARG(uint32_t domain_id));

/* initialize the proxy participant security attributes */
extern inline void q_omg_security_init_remote_participant(UNUSED_ARG(struct proxy_participant *proxypp));

/* ask to access control security plugin for the remote participant permissions */
extern inline int64_t q_omg_security_check_remote_participant_permissions(UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(struct participant *pp), UNUSED_ARG(struct proxy_participant *proxypp));

extern inline void q_omg_security_register_remote_participant(UNUSED_ARG(struct participant *pp), UNUSED_ARG(struct proxy_participant *proxypp), UNUSED_ARG(int64_t shared_secret), UNUSED_ARG(int64_t proxy_permissions));

extern inline void q_omg_security_deregister_remote_participant(UNUSED_ARG(struct proxy_participant *proxypp));

extern inline void q_omg_security_participant_send_tokens(UNUSED_ARG(struct participant *pp), UNUSED_ARG(struct proxy_participant *proxypp));

#endif /* DDSI_INCLUDE_SECURITY */
