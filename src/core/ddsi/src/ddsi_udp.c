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
#include <assert.h>
#include <string.h>
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/sockets.h"
#include "ddsi_eth.h"
#include "dds/ddsi/ddsi_tran.h"
#include "dds/ddsi/ddsi_udp.h"
#include "dds/ddsi/ddsi_ipaddr.h"
#include "dds/ddsi/ddsi_mcgroup.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/q_pcap.h"
#include "dds/ddsi/ddsi_domaingv.h"

union addr {
  struct sockaddr_storage x;
  struct sockaddr a;
  struct sockaddr_in a4;
#if DDSRT_HAVE_IPV6
  struct sockaddr_in6 a6;
#endif
};

typedef struct ddsi_udp_conn {
  struct ddsi_tran_conn m_base;
  ddsrt_socket_t m_sock;
#if defined _WIN32
  WSAEVENT m_sockEvent;
#endif
  int m_diffserv;
} *ddsi_udp_conn_t;

static void addr_to_loc (const struct ddsi_tran_factory *tran, nn_locator_t *dst, const union addr *src)
{
  ddsi_ipaddr_to_loc (tran, dst, &src->a, (src->a.sa_family == AF_INET) ? NN_LOCATOR_KIND_UDPv4 : NN_LOCATOR_KIND_UDPv6);
}

static ssize_t ddsi_udp_conn_read (ddsi_tran_conn_t conn_cmn, unsigned char * buf, size_t len, bool allow_spurious, nn_locator_t *srcloc)
{
  ddsi_udp_conn_t conn = (ddsi_udp_conn_t) conn_cmn;
  struct ddsi_domaingv * const gv = conn->m_base.m_base.gv;
  dds_return_t rc;
  ssize_t ret = 0;
  ddsrt_msghdr_t msghdr;
  union addr src;
  ddsrt_iovec_t msg_iov;
  socklen_t srclen = (socklen_t) sizeof (src);
  (void) allow_spurious;

  msg_iov.iov_base = (void *) buf;
  msg_iov.iov_len = (ddsrt_iov_len_t) len; /* Windows uses unsigned, POSIX (except Linux) int */

  msghdr.msg_name = &src.x;
  msghdr.msg_namelen = srclen;
  msghdr.msg_iov = &msg_iov;
  msghdr.msg_iovlen = 1;
#if defined(__sun) && !defined(_XPG4_2)
  msghdr.msg_accrights = NULL;
  msghdr.msg_accrightslen = 0;
#else
  msghdr.msg_control = NULL;
  msghdr.msg_controllen = 0;
#endif

  do {
    rc = ddsrt_recvmsg (conn->m_sock, &msghdr, 0, &ret);
  } while (rc == DDS_RETCODE_INTERRUPTED);

  if (ret > 0)
  {
    if (srcloc)
      addr_to_loc (conn->m_base.m_factory, srcloc, &src);

    if (gv->pcap_fp)
    {
      union addr dest;
      socklen_t dest_len = sizeof (dest);
      if (ddsrt_getsockname (conn->m_sock, &dest.a, &dest_len) != DDS_RETCODE_OK)
        memset (&dest, 0, sizeof (dest));
      write_pcap_received (gv, ddsrt_time_wallclock (), &src.x, &dest.x, buf, (size_t) ret);
    }

    /* Check for udp packet truncation */
#if DDSRT_MSGHDR_FLAGS
    const bool trunc_flag = (msghdr.msg_flags & MSG_TRUNC) != 0;
#else
    const bool trunc_flag = false;
#endif
    if ((size_t) ret > len || trunc_flag)
    {
      char addrbuf[DDSI_LOCSTRLEN];
      nn_locator_t tmp;
      addr_to_loc (conn->m_base.m_factory, &tmp, &src);
      ddsi_locator_to_string (addrbuf, sizeof (addrbuf), &tmp);
      GVWARNING ("%s => %d truncated to %d\n", addrbuf, (int) ret, (int) len);
    }
  }
  else if (rc != DDS_RETCODE_BAD_PARAMETER && rc != DDS_RETCODE_NO_CONNECTION)
  {
    GVERROR ("UDP recvmsg sock %d: ret %d retcode %"PRId32"\n", (int) conn->m_sock, (int) ret, rc);
    ret = -1;
  }
  return ret;
}

static void set_msghdr_iov (ddsrt_msghdr_t *mhdr, const ddsrt_iovec_t *iov, size_t iovlen)
{
  mhdr->msg_iov = (ddsrt_iovec_t *) iov;
  mhdr->msg_iovlen = (ddsrt_msg_iovlen_t) iovlen;
}

