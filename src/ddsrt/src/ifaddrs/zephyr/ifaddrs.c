// Copyright(c) 2006 to 2023 ZettaScale Technology and others
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

#include <zephyr/net/net_if.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/string.h"

extern const int *const os_supp_afs;

struct ifaddrs_data {
  ddsrt_ifaddrs_t *first;
  ddsrt_ifaddrs_t *prev;
  int getv4;
  int getv6;
  dds_return_t rc;
};

static uint32_t
getflags(
  struct net_if *iface)
{
  uint32_t flags = 0;

  if (net_if_is_up(iface)) {
    flags |= IFF_UP;
  }
  if (net_if_flag_is_set(iface, NET_IF_POINTOPOINT)) {
    flags |= IFF_POINTOPOINT;
  }
  flags |= IFF_BROADCAST;
  flags |= IFF_MULTICAST;

#if defined(CONFIG_NET_LOOPBACK)
  if (net_if_l2(iface) == &NET_L2_GET_NAME(DUMMY)) {
    flags |= IFF_LOOPBACK;
  }
#endif

  return flags;
}

static void netif_callback(struct net_if *iface, void *cb_data)
{
  ddsrt_ifaddrs_t *ifa;
  struct ifaddrs_data *data = (struct ifaddrs_data*)cb_data;


  if ((data->rc != DDS_RETCODE_OK)
#if defined(CONFIG_NET_L2_ETHERNET) && defined(CONFIG_NET_L2_DUMMY)
    || ((net_if_l2(iface) != &NET_L2_GET_NAME(ETHERNET)) && (net_if_l2(iface) != &NET_L2_GET_NAME(DUMMY)))
#elif defined(CONFIG_NET_L2_ETHERNET)
    || (net_if_l2(iface) != &NET_L2_GET_NAME(ETHERNET))
#elif defined(CONFIG_NET_L2_DUMMY)
    || (net_if_l2(iface) != &NET_L2_GET_NAME(DUMMY))
#endif
    ) {
    /* Skip on previous error or unsupported interface type */
    return;
  }

  if (data->getv4 && iface->config.ip.ipv4) {
    struct net_if_ipv4 *cfg = iface->config.ip.ipv4;
    struct net_if_addr *addr = NULL;
    int i;
    for (i = 0; i < NET_IF_MAX_IPV4_ADDR && !addr; i++) {
      if (cfg->unicast[i].is_used &&
          cfg->unicast[i].addr_state == NET_ADDR_PREFERRED &&
          cfg->unicast[i].address.family == AF_INET) {
        addr = &cfg->unicast[i];
      }
    }

    ifa = ddsrt_calloc_s(1, sizeof(ddsrt_ifaddrs_t));
    if (!ifa) {
      data->rc = DDS_RETCODE_OUT_OF_RESOURCES;
    } else {
      ifa->name = ddsrt_strdup(iface->if_dev->dev->name);
      if (addr) {
        ifa->addr = ddsrt_calloc_s(1, sizeof(struct sockaddr_in));
        ifa->netmask = ddsrt_calloc_s(1, sizeof(struct sockaddr_in));
        ifa->broadaddr = ddsrt_calloc_s(1, sizeof(struct sockaddr_in));
      }
      if (!ifa->name || (addr && (!ifa->addr || !ifa->netmask || !ifa->broadaddr))) {
        data->rc = DDS_RETCODE_OUT_OF_RESOURCES;
      } else {
        ifa->type = DDSRT_IFTYPE_UNKNOWN;
        ifa->flags = getflags(iface);
        ifa->index = net_if_get_by_iface(iface);

        if (addr) {
          net_ipaddr_copy(&(net_sin(ifa->addr)->sin_addr), &(addr->address.in_addr));
          ifa->addr->sa_family = AF_INET;

          net_ipaddr_copy(&(net_sin(ifa->netmask)->sin_addr), &(cfg->netmask));
          ifa->netmask->sa_family = AF_INET;

          ((struct sockaddr_in*)ifa->broadaddr)->sin_addr.s_addr = (net_sin(ifa->addr)->sin_addr.s_addr & net_sin(ifa->netmask)->sin_addr.s_addr) | ~net_sin(ifa->netmask)->sin_addr.s_addr;
          ifa->broadaddr->sa_family = AF_INET;
        }
      }
    }

    if (data->rc == DDS_RETCODE_OK) {
      if (data->prev) {
        data->prev->next = ifa;
      } else {
        data->first = ifa;
      }
      data->prev = ifa;
    } else {
      ddsrt_freeifaddrs(ifa);
    }
  }

#if DDSRT_HAVE_IPV6
  if (data->getv6 && iface->config.ip.ipv6) {
    struct net_if_ipv6 *cfg = iface->config.ip.ipv6;
    struct net_if_addr *addr = NULL;
    int i;
    for (i = 0; i < NET_IF_MAX_IPV6_ADDR; i++) {
      if (cfg->unicast[i].is_used &&
          cfg->unicast[i].addr_state == NET_ADDR_PREFERRED &&
          cfg->unicast[i].address.family == AF_INET6 &&
          !net_ipv6_is_ll_addr(&cfg->unicast[i].address.in6_addr)) {
        addr = &cfg->unicast[i];
      }
    }

    ifa = ddsrt_calloc_s(1, sizeof(ddsrt_ifaddrs_t));
    if (!ifa) {
      data->rc = DDS_RETCODE_OUT_OF_RESOURCES;
    } else {
      ifa->name = ddsrt_strdup(iface->if_dev->dev->name);
      if (addr) {
        ifa->addr = ddsrt_calloc_s(1, sizeof(struct sockaddr_in6));
      }
      if (!ifa->name || (addr && (!ifa->addr))) {
        data->rc = DDS_RETCODE_OUT_OF_RESOURCES;
      } else {
        ifa->type = DDSRT_IFTYPE_UNKNOWN;
        ifa->flags = getflags(iface);
        ifa->index = net_if_get_by_iface(iface);

        if (addr) {
          net_ipaddr_copy(&(net_sin6(ifa->addr)->sin6_addr), &(addr->address.in6_addr));
          ifa->addr->sa_family = AF_INET6;
        }
      }
    }

    if (data->rc == DDS_RETCODE_OK) {
      if (data->prev) {
        data->prev->next = ifa;
      } else {
        data->first = ifa;
      }
      data->prev = ifa;
    } else {
      ddsrt_freeifaddrs(ifa);
    }
  }
#endif

  return;
}

dds_return_t
ddsrt_getifaddrs(
  ddsrt_ifaddrs_t **ifap,
  const int *afs)
{
  struct ifaddrs_data data;

  assert(ifap != NULL);

  data.first = NULL;
  data.prev = NULL;
  data.rc = DDS_RETCODE_OK;
  data.getv4 = 0;
  data.getv6 = 0;

  if (afs == NULL) {
    afs = os_supp_afs;
  }

  for (int i = 0; afs[i] != DDSRT_AF_TERM; i++) {
    if (afs[i] == AF_INET) {
      data.getv4 = 1;
    }
#if DDSRT_HAVE_IPV6
    else if (afs[i] == AF_INET6) {
      data.getv6 = 1;
    }
#endif
  }

  (void)net_if_foreach(netif_callback, &data);

  if (data.rc == DDS_RETCODE_OK) {
    *ifap = data.first;
  } else {
    ddsrt_freeifaddrs(data.first);
  }
  
  return data.rc;
}
