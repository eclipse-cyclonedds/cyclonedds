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

// Windows offers three (four?) flavours of event handling mechanisms.
//  1. select (or WSAPoll)
//  2. WSAWaitForMultipleEvents (WaitForMultipleObjects)
//  3. I/O Completion Ports
//  4. Windows Registered I/O
//
// select is notoriously slow on Windows, which is not a big problem if used
// for two udp sockets (discovery+data), but is a problem if tcp connections
// are used. WSAPoll is broken (1) up to Windows 10 version 2004 (2), which was
// released in May of 2020. WSAWaitForMultipleEvents is more performant, which
// is why it is used for Windows CE already, but only allows for
// WSA_MAXIMUM_WAIT_EVENTS (MAXIMUM_WAIT_OBJECTS, or 64) sockets to be polled
// simultaneously, which again may be a problem if tcp connections are used.
// select is also limited to 64 sockets unless FD_SETSIZE is defined to a
// higher number before including winsock2.h (3). For high-performance I/O
// on Windows, OVERLAPPED sockets in combination with I/O Completion Ports is
// recommended, but the interface is completely different from interfaces like
// epoll and kqueue (4). Zero byte receives can of course be used (5,6,7), but
// it seems suboptimal to do so(?) Asynchronous I/O, which is offered by the
// likes of I/O Completion Ports and io_uring, seems worthwile, but the
// changes seem a bit to substantial at this point.
//
// OPTION #5: wepoll, epoll for windows (8)
//
// wepoll implements the epoll API for Windows using the Ancillart Function
// Driver, i.e. Winsock. wepoll was developed by one of the libuv authors (9)
// and is used by libevent (10,11) and ZeroMQ (12).
//
// 1: https://daniel.haxx.se/blog/2012/10/10/wsapoll-is-broken/
// 2: https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsapoll
// 3: https://docs.microsoft.com/en-us/windows/win32/winsock/maximum-number-of-sockets-supported-2
// 4: https://sudonull.com/post/14582-epoll-and-Windows-IO-Completion-Ports-The-Practical-Difference
// 5: https://stackoverflow.com/questions/49970454/zero-byte-receives-purpose-clarification
// 6: https://stackoverflow.com/questions/10635976/iocp-notifications-without-bytes-copy
// 7: https://stackoverflow.com/questions/24434289/select-equivalence-in-i-o-completion-ports
// 8: https://github.com/piscisaureus/wepoll
// 9: https://news.ycombinator.com/item?id=15978372
// 10: https://github.com/libevent/libevent/pull/1006
// 11: https://libev.schmorp.narkive.com/tXCCS0na/better-windows-backend-using-wepoll
// 12: https://github.com/zeromq/libzmq/pull/3127

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#if !_WIN32
# include <fcntl.h>
#endif

#include "event.h"
#include "eventlist.h"
#include "dds/ddsrt/static_assert.h"

dds_return_t
ddsrt_add_event(ddsrt_loop_t *loop, ddsrt_event_t *event)
{
  dds_return_t err;
  ddsrt_socket_t fd = DDSRT_INVALID_SOCKET;
  bool release = true;
  struct epoll_event ev = { .events = 0u, { .ptr = event } };

  assert(loop);
  assert(event);

  if (event->loop)
    return event->loop == loop ? DDS_RETCODE_OK : DDS_RETCODE_BAD_PARAMETER;

  fd = event_socket(event);
  release = lock_loop(loop);

  if ((err = add_event(&loop->active, event, INT_MAX - 1)))
    goto err_event;
  if (event->flags & READ_FLAGS)
    ev.events |= EPOLLIN;
  if (event->flags & DDSRT_WRITE)
    ev.events |= EPOLLOUT;
  if (epoll_ctl(loop->epollfd, EPOLL_CTL_ADD, fd, &ev) == -1)
    goto err_epoll;

  event->loop = loop;
  unlock_loop(loop, release);
  return DDS_RETCODE_OK;
err_epoll:
  err = (errno == ENOMEM)
    ? DDS_RETCODE_OUT_OF_RESOURCES : DDS_RETCODE_BAD_PARAMETER;
  delete_event(&loop->active, event);
err_event:
  unlock_loop(loop, release);
  return err;
}

dds_return_t
ddsrt_delete_event(ddsrt_loop_t *loop, ddsrt_event_t *event)
{
  dds_return_t err;
  bool release = true;
  uint64_t owner;

  assert(loop);
  assert(event);

  if (event->loop != loop)
    return DDS_RETCODE_BAD_PARAMETER;

  release = lock_loop(loop);
  // remove descriptor from epoll instance immediately to avoid
  // having to retrieve the socket from possibly freed memory
  ddsrt_socket_t fd = ddsrt_event_socket(event);
  if (epoll_ctl(loop->epollfd, EPOLL_CTL_DEL, fd, NULL) == -1)
    goto err_epoll;

  if ((owner = ddsrt_atomic_ldptr(&loop->owner))) {
    if ((err = add_event(&loop->cancelled, event, INT_MAX - 1)))
      goto err_event;
    wait_for_loop(loop, release);
  } else {
    delete_event(&loop->active, event);
  }

  event->loop = NULL;
  unlock_loop(loop, release);
  return DDS_RETCODE_OK;
err_epoll:
  err = (errno == ENOMEM)
    ? DDS_RETCODE_OUT_OF_RESOURCES : DDS_RETCODE_BAD_PARAMETER;
err_event:
  unlock_loop(loop, release);
  return err;
}

#if !_WIN32
# define epoll_close(x) close(x)
#endif

