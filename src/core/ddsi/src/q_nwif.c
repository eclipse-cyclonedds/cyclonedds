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

#ifndef _WIN32
#include <netdb.h>
#endif

#include "ddsi/q_log.h"
#include "ddsi/q_nwif.h"
#include "ddsi/q_globals.h"
#include "ddsi/q_config.h"
#include "ddsi/q_unused.h"
#include "ddsi/q_md5.h"
#include "ddsi/q_misc.h"
#include "ddsi/q_addrset.h" /* unspec locator */
#include "ddsi/q_feature_check.h"
#include "util/ut_avl.h"


struct nn_group_membership_node {
  ut_avlNode_t avlnode;
  os_socket sock;
  os_sockaddr_storage srcip;
  os_sockaddr_storage mcip;
  unsigned count;
};

struct nn_group_membership {
  os_mutex lock;
  ut_avlTree_t mships;
};

static int sockaddr_compare_no_port (const os_sockaddr_storage *as, const os_sockaddr_storage *bs)
{
  if (as->ss_family != bs->ss_family)
    return (as->ss_family < bs->ss_family) ? -1 : 1;
  else if (as->ss_family == 0)
    return 0; /* unspec address */
  else if (as->ss_family == AF_INET)
  {
    const os_sockaddr_in *a = (const os_sockaddr_in *) as;
    const os_sockaddr_in *b = (const os_sockaddr_in *) bs;
    if (a->sin_addr.s_addr != b->sin_addr.s_addr)
      return (a->sin_addr.s_addr < b->sin_addr.s_addr) ? -1 : 1;
    else
      return 0;
  }
#if OS_SOCKET_HAS_IPV6
  else if (as->ss_family == AF_INET6)
  {
    const os_sockaddr_in6 *a = (const os_sockaddr_in6 *) as;
    const os_sockaddr_in6 *b = (const os_sockaddr_in6 *) bs;
    int c;
    if ((c = memcmp (&a->sin6_addr, &b->sin6_addr, 16)) != 0)
      return c;
    else
      return 0;
  }
#endif
  else
  {
    assert (0);
    return 0;
  }
}

static int cmp_group_membership (const void *va, const void *vb)
{
  const struct nn_group_membership_node *a = va;
  const struct nn_group_membership_node *b = vb;
  int c;
  if (a->sock < b->sock)
    return -1;
  else if (a->sock > b->sock)
    return 1;
  else if ((c = sockaddr_compare_no_port (&a->srcip, &b->srcip)) != 0)
    return c;
  else if ((c = sockaddr_compare_no_port (&a->mcip, &b->mcip)) != 0)
    return c;
  else
    return 0;
}

static ut_avlTreedef_t mship_td = UT_AVL_TREEDEF_INITIALIZER(offsetof (struct nn_group_membership_node, avlnode), 0, cmp_group_membership, 0);

struct nn_group_membership *new_group_membership (void)
{
  struct nn_group_membership *mship = os_malloc (sizeof (*mship));
  os_mutexInit (&mship->lock);
  ut_avlInit (&mship_td, &mship->mships);
  return mship;
}

void free_group_membership (struct nn_group_membership *mship)
{
  ut_avlFree (&mship_td, &mship->mships, os_free);
  os_mutexDestroy (&mship->lock);
  os_free (mship);
}

static int reg_group_membership (struct nn_group_membership *mship, os_socket sock, const os_sockaddr_storage *srcip, const os_sockaddr_storage *mcip)
{
  struct nn_group_membership_node key, *n;
  ut_avlIPath_t ip;
  int isnew;
  key.sock = sock;
  if (srcip)
    key.srcip = *srcip;
  else
    memset (&key.srcip, 0, sizeof (key.srcip));
  key.mcip = *mcip;
  if ((n = ut_avlLookupIPath (&mship_td, &mship->mships, &key, &ip)) != NULL) {
    isnew = 0;
    n->count++;
  } else {
    isnew = 1;
    n = os_malloc (sizeof (*n));
    n->sock = sock;
    n->srcip = key.srcip;
    n->mcip = key.mcip;
    n->count = 1;
    ut_avlInsertIPath (&mship_td, &mship->mships, n, &ip);
  }
  return isnew;
}

