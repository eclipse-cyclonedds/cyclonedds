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
#include "os/os.h"
#include "ddsi/ddsi_tran.h"
#include "ddsi/ddsi_raweth.h"
#include "ddsi/ddsi_ipaddr.h"
#include "ddsi/ddsi_mcgroup.h"
#include "ddsi/q_nwif.h"
#include "ddsi/q_config.h"
#include "ddsi/q_log.h"
#include "ddsi/q_error.h"
#include "ddsi/q_pcap.h"
#include "os/os_atomics.h"

#ifdef __linux
#include <linux/if_packet.h>
#include <sys/types.h>
#include <ifaddrs.h>

typedef struct ddsi_tran_factory * ddsi_raweth_factory_t;

typedef struct ddsi_raweth_config
{
  struct nn_group_membership *mship;
}
* ddsi_raweth_config_t;

typedef struct ddsi_raweth_conn
{
  struct ddsi_tran_conn m_base;
  os_socket m_sock;
  int m_ifindex;
}
* ddsi_raweth_conn_t;

static struct ddsi_raweth_config ddsi_raweth_config_g;
static struct ddsi_tran_factory ddsi_raweth_factory_g;
static pa_uint32_t init_g = PA_UINT32_INIT(0);

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

static ssize_t ddsi_raweth_conn_read (ddsi_tran_conn_t conn, unsigned char * buf, size_t len, nn_locator_t *srcloc)
{
  int err;
  ssize_t ret;
  struct msghdr msghdr;
  struct sockaddr_ll src;
  struct iovec msg_iov;
  socklen_t srclen = (socklen_t) sizeof (src);

  msg_iov.iov_base = (void*) buf;
  msg_iov.iov_len = len;

  memset (&msghdr, 0, sizeof (msghdr));

  msghdr.msg_name = &src;
  msghdr.msg_namelen = srclen;
  msghdr.msg_iov = &msg_iov;
  msghdr.msg_iovlen = 1;

  do {
    ret = recvmsg(((ddsi_raweth_conn_t) conn)->m_sock, &msghdr, 0);
    err = (ret == -1) ? os_getErrno() : 0;
  } while (err == os_sockEINTR);

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
#if SYSDEPS_MSGHDR_FLAGS
        || (msghdr.msg_flags & MSG_TRUNC)
#endif
        )
    {
      char addrbuf[DDSI_LOCSTRLEN];
      snprintf(addrbuf, sizeof(addrbuf), "[%02x:%02x:%02x:%02x:%02x:%02x]:%u",
               src.sll_addr[0], src.sll_addr[1], src.sll_addr[2],
               src.sll_addr[3], src.sll_addr[4], src.sll_addr[5], ntohs(src.sll_protocol));
      NN_WARNING ("%s => %d truncated to %d\n", addrbuf, (int)ret, (int)len);
    }
  }
  else if (err != os_sockENOTSOCK && err != os_sockECONNRESET)
  {
    NN_ERROR ("UDP recvmsg sock %d: ret %d errno %d\n", (int) ((ddsi_raweth_conn_t) conn)->m_sock, (int) ret, err);
  }
  return ret;
}

static ssize_t ddsi_raweth_conn_write (ddsi_tran_conn_t conn, const nn_locator_t *dst, size_t niov, const ddsi_iovec_t *iov, uint32_t flags)
{
  ddsi_raweth_conn_t uc = (ddsi_raweth_conn_t) conn;
  int err;
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
  msg.msg_iov = (ddsi_iovec_t *) iov;
  msg.msg_iovlen = niov;
#ifdef MSG_NOSIGNAL
  sendflags |= MSG_NOSIGNAL;
#endif
  do {
    ret = sendmsg (uc->m_sock, &msg, sendflags);
    err = (ret == -1) ? os_getErrno() : 0;
  } while (err == os_sockEINTR || err == os_sockEWOULDBLOCK || (err == os_sockEPERM && retry-- > 0));
  if (ret == -1)
  {
    switch (err)
    {
      case os_sockEINTR:
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
        NN_ERROR("ddsi_raweth_conn_write failed with error code %d", err);
    }
  }
  return ret;
}

static os_handle ddsi_raweth_conn_handle (ddsi_tran_base_t base)
{
  return ((ddsi_raweth_conn_t) base)->m_sock;
}

static bool ddsi_raweth_supports (int32_t kind)
{
  return (kind == NN_LOCATOR_KIND_RAWETH);
}