dds_return_t
ddsrt_create_loop(ddsrt_loop_t *loop)
{
  struct eventlist *evset;
  ddsrt_socket_t pipefds[2];
  ddsrt_epoll_t epollfd = DDSRT_INVALID_EPOLL;
  int flags = 0;
  struct epoll_event ev = { .events = EPOLLIN, { .ptr = NULL } };

#if !_WIN32
  flags = EPOLL_CLOEXEC;
#endif

  assert(loop);

  DDSRT_STATIC_ASSERT(sizeof(loop->ready) == sizeof(struct eventlist));
  if ((epollfd = epoll_create1(flags)) == DDSRT_INVALID_EPOLL)
    goto err_epoll;
  ev.data.ptr = loop;
  if (open_pipe(pipefds) == -1)
    goto err_pipe;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, pipefds[0], &ev) == -1)
    goto err_epoll_ctl;
  ddsrt_atomic_st32(&loop->terminate, 0u);
  ddsrt_atomic_stptr(&loop->owner, 0u);
  loop->epollfd = epollfd;
  loop->pipefds[0] = pipefds[0];
  loop->pipefds[1] = pipefds[1];
  evset = (struct eventlist *)&loop->ready;
  evset->size = DDSRT_EMBEDDED_EVENTS;
  create_eventlist(&loop->active);
  create_eventlist(&loop->cancelled);
  ddsrt_mutex_init(&loop->lock);
  ddsrt_cond_init(&loop->condition);
  return DDS_RETCODE_OK;
err_epoll_ctl:
err_pipe:
  epoll_close(epollfd);
err_epoll:
  return DDS_RETCODE_OUT_OF_RESOURCES;
}

void
ddsrt_destroy_loop(ddsrt_loop_t *loop)
{
  struct eventlist *evset;

  assert(loop);
  assert(ddsrt_atomic_ldptr(&loop->owner) == 0u);
  close_pipe(loop->pipefds);
  ddsrt_cond_destroy(&loop->condition);
  ddsrt_mutex_destroy(&loop->lock);
  evset = (struct eventlist *)&loop->ready;
  if (evset->size > DDSRT_EMBEDDED_EVENTS)
    ddsrt_free(evset->events.dynamic);
  destroy_eventlist(&loop->active);
  destroy_eventlist(&loop->cancelled);
}

static inline struct epoll_event *
fit_eventlist(struct eventlist *eventlist, size_t size)
{
  static const size_t embedded = DDSRT_EMBEDDED_EVENTS;
  struct epoll_event *events;

  assert(eventlist);
  assert(eventlist->size >= embedded);

  if (size < embedded) {
    if (eventlist->size > embedded)
      ddsrt_free(eventlist->events.dynamic);
    eventlist->size = embedded;
    return eventlist->events.embedded;
  } else if (eventlist->size / embedded != size / embedded + 1) {
    events = eventlist->size == embedded ?
      NULL : eventlist->events.dynamic;
    size = (size / embedded + 1) * embedded;
    if (!(events = ddsrt_realloc(events, size * sizeof(*events))))
      return eventlist->size == embedded
        ? eventlist->events.embedded : eventlist->events.dynamic;
    eventlist->size = size;
    eventlist->events.dynamic = events;
    return eventlist->events.dynamic;
  }
  return eventlist->size == embedded
    ? eventlist->events.embedded : eventlist->events.dynamic;
}

static void delete_cancelled(ddsrt_loop_t *loop)
{
  if (!loop->cancelled.count)
    return;
  ddsrt_event_t **cancelled = get_events(&loop->cancelled);
  for (size_t i=0; i < loop->cancelled.count; i++)
    delete_event(&loop->active, cancelled[i]);
  destroy_eventlist(&loop->cancelled);
  // notify (potentially) blocking threads
  ddsrt_cond_broadcast(&loop->condition);
}

dds_return_t
ddsrt_run_loop(ddsrt_loop_t *loop, uint32_t flags, void *user_data)
{
  dds_return_t err = DDS_RETCODE_OK;

  assert(loop);

  ddsrt_mutex_lock(&loop->lock);
  assert(!ddsrt_atomic_ldptr(&loop->owner));
  ddsrt_atomic_stptr(&loop->owner, (uintptr_t)ddsrt_gettid());
  struct eventlist *list = (struct eventlist *)&loop->ready;

  do {
    delete_cancelled(loop);
    assert(loop->active.count + 1 <= INT_MAX);
    struct epoll_event *events = fit_eventlist(
      (struct eventlist *)&loop->ready, loop->active.count + 1);
    ddsrt_mutex_unlock(&loop->lock);
    int ready = epoll_wait(loop->epollfd, events, (int)list->size, -1);
    ddsrt_mutex_lock(&loop->lock);

    assert(ready >= 0 || errno == EINTR);
    if (ready == -1)
      continue;

    for (int i=0; i < ready && !loop->cancelled.count; i++) {
      if (events[i].data.ptr == (void*)loop) {
        char buf[1];
        read_pipe(loop->pipefds[0], buf, sizeof(buf));
        break;
      } else {
        uint32_t flags = 0u;
        if (events[i].events & EPOLLIN)
          flags |= DDSRT_READ;
        if (events[i].events & EPOLLOUT)
          flags |= DDSRT_WRITE;
        if ((err = ddsrt_handle_event(events[i].data.ptr, flags, user_data)))
          goto err_event;
      }
    }
  } while (!ddsrt_atomic_ld32(&loop->terminate) && !(flags & DDSRT_RUN_ONCE));

err_event:
  ddsrt_atomic_stptr(&loop->owner, 0u);
  ddsrt_atomic_st32(&loop->terminate, 0u);
  delete_cancelled(loop);
  ddsrt_mutex_unlock(&loop->lock);
  return err;
}
