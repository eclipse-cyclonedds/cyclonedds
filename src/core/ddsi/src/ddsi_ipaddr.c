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
#include <assert.h>
#include <string.h>

#include "dds/ddsrt/endian.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsi/ddsi_ipaddr.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/ddsi_domaingv.h"

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

enum ddsi_nearby_address_result ddsi_ipaddr_is_nearby_address (const nn_locator_t *loc, const nn_locator_t *ownloc, size_t ninterf, const struct nn_interface interf[])
{
  struct sockaddr_storage tmp, iftmp, nmtmp, ownip;
  size_t i;
  ddsi_ipaddr_from_loc(&tmp, loc);
  for (i = 0; i < ninterf; i++)
  {
    ddsi_ipaddr_from_loc(&iftmp, &interf[i].loc);
    ddsi_ipaddr_from_loc(&nmtmp, &interf[i].netmask);
    ddsi_ipaddr_from_loc(&ownip, ownloc);
    if (ddsrt_sockaddr_insamesubnet ((struct sockaddr *) &tmp, (struct sockaddr *) &iftmp, (struct sockaddr *) &nmtmp))
    {
      if (ddsi_ipaddr_compare((struct sockaddr *)&iftmp, (struct sockaddr *)&ownip) == 0)
        return DNAR_SAME;
      else
        return DNAR_LOCAL;
    }
  }
  return DNAR_DISTANT;
}

enum ddsi_locator_from_string_result ddsi_ipaddr_from_string (const struct ddsi_tran_factory *tran, nn_locator_t *loc, const char *str, int32_t kind)
{
  DDSRT_WARNING_MSVC_OFF(4996);
  char copy[264];
  int af = AF_INET;
  struct sockaddr_storage tmpaddr;

  switch (kind) {
    case NN_LOCATOR_KIND_UDPv4:
    case NN_LOCATOR_KIND_TCPv4:
      break;
#if DDSRT_HAVE_IPV6
    case NN_LOCATOR_KIND_UDPv6:
    case NN_LOCATOR_KIND_TCPv6:
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
  const size_t len = strlen(str);
  if (len == 0 || len >= sizeof(copy))
    return AFSR_INVALID;
  memcpy(copy, str, len + 1);
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
        if (copy[len - 1] != ']')
          return AFSR_INVALID;
        copy[len - 1] = 0;
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
  ddsi_ipaddr_to_loc (tran, loc, (struct sockaddr *)&tmpaddr, kind);
  return AFSR_OK;
  DDSRT_WARNING_MSVC_ON(4996);
}

char *ddsi_ipaddr_to_string (char *dst, size_t sizeof_dst, const nn_locator_t *loc, int with_port)
{
  assert (sizeof_dst > 1);
  if (loc->kind == NN_LOCATOR_KIND_INVALID)
    (void) snprintf (dst, sizeof_dst, "(invalid)");
  else
  {
    struct sockaddr_storage src;
    size_t pos;
    ddsi_ipaddr_from_loc(&src, loc);
    switch (src.ss_family)
    {
      case AF_INET:
        ddsrt_sockaddrtostr ((const struct sockaddr *) &src, dst, sizeof_dst);
        if (with_port) {
          pos = strlen (dst);
          assert(pos <= sizeof_dst);
          snprintf (dst + pos, sizeof_dst - pos, ":%"PRIu32, loc->port);
        }
        break;
#if DDSRT_HAVE_IPV6
      case AF_INET6:
        dst[0] = '[';
        ddsrt_sockaddrtostr ((const struct sockaddr *) &src, dst + 1, sizeof_dst);
        pos = strlen (dst);
        if (with_port) {
          assert(pos <= sizeof_dst);
          snprintf (dst + pos, sizeof_dst - pos, "]:%"PRIu32, loc->port);
        } else {
          snprintf (dst + pos, sizeof_dst - pos, "]");
        }
        break;
#endif
      default:
        assert(0);
        dst[0] = 0;
        break;
    }
  }
  return dst;
}

void ddsi_ipaddr_to_loc (const struct ddsi_tran_factory *tran, nn_locator_t *dst, const struct sockaddr *src, int32_t kind)
{
  dst->tran = (struct ddsi_tran_factory *) tran;
  dst->kind = kind;
  switch (src->sa_family)
  {
    case AF_INET:
    {
      const struct sockaddr_in *x = (const struct sockaddr_in *) src;
      assert (kind == NN_LOCATOR_KIND_UDPv4 || kind == NN_LOCATOR_KIND_TCPv4);
      if (x->sin_addr.s_addr == htonl (INADDR_ANY))
      {
        dst->tran = NULL;
        dst->kind = NN_LOCATOR_KIND_INVALID;
        dst->port = NN_LOCATOR_PORT_INVALID;
        memset (dst->address, 0, sizeof (dst->address));
      }
      else
      {
        dst->port = (x->sin_port == 0) ? NN_LOCATOR_PORT_INVALID : ntohs (x->sin_port);
        memset (dst->address, 0, 12);
        memcpy (dst->address + 12, &x->sin_addr.s_addr, 4);
      }
      break;
    }
#if DDSRT_HAVE_IPV6
    case AF_INET6:
    {
      const struct sockaddr_in6 *x = (const struct sockaddr_in6 *) src;
      assert (kind == NN_LOCATOR_KIND_UDPv6 || kind == NN_LOCATOR_KIND_TCPv6);
      if (IN6_IS_ADDR_UNSPECIFIED (&x->sin6_addr))
      {
        dst->tran = NULL;
        dst->kind = NN_LOCATOR_KIND_INVALID;
        dst->port = NN_LOCATOR_PORT_INVALID;
        memset (dst->address, 0, sizeof (dst->address));
      }
      else
      {
        dst->port = (x->sin6_port == 0) ? NN_LOCATOR_PORT_INVALID : ntohs (x->sin6_port);
        memcpy (dst->address, &x->sin6_addr.s6_addr, 16);
      }
      break;
    }
#endif
    default:
      DDS_FATAL("nn_address_to_loc: family %d unsupported\n", (int) src->sa_family);
  }
}

void ddsi_ipaddr_from_loc (struct sockaddr_storage *dst, const nn_locator_t *src)
{
  memset (dst, 0, sizeof (*dst));
  switch (src->kind)
  {
    case NN_LOCATOR_KIND_INVALID:
      assert (0);
      break;
    case NN_LOCATOR_KIND_UDPv4:
    case NN_LOCATOR_KIND_TCPv4:
    {
      struct sockaddr_in *x = (struct sockaddr_in *) dst;
      x->sin_family = AF_INET;
      x->sin_port = (src->port == NN_LOCATOR_PORT_INVALID) ? 0 : htons ((unsigned short) src->port);
      memcpy (&x->sin_addr.s_addr, src->address + 12, 4);
      break;
    }
#if DDSRT_HAVE_IPV6
    case NN_LOCATOR_KIND_UDPv6:
    case NN_LOCATOR_KIND_TCPv6:
    {
      struct sockaddr_in6 *x = (struct sockaddr_in6 *) dst;
      x->sin6_family = AF_INET6;
      x->sin6_port = (src->port == NN_LOCATOR_PORT_INVALID) ? 0 : htons ((unsigned short) src->port);
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
