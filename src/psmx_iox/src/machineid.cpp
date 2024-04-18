// Copyright(c) 2024 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "machineid.hpp"

#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/ifaddrs.h"
#if defined(__linux) && !LWIP_SOCKET
#include "linux/if_packet.h"
#elif defined(__APPLE__) || defined(__QNXNTO__) || defined(__FreeBSD__)
#include "net/if_dl.h"
#endif

std::optional<dds_psmx_node_identifier_t> get_machineid ()
{
  // the non-foolproof strategy here: if we have some MAC addresses, use the
  // MD5 hash of the concatenation of those as the machine id.  If we don't
  // have any, use the MD5 hash of the concatenation of IP addresses.  If we
  // don't have those give up.
  //
  // MAC addresses ought not to change, IP addresses can change, interfaces
  // can come and go (even hardware interfaces), so it is really far from
  // perfect.  You would hope the kernel provides a unique id somehow
  // somewhere, but I'm not sure enough about, e.g. kern.uuid on macOS.
  //
  // The reason this exists is simply that we need "something" for Iceoryx
  // support, but as MAC addresses are the best thing I can think of (and
  // ideally of the interface(s) used by Cyclone, but that gets into trouble
  // with the initialization order).  There is the fall-back of overriding
  // the locator in the config, so one is never totally dependent on this.
  //
  // FIXME: our getifaddrs on Windows doesn't return MAC addresses
  struct ddsrt_ifaddrs *ifa_root;
  if (ddsrt_getifaddrs (&ifa_root, NULL) != DDS_RETCODE_OK)
    return std::nullopt;
  bool have_mac = false, have_ip = false;
  ddsrt_md5_state_t md5st_mac, md5st_ip;
  ddsrt_md5_init (&md5st_mac);
  ddsrt_md5_init (&md5st_ip);
  for (const struct ddsrt_ifaddrs *ifa = ifa_root; ifa; ifa = ifa->next)
  {
    const struct sockaddr *sa = ifa->addr;
    switch (sa->sa_family) {
#if DDSRT_HAVE_IPV6
      case AF_INET6: {
        const struct sockaddr_in6 *x = (const struct sockaddr_in6 *) sa;
        ddsrt_md5_append (&md5st_ip, (const ddsrt_md5_byte_t *) &x->sin6_addr, sizeof (x->sin6_addr));
        have_ip = true;
        break;
      }
#endif /* DDSRT_HAVE_IPV6 */
      case AF_INET: {
        const struct sockaddr_in *x = (const struct sockaddr_in *) sa;
        ddsrt_md5_append (&md5st_ip, (const ddsrt_md5_byte_t *) &x->sin_addr, sizeof (x->sin_addr));
        have_ip = true;
        break;
      }
#if defined(__linux) && !LWIP_SOCKET
      case AF_PACKET: {
        const struct sockaddr_ll *x = (const struct sockaddr_ll *) sa;
        ddsrt_md5_append (&md5st_mac, (const ddsrt_md5_byte_t *) &x->sll_addr, sizeof (x->sll_addr));
        have_mac = true;
        break;
      }
#elif defined(__APPLE__) || defined(__QNXNTO__) || defined(__FreeBSD__)
      case AF_LINK: {
        const struct sockaddr_dl *x = (const struct sockaddr_dl *) sa;
        ddsrt_md5_append (&md5st_mac, (const ddsrt_md5_byte_t *) LLADDR (x), x->sdl_alen);
        have_mac = true;
        break;
      }
#endif /* __linux */
    }
  }
  ddsrt_freeifaddrs (ifa_root);
  dds_psmx_node_identifier_t mid;
  static_assert (sizeof (mid.x) == 16);
  if (have_mac) {
    ddsrt_md5_finish (&md5st_mac, static_cast<ddsrt_md5_byte_t *>(mid.x));
    return mid;
  } else if (have_ip) {
    ddsrt_md5_finish (&md5st_ip, static_cast<ddsrt_md5_byte_t *>(mid.x));
    return mid;
  } else {
    return std::nullopt;
  }
}