static ssize_t ddsi_udp_conn_write (ddsi_tran_conn_t conn_cmn, const nn_locator_t *dst, size_t niov, const ddsrt_iovec_t *iov, uint32_t flags)
{
  ddsi_udp_conn_t conn = (ddsi_udp_conn_t) conn_cmn;
  struct ddsi_domaingv * const gv = conn->m_base.m_base.gv;
  dds_return_t rc;
  ssize_t ret = -1;
  unsigned retry = 2;
  int sendflags = 0;
  ddsrt_msghdr_t msg;
  union addr dstaddr;
  assert (niov <= INT_MAX);
  ddsi_ipaddr_from_loc (&dstaddr.x, dst);
  set_msghdr_iov (&msg, iov, niov);
  msg.msg_name = &dstaddr.x;
  msg.msg_namelen = (socklen_t) ddsrt_sockaddr_get_size (&dstaddr.a);
#if defined(__sun) && !defined(_XPG4_2)
  msg.msg_accrights = NULL;
  msg.msg_accrightslen = 0;
#else
  msg.msg_control = NULL;
  msg.msg_controllen = 0;
#endif
#if DDSRT_MSGHDR_FLAGS
  msg.msg_flags = (int) flags;
#else
  DDSRT_UNUSED_ARG (flags);
#endif
#if MSG_NOSIGNAL && !LWIP_SOCKET
  sendflags |= MSG_NOSIGNAL;
#endif
  do {
    rc = ddsrt_sendmsg (conn->m_sock, &msg, sendflags, &ret);
#if defined _WIN32 && !defined WINCE
    if (rc == DDS_RETCODE_TRY_AGAIN)
    {
      WSANETWORKEVENTS ev;
      WaitForSingleObject (conn->m_sockEvent, INFINITE);
      WSAEnumNetworkEvents (conn->m_sock, conn->m_sockEvent, &ev);
    }
#endif
  } while (rc == DDS_RETCODE_INTERRUPTED || rc == DDS_RETCODE_TRY_AGAIN || (rc == DDS_RETCODE_NOT_ALLOWED && retry-- > 0));
  if (ret > 0 && gv->pcap_fp)
  {
    union addr sa;
    socklen_t alen = sizeof (sa);
    if (ddsrt_getsockname (conn->m_sock, &sa.a, &alen) != DDS_RETCODE_OK)
      memset(&sa, 0, sizeof(sa));
    write_pcap_sent (gv, ddsrt_time_wallclock (), &sa.x, &msg, (size_t) ret);
  }
  else if (rc != DDS_RETCODE_OK && rc != DDS_RETCODE_NOT_ALLOWED && rc != DDS_RETCODE_NO_CONNECTION)
  {
    GVERROR ("ddsi_udp_conn_write failed with retcode %"PRId32"\n", rc);
  }
  return (rc == DDS_RETCODE_OK) ? ret : -1;
}

static void ddsi_udp_disable_multiplexing (ddsi_tran_conn_t conn_cmn)
{
#if defined _WIN32 && !defined WINCE
  ddsi_udp_conn_t conn = (ddsi_udp_conn_t) conn_cmn;
  uint32_t zero = 0, dummy;
  WSAEventSelect (conn->m_sock, 0, 0);
  WSAIoctl (conn->m_sock, FIONBIO, &zero,sizeof(zero), NULL,0, &dummy, NULL,NULL);
#else
  (void) conn_cmn;
#endif
}

static ddsrt_socket_t ddsi_udp_conn_handle (ddsi_tran_base_t conn_cmn)
{
  ddsi_udp_conn_t conn = (ddsi_udp_conn_t) conn_cmn;
  return conn->m_sock;
}

static bool ddsi_udp_supports (const struct ddsi_tran_factory *fact, int32_t kind)
{
  return kind == fact->m_kind || (kind == NN_LOCATOR_KIND_UDPv4MCGEN && fact->m_kind == NN_LOCATOR_KIND_UDPv4);
}

static int ddsi_udp_conn_locator (ddsi_tran_factory_t fact, ddsi_tran_base_t conn_cmn, nn_locator_t *loc)
{
  ddsi_udp_conn_t conn = (ddsi_udp_conn_t) conn_cmn;
  int ret = -1;
  if (conn->m_sock != DDSRT_INVALID_SOCKET)
  {
    loc->kind = fact->m_kind;
    loc->port = conn->m_base.m_base.m_port;
    memcpy (loc->address, conn->m_base.m_base.gv->extloc.address, sizeof (loc->address));
    ret = 0;
  }
  return ret;
}

