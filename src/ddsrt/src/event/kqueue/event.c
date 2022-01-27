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
#include <errno.h>
#include <sys/event.h>

#include "event.h"
#include "eventlist.h"
#include "dds/ddsrt/static_assert.h"

dds_return_t
ddsrt_add_event(ddsrt_loop_t *loop, ddsrt_event_t *event)
{
  int err, fd;
  bool release;
  struct kevent ev;
  unsigned short flags = 0u;

  assert(loop);
  assert(loop->kqueuefd != -1);
  assert(event);

  if (event->loop)
    return event->loop == loop ? DDS_RETCODE_OK : DDS_RETCODE_BAD_PARAMETER;

  fd = event_socket(event);
  release = lock_loop(loop);

  if (add_event(&loop->active, event, INT_MAX))
    goto err_event;
  if (event->flags & READ_FLAGS)
    flags |= EVFILT_READ;
  if (event->flags & WRITE_FLAGS)
    flags |= EVFILT_WRITE;

  EV_SET(&ev, fd, flags, EV_ADD, 0, 0, event);
  do {
    err = kevent(loop->kqueuefd, &ev, 1, NULL, 0, NULL);
  } while (err == -1 && errno == EINTR);
  if (err == -1)
    goto err_kevent;

  event->loop = loop;
  unlock_loop(loop, release);
  return DDS_RETCODE_OK;
err_kevent:
  delete_event(&loop->active, event);
err_event:
  unlock_loop(loop, release);
  return DDS_RETCODE_OUT_OF_RESOURCES;
}

dds_return_t
ddsrt_delete_event(ddsrt_loop_t *loop, ddsrt_event_t *event)
{
  dds_return_t err;
  bool release = true;
  uint64_t owner;

  assert(loop);
  assert(loop->kqueuefd != -1);
  assert(event);

  if (event->loop != loop)
    return DDS_RETCODE_BAD_PARAMETER;

  release = lock_loop(loop);
  int fd = event_socket(event);
  struct kevent ev;
  EV_SET(&ev, fd, 0, EV_DELETE, 0, 0, NULL);
  do {
    if (kevent(loop->kqueuefd, &ev, 1, NULL, 0, NULL) == 0)
      break;
    if (errno != EINTR)
      goto err_kevent;
  } while (1);

  if ((owner = ddsrt_atomic_ldptr(&loop->owner))) {
    if ((err = add_event(&loop->cancelled, event, INT_MAX)))
      goto err_event;
    wait_for_loop(loop, release);
  } else {
    delete_event(&loop->active, event);
  }

  event->loop = NULL;
  unlock_loop(loop, release);
  return DDS_RETCODE_OK;
err_kevent:
  err = (errno == ENOMEM)
    ? DDS_RETCODE_OUT_OF_RESOURCES : DDS_RETCODE_BAD_PARAMETER;
err_event:
  unlock_loop(loop, release);
  return err;
}

dds_return_t
ddsrt_create_loop(ddsrt_loop_t *loop)
{
  int pipefds[2] = { -1, -1 };
  int kqueuefd = -1;
  struct kevent ev;
  struct eventlist *evset;

  assert(loop);

  DDSRT_STATIC_ASSERT(sizeof(loop->ready) == sizeof(struct eventlist));
  if ((kqueuefd = kqueue()) == -1)
    goto err_kqueue;
  if (fcntl(kqueuefd, F_SETFD, fcntl(kqueuefd, F_GETFD)|FD_CLOEXEC) == -1)
    goto err_fcntl;
  if (open_pipe(pipefds) == -1)
    goto err_pipe;
  EV_SET(&ev, pipefds[0], EVFILT_READ, EV_ADD, 0, 0, loop);
  if (kevent(kqueuefd, &ev, 1, NULL, 0, NULL) == -1)
    goto err_kevent;
  ddsrt_atomic_st32(&loop->terminate, 0u);
  ddsrt_atomic_stptr(&loop->owner, 0u);
  loop->pipefds[0] = pipefds[0];
  loop->pipefds[1] = pipefds[1];
  evset = (struct eventlist *)&loop->ready;
  evset->size = DDSRT_EMBEDDED_EVENTS;
  loop->kqueuefd = kqueuefd;
  create_eventlist(&loop->active);
  create_eventlist(&loop->cancelled);
  ddsrt_mutex_init(&loop->lock);
  ddsrt_cond_init(&loop->condition);
  return DDS_RETCODE_OK;
err_kevent:
  close(pipefds[0]);
  close(pipefds[1]);
err_pipe:
err_fcntl:
  close(kqueuefd);
err_kqueue:
  return DDS_RETCODE_OUT_OF_RESOURCES;
}

