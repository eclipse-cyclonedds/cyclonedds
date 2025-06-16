/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2022 Intel Corporation
 *
 * This software and the related documents are Intel copyrighted materials,
 * and your use of them is governed by the express license under which they
 * were provided to you ("License"). Unless the License provides otherwise,
 * you may not use, modify, copy, publish, distribute, disclose or transmit
 * this software or the related documents without Intel's prior written permission.
 * This software and the related documents are provided as is, with no express
 * or implied warranties, other than those that are expressly
 * stated in the License.
 */

#ifndef DDSRT_SOCKETS_FREERTOS_PLUS_TCP_H
#define DDSRT_SOCKETS_FREERTOS_PLUS_TCP_H

#define __need_size_t
#include <stddef.h>

/* This operating system-specific header file defines the SOCK_*, PF_*,
   AF_*, MSG_*, SOL_*, and SO_* constants, and the `struct sockaddr',
   `struct msghdr', and `struct linger' types.  */

#include <FreeRTOS.h>

#include <FreeRTOS_IP.h>
#include <FreeRTOS_Sockets.h>
#include <FreeRTOS_IP_Private.h>
#include <FreeRTOS_UDP_IP.h>

#include "dds/ddsrt/iovec.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#ifndef ntohl
#define ntohl   FreeRTOS_ntohl
#define htol    FreeRTOS_htonl

#define htos    FreeRTOS_htons
#define ntohs   FreeRTOS_ntohs
#endif

#define inet_addr FreeRTOS_inet_addr
#define IN_MULTICAST(ip)    (xIsIPv4Multicast(ntohl(ip)))


/* posix data for FreeRTOS+TCP stack */
#define DDSRT_HAVE_SSM 0
#define IFF_UP 0x1
#define IFF_BROADCAST 0x2
#define IFF_LOOPBACK 0x8
#define IFF_POINTOPOINT 0x10
#define IFF_MULTICAST 0x1000

struct msghdr
{
    void *msg_name;
    socklen_t msg_namelen;
    struct iovec *msg_iov;
    int msg_iovlen;
    void *msg_control;
    socklen_t msg_controllen;
    int msg_flags;
};

struct timeval
{
    long tv_sec;  /* seconds */
    long tv_usec; /* and microseconds */
};

typedef struct msghdr ddsrt_msghdr_t;
#define DDSRT_MSGHDR_FLAGS  0


/* adaption to ddsrt socket interface */
typedef Socket_t ddsrt_socket_t; /* Socket_t ==> FreeRTOS_Socket_t* */
// typedef SocketSelect_t fd_set;      /* SocketSelect_t => struct xSOCKET_SET */
typedef SocketSet_t    ddsrt_fd_set_t;

#define SELECT_TIMEOUT_MS       1000U /* 200U    experienced value from XRCE-client */

/*
FD_ZERO() clears a set.
FD_SET() and FD_CLR() respectively add and remove a given file descriptor from a set.
FD_ISSET() tests to see if a file descriptor  is  part  of  the  set; this is useful after select() returns.
*/
static inline void DDSRT_FD_ZERO(ddsrt_fd_set_t set)
{
    (void)set;
}

static ddsrt_fd_set_t DDSRT_FD_SET_CRATE(void)
{
    SocketSet_t set = FreeRTOS_CreateSocketSet();
    assert(set);
    return set;
}

static void DDSRT_FD_SET_DELETE(ddsrt_fd_set_t set)
{
    assert(set);
    FreeRTOS_DeleteSocketSet(set);
}

/*
#define DDSRT_FD_ISSET(fd, set)   FreeRTOS_FD_ISSET(fd, set)
#define DDSRT_FD_SET(fd, set)     FreeRTOS_FD_SET(fd, set, eSELECT_READ | eSELECT_EXCEPT)
#define DDSRT_FD_CLR(fd, set)     FreeRTOS_FD_CLR(fd, set, eSELECT_ALL)
*/

static inline int32_t DDSRT_FD_ISSET(ddsrt_socket_t fd, ddsrt_fd_set_t set)
{
    return (int32_t)FreeRTOS_FD_ISSET(fd, set);
}

/* for FD_SET only be used in CDDS for rdset */
static inline void DDSRT_FD_SET(ddsrt_socket_t fd, ddsrt_fd_set_t set)
{
    FreeRTOS_FD_SET(fd, set, eSELECT_READ | eSELECT_EXCEPT);
}

