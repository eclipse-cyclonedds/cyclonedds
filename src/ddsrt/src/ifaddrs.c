// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/ifaddrs.h"

void
ddsrt_freeifaddrs(ddsrt_ifaddrs_t *ifa)
{
  ddsrt_ifaddrs_t *next;

  while (ifa != NULL) {
    next = ifa->next;
    ddsrt_free(ifa->name);
    ddsrt_free(ifa->addr);
    ddsrt_free(ifa->netmask);
    ddsrt_free(ifa->broadaddr);
    ddsrt_free(ifa);
    ifa = next;
  }
}