static uint16_t get_socket_port (struct ddsi_domaingv const * const gv, ddsrt_socket_t sock)
{
  dds_return_t ret;
  union addr addr;
  socklen_t addrlen = sizeof (addr);
  ret = ddsrt_getsockname (sock, &addr.a, &addrlen);
  if (ret != DDS_RETCODE_OK)
  {
    GVERROR ("ddsi_udp_get_socket_port: getsockname returned %"PRId32"\n", ret);
    return 0;
  }
  return ddsrt_sockaddr_get_port (&addr.a);
}

static dds_return_t set_dont_route (struct ddsi_domaingv const * const gv, ddsrt_socket_t socket, bool ipv6)
{
  dds_return_t rc;
#if DDSRT_HAVE_IPV6
  if (ipv6)
  {
    const unsigned uone = 1;
    if ((rc = ddsrt_setsockopt (socket, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &uone, sizeof (uone))) != DDS_RETCODE_OK)
      GVERROR ("ddsi_udp_create_conn: set IPV6_UNICAST_HOPS = 1 failed: %s\n", dds_strretcode (rc));
    return rc;
  }
#else
  (void) ipv6;
#endif
  const int one = 1;
  if ((rc = ddsrt_setsockopt (socket, SOL_SOCKET, SO_DONTROUTE, &one, sizeof (one))) != DDS_RETCODE_OK)
    GVERROR ("ddsi_udp_create_conn: set SO_DONTROUTE = 1 failed: %s\n", dds_strretcode (rc));
  return rc;
}

static dds_return_t set_buf (
  struct ddsi_domaingv const * const gv,
  ddsrt_socket_t sock,
  int optname,
  const struct config_maybe_uint32 *maybe_min_size,
  const struct config_maybe_uint32 *maybe_max_size)
{
  const char * str_optname;
  switch (optname){
    case SO_RCVBUF:
      str_optname = "SO_RCVBUF";
      break;
    case SO_SNDBUF:
      str_optname = "SO_SNDBUF";
      break;
    default:
      return DDS_RETCODE_BAD_PARAMETER;
  }

  if (maybe_max_size->isdefault) {
    GVLOG(DDS_LC_CONFIG, "%s: Using system default %s buffer size\n", __func__, str_optname);
  } else {
    size_t optval = maybe_max_size->value;
    socklen_t optlen = sizeof(optval);
    GVLOG(DDS_LC_CONFIG, "%s: Requesting %s buffer of size %zu bytes\n", __func__, str_optname, optval);
    dds_return_t rc = ddsrt_setsockopt(sock, SOL_SOCKET, optname, &optval, optlen);
    switch (rc) {
      case DDS_RETCODE_OK:
        break;
      case DDS_RETCODE_UNSUPPORTED:
        /* not all stacks support getting/setting buffer size */
        GVLOG(DDS_LC_CONFIG, "%s: Cannot set %s\n", __func__, str_optname);
        break;
      default:
        GVLOG(DDS_LC_ERROR | DDS_LC_CONFIG, "%s: Set %s failed: %s\n", __func__, str_optname, dds_strretcode(rc));
        return rc;
    }
  }

  if (maybe_min_size->isdefault) {
    GVLOG(DDS_LC_CONFIG, "%s: Not checking %s buffer size\n", __func__, str_optname);
  } else {
    size_t min_size = maybe_min_size->value;
    size_t optval = 0;
    socklen_t optlen = sizeof(optval);
    GVLOG(DDS_LC_CONFIG, "%s: Checking buffer %s is at least %zu bytes\n", __func__, str_optname, optval);
    dds_return_t rc = ddsrt_getsockopt(sock, SOL_SOCKET, optname, &optval, &optlen);
    switch (rc) {
      case DDS_RETCODE_OK:
        break;
      case DDS_RETCODE_UNSUPPORTED:
        /* not all stacks support getting/setting buffer size */
        GVLOG(DDS_LC_WARNING | DDS_LC_CONFIG, "%s: Cannot get %s\n", __func__, str_optname);
        break;
      default:
        GVLOG(DDS_LC_ERROR | DDS_LC_CONFIG, "%s: Get %s failed: %s\n", __func__, str_optname, dds_strretcode(rc));
        return rc;
    }
    if (min_size <= optval ) {
      GVLOG(DDS_LC_CONFIG, "%s: %s is %zu bytes, which is OK. (minimum %zu bytes)\n", __func__, str_optname, optval, min_size);
    } else {
      GVLOG (DDS_LC_CONFIG | DDS_LC_ERROR, "%s: %s is too small! Got %zu bytes, but minimum is %zu bytes\n",
             __func__, str_optname, optval, min_size);
      return DDS_RETCODE_ERROR;
    }
  }
  return DDS_RETCODE_OK;
}

