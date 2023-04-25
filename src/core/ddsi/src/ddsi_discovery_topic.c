// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__discovery.h"
#include "ddsi__discovery_topic.h"
#include "ddsi__entity_index.h"
#include "ddsi__entity.h"
#include "ddsi__participant.h"
#include "ddsi__transmit.h"
#include "ddsi__security_omg.h"
#include "ddsi__endpoint.h"
#include "ddsi__plist.h"
#include "ddsi__topic.h"
#include "ddsi__vendor.h"
#include "ddsi__xqos.h"
#include "ddsi__typelib.h"

static int ddsi_sedp_write_topic_impl (struct ddsi_writer *wr, int alive, const ddsi_guid_t *guid, const dds_qos_t *xqos, ddsi_typeinfo_t *type_info)
{
  struct ddsi_domaingv * const gv = wr->e.gv;
  const dds_qos_t *defqos = &ddsi_default_qos_topic;

  ddsi_plist_t ps;
  ddsi_plist_init_empty (&ps);
  ps.present |= PP_CYCLONE_TOPIC_GUID;
  ps.topic_guid = *guid;

  assert (xqos != NULL);
  ps.present |= PP_PROTOCOL_VERSION | PP_VENDORID;
  ps.protocol_version.major = DDSI_RTPS_MAJOR;
  ps.protocol_version.minor = DDSI_RTPS_MINOR;
  ps.vendorid = DDSI_VENDORID_ECLIPSE;

  uint64_t qosdiff = ddsi_xqos_delta (xqos, defqos, ~(uint64_t)0);
  if (gv->config.explicitly_publish_qos_set_to_default)
    qosdiff |= ~DDSI_QP_UNRECOGNIZED_INCOMPATIBLE_MASK;

  if (type_info)
  {
    ps.qos.type_information = type_info;
    ps.qos.present |= DDSI_QP_TYPE_INFORMATION;
  }
  if (xqos)
    ddsi_xqos_mergein_missing (&ps.qos, xqos, qosdiff);
  return ddsi_write_and_fini_plist (wr, &ps, alive);
}

int ddsi_sedp_write_topic (struct ddsi_topic *tp, bool alive)
{
  int res = 0;
  if (!(tp->pp->bes & DDSI_DISC_BUILTIN_ENDPOINT_TOPICS_ANNOUNCER))
    return res;
  if (!ddsi_is_builtin_entityid (tp->e.guid.entityid, DDSI_VENDORID_ECLIPSE) && !tp->e.onlylocal)
  {
    unsigned entityid = ddsi_determine_topic_writer (tp);
    struct ddsi_writer *sedp_wr = ddsi_get_sedp_writer (tp->pp, entityid);
    ddsrt_mutex_lock (&tp->e.qos_lock);
    // the allocation type info object is freed with the plist
    res = ddsi_sedp_write_topic_impl (sedp_wr, alive, &tp->e.guid, tp->definition->xqos, ddsi_type_pair_complete_info (tp->e.gv, tp->definition->type_pair));
    ddsrt_mutex_unlock (&tp->e.qos_lock);
  }
  return res;
}

static const char *durability_to_string (dds_durability_kind_t k)
{
  switch (k)
  {
    case DDS_DURABILITY_VOLATILE: return "volatile";
    case DDS_DURABILITY_TRANSIENT_LOCAL: return "transient-local";
    case DDS_DURABILITY_TRANSIENT: return "transient";
    case DDS_DURABILITY_PERSISTENT: return "persistent";
  }
  return "undefined-durability";
}