static inline void DDSRT_FD_CLR(ddsrt_socket_t fd, ddsrt_fd_set_t set)
{
    FreeRTOS_FD_CLR(fd, set, eSELECT_ALL);
}

#ifndef sa_family_t
    typedef uint8_t sa_family_t;
#endif

    struct sockaddr_storage
    {
        uint8_t s2_len;
        sa_family_t ss_family;
        char s2_data1[2];
        uint32_t s2_data2[3];
    };

#if 1
    /* for posix compatible sa_family reference inside DDS
     * we declare sockaddr a new struct but same W/ freertos_sockaddr
     */
    struct sockaddr
    {
        uint8_t sa_len;
        sa_family_t sa_family;
        uint16_t sin_port;
        uint32_t sin_addr;
    };
#else
#define sockaddr freertos_sockaddr
#endif

#define sockaddr_in freertos_sockaddr
typedef uint32_t in_addr_t; /* base type for internet address */

/*
 * Options and types related to multicast membership
 */
#define IP_ADD_MEMBERSHIP 3
#define IP_DROP_MEMBERSHIP 4
    typedef struct ip_mreq
    {
        in_addr_t imr_multiaddr; /* IP multicast address of group */
        in_addr_t imr_interface; /* local IP address of interface */
    } ip_mreq;
    // end multicast

#define IP_MULTICAST_TTL 5
#define IP_MULTICAST_IF 6
#define IP_MULTICAST_LOOP 7

/** 255.255.255.255 */
#define INADDR_NONE ((uint32_t)0xffffffffUL)
/** 127.0.0.1 */
#define INADDR_LOOPBACK ((uint32_t)0x7f000000UL)
/** 0.0.0.0 */
#define INADDR_ANY ((uint32_t)0x00000000UL)
/** 255.255.255.255 */
#define INADDR_BROADCAST ((uint32_t)0xffffffffUL)

#define AF_INET FREERTOS_AF_INET
#define AF_INET6 FREERTOS_AF_INET6

#define IPPROTO_IP 0
#define IPPROTO_ICMP ipPROTOCOL_ICMP

#define SOCK_DGRAM FREERTOS_SOCK_DGRAM
#define IPPROTO_UDP FREERTOS_IPPROTO_UDP

#define SOCK_STREAM FREERTOS_SOCK_STREAM
#define IPPROTO_TCP FREERTOS_IPPROTO_TCP

#define SOL_SOCKET IPPROTO_IP /* set socket level, always 0 for RTOS */

#define SO_REUSEADDR FREERTOS_SO_REUSE_LISTEN_SOCKET
#define SO_RCVTIMEO FREERTOS_SO_RCVTIMEO
#define SO_SNDTIMEO FREERTOS_SO_SNDTIMEO
#define SO_SNDBUF FREERTOS_SO_SNDBUF
#define SO_RCVBUF FREERTOS_SO_RCVBUF

#define SO_OOB FREERTOS_MSG_OOB
#define SO_PEEK FREERTOS_MSG_PEEK
#define SO_DONTROUTE FREERTOS_MSG_DONTROUTE
#define SO_DONTWAIT FREERTOS_MSG_DONTWAIT

/*
  FreeRTOS_accept() has an optional timeout. The timeout defaults
  to ipconfigSOCK_DEFAULT_RECEIVE_BLOCK_TIME, and is modified
  using the FREERTOS_SO_RCVTIMEO parameter in a call to FreeRTOS_setsockopt()

  If a timeout occurs before a connection from a remote socket
  is accepted then NULL is returned.
*/
#define ACCEPT_TIMEOUT (NULL)  // accpet timeout
#define INVALID_SOCKET (FREERTOS_INVALID_SOCKET)

/* The following constants should be used for the second parameter of
   `shutdown'.  */
#define SHUT_RD FREERTOS_SHUT_RD
#define SHUT_WR FREERTOS_SHUT_WR
#define SHUT_RDWR FREERTOS_SHUT_RDWR


#define DDSRT_INVALID_SOCKET (INVALID_SOCKET)
#define PRIdSOCK PRIxPTR

// #define errno           FreeRTOS_errno
#define DDSRT_RET_OK (0)

#ifndef FD_SETSIZE
#define FD_SETSIZE 8
#endif

#if defined(__cplusplus)
}
#endif

#endif
