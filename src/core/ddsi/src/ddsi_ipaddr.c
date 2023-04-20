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
#include <string.h>

#include "dds/ddsrt/endian.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/bits.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__ipaddr.h"
#include "ddsi__tran.h"

int ddsi_ipaddr_compare (const struct sockaddr *const sa1, const struct sockaddr *const sa2)
{
  int eq;
  size_t sz;

  if ((eq = sa1->sa_family - sa2->sa_family) == 0) {
    switch(sa1->sa_family) {
#if DDSRT_HAVE_IPV6
      case AF_INET6: {
        struct sockaddr_in6 *sin61, *sin62;
        sin61 = (struct sockaddr_in6 *)sa1;
        sin62 = (struct sockaddr_in6 *)sa2;
        sz = sizeof(sin61->sin6_addr);
        eq = memcmp(&sin61->sin6_addr, &sin62->sin6_addr, sz);
        break;
      }
#endif
      case AF_INET: {
        struct sockaddr_in *sin1, *sin2;
        sin1 = (struct sockaddr_in *)sa1;
        sin2 = (struct sockaddr_in *)sa2;
        sz = sizeof(sin1->sin_addr);
        eq = memcmp(&sin1->sin_addr, &sin2->sin_addr, sz);
        break;
      }
      default: {
        assert(0);
      }
    }
  }

  return eq;
}

static uint32_t ipaddr_prefixlen (const struct sockaddr * const addr)
{
  uint32_t prefixlen = 0;
  switch(addr->sa_family) {
#if DDSRT_HAVE_IPV6
    case AF_INET6: {
      struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
      prefixlen = 0;
      for (size_t i = 0; i < sizeof (addr6->sin6_addr.s6_addr); i++)
      {
        if (addr6->sin6_addr.s6_addr[i] == 0xff)
          prefixlen += 8;
        else
        {
          if (addr6->sin6_addr.s6_addr[i] != 0)
            prefixlen += 9 - ddsrt_ffs32u (addr6->sin6_addr.s6_addr[i]);
          break;
        }
      }
      break;
    }
#endif
    case AF_INET: {
      struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
      const uint32_t netmask_nativeendian = ddsrt_fromBE4u (addr4->sin_addr.s_addr);
      const uint32_t x = ddsrt_ffs32u (netmask_nativeendian);
      // 255.255.255.255 => x =  1, maximally selective
      // 255.255.255.0   => x =  9, less selective
      // 255.0.0.0       => x = 25, much less selective
      // 0.0.0.0         => x =  0, illegal? in any case, by extension least selective
      prefixlen = (x == 0) ? 0 : 33 - x;
      break;
    }
    default: {
      assert(0);
    }
  }
  return prefixlen;
}

enum ddsi_nearby_address_result ddsi_ipaddr_is_nearby_address (const ddsi_locator_t *loc, size_t ninterf, const struct ddsi_network_interface interf[], size_t *interf_idx)
{
  enum ddsi_nearby_address_result default_result = DNAR_UNREACHABLE;

  // I'm not sure how common it is for a machine to have two network interfaces on
  // the same network, but I don't see why it can't happen.  In that configuration
  // the address might be that of interface k but also match the subnet on
  // interface j < k.  In that case, it seems better to return SELF than LOCAL,
  // and so we first check for an exact match.
  for (size_t i = 0; i < ninterf; i++)
  {
    if (interf[i].loc.kind != loc->kind)
      continue;
    default_result = DNAR_DISTANT;
    if (memcmp (interf[i].loc.address, loc->address, sizeof (loc->address)) == 0 ||
        memcmp (interf[i].extloc.address, loc->address, sizeof (loc->address)) == 0)
    {
      if (interf_idx)
        *interf_idx = i;
      return DNAR_SELF;
    }
  }

  uint32_t best_prefixlen = 0;
  struct sockaddr_storage tmp;
  ddsi_ipaddr_from_loc(&tmp, loc);
  for (size_t i = 0; i < ninterf; i++)
  {
    struct sockaddr_storage iftmp, xiftmp, nmtmp;
    if (interf[i].loc.kind != loc->kind)
      continue;
    ddsi_ipaddr_from_loc(&iftmp, &interf[i].loc);
    ddsi_ipaddr_from_loc(&xiftmp, &interf[i].extloc);
    ddsi_ipaddr_from_loc(&nmtmp, &interf[i].netmask);
    if (ddsrt_sockaddr_insamesubnet ((struct sockaddr *) &tmp, (struct sockaddr *) &iftmp, (struct sockaddr *) &nmtmp) ||
        ddsrt_sockaddr_insamesubnet ((struct sockaddr *) &tmp, (struct sockaddr *) &xiftmp, (struct sockaddr *) &nmtmp))
    {
      default_result = DNAR_LOCAL;
      if (interf_idx == NULL)
        break; // not returning an interface: no need to worry about most specific match
      const uint32_t plen = ipaddr_prefixlen ((struct sockaddr *) &nmtmp);
      if (plen >= best_prefixlen) // >= so (illegal?) edge case of prefixlen 0 handled gracefully
      {
        best_prefixlen = plen;
        *interf_idx = i;
      }
    }
  }
  return default_result;
}

