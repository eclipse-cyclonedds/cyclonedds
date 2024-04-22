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

#if (defined(__linux) || defined(__FreeBSD__) || defined(__QNXNTO__) || defined(__APPLE__)) && !LWIP_SOCKET
#include <sys/types.h>
#include <string.h>

#if defined(__linux)
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/filter.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#define DDSI_ETHERTYPE_VLAN ETH_P_8021Q
#elif defined(__FreeBSD__) || defined(__QNXNTO__) || defined(__APPLE__)
#define DDSI_USE_BSD (1)
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#if defined(__FreeBSD__) || defined(__APPLE__)
#include <net/ethernet.h>
#elif defined(__QNXNTO__)
#define DDSI_BPF_IS_CLONING_DEV (1)
#include <net/ethertypes.h>
#include <net/if_ether.h>
#endif
#include <net/bpf.h>
#include <net/if_dl.h>
#define DDSI_ETHERTYPE_VLAN ETHERTYPE_VLAN
#endif

#if defined(__linux)
#define DDSI_ETH_ADDR_LEN ETH_ALEN
#define DDSI_LINK_FAMILY AF_PACKET

union ddsi_cmessage {
  struct cmsghdr chdr;
  char buf[CMSG_SPACE(sizeof(struct tpacket_auxdata))];
};

#elif DDSI_USE_BSD
#define DDSI_ETH_ADDR_LEN ETHER_ADDR_LEN
#define DDSI_LINK_FAMILY AF_LINK
#endif

typedef struct ddsi_raweth_conn {
  struct ddsi_tran_conn m_base;
  ddsrt_socket_ext_t m_sockext;
  int m_ifindex;
#if DDSI_USE_BSD
  ddsrt_mutex_t lock;
  char *buffer;
  uint32_t buflen;
  ssize_t avail;
  char *bptr;
#endif
} *ddsi_raweth_conn_t;

struct ddsi_ethernet_header {
  unsigned char dmac[DDSI_ETH_ADDR_LEN];
  unsigned char smac[DDSI_ETH_ADDR_LEN];
  unsigned short proto;
} __attribute__((packed));

struct ddsi_vlan_header {
  struct ddsi_ethernet_header e;
  unsigned short vtag;
  unsigned short proto;
} __attribute__((packed));

static ddsrt_socket_t ddsi_raweth_conn_handle (struct ddsi_tran_base * base);
static int ddsi_raweth_conn_locator (struct ddsi_tran_factory * fact, struct ddsi_tran_base * base, ddsi_locator_t *loc);


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

static void set_pktinfo(struct ddsi_network_packet_info *pktinfo, const uint8_t * addr, uint16_t port, uint16_t vtag)
{
  pktinfo->src.kind = DDSI_LOCATOR_KIND_RAWETH;
  pktinfo->src.port = (uint32_t)port + (((uint32_t)vtag & 0xfff) << 20) + (((uint32_t)vtag & 0xf000) << 4);
  memset(pktinfo->src.address, 0, 10);
  memcpy(pktinfo->src.address + 10, addr, 6);
  pktinfo->if_index = 0;
  pktinfo->dst.kind = DDSI_LOCATOR_KIND_INVALID;
}

static size_t set_ethernet_header(struct ddsi_vlan_header *hdr, uint16_t proto, const ddsi_locator_t * dst, const ddsi_locator_t * src)
{
  uint16_t vtag = (uint16_t)(((dst->port >> 20) & 0xfff) + (((dst->port >> 16) & 0xe) << 12));
  size_t hdrlen;

  memcpy(hdr->e.dmac, dst->address + 10, 6);
  memcpy(hdr->e.smac, src->address + 10, 6);

  if (vtag) {
    hdr->e.proto = htons ((uint16_t) DDSI_ETHERTYPE_VLAN);
    hdr->vtag = htons (vtag);
    hdr->proto = htons (proto);
    hdrlen = sizeof(*hdr);
  } else {
    hdr->e.proto = htons (proto);
    hdrlen = 14;
  }
  return hdrlen;
}

