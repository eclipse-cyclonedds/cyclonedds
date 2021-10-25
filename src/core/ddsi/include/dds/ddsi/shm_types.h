/*
 * Copyright(c) 2021 Apex.AI Inc. All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#ifndef SHM_TYPES_H
#define SHM_TYPES_H

#include "dds/ddsi/q_protocol.h" /* for, e.g., SubmessageKind_t */
#include "dds/ddsi/ddsi_tran.h"
#include "dds/features.h"

#include "dds/ddsi/ddsi_keyhash.h"
#include "iceoryx_binding_c/chunk.h"

typedef enum {
  IOX_CHUNK_UNINITIALIZED,
  IOX_CHUNK_CONTAINS_RAW_DATA,
  IOX_CHUNK_CONTAINS_SERIALIZED_DATA
} iox_shm_data_state_t;

struct iceoryx_header {
   struct ddsi_guid guid;
   dds_time_t tstamp;
   uint32_t statusinfo;
   uint32_t data_size;
   unsigned char data_kind;
   ddsi_keyhash_t keyhash;
   iox_shm_data_state_t shm_data_state;
};

typedef struct iceoryx_header iceoryx_header_t;

iceoryx_header_t *iceoryx_header_from_chunk(void *iox_chunk);

#endif // SHM_TYPES_H