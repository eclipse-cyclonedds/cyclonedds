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
#include "dds/ddsi/q_plist.h"
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_xevent.h"
#include "dds/ddsi/q_addrset.h"
#include "dds/ddsi/q_ddsi_discovery.h"

#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_globals.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_transmit.h"
#include "dds/ddsi/q_lease.h"
#include "dds/ddsi/ddsi_serdata_default.h"
#include "dds/ddsi/q_feature_check.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsi/ddsi_pmd.h"

static int get_locator (const struct q_globals *gv, nn_locator_t *loc, const nn_locators_t *locs, int uc_same_subnet)
{
  struct nn_locators_one *l;
  nn_locator_t first, samenet;
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

    switch (ddsi_is_nearby_address(gv, &l->loc, &gv->ownloc, (size_t) gv->n_interfaces, gv->interfaces))
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

static void maybe_add_pp_as_meta_to_as_disc (struct q_globals *gv, const struct addrset *as_meta)
{
  if (addrset_empty_mc (as_meta) || !(gv->config.allowMulticast & AMC_SPDP))
  {
    nn_locator_t loc;
    if (addrset_any_uc (as_meta, &loc))
    {
      add_to_addrset (gv, gv->as_disc, &loc);
    }
  }
}

static int write_mpayload (struct writer *wr, int alive, nn_parameterid_t keyparam, struct nn_xmsg *mpayload)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  struct ddsi_plist_sample plist_sample;
  struct ddsi_serdata *serdata;
  nn_xmsg_payload_to_plistsample (&plist_sample, keyparam, mpayload);
  serdata = ddsi_serdata_from_sample (wr->e.gv->plist_topic, alive ? SDK_DATA : SDK_KEY, &plist_sample);
  serdata->statusinfo = alive ? 0 : NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER;
  serdata->timestamp = now ();
  return write_sample_nogc_notk (ts1, NULL, wr, serdata);
}

int spdp_write (struct participant *pp)
{
  struct nn_xmsg *mpayload;
  struct nn_locators_one def_uni_loc_one, def_multi_loc_one, meta_uni_loc_one, meta_multi_loc_one;
  nn_plist_t ps;
  struct writer *wr;
  size_t size;
  char node[64];
  uint64_t qosdiff;
  int ret;

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

  /* First create a fake message for the payload: we can add plists to
     xmsgs easily, but not to serdata.  But it is rather easy to copy
     the payload of an xmsg over to a serdata ...  Expected size isn't
     terribly important, the msg will grow as needed, address space is
     essentially meaningless because we only use the message to
     construct the payload. */
  mpayload = nn_xmsg_new (pp->e.gv->xmsgpool, &pp->e.guid, NULL, 0, NN_XMSG_KIND_DATA);

  nn_plist_init_empty (&ps);
  ps.present |= PP_PARTICIPANT_GUID | PP_BUILTIN_ENDPOINT_SET |
    PP_PROTOCOL_VERSION | PP_VENDORID | PP_PARTICIPANT_LEASE_DURATION |
    PP_DOMAIN_ID;
  ps.participant_guid = pp->e.guid;
  ps.builtin_endpoint_set = pp->bes;
  ps.protocol_version.major = RTPS_MAJOR;
  ps.protocol_version.minor = RTPS_MINOR;
  ps.vendorid = NN_VENDORID_ECLIPSE;
  ps.domain_id = pp->e.gv->config.extDomainId.value;
  /* Be sure not to send a DOMAIN_TAG when it is the default (an empty)
     string: it is an "incompatible-if-unrecognized" parameter, and so
     implementations that don't understand the parameter will refuse to
     discover us, and so sending the default would break backwards
     compatibility. */
  if (strcmp (pp->e.gv->config.domainTag, "") != 0)
  {
    ps.present |= PP_DOMAIN_TAG;
    ps.aliased |= PP_DOMAIN_TAG;
    ps.domain_tag = pp->e.gv->config.domainTag;
  }
  if (pp->prismtech_bes)
  {
    ps.present |= PP_PRISMTECH_BUILTIN_ENDPOINT_SET;
    ps.prismtech_builtin_endpoint_set = pp->prismtech_bes;
  }
  ps.default_unicast_locators.n = 1;
  ps.default_unicast_locators.first =
    ps.default_unicast_locators.last = &def_uni_loc_one;
  ps.metatraffic_unicast_locators.n = 1;
  ps.metatraffic_unicast_locators.first =
    ps.metatraffic_unicast_locators.last = &meta_uni_loc_one;
  def_uni_loc_one.next = NULL;
  meta_uni_loc_one.next = NULL;

  if (pp->e.gv->config.many_sockets_mode == MSM_MANY_UNICAST)
  {
    def_uni_loc_one.loc = pp->m_locator;
    meta_uni_loc_one.loc = pp->m_locator;
  }
  else
  {
    def_uni_loc_one.loc = pp->e.gv->loc_default_uc;
    meta_uni_loc_one.loc = pp->e.gv->loc_meta_uc;
  }

  if (pp->e.gv->config.publish_uc_locators)
  {
    ps.present |= PP_DEFAULT_UNICAST_LOCATOR | PP_METATRAFFIC_UNICAST_LOCATOR;
    ps.aliased |= PP_DEFAULT_UNICAST_LOCATOR | PP_METATRAFFIC_UNICAST_LOCATOR;
  }

  if (pp->e.gv->config.allowMulticast)
  {
    int include = 0;
#ifdef DDSI_INCLUDE_SSM
    /* Note that if the default multicast address is an SSM address,
       we will simply advertise it. The recipients better understand
       it means the writers will publish to address and the readers
       favour SSM. */
    if (ddsi_is_ssm_mcaddr (pp->e.gv, &pp->e.gv->loc_default_mc))
      include = (pp->e.gv->config.allowMulticast & AMC_SSM) != 0;
    else
      include = (pp->e.gv->config.allowMulticast & AMC_ASM) != 0;
#else
    if (pp->e.gv->config.allowMulticast & AMC_ASM)
      include = 1;
#endif
    if (include)
    {
      ps.present |= PP_DEFAULT_MULTICAST_LOCATOR | PP_METATRAFFIC_MULTICAST_LOCATOR;
      ps.aliased |= PP_DEFAULT_MULTICAST_LOCATOR | PP_METATRAFFIC_MULTICAST_LOCATOR;
      ps.default_multicast_locators.n = 1;
      ps.default_multicast_locators.first =
      ps.default_multicast_locators.last = &def_multi_loc_one;
      ps.metatraffic_multicast_locators.n = 1;
      ps.metatraffic_multicast_locators.first =
      ps.metatraffic_multicast_locators.last = &meta_multi_loc_one;
      def_multi_loc_one.next = NULL;
      def_multi_loc_one.loc = pp->e.gv->loc_default_mc;
      meta_multi_loc_one.next = NULL;
      meta_multi_loc_one.loc = pp->e.gv->loc_meta_mc;
    }
  }
  ps.participant_lease_duration = pp->lease_duration;

  /* Add PrismTech specific version information */
  {
    ps.present |= PP_PRISMTECH_PARTICIPANT_VERSION_INFO;
    memset (&ps.prismtech_participant_version_info, 0, sizeof (ps.prismtech_participant_version_info));
    ps.prismtech_participant_version_info.version = 0;
    ps.prismtech_participant_version_info.flags =
      NN_PRISMTECH_FL_DDSI2_PARTICIPANT_FLAG |
      NN_PRISMTECH_FL_PTBES_FIXED_0 |
      NN_PRISMTECH_FL_SUPPORTS_STATUSINFOX;
    if (pp->e.gv->config.besmode == BESMODE_MINIMAL)
      ps.prismtech_participant_version_info.flags |= NN_PRISMTECH_FL_MINIMAL_BES_MODE;
    ddsrt_mutex_lock (&pp->e.gv->privileged_pp_lock);
    if (pp->is_ddsi2_pp)
      ps.prismtech_participant_version_info.flags |= NN_PRISMTECH_FL_PARTICIPANT_IS_DDSI2;
    ddsrt_mutex_unlock (&pp->e.gv->privileged_pp_lock);

    if (ddsrt_gethostname(node, sizeof(node)-1) < 0)
      (void) ddsrt_strlcpy (node, "unknown", sizeof (node));
    size = strlen(node) + strlen(DDS_VERSION) + strlen(DDS_HOST_NAME) + strlen(DDS_TARGET_NAME) + 4; /* + ///'\0' */
    ps.prismtech_participant_version_info.internals = ddsrt_malloc(size);
    (void) snprintf(ps.prismtech_participant_version_info.internals, size, "%s/%s/%s/%s", node, DDS_VERSION, DDS_HOST_NAME, DDS_TARGET_NAME);
    ETRACE (pp, "spdp_write("PGUIDFMT") - internals: %s\n", PGUID (pp->e.guid), ps.prismtech_participant_version_info.internals);
  }

  /* Participant QoS's insofar as they are set, different from the default, and mapped to the SPDP data, rather than to the PrismTech-specific CMParticipant endpoint.  Currently, that means just USER_DATA. */
  qosdiff = nn_xqos_delta (&pp->plist->qos, &pp->e.gv->default_plist_pp.qos, QP_USER_DATA);
  if (pp->e.gv->config.explicitly_publish_qos_set_to_default)
    qosdiff |= ~QP_UNRECOGNIZED_INCOMPATIBLE_MASK;

  assert (ps.qos.present == 0);
  nn_plist_addtomsg (mpayload, &ps, ~(uint64_t)0, 0);
  nn_plist_addtomsg (mpayload, pp->plist, 0, qosdiff);
  nn_xmsg_addpar_sentinel (mpayload);
  nn_plist_fini (&ps);

  ret = write_mpayload (wr, 1, PID_PARTICIPANT_GUID, mpayload);
  nn_xmsg_free (mpayload);
  return ret;
}

