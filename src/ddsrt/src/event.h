#ifndef EVENT_H
#define EVENT_H

#include <string.h>
#include <stdio.h>

#include "dds/config.h"
#include "dds/ddsrt/event.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/threads.h"

static inline void create_eventlist(ddsrt_eventlist_t *list)
{
  list->length = DDSRT_EMBEDDED_EVENTS;
  list->count = 0u;
  list->start = list->end = 0u;
  memset(list->events.embedded, 0, list->length * sizeof(*list->events.dynamic));
}

static inline void destroy_eventlist(ddsrt_eventlist_t *list)
{
  if (!list)
    return;
  if (list->length > DDSRT_EMBEDDED_EVENTS)
    ddsrt_free(list->events.dynamic);
  list->length = DDSRT_EMBEDDED_EVENTS;
  list->count = 0;
  list->start = list->end = 0u;
}

static inline ddsrt_event_t **get_events(ddsrt_eventlist_t *list)
{
  if (list->length > DDSRT_EMBEDDED_EVENTS)
    return list->events.dynamic;
  return list->events.embedded;
}

#ifndef NDEBUG
static inline void assert_eventlist(ddsrt_eventlist_t *list)
{
  assert(list);
  assert(list->count <= list->length);
  if (list->count <= 1) {
    assert(list->start == list->end);
    assert(list->length == DDSRT_EMBEDDED_EVENTS);
  } else {
    assert(list->start != list->end);
    assert(list->length % DDSRT_EMBEDDED_EVENTS == 0);
  }

  size_t cnt = 0;
  ddsrt_event_t **buf = get_events(list);
  for (size_t i = 0; i < list->length; i++)
    cnt += buf[i] != NULL;
  assert(list->count == cnt);
}
#else
# define assert_eventlist(list)
#endif

static void left_trim(ddsrt_eventlist_t *list)
{
  size_t cnt = list->start;
  ddsrt_event_t **buf = get_events(list);

  if (list->start > list->end) {
    // move start if 1st entry was removed and buffer wraps around
    // -------------------------      -------------------------
    // | X . . . . | . . . . X |  >>  | X . . . . | . . . . X |
    // --^---------------^---^--      --^-------------------^--
    //   nth             1st 2nd        nth                 1st
    for (; cnt < (list->length - 1) && !buf[cnt]; cnt++) ;
    list->start = cnt;
    if (buf[cnt])
      return;
    // start from beginning if last entry before wrap around was removed
    // -------------------------      -------------------------
    // | . . X . . | . . . . . |  >>  | . . X . . | . . . . . |
    // ------^---------------^--      --^---^------------------
    //       nth             1st        1st nth
    assert(cnt == list->length - 1);
    cnt = list->start = 0;
  }
  // move start if 1st entry was removed and buffer does not wrap around
  // -------------------------      -------------------------
  // | X . X . . | X . . . . |  >>  | . . X . . | X . . . . |
  // --^---^-------^----------      ------^-------^----------
  //   1st 2nd     nth                    1st     nth
  for (; cnt < list->end && !buf[cnt]; cnt++) ;
  list->start = cnt;
  assert(list->start == list->end || list->count > 1);
}

static void right_trim(ddsrt_eventlist_t *list)
{
  size_t cnt = list->end;
  ddsrt_event_t **buf = get_events(list);

  if (list->end < list->start) {
    // move end if last entry was removed and buffer wraps around
    // -------------------------      -------------------------
    // | X . . . . | . . . X . |  >>  | X . . . . | . . . X . |
    // --^---^-------------^----      --^-----------------^----
    //   2nd nth           1st          nth               1st
    for (; cnt > 0 && !buf[cnt]; cnt--) ;
    list->end = cnt;
    if (buf[cnt])
      return;
    // start from end if first entry before wrap around was removed
    // -------------------------      -------------------------
    // | . . . . . | . X . X . |  >>  | . . . . . | . X . X . |
    // ----^-----------^--------      ----------------^-----^--
    //     nth         1st                            1st   nth
    assert(cnt == 0);
    cnt = list->end = list->length - 1;
  }
  // move end if last entry was removed and buffer does not wrap around
  // -------------------------      -------------------------
  // | . . . . X | . X . . . |  >>  | . . . . X | . X . . . |
  // ----------^-----^-----^--      ----------^-----^--------
  //           1st   2nd   nth                1st   nth
  for (; cnt > list->start && !buf[cnt]; cnt--) ;
  list->end = cnt;
  assert(list->end == list->start || list->count > 1);
}

