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
/****************************************************************
 * Implementation for socket services conforming to             *
 * OpenSplice requirements                                      *
 ****************************************************************/

/** \file os/code/os_socket.c
 *  \brief socket management
 */

#include <assert.h>
#include <string.h>

#ifdef __linux
#include <linux/if_packet.h> /* sockaddr_ll */
#endif /* __linux */

#include "os/os.h"

#if (OS_SOCKET_HAS_IPV6 == 1)
#ifndef _VXWORKS
const os_in6_addr os_in6addr_any = IN6ADDR_ANY_INIT;
const os_in6_addr os_in6addr_loopback = IN6ADDR_LOOPBACK_INIT;
#else
const os_in6_addr os_in6addr_any = { { 0 } };
const os_in6_addr os_in6addr_loopback = { { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 } } };
#endif
#endif

#ifndef OS_INET_NTOP
#define OS_INET_NTOP inet_ntop
#endif

#ifndef OS_INET_PTON
#define OS_INET_PTON inet_pton
#endif

const int afs[] = {
#ifdef __linux
    AF_PACKET,
#endif /* __linux */
#if OS_SOCKET_HAS_IPV6
    AF_INET6,
#endif /* OS_SOCKET_HAS_IPV6 */
    AF_INET,
    OS_AF_NULL /* Terminator */
};

const int *const os_supp_afs = afs;

size_t
os_sockaddr_get_size(const os_sockaddr *const sa)
{
    size_t sz;

    assert(sa != NULL);

    switch(sa->sa_family) {
#if OS_SOCKET_HAS_IPV6
        case AF_INET6:
            sz = sizeof(os_sockaddr_in6);
            break;
#endif /* OS_SOCKET_HAS_IPV6 */
#ifdef __linux
        case AF_PACKET:
            sz = sizeof(struct sockaddr_ll);
            break;
#endif /* __linux */
        default:
            assert(sa->sa_family == AF_INET);
            sz = sizeof(os_sockaddr_in);
            break;
    }

    return sz;
}

uint16_t os_sockaddr_get_port(const os_sockaddr *const sa)
{
    unsigned short port = 0;

    switch(sa->sa_family) {
#if OS_SOCKET_HAS_IPV6
        case AF_INET6:
            port = ntohs(((os_sockaddr_in6 *)sa)->sin6_port);
            break;
#endif /* OS_SOCKET_HAS_IPV6 */
        default:
            assert(sa->sa_family == AF_INET);
            port = ntohs(((os_sockaddr_in *)sa)->sin_port);
            break;
    }

    return port;
}

int os_sockaddr_compare(
    const os_sockaddr *const sa1,
    const os_sockaddr *const sa2)
{
    int eq;
    size_t sz;

    if ((eq = sa1->sa_family - sa2->sa_family) == 0) {
        switch(sa1->sa_family) {
#if (OS_SOCKET_HAS_IPV6 == 1)
            case AF_INET6:
            {
                os_sockaddr_in6 *sin61, *sin62;
                sin61 = (os_sockaddr_in6 *)sa1;
                sin62 = (os_sockaddr_in6 *)sa2;
                sz = sizeof(sin61->sin6_addr);
                eq = memcmp(&sin61->sin6_addr, &sin62->sin6_addr, sz);
            }
                break;
#endif /* OS_SOCKET_HAS_IPV6 */
#ifdef __linux
            case AF_PACKET:
            {
                struct sockaddr_ll *sll1, *sll2;
                sll1 = (struct sockaddr_ll *)sa1;
                sll2 = (struct sockaddr_ll *)sa2;
                sz = sizeof(sll1->sll_addr);
                eq = memcmp(sll1->sll_addr, sll2->sll_addr, sz);
            }
                break;
#endif /* __linux */
            default:
            {
                assert(sa1->sa_family == AF_INET);
                os_sockaddr_in *sin1, *sin2;
                sin1 = (os_sockaddr_in *)sa1;
                sin2 = (os_sockaddr_in *)sa2;
                sz = sizeof(sin1->sin_addr);
                eq = memcmp(sin1, sin2, sizeof(*sin1));
            }
                break;
        }
    }

    return eq;
}

int os_sockaddr_is_unspecified(const os_sockaddr *const sa)
{
  int unspec = 0;

  assert(sa != NULL);

  if (sa->sa_family == AF_INET6) {
    unspec = IN6_IS_ADDR_UNSPECIFIED(&((os_sockaddr_in6 *)sa)->sin6_addr);
  } else if (sa->sa_family == AF_INET) {
    unspec = (((os_sockaddr_in *)sa)->sin_addr.s_addr == 0);
  }

  return unspec;
}

/**
* Checks two socket IP host addresses for be on the same subnet, considering the given subnetmask.
* It will not consider the possibility of IPv6 mapped IPv4 addresses or anything arcane like that.
* @param thisSock First address
* @param thatSock Second address.
* @param mask Subnetmask.
* @return true if equal, false otherwise.
*/
bool
os_sockaddrSameSubnet(const os_sockaddr* thisSock,
                           const os_sockaddr* thatSock,
                           const os_sockaddr* mask)
{
    bool result = false;
#if (OS_SOCKET_HAS_IPV6 == 1)
    os_sockaddr_in6 thisV6, thatV6, *maskV6;
#endif

    if (thisSock->sa_family == thatSock->sa_family &&
        thisSock->sa_family == mask->sa_family)
    {
        if (thisSock->sa_family == AF_INET)
        {
            /* IPv4 */
            result = ((((os_sockaddr_in*)thisSock)->sin_addr.s_addr & ((os_sockaddr_in*)mask)->sin_addr.s_addr ) ==
                     (((os_sockaddr_in*)thatSock)->sin_addr.s_addr & ((os_sockaddr_in*)mask)->sin_addr.s_addr) ?
                     true: false);
        }
#if (OS_SOCKET_HAS_IPV6 == 1)
        else
        {
            size_t i, size;
            /* IPv6 */
            memcpy(&thisV6, thisSock, sizeof(thisV6));
            memcpy(&thatV6, thatSock, sizeof(thatV6));
            maskV6 = (os_sockaddr_in6*) mask;
            size = sizeof(thisV6.sin6_addr.s6_addr);
            for (i=0; i < size; i++) {
                thisV6.sin6_addr.s6_addr[i] &= maskV6->sin6_addr.s6_addr[i];
                thatV6.sin6_addr.s6_addr[i] &= maskV6->sin6_addr.s6_addr[i];
            }
            result = (memcmp(&thisV6.sin6_addr.s6_addr, &thatV6.sin6_addr.s6_addr, size) ?
                     false : true);
        }
#endif
    }
    return result;
}

