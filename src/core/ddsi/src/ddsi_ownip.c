// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

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
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_feature_check.h"
#include "dds/ddsi/ddsi_log.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_unused.h"
#include "ddsi__ownip.h"
#include "ddsi__misc.h"
#include "ddsi__addrset.h" /* unspec locator */
#include "ddsi__ipaddr.h"
#include "ddsi__tran.h"

#ifdef __linux
/* FIMXE: HACK HACK */
#include <linux/if_packet.h>
#endif

enum find_interface_result {
  FIR_OK,
  FIR_NOTFOUND,
  FIR_INVALID
};

static enum find_interface_result find_interface_by_name (const char *reqname, size_t n_interfaces, const struct ddsi_network_interface *interfaces, size_t *match)
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

static enum find_interface_result find_interface_by_address (const struct ddsi_domaingv *gv, const char *reqip, size_t n_interfaces, const struct ddsi_network_interface *interfaces, size_t *match)
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
    if (ddsi_compare_locators (&interfaces[k].loc, &req) == 0)
    {
      *match = k;
      return FIR_OK;
    }
  }

  /* For IPv4, try matching on network portion only, where the network portion
     is based on the netmask of the interface under consideration */
  if (req.kind == DDSI_LOCATOR_KIND_UDPv4)
  {
    for (size_t k = 0; k < n_interfaces; k++)
    {
      if (interfaces[k].loc.kind != DDSI_LOCATOR_KIND_UDPv4)
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

static int compare_interface_priority (const void *va, const void *vb)
{
  // compare function is used for sorting in descending priority order
  const struct interface_priority *a = va;
  const struct interface_priority *b = vb;
  return (a->priority == b->priority) ? 0 : (a->priority < b->priority) ? 1 : -1;
}

enum maybe_add_interface_result {
  MAI_IGNORED,
  MAI_ADDED,
  MAI_OUT_OF_MEMORY
};

static enum maybe_add_interface_result maybe_add_interface (struct ddsi_domaingv * const gv, struct ddsi_network_interface *dst, const ddsrt_ifaddrs_t *ifa, int *qout)
{
  // returns quality >= 0 if added, < 0 otherwise
  char addrbuf[DDSI_LOCSTRLEN];
  int q = 0;

  /* interface must be up */
  if ((ifa->flags & IFF_UP) == 0) {
    GVLOG (DDS_LC_CONFIG, " (interface down)");
    return MAI_IGNORED;
  } else if (ddsrt_sockaddr_isunspecified(ifa->addr)) {
    GVLOG (DDS_LC_CONFIG, " (address unspecified)");
    return MAI_IGNORED;
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

  if (ddsi_locator_from_sockaddr (gv->m_factory, &dst->loc, ifa->addr) < 0)
    return MAI_IGNORED;
  ddsi_locator_to_string_no_port(addrbuf, sizeof(addrbuf), &dst->loc);
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

  if (ifa->addr->sa_family == AF_INET && ifa->netmask)
  {
    if (ddsi_locator_from_sockaddr (gv->m_factory, &dst->netmask, ifa->netmask) < 0)
      return MAI_IGNORED;
  }
  else
  {
    dst->netmask.kind = dst->loc.kind;
    dst->netmask.port = DDSI_LOCATOR_PORT_INVALID;
    memset(&dst->netmask.address, 0, sizeof(dst->netmask.address));
  }
  // Default external (i.e., advertised in discovery) address to the actual interface
  // address.  This can subsequently be overridden by the configuration.
  dst->extloc = dst->loc;
  dst->mc_capable = ((ifa->flags & IFF_MULTICAST) != 0);
  dst->mc_flaky = ((ifa->type == DDSRT_IFTYPE_WIFI) != 0);
  dst->point_to_point = ((ifa->flags & IFF_POINTOPOINT) != 0);
  dst->loopback = loopback ? 1 : 0;
  dst->link_local = link_local ? 1 : 0;
  dst->if_index = ifa->index;
  if ((dst->name = ddsrt_strdup (ifa->name)) == NULL)
    return MAI_OUT_OF_MEMORY;
  dst->priority = loopback ? 2 : 0;
  dst->prefer_multicast = 0;
  *qout = q;
  return MAI_ADDED;
}

static bool gather_interfaces (struct ddsi_domaingv * const gv, size_t *n_interfaces, struct ddsi_network_interface **interfaces, size_t *maxq_count, size_t **maxq_list)
{
  ddsrt_ifaddrs_t *ifa_root = NULL;
  const int ret = ddsi_enumerate_interfaces(gv->m_factory, gv->config.transport_selector, &ifa_root);
  if (ret < 0) {
    GVERROR ("failed to enumerate interfaces for \"%s\": %d\n", gv->m_factory->m_typename, ret);
    return false;
  }

  *maxq_count = 0;
  *n_interfaces = 0;
  size_t max_interfaces = 8;
  *interfaces = NULL;
  if ((*maxq_list = ddsrt_malloc (max_interfaces * sizeof (**maxq_list))) == NULL ||
      (*interfaces = ddsrt_malloc (max_interfaces * sizeof (**interfaces))) == NULL)
  {
    goto fail;
  }
  const char *last_if_name = "";
  const char *sep = " ";
  int quality = -1;
  for (ddsrt_ifaddrs_t *ifa = ifa_root; ifa != NULL; ifa = ifa->next)
  {
    if (strcmp (ifa->name, last_if_name))
      GVLOG (DDS_LC_CONFIG, "%s%s", sep, ifa->name);
    last_if_name = ifa->name;

    if (*n_interfaces == max_interfaces)
    {
      max_interfaces *= 2;
      size_t *new_maxq_list;
      if ((new_maxq_list = ddsrt_realloc (*maxq_list, max_interfaces * sizeof (**maxq_list))) == NULL)
        goto fail;
      *maxq_list = new_maxq_list;
      struct ddsi_network_interface *new_interfaces;
      if ((new_interfaces = ddsrt_realloc (*interfaces, max_interfaces * sizeof (**interfaces))) == NULL)
        goto fail;
      *interfaces = new_interfaces;
    }

    int q;
    switch (maybe_add_interface (gv, &(*interfaces)[*n_interfaces], ifa, &q))
    {
      case MAI_IGNORED:
        break;
      case MAI_OUT_OF_MEMORY:
        goto fail;
      case MAI_ADDED:
        if (q == quality) {
          (*maxq_list)[(*maxq_count)++] = *n_interfaces;
        } else if (q > quality) {
          (*maxq_list)[0] = *n_interfaces;
          *maxq_count = 1;
          quality = q;
        }
        (*n_interfaces)++;
        break;
    }
  }
  GVLOG (DDS_LC_CONFIG, "\n");
  ddsrt_freeifaddrs (ifa_root);
  assert ((*n_interfaces > 0) == (*maxq_count > 0));
  if (*n_interfaces == 0)
  {
    GVERROR ("failed to find interfaces for \"%s\"\n", gv->m_factory->m_typename);
    goto fail;
  }
  return true;

fail:
  ddsrt_free (*maxq_list);
  ddsrt_free (*interfaces);
  return false;
}

static bool match_config_interface (struct ddsi_domaingv * const gv, size_t n_interfaces, struct ddsi_network_interface const * const interfaces, const struct ddsi_config_network_interface_listelem *iface, size_t *match, bool required)
{
  const bool has_name = iface->cfg.name != NULL && iface->cfg.name[0] != '\0';
  const bool has_address = iface->cfg.address != NULL && iface->cfg.address[0] != '\0';
  *match = SIZE_MAX;
  if (has_name && has_address) {
    // If the user specified both name and ip they better match the same interface
    size_t name_match = SIZE_MAX;
    size_t address_match = SIZE_MAX;
    enum find_interface_result name_result = find_interface_by_name (iface->cfg.name, n_interfaces, interfaces, &name_match);
    enum find_interface_result address_result = find_interface_by_address (gv, iface->cfg.address, n_interfaces, interfaces, &address_match);

    if (name_result != FIR_OK || address_result != FIR_OK) {
      if (required) {
        GVERROR ("%s/%s: does not match an available interface\n", iface->cfg.name, iface->cfg.address);
        return false;
      }

      GVWARNING ("%s/%s: optional interface was not found.\n", iface->cfg.name, iface->cfg.address);
      return true;
    }
    else if (name_match != address_match) {
      GVERROR ("%s/%s: do not match the same interface\n", iface->cfg.name, iface->cfg.address);
      return false;
    }
    else {
      *match = name_match;
    }
  } else if (has_name) {
    enum find_interface_result name_result = find_interface_by_name (iface->cfg.name, n_interfaces, interfaces, match);
    if (name_result != FIR_OK) {
      if (required) {
        GVERROR ("%s: does not match an available interface.\n", iface->cfg.name);
        return false;
      }
      GVWARNING ("%s: optional interface was not found.\n", iface->cfg.name);
      return true;
    }
  } else if (has_address) {
    enum find_interface_result address_result = find_interface_by_address (gv, iface->cfg.address, n_interfaces, interfaces, match);
    if (address_result != FIR_OK) {
      if (required) {
        GVERROR ("%s: does not match an available interface\n", iface->cfg.address);
        return false;
      }
      GVWARNING ("%s: optional interface was not found.\n", iface->cfg.address);
      return true;
    }
  } else {
    GVERROR ("Nameless and address-less interface listed in interfaces.\n");
    return false;
  }
  return true;
}

static bool add_matching_interface (struct ddsi_domaingv *gv, struct interface_priority *matches, size_t *num_matches, struct ddsi_network_interface *act_iface, size_t xx_idx, const struct ddsi_config_network_interface_listelem *cfg_iface)
{
  for (size_t i = 0; i < *num_matches; i++) {
    if (matches[i].match == xx_idx) {
      GVERROR ("%s: the same interface may not be selected twice\n", act_iface->name);
      return false;
    }
  }

  act_iface->prefer_multicast = ((unsigned) cfg_iface->cfg.prefer_multicast) & 1;

  if (!cfg_iface->cfg.priority.isdefault)
    act_iface->priority = cfg_iface->cfg.priority.value;

  if (cfg_iface->cfg.multicast != DDSI_BOOLDEF_DEFAULT) {
    if (act_iface->mc_capable && cfg_iface->cfg.multicast == DDSI_BOOLDEF_FALSE) {
      GVLOG (DDS_LC_CONFIG, "disabling multicast on interface %s.", act_iface->name);
      act_iface->mc_capable = 0;
    }
    else if (!act_iface->mc_capable && cfg_iface->cfg.multicast == DDSI_BOOLDEF_TRUE) {
      GVLOG (DDS_LC_CONFIG, "assuming multicast capable interface %s.", act_iface->name);
      act_iface->mc_capable = 1;
    }
  }

  if (*num_matches == MAX_XMIT_CONNS)
  {
    GVERROR ("too many interfaces specified\n");
    return false;
  }
  matches[*num_matches].match = xx_idx;
  matches[*num_matches].priority = act_iface->priority;
  (*num_matches)++;
  return true;
}

static void log_arbitrary_selection (struct ddsi_domaingv *gv, const struct ddsi_network_interface *interfaces, const size_t *maxq_list, size_t maxq_count)
{
  char addrbuf[DDSI_LOCSTRLEN];
  const size_t idx = maxq_list[0];
  ddsi_locator_to_string_no_port (addrbuf, sizeof(addrbuf), &interfaces[idx].loc);
  GVLOG (DDS_LC_INFO, "using network interface %s (%s) selected arbitrarily from: ", interfaces[idx].name, addrbuf);
  for (size_t i = 0; i < maxq_count; i++)
    GVLOG (DDS_LC_INFO, "%s%s", (i == 0) ? "" : ", ", interfaces[maxq_list[i]].name);
  GVLOG (DDS_LC_INFO, "\n");
}

int ddsi_find_own_ip (struct ddsi_domaingv *gv)
{
  char addrbuf[DDSI_LOCSTRLEN];

  GVLOG (DDS_LC_CONFIG, "interfaces:");

  size_t n_interfaces, maxq_count, *maxq_list;
  struct ddsi_network_interface *interfaces;
  if (!gather_interfaces (gv, &n_interfaces, &interfaces, &maxq_count, &maxq_list))
    return 0;
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

  assert (n_interfaces > 0 && maxq_count > 0);
  if (gv->config.network_interfaces == NULL)
  {
    if (maxq_count > 1)
      log_arbitrary_selection (gv, interfaces, maxq_list, maxq_count);
    gv->n_interfaces = 1;
    gv->interfaces[0] = interfaces[maxq_list[0]];
    if ((gv->interfaces[0].name = ddsrt_strdup (gv->interfaces[0].name)) == NULL)
      ok = false;
  }
  else // Obtain priority settings
  {
    size_t num_matches = 0;
    size_t maxq_index = 0;
    struct interface_priority *matches = ddsrt_malloc (n_interfaces * sizeof (*matches));
    if (matches == NULL)
      ok = false;
    for (struct ddsi_config_network_interface_listelem *iface = gv->config.network_interfaces; iface && ok; iface = iface->next)
    {
      size_t match = SIZE_MAX;
      bool has_name = iface->cfg.name != NULL && iface->cfg.name[0] != '\0';
      bool has_address = iface->cfg.address != NULL && iface->cfg.address[0] != '\0';
      if (!iface->cfg.automatic) {
        ok = match_config_interface(gv, n_interfaces, interfaces, iface, &match, iface->cfg.presence_required);
      } else if (has_name || has_address) {
        GVERROR ("An autodetermined interface should not have its name or address property specified.\n");
        ok = false;
      } else if (maxq_index == maxq_count) {
        GVERROR ("No appropriate interface remaining for autoselect.\n");
        ok = false;
      } else {
        match = maxq_list[maxq_index++];
        ddsi_locator_to_string_no_port (addrbuf, sizeof(addrbuf), &interfaces[match].loc);
        GVLOG (DDS_LC_INFO, "determined %s (%s) as highest quality interface, selected for automatic interface.\n", interfaces[match].name, addrbuf);
      }
      if (match != SIZE_MAX && ok) {
        ok = add_matching_interface (gv, matches, &num_matches, &interfaces[match], match, iface);
      }
    }
    if (num_matches == 0) {
      // gv->config.network_interfaces not empty, match_... and add_matching_... print on error
      // so must be silent here
      ok = false;
    } else {
      qsort (matches, num_matches, sizeof (*matches), compare_interface_priority);
      for(size_t i = 0; i < num_matches && ok; ++i) {
        gv->interfaces[gv->n_interfaces] = interfaces[matches[i].match];
        if ((gv->interfaces[gv->n_interfaces].name = ddsrt_strdup (gv->interfaces[gv->n_interfaces].name)) == NULL)
          ok = false;
        gv->n_interfaces++;
      }
    }
    ddsrt_free(matches);
  }

  gv->using_link_local_intf = false;
  for (int i = 0; i < gv->n_interfaces && ok; i++)
  {
    if (!gv->interfaces[i].link_local)
      continue;
    else if (!gv->using_link_local_intf)
      gv->using_link_local_intf = true;
    else
    {
      GVERROR ("multiple interfaces selected with at least one having a link-local address\n");
      ok = false;
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
      if (gv->interfaces[i].name)
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
