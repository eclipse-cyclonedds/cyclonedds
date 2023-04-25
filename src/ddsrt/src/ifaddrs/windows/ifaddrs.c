// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <errno.h>

#include "dds/ddsrt/sockets.h"

#include <iphlpapi.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/string.h"

extern const int *const os_supp_afs;

static dds_return_t
getifaces(PIP_ADAPTER_ADDRESSES *ptr)
{
  dds_return_t err = DDS_RETCODE_NOT_ENOUGH_SPACE;
  PIP_ADAPTER_ADDRESSES buf = NULL;
  ULONG bufsz = 0; /* Size is determined on first iteration. */
  ULONG ret;
  size_t i;

  static const size_t max = 2;
  static const ULONG filter = GAA_FLAG_INCLUDE_PREFIX |
                              GAA_FLAG_SKIP_ANYCAST |
                              GAA_FLAG_SKIP_MULTICAST |
                              GAA_FLAG_SKIP_DNS_SERVER;

  assert(ptr != NULL);

  for (i = 0; err == DDS_RETCODE_NOT_ENOUGH_SPACE && i < max; i++) {
    ret = GetAdaptersAddresses(AF_UNSPEC, filter, NULL, buf, &bufsz);
    assert(ret != ERROR_INVALID_PARAMETER);
    switch (ret) {
      case ERROR_BUFFER_OVERFLOW:
        err = DDS_RETCODE_NOT_ENOUGH_SPACE;
        ddsrt_free(buf);
        if ((buf = (IP_ADAPTER_ADDRESSES *)ddsrt_malloc(bufsz)) == NULL) {
          err = DDS_RETCODE_OUT_OF_RESOURCES;
        }
        break;
      case ERROR_NOT_ENOUGH_MEMORY:
        err = DDS_RETCODE_OUT_OF_RESOURCES;
        break;
      case ERROR_SUCCESS:
      case ERROR_ADDRESS_NOT_ASSOCIATED: /* No address associated yet. */
      case ERROR_NO_DATA: /* No adapters that match the filter. */
      default:
        err = DDS_RETCODE_OK;
        break;
    }
  }

  if (err == DDS_RETCODE_OK) {
    *ptr = buf;
  } else {
    ddsrt_free(buf);
  }

  return err;
}

static dds_return_t
getaddrtable(PMIB_IPADDRTABLE *ptr)
{
  dds_return_t err = DDS_RETCODE_NOT_ENOUGH_SPACE;
  PMIB_IPADDRTABLE buf = NULL;
  ULONG bufsz = 0;
  DWORD ret;
  size_t i;

  static const size_t max = 2;

  assert(ptr != NULL);

  for (i = 0; err == DDS_RETCODE_NOT_ENOUGH_SPACE && i < max; i++) {
    ret = GetIpAddrTable(buf, &bufsz, 0);
    assert(ret != ERROR_INVALID_PARAMETER &&
           ret != ERROR_NOT_SUPPORTED);
    switch (ret) {
      case ERROR_INSUFFICIENT_BUFFER:
        err = DDS_RETCODE_NOT_ENOUGH_SPACE;
        ddsrt_free(buf);
        if ((buf = (PMIB_IPADDRTABLE)ddsrt_malloc(bufsz)) == NULL) {
          err = DDS_RETCODE_OUT_OF_RESOURCES;
        }
        break;
      case NO_ERROR:
        err = DDS_RETCODE_OK;
        break;
      default:
        err = DDS_RETCODE_ERROR;
        break;
    }
  }

  if (err == 0) {
    *ptr = buf;
  } else {
    ddsrt_free(buf);
  }

  return err;
}

