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

#include "dds/version.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__discovery_spdp.h"
#include "ddsi__discovery_addrset.h"
#include "ddsi__discovery_endpoint.h"
#include "ddsi__serdata_plist.h"
#include "ddsi__entity_index.h"
#include "ddsi__entity.h"
#include "ddsi__security_omg.h"
#include "ddsi__endpoint.h"
#include "ddsi__plist.h"
#include "ddsi__proxy_participant.h"
#include "ddsi__topic.h"
#include "ddsi__vendor.h"
#include "ddsi__xevent.h"
#include "ddsi__transmit.h"
#include "ddsi__lease.h"
#include "ddsi__xqos.h"

static void maybe_add_pp_as_meta_to_as_disc (struct ddsi_domaingv *gv, const struct ddsi_addrset *as_meta)
{
  if (ddsi_addrset_empty_mc (as_meta) || !(gv->config.allowMulticast & DDSI_AMC_SPDP))
  {
    ddsi_xlocator_t loc;
    if (ddsi_addrset_any_uc (as_meta, &loc))
    {
      ddsi_add_xlocator_to_addrset (gv, gv->as_disc, &loc);
    }
  }
}

struct locators_builder {
  ddsi_locators_t *dst;
  struct ddsi_locators_one *storage;
  size_t storage_n;
};

static struct locators_builder locators_builder_init (ddsi_locators_t *dst, struct ddsi_locators_one *storage, size_t storage_n)
{
  dst->n = 0;
  dst->first = NULL;
  dst->last = NULL;
  return (struct locators_builder) {
    .dst = dst,
    .storage = storage,
    .storage_n = storage_n
  };
}

static bool locators_add_one (struct locators_builder *b, const ddsi_locator_t *loc, uint32_t port_override)
{
  if (b->dst->n >= b->storage_n)
    return false;
  if (b->dst->n == 0)
    b->dst->first = b->storage;
  else
    b->dst->last->next = &b->storage[b->dst->n];
  b->dst->last = &b->storage[b->dst->n++];
  b->dst->last->loc = *loc;
  if (port_override != DDSI_LOCATOR_PORT_INVALID)
    b->dst->last->loc.port = port_override;
  b->dst->last->next = NULL;
  return true;
}

