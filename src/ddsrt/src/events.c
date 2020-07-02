/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include "dds/ddsrt/events.h"
#include "dds/ddsrt/heap.h"

dds_return_t ddsrt_event_socket_init(ddsrt_event_t* ev, ddsrt_socket_t sock, uint32_t flags) 
{
  ev->type = DDSRT_EVENT_TYPE_SOCKET;
  ev->flags = flags;
  ev->data.socket.sock = sock;
  ddsrt_atomic_st32(&ev->triggered, DDSRT_EVENT_FLAG_UNSET);
  return DDS_RETCODE_OK;
}
