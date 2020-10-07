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
#include "dds/ddsi/q_protocol.h"
#include "dds/ddsi/q_rtps.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_xevent.h"
#include "dds/ddsi/q_addrset.h"
#include "dds/ddsi/q_ddsi_discovery.h"
#include "dds/ddsi/ddsi_serdata_plist.h"

#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_transmit.h"
#include "dds/ddsi/q_lease.h"
#include "dds/ddsi/ddsi_serdata_default.h"
#include "dds/ddsi/q_feature_check.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsi/ddsi_pmd.h"
#ifdef DDS_HAS_SECURITY
#include "dds/ddsi/ddsi_security_exchange.h"
#endif

typedef enum ddsi_sedp_kind {
  SEDP_KIND_READER,
  SEDP_KIND_WRITER,
  SEDP_KIND_TOPIC
} ddsi_sedp_kind_t;

static int get_locator (const struct ddsi_domaingv *gv, ddsi_locator_t *loc, const nn_locators_t *locs, int uc_same_subnet)
{
  struct nn_locators_one *l;
  ddsi_locator_t first, samenet;
  int first_set = 0, samenet_set = 0;
  memset (&first, 0, sizeof (first));
  memset (&samenet, 0, sizeof (samenet));

  /* Special case UDPv4 MC address generators - there is a bit of an type mismatch between an address generator (i.e., a set of addresses) and an address ... Whoever uses them is supposed to know that that is what he wants, so we simply given them priority. */
  if (ddsi_factory_supports (gv->m_factory, NN_LOCATOR_KIND_UDPv4))
  {
    for (l = locs->first; l != NULL; l = l->next)
    {
      if (l->loc.kind == NN_LOCATOR_KIND_UDPv4MCGEN)
      {
        *loc = l->loc;
        return 1;
      }
    }
  }

  /* Preferably an (the first) address that matches a network we are
     on; if none does, pick the first. No multicast locator ever will
     match, so the first one will be used. */
  for (l = locs->first; l != NULL; l = l->next)
  {
    /* Skip locators of the wrong kind */

    if (! ddsi_factory_supports (gv->m_factory, l->loc.kind))
    {
      continue;
    }

    if (l->loc.kind == NN_LOCATOR_KIND_UDPv4 && gv->extmask.kind != NN_LOCATOR_KIND_INVALID)
    {
      /* If the examined locator is in the same subnet as our own
         external IP address, this locator will be translated into one
         in the same subnet as our own local ip and selected. */
      struct in_addr tmp4 = *((struct in_addr *) (l->loc.address + 12));
      const struct in_addr ownip = *((struct in_addr *) (gv->ownloc.address + 12));
      const struct in_addr extip = *((struct in_addr *) (gv->extloc.address + 12));
      const struct in_addr extmask = *((struct in_addr *) (gv->extmask.address + 12));

      if ((tmp4.s_addr & extmask.s_addr) == (extip.s_addr & extmask.s_addr))
      {
        /* translate network part of the IP address from the external
           one to the internal one */
        tmp4.s_addr = (tmp4.s_addr & ~extmask.s_addr) | (ownip.s_addr & extmask.s_addr);
        memcpy (loc, &l->loc, sizeof (*loc));
        memcpy (loc->address + 12, &tmp4, 4);
        return 1;
      }
    }

#if DDSRT_HAVE_IPV6
    if ((l->loc.kind == NN_LOCATOR_KIND_UDPv6) || (l->loc.kind == NN_LOCATOR_KIND_TCPv6))
    {
      /* We (cowardly) refuse to accept advertised link-local
         addresses unles we're in "link-local" mode ourselves.  Then
         we just hope for the best.  */
      const struct in6_addr *ip6 = (const struct in6_addr *) l->loc.address;
      if (!gv->ipv6_link_local && IN6_IS_ADDR_LINKLOCAL (ip6))
        continue;
    }
#endif

    if (!first_set)
    {
      first = l->loc;
      first_set = 1;
    }

    switch (ddsi_is_nearby_address(&l->loc, &gv->ownloc, (size_t) gv->n_interfaces, gv->interfaces))
    {
      case DNAR_DISTANT:
        break;
      case DNAR_LOCAL:
        if (!samenet_set)
        {
          /* on a network we're connected to */
          samenet = l->loc;
          samenet_set = 1;
        }
        break;
      case DNAR_SAME:
        /* matches the preferred interface -> the very best situation */
        *loc = l->loc;
        return 1;
    }
  }
  if (!uc_same_subnet)
  {
    if (samenet_set)
    {
      /* prefer a directly connected network */
      *loc = samenet;
      return 1;
    }
    else if (first_set)
    {
      /* else any address we found will have to do */
      *loc = first;
      return 1;
    }
  }
  return 0;
}

/******************************************************************************
 ***
 *** SPDP
 ***
 *****************************************************************************/

static void maybe_add_pp_as_meta_to_as_disc (struct ddsi_domaingv *gv, const struct addrset *as_meta)
{
  if (addrset_empty_mc (as_meta) || !(gv->config.allowMulticast & DDSI_AMC_SPDP))
  {
    ddsi_locator_t loc;
    if (addrset_any_uc (as_meta, &loc))
    {
      add_to_addrset (gv, gv->as_disc, &loc);
    }
  }
}