static dds_return_t set_mc_options_transmit_ipv6 (struct ddsi_domaingv const * const gv, ddsrt_socket_t sock)
{
  /* Function is a never-called no-op if IPv6 is not supported to keep the call-site a bit cleaner  */
#if DDSRT_HAVE_IPV6
  const unsigned ifno = gv->interfaceNo;
  const unsigned ttl = (unsigned) gv->config.multicast_ttl;
  const unsigned loop = (unsigned) !!gv->config.enableMulticastLoopback;
  dds_return_t rc;
  if ((rc = ddsrt_setsockopt (sock, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifno, sizeof (ifno))) != DDS_RETCODE_OK) {
    GVERROR ("ddsi_udp_create_conn: set IPV6_MULTICAST_IF failed: %s\n", dds_strretcode (rc));
    return rc;
  }
  if ((rc = ddsrt_setsockopt (sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &ttl, sizeof (ttl))) != DDS_RETCODE_OK) {
    GVERROR ("ddsi_udp_create_conn: set IPV6_MULTICAST_HOPS failed: %s\n", dds_strretcode (rc));
    return rc;
  }
  if ((rc = ddsrt_setsockopt (sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop, sizeof (loop))) != DDS_RETCODE_OK) {
    GVERROR ("ddsi_udp_create_conn: set IPV6_MULTICAST_LOOP failed: %s\n", dds_strretcode (rc));
    return rc;
  }
  return DDS_RETCODE_OK;
#else
  (void) gv; (void) sock;
  return DDS_RETCODE_ERROR;
#endif
}

static dds_return_t set_mc_options_transmit_ipv4_if (struct ddsi_domaingv const * const gv, ddsrt_socket_t sock)
{
#if (defined(__linux) || defined(__APPLE__)) && !LWIP_SOCKET
  if (gv->config.use_multicast_if_mreqn)
  {
    struct ip_mreqn mreqn;
    memset (&mreqn, 0, sizeof (mreqn));
    /* looks like imr_multiaddr is not relevant, not sure about imr_address */
    mreqn.imr_multiaddr.s_addr = htonl (INADDR_ANY);
    if (gv->config.use_multicast_if_mreqn > 1)
      memcpy (&mreqn.imr_address.s_addr, gv->ownloc.address + 12, 4);
    else
      mreqn.imr_address.s_addr = htonl (INADDR_ANY);
    mreqn.imr_ifindex = (int) gv->interfaceNo;
    return ddsrt_setsockopt (sock, IPPROTO_IP, IP_MULTICAST_IF, &mreqn, sizeof (mreqn));
  }
#endif
  return ddsrt_setsockopt (sock, IPPROTO_IP, IP_MULTICAST_IF, gv->ownloc.address + 12, 4);
}

static dds_return_t set_mc_options_transmit_ipv4 (struct ddsi_domaingv const * const gv, ddsrt_socket_t sock)
{
  const unsigned char ttl = (unsigned char) gv->config.multicast_ttl;
  const unsigned char loop = (unsigned char) !!gv->config.enableMulticastLoopback;
  dds_return_t rc;
  if ((rc = set_mc_options_transmit_ipv4_if (gv, sock)) != DDS_RETCODE_OK) {
    GVERROR ("ddsi_udp_create_conn: set IP_MULTICAST_IF failed: %s\n", dds_strretcode (rc));
    return rc;
  }
  if ((rc = ddsrt_setsockopt (sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof (ttl))) != DDS_RETCODE_OK) {
    GVERROR ("ddsi_udp_create_conn: set IP_MULTICAST_TTL failed: %s\n", dds_strretcode (rc));
    return rc;
  }
  if ((rc = ddsrt_setsockopt (sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof (loop))) != DDS_RETCODE_OK) {
    GVERROR ("ddsi_udp_create_conn: set IP_MULTICAST_LOOP failed: %s\n", dds_strretcode (rc));
    return rc;
  }
  return DDS_RETCODE_OK;
}

