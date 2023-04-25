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
#include <errno.h>
#include <ifaddrs.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/string.h"

#if __APPLE__
  #include <TargetConditionals.h>
#endif

extern const int *const os_supp_afs;

#if defined __linux
#include <stdio.h>

static enum ddsrt_iftype guess_iftype (const struct ifaddrs *sys_ifa)
{
  FILE *fp;
  char ifnam[IFNAMSIZ + 1]; /* not sure whether IFNAMSIZ includes a terminating 0, can't be bothered */
  int c;
  size_t np;
  enum ddsrt_iftype type = DDSRT_IFTYPE_UNKNOWN;
  if ((fp = fopen ("/proc/net/wireless", "r")) == NULL)
    return type;
  /* expected format:
     :Inter-| sta-|   Quality        |   Discarded packets               | Missed | WE
     : face | tus | link level noise |  nwid  crypt   frag  retry   misc | beacon | 22
     : wlan0: 0000   67.  -43.  -256        0      0      0      0      0        0
     (where : denotes the start of the line)

     SKIP_HEADER_1 skips up to and including the first newline; then SKIP_TO_EOL skips
     up to and including the second newline, so the first line that gets interpreted is
     the third.
   */
  enum { SKIP_HEADER_1, SKIP_WHITE, READ_NAME, SKIP_TO_EOL } state = SKIP_HEADER_1;
  np = 0;
  while (type != DDSRT_IFTYPE_WIFI && (c = fgetc (fp)) != EOF) {
    switch (state) {
      case SKIP_HEADER_1:
        if (c == '\n') {
          state = SKIP_TO_EOL;
        }
        break;
      case SKIP_WHITE:
        if (c != ' ' && c != '\t') {
          ifnam[np++] = (char) c;
          state = READ_NAME;
        }
        break;
      case READ_NAME:
        if (c == ':') {
          ifnam[np] = 0;
          if (strcmp (ifnam, sys_ifa->ifa_name) == 0)
            type = DDSRT_IFTYPE_WIFI;
          state = SKIP_TO_EOL;
          np = 0;
        } else if (np < sizeof (ifnam) - 1) {
          ifnam[np++] = (char) c;
        }
        break;
      case SKIP_TO_EOL:
        if (c == '\n') {
          state = SKIP_WHITE;
        }
        break;
    }
  }
  fclose (fp);
  return type;
}
#elif (defined(__APPLE__) && !TARGET_OS_IPHONE) || defined(__QNXNTO__) || defined(__FreeBSD__)  /* probably works for all BSDs */

#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <net/if_media.h>

static enum ddsrt_iftype guess_iftype (const struct ifaddrs *sys_ifa)
{
  int sock;
  if ((sock = socket (sys_ifa->ifa_addr->sa_family, SOCK_DGRAM, 0)) == -1)
    return DDSRT_IFTYPE_UNKNOWN;

  struct ifmediareq ifmr;
  enum ddsrt_iftype type;
  memset (&ifmr, 0, sizeof (ifmr));
  (void) ddsrt_strlcpy (ifmr.ifm_name, sys_ifa->ifa_name, sizeof (ifmr.ifm_name));
  if (ioctl (sock, SIOCGIFMEDIA, (caddr_t) &ifmr) < 0)
  {
    type = DDSRT_IFTYPE_UNKNOWN;
  }
  else
  {
    switch (IFM_TYPE (ifmr.ifm_active))
    {
      case IFM_ETHER:
#if !defined __FreeBSD__
      case IFM_TOKEN:
      case IFM_FDDI:
#endif
        type = DDSRT_IFTYPE_WIRED;
        break;
      case IFM_IEEE80211:
        type = DDSRT_IFTYPE_WIFI;
        break;
      default:
        type = DDSRT_IFTYPE_UNKNOWN;
        break;
    }
  }
  close (sock);
  return type;
}
#else
static enum ddsrt_iftype guess_iftype (const struct ifaddrs *sys_ifa)
{
  return DDSRT_IFTYPE_UNKNOWN;
}
#endif

