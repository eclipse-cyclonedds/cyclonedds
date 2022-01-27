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
#include <assert.h>
#include <string.h>

#include "dds/ddsrt/static_assert.h"

#include "event.h"
#include "eventlist.h"

dds_return_t
ddsrt_add_event(ddsrt_loop_t *loop, ddsrt_event_t *event)
{
  dds_return_t err = DDS_RETCODE_OK;
  ddsrt_socket_t fd;
  bool release = true;

  assert(loop);
  assert(event);

  if (event->loop)
    return event->loop == loop ? DDS_RETCODE_OK : DDS_RETCODE_BAD_PARAMETER;

  fd = event_socket(event);
#if !_WIN32
  assert(fd >= 0);
  assert(fd < FD_SETSIZE);
#endif
  release = lock_loop(loop);

  if ((err = add_event(&loop->active, event, FD_SETSIZE)))
    goto err_event;
  if (event->flags & READ_FLAGS)
    FD_SET(fd, &loop->readfds);
  if (event->flags & WRITE_FLAGS)
    FD_SET(fd, &loop->writefds);

#if !_WIN32
  if (loop->fdmax_plus_1 < fd)
    loop->fdmax_plus_1 = fd + 1;
#endif
  event->loop = loop;
err_event:
  unlock_loop(loop, release);
  return err;
}

#if !_WIN32
static inline ddsrt_socket_t greatest_fd(ddsrt_loop_t *loop)
{
  assert(loop);
  assert(loop->pipefds[0] != DDSRT_INVALID_SOCKET);
  if (loop->active.count == 0)
    return loop->pipefds[0];
  ddsrt_socket_t fdmax = loop->pipefds[0];
  ddsrt_event_t **events = get_events(&loop->active);
  for (size_t i = 0; i < loop->active.count; i++) {
    ddsrt_socket_t fd = event_socket(events[i]);
    if (fd > fdmax)
      fdmax = fd;
  }
  assert(fdmax >= 0);
  assert(fdmax < FD_SETSIZE);
  return fdmax;
}
#endif

dds_return_t
ddsrt_delete_event(ddsrt_loop_t *loop, ddsrt_event_t *event)
{
  dds_return_t err = DDS_RETCODE_OK;
  bool release = true;

  assert(loop);
  assert(event);

  if (event->loop != loop)
    return DDS_RETCODE_BAD_PARAMETER;

  release = lock_loop(loop);
  ddsrt_socket_t fd = event_socket(event);
  if (event->flags & READ_FLAGS)
    FD_CLR(fd, &loop->readfds);
  if (event->flags & WRITE_FLAGS)
    FD_CLR(fd, &loop->writefds);
#if !_WIN32
  if (fd == loop->fdmax_plus_1)
    loop->fdmax_plus_1 = greatest_fd(loop) + 1;
#endif

  if (ddsrt_atomic_ldptr(&loop->owner) != 0u) {
    if ((err = add_event(&loop->cancelled, event, FD_SETSIZE)))
      goto err_event;
    wait_for_loop(loop, release);
  } else {
    delete_event(&loop->active, event);
  }

  event->loop = NULL;
err_event:
  unlock_loop(loop, release);
  return err;
}

dds_return_t
ddsrt_create_loop(ddsrt_loop_t *loop)
{
  ddsrt_socket_t pipefds[2];

  assert(loop);

  DDSRT_STATIC_ASSERT(sizeof(loop->ready) == sizeof(struct eventlist));
  if (open_pipe(pipefds))
    return DDS_RETCODE_OUT_OF_RESOURCES;
  ddsrt_atomic_st32(&loop->terminate, 0u);
  loop->pipefds[0] = pipefds[0];
  loop->pipefds[1] = pipefds[1];
  ddsrt_atomic_stptr(&loop->owner, 0u);
  FD_ZERO(&loop->readfds);
  FD_SET(loop->pipefds[0], &loop->readfds);
  FD_ZERO(&loop->writefds);
  create_eventlist(&loop->active);
  create_eventlist(&loop->cancelled);
#if _WIN32
  loop->fdmax_plus_1 = FD_SETSIZE;
#else
  loop->fdmax_plus_1 = loop->pipefds[0] + 1;
#endif
  ddsrt_mutex_init(&loop->lock);
  ddsrt_cond_init(&loop->condition);
  return DDS_RETCODE_OK;
}