void ddsi_get_participant_builtin_topic_data (const struct ddsi_participant *pp, ddsi_plist_t *dst, struct ddsi_participant_builtin_topic_data_locators *locs)
{
  struct ddsi_domaingv * const gv = pp->e.gv;
  size_t size;
  char node[64];
  uint64_t qosdiff;

  ddsi_plist_init_empty (dst);
  dst->present |= PP_PARTICIPANT_GUID | PP_BUILTIN_ENDPOINT_SET |
    PP_PROTOCOL_VERSION | PP_VENDORID | PP_DOMAIN_ID;
  dst->participant_guid = pp->e.guid;
  dst->builtin_endpoint_set = pp->bes;
  dst->protocol_version.major = DDSI_RTPS_MAJOR;
  dst->protocol_version.minor = DDSI_RTPS_MINOR;
  dst->vendorid = DDSI_VENDORID_ECLIPSE;
  dst->domain_id = gv->config.extDomainId.value;
  /* Be sure not to send a DOMAIN_TAG when it is the default (an empty)
     string: it is an "incompatible-if-unrecognized" parameter, and so
     implementations that don't understand the parameter will refuse to
     discover us, and so sending the default would break backwards
     compatibility. */
  if (strcmp (gv->config.domainTag, "") != 0)
  {
    dst->present |= PP_DOMAIN_TAG;
    dst->aliased |= PP_DOMAIN_TAG;
    dst->domain_tag = gv->config.domainTag;
  }

  // Construct unicast locator parameters
  {
    struct locators_builder def_uni = locators_builder_init (&dst->default_unicast_locators, locs->def_uni, MAX_XMIT_CONNS);
    struct locators_builder meta_uni = locators_builder_init (&dst->metatraffic_unicast_locators, locs->meta_uni, MAX_XMIT_CONNS);
    for (int i = 0; i < gv->n_interfaces; i++)
    {
      if (!gv->xmit_conns[i]->m_factory->m_enable_spdp)
      {
        // skip any interfaces where the address kind doesn't match the selected transport
        // as a reasonablish way of not advertising iceoryx locators here
        continue;
      }
#ifndef NDEBUG
      int32_t kind;
#endif
      uint32_t data_port, meta_port;
      if (gv->config.many_sockets_mode != DDSI_MSM_MANY_UNICAST)
      {
#ifndef NDEBUG
        kind = gv->loc_default_uc.kind;
#endif
        assert (kind == gv->loc_meta_uc.kind);
        data_port = gv->loc_default_uc.port;
        meta_port = gv->loc_meta_uc.port;
      }
      else
      {
#ifndef NDEBUG
        kind = pp->m_locator.kind;
#endif
        data_port = meta_port = pp->m_locator.port;
      }
      assert (kind == gv->interfaces[i].extloc.kind);
      locators_add_one (&def_uni, &gv->interfaces[i].extloc, data_port);
      locators_add_one (&meta_uni, &gv->interfaces[i].extloc, meta_port);
    }
    if (gv->config.publish_uc_locators)
    {
      dst->present |= PP_DEFAULT_UNICAST_LOCATOR | PP_METATRAFFIC_UNICAST_LOCATOR;
      dst->aliased |= PP_DEFAULT_UNICAST_LOCATOR | PP_METATRAFFIC_UNICAST_LOCATOR;
    }
  }

  if (ddsi_include_multicast_locator_in_discovery (gv))
  {
    dst->present |= PP_DEFAULT_MULTICAST_LOCATOR | PP_METATRAFFIC_MULTICAST_LOCATOR;
    dst->aliased |= PP_DEFAULT_MULTICAST_LOCATOR | PP_METATRAFFIC_MULTICAST_LOCATOR;
    struct locators_builder def_mc = locators_builder_init (&dst->default_multicast_locators, &locs->def_multi, 1);
    struct locators_builder meta_mc = locators_builder_init (&dst->metatraffic_multicast_locators, &locs->meta_multi, 1);
    locators_add_one (&def_mc, &gv->loc_default_mc, DDSI_LOCATOR_PORT_INVALID);
    locators_add_one (&meta_mc, &gv->loc_meta_mc, DDSI_LOCATOR_PORT_INVALID);
  }

  /* Add Adlink specific version information */
  {
    dst->present |= PP_ADLINK_PARTICIPANT_VERSION_INFO;
    memset (&dst->adlink_participant_version_info, 0, sizeof (dst->adlink_participant_version_info));
    dst->adlink_participant_version_info.version = 0;
    dst->adlink_participant_version_info.flags =
      DDSI_ADLINK_FL_DDSI2_PARTICIPANT_FLAG |
      DDSI_ADLINK_FL_PTBES_FIXED_0 |
      DDSI_ADLINK_FL_SUPPORTS_STATUSINFOX;
    if (gv->config.besmode == DDSI_BESMODE_MINIMAL)
      dst->adlink_participant_version_info.flags |= DDSI_ADLINK_FL_MINIMAL_BES_MODE;
    ddsrt_mutex_lock (&gv->privileged_pp_lock);
    if (pp->is_ddsi2_pp)
      dst->adlink_participant_version_info.flags |= DDSI_ADLINK_FL_PARTICIPANT_IS_DDSI2;
    ddsrt_mutex_unlock (&gv->privileged_pp_lock);

#if DDSRT_HAVE_GETHOSTNAME
    if (ddsrt_gethostname(node, sizeof(node)-1) < 0)
#endif
      (void) ddsrt_strlcpy (node, "unknown", sizeof (node));
    size = strlen(node) + strlen(DDS_VERSION) + strlen(DDS_HOST_NAME) + strlen(DDS_TARGET_NAME) + 4; // + '///' + '\0';
    dst->adlink_participant_version_info.internals = ddsrt_malloc(size);
    (void) snprintf(dst->adlink_participant_version_info.internals, size, "%s/%s/%s/%s", node, DDS_VERSION, DDS_HOST_NAME, DDS_TARGET_NAME);
    ETRACE (pp, "ddsi_spdp_write("PGUIDFMT") - internals: %s\n", PGUID (pp->e.guid), dst->adlink_participant_version_info.internals);
  }

  /* Add Cyclone specific information */
  {
    const uint32_t bufsz = ddsi_receive_buffer_size (gv->m_factory);
    if (bufsz > 0)
    {
      dst->present |= PP_CYCLONE_RECEIVE_BUFFER_SIZE;
      dst->cyclone_receive_buffer_size = bufsz;
    }
  }
  if (gv->config.redundant_networking)
  {
    dst->present |= PP_CYCLONE_REDUNDANT_NETWORKING;
    dst->cyclone_redundant_networking = true;
  }

#ifdef DDS_HAS_SECURITY
  /* Add Security specific information. */
  if (ddsi_omg_get_participant_security_info (pp, &(dst->participant_security_info))) {
    dst->present |= PP_PARTICIPANT_SECURITY_INFO;
    dst->aliased |= PP_PARTICIPANT_SECURITY_INFO;
  }
#endif

  /* Participant QoS's insofar as they are set, different from the default, and mapped to the SPDP data, rather than to the Adlink-specific CMParticipant endpoint. */
  qosdiff = ddsi_xqos_delta (&pp->plist->qos, &ddsi_default_qos_participant, DDSI_QP_USER_DATA | DDSI_QP_ENTITY_NAME | DDSI_QP_PROPERTY_LIST | DDSI_QP_LIVELINESS);
  if (gv->config.explicitly_publish_qos_set_to_default)
    qosdiff |= ~(DDSI_QP_UNRECOGNIZED_INCOMPATIBLE_MASK | DDSI_QP_LIVELINESS);

  assert (dst->qos.present == 0);
  ddsi_plist_mergein_missing (dst, pp->plist, 0, qosdiff);
#ifdef DDS_HAS_SECURITY
  if (ddsi_omg_participant_is_secure(pp))
    ddsi_plist_mergein_missing (dst, pp->plist, PP_IDENTITY_TOKEN | PP_PERMISSIONS_TOKEN, 0);
#endif
}