#if WIN32
/*
 * gai_strerror under Windows is not thread safe. See getaddrinfo on MSDN:
 * https://msdn.microsoft.com/en-us/library/windows/desktop/ms738520.aspx
 *
 * The error codes that getaddrinfo returns map directly onto WSA error codes.
 * os_strerror_r can therefore safely be used to retrieve their description.
 */
#define os_gai_strerror(errnum) os_strerror(errnum)
#else
#define os_gai_strerror(errnum) gai_strerror(errnum)
#endif /* WIN32 */

_Success_(return) bool
os_sockaddrStringToAddress(
    _In_z_  const char *addressString,
    _When_(isIPv4, _Out_writes_bytes_(sizeof(os_sockaddr_in)))
    _When_(!isIPv4, _Out_writes_bytes_(sizeof(os_sockaddr_in6)))
        os_sockaddr *addressOut,
    _In_ bool isIPv4)
{
    int ret;
    const char *fmt;
    struct addrinfo hints;
    struct addrinfo *res = NULL;

    assert(addressString != NULL);
    assert(addressOut != NULL);

    memset(&hints, 0, sizeof(hints));
#if (OS_SOCKET_HAS_IPV6 == 1)
    hints.ai_family = (isIPv4 ? AF_INET : AF_INET6);
#else
    hints.ai_family = AF_INET;
    OS_UNUSED_ARG(isIPv4);
#endif /* IPv6 */
    hints.ai_socktype = SOCK_DGRAM;

    ret = getaddrinfo(addressString, NULL, &hints, &res);
    if (ret != 0) {
        fmt = "getaddrinfo(\"%s\") failed: %s";
        OS_DEBUG(OS_FUNCTION, 0, fmt, addressString, os_gai_strerror(ret));
    } else if (res != NULL) {
        memcpy(addressOut, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);
    } else {
        fmt = "getaddrinfo(\"%s\") did not return any results";
        OS_DEBUG(OS_FUNCTION, 0, fmt, addressString);
    }

    return (ret == 0 && res != NULL);
}

/**
* Check this address to see if it represents loopback.
* @return true if it does. false otherwise, or if unknown address type.
* @param thisSock A pointer to an os_sockaddr to be checked.
*/
bool
os_sockaddrIsLoopback(const os_sockaddr* thisSock)
{
    bool result = false;

#if (OS_SOCKET_HAS_IPV6 == 1)
    static os_sockaddr_storage linkLocalLoopback;
    static os_sockaddr* linkLocalLoopbackPtr = NULL;

    if (linkLocalLoopbackPtr == NULL)
    {
        /* Initialise once (where 'once' implies some small integer) */
        os_sockaddrStringToAddress("fe80::1", (os_sockaddr*) &linkLocalLoopback, false /* ! ipv4 */ );
        linkLocalLoopbackPtr = (os_sockaddr*) &linkLocalLoopback;
    }

    if (thisSock->sa_family == AF_INET6)
    {
        result = IN6_IS_ADDR_LOOPBACK(&((os_sockaddr_in6*)thisSock)->sin6_addr) ||
                 os_sockaddr_compare(thisSock, linkLocalLoopbackPtr) == 0 ? true : false;
    }
    else
#endif
    if (thisSock->sa_family == AF_INET)
    {
        result = (INADDR_LOOPBACK == ntohl(((os_sockaddr_in*)thisSock)->sin_addr.s_addr)) ? true : false;
    }

    return result;
}

void
os_sockaddrSetInAddrAny(
    os_sockaddr* sa)
{
    assert(sa);
#if (OS_SOCKET_HAS_IPV6 == 1)
    assert(sa->sa_family == AF_INET6 || sa->sa_family == AF_INET);
    if (sa->sa_family == AF_INET6){
        ((os_sockaddr_in6*)sa)->sin6_addr = os_in6addr_any;
        ((os_sockaddr_in6*)sa)->sin6_scope_id = 0;
    }
    else
#else
    assert(sa->sa_family == AF_INET);
#endif
    if (sa->sa_family == AF_INET){
        ((os_sockaddr_in*)sa)->sin_addr.s_addr = htonl(INADDR_ANY);
    }
}

char*
os_sockaddrAddressToString(const os_sockaddr* sa,
                            char* buffer, size_t buflen)
{
    assert (buflen <= 0x7fffffff);

    switch(sa->sa_family) {
        case AF_INET:
            OS_INET_NTOP(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
                    buffer, (socklen_t) buflen);
            break;
#if (OS_SOCKET_HAS_IPV6 == 1)
        case AF_INET6:
            OS_INET_NTOP(AF_INET6, &(((os_sockaddr_in6 *)sa)->sin6_addr),
                    buffer, (socklen_t) buflen);
            break;
#endif
        default:
            (void) snprintf(buffer, buflen, "Unknown address family");
            break;
    }

    return buffer;
}
