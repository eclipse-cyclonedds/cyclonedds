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
#include <stddef.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsi/ddsi_proxy_participant.h"
#include "dds/ddsi/ddsi_qosmatch.h"
#include "ddsi__entity.h"
#include "ddsi__participant.h"
#include "ddsi__security_omg.h"
#include "ddsi__entity_index.h"
#include "ddsi__mcgroup.h"
#include "ddsi__rhc.h"
#include "ddsi__addrset.h"
#include "ddsi__xevent.h"
#include "ddsi__whc.h"
#include "ddsi__endpoint.h"
#include "ddsi__endpoint_match.h"
#include "ddsi__proxy_endpoint.h"
#include "ddsi__protocol.h"
#include "ddsi__tran.h"
#include "ddsi__typelib.h"
#include "ddsi__vendor.h"
#include "ddsi__lat_estim.h"
#include "ddsi__acknack.h"
#ifdef DDS_HAS_TYPE_DISCOVERY
#include "ddsi__typelookup.h"
#endif
#include "dds/dds.h"

static ddsi_entityid_t builtin_entityid_match (ddsi_entityid_t x)
{
  ddsi_entityid_t res;
  res.u = 0;
  switch (x.u)
  {
    case DDSI_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER:
      res.u = DDSI_ENTITYID_SEDP_BUILTIN_TOPIC_READER;
      break;
#ifdef DDS_HAS_TOPIC_DISCOVERY
    case DDSI_ENTITYID_SEDP_BUILTIN_TOPIC_READER:
      res.u = DDSI_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER;
      break;
#endif
    case DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER:
      res.u = DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER;
      break;
    case DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER:
      res.u = DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER;
      break;
    case DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER:
      res.u = DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER;
      break;
    case DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER:
      res.u = DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER;
      break;
    case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER:
      res.u = DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER;
      break;
    case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER:
      res.u = DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER;
      break;
#ifdef DDS_HAS_TYPE_DISCOVERY
    case DDSI_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER:
      res.u = DDSI_ENTITYID_TL_SVC_BUILTIN_REQUEST_READER;
      break;
    case DDSI_ENTITYID_TL_SVC_BUILTIN_REQUEST_READER:
      res.u = DDSI_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER;
      break;
    case DDSI_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER:
      res.u = DDSI_ENTITYID_TL_SVC_BUILTIN_REPLY_READER;
      break;
    case DDSI_ENTITYID_TL_SVC_BUILTIN_REPLY_READER:
      res.u = DDSI_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER;
      break;
#endif

    case DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER:
    case DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER:
      /* SPDP is special cased because we don't -- indeed can't --
         match readers with writers, only send to matched readers and
         only accept from matched writers. That way discovery wouldn't
         work at all. No entity with DDSI_ENTITYID_UNKNOWN exists,
         ever, so this guarantees no connection will be made. */
      res.u = DDSI_ENTITYID_UNKNOWN;
      break;

    case DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER:
      res.u = DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER;
      break;
    case DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER:
      res.u = DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER;
      break;

    case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER:
      res.u = DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER;
      break;
    case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER:
      res.u = DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER;
      break;
    case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER:
      res.u = DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER;
      break;
    case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER:
      res.u = DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER;
      break;
    case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER:
      res.u = DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER;
      break;
    case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER:
      res.u = DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER;
      break;
    case DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER:
      res.u = DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER;
      break;
    case DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER:
      res.u = DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER;
      break;
    case DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER:
      res.u = DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER;
      break;
    case DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER:
      res.u = DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER;
      break;

    default:
      assert (0);
  }
  return res;
}

static void writer_qos_mismatch (struct ddsi_writer * wr, dds_qos_policy_id_t reason)
{
  /* When the reason is DDS_INVALID_QOS_POLICY_ID, it means that we compared
   * readers/writers from different topics: ignore that. */
  if (reason != DDS_INVALID_QOS_POLICY_ID && wr->status_cb)
  {
    ddsi_status_cb_data_t data;
    data.raw_status_id = (int) DDS_OFFERED_INCOMPATIBLE_QOS_STATUS_ID;
    data.extra = reason;
    (wr->status_cb) (wr->status_cb_entity, &data);
  }
}

static void reader_qos_mismatch (struct ddsi_reader * rd, dds_qos_policy_id_t reason)
{
  /* When the reason is DDS_INVALID_QOS_POLICY_ID, it means that we compared
   * readers/writers from different topics: ignore that. */
  if (reason != DDS_INVALID_QOS_POLICY_ID && rd->status_cb)
  {
    ddsi_status_cb_data_t data;
    data.raw_status_id = (int) DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS_ID;
    data.extra = reason;
    (rd->status_cb) (rd->status_cb_entity, &data);
  }
}

