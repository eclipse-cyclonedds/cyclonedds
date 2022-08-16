/*
 * Copyright(c) 2006 to 2022 ZettaScale Technology and others
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
#include <stddef.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_participant.h"
#include "dds/ddsi/ddsi_proxy_participant.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/ddsi/ddsi_proxy_endpoint.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_mcgroup.h"
#include "dds/ddsi/ddsi_rhc.h"
#include "dds/ddsi/q_qosmatch.h"
#include "dds/ddsi/q_addrset.h"
#include "dds/ddsi/q_xevent.h"
#include "dds/ddsi/q_whc.h"

static ddsi_entityid_t builtin_entityid_match (ddsi_entityid_t x)
{
  ddsi_entityid_t res;
  res.u = 0;
  switch (x.u)
  {
    case NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_TOPIC_READER;
      break;
#ifdef DDS_HAS_TOPIC_DISCOVERY
    case NN_ENTITYID_SEDP_BUILTIN_TOPIC_READER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER;
      break;
#endif
    case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER:
      res.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER:
      res.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER;
      break;
#ifdef DDS_HAS_TYPE_DISCOVERY
    case NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER:
      res.u = NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_READER;
      break;
    case NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_READER:
      res.u = NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER;
      break;
    case NN_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER:
      res.u = NN_ENTITYID_TL_SVC_BUILTIN_REPLY_READER;
      break;
    case NN_ENTITYID_TL_SVC_BUILTIN_REPLY_READER:
      res.u = NN_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER;
      break;
#endif

    case NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER:
    case NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER:
      /* SPDP is special cased because we don't -- indeed can't --
         match readers with writers, only send to matched readers and
         only accept from matched writers. That way discovery wouldn't
         work at all. No entity with NN_ENTITYID_UNKNOWN exists,
         ever, so this guarantees no connection will be made. */
      res.u = NN_ENTITYID_UNKNOWN;
      break;

    case NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER:
      res.u = NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER;
      break;
    case NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER:
      res.u = NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER;
      break;

    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER:
      res.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER:
      res.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER:
      res.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER:
      res.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER:
      res.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER:
      res.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER;
      break;

    default:
      assert (0);
  }
  return res;
}

static void writer_qos_mismatch (struct writer * wr, dds_qos_policy_id_t reason)
{
  /* When the reason is DDS_INVALID_QOS_POLICY_ID, it means that we compared
   * readers/writers from different topics: ignore that. */
  if (reason != DDS_INVALID_QOS_POLICY_ID && wr->status_cb)
  {
    status_cb_data_t data;
    data.raw_status_id = (int) DDS_OFFERED_INCOMPATIBLE_QOS_STATUS_ID;
    data.extra = reason;
    (wr->status_cb) (wr->status_cb_entity, &data);
  }
}

static void reader_qos_mismatch (struct reader * rd, dds_qos_policy_id_t reason)
{
  /* When the reason is DDS_INVALID_QOS_POLICY_ID, it means that we compared
   * readers/writers from different topics: ignore that. */
  if (reason != DDS_INVALID_QOS_POLICY_ID && rd->status_cb)
  {
    status_cb_data_t data;
    data.raw_status_id = (int) DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS_ID;
    data.extra = reason;
    (rd->status_cb) (rd->status_cb_entity, &data);
  }
}

static bool topickind_qos_match_p_lock (struct ddsi_domaingv *gv, struct entity_common *rd, const dds_qos_t *rdqos, struct entity_common *wr, const dds_qos_t *wrqos, dds_qos_policy_id_t *reason
#ifdef DDS_HAS_TYPE_DISCOVERY
    , const struct ddsi_type_pair *rd_type_pair, const struct ddsi_type_pair *wr_type_pair
#endif
)
{
  assert (is_reader_entityid (rd->guid.entityid));
  assert (is_writer_entityid (wr->guid.entityid));
  if (is_keyed_endpoint_entityid (rd->guid.entityid) != is_keyed_endpoint_entityid (wr->guid.entityid))
  {
    *reason = DDS_INVALID_QOS_POLICY_ID;
    return false;
  }
  ddsrt_mutex_t * const locks[] = { &rd->qos_lock, &wr->qos_lock, &rd->qos_lock };
  const int shift = (uintptr_t) rd > (uintptr_t) wr;
  for (int i = 0; i < 2; i++)
    ddsrt_mutex_lock (locks[i + shift]);
#ifdef DDS_HAS_TYPE_DISCOVERY
  bool rd_type_lookup, wr_type_lookup;
  const ddsi_typeid_t *req_type_id = NULL;
  ddsi_guid_t *proxypp_guid = NULL;
  bool ret = qos_match_p (gv, rdqos, wrqos, reason, rd_type_pair, wr_type_pair, &rd_type_lookup, &wr_type_lookup);
  if (!ret)
  {
    /* In case qos_match_p returns false, one of rd_type_look and wr_type_lookup could
       be set to indicate that type information is missing. At this point, we know this
       is the case so do a type lookup request for either rd_type_pair->minimal or
       wr_type_pair->minimal or a dependent type for one of these. */
    if (rd_type_lookup && is_proxy_endpoint (rd))
    {
      req_type_id = ddsi_type_pair_minimal_id (rd_type_pair);
      proxypp_guid = &((struct generic_proxy_endpoint *) rd)->c.proxypp->e.guid;
    }
    else if (wr_type_lookup && is_proxy_endpoint (wr))
    {
      req_type_id = ddsi_type_pair_minimal_id (wr_type_pair);
      proxypp_guid = &((struct generic_proxy_endpoint *) wr)->c.proxypp->e.guid;
    }
  }
#else
  bool ret = qos_match_p (gv, rdqos, wrqos, reason);
#endif
  for (int i = 0; i < 2; i++)
    ddsrt_mutex_unlock (locks[i + shift]);

#ifdef DDS_HAS_TYPE_DISCOVERY
  if (req_type_id)
  {
    (void) ddsi_tl_request_type (gv, req_type_id, proxypp_guid, DDSI_TYPE_INCLUDE_DEPS);
    return false;
  }
#endif

  return ret;
}

void connect_writer_with_proxy_reader_secure (struct writer *wr, struct proxy_reader *prd, ddsrt_mtime_t tnow, int64_t crypto_handle)
{
  DDSRT_UNUSED_ARG(tnow);
  proxy_reader_add_connection (prd, wr, crypto_handle);
  writer_add_connection (wr, prd, crypto_handle);
}

void connect_reader_with_proxy_writer_secure (struct reader *rd, struct proxy_writer *pwr, ddsrt_mtime_t tnow, int64_t crypto_handle)
{
  nn_count_t init_count;
  struct alive_state alive_state;

  /* Initialize the reader's tracking information for the writer liveliness state to something
     sensible, but that may be outdated by the time the reader gets added to the writer's list
     of matching readers. */
  proxy_writer_get_alive_state (pwr, &alive_state);
  reader_add_connection (rd, pwr, &init_count,  &alive_state, crypto_handle);
  proxy_writer_add_connection (pwr, rd, tnow, init_count, crypto_handle);

  /* Once everything is set up: update with the latest state, any updates to the alive state
     happening in parallel will cause this to become a no-op. */
  proxy_writer_get_alive_state (pwr, &alive_state);
  reader_update_notify_pwr_alive_state (rd, pwr, &alive_state);
}

static void connect_writer_with_proxy_reader (struct writer *wr, struct proxy_reader *prd, ddsrt_mtime_t tnow)
{
  struct ddsi_domaingv *gv = wr->e.gv;
  const int isb0 = (is_builtin_entityid (wr->e.guid.entityid, NN_VENDORID_ECLIPSE) != 0);
  const int isb1 = (is_builtin_entityid (prd->e.guid.entityid, prd->c.vendor) != 0);
  dds_qos_policy_id_t reason;
  int64_t crypto_handle;
  bool relay_only;

  DDSRT_UNUSED_ARG(tnow);
  if (isb0 != isb1)
    return;
  if (wr->e.onlylocal)
    return;
#ifdef DDS_HAS_TYPE_DISCOVERY
  if (!isb0 && !topickind_qos_match_p_lock (gv, &prd->e, prd->c.xqos, &wr->e, wr->xqos, &reason, prd->c.type_pair, wr->c.type_pair))
#else
  if (!isb0 && !topickind_qos_match_p_lock (gv, &prd->e, prd->c.xqos, &wr->e, wr->xqos, &reason))
#endif
  {
    writer_qos_mismatch (wr, reason);
    return;
  }

  if (!q_omg_security_check_remote_reader_permissions (prd, wr->e.gv->config.domainId, wr->c.pp, &relay_only))
  {
    GVLOGDISC ("connect_writer_with_proxy_reader (wr "PGUIDFMT") with (prd "PGUIDFMT") not allowed by security\n", PGUID (wr->e.guid), PGUID (prd->e.guid));
  }
  else if (relay_only)
  {
    GVWARNING ("connect_writer_with_proxy_reader (wr "PGUIDFMT") with (prd "PGUIDFMT") relay_only not supported\n", PGUID (wr->e.guid), PGUID (prd->e.guid));
  }
  else if (!q_omg_security_match_remote_reader_enabled (wr, prd, relay_only, &crypto_handle))
  {
    GVLOGDISC ("connect_writer_with_proxy_reader (wr "PGUIDFMT") with (prd "PGUIDFMT") waiting for approval by security\n", PGUID (wr->e.guid), PGUID (prd->e.guid));
  }
  else
  {
    proxy_reader_add_connection (prd, wr, crypto_handle);
    writer_add_connection (wr, prd, crypto_handle);
  }
}

