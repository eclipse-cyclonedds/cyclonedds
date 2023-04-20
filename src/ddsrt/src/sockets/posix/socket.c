// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "sockets_priv.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/misc.h"

#if !LWIP_SOCKET
#if defined(__VXWORKS__)
#include <vxWorks.h>
#include <sockLib.h>
#include <ioLib.h>
#elif !defined(__QNXNTO__)
#include <sys/fcntl.h>
#endif /* __VXWORKS__ */
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#if defined(__sun) || defined(__QNXNTO__)
#include <fcntl.h>
#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/sockio.h>
#endif /* __APPLE__ || __FreeBSD__ */
#endif /* LWIP_SOCKET */

#if defined(__ZEPHYR__) && defined(CONFIG_NET_IPV4_IGMP)
#include <zephyr/net/igmp.h>
#endif

dds_return_t
ddsrt_socket(ddsrt_socket_t *sockptr, int domain, int type, int protocol)
{
  ddsrt_socket_t sock;

  assert(sockptr != NULL);

  sock = socket(domain, type, protocol);
  if (sock != -1) {
    *sockptr = sock;
    return DDS_RETCODE_OK;
  }

  switch (errno) {
    case EACCES:
      return DDS_RETCODE_NOT_ALLOWED;
    case EAFNOSUPPORT:
    case EINVAL:
      return DDS_RETCODE_BAD_PARAMETER;
    case EMFILE:
    case ENFILE:
    case ENOBUFS:
    case ENOMEM:
      return DDS_RETCODE_OUT_OF_RESOURCES;
    default:
      break;
  }

  return DDS_RETCODE_ERROR;
}

dds_return_t
ddsrt_close(
  ddsrt_socket_t sock)
{
  if (close(sock) != -1)
    return DDS_RETCODE_OK;

  switch (errno) {
    case EBADF:
      return DDS_RETCODE_BAD_PARAMETER;
    case EINTR:
      return DDS_RETCODE_INTERRUPTED;
    default:
      break;
  }

  return DDS_RETCODE_ERROR;
}

dds_return_t
ddsrt_bind(
  ddsrt_socket_t sock,
  const struct sockaddr *addr,
  socklen_t addrlen)
{
  if (bind(sock, addr, addrlen) == 0)
    return DDS_RETCODE_OK;

  switch (errno) {
    case EACCES:
      return DDS_RETCODE_NOT_ALLOWED;
    case EADDRINUSE:
      return DDS_RETCODE_PRECONDITION_NOT_MET;
    case EBADF:
    case EINVAL:
    case ENOTSOCK:
      return DDS_RETCODE_BAD_PARAMETER;
    default:
      break;
  }

  return DDS_RETCODE_ERROR;
}

dds_return_t
ddsrt_listen(
  ddsrt_socket_t sock,
  int backlog)
{
  if (listen(sock, backlog) == 0)
    return DDS_RETCODE_OK;

  switch (errno) {
    case EADDRINUSE:
      return DDS_RETCODE_PRECONDITION_NOT_MET;
    case EBADF:
      return DDS_RETCODE_BAD_PARAMETER;
    case ENOTSOCK:
    case EOPNOTSUPP:
      return DDS_RETCODE_ILLEGAL_OPERATION;
    default:
      break;
  }

  return DDS_RETCODE_ERROR;
}

dds_return_t
ddsrt_connect(
  ddsrt_socket_t sock,
  const struct sockaddr *addr,
  socklen_t addrlen)
{
  if (connect(sock, addr, addrlen) == 0)
    return DDS_RETCODE_OK;

  switch (errno) {
    case EACCES:
    case EPERM:
    case EISCONN:
      return DDS_RETCODE_NOT_ALLOWED;
    case EADDRINUSE:
    case EADDRNOTAVAIL:
      return DDS_RETCODE_PRECONDITION_NOT_MET;
    case EAFNOSUPPORT:
    case EBADF:
    case ENOTSOCK:
    case EPROTOTYPE:
      return DDS_RETCODE_BAD_PARAMETER;
    case EAGAIN:
      return DDS_RETCODE_OUT_OF_RESOURCES;
    case EALREADY:
      return DDS_RETCODE_TRY_AGAIN;
    case ECONNREFUSED:
    case ENETUNREACH:
      return DDS_RETCODE_NO_CONNECTION;
    case EINPROGRESS:
      return DDS_RETCODE_IN_PROGRESS;
    case EINTR:
      return DDS_RETCODE_INTERRUPTED;
    case ETIMEDOUT:
      return DDS_RETCODE_TIMEOUT;
    default:
      break;
  }

  return DDS_RETCODE_ERROR;
}

