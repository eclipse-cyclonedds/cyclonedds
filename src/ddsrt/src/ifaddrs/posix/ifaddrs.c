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
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/random.h"

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

static bool is_the_kernel_likely_lying_about_multicast (const ddsrt_ifaddrs_t *ifa)
{
  assert (ifa->addr->sa_family == AF_INET || ifa->addr->sa_family == AF_INET6);
  bool multicast_works = false;
  const bool ipv6 = (ifa->addr->sa_family == AF_INET6);
  socklen_t addrsz = ipv6 ? sizeof (struct sockaddr_in6) : sizeof (struct sockaddr_in);
  // multicast over link local address works in macOS, but the default firewall rule is not happy with this
  // so let us simply assume the "normal" loopback interface address like ::1 exists as well
  if (ipv6 && IN6_IS_ADDR_LINKLOCAL (&((const struct sockaddr_in6 *) ifa->addr)->sin6_addr))
    return false;
  int sock = socket (ifa->addr->sa_family, SOCK_DGRAM, 0);
  if (sock < 0)
    return false;
#ifdef __APPLE__
  // macOS needs a short timeout, curiously enough! (but we don't need this code on macOS
  // because the kernel tells it as it is
  const struct timeval recvtimeo = { .tv_sec = 0, .tv_usec = 10000 };
  if (setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &recvtimeo, sizeof (recvtimeo)) != 0)
    goto out;
#endif
  union ipsockaddr {
    struct sockaddr gen;
    struct sockaddr_in ipv4;
    struct sockaddr_in6 ipv6;
  } addr, mcaddr;
  memset (&addr, 0, sizeof (addr));
  memset (&mcaddr, 0, sizeof (mcaddr));
  // Multicast address: abuse DDSI's default address because we need to pick something
  if (ipv6) {
    addr.ipv6 = *((struct sockaddr_in6 *) ifa->addr);
    addr.ipv6.sin6_port = 0;
    mcaddr = addr;
    if (inet_pton (mcaddr.gen.sa_family, "ff02::ffff:239.255.0.1", &mcaddr.ipv6.sin6_addr) != 1)
      goto out;
  } else {
    addr.ipv4 = *((struct sockaddr_in *) ifa->addr);
    addr.ipv4.sin_addr.s_addr = htonl (INADDR_ANY); // because we can't receive multicasts otherwise
    addr.ipv4.sin_port = 0;
    mcaddr = addr;
    if (inet_pton (mcaddr.gen.sa_family, "239.255.0.1", &mcaddr.ipv4.sin_addr) != 1)
      goto out;
  }
  if (bind (sock, &addr.gen, addrsz) < 0)
    goto out;
  if (getsockname (sock, &addr.gen, &addrsz) < 0)
    goto out;
  if (ipv6)
  {
    const unsigned hops = 0;
    struct ipv6_mreq ipv6mreq;
    mcaddr.ipv6.sin6_port = addr.ipv6.sin6_port;
    memset (&ipv6mreq, 0, sizeof (ipv6mreq));
    ipv6mreq.ipv6mr_multiaddr = mcaddr.ipv6.sin6_addr;
    ipv6mreq.ipv6mr_interface = ifa->index;
    if (setsockopt (sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &ipv6mreq, sizeof (ipv6mreq)) != 0 ||
        setsockopt (sock, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifa->index, sizeof (ifa->index)) != 0 ||
        setsockopt (sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops, sizeof (hops)) != 0)
      goto out;
  }
  else
  {
    const unsigned char ttl = 0;
    struct ip_mreq mreq;
    mcaddr.ipv4.sin_port = addr.ipv4.sin_port;
    mreq.imr_multiaddr = mcaddr.ipv4.sin_addr;
    mreq.imr_interface = addr.ipv4.sin_addr;
    if (setsockopt (sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof (mreq)) != 0 ||
        setsockopt (sock, IPPROTO_IP, IP_MULTICAST_IF, &addr.ipv4.sin_addr, sizeof (addr.ipv4.sin_addr)) != 0 ||
        setsockopt (sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof (ttl)) != 0)
      goto out;
  }
  // Use a 128 bit random payload to make it unlikely that we conclude multicast works
  // because of some other packet that just happens to reach our socket in the (very)
  // short time it exists
  const uint32_t contents[4] = {
    ddsrt_random (), ddsrt_random (), ddsrt_random (), ddsrt_random ()
  };
  ddsrt_msghdr_t msg = {
    .msg_name = &mcaddr.gen,
    .msg_namelen = addrsz,
    .msg_iov = &(struct iovec) { .iov_len = sizeof (contents), .iov_base = (void *) &contents },
    .msg_iovlen = 1,
    .msg_control = NULL,
    .msg_controllen = 0,
    .msg_flags = 0
  };
  if (sendmsg (sock, &msg, 0) != (ssize_t) sizeof (contents))
    goto out;
#ifndef __APPLE__ // because we do a short timeout instead
  if (fcntl (sock, F_SETFL, O_NONBLOCK) == -1)
    goto out;
#endif
  unsigned char recvbuf[sizeof (contents)];
  msg.msg_iov = &(struct iovec) { .iov_len = sizeof (recvbuf), .iov_base = recvbuf };
  ssize_t nrecv;
  while ((nrecv = recvmsg (sock, &msg, 0)) > 0)
  {
    if (nrecv == sizeof (recvbuf) &&
        memcmp (recvbuf, contents, sizeof (recvbuf)) == 0 &&
        !(msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)))
    {
      multicast_works = true;
      break;
    }
  }
out:
  close (sock);
  return multicast_works;
}

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
    /* Common on Linux: a loopback interface that does not have the MULTICAST
       flag but that does support multicast in reality, at least on IPv4.  Do
       a trial run if we're doing something with INET */
    if (ifa->addr &&
        (ifa->flags & IFF_LOOPBACK) && !(ifa->flags & IFF_MULTICAST) &&
        (ifa->addr->sa_family == AF_INET || ifa->addr->sa_family == AF_INET6))
    {
      if (is_the_kernel_likely_lying_about_multicast (ifa))
        ifa->flags |= IFF_MULTICAST;
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