int ddsi_spdp_write (struct ddsi_participant *pp)
{
  struct ddsi_writer *wr;
  ddsi_plist_t ps;
  struct ddsi_participant_builtin_topic_data_locators locs;

  if (pp->e.onlylocal) {
      /* This topic is only locally available. */
      return 0;
  }

  ETRACE (pp, "ddsi_spdp_write("PGUIDFMT")\n", PGUID (pp->e.guid));

  if ((wr = ddsi_get_builtin_writer (pp, DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER)) == NULL)
  {
    ETRACE (pp, "ddsi_spdp_write("PGUIDFMT") - builtin participant writer not found\n", PGUID (pp->e.guid));
    return 0;
  }

  ddsi_get_participant_builtin_topic_data (pp, &ps, &locs);
  return ddsi_write_and_fini_plist (wr, &ps, true);
}

static int ddsi_spdp_dispose_unregister_with_wr (struct ddsi_participant *pp, unsigned entityid)
{
  ddsi_plist_t ps;
  struct ddsi_writer *wr;

  if ((wr = ddsi_get_builtin_writer (pp, entityid)) == NULL)
  {
    ETRACE (pp, "ddsi_spdp_dispose_unregister("PGUIDFMT") - builtin participant %s writer not found\n",
            PGUID (pp->e.guid),
            entityid == DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER ? "secure" : "");
    return 0;
  }

  ddsi_plist_init_empty (&ps);
  ps.present |= PP_PARTICIPANT_GUID;
  ps.participant_guid = pp->e.guid;
  return ddsi_write_and_fini_plist (wr, &ps, false);
}

int ddsi_spdp_dispose_unregister (struct ddsi_participant *pp)
{
  /*
   * When disposing a participant, it should be announced on both the
   * non-secure and secure writers.
   * The receiver will decide from which writer it accepts the dispose.
   */
  int ret = ddsi_spdp_dispose_unregister_with_wr(pp, DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER);
  if ((ret > 0) && ddsi_omg_participant_is_secure(pp))
  {
    ret = ddsi_spdp_dispose_unregister_with_wr(pp, DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER);
  }
  return ret;
}

struct ddsi_spdp_directed_xevent_cb_arg {
  ddsi_guid_t pp_guid;
  int nrepeats;
  ddsi_guid_prefix_t dest_proxypp_guid_prefix;
};

static bool resend_spdp_sample_by_guid_key (struct ddsi_writer *wr, const ddsi_guid_t *guid, struct ddsi_proxy_reader *prd)
{
  /* Look up data in (transient-local) WHC by key value -- FIXME: clearly
   a slightly more efficient and elegant way of looking up the key value
   is to be preferred */
  struct ddsi_domaingv *gv = wr->e.gv;
  bool sample_found;
  ddsi_plist_t ps;
  ddsi_plist_init_empty (&ps);
  ps.present |= PP_PARTICIPANT_GUID;
  ps.participant_guid = *guid;
  struct ddsi_serdata *sd = ddsi_serdata_from_sample (gv->spdp_type, SDK_KEY, &ps);
  ddsi_plist_fini (&ps);
  struct ddsi_whc_borrowed_sample sample;

  ddsrt_mutex_lock (&wr->e.lock);
  sample_found = ddsi_whc_borrow_sample_key (wr->whc, sd, &sample);
  if (sample_found)
  {
    /* Claiming it is new rather than a retransmit so that the rexmit
       limiting won't kick in.  It is best-effort and therefore the
       updating of the last transmitted sequence number won't take
       place anyway.  Nor is it necessary to fiddle with heartbeat
       control stuff. */
    ddsi_enqueue_spdp_sample_wrlock_held (wr, sample.seq, sample.serdata, prd);
    ddsi_whc_return_sample(wr->whc, &sample, false);
  }
  ddsrt_mutex_unlock (&wr->e.lock);
  ddsi_serdata_unref (sd);
  return sample_found;
}

static bool get_pp_and_spdp_wr (struct ddsi_domaingv *gv, const ddsi_guid_t *pp_guid, struct ddsi_participant **pp, struct ddsi_writer **spdp_wr)
{
  if ((*pp = ddsi_entidx_lookup_participant_guid (gv->entity_index, pp_guid)) == NULL)
  {
    GVTRACE ("handle_xevk_spdp "PGUIDFMT" - unknown guid\n", PGUID (*pp_guid));
    return false;
  }
  if ((*spdp_wr = ddsi_get_builtin_writer (*pp, DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER)) == NULL)
  {
    GVTRACE ("handle_xevk_spdp "PGUIDFMT" - spdp writer of participant not found\n", PGUID (*pp_guid));
    return false;
  }
  return true;
}