static inline void
pack_eventlist(ddsrt_eventlist_t *list)
{
  assert_eventlist(list);
  ddsrt_event_t **buf = get_events(list);

  if (list->start > list->end) {
    // compress tail on buffer wrap around
    // -------------------------      -------------------------
    // | X . X . X | . X . X . |  >>  | X X X . . | . X . X . |
    // ----------^-----^--------      ------^---------^--------
    //           nth   1st                  nth       1st
    size_t i, j;
    for (i = j = 0; i <= list->end; i++) {
      if (!buf[i])
        continue;
      if (i != j)
        (void)(buf[j] = buf[i]), buf[i] = NULL;
      j++;
    }
    assert(j != 0);
    list->end = j - 1;
    // compress head on buffer wrap around
    // -------------------------    -------------------------
    // | X X X . . | . X . X . | >> | X X X . . | . . . X X |
    // ------^---------^--------    ------^-------------^----
    //       nth       1st                nth           1st
    for (i = j = list->length - 1; i >= list->start; i--) {
      if (!buf[i])
        continue;
      if (i != j)
        (void)(buf[j] = buf[1]), buf[i] = NULL;
      j--;
    }
    assert(j != list->length - 1);
    list->start = j + 1;
  } else if (list->count != 0) {
    // compress
    // -------------------------      -------------------------
    // | . . X . . | X . X X X |  >>  | X X X X X | . . . . . |
    // ------^---------------^--      --^-------^--------------
    //       1st             nth        1st     nth
    size_t i, j;
    for (i = j = 0; i <= list->end; i++) {
      if (!buf[i])
        continue;
      if (i != j)
        (void)(buf[j] = buf[i]), buf[i] = NULL;
      j++;
    }
    list->start = 0;
    list->end = j - 1;
    assert(list->end == list->count - 1);
  }
}

static inline dds_return_t
grow_eventlist(ddsrt_eventlist_t *list, size_t max)
{
  static const size_t min = DDSRT_EMBEDDED_EVENTS;
  size_t len;
  ddsrt_event_t **buf = get_events(list);

  assert_eventlist(list);
  assert(list->count == list->length);

  len = list->length + min;
  if (len > max) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  } else if (list->length == min) {
    if (!(buf = ddsrt_malloc(len * sizeof(*buf))))
      return DDS_RETCODE_OUT_OF_RESOURCES;
    memmove(buf, list->events.embedded, list->length * sizeof(*buf));
  } else {
    if (!(buf = ddsrt_realloc(list->events.dynamic, len * sizeof(*buf))))
      return DDS_RETCODE_OUT_OF_RESOURCES;
  }

  // move head to end of newly allocated buffer
  if (list->start > list->end) {
    size_t mov = list->length - list->start;
    memmove(buf + (list->start + min), buf + list->start, mov * sizeof(*buf));
  }

  // zero newly allocated memory
  memset(buf + (list->end + 1), 0, min * sizeof(*buf));

  list->length = len;
  list->events.dynamic = buf;
  return DDS_RETCODE_OK;
}

static inline dds_return_t
maybe_shrink_eventlist(ddsrt_eventlist_t *list)
{
  static const size_t min = DDSRT_EMBEDDED_EVENTS;
  ddsrt_event_t **buf;

  assert_eventlist(list);
  assert(list->length > min);

  if (!(list->count == min || list->count < list->length - min))
    return DDS_RETCODE_OK;
  // eventlist can be sparse
  pack_eventlist(list);

  buf = list->events.dynamic;
  // pack operation moved head to end of buffer on wrap around. move head to
  // front to not discard it on reallocation
  if (list->count <= min) {
    ddsrt_event_t **embuf = list->events.embedded;
    if (list->end < list->start) {
      assert(list->count > 1);
      size_t mov = list->length - list->start;
      memmove(embuf, buf + list->start, mov * sizeof(*buf));
      memmove(embuf + mov, buf, (list->end + 1) * sizeof(*buf));
      list->start = 0u;
      list->end = list->count - 1;
    } else {
      assert(list->start == 0u);
      memmove(embuf, buf, list->end * sizeof(*buf));
    }
    ddsrt_free(buf);
    list->length = min;
  } else {
    size_t mov = 0;
    if (list->end < list->start) {
      assert(list->start - min > list->end);
      mov = (list->length - list->start) * sizeof(*buf);
      memmove(buf + (list->start - min), buf + list->start, mov);
      list->start -= min;
    } else {
      assert(list->start == 0u);
    }

    size_t len = ((list->count/min) + 1) * min;
    if (!(buf = ddsrt_realloc(buf, len * sizeof(*buf)))) {
      // move head back to end of buffer
      if (mov != 0) {
        list->start += min;
        memmove(buf + list->start, buf + (list->start - min), mov);
      }
      return DDS_RETCODE_OUT_OF_RESOURCES;
    }
    list->length = len;
    list->events.dynamic = buf;
  }

  return DDS_RETCODE_OK;
}