static int unreg_group_membership (struct nn_group_membership *mship, os_socket sock, const os_sockaddr_storage *srcip, const os_sockaddr_storage *mcip)
{
  struct nn_group_membership_node key, *n;
  ut_avlDPath_t dp;
  int mustdel;
  key.sock = sock;
  if (srcip)
    key.srcip = *srcip;
  else
    memset (&key.srcip, 0, sizeof (key.srcip));
  key.mcip = *mcip;
  n = ut_avlLookupDPath (&mship_td, &mship->mships, &key, &dp);
  assert (n != NULL);
  assert (n->count > 0);
  if (--n->count > 0)
    mustdel = 0;
  else
  {
    mustdel = 1;
    ut_avlDeleteDPath (&mship_td, &mship->mships, n, &dp);
    os_free (n);
  }
  return mustdel;
}

void nn_loc_to_address (os_sockaddr_storage *dst, const nn_locator_t *src)
{
  memset (dst, 0, sizeof (*dst));
  switch (src->kind)
  {
    case NN_LOCATOR_KIND_INVALID:
#if OS_SOCKET_HAS_IPV6
      dst->ss_family = config.useIpv6 ? AF_INET6 : AF_INET;
#else
      dst->ss_family = AF_INET;
#endif
      break;
    case NN_LOCATOR_KIND_UDPv4:
    case NN_LOCATOR_KIND_TCPv4:
    {
      os_sockaddr_in *x = (os_sockaddr_in *) dst;
      x->sin_family = AF_INET;
      x->sin_port = htons ((unsigned short) src->port);
      memcpy (&x->sin_addr.s_addr, src->address + 12, 4);
      break;
    }
#if OS_SOCKET_HAS_IPV6
    case NN_LOCATOR_KIND_UDPv6:
    case NN_LOCATOR_KIND_TCPv6:
    {
      os_sockaddr_in6 *x = (os_sockaddr_in6 *) dst;
      x->sin6_family = AF_INET6;
      x->sin6_port = htons ((unsigned short) src->port);
      memcpy (&x->sin6_addr.s6_addr, src->address, 16);
      if (IN6_IS_ADDR_LINKLOCAL (&x->sin6_addr))
      {
        x->sin6_scope_id = gv.interfaceNo;
      }
      break;
    }
#endif
    case NN_LOCATOR_KIND_UDPv4MCGEN:
      NN_ERROR ("nn_address_to_loc: kind %x unsupported\n", src->kind);
      break;
    default:
      break;
  }
}