static void connect_proxy_writer_with_reader (struct proxy_writer *pwr, struct reader *rd, ddsrt_mtime_t tnow)
{
  const int isb0 = (is_builtin_entityid (pwr->e.guid.entityid, pwr->c.vendor) != 0);
  const int isb1 = (is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE) != 0);
  dds_qos_policy_id_t reason;
  nn_count_t init_count;
  struct alive_state alive_state;
  int64_t crypto_handle;

  if (isb0 != isb1)
    return;
  if (rd->e.onlylocal)
    return;
#ifdef DDS_HAS_TYPE_DISCOVERY
  if (!isb0 && !topickind_qos_match_p_lock (rd->e.gv, &rd->e, rd->xqos, &pwr->e, pwr->c.xqos, &reason, rd->c.type_pair, pwr->c.type_pair))
#else
  if (!isb0 && !topickind_qos_match_p_lock (rd->e.gv, &rd->e, rd->xqos, &pwr->e, pwr->c.xqos, &reason))
#endif
  {
    reader_qos_mismatch (rd, reason);
    return;
  }

  if (!q_omg_security_check_remote_writer_permissions(pwr, rd->e.gv->config.domainId, rd->c.pp))
  {
    EELOGDISC (&rd->e, "connect_proxy_writer_with_reader (pwr "PGUIDFMT") with (rd "PGUIDFMT") not allowed by security\n",
        PGUID (pwr->e.guid), PGUID (rd->e.guid));
  }
  else if (!q_omg_security_match_remote_writer_enabled(rd, pwr, &crypto_handle))
  {
    EELOGDISC (&rd->e, "connect_proxy_writer_with_reader (pwr "PGUIDFMT") with  (rd "PGUIDFMT") waiting for approval by security\n",
        PGUID (pwr->e.guid), PGUID (rd->e.guid));
  }
  else
  {
    /* Initialize the reader's tracking information for the writer liveliness state to something
       sensible, but that may be outdated by the time the reader gets added to the writer's list
       of matching readers. */
    proxy_writer_get_alive_state (pwr, &alive_state);
    reader_add_connection (rd, pwr, &init_count, &alive_state, crypto_handle);
    proxy_writer_add_connection (pwr, rd, tnow, init_count, crypto_handle);

    /* Once everything is set up: update with the latest state, any updates to the alive state
       happening in parallel will cause this to become a no-op. */
    proxy_writer_get_alive_state (pwr, &alive_state);
    reader_update_notify_pwr_alive_state (rd, pwr, &alive_state);
  }
}

static bool ignore_local_p (const ddsi_guid_t *guid1, const ddsi_guid_t *guid2, const struct dds_qos *xqos1, const struct dds_qos *xqos2)
{
  assert (xqos1->present & QP_CYCLONE_IGNORELOCAL);
  assert (xqos2->present & QP_CYCLONE_IGNORELOCAL);
  switch (xqos1->ignorelocal.value)
  {
    case DDS_IGNORELOCAL_NONE:
      break;
    case DDS_IGNORELOCAL_PARTICIPANT:
      return memcmp (&guid1->prefix, &guid2->prefix, sizeof (guid1->prefix)) == 0;
    case DDS_IGNORELOCAL_PROCESS:
      return true;
  }
  switch (xqos2->ignorelocal.value)
  {
    case DDS_IGNORELOCAL_NONE:
      break;
    case DDS_IGNORELOCAL_PARTICIPANT:
      return memcmp (&guid1->prefix, &guid2->prefix, sizeof (guid1->prefix)) == 0;
    case DDS_IGNORELOCAL_PROCESS:
      return true;
  }
  return false;
}

static void connect_writer_with_reader (struct writer *wr, struct reader *rd, ddsrt_mtime_t tnow)
{
  dds_qos_policy_id_t reason;
  struct alive_state alive_state;
  (void)tnow;
  if (!is_local_orphan_endpoint (&wr->e) && (is_builtin_entityid (wr->e.guid.entityid, NN_VENDORID_ECLIPSE) || is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE)))
    return;
  if (ignore_local_p (&wr->e.guid, &rd->e.guid, wr->xqos, rd->xqos))
    return;
#ifdef DDS_HAS_TYPE_DISCOVERY
  if (!topickind_qos_match_p_lock (wr->e.gv, &rd->e, rd->xqos, &wr->e, wr->xqos, &reason, rd->c.type_pair, wr->c.type_pair))
#else
  if (!topickind_qos_match_p_lock (wr->e.gv, &rd->e, rd->xqos, &wr->e, wr->xqos, &reason))
#endif
  {
    writer_qos_mismatch (wr, reason);
    reader_qos_mismatch (rd, reason);
    return;
  }
  /* Initialze the reader's tracking information for the writer liveliness state to something
     sensible, but that may be outdated by the time the reader gets added to the writer's list
     of matching readers. */
  writer_get_alive_state (wr, &alive_state);
  reader_add_local_connection (rd, wr, &alive_state);
  writer_add_local_connection (wr, rd);

  /* Once everything is set up: update with the latest state, any updates to the alive state
     happening in parallel will cause this to become a no-op. */
  writer_get_alive_state (wr, &alive_state);
  reader_update_notify_wr_alive_state (rd, wr, &alive_state);
}

static void connect_writer_with_proxy_reader_wrapper (struct entity_common *vwr, struct entity_common *vprd, ddsrt_mtime_t tnow)
{
  struct writer *wr = (struct writer *) vwr;
  struct proxy_reader *prd = (struct proxy_reader *) vprd;
  assert (wr->e.kind == EK_WRITER);
  assert (prd->e.kind == EK_PROXY_READER);
  connect_writer_with_proxy_reader (wr, prd, tnow);
}

static void connect_proxy_writer_with_reader_wrapper (struct entity_common *vpwr, struct entity_common *vrd, ddsrt_mtime_t tnow)
{
  struct proxy_writer *pwr = (struct proxy_writer *) vpwr;
  struct reader *rd = (struct reader *) vrd;
  assert (pwr->e.kind == EK_PROXY_WRITER);
  assert (rd->e.kind == EK_READER);
  connect_proxy_writer_with_reader (pwr, rd, tnow);
}

static void connect_writer_with_reader_wrapper (struct entity_common *vwr, struct entity_common *vrd, ddsrt_mtime_t tnow)
{
  struct writer *wr = (struct writer *) vwr;
  struct reader *rd = (struct reader *) vrd;
  assert (wr->e.kind == EK_WRITER);
  assert (rd->e.kind == EK_READER);
  connect_writer_with_reader (wr, rd, tnow);
}

static enum entity_kind generic_do_match_mkind (enum entity_kind kind, bool local)
{
  switch (kind)
  {
    case EK_WRITER: return local ? EK_READER : EK_PROXY_READER;
    case EK_READER: return local ? EK_WRITER : EK_PROXY_WRITER;
    case EK_PROXY_WRITER: assert (!local); return EK_READER;
    case EK_PROXY_READER: assert (!local); return EK_WRITER;
    case EK_PARTICIPANT:
    case EK_PROXY_PARTICIPANT:
    case EK_TOPIC:
      assert(0);
      return EK_WRITER;
  }
  assert(0);
  return EK_WRITER;
}

static void generic_do_match_connect (struct entity_common *e, struct entity_common *em, ddsrt_mtime_t tnow, bool local)
{
  switch (e->kind)
  {
    case EK_WRITER:
      if (local)
        connect_writer_with_reader_wrapper (e, em, tnow);
      else
        connect_writer_with_proxy_reader_wrapper (e, em, tnow);
      break;
    case EK_READER:
      if (local)
        connect_writer_with_reader_wrapper (em, e, tnow);
      else
        connect_proxy_writer_with_reader_wrapper (em, e, tnow);
      break;
    case EK_PROXY_WRITER:
      assert (!local);
      connect_proxy_writer_with_reader_wrapper (e, em, tnow);
      break;
    case EK_PROXY_READER:
      assert (!local);
      connect_writer_with_proxy_reader_wrapper (em, e, tnow);
      break;
    case EK_PARTICIPANT:
    case EK_PROXY_PARTICIPANT:
    case EK_TOPIC:
      assert(0);
      break;
  }
}