dds_return_t
ddsrt_accept(
  ddsrt_socket_t sock,
  struct sockaddr *addr,
  socklen_t *addrlen,
  ddsrt_socket_t *connptr)
{
  ddsrt_socket_t conn;

  if ((conn = accept(sock, addr, addrlen)) != -1) {
    *connptr = conn;
    return DDS_RETCODE_OK;
  }

  switch (errno) {
    case EAGAIN:
#if EAGAIN != EWOULDBLOCK
    case EWOULDBLOCK:
#endif
      return DDS_RETCODE_TRY_AGAIN;
    case EBADF:
    case EFAULT:
    case EINVAL:
      return DDS_RETCODE_BAD_PARAMETER;
    case ECONNABORTED:
      return DDS_RETCODE_NO_CONNECTION;
    case EINTR:
      return DDS_RETCODE_INTERRUPTED;
    case EMFILE:
    case ENFILE:
    case ENOBUFS:
    case ENOMEM:
      return DDS_RETCODE_OUT_OF_RESOURCES;
    case ENOTSOCK:
    case EOPNOTSUPP:
      return DDS_RETCODE_ILLEGAL_OPERATION;
    case EPROTO:
      return DDS_RETCODE_ERROR;
    case EPERM:
      return DDS_RETCODE_NOT_ALLOWED;
    default:
      break;
  }

  return DDS_RETCODE_ERROR;
}

dds_return_t
ddsrt_getsockname(
  ddsrt_socket_t sock,
  struct sockaddr *addr,
  socklen_t *addrlen)
{
  if (getsockname(sock, addr, addrlen) == 0)
    return DDS_RETCODE_OK;

  switch (errno) {
    case EBADF:
    case EFAULT:
    case EINVAL:
    case ENOTSOCK:
      return DDS_RETCODE_BAD_PARAMETER;
    case ENOBUFS:
      return DDS_RETCODE_OUT_OF_RESOURCES;
    default:
      break;
  }

  return DDS_RETCODE_ERROR;
}

dds_return_t
ddsrt_getsockopt(
  ddsrt_socket_t sock,
  int32_t level,
  int32_t optname,
  void *optval,
  socklen_t *optlen)
{
#if defined(__ZEPHYR__)
  if (optname == IP_ADD_MEMBERSHIP || optname == IP_DROP_MEMBERSHIP)
  {
    /* note ddsrt_getsockopt never called with this optname */
    return DDS_RETCODE_UNSUPPORTED;
  }
#endif

  if (getsockopt(sock, level, optname, optval, optlen) == 0)
    return DDS_RETCODE_OK;

  switch (errno) {
    case EBADF:
    case EFAULT:
    case EINVAL:
    case ENOTSOCK:
      return DDS_RETCODE_BAD_PARAMETER;
    case ENOPROTOOPT:
      return DDS_RETCODE_UNSUPPORTED;
    default:
      break;
  }

  return DDS_RETCODE_ERROR;
}

