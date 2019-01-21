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
#include <ctype.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "os/os.h"

#include "ddsi/q_log.h"
#include "ddsi/q_nwif.h"
#include "ddsi/q_globals.h"
#include "ddsi/q_config.h"
#include "ddsi/q_unused.h"
#include "ddsi/q_md5.h"
#include "ddsi/q_misc.h"
#include "ddsi/q_addrset.h" /* unspec locator */
#include "ddsi/q_feature_check.h"
#include "ddsi/ddsi_ipaddr.h"
#include "util/ut_avl.h"

static void print_sockerror (const char *msg)
{
  int err = os_getErrno ();
  DDS_ERROR("SOCKET %s errno %d\n", msg, err);
}

unsigned locator_to_hopefully_unique_uint32 (const nn_locator_t *src)
{
  unsigned id;
  if (src->kind == NN_LOCATOR_KIND_UDPv4 || src->kind == NN_LOCATOR_KIND_TCPv4)
    memcpy (&id, src->address + 12, sizeof (id));
  else
  {
#if OS_SOCKET_HAS_IPV6
    md5_state_t st;
    md5_byte_t digest[16];
    md5_init (&st);
    md5_append (&st, (const md5_byte_t *) ((const os_sockaddr_in6 *) src)->sin6_addr.s6_addr, 16);
    md5_finish (&st, digest);
    memcpy (&id, digest, sizeof (id));
#else
    DDS_FATAL("IPv6 unavailable\n");
#endif
  }
  return id;
}

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
void set_socket_diffserv (os_socket sock, int diffserv)
{
  if (os_sockSetsockopt (sock, IPPROTO_IP, IP_TOS, (char*) &diffserv, sizeof (diffserv)) != os_resultSuccess)
  {
    print_sockerror ("IP_TOS");
  }
}
#endif

#ifdef SO_NOSIGPIPE
static void set_socket_nosigpipe (os_socket sock)
{
  int val = 1;
  if (os_sockSetsockopt (sock, SOL_SOCKET, SO_NOSIGPIPE, (char*) &val, sizeof (val)) != os_resultSuccess)
  {
    print_sockerror ("SO_NOSIGPIPE");
  }
}
#endif

#ifdef TCP_NODELAY
static void set_socket_nodelay (os_socket sock)
{
  int val = 1;
  if (os_sockSetsockopt (sock, IPPROTO_TCP, TCP_NODELAY, (char*) &val, sizeof (val)) != os_resultSuccess)
  {
    print_sockerror ("TCP_NODELAY");
  }
}
#endif

static int set_rcvbuf (os_socket socket)
{
  uint32_t ReceiveBufferSize;
  uint32_t optlen = (uint32_t) sizeof (ReceiveBufferSize);
  uint32_t socket_min_rcvbuf_size;
  if (config.socket_min_rcvbuf_size.isdefault)
    socket_min_rcvbuf_size = 1048576;
  else
    socket_min_rcvbuf_size = config.socket_min_rcvbuf_size.value;
  if (os_sockGetsockopt (socket, SOL_SOCKET, SO_RCVBUF, (char *) &ReceiveBufferSize, &optlen) != os_resultSuccess)
  {
    print_sockerror ("get SO_RCVBUF");
    return -2;
  }
  if (ReceiveBufferSize < socket_min_rcvbuf_size)
  {
    /* make sure the receive buffersize is at least the minimum required */
    ReceiveBufferSize = socket_min_rcvbuf_size;
    (void) os_sockSetsockopt (socket, SOL_SOCKET, SO_RCVBUF, (const char *) &ReceiveBufferSize, sizeof (ReceiveBufferSize));

    /* We don't check the return code from setsockopt, because some O/Ss tend
       to silently cap the buffer size.  The only way to make sure is to read
       the option value back and check it is now set correctly. */
    if (os_sockGetsockopt (socket, SOL_SOCKET, SO_RCVBUF, (char *) &ReceiveBufferSize, &optlen) != os_resultSuccess)
    {
      print_sockerror ("get SO_RCVBUF");
      return -2;
    }
    if (ReceiveBufferSize < socket_min_rcvbuf_size)
    {
      /* NN_ERROR does more than just DDS_ERROR(), hence the duplication */
      if (config.socket_min_rcvbuf_size.isdefault)
        DDS_LOG(DDS_LC_CONFIG, "failed to increase socket receive buffer size to %u bytes, continuing with %u bytes\n", socket_min_rcvbuf_size, ReceiveBufferSize);
      else
        DDS_ERROR("failed to increase socket receive buffer size to %u bytes, continuing with %u bytes\n", socket_min_rcvbuf_size, ReceiveBufferSize);
    }
    else
    {
      DDS_LOG(DDS_LC_CONFIG, "socket receive buffer size set to %u bytes\n", ReceiveBufferSize);
    }
  }
  return 0;
}