void ddsi_handle_sedp_alive_topic (const struct ddsi_receiver_state *rst, ddsi_seqno_t seq, ddsi_plist_t *datap /* note: potentially modifies datap */, const ddsi_guid_prefix_t *src_guid_prefix, ddsi_vendorid_t vendorid, ddsrt_wctime_t timestamp)
{
  struct ddsi_domaingv * const gv = rst->gv;
  struct ddsi_proxy_participant *proxypp;
  ddsi_guid_t ppguid;
  dds_qos_t *xqos;
  int reliable;
  const ddsi_typeid_t *type_id_minimal = NULL, *type_id_complete = NULL;

  assert (datap);
  assert (datap->present & PP_CYCLONE_TOPIC_GUID);
  GVLOGDISC (" "PGUIDFMT, PGUID (datap->topic_guid));

  if (!ddsi_handle_sedp_checks (gv, SEDP_KIND_TOPIC, &datap->topic_guid, datap, src_guid_prefix, vendorid, timestamp, &proxypp, &ppguid))
    return;

  xqos = &datap->qos;
  ddsi_xqos_mergein_missing (xqos, &ddsi_default_qos_topic, ~(uint64_t)0);
  /* After copy + merge, should have at least the ones present in the
     input. Also verify reliability and durability are present,
     because we explicitly read those. */
  assert ((xqos->present & datap->qos.present) == datap->qos.present);
  assert (xqos->present & DDSI_QP_RELIABILITY);
  assert (xqos->present & DDSI_QP_DURABILITY);
  reliable = (xqos->reliability.kind == DDS_RELIABILITY_RELIABLE);

  GVLOGDISC (" %s %s %s: %s/%s",
             reliable ? "reliable" : "best-effort",
             durability_to_string (xqos->durability.kind),
             "topic", xqos->topic_name, xqos->type_name);
  if (xqos->present & DDSI_QP_TYPE_INFORMATION)
  {
    struct ddsi_typeid_str strm, strc;
    type_id_minimal = ddsi_typeinfo_minimal_typeid (xqos->type_information);
    type_id_complete = ddsi_typeinfo_complete_typeid (xqos->type_information);
    GVLOGDISC (" tid %s/%s", ddsi_make_typeid_str(&strm, type_id_minimal), ddsi_make_typeid_str(&strc, type_id_complete));
  }
  GVLOGDISC (" QOS={");
  ddsi_xqos_log (DDS_LC_DISCOVERY, &gv->logconfig, xqos);
  GVLOGDISC ("}\n");

  if ((datap->topic_guid.entityid.u & DDSI_ENTITYID_SOURCE_MASK) == DDSI_ENTITYID_SOURCE_VENDOR && !ddsi_vendor_is_eclipse_or_adlink (vendorid))
  {
    GVLOGDISC ("ignoring vendor-specific topic "PGUIDFMT"\n", PGUID (datap->topic_guid));
  }
  else
  {
    // FIXME: check compatibility with known topic definitions
    struct ddsi_proxy_topic *ptp = ddsi_lookup_proxy_topic (proxypp, &datap->topic_guid);
    if (ptp)
    {
      GVLOGDISC (" update known proxy-topic%s\n", ddsi_vendor_is_cloud (vendorid) ? "-DS" : "");
      ddsi_update_proxy_topic (proxypp, ptp, seq, xqos, timestamp);
    }
    else
    {
      GVLOGDISC (" NEW proxy-topic");
      if (ddsi_new_proxy_topic (proxypp, seq, &datap->topic_guid, type_id_minimal, type_id_complete, xqos, timestamp) != DDS_RETCODE_OK)
        GVLOGDISC (" failed");
    }
  }
}

void ddsi_handle_sedp_dead_topic (const struct ddsi_receiver_state *rst, ddsi_plist_t *datap, ddsrt_wctime_t timestamp)
{
  struct ddsi_proxy_participant *proxypp;
  struct ddsi_proxy_topic *proxytp;
  struct ddsi_domaingv * const gv = rst->gv;
  assert (datap->present & PP_CYCLONE_TOPIC_GUID);
  GVLOGDISC (" "PGUIDFMT" ", PGUID (datap->topic_guid));
  if (!ddsi_check_sedp_kind_and_guid (SEDP_KIND_TOPIC, &datap->topic_guid))
    return;
  ddsi_guid_t ppguid = { .prefix = datap->topic_guid.prefix, .entityid.u = DDSI_ENTITYID_PARTICIPANT };
  if ((proxypp = ddsi_entidx_lookup_proxy_participant_guid (gv->entity_index, &ppguid)) == NULL)
    GVLOGDISC (" unknown proxypp\n");
  else if ((proxytp = ddsi_lookup_proxy_topic (proxypp, &datap->topic_guid)) == NULL)
    GVLOGDISC (" unknown proxy topic\n");
  else
  {
    ddsrt_mutex_lock (&proxypp->e.lock);
    int res = ddsi_delete_proxy_topic_locked (proxypp, proxytp, timestamp);
    GVLOGDISC (" %s\n", res == DDS_RETCODE_PRECONDITION_NOT_MET ? " already-deleting" : " delete");
    ddsrt_mutex_unlock (&proxypp->e.lock);
  }
}
