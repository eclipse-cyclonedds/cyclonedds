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
#include <stdbool.h>
#include <stddef.h>
#include <winerror.h>

#include "sockets_priv.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/time.h"

#ifdef ddsrt_select
#undef ddsrt_select /* See sockets.h for details. */
#endif

DDSRT_WARNING_GNUC_OFF(missing-prototypes)
DDSRT_WARNING_CLANG_OFF(missing-prototypes)

void
ddsrt_winsock_init(void)
{
  int err;
  WSADATA wsa_data;

  err = WSAStartup(MAKEWORD(2,0), &wsa_data);
  if (err != 0) {
    DDS_FATAL("WSAStartup(2.0, ...) failed with %d\n", err);
  }

  /* Confirm Windows Socket version 2.0 is supported. If versions greater
     than 2.0 in addition to 2.0 are supported, 2.0 will still be returned as
     that is the requested version. */
  if (LOBYTE(wsa_data.wVersion) != 2 ||
      HIBYTE(wsa_data.wVersion) != 0)
  {
    WSACleanup();
    DDS_FATAL("WSAStartup(2.0, ...) failed\n");
  }
}

void
ddsrt_winsock_fini(void)
{
  WSACleanup();
}

DDSRT_WARNING_GNUC_ON(missing-prototypes)
DDSRT_WARNING_CLANG_ON(missing-prototypes)

dds_return_t
ddsrt_socket(ddsrt_socket_t *sockptr, int domain, int type, int protocol)
{
  int err;
  ddsrt_socket_t sock = DDSRT_INVALID_SOCKET;

  assert(sockptr != NULL);

  sock = socket(domain, type, protocol);
  if (sock != INVALID_SOCKET) {
    *sockptr = sock;
    return DDS_RETCODE_OK;
  }

  err = WSAGetLastError();
  assert(err != WSANOTINITIALISED);
  assert(err != WSAEINVALIDPROCTABLE);
  assert(err != WSAEINVALIDPROVIDER);
  assert(err != WSAEPROVIDERFAILEDINIT);
  switch (err) {
    case WSAENETDOWN:
      return DDS_RETCODE_NO_NETWORK;
    case WSAEINVAL:
    case WSAEAFNOSUPPORT:
    case WSAEPROTONOSUPPORT:
    case WSAEPROTOTYPE:
    case WSAESOCKTNOSUPPORT:
      return DDS_RETCODE_BAD_PARAMETER;
    case WSAEINPROGRESS:
      return DDS_RETCODE_TRY_AGAIN;
    case WSAEMFILE:
    case WSAENOBUFS:
      return DDS_RETCODE_OUT_OF_RESOURCES;
    default:
      break;
  }

  return DDS_RETCODE_ERROR;
}