static void ddsi_spdp_directed_xevent_cb (struct ddsi_domaingv *gv, struct ddsi_xevent *ev, UNUSED_ARG (struct ddsi_xpack *xp), void *varg, ddsrt_mtime_t tnow)
{
  struct ddsi_spdp_directed_xevent_cb_arg * const arg = varg;
  struct ddsi_participant *pp;
  struct ddsi_writer *spdp_wr;

  if (!get_pp_and_spdp_wr (gv, &arg->pp_guid, &pp, &spdp_wr))
  {
    ddsi_delete_xevent (ev);
    return;
  }

  const ddsi_guid_t guid = { .prefix = arg->dest_proxypp_guid_prefix, .entityid = { .u = DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER } };
  struct ddsi_proxy_reader *prd;
  if ((prd = ddsi_entidx_lookup_proxy_reader_guid (gv->entity_index, &guid)) == NULL)
    GVTRACE ("xmit spdp: no proxy reader "PGUIDFMT"\n", PGUID (guid));
  else if (!resend_spdp_sample_by_guid_key (spdp_wr, &arg->pp_guid, prd))
    GVTRACE ("xmit spdp: suppressing early spdp response from "PGUIDFMT" to %"PRIx32":%"PRIx32":%"PRIx32":%x\n",
             PGUID (pp->e.guid), PGUIDPREFIX (arg->dest_proxypp_guid_prefix), DDSI_ENTITYID_PARTICIPANT);

  // Directed events are used to send SPDP packets to newly discovered peers, and used just once
  if (--arg->nrepeats == 0 || gv->config.spdp_interval < DDS_SECS (1) || pp->plist->qos.liveliness.lease_duration < DDS_SECS (1))
    ddsi_delete_xevent (ev);
  else
  {
    ddsrt_mtime_t tnext = ddsrt_mtime_add_duration (tnow, DDS_SECS (1));
    GVTRACE ("xmit spdp "PGUIDFMT" to %"PRIx32":%"PRIx32":%"PRIx32":%x (resched %gs)\n",
             PGUID (pp->e.guid),
             PGUIDPREFIX (arg->dest_proxypp_guid_prefix), DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER,
             (double)(tnext.v - tnow.v) / 1e9);
    (void) ddsi_resched_xevent_if_earlier (ev, tnext);
  }
}

static void resched_spdp_broadcast (struct ddsi_xevent *ev, struct ddsi_participant *pp, ddsrt_mtime_t tnow)
{
  /* schedule next when 80% of the interval has elapsed, or 2s
   before the lease ends, whichever comes first (similar to PMD),
   but never wait longer than spdp_interval */
  struct ddsi_domaingv * const gv = pp->e.gv;
  const dds_duration_t mindelta = DDS_MSECS (10);
  const dds_duration_t ldur = pp->plist->qos.liveliness.lease_duration;
  ddsrt_mtime_t tnext;
  int64_t intv;

  if (ldur < 5 * mindelta / 4)
    intv = mindelta;
  else if (ldur < DDS_SECS (10))
    intv = 4 * ldur / 5;
  else
    intv = ldur - DDS_SECS (2);
  if (intv > gv->config.spdp_interval)
    intv = gv->config.spdp_interval;

  tnext = ddsrt_mtime_add_duration (tnow, intv);
  GVTRACE ("xmit spdp "PGUIDFMT" to %"PRIx32":%"PRIx32":%"PRIx32":%x (resched %gs)\n",
           PGUID (pp->e.guid),
           0,0,0, DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER,
           (double)(tnext.v - tnow.v) / 1e9);
  (void) ddsi_resched_xevent_if_earlier (ev, tnext);
}

void ddsi_spdp_broadcast_xevent_cb (struct ddsi_domaingv *gv, struct ddsi_xevent *ev, UNUSED_ARG (struct ddsi_xpack *xp), void *varg, ddsrt_mtime_t tnow)
{
  /* Like the writer pointer in the heartbeat event, the participant pointer in the spdp event is assumed valid. */
  struct ddsi_spdp_broadcast_xevent_cb_arg * const arg = varg;
  struct ddsi_participant *pp;
  struct ddsi_writer *spdp_wr;

  if (!get_pp_and_spdp_wr (gv, &arg->pp_guid, &pp, &spdp_wr))
    return;

  if (!resend_spdp_sample_by_guid_key (spdp_wr, &arg->pp_guid, NULL))
  {
#ifndef NDEBUG
    /* If undirected, it is pp->spdp_xevent, and that one must never
       run into an empty WHC unless it is already marked for deletion.

       If directed, it may happen in response to an SPDP packet during
       creation of the participant.  This is because pp is inserted in
       the hash table quite early on, which, in turn, is because it
       needs to be visible for creating its builtin endpoints.  But in
       this case, the initial broadcast of the SPDP packet of pp will
       happen shortly. */
    ddsrt_mutex_lock (&pp->e.lock);
    assert (ddsi_delete_xevent_pending (ev));
    ddsrt_mutex_unlock (&pp->e.lock);
#endif
  }

  resched_spdp_broadcast (ev, pp, tnow);
}

static unsigned pseudo_random_delay (const ddsi_guid_t *x, const ddsi_guid_t *y, ddsrt_mtime_t tnow)
{
  /* You know, an ordinary random generator would be even better, but
     the C library doesn't have a reentrant one and I don't feel like
     integrating, say, the Mersenne Twister right now. */
  static const uint64_t cs[] = {
    UINT64_C (15385148050874689571),
    UINT64_C (17503036526311582379),
    UINT64_C (11075621958654396447),
    UINT64_C ( 9748227842331024047),
    UINT64_C (14689485562394710107),
    UINT64_C (17256284993973210745),
    UINT64_C ( 9288286355086959209),
    UINT64_C (17718429552426935775),
    UINT64_C (10054290541876311021),
    UINT64_C (13417933704571658407)
  };
  uint32_t a = x->prefix.u[0], b = x->prefix.u[1], c = x->prefix.u[2], d = x->entityid.u;
  uint32_t e = y->prefix.u[0], f = y->prefix.u[1], g = y->prefix.u[2], h = y->entityid.u;
  uint32_t i = (uint32_t) ((uint64_t) tnow.v >> 32), j = (uint32_t) tnow.v;
  uint64_t m = 0;
  m += (a + cs[0]) * (b + cs[1]);
  m += (c + cs[2]) * (d + cs[3]);
  m += (e + cs[4]) * (f + cs[5]);
  m += (g + cs[6]) * (h + cs[7]);
  m += (i + cs[8]) * (j + cs[9]);
  return (unsigned) (m >> 32);
}

