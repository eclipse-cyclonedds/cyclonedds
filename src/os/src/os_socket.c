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

int
os_sockaddr_is_unspecified(
    _In_ const os_sockaddr *__restrict sa)
{
    assert(sa != NULL);

    switch(sa->sa_family) {
#if OS_SOCKET_HAS_IPV6
        case AF_INET6:
            return IN6_IS_ADDR_UNSPECIFIED(&((os_sockaddr_in6*)sa)->sin6_addr);
#endif
        case AF_INET:
            return (((os_sockaddr_in *)sa)->sin_addr.s_addr == 0);
    }

    return 0;
}

int
os_sockaddr_is_loopback(
    _In_ const os_sockaddr *__restrict sa)
{
    assert(sa != NULL);

    switch (sa->sa_family) {
#if OS_SOCKET_HAS_IPV6
        case AF_INET6:
            return IN6_IS_ADDR_LOOPBACK(
                &((const os_sockaddr_in6 *)sa)->sin6_addr);
#endif /* OS_SOCKET_HAS_IPV6 */
        case AF_INET:
            return (((const os_sockaddr_in *)sa)->sin_addr.s_addr
                        == htonl(INADDR_LOOPBACK));
    }

    return 0;
}

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

int _Success_(return == 0)
os_sockaddrfromstr(
    _In_ int af,
    _In_z_ const char *str,
    _When_(af == AF_INET, _Out_writes_bytes(sizeof(os_sockaddr_in)))
#if OS_SOCKET_HAS_IPV6
    _When_(af == AF_INET6, _Out_writes_bytes(sizeof(os_sockaddr_in6)))
#endif /* OS_SOCKET_HAS_IPV6 */
        void *sa)
{
    int err = 0;

    assert(str != NULL);
    assert(sa != NULL);

    switch (af) {
        case AF_INET:
        {
            struct in_addr buf;
            if (inet_pton(af, str, &buf) != 1) {
                err = EINVAL;
            } else {
                memset(sa, 0, sizeof(os_sockaddr_in));
                ((os_sockaddr_in *)sa)->sin_family = AF_INET;
                memcpy(&((os_sockaddr_in *)sa)->sin_addr, &buf, sizeof(buf));
            }
        }
            break;
#if OS_SOCKET_HAS_IPV6
        case AF_INET6:
        {
            struct in6_addr buf;
            if (inet_pton(af, str, &buf) != 1) {
                err = EINVAL;
            } else {
                memset(sa, 0, sizeof(os_sockaddr_in6));
                ((os_sockaddr_in6 *)sa)->sin6_family = AF_INET6;
                memcpy(&((os_sockaddr_in6 *)sa)->sin6_addr, &buf, sizeof(buf));
            }
        }
            break;
#endif /* OS_SOCKET_HAS_IPV6 */
        default:
            err = EAFNOSUPPORT;
            break;
    }

    return err;
}

int _Success_(return == 0)
os_sockaddrtostr(
    _In_ const void *sa,
    _Out_writes_z_(size) char *buf,
    _In_ size_t size)
{
    int err = 0;
    const char *ptr;

    assert(sa != NULL);
    assert(buf != NULL);

    switch (((os_sockaddr *)sa)->sa_family) {
        case AF_INET:
            ptr = inet_ntop(
                AF_INET, &((os_sockaddr_in *)sa)->sin_addr, buf, (socklen_t)size);
            break;
#if OS_SOCKET_HAS_IPV6
        case AF_INET6:
            ptr = inet_ntop(
                AF_INET6, &((os_sockaddr_in6 *)sa)->sin6_addr, buf, (socklen_t)size);
            break;
#endif
        default:
            return EAFNOSUPPORT;
    }

    if (ptr == NULL) {
#if WIN32
        err = GetLastError();
        if (ERROR_INVALID_PARAMETER) {
            /* ERROR_INVALID_PARAMETER is returned if the buffer is a null
               pointer or the size of the buffer is not sufficiently large
               enough. *NIX platforms set errno to ENOSPC if the buffer is not
               large enough to store the IP address in text form. */
            err = ENOSPC;
        }
#else
        err = errno;
#endif
    }

    return err;
}
