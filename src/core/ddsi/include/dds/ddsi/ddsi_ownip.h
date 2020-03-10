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
#ifndef DDSI_OWNIP_H
#define DDSI_OWNIP_H

#include <stdbool.h>

#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsi/q_protocol.h" /* for nn_locator_t */

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_domaingv;

#define MAX_INTERFACES 128
struct nn_interface {
  nn_locator_t loc;
  nn_locator_t netmask;
  uint32_t if_index;
  unsigned mc_capable: 1;
  unsigned mc_flaky: 1;
  unsigned point_to_point: 1;
  char *name;
};

int find_own_ip (struct ddsi_domaingv *gv, const char *requested_address);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_OWNIP_H */