void
ddsrt_destroy_loop(ddsrt_loop_t *loop)
{
  if (!loop)
    return;
  assert(ddsrt_atomic_ldptr(&loop->owner) == 0u);
  close_pipe(loop->pipefds);
  ddsrt_cond_destroy(&loop->condition);
  ddsrt_mutex_destroy(&loop->lock);
  destroy_eventlist(&loop->active);
  destroy_eventlist(&loop->cancelled);
}

static void delete_cancelled(ddsrt_loop_t *loop)
{
  if (!loop->cancelled.count)
    return;
  ddsrt_event_t **events = get_events(&loop->cancelled);
  for (size_t i=0; i < loop->cancelled.count; i++)
    delete_event(&loop->active, events[i]);
  destroy_eventlist(&loop->cancelled);
  // notify (potentially) blocking threads
  ddsrt_cond_broadcast(&loop->condition);
}

dds_return_t
ddsrt_run_loop(ddsrt_loop_t *loop, uint32_t flags, void *user_data)
{
  dds_return_t err = DDS_RETCODE_OK;

  assert(loop);
  assert(!ddsrt_atomic_ldptr(&loop->owner));

  struct eventlist *evset = (struct eventlist *)&loop->ready;

  ddsrt_mutex_lock(&loop->lock);
  ddsrt_atomic_stptr(&loop->owner, (uintptr_t)ddsrt_gettid());

  do {
    delete_cancelled(loop);
#if !_WIN32
    evset->fdmax_plus_1 = loop->fdmax_plus_1;
#endif
    memcpy(&evset->readfds, &loop->readfds, sizeof(evset->readfds));
    memcpy(&evset->writefds, &loop->writefds, sizeof(evset->writefds));

    ddsrt_mutex_unlock(&loop->lock);
    int32_t ready = ddsrt_select(
      evset->fdmax_plus_1, &evset->readfds, &evset->writefds, NULL, DDS_INFINITY);
    ddsrt_mutex_lock(&loop->lock);

    if (ready < 0)
      switch (ready) {
        case DDS_RETCODE_TRY_AGAIN:
        case DDS_RETCODE_INTERRUPTED:
        case DDS_RETCODE_TIMEOUT:
          ready = 0;
          break;
        default:
          err = ready;
          goto err_select;
      }

#if !LWIP_SOCKET
    // pipe can safely be read, it is not an event
    if (ready && FD_ISSET(loop->pipefds[0], &evset->readfds)) {
      char buf[1];
      (void)read_pipe(loop->pipefds[0], buf, sizeof(buf));
      ready = 0; // continue with next iteration
    }
#endif

    if (ready) {
      ddsrt_event_t **events = get_events(&loop->active);
      for (size_t i=0; i < loop->active.count && ready > 0; i++) {
        // callback may have cancelled one or more events
        if (loop->cancelled.count)
          break;
        ddsrt_socket_t fd = event_socket(events[i]);
        uint32_t evflags = 0u;
        if (FD_ISSET(fd, &evset->readfds))
          evflags |= DDSRT_READ;
        if (FD_ISSET(fd, &evset->writefds))
          evflags |= DDSRT_WRITE;
        if (!evflags)
          continue;
        ready--;
        if ((err = ddsrt_handle_event(events[i], evflags, user_data)))
          goto err_handle;
      }
    }
  } while (!ddsrt_atomic_ld32(&loop->terminate) && !(flags & DDSRT_RUN_ONCE));

err_handle:
err_select:
  ddsrt_atomic_stptr(&loop->owner, 0u);
  ddsrt_atomic_st32(&loop->terminate, 0u);
  delete_cancelled(loop);
  ddsrt_mutex_unlock(&loop->lock);
  return err;
}
