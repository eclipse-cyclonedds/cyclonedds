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

static int multicast_override(const char *ifname, const struct ddsi_config *config)
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

static size_t count_commas (const char *str)
{
  size_t n = 0;
  const char *comma = strchr (str, ',');
  while (comma)
  {
    n++;
    comma = strchr (comma + 1, ',');
  }
  return n;
}

static char **split_at_comma (const char *str, size_t *nwords)
{
  *nwords = count_commas (str) + 1;
  size_t strsize = strlen (str) + 1;
  char **ptrs = ddsrt_malloc (*nwords * sizeof (*ptrs) + strsize);
  char *copy = (char *) ptrs + *nwords * sizeof (*ptrs);
  memcpy (copy, str, strsize);
  size_t i = 0;
  ptrs[i++] = copy;
  char *comma = strchr (copy, ',');
  while (comma)
  {
    *comma++ = 0;
    ptrs[i++] = comma;
    comma = strchr (comma, ',');
  }
  assert (i == *nwords);
  return ptrs;
}

enum find_interface_result {
  FIR_OK,
  FIR_NOTFOUND,
  FIR_INVALID
};

static enum find_interface_result find_interface (const struct ddsi_domaingv *gv, const char *reqname, size_t n_interfaces, const struct nn_interface *interfaces, size_t *match)
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

  // if not, try matching on address
  ddsi_locator_t req;
  if (ddsi_locator_from_string (gv, &req, reqname, gv->m_factory) != AFSR_OK)
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

static int compare_size_t (const void *va, const void *vb)
{
  const size_t *a = va;
  const size_t *b = vb;
  return (*a == *b) ? 0 : (*a < *b) ? -1 : 1;
}