static bool topickind_qos_match_p_lock (struct ddsi_domaingv *gv, struct ddsi_entity_common *rd, const dds_qos_t *rdqos, struct ddsi_entity_common *wr, const dds_qos_t *wrqos, dds_qos_policy_id_t *reason
#ifdef DDS_HAS_TYPE_DISCOVERY
    , const struct ddsi_type_pair *rd_type_pair, const struct ddsi_type_pair *wr_type_pair
#endif
)
{
  assert (ddsi_is_reader_entityid (rd->guid.entityid));
  assert (ddsi_is_writer_entityid (wr->guid.entityid));
  if (ddsi_is_keyed_endpoint_entityid (rd->guid.entityid) != ddsi_is_keyed_endpoint_entityid (wr->guid.entityid))
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
  bool ret = ddsi_qos_match_p (gv, rdqos, wrqos, reason, rd_type_pair, wr_type_pair, &rd_type_lookup, &wr_type_lookup);
  if (!ret)
  {
    /* In case qos_match_p returns false, one of rd_type_look and wr_type_lookup could
       be set to indicate that type information is missing. At this point, we know this
       is the case so do a type lookup request for either rd_type_pair->minimal or
       wr_type_pair->minimal or a dependent type for one of these. */
    if (rd_type_lookup && ddsi_is_proxy_endpoint (rd))
    {
      req_type_id = ddsi_type_pair_minimal_id (rd_type_pair);
      proxypp_guid = &((struct ddsi_generic_proxy_endpoint *) rd)->c.proxypp->e.guid;
    }
    else if (wr_type_lookup && ddsi_is_proxy_endpoint (wr))
    {
      req_type_id = ddsi_type_pair_minimal_id (wr_type_pair);
      proxypp_guid = &((struct ddsi_generic_proxy_endpoint *) wr)->c.proxypp->e.guid;
    }
  }
#else
  bool ret = ddsi_qos_match_p (gv, rdqos, wrqos, reason);
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

void ddsi_connect_writer_with_proxy_reader_secure (struct ddsi_writer *wr, struct ddsi_proxy_reader *prd, ddsrt_mtime_t tnow, int64_t crypto_handle)
{
  DDSRT_UNUSED_ARG(tnow);
  ddsi_proxy_reader_add_connection (prd, wr, crypto_handle);
  ddsi_writer_add_connection (wr, prd, crypto_handle);
}

void ddsi_connect_reader_with_proxy_writer_secure (struct ddsi_reader *rd, struct ddsi_proxy_writer *pwr, ddsrt_mtime_t tnow, int64_t crypto_handle)
{
  ddsi_count_t init_count;
  struct ddsi_alive_state alive_state;

  /* Initialize the reader's tracking information for the writer liveliness state to something
     sensible, but that may be outdated by the time the reader gets added to the writer's list
     of matching readers. */
  ddsi_proxy_writer_get_alive_state (pwr, &alive_state);
  ddsi_reader_add_connection (rd, pwr, &init_count,  &alive_state, crypto_handle);
  ddsi_proxy_writer_add_connection (pwr, rd, tnow, init_count, crypto_handle);

  /* Once everything is set up: update with the latest state, any updates to the alive state
     happening in parallel will cause this to become a no-op. */
  ddsi_proxy_writer_get_alive_state (pwr, &alive_state);
  ddsi_reader_update_notify_pwr_alive_state (rd, pwr, &alive_state);
}

static void connect_writer_with_proxy_reader (struct ddsi_writer *wr, struct ddsi_proxy_reader *prd, ddsrt_mtime_t tnow)
{
  struct ddsi_domaingv *gv = wr->e.gv;
  const int isb0 = (ddsi_is_builtin_entityid (wr->e.guid.entityid, DDSI_VENDORID_ECLIPSE) != 0);
  const int isb1 = (ddsi_is_builtin_entityid (prd->e.guid.entityid, prd->c.vendor) != 0);
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

  if (!ddsi_omg_security_check_remote_reader_permissions (prd, wr->e.gv->config.domainId, wr->c.pp, &relay_only))
  {
    GVLOGDISC ("connect_writer_with_proxy_reader (wr "PGUIDFMT") with (prd "PGUIDFMT") not allowed by security\n", PGUID (wr->e.guid), PGUID (prd->e.guid));
  }
  else if (relay_only)
  {
    GVWARNING ("connect_writer_with_proxy_reader (wr "PGUIDFMT") with (prd "PGUIDFMT") relay_only not supported\n", PGUID (wr->e.guid), PGUID (prd->e.guid));
  }
  else if (!ddsi_omg_security_match_remote_reader_enabled (wr, prd, relay_only, &crypto_handle))
  {
    GVLOGDISC ("connect_writer_with_proxy_reader (wr "PGUIDFMT") with (prd "PGUIDFMT") waiting for approval by security\n", PGUID (wr->e.guid), PGUID (prd->e.guid));
  }
  else
  {
    ddsi_proxy_reader_add_connection (prd, wr, crypto_handle);
    ddsi_writer_add_connection (wr, prd, crypto_handle);
  }
}

static void connect_proxy_writer_with_reader (struct ddsi_proxy_writer *pwr, struct ddsi_reader *rd, ddsrt_mtime_t tnow)
{
  const int isb0 = (ddsi_is_builtin_entityid (pwr->e.guid.entityid, pwr->c.vendor) != 0);
  const int isb1 = (ddsi_is_builtin_entityid (rd->e.guid.entityid, DDSI_VENDORID_ECLIPSE) != 0);
  dds_qos_policy_id_t reason;
  ddsi_count_t init_count;
  struct ddsi_alive_state alive_state;
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

  if (!ddsi_omg_security_check_remote_writer_permissions (pwr, rd->e.gv->config.domainId, rd->c.pp))
  {
    EELOGDISC (&rd->e, "connect_proxy_writer_with_reader (pwr "PGUIDFMT") with (rd "PGUIDFMT") not allowed by security\n",
        PGUID (pwr->e.guid), PGUID (rd->e.guid));
  }
  else if (!ddsi_omg_security_match_remote_writer_enabled (rd, pwr, &crypto_handle))
  {
    EELOGDISC (&rd->e, "connect_proxy_writer_with_reader (pwr "PGUIDFMT") with  (rd "PGUIDFMT") waiting for approval by security\n",
        PGUID (pwr->e.guid), PGUID (rd->e.guid));
  }
  else
  {
    /* Initialize the reader's tracking information for the writer liveliness state to something
       sensible, but that may be outdated by the time the reader gets added to the writer's list
       of matching readers. */
    ddsi_proxy_writer_get_alive_state (pwr, &alive_state);
    ddsi_reader_add_connection (rd, pwr, &init_count, &alive_state, crypto_handle);
    ddsi_proxy_writer_add_connection (pwr, rd, tnow, init_count, crypto_handle);

    /* Once everything is set up: update with the latest state, any updates to the alive state
       happening in parallel will cause this to become a no-op. */
    ddsi_proxy_writer_get_alive_state (pwr, &alive_state);
    ddsi_reader_update_notify_pwr_alive_state (rd, pwr, &alive_state);
  }
}

static bool ignore_local_p (const ddsi_guid_t *guid1, const ddsi_guid_t *guid2, const struct dds_qos *xqos1, const struct dds_qos *xqos2)
{
  assert (xqos1->present & DDSI_QP_CYCLONE_IGNORELOCAL);
  assert (xqos2->present & DDSI_QP_CYCLONE_IGNORELOCAL);
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

static void connect_writer_with_reader (struct ddsi_writer *wr, struct ddsi_reader *rd, ddsrt_mtime_t tnow)
{
  dds_qos_policy_id_t reason;
  struct ddsi_alive_state alive_state;
  (void)tnow;
  if (!ddsi_is_local_orphan_endpoint (&wr->e) && (ddsi_is_builtin_entityid (wr->e.guid.entityid, DDSI_VENDORID_ECLIPSE) || ddsi_is_builtin_entityid (rd->e.guid.entityid, DDSI_VENDORID_ECLIPSE)))
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
  ddsi_writer_get_alive_state (wr, &alive_state);
  ddsi_reader_add_local_connection (rd, wr, &alive_state);
  ddsi_writer_add_local_connection (wr, rd);

  /* Once everything is set up: update with the latest state, any updates to the alive state
     happening in parallel will cause this to become a no-op. */
  ddsi_writer_get_alive_state (wr, &alive_state);
  ddsi_reader_update_notify_wr_alive_state (rd, wr, &alive_state);
}

static void connect_writer_with_proxy_reader_wrapper (struct ddsi_entity_common *vwr, struct ddsi_entity_common *vprd, ddsrt_mtime_t tnow)
{
  struct ddsi_writer *wr = (struct ddsi_writer *) vwr;
  struct ddsi_proxy_reader *prd = (struct ddsi_proxy_reader *) vprd;
  assert (wr->e.kind == DDSI_EK_WRITER);
  assert (prd->e.kind == DDSI_EK_PROXY_READER);
  connect_writer_with_proxy_reader (wr, prd, tnow);
}

static void connect_proxy_writer_with_reader_wrapper (struct ddsi_entity_common *vpwr, struct ddsi_entity_common *vrd, ddsrt_mtime_t tnow)
{
  struct ddsi_proxy_writer *pwr = (struct ddsi_proxy_writer *) vpwr;
  struct ddsi_reader *rd = (struct ddsi_reader *) vrd;
  assert (pwr->e.kind == DDSI_EK_PROXY_WRITER);
  assert (rd->e.kind == DDSI_EK_READER);
  connect_proxy_writer_with_reader (pwr, rd, tnow);
}

static void connect_writer_with_reader_wrapper (struct ddsi_entity_common *vwr, struct ddsi_entity_common *vrd, ddsrt_mtime_t tnow)
{
  struct ddsi_writer *wr = (struct ddsi_writer *) vwr;
  struct ddsi_reader *rd = (struct ddsi_reader *) vrd;
  assert (wr->e.kind == DDSI_EK_WRITER);
  assert (rd->e.kind == DDSI_EK_READER);
  connect_writer_with_reader (wr, rd, tnow);
}

static enum ddsi_entity_kind generic_do_match_mkind (enum ddsi_entity_kind kind, bool local)
{
  switch (kind)
  {
    case DDSI_EK_WRITER: return local ? DDSI_EK_READER : DDSI_EK_PROXY_READER;
    case DDSI_EK_READER: return local ? DDSI_EK_WRITER : DDSI_EK_PROXY_WRITER;
    case DDSI_EK_PROXY_WRITER: assert (!local); return DDSI_EK_READER;
    case DDSI_EK_PROXY_READER: assert (!local); return DDSI_EK_WRITER;
    case DDSI_EK_PARTICIPANT:
    case DDSI_EK_PROXY_PARTICIPANT:
    case DDSI_EK_TOPIC:
      assert(0);
      return DDSI_EK_WRITER;
  }
  assert(0);
  return DDSI_EK_WRITER;
}

static void generic_do_match_connect (struct ddsi_entity_common *e, struct ddsi_entity_common *em, ddsrt_mtime_t tnow, bool local)
{
  switch (e->kind)
  {
    case DDSI_EK_WRITER:
      if (local)
        connect_writer_with_reader_wrapper (e, em, tnow);
      else
        connect_writer_with_proxy_reader_wrapper (e, em, tnow);
      break;
    case DDSI_EK_READER:
      if (local)
        connect_writer_with_reader_wrapper (em, e, tnow);
      else
        connect_proxy_writer_with_reader_wrapper (em, e, tnow);
      break;
    case DDSI_EK_PROXY_WRITER:
      assert (!local);
      connect_proxy_writer_with_reader_wrapper (e, em, tnow);
      break;
    case DDSI_EK_PROXY_READER:
      assert (!local);
      connect_writer_with_proxy_reader_wrapper (em, e, tnow);
      break;
    case DDSI_EK_PARTICIPANT:
    case DDSI_EK_PROXY_PARTICIPANT:
    case DDSI_EK_TOPIC:
      assert(0);
      break;
  }
}

static const char *entity_topic_name (const struct ddsi_entity_common *e)
{
  switch (e->kind)
  {
    case DDSI_EK_WRITER:
      return ((const struct ddsi_writer *) e)->xqos->topic_name;
    case DDSI_EK_READER:
      return ((const struct ddsi_reader *) e)->xqos->topic_name;
    case DDSI_EK_PROXY_WRITER:
    case DDSI_EK_PROXY_READER:
      return ((const struct ddsi_generic_proxy_endpoint *) e)->c.xqos->topic_name;
#ifdef DDS_HAS_TOPIC_DISCOVERY
    case DDSI_EK_TOPIC:
    {
      struct ddsi_topic * topic = (struct ddsi_topic *) e;
      ddsrt_mutex_lock (&topic->e.qos_lock);
      const char * name = topic->definition->xqos->topic_name;
      ddsrt_mutex_unlock (&topic->e.qos_lock);
      return name;
    }
#endif
    case DDSI_EK_PARTICIPANT:
    case DDSI_EK_PROXY_PARTICIPANT:
    default:
      assert (0);
  }
  return "";
}

static void generic_do_match (struct ddsi_entity_common *e, ddsrt_mtime_t tnow, bool local)
{
  static const struct { const char *full; const char *full_us; const char *abbrev; } kindstr[] = {
    [DDSI_EK_WRITER] = { "writer", "writer", "wr" },
    [DDSI_EK_READER] = { "reader", "reader", "rd" },
    [DDSI_EK_PROXY_WRITER] = { "proxy writer", "proxy_writer", "pwr" },
    [DDSI_EK_PROXY_READER] = { "proxy reader", "proxy_reader", "prd" },
    [DDSI_EK_PARTICIPANT] = { "participant", "participant", "pp" },
    [DDSI_EK_PROXY_PARTICIPANT] = { "proxy participant", "proxy_participant", "proxypp" }
  };

  enum ddsi_entity_kind mkind = generic_do_match_mkind (e->kind, local);
  struct ddsi_entity_index const * const entidx = e->gv->entity_index;
  struct ddsi_entity_enum it;
  struct ddsi_entity_common *em;

  if (!ddsi_is_builtin_entityid (e->guid.entityid, DDSI_VENDORID_ECLIPSE) || (local && ddsi_is_local_orphan_endpoint (e)))
  {
    /* Non-builtins need matching on topics, the local orphan endpoints
       are a bit weird because they reuse the builtin entityids but
       otherwise need to be treated as normal readers */
    struct ddsi_match_entities_range_key max;
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
    ddsi_entidx_enum_init_topic (&it, entidx, mkind, tp, &max);
    while ((em = ddsi_entidx_enum_next_max (&it, &max)) != NULL)
      generic_do_match_connect (e, em, tnow, local);
    ddsi_entidx_enum_fini (&it);
  }
  else if (!local)
  {
    /* Built-ins have fixed QoS and a known entity id to use, so instead of
       looking for the right topic, just probe the matching GUIDs for all
       (proxy) participants.  Local matching never needs to look at the
       discovery endpoints */
    const ddsi_entityid_t tgt_ent = builtin_entityid_match (e->guid.entityid);
    const bool isproxy = (e->kind == DDSI_EK_PROXY_WRITER || e->kind == DDSI_EK_PROXY_READER || e->kind == DDSI_EK_PROXY_PARTICIPANT);
    enum ddsi_entity_kind pkind = isproxy ? DDSI_EK_PARTICIPANT : DDSI_EK_PROXY_PARTICIPANT;
    EELOGDISC (e, "match_%s_with_%ss(%s "PGUIDFMT") scanning %sparticipants tgt=%"PRIx32"\n",
               kindstr[e->kind].full_us, kindstr[mkind].full_us,
               kindstr[e->kind].abbrev, PGUID (e->guid),
               isproxy ? "" : "proxy ", tgt_ent.u);
    if (tgt_ent.u != DDSI_ENTITYID_UNKNOWN)
    {
      ddsi_entidx_enum_init (&it, entidx, pkind);
      while ((em = ddsi_entidx_enum_next (&it)) != NULL)
      {
        const ddsi_guid_t tgt_guid = { em->guid.prefix, tgt_ent };
        struct ddsi_entity_common *ep;
        if ((ep = ddsi_entidx_lookup_guid (entidx, &tgt_guid, mkind)) != NULL)
          generic_do_match_connect (e, ep, tnow, local);
      }
      ddsi_entidx_enum_fini (&it);
    }
  }
}

void ddsi_match_writer_with_proxy_readers (struct ddsi_writer *wr, ddsrt_mtime_t tnow)
{
  generic_do_match (&wr->e, tnow, false);
}

void ddsi_match_writer_with_local_readers (struct ddsi_writer *wr, ddsrt_mtime_t tnow)
{
  generic_do_match (&wr->e, tnow, true);
}

void ddsi_match_reader_with_proxy_writers (struct ddsi_reader *rd, ddsrt_mtime_t tnow)
{
  generic_do_match (&rd->e, tnow, false);
}

void ddsi_match_reader_with_local_writers (struct ddsi_reader *rd, ddsrt_mtime_t tnow)
{
  generic_do_match (&rd->e, tnow, true);
}

void ddsi_match_proxy_writer_with_readers (struct ddsi_proxy_writer *pwr, ddsrt_mtime_t tnow)
{
  generic_do_match (&pwr->e, tnow, false);
}

void ddsi_match_proxy_reader_with_writers (struct ddsi_proxy_reader *prd, ddsrt_mtime_t tnow)
{
  generic_do_match(&prd->e, tnow, false);
}

void ddsi_free_wr_prd_match (const struct ddsi_domaingv *gv, const ddsi_guid_t *wr_guid, struct ddsi_wr_prd_match *m)
{
  if (m)
  {
#ifdef DDS_HAS_SECURITY
    ddsi_omg_security_deregister_remote_reader_match (gv, wr_guid, m);
#else
    (void) gv;
    (void) wr_guid;
#endif
    ddsi_lat_estim_fini (&m->hb_to_ack_latency);
    ddsrt_free (m);
  }
}

void ddsi_free_rd_pwr_match (struct ddsi_domaingv *gv, const ddsi_guid_t *rd_guid, struct ddsi_rd_pwr_match *m)
{
  if (m)
  {
#ifdef DDS_HAS_SECURITY
    ddsi_omg_security_deregister_remote_writer_match (gv, rd_guid, m);
#else
    (void) rd_guid;
#endif
#ifdef DDS_HAS_SSM
    if (!ddsi_is_unspec_xlocator (&m->ssm_mc_loc))
    {
      assert (ddsi_is_mcaddr (gv, &m->ssm_mc_loc.c));
      assert (!ddsi_is_unspec_xlocator (&m->ssm_src_loc));
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

void ddsi_free_pwr_rd_match (struct ddsi_pwr_rd_match *m)
{
  if (m)
  {
    if (m->acknack_xevent)
      ddsi_delete_xevent (m->acknack_xevent);
    ddsi_reorder_free (m->u.not_in_sync.reorder);
    ddsrt_free (m);
  }
}

void ddsi_free_prd_wr_match (struct ddsi_prd_wr_match *m)
{
  if (m) ddsrt_free (m);
}

void ddsi_free_rd_wr_match (struct ddsi_rd_wr_match *m)
{
  if (m) ddsrt_free (m);
}

void ddsi_free_wr_rd_match (struct ddsi_wr_rd_match *m)
{
  if (m) ddsrt_free (m);
}

void ddsi_writer_add_connection (struct ddsi_writer *wr, struct ddsi_proxy_reader *prd, int64_t crypto_handle)
{
  struct ddsi_wr_prd_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;
  int pretend_everything_acked;

#ifdef DDS_HAS_SHM
  const bool use_iceoryx = prd->is_iceoryx && !(wr->xqos->ignore_locator_type & DDSI_LOCATOR_KIND_SHEM);
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
    ELOGDISC (wr, "  ddsi_writer_add_connection(wr "PGUIDFMT" prd "PGUIDFMT") - prd is being deleted\n",
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
  ddsi_lat_estim_init (&m->hb_to_ack_latency);
  m->hb_to_ack_latency_tlastlog = ddsrt_time_wallclock ();
  m->t_acknack_accepted.v = 0;
  m->t_nackfrag_accepted.v = 0;

  ddsrt_mutex_lock (&wr->e.lock);
#ifdef DDS_HAS_SHM
  if (pretend_everything_acked || prd->is_iceoryx)
#else
  if (pretend_everything_acked)
#endif
    m->seq = DDSI_MAX_SEQ_NUMBER;
  else
    m->seq = wr->seq;
  m->last_seq = m->seq;
  if (ddsrt_avl_lookup_ipath (&ddsi_wr_readers_treedef, &wr->readers, &prd->e.guid, &path))
  {
    ELOGDISC (wr, "  ddsi_writer_add_connection(wr "PGUIDFMT" prd "PGUIDFMT") - already connected\n",
              PGUID (wr->e.guid), PGUID (prd->e.guid));
    ddsrt_mutex_unlock (&wr->e.lock);
    ddsi_lat_estim_fini (&m->hb_to_ack_latency);
    ddsrt_free (m);
  }
  else
  {
    ELOGDISC (wr, "  ddsi_writer_add_connection(wr "PGUIDFMT" prd "PGUIDFMT") - ack seq %"PRIu64"\n",
              PGUID (wr->e.guid), PGUID (prd->e.guid), m->seq);
    ddsrt_avl_insert_ipath (&ddsi_wr_readers_treedef, &wr->readers, m, &path);
    wr->num_readers++;
    wr->num_reliable_readers += m->is_reliable;
    wr->num_readers_requesting_keyhash += prd->requests_keyhash ? 1 : 0;
    ddsi_rebuild_writer_addrset (wr);
    ddsrt_mutex_unlock (&wr->e.lock);

    if (wr->status_cb)
    {
      ddsi_status_cb_data_t data;
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
        (void) ddsi_resched_xevent_if_earlier (wr->heartbeat_xevent, tnext);
      }
      ddsrt_mutex_unlock (&wr->e.lock);
    }
  }
}

void ddsi_writer_add_local_connection (struct ddsi_writer *wr, struct ddsi_reader *rd)
{
  struct ddsi_wr_rd_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;

  ddsrt_mutex_lock (&wr->e.lock);
  if (ddsrt_avl_lookup_ipath (&ddsi_wr_local_readers_treedef, &wr->local_readers, &rd->e.guid, &path))
  {
    ELOGDISC (wr, "  ddsi_writer_add_local_connection(wr "PGUIDFMT" rd "PGUIDFMT") - already connected\n",
              PGUID (wr->e.guid), PGUID (rd->e.guid));
    ddsrt_mutex_unlock (&wr->e.lock);
    ddsrt_free (m);
    return;
  }

  ELOGDISC (wr, "  ddsi_writer_add_local_connection(wr "PGUIDFMT" rd "PGUIDFMT")",
            PGUID (wr->e.guid), PGUID (rd->e.guid));
  m->rd_guid = rd->e.guid;
  ddsrt_avl_insert_ipath (&ddsi_wr_local_readers_treedef, &wr->local_readers, m, &path);

#ifdef DDS_HAS_SHM
  if (!wr->has_iceoryx || !rd->has_iceoryx)
    ddsi_local_reader_ary_insert(&wr->rdary, rd);
#else
  ddsi_local_reader_ary_insert(&wr->rdary, rd);
#endif

  /* Store available data into the late joining reader when it is reliable (we don't do
     historical data for best-effort data over the wire, so also not locally). */
  if (rd->xqos->reliability.kind > DDS_RELIABILITY_BEST_EFFORT && rd->xqos->durability.kind > DDS_DURABILITY_VOLATILE)
    ddsi_deliver_historical_data (wr, rd);

  ddsrt_mutex_unlock (&wr->e.lock);

  ELOGDISC (wr, "\n");

  if (wr->status_cb)
  {
    ddsi_status_cb_data_t data;
    data.raw_status_id = (int) DDS_PUBLICATION_MATCHED_STATUS_ID;
    data.add = true;
    data.handle = rd->e.iid;
    (wr->status_cb) (wr->status_cb_entity, &data);
  }
}

void ddsi_reader_add_connection (struct ddsi_reader *rd, struct ddsi_proxy_writer *pwr, ddsi_count_t *init_count, const struct ddsi_alive_state *alive_state, int64_t crypto_handle)
{
  struct ddsi_rd_pwr_match *m = ddsrt_malloc (sizeof (*m));
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

  if (ddsrt_avl_lookup_ipath (&ddsi_rd_writers_treedef, &rd->writers, &pwr->e.guid, &path))
  {
    ELOGDISC (rd, "  ddsi_reader_add_connection(pwr "PGUIDFMT" rd "PGUIDFMT") - already connected\n",
              PGUID (pwr->e.guid), PGUID (rd->e.guid));
    ddsrt_mutex_unlock (&rd->e.lock);
    ddsrt_free (m);
  }
  else
  {
    ELOGDISC (rd, "  ddsi_reader_add_connection(pwr "PGUIDFMT" rd "PGUIDFMT")\n",
              PGUID (pwr->e.guid), PGUID (rd->e.guid));

    ddsrt_avl_insert_ipath (&ddsi_rd_writers_treedef, &rd->writers, m, &path);
    rd->num_writers++;
    ddsrt_mutex_unlock (&rd->e.lock);

#ifdef DDS_HAS_SSM
    if (rd->favours_ssm && pwr->supports_ssm)
    {
      /* pwr->supports_ssm is set if ddsi_addrset_contains_ssm(pwr->ssm), so
       any_ssm must succeed. */
      if (!ddsi_addrset_any_uc (pwr->c.as, &m->ssm_src_loc))
        assert (0);
      if (!ddsi_addrset_any_ssm (rd->e.gv, pwr->c.as, &m->ssm_mc_loc))
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
      ddsi_set_unspec_xlocator (&m->ssm_src_loc);
      ddsi_set_unspec_xlocator (&m->ssm_mc_loc);
    }
#endif

    if (rd->status_cb)
    {
      ddsi_status_cb_data_t data;
      data.handle = pwr->e.iid;
      data.add = true;
      data.extra = (uint32_t) (alive_state->alive ? DDSI_LIVELINESS_CHANGED_ADD_ALIVE : DDSI_LIVELINESS_CHANGED_ADD_NOT_ALIVE);

      data.raw_status_id = (int) DDS_SUBSCRIPTION_MATCHED_STATUS_ID;
      (rd->status_cb) (rd->status_cb_entity, &data);

      data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
      (rd->status_cb) (rd->status_cb_entity, &data);
    }
  }
}

void ddsi_reader_add_local_connection (struct ddsi_reader *rd, struct ddsi_writer *wr, const struct ddsi_alive_state *alive_state)
{
  struct ddsi_rd_wr_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;

  m->wr_guid = wr->e.guid;
  m->wr_alive = alive_state->alive;
  m->wr_alive_vclock = alive_state->vclock;

  ddsrt_mutex_lock (&rd->e.lock);

  if (ddsrt_avl_lookup_ipath (&ddsi_rd_local_writers_treedef, &rd->local_writers, &wr->e.guid, &path))
  {
    ELOGDISC (rd, "  ddsi_reader_add_local_connection(wr "PGUIDFMT" rd "PGUIDFMT") - already connected\n",
              PGUID (wr->e.guid), PGUID (rd->e.guid));
    ddsrt_mutex_unlock (&rd->e.lock);
    ddsrt_free (m);
  }
  else
  {
    ELOGDISC (rd, "  ddsi_reader_add_local_connection(wr "PGUIDFMT" rd "PGUIDFMT")\n",
              PGUID (wr->e.guid), PGUID (rd->e.guid));
    ddsrt_avl_insert_ipath (&ddsi_rd_local_writers_treedef, &rd->local_writers, m, &path);
    ddsrt_mutex_unlock (&rd->e.lock);

    if (rd->status_cb)
    {
      ddsi_status_cb_data_t data;
      data.handle = wr->e.iid;
      data.add = true;
      data.extra = (uint32_t) (alive_state->alive ? DDSI_LIVELINESS_CHANGED_ADD_ALIVE : DDSI_LIVELINESS_CHANGED_ADD_NOT_ALIVE);

      data.raw_status_id = (int) DDS_SUBSCRIPTION_MATCHED_STATUS_ID;
      (rd->status_cb) (rd->status_cb_entity, &data);

      data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
      (rd->status_cb) (rd->status_cb_entity, &data);
    }
  }
}

void ddsi_proxy_writer_add_connection (struct ddsi_proxy_writer *pwr, struct ddsi_reader *rd, ddsrt_mtime_t tnow, ddsi_count_t init_count, int64_t crypto_handle)
{
  struct ddsi_pwr_rd_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;

  ddsrt_mutex_lock (&pwr->e.lock);
  if (ddsrt_avl_lookup_ipath (&ddsi_pwr_readers_treedef, &pwr->readers, &rd->e.guid, &path))
    goto already_matched;

  assert (rd->type || ddsi_is_builtin_endpoint (rd->e.guid.entityid, DDSI_VENDORID_ECLIPSE));

#ifdef DDS_HAS_SHM
  const bool use_iceoryx = pwr->is_iceoryx && !(rd->xqos->ignore_locator_type & DDSI_LOCATOR_KIND_SHEM);
#else
  const bool use_iceoryx = false;
#endif

  ELOGDISC (pwr, "  ddsi_proxy_writer_add_connection(pwr "PGUIDFMT" rd "PGUIDFMT")",
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
  if (!rd->reliable)
  {
    /* unreliable readers cannot be out-of-sync */
    m->in_sync = PRMSS_SYNC;
  }
  else if (ddsi_is_builtin_entityid (rd->e.guid.entityid, DDSI_VENDORID_ECLIPSE) && !ddsrt_avl_is_empty (&pwr->readers) && !pwr->filtered)
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
    else if (ddsi_vendor_is_eclipse (pwr->c.vendor))
      m->in_sync = PRMSS_OUT_OF_SYNC;
    else
      m->in_sync = PRMSS_SYNC;
    m->u.not_in_sync.end_of_tl_seq = DDSI_MAX_SEQ_NUMBER;
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
    ddsi_local_reader_ary_setfastpath_ok (&pwr->rdary, false);
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

    if (rd->e.guid.entityid.u == DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER)
    {
      secondary_reorder_maxsamples = pwr->e.gv->config.primary_reorder_maxsamples;
      m->filtered = 1;
    }

    const ddsrt_mtime_t tsched = use_iceoryx ? DDSRT_MTIME_NEVER : ddsrt_mtime_add_duration (tnow, pwr->e.gv->config.preemptive_ack_delay);
    {
      struct ddsi_acknack_xevent_cb_arg arg = { .pwr_guid = pwr->e.guid, .rd_guid = rd->e.guid };
      m->acknack_xevent = ddsi_qxev_callback (pwr->evq, tsched, ddsi_acknack_xevent_cb, &arg, sizeof (arg), false);
    }
    m->u.not_in_sync.reorder =
      ddsi_reorder_new (&pwr->e.gv->logconfig, DDSI_REORDER_MODE_NORMAL, secondary_reorder_maxsamples, pwr->e.gv->config.late_ack_mode);
    pwr->n_reliable_readers++;
  }
  else
  {
    m->acknack_xevent = NULL;
    m->u.not_in_sync.reorder =
      ddsi_reorder_new (&pwr->e.gv->logconfig, DDSI_REORDER_MODE_MONOTONICALLY_INCREASING, pwr->e.gv->config.secondary_reorder_maxsamples, pwr->e.gv->config.late_ack_mode);
  }

  ddsrt_avl_insert_ipath (&ddsi_pwr_readers_treedef, &pwr->readers, m, &path);

#ifdef DDS_HAS_SHM
  if (!pwr->is_iceoryx || !rd->has_iceoryx)
    ddsi_local_reader_ary_insert(&pwr->rdary, rd);
#else
  ddsi_local_reader_ary_insert(&pwr->rdary, rd);
#endif

  ddsrt_mutex_unlock (&pwr->e.lock);
  ddsi_send_entityid_to_pwr (pwr, &rd->e.guid);

  ELOGDISC (pwr, "\n");
  return;

already_matched:
  ELOGDISC (pwr, "  ddsi_proxy_writer_add_connection(pwr "PGUIDFMT" rd "PGUIDFMT") - already connected\n",
            PGUID (pwr->e.guid), PGUID (rd->e.guid));
  ddsrt_mutex_unlock (&pwr->e.lock);
  ddsrt_free (m);
  return;
}

void ddsi_proxy_reader_add_connection (struct ddsi_proxy_reader *prd, struct ddsi_writer *wr, int64_t crypto_handle)
{
  struct ddsi_prd_wr_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;

  m->wr_guid = wr->e.guid;
#ifdef DDS_HAS_SECURITY
  m->crypto_handle = crypto_handle;
#else
  DDSRT_UNUSED_ARG(crypto_handle);
#endif

  ddsrt_mutex_lock (&prd->e.lock);
  if (ddsrt_avl_lookup_ipath (&ddsi_prd_writers_treedef, &prd->writers, &wr->e.guid, &path))
  {
    ELOGDISC (prd, "  ddsi_proxy_reader_add_connection(wr "PGUIDFMT" prd "PGUIDFMT") - already connected\n",
              PGUID (wr->e.guid), PGUID (prd->e.guid));
    ddsrt_mutex_unlock (&prd->e.lock);
    ddsrt_free (m);
  }
  else
  {
    ELOGDISC (prd, "  ddsi_proxy_reader_add_connection(wr "PGUIDFMT" prd "PGUIDFMT")\n",
              PGUID (wr->e.guid), PGUID (prd->e.guid));
    ddsrt_avl_insert_ipath (&ddsi_prd_writers_treedef, &prd->writers, m, &path);
    ddsrt_mutex_unlock (&prd->e.lock);
    ddsi_send_entityid_to_prd (prd, &wr->e.guid);

  }
}

void ddsi_writer_drop_connection (const struct ddsi_guid *wr_guid, const struct ddsi_proxy_reader *prd)
{
  struct ddsi_writer *wr;
  if ((wr = ddsi_entidx_lookup_writer_guid (prd->e.gv->entity_index, wr_guid)) != NULL)
  {
    struct ddsi_whc_node *deferred_free_list = NULL;
    struct ddsi_wr_prd_match *m;
    ddsrt_mutex_lock (&wr->e.lock);
    if ((m = ddsrt_avl_lookup (&ddsi_wr_readers_treedef, &wr->readers, &prd->e.guid)) != NULL)
    {
      struct ddsi_whc_state whcst;
      ddsrt_avl_delete (&ddsi_wr_readers_treedef, &wr->readers, m);
      wr->num_readers--;
      wr->num_reliable_readers -= m->is_reliable;
      wr->num_readers_requesting_keyhash -= prd->requests_keyhash ? 1 : 0;
      ddsi_rebuild_writer_addrset (wr);
      ddsi_remove_acked_messages (wr, &whcst, &deferred_free_list);
    }

    ddsrt_mutex_unlock (&wr->e.lock);
    if (m != NULL && wr->status_cb)
    {
      ddsi_status_cb_data_t data;
      data.raw_status_id = (int) DDS_PUBLICATION_MATCHED_STATUS_ID;
      data.add = false;
      data.handle = prd->e.iid;
      (wr->status_cb) (wr->status_cb_entity, &data);
    }
    ddsi_whc_free_deferred_free_list (wr->whc, deferred_free_list);
    ddsi_free_wr_prd_match (wr->e.gv, &wr->e.guid, m);
  }
}

void ddsi_writer_drop_local_connection (const struct ddsi_guid *wr_guid, struct ddsi_reader *rd)
{
  /* Only called by gc_delete_reader, so we actually have a reader pointer */
  struct ddsi_writer *wr;
  if ((wr = ddsi_entidx_lookup_writer_guid (rd->e.gv->entity_index, wr_guid)) != NULL)
  {
    struct ddsi_wr_rd_match *m;

    ddsrt_mutex_lock (&wr->e.lock);
    if ((m = ddsrt_avl_lookup (&ddsi_wr_local_readers_treedef, &wr->local_readers, &rd->e.guid)) != NULL)
    {
      ddsrt_avl_delete (&ddsi_wr_local_readers_treedef, &wr->local_readers, m);
      ddsi_local_reader_ary_remove (&wr->rdary, rd);
    }
    ddsrt_mutex_unlock (&wr->e.lock);
    if (m != NULL && wr->status_cb)
    {
      ddsi_status_cb_data_t data;
      data.raw_status_id = (int) DDS_PUBLICATION_MATCHED_STATUS_ID;
      data.add = false;
      data.handle = rd->e.iid;
      (wr->status_cb) (wr->status_cb_entity, &data);
    }
    ddsi_free_wr_rd_match (m);
  }
}

void ddsi_reader_drop_connection (const struct ddsi_guid *rd_guid, const struct ddsi_proxy_writer *pwr)
{
  struct ddsi_reader *rd;
  if ((rd = ddsi_entidx_lookup_reader_guid (pwr->e.gv->entity_index, rd_guid)) != NULL)
  {
    struct ddsi_rd_pwr_match *m;
    ddsrt_mutex_lock (&rd->e.lock);
    if ((m = ddsrt_avl_lookup (&ddsi_rd_writers_treedef, &rd->writers, &pwr->e.guid)) != NULL)
    {
      ddsrt_avl_delete (&ddsi_rd_writers_treedef, &rd->writers, m);
      rd->num_writers--;
    }

    ddsrt_mutex_unlock (&rd->e.lock);
    if (m != NULL)
    {
      if (rd->rhc)
      {
        struct ddsi_writer_info wrinfo;
        ddsi_make_writer_info (&wrinfo, &pwr->e, pwr->c.xqos, DDSI_STATUSINFO_UNREGISTER);
        ddsi_rhc_unregister_wr (rd->rhc, &wrinfo);
      }
      if (rd->status_cb)
      {
        ddsi_status_cb_data_t data;
        data.handle = pwr->e.iid;
        data.add = false;
        data.extra = (uint32_t) (m->pwr_alive ? DDSI_LIVELINESS_CHANGED_REMOVE_ALIVE : DDSI_LIVELINESS_CHANGED_REMOVE_NOT_ALIVE);

        data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
        (rd->status_cb) (rd->status_cb_entity, &data);

        data.raw_status_id = (int) DDS_SUBSCRIPTION_MATCHED_STATUS_ID;
        (rd->status_cb) (rd->status_cb_entity, &data);
      }
    }
    ddsi_free_rd_pwr_match (pwr->e.gv, &rd->e.guid, m);
  }
}

void ddsi_reader_drop_local_connection (const struct ddsi_guid *rd_guid, const struct ddsi_writer *wr)
{
  struct ddsi_reader *rd;
  if ((rd = ddsi_entidx_lookup_reader_guid (wr->e.gv->entity_index, rd_guid)) != NULL)
  {
    struct ddsi_rd_wr_match *m;
    ddsrt_mutex_lock (&rd->e.lock);
    if ((m = ddsrt_avl_lookup (&ddsi_rd_local_writers_treedef, &rd->local_writers, &wr->e.guid)) != NULL)
      ddsrt_avl_delete (&ddsi_rd_local_writers_treedef, &rd->local_writers, m);
    ddsrt_mutex_unlock (&rd->e.lock);
    if (m != NULL)
    {
      if (rd->rhc)
      {
        /* FIXME: */
        struct ddsi_writer_info wrinfo;
        ddsi_make_writer_info (&wrinfo, &wr->e, wr->xqos, DDSI_STATUSINFO_UNREGISTER);
        ddsi_rhc_unregister_wr (rd->rhc, &wrinfo);
      }
      if (rd->status_cb)
      {
        ddsi_status_cb_data_t data;
        data.handle = wr->e.iid;
        data.add = false;
        data.extra = (uint32_t) (m->wr_alive ? DDSI_LIVELINESS_CHANGED_REMOVE_ALIVE : DDSI_LIVELINESS_CHANGED_REMOVE_NOT_ALIVE);

        data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
        (rd->status_cb) (rd->status_cb_entity, &data);

        data.raw_status_id = (int) DDS_SUBSCRIPTION_MATCHED_STATUS_ID;
        (rd->status_cb) (rd->status_cb_entity, &data);
      }
    }
    ddsi_free_rd_wr_match (m);
  }
}

void ddsi_proxy_writer_drop_connection (const struct ddsi_guid *pwr_guid, struct ddsi_reader *rd)
{
  /* Only called by gc_delete_reader, so we actually have a reader pointer */
  struct ddsi_proxy_writer *pwr;
  if ((pwr = ddsi_entidx_lookup_proxy_writer_guid (rd->e.gv->entity_index, pwr_guid)) != NULL)
  {
    struct ddsi_pwr_rd_match *m;

    ddsrt_mutex_lock (&pwr->e.lock);
    if ((m = ddsrt_avl_lookup (&ddsi_pwr_readers_treedef, &pwr->readers, &rd->e.guid)) != NULL)
    {
      ddsrt_avl_delete (&ddsi_pwr_readers_treedef, &pwr->readers, m);
      if (m->in_sync != PRMSS_SYNC)
      {
        if (--pwr->n_readers_out_of_sync == 0)
          ddsi_local_reader_ary_setfastpath_ok (&pwr->rdary, true);
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
        ddsi_defrag_notegap (pwr->defrag, 1, pwr->last_seq + 1);
        ddsi_reorder_drop_upto (pwr->reorder, pwr->last_seq + 1);
      }
      ddsi_local_reader_ary_remove (&pwr->rdary, rd);
    }
    ddsrt_mutex_unlock (&pwr->e.lock);
    if (m)
    {
      ddsi_update_reader_init_acknack_count (&rd->e.gv->logconfig, rd->e.gv->entity_index, &rd->e.guid, m->count);
      if (m->filtered)
        ddsi_defrag_prune(pwr->defrag, &m->rd_guid.prefix, m->last_seq);
    }
    ddsi_free_pwr_rd_match (m);
  }
}

void ddsi_proxy_reader_drop_connection (const struct ddsi_guid *prd_guid, struct ddsi_writer *wr)
{
  struct ddsi_proxy_reader *prd;
  if ((prd = ddsi_entidx_lookup_proxy_reader_guid (wr->e.gv->entity_index, prd_guid)) != NULL)
  {
    struct ddsi_prd_wr_match *m;
    ddsrt_mutex_lock (&prd->e.lock);
    m = ddsrt_avl_lookup (&ddsi_prd_writers_treedef, &prd->writers, &wr->e.guid);
    if (m)
    {
      ddsrt_avl_delete (&ddsi_prd_writers_treedef, &prd->writers, m);
    }
    ddsrt_mutex_unlock (&prd->e.lock);
    ddsi_free_prd_wr_match (m);
  }
}

void ddsi_local_reader_ary_init (struct ddsi_local_reader_ary *x)
{
  ddsrt_mutex_init (&x->rdary_lock);
  x->valid = 1;
  x->fastpath_ok = 1;
  x->n_readers = 0;
  x->rdary = ddsrt_malloc (sizeof (*x->rdary));
  x->rdary[0] = NULL;
}

void ddsi_local_reader_ary_fini (struct ddsi_local_reader_ary *x)
{
  ddsrt_free (x->rdary);
  ddsrt_mutex_destroy (&x->rdary_lock);
}

void ddsi_local_reader_ary_insert (struct ddsi_local_reader_ary *x, struct ddsi_reader *rd)
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

void ddsi_local_reader_ary_remove (struct ddsi_local_reader_ary *x, struct ddsi_reader *rd)
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

void ddsi_local_reader_ary_setfastpath_ok (struct ddsi_local_reader_ary *x, bool fastpath_ok)
{
  ddsrt_mutex_lock (&x->rdary_lock);
  if (x->valid)
    x->fastpath_ok = fastpath_ok;
  ddsrt_mutex_unlock (&x->rdary_lock);
}

void ddsi_local_reader_ary_setinvalid (struct ddsi_local_reader_ary *x)
{
  ddsrt_mutex_lock (&x->rdary_lock);
  x->valid = 0;
  x->fastpath_ok = 0;
  ddsrt_mutex_unlock (&x->rdary_lock);
}


#ifdef DDS_HAS_SECURITY

static void downgrade_to_nonsecure (struct ddsi_proxy_participant *proxypp)
{
  const ddsrt_wctime_t tnow = ddsrt_time_wallclock ();
  struct ddsi_guid guid;
  static const struct ddsi_setab setab[] = {
      {DDSI_EK_PROXY_WRITER, DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER},
      {DDSI_EK_PROXY_READER, DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER},
      {DDSI_EK_PROXY_WRITER, DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER},
      {DDSI_EK_PROXY_READER, DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER},
      {DDSI_EK_PROXY_WRITER, DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER},
      {DDSI_EK_PROXY_READER, DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER},
      {DDSI_EK_PROXY_WRITER, DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER},
      {DDSI_EK_PROXY_READER, DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER},
      {DDSI_EK_PROXY_WRITER, DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER},
      {DDSI_EK_PROXY_READER, DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER},
      {DDSI_EK_PROXY_WRITER, DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER},
      {DDSI_EK_PROXY_READER, DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER}
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
    case DDSI_EK_PROXY_READER:
      (void)ddsi_delete_proxy_reader (proxypp->e.gv, &guid, tnow, 0);
      break;
    case DDSI_EK_PROXY_WRITER:
      (void)ddsi_delete_proxy_writer (proxypp->e.gv, &guid, tnow, 0);
      break;
    default:
      assert(0);
    }
  }

  /* Cleanup all kinds of related security information. */
  ddsi_omg_security_deregister_remote_participant (proxypp);
  proxypp->bes &= DDSI_BES_MASK_NON_SECURITY;
}

void ddsi_match_volatile_secure_endpoints (struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp)
{
  struct ddsi_reader *rd;
  struct ddsi_writer *wr;
  struct ddsi_proxy_reader *prd;
  struct ddsi_proxy_writer *pwr;
  ddsi_guid_t guid;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();

  EELOGDISC (&pp->e, "match volatile endpoints (pp "PGUIDFMT") with (proxypp "PGUIDFMT")\n",
             PGUID(pp->e.guid), PGUID(proxypp->e.guid));

  guid = pp->e.guid;
  guid.entityid.u = DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER;
  if ((rd = ddsi_entidx_lookup_reader_guid (pp->e.gv->entity_index, &guid)) == NULL)
    return;

  guid.entityid.u = DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER;
  if ((wr = ddsi_entidx_lookup_writer_guid (pp->e.gv->entity_index, &guid)) == NULL)
    return;

  guid = proxypp->e.guid;
  guid.entityid.u = DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER;
  if ((prd = ddsi_entidx_lookup_proxy_reader_guid (pp->e.gv->entity_index, &guid)) == NULL)
    return;

  guid.entityid.u = DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER;
  if ((pwr = ddsi_entidx_lookup_proxy_writer_guid (pp->e.gv->entity_index, &guid)) == NULL)
    return;

  connect_proxy_writer_with_reader_wrapper(&pwr->e, &rd->e, tnow);
  connect_writer_with_proxy_reader_wrapper(&wr->e, &prd->e, tnow);
}

static struct ddsi_entity_common * get_entity_parent (struct ddsi_entity_common *e)
{
  switch (e->kind)
  {
#ifdef DDS_HAS_TOPIC_DISCOVERY
    case DDSI_EK_TOPIC:
      return &((struct ddsi_topic *)e)->pp->e;
#endif
    case DDSI_EK_WRITER:
      return &((struct ddsi_writer *)e)->c.pp->e;
    case DDSI_EK_READER:
      return &((struct ddsi_reader *)e)->c.pp->e;
    case DDSI_EK_PROXY_WRITER:
      return &((struct ddsi_proxy_writer *)e)->c.proxypp->e;
    case DDSI_EK_PROXY_READER:
      return &((struct ddsi_proxy_reader *)e)->c.proxypp->e;
    case DDSI_EK_PARTICIPANT:
    case DDSI_EK_PROXY_PARTICIPANT:
    default:
      return NULL;
  }
  return NULL;
}

void ddsi_update_proxy_participant_endpoint_matching (struct ddsi_proxy_participant *proxypp, struct ddsi_participant *pp)
{
  struct ddsi_entity_index * const entidx = pp->e.gv->entity_index;
  struct ddsi_proxy_endpoint_common *cep;
  ddsi_guid_t guid;
  ddsi_entityid_t *endpoint_ids;
  uint32_t num = 0, i;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();

  EELOGDISC (&proxypp->e, "ddsi_update_proxy_participant_endpoint_matching (proxypp "PGUIDFMT" pp "PGUIDFMT")\n",
             PGUID (proxypp->e.guid), PGUID (pp->e.guid));

  ddsrt_mutex_lock(&proxypp->e.lock);
  endpoint_ids = ddsrt_malloc(proxypp->refc * sizeof(ddsi_entityid_t));
  for (cep = proxypp->endpoints; cep != NULL; cep = cep->next_ep)
  {
    struct ddsi_entity_common *e = ddsi_entity_common_from_proxy_endpoint_common (cep);
    endpoint_ids[num++] = e->guid.entityid;
  }
  ddsrt_mutex_unlock(&proxypp->e.lock);

  guid.prefix = proxypp->e.guid.prefix;

  for (i = 0; i < num; i++)
  {
    struct ddsi_entity_common *e;
    enum ddsi_entity_kind mkind;

    guid.entityid = endpoint_ids[i];
    if ((e = ddsi_entidx_lookup_guid_untyped (entidx, &guid)) == NULL)
      continue;

    mkind = generic_do_match_mkind (e->kind, false);
    if (!ddsi_is_builtin_entityid (e->guid.entityid, DDSI_VENDORID_ECLIPSE))
    {
      struct ddsi_entity_enum it;
      struct ddsi_entity_common *em;
      struct ddsi_match_entities_range_key max;
      const char *tp = entity_topic_name (e);

      ddsi_entidx_enum_init_topic(&it, entidx, mkind, tp, &max);
      while ((em = ddsi_entidx_enum_next_max (&it, &max)) != NULL)
      {
        if (&pp->e == get_entity_parent(em))
          generic_do_match_connect (e, em, tnow, false);
      }
      ddsi_entidx_enum_fini (&it);
    }
    else
    {
      const ddsi_entityid_t tgt_ent = builtin_entityid_match (e->guid.entityid);
      const ddsi_guid_t tgt_guid = { pp->e.guid.prefix, tgt_ent };

      if (!ddsi_is_builtin_volatile_endpoint (tgt_ent))
      {
        struct ddsi_entity_common *ep;
        if ((ep = ddsi_entidx_lookup_guid (entidx, &tgt_guid, mkind)) != NULL)
          generic_do_match_connect (e, ep, tnow, false);
      }
    }
  }

  ddsrt_free(endpoint_ids);
}

void ddsi_handshake_end_cb (struct ddsi_handshake *handshake, struct ddsi_participant *pp, struct ddsi_proxy_participant *proxypp, enum ddsi_handshake_state result)
{
  const struct ddsi_domaingv * const gv = pp->e.gv;
  int64_t shared_secret;

  switch(result)
  {
  case STATE_HANDSHAKE_PROCESSED:
    shared_secret = ddsi_handshake_get_shared_secret(handshake);
    DDS_CLOG (DDS_LC_DISCOVERY, &gv->logconfig, "handshake (lguid="PGUIDFMT" rguid="PGUIDFMT") processed\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid));
    if (ddsi_omg_security_register_remote_participant (pp, proxypp, shared_secret)) {
      ddsi_match_volatile_secure_endpoints(pp, proxypp);
      ddsi_omg_security_set_remote_participant_authenticated (pp, proxypp);
    }
    break;

  case STATE_HANDSHAKE_SEND_TOKENS:
    DDS_CLOG (DDS_LC_DISCOVERY, &gv->logconfig, "handshake (lguid="PGUIDFMT" rguid="PGUIDFMT") send tokens\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid));
    ddsi_omg_security_participant_send_tokens (pp, proxypp);
    break;

  case STATE_HANDSHAKE_OK:
    DDS_CLOG (DDS_LC_DISCOVERY, &gv->logconfig, "handshake (lguid="PGUIDFMT" rguid="PGUIDFMT") succeeded\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid));
    ddsi_update_proxy_participant_endpoint_matching(proxypp, pp);
    ddsi_handshake_remove(pp, proxypp);
    break;

  case STATE_HANDSHAKE_TIMED_OUT:
    DDS_CERROR (&gv->logconfig, "handshake (lguid="PGUIDFMT" rguid="PGUIDFMT") failed: (%d) Timed out\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid), (int)result);
    if (ddsi_omg_participant_allow_unauthenticated(pp)) {
      downgrade_to_nonsecure(proxypp);
      ddsi_update_proxy_participant_endpoint_matching(proxypp, pp);
    }
    ddsi_handshake_remove(pp, proxypp);
    break;
  case STATE_HANDSHAKE_FAILED:
    DDS_CERROR (&gv->logconfig, "handshake (lguid="PGUIDFMT" rguid="PGUIDFMT") failed: (%d) Failed\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid), (int)result);
    if (ddsi_omg_participant_allow_unauthenticated(pp)) {
      downgrade_to_nonsecure(proxypp);
      ddsi_update_proxy_participant_endpoint_matching(proxypp, pp);
    }
    ddsi_handshake_remove(pp, proxypp);
    break;
  default:
    DDS_CERROR (&gv->logconfig, "handshake (lguid="PGUIDFMT" rguid="PGUIDFMT") failed: (%d) Unknown failure\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid), (int)result);
    ddsi_handshake_remove(pp, proxypp);
    break;
  }
}

bool ddsi_proxy_participant_has_pp_match (struct ddsi_domaingv *gv, struct ddsi_proxy_participant *proxypp)
{
  bool match = false;
  struct ddsi_participant *pp;
  struct ddsi_entity_enum_participant est;

  ddsi_entidx_enum_participant_init (&est, gv->entity_index);
  while ((pp = ddsi_entidx_enum_participant_next (&est)) != NULL && !match)
  {
    /* remote secure pp can possibly match with local non-secured pp in case allow-unauthenticated pp
       is enabled in the remote pp's security settings */
    match = !ddsi_omg_participant_is_secure (pp) || ddsi_omg_is_similar_participant_security_info (pp, proxypp);
  }
  ddsi_entidx_enum_participant_fini (&est);
  return match;
}

void ddsi_proxy_participant_create_handshakes (struct ddsi_domaingv *gv, struct ddsi_proxy_participant *proxypp)
{
  struct ddsi_participant *pp;
  struct ddsi_entity_enum_participant est;

  ddsi_omg_security_remote_participant_set_initialized (proxypp);

  ddsi_entidx_enum_participant_init (&est, gv->entity_index);
  while (((pp = ddsi_entidx_enum_participant_next (&est)) != NULL)) {
    if (ddsi_omg_security_participant_is_initialized (pp))
      ddsi_handshake_register(pp, proxypp, ddsi_handshake_end_cb);
  }
  ddsi_entidx_enum_participant_fini(&est);
}

void ddsi_disconnect_proxy_participant_secure (struct ddsi_proxy_participant *proxypp)
{
  struct ddsi_participant *pp;
  struct ddsi_entity_enum_participant it;
  struct ddsi_domaingv * const gv = proxypp->e.gv;

  if (ddsi_omg_proxy_participant_is_secure (proxypp))
  {
    ddsi_entidx_enum_participant_init (&it, gv->entity_index);
    while ((pp = ddsi_entidx_enum_participant_next (&it)) != NULL)
    {
      ddsi_handshake_remove(pp, proxypp);
    }
    ddsi_entidx_enum_participant_fini (&it);
  }
}

#endif /* DDS_HAS_SECURITY */

void ddsi_update_proxy_endpoint_matching (const struct ddsi_domaingv *gv, struct ddsi_generic_proxy_endpoint *proxy_ep)
{
  GVLOGDISC ("ddsi_update_proxy_endpoint_matching (proxy ep "PGUIDFMT")\n", PGUID (proxy_ep->e.guid));
  enum ddsi_entity_kind mkind = generic_do_match_mkind (proxy_ep->e.kind, false);
  assert (!ddsi_is_builtin_entityid (proxy_ep->e.guid.entityid, DDSI_VENDORID_ECLIPSE));
  struct ddsi_entity_enum it;
  struct ddsi_entity_common *em;
  struct ddsi_match_entities_range_key max;
  const char *tp = entity_topic_name (&proxy_ep->e);
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();

  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv);
  ddsi_entidx_enum_init_topic (&it, gv->entity_index, mkind, tp, &max);
  while ((em = ddsi_entidx_enum_next_max (&it, &max)) != NULL)
  {
    GVLOGDISC ("match proxy ep "PGUIDFMT" with "PGUIDFMT"\n", PGUID (proxy_ep->e.guid), PGUID (em->guid));
    generic_do_match_connect (&proxy_ep->e, em, tnow, false);
  }
  ddsi_entidx_enum_fini (&it);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
}

dds_return_t ddsi_writer_get_matched_subscriptions (struct ddsi_writer *wr, uint64_t *rds, size_t nrds)
{
  size_t nrds_act = 0;
  ddsrt_avl_iter_t it;
  struct ddsi_domaingv *gv = wr->e.gv;
  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv);
  ddsrt_mutex_lock (&wr->e.lock);
  for (const struct ddsi_wr_prd_match *m = ddsrt_avl_iter_first (&ddsi_wr_readers_treedef, &wr->readers, &it);
        m != NULL;
        m = ddsrt_avl_iter_next (&it))
  {
    struct ddsi_proxy_reader *prd;
    if ((prd = ddsi_entidx_lookup_proxy_reader_guid (gv->entity_index, &m->prd_guid)) != NULL)
    {
      if (nrds_act < nrds)
        rds[nrds_act] = prd->e.iid;
      nrds_act++;
    }
  }
  for (const struct ddsi_wr_rd_match *m = ddsrt_avl_iter_first (&ddsi_wr_local_readers_treedef, &wr->local_readers, &it);
        m != NULL;
        m = ddsrt_avl_iter_next (&it))
  {
    struct ddsi_reader *rd;
    if ((rd = ddsi_entidx_lookup_reader_guid (gv->entity_index, &m->rd_guid)) != NULL)
    {
      if (nrds_act < nrds)
        rds[nrds_act] = rd->e.iid;
      nrds_act++;
    }
  }
  ddsrt_mutex_unlock (&wr->e.lock);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());

  /* FIXME: is it really true that there can not be more than INT32_MAX matching readers?
      (in practice it'll come to a halt long before that) */
  assert (nrds_act <= INT32_MAX);

  return (dds_return_t) nrds_act;
}

dds_return_t ddsi_reader_get_matched_publications (struct ddsi_reader *rd, uint64_t *wrs, size_t nwrs)
{
  size_t nwrs_act = 0;
  ddsrt_avl_iter_t it;
  struct ddsi_domaingv *gv = rd->e.gv;
  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv);
  ddsrt_mutex_lock (&rd->e.lock);
  for (const struct ddsi_rd_pwr_match *m = ddsrt_avl_iter_first (&ddsi_rd_writers_treedef, &rd->writers, &it);
        m != NULL;
        m = ddsrt_avl_iter_next (&it))
  {
    struct ddsi_proxy_writer *pwr;
    if ((pwr = ddsi_entidx_lookup_proxy_writer_guid (gv->entity_index, &m->pwr_guid)) != NULL)
    {
      if (nwrs_act < nwrs)
        wrs[nwrs_act] = pwr->e.iid;
      nwrs_act++;
    }
  }
  for (const struct ddsi_rd_wr_match *m = ddsrt_avl_iter_first (&ddsi_rd_local_writers_treedef, &rd->local_writers, &it);
        m != NULL;
        m = ddsrt_avl_iter_next (&it))
  {
    struct ddsi_writer *wr;
    if ((wr = ddsi_entidx_lookup_writer_guid (gv->entity_index, &m->wr_guid)) != NULL)
    {
      if (nwrs_act < nwrs)
        wrs[nwrs_act] = wr->e.iid;
      nwrs_act++;
    }
  }
  ddsrt_mutex_unlock (&rd->e.lock);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());

  /* FIXME: is it really true that there can not be more than INT32_MAX matching readers?
    (in practice it'll come to a halt long before that) */
  assert (nwrs_act <= INT32_MAX);

  return (dds_return_t) nwrs_act;
}

bool ddsi_writer_find_matched_reader (struct ddsi_writer *wr, uint64_t ih, struct ddsi_entity_common **rdc, struct dds_qos **rdqos, struct ddsi_entity_common **ppc)
{
  /* FIXME: this ought not be so inefficient */
  struct ddsi_domaingv *gv = wr->e.gv;
  bool found = false;
  ddsrt_avl_iter_t it;
  assert (ddsi_thread_is_awake ());
  ddsrt_mutex_lock (&wr->e.lock);
  for (const struct ddsi_wr_prd_match *m = ddsrt_avl_iter_first (&ddsi_wr_readers_treedef, &wr->readers, &it);
        m != NULL && !found;
        m = ddsrt_avl_iter_next (&it))
  {
    struct ddsi_proxy_reader *prd;
    if ((prd = ddsi_entidx_lookup_proxy_reader_guid (gv->entity_index, &m->prd_guid)) != NULL && prd->e.iid == ih)
    {
      found = true;
      *rdc = &prd->e;
      *rdqos = prd->c.xqos;
      *ppc = &prd->c.proxypp->e;
    }
  }
  for (const struct ddsi_wr_rd_match *m = ddsrt_avl_iter_first (&ddsi_wr_local_readers_treedef, &wr->local_readers, &it);
        m != NULL && !found;
        m = ddsrt_avl_iter_next (&it))
  {
    struct ddsi_reader *rd;
    if ((rd = ddsi_entidx_lookup_reader_guid (gv->entity_index, &m->rd_guid)) != NULL && rd->e.iid == ih)
    {
      found = true;
      *rdc = &rd->e;
      *rdqos = rd->xqos;
      *ppc = &rd->c.pp->e;
    }
  }
  ddsrt_mutex_unlock (&wr->e.lock);
  return found;
}

bool ddsi_reader_find_matched_writer (struct ddsi_reader *rd, uint64_t ih, struct ddsi_entity_common **wrc, struct dds_qos **wrqos, struct ddsi_entity_common **ppc)
{
  /* FIXME: this ought not be so inefficient */
  struct ddsi_domaingv *gv = rd->e.gv;
  bool found = false;
  ddsrt_avl_iter_t it;
  assert (ddsi_thread_is_awake ());
  ddsrt_mutex_lock (&rd->e.lock);
  for (const struct ddsi_rd_pwr_match *m = ddsrt_avl_iter_first (&ddsi_rd_writers_treedef, &rd->writers, &it);
        m != NULL && !found;
        m = ddsrt_avl_iter_next (&it))
  {
    struct ddsi_proxy_writer *pwr;
    if ((pwr = ddsi_entidx_lookup_proxy_writer_guid (gv->entity_index, &m->pwr_guid)) != NULL && pwr->e.iid == ih)
    {
      found = true;
      *wrc = &pwr->e;
      *wrqos = pwr->c.xqos;
      *ppc = &pwr->c.proxypp->e;
    }
  }
  for (const struct ddsi_rd_wr_match *m = ddsrt_avl_iter_first (&ddsi_rd_local_writers_treedef, &rd->local_writers, &it);
        m != NULL && !found;
        m = ddsrt_avl_iter_next (&it))
  {
    struct ddsi_writer *wr;
    if ((wr = ddsi_entidx_lookup_writer_guid (gv->entity_index, &m->wr_guid)) != NULL && wr->e.iid == ih)
    {
      found = true;
      *wrc = &wr->e;
      *wrqos = wr->xqos;
      *ppc = &wr->c.pp->e;
    }
  }
  ddsrt_mutex_unlock (&rd->e.lock);
  return found;
}