enum ddsi_locator_from_string_result ddsi_ipaddr_from_string (ddsi_locator_t *loc, const char *str, int32_t kind)
{
  DDSRT_WARNING_MSVC_OFF(4996);
  char copy[264];
  int af = AF_INET;
  struct sockaddr_storage tmpaddr;

  switch (kind) {
    case DDSI_LOCATOR_KIND_UDPv4:
    case DDSI_LOCATOR_KIND_TCPv4:
      break;
#if DDSRT_HAVE_IPV6
    case DDSI_LOCATOR_KIND_UDPv6:
    case DDSI_LOCATOR_KIND_TCPv6:
      af = AF_INET6;
      break;
#endif
    default:
      return AFSR_MISMATCH;
  }

  // IPv6: we format numeric ones surrounded by brackets and we want to
  // parse port numbers, so a copy is the most pragamatic approach. POSIX
  // hostnames seem to be limited to 255 characters, add a port of max 5
  // digits and a colon, and so 262 should be enough.  (Numerical addresses
  // add a few other characters, but even so this ought to be plenty.)
  size_t cnt = ddsrt_strlcpy(copy, str, sizeof(copy));
  if (cnt == 0 || cnt >= sizeof(copy))
    return AFSR_INVALID;
  char *ipstr = copy;
  char *portstr = strrchr(copy, ':');
  if (af == AF_INET6 && portstr != strchr(copy, ':') && ipstr[0] != '[')
  {
    // IPv6 numerical addresses contain colons, so if there are multiple
    // colons, we require disambiguation by enclosing the IP part in
    // brackets and hence consider "portstr" only if the first character
    // is '['
    portstr = NULL;
  }
  uint16_t port = 0;
  if (portstr) {
    unsigned tmpport;
    int pos;
    if (sscanf (portstr + 1, "%u%n", &tmpport, &pos) == 1 && portstr[1 + pos] == 0)
    {
      if (tmpport < 1 || tmpport > 65535)
        return AFSR_INVALID;
      *portstr = 0;
      port = (uint16_t) tmpport;
    }
    else if (af == AF_INET)
    {
      // no colons in IPv4 addresses
      return AFSR_INVALID;
    }
    else
    {
      // allow for IPv6 address embedding IPv4 ones, like ff02::ffff:239.255.0.1
      portstr = NULL;
    }
  }

#if DDSRT_HAVE_IPV6
  if (af == AF_INET6)
  {
    if (copy[0] == '[') {
      // strip brackets: last character before the port must be a ']',
      // in the absence of a port, the last character in the string.
      ipstr = copy + 1;
      if (portstr == NULL) {
        if (copy[cnt - 1] != ']')
          return AFSR_INVALID;
        copy[cnt - 1] = 0;
      } else {
        assert (portstr > copy);
        if (portstr[-1] != ']')
          return AFSR_INVALID;
        portstr[-1] = 0;
      }
    }
  }
#endif

  if (ddsrt_sockaddrfromstr(af, ipstr, (struct sockaddr *) &tmpaddr) != 0) {
#if DDSRT_HAVE_DNS
    /* Not a valid IP address. User may have specified a hostname instead. */
    ddsrt_hostent_t *hent = NULL;
    if (ddsrt_gethostbyname(ipstr, af, &hent) != 0) {
      return AFSR_UNKNOWN;
    }
    memcpy(&tmpaddr, &hent->addrs[0], sizeof(hent->addrs[0]));
    ddsrt_free (hent);
#else
    return AFSR_INVALID;
#endif
  }
  // patch in port (sin_port/sin6_port is undefined at this point and must always be set
  // before calling ddsi_ipaddr_to_loc
  if (tmpaddr.ss_family != af) {
    return AFSR_MISMATCH;
  } else if (af == AF_INET) {
    struct sockaddr_in *x = (struct sockaddr_in *) &tmpaddr;
    x->sin_port = htons (port);
  } else {
#if DDSRT_HAVE_IPV6
    assert (af == AF_INET6);
    struct sockaddr_in6 *x = (struct sockaddr_in6 *) &tmpaddr;
    x->sin6_port = htons (port);
#else
    abort ();
#endif
  }
  ddsi_ipaddr_to_loc (loc, (struct sockaddr *)&tmpaddr, kind);
  return AFSR_OK;
  DDSRT_WARNING_MSVC_ON(4996);
}