dds_return_t
ddsrt_setsockopt(
  ddsrt_socket_t sock,
  int32_t level,
  int32_t optname,
  const void *optval,
  socklen_t optlen)
{
  switch (optname) {
    case SO_SNDBUF:
    case SO_RCVBUF:
      /* optlen == 4 && optval == 0 does not work. */
      if (!(optlen == 4 && *((unsigned *)optval) == 0)) {
        break;
      }
      /* falls through */
    case SO_DONTROUTE:
      /* SO_DONTROUTE causes problems on macOS (e.g. no multicasting). */
      return DDS_RETCODE_OK;
  }

#if defined(__ZEPHYR__)
  switch (optname) {
#if defined(DDSRT_HAVE_IPV6)
    case IPV6_MULTICAST_IF:
    case IPV6_MULTICAST_HOPS:
    case IPV6_MULTICAST_LOOP:
    case IPV6_UNICAST_HOPS:
      /* ignored */
      return DDS_RETCODE_OK;
    case IPV6_JOIN_GROUP:
    case IPV6_LEAVE_GROUP:
    {
      struct net_if *iface = NULL;
      struct ipv6_mreq *mreq = (struct ipv6_mreq*)optval;
      struct net_if_mcast_addr *maddr;
      assert(level == IPPROTO_IPV6);
      iface = net_if_get_by_index(mreq->ipv6mr_interface);
      if (iface) {
        maddr = net_if_ipv6_maddr_lookup(&(mreq->ipv6mr_multiaddr), &iface);
        if (optname == IPV6_JOIN_GROUP) {
          if (maddr) {
            /* already joined */
            return DDS_RETCODE_ERROR;
          } else {
            maddr = net_if_ipv6_maddr_add(iface, &(mreq->ipv6mr_multiaddr));
            if (maddr) {
              net_if_ipv6_maddr_join(maddr);
              net_if_mcast_monitor(iface, &(maddr->address), true);
              return DDS_RETCODE_OK;
            }
          }
        } else if (optname == IPV6_LEAVE_GROUP) {
          if (maddr) {
            if (net_if_ipv6_maddr_rm(iface, &(mreq->ipv6mr_multiaddr))) {
              net_if_ipv6_maddr_leave(maddr);
              net_if_mcast_monitor(iface, &(maddr->address), false);
              return DDS_RETCODE_OK;
            }
          }
        }
      }
      return DDS_RETCODE_ERROR;
    }
#endif /* DDSRT_HAVE_IPV6 */
    case IP_MULTICAST_IF:
    case IP_MULTICAST_TTL:
    case IP_MULTICAST_LOOP:
      /* ignored */
      return DDS_RETCODE_OK;
    case IP_ADD_MEMBERSHIP:
    case IP_DROP_MEMBERSHIP:
    {
      struct net_if *iface = NULL;
      struct ip_mreq *mreq = (struct ip_mreq*)optval;
      struct net_if_mcast_addr *maddr;
      assert(level == IPPROTO_IP);
      if (net_if_ipv4_addr_lookup(&(mreq->imr_interface), &iface)) {
#if defined(CONFIG_NET_IPV4_IGMP)
        int rc = -1;
        if (optname == IP_ADD_MEMBERSHIP) {
          rc = net_ipv4_igmp_join(iface, &(mreq->imr_multiaddr));
        } else {
          rc = net_ipv4_igmp_leave(iface, &(mreq->imr_multiaddr));
        }
        return (rc < 0) ? DDS_RETCODE_ERROR : DDS_RETCODE_OK;
#else
        maddr = net_if_ipv4_maddr_lookup(&(mreq->imr_multiaddr), &iface);
        if (optname == IP_ADD_MEMBERSHIP) {
          if (maddr && maddr->is_used) {
            /* already joined */
            return DDS_RETCODE_ERROR;
          } else {
            maddr = net_if_ipv4_maddr_add(iface, &(mreq->imr_multiaddr));
            if (maddr) {
              net_if_ipv4_maddr_join(maddr);
              net_if_mcast_monitor(iface, &(maddr->address), true);
              return DDS_RETCODE_OK;
            }
          }
        } else if (optname == IP_DROP_MEMBERSHIP) {
          if (maddr) {  
            if (net_if_ipv4_maddr_rm(iface, &(mreq->imr_multiaddr))) {
              net_if_ipv4_maddr_leave(maddr);
              net_if_mcast_monitor(iface, &(maddr->address), false);
              return DDS_RETCODE_OK;
            }
          }
        }
#endif /* CONFIG_NET_IPV4_IGMP */
      }
      return DDS_RETCODE_ERROR;
    }
  }
#endif /* __ZEPHYR__ */

  if (setsockopt(sock, level, optname, optval, optlen) == 0)
    return DDS_RETCODE_OK;

  switch (errno) {
    case EBADF:
    case EINVAL:
    case ENOTSOCK:
      return DDS_RETCODE_BAD_PARAMETER;
    case ENOPROTOOPT:
      return DDS_RETCODE_UNSUPPORTED;
    default:
      break;
  }

  return DDS_RETCODE_ERROR;
}

dds_return_t
ddsrt_setsocknonblocking(
  ddsrt_socket_t sock,
  bool nonblock)
{
  int flags;

  flags = fcntl(sock, F_GETFL, 0);
  if (flags == -1) {
    goto err_fcntl;
  } else {
    if (nonblock) {
      flags |= O_NONBLOCK;
    } else {
      flags &= ~O_NONBLOCK;
    }
    if (fcntl(sock, F_SETFL, flags) == -1) {
      goto err_fcntl;
    }
  }

  return DDS_RETCODE_OK;
err_fcntl:
  switch (errno) {
    case EACCES:
    case EAGAIN:
    case EPERM:
      return DDS_RETCODE_ERROR;
    case EBADF:
    case EINVAL:
      return DDS_RETCODE_BAD_PARAMETER;
    default:
      break;
  }

  return DDS_RETCODE_ERROR;
}

static inline dds_return_t
recv_error_to_retcode(int errnum)
{
  switch (errnum) {
    case EAGAIN:
#if EAGAIN != EWOULDBLOCK
    case EWOULDBLOCK:
#endif
      return DDS_RETCODE_TRY_AGAIN;
    case EBADF:
    case EFAULT:
    case EINVAL:
    case ENOTSOCK:
      return DDS_RETCODE_BAD_PARAMETER;
    case ECONNREFUSED:
      return DDS_RETCODE_NO_CONNECTION;
    case EINTR:
      return DDS_RETCODE_INTERRUPTED;
    case ENOMEM:
      return DDS_RETCODE_OUT_OF_RESOURCES;
    case ENOTCONN:
      return DDS_RETCODE_ILLEGAL_OPERATION;
    default:
      break;
  }

  return DDS_RETCODE_ERROR;
}

