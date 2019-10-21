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
#include "cyclonedds/ddsrt/atomics.h"
#include "cyclonedds/ddsrt/heap.h"
#include "cyclonedds/ddsrt/log.h"
#include "cyclonedds/ddsrt/misc.h"
#include "cyclonedds/ddsrt/sockets.h"
#include "ddsi_eth.h"
#include "cyclonedds/ddsi/ddsi_tran.h"
#include "cyclonedds/ddsi/ddsi_udp.h"
#include "cyclonedds/ddsi/ddsi_ipaddr.h"
#include "cyclonedds/ddsi/ddsi_mcgroup.h"
#include "cyclonedds/ddsi/q_nwif.h"
#include "cyclonedds/ddsi/q_config.h"
#include "cyclonedds/ddsi/q_log.h"
#include "cyclonedds/ddsi/q_pcap.h"
#include "cyclonedds/ddsi/q_globals.h"

typedef struct ddsi_udp_conn {
  struct ddsi_tran_conn m_base;
  ddsrt_socket_t m_sock;
#if defined _WIN32
  WSAEVENT m_sockEvent;
#endif
  int m_diffserv;
} *ddsi_udp_conn_t;

static ssize_t ddsi_udp_conn_read (ddsi_tran_conn_t conn, unsigned char * buf, size_t len, bool allow_spurious, nn_locator_t *srcloc)
{
  dds_return_t rc;
  ssize_t ret = 0;
  ddsrt_msghdr_t msghdr;
  struct sockaddr_storage src;
  ddsrt_iovec_t msg_iov;
  socklen_t srclen = (socklen_t) sizeof (src);
  (void) allow_spurious;

  msg_iov.iov_base = (void*) buf;
  msg_iov.iov_len = (ddsrt_iov_len_t)len; /* Windows uses unsigned, POSIX (except Linux) int */

  msghdr.msg_name = &src;
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
    rc = ddsrt_recvmsg(((ddsi_udp_conn_t) conn)->m_sock, &msghdr, 0, &ret);
  } while (rc == DDS_RETCODE_INTERRUPTED);

  if (ret > 0)
  {
    if (srcloc)
      ddsi_ipaddr_to_loc(srcloc, (struct sockaddr *)&src, src.ss_family == AF_INET ? NN_LOCATOR_KIND_UDPv4 : NN_LOCATOR_KIND_UDPv6);

    if(conn->m_base.gv->pcap_fp)
    {
      struct sockaddr_storage dest;
      socklen_t dest_len = sizeof (dest);
      if (ddsrt_getsockname (((ddsi_udp_conn_t) conn)->m_sock, (struct sockaddr *) &dest, &dest_len) != DDS_RETCODE_OK)
        memset(&dest, 0, sizeof(dest));
      write_pcap_received(conn->m_base.gv, now(), &src, &dest, buf, (size_t) ret);
    }

    /* Check for udp packet truncation */
    if ((((size_t) ret) > len)
#if DDSRT_MSGHDR_FLAGS
        || (msghdr.msg_flags & MSG_TRUNC)
#endif
        )
    {
      char addrbuf[DDSI_LOCSTRLEN];
      nn_locator_t tmp;
      ddsi_ipaddr_to_loc(&tmp, (struct sockaddr *)&src, src.ss_family == AF_INET ? NN_LOCATOR_KIND_UDPv4 : NN_LOCATOR_KIND_UDPv6);
      ddsi_locator_to_string(conn->m_base.gv, addrbuf, sizeof(addrbuf), &tmp);
      DDS_CWARNING(&conn->m_base.gv->logconfig, "%s => %d truncated to %d\n", addrbuf, (int)ret, (int)len);
    }
  }
  else if (rc != DDS_RETCODE_BAD_PARAMETER &&
           rc != DDS_RETCODE_NO_CONNECTION)
  {
    DDS_CERROR(&conn->m_base.gv->logconfig, "UDP recvmsg sock %d: ret %d retcode %"PRId32"\n", (int) ((ddsi_udp_conn_t) conn)->m_sock, (int) ret, rc);
    ret = -1;
  }
  return ret;
}

static void set_msghdr_iov (ddsrt_msghdr_t *mhdr, ddsrt_iovec_t *iov, size_t iovlen)
{
  mhdr->msg_iov = iov;
  mhdr->msg_iovlen = (ddsrt_msg_iovlen_t)iovlen;
}

