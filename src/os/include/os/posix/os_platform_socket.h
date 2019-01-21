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

#ifdef __VXWORKS__
#include <vxWorks.h>
#include <sockLib.h>
#include <ioLib.h>
#endif /* __VXWORKS__ */
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>

#ifdef __APPLE__
#include <sys/sockio.h>
#endif /* __APPLE__ */
#include <unistd.h>

#if defined (__cplusplus)
extern "C" {
#endif

/* Keep defines before common header */
#define OS_SOCKET_HAS_DNS       1
#define OS_SOCKET_HAS_IPV6      1
#define OS_SOCKET_HAS_SA_LEN    1
#define OS_NO_SIOCGIFINDEX      1
#define OS_NO_NETLINK           1
#define OS_SOCKET_HAS_SSM       1

#define os_sockEAGAIN       EAGAIN      /* Operation would block, or a timeout expired before operation succeeded */
#define os_sockEWOULDBLOCK  EWOULDBLOCK /* Operation would block */
#define os_sockEPERM        EPERM       /* Operation not permitted */
#define os_sockENOENT       ENOENT      /* No such file or directory */
#define os_sockEINTR        EINTR       /* Interrupted system call */
#define os_sockEBADF        EBADF       /* Bad file number */
#define os_sockENOMEM       ENOMEM      /* Out of memory */
#define os_sockEACCES       EACCES      /* Permission denied */
#define os_sockEINVAL       EINVAL      /* Invalid argument */
#define os_sockEMFILE       EMFILE          /* Too many open files */
#define os_sockENOSR        ENOSR       /* Out of streams resources */
#define os_sockENOTSOCK     ENOTSOCK    /* Socket operation on non-socket */
#define os_sockEMSGSIZE     EMSGSIZE    /* Message too long */
#define os_sockENOPROTOOPT  ENOPROTOOPT /* Protocol not available */
#define os_sockEPROTONOSUPPORT  EPROTONOSUPPORT /* Protocol not supported */
#define os_sockEADDRINUSE   EADDRINUSE  /* Address already in use */
#define os_sockEADDRNOTAVAIL    EADDRNOTAVAIL   /* Cannot assign requested address */
#define os_sockENETUNREACH  ENETUNREACH /* Network is unreachable */
#define os_sockENOBUFS      ENOBUFS     /* No buffer space available */
#define os_sockECONNRESET   ECONNRESET  /* Connection reset by peer */
#define os_sockEPIPE        EPIPE       /* Connection reset by peer */

    typedef int os_socket; /* signed */
    #define PRIsock "d"

#define OS_INVALID_SOCKET (-1)

    typedef struct iovec os_iovec_t;
    typedef size_t os_iov_len_t;

#if defined(__sun) && !defined(_XPG4_2)
#define OS_MSGHDR_FLAGS 0
#else
#define OS_MSGHDR_FLAGS 1
#endif

#if defined(__linux)
typedef size_t os_msg_iovlen_t;
#else /* POSIX says int (which macOS, FreeBSD, Solaris do) */
typedef int os_msg_iovlen_t;
#endif

#if defined (__cplusplus)
}
#endif

#endif /* OS_PLATFORM_SOCKET_H */