static dds_return_t ddsi_udp_create_conn (ddsi_tran_conn_t *conn_out, ddsi_tran_factory_t fact, uint32_t port, const ddsi_tran_qos_t *qos)
{
  struct ddsi_domaingv const * const gv = fact->gv;
  const int one = 1;

  dds_return_t rc;
  ddsrt_socket_t sock;
  bool reuse_addr = false, bind_to_any = false, ipv6 = false;
  const char *purpose_str = NULL;

  switch (qos->m_purpose)
  {
    case DDSI_TRAN_QOS_XMIT:
      reuse_addr = false;
      bind_to_any = false;
      purpose_str = "transmit";
      break;
    case DDSI_TRAN_QOS_RECV_UC:
      reuse_addr = false;
      bind_to_any = true;
      purpose_str = "unicast";
      break;
    case DDSI_TRAN_QOS_RECV_MC:
      reuse_addr = true;
      bind_to_any = true;
      purpose_str = "multicast";
      break;
  }
  assert (purpose_str != NULL);

  union addr socketname;
  nn_locator_t ownloc_w_port = gv->ownloc;
  assert (ownloc_w_port.port == NN_LOCATOR_PORT_INVALID);
  if (port) {
    /* PORT_INVALID maps to 0 in ipaddr_from_loc */
    ownloc_w_port.port = port;
  }
  ddsi_ipaddr_from_loc (&socketname.x, &ownloc_w_port);
  switch (fact->m_kind)
  {
    case NN_LOCATOR_KIND_UDPv4:
      if (bind_to_any)
        socketname.a4.sin_addr.s_addr = htonl (INADDR_ANY);
      break;
#if DDSRT_HAVE_IPV6
    case NN_LOCATOR_KIND_UDPv6:
      ipv6 = true;
      if (bind_to_any)
        socketname.a6.sin6_addr = ddsrt_in6addr_any;
      break;
#endif
    default:
      DDS_FATAL ("ddsi_udp_create_conn: unsupported kind %"PRId32"\n", fact->m_kind);
  }
  if ((rc = ddsrt_socket (&sock, socketname.a.sa_family, SOCK_DGRAM, 0)) != DDS_RETCODE_OK)
  {
    GVERROR ("ddsi_udp_create_conn: failed to create socket: %s\n", dds_strretcode (rc));
    goto fail;
  }

  if (reuse_addr && (rc = ddsrt_setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one))) != DDS_RETCODE_OK)
  {
    GVERROR ("ddsi_udp_create_conn: failed to enable address reuse: %s\n", dds_strretcode (rc));
    if (rc != DDS_RETCODE_UNSUPPORTED)
    {
      /* There must at some point have been an implementation that refused to do SO_REUSEADDR, but I
         don't know which */
      goto fail_w_socket;
    }
  }

  if ((rc = set_buf (gv, sock, SO_RCVBUF, &gv->config.socket_min_rcvbuf_size, &gv->config.socket_max_rcvbuf_size)) != DDS_RETCODE_OK)
    goto fail_w_socket;
  if ((rc = set_buf (gv, sock, SO_SNDBUF, &gv->config.socket_min_sndbuf_size, &gv->config.socket_max_sndbuf_size)) != DDS_RETCODE_OK)
    goto fail_w_socket;
  if (gv->config.dontRoute && (rc = set_dont_route (gv, sock, ipv6)) != DDS_RETCODE_OK)
    goto fail_w_socket;

  if ((rc = ddsrt_bind (sock, &socketname.a, ddsrt_sockaddr_get_size (&socketname.a))) != DDS_RETCODE_OK)
  {
    /* PRECONDITION_NOT_MET (= EADDRINUSE) is expected if reuse_addr isn't set, should be handled at
       a higher level and therefore needs to return a specific error message */
    if (!reuse_addr && rc == DDS_RETCODE_PRECONDITION_NOT_MET)
      goto fail_addrinuse;

    char buf[DDSI_LOCATORSTRLEN];
    if (bind_to_any)
      snprintf (buf, sizeof (buf), "ANY:%"PRIu32, port);
    else
      ddsi_locator_to_string (buf, sizeof (buf), &ownloc_w_port);
    GVERROR ("ddsi_udp_create_conn: failed to bind to %s: %s\n", buf,
             (rc == DDS_RETCODE_PRECONDITION_NOT_MET) ? "address in use" : dds_strretcode (rc));
    goto fail_w_socket;
  }

  rc = ipv6 ? set_mc_options_transmit_ipv6 (gv, sock) : set_mc_options_transmit_ipv4 (gv, sock);
  if (rc != DDS_RETCODE_OK)
    goto fail_w_socket;

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  if (qos->m_diffserv != 0 && fact->m_kind == NN_LOCATOR_KIND_UDPv4)
  {
    if ((rc = ddsrt_setsockopt (sock, IPPROTO_IP, IP_TOS, &qos->m_diffserv, sizeof (qos->m_diffserv))) != DDS_RETCODE_OK)
    {
      GVERROR ("ddsi_udp_create_conn: set diffserv retcode %"PRId32"\n", rc);
      goto fail_w_socket;
    }
  }
