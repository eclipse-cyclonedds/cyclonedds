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
#include "dds/ddsi/ddsi_config_impl.h"
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/q_addrset.h" /* unspec locator */
#include "dds/ddsi/q_feature_check.h"
#include "dds/ddsi/ddsi_ipaddr.h"
#include "dds/ddsrt/avl.h"

#ifdef __linux
/* FIMXE: HACK HACK */
#include <linux/if_packet.h>
#endif

enum find_interface_result {
  FIR_OK,
  FIR_NOTFOUND,
  FIR_INVALID
};

static enum find_interface_result find_interface_by_name (const char *reqname, size_t n_interfaces, const struct nn_interface *interfaces, size_t *match)
{
  // see if there's an interface with this name
  for (size_t k = 0; k < n_interfaces; k++)
  {
    if (strcmp (reqname, interfaces[k].name) == 0)
    {
      *match = k;
      return FIR_OK;
    }
  }
  return FIR_NOTFOUND;
}

static enum find_interface_result find_interface_by_address (const struct ddsi_domaingv *gv, const char *reqip, size_t n_interfaces, const struct nn_interface *interfaces, size_t *match)
{
  // try matching on address
  ddsi_locator_t req;
  if (ddsi_locator_from_string (gv, &req, reqip, gv->m_factory) != AFSR_OK)
  {
    return FIR_INVALID;
  }
  /* Try an exact match on the address */
  for (size_t k = 0; k < n_interfaces; k++)
  {
    if (compare_locators (&interfaces[k].loc, &req) == 0)
    {
      *match = k;
      return FIR_OK;
    }
  }

  /* For IPv4, try matching on network portion only, where the network portion
     is based on the netmask of the interface under consideration */
  if (req.kind == NN_LOCATOR_KIND_UDPv4)
  {
    for (size_t k = 0; k < n_interfaces; k++)
    {
      if (interfaces[k].loc.kind != NN_LOCATOR_KIND_UDPv4)
        continue;
      uint32_t req1, ip1, nm1;
      memcpy (&req1, req.address + 12, sizeof (req1));
      memcpy (&ip1, interfaces[k].loc.address + 12, sizeof (ip1));
      memcpy (&nm1, interfaces[k].netmask.address + 12, sizeof (nm1));

      /* If the host portion of the requested address is non-zero,
       skip this interface */
      if (req1 & ~nm1)
        continue;

      if ((req1 & nm1) == (ip1 & nm1))
      {
        *match = k;
        return FIR_OK;
      }
    }
  }
  return FIR_NOTFOUND;
}

struct interface_priority {
  size_t match;
  int32_t priority;
};

static int compare_interface_priority_t (const void *va, const void *vb)
{
  const struct interface_priority *a = va;
  const struct interface_priority *b = vb;
  return (a->priority == b->priority) ? 0 : (a->priority < b->priority) ? 1 : -1;
}


