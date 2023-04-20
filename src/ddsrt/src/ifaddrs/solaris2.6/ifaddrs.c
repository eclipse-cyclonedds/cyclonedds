// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <errno.h>
#include "dds/ddsrt/ifaddrs.h"
#include <string.h>
#include <stdio.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/string.h"

extern const int *const os_supp_afs;

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <stdlib.h>

dds_return_t
ddsrt_getifaddrs(
  ddsrt_ifaddrs_t **ret_ifap,
  const int *afs)
{
  int sock;
  char *buf;
  int32_t n;
  struct ifconf ifc;
  struct ifreq *ifr;

  /* get interfaces */
  buf = ddsrt_malloc (8192);
  memset (&ifc, 0, sizeof (ifc));
  ifc.ifc_len = 8192;
  ifc.ifc_buf = buf;
  sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (ioctl (sock, SIOCGIFCONF, (char *) &ifc) < 0)
  {
    perror ("ioctl (SIOCGIFCONF)");
    exit (77);
  }

  ddsrt_ifaddrs_t **ifap, *ifa_root;
  ifap = &ifa_root; ifa_root = NULL;
  ifr = ifc.ifc_req;
  for (n = ifc.ifc_len / sizeof (struct ifreq); --n >= 0; ifr++)
  {
    ddsrt_ifaddrs_t *ifa;
    if (ifr->ifr_name[0] == '\0')
      continue; /* Forget about anonymous network devices */

    if (ifr->ifr_addr.sa_family != AF_INET) {
      printf ("%s: not INET\n", ifr->ifr_name);
      continue;
    }

    ifa = ddsrt_malloc (sizeof (*ifa));
    memset (ifa, 0, sizeof (*ifa));
    ifa->index = (int) (ifr - ifc.ifc_req);
    ifa->flags = IFF_UP | ((strcmp(ifr->ifr_name, "lo0")==0) ? IFF_LOOPBACK : IFF_MULTICAST);
    ifa->name = strdup (ifr->ifr_name);
    ifa->addr = ddsrt_memdup (&ifr->ifr_addr, sizeof (struct sockaddr_in));
    ifa->netmask = ddsrt_memdup (&ifr->ifr_addr, sizeof (struct sockaddr_in));
    ((struct sockaddr_in *) ifa->netmask)->sin_addr.s_addr = htonl (0xffffff00);
    ifa->broadaddr = NULL;
    ifa->next = NULL;
    *ifap = ifa;
    ifap = &ifa->next;
  }

  ddsrt_free (buf);
  close (sock);
  *ret_ifap = ifa_root;
  return DDS_RETCODE_OK;
}
