// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

//#include <ctype.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "dds/version.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__discovery.h"
#include "ddsi__discovery_addrset.h"
#include "ddsi__discovery_endpoint.h"
#include "ddsi__serdata_plist.h"
#include "ddsi__entity_index.h"
#include "ddsi__entity.h"
#include "ddsi__participant.h"
#include "ddsi__transmit.h"
#include "ddsi__lease.h"
#include "ddsi__security_omg.h"
#include "ddsi__endpoint.h"
#include "ddsi__plist.h"
#include "ddsi__proxy_endpoint.h"
#include "ddsi__tran.h"
#include "ddsi__vendor.h"
#include "ddsi__xqos.h"
#include "ddsi__addrset.h"

struct add_locator_to_ps_arg {
  struct ddsi_domaingv *gv;
  ddsi_plist_t *ps;
};

static void add_locator_to_ps (const ddsi_locator_t *loc, void *varg)
{
  struct add_locator_to_ps_arg *arg = varg;
  struct ddsi_locators_one *elem = ddsrt_malloc (sizeof (struct ddsi_locators_one));
  struct ddsi_locators *locs;
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

static void add_xlocator_to_ps (const ddsi_xlocator_t *loc, void *varg)
{
  add_locator_to_ps (&loc->c, varg);
}

#ifdef DDS_HAS_SHM
static void add_iox_locator_to_ps(const ddsi_locator_t* loc, struct add_locator_to_ps_arg *arg)
{
  struct ddsi_locators_one* elem = ddsrt_malloc(sizeof(struct ddsi_locators_one));
  struct ddsi_locators* locs = &arg->ps->unicast_locators;
  unsigned present_flag = PP_UNICAST_LOCATOR;

  elem->loc = *loc;
  elem->next = NULL;

  if (!(arg->ps->present & present_flag))
  {
    locs->n = 0;
    locs->first = locs->last = NULL;
    arg->ps->present |= present_flag;
  }

  //add iceoryx to the FRONT of the list of addresses, to indicate its higher priority
  if (locs->first)
    elem->next = locs->first;
  else
    locs->last = elem;
  locs->first = elem;
  locs->n++;
}
#endif

static int sedp_write_endpoint_impl
(
   struct ddsi_writer *wr, int alive, const ddsi_guid_t *guid,
   const struct ddsi_endpoint_common *epcommon,
   const dds_qos_t *xqos, struct ddsi_addrset *as, ddsi_security_info_t *security
#ifdef DDS_HAS_TYPE_DISCOVERY
   , const struct ddsi_sertype *sertype
#endif
)
{
  struct ddsi_domaingv * const gv = wr->e.gv;
  const dds_qos_t *defqos = NULL;
  if (ddsi_is_writer_entityid (guid->entityid))
    defqos = &ddsi_default_qos_writer;
  else if (ddsi_is_reader_entityid (guid->entityid))
    defqos = &ddsi_default_qos_reader;
  else
    assert (false);

  uint64_t qosdiff;
  ddsi_plist_t ps;

  ddsi_plist_init_empty (&ps);
  ps.present |= PP_ENDPOINT_GUID;
  ps.endpoint_guid = *guid;

#ifdef DDS_HAS_SECURITY
  if (security)
  {
    ps.present |= PP_ENDPOINT_SECURITY_INFO;
    memcpy(&ps.endpoint_security_info, security, sizeof(ddsi_security_info_t));
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
    ps.protocol_version.major = DDSI_RTPS_MAJOR;
    ps.protocol_version.minor = DDSI_RTPS_MINOR;
    ps.vendorid = DDSI_VENDORID_ECLIPSE;

    assert (epcommon != NULL);

    if (epcommon->group_guid.entityid.u != 0)
    {
      ps.present |= PP_GROUP_GUID;
      ps.group_guid = epcommon->group_guid;
    }

    if (!ddsi_is_writer_entityid (guid->entityid))
    {
      const struct ddsi_reader *rd = ddsi_entidx_lookup_reader_guid (gv->entity_index, guid);
      assert (rd);
      if (rd->request_keyhash)
      {
        ps.present |= PP_CYCLONE_REQUESTS_KEYHASH;
        ps.cyclone_requests_keyhash = 1u;
      }
    }

#ifdef DDS_HAS_SSM
    /* A bit of a hack -- the easy alternative would be to make it yet
    another parameter.  We only set "reader favours SSM" if we
    really do: no point in telling the world that everything is at
    the default. */
    if (ddsi_is_reader_entityid (guid->entityid))
    {
      const struct ddsi_reader *rd = ddsi_entidx_lookup_reader_guid (gv->entity_index, guid);
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
      qosdiff |= ~DDSI_QP_UNRECOGNIZED_INCOMPATIBLE_MASK;

    struct add_locator_to_ps_arg arg;
    arg.gv = gv;
    arg.ps = &ps;
    if (as)
      ddsi_addrset_forall (as, add_xlocator_to_ps, &arg);

#ifdef DDS_HAS_SHM
    assert(wr->xqos->present & DDSI_QP_LOCATOR_MASK);
    if (!(xqos->ignore_locator_type & DDSI_LOCATOR_KIND_SHEM))
    {
      if (!(arg.ps->present & PP_UNICAST_LOCATOR) || 0 == arg.ps->unicast_locators.n)
      {
        if (epcommon->pp->e.gv->config.many_sockets_mode == DDSI_MSM_MANY_UNICAST)
          add_locator_to_ps(&epcommon->pp->m_locator, &arg);
        else
        {
          // FIXME: same as what SPDP uses, should be refactored, now more than ever
          for (int i = 0; i < epcommon->pp->e.gv->n_interfaces; i++)
          {
            if (!epcommon->pp->e.gv->xmit_conns[i]->m_factory->m_enable_spdp)
            {
              // skip any interfaces where the address kind doesn't match the selected transport
              // as a reasonablish way of not advertising iceoryx locators here
              continue;
            }
            // FIXME: should have multiple loc_default_uc/loc_meta_uc or compute ports here
            ddsi_locator_t loc = epcommon->pp->e.gv->interfaces[i].extloc;
            loc.port = epcommon->pp->e.gv->loc_default_uc.port;
            add_locator_to_ps(&loc, &arg);
          }
        }
      }

      if (!(arg.ps->present & PP_MULTICAST_LOCATOR) || 0 == arg.ps->multicast_locators.n)
      {
        if (ddsi_include_multicast_locator_in_discovery (epcommon->pp->e.gv))
          add_locator_to_ps (&epcommon->pp->e.gv->loc_default_mc, &arg);
      }

      add_iox_locator_to_ps(&gv->loc_iceoryx_addr, &arg);
    }
#endif

#ifdef DDS_HAS_TYPE_DISCOVERY
    assert (sertype);
    if ((ps.qos.type_information = ddsi_sertype_typeinfo (sertype)))
      ps.qos.present |= DDSI_QP_TYPE_INFORMATION;
#endif
  }

  if (xqos)
    ddsi_xqos_mergein_missing (&ps.qos, xqos, qosdiff);
  return ddsi_write_and_fini_plist (wr, &ps, alive);
}

int ddsi_sedp_write_writer (struct ddsi_writer *wr)
{
  if ((!ddsi_is_builtin_entityid(wr->e.guid.entityid, DDSI_VENDORID_ECLIPSE)) && (!wr->e.onlylocal))
  {
    unsigned entityid = ddsi_determine_publication_writer(wr);
    struct ddsi_writer *sedp_wr = ddsi_get_sedp_writer (wr->c.pp, entityid);
    ddsi_security_info_t *security = NULL;
#ifdef DDS_HAS_SSM
    struct ddsi_addrset *as = wr->ssm_as;
#else
    struct ddsi_addrset *as = NULL;
#endif
#ifdef DDS_HAS_SECURITY
    ddsi_security_info_t tmp;
    if (ddsi_omg_get_writer_security_info (wr, &tmp))
    {
      security = &tmp;
    }
#endif
#ifdef DDS_HAS_TYPE_DISCOVERY
    return sedp_write_endpoint_impl (sedp_wr, 1, &wr->e.guid, &wr->c, wr->xqos, as, security, wr->type);
#else
    return sedp_write_endpoint_impl (sedp_wr, 1, &wr->e.guid, &wr->c, wr->xqos, as, security);
#endif
  }
  return 0;
}

int ddsi_sedp_write_reader (struct ddsi_reader *rd)
{
  if (ddsi_is_builtin_entityid (rd->e.guid.entityid, DDSI_VENDORID_ECLIPSE) || rd->e.onlylocal)
    return 0;

  unsigned entityid = ddsi_determine_subscription_writer(rd);
  struct ddsi_writer *sedp_wr = ddsi_get_sedp_writer (rd->c.pp, entityid);
  ddsi_security_info_t *security = NULL;
  struct ddsi_addrset *as = NULL;
#ifdef DDS_HAS_NETWORK_PARTITIONS
  if (rd->uc_as != NULL || rd->mc_as != NULL)
  {
    // FIXME: do this without first creating a temporary addrset
    as = ddsi_new_addrset ();
    // use a placeholder connection to avoid exploding the multicast addreses to multiple
    // interfaces
    for (const struct ddsi_networkpartition_address *a = rd->uc_as; a != NULL; a = a->next)
      ddsi_add_xlocator_to_addrset(rd->e.gv, as, &(const ddsi_xlocator_t) {
        .c = a->loc,
        .conn = rd->e.gv->xmit_conns[0] });
    for (const struct ddsi_networkpartition_address *a = rd->mc_as; a != NULL; a = a->next)
      ddsi_add_xlocator_to_addrset(rd->e.gv, as, &(const ddsi_xlocator_t) {
        .c = a->loc,
        .conn = rd->e.gv->xmit_conns[0] });
  }
#endif
#ifdef DDS_HAS_SECURITY
  ddsi_security_info_t tmp;
  if (ddsi_omg_get_reader_security_info (rd, &tmp))
  {
    security = &tmp;
  }
#endif
#ifdef DDS_HAS_TYPE_DISCOVERY
  const int ret = sedp_write_endpoint_impl (sedp_wr, 1, &rd->e.guid, &rd->c, rd->xqos, as, security, rd->type);
#else
  const int ret = sedp_write_endpoint_impl (sedp_wr, 1, &rd->e.guid, &rd->c, rd->xqos, as, security);
#endif
  ddsi_unref_addrset (as);
  return ret;
}

int ddsi_sedp_dispose_unregister_writer (struct ddsi_writer *wr)
{
  if ((!ddsi_is_builtin_entityid(wr->e.guid.entityid, DDSI_VENDORID_ECLIPSE)) && (!wr->e.onlylocal))
  {
    unsigned entityid = ddsi_determine_publication_writer(wr);
    struct ddsi_writer *sedp_wr = ddsi_get_sedp_writer (wr->c.pp, entityid);
#ifdef DDS_HAS_TYPE_DISCOVERY
    return sedp_write_endpoint_impl (sedp_wr, 0, &wr->e.guid, NULL, NULL, NULL, NULL, NULL);
#else
    return sedp_write_endpoint_impl (sedp_wr, 0, &wr->e.guid, NULL, NULL, NULL, NULL);
#endif
  }
  return 0;
}

int ddsi_sedp_dispose_unregister_reader (struct ddsi_reader *rd)
{
  if ((!ddsi_is_builtin_entityid(rd->e.guid.entityid, DDSI_VENDORID_ECLIPSE)) && (!rd->e.onlylocal))
  {
    unsigned entityid = ddsi_determine_subscription_writer(rd);
    struct ddsi_writer *sedp_wr = ddsi_get_sedp_writer (rd->c.pp, entityid);
#ifdef DDS_HAS_TYPE_DISCOVERY
    return sedp_write_endpoint_impl (sedp_wr, 0, &rd->e.guid, NULL, NULL, NULL, NULL, NULL);
#else
    return sedp_write_endpoint_impl (sedp_wr, 0, &rd->e.guid, NULL, NULL, NULL, NULL);
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

struct ddsi_addrset_from_locatorlists_collect_interfaces_arg {
  const struct ddsi_domaingv *gv;
  ddsi_interface_set_t *intfs;
};

/** @brief Figure out which interfaces are touched by (extended) locator @p loc
 *
 * Does this by looking up the connection in @p loc in the set of transmit connections. (There's plenty of room for optimisation here.)
 *
 * @param[in] loc locator
 * @param[in] varg argument pointer, must point to a struct ddsi_addrset_from_locatorlists_collect_interfaces_arg
 */
static void addrset_from_locatorlists_collect_interfaces (const ddsi_xlocator_t *loc, void *varg)
{
  struct ddsi_addrset_from_locatorlists_collect_interfaces_arg *arg = varg;
  struct ddsi_domaingv const * const gv = arg->gv;
  for (int i = 0; i < gv->n_interfaces; i++)
  {
    //GVTRACE(" {%p,%p}", loc->conn, gv->xmit_conns[i]);
    if (loc->conn == gv->xmit_conns[i])
    {
      arg->intfs->xs[i] = true;
      break;
    }
  }
}

struct ddsi_addrset *ddsi_get_endpoint_addrset (const struct ddsi_domaingv *gv, const ddsi_plist_t *datap, struct ddsi_addrset *proxypp_as_default, const ddsi_locator_t *rst_srcloc)
{
  const ddsi_locators_t emptyset = { .n = 0, .first = NULL, .last = NULL };
  const ddsi_locators_t *uc = (datap->present & PP_UNICAST_LOCATOR) ? &datap->unicast_locators : &emptyset;
  const ddsi_locators_t *mc = (datap->present & PP_MULTICAST_LOCATOR) ? &datap->multicast_locators : &emptyset;
  ddsi_locator_t srcloc;
  if (rst_srcloc == NULL)
    ddsi_set_unspec_locator (&srcloc);
  else // force use of source locator
  {
    uc = &emptyset;
    srcloc = *rst_srcloc;
  }

  // any interface that works for the participant is presumed ok
  ddsi_interface_set_t intfs;
  ddsi_interface_set_init (&intfs);
  ddsi_addrset_forall (proxypp_as_default, addrset_from_locatorlists_collect_interfaces, &(struct ddsi_addrset_from_locatorlists_collect_interfaces_arg){
    .gv = gv, .intfs = &intfs
  });
  //GVTRACE(" {%d%d%d%d}", intfs.xs[0], intfs.xs[1], intfs.xs[2], intfs.xs[3]);
  struct ddsi_addrset *as = ddsi_addrset_from_locatorlists (gv, uc, mc, &srcloc, &intfs);
  // if SEDP gives:
  // - no addresses, use ppant uni- and multicast addresses
  // - only multicast, use those for multicast and use ppant address for unicast
  // - only unicast, use only those (i.e., disable multicast for this reader)
  // - both, use only those
  // FIXME: then you can't do a specific unicast address + SSM ... oh well
  if (ddsi_addrset_empty (as))
    ddsi_copy_addrset_into_addrset_mc (gv, as, proxypp_as_default);
  if (ddsi_addrset_empty_uc (as))
    ddsi_copy_addrset_into_addrset_uc (gv, as, proxypp_as_default);
  return as;
}

void ddsi_handle_sedp_alive_endpoint (const struct ddsi_receiver_state *rst, ddsi_seqno_t seq, ddsi_plist_t *datap /* note: potentially modifies datap */, ddsi_sedp_kind_t sedp_kind, const ddsi_guid_prefix_t *src_guid_prefix, ddsi_vendorid_t vendorid, ddsrt_wctime_t timestamp)
{
#define E(msg, lbl) do { GVLOGDISC (msg); goto lbl; } while (0)
  struct ddsi_domaingv * const gv = rst->gv;
  struct ddsi_proxy_participant *proxypp;
  struct ddsi_proxy_writer * pwr = NULL;
  struct ddsi_proxy_reader * prd = NULL;
  ddsi_guid_t ppguid;
  dds_qos_t *xqos;
  int reliable;
  struct ddsi_addrset *as;
#ifdef DDS_HAS_SSM
  int ssm;
#endif

  assert (datap);
  assert (datap->present & PP_ENDPOINT_GUID);
  GVLOGDISC (" "PGUIDFMT, PGUID (datap->endpoint_guid));

  if (!ddsi_handle_sedp_checks (gv, sedp_kind, &datap->endpoint_guid, datap, src_guid_prefix, vendorid, timestamp, &proxypp, &ppguid))
    goto err;

  xqos = &datap->qos;
  if (sedp_kind == SEDP_KIND_READER)
    ddsi_xqos_mergein_missing (xqos, &ddsi_default_qos_reader, ~(uint64_t)0);
  else if (sedp_kind == SEDP_KIND_WRITER)
  {
    ddsi_xqos_mergein_missing (xqos, &ddsi_default_qos_writer, ~(uint64_t)0);
    if (!ddsi_vendor_is_eclipse_or_adlink (vendorid))
    {
      // there is a difference in interpretation of autodispose between vendors
      xqos->writer_data_lifecycle.autodispose_unregistered_instances = 0;
    }
  }
  else
    E (" invalid entity kind\n", err);

  /* After copy + merge, should have at least the ones present in the
     input.  Also verify reliability and durability are present,
     because we explicitly read those. */
  assert ((xqos->present & datap->qos.present) == datap->qos.present);
  assert (xqos->present & DDSI_QP_RELIABILITY);
  assert (xqos->present & DDSI_QP_DURABILITY);
  reliable = (xqos->reliability.kind == DDS_RELIABILITY_RELIABLE);

  GVLOGDISC (" %s %s %s %s: %s%s.%s/%s",
             reliable ? "reliable" : "best-effort",
             durability_to_string (xqos->durability.kind),
             sedp_kind == SEDP_KIND_WRITER ? "writer" : "reader",
             (xqos->present & DDSI_QP_ENTITY_NAME) ? xqos->entity_name : "unnamed",
             ((!(xqos->present & DDSI_QP_PARTITION) || xqos->partition.n == 0 || *xqos->partition.strs[0] == '\0')
              ? "(default)" : xqos->partition.strs[0]),
             ((xqos->present & DDSI_QP_PARTITION) && xqos->partition.n > 1) ? "+" : "",
             xqos->topic_name, xqos->type_name);

  if (sedp_kind == SEDP_KIND_READER && (datap->present & PP_EXPECTS_INLINE_QOS) && datap->expects_inline_qos)
    E ("******* AARGH - it expects inline QoS ********\n", err);

  ddsi_omg_log_endpoint_protection (gv, datap);
  if (ddsi_omg_is_endpoint_protected (datap) && !ddsi_omg_proxy_participant_is_secure (proxypp))
    E (" remote endpoint is protected while local federation is not secure\n", err);

  if (sedp_kind == SEDP_KIND_WRITER)
    pwr = ddsi_entidx_lookup_proxy_writer_guid (gv->entity_index, &datap->endpoint_guid);
  else
    prd = ddsi_entidx_lookup_proxy_reader_guid (gv->entity_index, &datap->endpoint_guid);
  if (pwr || prd)
  {
    /* Re-bind the proxy participant to the discovery service - and do this if it is currently
       bound to another DS instance, because that other DS instance may have already failed and
       with a new one taking over, without our noticing it. */
    GVLOGDISC (" known%s", ddsi_vendor_is_cloud (vendorid) ? "-DS" : "");
    if (ddsi_vendor_is_cloud (vendorid) && proxypp->implicitly_created && memcmp (&proxypp->privileged_pp_guid.prefix, src_guid_prefix, sizeof(proxypp->privileged_pp_guid.prefix)) != 0)
    {
      GVLOGDISC (" "PGUIDFMT" attach-to-DS "PGUIDFMT, PGUID(proxypp->e.guid), PGUIDPREFIX(*src_guid_prefix), proxypp->privileged_pp_guid.entityid.u);
      ddsrt_mutex_lock (&proxypp->e.lock);
      proxypp->privileged_pp_guid.prefix = *src_guid_prefix;
      ddsi_lease_set_expiry (proxypp->lease, DDSRT_ETIME_NEVER);
      ddsrt_mutex_unlock (&proxypp->e.lock);
    }
    GVLOGDISC ("\n");
  }
  else
  {
    GVLOGDISC (" NEW");
  }

  as = ddsi_get_endpoint_addrset (gv, datap, proxypp->as_default, gv->config.tcp_use_peeraddr_for_unicast ? &rst->srcloc : NULL);
  if (ddsi_addrset_empty (as))
  {
    ddsi_unref_addrset (as);
    E (" no address", err);
  }

  ddsi_log_addrset(gv, DDS_LC_DISCOVERY, " (as", as);
#ifdef DDS_HAS_SSM
  ssm = 0;
  if (sedp_kind == SEDP_KIND_WRITER)
    ssm = ddsi_addrset_contains_ssm (gv, as);
  else if (datap->present & PP_READER_FAVOURS_SSM)
    ssm = (datap->reader_favours_ssm.state != 0);
  GVLOGDISC (" ssm=%u", ssm);
#endif
  GVLOGDISC (") QOS={");
  ddsi_xqos_log (DDS_LC_DISCOVERY, &gv->logconfig, xqos);
  GVLOGDISC ("}\n");

  if ((datap->endpoint_guid.entityid.u & DDSI_ENTITYID_SOURCE_MASK) == DDSI_ENTITYID_SOURCE_VENDOR && !ddsi_vendor_is_eclipse_or_adlink (vendorid))
  {
    GVLOGDISC ("ignoring vendor-specific endpoint "PGUIDFMT"\n", PGUID (datap->endpoint_guid));
  }
  else
  {
    if (sedp_kind == SEDP_KIND_WRITER)
    {
      if (pwr)
        ddsi_update_proxy_writer (pwr, seq, as, xqos, timestamp);
      else
      {
        /* not supposed to get here for built-in ones, so can determine the channel based on the transport priority */
        assert (!ddsi_is_builtin_entityid (datap->endpoint_guid.entityid, vendorid));
        ddsi_new_proxy_writer (gv, &ppguid, &datap->endpoint_guid, as, datap, gv->user_dqueue, gv->xevents, timestamp, seq);
      }
    }
    else
    {
      if (prd)
        ddsi_update_proxy_reader (prd, seq, as, xqos, timestamp);
      else
      {
#ifdef DDS_HAS_SSM
        ddsi_new_proxy_reader (gv, &ppguid, &datap->endpoint_guid, as, datap, timestamp, seq, ssm);
#else
        ddsi_new_proxy_reader (gv, &ppguid, &datap->endpoint_guid, as, datap, timestamp, seq);
#endif
      }
    }
  }
  ddsi_unref_addrset (as);

err:
  return;
#undef E
}

void ddsi_handle_sedp_dead_endpoint (const struct ddsi_receiver_state *rst, ddsi_plist_t *datap, ddsi_sedp_kind_t sedp_kind, ddsrt_wctime_t timestamp)
{
  struct ddsi_domaingv * const gv = rst->gv;
  int res = -1;
  assert (datap->present & PP_ENDPOINT_GUID);
  GVLOGDISC (" "PGUIDFMT" ", PGUID (datap->endpoint_guid));
  if (!ddsi_check_sedp_kind_and_guid (sedp_kind, &datap->endpoint_guid))
    return;
  else if (sedp_kind == SEDP_KIND_WRITER)
    res = ddsi_delete_proxy_writer (gv, &datap->endpoint_guid, timestamp, 0);
  else
    res = ddsi_delete_proxy_reader (gv, &datap->endpoint_guid, timestamp, 0);
  GVLOGDISC (" %s\n", (res < 0) ? " unknown" : " delete");
}