static uint32_t
getflags(const PIP_ADAPTER_ADDRESSES iface)
{
  uint32_t flags = 0;

  if (iface->OperStatus == IfOperStatusUp) {
    flags |= IFF_UP;
  }
  if (!(iface->Flags & IP_ADAPTER_NO_MULTICAST) &&
       (iface->IfType != IF_TYPE_SOFTWARE_LOOPBACK))
  {
    /* multicast over loopback doesn't seem to work despite the NO_MULTICAST
       flag being clear assuming an interface is multicast-capable when in fact
       it isn't is disastrous, so it makes more sense to err by assuming it
       won't work as there is always the General/Interfaces/NetworkInterface[@multicast] setting to
       overrule it */
    flags |= IFF_MULTICAST;
  }

  switch (iface->IfType) {
    case IF_TYPE_SOFTWARE_LOOPBACK:
      flags |= IFF_LOOPBACK;
      break;
    case IF_TYPE_ETHERNET_CSMACD:
    case IF_TYPE_IEEE80211:
    case IF_TYPE_IEEE1394:
    case IF_TYPE_ISO88025_TOKENRING:
      flags |= IFF_BROADCAST;
      break;
    default:
      flags |= IFF_POINTTOPOINT;
      break;
  }

  return flags;
}

static enum ddsrt_iftype
guess_iftype (const PIP_ADAPTER_ADDRESSES iface)
{
  switch (iface->IfType) {
    case IF_TYPE_IEEE80211:
      return DDSRT_IFTYPE_WIFI;
    case IF_TYPE_ETHERNET_CSMACD:
    case IF_TYPE_IEEE1394:
    case IF_TYPE_ISO88025_TOKENRING:
      return DDSRT_IFTYPE_WIRED;
    default:
      return DDSRT_IFTYPE_UNKNOWN;
  }
}

static dds_return_t
copyname(const wchar_t *wstr, char **strp)
{
  int cnt, len;
  char buf[1], *str;

  len = WideCharToMultiByte(
    CP_UTF8, WC_ERR_INVALID_CHARS, wstr, -1, buf, 0, NULL, NULL);
  if (len <= 0) {
    return DDS_RETCODE_BAD_PARAMETER;
  } else if ((str = ddsrt_malloc_s((size_t)len)) == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }

  cnt = WideCharToMultiByte(
    CP_UTF8, WC_ERR_INVALID_CHARS, wstr, -1, str, len, NULL, NULL);
  assert(cnt == len);
  assert(str[len - 1] == '\0');

  *strp = str;
  return DDS_RETCODE_OK;
}

static dds_return_t
copyaddr(
  ddsrt_ifaddrs_t **ifap,
  const PIP_ADAPTER_ADDRESSES iface,
  const PMIB_IPADDRTABLE addrtable,
  const PIP_ADAPTER_UNICAST_ADDRESS addr)
{
  dds_return_t rc = DDS_RETCODE_OK;
  ddsrt_ifaddrs_t *ifa;
  struct sockaddr *sa;
  size_t sz;

  assert(iface != NULL);
  assert(addrtable != NULL);
  assert(addr != NULL);

  sa = (struct sockaddr *)addr->Address.lpSockaddr;
  sz = (size_t)addr->Address.iSockaddrLength;

  if (!(ifa = ddsrt_calloc_s(1, sizeof(*ifa))))
    goto err_ifa;
  ifa->flags = getflags(iface);
  ifa->type = guess_iftype(iface);
  if (!(ifa->addr = ddsrt_memdup(sa, sz)))
    goto err_ifa_addr;
  if ((rc = copyname(iface->FriendlyName, &ifa->name)))
    goto err_ifa_name;

  if (ifa->addr->sa_family == AF_INET6) {
    ifa->index = iface->Ipv6IfIndex;

    /* Address is not in addrtable if the interface is not connected. */
  } else if (ifa->addr->sa_family == AF_INET && (ifa->flags & IFF_UP)) {
    DWORD i = 0;
    struct sockaddr_in nm, bc, *sin = (struct sockaddr_in *)sa;

    assert(sz == sizeof(nm));
    memset(&nm, 0, sz);
    memset(&bc, 0, sz);
    nm.sin_family = bc.sin_family = AF_INET;

    for (; i < addrtable->dwNumEntries;  i++) {
      if (sin->sin_addr.s_addr == addrtable->table[i].dwAddr) {
        ifa->index = addrtable->table[i].dwIndex;
        nm.sin_addr.s_addr = addrtable->table[i].dwMask;
        bc.sin_addr.s_addr = sin->sin_addr.s_addr | ~(nm.sin_addr.s_addr);
        break;
      }
    }

    assert(i < addrtable->dwNumEntries);
    if (!(ifa->netmask = ddsrt_memdup(&nm, sz)))
      goto err_ifa_netmask;
    if (!(ifa->broadaddr = ddsrt_memdup(&bc, sz)))
      goto err_ifa_broadaddr;
  }

  *ifap = ifa;
  return rc;
err_ifa:
err_ifa_addr:
err_ifa_netmask:
err_ifa_broadaddr:
  rc = DDS_RETCODE_OUT_OF_RESOURCES;
err_ifa_name:
  ddsrt_freeifaddrs(ifa);
  return rc;
}

