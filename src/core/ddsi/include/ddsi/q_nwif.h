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
#ifndef Q_NWIF_H
#define Q_NWIF_H

#include "os/os_socket.h"
#include "ddsi/q_protocol.h" /* for nn_locator_t */

#if defined (__cplusplus)
extern "C" {
#endif

#define MAX_INTERFACES 128
struct nn_interface {
  nn_locator_t loc;
  nn_locator_t netmask;
  unsigned if_index;
  unsigned mc_capable: 1;
  unsigned point_to_point: 1;
  char *name;
};

int make_socket (os_socket *socket, unsigned short port, bool stream, bool reuse);
int find_own_ip (const char *requested_address);
unsigned locator_to_hopefully_unique_uint32 (const nn_locator_t *src);

#if defined (__cplusplus)
}
#endif

#endif /* Q_NWIF_H */