int find_own_ip (struct ddsi_domaingv *gv)
{
  const char *sep = " ";
  char last_if_name[80] = "";
  int quality = -1;
  ddsrt_ifaddrs_t *ifa, *ifa_root = NULL;
  size_t *maxq_list;
  size_t maxq_count = 0;
  char addrbuf[DDSI_LOCSTRLEN];

  size_t n_interfaces;
  size_t max_interfaces;
  struct nn_interface *interfaces;

  GVLOG (DDS_LC_CONFIG, "interfaces:");

  {
    int ret;
    ret = ddsi_enumerate_interfaces(gv->m_factory, gv->config.transport_selector, &ifa_root);
    if (ret < 0) {
      GVERROR ("ddsi_enumerate_interfaces(%s): %d\n", gv->m_factory->m_typename, ret);
      return 0;
    }
  }

  n_interfaces = 0;
  max_interfaces = 8;
  maxq_list = ddsrt_malloc (max_interfaces * sizeof (*maxq_list));
  interfaces = ddsrt_malloc (max_interfaces * sizeof (*interfaces));
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

    if (n_interfaces == max_interfaces)
    {
      max_interfaces *= 2;
      maxq_list = ddsrt_realloc (maxq_list, max_interfaces * sizeof (*maxq_list));
      interfaces = ddsrt_realloc (interfaces, max_interfaces * sizeof (*interfaces));
    }

    if (ddsi_locator_from_sockaddr (gv->m_factory, &interfaces[n_interfaces].loc, ifa->addr) < 0)
    {
      // pretend we didn't see it
      continue;
    }
    ddsi_locator_to_string_no_port(addrbuf, sizeof(addrbuf), &interfaces[n_interfaces].loc);
    GVLOG (DDS_LC_CONFIG, " %s(", addrbuf);

    bool link_local = false;
    bool loopback = false;
    if (ifa->flags & IFF_LOOPBACK)
    {
      /* Loopback device has the lowest priority of every interface
         available, because the other interfaces at least in principle
         allow communicating with other machines. */
      loopback = true;
      q += 0;
#if DDSRT_HAVE_IPV6
      if (ifa->addr->sa_family == AF_INET6 && IN6_IS_ADDR_LINKLOCAL (&((struct sockaddr_in6 *)ifa->addr)->sin6_addr))
        link_local = true;
      else
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
      if (ifa->addr->sa_family == AF_INET6 && IN6_IS_ADDR_LINKLOCAL (&((struct sockaddr_in6 *)ifa->addr)->sin6_addr))
        link_local = true;
      else
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
      maxq_list[maxq_count] = n_interfaces;
      maxq_count++;
    } else if (q > quality) {
      maxq_list[0] = n_interfaces;
      maxq_count = 1;
      quality = q;
    }

    if (ifa->addr->sa_family == AF_INET && ifa->netmask)
    {
      if (ddsi_locator_from_sockaddr (gv->m_factory, &interfaces[n_interfaces].netmask, ifa->netmask) < 0)
        continue;
    }
    else
    {
      interfaces[n_interfaces].netmask.kind = interfaces[n_interfaces].loc.kind;
      interfaces[n_interfaces].netmask.port = NN_LOCATOR_PORT_INVALID;
      memset(&interfaces[n_interfaces].netmask.address, 0, sizeof(interfaces[n_interfaces].netmask.address));
    }
    // Default external (i.e., advertised in discovery) address to the actual interface
    // address.  This can subsequently be overridden by the configuration.
    interfaces[n_interfaces].extloc = interfaces[n_interfaces].loc;
    interfaces[n_interfaces].mc_capable = ((ifa->flags & IFF_MULTICAST) != 0);
    interfaces[n_interfaces].mc_flaky = ((ifa->type == DDSRT_IFTYPE_WIFI) != 0);
    interfaces[n_interfaces].point_to_point = ((ifa->flags & IFF_POINTOPOINT) != 0);
    interfaces[n_interfaces].loopback = loopback ? 1 : 0;
    interfaces[n_interfaces].link_local = link_local ? 1 : 0;
    interfaces[n_interfaces].if_index = ifa->index;
    interfaces[n_interfaces].name = ddsrt_strdup (if_name);
    interfaces[n_interfaces].priority = loopback ? 2 : 0;
    interfaces[n_interfaces].prefer_multicast = 0;
    n_interfaces++;
  }
  GVLOG (DDS_LC_CONFIG, "\n");
  ddsrt_freeifaddrs (ifa_root);

#ifndef NDEBUG
  // assert that we can identify all interfaces with an `int`, so we can
  // then just cast the size_t's to int's
  assert (n_interfaces <= (size_t) INT_MAX);
  assert (maxq_count <= n_interfaces);
  for (size_t i = 0; i < maxq_count; i++)
    assert (maxq_list[i] < n_interfaces);