static const size_t embedded = DDSRT_EMBEDDED_EVENTS;

void
ddsrt_destroy_loop(ddsrt_loop_t *loop)
{
  struct eventlist *eventlist;

  if (!loop)
    return;
  assert(ddsrt_atomic_ldptr(&loop->owner) == 0u);
  eventlist = (struct eventlist *)&loop->ready;
  if (eventlist->size > embedded)
    ddsrt_free(eventlist->events.dynamic);
  destroy_eventlist(&loop->active);
  destroy_eventlist(&loop->cancelled);
  ddsrt_mutex_destroy(&loop->lock);
  ddsrt_cond_destroy(&loop->condition);
}

static inline struct kevent *
fit_eventlist(struct eventlist *eventlist, size_t size)
{
  struct kevent *events;

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
      return eventlist->size == embedded ?
        eventlist->events.embedded : eventlist->events.dynamic;
    eventlist->size = size;
    eventlist->events.dynamic = events;
    return eventlist->events.dynamic;
  }

  return eventlist->size > embedded
    ? eventlist->events.dynamic : eventlist->events.embedded;
}

static void delete_cancelled(ddsrt_loop_t *loop)
{
  if (!loop->cancelled.count)
    return;
  ddsrt_event_t **cancelled = get_events(&loop->cancelled);
  for (size_t i=0; i < loop->cancelled.count; i++)
    delete_event(&loop->active, cancelled[i]);
  destroy_eventlist(&loop->cancelled);
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
    int ready;
    struct kevent *events = fit_eventlist(list, loop->active.count + 1);
    ddsrt_mutex_unlock(&loop->lock);
    do {
      ready = kevent(loop->kqueuefd, NULL, 0, events, (int)list->size, NULL);
      if (ready >= 0)
        break;
      if (errno == EINTR)
        continue;
      err = (errno == ENOMEM)
        ? DDS_RETCODE_OUT_OF_RESOURCES : DDS_RETCODE_ERROR;
      goto err_kevent;
    } while (1);
    ddsrt_mutex_lock(&loop->lock);

    assert(ready >= 0 || errno == EINTR);
    if (ready == -1)
      continue;

    for (int i=0; i < ready && !loop->cancelled.count; i++) {
      if (events[i].udata == (void*)loop) {
        char buf[1];
        read_pipe(loop->pipefds[0], buf, sizeof(buf));
        break;
      } else {
        uint32_t evflags = 0u;
        if (events[i].flags & EVFILT_READ)
          evflags |= DDSRT_READ;
        if (events[i].flags & EVFILT_WRITE)
          evflags |= DDSRT_WRITE;
        if ((err = ddsrt_handle_event(events[i].udata, evflags, user_data)))
          goto err_event;
      }
    }
  } while (!ddsrt_atomic_ld32(&loop->terminate) && !(flags & DDSRT_RUN_ONCE));

err_kevent:
err_event:
  ddsrt_atomic_stptr(&loop->owner, 0u);
  ddsrt_atomic_st32(&loop->terminate, 0u);
  delete_cancelled(loop);
  ddsrt_mutex_unlock(&loop->lock);
  return err;
}