static const char *entity_topic_name (const struct entity_common *e)
{
  switch (e->kind)
  {
    case EK_WRITER:
      return ((const struct writer *) e)->xqos->topic_name;
    case EK_READER:
      return ((const struct reader *) e)->xqos->topic_name;
    case EK_PROXY_WRITER:
    case EK_PROXY_READER:
      return ((const struct generic_proxy_endpoint *) e)->c.xqos->topic_name;
#ifdef DDS_HAS_TOPIC_DISCOVERY
    case EK_TOPIC:
    {
      struct topic * topic = (struct topic *) e;
      ddsrt_mutex_lock (&topic->e.qos_lock);
      const char * name = topic->definition->xqos->topic_name;
      ddsrt_mutex_unlock (&topic->e.qos_lock);
      return name;
    }
#endif
    case EK_PARTICIPANT:
    case EK_PROXY_PARTICIPANT:
    default:
      assert (0);
  }
  return "";
}

static void generic_do_match (struct entity_common *e, ddsrt_mtime_t tnow, bool local)
{
  static const struct { const char *full; const char *full_us; const char *abbrev; } kindstr[] = {
    [EK_WRITER] = { "writer", "writer", "wr" },
    [EK_READER] = { "reader", "reader", "rd" },
    [EK_PROXY_WRITER] = { "proxy writer", "proxy_writer", "pwr" },
    [EK_PROXY_READER] = { "proxy reader", "proxy_reader", "prd" },
    [EK_PARTICIPANT] = { "participant", "participant", "pp" },
    [EK_PROXY_PARTICIPANT] = { "proxy participant", "proxy_participant", "proxypp" }
  };

  enum entity_kind mkind = generic_do_match_mkind (e->kind, local);
  struct entity_index const * const entidx = e->gv->entity_index;
  struct entidx_enum it;
  struct entity_common *em;

  if (!is_builtin_entityid (e->guid.entityid, NN_VENDORID_ECLIPSE) || (local && is_local_orphan_endpoint (e)))
  {
    /* Non-builtins need matching on topics, the local orphan endpoints
       are a bit weird because they reuse the builtin entityids but
       otherwise need to be treated as normal readers */
    struct match_entities_range_key max;
    const char *tp = entity_topic_name (e);
    EELOGDISC (e, "match_%s_with_%ss(%s "PGUIDFMT") scanning all %ss%s%s\n",
               kindstr[e->kind].full_us, kindstr[mkind].full_us,
               kindstr[e->kind].abbrev, PGUID (e->guid),
               kindstr[mkind].abbrev,
               tp ? " of topic " : "", tp ? tp : "");
    /* Note: we visit at least all proxies that existed when we called
       init (with the -- possible -- exception of ones that were
       deleted between our calling init and our reaching it while
       enumerating), but we may visit a single proxy reader multiple
       times. */
    entidx_enum_init_topic (&it, entidx, mkind, tp, &max);
    while ((em = entidx_enum_next_max (&it, &max)) != NULL)
      generic_do_match_connect (e, em, tnow, local);
    entidx_enum_fini (&it);
  }
  else if (!local)
  {
    /* Built-ins have fixed QoS and a known entity id to use, so instead of
       looking for the right topic, just probe the matching GUIDs for all
       (proxy) participants.  Local matching never needs to look at the
       discovery endpoints */
    const ddsi_entityid_t tgt_ent = builtin_entityid_match (e->guid.entityid);
    const bool isproxy = (e->kind == EK_PROXY_WRITER || e->kind == EK_PROXY_READER || e->kind == EK_PROXY_PARTICIPANT);
    enum entity_kind pkind = isproxy ? EK_PARTICIPANT : EK_PROXY_PARTICIPANT;
    EELOGDISC (e, "match_%s_with_%ss(%s "PGUIDFMT") scanning %sparticipants tgt=%"PRIx32"\n",
               kindstr[e->kind].full_us, kindstr[mkind].full_us,
               kindstr[e->kind].abbrev, PGUID (e->guid),
               isproxy ? "" : "proxy ", tgt_ent.u);
    if (tgt_ent.u != NN_ENTITYID_UNKNOWN)
    {
      entidx_enum_init (&it, entidx, pkind);
      while ((em = entidx_enum_next (&it)) != NULL)
      {
        const ddsi_guid_t tgt_guid = { em->guid.prefix, tgt_ent };
        struct entity_common *ep;
        if ((ep = entidx_lookup_guid (entidx, &tgt_guid, mkind)) != NULL)
          generic_do_match_connect (e, ep, tnow, local);
      }
      entidx_enum_fini (&it);
    }
  }
}

void match_writer_with_proxy_readers (struct writer *wr, ddsrt_mtime_t tnow)
{
  generic_do_match (&wr->e, tnow, false);
}

void match_writer_with_local_readers (struct writer *wr, ddsrt_mtime_t tnow)
{
  generic_do_match (&wr->e, tnow, true);
}

void match_reader_with_proxy_writers (struct reader *rd, ddsrt_mtime_t tnow)
{
  generic_do_match (&rd->e, tnow, false);
}

void match_reader_with_local_writers (struct reader *rd, ddsrt_mtime_t tnow)
{
  generic_do_match (&rd->e, tnow, true);
}

void match_proxy_writer_with_readers (struct proxy_writer *pwr, ddsrt_mtime_t tnow)
{
  generic_do_match (&pwr->e, tnow, false);
}

void match_proxy_reader_with_writers (struct proxy_reader *prd, ddsrt_mtime_t tnow)
{
  generic_do_match(&prd->e, tnow, false);
}

void free_wr_prd_match (const struct ddsi_domaingv *gv, const ddsi_guid_t *wr_guid, struct wr_prd_match *m)
{
  if (m)
  {
#ifdef DDS_HAS_SECURITY
    q_omg_security_deregister_remote_reader_match (gv, wr_guid, m);
#else
    (void) gv;
    (void) wr_guid;
#endif
    nn_lat_estim_fini (&m->hb_to_ack_latency);
    ddsrt_free (m);
  }
}

void free_rd_pwr_match (struct ddsi_domaingv *gv, const ddsi_guid_t *rd_guid, struct rd_pwr_match *m)
{
  if (m)
  {
#ifdef DDS_HAS_SECURITY
    q_omg_security_deregister_remote_writer_match (gv, rd_guid, m);
#else
    (void) rd_guid;
#endif
#ifdef DDS_HAS_SSM
    if (!is_unspec_xlocator (&m->ssm_mc_loc))
    {
      assert (ddsi_is_mcaddr (gv, &m->ssm_mc_loc.c));
      assert (!is_unspec_xlocator (&m->ssm_src_loc));
      if (ddsi_leave_mc (gv, gv->mship, gv->data_conn_mc, &m->ssm_src_loc.c, &m->ssm_mc_loc.c) < 0)
        GVWARNING ("failed to leave network partition ssm group\n");
    }
#endif
#if !(defined DDS_HAS_SECURITY || defined DDS_HAS_SSM)
    (void) gv;
#endif
    ddsrt_free (m);
  }
}

void free_pwr_rd_match (struct pwr_rd_match *m)
{
  if (m)
  {
    if (m->acknack_xevent)
      delete_xevent (m->acknack_xevent);
    nn_reorder_free (m->u.not_in_sync.reorder);
    ddsrt_free (m);
  }
}

void free_prd_wr_match (struct prd_wr_match *m)
{
  if (m) ddsrt_free (m);
}

void free_rd_wr_match (struct rd_wr_match *m)
{
  if (m) ddsrt_free (m);
}

void free_wr_rd_match (struct wr_rd_match *m)
{
  if (m) ddsrt_free (m);
}