#if defined(__linux)
static ssize_t ddsi_raweth_conn_read (struct ddsi_tran_conn * conn, unsigned char * buf, size_t len, bool allow_spurious, struct ddsi_network_packet_info *pktinfo)
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
    rc = ddsrt_recvmsg(&((ddsi_raweth_conn_t) conn)->m_sockext, &msghdr, 0, &ret);
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

    if (pktinfo)
      set_pktinfo(pktinfo, src.sll_addr, ntohs (src.sll_protocol), vtag);

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
    DDS_CERROR(&conn->m_base.gv->logconfig, "UDP recvmsg sock %d: ret %d retcode %d\n", (int) ((ddsi_raweth_conn_t) conn)->m_sockext.sock, (int) ret, rc);
  }
  return ret;
}

static ssize_t ddsi_raweth_conn_write (struct ddsi_tran_conn * conn, const ddsi_locator_t *dst, const ddsi_tran_write_msgfrags_t *msgfrags, uint32_t flags)
{
  ddsi_raweth_conn_t uc = (ddsi_raweth_conn_t) conn;
  dds_return_t rc;
  ssize_t ret = -1;
  unsigned retry = 2;
  int sendflags = 0;
  struct msghdr msg;
  struct sockaddr_ll dstaddr;
  struct ddsi_vlan_header vhdr;
  size_t hdrlen;

  assert(msgfrags->niov <= INT_MAX - 1); // we'll be adding one later on
  memset (&dstaddr, 0, sizeof (dstaddr));
  dstaddr.sll_family = AF_PACKET;
  dstaddr.sll_protocol = htons ((uint16_t) uc->m_base.m_base.m_port);
  dstaddr.sll_ifindex = uc->m_ifindex;
  dstaddr.sll_halen = 6;
  memcpy(dstaddr.sll_addr, dst->address + 10, 6);

  hdrlen = set_ethernet_header(&vhdr, (uint16_t) uc->m_base.m_base.m_port, dst, &uc->m_base.m_base.gv->interfaces[0].loc);

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
    rc = ddsrt_sendmsg (uc->m_sockext.sock, &msg, sendflags, &ret);
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

/* The linux kernel appears to remove the vlan tag before applying the filter and adjusting the ethernet header.
 * Therefore the used filter only checks if the ethernet type is as expected.
 * When that would not be the case the following filter could be used instead.
 * Alternative filter: ether proto port or (ether proto 0x8100 and ether[16:2] == port)
 *
 * BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 12),                         ldh [12] - load half word from frame offset 12, which is the ethernet type
 * BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, etype, 3, 0),               jeq #0x1ce8 - equal to port goto accept
 * BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, DDSI_ETHERTYPE_VLAN, 0, 3), jeq #0x8100 - mot equal 802.1Q vlan protocol goto drop
 * BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 16),                         ldh [16] - load half word at offset 16
 * BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, etype, 0, 1),               jne #0x1ce8 - not equal to port goto drop
 * BPF_STMT(BPF_RET+BPF_K, (u_int)-1),                         accept: ret #-1 - accept packet
 * BPF_STMT(BPF_RET+BPF_K, 0),                                 drop: ret #0 - drop packet
 *
 */
static dds_return_t ddsi_raweth_set_filter (struct ddsi_tran_factory * fact, ddsrt_socket_t sock, uint32_t port)
{
  ushort etype = (ushort)(port & 0xFFFF);
  struct sock_filter code[] = {
    BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 12),           // ldh [12] - load half word from frame offset 12, which is the ethernet type
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, etype, 0, 1), // jne #0x1ce8 - not equal to port goto drop
    BPF_STMT(BPF_RET+BPF_K, (u_int)-1),           // accept: ret #-1 - accept packet
    BPF_STMT(BPF_RET+BPF_K, 0),                   // drop: ret #0 - drop packet
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
  ddsrt_socket_ext_init (&uc->m_sockext, sock);
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

  DDS_CTRACE (&fact->gv->logconfig, "ddsi_raweth_create_conn %s socket %d port %u\n", mcast ? "multicast" : "unicast", uc->m_sockext.sock, uc->m_base.m_base.m_port);
  *conn_out = &uc->m_base;
  return DDS_RETCODE_OK;
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

#elif DDSI_USE_BSD

#define DEFAULT_BUFFER_SIZE 1000000

struct ddsi_vlan_tag {
  unsigned short tag;
  unsigned short proto;
};

/* The ddsi_raweth_conn_read reads from the bpf file descriptor.
 * The read copies the contents of the kernel bpf buffer to a buffer maintained
 * by this raweth transport. The buffer that is returned may contain several ethernet frames.
 * Each ethernet frame is preceded by a header (bpf_hdr) which contains the following fields:
 * - struct bpf_ts bh tstamp : timestamp
 * - uint32_t bh_captlen     : captured length
 * - uint32_t bh_datalen     : length of the captured frame
 * - ushort bh_hdrlen        : length of this header including alignment
 * Each ddsi_raweth_conn_read will return one packet from the buffer. When the buffer has become empty
 * then the buffer is filled again by reading from the bpf file descriptor.
 * This bpf_header is provided by the kernel therefore we can trust the contents of these fields and
 * the manipulations using the field to obtain the next packet in the buffer can be safely done.
 */

static ssize_t ddsi_raweth_conn_read (struct ddsi_tran_conn * conn, unsigned char * buf, size_t len, bool allow_spurious, struct ddsi_network_packet_info *pktinfo)
{
  ssize_t ret  = 0;
  dds_return_t rc = DDS_RETCODE_OK;
  ddsi_raweth_conn_t uc = (ddsi_raweth_conn_t) conn;
  struct bpf_hdr *bpf_hdr;
  struct ddsi_ethernet_header *eth_hdr;
  struct ddsi_vlan_tag *vtag = NULL;
  char *ptr;
  (void) allow_spurious;

  ddsrt_mutex_lock (&uc->lock);

  if (uc->avail == 0)
  {
    if ((ret = read(uc->m_sockext.sock, uc->buffer, uc->buflen)) <= 0)
    {
      DDS_CERROR (&conn->m_base.gv->logconfig, "ddsi_raweth_create_conn read failed ... retcode = %"PRIdSIZE"\n", ret);
      rc = DDS_RETCODE_ERROR;
      goto error;
    }
    uc->avail = ret;
    uc->bptr = uc->buffer;
  }

  if (uc->bptr < uc->buffer + uc->avail)
  {
    ptr = uc->bptr;
    bpf_hdr = (struct bpf_hdr *) ptr;
    ptr += bpf_hdr->bh_hdrlen;

    eth_hdr = (struct ddsi_ethernet_header *)ptr;
    ptr += sizeof(*eth_hdr);

    if (bpf_hdr->bh_datalen == bpf_hdr->bh_caplen)
    {
      ret = (ssize_t)(bpf_hdr->bh_datalen - sizeof(struct ddsi_ethernet_header));
      if (ntohs(eth_hdr->proto) == ETHERTYPE_VLAN)
      {
        vtag = (struct ddsi_vlan_tag *)ptr;
        ptr += sizeof(*vtag);
        ret -= (ssize_t)sizeof(*vtag);
      }
      if ((size_t)ret <= len)
      {
        memcpy(buf, ptr, (size_t)ret);
        if (pktinfo)
          set_pktinfo(pktinfo, eth_hdr->smac, ntohs (eth_hdr->proto), (vtag ? ntohs(vtag->tag) : 0));
      }
      else
      {
        char addrbuf[DDSI_LOCSTRLEN];
        (void) snprintf(addrbuf, sizeof(addrbuf), "[%02x:%02x:%02x:%02x:%02x:%02x]:%u",
                  eth_hdr->smac[0], eth_hdr->smac[1], eth_hdr->smac[2],
                  eth_hdr->smac[3], eth_hdr->smac[4], eth_hdr->smac[5], vtag ? ntohs(vtag->proto) : ntohs(eth_hdr->proto));
        DDS_CWARNING(&conn->m_base.gv->logconfig, "%s => %d truncated to %d\n", addrbuf, (int)ret, (int)len);
        rc = DDS_RETCODE_ERROR;
        goto error;
      }
    }
    // else drop packet because it was truncated thus exceeded buffer size.

    uc->bptr += BPF_WORDALIGN(bpf_hdr->bh_hdrlen + bpf_hdr->bh_caplen);
    if (uc->bptr >= uc->buffer + uc->avail)
      uc->avail = 0;
  }
  else
    uc->avail = 0;

error:
  ddsrt_mutex_unlock (&uc->lock);
  return (rc == DDS_RETCODE_OK ? ret : -1);;
}

static ssize_t ddsi_raweth_conn_write (struct ddsi_tran_conn * conn, const ddsi_locator_t *dst, const ddsi_tran_write_msgfrags_t *msgfrags, uint32_t flags)
{
  ddsi_raweth_conn_t uc = (ddsi_raweth_conn_t) conn;
  dds_return_t rc = DDS_RETCODE_OK;
  ssize_t ret = -1;
  struct ddsi_vlan_header vhdr;
  size_t hdrlen;
  (void) flags;

  assert(msgfrags->niov <= INT_MAX - 1); // we'll be adding one later on

  hdrlen = set_ethernet_header(&vhdr, (uint16_t) uc->m_base.m_base.m_port, dst, &uc->m_base.m_base.gv->interfaces[0].loc);

  DDSRT_STATIC_ASSERT(DDSI_TRAN_RESERVED_IOV_SLOTS >= 1);

  ddsrt_iovec_t * const iovs = (ddsrt_iovec_t *) &msgfrags->tran_reserved[DDSI_TRAN_RESERVED_IOV_SLOTS - 1];
  iovs[0].iov_base = &vhdr;
  iovs[0].iov_len = hdrlen;

  if ((ret = writev (uc->m_sockext.sock, iovs, (int)(msgfrags->niov + 1))) < 0)
  {
      DDS_CERROR(&conn->m_base.gv->logconfig, "ddsi_raweth_conn_write failed with retcode %"PRIdSIZE, ret);
      rc = DDS_RETCODE_ERROR;
  }

  return (rc == DDS_RETCODE_OK ? ret : -1);
}

static dds_return_t ddsi_raweth_set_filter (struct ddsi_tran_factory * fact, ddsrt_socket_t sock, uint32_t port)
{
  int r;
  ushort etype = (ushort)(port & 0xFFFF);
  struct bpf_insn insns[] = {
    BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 12),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, etype, 3, 0),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, ETHERTYPE_VLAN, 0, 3),
    BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 16),
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, etype, 0, 1),
    BPF_STMT(BPF_RET+BPF_K, (u_int)-1),
    BPF_STMT(BPF_RET+BPF_K, 0),
  };
  unsigned flen = sizeof (insns)/sizeof (struct bpf_insn);
  struct bpf_program filter = {flen, insns};

  if ((r = ioctl (sock, BIOCSETF, &filter)) == -1 ) {
    ddsrt_close (sock);
    DDS_CERROR (&fact->gv->logconfig, "ddsrt_setsockopt attach filter for protocol %u failed ... retcode = %d\n", port, r);
    return DDS_RETCODE_ERROR;
  }
  return DDS_RETCODE_OK;
}

