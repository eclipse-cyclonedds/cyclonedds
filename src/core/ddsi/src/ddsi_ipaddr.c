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
#include "os/os.h"
#include "ddsi/ddsi_ipaddr.h"
#include "ddsi/q_nwif.h"
#include "ddsi/q_config.h"

int ddsi_ipaddr_compare (const os_sockaddr *const sa1, const os_sockaddr *const sa2)
{
  int eq;
  size_t sz;

  if ((eq = sa1->sa_family - sa2->sa_family) == 0) {
    switch(sa1->sa_family) {
#if (OS_SOCKET_HAS_IPV6 == 1)
      case AF_INET6: {
        os_sockaddr_in6 *sin61, *sin62;
        sin61 = (os_sockaddr_in6 *)sa1;
        sin62 = (os_sockaddr_in6 *)sa2;
        sz = sizeof(sin61->sin6_addr);
        eq = memcmp(&sin61->sin6_addr, &sin62->sin6_addr, sz);
        break;
      }
#endif /* OS_SOCKET_HAS_IPV6 */
      case AF_INET: {
        os_sockaddr_in *sin1, *sin2;
        sin1 = (os_sockaddr_in *)sa1;
        sin2 = (os_sockaddr_in *)sa2;
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

enum ddsi_nearby_address_result ddsi_ipaddr_is_nearby_address (ddsi_tran_factory_t tran, const nn_locator_t *loc, size_t ninterf, const struct nn_interface interf[])
{
  os_sockaddr_storage tmp, iftmp, nmtmp, ownip;
  size_t i;
  (void)tran;
  ddsi_ipaddr_from_loc(&tmp, loc);
  for (i = 0; i < ninterf; i++)
  {
    ddsi_ipaddr_from_loc(&iftmp, &interf[i].loc);
    ddsi_ipaddr_from_loc(&nmtmp, &interf[i].netmask);
    ddsi_ipaddr_from_loc(&ownip, &gv.ownloc);
    if (os_sockaddrSameSubnet ((os_sockaddr *) &tmp, (os_sockaddr *) &iftmp, (os_sockaddr *) &nmtmp))
    {
      if (ddsi_ipaddr_compare((os_sockaddr *)&iftmp, (os_sockaddr *)&ownip) == 0)
        return DNAR_SAME;
      else
        return DNAR_LOCAL;
    }
  }
  return DNAR_DISTANT;
}

enum ddsi_locator_from_string_result ddsi_ipaddr_from_string (ddsi_tran_factory_t tran, nn_locator_t *loc, const char *str, int32_t kind)
{
  int af = AF_INET;
  os_sockaddr_storage tmpaddr;

  switch (kind) {
    case NN_LOCATOR_KIND_UDPv4:
    case NN_LOCATOR_KIND_TCPv4:
      break;
#if OS_SOCKET_HAS_IPV6
    case NN_LOCATOR_KIND_UDPv6:
    case NN_LOCATOR_KIND_TCPv6:
      af = AF_INET6;
      break;
#endif /* OS_SOCKET_HAS_IPV6 */
    default:
      return AFSR_MISMATCH;
  }

  (void)tran;
  if (os_sockaddrfromstr(af, str, (os_sockaddr *) &tmpaddr) != 0) {
#if OS_SOCKET_HAS_DNS
      /* Not a valid IP address. User may have specified a hostname instead. */
      os_hostent_t *hent = NULL;
      if (os_gethostbyname(str, af, &hent) != 0) {
          return AFSR_UNKNOWN;
      }
      memcpy(&tmpaddr, &hent->addrs[0], sizeof(hent->addrs[0]));
#else
      return AFSR_INVALID;
#endif /* OS_SOCKET_HAS_DNS */
  }
  if (tmpaddr.ss_family != af) {
    return AFSR_MISMATCH;
  }
  ddsi_ipaddr_to_loc (loc, (os_sockaddr *)&tmpaddr, kind);
  /* This is just an address, so there is no valid value for port, other than INVALID.
     Without a guarantee that tmpaddr has port 0, best is to set it explicitly here */
  loc->port = NN_LOCATOR_PORT_INVALID;
  return AFSR_OK;
}

char *ddsi_ipaddr_to_string (ddsi_tran_factory_t tran, char *dst, size_t sizeof_dst, const nn_locator_t *loc, int with_port)
{
  os_sockaddr_storage src;
  size_t pos;
  (void)tran;
  assert (sizeof_dst > 1);
  ddsi_ipaddr_from_loc(&src, loc);
  switch (src.ss_family)
  {
    case AF_INET:
      os_sockaddrtostr ((const os_sockaddr *) &src, dst, sizeof_dst);
      if (with_port) {
        pos = strlen (dst);
        assert(pos <= sizeof_dst);
        snprintf (dst + pos, sizeof_dst - pos, ":%d", loc->port);
      }
      break;
#if OS_SOCKET_HAS_IPV6
    case AF_INET6:
      dst[0] = '[';
      os_sockaddrtostr ((const os_sockaddr *) &src, dst + 1, sizeof_dst);
      pos = strlen (dst);
      if (with_port) {
        assert(pos <= sizeof_dst);
        snprintf (dst + pos, sizeof_dst - pos, "]:%u", loc->port);
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
  return dst;
}

void ddsi_ipaddr_to_loc (nn_locator_t *dst, const os_sockaddr *src, int32_t kind)
{
  dst->kind = kind;
  switch (src->sa_family)
  {
    case AF_INET:
    {
      const os_sockaddr_in *x = (const os_sockaddr_in *) src;
      assert (kind == NN_LOCATOR_KIND_UDPv4 || kind == NN_LOCATOR_KIND_TCPv4);
      if (x->sin_addr.s_addr == htonl (INADDR_ANY))
      {
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
#if OS_SOCKET_HAS_IPV6
    case AF_INET6:
    {
      const os_sockaddr_in6 *x = (const os_sockaddr_in6 *) src;
      assert (kind == NN_LOCATOR_KIND_UDPv6 || kind == NN_LOCATOR_KIND_TCPv6);
      if (IN6_IS_ADDR_UNSPECIFIED (&x->sin6_addr))
      {
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

void ddsi_ipaddr_from_loc (os_sockaddr_storage *dst, const nn_locator_t *src)
{
  memset (dst, 0, sizeof (*dst));
  switch (src->kind)
  {
    case NN_LOCATOR_KIND_INVALID:
#if OS_SOCKET_HAS_IPV6
      dst->ss_family = (config.transport_selector == TRANS_UDP6 || config.transport_selector == TRANS_TCP6) ? AF_INET6 : AF_INET;
#else
      dst->ss_family = AF_INET;
#endif
      break;
    case NN_LOCATOR_KIND_UDPv4:
    case NN_LOCATOR_KIND_TCPv4:
    {
      os_sockaddr_in *x = (os_sockaddr_in *) dst;
      x->sin_family = AF_INET;
      x->sin_port = (src->port == NN_LOCATOR_PORT_INVALID) ? 0 : htons ((unsigned short) src->port);
      memcpy (&x->sin_addr.s_addr, src->address + 12, 4);
      break;
    }
#if OS_SOCKET_HAS_IPV6
    case NN_LOCATOR_KIND_UDPv6:
    case NN_LOCATOR_KIND_TCPv6:
    {
      os_sockaddr_in6 *x = (os_sockaddr_in6 *) dst;
      x->sin6_family = AF_INET6;
      x->sin6_port = (src->port == NN_LOCATOR_PORT_INVALID) ? 0 : htons ((unsigned short) src->port);
      memcpy (&x->sin6_addr.s6_addr, src->address, 16);
      if (IN6_IS_ADDR_LINKLOCAL (&x->sin6_addr))
      {
        x->sin6_scope_id = gv.interfaceNo;
      }
      break;
    }
#endif
    default:
      break;
  }
}
