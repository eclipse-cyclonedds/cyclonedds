/*
 * Copyright(c) 2022 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef EVENTLIST_H
#define EVENTLIST_H

#include <stddef.h>
#include <sys/types.h>
#include <sys/event.h>

struct eventlist {
  size_t size;
  struct {
    struct kevent embedded[ DDSRT_EMBEDDED_EVENTS ];
    struct kevent *dynamic;
  } events;
};

#endif // EVENTSET_H