static dds_return_t ddsi_raweth_create_conn (struct ddsi_tran_conn **conn_out, struct ddsi_tran_factory * fact, uint32_t port, const struct ddsi_tran_qos *qos)
{
  int r;
  int i;
  dds_return_t rc;
  ddsrt_socket_t sock = -1;
  ddsi_raweth_conn_t uc = NULL;
  struct sockaddr_dl addr = {0};
  bool mcast = (qos->m_purpose == DDSI_TRAN_QOS_RECV_MC);
  struct ddsi_domaingv const * const gv = fact->gv;
  struct ddsi_network_interface const * const intf = qos->m_interface ? qos->m_interface : &gv->interfaces[0];
  uint32_t buflen;

  if (port == 0 || port > 65535)
  {
    DDS_CERROR (&fact->gv->logconfig, "ddsi_raweth_create_conn %s port %u - using port number as ethernet type, %u won't do\n", mcast ? "multicast" : "unicast", port, port);
    return DDS_RETCODE_ERROR;
  }

#if defined(DDSI_BPF_IS_CLONING_DEV)
  sock = open ("/dev/bpf", O_RDWR);
#else
  for (i = 0; i < 100; ++i) {
    char name[11] = {0};
    snprintf (name, sizeof (name), "/dev/bpf%d", i);
    sock = open (name, O_RDWR);
    if (sock >=0)
      break;
  }
#endif

  if (sock < 0)
  {
    DDS_CERROR (&fact->gv->logconfig, "ddsi_raweth_create_conn %s port %u failed ... retcode = %d\n", mcast ? "multicast" : "unicast", port, sock);
    return DDS_RETCODE_ERROR;
  }

  // activate immediate mode (therefore, buf_len is initially set to "1")
  int mode = 1;
  if ((r = ioctl (sock, BIOCIMMEDIATE, &mode)) == -1 ) {
    ddsrt_close (sock);
    DDS_CERROR (&fact->gv->logconfig, "ddsi_raweth_create_conn %s port %u failed ... retcode = %d\n", mcast ? "multicast" : "unicast", port, r);
    return DDS_RETCODE_ERROR;
  }

  buflen = gv->config.socket_rcvbuf_size.max.value;
  if (buflen == 0)
    buflen = DEFAULT_BUFFER_SIZE;
  if ((r = ioctl (sock, BIOCSBLEN, &buflen)) < 0)
  {
    ddsrt_close (sock);
    DDS_CERROR (&fact->gv->logconfig, "ddsi_raweth_create_conn %s port %u failed ... retcode = %d\n", mcast ? "multicast" : "unicast", port, r);
    return DDS_RETCODE_ERROR;
  }

  struct ifreq bound_if;
  strcpy(bound_if.ifr_name, intf->name);
  if ((r = ioctl (sock, BIOCSETIF, &bound_if)) > 0) {
    ddsrt_close (sock);
    DDS_CERROR (&fact->gv->logconfig, "ddsi_raweth_create_conn %s port %u failed ... retcode = %d\n", mcast ? "multicast" : "unicast", port, r);
    return DDS_RETCODE_ERROR;
  }

  if ((r = ioctl (sock, BIOCPROMISC, &mode)) == -1 ) {
    ddsrt_close (sock);
    DDS_CERROR (&fact->gv->logconfig, "ddsi_raweth_create_conn %s port %u failed ... retcode = %d\n", mcast ? "multicast" : "unicast", port, r);
    return DDS_RETCODE_ERROR;
  }

#if defined(__FreeBSD__)
  uint32_t direction = BPF_D_IN;
  if ((r = ioctl (sock, BIOCGDIRECTION, &direction)) == -1 ) {
    ddsrt_close (sock);
    DDS_CWARNING (&fact->gv->logconfig, "ddsi_raweth_create_conn %s port %u could not set direction ... retcode = %d\n", mcast ? "multicast" : "unicast", port, r);
  }
#elif defined(__QNXNTO__) || defined(__APPLE__)
  uint32_t direction = 0;
  if ((r = ioctl (sock, BIOCSSEESENT, &direction)) == -1 ) {
    ddsrt_close (sock);
    DDS_CWARNING (&fact->gv->logconfig, "ddsi_raweth_create_conn %s port %u could not set direction ... retcode = %d\n", mcast ? "multicast" : "unicast", port, r);
  }
#endif

  rc = ddsi_raweth_set_filter (fact, sock, port);
  if (rc != DDS_RETCODE_OK)
  {
    ddsrt_close(sock);
    DDS_CERROR (&fact->gv->logconfig, "ddsi_raweth_create_conn %s set filter failed ... retcode = %d\n", mcast ? "multicast" : "unicast", rc);
    return rc;
  }

  if ((uc = (ddsi_raweth_conn_t) ddsrt_malloc (sizeof (*uc))) == NULL)
  {
    ddsrt_close(sock);
    return DDS_RETCODE_ERROR;
  }

  memset (uc, 0, sizeof (*uc));
  uc->m_sockext.sock = sock;
  uc->m_ifindex = addr.sdl_index;
  ddsi_factory_conn_init (fact, intf, &uc->m_base);
  uc->m_base.m_base.m_port = port;
  uc->m_base.m_base.m_trantype = DDSI_TRAN_CONN;
  uc->m_base.m_base.m_multicast = mcast;
  uc->m_base.m_base.m_handle_fn = ddsi_raweth_conn_handle;
  uc->m_base.m_locator_fn = ddsi_raweth_conn_locator;
  uc->m_base.m_read_fn = ddsi_raweth_conn_read;
  uc->m_base.m_write_fn = ddsi_raweth_conn_write;
  uc->m_base.m_disable_multiplexing_fn = 0;
  uc->buffer = ddsrt_malloc(buflen);
  uc->buflen = buflen;
  uc->bptr = uc->buffer;
  uc->avail = 0;
  ddsrt_mutex_init (&uc->lock);

  DDS_CTRACE (&fact->gv->logconfig, "ddsi_raweth_create_conn %s socket %d port %u\n", mcast ? "multicast" : "unicast", uc->m_sockext.sock, uc->m_base.m_base.m_port);
  *conn_out = &uc->m_base;

  return DDS_RETCODE_OK;
}

