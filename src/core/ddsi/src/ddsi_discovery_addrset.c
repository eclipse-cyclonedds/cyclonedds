// Copyright(c) 2023 ZettaScale Technology and others
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
#include "dds/ddsrt/log.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__discovery_addrset.h"
#include "ddsi__participant.h"
#include "ddsi__tran.h"
#include "ddsi__addrset.h"

void ddsi_interface_set_init (ddsi_interface_set_t *intfs)
{
  for (size_t i = 0; i < sizeof (intfs->xs) / sizeof (intfs->xs[0]); i++)
    intfs->xs[i] = false;
}

static uint32_t allow_multicast_mask_from_locator (const struct ddsi_domaingv *gv, const ddsi_locator_t *loc)
{
  uint32_t mask = 0;
#ifdef DDS_HAS_SSM
  if (ddsi_is_ssm_mcaddr (gv, loc))
    mask |= DDSI_AMC_SSM;
  else if (ddsi_is_mcaddr (gv, loc))
    mask |= DDSI_AMC_ASM;
#else
  if (ddsi_is_mcaddr (gv, loc))
    mask |= DDSI_AMC_ASM;
#endif
  return mask;
}

bool ddsi_include_multicast_locator_in_discovery (const struct ddsi_domaingv *gv)
{
  const uint32_t mask = allow_multicast_mask_from_locator (gv, &gv->loc_default_mc);
  for (int i = 0; i < gv->n_interfaces; i++)
    if (gv->interfaces[i].allow_multicast & mask)
      return true;
  return false;
}

static void addrset_from_locatorlists_add_one (struct ddsi_domaingv const * const gv, const struct ddsi_network_packet_info *pktinfo, const ddsi_locator_t *loc, struct ddsi_addrset *as, ddsi_interface_set_t *intfs, bool *direct, struct ddsi_addrset *routed_as)
{
  size_t interf_idx;
  switch (ddsi_is_nearby_address (gv, loc, (size_t) gv->n_interfaces, gv->interfaces, &interf_idx))
  {
    case DNAR_SELF:
    case DNAR_LOCAL:
      // if it matches an interface, use that one and record that this is a
      // directly connected interface: those will then all be possibilities
      // for transmitting multicasts (assuming capable, allowed, &c.)
      assert (interf_idx < MAX_XMIT_CONNS);
      ddsi_add_xlocator_to_addrset (gv, as, &(const ddsi_xlocator_t) {
        .conn = gv->xmit_conns[interf_idx],
        .c = *loc });
      intfs->xs[interf_idx] = true;
      *direct = true;
      break;
    case DNAR_DISTANT:
      // If DONT_ROUTE is set and there is no matching interface, then presumably
      // one would not be able to reach this address.
      if (!gv->config.dontRoute)
      {
        // If we know on what interface we received the packet that we are now
        // processing and it matches one that we use, pick that.
        //
        // Else pick the first selected interface that isn't link-local or loopback
        // (maybe it matters, maybe not, but it doesn't make sense to assign
        // a transmit socket for a local interface to a distant host).  If none
        // exists, skip the address.
        int i = gv->n_interfaces;
        if (pktinfo->if_index != 0)
        {
          for (i = 0; i < gv->n_interfaces; i++)
          {
            if (gv->interfaces[i].if_index == pktinfo->if_index &&
                gv->interfaces[i].loc.kind == loc->kind)
              break;
          }
        }
        if (i == gv->n_interfaces)
        {
          // no obvious choice based on what we know of the packet ingress ...
          // do not use link-local or loopback interfaces transmit conn for distant nodes
          for (i = 0; i < gv->n_interfaces; i++)
          {
            if (!gv->interfaces[i].link_local && !gv->interfaces[i].loopback &&
                gv->interfaces[i].loc.kind == loc->kind)
              break;
          }
        }
        if (i < gv->n_interfaces)
        {
          ddsi_add_xlocator_to_addrset (gv, routed_as ? routed_as : as, &(const ddsi_xlocator_t) {
            .conn = gv->xmit_conns[i],
            .c = *loc });
        }
      }
      break;
    case DNAR_UNREACHABLE:
      break;
  }
}

