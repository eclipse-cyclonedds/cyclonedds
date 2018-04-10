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
#ifndef OS_SOCKET_H
#define OS_SOCKET_H

#ifndef OS_SOCKET_HAS_IPV6
#error "OS_SOCKET_HAS_IPV6 should have been defined by os_platform_socket.h"
#endif
#ifndef OS_IFNAMESIZE
#error "OS_IFNAMESIZE should have been defined by os_platform_socket.h"
#endif
#ifndef OS_SOCKET_HAS_SA_LEN
#error "OS_SOCKET_HAS_SA_LEN should have been defined by os_platform_socket.h"
#endif
#ifndef OS_NO_SIOCGIFINDEX
#error "OS_NO_SIOCGIFINDEX should have been defined by os_platform_socket.h"
#endif
#ifndef OS_NO_NETLINK
#error "OS_NO_NETLINK should have been defined by os_platform_socket.h"
#endif
#ifndef OS_SOCKET_HAS_SSM
#error "OS_SOCKET_HAS_SSM should have been defined by os_platform_socket.h"
#endif

#if defined (__cplusplus)
extern "C" {
#endif

    /* !!!!!!!!NOTE From here no more includes are allowed!!!!!!! */

    /**
     * @file
     * @addtogroup OS_NET
     * @{
     */

    /**
     * Socket handle type. SOCKET on windows, int otherwise.
     */

    /* Indirecting all the socket types. Some IPv6 & protocol agnostic
       stuff seems to be not always be available */

    typedef struct sockaddr_in os_sockaddr_in;
    typedef struct sockaddr os_sockaddr;

#ifdef OS_SOCKET_HAS_IPV6
#if (OS_SOCKET_HAS_IPV6 == 0)
    struct foo_in6_addr {
        unsigned char   foo_s6_addr[16];
    };
    typedef struct foo_in6_addr os_in6_addr;

    struct foo_sockaddr_in6 {
        os_os_ushort sin6_family;
        os_os_ushort sin6_port;
        uint32_t sin6_flowinfo;
        os_in6_addr sin6_addr;
        uint32_t sin6_scope_id;
    };
    typedef struct foo_sockaddr_in6 os_sockaddr_in6;

    struct foo_sockaddr_storage {
#if (OS_SOCKET_HAS_SA_LEN == 1)
        os_uchar   ss_len;
        os_uchar   ss_family;
#else
        os_os_ushort  ss_family;
#endif
        /* Below aren't 'real' members. Just here for padding to make it big enough
           for any possible IPv6 address. Not that IPv6 works on this OS. */
        os_os_ushort sin6_port;
        uint32_t sin6_flowinfo;
        os_in6_addr sin6_addr;
        uint32_t sin6_scope_id;
    };
    typedef struct foo_sockaddr_storage os_sockaddr_storage;

    struct foo_ipv6_mreq {
        os_in6_addr ipv6mr_multiaddr;
        unsigned int    ipv6mr_interface;
    };
    typedef struct foo_ipv6_mreq os_ipv6_mreq;
#else
    typedef struct ipv6_mreq os_ipv6_mreq;
    typedef struct in6_addr os_in6_addr;

#if defined (OSPL_VXWORKS653)
    typedef struct sockaddr_in os_sockaddr_storage;
#else
    typedef struct sockaddr_storage os_sockaddr_storage;
#endif

    typedef struct sockaddr_in6 os_sockaddr_in6;
#endif
#else
#error OS_SOCKET_HAS_IPV6 not defined
#endif

    extern const os_in6_addr os_in6addr_any;
    extern const os_in6_addr os_in6addr_loopback;

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46 /* strlen("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255") + 1 */
#endif
#define INET6_ADDRSTRLEN_EXTENDED (INET6_ADDRSTRLEN + 8) /* + strlen("[]:12345") */

    /* The maximum buffersize needed to safely store the output of
     * os_sockaddrAddressPortToString or os_sockaddrAddressToString. */
#define OS_SOCKET_MAX_ADDRSTRLEN INET6_ADDRSTRLEN_EXTENDED

#define SD_FLAG_IS_SET(flags, flag) ((((uint32_t)(flags) & (uint32_t)(flag))) != 0U)

    /**
     * Structure to hold a network interface's attributes
     */
    typedef struct os_ifAttributes_s {
        /**
         * The network interface name (or at least OS_IFNAMESIZE - 1 charcaters thereof)
         */
        char name[OS_IFNAMESIZE];
        /**
         * Iff the interface is IPv4 holds the ioctl query result flags for this interface.
         */
        uint32_t flags;
        /**
         * The network interface address of this interface.
         */
        os_sockaddr_storage address;
        /**
         * Iff this is an IPv4 interface, holds the broadcast address for the sub-network this
         * interface is connected to.
         */
        os_sockaddr_storage broadcast_address;
        /**
         * Iff this is an IPv4 interface, holds the subnet mast for this interface
         */
        os_sockaddr_storage network_mask;
        /**
         * Holds the interface index for this interface.
         */
        unsigned interfaceIndexNo;
    } os_ifAttributes;

    OSAPI_EXPORT os_result
    os_sockQueryInterfaces(
            os_ifAttributes *ifList,
            uint32_t listSize,
            uint32_t *validElements);

    /**
     * Fill-in a pre-allocated list of os_ifAttributes_s with details of
     * the available IPv6 network interfaces.
     * @param listSize Number of elements there is space for in the list.
     * @param ifList Pointer to head of list
     * @param validElements Out param to hold the number of interfaces found
     * whose detauils have been returned.
     * @return os_resultSuccess if 0 or more interfaces were found, os_resultFail if
     * an error occurred.
     * @see os_sockQueryInterfaces
     */
    OSAPI_EXPORT os_result
    os_sockQueryIPv6Interfaces(
            os_ifAttributes *ifList,
            uint32_t listSize,
            uint32_t *validElements);

    OSAPI_EXPORT os_socket
    os_sockNew(
            int domain, /* AF_INET */
            int type    /* SOCK_DGRAM */);

    OSAPI_EXPORT os_result
    os_sockBind(
            os_socket s,
            const struct sockaddr *name,
            uint32_t namelen);

    OSAPI_EXPORT os_result
    os_sockGetsockname(
            os_socket s,
            const struct sockaddr *name,
            uint32_t namelen);

    OSAPI_EXPORT os_result
    os_sockSendto(
            os_socket s,
            const void *msg,
            size_t len,
            const struct sockaddr *to,
            size_t tolen,
            size_t *bytesSent);

    OSAPI_EXPORT os_result
    os_sockRecvfrom(
            os_socket s,
            void *buf,
            size_t len,
            struct sockaddr *from,
            size_t *fromlen,
            size_t *bytesRead);

    OSAPI_EXPORT os_result
    os_sockGetsockopt(
            os_socket s,
            int32_t level, /* SOL_SOCKET */
            int32_t optname, /* SO_REUSEADDR, SO_DONTROUTE, SO_BROADCAST, SO_SNDBUF, SO_RCVBUF */
            void *optval,
            uint32_t *optlen);

    OSAPI_EXPORT os_result
    os_sockSetsockopt(
            os_socket s,
            int32_t level, /* SOL_SOCKET */
            int32_t optname, /* SO_REUSEADDR, SO_DONTROUTE, SO_BROADCAST, SO_SNDBUF, SO_RCVBUF */
            const void *optval,
            uint32_t optlen);


    /**
     * Sets the I/O on the socket to nonblocking if value is nonzero,
     * or to blocking if value is 0.
     *
     * @param s The socket to set the I/O mode for
     * @param nonblock Boolean indicating whether nonblocking mode should be enabled
     * @return - os_resultSuccess: if the flag could be set successfully
     *         - os_resultBusy: if the flag could not be set because a blocking
     *              call is in progress on the socket
     *         - os_resultInvalid: if s is not a valid socket
     *         - os_resultFail: if an operating system error occurred
     */
    OSAPI_EXPORT os_result
    os_sockSetNonBlocking(
            os_socket s,
            bool nonblock);

    OSAPI_EXPORT os_result
    os_sockFree(
            os_socket s);

#ifdef WIN32
/* SOCKETs on Windows are NOT integers. The nfds parameter is only there for
   compatibility, the implementation ignores it. Implicit casts will generate
   warnings though, therefore os_sockSelect on Windows is a proxy macro that
   discards the parameter */
#define os_sockSelect(nfds, readfds, writefds, errorfds, timeout) \
    os__sockSelect((readfds), (writefds), (errorfds), (timeout))

    OSAPI_EXPORT int32_t
    os__sockSelect(
            fd_set *readfds,
            fd_set *writefds,
            fd_set *errorfds,
            os_time *timeout);
#else
    OSAPI_EXPORT int32_t
    os_sockSelect(
            int32_t nfds,
            fd_set *readfds,
            fd_set *writefds,
            fd_set *errorfds,
            os_time *timeout);
#endif /* WIN32 */

    /* docced in implementation file */
    OSAPI_EXPORT os_result
    os_sockaddrInit(os_sockaddr* sa,
                    bool isIPv4); /* IPvX is poorly abstracted; this is temporary */

    /* docced in implementation file */
    OSAPI_EXPORT bool
    os_sockaddrIPAddressEqual(const os_sockaddr* this_sock,
                              const os_sockaddr* that_sock);

    OSAPI_EXPORT int
    os_sockaddrIpAddressCompare(const os_sockaddr* addr1,
                                const os_sockaddr* addr2) __nonnull_all__
        __attribute_pure__;

    /* docced in implementation file */
    OSAPI_EXPORT bool
    os_sockaddrSameSubnet(const os_sockaddr* thisSock,
                          const os_sockaddr* thatSock,
                          const os_sockaddr* mask);

    /**
     * Convert a socket address to a string format presentation representation
     * @param sa The socket address struct.
     * @param buffer A character buffer to hold the string rep of the address.
     * @param buflen The (max) size of the buffer
     * @return Pointer to start of string
     */
    OSAPI_EXPORT char*
    os_sockaddrAddressToString(const os_sockaddr* sa,
                               char* buffer, size_t buflen);

    /**
     * Convert a socket address to a string format representation including the
     * portnumber.
     * @param sa The socket address struct.
     * @param buffer A character buffer to hold the string rep of the address.
     * @param buflen The (max) size of the buffer
     * @return Pointer to start of string
     */
    OSAPI_EXPORT char*
    os_sockaddrAddressPortToString(
            const os_sockaddr* sa,
            char* buffer,
            size_t buflen);

    /**
    * Convert the provided addressString into a os_sockaddr.
    *
    * @param addressString The string representation of a network address.
    * @param addressOut A pointer to an os_sockaddr. Must be big enough for
    * the address type specified by the string. This implies it should
    * generally be the address of an os_sockaddr_storage for safety's sake.
    * @param isIPv4 If the addressString is a hostname specifies whether
    * and IPv4 address should be returned. If false an Ipv6 address will be
    * requested. If the address is in either valid decimal presentation format
    * param will be ignored.
    * @return true on successful conversion. false otherwise
    */
    _Success_(return) OSAPI_EXPORT bool
    os_sockaddrStringToAddress(
        _In_z_  const char *addressString,
        _When_(isIPv4, _Out_writes_bytes_(sizeof(os_sockaddr_in)))
        _When_(!isIPv4, _Out_writes_bytes_(sizeof(os_sockaddr_in6)))
            os_sockaddr *addressOut,
        _In_ bool isIPv4);

    /* docced in implementation file */
    OSAPI_EXPORT bool
    os_sockaddrIsLoopback(const os_sockaddr* thisSock);

    /**
     * Returns sizeof(sa) based on the family of the actual content.
     * @param sa the sockaddr to get the actual size for
     * @return The actual size sa based on the family. If the family is
     * unknown 0 will be returned.
     * @pre sa is a valid sockaddr pointer
     */
    OSAPI_EXPORT size_t
    os_sockaddrSizeof(const os_sockaddr* sa);

    /**
     * Sets the address of the sockaddr to the special IN_ADDR_ANY value.
     * @param sa the sockaddr to set the address for
     * @pre sa is a valid sockaddr pointer
     * @post Address of sa is set to the special IN_ADDR_ANY value
     */
    OSAPI_EXPORT void
    os_sockaddrSetInAddrAny(os_sockaddr* sa);

    /**
     * Sets the port number in the supplied sockaddr.
     * @param sa the sockaddr to set the port in
     * @param port the port number to be set in network byte order
     * @pre sa is a valid sockaddr pointer
     * @post Port number of sa is set to port
     */
    OSAPI_EXPORT void
    os_sockaddrSetPort(os_sockaddr* sa, uint16_t port);

    /**
     * Retrieves the port number from the supplied sockaddr.
     * @param sa the sockaddr to retrieve the port from
     * @return The port number stored in the supplied sockaddr (hence in network byte order)
     * @pre sa is a valid sockaddr pointer
     */
    OSAPI_EXPORT uint16_t
    os_sockaddrGetPort(const os_sockaddr* const sa);


    /**
     * @}
     */

#if defined (__cplusplus)
}
#endif

#endif /* OS_SOCKET_H */
