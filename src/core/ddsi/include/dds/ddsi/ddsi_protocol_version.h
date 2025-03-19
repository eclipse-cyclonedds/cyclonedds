// Copyright(c) 2025 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_PROTOCOL_VERSION_H
#define DDSI_PROTOCOL_VERSION_H

#include <stdint.h>
#include "dds/export.h"

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct ddsi_protocol_version {
  uint8_t major, minor;
} ddsi_protocol_version_t;

/* This implements DDSI 2.5, accepts 2.1 and later */
#define DDSI_RTPS_MAJOR 2
#define DDSI_RTPS_MINOR_LATEST 5
#define DDSI_RTPS_MINOR_MINIMUM 1

#if defined (__cplusplus)
}
#endif

#endif