#endif

  bool ok = true;
  gv->n_interfaces = 0;

  // Obtain priority settings
  if (gv->config.network_interfaces != NULL)
  {
    size_t num_matches = 0;
    struct interface_priority *matches = ddsrt_malloc (n_interfaces * sizeof (*matches));

    struct ddsi_config_network_interface_listelem *iface = gv->config.network_interfaces;
    while (iface) {
      size_t match = SIZE_MAX;
      bool has_name = iface->cfg.name != NULL && iface->cfg.name[0] != '\0';
      bool has_address = iface->cfg.address != NULL && iface->cfg.address[0] != '\0';

      if (iface->cfg.automatic) {
        // Autoselect the most appropriate interface
        if (has_name || has_address) {
          GVERROR ("An autodetermined interface should not have its name or address property specified.\n");
          ok = false;
          break;
        } else if (maxq_count == 0) {
          GVERROR ("No appropriate interface to autoselect, cannot determine own ip.\n");
          ok = false;
          break;
        } else {
          match = maxq_list[0];
          ddsi_locator_to_string_no_port (addrbuf, sizeof(addrbuf), &interfaces[match].loc);
          GVLOG (DDS_LC_INFO, "determined %s (%s) as highest quality interface, selected for automatic interface.\n", interfaces[match].name, addrbuf);
        }
      }
      else if (has_name && has_address) {
        // If the user specified both name and ip they better match the same interface
        size_t name_match = SIZE_MAX;
        size_t address_match = SIZE_MAX;
        enum find_interface_result name_result = find_interface_by_name (iface->cfg.name, n_interfaces, interfaces, &name_match);
        enum find_interface_result address_result = find_interface_by_address (gv, iface->cfg.address, n_interfaces, interfaces, &address_match);

        if (name_result != FIR_OK || address_result != FIR_OK) {
          GVERROR ("%s/%s: does not match an available interface\n", iface->cfg.name, iface->cfg.address);
          ok = false;
        }
        else if (name_match != address_match) {
          GVERROR ("%s/%s: do not match the same interface\n", iface->cfg.name, iface->cfg.address);
          ok = false;
        }
        else {
          match = name_match;
        }
      } else if (has_name) {
        enum find_interface_result name_result = find_interface_by_name (iface->cfg.name, n_interfaces, interfaces, &match);

        if (name_result != FIR_OK) {
          GVERROR ("%s: does not match an available interface\n", iface->cfg.name);
          ok = false;
        }
      } else if (has_address) {
        enum find_interface_result address_result = find_interface_by_address (gv, iface->cfg.address, n_interfaces, interfaces, &match);

        if (address_result != FIR_OK) {
          GVERROR ("%s: does not match an available interface\n", iface->cfg.address);
          ok = false;
        }
      } else {
        GVERROR ("Nameless and address-less interface listed in interfaces.\n");
        ok = false;
      }

      if (match != SIZE_MAX && ok) {
        interfaces[match].prefer_multicast = (unsigned int) iface->cfg.prefer_multicast;

        if (!iface->cfg.priority.isdefault)
          interfaces[match].priority = iface->cfg.priority.value;

        if (!iface->cfg.multicast.isdefault) {
          if (interfaces[match].mc_capable && !iface->cfg.multicast.value) {
            GVLOG (DDS_LC_CONFIG, "disabling multicast on interface %s.", interfaces[match].name);
            interfaces[match].mc_capable = 0;
          }
          else if (!interfaces[match].mc_capable && iface->cfg.multicast.value) {
            GVLOG (DDS_LC_CONFIG, "assuming multicast capable interface %s.", interfaces[match].name);
            interfaces[match].mc_capable = 1;
          }
        }

        if (num_matches == MAX_XMIT_CONNS)
        {
          GVERROR ("too many interfaces specified\n");
          ok = false;
          break;
        }
        matches[num_matches].match = match;
        matches[num_matches].priority = interfaces[match].priority;
        num_matches++;
      }

      iface = iface->next;
    }

    if (ok) {
      if (num_matches == 0) {
        GVERROR ("No network interfaces selected\n");
        ok = false;
      } else {
        qsort (matches, num_matches, sizeof (*matches), compare_interface_priority_t);
        for (size_t i = 1; i < num_matches; i++) {
          if (matches[i].match == matches[i-1].match)
          {
            GVERROR ("%s: the same interface may not be selected twice\n", interfaces[matches[i].match].name);
            ok = false;
            break;
          }
        }
        if (ok) {
          for(size_t i = 0; i < num_matches; ++i) {
            gv->interfaces[gv->n_interfaces] = interfaces[matches[i].match];
            gv->interfaces[gv->n_interfaces].name = ddsrt_strdup (gv->interfaces[gv->n_interfaces].name);
            gv->n_interfaces++;
          }
        }
      }
    }

    free(matches);
  } else { /* if gv->config.network_interfaces == NULL */
    if (maxq_count > 1)
    {
      const size_t idx = maxq_list[0];
      ddsi_locator_to_string_no_port (addrbuf, sizeof(addrbuf), &interfaces[idx].loc);
      GVLOG (DDS_LC_INFO, "using network interface %s (%s) selected arbitrarily from: ", interfaces[idx].name, addrbuf);
      for (size_t i = 0; i < maxq_count; i++)
        GVLOG (DDS_LC_INFO, "%s%s", (i == 0) ? "" : ", ", interfaces[maxq_list[i]].name);
      GVLOG (DDS_LC_INFO, "\n");
    }

    if (maxq_count <= 0)
    {
      GVERROR ("failed to determine default own IP address\n");
      ok = false;
    }
    else
    {
      gv->n_interfaces = 1;
      gv->interfaces[0] = interfaces[maxq_list[0]];
      gv->interfaces[0].name = ddsrt_strdup (gv->interfaces[0].name);
    }
  }

  gv->using_link_local_intf = false;
  for (int i = 0; i < gv->n_interfaces; i++)
  {
    if (!gv->interfaces[i].link_local)
      continue;
    else if (!gv->using_link_local_intf)
      gv->using_link_local_intf = true;
    else
    {
      GVERROR ("multiple interfaces selected with at least one having a link-local address\n");
      ok = false;
      break;
    }
  }

  for (size_t i = 0; i < n_interfaces; i++)
    if (interfaces[i].name)
      ddsrt_free (interfaces[i].name);
  ddsrt_free (interfaces);
  ddsrt_free (maxq_list);

  if (!ok)
  {
    for (int i = 0; i < gv->n_interfaces; i++)
      ddsrt_free (gv->interfaces[i].name);
    gv->n_interfaces = 0;
    return 0;
  }

  assert (gv->n_interfaces > 0);
  assert (gv->n_interfaces <= MAX_XMIT_CONNS);

  GVLOG (DDS_LC_CONFIG, "selected interfaces: ");
  for (int i = 0; i < gv->n_interfaces; i++)
    GVLOG (DDS_LC_CONFIG, "%s%s (index %"PRIu32" priority %"PRId32")", (i == 0) ? "" : ", ", gv->interfaces[i].name, gv->interfaces[i].if_index, gv->interfaces[i].priority);
  GVLOG (DDS_LC_CONFIG, "\n");
  return 1;
}