#endif

  ddsi_udp_conn_t conn = ddsrt_malloc (sizeof (*conn));
  memset (conn, 0, sizeof (*conn));

  conn->m_sock = sock;
  conn->m_diffserv = qos->m_diffserv;
#if defined _WIN32 && !defined WINCE
  conn->m_sockEvent = WSACreateEvent ();
  WSAEventSelect (conn->m_sock, conn->m_sockEvent, FD_WRITE);
#endif

  ddsi_factory_conn_init (fact, &conn->m_base);
  conn->m_base.m_base.m_port = get_socket_port (gv, sock);
  conn->m_base.m_base.m_trantype = DDSI_TRAN_CONN;
  conn->m_base.m_base.m_multicast = (qos->m_purpose == DDSI_TRAN_QOS_RECV_MC);
  conn->m_base.m_base.m_handle_fn = ddsi_udp_conn_handle;

  conn->m_base.m_read_fn = ddsi_udp_conn_read;
  conn->m_base.m_write_fn = ddsi_udp_conn_write;
  conn->m_base.m_disable_multiplexing_fn = ddsi_udp_disable_multiplexing;
  conn->m_base.m_locator_fn = ddsi_udp_conn_locator;

  GVTRACE ("ddsi_udp_create_conn %s socket %"PRIdSOCK" port %"PRIu32"\n", purpose_str, conn->m_sock, conn->m_base.m_base.m_port);
  *conn_out = &conn->m_base;
  return DDS_RETCODE_OK;

fail_w_socket:
  ddsrt_close (sock);
fail:
  return DDS_RETCODE_ERROR;

fail_addrinuse:
  ddsrt_close (sock);
  return DDS_RETCODE_PRECONDITION_NOT_MET;
}

static int joinleave_asm_mcgroup (ddsrt_socket_t socket, int join, const nn_locator_t *mcloc, const struct nn_interface *interf)
{
  dds_return_t rc;
  union addr mcip;
  ddsi_ipaddr_from_loc (&mcip.x, mcloc);
#if DDSRT_HAVE_IPV6
  if (mcloc->kind == NN_LOCATOR_KIND_UDPv6)
  {
    struct ipv6_mreq ipv6mreq;
    memset (&ipv6mreq, 0, sizeof (ipv6mreq));
    ipv6mreq.ipv6mr_multiaddr = mcip.a6.sin6_addr;
    ipv6mreq.ipv6mr_interface = interf ? interf->if_index : 0;
    rc = ddsrt_setsockopt (socket, IPPROTO_IPV6, join ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP, &ipv6mreq, sizeof (ipv6mreq));
  }
  else
#endif
  {
    struct ip_mreq mreq;
    mreq.imr_multiaddr = mcip.a4.sin_addr;
    if (interf)
      memcpy (&mreq.imr_interface, interf->loc.address + 12, sizeof (mreq.imr_interface));
    else
      mreq.imr_interface.s_addr = htonl (INADDR_ANY);
    rc = ddsrt_setsockopt (socket, IPPROTO_IP, join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP, &mreq, sizeof (mreq));
  }
  return (rc == DDS_RETCODE_OK) ? 0 : -1;
}