dds_return_t
ddsrt_close(ddsrt_socket_t sock)
{
  int err;

  if (closesocket(sock) != SOCKET_ERROR)
    return DDS_RETCODE_OK;

  err = WSAGetLastError();
  assert(err != WSANOTINITIALISED);
  switch (err) {
    case WSAENETDOWN:
      return DDS_RETCODE_NO_NETWORK;
    case WSAENOTSOCK:
      return DDS_RETCODE_BAD_PARAMETER;
    case WSAEINPROGRESS:
    case WSAEWOULDBLOCK:
      return DDS_RETCODE_TRY_AGAIN;
    case WSAEINTR:
      return DDS_RETCODE_INTERRUPTED;
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
  int err;
  ddsrt_socket_t conn;

  if ((conn = accept(sock, addr, addrlen)) != INVALID_SOCKET) {
    *connptr = conn;
    return DDS_RETCODE_OK;
  }

  err = WSAGetLastError();
  assert(err != WSANOTINITIALISED);
  switch (err) {
    case WSAECONNRESET:
      return DDS_RETCODE_NO_CONNECTION;
    case WSAEFAULT:
      return DDS_RETCODE_BAD_PARAMETER;
    case WSAEINTR:
      return DDS_RETCODE_INTERRUPTED;
    case WSAEINVAL:
    case WSAENOTSOCK:
    case WSAEOPNOTSUPP:
      return DDS_RETCODE_ILLEGAL_OPERATION;
    case WSAEINPROGRESS:
      return DDS_RETCODE_IN_PROGRESS;
    case WSAEMFILE:
    case WSAENOBUFS:
      return DDS_RETCODE_OUT_OF_RESOURCES;
    case WSAEWOULDBLOCK:
      return DDS_RETCODE_TRY_AGAIN;
    default:
      break;
  }

  return DDS_RETCODE_ERROR;
}

dds_return_t
ddsrt_bind(ddsrt_socket_t sock, const struct sockaddr *addr, socklen_t addrlen)
{
  int err;

  if (bind(sock, addr, addrlen) != SOCKET_ERROR)
    return DDS_RETCODE_OK;

  err = WSAGetLastError();
  assert(err != WSANOTINITIALISED);
  switch (err) {
    case WSAENETDOWN:
      return DDS_RETCODE_NO_NETWORK;
    case WSAEACCES:
      return DDS_RETCODE_NOT_ALLOWED;
    case WSAEADDRINUSE:
    case WSAEADDRNOTAVAIL:
      return DDS_RETCODE_PRECONDITION_NOT_MET;
    case WSAEFAULT:
    case WSAEINVAL:
    case WSAENOTSOCK:
      return DDS_RETCODE_BAD_PARAMETER;
    case WSAEINPROGRESS:
      return DDS_RETCODE_TRY_AGAIN;
    case WSAENOBUFS:
      return DDS_RETCODE_OUT_OF_RESOURCES;
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
  int err;

  if (listen(sock, backlog) != SOCKET_ERROR)
    return DDS_RETCODE_OK;

  err = WSAGetLastError();
  assert(err != WSANOTINITIALISED);
  switch (err) {
    case WSAENETDOWN:
      return DDS_RETCODE_NO_NETWORK;
    case WSAEADDRINUSE:
    case WSAEINVAL:
      return DDS_RETCODE_PRECONDITION_NOT_MET;
    case WSAEINPROGRESS:
      return DDS_RETCODE_TRY_AGAIN;
    case WSAEMFILE:
      return DDS_RETCODE_OUT_OF_RESOURCES;
    case WSAENOTSOCK:
    case WSAEOPNOTSUPP:
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
  int err;

  if (connect(sock, addr, addrlen) != SOCKET_ERROR)
    return DDS_RETCODE_OK;

  err = WSAGetLastError();
  assert(err != WSANOTINITIALISED);
  switch (err) {
    case WSAENETDOWN:
      return DDS_RETCODE_NO_NETWORK;
    case WSAEADDRINUSE:
    case WSAEADDRNOTAVAIL:
      return DDS_RETCODE_PRECONDITION_NOT_MET;
    case WSAEINTR:
      return DDS_RETCODE_INTERRUPTED;
    case WSAEINPROGRESS:
    case WSAEALREADY:
    case WSAEWOULDBLOCK:
      return DDS_RETCODE_TRY_AGAIN;
    case WSAEAFNOSUPPORT:
    case WSAEFAULT:
    case WSAEINVAL:
    case WSAENOTSOCK:
      return DDS_RETCODE_BAD_PARAMETER;
    case WSAECONNREFUSED:
    case WSAENETUNREACH:
    case WSAEHOSTUNREACH:
      return DDS_RETCODE_NO_CONNECTION;
    case WSAEISCONN:
    case WSAEACCES:
      return DDS_RETCODE_NOT_ALLOWED;
    case WSAENOBUFS:
      return DDS_RETCODE_OUT_OF_RESOURCES;
    case WSAETIMEDOUT:
      return DDS_RETCODE_TIMEOUT;
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
  int err;

  assert(sock != INVALID_SOCKET);
  assert(addr != NULL);

  if (getsockname(sock, addr, addrlen) != SOCKET_ERROR)
    return DDS_RETCODE_OK;

  err = WSAGetLastError();
  assert(err != WSANOTINITIALISED);
  switch (err) {
    case WSAENETDOWN:
      return DDS_RETCODE_NO_NETWORK;
    case WSAEFAULT: /* addrlen parameter is too small. */
      return DDS_RETCODE_OUT_OF_RESOURCES;
    case WSAEINPROGRESS:
      return DDS_RETCODE_TRY_AGAIN;
    case WSAENOTSOCK:
      return DDS_RETCODE_BAD_PARAMETER;
    case WSAEINVAL:
      return DDS_RETCODE_PRECONDITION_NOT_MET;
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
  int err, ret;

  if (level == IPPROTO_IP && (optname == IP_MULTICAST_TTL ||
                              optname == IP_MULTICAST_LOOP))
  {
    /* IP_MULTICAST_TTL and IP_MULTICAST_LOOP take a DWORD* rather than a
       char* on Windows. */
    int dwoptlen = sizeof(DWORD);
    DWORD dwoptval = *((unsigned char *)optval);
    ret = getsockopt(sock, level, optname, (char *)&dwoptval, &dwoptlen);
    if (ret != SOCKET_ERROR) {
      assert(dwoptlen == sizeof(DWORD));
      *((unsigned char *)optval) = (unsigned char)dwoptval;
      *optlen = sizeof( unsigned char );
    }
  } else {
    ret = getsockopt(sock, level, optname, optval, (int *)optlen);
  }

  if (ret != SOCKET_ERROR)
    return DDS_RETCODE_OK;

  err = WSAGetLastError();
  assert(err != WSANOTINITIALISED);
  switch (err) {
    case WSAENETDOWN:
      return DDS_RETCODE_NO_NETWORK;
    case WSAEFAULT:
    case WSAEINVAL:
    case WSAENOTSOCK:
      return DDS_RETCODE_BAD_PARAMETER;
    case WSAENOPROTOOPT:
      return DDS_RETCODE_UNSUPPORTED;
    case WSAEINPROGRESS:
      return DDS_RETCODE_TRY_AGAIN;
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
  int err, ret;
  DWORD dwoptval;

  if (level == IPPROTO_IP && (optname == IP_MULTICAST_TTL ||
                              optname == IP_MULTICAST_LOOP))
  {
    /* On win32 IP_MULTICAST_TTL and IP_MULTICAST_LOOP take DWORD * param
       rather than char * */
    dwoptval = *((unsigned char *)optval);
    optval = &dwoptval;
    optlen = sizeof(DWORD);
    ret = setsockopt(sock, level, optname, optval, (int)optlen);
  } else {
    ret = setsockopt(sock, level, optname, optval, (int)optlen);
  }

  if (ret != SOCKET_ERROR)
    return DDS_RETCODE_OK;

  err = WSAGetLastError();
  assert(err != WSANOTINITIALISED);
  switch (err) {
    case WSAENETDOWN:
      return DDS_RETCODE_NO_NETWORK;
    case WSAEFAULT:
    case WSAEINVAL:
    case WSAENOTSOCK:
      return DDS_RETCODE_BAD_PARAMETER;
    case WSAENOPROTOOPT:
      return DDS_RETCODE_UNSUPPORTED;
    case WSAEINPROGRESS:
      return DDS_RETCODE_IN_PROGRESS;
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
  int err;
  u_long mode;

  /* If mode = 0, blocking is enabled,
   * if mode != 0, non-blocking is enabled. */
  mode = nonblock ? 1 : 0;

  if (ioctlsocket(sock, (long)FIONBIO, &mode) != SOCKET_ERROR)
    return DDS_RETCODE_OK;

  err = WSAGetLastError();
  assert(err != WSANOTINITIALISED);
  switch (err) {
    case WSAENETDOWN:
      return DDS_RETCODE_NO_NETWORK;
    case WSAEINPROGRESS:
      return DDS_RETCODE_TRY_AGAIN;
    case WSAENOTSOCK:
      return DDS_RETCODE_BAD_PARAMETER;
    default:
      break;
  }

  return DDS_RETCODE_ERROR;
}

static dds_return_t recv_error_to_retcode(int errnum)
{
  assert(errnum != WSANOTINITIALISED);
  switch (errnum) {
    case WSAENETDOWN:
      return DDS_RETCODE_NO_NETWORK;
    case WSAEFAULT:
      return DDS_RETCODE_BAD_PARAMETER;
    case WSAENOTCONN:
    case WSAEINVAL:
      return DDS_RETCODE_PRECONDITION_NOT_MET;
    case WSAEINTR:
      return DDS_RETCODE_INTERRUPTED;
    case WSAEINPROGRESS:
    case WSAEWOULDBLOCK:
      return DDS_RETCODE_TRY_AGAIN;
    case WSAENETRESET:
    case WSAECONNABORTED:
    case WSAECONNRESET:
    case WSAETIMEDOUT:
      return DDS_RETCODE_NO_CONNECTION;
    case WSAEISCONN:
    case WSAENOTSOCK:
    case WSAEOPNOTSUPP:
    case WSAESHUTDOWN:
      return DDS_RETCODE_ILLEGAL_OPERATION;
    case WSAEMSGSIZE:
      return DDS_RETCODE_NOT_ENOUGH_SPACE;
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

  assert(len < INT_MAX);

  if ((n = recv(sock, (char *)buf, (int)len, flags)) != SOCKET_ERROR) {
    *rcvd = n;
    return DDS_RETCODE_OK;
  }

  return recv_error_to_retcode(WSAGetLastError());
}

dds_return_t
ddsrt_recvmsg(
  ddsrt_socket_t sock,
  ddsrt_msghdr_t *msg,
  int flags,
  ssize_t *rcvd)
{
  int err, n;

  assert(msg != NULL);
  assert(msg->msg_iovlen == 1);
  assert(msg->msg_controllen == 0);
  assert(msg->msg_iov[0].iov_len < INT_MAX);

  msg->msg_flags = 0;
  n = recvfrom(
    sock,
    msg->msg_iov[0].iov_base,
    (int)msg->msg_iov[0].iov_len,
    flags,
    msg->msg_name,
   &msg->msg_namelen);

  if (n != -1) {
    *rcvd = n;
    return DDS_RETCODE_OK;
  }

  err = WSAGetLastError();
  if (err == WSAEMSGSIZE) {
    /* Windows returns an error for too-large messages, UNIX expects the
       original size and the MSG_TRUNC flag. MSDN states it is truncated, which
       presumably means it returned as much of the message as it could. Return
       that the message was one byte larger than the available space and set
       MSG_TRUNC. */
    *rcvd = msg->msg_iov[0].iov_len + 1;
    msg->msg_flags |= MSG_TRUNC;
  }

  return recv_error_to_retcode(err);
}

static dds_return_t
send_error_to_retcode(int errnum)
{
  assert(errnum != WSANOTINITIALISED);
  switch (errnum) {
    case WSAENETDOWN: /* Network subsystem failed. */
      return DDS_RETCODE_NO_NETWORK;
    case WSAEACCES: /* Remote address is a broadcast address. */
      return DDS_RETCODE_NOT_ALLOWED;
    case WSAEINTR: /* Blocking sockets call was cancelled. */
      return DDS_RETCODE_INTERRUPTED;
    case WSAEINPROGRESS: /* Blocking sockets call in progress. */
    case WSA_IO_PENDING: /* Operation pending (WSASentTo). */
      return DDS_RETCODE_IN_PROGRESS;
    case WSAEFAULT: /* A parameter was not part of the user address space or
                       destination address was to small (WSASendTo). */
    case WSAEADDRNOTAVAIL: /* Remote address is not valid (WSASentTo). */
    case WSAEAFNOSUPPORT: /* Remote address is in wrong family (WSASentTo). */
      return DDS_RETCODE_BAD_PARAMETER;
    case WSAENETRESET: /* Time to live expired. */
    case WSAEHOSTUNREACH: /* Host is unreachable. */
    case WSAECONNABORTED: /* Time-out or other failure (send). */
    case WSAETIMEDOUT: /* Network or remote host failure (send). */
    case WSAENETUNREACH: /* Network is unreachable. (WSASentTo). */
    case WSA_OPERATION_ABORTED: /* Socket was closed (WSASentTo). */
      return DDS_RETCODE_NO_CONNECTION;
    case WSAENOBUFS:
      return DDS_RETCODE_OUT_OF_RESOURCES;
    case WSAENOTCONN: /* Socket is not connected. */
    case WSAEINVAL: /* Socket has not been bound. */
      return DDS_RETCODE_PRECONDITION_NOT_MET;
    case WSAENOTSOCK: /* Descriptor is not a socket. */
    case WSAEOPNOTSUPP: /* Operation not supported (send). */
    case WSAESHUTDOWN: /* Socket shut down. */
      return DDS_RETCODE_ILLEGAL_OPERATION;
    case WSAEWOULDBLOCK: /* Socket is nonblocking and call would block. */
      return DDS_RETCODE_TRY_AGAIN;
    case WSAEMSGSIZE: /* Message is larger than transport maximum. */
      return DDS_RETCODE_NOT_ENOUGH_SPACE;
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
  int n;

  assert(buf != NULL);
  assert(len <= INT_MAX);
  assert(sent != NULL);

  if ((n = send(sock, buf, (int)len, flags)) != SOCKET_ERROR) {
    *sent = n;
    return DDS_RETCODE_OK;
  }

  return send_error_to_retcode(WSAGetLastError());
}

/* Compile time check to ensure iovec matches WSABUF. */
struct iovec_matches_WSABUF {
  char sizeof_matches[sizeof(ddsrt_iovec_t) == sizeof(WSABUF) ? 1 : -1];
  char base_off_matches[offsetof(ddsrt_iovec_t, iov_base) == offsetof(WSABUF, buf) ? 1 : -1];
  char base_size_matches[sizeof(((ddsrt_iovec_t *)8)->iov_base) == sizeof(((WSABUF *)8)->buf) ? 1 : -1];
  char len_off_matches[offsetof(ddsrt_iovec_t, iov_len) == offsetof(WSABUF, len) ? 1 : -1];
  char len_size_matches[sizeof(((ddsrt_iovec_t *)8)->iov_len) == sizeof(((WSABUF *)8)->len) ? 1 : -1];
};

dds_return_t
ddsrt_sendmsg(
  ddsrt_socket_t sock,
  const ddsrt_msghdr_t *msg,
  int flags,
  ssize_t *sent)
{
  int ret;
  DWORD n;

  assert(msg != NULL);
  assert(msg->msg_controllen == 0);

  ret = WSASendTo(
        sock,
        (WSABUF *)msg->msg_iov,
        (DWORD)msg->msg_iovlen,
        &n,
        (DWORD)flags,
        (SOCKADDR *)msg->msg_name,
        msg->msg_namelen,
        NULL,
        NULL);
  if (ret != SOCKET_ERROR) {
    *sent = (ssize_t)n;
    return DDS_RETCODE_OK;
  }

  return send_error_to_retcode(WSAGetLastError());
}

dds_return_t
ddsrt_select(
  int32_t nfds,
  fd_set *readfds,
  fd_set *writefds,
  fd_set *errorfds,
  dds_duration_t reltime)
{
  int err;
  int32_t n;
  struct timeval tv = { .tv_sec = 0, .tv_usec = 0 }, *tvp = NULL;

  (void)nfds;

  tvp = ddsrt_duration_to_timeval_ceil(reltime, &tv);
  if ((n = select(-1, readfds, writefds, errorfds, tvp)) != SOCKET_ERROR)
    return (n == 0 ? DDS_RETCODE_TIMEOUT : n);

  err = WSAGetLastError();
  assert(err != WSANOTINITIALISED);
  switch (err) {
    case WSAEFAULT:
      return DDS_RETCODE_OUT_OF_RESOURCES;
    case WSAENETDOWN:
      return DDS_RETCODE_NO_NETWORK;
    case WSAEINVAL:
    case WSAENOTSOCK:
      return DDS_RETCODE_BAD_PARAMETER;
    case WSAEINPROGRESS:
      return DDS_RETCODE_TRY_AGAIN;
    default:
      break;
  }

  return DDS_RETCODE_ERROR;
}
