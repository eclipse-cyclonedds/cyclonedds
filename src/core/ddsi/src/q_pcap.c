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
#include <stdio.h>
#include <assert.h>

#include "os/os.h"

#include "ddsi/q_log.h"
#include "ddsi/q_time.h"
#include "ddsi/q_config.h"
#include "ddsi/q_globals.h"
#include "ddsi/q_bswap.h"
#include "ddsi/q_pcap.h"

/* pcap format info taken from http://wiki.wireshark.org/Development/LibpcapFileFormat */

#define LINKTYPE_RAW 101 /* Raw IP; the packet begins with an IPv4 or IPv6 header, with the "version" field of the header indicating whether it's an IPv4 or IPv6 header. */

typedef struct pcap_hdr_s {
  uint32_t magic_number;   /* magic number */
  uint16_t version_major;  /* major version number */
  uint16_t version_minor;  /* minor version number */
  int32_t  thiszone;       /* GMT to local correction */
  uint32_t sigfigs;        /* accuracy of timestamps */
  uint32_t snaplen;        /* max length of captured packets, in octets */
  uint32_t network;        /* data link type */
} pcap_hdr_t;

typedef struct pcaprec_hdr_s {
  int32_t ts_sec;          /* timestamp seconds (orig: unsigned) */
  int32_t ts_usec;         /* timestamp microseconds (orig: unsigned) */
  uint32_t incl_len;       /* number of octets of packet saved in file */
  uint32_t orig_len;       /* actual length of packet */
} pcaprec_hdr_t;

typedef struct ipv4_hdr_s {
  unsigned char version_hdrlength;
  unsigned char diffserv_congestion;
  uint16_t totallength;
  uint16_t identification;
  uint16_t flags_fragment_offset;
  unsigned char ttl;
  unsigned char proto;
  uint16_t checksum;
  uint32_t srcip;
  uint32_t dstip;
} ipv4_hdr_t;

typedef struct udp_hdr_s {
  uint16_t srcport;
  uint16_t dstport;
  uint16_t length;
  uint16_t checksum;
} udp_hdr_t;

static const ipv4_hdr_t ipv4_hdr_template = {
  (4 << 4) | 5, /* IPv4, minimum header length */
  0 | 0,        /* no diffserv, congestion */
  0,            /* total length will be overridden */
  0,            /* not fragmenting, so irrelevant */
  0,            /* not fragmenting */
  0,            /* TTL: received has it at 128, sent at 255 */
  17,           /* UDP */
  0,            /* checksum will be overridden */
  0,            /* srcip: set per packet */
  0             /* dstip: set per packet */
};

#define IPV4_HDR_SIZE 20
#define UDP_HDR_SIZE 8

OS_WARNING_MSVC_OFF(4996);
FILE *new_pcap_file (const char *name)
{
  FILE *fp;
  pcap_hdr_t hdr;

  if ((fp = fopen (name, "wb")) == NULL)
  {
    DDS_WARNING ("packet capture disabled: file %s could not be opened for writing\n", name);
    return NULL;
  }

  hdr.magic_number = 0xa1b2c3d4;
  hdr.version_major = 2;
  hdr.version_minor = 4;
  hdr.thiszone = 0;
  hdr.sigfigs = 0;
  hdr.snaplen = 65535;
  hdr.network = LINKTYPE_RAW;
  fwrite (&hdr, sizeof (hdr), 1, fp);

  return fp;
}
OS_WARNING_MSVC_ON(4996);

static void write_data (FILE *fp, const struct msghdr *msghdr, size_t sz)
{
  size_t i, n = 0;
  for (i = 0; i < (size_t) msghdr->msg_iovlen && n < sz; i++)
  {
    size_t m1 = msghdr->msg_iov[i].iov_len;
    size_t m = (n + m1 <= sz) ? m1 : sz - n;
    fwrite (msghdr->msg_iov[i].iov_base, m, 1, fp);
    n += m;
  }
  assert (n == sz);
}

static uint16_t calc_ipv4_checksum (const uint16_t *x)
{
  uint32_t s = 0;
  int i;
  for (i = 0; i < 10; i++)
  {
    s += x[i];
  }
  s = (s & 0xffff) + (s >> 16);
  return (uint16_t) ~s;
}

