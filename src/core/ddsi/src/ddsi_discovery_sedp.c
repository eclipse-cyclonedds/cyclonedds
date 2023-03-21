/*
 * Copyright(c) 2023 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__discovery_sedp.h"
#include "ddsi__discovery_endpoint.h"
#ifdef DDS_HAS_TOPIC_DISCOVERY
#include "ddsi__discovery_topic.h"
#endif
#include "ddsi__serdata_plist.h"
#include "ddsi__entity_index.h"
#include "ddsi__entity.h"
#include "ddsi__security_omg.h"
#include "ddsi__endpoint.h"
#include "ddsi__plist.h"
#include "ddsi__proxy_participant.h"
#include "ddsi__topic.h"
#include "ddsi__vendor.h"

struct ddsi_writer *ddsi_get_sedp_writer (const struct ddsi_participant *pp, unsigned entityid)
{
  struct ddsi_writer *sedp_wr = ddsi_get_builtin_writer (pp, entityid);
  if (sedp_wr == NULL)
    DDS_FATAL ("sedp_write_writer: no SEDP builtin writer %x for "PGUIDFMT"\n", entityid, PGUID (pp->e.guid));
  return sedp_wr;
}

struct ddsi_proxy_participant *ddsi_implicitly_create_proxypp (struct ddsi_domaingv *gv, const ddsi_guid_t *ppguid, ddsi_plist_t *datap /* note: potentially modifies datap */, const ddsi_guid_prefix_t *src_guid_prefix, ddsi_vendorid_t vendorid, ddsrt_wctime_t timestamp, ddsi_seqno_t seq)
{
  ddsi_guid_t privguid;
  ddsi_plist_t pp_plist;

  if (memcmp (&ppguid->prefix, src_guid_prefix, sizeof (ppguid->prefix)) == 0)
    /* if the writer is owned by the participant itself, we're not interested */
    return NULL;

  privguid.prefix = *src_guid_prefix;
  privguid.entityid = ddsi_to_entityid (DDSI_ENTITYID_PARTICIPANT);
  ddsi_plist_init_empty(&pp_plist);

  if (ddsi_vendor_is_cloud (vendorid))
  {
    ddsi_vendorid_t actual_vendorid;
    /* Some endpoint that we discovered through the DS, but then it must have at least some locators */
    GVTRACE (" from-DS %"PRIx32":%"PRIx32":%"PRIx32":%"PRIx32, PGUID (privguid));
    /* avoid "no address" case, so we never create the proxy participant for nothing (FIXME: rework some of this) */
    if (!(datap->present & (PP_UNICAST_LOCATOR | PP_MULTICAST_LOCATOR)))
    {
      GVTRACE (" data locator absent\n");
      goto err;
    }
    GVTRACE (" new-proxypp "PGUIDFMT"\n", PGUID (*ppguid));
    /* We need to handle any source of entities, but we really want to try to keep the GIDs (and
       certainly the systemId component) unchanged for OSPL.  The new proxy participant will take
       the GID from the GUID if it is from a "modern" OSPL that advertises it includes all GIDs in
       the endpoint discovery; else if it is OSPL it will take at the systemId and fake the rest.
       However, (1) Cloud filters out the GIDs from the discovery, and (2) DDSI2 deliberately
       doesn't include the GID for internally generated endpoints (such as the fictitious transient
       data readers) to signal that these are internal and have no GID (and not including a GID if
       there is none is quite a reasonable approach).  Point (2) means we have no reliable way of
       determining whether GIDs are included based on the first endpoint, and so there is no point
       doing anything about (1).  That means we fall back to the legacy mode of locally generating
       GIDs but leaving the system id unchanged if the remote is OSPL.  */
    actual_vendorid = (datap->present & PP_VENDORID) ?  datap->vendorid : vendorid;
    (void) ddsi_new_proxy_participant (gv, ppguid, 0, &privguid, ddsi_new_addrset(), ddsi_new_addrset(), &pp_plist, DDS_INFINITY, actual_vendorid, DDSI_CF_IMPLICITLY_CREATED_PROXYPP, timestamp, seq);
  }
  else if (ppguid->prefix.u[0] == src_guid_prefix->u[0] && ddsi_vendor_is_eclipse_or_opensplice (vendorid))
  {
    /* FIXME: requires address sets to be those of ddsi2, no built-in
       readers or writers, only if remote ddsi2 is provably running
       with a minimal built-in endpoint set */
    struct ddsi_proxy_participant *privpp;
    if ((privpp = ddsi_entidx_lookup_proxy_participant_guid (gv->entity_index, &privguid)) == NULL) {
      GVTRACE (" unknown-src-proxypp?\n");
      goto err;
    } else if (!privpp->is_ddsi2_pp) {
      GVTRACE (" src-proxypp-not-ddsi2?\n");
      goto err;
    } else if (!privpp->minimal_bes_mode) {
      GVTRACE (" src-ddsi2-not-minimal-bes-mode?\n");
      goto err;
    } else {
      struct ddsi_addrset *as_default, *as_meta;
      ddsi_plist_t tmp_plist;
      GVTRACE (" from-ddsi2 "PGUIDFMT, PGUID (privguid));
      ddsi_plist_init_empty (&pp_plist);

      ddsrt_mutex_lock (&privpp->e.lock);
      as_default = ddsi_ref_addrset(privpp->as_default);
      as_meta = ddsi_ref_addrset(privpp->as_meta);
      /* copy just what we need */
      tmp_plist = *privpp->plist;
      tmp_plist.present = PP_PARTICIPANT_GUID | PP_ADLINK_PARTICIPANT_VERSION_INFO;
      tmp_plist.participant_guid = *ppguid;
      ddsi_plist_mergein_missing (&pp_plist, &tmp_plist, ~(uint64_t)0, ~(uint64_t)0);
      ddsrt_mutex_unlock (&privpp->e.lock);

      pp_plist.adlink_participant_version_info.flags &= ~DDSI_ADLINK_FL_PARTICIPANT_IS_DDSI2;
      ddsi_new_proxy_participant (gv, ppguid, 0, &privguid, as_default, as_meta, &pp_plist, DDS_INFINITY, vendorid, DDSI_CF_IMPLICITLY_CREATED_PROXYPP | DDSI_CF_PROXYPP_NO_SPDP, timestamp, seq);
    }
  }

