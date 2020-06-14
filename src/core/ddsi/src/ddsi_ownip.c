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
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/sockets.h"

#include "dds/ddsi/q_log.h"
#include "dds/ddsi/ddsi_ownip.h"

#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/q_addrset.h" /* unspec locator */
#include "dds/ddsi/q_feature_check.h"
#include "dds/ddsi/ddsi_ipaddr.h"
#include "dds/ddsrt/avl.h"

static int multicast_override(const char *ifname, const struct config *config)
{
  char *copy = ddsrt_strdup (config->assumeMulticastCapable), *cursor = copy, *tok;
  int match = 0;
  if (copy != NULL)
  {
    while ((tok = ddsrt_strsep (&cursor, ",")) != NULL)
    {
      if (ddsi2_patmatch (tok, ifname))
        match = 1;
    }
  }
  ddsrt_free (copy);
  return match;
}

#ifdef __linux
/* FIMXE: HACK HACK */
#include <linux/if_packet.h>
#endif

int find_own_ip (struct ddsi_domaingv *gv, const char *requested_address)
{
  const char *sep = " ";
  char last_if_name[80] = "";
  int quality = -1;
  int i;
  ddsrt_ifaddrs_t *ifa, *ifa_root = NULL;
  int maxq_list[MAX_INTERFACES];
  int maxq_count = 0;
  size_t maxq_strlen = 0;
  int selected_idx = -1;
  char addrbuf[DDSI_LOCSTRLEN];

  GVLOG (DDS_LC_CONFIG, "interfaces:");

  {
    int ret;
    ret = ddsi_enumerate_interfaces(gv->m_factory, gv->config.transport_selector, &ifa_root);
    if (ret < 0) {
      GVERROR ("ddsi_enumerate_interfaces(%s): %d\n", gv->m_factory->m_typename, ret);
      return 0;
    }
  }

  gv->n_interfaces = 0;
  last_if_name[0] = 0;
  for (ifa = ifa_root; ifa != NULL; ifa = ifa->next)
  {
    char if_name[sizeof (last_if_name)];
    int q = 0;

    (void) ddsrt_strlcpy(if_name, ifa->name, sizeof(if_name));

    if (strcmp (if_name, last_if_name))
      GVLOG (DDS_LC_CONFIG, "%s%s", sep, if_name);
    (void) ddsrt_strlcpy(last_if_name, if_name, sizeof(last_if_name));

    /* interface must be up */
    if ((ifa->flags & IFF_UP) == 0) {
      GVLOG (DDS_LC_CONFIG, " (interface down)");
      continue;
    } else if (ddsrt_sockaddr_isunspecified(ifa->addr)) {
      GVLOG (DDS_LC_CONFIG, " (address unspecified)");
      continue;
    }

    switch (ifa->type)
    {
      case DDSRT_IFTYPE_WIFI:
        GVLOG (DDS_LC_CONFIG, " wireless");
        break;
      case DDSRT_IFTYPE_WIRED:
        GVLOG (DDS_LC_CONFIG, " wired");
        break;
      case DDSRT_IFTYPE_UNKNOWN:
        break;
    }

#if defined(__linux) && !LWIP_SOCKET
    if (ifa->addr->sa_family == AF_PACKET)
    {
      /* FIXME: weirdo warning warranted */
      nn_locator_t *l = &gv->interfaces[gv->n_interfaces].loc;
      l->kind = NN_LOCATOR_KIND_RAWETH;
      l->port = NN_LOCATOR_PORT_INVALID;
      memset(l->address, 0, 10);
      memcpy(l->address + 10, ((struct sockaddr_ll *)ifa->addr)->sll_addr, 6);
    }
    else
#endif
    {
      ddsi_ipaddr_to_loc(gv->m_factory, &gv->interfaces[gv->n_interfaces].loc, ifa->addr, gv->m_factory->m_kind);
    }
    ddsi_locator_to_string_no_port(addrbuf, sizeof(addrbuf), &gv->interfaces[gv->n_interfaces].loc);
    GVLOG (DDS_LC_CONFIG, " %s(", addrbuf);

    if (!(ifa->flags & IFF_MULTICAST) && multicast_override (if_name, &gv->config))
    {
      GVLOG (DDS_LC_CONFIG, "assume-mc:");
      ifa->flags |= IFF_MULTICAST;
    }

    if (ifa->flags & IFF_LOOPBACK)
    {
      /* Loopback device has the lowest priority of every interface
         available, because the other interfaces at least in principle
         allow communicating with other machines. */
      q += 0;
#if DDSRT_HAVE_IPV6
      if (!(ifa->addr->sa_family == AF_INET6 && IN6_IS_ADDR_LINKLOCAL (&((struct sockaddr_in6 *)ifa->addr)->sin6_addr)))
        q += 1;
#endif
    }
    else
    {
#if DDSRT_HAVE_IPV6
      /* We accept link-local IPv6 addresses, but an interface with a
         link-local address will end up lower in the ordering than one
         with a global address.  When forced to use a link-local
         address, we restrict ourselves to operating on that one
         interface only and assume any advertised (incoming) link-local
         address belongs to that interface.  FIXME: this is wrong, and
         should be changed to tag addresses with the interface over
         which it was received.  But that means proper multi-homing
         support and has quite an impact in various places, not least of
         which is the abstraction layer. */
      if (!(ifa->addr->sa_family == AF_INET6 && IN6_IS_ADDR_LINKLOCAL (&((struct sockaddr_in6 *)ifa->addr)->sin6_addr)))
        q += 5;
#endif

      /* We strongly prefer a multicast capable interface, if that's
         not available anything that's not point-to-point, or else we
         hope IP routing will take care of the issues. */
      if (ifa->flags & IFF_MULTICAST)
        q += 4;
      else if (!(ifa->flags & IFF_POINTOPOINT))
        q += 3;
      else
        q += 2;
    }

    GVLOG (DDS_LC_CONFIG, "q%d)", q);
    if (q == quality) {
      maxq_list[maxq_count] = gv->n_interfaces;
      maxq_strlen += 2 + strlen (if_name);
      maxq_count++;
    } else if (q > quality) {
      maxq_list[0] = gv->n_interfaces;
      maxq_strlen += 2 + strlen (if_name);
      maxq_count = 1;
      quality = q;
    }

    if (ifa->addr->sa_family == AF_INET && ifa->netmask)
    {
      ddsi_ipaddr_to_loc(gv->m_factory, &gv->interfaces[gv->n_interfaces].netmask, ifa->netmask, gv->m_factory->m_kind);
    }
    else
    {
      gv->interfaces[gv->n_interfaces].netmask.kind = gv->m_factory->m_kind;
      gv->interfaces[gv->n_interfaces].netmask.port = NN_LOCATOR_PORT_INVALID;
      memset(&gv->interfaces[gv->n_interfaces].netmask.address, 0, sizeof(gv->interfaces[gv->n_interfaces].netmask.address));
    }
    gv->interfaces[gv->n_interfaces].mc_capable = ((ifa->flags & IFF_MULTICAST) != 0);
    gv->interfaces[gv->n_interfaces].mc_flaky = ((ifa->type == DDSRT_IFTYPE_WIFI) != 0);
    gv->interfaces[gv->n_interfaces].point_to_point = ((ifa->flags & IFF_POINTOPOINT) != 0);
    gv->interfaces[gv->n_interfaces].if_index = ifa->index;
    gv->interfaces[gv->n_interfaces].name = ddsrt_strdup (if_name);
    gv->n_interfaces++;
  }
  GVLOG (DDS_LC_CONFIG, "\n");
  ddsrt_freeifaddrs (ifa_root);

  if (requested_address == NULL)
  {
    if (maxq_count > 1)
    {
      const int idx = maxq_list[0];
      char *names;
      int p;
      ddsi_locator_to_string_no_port (addrbuf, sizeof(addrbuf), &gv->interfaces[idx].loc);
      names = ddsrt_malloc (maxq_strlen + 1);
      p = 0;
      for (i = 0; i < maxq_count && (size_t) p < maxq_strlen; i++)
        p += snprintf (names + p, maxq_strlen - (size_t) p, ", %s", gv->interfaces[maxq_list[i]].name);
      GVWARNING ("using network interface %s (%s) selected arbitrarily from: %s\n",
                 gv->interfaces[idx].name, addrbuf, names + 2);
      ddsrt_free (names);
    }

    if (maxq_count > 0)
      selected_idx = maxq_list[0];
    else
      GVERROR ("failed to determine default own IP address\n");
  }
  else
  {
    nn_locator_t req;
    /* Presumably an interface name */
    for (i = 0; i < gv->n_interfaces; i++)
    {
      if (strcmp (gv->interfaces[i].name, gv->config.networkAddressString) == 0)
        break;
    }
    if (i < gv->n_interfaces)
      ; /* got a match */
    else if (ddsi_locator_from_string(gv, &req, gv->config.networkAddressString, gv->m_factory) != AFSR_OK)
      ; /* not good, i = gv->n_interfaces, so error handling will kick in */
    else
    {
      /* Try an exact match on the address */
      for (i = 0; i < gv->n_interfaces; i++)
        if (compare_locators(&gv->interfaces[i].loc, &req) == 0)
          break;
      if (i == gv->n_interfaces && req.kind == NN_LOCATOR_KIND_UDPv4)
      {
        /* Try matching on network portion only, where the network
           portion is based on the netmask of the interface under
           consideration */
        for (i = 0; i < gv->n_interfaces; i++)
        {
          uint32_t req1, ip1, nm1;
          memcpy (&req1, req.address + 12, sizeof (req1));
          memcpy (&ip1, gv->interfaces[i].loc.address + 12, sizeof (ip1));
          memcpy (&nm1, gv->interfaces[i].netmask.address + 12, sizeof (nm1));

          /* If the host portion of the requested address is non-zero,
             skip this interface */
          if (req1 & ~nm1)
            continue;

          if ((req1 & nm1) == (ip1 & nm1))
            break;
        }
      }
    }

    if (i < gv->n_interfaces)
      selected_idx = i;
    else
      GVERROR ("%s: does not match an available interface supporting %s\n", gv->config.networkAddressString, gv->m_factory->m_typename);
  }

  if (selected_idx < 0)
    return 0;
  else
  {
    gv->ownloc = gv->interfaces[selected_idx].loc;
    gv->selected_interface = selected_idx;
    gv->interfaceNo = gv->interfaces[selected_idx].if_index;
#if DDSRT_HAVE_IPV6
    if (gv->extloc.kind == NN_LOCATOR_KIND_TCPv6 || gv->extloc.kind == NN_LOCATOR_KIND_UDPv6)
    {
      struct sockaddr_in6 addr;
      memcpy(&addr.sin6_addr, gv->ownloc.address, sizeof(addr.sin6_addr));
      gv->ipv6_link_local = IN6_IS_ADDR_LINKLOCAL (&addr.sin6_addr) != 0;
    }
    else
    {
      gv->ipv6_link_local = 0;
    }
#endif
    GVLOG (DDS_LC_CONFIG, "selected interface: %s (index %u)\n",
           gv->interfaces[selected_idx].name, gv->interfaceNo);

    return 1;
  }
}
