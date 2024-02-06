// Copyright(c) 2019 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_PORTMAPPING_H
#define DDSI_PORTMAPPING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#if defined (__cplusplus)
extern "C" {
#endif

enum ddsi_port {
  DDSI_PORT_MULTI_DISC,
  DDSI_PORT_MULTI_DATA,
  DDSI_PORT_UNI_DISC,
  DDSI_PORT_UNI_DATA
};

struct ddsi_portmapping {
  uint32_t base;
  uint32_t dg;
  uint32_t pg;
  uint32_t d0;
  uint32_t d1;
  uint32_t d2;
  uint32_t d3;
};

/** @component port_mapping */
DDS_EXPORT bool ddsi_get_port_int (uint32_t *port, const struct ddsi_portmapping *map, enum ddsi_port which, uint32_t domain_id, int32_t participant_index, char *str_if_overflow, size_t strsize);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_PORTMAPPING_H */