 err:
  ddsi_plist_fini (&pp_plist);
  return ddsi_entidx_lookup_proxy_participant_guid (gv->entity_index, ppguid);
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

bool ddsi_handle_sedp_checks (struct ddsi_domaingv * const gv, ddsi_sedp_kind_t sedp_kind, ddsi_guid_t *entity_guid, ddsi_plist_t *datap,
    const ddsi_guid_prefix_t *src_guid_prefix, ddsi_vendorid_t vendorid, ddsrt_wctime_t timestamp,
    struct ddsi_proxy_participant **proxypp, ddsi_guid_t *ppguid)
{
#define E(msg, lbl) do { GVLOGDISC (msg); return false; } while (0)
  if (!ddsi_check_sedp_kind_and_guid (sedp_kind, entity_guid))
    E (" SEDP topic/GUID entity kind mismatch\n", err);
  ppguid->prefix = entity_guid->prefix;
  ppguid->entityid.u = DDSI_ENTITYID_PARTICIPANT;
  // Accept the presence of a participant GUID, but only if it matches
  if ((datap->present & PP_PARTICIPANT_GUID) && memcmp (&datap->participant_guid, ppguid, sizeof (*ppguid)) != 0)
    E (" endpoint/participant GUID mismatch", err);
  if (ddsi_is_deleted_participant_guid (gv->deleted_participants, ppguid, DDSI_DELETED_PPGUID_REMOTE))
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
  {
    GVLOGDISC (" unknown-proxypp");
    if ((*proxypp = ddsi_implicitly_create_proxypp (gv, ppguid, datap, src_guid_prefix, vendorid, timestamp, 0)) == NULL)
      E ("?\n", err);
    /* Repeat regular SEDP trace for convenience */
    GVLOGDISC ("SEDP ST0 "PGUIDFMT" (cont)", PGUID (*entity_guid));
  }
  return true;
#undef E
}

void ddsi_handle_sedp (const struct ddsi_receiver_state *rst, ddsi_seqno_t seq, struct ddsi_serdata *serdata, ddsi_sedp_kind_t sedp_kind)
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
            ddsi_handle_sedp_alive_topic (rst, seq, &decoded_data, &rst->src_guid_prefix, rst->vendor, serdata->timestamp);
#endif
            break;
          case SEDP_KIND_READER:
          case SEDP_KIND_WRITER:
            ddsi_handle_sedp_alive_endpoint (rst, seq, &decoded_data, sedp_kind, &rst->src_guid_prefix, rst->vendor, serdata->timestamp);
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