static int set_sndbuf (os_socket socket)
{
  unsigned SendBufferSize;
  uint32_t optlen = (uint32_t) sizeof(SendBufferSize);
  if (os_sockGetsockopt(socket, SOL_SOCKET, SO_SNDBUF,(char *)&SendBufferSize, &optlen) != os_resultSuccess)
  {
    print_sockerror ("get SO_SNDBUF");
    return -2;
  }
  if (SendBufferSize < config.socket_min_sndbuf_size )
  {
    /* make sure the send buffersize is at least the minimum required */
    SendBufferSize = config.socket_min_sndbuf_size;
    if (os_sockSetsockopt (socket, SOL_SOCKET, SO_SNDBUF, (const char *)&SendBufferSize, sizeof (SendBufferSize)) != os_resultSuccess)
    {
      print_sockerror ("SO_SNDBUF");
      return -2;
    }
  }
  return 0;
}

static int maybe_set_dont_route (os_socket socket)
{
  if (config.dontRoute)
  {
#if OS_SOCKET_HAS_IPV6
    if (config.transport_selector == TRANS_TCP6 || config.transport_selector == TRANS_UDP6)
    {
      unsigned ipv6Flag = 1;
      if (os_sockSetsockopt (socket, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &ipv6Flag, sizeof (ipv6Flag)))
      {
        print_sockerror ("IPV6_UNICAST_HOPS");
        return -2;
      }
    }
    else
#endif
    if (config.transport_selector == TRANS_TCP || config.transport_selector == TRANS_UDP)
    {
      int one = 1;
      if (os_sockSetsockopt (socket, SOL_SOCKET, SO_DONTROUTE, (char *) &one, sizeof (one)) != os_resultSuccess)
      {
        print_sockerror ("SO_DONTROUTE");
        return -2;
      }
    }
  }
  return 0;
}

static int set_reuse_options (os_socket socket)
{
  /* Set REUSEADDR (if available on platform) for
     multicast sockets, leave unicast sockets alone. */
  int one = 1;

  if (os_sockSetsockopt (socket, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof (one)) != os_resultSuccess)
  {
    print_sockerror ("SO_REUSEADDR");
    return -2;
  }
  return 0;
}