static ssize_t ddsi_udp_conn_write (ddsi_tran_conn_t conn, const nn_locator_t *dst, size_t niov, const ddsrt_iovec_t *iov, uint32_t flags)
{
  dds_return_t rc;
  ssize_t ret = -1;
  unsigned retry = 2;
  int sendflags = 0;
  ddsrt_msghdr_t msg;
  struct sockaddr_storage dstaddr;
  assert(niov <= INT_MAX);
  ddsi_ipaddr_from_loc(&dstaddr, dst);
  set_msghdr_iov (&msg, (ddsrt_iovec_t *) iov, niov);
  msg.msg_name = &dstaddr;
  msg.msg_namelen = (socklen_t) ddsrt_sockaddr_get_size((struct sockaddr *) &dstaddr);
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
  DDSRT_UNUSED_ARG(flags);
#endif
#if MSG_NOSIGNAL && !LWIP_SOCKET
  sendflags |= MSG_NOSIGNAL;
#endif
  do {
    ddsi_udp_conn_t uc = (ddsi_udp_conn_t) conn;
    rc = ddsrt_sendmsg (uc->m_sock, &msg, sendflags, &ret);
#if defined _WIN32 && !defined WINCE
    if (rc == DDS_RETCODE_TRY_AGAIN) {
      WSANETWORKEVENTS ev;
      WaitForSingleObject(uc->m_sockEvent, INFINITE);
      WSAEnumNetworkEvents(uc->m_sock, uc->m_sockEvent, &ev);
    }
#endif
  } while ((rc == DDS_RETCODE_INTERRUPTED) ||
           (rc == DDS_RETCODE_TRY_AGAIN) ||
           (rc == DDS_RETCODE_NOT_ALLOWED && retry-- > 0));
  if (ret > 0 && conn->m_base.gv->pcap_fp)
  {
    struct sockaddr_storage sa;
    socklen_t alen = sizeof (sa);
    if (ddsrt_getsockname (((ddsi_udp_conn_t) conn)->m_sock, (struct sockaddr *) &sa, &alen) != DDS_RETCODE_OK)
      memset(&sa, 0, sizeof(sa));
    write_pcap_sent (conn->m_base.gv, now (), &sa, &msg, (size_t) ret);
  }
  else if (rc != DDS_RETCODE_OK &&
           rc != DDS_RETCODE_NOT_ALLOWED &&
           rc != DDS_RETCODE_NO_CONNECTION)
  {
    DDS_CERROR(&conn->m_base.gv->logconfig, "ddsi_udp_conn_write failed with retcode %"PRId32"\n", rc);
  }
  return (rc == DDS_RETCODE_OK ? ret : -1);
}

static void ddsi_udp_disable_multiplexing (ddsi_tran_conn_t base)
{
#if defined _WIN32 && !defined WINCE
  ddsi_udp_conn_t uc = (ddsi_udp_conn_t) base;
  uint32_t zero = 0, dummy;
  WSAEventSelect(uc->m_sock, 0, 0);
  WSAIoctl(uc->m_sock, FIONBIO, &zero,sizeof(zero), NULL,0, &dummy, NULL,NULL);
#else
  (void)base;
#endif
}

static ddsrt_socket_t ddsi_udp_conn_handle (ddsi_tran_base_t base)
{
  return ((ddsi_udp_conn_t) base)->m_sock;
}

static bool ddsi_udp_supports (const struct ddsi_tran_factory *fact, int32_t kind)
{
  return kind == fact->m_kind || (kind == NN_LOCATOR_KIND_UDPv4MCGEN && fact->m_kind == NN_LOCATOR_KIND_UDPv4);
}

static int ddsi_udp_conn_locator (ddsi_tran_factory_t fact, ddsi_tran_base_t base, nn_locator_t *loc)
{
  int ret = -1;
  ddsi_udp_conn_t uc = (ddsi_udp_conn_t) base;
  if (uc->m_sock != DDSRT_INVALID_SOCKET)
  {
    loc->kind = fact->m_kind;
    loc->port = uc->m_base.m_base.m_port;
    memcpy(loc->address, uc->m_base.m_base.gv->extloc.address, sizeof (loc->address));
    ret = 0;
  }
  return ret;
}

static unsigned short get_socket_port (const struct ddsrt_log_cfg *logcfg, ddsrt_socket_t socket)
{
  dds_return_t ret;
  struct sockaddr_storage addr;
  socklen_t addrlen = sizeof (addr);

  ret = ddsrt_getsockname (socket, (struct sockaddr *)&addr, &addrlen);
  if (ret != DDS_RETCODE_OK)
  {
    DDS_CERROR (logcfg, "ddsi_udp_get_socket_port: getsockname returned %"PRId32"\n", ret);
    return 0;
  }

  return ddsrt_sockaddr_get_port((struct sockaddr *)&addr);
}

