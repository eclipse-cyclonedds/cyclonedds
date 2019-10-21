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
#include "cyclonedds/ddsi/ddsi_tran.h"
#include "cyclonedds/ddsi/ddsi_raweth.h"
#include "cyclonedds/ddsi/ddsi_ipaddr.h"
#include "cyclonedds/ddsi/ddsi_mcgroup.h"
#include "cyclonedds/ddsi/q_nwif.h"
#include "cyclonedds/ddsi/q_config.h"
#include "cyclonedds/ddsi/q_log.h"
#include "cyclonedds/ddsi/q_pcap.h"
#include "cyclonedds/ddsi/q_globals.h"
#include "cyclonedds/ddsrt/atomics.h"
#include "cyclonedds/ddsrt/heap.h"
#include "cyclonedds/ddsrt/log.h"
#include "cyclonedds/ddsrt/sockets.h"

#if defined(__linux) && !LWIP_SOCKET
#include <linux/if_packet.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

typedef struct ddsi_raweth_conn {
  struct ddsi_tran_conn m_base;
  ddsrt_socket_t m_sock;
  int m_ifindex;
} *ddsi_raweth_conn_t;

static char *ddsi_raweth_to_string (ddsi_tran_factory_t tran, char *dst, size_t sizeof_dst, const nn_locator_t *loc, int with_port)
{
  (void)tran;
  if (with_port)
    snprintf(dst, sizeof_dst, "[%02x:%02x:%02x:%02x:%02x:%02x]:%u",
             loc->address[10], loc->address[11], loc->address[12],
             loc->address[13], loc->address[14], loc->address[15], loc->port);
  else
    snprintf(dst, sizeof_dst, "[%02x:%02x:%02x:%02x:%02x:%02x]",
             loc->address[10], loc->address[11], loc->address[12],
             loc->address[13], loc->address[14], loc->address[15]);
  return dst;
}

static ssize_t ddsi_raweth_conn_read (ddsi_tran_conn_t conn, unsigned char * buf, size_t len, bool allow_spurious, nn_locator_t *srcloc)
{
  dds_return_t rc;
  ssize_t ret = 0;
  struct msghdr msghdr;
  struct sockaddr_ll src;
  struct iovec msg_iov;
  socklen_t srclen = (socklen_t) sizeof (src);
  (void) allow_spurious;

  msg_iov.iov_base = (void*) buf;
  msg_iov.iov_len = len;

  memset (&msghdr, 0, sizeof (msghdr));

  msghdr.msg_name = &src;
  msghdr.msg_namelen = srclen;
  msghdr.msg_iov = &msg_iov;
  msghdr.msg_iovlen = 1;

  do {
    rc = ddsrt_recvmsg(((ddsi_raweth_conn_t) conn)->m_sock, &msghdr, 0, &ret);
  } while (rc == DDS_RETCODE_INTERRUPTED);

  if (ret > 0)
  {
    if (srcloc)
    {
      srcloc->kind = NN_LOCATOR_KIND_RAWETH;
      srcloc->port = ntohs (src.sll_protocol);
      memset(srcloc->address, 0, 10);
      memcpy(srcloc->address + 10, src.sll_addr, 6);
    }

    /* Check for udp packet truncation */
    if ((((size_t) ret) > len)
#if DDSRT_MSGHDR_FLAGS
        || (msghdr.msg_flags & MSG_TRUNC)
#endif
        )
    {
      char addrbuf[DDSI_LOCSTRLEN];
      snprintf(addrbuf, sizeof(addrbuf), "[%02x:%02x:%02x:%02x:%02x:%02x]:%u",
               src.sll_addr[0], src.sll_addr[1], src.sll_addr[2],
               src.sll_addr[3], src.sll_addr[4], src.sll_addr[5], ntohs(src.sll_protocol));
      DDS_CWARNING(&conn->m_base.gv->logconfig, "%s => %d truncated to %d\n", addrbuf, (int)ret, (int)len);
    }
  }
  else if (rc != DDS_RETCODE_OK &&
           rc != DDS_RETCODE_BAD_PARAMETER &&
           rc != DDS_RETCODE_NO_CONNECTION)
  {
    DDS_CERROR(&conn->m_base.gv->logconfig, "UDP recvmsg sock %d: ret %d retcode %d\n", (int) ((ddsi_raweth_conn_t) conn)->m_sock, (int) ret, rc);
  }
  return ret;
}

