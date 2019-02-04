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

#include "os/os.h"

#include "util/ut_avl.h"
#include "ddsi/q_protocol.h"
#include "ddsi/q_rtps.h"
#include "ddsi/q_misc.h"
#include "ddsi/q_config.h"
#include "ddsi/q_log.h"
#include "ddsi/q_plist.h"
#include "ddsi/q_unused.h"
#include "ddsi/q_xevent.h"
#include "ddsi/q_addrset.h"
#include "ddsi/q_ddsi_discovery.h"
#include "ddsi/q_radmin.h"
#include "ddsi/q_ephash.h"
#include "ddsi/q_entity.h"
#include "ddsi/q_globals.h"
#include "ddsi/q_xmsg.h"
#include "ddsi/q_bswap.h"
#include "ddsi/q_transmit.h"
#include "ddsi/q_lease.h"
#include "ddsi/q_error.h"
#include "ddsi/ddsi_serdata_default.h"
#include "ddsi/q_md5.h"
#include "ddsi/q_feature_check.h"

static int get_locator (nn_locator_t *loc, const nn_locators_t *locs, int uc_same_subnet)
{
  struct nn_locators_one *l;
  nn_locator_t first, samenet;
  int first_set = 0, samenet_set = 0;
  memset (&first, 0, sizeof (first));
  memset (&samenet, 0, sizeof (samenet));

  /* Special case UDPv4 MC address generators - there is a bit of an type mismatch between an address generator (i.e., a set of addresses) and an address ... Whoever uses them is supposed to know that that is what he wants, so we simply given them priority. */
  if (ddsi_factory_supports (gv.m_factory, NN_LOCATOR_KIND_UDPv4))
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

    if (! ddsi_factory_supports (gv.m_factory, l->loc.kind))
    {
      continue;
    }

    if (l->loc.kind == NN_LOCATOR_KIND_UDPv4 && gv.extmask.kind != NN_LOCATOR_KIND_INVALID)
    {
      /* If the examined locator is in the same subnet as our own
         external IP address, this locator will be translated into one
         in the same subnet as our own local ip and selected. */
      struct in_addr tmp4 = *((struct in_addr *) (l->loc.address + 12));
      const struct in_addr ownip = *((struct in_addr *) (gv.ownloc.address + 12));
      const struct in_addr extip = *((struct in_addr *) (gv.extloc.address + 12));
      const struct in_addr extmask = *((struct in_addr *) (gv.extmask.address + 12));

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

#if OS_SOCKET_HAS_IPV6
    if ((l->loc.kind == NN_LOCATOR_KIND_UDPv6) || (l->loc.kind == NN_LOCATOR_KIND_TCPv6))
    {
      /* We (cowardly) refuse to accept advertised link-local
         addresses unles we're in "link-local" mode ourselves.  Then
         we just hope for the best.  */
      const struct in6_addr *ip6 = (const struct in6_addr *) l->loc.address;
      if (!gv.ipv6_link_local && IN6_IS_ADDR_LINKLOCAL (ip6))
        continue;
    }
#endif

    if (!first_set)
    {
      first = l->loc;
      first_set = 1;
    }

    switch (ddsi_is_nearby_address(&l->loc, (size_t)gv.n_interfaces, gv.interfaces))
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

static void maybe_add_pp_as_meta_to_as_disc (const struct addrset *as_meta)
{
  if (addrset_empty_mc (as_meta))
  {
    nn_locator_t loc;
    if (addrset_any_uc (as_meta, &loc))
    {
      add_to_addrset (gv.as_disc, &loc);
    }
  }
}

static int write_mpayload (struct writer *wr, int alive, nn_parameterid_t keyparam, struct nn_xmsg *mpayload)
{
  struct ddsi_plist_sample plist_sample;
  struct ddsi_serdata *serdata;
  nn_xmsg_payload_to_plistsample (&plist_sample, keyparam, mpayload);
  serdata = ddsi_serdata_from_sample (gv.plist_topic, alive ? SDK_DATA : SDK_KEY, &plist_sample);
  serdata->statusinfo = alive ? 0 : NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER;
  serdata->timestamp = now ();
  return write_sample_nogc_notk (NULL, wr, serdata);
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

  DDS_TRACE("spdp_write(%x:%x:%x:%x)\n", PGUID (pp->e.guid));

  if ((wr = get_builtin_writer (pp, NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER)) == NULL)
  {
    DDS_TRACE("spdp_write(%x:%x:%x:%x) - builtin participant writer not found\n", PGUID (pp->e.guid));
    return 0;
  }

  /* First create a fake message for the payload: we can add plists to
     xmsgs easily, but not to serdata.  But it is rather easy to copy
     the payload of an xmsg over to a serdata ...  Expected size isn't
     terribly important, the msg will grow as needed, address space is
     essentially meaningless because we only use the message to
     construct the payload. */
  mpayload = nn_xmsg_new (gv.xmsgpool, &pp->e.guid.prefix, 0, NN_XMSG_KIND_DATA);

  nn_plist_init_empty (&ps);
  ps.present |= PP_PARTICIPANT_GUID | PP_BUILTIN_ENDPOINT_SET |
    PP_PROTOCOL_VERSION | PP_VENDORID | PP_PARTICIPANT_LEASE_DURATION;
  ps.participant_guid = pp->e.guid;
  ps.builtin_endpoint_set = pp->bes;
  ps.protocol_version.major = RTPS_MAJOR;
  ps.protocol_version.minor = RTPS_MINOR;
  ps.vendorid = NN_VENDORID_ECLIPSE;
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

  if (config.many_sockets_mode == MSM_MANY_UNICAST)
  {
    def_uni_loc_one.loc = pp->m_locator;
    meta_uni_loc_one.loc = pp->m_locator;
  }
  else
  {
    def_uni_loc_one.loc = gv.loc_default_uc;
    meta_uni_loc_one.loc = gv.loc_meta_uc;
  }

  if (config.publish_uc_locators)
  {
    ps.present |= PP_DEFAULT_UNICAST_LOCATOR | PP_METATRAFFIC_UNICAST_LOCATOR;
    ps.aliased |= PP_DEFAULT_UNICAST_LOCATOR | PP_METATRAFFIC_UNICAST_LOCATOR;
  }

  if (config.allowMulticast)
  {
    int include = 0;
#ifdef DDSI_INCLUDE_SSM
    /* Note that if the default multicast address is an SSM address,
       we will simply advertise it. The recipients better understand
       it means the writers will publish to address and the readers
       favour SSM. */
    if (ddsi_is_ssm_mcaddr (&gv.loc_default_mc))
      include = (config.allowMulticast & AMC_SSM) != 0;
    else
      include = (config.allowMulticast & AMC_ASM) != 0;
#else
    if (config.allowMulticast & AMC_ASM)
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
      def_multi_loc_one.loc = gv.loc_default_mc;
      meta_multi_loc_one.next = NULL;
      meta_multi_loc_one.loc = gv.loc_meta_mc;
    }
  }
  ps.participant_lease_duration = nn_to_ddsi_duration (pp->lease_duration);

  /* Add PrismTech specific version information */
  {
    ps.present |= PP_PRISMTECH_PARTICIPANT_VERSION_INFO;
    ps.prismtech_participant_version_info.version = 0;
    ps.prismtech_participant_version_info.flags =
      NN_PRISMTECH_FL_DDSI2_PARTICIPANT_FLAG |
      NN_PRISMTECH_FL_PTBES_FIXED_0 |
      NN_PRISMTECH_FL_SUPPORTS_STATUSINFOX;
    if (config.besmode == BESMODE_MINIMAL)
      ps.prismtech_participant_version_info.flags |= NN_PRISMTECH_FL_MINIMAL_BES_MODE;
    os_mutexLock (&gv.privileged_pp_lock);
    if (pp->is_ddsi2_pp)
      ps.prismtech_participant_version_info.flags |= NN_PRISMTECH_FL_PARTICIPANT_IS_DDSI2;
    os_mutexUnlock (&gv.privileged_pp_lock);

    os_gethostname(node, sizeof(node)-1);
    node[sizeof(node)-1] = '\0';
    size = strlen(node) + strlen(OS_VERSION) + strlen(OS_HOST_NAME) + strlen(OS_TARGET_NAME) + 4; /* + ///'\0' */
    ps.prismtech_participant_version_info.internals = os_malloc(size);
    (void) snprintf(ps.prismtech_participant_version_info.internals, size, "%s/%s/%s/%s", node, OS_VERSION, OS_HOST_NAME, OS_TARGET_NAME);
    DDS_TRACE("spdp_write(%x:%x:%x:%x) - internals: %s\n", PGUID (pp->e.guid), ps.prismtech_participant_version_info.internals);
  }

  /* Participant QoS's insofar as they are set, different from the default, and mapped to the SPDP data, rather than to the PrismTech-specific CMParticipant endpoint.  Currently, that means just USER_DATA. */
  qosdiff = nn_xqos_delta (&pp->plist->qos, &gv.default_plist_pp.qos, QP_USER_DATA);
  if (config.explicitly_publish_qos_set_to_default)
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

int spdp_dispose_unregister (struct participant *pp)
{
  struct nn_xmsg *mpayload;
  nn_plist_t ps;
  struct writer *wr;
  int ret;

  if ((wr = get_builtin_writer (pp, NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER)) == NULL)
  {
    DDS_TRACE("spdp_dispose_unregister(%x:%x:%x:%x) - builtin participant writer not found\n", PGUID (pp->e.guid));
    return 0;
  }

  mpayload = nn_xmsg_new (gv.xmsgpool, &pp->e.guid.prefix, 0, NN_XMSG_KIND_DATA);
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

static unsigned pseudo_random_delay (const nn_guid_t *x, const nn_guid_t *y, nn_mtime_t tnow)
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

static void respond_to_spdp (const nn_guid_t *dest_proxypp_guid)
{
  struct ephash_enum_participant est;
  struct participant *pp;
  nn_mtime_t tnow = now_mt ();
  ephash_enum_participant_init (&est);
  while ((pp = ephash_enum_participant_next (&est)) != NULL)
  {
    /* delay_base has 32 bits, so delay_norm is approximately 1s max;
       delay_max <= 1s by config checks */
    unsigned delay_base = pseudo_random_delay (&pp->e.guid, dest_proxypp_guid, tnow);
    unsigned delay_norm = delay_base >> 2;
    int64_t delay_max_ms = config.spdp_response_delay_max / 1000000;
    int64_t delay = (int64_t) delay_norm * delay_max_ms / 1000;
    nn_mtime_t tsched = add_duration_to_mtime (tnow, delay);
    DDS_TRACE(" %"PRId64, delay);
    if (!config.unicast_response_to_spdp_messages)
      /* pp can't reach gc_delete_participant => can safely reschedule */
      resched_xevent_if_earlier (pp->spdp_xevent, tsched);
    else
      qxev_spdp (tsched, &pp->e.guid, dest_proxypp_guid);
  }
  ephash_enum_participant_fini (&est);
}

static int handle_SPDP_dead (const struct receiver_state *rst, nn_wctime_t timestamp, const nn_plist_t *datap, unsigned statusinfo)
{
  nn_guid_t guid;

  if (!(dds_get_log_mask() & DDS_LC_DISCOVERY))
    DDS_LOG(DDS_LC_DISCOVERY, "SPDP ST%x", statusinfo);

  if (datap->present & PP_PARTICIPANT_GUID)
  {
    guid = datap->participant_guid;
    DDS_LOG(DDS_LC_DISCOVERY, " %x:%x:%x:%x", PGUID (guid));
    assert (guid.entityid.u == NN_ENTITYID_PARTICIPANT);
    if (delete_proxy_participant_by_guid (&guid, timestamp, 0) < 0)
    {
      DDS_LOG(DDS_LC_DISCOVERY, " unknown");
    }
    else
    {
      DDS_LOG(DDS_LC_DISCOVERY, " delete");
    }
  }
  else
  {
    DDS_WARNING("data (SPDP, vendor %u.%u): no/invalid payload\n", rst->vendor.id[0], rst->vendor.id[1]);
  }
  return 1;
}

static void allowmulticast_aware_add_to_addrset (struct addrset *as, const nn_locator_t *loc)
{
#if DDSI_INCLUDE_SSM
  if (ddsi_is_ssm_mcaddr (loc))
  {
    if (!(config.allowMulticast & AMC_SSM))
      return;
  }
  else if (ddsi_is_mcaddr (loc))
  {
    if (!(config.allowMulticast & AMC_ASM))
      return;
  }
#else
  if (ddsi_is_mcaddr (loc) && !(config.allowMulticast & AMC_ASM))
    return;
#endif
  add_to_addrset (as, loc);
}

static struct proxy_participant *find_ddsi2_proxy_participant (const nn_guid_t *ppguid)
{
  struct ephash_enum_proxy_participant it;
  struct proxy_participant *pp;
  ephash_enum_proxy_participant_init (&it);
  while ((pp = ephash_enum_proxy_participant_next (&it)) != NULL)
  {
    if (vendor_is_eclipse_or_opensplice (pp->vendor) && pp->e.guid.prefix.u[0] == ppguid->prefix.u[0] && pp->is_ddsi2_pp)
      break;
  }
  ephash_enum_proxy_participant_fini (&it);
  return pp;
}

static void make_participants_dependent_on_ddsi2 (const nn_guid_t *ddsi2guid, nn_wctime_t timestamp)
{
  struct ephash_enum_proxy_participant it;
  struct proxy_participant *pp, *d2pp;
  struct lease *d2pp_lease;
  if ((d2pp = ephash_lookup_proxy_participant_guid (ddsi2guid)) == NULL)
    return;
  d2pp_lease = os_atomic_ldvoidp (&d2pp->lease);
  ephash_enum_proxy_participant_init (&it);
  while ((pp = ephash_enum_proxy_participant_next (&it)) != NULL)
  {
    if (vendor_is_eclipse_or_opensplice (pp->vendor) && pp->e.guid.prefix.u[0] == ddsi2guid->prefix.u[0] && !pp->is_ddsi2_pp)
    {
      DDS_TRACE("proxy participant %x:%x:%x:%x depends on ddsi2 %x:%x:%x:%x", PGUID (pp->e.guid), PGUID (*ddsi2guid));
      os_mutexLock (&pp->e.lock);
      pp->privileged_pp_guid = *ddsi2guid;
      os_mutexUnlock (&pp->e.lock);
      proxy_participant_reassign_lease (pp, d2pp_lease);
      DDS_TRACE("\n");

      if (ephash_lookup_proxy_participant_guid (ddsi2guid) == NULL)
      {
        /* If DDSI2 has been deleted here (i.e., very soon after
           having been created), we don't know whether pp will be
           deleted */
        break;
      }
    }
  }
  ephash_enum_proxy_participant_fini (&it);

  if (pp != NULL)
  {
    DDS_TRACE("make_participants_dependent_on_ddsi2: ddsi2 %x:%x:%x:%x is no more, delete %x:%x:%x:%x\n", PGUID (*ddsi2guid), PGUID (pp->e.guid));
    delete_proxy_participant_by_guid (&pp->e.guid, timestamp, 1);
  }
}

static int handle_SPDP_alive (const struct receiver_state *rst, nn_wctime_t timestamp, const nn_plist_t *datap)
{
  const unsigned bes_sedp_announcer_mask =
    NN_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER |
    NN_DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER;
  struct addrset *as_meta, *as_default;
  struct proxy_participant *proxypp;
  unsigned builtin_endpoint_set;
  unsigned prismtech_builtin_endpoint_set;
  nn_guid_t privileged_pp_guid;
  nn_duration_t lease_duration;
  unsigned custom_flags = 0;

  if (!(datap->present & PP_PARTICIPANT_GUID) || !(datap->present & PP_BUILTIN_ENDPOINT_SET))
  {
    DDS_WARNING("data (SPDP, vendor %u.%u): no/invalid payload\n", rst->vendor.id[0], rst->vendor.id[1]);
    return 1;
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
      config.assume_rti_has_pmd_endpoints)
  {
    DDS_LOG(DDS_LC_DISCOVERY, "data (SPDP, vendor %u.%u): assuming unadvertised PMD endpoints do exist\n",
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
      DDS_LOG(DDS_LC_DISCOVERY, " (ptbes_fixed_0 %x)", prismtech_builtin_endpoint_set);
      if (prismtech_builtin_endpoint_set & NN_DISC_BUILTIN_ENDPOINT_CM_PARTICIPANT_READER)
        prismtech_builtin_endpoint_set |= NN_DISC_BUILTIN_ENDPOINT_CM_PUBLISHER_READER | NN_DISC_BUILTIN_ENDPOINT_CM_SUBSCRIBER_READER;
      if (prismtech_builtin_endpoint_set & NN_DISC_BUILTIN_ENDPOINT_CM_PARTICIPANT_WRITER)
        prismtech_builtin_endpoint_set |= NN_DISC_BUILTIN_ENDPOINT_CM_PUBLISHER_WRITER | NN_DISC_BUILTIN_ENDPOINT_CM_SUBSCRIBER_WRITER;
  }

  /* Local SPDP packets may be looped back, and that may include ones
     currently being deleted.  The first thing that happens when
     deleting a participant is removing it from the hash table, and
     consequently the looped back packet may appear to be from an
     unknown participant.  So we handle that, too. */

  if (is_deleted_participant_guid (&datap->participant_guid, DPG_REMOTE))
  {
    DDS_LOG(DDS_LC_TRACE, "SPDP ST0 %x:%x:%x:%x (recently deleted)", PGUID (datap->participant_guid));
    return 1;
  }

  {
    int islocal = 0;
    if (ephash_lookup_participant_guid (&datap->participant_guid))
      islocal = 1;
    if (islocal)
    {
      DDS_LOG(DDS_LC_TRACE, "SPDP ST0 %x:%x:%x:%x (local %d)", islocal, PGUID (datap->participant_guid));
      return 0;
    }
  }

  if ((proxypp = ephash_lookup_proxy_participant_guid (&datap->participant_guid)) != NULL)
  {
    /* SPDP processing is so different from normal processing that we
       are even skipping the automatic lease renewal.  Therefore do it
       regardless of
       config.arrival_of_data_asserts_pp_and_ep_liveliness. */
    DDS_LOG(DDS_LC_TRACE, "SPDP ST0 %x:%x:%x:%x (known)", PGUID (datap->participant_guid));
    lease_renew (os_atomic_ldvoidp (&proxypp->lease), now_et ());
    os_mutexLock (&proxypp->e.lock);
    if (proxypp->implicitly_created)
    {
      DDS_LOG(DDS_LC_DISCOVERY, " (NEW was-implicitly-created)");
      proxypp->implicitly_created = 0;
      update_proxy_participant_plist_locked (proxypp, datap, UPD_PROXYPP_SPDP, timestamp);
    }
    os_mutexUnlock (&proxypp->e.lock);
    return 0;
  }

  DDS_LOG(DDS_LC_DISCOVERY, "SPDP ST0 %x:%x:%x:%x bes %x ptbes %x NEW", PGUID (datap->participant_guid), builtin_endpoint_set, prismtech_builtin_endpoint_set);

  if (datap->present & PP_PARTICIPANT_LEASE_DURATION)
  {
    lease_duration = datap->participant_lease_duration;
  }
  else
  {
    DDS_LOG(DDS_LC_DISCOVERY, " (PARTICIPANT_LEASE_DURATION defaulting to 100s)");
    lease_duration = nn_to_ddsi_duration (100 * T_SECOND);
  }

  if (datap->present & PP_PRISMTECH_PARTICIPANT_VERSION_INFO) {
    if (datap->prismtech_participant_version_info.flags & NN_PRISMTECH_FL_KERNEL_SEQUENCE_NUMBER)
      custom_flags |= CF_INC_KERNEL_SEQUENCE_NUMBERS;

    if ((datap->prismtech_participant_version_info.flags & NN_PRISMTECH_FL_DDSI2_PARTICIPANT_FLAG) &&
        (datap->prismtech_participant_version_info.flags & NN_PRISMTECH_FL_PARTICIPANT_IS_DDSI2))
      custom_flags |= CF_PARTICIPANT_IS_DDSI2;

    DDS_LOG(DDS_LC_DISCOVERY, " (0x%08x-0x%08x-0x%08x-0x%08x-0x%08x %s)",
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
     NN_ENTITYID_PARTICIPANT or ephash_lookup will assert.  So we only
     zero the prefix. */
  privileged_pp_guid.prefix = rst->src_guid_prefix;
  privileged_pp_guid.entityid.u = NN_ENTITYID_PARTICIPANT;
  if ((builtin_endpoint_set & bes_sedp_announcer_mask) != bes_sedp_announcer_mask &&
      memcmp (&privileged_pp_guid, &datap->participant_guid, sizeof (nn_guid_t)) != 0)
  {
    DDS_LOG(DDS_LC_DISCOVERY, " (depends on %x:%x:%x:%x)", PGUID (privileged_pp_guid));
    /* never expire lease for this proxy: it won't actually expire
       until the "privileged" one expires anyway */
    lease_duration = nn_to_ddsi_duration (T_NEVER);
  }
  else if (vendor_is_eclipse_or_opensplice (rst->vendor) && !(custom_flags & CF_PARTICIPANT_IS_DDSI2))
  {
    /* Non-DDSI2 participants are made dependent on DDSI2 (but DDSI2
       itself need not be discovered yet) */
    struct proxy_participant *ddsi2;
    if ((ddsi2 = find_ddsi2_proxy_participant (&datap->participant_guid)) == NULL)
      memset (&privileged_pp_guid.prefix, 0, sizeof (privileged_pp_guid.prefix));
    else
    {
      privileged_pp_guid.prefix = ddsi2->e.guid.prefix;
      lease_duration = nn_to_ddsi_duration (T_NEVER);
      DDS_LOG(DDS_LC_DISCOVERY, " (depends on %x:%x:%x:%x)", PGUID (privileged_pp_guid));
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

    if ((datap->present & PP_DEFAULT_MULTICAST_LOCATOR) && (get_locator (&loc, &datap->default_multicast_locators, 0)))
      allowmulticast_aware_add_to_addrset (as_default, &loc);
    if ((datap->present & PP_METATRAFFIC_MULTICAST_LOCATOR) && (get_locator (&loc, &datap->metatraffic_multicast_locators, 0)))
      allowmulticast_aware_add_to_addrset (as_meta, &loc);

    /* If no multicast locators or multicast TTL > 1, assume IP (multicast) routing can be relied upon to reach
       the remote participant, else only accept nodes with an advertised unicast address in the same subnet to
       protect against multicasts being received over an unexpected interface (which sometimes appears to occur) */
    if (addrset_empty_mc (as_default) && addrset_empty_mc (as_meta))
      uc_same_subnet = 0;
    else if (config.multicast_ttl > 1)
      uc_same_subnet = 0;
    else
    {
      uc_same_subnet = 1;
      DDS_LOG(DDS_LC_DISCOVERY, " subnet-filter");
    }

    /* If unicast locators not present, then try to obtain from connection */
    if (!config.tcp_use_peeraddr_for_unicast && (datap->present & PP_DEFAULT_UNICAST_LOCATOR) && (get_locator (&loc, &datap->default_unicast_locators, uc_same_subnet)))
      add_to_addrset (as_default, &loc);
    else {
      DDS_LOG(DDS_LC_DISCOVERY, " (srclocD)");
      add_to_addrset (as_default, &rst->srcloc);
    }

    if (!config.tcp_use_peeraddr_for_unicast && (datap->present & PP_METATRAFFIC_UNICAST_LOCATOR) && (get_locator (&loc, &datap->metatraffic_unicast_locators, uc_same_subnet)))
      add_to_addrset (as_meta, &loc);
    else {
      DDS_LOG(DDS_LC_DISCOVERY, " (srclocM)");
      add_to_addrset (as_meta, &rst->srcloc);
    }

    nn_log_addrset(DDS_LC_DISCOVERY, " (data", as_default);
    nn_log_addrset(DDS_LC_DISCOVERY, " meta", as_meta);
    DDS_LOG(DDS_LC_DISCOVERY, ")");
  }

  if (addrset_empty_uc (as_default) || addrset_empty_uc (as_meta))
  {
    DDS_LOG(DDS_LC_DISCOVERY, " (no unicast address");
    unref_addrset (as_default);
    unref_addrset (as_meta);
    return 1;
  }

  DDS_LOG(DDS_LC_DISCOVERY, " QOS={");
  nn_log_xqos(DDS_LC_DISCOVERY, &datap->qos);
  DDS_LOG(DDS_LC_DISCOVERY, "}\n");

  maybe_add_pp_as_meta_to_as_disc (as_meta);

  new_proxy_participant
  (
    &datap->participant_guid,
    builtin_endpoint_set,
    prismtech_builtin_endpoint_set,
    &privileged_pp_guid,
    as_default,
    as_meta,
    datap,
    nn_from_ddsi_duration (lease_duration),
    rst->vendor,
    custom_flags,
    timestamp
  );

  /* Force transmission of SPDP messages - we're not very careful
     in avoiding the processing of SPDP packets addressed to others
     so filter here */
  {
    int have_dst =
      (rst->dst_guid_prefix.u[0] != 0 || rst->dst_guid_prefix.u[1] != 0 || rst->dst_guid_prefix.u[2] != 0);
    if (!have_dst)
    {
      DDS_LOG(DDS_LC_DISCOVERY, "broadcasted SPDP packet -> answering");
      respond_to_spdp (&datap->participant_guid);
    }
    else
    {
      DDS_LOG(DDS_LC_DISCOVERY, "directed SPDP packet -> not responding\n");
    }
  }

  if (custom_flags & CF_PARTICIPANT_IS_DDSI2)
  {
    /* If we just discovered DDSI2, make sure any existing
       participants served by it are made dependent on it */
    make_participants_dependent_on_ddsi2 (&datap->participant_guid, timestamp);
  }
  else if (privileged_pp_guid.prefix.u[0] || privileged_pp_guid.prefix.u[1] || privileged_pp_guid.prefix.u[2])
  {
    /* If we just created a participant dependent on DDSI2, make sure
       DDSI2 still exists.  There is a risk of racing the lease expiry
       of DDSI2. */
    if (ephash_lookup_proxy_participant_guid (&privileged_pp_guid) == NULL)
    {
      DDS_LOG(DDS_LC_DISCOVERY, "make_participants_dependent_on_ddsi2: ddsi2 %x:%x:%x:%x is no more, delete %x:%x:%x:%x\n", PGUID (privileged_pp_guid), PGUID (datap->participant_guid));
      delete_proxy_participant_by_guid (&datap->participant_guid, timestamp, 1);
    }
  }
  return 1;
}

static void handle_SPDP (const struct receiver_state *rst, nn_wctime_t timestamp, unsigned statusinfo, const void *vdata, unsigned len)
{
  const struct CDRHeader *data = vdata; /* built-ins not deserialized (yet) */
  DDS_TRACE("SPDP ST%x", statusinfo);
  if (data == NULL)
  {
    DDS_TRACE(" no payload?\n");
    return;
  }
  else
  {
    nn_plist_t decoded_data;
    nn_plist_src_t src;
    int interesting = 0;
    int plist_ret;
    src.protocol_version = rst->protocol_version;
    src.vendorid = rst->vendor;
    src.encoding = data->identifier;
    src.buf = (unsigned char *) data + 4;
    src.bufsz = len - 4;
    if ((plist_ret = nn_plist_init_frommsg (&decoded_data, NULL, ~(uint64_t)0, ~(uint64_t)0, &src)) < 0)
    {
      if (plist_ret != ERR_INCOMPATIBLE)
        DDS_WARNING("SPDP (vendor %u.%u): invalid qos/parameters\n", src.vendorid.id[0], src.vendorid.id[1]);
      return;
    }

    switch (statusinfo & (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER))
    {
      case 0:
        interesting = handle_SPDP_alive (rst, timestamp, &decoded_data);
        break;

      case NN_STATUSINFO_DISPOSE:
      case NN_STATUSINFO_UNREGISTER:
      case (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER):
        interesting = handle_SPDP_dead (rst, timestamp, &decoded_data, statusinfo);
        break;
    }

    nn_plist_fini (&decoded_data);
    DDS_LOG(interesting ? DDS_LC_DISCOVERY : DDS_LC_TRACE, "\n");
  }
}

static void add_locator_to_ps (const nn_locator_t *loc, void *arg)
{
  nn_plist_t *ps = (nn_plist_t *) arg;
  struct nn_locators_one *elem = os_malloc (sizeof (struct nn_locators_one));
  struct nn_locators *locs;
  unsigned present_flag;

  elem->loc = *loc;
  elem->next = NULL;

  if (ddsi_is_mcaddr (loc)) {
    locs = &ps->multicast_locators;
    present_flag = PP_MULTICAST_LOCATOR;
  } else {
    locs = &ps->unicast_locators;
    present_flag = PP_UNICAST_LOCATOR;
  }

  if (!(ps->present & present_flag))
  {
    locs->n = 0;
    locs->first = locs->last = NULL;
    ps->present |= present_flag;
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
   struct writer *wr, int alive, const nn_guid_t *epguid,
   const struct entity_common *common, const struct endpoint_common *epcommon,
   const nn_xqos_t *xqos, struct addrset *as)
{
  const nn_xqos_t *defqos = is_writer_entityid (epguid->entityid) ? &gv.default_xqos_wr : &gv.default_xqos_rd;
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
      const struct reader *rd = ephash_lookup_reader_guid (epguid);
      assert (rd);
      if (rd->favours_ssm)
      {
        ps.present |= PP_READER_FAVOURS_SSM;
        ps.reader_favours_ssm.state = 1u;
      }
    }
#endif

    qosdiff = nn_xqos_delta (xqos, defqos, ~(uint64_t)0);
    if (config.explicitly_publish_qos_set_to_default)
      qosdiff |= ~QP_UNRECOGNIZED_INCOMPATIBLE_MASK;

    if (as)
    {
      addrset_forall (as, add_locator_to_ps, &ps);
    }
  }

  /* The message is only a temporary thing, used only for encoding
     the QoS and other settings. So the header fields aren't really
     important, except that they need to be set to reasonable things
     or it'll crash */
  mpayload = nn_xmsg_new (gv.xmsgpool, &wr->e.guid.prefix, 0, NN_XMSG_KIND_DATA);
  nn_plist_addtomsg (mpayload, &ps, ~(uint64_t)0, ~(uint64_t)0);
  if (xqos) nn_xqos_addtomsg (mpayload, xqos, qosdiff);
  nn_xmsg_addpar_sentinel (mpayload);
  nn_plist_fini (&ps);

  DDS_LOG(DDS_LC_DISCOVERY, "sedp: write for %x:%x:%x:%x via %x:%x:%x:%x\n", PGUID (*epguid), PGUID (wr->e.guid));
  ret = write_mpayload (wr, alive, PID_ENDPOINT_GUID, mpayload);
  nn_xmsg_free (mpayload);
  return ret;
}

static struct writer *get_sedp_writer (const struct participant *pp, unsigned entityid)
{
  struct writer *sedp_wr = get_builtin_writer (pp, entityid);
  if (sedp_wr == NULL)
    DDS_FATAL("sedp_write_writer: no SEDP builtin writer %x for %x:%x:%x:%x\n", entityid, PGUID (pp->e.guid));
  return sedp_wr;
}

int sedp_write_writer (struct writer *wr)
{
  if ((!is_builtin_entityid(wr->e.guid.entityid, NN_VENDORID_ECLIPSE)) && (!wr->e.onlylocal))
  {
    struct writer *sedp_wr = get_sedp_writer (wr->c.pp, NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER);
#ifdef DDSI_INCLUDE_SSM
    struct addrset *as = wr->ssm_as;
#else
    struct addrset *as = NULL;
#endif
    return sedp_write_endpoint (sedp_wr, 1, &wr->e.guid, &wr->e, &wr->c, wr->xqos, as);
  }
  return 0;
}

int sedp_write_reader (struct reader *rd)
{
  if ((!is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE)) && (!rd->e.onlylocal))
  {
    struct writer *sedp_wr = get_sedp_writer (rd->c.pp, NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER);
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
    struct addrset *as = rd->as;
#else
    struct addrset *as = NULL;
#endif
    return sedp_write_endpoint (sedp_wr, 1, &rd->e.guid, &rd->e, &rd->c, rd->xqos, as);
  }
  return 0;
}

int sedp_dispose_unregister_writer (struct writer *wr)
{
  if ((!is_builtin_entityid(wr->e.guid.entityid, NN_VENDORID_ECLIPSE)) && (!wr->e.onlylocal))
  {
    struct writer *sedp_wr = get_sedp_writer (wr->c.pp, NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER);
    return sedp_write_endpoint (sedp_wr, 0, &wr->e.guid, NULL, NULL, NULL, NULL);
  }
  return 0;
}

int sedp_dispose_unregister_reader (struct reader *rd)
{
  if ((!is_builtin_entityid(rd->e.guid.entityid, NN_VENDORID_ECLIPSE)) && (!rd->e.onlylocal))
  {
    struct writer *sedp_wr = get_sedp_writer (rd->c.pp, NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER);
    return sedp_write_endpoint (sedp_wr, 0, &rd->e.guid, NULL, NULL, NULL, NULL);
  }
  return 0;
}

static const char *durability_to_string (nn_durability_kind_t k)
{
  switch (k)
  {
    case NN_VOLATILE_DURABILITY_QOS: return "volatile";
    case NN_TRANSIENT_LOCAL_DURABILITY_QOS: return "transient-local";
    case NN_TRANSIENT_DURABILITY_QOS: return "transient";
    case NN_PERSISTENT_DURABILITY_QOS: return "persistent";
  }
  return "undefined-durability";
}

static struct proxy_participant *implicitly_create_proxypp (const nn_guid_t *ppguid, nn_plist_t *datap /* note: potentially modifies datap */, const nn_guid_prefix_t *src_guid_prefix, nn_vendorid_t vendorid, nn_wctime_t timestamp)
{
  nn_guid_t privguid;
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
    DDS_TRACE(" from-DS %x:%x:%x:%x", PGUID (privguid));
    /* avoid "no address" case, so we never create the proxy participant for nothing (FIXME: rework some of this) */
    if (!(datap->present & (PP_UNICAST_LOCATOR | PP_MULTICAST_LOCATOR)))
    {
      DDS_TRACE(" data locator absent\n");
      goto err;
    }
    DDS_TRACE(" new-proxypp %x:%x:%x:%x\n", PGUID (*ppguid));
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
    new_proxy_participant(ppguid, 0, 0, &privguid, new_addrset(), new_addrset(), &pp_plist, T_NEVER, actual_vendorid, CF_IMPLICITLY_CREATED_PROXYPP, timestamp);
  }
  else if (ppguid->prefix.u[0] == src_guid_prefix->u[0] && vendor_is_eclipse_or_opensplice (vendorid))
  {
    /* FIXME: requires address sets to be those of ddsi2, no built-in
       readers or writers, only if remote ddsi2 is provably running
       with a minimal built-in endpoint set */
    struct proxy_participant *privpp;
    if ((privpp = ephash_lookup_proxy_participant_guid (&privguid)) == NULL) {
      DDS_TRACE(" unknown-src-proxypp?\n");
      goto err;
    } else if (!privpp->is_ddsi2_pp) {
      DDS_TRACE(" src-proxypp-not-ddsi2?\n");
      goto err;
    } else if (!privpp->minimal_bes_mode) {
      DDS_TRACE(" src-ddsi2-not-minimal-bes-mode?\n");
      goto err;
    } else {
      struct addrset *as_default, *as_meta;
      nn_plist_t tmp_plist;
      DDS_TRACE(" from-ddsi2 %x:%x:%x:%x", PGUID (privguid));
      nn_plist_init_empty (&pp_plist);

      os_mutexLock (&privpp->e.lock);
      as_default = ref_addrset(privpp->as_default);
      as_meta = ref_addrset(privpp->as_meta);
      /* copy just what we need */
      tmp_plist = *privpp->plist;
      tmp_plist.present = PP_PARTICIPANT_GUID | PP_PRISMTECH_PARTICIPANT_VERSION_INFO;
      tmp_plist.participant_guid = *ppguid;
      nn_plist_mergein_missing (&pp_plist, &tmp_plist);
      os_mutexUnlock (&privpp->e.lock);

      pp_plist.prismtech_participant_version_info.flags &= ~NN_PRISMTECH_FL_PARTICIPANT_IS_DDSI2;
      new_proxy_participant (ppguid, 0, 0, &privguid, as_default, as_meta, &pp_plist, T_NEVER, vendorid, CF_IMPLICITLY_CREATED_PROXYPP | CF_PROXYPP_NO_SPDP, timestamp);
    }
  }

 err:
  nn_plist_fini (&pp_plist);
  return ephash_lookup_proxy_participant_guid (ppguid);
}

static void handle_SEDP_alive (const struct receiver_state *rst, nn_plist_t *datap /* note: potentially modifies datap */, const nn_guid_prefix_t *src_guid_prefix, nn_vendorid_t vendorid, nn_wctime_t timestamp)
{
#define E(msg, lbl) do { DDS_LOG(DDS_LC_DISCOVERY, msg); goto lbl; } while (0)
  struct proxy_participant *pp;
  struct proxy_writer * pwr = NULL;
  struct proxy_reader * prd = NULL;
  nn_guid_t ppguid;
  nn_xqos_t *xqos;
  int reliable;
  struct addrset *as;
  int is_writer;
#ifdef DDSI_INCLUDE_SSM
  int ssm;
#endif

  assert (datap);

  if (!(datap->present & PP_ENDPOINT_GUID))
    E (" no guid?\n", err);
  DDS_LOG(DDS_LC_DISCOVERY, " %x:%x:%x:%x", PGUID (datap->endpoint_guid));

  ppguid.prefix = datap->endpoint_guid.prefix;
  ppguid.entityid.u = NN_ENTITYID_PARTICIPANT;
  if (is_deleted_participant_guid (&ppguid, DPG_REMOTE))
    E (" local dead pp?\n", err);

  if (ephash_lookup_participant_guid (&ppguid) != NULL)
    E (" local pp?\n", err);

  if (is_builtin_entityid (datap->endpoint_guid.entityid, vendorid))
    E (" built-in\n", err);
  if (!(datap->qos.present & QP_TOPIC_NAME))
    E (" no topic?\n", err);
  if (!(datap->qos.present & QP_TYPE_NAME))
    E (" no typename?\n", err);

  if ((pp = ephash_lookup_proxy_participant_guid (&ppguid)) == NULL)
  {
    DDS_LOG(DDS_LC_DISCOVERY, " unknown-proxypp");
    if ((pp = implicitly_create_proxypp (&ppguid, datap, src_guid_prefix, vendorid, timestamp)) == NULL)
      E ("?\n", err);
    /* Repeat regular SEDP trace for convenience */
    DDS_LOG(DDS_LC_DISCOVERY, "SEDP ST0 %x:%x:%x:%x (cont)", PGUID (datap->endpoint_guid));
  }

  xqos = &datap->qos;
  is_writer = is_writer_entityid (datap->endpoint_guid.entityid);
  if (!is_writer)
    nn_xqos_mergein_missing (xqos, &gv.default_xqos_rd);
  else if (vendor_is_eclipse_or_prismtech(vendorid))
    nn_xqos_mergein_missing (xqos, &gv.default_xqos_wr);
  else
    nn_xqos_mergein_missing (xqos, &gv.default_xqos_wr_nad);

  /* After copy + merge, should have at least the ones present in the
     input.  Also verify reliability and durability are present,
     because we explicitly read those. */
  assert ((xqos->present & datap->qos.present) == datap->qos.present);
  assert (xqos->present & QP_RELIABILITY);
  assert (xqos->present & QP_DURABILITY);
  reliable = (xqos->reliability.kind == NN_RELIABLE_RELIABILITY_QOS);

  DDS_LOG(DDS_LC_DISCOVERY, " %s %s %s: %s%s.%s/%s",
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
    pwr = ephash_lookup_proxy_writer_guid (&datap->endpoint_guid);
  }
  else
  {
    prd = ephash_lookup_proxy_reader_guid (&datap->endpoint_guid);
  }
  if (pwr || prd)
  {
    /* Cloud load balances by updating participant endpoints */

    if (! vendor_is_cloud (vendorid))
    {
      DDS_LOG(DDS_LC_DISCOVERY, " known\n");
      goto err;
    }

    /* Re-bind the proxy participant to the discovery service - and do this if it is currently
       bound to another DS instance, because that other DS instance may have already failed and
       with a new one taking over, without our noticing it. */
    DDS_LOG(DDS_LC_DISCOVERY, " known-DS");
    if (vendor_is_cloud (vendorid) && pp->implicitly_created && memcmp(&pp->privileged_pp_guid.prefix, src_guid_prefix, sizeof(pp->privileged_pp_guid.prefix)) != 0)
    {
      nn_etime_t never = { T_NEVER };
      DDS_LOG(DDS_LC_DISCOVERY, " %x:%x:%x:%x attach-to-DS %x:%x:%x:%x", PGUID(pp->e.guid), PGUIDPREFIX(*src_guid_prefix), pp->privileged_pp_guid.entityid.u);
      os_mutexLock (&pp->e.lock);
      pp->privileged_pp_guid.prefix = *src_guid_prefix;
      lease_set_expiry(os_atomic_ldvoidp(&pp->lease), never);
      os_mutexUnlock (&pp->e.lock);
    }
    DDS_LOG(DDS_LC_DISCOVERY, "\n");
  }
  else
  {
    DDS_LOG(DDS_LC_DISCOVERY, " NEW");
  }

  {
    nn_locator_t loc;
    as = new_addrset ();
    if (!config.tcp_use_peeraddr_for_unicast && (datap->present & PP_UNICAST_LOCATOR) && get_locator (&loc, &datap->unicast_locators, 0))
      add_to_addrset (as, &loc);
    else if (config.tcp_use_peeraddr_for_unicast)
    {
      DDS_LOG(DDS_LC_DISCOVERY, " (srcloc)");
      add_to_addrset (as, &rst->srcloc);
    }
    else
    {
      copy_addrset_into_addrset_uc (as, pp->as_default);
    }
    if ((datap->present & PP_MULTICAST_LOCATOR) && get_locator (&loc, &datap->multicast_locators, 0))
      allowmulticast_aware_add_to_addrset (as, &loc);
    else
      copy_addrset_into_addrset_mc (as, pp->as_default);
  }
  if (addrset_empty (as))
  {
    unref_addrset (as);
    E (" no address", err);
  }

  nn_log_addrset(DDS_LC_DISCOVERY, " (as", as);
#ifdef DDSI_INCLUDE_SSM
  ssm = 0;
  if (is_writer)
    ssm = addrset_contains_ssm (as);
  else if (datap->present & PP_READER_FAVOURS_SSM)
    ssm = (datap->reader_favours_ssm.state != 0);
  DDS_LOG(DDS_LC_DISCOVERY, " ssm=%u", ssm);
#endif
  DDS_LOG(DDS_LC_DISCOVERY, ") QOS={");
  nn_log_xqos(DDS_LC_DISCOVERY, xqos);
  DDS_LOG(DDS_LC_DISCOVERY, "}\n");

  if ((datap->endpoint_guid.entityid.u & NN_ENTITYID_SOURCE_MASK) == NN_ENTITYID_SOURCE_VENDOR && !vendor_is_eclipse_or_prismtech (vendorid))
  {
    DDS_LOG(DDS_LC_DISCOVERY, "ignoring vendor-specific endpoint %x:%x:%x:%x\n", PGUID (datap->endpoint_guid));
  }
  else
  {
    if (is_writer)
    {
      if (pwr)
      {
        update_proxy_writer (pwr, as);
      }
      else
      {
        /* not supposed to get here for built-in ones, so can determine the channel based on the transport priority */
        assert (!is_builtin_entityid (datap->endpoint_guid.entityid, vendorid));
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
        {
          struct config_channel_listelem *channel = find_channel (xqos->transport_priority);
          new_proxy_writer (&ppguid, &datap->endpoint_guid, as, datap, channel->dqueue, channel->evq ? channel->evq : gv.xevents, timestamp);
        }
#else
        new_proxy_writer (&ppguid, &datap->endpoint_guid, as, datap, gv.user_dqueue, gv.xevents, timestamp);
#endif
      }
    }
    else
    {
      if (prd)
      {
        update_proxy_reader (prd, as);
      }
      else
      {
#ifdef DDSI_INCLUDE_SSM
        new_proxy_reader (&ppguid, &datap->endpoint_guid, as, datap, timestamp, ssm);
#else
        new_proxy_reader (&ppguid, &datap->endpoint_guid, as, datap, timestamp);
#endif
      }
    }
  }

  unref_addrset (as);

err:

  return;
#undef E
}

static void handle_SEDP_dead (nn_plist_t *datap, nn_wctime_t timestamp)
{
  int res;
  if (!(datap->present & PP_ENDPOINT_GUID))
  {
    DDS_LOG(DDS_LC_DISCOVERY, " no guid?\n");
    return;
  }
  DDS_LOG(DDS_LC_DISCOVERY, " %x:%x:%x:%x", PGUID (datap->endpoint_guid));
  if (is_writer_entityid (datap->endpoint_guid.entityid))
  {
    res = delete_proxy_writer (&datap->endpoint_guid, timestamp, 0);
  }
  else
  {
    res = delete_proxy_reader (&datap->endpoint_guid, timestamp, 0);
  }
  DDS_LOG(DDS_LC_DISCOVERY, " %s\n", (res < 0) ? " unknown" : " delete");
}

static void handle_SEDP (const struct receiver_state *rst, nn_wctime_t timestamp, unsigned statusinfo, const void *vdata, unsigned len)
{
  const struct CDRHeader *data = vdata; /* built-ins not deserialized (yet) */
  DDS_LOG(DDS_LC_DISCOVERY, "SEDP ST%x", statusinfo);
  if (data == NULL)
  {
    DDS_LOG(DDS_LC_DISCOVERY, " no payload?\n");
    return;
  }
  else
  {
    nn_plist_t decoded_data;
    nn_plist_src_t src;
    int plist_ret;
    src.protocol_version = rst->protocol_version;
    src.vendorid = rst->vendor;
    src.encoding = data->identifier;
    src.buf = (unsigned char *) data + 4;
    src.bufsz = len - 4;
    if ((plist_ret = nn_plist_init_frommsg (&decoded_data, NULL, ~(uint64_t)0, ~(uint64_t)0, &src)) < 0)
    {
      if (plist_ret != ERR_INCOMPATIBLE)
        DDS_WARNING("SEDP (vendor %u.%u): invalid qos/parameters\n", src.vendorid.id[0], src.vendorid.id[1]);
      return;
    }

    switch (statusinfo & (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER))
    {
      case 0:
        handle_SEDP_alive (rst, &decoded_data, &rst->src_guid_prefix, rst->vendor, timestamp);
        break;

      case NN_STATUSINFO_DISPOSE:
      case NN_STATUSINFO_UNREGISTER:
      case (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER):
        handle_SEDP_dead (&decoded_data, timestamp);
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

  mpayload = nn_xmsg_new (gv.xmsgpool, &sedp_wr->e.guid.prefix, 0, NN_XMSG_KIND_DATA);
  delta = nn_xqos_delta (&datap->qos, &gv.default_xqos_tp, ~(uint64_t)0);
  if (config.explicitly_publish_qos_set_to_default)
    delta |= ~QP_UNRECOGNIZED_INCOMPATIBLE_MASK;
  nn_plist_addtomsg (mpayload, datap, ~(uint64_t)0, delta);
  nn_xmsg_addpar_sentinel (mpayload);

  DDS_TRACE("sedp: write topic %s via %x:%x:%x:%x\n", datap->qos.topic_name, PGUID (sedp_wr->e.guid));
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
  mpayload = nn_xmsg_new (gv.xmsgpool, &sedp_wr->e.guid.prefix, 0, NN_XMSG_KIND_DATA);
  nn_plist_init_empty (&ps);
  ps.present = PP_PARTICIPANT_GUID;
  ps.participant_guid = pp->e.guid;
  nn_plist_addtomsg (mpayload, &ps, ~(uint64_t)0, ~(uint64_t)0);
  nn_plist_fini (&ps);
  if (alive)
  {
    nn_plist_addtomsg (mpayload, pp->plist,
                       PP_PRISMTECH_NODE_NAME | PP_PRISMTECH_EXEC_NAME | PP_PRISMTECH_PROCESS_ID |
                       PP_PRISMTECH_WATCHDOG_SCHEDULING | PP_PRISMTECH_LISTENER_SCHEDULING |
                       PP_PRISMTECH_SERVICE_TYPE | PP_ENTITY_NAME,
                       QP_PRISMTECH_ENTITY_FACTORY);
  }
  nn_xmsg_addpar_sentinel (mpayload);

  DDS_TRACE("sedp: write CMParticipant ST%x for %x:%x:%x:%x via %x:%x:%x:%x\n",
          alive ? 0 : NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER, PGUID (pp->e.guid), PGUID (sedp_wr->e.guid));
  ret = write_mpayload (sedp_wr, alive, PID_PARTICIPANT_GUID, mpayload);
  nn_xmsg_free (mpayload);
  return ret;
}

static void handle_SEDP_CM (const struct receiver_state *rst, nn_entityid_t wr_entity_id, nn_wctime_t timestamp, unsigned statusinfo, const void *vdata, unsigned len)
{
  const struct CDRHeader *data = vdata; /* built-ins not deserialized (yet) */
  DDS_LOG(DDS_LC_DISCOVERY, "SEDP_CM ST%x", statusinfo);
  assert (wr_entity_id.u == NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_WRITER);
  (void) wr_entity_id;
  if (data == NULL)
  {
    DDS_LOG(DDS_LC_DISCOVERY, " no payload?\n");
    return;
  }
  else
  {
    nn_plist_t decoded_data;
    nn_plist_src_t src;
    int plist_ret;
    src.protocol_version = rst->protocol_version;
    src.vendorid = rst->vendor;
    src.encoding = data->identifier;
    src.buf = (unsigned char *) data + 4;
    src.bufsz = len - 4;
    if ((plist_ret = nn_plist_init_frommsg (&decoded_data, NULL, ~(uint64_t)0, ~(uint64_t)0, &src)) < 0)
    {
      if (plist_ret != ERR_INCOMPATIBLE)
        DDS_WARNING("SEDP_CM (vendor %u.%u): invalid qos/parameters\n", src.vendorid.id[0], src.vendorid.id[1]);
      return;
    }

    /* ignore: dispose/unregister is tied to deleting the participant, which will take care of the dispose/unregister for us */;
    if ((statusinfo & (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER)) == 0)
    {
      struct proxy_participant *proxypp;
      if (!(decoded_data.present & PP_PARTICIPANT_GUID))
        DDS_WARNING("SEDP_CM (vendor %u.%u): missing participant GUID\n", src.vendorid.id[0], src.vendorid.id[1]);
      else
      {
        if ((proxypp = ephash_lookup_proxy_participant_guid (&decoded_data.participant_guid)) == NULL)
          proxypp = implicitly_create_proxypp (&decoded_data.participant_guid, &decoded_data, &rst->src_guid_prefix, rst->vendor, timestamp);
        if (proxypp != NULL)
          update_proxy_participant_plist (proxypp, &decoded_data, UPD_PROXYPP_CM, timestamp);
      }
    }

    nn_plist_fini (&decoded_data);
  }
  DDS_LOG(DDS_LC_DISCOVERY, "\n");
}

static struct participant *group_guid_to_participant (const nn_guid_t *group_guid)
{
  nn_guid_t ppguid;
  ppguid.prefix = group_guid->prefix;
  ppguid.entityid.u = NN_ENTITYID_PARTICIPANT;
  return ephash_lookup_participant_guid (&ppguid);
}

int sedp_write_cm_publisher (const struct nn_plist *datap, int alive)
{
  struct participant *pp;
  struct writer *sedp_wr;
  struct nn_xmsg *mpayload;
  uint64_t delta;
  int ret;

  if ((pp = group_guid_to_participant (&datap->group_guid)) == NULL)
  {
      DDS_TRACE("sedp: write CMPublisher alive:%d for %x:%x:%x:%x dropped: no participant\n",
              alive, PGUID (datap->group_guid));
      return 0;
  }
  sedp_wr = get_sedp_writer (pp, NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_WRITER);

  /* The message is only a temporary thing, used only for encoding
   the QoS and other settings. So the header fields aren't really
   important, except that they need to be set to reasonable things
   or it'll crash */
  mpayload = nn_xmsg_new (gv.xmsgpool, &sedp_wr->e.guid.prefix, 0, NN_XMSG_KIND_DATA);
  if (!alive)
    delta = 0;
  else
  {
    delta = nn_xqos_delta (&datap->qos, &gv.default_xqos_pub, ~(uint64_t)0);
    if (!config.explicitly_publish_qos_set_to_default)
      delta |= ~QP_UNRECOGNIZED_INCOMPATIBLE_MASK;
  }
  nn_plist_addtomsg (mpayload, datap, ~(uint64_t)0, delta);
  nn_xmsg_addpar_sentinel (mpayload);
  ret = write_mpayload (sedp_wr, alive, PID_GROUP_GUID ,mpayload);
  nn_xmsg_free (mpayload);
  return ret;
}

int sedp_write_cm_subscriber (const struct nn_plist *datap, int alive)
{
  struct participant *pp;
  struct writer *sedp_wr;
  struct nn_xmsg *mpayload;
  uint64_t delta;
  int ret;

  if ((pp = group_guid_to_participant (&datap->group_guid)) == NULL)
  {
      DDS_LOG(DDS_LC_DISCOVERY, "sedp: write CMSubscriber alive:%d for %x:%x:%x:%x dropped: no participant\n",
              alive, PGUID (datap->group_guid));
      return 0;
  }
  sedp_wr = get_sedp_writer (pp, NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_WRITER);

  /* The message is only a temporary thing, used only for encoding
   the QoS and other settings. So the header fields aren't really
   important, except that they need to be set to reasonable things
   or it'll crash */
  mpayload = nn_xmsg_new (gv.xmsgpool, &sedp_wr->e.guid.prefix, 0, NN_XMSG_KIND_DATA);
  if (!alive)
    delta = 0;
  else
  {
    delta = nn_xqos_delta (&datap->qos, &gv.default_xqos_sub, ~(uint64_t)0);
    if (!config.explicitly_publish_qos_set_to_default)
      delta |= ~QP_UNRECOGNIZED_INCOMPATIBLE_MASK;
  }
  nn_plist_addtomsg (mpayload, datap, ~(uint64_t)0, delta);
  nn_xmsg_addpar_sentinel (mpayload);
  ret = write_mpayload (sedp_wr, alive, PID_GROUP_GUID, mpayload);
  nn_xmsg_free (mpayload);
  return ret;
}

static void handle_SEDP_GROUP_alive (nn_plist_t *datap /* note: potentially modifies datap */, nn_wctime_t timestamp)
{
#define E(msg, lbl) do { DDS_LOG(DDS_LC_DISCOVERY, msg); goto lbl; } while (0)
  nn_guid_t ppguid;

  if (!(datap->present & PP_GROUP_GUID))
    E (" no guid?\n", err);
  DDS_LOG(DDS_LC_DISCOVERY, " %x:%x:%x:%x", PGUID (datap->group_guid));

  ppguid.prefix = datap->group_guid.prefix;
  ppguid.entityid.u = NN_ENTITYID_PARTICIPANT;
  if (ephash_lookup_proxy_participant_guid (&ppguid) == NULL)
    E (" unknown proxy pp?\n", err);

  DDS_LOG(DDS_LC_DISCOVERY, " alive\n");

  {
    char *name = (datap->present & PP_ENTITY_NAME) ? datap->entity_name : "";
    new_proxy_group (&datap->group_guid, name, &datap->qos, timestamp);
  }
err:
  return;
#undef E
}

static void handle_SEDP_GROUP_dead (nn_plist_t *datap, nn_wctime_t timestamp)
{
  if (!(datap->present & PP_GROUP_GUID))
  {
    DDS_LOG(DDS_LC_DISCOVERY, " no guid?\n");
    return;
  }
  DDS_LOG(DDS_LC_DISCOVERY, " %x:%x:%x:%x\n", PGUID (datap->group_guid));
  delete_proxy_group (&datap->group_guid, timestamp, 0);
}

static void handle_SEDP_GROUP (const struct receiver_state *rst, nn_wctime_t timestamp, unsigned statusinfo, const void *vdata, unsigned len)
{
  const struct CDRHeader *data = vdata; /* built-ins not deserialized (yet) */
  DDS_LOG(DDS_LC_DISCOVERY, "SEDP_GROUP ST%x", statusinfo);
  if (data == NULL)
  {
    DDS_LOG(DDS_LC_DISCOVERY, " no payload?\n");
    return;
  }
  else
  {
    nn_plist_t decoded_data;
    nn_plist_src_t src;
    int plist_ret;
    src.protocol_version = rst->protocol_version;
    src.vendorid = rst->vendor;
    src.encoding = data->identifier;
    src.buf = (unsigned char *) data + 4;
    src.bufsz = len - 4;
    if ((plist_ret = nn_plist_init_frommsg (&decoded_data, NULL, ~(uint64_t)0, ~(uint64_t)0, &src)) < 0)
    {
      if (plist_ret != ERR_INCOMPATIBLE)
        DDS_WARNING("SEDP_GROUP (vendor %u.%u): invalid qos/parameters\n", src.vendorid.id[0], src.vendorid.id[1]);
      return;
    }

    switch (statusinfo & (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER))
    {
      case 0:
        handle_SEDP_GROUP_alive (&decoded_data, timestamp);
        break;

      case NN_STATUSINFO_DISPOSE:
      case NN_STATUSINFO_UNREGISTER:
      case (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER):
        handle_SEDP_GROUP_dead (&decoded_data, timestamp);
        break;
    }

    nn_plist_fini (&decoded_data);
  }
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
    buf = os_malloc (sz);
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

int builtins_dqueue_handler (const struct nn_rsample_info *sampleinfo, const struct nn_rdata *fragchain, UNUSED_ARG (const nn_guid_t *rdguid), UNUSED_ARG (void *qarg))
{
  struct proxy_writer *pwr;
  struct {
    struct CDRHeader cdr;
    nn_parameter_t p_endpoint_guid;
    char kh[16];
    nn_parameter_t p_sentinel;
  } keyhash_payload;
  unsigned statusinfo;
  int need_keyhash;
  nn_guid_t srcguid;
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
  data_smhdr_flags = normalize_data_datafrag_flags (&msg->smhdr, config.buggy_datafrag_flags_mode);
  srcguid.prefix = sampleinfo->rst->src_guid_prefix;
  srcguid.entityid = msg->writerId;

  pwr = sampleinfo->pwr;
  if (pwr == NULL)
    assert (srcguid.entityid.u == NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER);
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
    int plist_ret;
    src.protocol_version = sampleinfo->rst->protocol_version;
    src.vendorid = sampleinfo->rst->vendor;
    src.encoding = (msg->smhdr.flags & SMFLAG_ENDIANNESS) ? PL_CDR_LE : PL_CDR_BE;
    src.buf = NN_RMSG_PAYLOADOFF (fragchain->rmsg, qos_offset);
    src.bufsz = NN_RDATA_PAYLOAD_OFF (fragchain) - qos_offset;
    if ((plist_ret = nn_plist_init_frommsg (&qos, NULL, PP_STATUSINFO | PP_KEYHASH, 0, &src)) < 0)
    {
      if (plist_ret != ERR_INCOMPATIBLE)
        DDS_WARNING("data(builtin, vendor %u.%u): %x:%x:%x:%x #%"PRId64": invalid inline qos\n",
                    src.vendorid.id[0], src.vendorid.id[1], PGUID (srcguid), sampleinfo->seq);
      goto done_upd_deliv;
    }
    /* Complex qos bit also gets set when statusinfo bits other than
       dispose/unregister are set.  They are not currently defined,
       but this may save us if they do get defined one day. */
    statusinfo = (qos.present & PP_STATUSINFO) ? qos.statusinfo : 0;
  }

  if (pwr && ut_avlIsEmpty (&pwr->readers))
  {
    /* Wasn't empty when enqueued, but needn't still be; SPDP has no
       proxy writer, and is always accepted */
    goto done_upd_deliv;
  }

  /* Built-ins still do their own deserialization (SPDP <=> pwr ==
     NULL)). */
  assert (pwr == NULL || pwr->c.topic == NULL);
  if (statusinfo == 0)
  {
    if (datasz == 0 || !(data_smhdr_flags & DATA_FLAG_DATAFLAG))
    {
      DDS_WARNING("data(builtin, vendor %u.%u): %x:%x:%x:%x #%"PRId64": "
                   "built-in data but no payload\n",
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
      DDS_WARNING("data(builtin, vendor %u.%u): %x:%x:%x:%x #%"PRId64": "
                   "dispose/unregister of built-in data but payload not just key\n",
                   sampleinfo->rst->vendor.id[0], sampleinfo->rst->vendor.id[1],
                   PGUID (srcguid), sampleinfo->seq);
      goto done_upd_deliv;
    }
  }
  else if ((qos.present & PP_KEYHASH) && !NN_STRICT_P)
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
      keyhash_payload.cdr.identifier = PLATFORM_IS_LITTLE_ENDIAN ? PL_CDR_LE : PL_CDR_BE;
      keyhash_payload.cdr.options = 0;
      switch (srcguid.entityid.u)
      {
        case NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER:
        case NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_WRITER:
          pid = PID_PARTICIPANT_GUID;
          break;
        case NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_WRITER:
        case NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_WRITER:
          pid = PID_GROUP_GUID;
          break;
        case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER:
        case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER:
          pid = PID_ENDPOINT_GUID;
          break;
        case NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER:
        case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER:
          /* placeholders */
          pid = PID_ENDPOINT_GUID;
          break;
        default:
          DDS_LOG(DDS_LC_DISCOVERY, "data(builtin, vendor %u.%u): %x:%x:%x:%x #%"PRId64": mapping keyhash to ENDPOINT_GUID",
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
    DDS_WARNING("data(builtin, vendor %u.%u): %x:%x:%x:%x #%"PRId64": "
                 "dispose/unregister with no content\n",
                 sampleinfo->rst->vendor.id[0], sampleinfo->rst->vendor.id[1],
                 PGUID (srcguid), sampleinfo->seq);
    goto done_upd_deliv;
  }

  timestamp = valid_ddsi_timestamp(sampleinfo->timestamp) ? nn_wctime_from_ddsi_time(sampleinfo->timestamp): now();
  switch (srcguid.entityid.u)
  {
    case NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER:
      handle_SPDP (sampleinfo->rst, timestamp, statusinfo, datap, datasz);
      break;
    case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER:
    case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER:
      handle_SEDP (sampleinfo->rst, timestamp, statusinfo, datap, datasz);
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER:
      handle_PMD (sampleinfo->rst, timestamp, statusinfo, datap, datasz);
      break;
    case NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_WRITER:
      handle_SEDP_CM (sampleinfo->rst, srcguid.entityid, timestamp, statusinfo, datap, datasz);
      break;
    case NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_WRITER:
    case NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_WRITER:
      handle_SEDP_GROUP (sampleinfo->rst, timestamp, statusinfo, datap, datasz);
      break;
    default:
      DDS_LOG (DDS_LC_DISCOVERY, "data(builtin, vendor %u.%u): %x:%x:%x:%x #%"PRId64": not handled\n",
               sampleinfo->rst->vendor.id[0], sampleinfo->rst->vendor.id[1],
               PGUID (srcguid), sampleinfo->seq);
      break;
  }

 done_upd_deliv:
  if (needs_free)
    os_free (datap);
  if (pwr)
  {
    /* No proxy writer for SPDP */
    os_atomic_st32 (&pwr->next_deliv_seq_lowword, (uint32_t) (sampleinfo->seq + 1));
  }
  return 0;
}