static ddsi_tran_conn_t ddsi_udp_create_conn (ddsi_tran_factory_t fact, uint32_t port, ddsi_tran_qos_t qos)
{
  int ret;
  ddsrt_socket_t sock;
  ddsi_udp_conn_t uc = NULL;
  bool mcast = (bool) (qos ? qos->m_multicast : false);

  /* If port is zero, need to create dynamic port */

  ret = make_socket (&sock, (unsigned short) port, false, mcast, fact->gv);

  if (ret == 0)
  {
    uc = (ddsi_udp_conn_t) ddsrt_malloc (sizeof (*uc));
    memset (uc, 0, sizeof (*uc));

    uc->m_sock = sock;
    uc->m_diffserv = qos ? qos->m_diffserv : 0;
#if defined _WIN32 && !defined WINCE
    uc->m_sockEvent = WSACreateEvent();
    WSAEventSelect(uc->m_sock, uc->m_sockEvent, FD_WRITE);
#endif

    ddsi_factory_conn_init (fact, &uc->m_base);
    uc->m_base.m_base.m_port = get_socket_port (&fact->gv->logconfig, sock);
    uc->m_base.m_base.m_trantype = DDSI_TRAN_CONN;
    uc->m_base.m_base.m_multicast = mcast;
    uc->m_base.m_base.m_handle_fn = ddsi_udp_conn_handle;

    uc->m_base.m_read_fn = ddsi_udp_conn_read;
    uc->m_base.m_write_fn = ddsi_udp_conn_write;
    uc->m_base.m_disable_multiplexing_fn = ddsi_udp_disable_multiplexing;
    uc->m_base.m_locator_fn = ddsi_udp_conn_locator;

    DDS_CTRACE (&fact->gv->logconfig,
                "ddsi_udp_create_conn %s socket %"PRIdSOCK" port %"PRIu32"\n",
                mcast ? "multicast" : "unicast",
                uc->m_sock,
                uc->m_base.m_base.m_port);
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
    if ((uc->m_diffserv != 0) && (fact->m_kind == NN_LOCATOR_KIND_UDPv4))
    {
      dds_return_t rc;
      rc = ddsrt_setsockopt(sock, IPPROTO_IP, IP_TOS, (char *)&uc->m_diffserv, sizeof(uc->m_diffserv));
      if (rc != DDS_RETCODE_OK)
        DDS_CERROR (fact->gv->logconfig, "ddsi_udp_create_conn: set diffserv retcode %"PRId32"\n", rc);
    }
#endif
  }
  else
  {
    if (fact->gv->config.participantIndex != PARTICIPANT_INDEX_AUTO)
    {
      DDS_CERROR (&fact->gv->logconfig, "UDP make_socket failed for %s port %"PRIu32"\n", mcast ? "multicast" : "unicast", port);
    }
  }

  return uc ? &uc->m_base : NULL;
}

static int joinleave_asm_mcgroup (ddsrt_socket_t socket, int join, const nn_locator_t *mcloc, const struct nn_interface *interf)
{
  dds_return_t rc;
  struct sockaddr_storage mcip;
  ddsi_ipaddr_from_loc(&mcip, mcloc);
#if DDSRT_HAVE_IPV6
  if (mcloc->kind == NN_LOCATOR_KIND_UDPv6)
  {
    struct ipv6_mreq ipv6mreq;
    memset (&ipv6mreq, 0, sizeof (ipv6mreq));
    memcpy (&ipv6mreq.ipv6mr_multiaddr, &((struct sockaddr_in6 *) &mcip)->sin6_addr, sizeof (ipv6mreq.ipv6mr_multiaddr));
    ipv6mreq.ipv6mr_interface = interf ? interf->if_index : 0;
    rc = ddsrt_setsockopt (socket, IPPROTO_IPV6, join ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP, &ipv6mreq, sizeof (ipv6mreq));
  }
  else
#endif
  {
    struct ip_mreq mreq;
    mreq.imr_multiaddr = ((struct sockaddr_in *) &mcip)->sin_addr;
    if (interf)
      memcpy (&mreq.imr_interface, interf->loc.address + 12, sizeof (mreq.imr_interface));
    else
      mreq.imr_interface.s_addr = htonl (INADDR_ANY);
    rc = ddsrt_setsockopt (socket, IPPROTO_IP, join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP, (char *) &mreq, sizeof (mreq));
  }
  return (rc == DDS_RETCODE_OK) ? 0 : -1;
}

