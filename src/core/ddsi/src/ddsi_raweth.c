// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsi/ddsi_log.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__tran.h"
#include "ddsi__raweth.h"
#include "ddsi__ipaddr.h"
#include "ddsi__mcgroup.h"
#include "ddsi__pcap.h"

#if defined(__linux) && !LWIP_SOCKET
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/filter.h>
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


struct ddsi_ethernet_header {
  unsigned char dmac[ETH_ALEN];
  unsigned char smac[ETH_ALEN];
  unsigned short proto;
} __attribute__((packed));


struct ddsi_vlan_header {
  struct ddsi_ethernet_header e;
  unsigned short vtag;
  unsigned short proto;
} __attribute__((packed));

union ddsi_cmessage {
  struct cmsghdr chdr;
  char buf[CMSG_SPACE(sizeof(struct tpacket_auxdata))];
};

static char *ddsi_raweth_to_string (char *dst, size_t sizeof_dst, const ddsi_locator_t *loc, struct ddsi_tran_conn * conn, int with_port)
{
  (void) conn;
  if (with_port)
    (void) snprintf(dst, sizeof_dst, "[%02x:%02x:%02x:%02x:%02x:%02x.%u.%u]:%u",
                    loc->address[10], loc->address[11], loc->address[12],
                    loc->address[13], loc->address[14], loc->address[15],
                    loc->port >> 20, (loc->port >> 17) & 7, loc->port & 0xffff);
  else
    (void) snprintf(dst, sizeof_dst, "[%02x:%02x:%02x:%02x:%02x:%02x.%u.%u]",
                    loc->address[10], loc->address[11], loc->address[12],
                    loc->address[13], loc->address[14], loc->address[15],
                    loc->port >> 20, (loc->port >> 17) & 7);
  return dst;
}

