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

#include <string.h>

#include "dds/ddsrt/events.h"
#include "dds/ddsrt/heap.h"

void ddsrt_event_socket_null(ddsrt_event_t* ev)
{
  ev->type = DDSRT_EVENT_TYPE_UNSET;
  ev->flags = DDSRT_EVENT_FLAG_UNSET;
  memset(&ev->u, 0x0, sizeof(ev->u));
  ddsrt_atomic_st32(&ev->triggered, DDSRT_EVENT_FLAG_UNSET);
  ev->parent = NULL;
}

void ddsrt_event_socket_init(ddsrt_event_t* ev, ddsrt_socket_t sock, uint32_t flags) 
{
  ev->type = DDSRT_EVENT_TYPE_SOCKET;
  ev->flags = flags;
  ev->u.socket.sock = sock;
  ddsrt_atomic_st32(&ev->triggered, DDSRT_EVENT_FLAG_UNSET);
  ev->parent = NULL;
}