static int bind_socket (os_socket socket, unsigned short port)
{
  os_result rc;

#if OS_SOCKET_HAS_IPV6
  if (config.transport_selector == TRANS_TCP6 || config.transport_selector == TRANS_UDP6)
  {
    os_sockaddr_in6 socketname;
    memset (&socketname, 0, sizeof (socketname));
    socketname.sin6_family = AF_INET6;
    socketname.sin6_port = htons (port);
    socketname.sin6_addr = os_in6addr_any;
    if (IN6_IS_ADDR_LINKLOCAL (&socketname.sin6_addr)) {
      socketname.sin6_scope_id = gv.interfaceNo;
    }
    rc = os_sockBind (socket, (struct sockaddr *) &socketname, sizeof (socketname));
  }
  else
#endif
  if (config.transport_selector == TRANS_TCP || config.transport_selector == TRANS_UDP)
  {
    struct sockaddr_in socketname;
    socketname.sin_family = AF_INET;
    socketname.sin_port = htons (port);
    socketname.sin_addr.s_addr = htonl (INADDR_ANY);
    rc = os_sockBind (socket, (struct sockaddr *) &socketname, sizeof (socketname));
  }
  else
  {
    rc = os_resultFail;
  }
  if (rc != os_resultSuccess)
  {
    if (os_getErrno () != os_sockEADDRINUSE)
    {
      print_sockerror ("bind");
    }
  }
  return (rc == os_resultSuccess) ? 0 : -1;
}

#if OS_SOCKET_HAS_IPV6
static int set_mc_options_transmit_ipv6 (os_socket socket)
{
  unsigned interfaceNo = gv.interfaceNo;
  unsigned ttl = (unsigned) config.multicast_ttl;
  unsigned loop;
  if (os_sockSetsockopt (socket, IPPROTO_IPV6, IPV6_MULTICAST_IF, &interfaceNo, sizeof (interfaceNo)) != os_resultSuccess)
  {
    print_sockerror ("IPV6_MULTICAST_IF");
    return -2;
  }
  if (os_sockSetsockopt (socket, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (char *) &ttl, sizeof (ttl)) != os_resultSuccess)
  {
    print_sockerror ("IPV6_MULTICAST_HOPS");
    return -2;
  }
  loop = (unsigned) !!config.enableMulticastLoopback;
  if (os_sockSetsockopt (socket, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop, sizeof (loop)) != os_resultSuccess)
  {
    print_sockerror ("IPV6_MULTICAST_LOOP");
    return -2;
  }
  return 0;
}
#endif

static int set_mc_options_transmit_ipv4 (os_socket socket)
{
  unsigned char ttl = (unsigned char) config.multicast_ttl;
  unsigned char loop;
  os_result ret;

#if defined __linux || defined __APPLE__
  if (config.use_multicast_if_mreqn)
  {
    struct ip_mreqn mreqn;
    memset (&mreqn, 0, sizeof (mreqn));
    /* looks like imr_multiaddr is not relevant, not sure about imr_address */
    mreqn.imr_multiaddr.s_addr = htonl (INADDR_ANY);
    if (config.use_multicast_if_mreqn > 1)
      memcpy (&mreqn.imr_address.s_addr, gv.ownloc.address + 12, 4);
    else
      mreqn.imr_address.s_addr = htonl (INADDR_ANY);
    mreqn.imr_ifindex = (int) gv.interfaceNo;
    ret = os_sockSetsockopt (socket, IPPROTO_IP, IP_MULTICAST_IF, &mreqn, sizeof (mreqn));
  }
  else
#endif
  {
    ret = os_sockSetsockopt (socket, IPPROTO_IP, IP_MULTICAST_IF, gv.ownloc.address + 12, 4);
  }
  if (ret != os_resultSuccess)
  {
    print_sockerror ("IP_MULTICAST_IF");
    return -2;
  }
  if (os_sockSetsockopt (socket, IPPROTO_IP, IP_MULTICAST_TTL, (char *) &ttl, sizeof (ttl)) != os_resultSuccess)
  {
    print_sockerror ("IP_MULICAST_TTL");
    return -2;
  }
  loop = (unsigned char) config.enableMulticastLoopback;

  if (os_sockSetsockopt (socket, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof (loop)) != os_resultSuccess)
  {
    print_sockerror ("IP_MULTICAST_LOOP");
    return -2;
  }
  return 0;
}