void writer_add_connection (struct writer *wr, struct proxy_reader *prd, int64_t crypto_handle)
{
  struct wr_prd_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;
  int pretend_everything_acked;

#ifdef DDS_HAS_SHM
  const bool use_iceoryx = prd->is_iceoryx && !(wr->xqos->ignore_locator_type & NN_LOCATOR_KIND_SHEM);
#else
  const bool use_iceoryx = false;
#endif

  m->prd_guid = prd->e.guid;
  m->is_reliable = (prd->c.xqos->reliability.kind > DDS_RELIABILITY_BEST_EFFORT);
  m->assumed_in_sync = (wr->e.gv->config.retransmit_merging == DDSI_REXMIT_MERGE_ALWAYS);
  m->has_replied_to_hb = !m->is_reliable || use_iceoryx;
  m->all_have_replied_to_hb = 0;
  m->non_responsive_count = 0;
  m->rexmit_requests = 0;
#ifdef DDS_HAS_SECURITY
  m->crypto_handle = crypto_handle;
#else
  DDSRT_UNUSED_ARG(crypto_handle);
#endif
  /* m->demoted: see below */
  ddsrt_mutex_lock (&prd->e.lock);
  if (prd->deleting)
  {
    ELOGDISC (wr, "  writer_add_connection(wr "PGUIDFMT" prd "PGUIDFMT") - prd is being deleted\n",
              PGUID (wr->e.guid), PGUID (prd->e.guid));
    pretend_everything_acked = 1;
  }
  else if (!m->is_reliable || use_iceoryx)
  {
    /* Pretend a best-effort reader has ack'd everything, even waht is
       still to be published. */
    pretend_everything_acked = 1;
  }
  else
  {
    pretend_everything_acked = 0;
  }
  ddsrt_mutex_unlock (&prd->e.lock);
  m->prev_acknack = 0;
  m->prev_nackfrag = 0;
  nn_lat_estim_init (&m->hb_to_ack_latency);
  m->hb_to_ack_latency_tlastlog = ddsrt_time_wallclock ();
  m->t_acknack_accepted.v = 0;
  m->t_nackfrag_accepted.v = 0;

  ddsrt_mutex_lock (&wr->e.lock);
#ifdef DDS_HAS_SHM
  if (pretend_everything_acked || prd->is_iceoryx)
#else
  if (pretend_everything_acked)
#endif
    m->seq = MAX_SEQ_NUMBER;
  else
    m->seq = wr->seq;
  m->last_seq = m->seq;
  if (ddsrt_avl_lookup_ipath (&wr_readers_treedef, &wr->readers, &prd->e.guid, &path))
  {
    ELOGDISC (wr, "  writer_add_connection(wr "PGUIDFMT" prd "PGUIDFMT") - already connected\n",
              PGUID (wr->e.guid), PGUID (prd->e.guid));
    ddsrt_mutex_unlock (&wr->e.lock);
    nn_lat_estim_fini (&m->hb_to_ack_latency);
    ddsrt_free (m);
  }
  else
  {
    ELOGDISC (wr, "  writer_add_connection(wr "PGUIDFMT" prd "PGUIDFMT") - ack seq %"PRIu64"\n",
              PGUID (wr->e.guid), PGUID (prd->e.guid), m->seq);
    ddsrt_avl_insert_ipath (&wr_readers_treedef, &wr->readers, m, &path);
    wr->num_readers++;
    wr->num_reliable_readers += m->is_reliable;
    wr->num_readers_requesting_keyhash += prd->requests_keyhash ? 1 : 0;
    rebuild_writer_addrset (wr);
    ddsrt_mutex_unlock (&wr->e.lock);

    if (wr->status_cb)
    {
      status_cb_data_t data;
      data.raw_status_id = (int) DDS_PUBLICATION_MATCHED_STATUS_ID;
      data.add = true;
      data.handle = prd->e.iid;
      (wr->status_cb) (wr->status_cb_entity, &data);
    }

    /* If reliable and/or transient-local, we may have data available
       in the WHC, but if all has been acknowledged by the previously
       known proxy readers (or if the is the first proxy reader),
       there is no heartbeat event scheduled.

       A pre-emptive AckNack may be sent, but need not be, and we
       can't be certain it won't have the final flag set. So we must
       ensure a heartbeat is scheduled soon. */
    if (wr->heartbeat_xevent)
    {
      const int64_t delta = DDS_MSECS (1);
      const ddsrt_mtime_t tnext = ddsrt_mtime_add_duration (ddsrt_time_monotonic (), delta);
      ddsrt_mutex_lock (&wr->e.lock);
      /* To make sure that we keep sending heartbeats at a higher rate
         at the start of this discovery, reset the hbs_since_last_write
         count to zero. */
      wr->hbcontrol.hbs_since_last_write = 0;
      if (tnext.v < wr->hbcontrol.tsched.v)
      {
        wr->hbcontrol.tsched = tnext;
        (void) resched_xevent_if_earlier (wr->heartbeat_xevent, tnext);
      }
      ddsrt_mutex_unlock (&wr->e.lock);
    }
  }
}

void writer_add_local_connection (struct writer *wr, struct reader *rd)
{
  struct wr_rd_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;

  ddsrt_mutex_lock (&wr->e.lock);
  if (ddsrt_avl_lookup_ipath (&wr_local_readers_treedef, &wr->local_readers, &rd->e.guid, &path))
  {
    ELOGDISC (wr, "  writer_add_local_connection(wr "PGUIDFMT" rd "PGUIDFMT") - already connected\n",
              PGUID (wr->e.guid), PGUID (rd->e.guid));
    ddsrt_mutex_unlock (&wr->e.lock);
    ddsrt_free (m);
    return;
  }

  ELOGDISC (wr, "  writer_add_local_connection(wr "PGUIDFMT" rd "PGUIDFMT")",
            PGUID (wr->e.guid), PGUID (rd->e.guid));
  m->rd_guid = rd->e.guid;
  ddsrt_avl_insert_ipath (&wr_local_readers_treedef, &wr->local_readers, m, &path);

#ifdef DDS_HAS_SHM
  if (!wr->has_iceoryx || !rd->has_iceoryx)
    local_reader_ary_insert(&wr->rdary, rd);
#else
  local_reader_ary_insert(&wr->rdary, rd);
#endif

  /* Store available data into the late joining reader when it is reliable (we don't do
     historical data for best-effort data over the wire, so also not locally). */
  if (rd->xqos->reliability.kind > DDS_RELIABILITY_BEST_EFFORT && rd->xqos->durability.kind > DDS_DURABILITY_VOLATILE)
    deliver_historical_data (wr, rd);

  ddsrt_mutex_unlock (&wr->e.lock);

  ELOGDISC (wr, "\n");

  if (wr->status_cb)
  {
    status_cb_data_t data;
    data.raw_status_id = (int) DDS_PUBLICATION_MATCHED_STATUS_ID;
    data.add = true;
    data.handle = rd->e.iid;
    (wr->status_cb) (wr->status_cb_entity, &data);
  }
}

void reader_add_connection (struct reader *rd, struct proxy_writer *pwr, nn_count_t *init_count, const struct alive_state *alive_state, int64_t crypto_handle)
{
  struct rd_pwr_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;

  m->pwr_guid = pwr->e.guid;
  m->pwr_alive = alive_state->alive;
  m->pwr_alive_vclock = alive_state->vclock;
#ifdef DDS_HAS_SECURITY
  m->crypto_handle = crypto_handle;
#else
  DDSRT_UNUSED_ARG(crypto_handle);
#endif

  ddsrt_mutex_lock (&rd->e.lock);

  /* Initial sequence number of acknacks is the highest stored (+ 1,
     done when generating the acknack) -- existing connections may be
     beyond that already, but this guarantees that one particular
     writer will always see monotonically increasing sequence numbers
     from one particular reader.  This is then used for the
     pwr_rd_match initialization */
  ELOGDISC (rd, "  reader "PGUIDFMT" init_acknack_count = %"PRIu32"\n",
            PGUID (rd->e.guid), rd->init_acknack_count);
  *init_count = rd->init_acknack_count;

  if (ddsrt_avl_lookup_ipath (&rd_writers_treedef, &rd->writers, &pwr->e.guid, &path))
  {
    ELOGDISC (rd, "  reader_add_connection(pwr "PGUIDFMT" rd "PGUIDFMT") - already connected\n",
              PGUID (pwr->e.guid), PGUID (rd->e.guid));
    ddsrt_mutex_unlock (&rd->e.lock);
    ddsrt_free (m);
  }
  else
  {
    ELOGDISC (rd, "  reader_add_connection(pwr "PGUIDFMT" rd "PGUIDFMT")\n",
              PGUID (pwr->e.guid), PGUID (rd->e.guid));

    ddsrt_avl_insert_ipath (&rd_writers_treedef, &rd->writers, m, &path);
    rd->num_writers++;
    ddsrt_mutex_unlock (&rd->e.lock);

#ifdef DDS_HAS_SSM
    if (rd->favours_ssm && pwr->supports_ssm)
    {
      /* pwr->supports_ssm is set if addrset_contains_ssm(pwr->ssm), so
       any_ssm must succeed. */
      if (!addrset_any_uc (pwr->c.as, &m->ssm_src_loc))
        assert (0);
      if (!addrset_any_ssm (rd->e.gv, pwr->c.as, &m->ssm_mc_loc))
        assert (0);
      /* FIXME: for now, assume that the ports match for datasock_mc --
       't would be better to dynamically create and destroy sockets on
       an as needed basis. */
      int ret = ddsi_join_mc (rd->e.gv, rd->e.gv->mship, rd->e.gv->data_conn_mc, &m->ssm_src_loc.c, &m->ssm_mc_loc.c);
      if (ret < 0)
        ELOGDISC (rd, "  unable to join\n");
    }
    else
    {
      set_unspec_xlocator (&m->ssm_src_loc);
      set_unspec_xlocator (&m->ssm_mc_loc);
    }
#endif

    if (rd->status_cb)
    {
      status_cb_data_t data;
      data.handle = pwr->e.iid;
      data.add = true;
      data.extra = (uint32_t) (alive_state->alive ? LIVELINESS_CHANGED_ADD_ALIVE : LIVELINESS_CHANGED_ADD_NOT_ALIVE);

      data.raw_status_id = (int) DDS_SUBSCRIPTION_MATCHED_STATUS_ID;
      (rd->status_cb) (rd->status_cb_entity, &data);

      data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
      (rd->status_cb) (rd->status_cb_entity, &data);
    }
  }
}