static ssize_t ddsi_raweth_conn_read (struct ddsi_tran_conn * conn, unsigned char * buf, size_t len, bool allow_spurious, ddsi_locator_t *srcloc)
{
  dds_return_t rc;
  ssize_t ret = 0;
  struct msghdr msghdr;
  struct sockaddr_ll src;
  struct ddsi_ethernet_header ehdr;
  union ddsi_cmessage cmessage;
  struct tpacket_auxdata *pauxd;
  struct cmsghdr *cptr;
  uint16_t vtag = 0;
  uint32_t port;
  struct iovec msg_iov[2];
  socklen_t srclen = (socklen_t) sizeof (src);
  (void) allow_spurious;

  msg_iov[0].iov_base = (void *) &ehdr;;
  msg_iov[0].iov_len = sizeof (ehdr);
  msg_iov[1].iov_base = (void *) buf;
  msg_iov[1].iov_len = len;

  memset (&msghdr, 0, sizeof (msghdr));

  msghdr.msg_name = &src;
  msghdr.msg_namelen = srclen;
  msghdr.msg_iov = msg_iov;
  msghdr.msg_iovlen = 2;
  msghdr.msg_control = &cmessage;
  msghdr.msg_controllen = sizeof(cmessage);

  do {
    rc = ddsrt_recvmsg(((ddsi_raweth_conn_t) conn)->m_sock, &msghdr, 0, &ret);
  } while (rc == DDS_RETCODE_INTERRUPTED);

  if (ret > (ssize_t) sizeof (ehdr))
  {
    ret -= (ssize_t) sizeof (ehdr);

    for (cptr = CMSG_FIRSTHDR(&msghdr); cptr; cptr = CMSG_NXTHDR( &msghdr, cptr))
    {
      if ((cptr->cmsg_len < CMSG_LEN(sizeof( struct tpacket_auxdata))) || (cptr->cmsg_level != SOL_PACKET) || (cptr->cmsg_type != PACKET_AUXDATA))
        continue;
      pauxd = (struct tpacket_auxdata *)CMSG_DATA(cptr);
      vtag = pauxd->tp_vlan_tci;
      break;
    }

    // FIXME: ((vtag & 0xf000) << 16)) looks decidedly odd, << 4 would make more sense
    port = (uint32_t)(ntohs (src.sll_protocol) + ((vtag & 0xfff) << 20) + ((vtag & 0xf000) << 16));
    if (srcloc)
    {
      srcloc->kind = DDSI_LOCATOR_KIND_RAWETH;
      srcloc->port = port;
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
      (void) snprintf(addrbuf, sizeof(addrbuf), "[%02x:%02x:%02x:%02x:%02x:%02x]:%u",
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

static ssize_t ddsi_raweth_conn_write (struct ddsi_tran_conn * conn, const ddsi_locator_t *dst, const ddsi_tran_write_msgfrags_t *msgfrags, uint32_t flags)
{
  ddsi_raweth_conn_t uc = (ddsi_raweth_conn_t) conn;
  dds_return_t rc;
  ssize_t ret;
  unsigned retry = 2;
  int sendflags = 0;
  struct msghdr msg;
  struct sockaddr_ll dstaddr;
  struct ddsi_vlan_header vhdr;
  uint16_t vtag;
  size_t hdrlen;

  assert(msgfrags->niov <= INT_MAX - 1); // we'll be adding one later on
  memset (&dstaddr, 0, sizeof (dstaddr));
  dstaddr.sll_family = AF_PACKET;
  dstaddr.sll_protocol = htons ((uint16_t) uc->m_base.m_base.m_port);
  dstaddr.sll_ifindex = uc->m_ifindex;
  dstaddr.sll_halen = 6;
  memcpy(dstaddr.sll_addr, dst->address + 10, 6);

  vtag = (uint16_t)(((dst->port >> 20) & 0xfff) + (((dst->port >> 16) & 0xe) << 12));

  memcpy(vhdr.e.dmac, dstaddr.sll_addr, 6);
  memcpy(vhdr.e.smac, uc->m_base.m_base.gv->interfaces[0].loc.address + 10, 6);

  if (vtag) {
    vhdr.e.proto = htons ((uint16_t) ETH_P_8021Q);
    vhdr.vtag = htons (vtag);
    vhdr.proto = htons ((uint16_t) uc->m_base.m_base.m_port);
    hdrlen = sizeof(vhdr);
  } else {
    vhdr.e.proto = htons ((uint16_t) uc->m_base.m_base.m_port);
    hdrlen = 14;
  }

  DDSRT_STATIC_ASSERT(DDSI_TRAN_RESERVED_IOV_SLOTS >= 1);
  // beware: casting const away; it works with how things are now, but it is a bit nasty
  ddsrt_iovec_t * const iovs = (ddsrt_iovec_t *) &msgfrags->tran_reserved[DDSI_TRAN_RESERVED_IOV_SLOTS - 1];
  iovs[0].iov_base = &vhdr;
  iovs[0].iov_len = hdrlen;

  memset(&msg, 0, sizeof(msg));
  msg.msg_name = &dstaddr;
  msg.msg_namelen = sizeof(dstaddr);
  msg.msg_flags = (int) flags;
  msg.msg_iov = iovs;
  msg.msg_iovlen = msgfrags->niov + 1;
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

static ddsrt_socket_t ddsi_raweth_conn_handle (struct ddsi_tran_base * base)
{
  return ((ddsi_raweth_conn_t) base)->m_sock;
}

static bool ddsi_raweth_supports (const struct ddsi_tran_factory *fact, int32_t kind)
{
  (void) fact;
  return (kind == DDSI_LOCATOR_KIND_RAWETH);
}

static int ddsi_raweth_conn_locator (struct ddsi_tran_factory * fact, struct ddsi_tran_base * base, ddsi_locator_t *loc)
{
  ddsi_raweth_conn_t uc = (ddsi_raweth_conn_t) base;
  int ret = -1;
  (void) fact;
  if (uc->m_sock != DDSRT_INVALID_SOCKET)
  {
    loc->kind = DDSI_LOCATOR_KIND_RAWETH;
    loc->port = uc->m_base.m_base.m_port;
    memcpy(loc->address, uc->m_base.m_base.gv->interfaces[0].loc.address, sizeof (loc->address));
    ret = 0;
  }
  return ret;
}

/* The linux kernel appears to remove the vlan tag before applying the filter and adjusting the ethernet header.
 * Therefore the used filter only checks if the ethernet type is as expected.
 * When that would not be the case the following filter could be used instead.
 * Alternative filter: ether proto port or (ether proto 0x8100 and ether[16:2] == port)
 *
 *   { 0x28, 0, 0, 0x0000000c },  ldh [12] - load half word from frame offset 12, which is the ethernet type
 *   { 0x15, 3, 0, 0x00001ce8 },  jeq #0x1ce8 - equal to port goto accept
 *   { 0x15, 0, 3, 0x00008100 },  jeq #0x8100 - mot equal 802.1Q vlan protocol goto drop
 *   { 0x28, 0, 0, 0x00000010 },  ldh [16] - load half word at offset 16
 *   { 0x15, 0, 1, 0x00001ce8 },  jne #0x1ce8 - not equal to port goto drop
 *   { 0x6, 0, 0, 0x00040000 },   accept: ret #-1 - accept packet
 *   { 0x6, 0, 0, 0x00000000 }    drop: ret #0 - drop packet
 */
static dds_return_t ddsi_raweth_set_filter (struct ddsi_tran_factory * fact, ddsrt_socket_t sock, uint32_t port)
{
  struct sock_filter code[] = {
      { 0x28, 0, 0, 0x0000000c },        /* ldh [12] - load half word from frame offset 12, which is the ethernet type */
      { 0x15, 0, 1, (port & 0xffff) },   /* jeq port- not equal to port goto drop */
      { 0x6, 0, 0, 0x00040000 },         /* ret #-1 - accept packe t*/
      { 0x6, 0, 0, 0x00000000 }          /* drop: #0 - drop packet */
  };
  struct sock_fprog prg = { .len = sizeof(code)/sizeof(struct sock_filter), .filter = code };
  dds_return_t rc;

  rc = ddsrt_setsockopt (sock, SOL_SOCKET, SO_ATTACH_FILTER, &prg, sizeof (prg));
  if (rc != DDS_RETCODE_OK)
  {
    DDS_CERROR (&fact->gv->logconfig, "ddsrt_setsockopt attach filter for protocol %u failed ... retcode = %d\n", port, rc);
    return DDS_RETCODE_ERROR;
  }
  return DDS_RETCODE_OK;
}

static dds_return_t ddsi_raweth_create_conn (struct ddsi_tran_conn **conn_out, struct ddsi_tran_factory * fact, uint32_t port, const struct ddsi_tran_qos *qos)
{
  ddsrt_socket_t sock;
  dds_return_t rc;
  ddsi_raweth_conn_t uc = NULL;
  struct sockaddr_ll addr;
  struct packet_mreq mreq;
  bool mcast = (qos->m_purpose == DDSI_TRAN_QOS_RECV_MC);
  struct ddsi_domaingv const * const gv = fact->gv;
  struct ddsi_network_interface const * const intf = qos->m_interface ? qos->m_interface : &gv->interfaces[0];

  /* If port is zero, need to create dynamic port */

  if (port == 0 || port > 65535)
  {
    DDS_CERROR (&fact->gv->logconfig, "ddsi_raweth_create_conn %s port %u - using port number as ethernet type, %u won't do\n", mcast ? "multicast" : "unicast", port, port);
    return DDS_RETCODE_ERROR;
  }

  rc = ddsrt_socket(&sock, PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if (rc != DDS_RETCODE_OK)
  {
    DDS_CERROR (&fact->gv->logconfig, "ddsi_raweth_create_conn %s port %u failed ... retcode = %d\n", mcast ? "multicast" : "unicast", port, rc);
    return DDS_RETCODE_ERROR;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sll_family = AF_PACKET;
  addr.sll_protocol = htons(ETH_P_ALL);
  addr.sll_ifindex = (int)intf->if_index;
  addr.sll_pkttype = PACKET_HOST | PACKET_BROADCAST | PACKET_MULTICAST;
  rc = ddsrt_bind(sock, (struct sockaddr *)&addr, sizeof(addr));
  if (rc != DDS_RETCODE_OK)
  {
    ddsrt_close(sock);
    DDS_CERROR (&fact->gv->logconfig, "ddsi_raweth_create_conn %s bind port %u failed ... retcode = %d\n", mcast ? "multicast" : "unicast", port, rc);
    return DDS_RETCODE_ERROR;
  }

  memset(&mreq, 0, sizeof (mreq));
  mreq.mr_ifindex = (int)intf->if_index;
  mreq.mr_type = PACKET_MR_PROMISC;
  mreq.mr_alen = 6;

  rc = ddsrt_setsockopt (sock, SOL_PACKET, PACKET_ADD_MEMBERSHIP, (void*)&mreq, (socklen_t)sizeof (mreq));
  if (rc != DDS_RETCODE_OK)
  {
    ddsrt_close(sock);
    DDS_CERROR (&fact->gv->logconfig, "ddsi_raweth_create_conn %s set promiscuous mode failed ... retcode = %d\n", mcast ? "multicast" : "unicast", rc);
    return DDS_RETCODE_ERROR;
  }

  int one = 1;
  rc = ddsrt_setsockopt (sock, SOL_PACKET, PACKET_AUXDATA, &one, sizeof(one));
  if (rc != DDS_RETCODE_OK)
  {
    DDS_CWARNING (&fact->gv->logconfig, "ddsi_raweth_create_conn %s set to receive auxilary data failed ... retcode = %d\n", mcast ? "multicast" : "unicast", rc);
  }

  rc = ddsi_raweth_set_filter (fact, sock, port);
  if (rc != DDS_RETCODE_OK)
  {
    ddsrt_close(sock);
    DDS_CERROR (&fact->gv->logconfig, "ddsi_raweth_create_conn %s set fiter failed ... retcode = %d\n", mcast ? "multicast" : "unicast", rc);
    return rc;
  }

  if ((uc = (ddsi_raweth_conn_t) ddsrt_malloc (sizeof (*uc))) == NULL)
  {
    ddsrt_close(sock);
    return DDS_RETCODE_ERROR;
  }

  memset (uc, 0, sizeof (*uc));
  uc->m_sock = sock;
  uc->m_ifindex = addr.sll_ifindex;
  ddsi_factory_conn_init (fact, intf, &uc->m_base);
  uc->m_base.m_base.m_port = port;
  uc->m_base.m_base.m_trantype = DDSI_TRAN_CONN;
  uc->m_base.m_base.m_multicast = mcast;
  uc->m_base.m_base.m_handle_fn = ddsi_raweth_conn_handle;
  uc->m_base.m_locator_fn = ddsi_raweth_conn_locator;
  uc->m_base.m_read_fn = ddsi_raweth_conn_read;
  uc->m_base.m_write_fn = ddsi_raweth_conn_write;
  uc->m_base.m_disable_multiplexing_fn = 0;

  DDS_CTRACE (&fact->gv->logconfig, "ddsi_raweth_create_conn %s socket %d port %u\n", mcast ? "multicast" : "unicast", uc->m_sock, uc->m_base.m_base.m_port);
  *conn_out = &uc->m_base;
  return DDS_RETCODE_OK;
}


static int isbroadcast(const ddsi_locator_t *loc)
{
  int i;
  for(i = 0; i < 6; i++)
    if (loc->address[10 + i] != 0xff)
      return 0;
  return 1;
}

static int joinleave_asm_mcgroup (ddsrt_socket_t socket, int join, const ddsi_locator_t *mcloc, const struct ddsi_network_interface *interf)
{
  int rc;
  struct packet_mreq mreq;
  mreq.mr_ifindex = (int)interf->if_index;
  mreq.mr_type = PACKET_MR_MULTICAST;
  mreq.mr_alen = 6;
  memcpy(mreq.mr_address, mcloc->address + 10, 6);
  rc = ddsrt_setsockopt(socket, SOL_PACKET, join ? PACKET_ADD_MEMBERSHIP : PACKET_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
  return (rc == DDS_RETCODE_OK) ? 0 : rc;
}

static int ddsi_raweth_join_mc (struct ddsi_tran_conn * conn, const ddsi_locator_t *srcloc, const ddsi_locator_t *mcloc, const struct ddsi_network_interface *interf)
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

static int ddsi_raweth_leave_mc (struct ddsi_tran_conn * conn, const ddsi_locator_t *srcloc, const ddsi_locator_t *mcloc, const struct ddsi_network_interface *interf)
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

static void ddsi_raweth_release_conn (struct ddsi_tran_conn * conn)
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

static int ddsi_raweth_is_loopbackaddr (const struct ddsi_tran_factory *tran, const ddsi_locator_t *loc)
{
  (void) tran;
  (void) loc;
  return 0;
}

static int ddsi_raweth_is_mcaddr (const struct ddsi_tran_factory *tran, const ddsi_locator_t *loc)
{
  (void) tran;
  assert (loc->kind == DDSI_LOCATOR_KIND_RAWETH);
  return (loc->address[10] & 1);
}

static int ddsi_raweth_is_ssm_mcaddr (const struct ddsi_tran_factory *tran, const ddsi_locator_t *loc)
{
  (void) tran;
  (void) loc;
  return 0;
}

static enum ddsi_nearby_address_result ddsi_raweth_is_nearby_address (const ddsi_locator_t *loc, size_t ninterf, const struct ddsi_network_interface interf[], size_t *interf_idx)
{
  (void) ninterf;
  if (interf_idx)
    *interf_idx = 0;
  if (memcmp (interf[0].loc.address, loc->address, sizeof (loc->address)) == 0)
    return DNAR_SELF;
  else
    return DNAR_LOCAL;
}

static enum ddsi_locator_from_string_result ddsi_raweth_address_from_string (const struct ddsi_tran_factory *tran, ddsi_locator_t *loc, const char *str)
{
  int i = 0;
  (void)tran;
  loc->kind = DDSI_LOCATOR_KIND_RAWETH;
  loc->port = DDSI_LOCATOR_PORT_INVALID;
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
  if (*str == '.')
  {
    unsigned vlanid, vlancfi;
    int p;
    if (sscanf (str, ".%u.%u%n", &vlanid, &vlancfi, &p) != 2)
      return AFSR_INVALID;
    else if (vlanid == 0 || vlanid > 4094)
      return AFSR_INVALID;
    else if (vlancfi > 7)
      return AFSR_INVALID;
    loc->port = (vlanid << 20) | (vlancfi << 17);
    str += p;
  }
  if (*str)
  {
    return AFSR_INVALID;
  }
  return AFSR_OK;
}

static void ddsi_raweth_deinit(struct ddsi_tran_factory * fact)
{
  DDS_CLOG (DDS_LC_CONFIG, &fact->gv->logconfig, "raweth de-initialized\n");
  ddsrt_free (fact);
}

static int ddsi_raweth_enumerate_interfaces (struct ddsi_tran_factory * fact, enum ddsi_transport_selector transport_selector, ddsrt_ifaddrs_t **ifs)
{
  int afs[] = { AF_PACKET, DDSRT_AF_TERM };
  (void)fact;
  (void)transport_selector;
  return ddsrt_getifaddrs(ifs, afs);
}

static int ddsi_raweth_is_valid_port (const struct ddsi_tran_factory *fact, uint32_t port)
{
  uint32_t eport;
  uint32_t vlanid;
  uint32_t vlancfi;

  eport = port & 0xffffu;
  vlanid = (port >> 20) & 0xfffu;
  vlancfi = (port >> 16) & 0x1u;

  (void) fact;
  return (eport >= 1 && eport <= 65535) && (vlanid < 4095) && vlancfi == 0;
}

static uint32_t ddsi_raweth_receive_buffer_size (const struct ddsi_tran_factory *fact)
{
  (void) fact;
  return 0;
}

static int ddsi_raweth_locator_from_sockaddr (const struct ddsi_tran_factory *tran, ddsi_locator_t *loc, const struct sockaddr *sockaddr)
{
  (void) tran;

  if (sockaddr->sa_family != AF_PACKET)
    return -1;

  loc->kind = DDSI_LOCATOR_KIND_RAWETH;
  loc->port = DDSI_LOCATOR_PORT_INVALID;
  memset (loc->address, 0, 10);
  memcpy (loc->address + 10, ((struct sockaddr_ll *) sockaddr)->sll_addr, 6);
  return 0;
}

static uint32_t ddsi_raweth_get_locator_port (const struct ddsi_tran_factory *factory, const ddsi_locator_t *loc)
{
  (void) factory;
  return loc->port & 0xffff;
}

static uint32_t ddsi_raweth_get_locator_aux (const struct ddsi_tran_factory *factory, const ddsi_locator_t *loc)
{
  (void) factory;
  return loc->port >> 16;
}

static void ddsi_raweth_set_locator_port (const struct ddsi_tran_factory *factory, ddsi_locator_t *loc, uint32_t port)
{
  (void) factory;
  assert ((port & 0xffff0000) == 0);
  loc->port = (loc->port & 0xffff0000) | port;
}

static void ddsi_raweth_set_locator_aux (const struct ddsi_tran_factory *factory, ddsi_locator_t *loc, uint32_t aux)
{
  (void) factory;
  assert ((aux & 0xffff0000) == 0);
  loc->port = (loc->port & 0xffff) | (aux << 16);
}

int ddsi_raweth_init (struct ddsi_domaingv *gv)
{
  struct ddsi_tran_factory *fact = ddsrt_malloc (sizeof (*fact));
  memset (fact, 0, sizeof (*fact));
  fact->gv = gv;
  fact->m_free_fn = ddsi_raweth_deinit;
  fact->m_typename = "raweth";
  fact->m_default_spdp_address = "raweth/01:00:5e:7f:00:01";
  fact->m_connless = 1;
  fact->m_enable_spdp = 1;
  fact->m_supports_fn = ddsi_raweth_supports;
  fact->m_create_conn_fn = ddsi_raweth_create_conn;
  fact->m_release_conn_fn = ddsi_raweth_release_conn;
  fact->m_join_mc_fn = ddsi_raweth_join_mc;
  fact->m_leave_mc_fn = ddsi_raweth_leave_mc;
  fact->m_is_loopbackaddr_fn = ddsi_raweth_is_loopbackaddr;
  fact->m_is_mcaddr_fn = ddsi_raweth_is_mcaddr;
  fact->m_is_ssm_mcaddr_fn = ddsi_raweth_is_ssm_mcaddr;
  fact->m_is_nearby_address_fn = ddsi_raweth_is_nearby_address;
  fact->m_locator_from_string_fn = ddsi_raweth_address_from_string;
  fact->m_locator_to_string_fn = ddsi_raweth_to_string;
  fact->m_enumerate_interfaces_fn = ddsi_raweth_enumerate_interfaces;
  fact->m_is_valid_port_fn = ddsi_raweth_is_valid_port;
  fact->m_receive_buffer_size_fn = ddsi_raweth_receive_buffer_size;
  fact->m_locator_from_sockaddr_fn = ddsi_raweth_locator_from_sockaddr;
  fact->m_get_locator_port_fn = ddsi_raweth_get_locator_port;
  fact->m_set_locator_port_fn = ddsi_raweth_set_locator_port;
  fact->m_get_locator_aux_fn = ddsi_raweth_get_locator_aux;
  fact->m_set_locator_aux_fn = ddsi_raweth_set_locator_aux;
  ddsi_factory_add (gv, fact);
  GVLOG (DDS_LC_CONFIG, "raweth initialized\n");
  return 0;
}

#else

int ddsi_raweth_init (struct ddsi_domaingv *gv) { (void) gv; return 0; }

#endif /* defined __linux */