static inline ssize_t
find_event(ddsrt_eventlist_t *list, ddsrt_event_t *event)
{
  assert(list);

  ddsrt_event_t **buf = get_events(list);
  // buffer is circular, so window does not have to be consecutive
  size_t len = list->start > list->end ? list->length - 1 : list->end;
  for (size_t cnt = list->start; cnt <= len; cnt++)
    if (buf[cnt] == event)
      return (ssize_t)cnt;

  if (list->start < list->end)
    return -1;

  len = list->end;
  for (size_t cnt = 0; cnt <= len; cnt++)
    if (buf[cnt] == event)
      return (ssize_t)cnt;

  return -1;
}

static inline dds_return_t
add_event(ddsrt_eventlist_t *list, ddsrt_event_t *event, size_t max)
{
  ssize_t cnt;
  ddsrt_event_t **buf;

  assert(list);
  assert(event);
  // ensure event is not listed
  assert(find_event(list, event) == -1);

  // allocate more space if list is full
  if (list->count == list->length) {
    assert(list->end != list->start);
    if (grow_eventlist(list, max) == -1)
      return DDS_RETCODE_OUT_OF_RESOURCES;
    cnt = (ssize_t)(list->end += 1);
  } else if (list->end < list->start) {
    if (list->end + 1 == list->start)
      cnt = find_event(list, NULL);
    else
      cnt = (ssize_t)(list->end += 1);
  // take into account wrap around
  } else if (list->end == list->length - 1) {
    if (list->start == 0)
      cnt = find_event(list, NULL);
    else
      cnt = (ssize_t)(list->end = 0);
  } else if (list->end > list->start) {
    cnt = (ssize_t)(list->end += 1);
  } else {
    cnt = (ssize_t)(list->end += (list->count != 0));
  }

  buf = get_events(list);
  buf[cnt] = event;
  list->count++;

  return DDS_RETCODE_OK;
}

static inline dds_return_t
delete_event(ddsrt_eventlist_t *list, ddsrt_event_t *event)
{
  static const size_t min = DDSRT_EMBEDDED_EVENTS;
  ssize_t cnt;
  ddsrt_event_t **buf;

  assert(list);
  assert(event);

  if ((cnt = find_event(list, event)) == -1)
    return DDS_RETCODE_OK;

  buf = get_events(list);
  buf[cnt] = NULL;

  list->count--;
  if (list->count == 0)
    list->start = list->end = 0;
  else if (cnt == (ssize_t)list->start)
    left_trim(list);
  else if (cnt == (ssize_t)list->end)
    right_trim(list);

  // do not attempt to shrink embedded buffer
  if (list->length == min)
    return DDS_RETCODE_OK;
  (void)maybe_shrink_eventlist(list); // failure can safely be ignored

  return DDS_RETCODE_OK;
}