static dds_return_t
copyaddr(ddsrt_ifaddrs_t **ifap, const struct ifaddrs *sys_ifa, enum ddsrt_iftype type)
{
  dds_return_t err = DDS_RETCODE_OK;
  ddsrt_ifaddrs_t *ifa;
  size_t sz;

  assert(ifap != NULL);
  assert(sys_ifa != NULL);

  sz = ddsrt_sockaddr_get_size(sys_ifa->ifa_addr);
  ifa = ddsrt_calloc_s(1, sizeof(*ifa));
  if (ifa == NULL) {
    err = DDS_RETCODE_OUT_OF_RESOURCES;
  } else {
    ifa->index = if_nametoindex(sys_ifa->ifa_name);
    ifa->type = type;
    ifa->flags = sys_ifa->ifa_flags;
    if ((ifa->name = ddsrt_strdup(sys_ifa->ifa_name)) == NULL ||
        (ifa->addr = ddsrt_memdup(sys_ifa->ifa_addr, sz)) == NULL ||
          (sys_ifa->ifa_netmask != NULL &&
        (ifa->netmask = ddsrt_memdup(sys_ifa->ifa_netmask, sz)) == NULL) ||
          (sys_ifa->ifa_broadaddr != NULL &&
          (sys_ifa->ifa_flags & IFF_BROADCAST) &&
        (ifa->broadaddr = ddsrt_memdup(sys_ifa->ifa_broadaddr, sz)) == NULL))
    {
      err = DDS_RETCODE_OUT_OF_RESOURCES;
    }
    /* Seen on macOS using OpenVPN: netmask without an address family,
       in which case copy it from the interface address */
    if (ifa->addr && ifa->netmask && ifa->netmask->sa_family == 0) {
      ifa->netmask->sa_family = ifa->addr->sa_family;
    }
  }

  if (err == 0) {
    *ifap = ifa;
  } else {
    ddsrt_freeifaddrs(ifa);
  }

  return err;
}

dds_return_t
ddsrt_getifaddrs(
  ddsrt_ifaddrs_t **ifap,
  const int *afs)
{
  dds_return_t err = DDS_RETCODE_OK;
  int use;
  ddsrt_ifaddrs_t *ifa, *ifa_root, *ifa_next;
  struct ifaddrs *sys_ifa, *sys_ifa_root;
  struct sockaddr *sa;

  assert(ifap != NULL);

  if (afs == NULL) {
    afs = os_supp_afs;
  }

  if (getifaddrs(&sys_ifa_root) == -1) {
    switch (errno) {
      case EACCES:
        err = DDS_RETCODE_NOT_ALLOWED;
        break;
      case ENOMEM:
      case ENOBUFS:
        err = DDS_RETCODE_OUT_OF_RESOURCES;
        break;
      default:
        err = DDS_RETCODE_ERROR;
        break;
    }
  } else {
    ifa = ifa_root = NULL;

    for (sys_ifa = sys_ifa_root;
         sys_ifa != NULL && err == 0;
         sys_ifa = sys_ifa->ifa_next)
    {
      sa = sys_ifa->ifa_addr;
      if (sa != NULL) {
        use = 0;
        for (int i = 0; !use && afs[i] != DDSRT_AF_TERM; i++) {
          use = (sa->sa_family == afs[i]);
        }

        if (use) {
          enum ddsrt_iftype type = guess_iftype (sys_ifa);
          err = copyaddr(&ifa_next, sys_ifa, type);
          if (err == DDS_RETCODE_OK) {
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

    freeifaddrs(sys_ifa_root);

    if (err == 0) {
      *ifap = ifa_root;
    } else {
      ddsrt_freeifaddrs(ifa_root);
    }
  }

  return err;
}

#if !(defined __linux || defined __APPLE__)

dds_return_t ddsrt_eth_get_mac_addr (char *interface_name, unsigned char *mac_addr)
{
  (void) interface_name; (void) mac_addr;
  return DDS_RETCODE_UNSUPPORTED;
}

#else

#if defined __linux
#include <linux/if_packet.h>
#elif defined(__APPLE__) || defined(__QNXNTO__)
#include <net/if_dl.h>
#endif

dds_return_t ddsrt_eth_get_mac_addr (char *interface_name, unsigned char *mac_addr)
{
    int ret = DDS_RETCODE_ERROR;
    ddsrt_ifaddrs_t *ifa, *ifa_root = NULL;
#if defined __linux
    int afs[] = { AF_PACKET, DDSRT_AF_TERM };
#elif defined(__APPLE__) || defined(__QNXNTO__)
    int afs[] = { AF_LINK, DDSRT_AF_TERM };
#else
#error
#endif
    if (ddsrt_getifaddrs (&ifa_root, afs) < 0)
        return ret;
    for (ifa = ifa_root; ifa != NULL; ifa = ifa->next)
    {
        if (strcmp (ifa->name, interface_name) == 0)
        {
#if defined __linux
            memcpy (mac_addr, ((struct sockaddr_ll *)ifa->addr)->sll_addr, 6);
#elif defined(__APPLE__) || defined(__QNXNTO__)
            memcpy (mac_addr, LLADDR((struct sockaddr_dl *)ifa->addr), 6);
#else
#error
#endif
            ret = DDS_RETCODE_OK;
            break;
        }
    }
    ddsrt_freeifaddrs (ifa_root);
    return ret;
}

#endif
