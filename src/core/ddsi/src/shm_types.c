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

#include "dds/ddsi/shm_types.h"

#ifdef DDS_HAS_SHM

iceoryx_header_t *iceoryx_header_from_chunk(void *iox_chunk) {
  iox_chunk_header_t *chunk_header =
      iox_chunk_header_from_user_payload(iox_chunk);
  return iox_chunk_header_to_user_header(chunk_header);
}

#endif