#ifdef DDSI_INCLUDE_SSM
static int joinleave_ssm_mcgroup (ddsrt_socket_t socket, int join, const nn_locator_t *srcloc, const nn_locator_t *mcloc, const struct nn_interface *interf)
{
  dds_return_t rc;
  union addr mcip, srcip;
  ddsi_ipaddr_from_loc (&mcip.x, mcloc);
  ddsi_ipaddr_from_loc (&srcip.x, srcloc);
#if DDSRT_HAVE_IPV6
  if (mcloc->kind == NN_LOCATOR_KIND_UDPv6)
  {
    struct group_source_req gsr;
    memset (&gsr, 0, sizeof (gsr));
    gsr.gsr_interface = interf ? interf->if_index : 0;
    gsr.gsr_group = mcip.x;
    gsr.gsr_source = srcip.x;
    rc = ddsrt_setsockopt (socket, IPPROTO_IPV6, join ? MCAST_JOIN_SOURCE_GROUP : MCAST_LEAVE_SOURCE_GROUP, &gsr, sizeof (gsr));
  }
  else
#endif
  {
    struct ip_mreq_source mreq;
    memset (&mreq, 0, sizeof (mreq));
    mreq.imr_sourceaddr = srcip.a4.sin_addr;
    mreq.imr_multiaddr = mcip.a4.sin_addr;
    if (interf)
      memcpy (&mreq.imr_interface, interf->loc.address + 12, sizeof (mreq.imr_interface));
    else
      mreq.imr_interface.s_addr = INADDR_ANY;
    rc = ddsrt_setsockopt (socket, IPPROTO_IP, join ? IP_ADD_SOURCE_MEMBERSHIP : IP_DROP_SOURCE_MEMBERSHIP, &mreq, sizeof (mreq));
  }
  return (rc == DDS_RETCODE_OK) ? 0 : -1;
}
#endif

static int ddsi_udp_join_mc (ddsi_tran_conn_t conn_cmn, const nn_locator_t *srcloc, const nn_locator_t *mcloc, const struct nn_interface *interf)
{
  ddsi_udp_conn_t conn = (ddsi_udp_conn_t) conn_cmn;
  (void) srcloc;
#ifdef DDSI_INCLUDE_SSM
  if (srcloc)
    return joinleave_ssm_mcgroup (conn->m_sock, 1, srcloc, mcloc, interf);
  else
#endif
    return joinleave_asm_mcgroup (conn->m_sock, 1, mcloc, interf);
}

static int ddsi_udp_leave_mc (ddsi_tran_conn_t conn_cmn, const nn_locator_t *srcloc, const nn_locator_t *mcloc, const struct nn_interface *interf)
{
  ddsi_udp_conn_t conn = (ddsi_udp_conn_t) conn_cmn;
  (void) srcloc;
#ifdef DDSI_INCLUDE_SSM
  if (srcloc)
    return joinleave_ssm_mcgroup (conn->m_sock, 0, srcloc, mcloc, interf);
  else
#endif
    return joinleave_asm_mcgroup (conn->m_sock, 0, mcloc, interf);
}

static void ddsi_udp_release_conn (ddsi_tran_conn_t conn_cmn)
{
  ddsi_udp_conn_t conn = (ddsi_udp_conn_t) conn_cmn;
  struct ddsi_domaingv const * const gv = conn->m_base.m_base.gv;
  GVTRACE ("ddsi_udp_release_conn %s socket %"PRIdSOCK" port %"PRIu32"\n",
           conn_cmn->m_base.m_multicast ? "multicast" : "unicast",
           conn->m_sock, conn->m_base.m_base.m_port);
  ddsrt_close (conn->m_sock);
#if defined _WIN32 && !defined WINCE
  WSACloseEvent (conn->m_sockEvent);
#endif
  ddsrt_free (conn_cmn);
}

static int ddsi_udp_is_mcaddr (const ddsi_tran_factory_t tran, const nn_locator_t *loc)
{
  (void) tran;
  switch (loc->kind)
  {
    case NN_LOCATOR_KIND_UDPv4: {
      const struct in_addr *ipv4 = (const struct in_addr *) (loc->address + 12);
      return IN_MULTICAST (ntohl (ipv4->s_addr));
    }
    case NN_LOCATOR_KIND_UDPv4MCGEN: {
      const nn_udpv4mcgen_address_t *mcgen = (const nn_udpv4mcgen_address_t *) loc->address;
      return IN_MULTICAST (ntohl (mcgen->ipv4.s_addr));
    }
#if DDSRT_HAVE_IPV6
    case NN_LOCATOR_KIND_UDPv6: {
      const struct in6_addr *ipv6 = (const struct in6_addr *) loc->address;
      return IN6_IS_ADDR_MULTICAST (ipv6);
    }
#endif
    default: {
      return 0;
    }
  }
}

