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
#define OS_VALID_SOCKET(s) ((s) != OS_INVALID_SOCKET)

    /**
     * Socket handle type. SOCKET on windows, int otherwise.
     */

    /* Indirecting all the socket types. Some IPv6 & protocol agnostic
       stuff seems to be not always be available */

    typedef struct sockaddr_in os_sockaddr_in;
    typedef struct sockaddr os_sockaddr;
    typedef struct sockaddr_storage os_sockaddr_storage;

#if OS_SOCKET_HAS_IPV6
    typedef struct ipv6_mreq os_ipv6_mreq;
    typedef struct in6_addr os_in6_addr;

    typedef struct sockaddr_in6 os_sockaddr_in6;

    extern const os_in6_addr os_in6addr_any;
    extern const os_in6_addr os_in6addr_loopback;
#endif /* OS_SOCKET_HAS_IPV6 */


#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46 /* strlen("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255") + 1 */
#endif
#define INET6_ADDRSTRLEN_EXTENDED (INET6_ADDRSTRLEN + 8) /* + strlen("[]:12345") */

#define SD_FLAG_IS_SET(flags, flag) ((((uint32_t)(flags) & (uint32_t)(flag))) != 0U)

#define OS_AF_NULL (-1)

    /** Network interface attributes */
    typedef struct os_ifaddrs_s {
        struct os_ifaddrs_s *next;
        char *name;
        uint32_t index;
        uint32_t flags;
        os_sockaddr *addr;
        os_sockaddr *netmask;
        os_sockaddr *broadaddr;
    } os_ifaddrs_t;

    /**
     * @brief Get interface addresses
     *
     * Retrieve network interfaces available on the local system and store
     * them in a linked list of os_ifaddrs_t structures.
     *
     * The data returned by os_getifaddrs() is dynamically allocated and should
     * be freed using os_freeifaddrs when no longer needed.
     *
     * @param[in,out] ifap Address of first os_ifaddrs_t structure in the list.
     * @param[in] afs NULL-terminated array of address families (AF_xyz) to
     *                restrict resulting set of network interfaces too. NULL to
     *                return all network interfaces for all supported address
     *                families. Terminate the array with OS_AF_NULL.
     *
     * @returns Returns zero on success or a valid errno value on error.
     */
    OSAPI_EXPORT _Success_(return == 0) int
    os_getifaddrs(
        _Inout_ os_ifaddrs_t **ifap,
        _In_opt_ const int *afs);

    /**
     * @brief Free os_ifaddrs_t structure list allocated by os_getifaddrs()
     *
     * @param[in] Address of first os_ifaddrs_t structure in the list.
     */
    OSAPI_EXPORT void
    os_freeifaddrs(
        _Pre_maybenull_ _Post_ptr_invalid_ os_ifaddrs_t *ifa);

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

    /**
     * Returns size of socket address.
     * @param sa Socket address to return the size for.
     * @return Size of the socket address based on the address family, or 0 if
     *         the address family is unknown.
     * @pre sa is a valid os_sockaddr pointer.
     */
    OSAPI_EXPORT size_t
    os_sockaddr_get_size(
        const os_sockaddr *const sa) __nonnull_all__;

    /**
     * Retrieve port number from the given socket address.
     * @param sa Socket address to retrieve the port from.
     * @return Port number in host order.
     * @pre sa is a valid os_sockaddr pointer.
     */
    OSAPI_EXPORT uint16_t
    os_sockaddr_get_port(const os_sockaddr *const sa) __nonnull_all__;

    /**
     * Check if IP address of given socket address is unspecified.
     * @param sa Socket address
     * @return true if unspecified, false otherwise.
     * @pre sa is a valid os_sockaddr pointer.
     */
    OSAPI_EXPORT int
    os_sockaddr_is_unspecified(
        _In_ const os_sockaddr *__restrict sa) __nonnull_all__;

    /**
    * Check this address to see if it represents loopback.
    * @return true if it does. false otherwise, or if unknown address type.
    * @param thisSock A pointer to an os_sockaddr to be checked.
    */
    OSAPI_EXPORT int
    os_sockaddr_is_loopback(
        _In_ const os_sockaddr *__restrict sa) __nonnull_all__;

    /**
    * Checks two socket IP host addresses for be on the same subnet, considering the given subnetmask.
    * It will not consider the possibility of IPv6 mapped IPv4 addresses or anything arcane like that.
    * @param thisSock First address
    * @param thatSock Second address.
    * @param mask Subnetmask.
    * @return true if equal, false otherwise.
    */
    OSAPI_EXPORT bool
    os_sockaddrSameSubnet(const os_sockaddr* thisSock,
                          const os_sockaddr* thatSock,
                          const os_sockaddr* mask);

#ifdef OS_SOCKET_HAS_DNS

    typedef struct {
        size_t naddrs;
        os_sockaddr_storage addrs[];
    } os_hostent_t;

    /**
     * Lookup addresses for given host name.
     *
     * @param[in]   name  Host name to resolve.
     * @param[in]   af    Address family, either AF_INET, AF_INET6 or AF_UNSPEC.
     * @param[out]  hent  Structure of type os_hostent_t.
     *
     * @returns 0 on success or valid error number on failure.
     *
     * @retval 0
     *           Success. Host name successfully resolved to address(es).
     * @retval OS_HOST_NOT_FOUND
     *           Host not found.
     * @retval OS_NO_DATA
     *           Valid name, no data record of requested type.
     * @retval OS_NO_RECOVERY
     *           Nonrecoverable error.
     * @retval OS_TRY_AGAIN
     *           Nonauthoratitative host not found.
     */
    OSAPI_EXPORT _Success_(return == 0) int
    os_gethostbyname(
        _In_z_ const char *name,
        _In_ int af,
        _Out_ os_hostent_t **hent);

#endif /* OS_SOCKET_HAS_DNS */

    /**
     * Convert IPv4 and IPv6 addresses from text to socket address.
     *
     * @param  af[in]   Address family, either AF_INET or AF_INET6.
     * @param  str[in]  Network address in text form.
     * @param  sa[out]  Pointer to a sufficiently large enough socket address
     *                  structure. This implies it should generally be the
     *                  address to a structure of type struct sockaddr_storage.
     *
     * @return 0 on success or a valid error number on failure.
     */
    OSAPI_EXPORT _Success_(return) int
    os_sockaddrfromstr(
        _In_ int af,
        _In_z_  const char *str,
        _When_(af == AF_INET, _Out_writes_bytes_(sizeof(os_sockaddr_in)))
#if OS_SOCKET_HAS_IPV6
        _When_(af == AF_INET6, _Out_writes_bytes_(sizeof(os_sockaddr_in6)))
#endif
            void *sa);

    /**
     * Convert a socket address to text form.
     *
     * @param[in]   sa    Socket address structure.
     * @param[out]  buf   Buffer to which resulting string is copied.
     * @param[in]   size  Number of bytes available in the buffer.
     *
     * @returns 0 on success or a valid error number on failure.
     *
     * @retval 0
     *           Success. Socket address structure converted to text form.
     * @retval EAFNOSUPPORT
     *           Socket address structure of unsupported valid address family.
     * @retval ENOSPC
     *           Text form would exceed the size specified by size.
     */
    OSAPI_EXPORT _Success_(return == 0) int
    os_sockaddrtostr(
        _In_ const void *sa,
        _Out_writes_z_(size) char *buf,
        _In_ size_t size);

    /**
     * @}
     */

#if defined (__cplusplus)
}
#endif

#endif /* OS_SOCKET_H */