static ssize_t ddsi_raweth_conn_write (ddsi_tran_conn_t conn, const nn_locator_t *dst, size_t niov, const ddsrt_iovec_t *iov, uint32_t flags)
{
  ddsi_raweth_conn_t uc = (ddsi_raweth_conn_t) conn;
  dds_return_t rc;
  ssize_t ret;
  unsigned retry = 2;
  int sendflags = 0;
  struct msghdr msg;
  struct sockaddr_ll dstaddr;
  assert(niov <= INT_MAX);
  memset (&dstaddr, 0, sizeof (dstaddr));
  dstaddr.sll_family = AF_PACKET;
  dstaddr.sll_protocol = htons ((uint16_t) dst->port);
  dstaddr.sll_ifindex = uc->m_ifindex;
  dstaddr.sll_halen = 6;
  memcpy(dstaddr.sll_addr, dst->address + 10, 6);
  memset(&msg, 0, sizeof(msg));
  msg.msg_name = &dstaddr;
  msg.msg_namelen = sizeof(dstaddr);
  msg.msg_flags = (int) flags;
  msg.msg_iov = (ddsrt_iovec_t *) iov;
  msg.msg_iovlen = niov;
#ifdef MSG_NOSIGNAL
  sendflags |= MSG_NOSIGNAL;
#endif
  do {
    rc = ddsrt_sendmsg (uc->m_sock, &msg, sendflags, &ret);
  } while ((rc == DDS_RETCODE_INTERRUPTED) ||
           (rc == DDS_RETCODE_TRY_AGAIN) ||
           (rc == DDS_RETCODE_NOT_ALLOWED && retry-- > 0));
  if (rc != DDS_RETCODE_OK &&
      rc != DDS_RETCODE_INTERRUPTED &&
      rc != DDS_RETCODE_NOT_ALLOWED &&
      rc != DDS_RETCODE_NO_CONNECTION)
  {
    DDS_CERROR(&conn->m_base.gv->logconfig, "ddsi_raweth_conn_write failed with retcode %d", rc);
  }
  return (rc == DDS_RETCODE_OK ? ret : -1);
}

static ddsrt_socket_t ddsi_raweth_conn_handle (ddsi_tran_base_t base)
{
  return ((ddsi_raweth_conn_t) base)->m_sock;
}

static bool ddsi_raweth_supports (const struct ddsi_tran_factory *fact, int32_t kind)
{
  (void) fact;
  return (kind == NN_LOCATOR_KIND_RAWETH);
}

static int ddsi_raweth_conn_locator (ddsi_tran_factory_t fact, ddsi_tran_base_t base, nn_locator_t *loc)
{
  ddsi_raweth_conn_t uc = (ddsi_raweth_conn_t) base;
  int ret = -1;
  (void) fact;
  if (uc->m_sock != DDSRT_INVALID_SOCKET)
  {
    loc->kind = NN_LOCATOR_KIND_RAWETH;
    loc->port = uc->m_base.m_base.m_port;
    memcpy(loc->address, uc->m_base.m_base.gv->extloc.address, sizeof (loc->address));
    ret = 0;
  }
  return ret;
}