static int joinleave_asm_mcgroup (ddsrt_socket_t socket, int join, const ddsi_locator_t *mcloc, const struct ddsi_network_interface *interf)
{
  int rc = DDS_RETCODE_OK;
  (void)socket;
  (void)join;
  (void)mcloc;
  (void)interf;

  return rc;
}
#endif

static ddsrt_socket_t ddsi_raweth_conn_handle (struct ddsi_tran_base * base)
{
  return ((ddsi_raweth_conn_t) base)->m_sockext.sock;
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
  if (uc->m_sockext.sock != DDSRT_INVALID_SOCKET)
  {
    loc->kind = DDSI_LOCATOR_KIND_RAWETH;
    loc->port = uc->m_base.m_base.m_port;
    memcpy(loc->address, uc->m_base.m_base.gv->interfaces[0].loc.address, sizeof (loc->address));
    ret = 0;
  }
  return ret;
}

static int isbroadcast(const ddsi_locator_t *loc)
{
  int i;
  for(i = 0; i < 6; i++)
    if (loc->address[10 + i] != 0xff)
      return 0;
  return 1;
}

static int ddsi_raweth_join_mc (struct ddsi_tran_conn * conn, const ddsi_locator_t *srcloc, const ddsi_locator_t *mcloc, const struct ddsi_network_interface *interf)
{
  if (isbroadcast(mcloc))
    return 0;
  else
  {
    ddsi_raweth_conn_t uc = (ddsi_raweth_conn_t) conn;
    (void)srcloc;
    return joinleave_asm_mcgroup(uc->m_sockext.sock, 1, mcloc, interf);
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
    return joinleave_asm_mcgroup(uc->m_sockext.sock, 0, mcloc, interf);
  }
}