static int ddsi_raweth_conn_locator (ddsi_tran_base_t base, nn_locator_t *loc)
{
  ddsi_raweth_conn_t uc = (ddsi_raweth_conn_t) base;
  int ret = -1;
  if (uc->m_sock != Q_INVALID_SOCKET)
  {
    loc->kind = NN_LOCATOR_KIND_RAWETH;
    loc->port = uc->m_base.m_base.m_port;
    memcpy(loc->address, gv.extloc.address, sizeof (loc->address));
    ret = 0;
  }
  return ret;
}

static ddsi_tran_conn_t ddsi_raweth_create_conn (uint32_t port, ddsi_tran_qos_t qos)
{
  os_socket sock;
  int rc;
  ddsi_raweth_conn_t uc = NULL;
  struct sockaddr_ll addr;
  bool mcast = (bool) (qos ? qos->m_multicast : FALSE);

  /* If port is zero, need to create dynamic port */

  if (port == 0 || port > 65535)
  {
    NN_ERROR("ddsi_raweth_create_conn %s port %u - using port number as ethernet type, %u won't do\n", mcast ? "multicast" : "unicast", port, port);
    return NULL;
  }

  if ((sock = socket(PF_PACKET, SOCK_DGRAM, htons((uint16_t)port))) == -1)
  {
    rc = os_getErrno();
    NN_ERROR("ddsi_raweth_create_conn %s port %u failed ... errno = %d\n", mcast ? "multicast" : "unicast", port, rc);
    return NULL;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sll_family = AF_PACKET;
  addr.sll_protocol = htons((uint16_t)port);
  addr.sll_ifindex = (int)gv.interfaceNo;
  addr.sll_pkttype = PACKET_HOST | PACKET_BROADCAST | PACKET_MULTICAST;
  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
  {
    rc = os_getErrno();
    close(sock);
    NN_ERROR("ddsi_raweth_create_conn %s bind port %u failed ... errno = %d\n", mcast ? "multicast" : "unicast", port, rc);
    return NULL;
  }

  uc = (ddsi_raweth_conn_t) os_malloc (sizeof (*uc));
  memset (uc, 0, sizeof (*uc));
  uc->m_sock = sock;
  uc->m_ifindex = addr.sll_ifindex;
  ddsi_factory_conn_init (&ddsi_raweth_factory_g, &uc->m_base);
  uc->m_base.m_base.m_port = port;
  uc->m_base.m_base.m_trantype = DDSI_TRAN_CONN;
  uc->m_base.m_base.m_multicast = mcast;
  uc->m_base.m_base.m_handle_fn = ddsi_raweth_conn_handle;
  uc->m_base.m_base.m_locator_fn = ddsi_raweth_conn_locator;
  uc->m_base.m_read_fn = ddsi_raweth_conn_read;
  uc->m_base.m_write_fn = ddsi_raweth_conn_write;

  nn_log(LC_INFO, "ddsi_raweth_create_conn %s socket %d port %u\n", mcast ? "multicast" : "unicast", uc->m_sock, uc->m_base.m_base.m_port);
  return uc ? &uc->m_base : NULL;
}

static int isbroadcast(const nn_locator_t *loc)
{
  int i;
  for(i = 0; i < 6; i++)
    if (loc->address[10 + i] != 0xff)
      return 0;
  return 1;
}

static int joinleave_asm_mcgroup (os_socket socket, int join, const nn_locator_t *mcloc, const struct nn_interface *interf)
{
  int rc;
  struct packet_mreq mreq;
  mreq.mr_ifindex = (int)interf->if_index;
  mreq.mr_type = PACKET_MR_MULTICAST;
  mreq.mr_alen = 6;
  memcpy(mreq.mr_address, mcloc + 10, 6);
  rc = setsockopt(socket, SOL_PACKET, join ? PACKET_ADD_MEMBERSHIP : PACKET_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
  return (rc == -1) ? os_getErrno() : 0;
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
  nn_log
  (
    LC_INFO,
    "ddsi_raweth_release_conn %s socket %d port %d\n",
    conn->m_base.m_multicast ? "multicast" : "unicast",
    uc->m_sock,
    uc->m_base.m_base.m_port
  );
  os_sockFree (uc->m_sock);
  os_free (conn);
}

static int ddsi_raweth_is_mcaddr (const ddsi_tran_factory_t tran, const nn_locator_t *loc)
{
  (void) tran;
  assert (loc->kind == NN_LOCATOR_KIND_RAWETH);
  return (loc->address[10] & 1);
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
    int o, p;
    if (sscanf (str, "%x%n", &o, &p) != 1 || o < 0 || o > 255)
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

static void ddsi_raweth_deinit(void)
{
  if (pa_dec32_nv(&init_g) == 0) {
    if (ddsi_raweth_config_g.mship)
      free_group_membership(ddsi_raweth_config_g.mship);
    nn_log (LC_INFO | LC_CONFIG, "raweth de-initialized\n");
  }
}

static int ddsi_raweth_enumerate_interfaces (ddsi_tran_factory_t factory, int max, struct os_ifAttributes_s *interfs)
{
  struct ifaddrs *ifaddr, *ifa;
  int cnt = 0;
  (void)factory;
  if (getifaddrs (&ifaddr) == -1)
    return -errno;
  for (ifa = ifaddr; ifa && cnt < max; ifa = ifa->ifa_next)
  {
    struct os_ifAttributes_s *f = &interfs[cnt];
    struct sockaddr_ll *x;
    if (ifa->ifa_addr == NULL)
      continue;
    if (ifa->ifa_addr->sa_family != AF_PACKET)
      continue;
    if ((ifa->ifa_flags & (IFF_UP | IFF_BROADCAST)) != (IFF_UP | IFF_BROADCAST))
      continue;
    strncpy(f->name, ifa->ifa_name, sizeof(f->name));
    f->name[sizeof(f->name)-1] = 0;
    f->flags = ifa->ifa_flags;
    f->interfaceIndexNo = if_nametoindex(f->name);
    x = (struct sockaddr_ll *)&f->address;
    memcpy(x, ifa->ifa_addr, sizeof (*x));
    x = (struct sockaddr_ll *)&f->network_mask;
    if (ifa->ifa_netmask)
      memcpy(x, ifa->ifa_netmask, sizeof (*x));
    else
      memset(x, 0, sizeof(*x));
    x = (struct sockaddr_ll *)&f->broadcast_address;
    if (ifa->ifa_broadaddr)
      memcpy(x, ifa->ifa_broadaddr, sizeof (*x));
    else
      memset(x, 0, sizeof(*x));
    cnt++;
  }
  freeifaddrs (ifaddr);
  return cnt;
}

int ddsi_raweth_init (void)
{
  if (pa_inc32_nv(&init_g) == 1) {
    memset (&ddsi_raweth_factory_g, 0, sizeof (ddsi_raweth_factory_g));
    ddsi_raweth_factory_g.m_free_fn = ddsi_raweth_deinit;
    ddsi_raweth_factory_g.m_kind = NN_LOCATOR_KIND_RAWETH;
    ddsi_raweth_factory_g.m_typename = "raweth";
    ddsi_raweth_factory_g.m_default_spdp_address = "raweth/ff:ff:ff:ff:ff:ff";
    ddsi_raweth_factory_g.m_connless = TRUE;
    ddsi_raweth_factory_g.m_supports_fn = ddsi_raweth_supports;
    ddsi_raweth_factory_g.m_create_conn_fn = ddsi_raweth_create_conn;
    ddsi_raweth_factory_g.m_release_conn_fn = ddsi_raweth_release_conn;
    ddsi_raweth_factory_g.m_join_mc_fn = ddsi_raweth_join_mc;
    ddsi_raweth_factory_g.m_leave_mc_fn = ddsi_raweth_leave_mc;
    ddsi_raweth_factory_g.m_is_mcaddr_fn = ddsi_raweth_is_mcaddr;
    ddsi_raweth_factory_g.m_is_nearby_address_fn = ddsi_ipaddr_is_nearby_address;
    ddsi_raweth_factory_g.m_locator_from_string_fn = ddsi_raweth_address_from_string;
    ddsi_raweth_factory_g.m_locator_to_string_fn = ddsi_raweth_to_string;
    ddsi_raweth_factory_g.m_enumerate_interfaces_fn = ddsi_raweth_enumerate_interfaces;
    ddsi_factory_add (&ddsi_raweth_factory_g);

    ddsi_raweth_config_g.mship = new_group_membership();

    nn_log (LC_INFO | LC_CONFIG, "raweth initialized\n");
  }
  return 0;
}

#else

int ddsi_raweth_init (void) { return 0; }

#endif /* defined __linux */
