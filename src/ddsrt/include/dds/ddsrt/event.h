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
#ifndef DDSRT_EVENT_H
#define DDSRT_EVENT_H

#include "dds/export.h"
#include "dds/config.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/sync.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define DDSRT_READ (1u<<0)
#define DDSRT_WRITE (1u<<1)
#define DDSRT_IPV4_ADDED (1u<<2)
#define DDSRT_IPV4_DELETED (1u<<3)
#if DDSRT_HAVE_IPV6
# define DDSRT_IPV6_ADDED (1u<<4)
# define DDSRT_IPV6_DELETED (1u<<5)
#endif
#define DDSRT_LINK_UP (1u<<6)
#define DDSRT_LINK_DOWN (1u<<7)

#if DDSRT_HAVE_NETLINK_EVENT
// events are socket events by default
# define DDSRT_NETLINK (1u<<31)
#endif
// more event types to follow, e.g. TIMER_EVENT, SIGNAL_EVENT

#define DDSRT_RUN_ONCE (1u<<0)

typedef struct ddsrt_event ddsrt_event_t;
struct ddsrt_event;

typedef struct ddsrt_loop ddsrt_loop_t;
struct ddsrt_loop;

typedef dds_return_t(*ddsrt_event_callback_t)(
  ddsrt_event_t *event, uint32_t flags, const void *data, void *user_data);

struct ddsrt_event {
  uint32_t flags;
  const ddsrt_loop_t *loop;
  ddsrt_event_callback_t callback;
  void *user_data;
  union {
    struct {
      ddsrt_socket_t socketfd;
    } socket;
#if DDSRT_HAVE_NETLINK_EVENT
    struct {
# if _WIN32
      ddsrt_socket_t pipefds[2];
      HANDLE address_handle;
      HANDLE interface_handle;
# else
      ddsrt_socket_t socketfd;
# endif
    } netlink;
#endif
  } source;
};

typedef struct ddsrt_netlink_message ddsrt_netlink_message_t;
struct ddsrt_netlink_message {
  uint32_t index;
  struct sockaddr_storage address; // zeroed out on LINK_UP/LINK_DOWN
};

typedef struct ddsrt_eventlist ddsrt_eventlist_t;
struct ddsrt_eventlist {
  size_t length; /**< number of slots available for use */
  size_t count; /**< number of slots currently in use */
  size_t start;
  size_t end;
  union {
    ddsrt_event_t *embedded[ DDSRT_EMBEDDED_EVENTS ];
    ddsrt_event_t **dynamic;
  } events;
};

#if _WIN32
typedef HANDLE ddsrt_epoll_t;
# define DDSRT_INVALID_EPOLL NULL
#else
typedef int ddsrt_epoll_t;
# define DDSRT_INVALID_EPOLL (-1)
#endif

struct ddsrt_loop {
  ddsrt_atomic_uint32_t terminate;
  ddsrt_socket_t pipefds[2];
  // owner field is used to avoid recursive deadlocks. owner is set atomically
  // when a thread starts the event loop and unset when it stops. operations
  // that modify the event queue check if the owner matches the identifier of
  // the calling thread and makes locking a no-op if it does
  ddsrt_atomic_uintptr_t owner; /**< thread identifier of dispatcher */
  ddsrt_mutex_t lock;
  ddsrt_cond_t condition;
  ddsrt_eventlist_t active;
  ddsrt_eventlist_t cancelled;
  // type-punned representation of eventlist used by event backend.
  union {
    char data[ DDSRT_SIZEOF_EVENTLIST ];
    void *align;
  } ready;
#if DDSRT_EVENT == DDSRT_EVENT_EPOLL
  ddsrt_epoll_t epollfd;
#elif DDSRT_EVENT == DDSRT_EVENT_KQUEUE
  int kqueuefd;
#else
  ddsrt_socket_t fdmax_plus_1;
  fd_set readfds;
  fd_set writefds;
#endif
};

DDS_EXPORT dds_return_t
ddsrt_create_event(
  ddsrt_event_t *event,
  ddsrt_socket_t socketfd,
  uint32_t flags,
  ddsrt_event_callback_t callback,
  void *user_data)
ddsrt_nonnull((1));

DDS_EXPORT dds_return_t
ddsrt_destroy_event(
  ddsrt_event_t *event);

DDS_EXPORT dds_return_t
ddsrt_handle_event(
  ddsrt_event_t *event,
  uint32_t flags,
  void *user_data)
ddsrt_nonnull((1));

DDS_EXPORT ddsrt_socket_t
ddsrt_event_socket(
  ddsrt_event_t *event)
ddsrt_nonnull_all;

DDS_EXPORT dds_return_t
ddsrt_add_event(
  ddsrt_loop_t *loop,
  ddsrt_event_t *event)
ddsrt_nonnull_all;

DDS_EXPORT dds_return_t
ddsrt_delete_event(
  ddsrt_loop_t *loop,
  ddsrt_event_t *event)
ddsrt_nonnull_all;

DDS_EXPORT dds_return_t
ddsrt_create_loop(
  ddsrt_loop_t *loop)
ddsrt_nonnull_all;

DDS_EXPORT void
ddsrt_destroy_loop(
  ddsrt_loop_t *loop);

DDS_EXPORT void
ddsrt_trigger_loop(
  ddsrt_loop_t *loop)
ddsrt_nonnull_all;

DDS_EXPORT dds_return_t
ddsrt_run_loop(
  ddsrt_loop_t *loop,
  uint32_t flags,
  void *user_data)
ddsrt_nonnull((1));

#if defined(__cplusplus)
}
#endif

#endif // DDSRT_EVENT_H
