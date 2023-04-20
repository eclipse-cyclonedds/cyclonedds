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
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsi/ddsi_log.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__eth.h"
#include "ddsi__tran.h"
#include "ddsi__udp.h"
#include "ddsi__ipaddr.h"
#include "ddsi__mcgroup.h"
#include "ddsi__pcap.h"

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

typedef struct ddsi_udp_tran_factory {
  struct ddsi_tran_factory fact;
  int32_t m_kind;

  // actual minimum receive buffer size in use
  // atomically loaded/stored so we don't have to lie about constness
  ddsrt_atomic_uint32_t receive_buf_size;
} *ddsi_udp_tran_factory_t;

static void addr_to_loc (const struct ddsi_tran_factory *tran, ddsi_locator_t *dst, const union addr *src)
{
  (void) tran;
  ddsi_ipaddr_to_loc (dst, &src->a, (src->a.sa_family == AF_INET) ? DDSI_LOCATOR_KIND_UDPv4 : DDSI_LOCATOR_KIND_UDPv6);
}

static ssize_t ddsi_udp_conn_read (struct ddsi_tran_conn * conn_cmn, unsigned char * buf, size_t len, bool allow_spurious, ddsi_locator_t *srcloc)
{
  ddsi_udp_conn_t conn = (ddsi_udp_conn_t) conn_cmn;
  struct ddsi_domaingv * const gv = conn->m_base.m_base.gv;
  union addr src;
  ddsrt_iovec_t msg_iov = {
    .iov_base = (void *) buf,
    .iov_len = (ddsrt_iov_len_t) len /* Windows uses unsigned, POSIX (except Linux) int */
  };
  ddsrt_msghdr_t msghdr = {
    .msg_name = &src.x,
    .msg_namelen = (socklen_t) sizeof (src),
    .msg_iov = &msg_iov,
    .msg_iovlen = 1
    // accrights/control implicitly initialised to 0
    // msg_flags is an out parameter anyway
  };
  (void) allow_spurious;

  dds_return_t rc;
  ssize_t nrecv = 0;
  do {
    rc = ddsrt_recvmsg (conn->m_sock, &msghdr, 0, &nrecv);
  } while (rc == DDS_RETCODE_INTERRUPTED);

  if (nrecv > 0)
  {
    if (srcloc)
      addr_to_loc (conn->m_base.m_factory, srcloc, &src);

    if (gv->pcap_fp)
    {
      union addr dest;
      socklen_t dest_len = sizeof (dest);
      if (ddsrt_getsockname (conn->m_sock, &dest.a, &dest_len) != DDS_RETCODE_OK)
        memset (&dest, 0, sizeof (dest));
      ddsi_write_pcap_received (gv, ddsrt_time_wallclock (), &src.x, &dest.x, buf, (size_t) nrecv);
    }

    /* Check for udp packet truncation */
#if DDSRT_MSGHDR_FLAGS
    const bool trunc_flag = (msghdr.msg_flags & MSG_TRUNC) != 0;
#else
    const bool trunc_flag = false;
#endif
    if ((size_t) nrecv > len || trunc_flag)
    {
      char addrbuf[DDSI_LOCSTRLEN];
      ddsi_locator_t tmp;
      addr_to_loc (conn->m_base.m_factory, &tmp, &src);
      ddsi_locator_to_string (addrbuf, sizeof (addrbuf), &tmp);
      GVWARNING ("%s => %d truncated to %d\n", addrbuf, (int) nrecv, (int) len);
    }
  }
  else if (rc != DDS_RETCODE_BAD_PARAMETER && rc != DDS_RETCODE_NO_CONNECTION)
  {
    GVERROR ("UDP recvmsg sock %d: ret %d retcode %"PRId32"\n", (int) conn->m_sock, (int) nrecv, rc);
    nrecv = -1;
  }
  return nrecv;
}