#ifdef DDSI_INCLUDE_SSM
static int joinleave_ssm_mcgroup (ddsrt_socket_t socket, int join, const nn_locator_t *srcloc, const nn_locator_t *mcloc, const struct nn_interface *interf)
{
  dds_return_t rc;
  struct sockaddr_storage mcip, srcip;
  ddsi_ipaddr_from_loc(&mcip, mcloc);
  ddsi_ipaddr_from_loc(&srcip, srcloc);
#if DDSRT_HAVE_IPV6
  if (mcloc->kind == NN_LOCATOR_KIND_UDPv6)
  {
    struct group_source_req gsr;
    memset (&gsr, 0, sizeof (gsr));
    gsr.gsr_interface = interf ? interf->if_index : 0;
    memcpy (&gsr.gsr_group, &mcip, sizeof (gsr.gsr_group));
    memcpy (&gsr.gsr_source, &srcip, sizeof (gsr.gsr_source));
    rc = ddsrt_setsockopt (socket, IPPROTO_IPV6, join ? MCAST_JOIN_SOURCE_GROUP : MCAST_LEAVE_SOURCE_GROUP, &gsr, sizeof (gsr));
  }
  else
#endif
  {
    struct ip_mreq_source mreq;
    memset (&mreq, 0, sizeof (mreq));
    mreq.imr_sourceaddr = ((struct sockaddr_in *) &srcip)->sin_addr;
    mreq.imr_multiaddr = ((struct sockaddr_in *) &mcip)->sin_addr;
    if (interf)
      memcpy (&mreq.imr_interface, interf->loc.address + 12, sizeof (mreq.imr_interface));
    else
      mreq.imr_interface.s_addr = INADDR_ANY;
    rc = ddsrt_setsockopt (socket, IPPROTO_IP, join ? IP_ADD_SOURCE_MEMBERSHIP : IP_DROP_SOURCE_MEMBERSHIP, &mreq, sizeof (mreq));
  }
  return (rc == DDS_RETCODE_OK) ? 0 : -1;
}
#endif

static int ddsi_udp_join_mc (ddsi_tran_conn_t conn, const nn_locator_t *srcloc, const nn_locator_t *mcloc, const struct nn_interface *interf)
{
  ddsi_udp_conn_t uc = (ddsi_udp_conn_t) conn;
  (void)srcloc;
#ifdef DDSI_INCLUDE_SSM
  if (srcloc)
    return joinleave_ssm_mcgroup(uc->m_sock, 1, srcloc, mcloc, interf);
  else
#endif
    return joinleave_asm_mcgroup(uc->m_sock, 1, mcloc, interf);
}

static int ddsi_udp_leave_mc (ddsi_tran_conn_t conn, const nn_locator_t *srcloc, const nn_locator_t *mcloc, const struct nn_interface *interf)
{
  ddsi_udp_conn_t uc = (ddsi_udp_conn_t) conn;
  (void)srcloc;
#ifdef DDSI_INCLUDE_SSM
  if (srcloc)
    return joinleave_ssm_mcgroup(uc->m_sock, 0, srcloc, mcloc, interf);
  else
#endif
    return joinleave_asm_mcgroup(uc->m_sock, 0, mcloc, interf);
}

static void ddsi_udp_release_conn (ddsi_tran_conn_t conn)
{
  ddsi_udp_conn_t uc = (ddsi_udp_conn_t) conn;
  DDS_CTRACE (&conn->m_base.gv->logconfig,
              "ddsi_udp_release_conn %s socket %"PRIdSOCK" port %"PRIu32"\n",
              conn->m_base.m_multicast ? "multicast" : "unicast",
              uc->m_sock,
              uc->m_base.m_base.m_port);
  ddsrt_close (uc->m_sock);
#if defined _WIN32 && !defined WINCE
  WSACloseEvent(uc->m_sockEvent);
#endif
  ddsrt_free (conn);
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

static char *ddsi_udp_locator_to_string (ddsi_tran_factory_t tran, char *dst, size_t sizeof_dst, const nn_locator_t *loc, int with_port)
{
  if (loc->kind != NN_LOCATOR_KIND_UDPv4MCGEN) {
    return ddsi_ipaddr_to_string(tran, dst, sizeof_dst, loc, with_port);
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
      snprintf (dst + pos, sizeof_dst - pos, ":%"PRIu32, loc->port);
    }
    return dst;
  }
}

static void ddsi_udp_fini (ddsi_tran_factory_t fact)
{
  DDS_CLOG (DDS_LC_CONFIG, &fact->gv->logconfig, "udp finalized\n");
  ddsrt_free (fact);
}

int ddsi_udp_init (struct q_globals *gv)
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