#if _WIN32
static inline int open_pipe(ddsrt_socket_t fds[2])
{
  struct sockaddr_in addr;
  socklen_t asize = sizeof (addr);
  ddsrt_socket_t listener = socket (AF_INET, SOCK_STREAM, 0);
  ddsrt_socket_t s1 = socket (AF_INET, SOCK_STREAM, 0);
  ddsrt_socket_t s2 = DDSRT_INVALID_SOCKET;

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
  addr.sin_port = 0;
  if (bind (listener, (struct sockaddr *)&addr, sizeof (addr)) == -1)
    goto fail;
  if (getsockname (listener, (struct sockaddr *)&addr, &asize) == -1)
    goto fail;
  if (listen (listener, 1) == -1)
    goto fail;
  if (connect (s1, (struct sockaddr *)&addr, sizeof (addr)) == -1)
    goto fail;
  if ((s2 = accept (listener, 0, 0)) == INVALID_SOCKET)
    goto fail;
  closesocket (listener);
  /* Equivalent to FD_CLOEXEC */
  SetHandleInformation ((HANDLE) s1, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation ((HANDLE) s2, HANDLE_FLAG_INHERIT, 0);
  fds[0] = s1;
  fds[1] = s2;
  return 0;

fail:
  closesocket (listener);
  closesocket (s1);
  closesocket (s2);
  return -1;
}

static inline void close_pipe(ddsrt_socket_t fds[2])
{
  closesocket(fds[0]);
  closesocket(fds[1]);
}
#else
#include <fcntl.h>
#include <unistd.h>

static inline int open_pipe(int fds[2])
{
  int pipefds[2];

  if (pipe(pipefds) == -1)
    goto err_pipe;
  if (fcntl(pipefds[0], F_SETFD, fcntl(pipefds[0], F_GETFD)|O_NONBLOCK|FD_CLOEXEC) == -1)
    goto err_fcntl;
  if (fcntl(pipefds[1], F_SETFD, fcntl(pipefds[1], F_GETFD)|O_NONBLOCK|FD_CLOEXEC) == -1)
    goto err_fcntl;
  fds[0] = pipefds[0];
  fds[1] = pipefds[1];
  return 0;
err_fcntl:
  close(pipefds[0]);
  close(pipefds[1]);
err_pipe:
  return -1;
}

static inline void close_pipe(int fds[2])
{
  close(fds[0]);
  close(fds[1]);
}
#endif

static ssize_t read_pipe(ddsrt_socket_t fd, void *buf, size_t len)
{
#if _WIN32
  return recv(fd, buf, (int)len, 0);
#else
  return read(fd, buf, len);
#endif
}

static ssize_t write_pipe(ddsrt_socket_t fd, const void *buf, size_t len)
{
#if _WIN32
  return send(fd, buf, (int)len, 0);
#else
  return write(fd, buf, len);
#endif
}

#define WRITE_FLAGS (DDSRT_WRITE)
#if DDSRT_HAVE_NETLINK_EVENT
# define READ_FLAGS (DDSRT_READ|DDSRT_NETLINK)
#else
# define READ_FLAGS (DDSRT_READ)
#endif

#define FIXED DDSRT_FIXED_EVENTS

static inline ddsrt_socket_t event_socket(ddsrt_event_t *event)
{
  ddsrt_socket_t fd;
#if DDSRT_HAVE_NETLINK_EVENT
  if (event->flags & DDSRT_NETLINK)
# if _WIN32
    fd = event->source.netlink.pipefds[0];
# else
    fd = event->source.netlink.socketfd;
# endif
  else
#endif
    fd = event->source.socket.socketfd;

  assert(fd != DDSRT_INVALID_SOCKET);
#if !_WIN32
  assert(fd >= 0);
  assert(fd < FD_SETSIZE);
#endif

  return fd;
}

static inline bool lock_loop(ddsrt_loop_t *loop)
{
  uintptr_t tid = (uintptr_t)ddsrt_gettid();
  if (ddsrt_atomic_ldptr(&loop->owner) == tid)
    return false;
  ddsrt_mutex_lock(&loop->lock);
  return true;
}

static inline void unlock_loop(ddsrt_loop_t *loop, bool release)
{
  uintptr_t tid = (uintptr_t)ddsrt_gettid();
  assert(release || ddsrt_atomic_ldptr(&loop->owner) == tid);
  if (!release)
    return; // no-op
  ddsrt_mutex_unlock(&loop->lock);
}

static inline void wait_for_loop(ddsrt_loop_t *loop, bool release)
{
  char buf[1] = { '\0' };
  uintptr_t tid = (uintptr_t)ddsrt_gettid();
  assert(release || ddsrt_atomic_ldptr(&loop->owner) == tid);
  if (!release)
    return; // no-op
  write_pipe(loop->pipefds[0], buf, sizeof(buf));
  ddsrt_cond_wait(&loop->condition, &loop->lock);
}

#endif // EVENT_H