static ssize_t ddsi_udp_conn_write (struct ddsi_tran_conn * conn_cmn, const ddsi_locator_t *dst, size_t niov, const ddsrt_iovec_t *iov, uint32_t flags)
{
  ddsi_udp_conn_t conn = (ddsi_udp_conn_t) conn_cmn;
  struct ddsi_domaingv * const gv = conn->m_base.m_base.gv;
  dds_return_t rc;
  ssize_t nsent = -1;
  unsigned retry = 2;
  int sendflags = 0;
#if defined _WIN32 && !defined WINCE
  ddsrt_mtime_t timeout = DDSRT_MTIME_NEVER;
  ddsrt_mtime_t tnow = { 0 };
#endif
  union addr dstaddr;
  assert (niov <= INT_MAX);
  ddsi_ipaddr_from_loc (&dstaddr.x, dst);
  ddsrt_msghdr_t msg = {
    .msg_name = &dstaddr.x,
    .msg_namelen = (socklen_t) ddsrt_sockaddr_get_size (&dstaddr.a),
    .msg_iov = (ddsrt_iovec_t *) iov,
    .msg_iovlen = (ddsrt_msg_iovlen_t) niov
#if DDSRT_MSGHDR_FLAGS
    , .msg_flags = (int) flags
#endif
    // accrights/control implicitly initialised to 0
  };
  (void) flags; // in case ! DDSRT_MSGHDR_FLAGS

#if MSG_NOSIGNAL && !LWIP_SOCKET
  sendflags |= MSG_NOSIGNAL;
#endif
  rc = ddsrt_sendmsg (conn->m_sock, &msg, sendflags, &nsent);
  if (rc != DDS_RETCODE_OK)
  {
    // IIRC, NOT_ALLOWED is something that spuriously happens on some old versions of Linux i.c.w. firewalls
    // details never understood properly, but retrying a few times helped make it behave a bit better
    //
    // TRY_AGAIN should only occur on Windows because on Linux we can (and do) use blocking sockets.  It may
    // be that currently the socket is also blocking on Windows, because we currently separate create sockets
    // for transmitting data, but it would actually be preferable to go back to re-using the socket used for
    // receiving unicast traffic if there is only a single network interface (less resource usage, easier
    // for firewall configuration) and so it makes sense to keep the code.
    while (rc == DDS_RETCODE_INTERRUPTED || rc == DDS_RETCODE_TRY_AGAIN || (rc == DDS_RETCODE_NOT_ALLOWED && retry-- > 0))
    {
#if defined _WIN32 && !defined WINCE
      if (rc == DDS_RETCODE_TRY_AGAIN)
      {
        // I've once seen a case where a passing INFINITE could cause WaitForSingleObject to block
        // for hours on end on a functioning Ethernet.  That's definitely not what one would expect,
        // the best guess is that it was a hardware or driver issue.  Better to drop the datagram
        // after a little while, the upper layers allow for packet loss anyway.
        tnow = ddsrt_time_monotonic ();
        if (timeout.v == DDSRT_MTIME_NEVER.v)
          timeout = ddsrt_mtime_add_duration (tnow, DDS_MSECS (100));
        else if (tnow.v >= timeout.v)
          break;
        WSANETWORKEVENTS ev;
        WaitForSingleObject (conn->m_sockEvent, 5);
        WSAEnumNetworkEvents (conn->m_sock, conn->m_sockEvent, &ev);
      }
#endif
      rc = ddsrt_sendmsg (conn->m_sock, &msg, sendflags, &nsent);
    }
  }

  if (nsent > 0 && gv->pcap_fp)
  {
    union addr sa;
    socklen_t alen = sizeof (sa);
    if (ddsrt_getsockname (conn->m_sock, &sa.a, &alen) != DDS_RETCODE_OK)
      memset(&sa, 0, sizeof(sa));
    ddsi_write_pcap_sent (gv, ddsrt_time_wallclock (), &sa.x, &msg, (size_t) nsent);
  }
  else if (rc != DDS_RETCODE_OK && rc != DDS_RETCODE_NOT_ALLOWED && rc != DDS_RETCODE_NO_CONNECTION)
  {
    char locbuf[DDSI_LOCSTRLEN];
    GVERROR ("ddsi_udp_conn_write to %s failed with retcode %"PRId32"\n", ddsi_locator_to_string (locbuf, sizeof (locbuf), dst), rc);
  }
  return (rc == DDS_RETCODE_OK) ? nsent : -1;
}

static void ddsi_udp_disable_multiplexing (struct ddsi_tran_conn * conn_cmn)
{
#if defined _WIN32 && !defined WINCE
  ddsi_udp_conn_t conn = (ddsi_udp_conn_t) conn_cmn;
  uint32_t zero = 0;
  DWORD dummy;
  WSAEventSelect (conn->m_sock, 0, 0);
  WSAIoctl (conn->m_sock, FIONBIO, &zero,sizeof(zero), NULL,0, &dummy, NULL,NULL);
#else
  (void) conn_cmn;
#endif
}

