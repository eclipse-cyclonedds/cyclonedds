// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSRT_SOCKETS_POSIX_H
#define DDSRT_SOCKETS_POSIX_H

#if DDSRT_WITH_LWIP
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#else
#if __ZEPHYR__
#include <netdb.h>
#endif
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/select.h>
#endif

#include "dds/ddsrt/iovec.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef int ddsrt_socket_t;
#define DDSRT_INVALID_SOCKET (-1)
#define PRIdSOCK "d"

#if LWIP_SOCKET
# define DDSRT_HAVE_SSM         0
# define IFF_UP               0x1
# define IFF_BROADCAST        0x2
# define IFF_LOOPBACK         0x8
# define IFF_POINTOPOINT     0x10
# define IFF_MULTICAST     0x1000
#elif __SunOS_5_6
# define DDSRT_HAVE_SSM         0
#elif __ZEPHYR__
/* In Zephyr, a network interface can join a multi-cast group only once.
   So setsockopt is called only for the first socket to join a group, other sockets
   for the same group will be skipped */
#define DDSRT_MCGROUP_JOIN_ONCE 1

# define DDSRT_HAVE_SSM         0
# define INADDR_LOOPBACK 0x7f000001 /* 127.0.0.1 */
# define IN_MULTICAST(a) ((((long int) (a)) & 0xf0000000) == 0xe0000000)

/* socket options */
# define IP_ADD_MEMBERSHIP  35
# define IP_DROP_MEMBERSHIP 36
/* Ignored? */
# define IP_MULTICAST_IF    32
# define IP_MULTICAST_TTL   33
# define IP_MULTICAST_LOOP  34

struct ip_mreq {
    struct in_addr imr_multiaddr; 
    struct in_addr imr_interface;
};

/* for ddsrt_getifaddrs */
# define IFF_UP              0x1
# define IFF_BROADCAST       0x2
# define IFF_LOOPBACK        0x8
# define IFF_POINTOPOINT    0x10
# define IFF_MULTICAST    0x1000

#if DDSRT_HAVE_IPV6
# define IN6_IS_ADDR_UNSPECIFIED(a) (!((a)->s6_addr32[0] | (a)->s6_addr32[1] | (a)->s6_addr32[2] | (a)->s6_addr32[3]))
# define IN6_IS_ADDR_LOOPBACK(a)    (((a)->s6_addr[15]) == 1 && !memcmp((a)->s6_addr, &in6addr_loopback, sizeof(struct in6_addr)))
# define IN6_IS_ADDR_LINKLOCAL(a)   (((a)->s6_addr[0] & 0xff) == 0xfe && ((a)->s6_addr[1] & 0xc0) == 0x80)
# define IN6_IS_ADDR_MULTICAST(a)   (((a)->s6_addr[0] & 0xff) == 0xff)

struct ipv6_mreq {
  struct in6_addr ipv6mr_multiaddr;
  unsigned int    ipv6mr_interface;
};

/* socket options */
# define IPV6_JOIN_GROUP        91
# define IPV6_LEAVE_GROUP       92
/* ignored? */
# define IPV6_MULTICAST_HOPS    93
# define IPV6_UNICAST_HOPS      94
# define IPV6_MULTICAST_IF      95
# define IPV6_MULTICAST_LOOP    96
#endif

#else
# define DDSRT_HAVE_SSM         1
#endif /* LWIP_SOCKET */

typedef struct msghdr ddsrt_msghdr_t;

#if (defined(__sun) && !defined(_XPG4_2)) || \
    (defined(LWIP_SOCKET))
# define DDSRT_MSGHDR_FLAGS 0
#else
# define DDSRT_MSGHDR_FLAGS 1
#endif

#if defined(__cplusplus)
}
#endif

#endif /* DDSRT_SOCKETS_POSIX_H */