char *ddsi_ipaddr_to_string (char *dst, size_t sizeof_dst, const ddsi_locator_t *loc, int with_port, const struct ddsi_network_interface *interf)
{
  assert (sizeof_dst > 1);
  if (loc->kind == DDSI_LOCATOR_KIND_INVALID)
    (void) snprintf (dst, sizeof_dst, "(invalid)");
  else
  {
    struct sockaddr_storage src;
    size_t pos = 0;
    int cnt = 0;
    ddsi_ipaddr_from_loc(&src, loc);
    switch (src.ss_family)
    {
      case AF_INET:
        ddsrt_sockaddrtostr ((const struct sockaddr *) &src, dst, sizeof_dst);
        pos = strlen (dst);
        if (with_port) {
          assert(pos <= sizeof_dst);
          cnt = snprintf (dst + pos, sizeof_dst - pos, ":%"PRIu32, loc->port);
        }
        break;
#if DDSRT_HAVE_IPV6
      case AF_INET6:
        dst[0] = '[';
        ddsrt_sockaddrtostr ((const struct sockaddr *) &src, dst + 1, sizeof_dst);
        pos = strlen (dst);
        if (with_port) {
          assert(pos <= sizeof_dst);
          cnt = snprintf (dst + pos, sizeof_dst - pos, "]:%"PRIu32, loc->port);
        } else {
          cnt = snprintf (dst + pos, sizeof_dst - pos, "]");
        }
        break;
#endif
      default:
        assert(0);
        dst[0] = 0;
        break;
    }
    if (cnt >= 0)
      pos += (size_t) cnt;
    if (interf && pos < sizeof_dst)
      snprintf (dst + pos, sizeof_dst - pos, "@%"PRIu32, interf->if_index);
  }
  return dst;
}

void ddsi_ipaddr_to_loc (ddsi_locator_t *dst, const struct sockaddr *src, int32_t kind)
{
  dst->kind = kind;
  switch (src->sa_family)
  {
    case AF_INET:
    {
      const struct sockaddr_in *x = (const struct sockaddr_in *) src;
      assert (kind == DDSI_LOCATOR_KIND_UDPv4 || kind == DDSI_LOCATOR_KIND_TCPv4);
      if (x->sin_addr.s_addr == htonl (INADDR_ANY))
      {
        dst->kind = DDSI_LOCATOR_KIND_INVALID;
        dst->port = DDSI_LOCATOR_PORT_INVALID;
        memset (dst->address, 0, sizeof (dst->address));
      }
      else
      {
        dst->port = (x->sin_port == 0) ? DDSI_LOCATOR_PORT_INVALID : ntohs (x->sin_port);
        memset (dst->address, 0, 12);
        memcpy (dst->address + 12, &x->sin_addr.s_addr, 4);
      }
      break;
    }
#if DDSRT_HAVE_IPV6
    case AF_INET6:
    {
      const struct sockaddr_in6 *x = (const struct sockaddr_in6 *) src;
      assert (kind == DDSI_LOCATOR_KIND_UDPv6 || kind == DDSI_LOCATOR_KIND_TCPv6);
      if (IN6_IS_ADDR_UNSPECIFIED (&x->sin6_addr))
      {
        dst->kind = DDSI_LOCATOR_KIND_INVALID;
        dst->port = DDSI_LOCATOR_PORT_INVALID;
        memset (dst->address, 0, sizeof (dst->address));
      }
      else
      {
        dst->port = (x->sin6_port == 0) ? DDSI_LOCATOR_PORT_INVALID : ntohs (x->sin6_port);
        memcpy (dst->address, &x->sin6_addr.s6_addr, 16);
      }
      break;
    }
#endif
    default:
      DDS_FATAL("nn_address_to_loc: family %d unsupported\n", (int) src->sa_family);
  }
}

void ddsi_ipaddr_from_loc (struct sockaddr_storage *dst, const ddsi_locator_t *src)
{
  memset (dst, 0, sizeof (*dst));
  switch (src->kind)
  {
    case DDSI_LOCATOR_KIND_INVALID:
      assert (0);
      break;
    case DDSI_LOCATOR_KIND_UDPv4:
    case DDSI_LOCATOR_KIND_TCPv4:
    {
      struct sockaddr_in *x = (struct sockaddr_in *) dst;
      x->sin_family = AF_INET;
      x->sin_port = (src->port == DDSI_LOCATOR_PORT_INVALID) ? 0 : htons ((unsigned short) src->port);
      memcpy (&x->sin_addr.s_addr, src->address + 12, 4);
      break;
    }
#if DDSRT_HAVE_IPV6
    case DDSI_LOCATOR_KIND_UDPv6:
    case DDSI_LOCATOR_KIND_TCPv6:
    {
      struct sockaddr_in6 *x = (struct sockaddr_in6 *) dst;
      x->sin6_family = AF_INET6;
      x->sin6_port = (src->port == DDSI_LOCATOR_PORT_INVALID) ? 0 : htons ((unsigned short) src->port);
      memcpy (&x->sin6_addr.s6_addr, src->address, 16);
      if (IN6_IS_ADDR_LINKLOCAL (&x->sin6_addr))
      {
        x->sin6_scope_id = 0;//FIXME: gv.interfaceNo;
      }
      break;
    }
#endif
    default:
      break;
  }
}