dds_return_t
ddsrt_getifaddrs(
  ddsrt_ifaddrs_t **ifap,
  const int *afs)
{
  dds_return_t rc = DDS_RETCODE_OK;
  int use;
  PIP_ADAPTER_ADDRESSES ifaces = NULL, iface;
  PIP_ADAPTER_UNICAST_ADDRESS addr = NULL;
  PMIB_IPADDRTABLE addrtable = NULL;
  ddsrt_ifaddrs_t *ifa, *ifa_root, *ifa_next;
  struct sockaddr *sa;

  assert(ifap != NULL);

  if (afs == NULL) {
    afs = os_supp_afs;
  }

  ifa = ifa_root = ifa_next = NULL;

  if ((rc = getifaces(&ifaces)) == DDS_RETCODE_OK &&
      (rc = getaddrtable(&addrtable)) == DDS_RETCODE_OK)
  {
    for (iface = ifaces;
         iface != NULL && rc == DDS_RETCODE_OK;
         iface = iface->Next)
    {
      for (addr = iface->FirstUnicastAddress;
           addr != NULL && rc == DDS_RETCODE_OK;
           addr = addr->Next)
      {
        sa = (struct sockaddr *)addr->Address.lpSockaddr;
        use = 0;
        for (int i = 0; !use && afs[i] != DDSRT_AF_TERM; i++) {
          use = (afs[i] == sa->sa_family);
        }

        if (use) {
          rc = copyaddr(&ifa_next, iface, addrtable, addr);
          if (rc == DDS_RETCODE_OK) {
            if (ifa == NULL) {
              ifa = ifa_root = ifa_next;
            } else {
              ifa->next = ifa_next;
              ifa = ifa_next;
            }
          }
        }
      }
    }
  }

  ddsrt_free(ifaces);
  ddsrt_free(addrtable);

  if (rc == DDS_RETCODE_OK) {
    *ifap = ifa_root;
  } else {
    ddsrt_freeifaddrs(ifa_root);
  }

  return rc;
}

dds_return_t ddsrt_eth_get_mac_addr (char *interface_name, unsigned char *mac_addr)
{
  // Follow example from:
  // https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getadaptersaddresses?redirectedfrom=MSDN
  int ret = DDS_RETCODE_ERROR;
  
  PIP_ADAPTER_ADDRESSES interfaces = NULL, current_interface = NULL;
  if ((ret = getifaces(&interfaces)) != DDS_RETCODE_OK)
  {
    return ret;  
  }

  current_interface = interfaces;
  while (current_interface)
  {
    char converted_name[256];
    sprintf_s(converted_name, 256, "%ws", current_interface->FriendlyName);
    if (strcmp (converted_name, interface_name) == 0)
    {
      memcpy (mac_addr, current_interface->PhysicalAddress, 6);
      ret = DDS_RETCODE_OK;
      break;
    }
    current_interface = current_interface->Next;
  }
  ddsrt_free(interfaces);
  return ret;
}