void get_participant_builtin_topic_data (const struct participant *pp, ddsi_plist_t *dst, struct participant_builtin_topic_data_locators *locs)
{
  size_t size;
  char node[64];
  uint64_t qosdiff;

  ddsi_plist_init_empty (dst);
  dst->present |= PP_PARTICIPANT_GUID | PP_BUILTIN_ENDPOINT_SET |
    PP_PROTOCOL_VERSION | PP_VENDORID | PP_PARTICIPANT_LEASE_DURATION |
    PP_DOMAIN_ID;
  dst->participant_guid = pp->e.guid;
  dst->builtin_endpoint_set = pp->bes;
  dst->protocol_version.major = RTPS_MAJOR;
  dst->protocol_version.minor = RTPS_MINOR;
  dst->vendorid = NN_VENDORID_ECLIPSE;
  dst->domain_id = pp->e.gv->config.extDomainId.value;
  /* Be sure not to send a DOMAIN_TAG when it is the default (an empty)
     string: it is an "incompatible-if-unrecognized" parameter, and so
     implementations that don't understand the parameter will refuse to
     discover us, and so sending the default would break backwards
     compatibility. */
  if (strcmp (pp->e.gv->config.domainTag, "") != 0)
  {
    dst->present |= PP_DOMAIN_TAG;
    dst->aliased |= PP_DOMAIN_TAG;
    dst->domain_tag = pp->e.gv->config.domainTag;
  }
  dst->default_unicast_locators.n = 1;
  dst->default_unicast_locators.first =
    dst->default_unicast_locators.last = &locs->def_uni_loc_one;
  dst->metatraffic_unicast_locators.n = 1;
  dst->metatraffic_unicast_locators.first =
    dst->metatraffic_unicast_locators.last = &locs->meta_uni_loc_one;
  locs->def_uni_loc_one.next = NULL;
  locs->meta_uni_loc_one.next = NULL;

  if (pp->e.gv->config.many_sockets_mode == DDSI_MSM_MANY_UNICAST)
  {
    locs->def_uni_loc_one.loc = pp->m_locator;
    locs->meta_uni_loc_one.loc = pp->m_locator;
  }
  else
  {
    locs->def_uni_loc_one.loc = pp->e.gv->loc_default_uc;
    locs->meta_uni_loc_one.loc = pp->e.gv->loc_meta_uc;
  }

  if (pp->e.gv->config.publish_uc_locators)
  {
    dst->present |= PP_DEFAULT_UNICAST_LOCATOR | PP_METATRAFFIC_UNICAST_LOCATOR;
    dst->aliased |= PP_DEFAULT_UNICAST_LOCATOR | PP_METATRAFFIC_UNICAST_LOCATOR;
  }

  if (pp->e.gv->config.allowMulticast)
  {
    int include = 0;
#ifdef DDS_HAS_SSM
    /* Note that if the default multicast address is an SSM address,
       we will simply advertise it. The recipients better understand
       it means the writers will publish to address and the readers
       favour SSM. */
    if (ddsi_is_ssm_mcaddr (pp->e.gv, &pp->e.gv->loc_default_mc))
      include = (pp->e.gv->config.allowMulticast & DDSI_AMC_SSM) != 0;
    else
      include = (pp->e.gv->config.allowMulticast & DDSI_AMC_ASM) != 0;
#else
    if (pp->e.gv->config.allowMulticast & DDSI_AMC_ASM)
      include = 1;
#endif
    if (include)
    {
      dst->present |= PP_DEFAULT_MULTICAST_LOCATOR | PP_METATRAFFIC_MULTICAST_LOCATOR;
      dst->aliased |= PP_DEFAULT_MULTICAST_LOCATOR | PP_METATRAFFIC_MULTICAST_LOCATOR;
      dst->default_multicast_locators.n = 1;
      dst->default_multicast_locators.first =
      dst->default_multicast_locators.last = &locs->def_multi_loc_one;
      dst->metatraffic_multicast_locators.n = 1;
      dst->metatraffic_multicast_locators.first =
      dst->metatraffic_multicast_locators.last = &locs->meta_multi_loc_one;
      locs->def_multi_loc_one.next = NULL;
      locs->def_multi_loc_one.loc = pp->e.gv->loc_default_mc;
      locs->meta_multi_loc_one.next = NULL;
      locs->meta_multi_loc_one.loc = pp->e.gv->loc_meta_mc;
    }
  }
  dst->participant_lease_duration = pp->lease_duration;

  /* Add Adlink specific version information */
  {
    dst->present |= PP_ADLINK_PARTICIPANT_VERSION_INFO;
    memset (&dst->adlink_participant_version_info, 0, sizeof (dst->adlink_participant_version_info));
    dst->adlink_participant_version_info.version = 0;
    dst->adlink_participant_version_info.flags =
      NN_ADLINK_FL_DDSI2_PARTICIPANT_FLAG |
      NN_ADLINK_FL_PTBES_FIXED_0 |
      NN_ADLINK_FL_SUPPORTS_STATUSINFOX;
    if (pp->e.gv->config.besmode == DDSI_BESMODE_MINIMAL)
      dst->adlink_participant_version_info.flags |= NN_ADLINK_FL_MINIMAL_BES_MODE;
    ddsrt_mutex_lock (&pp->e.gv->privileged_pp_lock);
    if (pp->is_ddsi2_pp)
      dst->adlink_participant_version_info.flags |= NN_ADLINK_FL_PARTICIPANT_IS_DDSI2;
    ddsrt_mutex_unlock (&pp->e.gv->privileged_pp_lock);

    if (ddsrt_gethostname(node, sizeof(node)-1) < 0)
      (void) ddsrt_strlcpy (node, "unknown", sizeof (node));
    size = strlen(node) + strlen(DDS_VERSION) + strlen(DDS_HOST_NAME) + strlen(DDS_TARGET_NAME) + 4; /* + ///'\0' */
    dst->adlink_participant_version_info.internals = ddsrt_malloc(size);
    (void) snprintf(dst->adlink_participant_version_info.internals, size, "%s/%s/%s/%s", node, DDS_VERSION, DDS_HOST_NAME, DDS_TARGET_NAME);
    ETRACE (pp, "spdp_write("PGUIDFMT") - internals: %s\n", PGUID (pp->e.guid), dst->adlink_participant_version_info.internals);
  }

  /* Add Cyclone specific information */
  {
    const uint32_t bufsz = ddsi_receive_buffer_size (pp->e.gv->m_factory);
    if (bufsz > 0)
    {
      dst->present |= PP_CYCLONE_RECEIVE_BUFFER_SIZE;
      dst->cyclone_receive_buffer_size = bufsz;
    }
  }

#ifdef DDS_HAS_SECURITY
  /* Add Security specific information. */
  if (q_omg_get_participant_security_info(pp, &(dst->participant_security_info))) {
    dst->present |= PP_PARTICIPANT_SECURITY_INFO;
    dst->aliased |= PP_PARTICIPANT_SECURITY_INFO;
  }
#endif

  /* Participant QoS's insofar as they are set, different from the default, and mapped to the SPDP data, rather than to the Adlink-specific CMParticipant endpoint.  Currently, that means just USER_DATA. */
  qosdiff = ddsi_xqos_delta (&pp->plist->qos, &pp->e.gv->default_plist_pp.qos, QP_USER_DATA);
  if (pp->e.gv->config.explicitly_publish_qos_set_to_default)
    qosdiff |= ~QP_UNRECOGNIZED_INCOMPATIBLE_MASK;

  assert (dst->qos.present == 0);
  ddsi_plist_mergein_missing (dst, pp->plist, 0, qosdiff);
#ifdef DDS_HAS_SECURITY
  if (q_omg_participant_is_secure(pp))
    ddsi_plist_mergein_missing (dst, pp->plist, PP_IDENTITY_TOKEN | PP_PERMISSIONS_TOKEN, 0);
#endif
}

static int write_and_fini_plist (struct writer *wr, ddsi_plist_t *ps, bool alive)
{
  struct ddsi_serdata *serdata = ddsi_serdata_from_sample (wr->type, alive ? SDK_DATA : SDK_KEY, ps);
  ddsi_plist_fini (ps);
  serdata->statusinfo = alive ? 0 : (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER);
  serdata->timestamp = ddsrt_time_wallclock ();
  return write_sample_nogc_notk (lookup_thread_state (), NULL, wr, serdata);
}

int spdp_write (struct participant *pp)
{
  struct writer *wr;
  ddsi_plist_t ps;
  struct participant_builtin_topic_data_locators locs;

  if (pp->e.onlylocal) {
      /* This topic is only locally available. */
      return 0;
  }

  ETRACE (pp, "spdp_write("PGUIDFMT")\n", PGUID (pp->e.guid));

  if ((wr = get_builtin_writer (pp, NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER)) == NULL)
  {
    ETRACE (pp, "spdp_write("PGUIDFMT") - builtin participant writer not found\n", PGUID (pp->e.guid));
    return 0;
  }

  get_participant_builtin_topic_data (pp, &ps, &locs);
  return write_and_fini_plist (wr, &ps, true);
}

static int spdp_dispose_unregister_with_wr (struct participant *pp, unsigned entityid)
{
  ddsi_plist_t ps;
  struct writer *wr;

  if ((wr = get_builtin_writer (pp, entityid)) == NULL)
  {
    ETRACE (pp, "spdp_dispose_unregister("PGUIDFMT") - builtin participant %s writer not found\n",
            PGUID (pp->e.guid),
            entityid == NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER ? "secure" : "");
    return 0;
  }

  ddsi_plist_init_empty (&ps);
  ps.present |= PP_PARTICIPANT_GUID;
  ps.participant_guid = pp->e.guid;
  return write_and_fini_plist (wr, &ps, false);
}

int spdp_dispose_unregister (struct participant *pp)
{
  /*
   * When disposing a participant, it should be announced on both the
   * non-secure and secure writers.
   * The receiver will decide from which writer it accepts the dispose.
   */
  int ret = spdp_dispose_unregister_with_wr(pp, NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER);
  if ((ret > 0) && q_omg_participant_is_secure(pp))
  {
    ret = spdp_dispose_unregister_with_wr(pp, NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER);
  }
  return ret;
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
  struct entidx_enum_participant est;
  struct participant *pp;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();
  entidx_enum_participant_init (&est, gv->entity_index);
  while ((pp = entidx_enum_participant_next (&est)) != NULL)
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
      (void) resched_xevent_if_earlier (pp->spdp_xevent, tsched);
    else
      qxev_spdp (gv->xevents, tsched, &pp->e.guid, dest_proxypp_guid);
  }
  entidx_enum_participant_fini (&est);
}