void reader_add_local_connection (struct reader *rd, struct writer *wr, const struct alive_state *alive_state)
{
  struct rd_wr_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;

  m->wr_guid = wr->e.guid;
  m->wr_alive = alive_state->alive;
  m->wr_alive_vclock = alive_state->vclock;

  ddsrt_mutex_lock (&rd->e.lock);

  if (ddsrt_avl_lookup_ipath (&rd_local_writers_treedef, &rd->local_writers, &wr->e.guid, &path))
  {
    ELOGDISC (rd, "  reader_add_local_connection(wr "PGUIDFMT" rd "PGUIDFMT") - already connected\n",
              PGUID (wr->e.guid), PGUID (rd->e.guid));
    ddsrt_mutex_unlock (&rd->e.lock);
    ddsrt_free (m);
  }
  else
  {
    ELOGDISC (rd, "  reader_add_local_connection(wr "PGUIDFMT" rd "PGUIDFMT")\n",
              PGUID (wr->e.guid), PGUID (rd->e.guid));
    ddsrt_avl_insert_ipath (&rd_local_writers_treedef, &rd->local_writers, m, &path);
    ddsrt_mutex_unlock (&rd->e.lock);

    if (rd->status_cb)
    {
      status_cb_data_t data;
      data.handle = wr->e.iid;
      data.add = true;
      data.extra = (uint32_t) (alive_state->alive ? LIVELINESS_CHANGED_ADD_ALIVE : LIVELINESS_CHANGED_ADD_NOT_ALIVE);

      data.raw_status_id = (int) DDS_SUBSCRIPTION_MATCHED_STATUS_ID;
      (rd->status_cb) (rd->status_cb_entity, &data);

      data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
      (rd->status_cb) (rd->status_cb_entity, &data);
    }
  }
}

void proxy_writer_add_connection (struct proxy_writer *pwr, struct reader *rd, ddsrt_mtime_t tnow, nn_count_t init_count, int64_t crypto_handle)
{
  struct pwr_rd_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;

  ddsrt_mutex_lock (&pwr->e.lock);
  if (ddsrt_avl_lookup_ipath (&pwr_readers_treedef, &pwr->readers, &rd->e.guid, &path))
    goto already_matched;

  assert (rd->type || is_builtin_endpoint (rd->e.guid.entityid, NN_VENDORID_ECLIPSE));
  if (pwr->ddsi2direct_cb == 0 && rd->ddsi2direct_cb != 0)
  {
    pwr->ddsi2direct_cb = rd->ddsi2direct_cb;
    pwr->ddsi2direct_cbarg = rd->ddsi2direct_cbarg;
  }

#ifdef DDS_HAS_SHM
  const bool use_iceoryx = pwr->is_iceoryx && !(rd->xqos->ignore_locator_type & NN_LOCATOR_KIND_SHEM);
#else
  const bool use_iceoryx = false;
#endif

  ELOGDISC (pwr, "  proxy_writer_add_connection(pwr "PGUIDFMT" rd "PGUIDFMT")",
            PGUID (pwr->e.guid), PGUID (rd->e.guid));
  m->rd_guid = rd->e.guid;
  m->tcreate = ddsrt_time_monotonic ();

  /* We track the last heartbeat count value per reader--proxy-writer
     pair, so that we can correctly handle directed heartbeats. The
     only reason to bother is to prevent a directed heartbeat (with
     the FINAL flag clear) from causing AckNacks from all readers
     instead of just the addressed ones.

     If we don't mind those extra AckNacks, we could track the count
     at the proxy-writer and simply treat all incoming heartbeats as
     undirected. */
  m->prev_heartbeat = 0;
  m->hb_timestamp.v = 0;
  m->t_heartbeat_accepted.v = 0;
  m->t_last_nack.v = 0;
  m->t_last_ack.v = 0;
  m->last_nack.seq_end_p1 = 0;
  m->last_nack.seq_base = 0;
  m->last_nack.frag_end_p1 = 0;
  m->last_nack.frag_base = 0;
  m->last_seq = 0;
  m->filtered = 0;
  m->ack_requested = 0;
  m->heartbeat_since_ack = 0;
  m->heartbeatfrag_since_ack = 0;
  m->directed_heartbeat = 0;
  m->nack_sent_on_nackdelay = 0;

#ifdef DDS_HAS_SECURITY
  m->crypto_handle = crypto_handle;
#else
  DDSRT_UNUSED_ARG(crypto_handle);
#endif

  /* These can change as a consequence of handling data and/or
     discovery activities. The safe way of dealing with them is to
     lock the proxy writer */
  if (is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE) && !ddsrt_avl_is_empty (&pwr->readers) && !pwr->filtered)
  {
    /* builtins really don't care about multiple copies or anything */
    m->in_sync = PRMSS_SYNC;
  }
  else if (use_iceoryx)
  {
    m->in_sync = PRMSS_SYNC;
  }
  else if (!pwr->have_seen_heartbeat || !rd->handle_as_transient_local)
  {
    /* Proxy writer hasn't seen a heartbeat yet: means we have no
       clue from what sequence number to start accepting data, nor
       where historical data ends and live data begins.

       A transient-local reader should always get all historical
       data, and so can always start-out as "out-of-sync".  Cyclone
       refuses to retransmit already ACK'd samples to a Cyclone
       reader, so if the other side is Cyclone, we can always start
       from sequence number 1.

       For non-Cyclone, if the reader is volatile, we have to just
       start from the most recent sample, even though that means
       the first samples written after matching the reader may be
       lost.  The alternative not only gets too much historical data
       but may also result in "sample lost" notifications because the
       writer is (may not be) retaining samples on behalf of this
       reader for the oldest samples and so this reader may end up
       with a partial set of old-ish samples.  Even when both are
       using KEEP_ALL and the connection doesn't fail ... */
    if (rd->handle_as_transient_local)
      m->in_sync = PRMSS_OUT_OF_SYNC;
    else if (vendor_is_eclipse (pwr->c.vendor))
      m->in_sync = PRMSS_OUT_OF_SYNC;
    else
      m->in_sync = PRMSS_SYNC;
    m->u.not_in_sync.end_of_tl_seq = MAX_SEQ_NUMBER;
  }
  else
  {
    /* transient-local reader; range of sequence numbers is already
       known */
    m->in_sync = PRMSS_OUT_OF_SYNC;
    m->u.not_in_sync.end_of_tl_seq = pwr->last_seq;
  }
  if (m->in_sync != PRMSS_SYNC)
  {
    ELOGDISC (pwr, " - out-of-sync");
    pwr->n_readers_out_of_sync++;
    local_reader_ary_setfastpath_ok (&pwr->rdary, false);
  }
  m->count = init_count;
  /* Spec says we may send a pre-emptive AckNack (8.4.2.3.4), hence we
     schedule it for the configured delay. From then on it it'll keep
     sending pre-emptive ones until the proxy writer receives a heartbeat.
     (We really only need a pre-emptive AckNack per proxy writer, but
     hopefully it won't make that much of a difference in practice.) */
  if (rd->reliable)
  {
    uint32_t secondary_reorder_maxsamples = pwr->e.gv->config.secondary_reorder_maxsamples;

    if (rd->e.guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER)
    {
      secondary_reorder_maxsamples = pwr->e.gv->config.primary_reorder_maxsamples;
      m->filtered = 1;
    }

    const ddsrt_mtime_t tsched = use_iceoryx ? DDSRT_MTIME_NEVER : ddsrt_mtime_add_duration (tnow, pwr->e.gv->config.preemptive_ack_delay);
    m->acknack_xevent = qxev_acknack (pwr->evq, tsched, &pwr->e.guid, &rd->e.guid);
    m->u.not_in_sync.reorder =
      nn_reorder_new (&pwr->e.gv->logconfig, NN_REORDER_MODE_NORMAL, secondary_reorder_maxsamples, pwr->e.gv->config.late_ack_mode);
    pwr->n_reliable_readers++;
  }
  else
  {
    m->acknack_xevent = NULL;
    m->u.not_in_sync.reorder =
      nn_reorder_new (&pwr->e.gv->logconfig, NN_REORDER_MODE_MONOTONICALLY_INCREASING, pwr->e.gv->config.secondary_reorder_maxsamples, pwr->e.gv->config.late_ack_mode);
  }

  ddsrt_avl_insert_ipath (&pwr_readers_treedef, &pwr->readers, m, &path);