static int set_mc_options_transmit (os_socket socket)
{
#if OS_SOCKET_HAS_IPV6
  if (config.transport_selector == TRANS_TCP6 || config.transport_selector == TRANS_UDP6)
  {
    return set_mc_options_transmit_ipv6 (socket);
  }
  else
#endif
  if (config.transport_selector == TRANS_TCP || config.transport_selector == TRANS_UDP)
  {
    return set_mc_options_transmit_ipv4 (socket);
  }
  else
  {
    return -2;
  }
}

int make_socket
(
  os_socket * sock,
  unsigned short port,
  bool stream,
  bool reuse
)
{
  int rc = -2;

#if OS_SOCKET_HAS_IPV6
  if (config.transport_selector == TRANS_TCP6 || config.transport_selector == TRANS_UDP6)
  {
    *sock = os_sockNew (AF_INET6, stream ? SOCK_STREAM : SOCK_DGRAM);
  }
  else
#endif
  if (config.transport_selector == TRANS_TCP || config.transport_selector == TRANS_UDP)
  {
    *sock = os_sockNew (AF_INET, stream ? SOCK_STREAM : SOCK_DGRAM);
  }
  else
  {
    return -2;
  }

  if (! OS_VALID_SOCKET (*sock))
  {
    print_sockerror ("socket");
    return rc;
  }

  if (port && reuse && ((rc = set_reuse_options (*sock)) < 0))
  {
    goto fail;
  }

  if
  (
    (rc = set_rcvbuf (*sock) < 0) ||
    (rc = set_sndbuf (*sock) < 0) ||
    ((rc = maybe_set_dont_route (*sock)) < 0) ||
    ((rc = bind_socket (*sock, port)) < 0)
  )
  {
    goto fail;
  }

  if (! stream)
  {
    if ((rc = set_mc_options_transmit (*sock)) < 0)
    {
      goto fail;
    }
  }

  if (stream)
  {
#ifdef SO_NOSIGPIPE
    set_socket_nosigpipe (*sock);
#endif
#ifdef TCP_NODELAY
    if (config.tcp_nodelay)
    {
      set_socket_nodelay (*sock);
    }
#endif
  }

  return 0;

fail:

  os_sockFree (*sock);
  *sock = OS_INVALID_SOCKET;
  return rc;
}

static int multicast_override(const char *ifname)
{
  char *copy = os_strdup (config.assumeMulticastCapable), *cursor = copy, *tok;
  int match = 0;
  if (copy != NULL)
  {
    while ((tok = os_strsep (&cursor, ",")) != NULL)
    {
      if (ddsi2_patmatch (tok, ifname))
        match = 1;
    }
  }
  os_free (copy);
  return match;
}

#ifdef __linux
/* FIMXE: HACK HACK */
#include <linux/if_packet.h>
#endif