static ddsi_tran_conn_t ddsi_raweth_create_conn (ddsi_tran_factory_t fact, uint32_t port, ddsi_tran_qos_t qos)
{
  ddsrt_socket_t sock;
  dds_return_t rc;
  ddsi_raweth_conn_t uc = NULL;
  struct sockaddr_ll addr;
  bool mcast = (bool) (qos ? qos->m_multicast : 0);

  /* If port is zero, need to create dynamic port */

  if (port == 0 || port > 65535)
  {
    DDS_CERROR (&fact->gv->logconfig, "ddsi_raweth_create_conn %s port %u - using port number as ethernet type, %u won't do\n", mcast ? "multicast" : "unicast", port, port);
    return NULL;
  }

  rc = ddsrt_socket(&sock, PF_PACKET, SOCK_DGRAM, htons((uint16_t)port));
  if (rc != DDS_RETCODE_OK)
  {
    DDS_CERROR (&fact->gv->logconfig, "ddsi_raweth_create_conn %s port %u failed ... retcode = %d\n", mcast ? "multicast" : "unicast", port, rc);
    return NULL;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sll_family = AF_PACKET;
  addr.sll_protocol = htons((uint16_t)port);
  addr.sll_ifindex = (int)fact->gv->interfaceNo;
  addr.sll_pkttype = PACKET_HOST | PACKET_BROADCAST | PACKET_MULTICAST;
  rc = ddsrt_bind(sock, (struct sockaddr *)&addr, sizeof(addr));
  if (rc != DDS_RETCODE_OK)
  {
    ddsrt_close(sock);
    DDS_CERROR (&fact->gv->logconfig, "ddsi_raweth_create_conn %s bind port %u failed ... retcode = %d\n", mcast ? "multicast" : "unicast", port, rc);
    return NULL;
  }

  if ((uc = (ddsi_raweth_conn_t) ddsrt_malloc (sizeof (*uc))) == NULL)
  {
    ddsrt_close(sock);
    return NULL;
  }

  memset (uc, 0, sizeof (*uc));
  uc->m_sock = sock;
  uc->m_ifindex = addr.sll_ifindex;
  ddsi_factory_conn_init (fact, &uc->m_base);
  uc->m_base.m_base.m_port = port;
  uc->m_base.m_base.m_trantype = DDSI_TRAN_CONN;
  uc->m_base.m_base.m_multicast = mcast;
  uc->m_base.m_base.m_handle_fn = ddsi_raweth_conn_handle;
  uc->m_base.m_locator_fn = ddsi_raweth_conn_locator;
  uc->m_base.m_read_fn = ddsi_raweth_conn_read;
  uc->m_base.m_write_fn = ddsi_raweth_conn_write;
  uc->m_base.m_disable_multiplexing_fn = 0;

  DDS_CTRACE (&fact->gv->logconfig, "ddsi_raweth_create_conn %s socket %d port %u\n", mcast ? "multicast" : "unicast", uc->m_sock, uc->m_base.m_base.m_port);
  return &uc->m_base;
}

static int isbroadcast(const nn_locator_t *loc)
{
  int i;
  for(i = 0; i < 6; i++)
    if (loc->address[10 + i] != 0xff)
      return 0;
  return 1;
}

static int joinleave_asm_mcgroup (ddsrt_socket_t socket, int join, const nn_locator_t *mcloc, const struct nn_interface *interf)
{
  int rc;
  struct packet_mreq mreq;
  mreq.mr_ifindex = (int)interf->if_index;
  mreq.mr_type = PACKET_MR_MULTICAST;
  mreq.mr_alen = 6;
  memcpy(mreq.mr_address, mcloc + 10, 6);
  rc = ddsrt_setsockopt(socket, SOL_PACKET, join ? PACKET_ADD_MEMBERSHIP : PACKET_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
  return (rc == DDS_RETCODE_OK) ? 0 : rc;
}

static int ddsi_raweth_join_mc (ddsi_tran_conn_t conn, const nn_locator_t *srcloc, const nn_locator_t *mcloc, const struct nn_interface *interf)
{
  if (isbroadcast(mcloc))
    return 0;
  else
  {
    ddsi_raweth_conn_t uc = (ddsi_raweth_conn_t) conn;
    (void)srcloc;
    return joinleave_asm_mcgroup(uc->m_sock, 1, mcloc, interf);
  }
}

static int ddsi_raweth_leave_mc (ddsi_tran_conn_t conn, const nn_locator_t *srcloc, const nn_locator_t *mcloc, const struct nn_interface *interf)
{
  if (isbroadcast(mcloc))
    return 0;
  else
  {
    ddsi_raweth_conn_t uc = (ddsi_raweth_conn_t) conn;
    (void)srcloc;
    return joinleave_asm_mcgroup(uc->m_sock, 0, mcloc, interf);
  }
}

static void ddsi_raweth_release_conn (ddsi_tran_conn_t conn)
{
  ddsi_raweth_conn_t uc = (ddsi_raweth_conn_t) conn;
  DDS_CTRACE (&conn->m_base.gv->logconfig,
              "ddsi_raweth_release_conn %s socket %d port %d\n",
              conn->m_base.m_multicast ? "multicast" : "unicast",
              uc->m_sock,
              uc->m_base.m_base.m_port);
  ddsrt_close (uc->m_sock);
  ddsrt_free (conn);
}

static int ddsi_raweth_is_mcaddr (const ddsi_tran_factory_t tran, const nn_locator_t *loc)
{
  (void) tran;
  assert (loc->kind == NN_LOCATOR_KIND_RAWETH);
  return (loc->address[10] & 1);
}

static int ddsi_raweth_is_ssm_mcaddr (const ddsi_tran_factory_t tran, const nn_locator_t *loc)
{
  (void) tran;
  (void) loc;
  return 0;
}

static enum ddsi_nearby_address_result ddsi_raweth_is_nearby_address (ddsi_tran_factory_t tran, const nn_locator_t *loc, const nn_locator_t *ownloc, size_t ninterf, const struct nn_interface interf[])
{
  (void) tran;
  (void) loc;
  (void) ownloc;
  (void) ninterf;
  (void) interf;
  return DNAR_LOCAL;
}

static enum ddsi_locator_from_string_result ddsi_raweth_address_from_string (ddsi_tran_factory_t tran, nn_locator_t *loc, const char *str)
{
  int i = 0;
  (void)tran;
  loc->kind = NN_LOCATOR_KIND_RAWETH;
  loc->port = NN_LOCATOR_PORT_INVALID;
  memset (loc->address, 0, sizeof (loc->address));
  while (i < 6 && *str != 0)
  {
    unsigned o;
    int p;
    if (sscanf (str, "%x%n", &o, &p) != 1 || o > 255)
      return AFSR_INVALID;
    loc->address[10 + i++] = (unsigned char) o;
    str += p;
    if (i < 6)
    {
      if (*str != ':')
        return AFSR_INVALID;
      str++;
    }
  }
  if (*str)
    return AFSR_INVALID;
  return AFSR_OK;
}

static void ddsi_raweth_deinit(ddsi_tran_factory_t fact)
{
  DDS_CLOG (DDS_LC_CONFIG, &fact->gv->logconfig, "raweth de-initialized\n");
  ddsrt_free (fact);
}

static int ddsi_raweth_enumerate_interfaces (ddsi_tran_factory_t fact, enum transport_selector transport_selector, ddsrt_ifaddrs_t **ifs)
{
  int afs[] = { AF_PACKET, DDSRT_AF_TERM };
  (void)fact;
  (void)transport_selector;
  return ddsrt_getifaddrs(ifs, afs);
}

int ddsi_raweth_init (struct q_globals *gv)
{
  struct ddsi_tran_factory *fact = ddsrt_malloc (sizeof (*fact));
  memset (fact, 0, sizeof (*fact));
  fact->gv = gv;
  fact->m_free_fn = ddsi_raweth_deinit;
  fact->m_kind = NN_LOCATOR_KIND_RAWETH;
  fact->m_typename = "raweth";
  fact->m_default_spdp_address = "raweth/ff:ff:ff:ff:ff:ff";
  fact->m_connless = 1;
  fact->m_supports_fn = ddsi_raweth_supports;
  fact->m_create_conn_fn = ddsi_raweth_create_conn;
  fact->m_release_conn_fn = ddsi_raweth_release_conn;
  fact->m_join_mc_fn = ddsi_raweth_join_mc;
  fact->m_leave_mc_fn = ddsi_raweth_leave_mc;
  fact->m_is_mcaddr_fn = ddsi_raweth_is_mcaddr;
  fact->m_is_ssm_mcaddr_fn = ddsi_raweth_is_ssm_mcaddr;
  fact->m_is_nearby_address_fn = ddsi_raweth_is_nearby_address;
  fact->m_locator_from_string_fn = ddsi_raweth_address_from_string;
  fact->m_locator_to_string_fn = ddsi_raweth_to_string;
  fact->m_enumerate_interfaces_fn = ddsi_raweth_enumerate_interfaces;
  ddsi_factory_add (gv, fact);
  GVLOG (DDS_LC_CONFIG, "raweth initialized\n");
  return 0;
}

#else

int ddsi_raweth_init (struct q_globals *gv) { (void) gv; return 0; }

#endif /* defined __linux */