static ddsrt_socket_t ddsi_udp_conn_handle (struct ddsi_tran_base * conn_cmn)
{
  ddsi_udp_conn_t conn = (ddsi_udp_conn_t) conn_cmn;
  return conn->m_sock;
}

static bool ddsi_udp_supports (const struct ddsi_tran_factory *fact_cmn, int32_t kind)
{
  struct ddsi_udp_tran_factory const * const fact = (const struct ddsi_udp_tran_factory *) fact_cmn;
  return kind == fact->m_kind || (kind == DDSI_LOCATOR_KIND_UDPv4MCGEN && fact->m_kind == DDSI_LOCATOR_KIND_UDPv4);
}

static int ddsi_udp_conn_locator (struct ddsi_tran_factory * fact_cmn, struct ddsi_tran_base * conn_cmn, ddsi_locator_t *loc)
{
  struct ddsi_udp_tran_factory const * const fact = (const struct ddsi_udp_tran_factory *) fact_cmn;
  ddsi_udp_conn_t conn = (ddsi_udp_conn_t) conn_cmn;
  int ret = -1;
  if (conn->m_sock != DDSRT_INVALID_SOCKET)
  {
    loc->kind = fact->m_kind;
    loc->port = conn->m_base.m_base.m_port;
    memcpy (loc->address, conn->m_base.m_base.gv->interfaces[0].loc.address, sizeof (loc->address));
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

static dds_return_t set_socket_buffer (struct ddsi_domaingv const * const gv, ddsrt_socket_t sock, int32_t socket_option, const char *socket_option_name, const char *name, const struct ddsi_config_socket_buf_size *config, uint32_t default_min_size)
{
  // if (min, max)=   and   initbuf=   then  request=  and  result=
  //    (def, def)          < defmin         defmin         whatever it is
  //    (def, N)            anything         N              whatever it is
  //    (M,   def)          < M              M              error if < M
  //    (M,   N<M)          < M              M              error if < M
  //    (M,  N>=M)          anything         N              error if < M
  // defmin = 1MB for receive buffer, 0B for send buffer
  const bool always_set_size = // whether to call setsockopt unconditionally
    ((config->min.isdefault && !config->max.isdefault) ||
     (!config->min.isdefault && !config->max.isdefault && config->max.value >= config->min.value));
  const uint32_t socket_min_buf_size = // error if it ends up below this
    !config->min.isdefault ? config->min.value : 0;
  const uint32_t socket_req_buf_size = // size to request
    (!config->max.isdefault && config->max.value > socket_min_buf_size) ? config->max.value
    : !config->min.isdefault ? config->min.value
    : default_min_size;

  uint32_t actsize;
  socklen_t optlen = (socklen_t) sizeof (actsize);
  dds_return_t rc;

  rc = ddsrt_getsockopt (sock, SOL_SOCKET, socket_option, &actsize, &optlen);
  if (rc == DDS_RETCODE_BAD_PARAMETER || rc == DDS_RETCODE_UNSUPPORTED)
  {
    /* not all stacks support getting/setting RCVBUF */
    GVLOG (DDS_LC_CONFIG, "cannot retrieve socket %s buffer size\n", name);
    return DDS_RETCODE_OK;
  }
  else if (rc != DDS_RETCODE_OK)
  {
    GVERROR ("ddsi_udp_create_conn: get %s failed: %s\n", socket_option_name, dds_strretcode (rc));
    return rc;
  }

  if (always_set_size || actsize < socket_req_buf_size)
  {
    (void) ddsrt_setsockopt (sock, SOL_SOCKET, socket_option, &socket_req_buf_size, sizeof (actsize));

    /* We don't check the return code from setsockopt, because some O/Ss tend
       to silently cap the buffer size.  The only way to make sure is to read
       the option value back and check it is now set correctly. */
    if ((rc = ddsrt_getsockopt (sock, SOL_SOCKET, socket_option, &actsize, &optlen)) != DDS_RETCODE_OK)
    {
      GVERROR ("ddsi_udp_create_conn: get %s failed: %s\n", socket_option_name, dds_strretcode (rc));
      return rc;
    }

    if (actsize >= socket_req_buf_size)
      GVLOG (DDS_LC_CONFIG, "socket %s buffer size set to %"PRIu32" bytes\n", name, actsize);
    else if (actsize >= socket_min_buf_size)
      GVLOG (DDS_LC_CONFIG,
             "failed to increase socket %s buffer size to %"PRIu32" bytes, continuing with %"PRIu32" bytes\n",
             name, socket_req_buf_size, actsize);
    else
    {
      /* If the configuration states it must be >= X, then error out if the
         kernel doesn't give us at least X */
      GVLOG (DDS_LC_CONFIG | DDS_LC_ERROR,
             "failed to increase socket %s buffer size to at least %"PRIu32" bytes, current is %"PRIu32" bytes\n",
             name, socket_min_buf_size, actsize);
      rc = DDS_RETCODE_NOT_ENOUGH_SPACE;
    }
  }

  return (rc < 0) ? rc : (actsize > (uint32_t) INT32_MAX) ? INT32_MAX : (int32_t) actsize;
}

static dds_return_t set_rcvbuf (struct ddsi_domaingv const * const gv, ddsrt_socket_t sock, const struct ddsi_config_socket_buf_size *config)
{
  return set_socket_buffer (gv, sock, SO_RCVBUF, "SO_RCVBUF", "receive", config, 1048576);
}

static dds_return_t set_sndbuf (struct ddsi_domaingv const * const gv, ddsrt_socket_t sock, const struct ddsi_config_socket_buf_size *config)
{
  return set_socket_buffer (gv, sock, SO_SNDBUF, "SO_SNDBUF", "send", config, 65536);
}

static dds_return_t set_mc_options_transmit_ipv6 (struct ddsi_domaingv const * const gv, struct ddsi_network_interface const * const intf, ddsrt_socket_t sock)
{
  /* Function is a never-called no-op if IPv6 is not supported to keep the call-site a bit cleaner  */
#if DDSRT_HAVE_IPV6
  const unsigned ifno = intf->if_index;
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
  (void) gv; (void) intf; (void) sock;
  return DDS_RETCODE_ERROR;
#endif
}

static dds_return_t set_mc_options_transmit_ipv4_if (struct ddsi_domaingv const * const gv, struct ddsi_network_interface const * const intf, ddsrt_socket_t sock)
{
#if (defined(__linux) || defined(__APPLE__)) && !LWIP_SOCKET
  if (gv->config.use_multicast_if_mreqn)
  {
    struct ip_mreqn mreqn;
    memset (&mreqn, 0, sizeof (mreqn));
    /* looks like imr_multiaddr is not relevant, not sure about imr_address */
    mreqn.imr_multiaddr.s_addr = htonl (INADDR_ANY);
    if (gv->config.use_multicast_if_mreqn > 1)
      memcpy (&mreqn.imr_address.s_addr, intf->loc.address + 12, 4);
    else
      mreqn.imr_address.s_addr = htonl (INADDR_ANY);
    mreqn.imr_ifindex = (int) intf->if_index;
    return ddsrt_setsockopt (sock, IPPROTO_IP, IP_MULTICAST_IF, &mreqn, sizeof (mreqn));
  }
#else
  (void) gv;
#endif
  return ddsrt_setsockopt (sock, IPPROTO_IP, IP_MULTICAST_IF, intf->loc.address + 12, 4);
}

static dds_return_t set_mc_options_transmit_ipv4 (struct ddsi_domaingv const * const gv, struct ddsi_network_interface const * const intf, ddsrt_socket_t sock)
{
  const unsigned char ttl = (unsigned char) gv->config.multicast_ttl;
  const unsigned char loop = (unsigned char) !!gv->config.enableMulticastLoopback;
  dds_return_t rc;
  if ((rc = set_mc_options_transmit_ipv4_if (gv, intf, sock)) != DDS_RETCODE_OK) {
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

static dds_return_t ddsi_udp_create_conn (struct ddsi_tran_conn **conn_out, struct ddsi_tran_factory * fact_cmn, uint32_t port, const struct ddsi_tran_qos *qos)
{
  struct ddsi_udp_tran_factory *fact = (struct ddsi_udp_tran_factory *) fact_cmn;
  struct ddsi_domaingv const * const gv = fact->fact.gv;
  struct ddsi_network_interface const * const intf = qos->m_interface ? qos->m_interface : &gv->interfaces[0];

  dds_return_t rc;
  ddsrt_socket_t sock;
  bool reuse_addr = false, bind_to_any = false, ipv6 = false, set_mc_xmit_options = false;
  const char *purpose_str = NULL;

  switch (qos->m_purpose)
  {
    case DDSI_TRAN_QOS_XMIT_UC:
      reuse_addr = false;
      bind_to_any = false;
      set_mc_xmit_options = false;
      purpose_str = "transmit(uc)";
      break;
    case DDSI_TRAN_QOS_XMIT_MC:
      reuse_addr = false;
      bind_to_any = false;
      set_mc_xmit_options = true;
      purpose_str = "transmit(uc/mc)";
      break;
    case DDSI_TRAN_QOS_RECV_UC:
      reuse_addr = false;
      bind_to_any = true;
      set_mc_xmit_options = false;
      purpose_str = "unicast";
      break;
    case DDSI_TRAN_QOS_RECV_MC:
      reuse_addr = true;
      bind_to_any = true;
      set_mc_xmit_options = false;
      purpose_str = "multicast";
      break;
  }
  assert (purpose_str != NULL);

  union addr socketname;
  ddsi_locator_t ownloc_w_port = intf->loc;
  assert (ownloc_w_port.port == DDSI_LOCATOR_PORT_INVALID);
  if (port) {
    /* PORT_INVALID maps to 0 in ipaddr_from_loc */
    ownloc_w_port.port = port;
  }
  ddsi_ipaddr_from_loc (&socketname.x, &ownloc_w_port);
  switch (fact->m_kind)
  {
    case DDSI_LOCATOR_KIND_UDPv4:
      if (bind_to_any)
        socketname.a4.sin_addr.s_addr = htonl (INADDR_ANY);
      break;
#if DDSRT_HAVE_IPV6
    case DDSI_LOCATOR_KIND_UDPv6:
      ipv6 = true;
      if (bind_to_any)
        socketname.a6.sin6_addr = ddsrt_in6addr_any;
      else if (intf->link_local)
        socketname.a6.sin6_scope_id = intf->if_index;
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

  if (reuse_addr && (rc = ddsrt_setsockreuse (sock, true)) != DDS_RETCODE_OK) {
    if (rc != DDS_RETCODE_UNSUPPORTED) {
      GVERROR ("ddsi_udp_create_conn: failed to enable port reuse: %s\n", dds_strretcode(rc));
      goto fail_w_socket;
    } else {
      // If the network stack doesn't support it, do make it fairly easy to find out,
      // but don't always print it to stderr because it would likely be more annoying
      // than helpful.
      GVLOG (DDS_LC_CONFIG, "ddsi_udp_create_conn: port reuse not supported by network stack\n");
    }
  }

  if ((rc = set_rcvbuf (gv, sock, &gv->config.socket_rcvbuf_size)) < 0)
    goto fail_w_socket;
  if (rc > 0) {
    // set fact->receive_buf_size to the smallest observed value
    uint32_t old;
    do {
      old = ddsrt_atomic_ld32 (&fact->receive_buf_size);
      if ((uint32_t) rc >= old)
        break;
    } while (!ddsrt_atomic_cas32 (&fact->receive_buf_size, old, (uint32_t) rc));
  }

  if (set_sndbuf (gv, sock, &gv->config.socket_sndbuf_size) < 0)
    goto fail_w_socket;
  if (gv->config.dontRoute && set_dont_route (gv, sock, ipv6) != DDS_RETCODE_OK)
    goto fail_w_socket;

  if ((rc = ddsrt_bind (sock, &socketname.a, ddsrt_sockaddr_get_size (&socketname.a))) != DDS_RETCODE_OK)
  {
    /* PRECONDITION_NOT_MET (= EADDRINUSE) is expected if reuse_addr isn't set, should be handled at
       a higher level and therefore needs to return a specific error message */
    if (!reuse_addr && rc == DDS_RETCODE_PRECONDITION_NOT_MET)
      goto fail_addrinuse;

    char buf[DDSI_LOCSTRLEN];
    if (bind_to_any)
      snprintf (buf, sizeof (buf), "ANY:%"PRIu32, port);
    else
      ddsi_locator_to_string (buf, sizeof (buf), &ownloc_w_port);
    GVERROR ("ddsi_udp_create_conn: failed to bind to %s: %s\n", buf,
             (rc == DDS_RETCODE_PRECONDITION_NOT_MET) ? "address in use" : dds_strretcode (rc));
    goto fail_w_socket;
  }

  if (set_mc_xmit_options)
  {
    rc = ipv6 ? set_mc_options_transmit_ipv6 (gv, intf, sock) : set_mc_options_transmit_ipv4 (gv, intf, sock);
    if (rc != DDS_RETCODE_OK)
      goto fail_w_socket;
  }

  ddsi_udp_conn_t conn = ddsrt_malloc (sizeof (*conn));
  memset (conn, 0, sizeof (*conn));

  conn->m_sock = sock;
  conn->m_diffserv = qos->m_diffserv;
#if defined _WIN32 && !defined WINCE
  conn->m_sockEvent = WSACreateEvent ();
  WSAEventSelect (conn->m_sock, conn->m_sockEvent, FD_WRITE);
#endif

  ddsi_factory_conn_init (&fact->fact, intf, &conn->m_base);
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

static int joinleave_asm_mcgroup (ddsrt_socket_t socket, int join, const ddsi_locator_t *mcloc, const struct ddsi_network_interface *interf)
{
  dds_return_t rc;
  union addr mcip;
  ddsi_ipaddr_from_loc (&mcip.x, mcloc);
#if DDSRT_HAVE_IPV6
  if (mcloc->kind == DDSI_LOCATOR_KIND_UDPv6)
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

#ifdef DDS_HAS_SSM
static int joinleave_ssm_mcgroup (ddsrt_socket_t socket, int join, const ddsi_locator_t *srcloc, const ddsi_locator_t *mcloc, const struct ddsi_network_interface *interf)
{
  dds_return_t rc;
  union addr mcip, srcip;
  ddsi_ipaddr_from_loc (&mcip.x, mcloc);
  ddsi_ipaddr_from_loc (&srcip.x, srcloc);
#if DDSRT_HAVE_IPV6
  if (mcloc->kind == DDSI_LOCATOR_KIND_UDPv6)
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

static int ddsi_udp_join_mc (struct ddsi_tran_conn * conn_cmn, const ddsi_locator_t *srcloc, const ddsi_locator_t *mcloc, const struct ddsi_network_interface *interf)
{
  ddsi_udp_conn_t conn = (ddsi_udp_conn_t) conn_cmn;
  (void) srcloc;
#ifdef DDS_HAS_SSM
  if (srcloc)
    return joinleave_ssm_mcgroup (conn->m_sock, 1, srcloc, mcloc, interf);
  else
#endif
    return joinleave_asm_mcgroup (conn->m_sock, 1, mcloc, interf);
}

static int ddsi_udp_leave_mc (struct ddsi_tran_conn * conn_cmn, const ddsi_locator_t *srcloc, const ddsi_locator_t *mcloc, const struct ddsi_network_interface *interf)
{
  ddsi_udp_conn_t conn = (ddsi_udp_conn_t) conn_cmn;
  (void) srcloc;
#ifdef DDS_HAS_SSM
  if (srcloc)
    return joinleave_ssm_mcgroup (conn->m_sock, 0, srcloc, mcloc, interf);
  else
#endif
    return joinleave_asm_mcgroup (conn->m_sock, 0, mcloc, interf);
}

static void ddsi_udp_release_conn (struct ddsi_tran_conn * conn_cmn)
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

static int ddsi_udp_is_loopbackaddr (const struct ddsi_tran_factory *tran, const ddsi_locator_t *loc)
{
  (void) tran;
  switch (loc->kind)
  {
    case DDSI_LOCATOR_KIND_UDPv4: {
      return loc->address[12] == 127;
    }
    case DDSI_LOCATOR_KIND_UDPv4MCGEN: {
      return false;
    }
#if DDSRT_HAVE_IPV6
    case DDSI_LOCATOR_KIND_UDPv6: {
      const struct in6_addr *ipv6 = (const struct in6_addr *) loc->address;
      return IN6_IS_ADDR_LOOPBACK (ipv6);
    }
#endif
    default: {
      return 0;
    }
  }
}

static int ddsi_udp_is_mcaddr (const struct ddsi_tran_factory *tran, const ddsi_locator_t *loc)
{
  (void) tran;
  switch (loc->kind)
  {
    case DDSI_LOCATOR_KIND_UDPv4: {
      const struct in_addr *ipv4 = (const struct in_addr *) (loc->address + 12);
      DDSRT_WARNING_GNUC_OFF(sign-conversion)
      return IN_MULTICAST (ntohl (ipv4->s_addr));
      DDSRT_WARNING_GNUC_ON(sign-conversion)
    }
    case DDSI_LOCATOR_KIND_UDPv4MCGEN: {
      const ddsi_udpv4mcgen_address_t *mcgen = (const ddsi_udpv4mcgen_address_t *) loc->address;
      DDSRT_WARNING_GNUC_OFF(sign-conversion)
      return IN_MULTICAST (ntohl (mcgen->ipv4.s_addr));
      DDSRT_WARNING_GNUC_ON(sign-conversion)
    }
#if DDSRT_HAVE_IPV6
    case DDSI_LOCATOR_KIND_UDPv6: {
      const struct in6_addr *ipv6 = (const struct in6_addr *) loc->address;
      return IN6_IS_ADDR_MULTICAST (ipv6);
    }
#endif
    default: {
      return 0;
    }
  }
}

#ifdef DDS_HAS_SSM
static int ddsi_udp_is_ssm_mcaddr (const struct ddsi_tran_factory *tran, const ddsi_locator_t *loc)
{
  (void) tran;
  switch (loc->kind)
  {
    case DDSI_LOCATOR_KIND_UDPv4: {
      const struct in_addr *x = (const struct in_addr *) (loc->address + 12);
      return (((uint32_t) ntohl (x->s_addr)) >> 24) == 232;
    }
#if DDSRT_HAVE_IPV6
    case DDSI_LOCATOR_KIND_UDPv6: {
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

static enum ddsi_locator_from_string_result mcgen_address_from_string (const struct ddsi_tran_factory *tran_cmn, ddsi_locator_t *loc, const char *str)
{
  // check for UDPv4MCGEN string, be lazy and refuse to recognize as a MCGEN form if there's anything "wrong" with it
  struct ddsi_udp_tran_factory const * const tran = (const struct ddsi_udp_tran_factory *) tran_cmn;
  DDSRT_WARNING_MSVC_OFF(4996);
  char ipstr[280];
  unsigned base, count, idx;
  int ipstrlen, pos;
  if (strlen (str) + 10 >= sizeof (ipstr)) // + 6 for appending a port
    return AFSR_INVALID;
  else if (sscanf (str, "%255[^;]%n;%u;%u;%u%n", ipstr, &ipstrlen, &base, &count, &idx, &pos) != 4)
    return AFSR_INVALID;
  else if (str[pos] != 0 && str[pos] != ':')
    return AFSR_INVALID;
  else if (!(count > 0 && base < 28 && count < 28 && base + count < 28 && idx < count))
    return AFSR_INVALID;
  if (str[pos] == ':')
  {
    unsigned port;
    int pos2;
    if (sscanf (str + pos, ":%u%n", &port, &pos2) != 1 || str[pos + pos2] != 0)
      return AFSR_INVALID;
    // append port to IP component so that ddsi_ipaddr_from_string can do all of the work
    // except for filling the specials
    assert (ipstrlen >= 0 && (size_t) ipstrlen < sizeof (ipstr));
    assert (pos2 >= 0 && (size_t) pos2 < sizeof (ipstr) - (size_t) ipstrlen);
    ddsrt_strlcpy (ipstr + ipstrlen, str + pos, sizeof (ipstr) - (size_t) ipstrlen);
  }

  enum ddsi_locator_from_string_result res = ddsi_ipaddr_from_string (loc, ipstr, tran->m_kind);
  if (res != AFSR_OK)
    return res;
  assert (loc->kind == DDSI_LOCATOR_KIND_UDPv4);
  if (!ddsi_udp_is_mcaddr (tran_cmn, loc))
    return AFSR_INVALID;

  ddsi_udpv4mcgen_address_t x;
  DDSRT_STATIC_ASSERT (sizeof (x) <= sizeof (loc->address));
  memset (&x, 0, sizeof(x));
  memcpy (&x.ipv4, loc->address + 12, 4);
  x.base = (unsigned char) base;
  x.count = (unsigned char) count;
  x.idx = (unsigned char) idx;
  memset (loc->address, 0, sizeof (loc->address));
  memcpy (loc->address, &x, sizeof (x));
  loc->kind = DDSI_LOCATOR_KIND_UDPv4MCGEN;
  return AFSR_OK;
  DDSRT_WARNING_MSVC_ON(4996);
}

static enum ddsi_locator_from_string_result ddsi_udp_address_from_string (const struct ddsi_tran_factory *tran_cmn, ddsi_locator_t *loc, const char *str)
{
  struct ddsi_udp_tran_factory const * const tran = (const struct ddsi_udp_tran_factory *) tran_cmn;
  if (tran->m_kind == DDSI_TRANS_UDP && mcgen_address_from_string (tran_cmn, loc, str) == AFSR_OK)
    return AFSR_OK;
  else
    return ddsi_ipaddr_from_string (loc, str, tran->m_kind);
}

static char *ddsi_udp_locator_to_string (char *dst, size_t sizeof_dst, const ddsi_locator_t *loc, struct ddsi_tran_conn * conn, int with_port)
{
  if (loc->kind != DDSI_LOCATOR_KIND_UDPv4MCGEN) {
    return ddsi_ipaddr_to_string(dst, sizeof_dst, loc, with_port, conn ? conn->m_interf : NULL);
  } else {
    struct sockaddr_in src;
    ddsi_udpv4mcgen_address_t mcgen;
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

static void ddsi_udp_fini (struct ddsi_tran_factory * fact_cmn)
{
  struct ddsi_udp_tran_factory *fact = (struct ddsi_udp_tran_factory *) fact_cmn;
  struct ddsi_domaingv const * const gv = fact->fact.gv;
  GVLOG (DDS_LC_CONFIG, "udp finalized\n");
  ddsrt_free (fact);
}

static int ddsi_udp_is_valid_port (const struct ddsi_tran_factory *fact, uint32_t port)
{
  (void) fact;
  return (port <= 65535);
}

static uint32_t ddsi_udp_receive_buffer_size (const struct ddsi_tran_factory *fact_cmn)
{
  const struct ddsi_udp_tran_factory *fact = (const struct ddsi_udp_tran_factory *) fact_cmn;
  return ddsrt_atomic_ld32 (&fact->receive_buf_size);
}

static int ddsi_udp_locator_from_sockaddr (const struct ddsi_tran_factory *tran_cmn, ddsi_locator_t *loc, const struct sockaddr *sockaddr)
{
  struct ddsi_udp_tran_factory const * const tran = (const struct ddsi_udp_tran_factory *) tran_cmn;
  switch (sockaddr->sa_family)
  {
    case AF_INET:
      if (tran->m_kind != DDSI_LOCATOR_KIND_UDPv4)
        return -1;
      break;
    case AF_INET6:
      if (tran->m_kind != DDSI_LOCATOR_KIND_UDPv6)
        return -1;
      break;
  }
  ddsi_ipaddr_to_loc (loc, sockaddr, tran->m_kind);
  return 0;
}

int ddsi_udp_init (struct ddsi_domaingv*gv)
{
  struct ddsi_udp_tran_factory *fact = ddsrt_malloc (sizeof (*fact));
  memset (fact, 0, sizeof (*fact));
  fact->m_kind = DDSI_LOCATOR_KIND_UDPv4;
  fact->fact.gv = gv;
  fact->fact.m_free_fn = ddsi_udp_fini;
  fact->fact.m_typename = "udp";
  fact->fact.m_default_spdp_address = "udp/239.255.0.1";
  fact->fact.m_connless = true;
  fact->fact.m_enable_spdp = true;
  fact->fact.m_supports_fn = ddsi_udp_supports;
  fact->fact.m_create_conn_fn = ddsi_udp_create_conn;
  fact->fact.m_release_conn_fn = ddsi_udp_release_conn;
  fact->fact.m_join_mc_fn = ddsi_udp_join_mc;
  fact->fact.m_leave_mc_fn = ddsi_udp_leave_mc;
  fact->fact.m_is_loopbackaddr_fn = ddsi_udp_is_loopbackaddr;
  fact->fact.m_is_mcaddr_fn = ddsi_udp_is_mcaddr;
#ifdef DDS_HAS_SSM
  fact->fact.m_is_ssm_mcaddr_fn = ddsi_udp_is_ssm_mcaddr;
#endif
  fact->fact.m_is_nearby_address_fn = ddsi_ipaddr_is_nearby_address;
  fact->fact.m_locator_from_string_fn = ddsi_udp_address_from_string;
  fact->fact.m_locator_to_string_fn = ddsi_udp_locator_to_string;
  fact->fact.m_enumerate_interfaces_fn = ddsi_eth_enumerate_interfaces;
  fact->fact.m_is_valid_port_fn = ddsi_udp_is_valid_port;
  fact->fact.m_receive_buffer_size_fn = ddsi_udp_receive_buffer_size;
  fact->fact.m_locator_from_sockaddr_fn = ddsi_udp_locator_from_sockaddr;
#if DDSRT_HAVE_IPV6
  if (gv->config.transport_selector == DDSI_TRANS_UDP6)
  {
    fact->m_kind = DDSI_LOCATOR_KIND_UDPv6;
    fact->fact.m_typename = "udp6";
    fact->fact.m_default_spdp_address = "udp6/ff02::ffff:239.255.0.1";
  }
#endif
  ddsrt_atomic_st32 (&fact->receive_buf_size, UINT32_MAX);

  ddsi_factory_add (gv, &fact->fact);
  GVLOG (DDS_LC_CONFIG, "udp initialized\n");
  return 0;
}