#ifdef DDS_HAS_SHM
  if (!pwr->is_iceoryx || !rd->has_iceoryx)
    local_reader_ary_insert(&pwr->rdary, rd);
#else
  local_reader_ary_insert(&pwr->rdary, rd);
#endif

  ddsrt_mutex_unlock (&pwr->e.lock);
  qxev_pwr_entityid (pwr, &rd->e.guid);

  ELOGDISC (pwr, "\n");
  return;

already_matched:
  ELOGDISC (pwr, "  proxy_writer_add_connection(pwr "PGUIDFMT" rd "PGUIDFMT") - already connected\n",
            PGUID (pwr->e.guid), PGUID (rd->e.guid));
  ddsrt_mutex_unlock (&pwr->e.lock);
  ddsrt_free (m);
  return;
}

void proxy_reader_add_connection (struct proxy_reader *prd, struct writer *wr, int64_t crypto_handle)
{
  struct prd_wr_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;

  m->wr_guid = wr->e.guid;
#ifdef DDS_HAS_SECURITY
  m->crypto_handle = crypto_handle;
#else
  DDSRT_UNUSED_ARG(crypto_handle);
#endif

  ddsrt_mutex_lock (&prd->e.lock);
  if (ddsrt_avl_lookup_ipath (&prd_writers_treedef, &prd->writers, &wr->e.guid, &path))
  {
    ELOGDISC (prd, "  proxy_reader_add_connection(wr "PGUIDFMT" prd "PGUIDFMT") - already connected\n",
              PGUID (wr->e.guid), PGUID (prd->e.guid));
    ddsrt_mutex_unlock (&prd->e.lock);
    ddsrt_free (m);
  }
  else
  {
    ELOGDISC (prd, "  proxy_reader_add_connection(wr "PGUIDFMT" prd "PGUIDFMT")\n",
              PGUID (wr->e.guid), PGUID (prd->e.guid));
    ddsrt_avl_insert_ipath (&prd_writers_treedef, &prd->writers, m, &path);
    ddsrt_mutex_unlock (&prd->e.lock);
    qxev_prd_entityid (prd, &wr->e.guid);

  }
}

void writer_drop_connection (const struct ddsi_guid *wr_guid, const struct proxy_reader *prd)
{
  struct writer *wr;
  if ((wr = entidx_lookup_writer_guid (prd->e.gv->entity_index, wr_guid)) != NULL)
  {
    struct whc_node *deferred_free_list = NULL;
    struct wr_prd_match *m;
    ddsrt_mutex_lock (&wr->e.lock);
    if ((m = ddsrt_avl_lookup (&wr_readers_treedef, &wr->readers, &prd->e.guid)) != NULL)
    {
      struct whc_state whcst;
      ddsrt_avl_delete (&wr_readers_treedef, &wr->readers, m);
      wr->num_readers--;
      wr->num_reliable_readers -= m->is_reliable;
      wr->num_readers_requesting_keyhash -= prd->requests_keyhash ? 1 : 0;
      rebuild_writer_addrset (wr);
      remove_acked_messages (wr, &whcst, &deferred_free_list);
    }

    ddsrt_mutex_unlock (&wr->e.lock);
    if (m != NULL && wr->status_cb)
    {
      status_cb_data_t data;
      data.raw_status_id = (int) DDS_PUBLICATION_MATCHED_STATUS_ID;
      data.add = false;
      data.handle = prd->e.iid;
      (wr->status_cb) (wr->status_cb_entity, &data);
    }
    whc_free_deferred_free_list (wr->whc, deferred_free_list);
    free_wr_prd_match (wr->e.gv, &wr->e.guid, m);
  }
}

void writer_drop_local_connection (const struct ddsi_guid *wr_guid, struct reader *rd)
{
  /* Only called by gc_delete_reader, so we actually have a reader pointer */
  struct writer *wr;
  if ((wr = entidx_lookup_writer_guid (rd->e.gv->entity_index, wr_guid)) != NULL)
  {
    struct wr_rd_match *m;

    ddsrt_mutex_lock (&wr->e.lock);
    if ((m = ddsrt_avl_lookup (&wr_local_readers_treedef, &wr->local_readers, &rd->e.guid)) != NULL)
    {
      ddsrt_avl_delete (&wr_local_readers_treedef, &wr->local_readers, m);
      local_reader_ary_remove (&wr->rdary, rd);
    }
    ddsrt_mutex_unlock (&wr->e.lock);
    if (m != NULL && wr->status_cb)
    {
      status_cb_data_t data;
      data.raw_status_id = (int) DDS_PUBLICATION_MATCHED_STATUS_ID;
      data.add = false;
      data.handle = rd->e.iid;
      (wr->status_cb) (wr->status_cb_entity, &data);
    }
    free_wr_rd_match (m);
  }
}

void reader_drop_connection (const struct ddsi_guid *rd_guid, const struct proxy_writer *pwr)
{
  struct reader *rd;
  if ((rd = entidx_lookup_reader_guid (pwr->e.gv->entity_index, rd_guid)) != NULL)
  {
    struct rd_pwr_match *m;
    ddsrt_mutex_lock (&rd->e.lock);
    if ((m = ddsrt_avl_lookup (&rd_writers_treedef, &rd->writers, &pwr->e.guid)) != NULL)
    {
      ddsrt_avl_delete (&rd_writers_treedef, &rd->writers, m);
      rd->num_writers--;
    }

    ddsrt_mutex_unlock (&rd->e.lock);
    if (m != NULL)
    {
      if (rd->rhc)
      {
        struct ddsi_writer_info wrinfo;
        ddsi_make_writer_info (&wrinfo, &pwr->e, pwr->c.xqos, NN_STATUSINFO_UNREGISTER);
        ddsi_rhc_unregister_wr (rd->rhc, &wrinfo);
      }
      if (rd->status_cb)
      {
        status_cb_data_t data;
        data.handle = pwr->e.iid;
        data.add = false;
        data.extra = (uint32_t) (m->pwr_alive ? LIVELINESS_CHANGED_REMOVE_ALIVE : LIVELINESS_CHANGED_REMOVE_NOT_ALIVE);

        data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
        (rd->status_cb) (rd->status_cb_entity, &data);

        data.raw_status_id = (int) DDS_SUBSCRIPTION_MATCHED_STATUS_ID;
        (rd->status_cb) (rd->status_cb_entity, &data);
      }
    }
    free_rd_pwr_match (pwr->e.gv, &rd->e.guid, m);
  }
}

void reader_drop_local_connection (const struct ddsi_guid *rd_guid, const struct writer *wr)
{
  struct reader *rd;
  if ((rd = entidx_lookup_reader_guid (wr->e.gv->entity_index, rd_guid)) != NULL)
  {
    struct rd_wr_match *m;
    ddsrt_mutex_lock (&rd->e.lock);
    if ((m = ddsrt_avl_lookup (&rd_local_writers_treedef, &rd->local_writers, &wr->e.guid)) != NULL)
      ddsrt_avl_delete (&rd_local_writers_treedef, &rd->local_writers, m);
    ddsrt_mutex_unlock (&rd->e.lock);
    if (m != NULL)
    {
      if (rd->rhc)
      {
        /* FIXME: */
        struct ddsi_writer_info wrinfo;
        ddsi_make_writer_info (&wrinfo, &wr->e, wr->xqos, NN_STATUSINFO_UNREGISTER);
        ddsi_rhc_unregister_wr (rd->rhc, &wrinfo);
      }
      if (rd->status_cb)
      {
        status_cb_data_t data;
        data.handle = wr->e.iid;
        data.add = false;
        data.extra = (uint32_t) (m->wr_alive ? LIVELINESS_CHANGED_REMOVE_ALIVE : LIVELINESS_CHANGED_REMOVE_NOT_ALIVE);

        data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
        (rd->status_cb) (rd->status_cb_entity, &data);

        data.raw_status_id = (int) DDS_SUBSCRIPTION_MATCHED_STATUS_ID;
        (rd->status_cb) (rd->status_cb_entity, &data);
      }
    }
    free_rd_wr_match (m);
  }
}