void write_pcap_received
(
  FILE * fp,
  nn_wctime_t tstamp,
  const os_sockaddr_storage * src,
  const os_sockaddr_storage * dst,
  unsigned char * buf,
  size_t sz
)
{
  if (config.transport_selector == TRANS_UDP)
  {
    pcaprec_hdr_t pcap_hdr;
    union {
      ipv4_hdr_t ipv4_hdr;
      uint16_t x[10];
    } u;
    udp_hdr_t udp_hdr;
    size_t sz_ud = sz + UDP_HDR_SIZE;
    size_t sz_iud = sz_ud + IPV4_HDR_SIZE;
    os_mutexLock (&gv.pcap_lock);
    wctime_to_sec_usec (&pcap_hdr.ts_sec, &pcap_hdr.ts_usec, tstamp);
    pcap_hdr.incl_len = pcap_hdr.orig_len = (uint32_t) sz_iud;
    fwrite (&pcap_hdr, sizeof (pcap_hdr), 1, fp);
    u.ipv4_hdr = ipv4_hdr_template;
    u.ipv4_hdr.totallength = toBE2u ((unsigned short) sz_iud);
    u.ipv4_hdr.ttl = 128;
    u.ipv4_hdr.srcip = ((os_sockaddr_in*) src)->sin_addr.s_addr;
    u.ipv4_hdr.dstip = ((os_sockaddr_in*) dst)->sin_addr.s_addr;
    u.ipv4_hdr.checksum = calc_ipv4_checksum (u.x);
    fwrite (&u.ipv4_hdr, sizeof (u.ipv4_hdr), 1, fp);
    udp_hdr.srcport = ((os_sockaddr_in*) src)->sin_port;
    udp_hdr.dstport = ((os_sockaddr_in*) dst)->sin_port;
    udp_hdr.length = toBE2u ((unsigned short) sz_ud);
    udp_hdr.checksum = 0; /* don't have to compute a checksum for UDPv4 */
    fwrite (&udp_hdr, sizeof (udp_hdr), 1, fp);
    fwrite (buf, sz, 1, fp);
    os_mutexUnlock (&gv.pcap_lock);
  }
}

void write_pcap_sent
(
  FILE * fp,
  nn_wctime_t tstamp,
  const os_sockaddr_storage * src,
  const struct msghdr * hdr,
  size_t sz
)
{
  if (config.transport_selector == TRANS_UDP)
  {
    pcaprec_hdr_t pcap_hdr;
    union {
      ipv4_hdr_t ipv4_hdr;
      uint16_t x[10];
    } u;
    udp_hdr_t udp_hdr;
    size_t sz_ud = sz + UDP_HDR_SIZE;
    size_t sz_iud = sz_ud + IPV4_HDR_SIZE;
    os_mutexLock (&gv.pcap_lock);
    wctime_to_sec_usec (&pcap_hdr.ts_sec, &pcap_hdr.ts_usec, tstamp);
    pcap_hdr.incl_len = pcap_hdr.orig_len = (uint32_t) sz_iud;
    fwrite (&pcap_hdr, sizeof (pcap_hdr), 1, fp);
    u.ipv4_hdr = ipv4_hdr_template;
    u.ipv4_hdr.totallength = toBE2u ((unsigned short) sz_iud);
    u.ipv4_hdr.ttl = 255;
    u.ipv4_hdr.srcip = ((os_sockaddr_in*) src)->sin_addr.s_addr;
    u.ipv4_hdr.dstip = ((os_sockaddr_in*) hdr->msg_name)->sin_addr.s_addr;
    u.ipv4_hdr.checksum = calc_ipv4_checksum (u.x);
    fwrite (&u.ipv4_hdr, sizeof (u.ipv4_hdr), 1, fp);
    udp_hdr.srcport = ((os_sockaddr_in*) src)->sin_port;
    udp_hdr.dstport = ((os_sockaddr_in*) hdr->msg_name)->sin_port;
    udp_hdr.length = toBE2u ((unsigned short) sz_ud);
    udp_hdr.checksum = 0; /* don't have to compute a checksum for UDPv4 */
    fwrite (&udp_hdr, sizeof (udp_hdr), 1, fp);
    write_data (fp, hdr, sz);
    os_mutexUnlock (&gv.pcap_lock);
  }
}