static void respond_to_spdp (const struct ddsi_domaingv *gv, const ddsi_guid_t *dest_proxypp_guid)
{
  struct ddsi_entity_enum_participant est;
  struct ddsi_participant *pp;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();
  ddsi_entidx_enum_participant_init (&est, gv->entity_index);
  while ((pp = ddsi_entidx_enum_participant_next (&est)) != NULL)
  {
    /* delay_base has 32 bits, so delay_norm is approximately 1s max;
       delay_max <= 1s by gv.config checks */
    unsigned delay_base = pseudo_random_delay (&pp->e.guid, dest_proxypp_guid, tnow);
    unsigned delay_norm = delay_base >> 2;
    int64_t delay_max_ms = gv->config.spdp_response_delay_max / 1000000;
    int64_t delay = (int64_t) delay_norm * delay_max_ms / 1000;
    ddsrt_mtime_t tsched = ddsrt_mtime_add_duration (tnow, delay);
    GVTRACE (" %"PRId64, delay);
    if (!pp->e.gv->config.unicast_response_to_spdp_messages)
      /* pp can't reach gc_delete_participant => can safely reschedule */
      (void) ddsi_resched_xevent_if_earlier (pp->spdp_xevent, tsched);
    else
    {
      struct ddsi_spdp_directed_xevent_cb_arg arg = {
        .pp_guid = pp->e.guid,
        .nrepeats = 4, .dest_proxypp_guid_prefix = dest_proxypp_guid->prefix
      };
      ddsi_qxev_callback (gv->xevents, tsched, ddsi_spdp_directed_xevent_cb, &arg, sizeof (arg), false);
    }
  }
  ddsi_entidx_enum_participant_fini (&est);
}

static int handle_spdp_dead (const struct ddsi_receiver_state *rst, ddsi_entityid_t pwr_entityid, ddsrt_wctime_t timestamp, const ddsi_plist_t *datap, unsigned statusinfo)
{
  struct ddsi_domaingv * const gv = rst->gv;
  ddsi_guid_t guid;

  GVLOGDISC ("SPDP ST%x", statusinfo);

  if (datap->present & PP_PARTICIPANT_GUID)
  {
    guid = datap->participant_guid;
    GVLOGDISC (" %"PRIx32":%"PRIx32":%"PRIx32":%"PRIx32, PGUID (guid));
    assert (guid.entityid.u == DDSI_ENTITYID_PARTICIPANT);
    if (ddsi_is_proxy_participant_deletion_allowed(gv, &guid, pwr_entityid))
    {
      if (ddsi_delete_proxy_participant_by_guid (gv, &guid, timestamp, 0) < 0)
      {
        GVLOGDISC (" unknown");
      }
      else
      {
        GVLOGDISC (" delete");
      }
    }
    else
    {
      GVLOGDISC (" not allowed");
    }
  }
  else
  {
    GVWARNING ("data (SPDP, vendor %u.%u): no/invalid payload\n", rst->vendor.id[0], rst->vendor.id[1]);
  }
  return 1;
}

static struct ddsi_proxy_participant *find_ddsi2_proxy_participant (const struct ddsi_entity_index *entidx, const ddsi_guid_t *ppguid)
{
  struct ddsi_entity_enum_proxy_participant it;
  struct ddsi_proxy_participant *pp;
  ddsi_entidx_enum_proxy_participant_init (&it, entidx);
  while ((pp = ddsi_entidx_enum_proxy_participant_next (&it)) != NULL)
  {
    if (ddsi_vendor_is_eclipse_or_opensplice (pp->vendor) && pp->e.guid.prefix.u[0] == ppguid->prefix.u[0] && pp->is_ddsi2_pp)
      break;
  }
  ddsi_entidx_enum_proxy_participant_fini (&it);
  return pp;
}

static void make_participants_dependent_on_ddsi2 (struct ddsi_domaingv *gv, const ddsi_guid_t *ddsi2guid, ddsrt_wctime_t timestamp)
{
  struct ddsi_entity_enum_proxy_participant it;
  struct ddsi_proxy_participant *pp, *d2pp;
  if ((d2pp = ddsi_entidx_lookup_proxy_participant_guid (gv->entity_index, ddsi2guid)) == NULL)
    return;
  ddsi_entidx_enum_proxy_participant_init (&it, gv->entity_index);
  while ((pp = ddsi_entidx_enum_proxy_participant_next (&it)) != NULL)
  {
    if (ddsi_vendor_is_eclipse_or_opensplice (pp->vendor) && pp->e.guid.prefix.u[0] == ddsi2guid->prefix.u[0] && !pp->is_ddsi2_pp)
    {
      GVTRACE ("proxy participant "PGUIDFMT" depends on ddsi2 "PGUIDFMT, PGUID (pp->e.guid), PGUID (*ddsi2guid));
      ddsrt_mutex_lock (&pp->e.lock);
      pp->privileged_pp_guid = *ddsi2guid;
      ddsrt_mutex_unlock (&pp->e.lock);
      ddsi_proxy_participant_reassign_lease (pp, d2pp->lease);
      GVTRACE ("\n");

      if (ddsi_entidx_lookup_proxy_participant_guid (gv->entity_index, ddsi2guid) == NULL)
      {
        /* If DDSI2 has been deleted here (i.e., very soon after
           having been created), we don't know whether pp will be
           deleted */
        break;
      }
    }
  }
  ddsi_entidx_enum_proxy_participant_fini (&it);

  if (pp != NULL)
  {
    GVTRACE ("make_participants_dependent_on_ddsi2: ddsi2 "PGUIDFMT" is no more, delete "PGUIDFMT"\n", PGUID (*ddsi2guid), PGUID (pp->e.guid));
    ddsi_delete_proxy_participant_by_guid (gv, &pp->e.guid, timestamp, 1);
  }
}

