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
#include "os/os.h"
#include "os/os_atomics.h"
#include "ddsi_eth.h"
#include "ddsi/ddsi_tran.h"
#include "ddsi/ddsi_udp.h"
#include "ddsi/ddsi_ipaddr.h"
#include "ddsi/ddsi_mcgroup.h"
#include "ddsi/q_nwif.h"
#include "ddsi/q_config.h"
#include "ddsi/q_log.h"
#include "ddsi/q_pcap.h"

extern void ddsi_factory_conn_init (ddsi_tran_factory_t factory, ddsi_tran_conn_t conn);

typedef struct ddsi_tran_factory * ddsi_udp_factory_t;

typedef struct ddsi_udp_config
{
  struct nn_group_membership *mship;
}
* ddsi_udp_config_t;

typedef struct ddsi_udp_conn
{
  struct ddsi_tran_conn m_base;
  os_socket m_sock;
#if defined _WIN32 && !defined WINCE
  WSAEVENT m_sockEvent;
#endif
  int m_diffserv;
}
* ddsi_udp_conn_t;

static struct ddsi_udp_config ddsi_udp_config_g;
static struct ddsi_tran_factory ddsi_udp_factory_g;
static os_atomic_uint32_t ddsi_udp_init_g = OS_ATOMIC_UINT32_INIT(0);

static ssize_t ddsi_udp_conn_read (ddsi_tran_conn_t conn, unsigned char * buf, size_t len, bool allow_spurious, nn_locator_t *srcloc)
{
  int err;
  ssize_t ret;
  struct msghdr msghdr;
  os_sockaddr_storage src;
  os_iovec_t msg_iov;
  socklen_t srclen = (socklen_t) sizeof (src);
  (void) allow_spurious;

  msg_iov.iov_base = (void*) buf;
  msg_iov.iov_len = (os_iov_len_t)len; /* Windows uses unsigned, POSIX (except Linux) int */

  msghdr.msg_name = &src;
  msghdr.msg_namelen = srclen;
  msghdr.msg_iov = &msg_iov;
  msghdr.msg_iovlen = 1;
#if !defined(__sun) || defined(_XPG4_2)
  msghdr.msg_control = NULL;
  msghdr.msg_controllen = 0;
#else
  msghdr.msg_accrights = NULL;
  msghdr.msg_accrightslen = 0;
#endif

  do {
    ret = recvmsg(((ddsi_udp_conn_t) conn)->m_sock, &msghdr, 0);
    err = (ret == -1) ? os_getErrno() : 0;
  } while (err == os_sockEINTR);

  if (ret > 0)
  {
    if (srcloc)
      ddsi_ipaddr_to_loc(srcloc, (os_sockaddr *)&src, src.ss_family == AF_INET ? NN_LOCATOR_KIND_UDPv4 : NN_LOCATOR_KIND_UDPv6);

    /* Check for udp packet truncation */
    if ((((size_t) ret) > len)
#if OS_MSGHDR_FLAGS
        || (msghdr.msg_flags & MSG_TRUNC)
#endif
        )
    {
      char addrbuf[DDSI_LOCSTRLEN];
      nn_locator_t tmp;
      ddsi_ipaddr_to_loc(&tmp, (os_sockaddr *)&src, src.ss_family == AF_INET ? NN_LOCATOR_KIND_UDPv4 : NN_LOCATOR_KIND_UDPv6);
      ddsi_locator_to_string(addrbuf, sizeof(addrbuf), &tmp);
      DDS_WARNING("%s => %d truncated to %d\n", addrbuf, (int)ret, (int)len);
    }
  }
  else if (err != os_sockENOTSOCK && err != os_sockECONNRESET)
  {
    DDS_ERROR("UDP recvmsg sock %d: ret %d errno %d\n", (int) ((ddsi_udp_conn_t) conn)->m_sock, (int) ret, err);
  }
  return ret;
}

static void set_msghdr_iov (struct msghdr *mhdr, os_iovec_t *iov, size_t iovlen)
{
  mhdr->msg_iov = iov;
  mhdr->msg_iovlen = (os_msg_iovlen_t)iovlen;
}