#ifdef DDSI_INCLUDE_SSM
static int ddsi_udp_is_ssm_mcaddr (const ddsi_tran_factory_t tran, const nn_locator_t *loc)
{
  (void) tran;
  switch (loc->kind)
  {
    case NN_LOCATOR_KIND_UDPv4: {
      const struct in_addr *x = (const struct in_addr *) (loc->address + 12);
      return (((uint32_t) ntohl (x->s_addr)) >> 24) == 232;
    }
#if DDSRT_HAVE_IPV6
    case NN_LOCATOR_KIND_UDPv6: {
      const struct in6_addr *x = (const struct in6_addr *) loc->address;
      return x->s6_addr[0] == 0xff && (x->s6_addr[1] & 0xf0) == 0x30;
    }
#endif
    default: {
      return 0;
    }
  }
}
#endif

static enum ddsi_locator_from_string_result ddsi_udp_address_from_string (ddsi_tran_factory_t tran, nn_locator_t *loc, const char *str)
{
  return ddsi_ipaddr_from_string (tran, loc, str, tran->m_kind);
}

static char *ddsi_udp_locator_to_string (char *dst, size_t sizeof_dst, const nn_locator_t *loc, int with_port)
{
  if (loc->kind != NN_LOCATOR_KIND_UDPv4MCGEN) {
    return ddsi_ipaddr_to_string(dst, sizeof_dst, loc, with_port);
  } else {
    struct sockaddr_in src;
    nn_udpv4mcgen_address_t mcgen;
    size_t pos;
    int cnt;
    assert(sizeof_dst > 1);
    memcpy (&mcgen, loc->address, sizeof (mcgen));
    memset (&src, 0, sizeof (src));
    src.sin_family = AF_INET;
    memcpy (&src.sin_addr.s_addr, &mcgen.ipv4, 4);
    ddsrt_sockaddrtostr ((const struct sockaddr *) &src, dst, sizeof_dst);
    pos = strlen (dst);
    assert (pos <= sizeof_dst);
    cnt = snprintf (dst + pos, sizeof_dst - pos, ";%u;%u;%u", mcgen.base, mcgen.count, mcgen.idx);
    if (cnt > 0) {
      pos += (size_t)cnt;
    }
    if (with_port && pos < sizeof_dst) {
      (void) snprintf (dst + pos, sizeof_dst - pos, ":%"PRIu32, loc->port);
    }
    return dst;
  }
}

static void ddsi_udp_fini (ddsi_tran_factory_t fact)
{
  struct ddsi_domaingv const * const gv = fact->gv;
  GVLOG (DDS_LC_CONFIG, "udp finalized\n");
  ddsrt_free (fact);
}

static int ddsi_udp_is_valid_port (ddsi_tran_factory_t fact, uint32_t port)
{
  (void) fact;
  return (port <= 65535);
}

int ddsi_udp_init (struct ddsi_domaingv*gv)
{
  struct ddsi_tran_factory *fact = ddsrt_malloc (sizeof (*fact));
  memset (fact, 0, sizeof (*fact));
  fact->gv = gv;
  fact->m_free_fn = ddsi_udp_fini;
  fact->m_kind = NN_LOCATOR_KIND_UDPv4;
  fact->m_typename = "udp";
  fact->m_default_spdp_address = "udp/239.255.0.1";
  fact->m_connless = true;
  fact->m_supports_fn = ddsi_udp_supports;
  fact->m_create_conn_fn = ddsi_udp_create_conn;
  fact->m_release_conn_fn = ddsi_udp_release_conn;
  fact->m_join_mc_fn = ddsi_udp_join_mc;
  fact->m_leave_mc_fn = ddsi_udp_leave_mc;
  fact->m_is_mcaddr_fn = ddsi_udp_is_mcaddr;
#ifdef DDSI_INCLUDE_SSM
  fact->m_is_ssm_mcaddr_fn = ddsi_udp_is_ssm_mcaddr;
#endif
  fact->m_is_nearby_address_fn = ddsi_ipaddr_is_nearby_address;
  fact->m_locator_from_string_fn = ddsi_udp_address_from_string;
  fact->m_locator_to_string_fn = ddsi_udp_locator_to_string;
  fact->m_enumerate_interfaces_fn = ddsi_eth_enumerate_interfaces;
  fact->m_is_valid_port_fn = ddsi_udp_is_valid_port;
#if DDSRT_HAVE_IPV6
  if (gv->config.transport_selector == TRANS_UDP6)
  {
    fact->m_kind = NN_LOCATOR_KIND_UDPv6;
    fact->m_typename = "udp6";
    fact->m_default_spdp_address = "udp6/ff02::ffff:239.255.0.1";
  }
#endif

  ddsi_factory_add (gv, fact);
  GVLOG (DDS_LC_CONFIG, "udp initialized\n");
  return 0;
}