dds_return_t
ddsrt_recv(
  ddsrt_socket_t sock,
  void *buf,
  size_t len,
  int flags,
  ssize_t *rcvd)
{
  ssize_t n;

  if ((n = recv(sock, buf, len, flags)) != -1) {
    assert(n >= 0);
    *rcvd = n;
    return DDS_RETCODE_OK;
  }

  return recv_error_to_retcode(errno);
}

#if (LWIP_SOCKET && !defined(recvmsg)) || defined(__ZEPHYR__)
static ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags)
{
  assert(msg->msg_iovlen == 1);
  assert(msg->msg_controllen == 0);

  msg->msg_flags = 0;

  return recvfrom(
    sockfd,
    msg->msg_iov[0].iov_base,
    msg->msg_iov[0].iov_len,
    flags,
    msg->msg_name,
   &msg->msg_namelen);
}
#endif /* LWIP_SOCKET */

dds_return_t
ddsrt_recvmsg(
  ddsrt_socket_t sock,
  ddsrt_msghdr_t *msg,
  int flags,
  ssize_t *rcvd)
{
  ssize_t n;

  if ((n = recvmsg(sock, msg, flags)) != -1) {
    assert(n >= 0);
    *rcvd = n;
    return DDS_RETCODE_OK;
  }

  return recv_error_to_retcode(errno);
}

static inline dds_return_t
send_error_to_retcode(int errnum)
{
  switch (errnum) {
    case EACCES:
    case EPERM:
      return DDS_RETCODE_NOT_ALLOWED;
    case EAGAIN:
#if EAGAIN != EWOULDBLOCK
    case EWOULDBLOCK:
#endif
    case EALREADY:
      return DDS_RETCODE_TRY_AGAIN;
    case EBADF:
    case EFAULT:
    case EINVAL:
    case ENOTSOCK:
    case EOPNOTSUPP:
      return DDS_RETCODE_BAD_PARAMETER;
    case ECONNRESET:
      return DDS_RETCODE_NO_CONNECTION;
    case EDESTADDRREQ:
    case EISCONN:
    case ENOTCONN:
    case EPIPE:
      return DDS_RETCODE_ILLEGAL_OPERATION;
    case EINTR:
      return DDS_RETCODE_INTERRUPTED;
    case EMSGSIZE:
      return DDS_RETCODE_NOT_ENOUGH_SPACE;
    case ENOBUFS:
    case ENOMEM:
      return DDS_RETCODE_OUT_OF_RESOURCES;
    case EHOSTUNREACH:
    case EHOSTDOWN:
      return DDS_RETCODE_NO_CONNECTION;
    default:
      break;
  }

  return DDS_RETCODE_ERROR;
}

dds_return_t
ddsrt_send(
  ddsrt_socket_t sock,
  const void *buf,
  size_t len,
  int flags,
  ssize_t *sent)
{
  ssize_t n;

  if ((n = send(sock, buf, len, flags)) != -1) {
    assert(n >= 0);
    *sent = n;
    return DDS_RETCODE_OK;
  }

  return send_error_to_retcode(errno);
}

dds_return_t
ddsrt_sendmsg(
  ddsrt_socket_t sock,
  const ddsrt_msghdr_t *msg,
  int flags,
  ssize_t *sent)
{
  ssize_t n;

  if ((n = sendmsg(sock, msg, flags)) != -1) {
    assert(n >= 0);
    *sent = n;
    return DDS_RETCODE_OK;
  }

  return send_error_to_retcode(errno);
}

dds_return_t
ddsrt_select(
  int32_t nfds,
  fd_set *readfds,
  fd_set *writefds,
  fd_set *errorfds,
  dds_duration_t reltime)
{
  int n;
  struct timeval tv, *tvp = NULL;

  tvp = ddsrt_duration_to_timeval_ceil(reltime, &tv);
  if ((n = select(nfds, readfds, writefds, errorfds, tvp)) != -1)
    return (n == 0 ? DDS_RETCODE_TIMEOUT : n);

  switch (errno) {
    case EINTR:
      return DDS_RETCODE_INTERRUPTED;
    case EBADF:
    case EINVAL:
      return DDS_RETCODE_BAD_PARAMETER;
    case ENOMEM:
      return DDS_RETCODE_OUT_OF_RESOURCES;
    default:
      break;
  }

  return DDS_RETCODE_ERROR;
}