static bool ddsi_addrset_from_locatorlist_allow_loopback (const struct ddsi_domaingv *gv, const ddsi_locators_t *uc)
{
  bool allow_loopback;
  {
    bool a = true;
    for (int i = 0; i < gv->n_interfaces && a; i++)
      if (!gv->interfaces[i].loopback)
        a = false;
    bool b = true;
    // FIXME: what about the cases where SEDP gives just a loopback address, but the proxypp is known to be on a remote node?
    for (struct ddsi_locators_one *l = uc->first; l != NULL && b; l = l->next)
      b = ddsi_is_loopbackaddr (gv, &l->loc);
    allow_loopback = (a || b);
  }

  // if any non-loopback address is identical to one of our own addresses (actual or advertised),
  // assume it is the same machine, in which case loopback addresses may be picked up
  for (struct ddsi_locators_one *l = uc->first; l != NULL && !allow_loopback; l = l->next)
  {
    if (ddsi_is_loopbackaddr (gv, &l->loc))
      continue;
    allow_loopback = (ddsi_is_nearby_address (gv, &l->loc, (size_t) gv->n_interfaces, gv->interfaces, NULL) == DNAR_SELF);
  }
  return allow_loopback;
}

static struct ddsi_addrset *ddsi_addrset_from_locatorlists_handle_uc (const struct ddsi_domaingv *gv, const struct ddsi_network_packet_info *pktinfo, bool allow_loopback, const ddsi_locators_t *uc, ddsi_interface_set_t *intfs, bool *direct)
{
  struct ddsi_addrset *as = ddsi_new_addrset ();

  // When we encounter addresses that do not match an interface we are using but that routable,
  // they temporarily get added to this second address set, to be used only if there are no
  // addresses that do match an interface
  struct ddsi_addrset *routed_as = ddsi_new_addrset ();

  *direct = false;
  ddsi_interface_set_init (intfs);
  for (struct ddsi_locators_one *l = uc->first; l != NULL; l = l->next)
  {
#if 0
    {
      char buf[DDSI_LOCSTRLEN];
      ddsi_locator_to_string_no_port (buf, sizeof (buf), &l->loc);
      GVTRACE("%s: ignore %d loopback %d\n", buf, l->loc.tran->m_ignore, ddsi_is_loopbackaddr (gv, &l->loc));
    }
#endif
    // skip unrecognized ones, as well as loopback ones if not on the same host
    if (!allow_loopback && ddsi_is_loopbackaddr (gv, &l->loc))
      continue;

    ddsi_locator_t loc = l->loc;

    // if the advertised locator matches our own external locator, than presumably
    // it is the same machine and should be addressed using the actual interface
    // address
    bool extloc_of_self = false;
    for (int i = 0; i < gv->n_interfaces; i++)
    {
      if (loc.kind == gv->interfaces[i].loc.kind && memcmp (loc.address, gv->interfaces[i].extloc.address, sizeof (loc.address)) == 0)
      {
        memcpy (loc.address, gv->interfaces[i].loc.address, sizeof (loc.address));
        extloc_of_self = true;
        break;
      }
    }

    if (!extloc_of_self && loc.kind == DDSI_LOCATOR_KIND_UDPv4 && gv->extmask.kind != DDSI_LOCATOR_KIND_INVALID)
    {
      /* If the examined locator is in the same subnet as our own
         external IP address, this locator will be translated into one
         in the same subnet as our own local ip and selected. */
      assert (gv->n_interfaces == 1); // gv->extmask: the hack is only supported if limited to a single interface
      struct in_addr tmp4 = *((struct in_addr *) (loc.address + 12));
      const struct in_addr ownip = *((struct in_addr *) (gv->interfaces[0].loc.address + 12));
      const struct in_addr extip = *((struct in_addr *) (gv->interfaces[0].extloc.address + 12));
      const struct in_addr extmask = *((struct in_addr *) (gv->extmask.address + 12));

      if ((tmp4.s_addr & extmask.s_addr) == (extip.s_addr & extmask.s_addr))
      {
        /* translate network part of the IP address from the external
           one to the internal one */
        tmp4.s_addr = (tmp4.s_addr & ~extmask.s_addr) | (ownip.s_addr & extmask.s_addr);
        memcpy (loc.address + 12, &tmp4, 4);
      }
    }

    addrset_from_locatorlists_add_one (gv, pktinfo, &loc, as, intfs, direct, routed_as);
  }

  if (ddsi_addrset_empty (as))
    ddsi_copy_addrset_into_addrset (gv, as, routed_as);
  ddsi_unref_addrset (routed_as);
  return as;
}