void nn_address_to_loc (nn_locator_t *dst, const os_sockaddr_storage *src, int32_t kind)
{
  dst->kind = kind;
  switch (src->ss_family)
  {
    case AF_INET:
    {
      const os_sockaddr_in *x = (const os_sockaddr_in *) src;
      assert (kind == NN_LOCATOR_KIND_UDPv4 || kind == NN_LOCATOR_KIND_TCPv4);
      if (x->sin_addr.s_addr == htonl (INADDR_ANY))
        set_unspec_locator (dst);
      else
      {
        dst->port = ntohs (x->sin_port);
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
        set_unspec_locator (dst);
      else
      {
        dst->port = ntohs (x->sin6_port);
        memcpy (dst->address, &x->sin6_addr.s6_addr, 16);
      }
      break;
    }
#endif
    default:
      NN_FATAL ("nn_address_to_loc: family %d unsupported\n", (int) src->ss_family);
  }
}

void print_sockerror (const char *msg)
{
  int err = os_getErrno ();
  NN_ERROR ("SOCKET %s errno %d\n", msg, err);
}

unsigned short sockaddr_get_port (const os_sockaddr_storage *addr)
{
  if (addr->ss_family == AF_INET)
    return ntohs (((os_sockaddr_in *) addr)->sin_port);
#if OS_SOCKET_HAS_IPV6
  else
    return ntohs (((os_sockaddr_in6 *) addr)->sin6_port);
#endif
}

void sockaddr_set_port (os_sockaddr_storage *addr, unsigned short port)
{
  if (addr->ss_family == AF_INET)
    ((os_sockaddr_in *) addr)->sin_port = htons (port);
#if OS_SOCKET_HAS_IPV6
  else
    ((os_sockaddr_in6 *) addr)->sin6_port = htons (port);
#endif
}

char *sockaddr_to_string_with_port (char addrbuf[INET6_ADDRSTRLEN_EXTENDED], const os_sockaddr_storage *src)
{
  size_t pos;
  int n;
  switch (src->ss_family)
  {
    case AF_INET:
      os_sockaddrAddressToString ((const os_sockaddr *) src, addrbuf, INET6_ADDRSTRLEN);
      pos = strlen (addrbuf);
      assert(pos <= INET6_ADDRSTRLEN_EXTENDED);
      n = snprintf (addrbuf + pos, INET6_ADDRSTRLEN_EXTENDED - pos, ":%u", ntohs (((os_sockaddr_in *) src)->sin_port));
      assert (n < INET6_ADDRSTRLEN_EXTENDED);
      (void)n;
      break;
#if OS_SOCKET_HAS_IPV6
    case AF_INET6:
      addrbuf[0] = '[';
      os_sockaddrAddressToString ((const os_sockaddr *) src, addrbuf + 1, INET6_ADDRSTRLEN);
      pos = strlen (addrbuf);
      assert(pos <= INET6_ADDRSTRLEN_EXTENDED);
      n = snprintf (addrbuf + pos, INET6_ADDRSTRLEN_EXTENDED - pos, "]:%u", ntohs (((os_sockaddr_in6 *) src)->sin6_port));
      assert (n < INET6_ADDRSTRLEN_EXTENDED);
      (void)n;
      break;
#endif
    default:
      NN_WARNING ("sockaddr_to_string_with_port: unknown address family\n");
      strcpy (addrbuf, "???");
      break;
  }
  return addrbuf;
}

char *sockaddr_to_string_no_port (char addrbuf[INET6_ADDRSTRLEN_EXTENDED], const os_sockaddr_storage *src)
{
  return os_sockaddrAddressToString ((const os_sockaddr *) src, addrbuf, INET6_ADDRSTRLEN);
}

char *locator_to_string_with_port (char addrbuf[INET6_ADDRSTRLEN_EXTENDED], const nn_locator_t *loc)
{
  os_sockaddr_storage addr;
  nn_locator_t tmploc = *loc;
  if (tmploc.kind == NN_LOCATOR_KIND_UDPv4MCGEN)
  {
    nn_udpv4mcgen_address_t *x = (nn_udpv4mcgen_address_t *) &tmploc.address;
    memmove(tmploc.address + 12, &x->ipv4, 4);
    memset(tmploc.address, 0, 12);
    tmploc.kind = NN_LOCATOR_KIND_UDPv4;
  }
  nn_loc_to_address (&addr, &tmploc);
  return sockaddr_to_string_with_port (addrbuf, &addr);
}

char *locator_to_string_no_port (char addrbuf[INET6_ADDRSTRLEN_EXTENDED], const nn_locator_t *loc)
{
  os_sockaddr_storage addr;
  nn_locator_t tmploc = *loc;
  if (tmploc.kind == NN_LOCATOR_KIND_UDPv4MCGEN)
  {
    nn_udpv4mcgen_address_t *x = (nn_udpv4mcgen_address_t *) &tmploc.address;
    memmove(tmploc.address + 12, &x->ipv4, 4);
    memset(tmploc.address, 0, 12);
    tmploc.kind = NN_LOCATOR_KIND_UDPv4;
  }
  nn_loc_to_address (&addr, &tmploc);
  return sockaddr_to_string_no_port (addrbuf, &addr);
}

unsigned sockaddr_to_hopefully_unique_uint32 (const os_sockaddr_storage *src)
{
  switch (src->ss_family)
  {
    case AF_INET:
      return (unsigned) ((const os_sockaddr_in *) src)->sin_addr.s_addr;
#if OS_SOCKET_HAS_IPV6
    case AF_INET6:
    {
      unsigned id;
      md5_state_t st;
      md5_byte_t digest[16];
      md5_init (&st);
      md5_append (&st, (const md5_byte_t *) ((const os_sockaddr_in6 *) src)->sin6_addr.s6_addr, 16);
      md5_finish (&st, digest);
      memcpy (&id, digest, sizeof (id));
      return id;
    }
#endif
    default:
      NN_FATAL ("sockaddr_to_hopefully_unique_uint32: unknown address family\n");
      return 0;
  }
}

unsigned short get_socket_port (os_socket socket)
{
  os_sockaddr_storage addr;
  socklen_t addrlen = sizeof (addr);
  if (getsockname (socket, (os_sockaddr *) &addr, &addrlen) < 0)
  {
    print_sockerror ("getsockname");
    return 0;
  }
  switch (addr.ss_family)
  {
    case AF_INET:
      return ntohs (((os_sockaddr_in *) &addr)->sin_port);
#if OS_SOCKET_HAS_IPV6
    case AF_INET6:
      return ntohs (((os_sockaddr_in6 *) &addr)->sin6_port);
#endif
    default:
      abort ();
      return 0;
  }
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
      /* NN_ERROR does more than just nn_log(LC_ERROR), hence the duplication */
      if (config.socket_min_rcvbuf_size.isdefault)
        nn_log (LC_CONFIG, "failed to increase socket receive buffer size to %u bytes, continuing with %u bytes\n", socket_min_rcvbuf_size, ReceiveBufferSize);
      else
        NN_ERROR ("failed to increase socket receive buffer size to %u bytes, continuing with %u bytes\n", socket_min_rcvbuf_size, ReceiveBufferSize);
    }
    else
    {
      nn_log (LC_CONFIG, "socket receive buffer size set to %u bytes\n", ReceiveBufferSize);
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
    if (config.useIpv6)
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

static int interface_in_recvips_p (const struct nn_interface *interf)
{
  struct ospl_in_addr_node *nodeaddr;
  for (nodeaddr = gv.recvips; nodeaddr; nodeaddr = nodeaddr->next)
  {
    if (os_sockaddrIPAddressEqual ((const os_sockaddr *) &nodeaddr->addr, (const os_sockaddr *) &interf->addr))
      return 1;
  }
  return 0;
}

static int bind_socket (os_socket socket, unsigned short port)
{
  int rc;

#if OS_SOCKET_HAS_IPV6
  if (config.useIpv6)
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
  {
    struct sockaddr_in socketname;
    socketname.sin_family = AF_INET;
    socketname.sin_port = htons (port);
    socketname.sin_addr.s_addr = htonl (INADDR_ANY);
    rc = os_sockBind (socket, (struct sockaddr *) &socketname, sizeof (socketname));
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
      mreqn.imr_address.s_addr = ((os_sockaddr_in *) &gv.ownip)->sin_addr.s_addr;
    else
      mreqn.imr_address.s_addr = htonl (INADDR_ANY);
    mreqn.imr_ifindex = (int) gv.interfaceNo;
    ret = os_sockSetsockopt (socket, IPPROTO_IP, IP_MULTICAST_IF, &mreqn, sizeof (mreqn));
  }
  else
#endif
  {
    ret = os_sockSetsockopt (socket, IPPROTO_IP, IP_MULTICAST_IF, (char *) &((os_sockaddr_in *) &gv.ownip)->sin_addr, sizeof (((os_sockaddr_in *) &gv.ownip)->sin_addr));
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
  if (config.useIpv6)
  {
    return set_mc_options_transmit_ipv6 (socket);
  }
  else
#endif
  {
    return set_mc_options_transmit_ipv4 (socket);
  }
}

static int joinleave_asm_mcgroup (os_socket socket, int join, const os_sockaddr_storage *mcip, const struct nn_interface *interf)
{
  int rc;
#if OS_SOCKET_HAS_IPV6
  if (config.useIpv6)
  {
    os_ipv6_mreq ipv6mreq;
    memset (&ipv6mreq, 0, sizeof (ipv6mreq));
    memcpy (&ipv6mreq.ipv6mr_multiaddr, &((os_sockaddr_in6 *) mcip)->sin6_addr, sizeof (ipv6mreq.ipv6mr_multiaddr));
    ipv6mreq.ipv6mr_interface = interf ? interf->if_index : 0;
    rc = os_sockSetsockopt (socket, IPPROTO_IPV6, join ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP, &ipv6mreq, sizeof (ipv6mreq));
  }
  else
#endif
  {
    struct ip_mreq mreq;
    mreq.imr_multiaddr = ((os_sockaddr_in *) mcip)->sin_addr;
    if (interf)
      mreq.imr_interface = ((os_sockaddr_in *) &interf->addr)->sin_addr;
    else
      mreq.imr_interface.s_addr = htonl (INADDR_ANY);
    rc = os_sockSetsockopt (socket, IPPROTO_IP, join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP, (char *) &mreq, sizeof (mreq));
  }
  return (rc == -1) ? os_getErrno() : 0;
}

#ifdef DDSI_INCLUDE_SSM
static int joinleave_ssm_mcgroup (os_socket socket, int join, const os_sockaddr_storage *srcip, const os_sockaddr_storage *mcip, const struct nn_interface *interf)
{
  int rc;
#if OS_SOCKET_HAS_IPV6
  if (config.useIpv6)
  {
    struct group_source_req gsr;
    memset (&gsr, 0, sizeof (gsr));
    gsr.gsr_interface = interf ? interf->if_index : 0;
    memcpy (&gsr.gsr_group, mcip, sizeof (gsr.gsr_group));
    memcpy (&gsr.gsr_source, srcip, sizeof (gsr.gsr_source));
    rc = os_sockSetsockopt (socket, IPPROTO_IPV6, join ? MCAST_JOIN_SOURCE_GROUP : MCAST_LEAVE_SOURCE_GROUP, &gsr, sizeof (gsr));
  }
  else
#endif
  {
    struct ip_mreq_source mreq;
    memset (&mreq, 0, sizeof (mreq));
    mreq.imr_sourceaddr = ((os_sockaddr_in *) srcip)->sin_addr;
    mreq.imr_multiaddr = ((os_sockaddr_in *) mcip)->sin_addr;
    if (interf)
      mreq.imr_interface = ((os_sockaddr_in *) &interf->addr)->sin_addr;
    else
      mreq.imr_interface.s_addr = INADDR_ANY;
    rc = os_sockSetsockopt (socket, IPPROTO_IP, join ? IP_ADD_SOURCE_MEMBERSHIP : IP_DROP_SOURCE_MEMBERSHIP, &mreq, sizeof (mreq));
  }
  return (rc == -1) ? os_getErrno() : 0;
}
#endif

static char *make_joinleave_msg (char *buf, size_t bufsz, os_socket socket, int join, const os_sockaddr_storage *srcip, const os_sockaddr_storage *mcip, const struct nn_interface *interf, int err)
{
  char mcstr[INET6_ADDRSTRLEN_EXTENDED], srcstr[INET6_ADDRSTRLEN_EXTENDED], interfstr[INET6_ADDRSTRLEN_EXTENDED];
  int n;
#ifdef DDSI_INCLUDE_SSM
  if (srcip)
    sockaddr_to_string_no_port(srcstr, srcip);
  else
    strcpy (srcstr, "*");
#else
  OS_UNUSED_ARG (srcip);
  strcpy (srcstr, "*");
#endif
  sockaddr_to_string_no_port (mcstr, mcip);
  if (interf)
    sockaddr_to_string_no_port(interfstr, &interf->addr);
  else
    (void) snprintf (interfstr, sizeof (interfstr), "(default)");
  n = err ? snprintf (buf, bufsz, "error %d in ", err) : 0;
  if ((size_t) n  < bufsz)
    (void) snprintf (buf + n, bufsz - (size_t) n, "%s socket %lu for (%s, %s) interface %s", join ? "join" : "leave", (unsigned long) socket, mcstr, srcstr, interfstr);
  return buf;
}

static int joinleave_mcgroup (os_socket socket, int join, const os_sockaddr_storage *srcip, const os_sockaddr_storage *mcip, const struct nn_interface *interf)
{
  char buf[256];
  int err;
  nn_log (LC_DISCOVERY, "%s\n", make_joinleave_msg (buf, sizeof(buf), socket, join, srcip, mcip, interf, 0));
#ifdef DDSI_INCLUDE_SSM
  if (srcip)
    err = joinleave_ssm_mcgroup(socket, join, srcip, mcip, interf);
  else
    err = joinleave_asm_mcgroup(socket, join, mcip, interf);
#else
  assert (srcip == NULL);
  err = joinleave_asm_mcgroup(socket, join, mcip, interf);
#endif
  if (err)
    NN_WARNING ("%s\n", make_joinleave_msg (buf, sizeof(buf), socket, join, srcip, mcip, interf, err));
  return err ? -1 : 0;
}

static int joinleave_mcgroups (os_socket socket, int join, const os_sockaddr_storage *srcip, const os_sockaddr_storage *mcip)
{
  int rc;
  switch (gv.recvips_mode)
  {
    case RECVIPS_MODE_NONE:
      break;
    case RECVIPS_MODE_ANY:
      /* User has specified to use the OS default interface */
      if ((rc = joinleave_mcgroup (socket, join, srcip, mcip, NULL)) < 0)
        return rc;
      break;
    case RECVIPS_MODE_PREFERRED:
      if (gv.interfaces[gv.selected_interface].mc_capable)
        return joinleave_mcgroup (socket, join, srcip, mcip, &gv.interfaces[gv.selected_interface]);
      return 0;
    case RECVIPS_MODE_ALL:
    case RECVIPS_MODE_SOME:
      {
        int i, fails = 0, oks = 0;
        for (i = 0; i < gv.n_interfaces; i++)
        {
          if (gv.interfaces[i].mc_capable)
          {
            if (gv.recvips_mode == RECVIPS_MODE_ALL || interface_in_recvips_p (&gv.interfaces[i]))
            {
              if ((rc = joinleave_mcgroup (socket, join, srcip, mcip, &gv.interfaces[i])) < 0)
                fails++;
              else
                oks++;
            }
          }
        }
        if (fails > 0)
        {
          if (oks > 0)
            TRACE (("multicast join failed for some but not all interfaces, proceeding\n"));
          else
            return -2;
        }
      }
      break;
  }
  return 0;
}

int join_mcgroups (struct nn_group_membership *mship, os_socket socket, const os_sockaddr_storage *srcip, const os_sockaddr_storage *mcip)
{
  int ret;
  os_mutexLock (&mship->lock);
  if (!reg_group_membership (mship, socket, srcip, mcip))
  {
    char buf[256];
    TRACE (("%s: already joined\n", make_joinleave_msg (buf, sizeof(buf), socket, 1, srcip, mcip, NULL, 0)));
    ret = 0;
  }
  else
  {
    ret = joinleave_mcgroups (socket, 1, srcip, mcip);
  }
  os_mutexUnlock (&mship->lock);
  return ret;
}

int leave_mcgroups (struct nn_group_membership *mship, os_socket socket, const os_sockaddr_storage *srcip, const os_sockaddr_storage *mcip)
{
  int ret;
  os_mutexLock (&mship->lock);
  if (!unreg_group_membership (mship, socket, srcip, mcip))
  {
    char buf[256];
    TRACE (("%s: not leaving yet\n", make_joinleave_msg (buf, sizeof(buf), socket, 0, srcip, mcip, NULL, 0)));
    ret = 0;
  }
  else
  {
    ret = joinleave_mcgroups (socket, 0, srcip, mcip);
  }
  os_mutexUnlock (&mship->lock);
  return ret;
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
  if (config.useIpv6)
  {
    *sock = os_sockNew (AF_INET6, stream ? SOCK_STREAM : SOCK_DGRAM);
  }
  else
#endif
  {
    *sock = os_sockNew (AF_INET, stream ? SOCK_STREAM : SOCK_DGRAM);
  }

  if (! Q_VALID_SOCKET (*sock))
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
  *sock = Q_INVALID_SOCKET;
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

int find_own_ip (const char *requested_address)
{
  const char *sep = " ";
  char last_if_name[80] = "";
  int quality = -1;
  os_result res;
  int i;
  unsigned int nif;
  os_ifAttributes *ifs;
  int maxq_list[MAX_INTERFACES];
  int maxq_count = 0;
  size_t maxq_strlen = 0;
  int selected_idx = -1;
  char addrbuf[INET6_ADDRSTRLEN_EXTENDED];

  ifs = os_malloc (MAX_INTERFACES * sizeof (*ifs));

  nn_log (LC_CONFIG, "interfaces:");

  if (config.useIpv6)
    res = os_sockQueryIPv6Interfaces (ifs, MAX_INTERFACES, &nif);
  else
    res = os_sockQueryInterfaces (ifs, MAX_INTERFACES, &nif);
  if (res != os_resultSuccess)
  {
    NN_ERROR ("os_sockQueryInterfaces: %d\n", (int) res);
    os_free (ifs);
    return 0;
  }

  gv.n_interfaces = 0;
  last_if_name[0] = 0;
  for (i = 0; i < (int) nif; i++, sep = ", ")
  {
    os_sockaddr_storage tmpip, tmpmask;
    char if_name[sizeof (last_if_name)];
    int q = 0;

    strncpy (if_name, ifs[i].name, sizeof (if_name) - 1);
    if_name[sizeof (if_name) - 1] = 0;

    if (strcmp (if_name, last_if_name))
      nn_log (LC_CONFIG, "%s%s", sep, if_name);
    strcpy (last_if_name, if_name);

    /* interface must be up */
    if ((ifs[i].flags & IFF_UP) == 0)
    {
      nn_log (LC_CONFIG, " (interface down)");
      continue;
    }

    tmpip = ifs[i].address;
    tmpmask = ifs[i].network_mask;
    sockaddr_to_string_no_port (addrbuf, &tmpip);
    nn_log (LC_CONFIG, " %s(", addrbuf);

    if (!(ifs[i].flags & IFF_MULTICAST) && multicast_override (if_name))
    {
      nn_log (LC_CONFIG, "assume-mc:");
      ifs[i].flags |= IFF_MULTICAST;
    }

    if (ifs[i].flags & IFF_LOOPBACK)
    {
      /* Loopback device has the lowest priority of every interface
         available, because the other interfaces at least in principle
         allow communicating with other machines. */
      q += 0;
#if OS_SOCKET_HAS_IPV6
      if (!(tmpip.ss_family == AF_INET6 && IN6_IS_ADDR_LINKLOCAL (&((os_sockaddr_in6 *) &tmpip)->sin6_addr)))
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
      if (!(tmpip.ss_family == AF_INET6 && IN6_IS_ADDR_LINKLOCAL (&((os_sockaddr_in6 *) &tmpip)->sin6_addr)))
        q += 5;
#endif

      /* We strongly prefer a multicast capable interface, if that's
         not available anything that's not point-to-point, or else we
         hope IP routing will take care of the issues. */
      if (ifs[i].flags & IFF_MULTICAST)
        q += 4;
      else if (!(ifs[i].flags & IFF_POINTOPOINT))
        q += 3;
      else
        q += 2;
    }

    nn_log (LC_CONFIG, "q%d)", q);
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

    gv.interfaces[gv.n_interfaces].addr = tmpip;
    gv.interfaces[gv.n_interfaces].netmask = tmpmask;
    gv.interfaces[gv.n_interfaces].mc_capable = ((ifs[i].flags & IFF_MULTICAST) != 0);
    gv.interfaces[gv.n_interfaces].point_to_point = ((ifs[i].flags & IFF_POINTOPOINT) != 0);
    gv.interfaces[gv.n_interfaces].if_index = ifs[i].interfaceIndexNo;
    gv.interfaces[gv.n_interfaces].name = os_strdup (if_name);
    gv.n_interfaces++;
  }
  nn_log (LC_CONFIG, "\n");
  os_free (ifs);

  if (requested_address == NULL)
  {
    if (maxq_count > 1)
    {
      const int idx = maxq_list[0];
      char *names;
      int p;
      sockaddr_to_string_no_port (addrbuf, &gv.interfaces[idx].addr);
      names = os_malloc (maxq_strlen + 1);
      p = 0;
      for (i = 0; i < maxq_count && (size_t) p < maxq_strlen; i++)
        p += snprintf (names + p, maxq_strlen - (size_t) p, ", %s", gv.interfaces[maxq_list[i]].name);
      NN_WARNING ("using network interface %s (%s) selected arbitrarily from: %s\n",
                   gv.interfaces[idx].name, addrbuf, names + 2);
      os_free (names);
    }

    if (maxq_count > 0)
      selected_idx = maxq_list[0];
    else
      NN_ERROR ("failed to determine default own IP address\n");
  }
  else
  {
    os_sockaddr_storage req;
    /* Presumably an interface name */
    for (i = 0; i < gv.n_interfaces; i++)
    {
      if (strcmp (gv.interfaces[i].name, config.networkAddressString) == 0)
        break;
    }
    if (i < gv.n_interfaces)
      ; /* got a match */
    else if (!os_sockaddrStringToAddress (config.networkAddressString, (os_sockaddr *) &req, !config.useIpv6))
      ; /* not good, i = gv.n_interfaces, so error handling will kick in */
    else
    {
      /* Try an exact match on the address */
      for (i = 0; i < gv.n_interfaces; i++)
        if (os_sockaddrIPAddressEqual ((os_sockaddr *) &gv.interfaces[i].addr, (os_sockaddr *) &req))
          break;
      if (i == gv.n_interfaces && !config.useIpv6)
      {
        /* Try matching on network portion only, where the network
           portion is based on the netmask of the interface under
           consideration */
        for (i = 0; i < gv.n_interfaces; i++)
        {
          os_sockaddr_storage req1 = req, ip1 = gv.interfaces[i].addr;
          assert (req1.ss_family == AF_INET);
          assert (ip1.ss_family == AF_INET);

          /* If the host portion of the requested address is non-zero,
             skip this interface */
          if (((os_sockaddr_in *) &req1)->sin_addr.s_addr &
              ~((os_sockaddr_in *) &gv.interfaces[i].netmask)->sin_addr.s_addr)
            continue;

          ((os_sockaddr_in *) &req1)->sin_addr.s_addr &=
            ((os_sockaddr_in *) &gv.interfaces[i].netmask)->sin_addr.s_addr;
          ((os_sockaddr_in *) &ip1)->sin_addr.s_addr &=
            ((os_sockaddr_in *) &gv.interfaces[i].netmask)->sin_addr.s_addr;
          if (os_sockaddrIPAddressEqual ((os_sockaddr *) &ip1, (os_sockaddr *) &req1))
            break;
        }
      }
    }

    if (i < gv.n_interfaces)
      selected_idx = i;
    else
      NN_ERROR ("%s: does not match an available interface\n", config.networkAddressString);
  }

  if (selected_idx < 0)
    return 0;
  else
  {
    gv.ownip = gv.interfaces[selected_idx].addr;
    sockaddr_set_port (&gv.ownip, 0);
    gv.selected_interface = selected_idx;
    gv.interfaceNo = gv.interfaces[selected_idx].if_index;
#if OS_SOCKET_HAS_IPV6
    if (config.useIpv6)
    {
      assert (gv.ownip.ss_family == AF_INET6);
      gv.ipv6_link_local =
        IN6_IS_ADDR_LINKLOCAL (&((os_sockaddr_in6 *) &gv.ownip)->sin6_addr) != 0;
    }
    else
    {
      gv.ipv6_link_local = 0;
    }
#endif
    nn_log (LC_CONFIG, "selected interface: %s (index %u)\n",
            gv.interfaces[selected_idx].name, gv.interfaceNo);

    return 1;
  }
}