static void ddsi_raweth_release_conn (struct ddsi_tran_conn * conn)
{
  ddsi_raweth_conn_t uc = (ddsi_raweth_conn_t) conn;
  DDS_CTRACE (&conn->m_base.gv->logconfig,
              "ddsi_raweth_release_conn %s socket %d port %d\n",
              conn->m_base.m_multicast ? "multicast" : "unicast",
              uc->m_sockext.sock,
              uc->m_base.m_base.m_port);
  ddsrt_socket_ext_fini (&uc->m_sockext);
  ddsrt_close (uc->m_sockext.sock);
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
  int afs[] = { DDSI_LINK_FAMILY, DDSRT_AF_TERM };
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

  if (sockaddr->sa_family != DDSI_LINK_FAMILY)
    return -1;

  loc->kind = DDSI_LOCATOR_KIND_RAWETH;
  loc->port = DDSI_LOCATOR_PORT_INVALID;
  memset (loc->address, 0, 10);
#if defined(__linux)
  memcpy (loc->address + 10, ((struct sockaddr_ll *) sockaddr)->sll_addr, 6);
#elif DDSI_USE_BSD
  {
    struct sockaddr_dl *sa = ((struct sockaddr_dl *) sockaddr);
    memcpy (loc->address + 10, sa->sdl_data + sa->sdl_nlen, 6);
  }
#endif
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