static void ddsi_addrset_from_locatorlists_intfs_fallback (const struct ddsi_domaingv *gv, const struct ddsi_addrset *as, const ddsi_interface_set_t *inherited_intfs, bool direct, ddsi_interface_set_t *intfs)
{
  if (ddsi_addrset_empty (as) && inherited_intfs)
  {
    // implies no interfaces enabled in "intfs" yet -- just use whatever
    // we inherited for the purposes of selecting multicast addresses
    assert (!direct);
    for (int i = 0; i < gv->n_interfaces; i++)
      assert (!intfs->xs[i]);
    //GVTRACE (" using-inherited-intfs");
    *intfs = *inherited_intfs;
  }
  else if (!direct && gv->config.multicast_ttl > 1)
  {
    //GVTRACE("assuming multicast routing works\n");
    // if not directly connected but multicast TTL allows routing,
    // assume any non-local interface will do
    //GVTRACE (" enabling-non-loopback/link-local");
    for (int i = 0; i < gv->n_interfaces; i++)
    {
      assert (!intfs->xs[i]);
      intfs->xs[i] = !(gv->interfaces[i].link_local || gv->interfaces[i].loopback);
    }
  }
}

static void ddsi_addrset_from_locatorlist_handle_mc (const struct ddsi_domaingv *gv, const ddsi_locators_t *mc, const ddsi_interface_set_t *intfs, struct ddsi_addrset *as)
{
#if 0
  GVTRACE("enabled interfaces for multicast:");
  for (int i = 0; i < gv->n_interfaces; i++)
  {
    if (intfs->xs[i])
      GVTRACE(" %s(%d)", gv->interfaces[i].name, gv->interfaces[i].mc_capable);
  }
  GVTRACE("\n");
#endif

  for (struct ddsi_locators_one *l = mc->first; l != NULL; l = l->next)
  {
    const uint32_t mask = allow_multicast_mask_from_locator (gv, &l->loc);
    for (int i = 0; i < gv->n_interfaces; i++)
    {
      if (intfs->xs[i] && // interface must be enabled for this peer
          (gv->interfaces[i].allow_multicast & mask) && // and must allow multicast
          ddsi_factory_supports (gv->xmit_conns[i]->m_factory, l->loc.kind))
      {
        const ddsi_xlocator_t loc = {
          .conn = gv->xmit_conns[i],
          .c = l->loc
        };
        ddsi_add_xlocator_to_addrset (gv, as, &loc);
      }
    }
  }
}

struct ddsi_addrset *ddsi_addrset_from_locatorlists (const struct ddsi_domaingv *gv, const ddsi_locators_t *uc, const ddsi_locators_t *mc, const struct ddsi_network_packet_info *pktinfo, bool allow_srcloc, const ddsi_interface_set_t *inherited_intfs)
{
  // if all interfaces are loopback, or all locators in uc are loopback, we're cool with loopback addresses
  const bool allow_loopback = ddsi_addrset_from_locatorlist_allow_loopback (gv, uc);
  //GVTRACE(" allow_loopback=%d\n", allow_loopback);

  // choose unicast locators looking at interfaces, keeping track of which interfaces seem to connect directly
  // the interfaces are needed to decide on which interfaces the multicast addresses are considered meaningful
  bool direct;
  ddsi_interface_set_t intfs;
  struct ddsi_addrset *as = ddsi_addrset_from_locatorlists_handle_uc (gv, pktinfo, allow_loopback, uc, &intfs, &direct);

  // if no addresses were picked yet but we have a suitable source locator, use that source locator
  if (ddsi_addrset_empty (as) && allow_srcloc && !ddsi_is_unspec_locator (&pktinfo->src))
  {
    // FIXME: conn_read should provide interface information in source address
    //GVTRACE (" add-srcloc");
    addrset_from_locatorlists_add_one (gv, pktinfo, &pktinfo->src, as, &intfs, &direct, NULL);
  }

  // if no decisions yet on suitable interfaces, fall back on inherited interfaces (if any), or else whatever
  // seems reasonable for the purposes of multicastinf
  ddsi_addrset_from_locatorlists_intfs_fallback (gv, as, inherited_intfs, direct, &intfs);

  // now that we have decided on interfaces, add multicast addresses if we think we can do something with them
  ddsi_addrset_from_locatorlist_handle_mc (gv, mc, &intfs, as);
  return as;
}