int find_own_ip (const char *requested_address)
{
  const char *sep = " ";
  char last_if_name[80] = "";
  int quality = -1;
  int i;
  os_ifaddrs_t *ifa, *ifa_root = NULL;
  int maxq_list[MAX_INTERFACES];
  int maxq_count = 0;
  size_t maxq_strlen = 0;
  int selected_idx = -1;
  char addrbuf[DDSI_LOCSTRLEN];

  DDS_LOG(DDS_LC_CONFIG, "interfaces:");

  {
    int ret;
    ret = ddsi_enumerate_interfaces(gv.m_factory, &ifa_root);
    if (ret < 0) {
      DDS_ERROR("ddsi_enumerate_interfaces(%s): %d\n", gv.m_factory->m_typename, ret);
      return 0;
    }
  }

  gv.n_interfaces = 0;
  last_if_name[0] = 0;
  for (ifa = ifa_root; ifa != NULL; ifa = ifa->next)
  {
    char if_name[sizeof (last_if_name)];
    int q = 0;

    os_strlcpy(if_name, ifa->name, sizeof(if_name));

    if (strcmp (if_name, last_if_name))
      DDS_LOG(DDS_LC_CONFIG, "%s%s", sep, if_name);
    os_strlcpy(last_if_name, if_name, sizeof(last_if_name));

    /* interface must be up */
    if ((ifa->flags & IFF_UP) == 0) {
      DDS_LOG(DDS_LC_CONFIG, " (interface down)");
      continue;
    } else if (os_sockaddr_is_unspecified(ifa->addr)) {
      DDS_LOG(DDS_LC_CONFIG, " (address unspecified)");
      continue;
    }

#ifdef __linux
    if (ifa->addr->sa_family == AF_PACKET)
    {
      /* FIXME: weirdo warning warranted */
      nn_locator_t *l = &gv.interfaces[gv.n_interfaces].loc;
      l->kind = NN_LOCATOR_KIND_RAWETH;
      l->port = NN_LOCATOR_PORT_INVALID;
      memset(l->address, 0, 10);
      memcpy(l->address + 10, ((struct sockaddr_ll *)ifa->addr)->sll_addr, 6);
    }
    else
#endif
    {
      ddsi_ipaddr_to_loc(&gv.interfaces[gv.n_interfaces].loc, ifa->addr, gv.m_factory->m_kind);
    }
    ddsi_locator_to_string_no_port(addrbuf, sizeof(addrbuf), &gv.interfaces[gv.n_interfaces].loc);
    DDS_LOG(DDS_LC_CONFIG, " %s(", addrbuf);

    if (!(ifa->flags & IFF_MULTICAST) && multicast_override (if_name))
    {
      DDS_LOG(DDS_LC_CONFIG, "assume-mc:");
      ifa->flags |= IFF_MULTICAST;
    }

    if (ifa->flags & IFF_LOOPBACK)
    {
      /* Loopback device has the lowest priority of every interface
         available, because the other interfaces at least in principle
         allow communicating with other machines. */
      q += 0;
#if OS_SOCKET_HAS_IPV6
      if (!(ifa->addr->sa_family == AF_INET6 && IN6_IS_ADDR_LINKLOCAL (&((os_sockaddr_in6 *)ifa->addr)->sin6_addr)))
        q += 1;
#endif
    }
    else
    {
#if OS_SOCKET_HAS_IPV6
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
      if (!(ifa->addr->sa_family == AF_INET6 && IN6_IS_ADDR_LINKLOCAL (&((os_sockaddr_in6 *)ifa->addr)->sin6_addr)))
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

    DDS_LOG(DDS_LC_CONFIG, "q%d)", q);
    if (q == quality) {
      maxq_list[maxq_count] = gv.n_interfaces;
      maxq_strlen += 2 + strlen (if_name);
      maxq_count++;
    } else if (q > quality) {
      maxq_list[0] = gv.n_interfaces;
      maxq_strlen += 2 + strlen (if_name);
      maxq_count = 1;
      quality = q;
    }

    /* FIXME: HACK HACK */
    //ddsi_ipaddr_to_loc(&gv.interfaces[gv.n_interfaces].loc, &tmpip, gv.m_factory->m_kind);
    if (ifa->addr->sa_family == AF_INET || ifa->addr->sa_family == AF_INET6)
    {
      ddsi_ipaddr_to_loc(&gv.interfaces[gv.n_interfaces].netmask, ifa->netmask, gv.m_factory->m_kind);
    }
    else
    {
      gv.interfaces[gv.n_interfaces].netmask.kind = gv.m_factory->m_kind;
      gv.interfaces[gv.n_interfaces].netmask.port = NN_LOCATOR_PORT_INVALID;
      memset(&gv.interfaces[gv.n_interfaces].netmask.address, 0, sizeof(gv.interfaces[gv.n_interfaces].netmask.address));
    }
    gv.interfaces[gv.n_interfaces].mc_capable = ((ifa->flags & IFF_MULTICAST) != 0);
    gv.interfaces[gv.n_interfaces].point_to_point = ((ifa->flags & IFF_POINTOPOINT) != 0);
    gv.interfaces[gv.n_interfaces].if_index = ifa->index;
    gv.interfaces[gv.n_interfaces].name = os_strdup (if_name);
    gv.n_interfaces++;
  }
  DDS_LOG(DDS_LC_CONFIG, "\n");
  os_freeifaddrs (ifa_root);

  if (requested_address == NULL)
  {
    if (maxq_count > 1)
    {
      const int idx = maxq_list[0];
      char *names;
      int p;
      ddsi_locator_to_string_no_port (addrbuf, sizeof(addrbuf), &gv.interfaces[idx].loc);
      names = os_malloc (maxq_strlen + 1);
      p = 0;
      for (i = 0; i < maxq_count && (size_t) p < maxq_strlen; i++)
        p += snprintf (names + p, maxq_strlen - (size_t) p, ", %s", gv.interfaces[maxq_list[i]].name);
      DDS_WARNING("using network interface %s (%s) selected arbitrarily from: %s\n",
                   gv.interfaces[idx].name, addrbuf, names + 2);
      os_free (names);
    }

    if (maxq_count > 0)
      selected_idx = maxq_list[0];
    else
      DDS_ERROR("failed to determine default own IP address\n");
  }
  else
  {
    nn_locator_t req;
    /* Presumably an interface name */
    for (i = 0; i < gv.n_interfaces; i++)
    {
      if (strcmp (gv.interfaces[i].name, config.networkAddressString) == 0)
        break;
    }
    if (i < gv.n_interfaces)
      ; /* got a match */
    else if (ddsi_locator_from_string(&req, config.networkAddressString) != AFSR_OK)
      ; /* not good, i = gv.n_interfaces, so error handling will kick in */
    else
    {
      /* Try an exact match on the address */
      for (i = 0; i < gv.n_interfaces; i++)
        if (compare_locators(&gv.interfaces[i].loc, &req) == 0)
          break;
      if (i == gv.n_interfaces && req.kind == NN_LOCATOR_KIND_UDPv4)
      {
        /* Try matching on network portion only, where the network
           portion is based on the netmask of the interface under
           consideration */
        for (i = 0; i < gv.n_interfaces; i++)
        {
          uint32_t req1, ip1, nm1;
          memcpy (&req1, req.address + 12, sizeof (req1));
          memcpy (&ip1, gv.interfaces[i].loc.address + 12, sizeof (ip1));
          memcpy (&nm1, gv.interfaces[i].netmask.address + 12, sizeof (nm1));

          /* If the host portion of the requested address is non-zero,
             skip this interface */
          if (req1 & ~nm1)
            continue;

          if ((req1 & nm1) == (ip1 & nm1))
            break;
        }
      }
    }

    if (i < gv.n_interfaces)
      selected_idx = i;
    else
      DDS_ERROR("%s: does not match an available interface\n", config.networkAddressString);
  }

  if (selected_idx < 0)
    return 0;
  else
  {
    gv.ownloc = gv.interfaces[selected_idx].loc;
    gv.selected_interface = selected_idx;
    gv.interfaceNo = gv.interfaces[selected_idx].if_index;
#if OS_SOCKET_HAS_IPV6
    if (gv.extloc.kind == NN_LOCATOR_KIND_TCPv6 || gv.extloc.kind == NN_LOCATOR_KIND_UDPv6)
    {
      os_sockaddr_in6 addr;
      memcpy(&addr.sin6_addr, gv.ownloc.address, sizeof(addr.sin6_addr));
      gv.ipv6_link_local = IN6_IS_ADDR_LINKLOCAL (&addr.sin6_addr) != 0;
    }
    else
    {
      gv.ipv6_link_local = 0;
    }
#endif
    DDS_LOG(DDS_LC_CONFIG, "selected interface: %s (index %u)\n",
            gv.interfaces[selected_idx].name, gv.interfaceNo);

    return 1;
  }
}