static ssize_t ddsi_udp_conn_write (ddsi_tran_conn_t conn, const nn_locator_t *dst, size_t niov, const os_iovec_t *iov, uint32_t flags)
{
  int err;
  ssize_t ret;
  unsigned retry = 2;
  int sendflags = 0;
  struct msghdr msg;
  os_sockaddr_storage dstaddr;
  assert(niov <= INT_MAX);
  ddsi_ipaddr_from_loc(&dstaddr, dst);
  set_msghdr_iov (&msg, (os_iovec_t *) iov, niov);
  msg.msg_name = &dstaddr;
  msg.msg_namelen = (socklen_t) os_sockaddr_get_size((os_sockaddr *) &dstaddr);
#if !defined(__sun) || defined(_XPG4_2)
  msg.msg_control = NULL;
  msg.msg_controllen = 0;
#else
  msg.msg_accrights = NULL;
  msg.msg_accrightslen = 0;
#endif
#if OS_MSGHDR_FLAGS
  msg.msg_flags = (int) flags;
#else
  OS_UNUSED_ARG(flags);
#endif
#ifdef MSG_NOSIGNAL
  sendflags |= MSG_NOSIGNAL;
#endif
  do {
    ddsi_udp_conn_t uc = (ddsi_udp_conn_t) conn;
    ret = sendmsg (uc->m_sock, &msg, sendflags);
    err = (ret == -1) ? os_getErrno() : 0;
#if defined _WIN32 && !defined WINCE
    if (err == os_sockEWOULDBLOCK) {
      WSANETWORKEVENTS ev;
      WaitForSingleObject(uc->m_sockEvent, INFINITE);
      WSAEnumNetworkEvents(uc->m_sock, uc->m_sockEvent, &ev);
    }
#endif
  } while (err == os_sockEINTR || err == os_sockEWOULDBLOCK || (err == os_sockEPERM && retry-- > 0));
  if (ret > 0 && gv.pcap_fp)
  {
    os_sockaddr_storage sa;
    socklen_t alen = sizeof (sa);
    if (getsockname (((ddsi_udp_conn_t) conn)->m_sock, (struct sockaddr *) &sa, &alen) == -1)
      memset(&sa, 0, sizeof(sa));
    write_pcap_sent (gv.pcap_fp, now (), &sa, &msg, (size_t) ret);
  }
  else if (ret == -1)
  {
    switch (err)
    {
      case os_sockEPERM:
      case os_sockECONNRESET:
#ifdef os_sockENETUNREACH
      case os_sockENETUNREACH:
#endif
#ifdef os_sockEHOSTUNREACH
      case os_sockEHOSTUNREACH:
#endif
        break;
      default:
        DDS_ERROR("ddsi_udp_conn_write failed with error code %d", err);
    }
  }
  return ret;
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

static os_socket ddsi_udp_conn_handle (ddsi_tran_base_t base)
{
  return ((ddsi_udp_conn_t) base)->m_sock;
}

static bool ddsi_udp_supports (int32_t kind)
{
  return
    kind == ddsi_udp_factory_g.m_kind ||
    (kind == NN_LOCATOR_KIND_UDPv4MCGEN && ddsi_udp_factory_g.m_kind == NN_LOCATOR_KIND_UDPv4);
}

static int ddsi_udp_conn_locator (ddsi_tran_base_t base, nn_locator_t *loc)
{
  int ret = -1;
  ddsi_udp_conn_t uc = (ddsi_udp_conn_t) base;
  if (uc->m_sock != OS_INVALID_SOCKET)
  {
    loc->kind = ddsi_udp_factory_g.m_kind;
    loc->port = uc->m_base.m_base.m_port;
    memcpy(loc->address, gv.extloc.address, sizeof (loc->address));
    ret = 0;
  }
  return ret;
}

static unsigned short get_socket_port (os_socket socket)
{
  os_sockaddr_storage addr;
  socklen_t addrlen = sizeof (addr);
  if (getsockname (socket, (os_sockaddr *) &addr, &addrlen) < 0)
  {
    int err = os_getErrno();
    DDS_ERROR("ddsi_udp_get_socket_port: getsockname errno %d\n", err);
    return 0;
  }

  return os_sockaddr_get_port((os_sockaddr *)&addr);
}

static ddsi_tran_conn_t ddsi_udp_create_conn
(
  uint32_t port,
  ddsi_tran_qos_t qos
)
{
  int ret;
  os_socket sock;
  ddsi_udp_conn_t uc = NULL;
  bool mcast = (bool) (qos ? qos->m_multicast : false);

  /* If port is zero, need to create dynamic port */

  ret = make_socket
  (
    &sock,
    (unsigned short) port,
    false,
    mcast
  );

  if (ret == 0)
  {
    uc = (ddsi_udp_conn_t) os_malloc (sizeof (*uc));
    memset (uc, 0, sizeof (*uc));

    uc->m_sock = sock;
    uc->m_diffserv = qos ? qos->m_diffserv : 0;
#if defined _WIN32 && !defined WINCE
    uc->m_sockEvent = WSACreateEvent();
    WSAEventSelect(uc->m_sock, uc->m_sockEvent, FD_WRITE);
#endif

    ddsi_factory_conn_init (&ddsi_udp_factory_g, &uc->m_base);
    uc->m_base.m_base.m_port = get_socket_port (sock);
    uc->m_base.m_base.m_trantype = DDSI_TRAN_CONN;
    uc->m_base.m_base.m_multicast = mcast;
    uc->m_base.m_base.m_handle_fn = ddsi_udp_conn_handle;
    uc->m_base.m_base.m_locator_fn = ddsi_udp_conn_locator;

    uc->m_base.m_read_fn = ddsi_udp_conn_read;
    uc->m_base.m_write_fn = ddsi_udp_conn_write;
    uc->m_base.m_disable_multiplexing_fn = ddsi_udp_disable_multiplexing;

    DDS_TRACE
    (
      "ddsi_udp_create_conn %s socket %"PRIsock" port %u\n",
      mcast ? "multicast" : "unicast",
      uc->m_sock,
      uc->m_base.m_base.m_port
    );
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
    if ((uc->m_diffserv != 0) && (ddsi_udp_factory_g.m_kind == NN_LOCATOR_KIND_UDPv4))
    {
      if (os_sockSetsockopt (sock, IPPROTO_IP, IP_TOS, (char*) &uc->m_diffserv, sizeof (uc->m_diffserv)) != os_resultSuccess)
      {
        int err = os_getErrno();
        DDS_ERROR("ddsi_udp_create_conn: set diffserv error %d\n", err);
      }
    }
#endif
  }
  else
  {
    if (config.participantIndex != PARTICIPANT_INDEX_AUTO)
    {
      DDS_ERROR
      (
        "UDP make_socket failed for %s port %u\n",
        mcast ? "multicast" : "unicast",
        port
      );
    }
  }

  return uc ? &uc->m_base : NULL;
}

static int joinleave_asm_mcgroup (os_socket socket, int join, const nn_locator_t *mcloc, const struct nn_interface *interf)
{
  os_result rc;
  os_sockaddr_storage mcip;
  ddsi_ipaddr_from_loc(&mcip, mcloc);
#if OS_SOCKET_HAS_IPV6
  if (config.transport_selector == TRANS_UDP6)
  {
    os_ipv6_mreq ipv6mreq;
    memset (&ipv6mreq, 0, sizeof (ipv6mreq));
    memcpy (&ipv6mreq.ipv6mr_multiaddr, &((os_sockaddr_in6 *) &mcip)->sin6_addr, sizeof (ipv6mreq.ipv6mr_multiaddr));
    ipv6mreq.ipv6mr_interface = interf ? interf->if_index : 0;
    rc = os_sockSetsockopt (socket, IPPROTO_IPV6, join ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP, &ipv6mreq, sizeof (ipv6mreq));
  }
  else
#endif
  {
    struct ip_mreq mreq;
    mreq.imr_multiaddr = ((os_sockaddr_in *) &mcip)->sin_addr;
    if (interf)
      memcpy (&mreq.imr_interface, interf->loc.address + 12, sizeof (mreq.imr_interface));
    else
      mreq.imr_interface.s_addr = htonl (INADDR_ANY);
    rc = os_sockSetsockopt (socket, IPPROTO_IP, join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP, (char *) &mreq, sizeof (mreq));
  }
  return (rc != os_resultSuccess) ? os_getErrno() : 0;
}

#ifdef DDSI_INCLUDE_SSM
static int joinleave_ssm_mcgroup (os_socket socket, int join, const nn_locator_t *srcloc, const nn_locator_t *mcloc, const struct nn_interface *interf)
{
  os_result rc;
  os_sockaddr_storage mcip, srcip;
  ddsi_ipaddr_from_loc(&mcip, mcloc);
  ddsi_ipaddr_from_loc(&srcip, srcloc);
#if OS_SOCKET_HAS_IPV6
  if (config.transport_selector == TRANS_UDP6)
  {
    struct group_source_req gsr;
    memset (&gsr, 0, sizeof (gsr));
    gsr.gsr_interface = interf ? interf->if_index : 0;
    memcpy (&gsr.gsr_group, &mcip, sizeof (gsr.gsr_group));
    memcpy (&gsr.gsr_source, &srcip, sizeof (gsr.gsr_source));
    rc = os_sockSetsockopt (socket, IPPROTO_IPV6, join ? MCAST_JOIN_SOURCE_GROUP : MCAST_LEAVE_SOURCE_GROUP, &gsr, sizeof (gsr));
  }
  else
#endif
  {
    struct ip_mreq_source mreq;
    memset (&mreq, 0, sizeof (mreq));
    mreq.imr_sourceaddr = ((os_sockaddr_in *) &srcip)->sin_addr;
    mreq.imr_multiaddr = ((os_sockaddr_in *) &mcip)->sin_addr;
    if (interf)
      memcpy (&mreq.imr_interface, interf->loc.address + 12, sizeof (mreq.imr_interface));
    else
      mreq.imr_interface.s_addr = INADDR_ANY;
    rc = os_sockSetsockopt (socket, IPPROTO_IP, join ? IP_ADD_SOURCE_MEMBERSHIP : IP_DROP_SOURCE_MEMBERSHIP, &mreq, sizeof (mreq));
  }
  return (rc != os_resultSuccess) ? os_getErrno() : 0;
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
  DDS_TRACE
  (
    "ddsi_udp_release_conn %s socket %"PRIsock" port %u\n",
    conn->m_base.m_multicast ? "multicast" : "unicast",
    uc->m_sock,
    uc->m_base.m_base.m_port
  );
  os_sockFree (uc->m_sock);
#if defined _WIN32 && !defined WINCE
  WSACloseEvent(uc->m_sockEvent);
#endif
  os_free (conn);
}

void ddsi_udp_fini (void)
{
    if(os_atomic_dec32_nv (&ddsi_udp_init_g) == 0) {
        free_group_membership(ddsi_udp_config_g.mship);
        memset (&ddsi_udp_factory_g, 0, sizeof (ddsi_udp_factory_g));
        DDS_LOG(DDS_LC_CONFIG, "udp finalized\n");
    }
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
#if OS_SOCKET_HAS_IPV6
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
#if OS_SOCKET_HAS_IPV6
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
  return ddsi_ipaddr_from_string(tran, loc, str, ddsi_udp_factory_g.m_kind);
}

static char *ddsi_udp_locator_to_string (ddsi_tran_factory_t tran, char *dst, size_t sizeof_dst, const nn_locator_t *loc, int with_port)
{
  if (loc->kind != NN_LOCATOR_KIND_UDPv4MCGEN) {
    return ddsi_ipaddr_to_string(tran, dst, sizeof_dst, loc, with_port);
  } else {
    os_sockaddr_in src;
    nn_udpv4mcgen_address_t mcgen;
    size_t pos;
    int cnt;
    assert(sizeof_dst > 1);
    memcpy (&mcgen, loc->address, sizeof (mcgen));
    memset (&src, 0, sizeof (src));
    src.sin_family = AF_INET;
    memcpy (&src.sin_addr.s_addr, &mcgen.ipv4, 4);
    os_sockaddrtostr ((const os_sockaddr *) &src, dst, sizeof_dst);
    pos = strlen (dst);
    assert (pos <= sizeof_dst);
    cnt = snprintf (dst + pos, sizeof_dst - pos, ";%u;%u;%u", mcgen.base, mcgen.count, mcgen.idx);
    if (cnt > 0) {
      pos += (size_t)cnt;
    }
    if (with_port && pos < sizeof_dst) {
      snprintf (dst + pos, sizeof_dst - pos, ":%d", loc->port);
    }
    return dst;
  }
}

static void ddsi_udp_deinit(void)
{
  if (os_atomic_dec32_nv(&ddsi_udp_init_g) == 0) {
    if (ddsi_udp_config_g.mship)
      free_group_membership(ddsi_udp_config_g.mship);
    DDS_LOG(DDS_LC_CONFIG, "udp de-initialized\n");
  }
}

int ddsi_udp_init (void)
{
  /* TODO: proper init_once. Either the call doesn't need it, in which case
   * this can be removed. Or the call does, in which case it should be done right.
   * The lack of locking suggests it isn't needed.
   */
  if (os_atomic_inc32_nv (&ddsi_udp_init_g) == 1)
  {
    memset (&ddsi_udp_factory_g, 0, sizeof (ddsi_udp_factory_g));
    ddsi_udp_factory_g.m_free_fn = ddsi_udp_deinit;
    ddsi_udp_factory_g.m_kind = NN_LOCATOR_KIND_UDPv4;
    ddsi_udp_factory_g.m_typename = "udp";
    ddsi_udp_factory_g.m_default_spdp_address = "udp/239.255.0.1";
    ddsi_udp_factory_g.m_connless = true;
    ddsi_udp_factory_g.m_supports_fn = ddsi_udp_supports;
    ddsi_udp_factory_g.m_create_conn_fn = ddsi_udp_create_conn;
    ddsi_udp_factory_g.m_release_conn_fn = ddsi_udp_release_conn;
    ddsi_udp_factory_g.m_free_fn = ddsi_udp_fini;
    ddsi_udp_factory_g.m_join_mc_fn = ddsi_udp_join_mc;
    ddsi_udp_factory_g.m_leave_mc_fn = ddsi_udp_leave_mc;
    ddsi_udp_factory_g.m_is_mcaddr_fn = ddsi_udp_is_mcaddr;
#ifdef DDSI_INCLUDE_SSM
    ddsi_udp_factory_g.m_is_ssm_mcaddr_fn = ddsi_udp_is_ssm_mcaddr;
#endif
    ddsi_udp_factory_g.m_is_nearby_address_fn = ddsi_ipaddr_is_nearby_address;
    ddsi_udp_factory_g.m_locator_from_string_fn = ddsi_udp_address_from_string;
    ddsi_udp_factory_g.m_locator_to_string_fn = ddsi_udp_locator_to_string;
    ddsi_udp_factory_g.m_enumerate_interfaces_fn = ddsi_eth_enumerate_interfaces;
#if OS_SOCKET_HAS_IPV6
    if (config.transport_selector == TRANS_UDP6)
    {
      ddsi_udp_factory_g.m_kind = NN_LOCATOR_KIND_UDPv6;
      ddsi_udp_factory_g.m_typename = "udp6";
      ddsi_udp_factory_g.m_default_spdp_address = "udp6/ff02::ffff:239.255.0.1";
    }
#endif

    ddsi_udp_config_g.mship = new_group_membership();

    ddsi_factory_add (&ddsi_udp_factory_g);

    DDS_LOG(DDS_LC_CONFIG, "udp initialized\n");
  }
  return 0;
}
