// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__UDP_H
#define DDSI__UDP_H

#if defined (__cplusplus)
extern "C" {
#endif

struct in_addr;
struct ddsi_domaingv;

typedef struct ddsi_udpv4mcgen_address {
  /* base IPv4 MC address is ipv4, host bits are bits base .. base+count-1, this machine is bit idx */
  struct in_addr ipv4;
  uint8_t base;
  uint8_t count;
  uint8_t idx; /* must be last: then sorting will put them consecutively */
} ddsi_udpv4mcgen_address_t;

/** @component udp_transport */
int ddsi_udp_init (struct ddsi_domaingv *gv);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__UDP_H */