static int handle_spdp_alive (const struct ddsi_receiver_state *rst, ddsi_seqno_t seq, ddsrt_wctime_t timestamp, const ddsi_plist_t *datap)
{
  struct ddsi_domaingv * const gv = rst->gv;
  const unsigned bes_sedp_announcer_mask =
    DDSI_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER |
    DDSI_DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER;
  struct ddsi_addrset *as_meta, *as_default;
  uint32_t builtin_endpoint_set;
  ddsi_guid_t privileged_pp_guid;
  dds_duration_t lease_duration;
  unsigned custom_flags = 0;

  /* If advertised domain id or domain tag doesn't match, ignore the message.  Do this first to
     minimize the impact such messages have. */
  {
    const uint32_t domain_id = (datap->present & PP_DOMAIN_ID) ? datap->domain_id : gv->config.extDomainId.value;
    const char *domain_tag = (datap->present & PP_DOMAIN_TAG) ? datap->domain_tag : "";
    if (domain_id != gv->config.extDomainId.value || strcmp (domain_tag, gv->config.domainTag) != 0)
    {
      GVTRACE ("ignore remote participant in mismatching domain %"PRIu32" tag \"%s\"\n", domain_id, domain_tag);
      return 0;
    }
  }

  if (!(datap->present & PP_PARTICIPANT_GUID) || !(datap->present & PP_BUILTIN_ENDPOINT_SET))
  {
    GVWARNING ("data (SPDP, vendor %u.%u): no/invalid payload\n", rst->vendor.id[0], rst->vendor.id[1]);
    return 0;
  }

  /* At some point the RTI implementation didn't mention
     BUILTIN_ENDPOINT_DDSI_PARTICIPANT_MESSAGE_DATA_READER & ...WRITER, or
     so it seemed; and yet they are necessary for correct operation,
     so add them. */
  builtin_endpoint_set = datap->builtin_endpoint_set;
  if (ddsi_vendor_is_rti (rst->vendor) &&
      ((builtin_endpoint_set &
        (DDSI_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER |
         DDSI_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER))
       != (DDSI_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER |
           DDSI_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER)) &&
      gv->config.assume_rti_has_pmd_endpoints)
  {
    GVLOGDISC ("data (SPDP, vendor %u.%u): assuming unadvertised PMD endpoints do exist\n",
             rst->vendor.id[0], rst->vendor.id[1]);
    builtin_endpoint_set |=
      DDSI_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER |
      DDSI_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER;
  }

  /* Do we know this GUID already? */
  {
    struct ddsi_entity_common *existing_entity;
    if ((existing_entity = ddsi_entidx_lookup_guid_untyped (gv->entity_index, &datap->participant_guid)) == NULL)
    {
      /* Local SPDP packets may be looped back, and that can include ones
         for participants currently being deleted.  The first thing that
         happens when deleting a participant is removing it from the hash
         table, and consequently the looped back packet may appear to be
         from an unknown participant.  So we handle that. */
      if (ddsi_is_deleted_participant_guid (gv->deleted_participants, &datap->participant_guid, DDSI_DELETED_PPGUID_REMOTE))
      {
        RSTTRACE ("SPDP ST0 "PGUIDFMT" (recently deleted)", PGUID (datap->participant_guid));
        return 0;
      }
    }
    else if (existing_entity->kind == DDSI_EK_PARTICIPANT)
    {
      RSTTRACE ("SPDP ST0 "PGUIDFMT" (local)", PGUID (datap->participant_guid));
      return 0;
    }
    else if (existing_entity->kind == DDSI_EK_PROXY_PARTICIPANT)
    {
      struct ddsi_proxy_participant *proxypp = (struct ddsi_proxy_participant *) existing_entity;
      struct ddsi_lease *lease;
      int interesting = 0;
      RSTTRACE ("SPDP ST0 "PGUIDFMT" (known)", PGUID (datap->participant_guid));
      /* SPDP processing is so different from normal processing that we are
         even skipping the automatic lease renewal. Note that proxy writers
         that are not alive are not set alive here. This is done only when
         data is received from a particular pwr (in handle_regular) */
      if ((lease = ddsrt_atomic_ldvoidp (&proxypp->minl_auto)) != NULL)
        ddsi_lease_renew (lease, ddsrt_time_elapsed ());
      ddsrt_mutex_lock (&proxypp->e.lock);
      if (proxypp->implicitly_created || seq > proxypp->seq)
      {
        interesting = 1;
        if (!(gv->logconfig.c.mask & DDS_LC_TRACE))
          GVLOGDISC ("SPDP ST0 "PGUIDFMT, PGUID (datap->participant_guid));
        GVLOGDISC (proxypp->implicitly_created ? " (NEW was-implicitly-created)" : " (update)");
        proxypp->implicitly_created = 0;
        ddsi_update_proxy_participant_plist_locked (proxypp, seq, datap, timestamp);
      }
      ddsrt_mutex_unlock (&proxypp->e.lock);
      return interesting;
    }
    else
    {
      /* mismatch on entity kind: that should never have gotten past the
         input validation */
      GVWARNING ("data (SPDP, vendor %u.%u): "PGUIDFMT" kind mismatch\n", rst->vendor.id[0], rst->vendor.id[1], PGUID (datap->participant_guid));
      return 0;
    }
  }

  const bool is_secure = ((datap->builtin_endpoint_set & DDSI_DISC_BUILTIN_ENDPOINT_PARTICIPANT_SECURE_ANNOUNCER) != 0 &&
                          (datap->present & PP_IDENTITY_TOKEN));
  /* Make sure we don't create any security builtin endpoint when it's considered unsecure. */
  if (!is_secure)
    builtin_endpoint_set &= DDSI_BES_MASK_NON_SECURITY;
  GVLOGDISC ("SPDP ST0 "PGUIDFMT" bes %"PRIx32"%s NEW", PGUID (datap->participant_guid), builtin_endpoint_set, is_secure ? " (secure)" : "");

  if (datap->present & PP_ADLINK_PARTICIPANT_VERSION_INFO) {
    if ((datap->adlink_participant_version_info.flags & DDSI_ADLINK_FL_DDSI2_PARTICIPANT_FLAG) &&
        (datap->adlink_participant_version_info.flags & DDSI_ADLINK_FL_PARTICIPANT_IS_DDSI2))
      custom_flags |= DDSI_CF_PARTICIPANT_IS_DDSI2;

    GVLOGDISC (" (0x%08"PRIx32"-0x%08"PRIx32"-0x%08"PRIx32"-0x%08"PRIx32"-0x%08"PRIx32" %s)",
               datap->adlink_participant_version_info.version,
               datap->adlink_participant_version_info.flags,
               datap->adlink_participant_version_info.unused[0],
               datap->adlink_participant_version_info.unused[1],
               datap->adlink_participant_version_info.unused[2],
               datap->adlink_participant_version_info.internals);
  }

  /* Can't do "mergein_missing" because of constness of *datap */
  if (datap->qos.present & DDSI_QP_LIVELINESS)
    lease_duration = datap->qos.liveliness.lease_duration;
  else
  {
    assert (ddsi_default_qos_participant.present & DDSI_QP_LIVELINESS);
    lease_duration = ddsi_default_qos_participant.liveliness.lease_duration;
  }
  /* If any of the SEDP announcer are missing AND the guid prefix of
     the SPDP writer differs from the guid prefix of the new participant,
     we make it dependent on the writer's participant.  See also the
     lease expiration handling.  Note that the entityid MUST be
     DDSI_ENTITYID_PARTICIPANT or entidx_lookup will assert.  So we only
     zero the prefix. */
  privileged_pp_guid.prefix = rst->src_guid_prefix;
  privileged_pp_guid.entityid.u = DDSI_ENTITYID_PARTICIPANT;
  if ((builtin_endpoint_set & bes_sedp_announcer_mask) != bes_sedp_announcer_mask &&
      memcmp (&privileged_pp_guid, &datap->participant_guid, sizeof (ddsi_guid_t)) != 0)
  {
    GVLOGDISC (" (depends on "PGUIDFMT")", PGUID (privileged_pp_guid));
    /* never expire lease for this proxy: it won't actually expire
       until the "privileged" one expires anyway */
    lease_duration = DDS_INFINITY;
  }
  else if (ddsi_vendor_is_eclipse_or_opensplice (rst->vendor) && !(custom_flags & DDSI_CF_PARTICIPANT_IS_DDSI2))
  {
    /* Non-DDSI2 participants are made dependent on DDSI2 (but DDSI2
       itself need not be discovered yet) */
    struct ddsi_proxy_participant *ddsi2;
    if ((ddsi2 = find_ddsi2_proxy_participant (gv->entity_index, &datap->participant_guid)) == NULL)
      memset (&privileged_pp_guid.prefix, 0, sizeof (privileged_pp_guid.prefix));
    else
    {
      privileged_pp_guid.prefix = ddsi2->e.guid.prefix;
      lease_duration = DDS_INFINITY;
      GVLOGDISC (" (depends on "PGUIDFMT")", PGUID (privileged_pp_guid));
    }
  }
  else
  {
    memset (&privileged_pp_guid.prefix, 0, sizeof (privileged_pp_guid.prefix));
  }

  /* Choose locators */
  {
    const ddsi_locators_t emptyset = { .n = 0, .first = NULL, .last = NULL };
    const ddsi_locators_t *uc;
    const ddsi_locators_t *mc;
    ddsi_locator_t srcloc;
    ddsi_interface_set_t intfs;

    srcloc = rst->srcloc;
    uc = (datap->present & PP_DEFAULT_UNICAST_LOCATOR) ? &datap->default_unicast_locators : &emptyset;
    mc = (datap->present & PP_DEFAULT_MULTICAST_LOCATOR) ? &datap->default_multicast_locators : &emptyset;
    if (gv->config.tcp_use_peeraddr_for_unicast)
      uc = &emptyset; // force use of source locator
    else if (uc != &emptyset)
      ddsi_set_unspec_locator (&srcloc); // can't always use the source address

    ddsi_interface_set_init (&intfs);
    as_default = ddsi_addrset_from_locatorlists (gv, uc, mc, &srcloc, &intfs);

    srcloc = rst->srcloc;
    uc = (datap->present & PP_METATRAFFIC_UNICAST_LOCATOR) ? &datap->metatraffic_unicast_locators : &emptyset;
    mc = (datap->present & PP_METATRAFFIC_MULTICAST_LOCATOR) ? &datap->metatraffic_multicast_locators : &emptyset;
    if (gv->config.tcp_use_peeraddr_for_unicast)
      uc = &emptyset; // force use of source locator
    else if (uc != &emptyset)
      ddsi_set_unspec_locator (&srcloc); // can't always use the source address
    ddsi_interface_set_init (&intfs);
    as_meta = ddsi_addrset_from_locatorlists (gv, uc, mc, &srcloc, &intfs);

    ddsi_log_addrset (gv, DDS_LC_DISCOVERY, " (data", as_default);
    ddsi_log_addrset (gv, DDS_LC_DISCOVERY, " meta", as_meta);
    GVLOGDISC (")");
  }

  if (ddsi_addrset_empty_uc (as_default) || ddsi_addrset_empty_uc (as_meta))
  {
    GVLOGDISC (" (no unicast address");
    ddsi_unref_addrset (as_default);
    ddsi_unref_addrset (as_meta);
    return 1;
  }

  GVLOGDISC (" QOS={");
  ddsi_xqos_log (DDS_LC_DISCOVERY, &gv->logconfig, &datap->qos);
  GVLOGDISC ("}\n");

  maybe_add_pp_as_meta_to_as_disc (gv, as_meta);

  if (!ddsi_new_proxy_participant (gv, &datap->participant_guid, builtin_endpoint_set, &privileged_pp_guid, as_default, as_meta, datap, lease_duration, rst->vendor, custom_flags, timestamp, seq))
  {
    /* If no proxy participant was created, don't respond */
    return 0;
  }
  else
  {
    /* Force transmission of SPDP messages - we're not very careful
       in avoiding the processing of SPDP packets addressed to others
       so filter here */
    int have_dst = (rst->dst_guid_prefix.u[0] != 0 || rst->dst_guid_prefix.u[1] != 0 || rst->dst_guid_prefix.u[2] != 0);
    if (!have_dst)
    {
      GVLOGDISC ("broadcasted SPDP packet -> answering");
      respond_to_spdp (gv, &datap->participant_guid);
    }
    else
    {
      GVLOGDISC ("directed SPDP packet -> not responding\n");
    }

    if (custom_flags & DDSI_CF_PARTICIPANT_IS_DDSI2)
    {
      /* If we just discovered DDSI2, make sure any existing
         participants served by it are made dependent on it */
      make_participants_dependent_on_ddsi2 (gv, &datap->participant_guid, timestamp);
    }
    else if (privileged_pp_guid.prefix.u[0] || privileged_pp_guid.prefix.u[1] || privileged_pp_guid.prefix.u[2])
    {
      /* If we just created a participant dependent on DDSI2, make sure
         DDSI2 still exists.  There is a risk of racing the lease expiry
         of DDSI2. */
      if (ddsi_entidx_lookup_proxy_participant_guid (gv->entity_index, &privileged_pp_guid) == NULL)
      {
        GVLOGDISC ("make_participants_dependent_on_ddsi2: ddsi2 "PGUIDFMT" is no more, delete "PGUIDFMT"\n",
                   PGUID (privileged_pp_guid), PGUID (datap->participant_guid));
        ddsi_delete_proxy_participant_by_guid (gv, &datap->participant_guid, timestamp, 1);
      }
    }
    return 1;
  }
}

void ddsi_handle_spdp (const struct ddsi_receiver_state *rst, ddsi_entityid_t pwr_entityid, ddsi_seqno_t seq, const struct ddsi_serdata *serdata)
{
  struct ddsi_domaingv * const gv = rst->gv;
  ddsi_plist_t decoded_data;
  if (ddsi_serdata_to_sample (serdata, &decoded_data, NULL, NULL))
  {
    int interesting = 0;
    switch (serdata->statusinfo & (DDSI_STATUSINFO_DISPOSE | DDSI_STATUSINFO_UNREGISTER))
    {
      case 0:
        interesting = handle_spdp_alive (rst, seq, serdata->timestamp, &decoded_data);
        break;

      case DDSI_STATUSINFO_DISPOSE:
      case DDSI_STATUSINFO_UNREGISTER:
      case (DDSI_STATUSINFO_DISPOSE | DDSI_STATUSINFO_UNREGISTER):
        interesting = handle_spdp_dead (rst, pwr_entityid, serdata->timestamp, &decoded_data, serdata->statusinfo);
        break;
    }

    ddsi_plist_fini (&decoded_data);
    GVLOG (interesting ? DDS_LC_DISCOVERY : DDS_LC_TRACE, "\n");
  }
}
