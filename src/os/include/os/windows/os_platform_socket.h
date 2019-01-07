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
#ifndef OS_PLATFORM_SOCKET_H
#define OS_PLATFORM_SOCKET_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#if defined (__cplusplus)
extern "C" {
#endif

#define OS_SOCK_VERSION         2
#define OS_SOCK_REVISION        0

/* Keep defines before common header */
#define OS_SOCKET_HAS_IPV6      1
#define OS_SOCKET_HAS_DNS       1
#define OS_SOCKET_HAS_SA_LEN    0
#define OS_NO_SIOCGIFINDEX      1
#define OS_NO_NETLINK           1

#if defined NTDDI_VERSION && defined _WIN32_WINNT_WS03 && NTDDI_VERSION >= _WIN32_WINNT_WS03
#define OS_SOCKET_HAS_SSM 1
#else
#define OS_SOCKET_HAS_SSM 0
#endif

#define IFF_POINTOPOINT     IFF_POINTTOPOINT

#define os_sockEAGAIN       WSAEWOULDBLOCK /* Operation would block, or a timeout expired before operation succeeded */
#define os_sockEWOULDBLOCK  WSAEWOULDBLOCK /* Operation would block */
#define os_sockENOMEM       WSABASEERR
#define os_sockENOSR        WSABASEERR
#define os_sockENOENT       WSABASEERR
#define os_sockEPERM        WSAEACCES
#define os_sockEINTR        WSAEINTR
#define os_sockEBADF        WSAEBADF
#define os_sockEACCES       WSAEACCES
#define os_sockEINVAL       WSAEINVAL
#define os_sockEMFILE       WSAEMFILE
#define os_sockENOTSOCK     WSAENOTSOCK
#define os_sockEMSGSIZE     WSAEMSGSIZE
#define os_sockENOPROTOOPT  WSAENOPROTOOPT
#define os_sockEPROTONOSUPPORT  WSAEPROTONOSUPPORT
#define os_sockEADDRINUSE   WSAEADDRINUSE
#define os_sockEADDRNOTAVAIL    WSAEADDRNOTAVAIL
#define os_sockEHOSTUNREACH WSAEHOSTUNREACH
#define os_sockENOBUFS      WSAENOBUFS
#define os_sockECONNRESET   WSAECONNRESET   /* Connection reset by peer */

    typedef SOCKET os_socket;
    #define PRIsock PRIuPTR

#define OS_INVALID_SOCKET (INVALID_SOCKET)

    typedef unsigned os_iov_len_t;
    typedef struct os_iovec {
        os_iov_len_t iov_len;
        void *iov_base;
    } os_iovec_t;

    typedef DWORD os_msg_iovlen_t;

    struct msghdr {
        void *msg_name;
        socklen_t msg_namelen;
        os_iovec_t *msg_iov;
        os_msg_iovlen_t msg_iovlen;
        void *msg_control;
        size_t msg_controllen;
        int msg_flags;
    };

#define OS_MSGHDR_FLAGS 1

    /* Only implements iovec of length 1, no control */
    ssize_t recvmsg (os_socket fd, struct msghdr *message, int flags);
    ssize_t sendmsg (os_socket fd, const struct msghdr *message, int flags);

#ifndef MSG_TRUNC
#define MSG_TRUNC 1
#endif

	void os_socketModuleInit(void);
	void os_socketModuleExit(void);

#if defined (__cplusplus)
}
#endif

#endif /* OS_PLATFORM_SOCKET_H */