void proxy_writer_drop_connection (const struct ddsi_guid *pwr_guid, struct reader *rd)
{
  /* Only called by gc_delete_reader, so we actually have a reader pointer */
  struct proxy_writer *pwr;
  if ((pwr = entidx_lookup_proxy_writer_guid (rd->e.gv->entity_index, pwr_guid)) != NULL)
  {
    struct pwr_rd_match *m;

    ddsrt_mutex_lock (&pwr->e.lock);
    if ((m = ddsrt_avl_lookup (&pwr_readers_treedef, &pwr->readers, &rd->e.guid)) != NULL)
    {
      ddsrt_avl_delete (&pwr_readers_treedef, &pwr->readers, m);
      if (m->in_sync != PRMSS_SYNC)
      {
        if (--pwr->n_readers_out_of_sync == 0)
          local_reader_ary_setfastpath_ok (&pwr->rdary, true);
      }
      if (rd->reliable)
        pwr->n_reliable_readers--;
      /* If no reliable readers left, there is no reason to believe the heartbeats will keep
         coming and therefore reset have_seen_heartbeat so the next reader to be created
         doesn't get initialised based on stale data */
      const bool isreliable = (pwr->c.xqos->reliability.kind != DDS_RELIABILITY_BEST_EFFORT);
      if (pwr->n_reliable_readers == 0 && isreliable && pwr->have_seen_heartbeat)
      {
        pwr->have_seen_heartbeat = 0;
        nn_defrag_notegap (pwr->defrag, 1, pwr->last_seq + 1);
        nn_reorder_drop_upto (pwr->reorder, pwr->last_seq + 1);
      }
      local_reader_ary_remove (&pwr->rdary, rd);
    }
    ddsrt_mutex_unlock (&pwr->e.lock);
    if (m)
    {
      update_reader_init_acknack_count (&rd->e.gv->logconfig, rd->e.gv->entity_index, &rd->e.guid, m->count);
      if (m->filtered)
        nn_defrag_prune(pwr->defrag, &m->rd_guid.prefix, m->last_seq);
    }
    free_pwr_rd_match (m);
  }
}

void proxy_reader_drop_connection (const struct ddsi_guid *prd_guid, struct writer *wr)
{
  struct proxy_reader *prd;
  if ((prd = entidx_lookup_proxy_reader_guid (wr->e.gv->entity_index, prd_guid)) != NULL)
  {
    struct prd_wr_match *m;
    ddsrt_mutex_lock (&prd->e.lock);
    m = ddsrt_avl_lookup (&prd_writers_treedef, &prd->writers, &wr->e.guid);
    if (m)
    {
      ddsrt_avl_delete (&prd_writers_treedef, &prd->writers, m);
    }
    ddsrt_mutex_unlock (&prd->e.lock);
    free_prd_wr_match (m);
  }
}

void local_reader_ary_init (struct local_reader_ary *x)
{
  ddsrt_mutex_init (&x->rdary_lock);
  x->valid = 1;
  x->fastpath_ok = 1;
  x->n_readers = 0;
  x->rdary = ddsrt_malloc (sizeof (*x->rdary));
  x->rdary[0] = NULL;
}

void local_reader_ary_fini (struct local_reader_ary *x)
{
  ddsrt_free (x->rdary);
  ddsrt_mutex_destroy (&x->rdary_lock);
}

void local_reader_ary_insert (struct local_reader_ary *x, struct reader *rd)
{
  ddsrt_mutex_lock (&x->rdary_lock);
  x->rdary = ddsrt_realloc (x->rdary, (x->n_readers + 2) * sizeof (*x->rdary));
  if (x->n_readers <= 1 || rd->type == x->rdary[x->n_readers - 1]->type)
  {
    /* if the first or second reader, or if the type is the same as that of
       the last one in the list simply appending the new will maintain order */
    x->rdary[x->n_readers] = rd;
  }
  else
  {
    uint32_t i;
    for (i = 0; i < x->n_readers; i++)
      if (x->rdary[i]->type == rd->type)
        break;
    if (i < x->n_readers)
    {
      /* shift any with the same type plus whichever follow to make room */
      memmove (&x->rdary[i + 1], &x->rdary[i], (x->n_readers - i) * sizeof (x->rdary[i]));
    }
    x->rdary[i] = rd;
  }
  x->rdary[x->n_readers + 1] = NULL;
  x->n_readers++;
  ddsrt_mutex_unlock (&x->rdary_lock);
}

void local_reader_ary_remove (struct local_reader_ary *x, struct reader *rd)
{
  uint32_t i;
  ddsrt_mutex_lock (&x->rdary_lock);
  for (i = 0; i < x->n_readers; i++)
    if (x->rdary[i] == rd)
      break;
  if (i >= x->n_readers) {
    ddsrt_mutex_unlock(&x->rdary_lock);
    return; // rd not found, nothing to do
  }
  if (i + 1 < x->n_readers)
  {
    /* dropping the final one never requires any fixups; dropping one that has
       the same type as the last is as simple as moving the last one in the
       removed one's location; else shift all following readers to keep it
       grouped by type */
    if (rd->type == x->rdary[x->n_readers - 1]->type)
      x->rdary[i] = x->rdary[x->n_readers - 1];
    else
      memmove (&x->rdary[i], &x->rdary[i + 1], (x->n_readers - i - 1) * sizeof (x->rdary[i]));
  }
  x->n_readers--;
  x->rdary[x->n_readers] = NULL;
  x->rdary = ddsrt_realloc (x->rdary, (x->n_readers + 1) * sizeof (*x->rdary));
  ddsrt_mutex_unlock (&x->rdary_lock);
}

void local_reader_ary_setfastpath_ok (struct local_reader_ary *x, bool fastpath_ok)
{
  ddsrt_mutex_lock (&x->rdary_lock);
  if (x->valid)
    x->fastpath_ok = fastpath_ok;
  ddsrt_mutex_unlock (&x->rdary_lock);
}

void local_reader_ary_setinvalid (struct local_reader_ary *x)
{
  ddsrt_mutex_lock (&x->rdary_lock);
  x->valid = 0;
  x->fastpath_ok = 0;
  ddsrt_mutex_unlock (&x->rdary_lock);
}


#ifdef DDS_HAS_SECURITY

static void downgrade_to_nonsecure (struct proxy_participant *proxypp)
{
  const ddsrt_wctime_t tnow = ddsrt_time_wallclock ();
  struct ddsi_guid guid;
  static const struct setab setab[] = {
      {EK_PROXY_WRITER, NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER},
      {EK_PROXY_READER, NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER},
      {EK_PROXY_WRITER, NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER},
      {EK_PROXY_READER, NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER},
      {EK_PROXY_WRITER, NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER},
      {EK_PROXY_READER, NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER},
      {EK_PROXY_WRITER, NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER},
      {EK_PROXY_READER, NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER},
      {EK_PROXY_WRITER, NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER},
      {EK_PROXY_READER, NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER},
      {EK_PROXY_WRITER, NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER},
      {EK_PROXY_READER, NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER}
  };
  int i;

  DDS_CWARNING (&proxypp->e.gv->logconfig, "downgrade participant "PGUIDFMT" to non-secure\n", PGUID (proxypp->e.guid));

  guid.prefix = proxypp->e.guid.prefix;
  /* Remove security related endpoints. */
  for (i = 0; i < (int)(sizeof(setab)/sizeof(*setab)); i++)
  {
    guid.entityid.u = setab[i].id;
    switch (setab[i].kind)
    {
    case EK_PROXY_READER:
      (void)delete_proxy_reader (proxypp->e.gv, &guid, tnow, 0);
      break;
    case EK_PROXY_WRITER:
      (void)delete_proxy_writer (proxypp->e.gv, &guid, tnow, 0);
      break;
    default:
      assert(0);
    }
  }

  /* Cleanup all kinds of related security information. */
  q_omg_security_deregister_remote_participant(proxypp);
  proxypp->bes &= NN_BES_MASK_NON_SECURITY;
}

void match_volatile_secure_endpoints (struct participant *pp, struct proxy_participant *proxypp)
{
  struct reader *rd;
  struct writer *wr;
  struct proxy_reader *prd;
  struct proxy_writer *pwr;
  ddsi_guid_t guid;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();

  EELOGDISC (&pp->e, "match volatile endpoints (pp "PGUIDFMT") with (proxypp "PGUIDFMT")\n",
             PGUID(pp->e.guid), PGUID(proxypp->e.guid));

  guid = pp->e.guid;
  guid.entityid.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER;
  if ((rd = entidx_lookup_reader_guid (pp->e.gv->entity_index, &guid)) == NULL)
    return;

  guid.entityid.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER;
  if ((wr = entidx_lookup_writer_guid (pp->e.gv->entity_index, &guid)) == NULL)
    return;

  guid = proxypp->e.guid;
  guid.entityid.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER;
  if ((prd = entidx_lookup_proxy_reader_guid (pp->e.gv->entity_index, &guid)) == NULL)
    return;

  guid.entityid.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER;
  if ((pwr = entidx_lookup_proxy_writer_guid (pp->e.gv->entity_index, &guid)) == NULL)
    return;

  connect_proxy_writer_with_reader_wrapper(&pwr->e, &rd->e, tnow);
  connect_writer_with_proxy_reader_wrapper(&wr->e, &prd->e, tnow);
}

