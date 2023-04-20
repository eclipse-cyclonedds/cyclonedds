// Copyright(c) 2019 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__PORTMAPPING_H
#define DDSI__PORTMAPPING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "dds/ddsi/ddsi_portmapping.h"

#if defined (__cplusplus)
extern "C" {
#endif

enum ddsi_port {
  DDSI_PORT_MULTI_DISC,
  DDSI_PORT_MULTI_DATA,
  DDSI_PORT_UNI_DISC,
  DDSI_PORT_UNI_DATA
};

struct ddsi_config;

/** @component port_mapping */
bool ddsi_valid_portmapping (const struct ddsi_config *config, int32_t participant_index, char *msg, size_t msgsize);

/** @component port_mapping */
uint32_t ddsi_get_port (const struct ddsi_config *config, enum ddsi_port which, int32_t participant_index);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__PORTMAPPING_H */