static int handle_spdp_dead (const struct receiver_state *rst, ddsi_entityid_t pwr_entityid, ddsrt_wctime_t timestamp, const ddsi_plist_t *datap, unsigned statusinfo)
{
  struct ddsi_domaingv * const gv = rst->gv;
  ddsi_guid_t guid;

  GVLOGDISC ("SPDP ST%x", statusinfo);

  if (datap->present & PP_PARTICIPANT_GUID)
  {
    guid = datap->participant_guid;
    GVLOGDISC (" %"PRIx32":%"PRIx32":%"PRIx32":%"PRIx32, PGUID (guid));
    assert (guid.entityid.u == NN_ENTITYID_PARTICIPANT);
    if (is_proxy_participant_deletion_allowed(gv, &guid, pwr_entityid))
    {
      if (delete_proxy_participant_by_guid (gv, &guid, timestamp, 0) < 0)
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

static void allowmulticast_aware_add_to_addrset (const struct ddsi_domaingv *gv, uint32_t allow_multicast, struct addrset *as, const ddsi_locator_t *loc)
{
#if DDS_HAS_SSM
  if (ddsi_is_ssm_mcaddr (gv, loc))
  {
    if (!(allow_multicast & DDSI_AMC_SSM))
      return;
  }
  else if (ddsi_is_mcaddr (gv, loc))
  {
    if (!(allow_multicast & DDSI_AMC_ASM))
      return;
  }
#else
  if (ddsi_is_mcaddr (gv, loc) && !(allow_multicast & DDSI_AMC_ASM))
    return;
#endif
  add_to_addrset (gv, as, loc);
}

static struct proxy_participant *find_ddsi2_proxy_participant (const struct entity_index *entidx, const ddsi_guid_t *ppguid)
{
  struct entidx_enum_proxy_participant it;
  struct proxy_participant *pp;
  entidx_enum_proxy_participant_init (&it, entidx);
  while ((pp = entidx_enum_proxy_participant_next (&it)) != NULL)
  {
    if (vendor_is_eclipse_or_opensplice (pp->vendor) && pp->e.guid.prefix.u[0] == ppguid->prefix.u[0] && pp->is_ddsi2_pp)
      break;
  }
  entidx_enum_proxy_participant_fini (&it);
  return pp;
}

static void make_participants_dependent_on_ddsi2 (struct ddsi_domaingv *gv, const ddsi_guid_t *ddsi2guid, ddsrt_wctime_t timestamp)
{
  struct entidx_enum_proxy_participant it;
  struct proxy_participant *pp, *d2pp;
  if ((d2pp = entidx_lookup_proxy_participant_guid (gv->entity_index, ddsi2guid)) == NULL)
    return;
  entidx_enum_proxy_participant_init (&it, gv->entity_index);
  while ((pp = entidx_enum_proxy_participant_next (&it)) != NULL)
  {
    if (vendor_is_eclipse_or_opensplice (pp->vendor) && pp->e.guid.prefix.u[0] == ddsi2guid->prefix.u[0] && !pp->is_ddsi2_pp)
    {
      GVTRACE ("proxy participant "PGUIDFMT" depends on ddsi2 "PGUIDFMT, PGUID (pp->e.guid), PGUID (*ddsi2guid));
      ddsrt_mutex_lock (&pp->e.lock);
      pp->privileged_pp_guid = *ddsi2guid;
      ddsrt_mutex_unlock (&pp->e.lock);
      proxy_participant_reassign_lease (pp, d2pp->lease);
      GVTRACE ("\n");

      if (entidx_lookup_proxy_participant_guid (gv->entity_index, ddsi2guid) == NULL)
      {
        /* If DDSI2 has been deleted here (i.e., very soon after
           having been created), we don't know whether pp will be
           deleted */
        break;
      }
    }
  }
  entidx_enum_proxy_participant_fini (&it);

  if (pp != NULL)
  {
    GVTRACE ("make_participants_dependent_on_ddsi2: ddsi2 "PGUIDFMT" is no more, delete "PGUIDFMT"\n", PGUID (*ddsi2guid), PGUID (pp->e.guid));
    delete_proxy_participant_by_guid (gv, &pp->e.guid, timestamp, 1);
  }
}

static int handle_spdp_alive (const struct receiver_state *rst, seqno_t seq, ddsrt_wctime_t timestamp, const ddsi_plist_t *datap)
{
  struct ddsi_domaingv * const gv = rst->gv;
  const unsigned bes_sedp_announcer_mask =
    NN_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER |
    NN_DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER;
  struct addrset *as_meta, *as_default;
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
     BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER & ...WRITER, or
     so it seemed; and yet they are necessary for correct operation,
     so add them. */
  builtin_endpoint_set = datap->builtin_endpoint_set;
  if (vendor_is_rti (rst->vendor) &&
      ((builtin_endpoint_set &
        (NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER |
         NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER))
       != (NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER |
           NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER)) &&
      gv->config.assume_rti_has_pmd_endpoints)
  {
    GVLOGDISC ("data (SPDP, vendor %u.%u): assuming unadvertised PMD endpoints do exist\n",
             rst->vendor.id[0], rst->vendor.id[1]);
    builtin_endpoint_set |=
      NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER |
      NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER;
  }

  /* Do we know this GUID already? */
  {
    struct entity_common *existing_entity;
    if ((existing_entity = entidx_lookup_guid_untyped (gv->entity_index, &datap->participant_guid)) == NULL)
    {
      /* Local SPDP packets may be looped back, and that can include ones
         for participants currently being deleted.  The first thing that
         happens when deleting a participant is removing it from the hash
         table, and consequently the looped back packet may appear to be
         from an unknown participant.  So we handle that. */
      if (is_deleted_participant_guid (gv->deleted_participants, &datap->participant_guid, DPG_REMOTE))
      {
        RSTTRACE ("SPDP ST0 "PGUIDFMT" (recently deleted)", PGUID (datap->participant_guid));
        return 0;
      }
    }
    else if (existing_entity->kind == EK_PARTICIPANT)
    {
      RSTTRACE ("SPDP ST0 "PGUIDFMT" (local)", PGUID (datap->participant_guid));
      return 0;
    }
    else if (existing_entity->kind == EK_PROXY_PARTICIPANT)
    {
      struct proxy_participant *proxypp = (struct proxy_participant *) existing_entity;
      struct lease *lease;
      int interesting = 0;
      RSTTRACE ("SPDP ST0 "PGUIDFMT" (known)", PGUID (datap->participant_guid));
      /* SPDP processing is so different from normal processing that we are
         even skipping the automatic lease renewal. Note that proxy writers
         that are not alive are not set alive here. This is done only when
         data is received from a particular pwr (in handle_regular) */
      if ((lease = ddsrt_atomic_ldvoidp (&proxypp->minl_auto)) != NULL)
        lease_renew (lease, ddsrt_time_elapsed ());
      ddsrt_mutex_lock (&proxypp->e.lock);
      if (proxypp->implicitly_created || seq > proxypp->seq)
      {
        interesting = 1;
        if (!(gv->logconfig.c.mask & DDS_LC_TRACE))
          GVLOGDISC ("SPDP ST0 "PGUIDFMT, PGUID (datap->participant_guid));
        GVLOGDISC (proxypp->implicitly_created ? " (NEW was-implicitly-created)" : " (update)");
        proxypp->implicitly_created = 0;
        update_proxy_participant_plist_locked (proxypp, seq, datap, timestamp);
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

  const bool is_secure = ((datap->builtin_endpoint_set & NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_SECURE_ANNOUNCER) != 0 &&
                          (datap->present & PP_IDENTITY_TOKEN));
  /* Make sure we don't create any security builtin endpoint when it's considered unsecure. */
  if (!is_secure)
    builtin_endpoint_set &= NN_BES_MASK_NON_SECURITY;
  GVLOGDISC ("SPDP ST0 "PGUIDFMT" bes %"PRIx32"%s NEW", PGUID (datap->participant_guid), builtin_endpoint_set, is_secure ? " (secure)" : "");

  if (datap->present & PP_PARTICIPANT_LEASE_DURATION)
  {
    lease_duration = datap->participant_lease_duration;
  }
  else
  {
    GVLOGDISC (" (PARTICIPANT_LEASE_DURATION defaulting to 100s)");
    lease_duration = DDS_SECS (100);
  }

  if (datap->present & PP_ADLINK_PARTICIPANT_VERSION_INFO) {
    if ((datap->adlink_participant_version_info.flags & NN_ADLINK_FL_DDSI2_PARTICIPANT_FLAG) &&
        (datap->adlink_participant_version_info.flags & NN_ADLINK_FL_PARTICIPANT_IS_DDSI2))
      custom_flags |= CF_PARTICIPANT_IS_DDSI2;

    GVLOGDISC (" (0x%08"PRIx32"-0x%08"PRIx32"-0x%08"PRIx32"-0x%08"PRIx32"-0x%08"PRIx32" %s)",
               datap->adlink_participant_version_info.version,
               datap->adlink_participant_version_info.flags,
               datap->adlink_participant_version_info.unused[0],
               datap->adlink_participant_version_info.unused[1],
               datap->adlink_participant_version_info.unused[2],
               datap->adlink_participant_version_info.internals);
  }

  /* If any of the SEDP announcer are missing AND the guid prefix of
     the SPDP writer differs from the guid prefix of the new participant,
     we make it dependent on the writer's participant.  See also the
     lease expiration handling.  Note that the entityid MUST be
     NN_ENTITYID_PARTICIPANT or entidx_lookup will assert.  So we only
     zero the prefix. */
  privileged_pp_guid.prefix = rst->src_guid_prefix;
  privileged_pp_guid.entityid.u = NN_ENTITYID_PARTICIPANT;
  if ((builtin_endpoint_set & bes_sedp_announcer_mask) != bes_sedp_announcer_mask &&
      memcmp (&privileged_pp_guid, &datap->participant_guid, sizeof (ddsi_guid_t)) != 0)
  {
    GVLOGDISC (" (depends on "PGUIDFMT")", PGUID (privileged_pp_guid));
    /* never expire lease for this proxy: it won't actually expire
       until the "privileged" one expires anyway */
    lease_duration = DDS_INFINITY;
  }
  else if (vendor_is_eclipse_or_opensplice (rst->vendor) && !(custom_flags & CF_PARTICIPANT_IS_DDSI2))
  {
    /* Non-DDSI2 participants are made dependent on DDSI2 (but DDSI2
       itself need not be discovered yet) */
    struct proxy_participant *ddsi2;
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
    ddsi_locator_t loc;
    int uc_same_subnet;

    as_default = new_addrset ();
    as_meta = new_addrset ();

    if ((datap->present & PP_DEFAULT_MULTICAST_LOCATOR) && (get_locator (gv, &loc, &datap->default_multicast_locators, 0)))
      allowmulticast_aware_add_to_addrset (gv, gv->config.allowMulticast, as_default, &loc);
    if ((datap->present & PP_METATRAFFIC_MULTICAST_LOCATOR) && (get_locator (gv, &loc, &datap->metatraffic_multicast_locators, 0)))
      allowmulticast_aware_add_to_addrset (gv, gv->config.allowMulticast, as_meta, &loc);

    /* If no multicast locators or multicast TTL > 1, assume IP (multicast) routing can be relied upon to reach
       the remote participant, else only accept nodes with an advertised unicast address in the same subnet to
       protect against multicasts being received over an unexpected interface (which sometimes appears to occur) */
    if (addrset_empty_mc (as_default) && addrset_empty_mc (as_meta))
      uc_same_subnet = 0;
    else if (gv->config.multicast_ttl > 1)
      uc_same_subnet = 0;
    else
    {
      uc_same_subnet = 1;
      GVLOGDISC (" subnet-filter");
    }

    /* If unicast locators not present, then try to obtain from connection */
    if (!gv->config.tcp_use_peeraddr_for_unicast && (datap->present & PP_DEFAULT_UNICAST_LOCATOR) && (get_locator (gv, &loc, &datap->default_unicast_locators, uc_same_subnet)))
      add_to_addrset (gv, as_default, &loc);
    else {
      GVLOGDISC (" (srclocD)");
      add_to_addrset (gv, as_default, &rst->srcloc);
    }

    if (!gv->config.tcp_use_peeraddr_for_unicast && (datap->present & PP_METATRAFFIC_UNICAST_LOCATOR) && (get_locator (gv, &loc, &datap->metatraffic_unicast_locators, uc_same_subnet)))
      add_to_addrset (gv, as_meta, &loc);
    else {
      GVLOGDISC (" (srclocM)");
      add_to_addrset (gv, as_meta, &rst->srcloc);
    }

    nn_log_addrset (gv, DDS_LC_DISCOVERY, " (data", as_default);
    nn_log_addrset (gv, DDS_LC_DISCOVERY, " meta", as_meta);
    GVLOGDISC (")");
  }

  if (addrset_empty_uc (as_default) || addrset_empty_uc (as_meta))
  {
    GVLOGDISC (" (no unicast address");
    unref_addrset (as_default);
    unref_addrset (as_meta);
    return 1;
  }

  GVLOGDISC (" QOS={");
  ddsi_xqos_log (DDS_LC_DISCOVERY, &gv->logconfig, &datap->qos);
  GVLOGDISC ("}\n");

  maybe_add_pp_as_meta_to_as_disc (gv, as_meta);

  if (!new_proxy_participant (gv, &datap->participant_guid, builtin_endpoint_set, &privileged_pp_guid, as_default, as_meta, datap, lease_duration, rst->vendor, custom_flags, timestamp, seq))
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

    if (custom_flags & CF_PARTICIPANT_IS_DDSI2)
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
      if (entidx_lookup_proxy_participant_guid (gv->entity_index, &privileged_pp_guid) == NULL)
      {
        GVLOGDISC ("make_participants_dependent_on_ddsi2: ddsi2 "PGUIDFMT" is no more, delete "PGUIDFMT"\n",
                   PGUID (privileged_pp_guid), PGUID (datap->participant_guid));
        delete_proxy_participant_by_guid (gv, &datap->participant_guid, timestamp, 1);
      }
    }
    return 1;
  }
}

static void handle_spdp (const struct receiver_state *rst, ddsi_entityid_t pwr_entityid, seqno_t seq, const struct ddsi_serdata *serdata)
{
  struct ddsi_domaingv * const gv = rst->gv;
  ddsi_plist_t decoded_data;
  if (ddsi_serdata_to_sample (serdata, &decoded_data, NULL, NULL))
  {
    int interesting = 0;
    switch (serdata->statusinfo & (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER))
    {
      case 0:
        interesting = handle_spdp_alive (rst, seq, serdata->timestamp, &decoded_data);
        break;

      case NN_STATUSINFO_DISPOSE:
      case NN_STATUSINFO_UNREGISTER:
      case (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER):
        interesting = handle_spdp_dead (rst, pwr_entityid, serdata->timestamp, &decoded_data, serdata->statusinfo);
        break;
    }

    ddsi_plist_fini (&decoded_data);
    GVLOG (interesting ? DDS_LC_DISCOVERY : DDS_LC_TRACE, "\n");
  }
}

struct add_locator_to_ps_arg {
  struct ddsi_domaingv *gv;
  ddsi_plist_t *ps;
};

static void add_locator_to_ps (const ddsi_locator_t *loc, void *varg)
{
  struct add_locator_to_ps_arg *arg = varg;
  struct nn_locators_one *elem = ddsrt_malloc (sizeof (struct nn_locators_one));
  struct nn_locators *locs;
  unsigned present_flag;

  elem->loc = *loc;
  elem->next = NULL;

  if (ddsi_is_mcaddr (arg->gv, loc)) {
    locs = &arg->ps->multicast_locators;
    present_flag = PP_MULTICAST_LOCATOR;
  } else {
    locs = &arg->ps->unicast_locators;
    present_flag = PP_UNICAST_LOCATOR;
  }

  if (!(arg->ps->present & present_flag))
  {
    locs->n = 0;
    locs->first = locs->last = NULL;
    arg->ps->present |= present_flag;
  }
  locs->n++;
  if (locs->first)
    locs->last->next = elem;
  else
    locs->first = elem;
  locs->last = elem;
}

/******************************************************************************
 ***
 *** SEDP
 ***
 *****************************************************************************/

static struct writer *get_sedp_writer (const struct participant *pp, unsigned entityid)
{
  struct writer *sedp_wr = get_builtin_writer (pp, entityid);
  if (sedp_wr == NULL)
    DDS_FATAL ("sedp_write_writer: no SEDP builtin writer %x for "PGUIDFMT"\n", entityid, PGUID (pp->e.guid));
  return sedp_wr;
}

static int sedp_write_endpoint_impl
(
   struct writer *wr, int alive, const ddsi_guid_t *guid,
   const struct entity_common *common, const struct endpoint_common *epcommon,
   const dds_qos_t *xqos, struct addrset *as, nn_security_info_t *security
#ifdef DDS_HAS_TYPE_DISCOVERY
   , type_identifier_t *type_id
#endif
)
{
  struct ddsi_domaingv * const gv = wr->e.gv;
  const dds_qos_t *defqos = NULL;
  if (is_writer_entityid (guid->entityid))
    defqos = &gv->default_xqos_wr;
  else if (is_reader_entityid (guid->entityid))
    defqos = &gv->default_xqos_rd;
  else
    assert (false);

  uint64_t qosdiff;
  ddsi_plist_t ps;

  ddsi_plist_init_empty (&ps);
  ps.present |= PP_ENDPOINT_GUID;
  ps.endpoint_guid = *guid;

  if (common && *common->name != 0)
  {
    ps.present |= PP_ENTITY_NAME;
    ps.aliased |= PP_ENTITY_NAME;
    ps.entity_name = common->name;
  }

#ifdef DDS_HAS_SECURITY
  if (security)
  {
    ps.present |= PP_ENDPOINT_SECURITY_INFO;
    memcpy(&ps.endpoint_security_info, security, sizeof(nn_security_info_t));
  }
#else
  (void)security;
  assert(security == NULL);
#endif

  if (!alive)
  {
    assert (xqos == NULL);
    assert (epcommon == NULL);
    qosdiff = 0;
  }
  else
  {
    assert (xqos != NULL);
    ps.present |= PP_PROTOCOL_VERSION | PP_VENDORID;
    ps.protocol_version.major = RTPS_MAJOR;
    ps.protocol_version.minor = RTPS_MINOR;
    ps.vendorid = NN_VENDORID_ECLIPSE;

    assert (epcommon != NULL);

    if (epcommon->group_guid.entityid.u != 0)
    {
      ps.present |= PP_GROUP_GUID;
      ps.group_guid = epcommon->group_guid;
    }

#ifdef DDS_HAS_SSM
    /* A bit of a hack -- the easy alternative would be to make it yet
    another parameter.  We only set "reader favours SSM" if we
    really do: no point in telling the world that everything is at
    the default. */
    if (is_reader_entityid (guid->entityid))
    {
      const struct reader *rd = entidx_lookup_reader_guid (gv->entity_index, guid);
      assert (rd);
      if (rd->favours_ssm)
      {
        ps.present |= PP_READER_FAVOURS_SSM;
        ps.reader_favours_ssm.state = 1u;
      }
    }
#endif

    qosdiff = ddsi_xqos_delta (xqos, defqos, ~(uint64_t)0);
    if (gv->config.explicitly_publish_qos_set_to_default)
      qosdiff |= ~QP_UNRECOGNIZED_INCOMPATIBLE_MASK;

    if (as)
    {
      struct add_locator_to_ps_arg arg;
      arg.gv = gv;
      arg.ps = &ps;
      addrset_forall (as, add_locator_to_ps, &arg);
    }

#ifdef DDS_HAS_TYPE_DISCOVERY
    ps.qos.present |= QP_CYCLONE_TYPE_INFORMATION;
    ps.qos.type_information.length = sizeof (*type_id);
    ps.qos.type_information.value = ddsrt_memdup (&type_id->hash, ps.qos.type_information.length);
#endif
  }

  if (xqos)
    ddsi_xqos_mergein_missing (&ps.qos, xqos, qosdiff);
  return write_and_fini_plist (wr, &ps, alive);
}

#ifdef DDS_HAS_TOPIC_DISCOVERY

static int sedp_write_topic_impl (struct writer *wr, int alive, const ddsi_guid_t *guid, const dds_qos_t *xqos, type_identifier_t *type_id)
{
  struct ddsi_domaingv * const gv = wr->e.gv;
  const dds_qos_t *defqos = &gv->default_xqos_tp;

  ddsi_plist_t ps;
  ddsi_plist_init_empty (&ps);
  ps.present |= PP_CYCLONE_TOPIC_GUID;
  ps.topic_guid = *guid;

  assert (xqos != NULL);
  ps.present |= PP_PROTOCOL_VERSION | PP_VENDORID;
  ps.protocol_version.major = RTPS_MAJOR;
  ps.protocol_version.minor = RTPS_MINOR;
  ps.vendorid = NN_VENDORID_ECLIPSE;

  uint64_t qosdiff = ddsi_xqos_delta (xqos, defqos, ~(uint64_t)0);
  if (gv->config.explicitly_publish_qos_set_to_default)
    qosdiff |= ~QP_UNRECOGNIZED_INCOMPATIBLE_MASK;

  if (!ddsi_typeid_none (type_id))
  {
    ps.qos.present |= QP_CYCLONE_TYPE_INFORMATION;
    ps.qos.type_information.length = sizeof (*type_id);
    ps.qos.type_information.value = ddsrt_memdup (&type_id->hash, ps.qos.type_information.length);
  }
  if (xqos)
    ddsi_xqos_mergein_missing (&ps.qos, xqos, qosdiff);
  return write_and_fini_plist (wr, &ps, alive);
}

int sedp_write_topic (struct topic *tp, bool alive)
{
  int res = 0;
  if (!(tp->pp->bes & NN_DISC_BUILTIN_ENDPOINT_TOPICS_ANNOUNCER))
    return res;
  if (!is_builtin_entityid (tp->e.guid.entityid, NN_VENDORID_ECLIPSE) && !tp->e.onlylocal)
  {
    unsigned entityid = determine_topic_writer (tp);
    struct writer *sedp_wr = get_sedp_writer (tp->pp, entityid);
    ddsrt_mutex_lock (&tp->e.qos_lock);
    res = sedp_write_topic_impl (sedp_wr, alive, &tp->e.guid, tp->definition->xqos, &tp->definition->type_id);
    ddsrt_mutex_unlock (&tp->e.qos_lock);
  }
  return res;
}

#endif /* DDS_HAS_TOPIC_DISCOVERY */

int sedp_write_writer (struct writer *wr)
{
  if ((!is_builtin_entityid(wr->e.guid.entityid, NN_VENDORID_ECLIPSE)) && (!wr->e.onlylocal))
  {
    unsigned entityid = determine_publication_writer(wr);
    struct writer *sedp_wr = get_sedp_writer (wr->c.pp, entityid);
    nn_security_info_t *security = NULL;
#ifdef DDS_HAS_SSM
    struct addrset *as = wr->ssm_as;
#else
    struct addrset *as = NULL;
#endif
#ifdef DDS_HAS_SECURITY
    nn_security_info_t tmp;
    if (q_omg_get_writer_security_info(wr, &tmp))
    {
      security = &tmp;
    }
#endif
#ifdef DDS_HAS_TYPE_DISCOVERY
    return sedp_write_endpoint_impl (sedp_wr, 1, &wr->e.guid, &wr->e, &wr->c, wr->xqos, as, security, &wr->c.type_id);
#else
    return sedp_write_endpoint_impl (sedp_wr, 1, &wr->e.guid, &wr->e, &wr->c, wr->xqos, as, security);
#endif
  }
  return 0;
}

int sedp_write_reader (struct reader *rd)
{
  if ((!is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE)) && (!rd->e.onlylocal))
  {
    unsigned entityid = determine_subscription_writer(rd);
    struct writer *sedp_wr = get_sedp_writer (rd->c.pp, entityid);
    nn_security_info_t *security = NULL;
#ifdef DDS_HAS_NETWORK_PARTITIONS
    struct addrset *as = rd->as;
#else
    struct addrset *as = NULL;
#endif
#ifdef DDS_HAS_SECURITY
    nn_security_info_t tmp;
    if (q_omg_get_reader_security_info(rd, &tmp))
    {
      security = &tmp;
    }
#endif
#ifdef DDS_HAS_TYPE_DISCOVERY
    return sedp_write_endpoint_impl (sedp_wr, 1, &rd->e.guid, &rd->e, &rd->c, rd->xqos, as, security, &rd->c.type_id);
#else
    return sedp_write_endpoint_impl (sedp_wr, 1, &rd->e.guid, &rd->e, &rd->c, rd->xqos, as, security);
#endif
  }
  return 0;
}

int sedp_dispose_unregister_writer (struct writer *wr)
{
  if ((!is_builtin_entityid(wr->e.guid.entityid, NN_VENDORID_ECLIPSE)) && (!wr->e.onlylocal))
  {
    unsigned entityid = determine_publication_writer(wr);
    struct writer *sedp_wr = get_sedp_writer (wr->c.pp, entityid);
#ifdef DDS_HAS_TYPE_DISCOVERY
    return sedp_write_endpoint_impl (sedp_wr, 0, &wr->e.guid, NULL, NULL, NULL, NULL, NULL, NULL);
#else
    return sedp_write_endpoint_impl (sedp_wr, 0, &wr->e.guid, NULL, NULL, NULL, NULL, NULL);
#endif
  }
  return 0;
}

int sedp_dispose_unregister_reader (struct reader *rd)
{
  if ((!is_builtin_entityid(rd->e.guid.entityid, NN_VENDORID_ECLIPSE)) && (!rd->e.onlylocal))
  {
    unsigned entityid = determine_subscription_writer(rd);
    struct writer *sedp_wr = get_sedp_writer (rd->c.pp, entityid);
#ifdef DDS_HAS_TYPE_DISCOVERY
    return sedp_write_endpoint_impl (sedp_wr, 0, &rd->e.guid, NULL, NULL, NULL, NULL, NULL, NULL);
#else
    return sedp_write_endpoint_impl (sedp_wr, 0, &rd->e.guid, NULL, NULL, NULL, NULL, NULL);
#endif
  }
  return 0;
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

static struct proxy_participant *implicitly_create_proxypp (struct ddsi_domaingv *gv, const ddsi_guid_t *ppguid, ddsi_plist_t *datap /* note: potentially modifies datap */, const ddsi_guid_prefix_t *src_guid_prefix, nn_vendorid_t vendorid, ddsrt_wctime_t timestamp, seqno_t seq)
{
  ddsi_guid_t privguid;
  ddsi_plist_t pp_plist;

  if (memcmp (&ppguid->prefix, src_guid_prefix, sizeof (ppguid->prefix)) == 0)
    /* if the writer is owned by the participant itself, we're not interested */
    return NULL;

  privguid.prefix = *src_guid_prefix;
  privguid.entityid = to_entityid (NN_ENTITYID_PARTICIPANT);
  ddsi_plist_init_empty(&pp_plist);

  if (vendor_is_cloud (vendorid))
  {
    nn_vendorid_t actual_vendorid;
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
    (void) new_proxy_participant(gv, ppguid, 0, &privguid, new_addrset(), new_addrset(), &pp_plist, DDS_INFINITY, actual_vendorid, CF_IMPLICITLY_CREATED_PROXYPP, timestamp, seq);
  }
  else if (ppguid->prefix.u[0] == src_guid_prefix->u[0] && vendor_is_eclipse_or_opensplice (vendorid))
  {
    /* FIXME: requires address sets to be those of ddsi2, no built-in
       readers or writers, only if remote ddsi2 is provably running
       with a minimal built-in endpoint set */
    struct proxy_participant *privpp;
    if ((privpp = entidx_lookup_proxy_participant_guid (gv->entity_index, &privguid)) == NULL) {
      GVTRACE (" unknown-src-proxypp?\n");
      goto err;
    } else if (!privpp->is_ddsi2_pp) {
      GVTRACE (" src-proxypp-not-ddsi2?\n");
      goto err;
    } else if (!privpp->minimal_bes_mode) {
      GVTRACE (" src-ddsi2-not-minimal-bes-mode?\n");
      goto err;
    } else {
      struct addrset *as_default, *as_meta;
      ddsi_plist_t tmp_plist;
      GVTRACE (" from-ddsi2 "PGUIDFMT, PGUID (privguid));
      ddsi_plist_init_empty (&pp_plist);

      ddsrt_mutex_lock (&privpp->e.lock);
      as_default = ref_addrset(privpp->as_default);
      as_meta = ref_addrset(privpp->as_meta);
      /* copy just what we need */
      tmp_plist = *privpp->plist;
      tmp_plist.present = PP_PARTICIPANT_GUID | PP_ADLINK_PARTICIPANT_VERSION_INFO;
      tmp_plist.participant_guid = *ppguid;
      ddsi_plist_mergein_missing (&pp_plist, &tmp_plist, ~(uint64_t)0, ~(uint64_t)0);
      ddsrt_mutex_unlock (&privpp->e.lock);

      pp_plist.adlink_participant_version_info.flags &= ~NN_ADLINK_FL_PARTICIPANT_IS_DDSI2;
      new_proxy_participant (gv, ppguid, 0, &privguid, as_default, as_meta, &pp_plist, DDS_INFINITY, vendorid, CF_IMPLICITLY_CREATED_PROXYPP | CF_PROXYPP_NO_SPDP, timestamp, seq);
    }
  }

 err:
  ddsi_plist_fini (&pp_plist);
  return entidx_lookup_proxy_participant_guid (gv->entity_index, ppguid);
}

static bool handle_sedp_checks (struct ddsi_domaingv * const gv, ddsi_guid_t *entity_guid, ddsi_plist_t *datap,
    const ddsi_guid_prefix_t *src_guid_prefix, nn_vendorid_t vendorid, ddsrt_wctime_t timestamp,
    struct proxy_participant **proxypp, ddsi_guid_t *ppguid)
{
#define E(msg, lbl) do { GVLOGDISC (msg); return false; } while (0)
  ppguid->prefix = entity_guid->prefix;
  ppguid->entityid.u = NN_ENTITYID_PARTICIPANT;
  if (is_deleted_participant_guid (gv->deleted_participants, ppguid, DPG_REMOTE))
    E (" local dead pp?\n", err);
  if (entidx_lookup_participant_guid (gv->entity_index, ppguid) != NULL)
    E (" local pp?\n", err);
  if (is_builtin_entityid (entity_guid->entityid, vendorid))
    E (" built-in\n", err);
  if (!(datap->qos.present & QP_TOPIC_NAME))
    E (" no topic?\n", err);
  if (!(datap->qos.present & QP_TYPE_NAME))
    E (" no typename?\n", err);
  if ((*proxypp = entidx_lookup_proxy_participant_guid (gv->entity_index, ppguid)) == NULL)
  {
    GVLOGDISC (" unknown-proxypp");
    if ((*proxypp = implicitly_create_proxypp (gv, ppguid, datap, src_guid_prefix, vendorid, timestamp, 0)) == NULL)
      E ("?\n", err);
    /* Repeat regular SEDP trace for convenience */
    GVLOGDISC ("SEDP ST0 "PGUIDFMT" (cont)", PGUID (*entity_guid));
  }
  return true;
#undef E
}

static void handle_sedp_alive_endpoint (const struct receiver_state *rst, seqno_t seq, ddsi_plist_t *datap /* note: potentially modifies datap */, ddsi_sedp_kind_t sedp_kind, const ddsi_guid_prefix_t *src_guid_prefix, nn_vendorid_t vendorid, ddsrt_wctime_t timestamp)
{
#define E(msg, lbl) do { GVLOGDISC (msg); goto lbl; } while (0)
  struct ddsi_domaingv * const gv = rst->gv;
  struct proxy_participant *proxypp;
  struct proxy_writer * pwr = NULL;
  struct proxy_reader * prd = NULL;
  ddsi_guid_t ppguid;
  dds_qos_t *xqos;
  int reliable;
  struct addrset *as;
#ifdef DDS_HAS_SSM
  int ssm;
#endif

  assert (datap);
  assert (datap->present & PP_ENDPOINT_GUID);
  GVLOGDISC (" "PGUIDFMT, PGUID (datap->endpoint_guid));

  if (!handle_sedp_checks (gv, &datap->endpoint_guid, datap, src_guid_prefix, vendorid, timestamp, &proxypp, &ppguid))
    goto err;

  xqos = &datap->qos;
  if (sedp_kind == SEDP_KIND_READER)
    ddsi_xqos_mergein_missing (xqos, &gv->default_xqos_rd, ~(uint64_t)0);
  else if (sedp_kind == SEDP_KIND_WRITER)
  {
    if (vendor_is_eclipse_or_adlink (vendorid))
      ddsi_xqos_mergein_missing (xqos, &gv->default_xqos_wr, ~(uint64_t)0);
    else
      ddsi_xqos_mergein_missing (xqos, &gv->default_xqos_wr_nad, ~(uint64_t)0);
  }
  else
    E (" invalid entity kind\n", err);

  /* After copy + merge, should have at least the ones present in the
     input.  Also verify reliability and durability are present,
     because we explicitly read those. */
  assert ((xqos->present & datap->qos.present) == datap->qos.present);
  assert (xqos->present & QP_RELIABILITY);
  assert (xqos->present & QP_DURABILITY);
  reliable = (xqos->reliability.kind == DDS_RELIABILITY_RELIABLE);

  GVLOGDISC (" %s %s %s: %s%s.%s/%s",
             reliable ? "reliable" : "best-effort",
             durability_to_string (xqos->durability.kind),
             sedp_kind == SEDP_KIND_WRITER ? "writer" : "reader",
             ((!(xqos->present & QP_PARTITION) || xqos->partition.n == 0 || *xqos->partition.strs[0] == '\0')
              ? "(default)" : xqos->partition.strs[0]),
             ((xqos->present & QP_PARTITION) && xqos->partition.n > 1) ? "+" : "",
             xqos->topic_name, xqos->type_name);
#ifdef DDS_HAS_TYPE_DISCOVERY
  type_identifier_t type_id;
  if ((xqos->present & QP_CYCLONE_TYPE_INFORMATION) && xqos->type_information.length == sizeof (type_id.hash))
  {
    memcpy (type_id.hash, xqos->type_information.value, sizeof (type_id.hash));
    GVLOGDISC (" type-hash "PTYPEIDFMT, PTYPEID (type_id));
  }
#endif

  if (sedp_kind == SEDP_KIND_READER && (datap->present & PP_EXPECTS_INLINE_QOS) && datap->expects_inline_qos)
    E ("******* AARGH - it expects inline QoS ********\n", err);

  q_omg_log_endpoint_protection (gv, datap);
  if (q_omg_is_endpoint_protected (datap) && !q_omg_proxy_participant_is_secure (proxypp))
    E (" remote endpoint is protected while local federation is not secure\n", err);

  if (sedp_kind == SEDP_KIND_WRITER)
    pwr = entidx_lookup_proxy_writer_guid (gv->entity_index, &datap->endpoint_guid);
  else
    prd = entidx_lookup_proxy_reader_guid (gv->entity_index, &datap->endpoint_guid);
  if (pwr || prd)
  {
    /* Re-bind the proxy participant to the discovery service - and do this if it is currently
       bound to another DS instance, because that other DS instance may have already failed and
       with a new one taking over, without our noticing it. */
    GVLOGDISC (" known%s", vendor_is_cloud (vendorid) ? "-DS" : "");
    if (vendor_is_cloud (vendorid) && proxypp->implicitly_created && memcmp (&proxypp->privileged_pp_guid.prefix, src_guid_prefix, sizeof(proxypp->privileged_pp_guid.prefix)) != 0)
    {
      GVLOGDISC (" "PGUIDFMT" attach-to-DS "PGUIDFMT, PGUID(proxypp->e.guid), PGUIDPREFIX(*src_guid_prefix), proxypp->privileged_pp_guid.entityid.u);
      ddsrt_mutex_lock (&proxypp->e.lock);
      proxypp->privileged_pp_guid.prefix = *src_guid_prefix;
      lease_set_expiry (proxypp->lease, DDSRT_ETIME_NEVER);
      ddsrt_mutex_unlock (&proxypp->e.lock);
    }
    GVLOGDISC ("\n");
  }
  else
  {
    GVLOGDISC (" NEW");
  }

  {
    ddsi_locator_t loc;
    as = new_addrset ();
    if (!gv->config.tcp_use_peeraddr_for_unicast && (datap->present & PP_UNICAST_LOCATOR) && get_locator (gv, &loc, &datap->unicast_locators, 0))
      add_to_addrset (gv, as, &loc);
    else if (gv->config.tcp_use_peeraddr_for_unicast)
    {
      GVLOGDISC (" (srcloc)");
      add_to_addrset (gv, as, &rst->srcloc);
    }
    else
    {
      copy_addrset_into_addrset_uc (gv, as, proxypp->as_default);
    }
    if ((datap->present & PP_MULTICAST_LOCATOR) && get_locator (gv, &loc, &datap->multicast_locators, 0))
      allowmulticast_aware_add_to_addrset (gv, gv->config.allowMulticast, as, &loc);
    else
      copy_addrset_into_addrset_mc (gv, as, proxypp->as_default);
  }
  if (addrset_empty (as))
  {
    unref_addrset (as);
    E (" no address", err);
  }

  nn_log_addrset(gv, DDS_LC_DISCOVERY, " (as", as);
#ifdef DDS_HAS_SSM
  ssm = 0;
  if (sedp_kind == SEDP_KIND_WRITER)
    ssm = addrset_contains_ssm (gv, as);
  else if (datap->present & PP_READER_FAVOURS_SSM)
    ssm = (datap->reader_favours_ssm.state != 0);
  GVLOGDISC (" ssm=%u", ssm);
#endif
  GVLOGDISC (") QOS={");
  ddsi_xqos_log (DDS_LC_DISCOVERY, &gv->logconfig, xqos);
  GVLOGDISC ("}\n");

  if ((datap->endpoint_guid.entityid.u & NN_ENTITYID_SOURCE_MASK) == NN_ENTITYID_SOURCE_VENDOR && !vendor_is_eclipse_or_adlink (vendorid))
  {
    GVLOGDISC ("ignoring vendor-specific endpoint "PGUIDFMT"\n", PGUID (datap->endpoint_guid));
  }
  else
  {
    if (sedp_kind == SEDP_KIND_WRITER)
    {
      if (pwr)
        update_proxy_writer (pwr, seq, as, xqos, timestamp);
      else
      {
        /* not supposed to get here for built-in ones, so can determine the channel based on the transport priority */
        assert (!is_builtin_entityid (datap->endpoint_guid.entityid, vendorid));
#ifdef DDS_HAS_NETWORK_CHANNELS
        {
          struct ddsi_config_channel_listelem *channel = find_channel (&gv->config, xqos->transport_priority);
          new_proxy_writer (gv, &ppguid, &datap->endpoint_guid, as, datap, channel->dqueue, channel->evq ? channel->evq : gv->xevents, timestamp, seq);
        }
#else
        new_proxy_writer (gv, &ppguid, &datap->endpoint_guid, as, datap, gv->user_dqueue, gv->xevents, timestamp, seq);
#endif
      }
    }
    else
    {
      if (prd)
        update_proxy_reader (prd, seq, as, xqos, timestamp);
      else
      {
#ifdef DDS_HAS_SSM
        new_proxy_reader (gv, &ppguid, &datap->endpoint_guid, as, datap, timestamp, seq, ssm);
#else
        new_proxy_reader (gv, &ppguid, &datap->endpoint_guid, as, datap, timestamp, seq);
#endif
      }
    }
  }
  unref_addrset (as);

err:
  return;
#undef E
}

static void handle_sedp_dead_endpoint (const struct receiver_state *rst, ddsi_plist_t *datap, ddsi_sedp_kind_t sedp_kind, ddsrt_wctime_t timestamp)
{
  struct ddsi_domaingv * const gv = rst->gv;
  int res = -1;
  assert (datap->present & PP_ENDPOINT_GUID);
  GVLOGDISC (" "PGUIDFMT" ", PGUID (datap->endpoint_guid));
  if (sedp_kind == SEDP_KIND_WRITER)
    res = delete_proxy_writer (gv, &datap->endpoint_guid, timestamp, 0);
  else
    res = delete_proxy_reader (gv, &datap->endpoint_guid, timestamp, 0);
  GVLOGDISC (" %s\n", (res < 0) ? " unknown" : " delete");
}

#ifdef DDS_HAS_TOPIC_DISCOVERY

static void handle_sedp_alive_topic (const struct receiver_state *rst, seqno_t seq, ddsi_plist_t *datap /* note: potentially modifies datap */, const ddsi_guid_prefix_t *src_guid_prefix, nn_vendorid_t vendorid, ddsrt_wctime_t timestamp)
{
  struct ddsi_domaingv * const gv = rst->gv;
  struct proxy_participant *proxypp;
  ddsi_guid_t ppguid;
  dds_qos_t *xqos;
  int reliable;
  type_identifier_t type_id = { .hash = { 0 }};

  assert (datap);
  assert (datap->present & PP_CYCLONE_TOPIC_GUID);
  GVLOGDISC (" "PGUIDFMT, PGUID (datap->topic_guid));

  if (!handle_sedp_checks (gv, &datap->topic_guid, datap, src_guid_prefix, vendorid, timestamp, &proxypp, &ppguid))
    return;

  xqos = &datap->qos;
  ddsi_xqos_mergein_missing (xqos, &gv->default_xqos_tp, ~(uint64_t)0);
  /* After copy + merge, should have at least the ones present in the
     input. Also verify reliability and durability are present,
     because we explicitly read those. */
  assert ((xqos->present & datap->qos.present) == datap->qos.present);
  assert (xqos->present & QP_RELIABILITY);
  assert (xqos->present & QP_DURABILITY);
  reliable = (xqos->reliability.kind == DDS_RELIABILITY_RELIABLE);

  GVLOGDISC (" %s %s %s: %s/%s",
             reliable ? "reliable" : "best-effort",
             durability_to_string (xqos->durability.kind),
             "topic", xqos->topic_name, xqos->type_name);
  if ((xqos->present & QP_CYCLONE_TYPE_INFORMATION) && xqos->type_information.length == sizeof (type_id.hash))
  {
    memcpy (type_id.hash, xqos->type_information.value, sizeof (type_id.hash));
    GVLOGDISC (" type-hash "PTYPEIDFMT, PTYPEID(type_id));
  }
  GVLOGDISC (" QOS={");
  ddsi_xqos_log (DDS_LC_DISCOVERY, &gv->logconfig, xqos);
  GVLOGDISC ("}\n");

  if ((datap->topic_guid.entityid.u & NN_ENTITYID_SOURCE_MASK) == NN_ENTITYID_SOURCE_VENDOR && !vendor_is_eclipse_or_adlink (vendorid))
  {
    GVLOGDISC ("ignoring vendor-specific topic "PGUIDFMT"\n", PGUID (datap->topic_guid));
  }
  else
  {
    struct proxy_topic *ptp = lookup_proxy_topic (proxypp, &datap->topic_guid);
    if (ptp)
    {
      GVLOGDISC (" update known proxy-topic%s\n", vendor_is_cloud (vendorid) ? "-DS" : "");
      update_proxy_topic (proxypp, ptp, seq, xqos, timestamp);
    }
    else
    {
      GVLOGDISC (" NEW proxy-topic");
      new_proxy_topic (proxypp, seq, &datap->topic_guid, &type_id, xqos, timestamp);
    }
  }
}

static void handle_sedp_dead_topic (const struct receiver_state *rst, ddsi_plist_t *datap, ddsrt_wctime_t timestamp)
{
  struct proxy_participant *proxypp;
  struct proxy_topic *proxytp;
  struct ddsi_domaingv * const gv = rst->gv;
  assert (datap->present & PP_CYCLONE_TOPIC_GUID);
  GVLOGDISC (" "PGUIDFMT" ", PGUID (datap->topic_guid));
  ddsi_guid_t ppguid = { .prefix = datap->topic_guid.prefix, .entityid.u = NN_ENTITYID_PARTICIPANT };
  if ((proxypp = entidx_lookup_proxy_participant_guid (gv->entity_index, &ppguid)) == NULL)
    GVLOGDISC (" unknown proxypp\n");
  else if ((proxytp = lookup_proxy_topic (proxypp, &datap->topic_guid)) == NULL)
    GVLOGDISC (" unknown proxy topic\n");
  else
  {
    ddsrt_mutex_lock (&proxypp->e.lock);
    int res = delete_proxy_topic_locked (proxypp, proxytp, timestamp);
    GVLOGDISC (" %s\n", res == DDS_RETCODE_PRECONDITION_NOT_MET ? " already-deleting" : " delete");
    ddsrt_mutex_unlock (&proxypp->e.lock);
  }
}

#endif /* DDS_HAS_TOPIC_DISCOVERY */

static void handle_sedp (const struct receiver_state *rst, seqno_t seq, struct ddsi_serdata *serdata, ddsi_sedp_kind_t sedp_kind)
{
  ddsi_plist_t decoded_data;
  if (ddsi_serdata_to_sample (serdata, &decoded_data, NULL, NULL))
  {
    struct ddsi_domaingv * const gv = rst->gv;
    GVLOGDISC ("SEDP ST%"PRIx32, serdata->statusinfo);
    switch (serdata->statusinfo & (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER))
    {
      case 0:
#ifdef DDS_HAS_TOPIC_DISCOVERY
        if (sedp_kind == SEDP_KIND_TOPIC)
          handle_sedp_alive_topic (rst, seq, &decoded_data, &rst->src_guid_prefix, rst->vendor, serdata->timestamp);
        else
#endif
          handle_sedp_alive_endpoint (rst, seq, &decoded_data, sedp_kind, &rst->src_guid_prefix, rst->vendor, serdata->timestamp);
        break;
      case NN_STATUSINFO_DISPOSE:
      case NN_STATUSINFO_UNREGISTER:
      case (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER):
#ifdef DDS_HAS_TOPIC_DISCOVERY
        if (sedp_kind == SEDP_KIND_TOPIC)
          handle_sedp_dead_topic (rst, &decoded_data, serdata->timestamp);
        else
#endif
          handle_sedp_dead_endpoint (rst, &decoded_data, sedp_kind, serdata->timestamp);
        break;
    }
    ddsi_plist_fini (&decoded_data);
  }
}

#ifdef DDS_HAS_TYPE_DISCOVERY
static void handle_typelookup (const struct receiver_state *rst, ddsi_entityid_t wr_entity_id, struct ddsi_serdata *serdata)
{
  if (!(serdata->statusinfo & (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER)))
  {
    struct ddsi_domaingv * const gv = rst->gv;
    if (wr_entity_id.u == NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER)
      ddsi_tl_handle_request (gv, serdata);
    else if (wr_entity_id.u == NN_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER)
      ddsi_tl_handle_reply (gv, serdata);
    else
      assert (false);
  }
}
#endif

/******************************************************************************
 *****************************************************************************/

int builtins_dqueue_handler (const struct nn_rsample_info *sampleinfo, const struct nn_rdata *fragchain, UNUSED_ARG (const ddsi_guid_t *rdguid), UNUSED_ARG (void *qarg))
{
  struct ddsi_domaingv * const gv = sampleinfo->rst->gv;
  struct proxy_writer *pwr;
  unsigned statusinfo;
  int need_keyhash;
  ddsi_guid_t srcguid;
  Data_DataFrag_common_t *msg;
  unsigned char data_smhdr_flags;
  ddsi_plist_t qos;

  /* Luckily, most of the Data and DataFrag headers are the same - and
     in particular, all that we care about here is the same.  The
     key/data flags of DataFrag are different from those of Data, but
     DDSI2 used to treat them all as if they are data :( so now,
     instead of splitting out all the code, we reformat these flags
     from the submsg to always conform to that of the "Data"
     submessage regardless of the input. */
  msg = (Data_DataFrag_common_t *) NN_RMSG_PAYLOADOFF (fragchain->rmsg, NN_RDATA_SUBMSG_OFF (fragchain));
  data_smhdr_flags = normalize_data_datafrag_flags (&msg->smhdr);
  srcguid.prefix = sampleinfo->rst->src_guid_prefix;
  srcguid.entityid = msg->writerId;

  pwr = sampleinfo->pwr;
  if (pwr == NULL)
  {
    /* NULL with NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER is normal. It is possible that
     * NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER has NULL as well if there
     * is a security mismatch being handled. */
    assert ((srcguid.entityid.u == NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER) ||
            (srcguid.entityid.u == NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER));
  }
  else
  {
    assert (is_builtin_entityid (pwr->e.guid.entityid, pwr->c.vendor));
    assert (memcmp (&pwr->e.guid, &srcguid, sizeof (srcguid)) == 0);
    assert (srcguid.entityid.u != NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER);
  }

  /* If there is no payload, it is either a completely invalid message
     or a dispose/unregister in RTI style. We assume the latter,
     consequently expect to need the keyhash.  Then, if sampleinfo
     says it is a complex qos, or the keyhash is required, extract all
     we need from the inline qos. */
  need_keyhash = (sampleinfo->size == 0 || (data_smhdr_flags & (DATA_FLAG_KEYFLAG | DATA_FLAG_DATAFLAG)) == 0);
  if (!(sampleinfo->complex_qos || need_keyhash))
  {
    ddsi_plist_init_empty (&qos);
    statusinfo = sampleinfo->statusinfo;
  }
  else
  {
    ddsi_plist_src_t src;
    size_t qos_offset = NN_RDATA_SUBMSG_OFF (fragchain) + offsetof (Data_DataFrag_common_t, octetsToInlineQos) + sizeof (msg->octetsToInlineQos) + msg->octetsToInlineQos;
    dds_return_t plist_ret;
    src.protocol_version = sampleinfo->rst->protocol_version;
    src.vendorid = sampleinfo->rst->vendor;
    src.encoding = (msg->smhdr.flags & SMFLAG_ENDIANNESS) ? PL_CDR_LE : PL_CDR_BE;
    src.buf = NN_RMSG_PAYLOADOFF (fragchain->rmsg, qos_offset);
    src.bufsz = NN_RDATA_PAYLOAD_OFF (fragchain) - qos_offset;
    src.strict = DDSI_SC_STRICT_P (gv->config);
    src.factory = gv->m_factory;
    src.logconfig = &gv->logconfig;
    if ((plist_ret = ddsi_plist_init_frommsg (&qos, NULL, PP_STATUSINFO | PP_KEYHASH, 0, &src)) < 0)
    {
      if (plist_ret != DDS_RETCODE_UNSUPPORTED)
        GVWARNING ("data(builtin, vendor %u.%u): "PGUIDFMT" #%"PRId64": invalid inline qos\n",
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
    case NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER:
      type = gv->spdp_type;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER:
      type = gv->sedp_writer_type;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER:
      type = gv->sedp_reader_type;
      break;
#ifdef DDS_HAS_TOPIC_DISCOVERY
    case NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER:
      type = gv->sedp_topic_type;
      break;
#endif
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER:
      type = gv->pmd_type;
      break;
#ifdef DDS_HAS_TYPE_DISCOVERY
    case NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER:
      type = gv->tl_svc_request_type;
      break;
    case NN_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER:
      type = gv->tl_svc_reply_type;
      break;
#endif
#ifdef DDS_HAS_SECURITY
    case NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER:
      type = gv->spdp_secure_type;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER:
      type = gv->sedp_writer_secure_type;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER:
      type = gv->sedp_reader_secure_type;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER:
      type = gv->pmd_secure_type;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER:
      type = gv->pgm_stateless_type;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER:
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
  if (data_smhdr_flags & DATA_FLAG_DATAFLAG)
    d = ddsi_serdata_from_ser (type, SDK_DATA, fragchain, sampleinfo->size);
  else if (data_smhdr_flags & DATA_FLAG_KEYFLAG)
    d = ddsi_serdata_from_ser (type, SDK_KEY, fragchain, sampleinfo->size);
  else if ((qos.present & PP_KEYHASH) && !DDSI_SC_STRICT_P(gv->config))
    d = ddsi_serdata_from_keyhash (type, &qos.keyhash);
  else
  {
    GVLOGDISC ("data(builtin, vendor %u.%u): "PGUIDFMT" #%"PRId64": missing payload\n",
               sampleinfo->rst->vendor.id[0], sampleinfo->rst->vendor.id[1],
               PGUID (srcguid), sampleinfo->seq);
    goto done_upd_deliv;
  }
  if (d == NULL)
  {
    GVLOG (DDS_LC_DISCOVERY | DDS_LC_WARNING, "data(builtin, vendor %u.%u): "PGUIDFMT" #%"PRId64": deserialization failed\n",
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
    GVTRACE ("data(builtin, vendor %u.%u): "PGUIDFMT" #%"PRId64": ST%x %s/%s:%s%s\n",
             sampleinfo->rst->vendor.id[0], sampleinfo->rst->vendor.id[1],
             PGUID (guid), sampleinfo->seq, statusinfo,
             pwr ? pwr->c.xqos->topic_name : "", d->type->type_name,
             tmp, res < sizeof (tmp) - 1 ? "" : "(trunc)");
  }

  switch (srcguid.entityid.u)
  {
    case NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER:
    case NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER:
      handle_spdp (sampleinfo->rst, srcguid.entityid, sampleinfo->seq, d);
      break;
    case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER:
    case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER:
      handle_sedp (sampleinfo->rst, sampleinfo->seq, d, SEDP_KIND_WRITER);
      break;
    case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER:
    case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER:
      handle_sedp (sampleinfo->rst, sampleinfo->seq, d, SEDP_KIND_READER);
      break;
    case NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER:
      handle_sedp (sampleinfo->rst, sampleinfo->seq, d, SEDP_KIND_TOPIC);
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER:
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER:
      handle_pmd_message (sampleinfo->rst, d);
      break;
#ifdef DDS_HAS_TYPE_DISCOVERY
    case NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER:
    case NN_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER:
      handle_typelookup (sampleinfo->rst, srcguid.entityid, d);
      break;
#endif
#ifdef DDS_HAS_SECURITY
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER:
      handle_auth_handshake_message(sampleinfo->rst, srcguid.entityid, d);
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER:
      handle_crypto_exchange_message(sampleinfo->rst, d);
      break;
#endif
    default:
      GVLOGDISC ("data(builtin, vendor %u.%u): "PGUIDFMT" #%"PRId64": not handled\n",
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