static int spdp_dispose_unregister_with_wr (struct participant *pp, unsigned entityid)
{
  struct nn_xmsg *mpayload;
  nn_plist_t ps;
  struct writer *wr;
  int ret;

  if ((wr = get_builtin_writer (pp, entityid)) == NULL)
  {
    ETRACE (pp, "spdp_dispose_unregister("PGUIDFMT") - builtin participant %s writer not found\n",
            PGUID (pp->e.guid),
            entityid == NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER ? "secure" : "");
    return 0;
  }

  mpayload = nn_xmsg_new (pp->e.gv->xmsgpool, &pp->e.guid, NULL, 0, NN_XMSG_KIND_DATA);
  nn_plist_init_empty (&ps);
  ps.present |= PP_PARTICIPANT_GUID;
  ps.participant_guid = pp->e.guid;
  nn_plist_addtomsg (mpayload, &ps, ~(uint64_t)0, ~(uint64_t)0);
  nn_xmsg_addpar_sentinel (mpayload);
  nn_plist_fini (&ps);

  ret = write_mpayload (wr, 0, PID_PARTICIPANT_GUID, mpayload);
  nn_xmsg_free (mpayload);
  return ret;
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

static unsigned pseudo_random_delay (const ddsi_guid_t *x, const ddsi_guid_t *y, nn_mtime_t tnow)
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

static void respond_to_spdp (const struct q_globals *gv, const ddsi_guid_t *dest_proxypp_guid)
{
  struct entidx_enum_participant est;
  struct participant *pp;
  nn_mtime_t tnow = now_mt ();
  entidx_enum_participant_init (&est, gv->entity_index);
  while ((pp = entidx_enum_participant_next (&est)) != NULL)
  {
    /* delay_base has 32 bits, so delay_norm is approximately 1s max;
       delay_max <= 1s by gv.config checks */
    unsigned delay_base = pseudo_random_delay (&pp->e.guid, dest_proxypp_guid, tnow);
    unsigned delay_norm = delay_base >> 2;
    int64_t delay_max_ms = gv->config.spdp_response_delay_max / 1000000;
    int64_t delay = (int64_t) delay_norm * delay_max_ms / 1000;
    nn_mtime_t tsched = add_duration_to_mtime (tnow, delay);
    GVTRACE (" %"PRId64, delay);
    if (!pp->e.gv->config.unicast_response_to_spdp_messages)
      /* pp can't reach gc_delete_participant => can safely reschedule */
      (void) resched_xevent_if_earlier (pp->spdp_xevent, tsched);
    else
      qxev_spdp (gv->xevents, tsched, &pp->e.guid, dest_proxypp_guid);
  }
  entidx_enum_participant_fini (&est);
}

static int handle_SPDP_dead (const struct receiver_state *rst, ddsi_entityid_t pwr_entityid, nn_wctime_t timestamp, const nn_plist_t *datap, unsigned statusinfo)
{
  struct q_globals * const gv = rst->gv;
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

static void allowmulticast_aware_add_to_addrset (const struct q_globals *gv, uint32_t allow_multicast, struct addrset *as, const nn_locator_t *loc)
{
#if DDSI_INCLUDE_SSM
  if (ddsi_is_ssm_mcaddr (gv, loc))
  {
    if (!(allow_multicast & AMC_SSM))
      return;
  }
  else if (ddsi_is_mcaddr (gv, loc))
  {
    if (!(allow_multicast & AMC_ASM))
      return;
  }
#else
  if (ddsi_is_mcaddr (gv, loc) && !(allow_multicast & AMC_ASM))
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

static void make_participants_dependent_on_ddsi2 (struct q_globals *gv, const ddsi_guid_t *ddsi2guid, nn_wctime_t timestamp)
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

static int handle_SPDP_alive (const struct receiver_state *rst, seqno_t seq, nn_wctime_t timestamp, const nn_plist_t *datap)
{
  struct q_globals * const gv = rst->gv;
  const unsigned bes_sedp_announcer_mask =
    NN_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER |
    NN_DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER;
  struct addrset *as_meta, *as_default;
  unsigned builtin_endpoint_set;
  unsigned prismtech_builtin_endpoint_set;
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
  prismtech_builtin_endpoint_set = (datap->present & PP_PRISMTECH_BUILTIN_ENDPOINT_SET) ? datap->prismtech_builtin_endpoint_set : 0;
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
  if ((datap->present & PP_PRISMTECH_PARTICIPANT_VERSION_INFO) &&
      (datap->present & PP_PRISMTECH_BUILTIN_ENDPOINT_SET) &&
      !(datap->prismtech_participant_version_info.flags & NN_PRISMTECH_FL_PTBES_FIXED_0))
  {
    /* FIXED_0 (bug 0) indicates that this is an updated version that advertises
       CM readers/writers correctly (without it, we could make a reasonable guess,
       but it would cause problems with cases where we would be happy with only
       (say) CM participant. Have to do a backwards-compatible fix because it has
       already been released with the flags all aliased to bits 0 and 1 ... */
      GVLOGDISC (" (ptbes_fixed_0 %x)", prismtech_builtin_endpoint_set);
      if (prismtech_builtin_endpoint_set & NN_DISC_BUILTIN_ENDPOINT_CM_PARTICIPANT_READER)
        prismtech_builtin_endpoint_set |= NN_DISC_BUILTIN_ENDPOINT_CM_PUBLISHER_READER | NN_DISC_BUILTIN_ENDPOINT_CM_SUBSCRIBER_READER;
      if (prismtech_builtin_endpoint_set & NN_DISC_BUILTIN_ENDPOINT_CM_PARTICIPANT_WRITER)
        prismtech_builtin_endpoint_set |= NN_DISC_BUILTIN_ENDPOINT_CM_PUBLISHER_WRITER | NN_DISC_BUILTIN_ENDPOINT_CM_SUBSCRIBER_WRITER;
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
        lease_renew (lease, now_et ());
      ddsrt_mutex_lock (&proxypp->e.lock);
      if (proxypp->implicitly_created || seq > proxypp->seq)
      {
        interesting = 1;
        if (!(gv->logconfig.c.mask & DDS_LC_TRACE))
          GVLOGDISC ("SPDP ST0 "PGUIDFMT, PGUID (datap->participant_guid));
        GVLOGDISC (proxypp->implicitly_created ? " (NEW was-implicitly-created)" : " (update)");
        proxypp->implicitly_created = 0;
        update_proxy_participant_plist_locked (proxypp, seq, datap, UPD_PROXYPP_SPDP, timestamp);
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

  GVLOGDISC ("SPDP ST0 "PGUIDFMT" bes %x ptbes %x NEW", PGUID (datap->participant_guid), builtin_endpoint_set, prismtech_builtin_endpoint_set);

  if (datap->present & PP_PARTICIPANT_LEASE_DURATION)
  {
    lease_duration = datap->participant_lease_duration;
  }
  else
  {
    GVLOGDISC (" (PARTICIPANT_LEASE_DURATION defaulting to 100s)");
    lease_duration = 100 * T_SECOND;
  }

  if (datap->present & PP_PRISMTECH_PARTICIPANT_VERSION_INFO) {
    if (datap->prismtech_participant_version_info.flags & NN_PRISMTECH_FL_KERNEL_SEQUENCE_NUMBER)
      custom_flags |= CF_INC_KERNEL_SEQUENCE_NUMBERS;

    if ((datap->prismtech_participant_version_info.flags & NN_PRISMTECH_FL_DDSI2_PARTICIPANT_FLAG) &&
        (datap->prismtech_participant_version_info.flags & NN_PRISMTECH_FL_PARTICIPANT_IS_DDSI2))
      custom_flags |= CF_PARTICIPANT_IS_DDSI2;

    GVLOGDISC (" (0x%08x-0x%08x-0x%08x-0x%08x-0x%08x %s)",
               datap->prismtech_participant_version_info.version,
               datap->prismtech_participant_version_info.flags,
               datap->prismtech_participant_version_info.unused[0],
               datap->prismtech_participant_version_info.unused[1],
               datap->prismtech_participant_version_info.unused[2],
               datap->prismtech_participant_version_info.internals);
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
    lease_duration = T_NEVER;
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
      lease_duration = T_NEVER;
      GVLOGDISC (" (depends on "PGUIDFMT")", PGUID (privileged_pp_guid));
    }
  }
  else
  {
    memset (&privileged_pp_guid.prefix, 0, sizeof (privileged_pp_guid.prefix));
  }

  /* Choose locators */
  {
    nn_locator_t loc;
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
  nn_log_xqos (DDS_LC_DISCOVERY, &gv->logconfig, &datap->qos);
  GVLOGDISC ("}\n");

  maybe_add_pp_as_meta_to_as_disc (gv, as_meta);

  new_proxy_participant
  (
    gv,
    &datap->participant_guid,
    builtin_endpoint_set,
    prismtech_builtin_endpoint_set,
    &privileged_pp_guid,
    as_default,
    as_meta,
    datap,
    lease_duration,
    rst->vendor,
    custom_flags,
    timestamp,
    seq
  );

  /* Force transmission of SPDP messages - we're not very careful
     in avoiding the processing of SPDP packets addressed to others
     so filter here */
  {
    int have_dst =
      (rst->dst_guid_prefix.u[0] != 0 || rst->dst_guid_prefix.u[1] != 0 || rst->dst_guid_prefix.u[2] != 0);
    if (!have_dst)
    {
      GVLOGDISC ("broadcasted SPDP packet -> answering");
      respond_to_spdp (gv, &datap->participant_guid);
    }
    else
    {
      GVLOGDISC ("directed SPDP packet -> not responding\n");
    }
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

static void handle_SPDP (const struct receiver_state *rst, ddsi_entityid_t pwr_entityid, seqno_t seq, nn_wctime_t timestamp, unsigned statusinfo, const void *vdata, uint32_t len)
{
  struct q_globals * const gv = rst->gv;
  const struct CDRHeader *data = vdata; /* built-ins not deserialized (yet) */
  if (data == NULL)
  {
    RSTTRACE ("SPDP ST%x no payload?\n", statusinfo);
    return;
  }
  else
  {
    nn_plist_t decoded_data;
    nn_plist_src_t src;
    int interesting = 0;
    dds_return_t plist_ret;
    src.protocol_version = rst->protocol_version;
    src.vendorid = rst->vendor;
    src.encoding = data->identifier;
    src.buf = (unsigned char *) data + 4;
    src.bufsz = len - 4;
    src.strict = NN_STRICT_P (gv->config);
    src.factory = gv->m_factory;
    src.logconfig = &gv->logconfig;
    if ((plist_ret = nn_plist_init_frommsg (&decoded_data, NULL, ~(uint64_t)0, ~(uint64_t)0, &src)) < 0)
    {
      if (plist_ret != DDS_RETCODE_UNSUPPORTED)
        GVWARNING ("SPDP (vendor %u.%u): invalid qos/parameters\n", src.vendorid.id[0], src.vendorid.id[1]);
      return;
    }

    switch (statusinfo & (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER))
    {
      case 0:
        interesting = handle_SPDP_alive (rst, seq, timestamp, &decoded_data);
        break;

      case NN_STATUSINFO_DISPOSE:
      case NN_STATUSINFO_UNREGISTER:
      case (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER):
        interesting = handle_SPDP_dead (rst, pwr_entityid, timestamp, &decoded_data, statusinfo);
        break;
    }

    nn_plist_fini (&decoded_data);
    GVLOG (interesting ? DDS_LC_DISCOVERY : DDS_LC_TRACE, "\n");
  }
}

struct add_locator_to_ps_arg {
  struct q_globals *gv;
  nn_plist_t *ps;
};

static void add_locator_to_ps (const nn_locator_t *loc, void *varg)
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

static int sedp_write_endpoint
(
   struct writer *wr, int alive, const ddsi_guid_t *epguid,
   const struct entity_common *common, const struct endpoint_common *epcommon,
   const dds_qos_t *xqos, struct addrset *as, nn_security_info_t *security)
{
  struct q_globals * const gv = wr->e.gv;
  const dds_qos_t *defqos = is_writer_entityid (epguid->entityid) ? &gv->default_xqos_wr : &gv->default_xqos_rd;
  struct nn_xmsg *mpayload;
  uint64_t qosdiff;
  nn_plist_t ps;
  int ret;

  nn_plist_init_empty (&ps);
  ps.present |= PP_ENDPOINT_GUID;
  ps.endpoint_guid = *epguid;

  if (common && *common->name != 0)
  {
    ps.present |= PP_ENTITY_NAME;
    ps.aliased |= PP_ENTITY_NAME;
    ps.entity_name = common->name;
  }

#ifdef DDSI_INCLUDE_SECURITY
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
    assert (epcommon != NULL);
    ps.present |= PP_PROTOCOL_VERSION | PP_VENDORID;
    ps.protocol_version.major = RTPS_MAJOR;
    ps.protocol_version.minor = RTPS_MINOR;
    ps.vendorid = NN_VENDORID_ECLIPSE;

    if (epcommon->group_guid.entityid.u != 0)
    {
      ps.present |= PP_GROUP_GUID;
      ps.group_guid = epcommon->group_guid;
    }

#ifdef DDSI_INCLUDE_SSM
    /* A bit of a hack -- the easy alternative would be to make it yet
     another parameter.  We only set "reader favours SSM" if we
     really do: no point in telling the world that everything is at
     the default. */
    if (!is_writer_entityid (epguid->entityid))
    {
      const struct reader *rd = entidx_lookup_reader_guid (gv->entity_index, epguid);
      assert (rd);
      if (rd->favours_ssm)
      {
        ps.present |= PP_READER_FAVOURS_SSM;
        ps.reader_favours_ssm.state = 1u;
      }
    }
#endif

    qosdiff = nn_xqos_delta (xqos, defqos, ~(uint64_t)0);
    if (gv->config.explicitly_publish_qos_set_to_default)
      qosdiff |= ~QP_UNRECOGNIZED_INCOMPATIBLE_MASK;

    if (as)
    {
      struct add_locator_to_ps_arg arg;
      arg.gv = gv;
      arg.ps = &ps;
      addrset_forall (as, add_locator_to_ps, &arg);
    }
  }

  /* The message is only a temporary thing, used only for encoding
     the QoS and other settings. So the header fields aren't really
     important, except that they need to be set to reasonable things
     or it'll crash */
  mpayload = nn_xmsg_new (gv->xmsgpool, &wr->e.guid, NULL, 0, NN_XMSG_KIND_DATA);
  nn_plist_addtomsg (mpayload, &ps, ~(uint64_t)0, ~(uint64_t)0);
  if (xqos) nn_xqos_addtomsg (mpayload, xqos, qosdiff);
  nn_xmsg_addpar_sentinel (mpayload);
  nn_plist_fini (&ps);

  GVLOGDISC ("sedp: write for "PGUIDFMT" via "PGUIDFMT"\n", PGUID (*epguid), PGUID (wr->e.guid));
  ret = write_mpayload (wr, alive, PID_ENDPOINT_GUID, mpayload);
  nn_xmsg_free (mpayload);
  return ret;
}

static struct writer *get_sedp_writer (const struct participant *pp, unsigned entityid)
{
  struct writer *sedp_wr = get_builtin_writer (pp, entityid);
  if (sedp_wr == NULL)
    DDS_FATAL ("sedp_write_writer: no SEDP builtin writer %x for "PGUIDFMT"\n", entityid, PGUID (pp->e.guid));
  return sedp_wr;
}

int sedp_write_writer (struct writer *wr)
{
  if ((!is_builtin_entityid(wr->e.guid.entityid, NN_VENDORID_ECLIPSE)) && (!wr->e.onlylocal))
  {
    unsigned entityid = determine_publication_writer(wr);
    struct writer *sedp_wr = get_sedp_writer (wr->c.pp, entityid);
    nn_security_info_t *security = NULL;
#ifdef DDSI_INCLUDE_SSM
    struct addrset *as = wr->ssm_as;
#else
    struct addrset *as = NULL;
#endif
#ifdef DDSI_INCLUDE_SECURITY
    nn_security_info_t tmp;
    if (q_omg_get_writer_security_info(wr, &tmp))
    {
      security = &tmp;
    }
#endif
    return sedp_write_endpoint (sedp_wr, 1, &wr->e.guid, &wr->e, &wr->c, wr->xqos, as, security);
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
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
    struct addrset *as = rd->as;
#else
    struct addrset *as = NULL;
#endif
#ifdef DDSI_INCLUDE_SECURITY
    nn_security_info_t tmp;
    if (q_omg_get_reader_security_info(rd, &tmp))
    {
      security = &tmp;
    }
#endif
    return sedp_write_endpoint (sedp_wr, 1, &rd->e.guid, &rd->e, &rd->c, rd->xqos, as, security);
  }
  return 0;
}

int sedp_dispose_unregister_writer (struct writer *wr)
{
  if ((!is_builtin_entityid(wr->e.guid.entityid, NN_VENDORID_ECLIPSE)) && (!wr->e.onlylocal))
  {
    unsigned entityid = determine_publication_writer(wr);
    struct writer *sedp_wr = get_sedp_writer (wr->c.pp, entityid);
    return sedp_write_endpoint (sedp_wr, 0, &wr->e.guid, NULL, NULL, NULL, NULL, NULL);
  }
  return 0;
}

int sedp_dispose_unregister_reader (struct reader *rd)
{
  if ((!is_builtin_entityid(rd->e.guid.entityid, NN_VENDORID_ECLIPSE)) && (!rd->e.onlylocal))
  {
    unsigned entityid = determine_subscription_writer(rd);
    struct writer *sedp_wr = get_sedp_writer (rd->c.pp, entityid);
    return sedp_write_endpoint (sedp_wr, 0, &rd->e.guid, NULL, NULL, NULL, NULL, NULL);
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

static struct proxy_participant *implicitly_create_proxypp (struct q_globals *gv, const ddsi_guid_t *ppguid, nn_plist_t *datap /* note: potentially modifies datap */, const ddsi_guid_prefix_t *src_guid_prefix, nn_vendorid_t vendorid, nn_wctime_t timestamp, seqno_t seq)
{
  ddsi_guid_t privguid;
  nn_plist_t pp_plist;

  if (memcmp (&ppguid->prefix, src_guid_prefix, sizeof (ppguid->prefix)) == 0)
    /* if the writer is owned by the participant itself, we're not interested */
    return NULL;

  privguid.prefix = *src_guid_prefix;
  privguid.entityid = to_entityid (NN_ENTITYID_PARTICIPANT);
  nn_plist_init_empty(&pp_plist);

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
    new_proxy_participant(gv, ppguid, 0, 0, &privguid, new_addrset(), new_addrset(), &pp_plist, T_NEVER, actual_vendorid, CF_IMPLICITLY_CREATED_PROXYPP, timestamp, seq);
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
      nn_plist_t tmp_plist;
      GVTRACE (" from-ddsi2 "PGUIDFMT, PGUID (privguid));
      nn_plist_init_empty (&pp_plist);

      ddsrt_mutex_lock (&privpp->e.lock);
      as_default = ref_addrset(privpp->as_default);
      as_meta = ref_addrset(privpp->as_meta);
      /* copy just what we need */
      tmp_plist = *privpp->plist;
      tmp_plist.present = PP_PARTICIPANT_GUID | PP_PRISMTECH_PARTICIPANT_VERSION_INFO;
      tmp_plist.participant_guid = *ppguid;
      nn_plist_mergein_missing (&pp_plist, &tmp_plist, ~(uint64_t)0, ~(uint64_t)0);
      ddsrt_mutex_unlock (&privpp->e.lock);

      pp_plist.prismtech_participant_version_info.flags &= ~NN_PRISMTECH_FL_PARTICIPANT_IS_DDSI2;
      new_proxy_participant (gv, ppguid, 0, 0, &privguid, as_default, as_meta, &pp_plist, T_NEVER, vendorid, CF_IMPLICITLY_CREATED_PROXYPP | CF_PROXYPP_NO_SPDP, timestamp, seq);
    }
  }

 err:
  nn_plist_fini (&pp_plist);
  return entidx_lookup_proxy_participant_guid (gv->entity_index, ppguid);
}

static void handle_SEDP_alive (const struct receiver_state *rst, seqno_t seq, nn_plist_t *datap /* note: potentially modifies datap */, const ddsi_guid_prefix_t *src_guid_prefix, nn_vendorid_t vendorid, nn_wctime_t timestamp)
{
#define E(msg, lbl) do { GVLOGDISC (msg); goto lbl; } while (0)
  struct q_globals * const gv = rst->gv;
  struct proxy_participant *pp;
  struct proxy_writer * pwr = NULL;
  struct proxy_reader * prd = NULL;
  ddsi_guid_t ppguid;
  dds_qos_t *xqos;
  int reliable;
  struct addrset *as;
  int is_writer;
#ifdef DDSI_INCLUDE_SSM
  int ssm;
#endif

  assert (datap);

  if (!(datap->present & PP_ENDPOINT_GUID))
    E (" no guid?\n", err);
  GVLOGDISC (" "PGUIDFMT, PGUID (datap->endpoint_guid));

  ppguid.prefix = datap->endpoint_guid.prefix;
  ppguid.entityid.u = NN_ENTITYID_PARTICIPANT;
  if (is_deleted_participant_guid (gv->deleted_participants, &ppguid, DPG_REMOTE))
    E (" local dead pp?\n", err);

  if (entidx_lookup_participant_guid (gv->entity_index, &ppguid) != NULL)
    E (" local pp?\n", err);

  if (is_builtin_entityid (datap->endpoint_guid.entityid, vendorid))
    E (" built-in\n", err);
  if (!(datap->qos.present & QP_TOPIC_NAME))
    E (" no topic?\n", err);
  if (!(datap->qos.present & QP_TYPE_NAME))
    E (" no typename?\n", err);

  if ((pp = entidx_lookup_proxy_participant_guid (gv->entity_index, &ppguid)) == NULL)
  {
    GVLOGDISC (" unknown-proxypp");
    if ((pp = implicitly_create_proxypp (gv, &ppguid, datap, src_guid_prefix, vendorid, timestamp, 0)) == NULL)
      E ("?\n", err);
    /* Repeat regular SEDP trace for convenience */
    GVLOGDISC ("SEDP ST0 "PGUIDFMT" (cont)", PGUID (datap->endpoint_guid));
  }

  xqos = &datap->qos;
  is_writer = is_writer_entityid (datap->endpoint_guid.entityid);
  if (!is_writer)
    nn_xqos_mergein_missing (xqos, &gv->default_xqos_rd, ~(uint64_t)0);
  else if (vendor_is_eclipse_or_prismtech(vendorid))
    nn_xqos_mergein_missing (xqos, &gv->default_xqos_wr, ~(uint64_t)0);
  else
    nn_xqos_mergein_missing (xqos, &gv->default_xqos_wr_nad, ~(uint64_t)0);

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
             is_writer ? "writer" : "reader",
             ((!(xqos->present & QP_PARTITION) || xqos->partition.n == 0 || *xqos->partition.strs[0] == '\0')
              ? "(default)" : xqos->partition.strs[0]),
             ((xqos->present & QP_PARTITION) && xqos->partition.n > 1) ? "+" : "",
             xqos->topic_name, xqos->type_name);

  if (! is_writer && (datap->present & PP_EXPECTS_INLINE_QOS) && datap->expects_inline_qos)
  {
    E ("******* AARGH - it expects inline QoS ********\n", err);
  }

  if (is_writer)
  {
    pwr = entidx_lookup_proxy_writer_guid (gv->entity_index, &datap->endpoint_guid);
  }
  else
  {
    prd = entidx_lookup_proxy_reader_guid (gv->entity_index, &datap->endpoint_guid);
  }
  if (pwr || prd)
  {
    /* Re-bind the proxy participant to the discovery service - and do this if it is currently
       bound to another DS instance, because that other DS instance may have already failed and
       with a new one taking over, without our noticing it. */
    GVLOGDISC (" known%s", vendor_is_cloud (vendorid) ? "-DS" : "");
    if (vendor_is_cloud (vendorid) && pp->implicitly_created && memcmp(&pp->privileged_pp_guid.prefix, src_guid_prefix, sizeof(pp->privileged_pp_guid.prefix)) != 0)
    {
      GVLOGDISC (" "PGUIDFMT" attach-to-DS "PGUIDFMT, PGUID(pp->e.guid), PGUIDPREFIX(*src_guid_prefix), pp->privileged_pp_guid.entityid.u);
      ddsrt_mutex_lock (&pp->e.lock);
      pp->privileged_pp_guid.prefix = *src_guid_prefix;
      lease_set_expiry(pp->lease, NN_ETIME_NEVER);
      ddsrt_mutex_unlock (&pp->e.lock);
    }
    GVLOGDISC ("\n");
  }
  else
  {
    GVLOGDISC (" NEW");
  }

  {
    nn_locator_t loc;
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
      copy_addrset_into_addrset_uc (gv, as, pp->as_default);
    }
    if ((datap->present & PP_MULTICAST_LOCATOR) && get_locator (gv, &loc, &datap->multicast_locators, 0))
      allowmulticast_aware_add_to_addrset (gv, gv->config.allowMulticast, as, &loc);
    else
      copy_addrset_into_addrset_mc (gv, as, pp->as_default);
  }
  if (addrset_empty (as))
  {
    unref_addrset (as);
    E (" no address", err);
  }

  nn_log_addrset(gv, DDS_LC_DISCOVERY, " (as", as);
#ifdef DDSI_INCLUDE_SSM
  ssm = 0;
  if (is_writer)
    ssm = addrset_contains_ssm (gv, as);
  else if (datap->present & PP_READER_FAVOURS_SSM)
    ssm = (datap->reader_favours_ssm.state != 0);
  GVLOGDISC (" ssm=%u", ssm);
#endif
  GVLOGDISC (") QOS={");
  nn_log_xqos (DDS_LC_DISCOVERY, &gv->logconfig, xqos);
  GVLOGDISC ("}\n");

  if ((datap->endpoint_guid.entityid.u & NN_ENTITYID_SOURCE_MASK) == NN_ENTITYID_SOURCE_VENDOR && !vendor_is_eclipse_or_prismtech (vendorid))
  {
    GVLOGDISC ("ignoring vendor-specific endpoint "PGUIDFMT"\n", PGUID (datap->endpoint_guid));
  }
  else
  {
    if (is_writer)
    {
      if (pwr)
      {
        update_proxy_writer (pwr, seq, as, xqos, timestamp);
      }
      else
      {
        /* not supposed to get here for built-in ones, so can determine the channel based on the transport priority */
        assert (!is_builtin_entityid (datap->endpoint_guid.entityid, vendorid));
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
        {
          struct config_channel_listelem *channel = find_channel (&gv->config, xqos->transport_priority);
          new_proxy_writer (&ppguid, &datap->endpoint_guid, as, datap, channel->dqueue, channel->evq ? channel->evq : gv->xevents, timestamp);
        }
#else
        new_proxy_writer (gv, &ppguid, &datap->endpoint_guid, as, datap, gv->user_dqueue, gv->xevents, timestamp, seq);
#endif
      }
    }
    else
    {
      if (prd)
      {
        update_proxy_reader (prd, seq, as, xqos, timestamp);
      }
      else
      {
#ifdef DDSI_INCLUDE_SSM
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

static void handle_SEDP_dead (const struct receiver_state *rst, nn_plist_t *datap, nn_wctime_t timestamp)
{
  struct q_globals * const gv = rst->gv;
  int res;
  if (!(datap->present & PP_ENDPOINT_GUID))
  {
    GVLOGDISC (" no guid?\n");
    return;
  }
  GVLOGDISC (" "PGUIDFMT, PGUID (datap->endpoint_guid));
  if (is_writer_entityid (datap->endpoint_guid.entityid))
    res = delete_proxy_writer (gv, &datap->endpoint_guid, timestamp, 0);
  else
    res = delete_proxy_reader (gv, &datap->endpoint_guid, timestamp, 0);
  GVLOGDISC (" %s\n", (res < 0) ? " unknown" : " delete");
}

static void handle_SEDP (const struct receiver_state *rst, seqno_t seq, nn_wctime_t timestamp, unsigned statusinfo, const void *vdata, uint32_t len)
{
  struct q_globals * const gv = rst->gv;
  const struct CDRHeader *data = vdata; /* built-ins not deserialized (yet) */
  GVLOGDISC ("SEDP ST%x", statusinfo);
  if (data == NULL)
  {
    GVLOGDISC (" no payload?\n");
    return;
  }
  else
  {
    nn_plist_t decoded_data;
    nn_plist_src_t src;
    dds_return_t plist_ret;
    src.protocol_version = rst->protocol_version;
    src.vendorid = rst->vendor;
    src.encoding = data->identifier;
    src.buf = (unsigned char *) data + 4;
    src.bufsz = len - 4;
    src.strict = NN_STRICT_P (gv->config);
    src.factory = gv->m_factory;
    src.logconfig = &gv->logconfig;
    if ((plist_ret = nn_plist_init_frommsg (&decoded_data, NULL, ~(uint64_t)0, ~(uint64_t)0, &src)) < 0)
    {
      if (plist_ret != DDS_RETCODE_UNSUPPORTED)
        GVWARNING ("SEDP (vendor %u.%u): invalid qos/parameters\n", src.vendorid.id[0], src.vendorid.id[1]);
      return;
    }

    switch (statusinfo & (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER))
    {
      case 0:
        handle_SEDP_alive (rst, seq, &decoded_data, &rst->src_guid_prefix, rst->vendor, timestamp);
        break;

      case NN_STATUSINFO_DISPOSE:
      case NN_STATUSINFO_UNREGISTER:
      case (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER):
        handle_SEDP_dead (rst, &decoded_data, timestamp);
        break;
    }

    nn_plist_fini (&decoded_data);
  }
}

/******************************************************************************
 ***
 *** Topics
 ***
 *****************************************************************************/

int sedp_write_topic (struct participant *pp, const struct nn_plist *datap)
{
  struct writer *sedp_wr;
  struct nn_xmsg *mpayload;
  uint64_t delta;
  int ret;

  assert (datap->qos.present & QP_TOPIC_NAME);

  if (pp->e.onlylocal) {
      /* This topic is only locally available. */
      return 0;
  }

  sedp_wr = get_sedp_writer (pp, NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER);

  mpayload = nn_xmsg_new (sedp_wr->e.gv->xmsgpool, &sedp_wr->e.guid, NULL, 0, NN_XMSG_KIND_DATA);
  delta = nn_xqos_delta (&datap->qos, &sedp_wr->e.gv->default_xqos_tp, ~(uint64_t)0);
  if (sedp_wr->e.gv->config.explicitly_publish_qos_set_to_default)
    delta |= ~QP_UNRECOGNIZED_INCOMPATIBLE_MASK;
  nn_plist_addtomsg (mpayload, datap, ~(uint64_t)0, delta);
  nn_xmsg_addpar_sentinel (mpayload);

  ETRACE (pp, "sedp: write topic %s via "PGUIDFMT"\n", datap->qos.topic_name, PGUID (sedp_wr->e.guid));
  ret = write_mpayload (sedp_wr, 1, PID_TOPIC_NAME, mpayload);
  nn_xmsg_free (mpayload);
  return ret;
}


/******************************************************************************
 ***
 *** PrismTech CM data
 ***
 *****************************************************************************/

int sedp_write_cm_participant (struct participant *pp, int alive)
{
  struct writer * sedp_wr;
  struct nn_xmsg *mpayload;
  nn_plist_t ps;
  int ret;

  if (pp->e.onlylocal) {
      /* This topic is only locally available. */
      return 0;
  }

  sedp_wr = get_sedp_writer (pp, NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_WRITER);

  /* The message is only a temporary thing, used only for encoding
   the QoS and other settings. So the header fields aren't really
   important, except that they need to be set to reasonable things
   or it'll crash */
  mpayload = nn_xmsg_new (sedp_wr->e.gv->xmsgpool, &sedp_wr->e.guid, NULL, 0, NN_XMSG_KIND_DATA);
  nn_plist_init_empty (&ps);
  ps.present = PP_PARTICIPANT_GUID;
  ps.participant_guid = pp->e.guid;
  nn_plist_addtomsg (mpayload, &ps, ~(uint64_t)0, ~(uint64_t)0);
  nn_plist_fini (&ps);
  if (alive)
  {
    nn_plist_addtomsg (mpayload, pp->plist,
                       PP_PRISMTECH_NODE_NAME | PP_PRISMTECH_EXEC_NAME | PP_PRISMTECH_PROCESS_ID |
                       PP_ENTITY_NAME,
                       QP_PRISMTECH_ENTITY_FACTORY);
  }
  nn_xmsg_addpar_sentinel (mpayload);

  ETRACE (pp, "sedp: write CMParticipant ST%x for "PGUIDFMT" via "PGUIDFMT"\n",
          alive ? 0 : NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER, PGUID (pp->e.guid), PGUID (sedp_wr->e.guid));
  ret = write_mpayload (sedp_wr, alive, PID_PARTICIPANT_GUID, mpayload);
  nn_xmsg_free (mpayload);
  return ret;
}

static void handle_SEDP_CM (const struct receiver_state *rst, ddsi_entityid_t wr_entity_id, nn_wctime_t timestamp, uint32_t statusinfo, const void *vdata, uint32_t len)
{
  struct q_globals * const gv = rst->gv;
  const struct CDRHeader *data = vdata; /* built-ins not deserialized (yet) */
  GVLOGDISC ("SEDP_CM ST%x", statusinfo);
  assert (wr_entity_id.u == NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_WRITER);
  (void) wr_entity_id;
  if (data == NULL)
  {
    GVLOGDISC (" no payload?\n");
    return;
  }
  else
  {
    nn_plist_t decoded_data;
    nn_plist_src_t src;
    dds_return_t plist_ret;
    src.protocol_version = rst->protocol_version;
    src.vendorid = rst->vendor;
    src.encoding = data->identifier;
    src.buf = (unsigned char *) data + 4;
    src.bufsz = len - 4;
    src.strict = NN_STRICT_P (gv->config);
    src.factory = gv->m_factory;
    src.logconfig = &gv->logconfig;
    if ((plist_ret = nn_plist_init_frommsg (&decoded_data, NULL, ~(uint64_t)0, ~(uint64_t)0, &src)) < 0)
    {
      if (plist_ret != DDS_RETCODE_UNSUPPORTED)
        GVWARNING ("SEDP_CM (vendor %u.%u): invalid qos/parameters\n", src.vendorid.id[0], src.vendorid.id[1]);
      return;
    }

    /* ignore: dispose/unregister is tied to deleting the participant, which will take care of the dispose/unregister for us */;
    if ((statusinfo & (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER)) == 0)
    {
      struct proxy_participant *proxypp;
      if (!(decoded_data.present & PP_PARTICIPANT_GUID))
        GVWARNING ("SEDP_CM (vendor %u.%u): missing participant GUID\n", src.vendorid.id[0], src.vendorid.id[1]);
      else
      {
        if ((proxypp = entidx_lookup_proxy_participant_guid (gv->entity_index, &decoded_data.participant_guid)) == NULL)
          proxypp = implicitly_create_proxypp (gv, &decoded_data.participant_guid, &decoded_data, &rst->src_guid_prefix, rst->vendor, timestamp, 0);
        if (proxypp != NULL)
          update_proxy_participant_plist (proxypp, 0, &decoded_data, UPD_PROXYPP_CM, timestamp);
      }
    }

    nn_plist_fini (&decoded_data);
  }
  GVLOGDISC ("\n");
}

/******************************************************************************
 *****************************************************************************/

/* FIXME: defragment is a copy of the one in q_receive.c, but the deserialised should be enhanced to handle fragmented data (and arguably the processing here should be built on proper data readers) */
static int defragment (unsigned char **datap, const struct nn_rdata *fragchain, uint32_t sz)
{
  if (fragchain->nextfrag == NULL)
  {
    *datap = NN_RMSG_PAYLOADOFF (fragchain->rmsg, NN_RDATA_PAYLOAD_OFF (fragchain));
    return 0;
  }
  else
  {
    unsigned char *buf;
    uint32_t off = 0;
    buf = ddsrt_malloc (sz);
    while (fragchain)
    {
      assert (fragchain->min <= off);
      assert (fragchain->maxp1 <= sz);
      if (fragchain->maxp1 > off)
      {
        /* only copy if this fragment adds data */
        const unsigned char *payload = NN_RMSG_PAYLOADOFF (fragchain->rmsg, NN_RDATA_PAYLOAD_OFF (fragchain));
        memcpy (buf + off, payload + off - fragchain->min, fragchain->maxp1 - off);
        off = fragchain->maxp1;
      }
      fragchain = fragchain->nextfrag;
    }
    *datap = buf;
    return 1;
  }
}

int builtins_dqueue_handler (const struct nn_rsample_info *sampleinfo, const struct nn_rdata *fragchain, UNUSED_ARG (const ddsi_guid_t *rdguid), UNUSED_ARG (void *qarg))
{
  struct q_globals * const gv = sampleinfo->rst->gv;
  struct proxy_writer *pwr;
  struct {
    struct CDRHeader cdr;
    nn_parameter_t p_endpoint_guid;
    char kh[16];
    nn_parameter_t p_sentinel;
  } keyhash_payload;
  unsigned statusinfo;
  int need_keyhash;
  ddsi_guid_t srcguid;
  Data_DataFrag_common_t *msg;
  unsigned char data_smhdr_flags;
  nn_plist_t qos;
  unsigned char *datap;
  int needs_free;
  uint32_t datasz = sampleinfo->size;
  nn_wctime_t timestamp;

  needs_free = defragment (&datap, fragchain, sampleinfo->size);

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
  need_keyhash = (datasz == 0 || (data_smhdr_flags & (DATA_FLAG_KEYFLAG | DATA_FLAG_DATAFLAG)) == 0);
  if (!(sampleinfo->complex_qos || need_keyhash))
  {
    nn_plist_init_empty (&qos);
    statusinfo = sampleinfo->statusinfo;
  }
  else
  {
    nn_plist_src_t src;
    size_t qos_offset = NN_RDATA_SUBMSG_OFF (fragchain) + offsetof (Data_DataFrag_common_t, octetsToInlineQos) + sizeof (msg->octetsToInlineQos) + msg->octetsToInlineQos;
    dds_return_t plist_ret;
    src.protocol_version = sampleinfo->rst->protocol_version;
    src.vendorid = sampleinfo->rst->vendor;
    src.encoding = (msg->smhdr.flags & SMFLAG_ENDIANNESS) ? PL_CDR_LE : PL_CDR_BE;
    src.buf = NN_RMSG_PAYLOADOFF (fragchain->rmsg, qos_offset);
    src.bufsz = NN_RDATA_PAYLOAD_OFF (fragchain) - qos_offset;
    src.strict = NN_STRICT_P (gv->config);
    src.factory = gv->m_factory;
    src.logconfig = &gv->logconfig;
    if ((plist_ret = nn_plist_init_frommsg (&qos, NULL, PP_STATUSINFO | PP_KEYHASH, 0, &src)) < 0)
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

  /* Built-ins still do their own deserialization (SPDP <=> pwr ==
     NULL)). */
  if (statusinfo == 0)
  {
    if (datasz == 0 || !(data_smhdr_flags & DATA_FLAG_DATAFLAG))
    {
      GVWARNING ("data(builtin, vendor %u.%u): "PGUIDFMT" #%"PRId64": built-in data but no payload\n",
                 sampleinfo->rst->vendor.id[0], sampleinfo->rst->vendor.id[1],
                 PGUID (srcguid), sampleinfo->seq);
      goto done_upd_deliv;
    }
  }
  else if (datasz)
  {
    /* Raw data must be full payload for write, just keys for
       dispose and unregister. First has been checked; the second
       hasn't been checked fully yet. */
    if (!(data_smhdr_flags & DATA_FLAG_KEYFLAG))
    {
      GVWARNING ("data(builtin, vendor %u.%u): "PGUIDFMT" #%"PRId64": dispose/unregister of built-in data but payload not just key\n",
                 sampleinfo->rst->vendor.id[0], sampleinfo->rst->vendor.id[1],
                 PGUID (srcguid), sampleinfo->seq);
      goto done_upd_deliv;
    }
  }
  else if ((qos.present & PP_KEYHASH) && !NN_STRICT_P(gv->config))
  {
    /* For SPDP/SEDP, fake a parameter list with just a keyhash.  For
       PMD, just use the keyhash directly.  Too hard to fix everything
       at the same time ... */
    if (srcguid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER)
    {
      datap = qos.keyhash.value;
      datasz = sizeof (qos.keyhash);
    }
    else
    {
      nn_parameterid_t pid;
      keyhash_payload.cdr.identifier = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN ? PL_CDR_LE : PL_CDR_BE);
      keyhash_payload.cdr.options = 0;
      switch (srcguid.entityid.u)
      {
        case NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER:
        case NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_WRITER:
        case NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER:
          pid = PID_PARTICIPANT_GUID;
          break;
        case NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_WRITER:
        case NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_WRITER:
          pid = PID_GROUP_GUID;
          break;
        case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER:
        case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER:
        case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER:
        case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER:
          pid = PID_ENDPOINT_GUID;
          break;
        case NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER:
        case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER:
          /* placeholders */
          pid = PID_ENDPOINT_GUID;
          break;
        default:
          GVLOGDISC ("data(builtin, vendor %u.%u): "PGUIDFMT" #%"PRId64": mapping keyhash to ENDPOINT_GUID",
                     sampleinfo->rst->vendor.id[0], sampleinfo->rst->vendor.id[1],
                     PGUID (srcguid), sampleinfo->seq);
          pid = PID_ENDPOINT_GUID;
          break;
      }
      keyhash_payload.p_endpoint_guid.parameterid = pid;
      keyhash_payload.p_endpoint_guid.length = sizeof (nn_keyhash_t);
      memcpy (keyhash_payload.kh, &qos.keyhash, sizeof (qos.keyhash));
      keyhash_payload.p_sentinel.parameterid = PID_SENTINEL;
      keyhash_payload.p_sentinel.length = 0;
      datap = (unsigned char *) &keyhash_payload;
      datasz = sizeof (keyhash_payload);
    }
  }
  else
  {
    GVWARNING ("data(builtin, vendor %u.%u): "PGUIDFMT" #%"PRId64": dispose/unregister with no content\n",
               sampleinfo->rst->vendor.id[0], sampleinfo->rst->vendor.id[1],
               PGUID (srcguid), sampleinfo->seq);
    goto done_upd_deliv;
  }

  if (sampleinfo->timestamp.v != NN_WCTIME_INVALID.v)
    timestamp = sampleinfo->timestamp;
  else
    timestamp = now ();
  switch (srcguid.entityid.u)
  {
    case NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER:
    case NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER:
      handle_SPDP (sampleinfo->rst, srcguid.entityid, sampleinfo->seq, timestamp, statusinfo, datap, datasz);
      break;
    case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER:
    case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER:
    case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER:
    case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER:
      handle_SEDP (sampleinfo->rst, sampleinfo->seq, timestamp, statusinfo, datap, datasz);
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER:
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER:
      handle_pmd_message (sampleinfo->rst, timestamp, statusinfo, datap, datasz);
      break;
    case NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_WRITER:
      handle_SEDP_CM (sampleinfo->rst, srcguid.entityid, timestamp, statusinfo, datap, datasz);
      break;
#ifdef DDSI_INCLUDE_SECURITY
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER:
      /* TODO: Handshake */
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER:
      /* TODO: Key exchange */
      break;
#endif
    default:
      GVLOGDISC ("data(builtin, vendor %u.%u): "PGUIDFMT" #%"PRId64": not handled\n",
                 sampleinfo->rst->vendor.id[0], sampleinfo->rst->vendor.id[1],
                 PGUID (srcguid), sampleinfo->seq);
      break;
  }

 done_upd_deliv:
  if (needs_free)
    ddsrt_free (datap);
  if (pwr)
  {
    /* No proxy writer for SPDP */
    ddsrt_atomic_st32 (&pwr->next_deliv_seq_lowword, (uint32_t) (sampleinfo->seq + 1));
  }
  return 0;
}