int find_own_ip (struct ddsi_domaingv *gv, const char *requested_address)
{
  const char *sep = " ";
  char last_if_name[80] = "";
  int quality = -1;
  ddsrt_ifaddrs_t *ifa, *ifa_root = NULL;
  size_t *maxq_list;
  size_t maxq_count = 0;
  size_t maxq_strlen = 0;
  char addrbuf[DDSI_LOCSTRLEN];

  size_t n_interfaces;
  size_t max_interfaces;
  struct nn_interface *interfaces;

  GVLOG (DDS_LC_CONFIG, "interfaces:");

  {
    int ret;
    ret = ddsi_enumerate_interfaces(gv->m_factory, &ifa_root);
    if (ret < 0) {
      GVERROR ("ddsi_enumerate_interfaces(%s): %d\n", gv->m_factory->m_typename, ret);
      return 0;
    }
  }

  n_interfaces = 0;
  max_interfaces = 8;
  maxq_list = ddsrt_malloc (max_interfaces * sizeof (*interfaces));
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

#if defined(__linux) && !LWIP_SOCKET
    if (ifa->addr->sa_family == AF_PACKET)
    {
      /* FIXME: weirdo warning warranted */
      ddsi_locator_t *l = &interfaces[n_interfaces].loc;
      l->tran = NULL;
      l->conn = NULL;
      l->kind = NN_LOCATOR_KIND_RAWETH;
      l->port = NN_LOCATOR_PORT_INVALID;
      memset(l->address, 0, 10);
      memcpy(l->address + 10, ((struct sockaddr_ll *)ifa->addr)->sll_addr, 6);
    }
    else
#endif
    {
      ddsi_ipaddr_to_loc(gv->m_factory, &interfaces[n_interfaces].loc, ifa->addr, gv->m_factory->m_kind);
    }
    ddsi_locator_to_string_no_port(addrbuf, sizeof(addrbuf), &interfaces[n_interfaces].loc);
    GVLOG (DDS_LC_CONFIG, " %s(", addrbuf);

    if (!(ifa->flags & IFF_MULTICAST) && multicast_override (if_name, &gv->config))
    {
      GVLOG (DDS_LC_CONFIG, "assume-mc:");
      ifa->flags |= IFF_MULTICAST;
    }

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
      maxq_strlen += 2 + strlen (if_name);
      maxq_count++;
    } else if (q > quality) {
      maxq_list[0] = n_interfaces;
      maxq_strlen += 2 + strlen (if_name);
      maxq_count = 1;
      quality = q;
    }

    if (ifa->addr->sa_family == AF_INET && ifa->netmask)
    {
      ddsi_ipaddr_to_loc(gv->m_factory, &interfaces[n_interfaces].netmask, ifa->netmask, gv->m_factory->m_kind);
    }
    else
    {
      interfaces[n_interfaces].netmask.kind = gv->m_factory->m_kind;
      interfaces[n_interfaces].netmask.port = NN_LOCATOR_PORT_INVALID;
      memset(&interfaces[n_interfaces].netmask.address, 0, sizeof(interfaces[n_interfaces].netmask.address));
    }
    interfaces[n_interfaces].mc_capable = ((ifa->flags & IFF_MULTICAST) != 0);
    interfaces[n_interfaces].mc_flaky = ((ifa->type == DDSRT_IFTYPE_WIFI) != 0);
    interfaces[n_interfaces].point_to_point = ((ifa->flags & IFF_POINTOPOINT) != 0);
    interfaces[n_interfaces].loopback = loopback ? 1 : 0;
    interfaces[n_interfaces].link_local = link_local ? 1 : 0;
    interfaces[n_interfaces].if_index = ifa->index;
    interfaces[n_interfaces].name = ddsrt_strdup (if_name);
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
  if (requested_address == NULL)
  {
    if (maxq_count > 1)
    {
      const size_t idx = maxq_list[0];
      ddsi_locator_to_string_no_port (addrbuf, sizeof(addrbuf), &interfaces[idx].loc);
      GVWARNING ("using network interface %s (%s) selected arbitrarily from: ", interfaces[idx].name, addrbuf);
      for (size_t i = 0; i < maxq_count; i++)
        GVWARNING ("%s%s", (i == 0) ? "" : ", ", interfaces[maxq_list[i]].name);
      GVWARNING ("\n");
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
  else
  {
    size_t nnames;
    char **reqnames = split_at_comma (requested_address, &nnames);
    size_t *selected = ddsrt_malloc (nnames * sizeof (*selected));
    for (size_t i = 0; i < nnames; i++)
    {
      size_t match = SIZE_MAX;
      switch (find_interface (gv, reqnames[i], n_interfaces, interfaces, &match))
      {
        case FIR_OK:
          if (gv->n_interfaces == MAX_XMIT_CONNS)
          {
            GVERROR ("too many interfaces specified\n");
            ok = false;
            break;
          }
          assert (match < n_interfaces);
          selected[gv->n_interfaces] = match;
          gv->interfaces[gv->n_interfaces] = interfaces[match];
          gv->interfaces[gv->n_interfaces].name = ddsrt_strdup (gv->interfaces[gv->n_interfaces].name);
          gv->n_interfaces++;
          break;
        case FIR_NOTFOUND:
        case FIR_INVALID:
          GVERROR ("%s: does not match an available interface supporting %s\n", reqnames[i], gv->m_factory->m_typename);
          ok = false;
          break;
      }
    }
    qsort (selected, (size_t) gv->n_interfaces, sizeof (*selected), compare_size_t);
    for (int i = 1; i < gv->n_interfaces; i++)
    {
      if (selected[i] == selected[i-1])
      {
        if (strcmp (reqnames[i-1], reqnames[i]) == 0)
          GVERROR ("%s: the same interface may not be selected twice\n", reqnames[i]);
        else
          GVERROR ("%s, %s: the same interface may not be selected twice\n", reqnames[i-1], reqnames[i]);
        ok = false;
        break;
      }
    }
    ddsrt_free (reqnames);
    ddsrt_free (selected);
  }

  gv->ipv6_link_local = false;
  for (int i = 0; i < gv->n_interfaces; i++)
  {
    if (!gv->interfaces[i].link_local)
      continue;
    else if (!gv->ipv6_link_local)
      gv->ipv6_link_local = true;
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
    GVLOG (DDS_LC_CONFIG, "%s%s (index %"PRIu32")", (i == 0) ? "" : ", ", gv->interfaces[i].name, gv->interfaces[i].if_index);
  GVLOG (DDS_LC_CONFIG, "\n");
  return 1;
}