void update_proxy_participant_endpoint_matching (struct proxy_participant *proxypp, struct participant *pp)
{
  struct entity_index * const entidx = pp->e.gv->entity_index;
  struct proxy_endpoint_common *cep;
  ddsi_guid_t guid;
  ddsi_entityid_t *endpoint_ids;
  uint32_t num = 0, i;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();

  EELOGDISC (&proxypp->e, "update_proxy_participant_endpoint_matching (proxypp "PGUIDFMT" pp "PGUIDFMT")\n",
             PGUID (proxypp->e.guid), PGUID (pp->e.guid));

  ddsrt_mutex_lock(&proxypp->e.lock);
  endpoint_ids = ddsrt_malloc(proxypp->refc * sizeof(ddsi_entityid_t));
  for (cep = proxypp->endpoints; cep != NULL; cep = cep->next_ep)
  {
    struct entity_common *e = entity_common_from_proxy_endpoint_common (cep);
    endpoint_ids[num++] = e->guid.entityid;
  }
  ddsrt_mutex_unlock(&proxypp->e.lock);

  guid.prefix = proxypp->e.guid.prefix;

  for (i = 0; i < num; i++)
  {
    struct entity_common *e;
    enum entity_kind mkind;

    guid.entityid = endpoint_ids[i];
    if ((e = entidx_lookup_guid_untyped(entidx, &guid)) == NULL)
      continue;

    mkind = generic_do_match_mkind (e->kind, false);
    if (!is_builtin_entityid (e->guid.entityid, NN_VENDORID_ECLIPSE))
    {
      struct entidx_enum it;
      struct entity_common *em;
      struct match_entities_range_key max;
      const char *tp = entity_topic_name (e);

      entidx_enum_init_topic(&it, entidx, mkind, tp, &max);
      while ((em = entidx_enum_next_max (&it, &max)) != NULL)
      {
        if (&pp->e == get_entity_parent(em))
          generic_do_match_connect (e, em, tnow, false);
      }
      entidx_enum_fini (&it);
    }
    else
    {
      const ddsi_entityid_t tgt_ent = builtin_entityid_match (e->guid.entityid);
      const ddsi_guid_t tgt_guid = { pp->e.guid.prefix, tgt_ent };

      if (!is_builtin_volatile_endpoint (tgt_ent))
      {
        struct entity_common *ep;
        if ((ep = entidx_lookup_guid (entidx, &tgt_guid, mkind)) != NULL)
          generic_do_match_connect (e, ep, tnow, false);
      }
    }
  }

  ddsrt_free(endpoint_ids);
}

void handshake_end_cb (struct ddsi_handshake *handshake, struct participant *pp, struct proxy_participant *proxypp, enum ddsi_handshake_state result)
{
  const struct ddsi_domaingv * const gv = pp->e.gv;
  int64_t shared_secret;

  switch(result)
  {
  case STATE_HANDSHAKE_PROCESSED:
    shared_secret = ddsi_handshake_get_shared_secret(handshake);
    DDS_CLOG (DDS_LC_DISCOVERY, &gv->logconfig, "handshake (lguid="PGUIDFMT" rguid="PGUIDFMT") processed\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid));
    if (q_omg_security_register_remote_participant(pp, proxypp, shared_secret)) {
      match_volatile_secure_endpoints(pp, proxypp);
      q_omg_security_set_remote_participant_authenticated(pp, proxypp);
    }
    break;

  case STATE_HANDSHAKE_SEND_TOKENS:
    DDS_CLOG (DDS_LC_DISCOVERY, &gv->logconfig, "handshake (lguid="PGUIDFMT" rguid="PGUIDFMT") send tokens\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid));
    q_omg_security_participant_send_tokens(pp, proxypp);
    break;

  case STATE_HANDSHAKE_OK:
    DDS_CLOG (DDS_LC_DISCOVERY, &gv->logconfig, "handshake (lguid="PGUIDFMT" rguid="PGUIDFMT") succeeded\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid));
    update_proxy_participant_endpoint_matching(proxypp, pp);
    ddsi_handshake_remove(pp, proxypp);
    break;

  case STATE_HANDSHAKE_TIMED_OUT:
    DDS_CERROR (&gv->logconfig, "handshake (lguid="PGUIDFMT" rguid="PGUIDFMT") failed: (%d) Timed out\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid), (int)result);
    if (q_omg_participant_allow_unauthenticated(pp)) {
      downgrade_to_nonsecure(proxypp);
      update_proxy_participant_endpoint_matching(proxypp, pp);
    }
    ddsi_handshake_remove(pp, proxypp);
    break;
  case STATE_HANDSHAKE_FAILED:
    DDS_CERROR (&gv->logconfig, "handshake (lguid="PGUIDFMT" rguid="PGUIDFMT") failed: (%d) Failed\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid), (int)result);
    if (q_omg_participant_allow_unauthenticated(pp)) {
      downgrade_to_nonsecure(proxypp);
      update_proxy_participant_endpoint_matching(proxypp, pp);
    }
    ddsi_handshake_remove(pp, proxypp);
    break;
  default:
    DDS_CERROR (&gv->logconfig, "handshake (lguid="PGUIDFMT" rguid="PGUIDFMT") failed: (%d) Unknown failure\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid), (int)result);
    ddsi_handshake_remove(pp, proxypp);
    break;
  }
}

bool proxy_participant_has_pp_match (struct ddsi_domaingv *gv, struct proxy_participant *proxypp)
{
  bool match = false;
  struct participant *pp;
  struct entidx_enum_participant est;

  entidx_enum_participant_init (&est, gv->entity_index);
  while ((pp = entidx_enum_participant_next (&est)) != NULL && !match)
  {
    /* remote secure pp can possibly match with local non-secured pp in case allow-unauthenticated pp
       is enabled in the remote pp's security settings */
    match = !q_omg_participant_is_secure (pp) || q_omg_is_similar_participant_security_info (pp, proxypp);
  }
  entidx_enum_participant_fini (&est);
  return match;
}

void proxy_participant_create_handshakes (struct ddsi_domaingv *gv, struct proxy_participant *proxypp)
{
  struct participant *pp;
  struct entidx_enum_participant est;

  q_omg_security_remote_participant_set_initialized(proxypp);

  entidx_enum_participant_init (&est, gv->entity_index);
  while (((pp = entidx_enum_participant_next (&est)) != NULL)) {
    if (q_omg_security_participant_is_initialized(pp))
      ddsi_handshake_register(pp, proxypp, handshake_end_cb);
  }
  entidx_enum_participant_fini(&est);
}

void disconnect_proxy_participant_secure (struct proxy_participant *proxypp)
{
  struct participant *pp;
  struct entidx_enum_participant it;
  struct ddsi_domaingv * const gv = proxypp->e.gv;

  if (q_omg_proxy_participant_is_secure(proxypp))
  {
    entidx_enum_participant_init (&it, gv->entity_index);
    while ((pp = entidx_enum_participant_next (&it)) != NULL)
    {
      ddsi_handshake_remove(pp, proxypp);
    }
    entidx_enum_participant_fini (&it);
  }
}

#endif /* DDS_HAS_SECURITY */

void update_proxy_endpoint_matching (const struct ddsi_domaingv *gv, struct generic_proxy_endpoint *proxy_ep)
{
  GVLOGDISC ("update_proxy_endpoint_matching (proxy ep "PGUIDFMT")\n", PGUID (proxy_ep->e.guid));
  enum entity_kind mkind = generic_do_match_mkind (proxy_ep->e.kind, false);
  assert (!is_builtin_entityid (proxy_ep->e.guid.entityid, NN_VENDORID_ECLIPSE));
  struct entidx_enum it;
  struct entity_common *em;
  struct match_entities_range_key max;
  const char *tp = entity_topic_name (&proxy_ep->e);
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();

  thread_state_awake (lookup_thread_state (), gv);
  entidx_enum_init_topic (&it, gv->entity_index, mkind, tp, &max);
  while ((em = entidx_enum_next_max (&it, &max)) != NULL)
  {
    GVLOGDISC ("match proxy ep "PGUIDFMT" with "PGUIDFMT"\n", PGUID (proxy_ep->e.guid), PGUID (em->guid));
    generic_do_match_connect (&proxy_ep->e, em, tnow, false);
  }
  entidx_enum_fini (&it);
  thread_state_asleep (lookup_thread_state ());
}