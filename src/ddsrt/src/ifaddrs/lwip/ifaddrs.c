// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <string.h>
#include <lwip/inet.h>
#include <lwip/netif.h> /* netif_list */
#include <lwip/sockets.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/string.h"

extern const int *const os_supp_afs;

static uint32_t
getflags(
  const struct netif *netif,
  const ip_addr_t *addr)
{
  uint32_t flags = 0;

  if (netif->flags & NETIF_FLAG_UP) {
    flags |= IFF_UP;
  }
  if (netif->flags & NETIF_FLAG_BROADCAST) {
    flags |= IFF_BROADCAST;
  }
  if (netif->flags & NETIF_FLAG_IGMP) {
    flags |= IFF_MULTICAST;
  }
  if (ip_addr_isloopback(addr)) {
    flags |= IFF_LOOPBACK;
  }

  return flags;
}

static void
sockaddr_from_ip_addr(
  struct sockaddr *sockaddr,
  const ip_addr_t *addr)
{
  if (IP_IS_V4(addr)) {
    memset(sockaddr, 0, sizeof(struct sockaddr_in));
    ((struct sockaddr_in *)sockaddr)->sin_len = sizeof(struct sockaddr_in);
    ((struct sockaddr_in *)sockaddr)->sin_family = AF_INET;
    inet_addr_from_ip4addr(&((struct sockaddr_in *)sockaddr)->sin_addr, addr);
#if DDSRT_HAVE_IPV6
  } else {
    assert(IP_IS_V6(addr));
    memset(sockaddr, 0, sizeof(struct sockaddr_in6));
    ((struct sockaddr_in6 *)sockaddr)->sin6_len = sizeof(struct sockaddr_in6);
    ((struct sockaddr_in6 *)sockaddr)->sin6_family = AF_INET6;
    inet6_addr_from_ip6addr(&((struct sockaddr_in6 *)sockaddr)->sin6_addr, addr);
#endif
  }
}

static dds_return_t
copyaddr(
  ddsrt_ifaddrs_t **ifap,
  const struct netif *netif,
  const ip_addr_t *addr)
{
  dds_return_t rc = DDS_RETCODE_OK;
  ddsrt_ifaddrs_t *ifa;
  struct sockaddr_storage sa;

  assert(ifap != NULL);
  assert(netif != NULL);
  assert(addr != NULL);

  sockaddr_from_ip_addr((struct sockaddr *)&sa, addr);

  /* Network interface name is of the form "et0", where the first two letters
     are the "name" field and the digit is the num field of the netif
     structure as described in lwip/netif.h */

  if ((ifa = ddsrt_calloc_s(1, sizeof(*ifa))) == NULL ||
      (ifa->addr = ddsrt_memdup(&sa, sa.s2_len)) == NULL ||
      (ddsrt_asprintf(&ifa->name, "%s%d", netif->name, netif->num) == -1))
  {
    rc = DDS_RETCODE_OUT_OF_RESOURCES;
  } else {
    ifa->flags = getflags(netif, addr);
    ifa->index = netif->num;
    ifa->type = DDSRT_IFTYPE_UNKNOWN;

    if (IP_IS_V4(addr)) {
      static const size_t sz = sizeof(struct sockaddr_in);
      if ((ifa->netmask = ddsrt_calloc_s(1, sz)) == NULL ||
          (ifa->broadaddr = ddsrt_calloc_s(1, sz)) == NULL)
      {
        rc = DDS_RETCODE_OUT_OF_RESOURCES;
      } else {
        ip_addr_t broadaddr = IPADDR4_INIT(
          ip_2_ip4(&netif->ip_addr)->addr |
          ip_2_ip4(&netif->netmask)->addr);

        sockaddr_from_ip_addr((struct sockaddr*)ifa->netmask, &netif->netmask);
        sockaddr_from_ip_addr((struct sockaddr*)ifa->broadaddr, &broadaddr);
      }
    }
  }

  if (rc == DDS_RETCODE_OK) {
    *ifap = ifa;
  } else {
    ddsrt_freeifaddrs(ifa);
  }

  return rc;
}

dds_return_t
ddsrt_getifaddrs(
  ddsrt_ifaddrs_t **ifap,
  const int *afs)
{
  dds_return_t rc = DDS_RETCODE_OK;
  int use_ip4, use_ip6;
  struct netif *netif;
  ddsrt_ifaddrs_t *ifa, *next_ifa, *root_ifa;

  assert(ifap != NULL);

  if (afs == NULL) {
    afs = os_supp_afs;
  }

  use_ip4 = use_ip6 = 0;
  for (int i = 0; afs[i] != DDSRT_AF_TERM; i++) {
    if (afs[i] == AF_INET) {
      use_ip4 = 1;
    } else if (afs[i] == AF_INET6) {
      use_ip6 = 1;
    }
  }

  ifa = next_ifa = root_ifa = NULL;

  for (netif = netif_list;
       netif != NULL && rc == DDS_RETCODE_OK;
       netif = netif->next)
  {
    if (use_ip4 && IP_IS_V4(&netif->ip_addr)) {
      rc = copyaddr(&next_ifa, netif, &netif->ip_addr);
      if (rc == DDS_RETCODE_OK) {
        if (ifa == NULL) {
          ifa = root_ifa = next_ifa;
        } else {
          ifa->next = next_ifa;
          ifa = next_ifa;
        }
      }
    }

#if DDSRT_HAVE_IPV6
    if (use_ip6) {
      int pref = 1;
again:
      /* List preferred IPv6 address first. */
      for (int i = 0;
               i < LWIP_IPV6_NUM_ADDRESSES && rc == DDS_RETCODE_OK;
               i++)
      {
        if ((ip6_addr_ispreferred(netif->ip_addr_state[i]) &&  pref) ||
            (ip6_addr_isvalid(netif->ip_addr_state[i])     && !pref))
        {
          rc = copyaddr(&next_ifa, netif, &netif->ip_addr[i]);
          if (rc == DDS_RETCODE_OK) {
            if (ifa == NULL) {
              ifa = root_ifa = next_ifa;
            } else {
              ifa->next = next_ifa;
              ifa = next_ifa;
            }
          }
        }
      }

      if (rc == DDS_RETCODE_OK && pref) {
        pref = 0;
        goto again;
      }
    }
#endif
  }

  if (rc == DDS_RETCODE_OK) {
    *ifap = root_ifa;
  } else {
    ddsrt_freeifaddrs(root_ifa);
  }

  return rc;
}
