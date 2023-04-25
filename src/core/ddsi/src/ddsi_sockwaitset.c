// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/sync.h"

#include "dds/ddsi/ddsi_log.h"
#include "ddsi__sockwaitset.h"
#include "ddsi__tran.h"

#define WAITSET_DELTA 8

#define MODE_KQUEUE 1
#define MODE_SELECT 2
#define MODE_WFMEVS 3

#if defined __APPLE__
#define MODE_SEL MODE_KQUEUE
#elif defined WINCE
#define MODE_SEL MODE_WFMEVS
#else
#define MODE_SEL MODE_SELECT
#endif

#if MODE_SEL == MODE_KQUEUE

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#ifndef __QNXNTO__
#include <sys/fcntl.h>
#endif  // __QNXNTO__
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

struct ddsi_sock_waitset_ctx
{
  struct kevent *evs;
  uint32_t nevs;
  uint32_t evs_sz;
  uint32_t index; /* cursor for enumerating */
};

struct entry {
  uint32_t index;
  int fd;
  struct ddsi_tran_conn * conn;
};

struct ddsi_sock_waitset
{
  int kqueue;
  int pipe[2]; /* pipe used for triggering */
  ddsrt_atomic_uint32_t sz;
  struct entry *entries;
  struct ddsi_sock_waitset_ctx ctx; /* set of descriptors being handled */
  ddsrt_mutex_t lock; /* for add/delete */
};

static int add_entry_locked (struct ddsi_sock_waitset * ws, struct ddsi_tran_conn * conn, int fd)
{
  uint32_t idx, fidx, sz, n;
  struct kevent kev;
  assert (fd >= 0);
  sz = ddsrt_atomic_ld32 (&ws->sz);
  for (idx = 0, fidx = UINT32_MAX, n = 0; idx < sz; idx++)
  {
    if (ws->entries[idx].fd == -1)
      fidx = (idx < fidx) ? idx : fidx;
    else if (ws->entries[idx].conn == conn)
      return 0;
    else
      n++;
  }

  if (fidx == UINT32_MAX)
  {
    const uint32_t newsz = ddsrt_atomic_add32_nv (&ws->sz, WAITSET_DELTA);
    ws->entries = ddsrt_realloc (ws->entries, newsz * sizeof (*ws->entries));
    for (idx = sz; idx < newsz; idx++)
      ws->entries[idx].fd = -1;
    fidx = sz;
  }
  EV_SET (&kev, (unsigned)fd, EVFILT_READ, EV_ADD, 0, 0, &ws->entries[fidx]);
  if (kevent(ws->kqueue, &kev, 1, NULL, 0, NULL) == -1)
    return -1;
  ws->entries[fidx].conn = conn;
  ws->entries[fidx].fd = fd;
  ws->entries[fidx].index = n;
  return 1;
}

struct ddsi_sock_waitset * ddsi_sock_waitset_new (void)
{
  const uint32_t sz = WAITSET_DELTA;
  struct ddsi_sock_waitset * ws;
  uint32_t i;
  if ((ws = ddsrt_malloc (sizeof (*ws))) == NULL)
    goto fail_waitset;
  ddsrt_atomic_st32 (&ws->sz, sz);
  if ((ws->entries = ddsrt_malloc (sz * sizeof (*ws->entries))) == NULL)
    goto fail_entries;
  for (i = 0; i < sz; i++)
    ws->entries[i].fd = -1;
  ws->ctx.nevs = 0;
  ws->ctx.index = 0;
  ws->ctx.evs_sz = sz;
  if ((ws->ctx.evs = ddsrt_malloc (ws->ctx.evs_sz * sizeof (*ws->ctx.evs))) == NULL)
    goto fail_ctx_evs;
  if ((ws->kqueue = kqueue ()) == -1)
    goto fail_kqueue;
  if (pipe (ws->pipe) == -1)
    goto fail_pipe;
  if (add_entry_locked (ws, NULL, ws->pipe[0]) < 0)
    goto fail_add_trigger;
  assert (ws->entries[0].fd == ws->pipe[0]);
  if (fcntl (ws->kqueue, F_SETFD, fcntl (ws->kqueue, F_GETFD) | FD_CLOEXEC) == -1)
    goto fail_fcntl;
  if (fcntl (ws->pipe[0], F_SETFD, fcntl (ws->pipe[0], F_GETFD) | FD_CLOEXEC) == -1)
    goto fail_fcntl;
  if (fcntl (ws->pipe[1], F_SETFD, fcntl (ws->pipe[1], F_GETFD) | FD_CLOEXEC) == -1)
    goto fail_fcntl;
  ddsrt_mutex_init (&ws->lock);
  return ws;

fail_fcntl:
fail_add_trigger:
  close (ws->pipe[0]);
  close (ws->pipe[1]);
fail_pipe:
  close (ws->kqueue);
fail_kqueue:
  ddsrt_free (ws->ctx.evs);
fail_ctx_evs:
  ddsrt_free (ws->entries);
fail_entries:
  ddsrt_free (ws);
fail_waitset:
  return NULL;
}

void ddsi_sock_waitset_free (struct ddsi_sock_waitset * ws)
{
  ddsrt_mutex_destroy (&ws->lock);
  close (ws->pipe[0]);
  close (ws->pipe[1]);
  close (ws->kqueue);
  ddsrt_free (ws->entries);
  ddsrt_free (ws->ctx.evs);
  ddsrt_free (ws);
}

void ddsi_sock_waitset_trigger (struct ddsi_sock_waitset * ws)
{
  char buf = 0;
  int n;
  n = (int)write (ws->pipe[1], &buf, 1);
  if (n != 1)
  {
    DDS_WARNING("ddsi_sock_waitset_trigger: read failed on trigger pipe, errno = %d\n", errno);
  }
}

int ddsi_sock_waitset_add (struct ddsi_sock_waitset * ws, struct ddsi_tran_conn * conn)
{
  int ret;
  ddsrt_mutex_lock (&ws->lock);
  ret = add_entry_locked (ws, conn, ddsi_conn_handle (conn));
  ddsrt_mutex_unlock (&ws->lock);
  return ret;
}

void ddsi_sock_waitset_purge (struct ddsi_sock_waitset * ws, unsigned index)
{
  /* Sockets may have been closed by the Purge is called, but any closed sockets
     are automatically deleted from the kqueue and the file descriptors be reused
     in the meantime.  It therefore seems wiser replace the kqueue then to delete
     entries */
  uint32_t i, sz;
  struct kevent kev;
  ddsrt_mutex_lock (&ws->lock);
  sz = ddsrt_atomic_ld32 (&ws->sz);
  close (ws->kqueue);
  if ((ws->kqueue = kqueue()) == -1)
    abort (); /* FIXME */
  for (i = 0; i <= index; i++)
  {
    assert (ws->entries[i].fd >= 0);
    EV_SET(&kev, (unsigned)ws->entries[i].fd, EVFILT_READ, EV_ADD, 0, 0, &ws->entries[i]);
    if (kevent(ws->kqueue, &kev, 1, NULL, 0, NULL) == -1)
      abort (); /* FIXME */
  }
  for (; i < sz; i++)
  {
    ws->entries[i].conn = NULL;
    ws->entries[i].fd = -1;
  }
  ddsrt_mutex_unlock (&ws->lock);
}

void ddsi_sock_waitset_remove (struct ddsi_sock_waitset * ws, struct ddsi_tran_conn * conn)
{
  const int fd = ddsi_conn_handle (conn);
  uint32_t i, sz;
  assert (fd >= 0);
  ddsrt_mutex_lock (&ws->lock);
  sz = ddsrt_atomic_ld32 (&ws->sz);
  for (i = 1; i < sz; i++)
    if (ws->entries[i].fd == fd)
      break;
  if (i < sz)
  {
    struct kevent kev;
    EV_SET(&kev, (unsigned)ws->entries[i].fd, EVFILT_READ, EV_DELETE, 0, 0, 0);
    if (kevent(ws->kqueue, &kev, 1, NULL, 0, NULL) == -1)
      abort (); /* FIXME */
    ws->entries[i].fd = -1;
  }
  ddsrt_mutex_unlock (&ws->lock);
}

struct ddsi_sock_waitset_ctx * ddsi_sock_waitset_wait (struct ddsi_sock_waitset * ws)
{
  /* if the array of events is smaller than the number of file descriptors in the
     kqueue, things will still work fine, as the kernel will just return what can
     be stored, and the set will be grown on the next call */
  uint32_t ws_sz = ddsrt_atomic_ld32 (&ws->sz);
  int nevs;
  if (ws->ctx.evs_sz < ws_sz)
  {
    ws->ctx.evs_sz = ws_sz;
    ws->ctx.evs = ddsrt_realloc (ws->ctx.evs, ws_sz * sizeof(*ws->ctx.evs));
  }
  nevs = kevent (ws->kqueue, NULL, 0, ws->ctx.evs, (int)ws->ctx.evs_sz, NULL);
  if (nevs < 0)
  {
    if (errno == EINTR)
      nevs = 0;
    else
    {
      DDS_WARNING("ddsi_sock_waitset_wait: kevent failed, errno = %d\n", errno);
      return NULL;
    }
  }
  ws->ctx.nevs = (uint32_t)nevs;
  ws->ctx.index = 0;
  return &ws->ctx;
}

int ddsi_sock_waitset_next_event (struct ddsi_sock_waitset_ctx * ctx, struct ddsi_tran_conn **conn)
{
  while (ctx->index < ctx->nevs)
  {
    uint32_t idx = ctx->index++;
    struct entry * const entry = ctx->evs[idx].udata;
    if (entry->index > 0)
    {
      *conn = entry->conn;
      return (int)(entry->index - 1);
    }
    else
    {
      /* trigger pipe, read & try again */
      char dummy;
      read ((int)ctx->evs[idx].ident, &dummy, 1);
    }
  }
  return -1;
}

#elif MODE_SEL == MODE_WFMEVS

struct ddsi_sock_waitset_ctx
{
  struct ddsi_tran_conn * conns[MAXIMUM_WAIT_OBJECTS]; /* connections and listeners */
  WSAEVENT events[MAXIMUM_WAIT_OBJECTS];        /* events associated with sockets */
  int index; /* last wakeup index, or -1 */
  unsigned n; /* sockets/events [0 .. n-1] are occupied */
};

struct ddsi_sock_waitset
{
  ddsrt_mutex_t mutex;  /* concurrency guard */
  struct ddsi_sock_waitset_ctx ctx;
  struct ddsi_sock_waitset_ctx ctx0;
};

struct ddsi_sock_waitset * ddsi_sock_waitset_new (void)
{
  struct ddsi_sock_waitset * ws = ddsrt_malloc (sizeof (*ws));
  ws->ctx.conns[0] = NULL;
  ws->ctx.events[0] = WSACreateEvent ();
  ws->ctx.n = 1;
  ws->ctx.index = -1;
  ddsrt_mutex_init (&ws->mutex);
  return ws;
}

void ddsi_sock_waitset_free (struct ddsi_sock_waitset * ws)
{
  for (unsigned i = 0; i < ws->ctx.n; i++)
  {
    WSACloseEvent (ws->ctx.events[i]);
  }
  ddsrt_mutex_destroy (&ws->mutex);
  ddsrt_free (ws);
}

void ddsi_sock_waitset_purge (struct ddsi_sock_waitset * ws, unsigned index)
{
  ddsrt_mutex_lock (&ws->mutex);
  for (unsigned i = index + 1; i < ws->ctx.n; i++)
  {
    ws->ctx.conns[i] = NULL;
    if (!WSACloseEvent (ws->ctx.events[i]))
    {
      DDS_WARNING("ddsi_sock_waitset_purge: WSACloseEvent (%x failed, error %d\n", (os_uint32) ws->ctx.events[i], os_getErrno ());
    }
  }
  ws->ctx.n = index + 1;
  ddsrt_mutex_unlock (&ws->mutex);
}

void ddsi_sock_waitset_remove (struct ddsi_sock_waitset * ws, struct ddsi_tran_conn * conn)
{
  ddsrt_mutex_lock (&ws->mutex);
  for (unsigned i = 0; i < ws->ctx.n; i++)
  {
    if (conn == ws->ctx.conns[i])
    {
      WSACloseEvent (ws->ctx.events[i]);
      ws->ctx.n--;
      if (i != ws->ctx.n)
      {
        ws->ctx.events[i] = ws->ctx.events[ws->ctx.n];
        ws->ctx.conns[i] = ws->ctx.conns[ws->ctx.n];
      }
      break;
    }
  }
  ddsrt_mutex_unlock (&ws->mutex);
}

void ddsi_sock_waitset_trigger (struct ddsi_sock_waitset * ws)
{
  if (! WSASetEvent (ws->ctx.events[0]))
  {
    DDS_WARNING("ddsi_sock_waitset_trigger: WSASetEvent(%x) failed, error %d\n", (os_uint32) ws->ctx.events[0], os_getErrno ());
  }
}

int ddsi_sock_waitset_add (struct ddsi_sock_waitset * ws, struct ddsi_tran_conn * conn)
{
  WSAEVENT ev;
  os_socket sock = ddsi_conn_handle (conn);
  unsigned idx;
  int ret;

  ddsrt_mutex_lock (&ws->mutex);

  for (idx = 0; idx < ws->ctx.n; idx++)
  {
    if (ws->ctx.conns[idx] == conn)
      break;
  }
  if (idx < ws->ctx.n)
    ret = 0;
  else
  {
    assert (ws->n < MAXIMUM_WAIT_OBJECTS);
    if ((ev = WSACreateEvent ()) == WSA_INVALID_EVENT)
      ret = -1;
    else
    {
      if (WSAEventSelect (sock, ev, FD_READ) == SOCKET_ERROR)
      {
        DDS_WARNING("ddsi_sock_waitset_add: WSAEventSelect(%x,%x) failed, error %d\n", (os_uint32) sock, (os_uint32) ev, os_getErrno ());
        WSACloseEvent (ev);
        ret = -1;
      }
      else
      {
        ws->ctx.conns[ws->ctx.n] = conn;
        ws->ctx.events[ws->ctx.n] = ev;
        ws->ctx.n++;
        ret = 1;
      }
    }
  }

  ddsrt_mutex_unlock (&ws->mutex);
  return ret;
}

struct ddsi_sock_waitset_ctx * ddsi_sock_waitset_wait (struct ddsi_sock_waitset * ws)
{
  unsigned idx;

  assert (ws->index == -1);

  ddsrt_mutex_lock (&ws->mutex);
  ws->ctx0 = ws->ctx;
  ddsrt_mutex_unlock (&ws->mutex);

  if ((idx = WSAWaitForMultipleEvents (ws->ctx0.n, ws->ctx0.events, FALSE, WSA_INFINITE, FALSE)) == WSA_WAIT_FAILED)
  {
    DDS_WARNING("ddsi_sock_waitset_wait: WSAWaitForMultipleEvents(%d,...,0,0,0) failed, error %d\n", ws->ctx0.n, os_getErrno ());
    return NULL;
  }

#ifndef WAIT_IO_COMPLETION /* curious omission in the WinCE headers */
#define TEMP_DEF_WAIT_IO_COMPLETION
#define WAIT_IO_COMPLETION 0xc0L
#endif
  if (idx >= WSA_WAIT_EVENT_0 && idx < WSA_WAIT_EVENT_0 + ws->ctx0.n)
  {
    ws->ctx0.index = idx - WSA_WAIT_EVENT_0;
    if (ws->ctx0.index == 0)
    {
      /* pretend a spurious wakeup */
      WSAResetEvent (ws->ctx0.events[0]);
      ws->ctx0.index = -1;
    }
    return &ws->ctx0;
  }

  if (idx == WAIT_IO_COMPLETION)
  {
    /* Presumably can't happen with alertable = FALSE */
    DDS_WARNING("ddsi_sock_waitset_wait: WSAWaitForMultipleEvents(%d,...,0,0,0) returned unexpected WAIT_IO_COMPLETION\n", ws->ctx0.n);
  }
  else
  {
    DDS_WARNING("ddsi_sock_waitset_wait: WSAWaitForMultipleEvents(%d,...,0,0,0) returned unrecognised %d\n", ws->ctx0.n, idx);
  }
#ifdef TEMP_DEF_WAIT_IO_COMPLETION
#undef WAIT_IO_COMPLETION
#undef TEMP_DEF_WAIT_IO_COMPLETION
#endif
  return NULL;
}

/* This implementation follows the pattern of simply looking at the
 socket that triggered the wakeup; alternatively, one could scan the
 entire set as we do for select().  If the likelihood of two sockets
 having an event simultaneously is small, this is better, but if it
 is large, the lower indices may get a disproportionally large
 amount of attention. */

int ddsi_sock_waitset_next_event (struct ddsi_sock_waitset_ctx * ctx, struct ddsi_tran_conn ** conn)
{
  assert (-1 <= ctx->index && ctx->index < ctx->n);
  assert (0 < ctx->n && ctx->n <= ctx->sz);
  if (ctx->index == -1)
  {
    return -1;
  }
  else
  {
    WSANETWORKEVENTS nwev;
    int idx = ctx->index;
    os_socket handle;

    ctx->index = -1;
    handle = ddsi_conn_handle (ctx->conns[idx]);
    if (WSAEnumNetworkEvents (handle, ctx->events[idx], &nwev) == SOCKET_ERROR)
    {
      int err = os_getErrno ();
      if (err != WSAENOTSOCK)
      {
        /* May have a wakeup and a close in parallel, so the handle
         need not exist anymore. */
        DDS_ERROR("ddsi_sock_waitset_next_event: WSAEnumNetworkEvents(%x,%x,...) failed, error %d", (os_uint32) handle, (os_uint32) ctx->events[idx], err);
      }
      return -1;
    }

    *conn = ctx->conns[idx];
    return idx - 1;
  }
}

#elif MODE_SEL == MODE_SELECT

#ifdef __VXWORKS__
#include <pipeDrv.h>
#include <ioLib.h>
#include <string.h>
#include <selectLib.h>
#define OSPL_PIPENAMESIZE 26
#endif

#if !_WIN32 && !LWIP_SOCKET && !__ZEPHYR__

#if ! __VXWORKS__&& !__QNXNTO__
#include <sys/fcntl.h>
#endif /* __VXWORKS__ __QNXNTO__ */

#ifndef _WRS_KERNEL
#include <sys/select.h>
#endif
#ifdef __sun
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#endif /* !_WIN32 && !LWIP_SOCKET && !__ZEPHYR__ */

typedef struct ddsi_sock_waitset_set
{
  struct ddsi_tran_conn ** conns;  /* connections in set */
  ddsrt_socket_t * fds;           /* file descriptors in set */
  unsigned sz;               /* max number of fds in context */
  unsigned n;                /* actual number of fds in context */
} ddsi_sock_waitset_set_t;

struct ddsi_sock_waitset_ctx
{
  ddsi_sock_waitset_set_t set;     /* set of connections and descriptors */
  unsigned index;            /* cursor for enumerating */
  fd_set rdset;              /* read file descriptors */
};

struct ddsi_sock_waitset
{
  ddsrt_socket_t pipe[2];             /* pipe used for triggering */
  ddsrt_mutex_t mutex;                /* concurrency guard */
  int fdmax_plus_1;              /* value for first parameter of select() */
  ddsi_sock_waitset_set_t set;         /* set of descriptors handled next */
  struct ddsi_sock_waitset_ctx ctx;  /* set of descriptors being handled  */
};

#if defined (_WIN32)
static int make_pipe (ddsrt_socket_t fd[2])
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
  fd[0] = s1;
  fd[1] = s2;
  return 0;

fail:
  closesocket (listener);
  closesocket (s1);
  closesocket (s2);
  return -1;
}
#elif defined(__VXWORKS__)
static int make_pipe (int pfd[2])
{
  char pipename[OSPL_PIPENAMESIZE];
  int pipecount = 0;
  do {
    snprintf ((char*)&pipename, sizeof (pipename), "/pipe/ospl%d", pipecount++);
  } while ((result = pipeDevCreate ((char*)&pipename, 1, 1)) == -1 && os_getErrno() == EINVAL);
  if (result == -1)
    goto fail_pipedev;
  if ((pfd[0] = open ((char*)&pipename, O_RDWR, 0644)) == -1)
    goto fail_open0;
  if ((pfd[1] = open ((char*)&pipename, O_RDWR, 0644)) == -1)
    goto fail_open1;
  return 0;

fail_open1:
  close (pfd[0]);
fail_open0:
  pipeDevDelete (pipename, 0);
fail_pipedev:
  return -1;
}
#elif defined(__ZEPHYR__)
static int make_pipe (int pfd[2])
{
  /* needs CONFIG_NET_SOCKETPAIR */
  int res;
  res = socketpair(AF_UNIX, SOCK_STREAM, 0, pfd);
  if (res < 0) {
    return -1;
  }
  return res;
}
#elif !defined(LWIP_SOCKET)
static int make_pipe (int pfd[2])
{
  return pipe (pfd);
}
#endif

static void ddsi_sock_waitset_new_set (ddsi_sock_waitset_set_t * set)
{
  set->fds = ddsrt_malloc (WAITSET_DELTA * sizeof (*set->fds));
  set->conns = ddsrt_malloc (WAITSET_DELTA * sizeof (*set->conns));
  set->sz = WAITSET_DELTA;
  set->n = 1;
}

static void ddsi_sock_waitset_free_set (ddsi_sock_waitset_set_t * set)
{
  ddsrt_free (set->fds);
  ddsrt_free (set->conns);
}

static void ddsi_sock_waitset_new_ctx (struct ddsi_sock_waitset_ctx * ctx)
{
  ddsi_sock_waitset_new_set (&ctx->set);
  FD_ZERO (&ctx->rdset);
}

static void ddsi_sock_waitset_free_ctx (struct ddsi_sock_waitset_ctx * ctx)
{
  ddsi_sock_waitset_free_set (&ctx->set);
}

struct ddsi_sock_waitset * ddsi_sock_waitset_new (void)
{
  int result;
  struct ddsi_sock_waitset * ws = ddsrt_malloc (sizeof (*ws));

  ddsi_sock_waitset_new_set (&ws->set);
  ddsi_sock_waitset_new_ctx (&ws->ctx);

#if ! defined (_WIN32)
  ws->fdmax_plus_1 = 0;
#else
  ws->fdmax_plus_1 = FD_SETSIZE;
#endif

#if defined(LWIP_SOCKET)
  ws->pipe[0] = -1;
  ws->pipe[1] = -1;
  result = 0;
#else
  result = make_pipe (ws->pipe);
#endif
  if (result == -1)
  {
    ddsi_sock_waitset_free_ctx (&ws->ctx);
    ddsi_sock_waitset_free_set (&ws->set);
    ddsrt_free (ws);
    return NULL;
  }

#if !defined(LWIP_SOCKET)
  ws->set.fds[0] = ws->pipe[0];
#else
  ws->set.fds[0] = 0;
#endif
  ws->set.conns[0] = NULL;

#if !defined(__VXWORKS__) && !defined(_WIN32) && !defined(LWIP_SOCKET) && !defined(__QNXNTO__) && !defined(__ZEPHYR__)
  (void) fcntl (ws->pipe[0], F_SETFD, fcntl (ws->pipe[0], F_GETFD) | FD_CLOEXEC);
  (void) fcntl (ws->pipe[1], F_SETFD, fcntl (ws->pipe[1], F_GETFD) | FD_CLOEXEC);
#endif
#if !defined(LWIP_SOCKET)
  FD_SET (ws->set.fds[0], &ws->ctx.rdset);
#endif
#if !defined(_WIN32)
  ws->fdmax_plus_1 = ws->set.fds[0] + 1;
#endif

  ddsrt_mutex_init (&ws->mutex);

  return ws;
}

static void ddsi_sock_waitset_grow (ddsi_sock_waitset_set_t * set)
{
  set->sz += WAITSET_DELTA;
  set->conns = ddsrt_realloc (set->conns, set->sz * sizeof (*set->conns));
  set->fds = ddsrt_realloc (set->fds, set->sz * sizeof (*set->fds));
}

void ddsi_sock_waitset_free (struct ddsi_sock_waitset * ws)
{
#if defined(__VXWORKS__) && defined(__RTP__)
  char nameBuf[OSPL_PIPENAMESIZE];
  ioctl (ws->pipe[0], FIOGETNAME, &nameBuf);
#endif
#if defined(_WIN32)
  closesocket (ws->pipe[0]);
  closesocket (ws->pipe[1]);
#elif !defined(LWIP_SOCKET)
  (void) close (ws->pipe[0]);
  (void) close (ws->pipe[1]);
#endif
#if defined(__VXWORKS__) && defined(__RTP__)
  pipeDevDelete ((char*) &nameBuf, 0);
#endif
  ddsi_sock_waitset_free_set (&ws->set);
  ddsi_sock_waitset_free_ctx (&ws->ctx);
  ddsrt_mutex_destroy (&ws->mutex);
  ddsrt_free (ws);
}

void ddsi_sock_waitset_trigger (struct ddsi_sock_waitset * ws)
{
#if defined(LWIP_SOCKET)
  (void)ws;
#else
  char buf = 0;
  int n;

#if defined (_WIN32)
  n = send (ws->pipe[1], &buf, 1, 0);
#else
  n = (int) write (ws->pipe[1], &buf, 1);
#endif
  if (n != 1)
  {
    DDS_WARNING("ddsi_sock_waitset_trigger: write failed on trigger pipe\n");
  }
#endif /* defined(LWIP_SOCKET) */
}

int ddsi_sock_waitset_add (struct ddsi_sock_waitset * ws, struct ddsi_tran_conn * conn)
{
  ddsrt_socket_t handle = ddsi_conn_handle (conn);
  ddsi_sock_waitset_set_t * set = &ws->set;
  unsigned idx;
  int ret;

#if ! defined (_WIN32)
  assert (handle >= 0);
  assert (handle < FD_SETSIZE);
#endif

  ddsrt_mutex_lock (&ws->mutex);
  for (idx = 0; idx < set->n; idx++)
  {
    if (set->conns[idx] == conn)
      break;
  }
  if (idx < set->n)
    ret = 0;
  else
  {
    if (set->n == set->sz)
      ddsi_sock_waitset_grow (set);
#if ! defined (_WIN32)
    if ((int) handle >= ws->fdmax_plus_1)
      ws->fdmax_plus_1 = handle + 1;
#endif
    set->conns[set->n] = conn;
    set->fds[set->n] = handle;
    set->n++;
    ret = 1;
  }
  ddsrt_mutex_unlock (&ws->mutex);
  return ret;
}

void ddsi_sock_waitset_purge (struct ddsi_sock_waitset * ws, unsigned index)
{
  ddsi_sock_waitset_set_t * set = &ws->set;

  ddsrt_mutex_lock (&ws->mutex);
  if (index + 1 <= set->n)
  {
    for (unsigned i = index + 1; i < set->n; i++)
    {
      set->conns[i] = NULL;
      set->fds[i] = 0;
    }
    set->n = index + 1;
  }
  ddsrt_mutex_unlock (&ws->mutex);
}

void ddsi_sock_waitset_remove (struct ddsi_sock_waitset * ws, struct ddsi_tran_conn * conn)
{
  ddsi_sock_waitset_set_t * set = &ws->set;

  ddsrt_mutex_lock (&ws->mutex);
  for (unsigned i = 0; i < set->n; i++)
  {
    if (conn == set->conns[i])
    {
      set->n--;
      if (i != set->n)
      {
        set->fds[i] = set->fds[set->n];
        set->conns[i] = set->conns[set->n];
      }
      break;
    }
  }
  ddsrt_mutex_unlock (&ws->mutex);
}

struct ddsi_sock_waitset_ctx * ddsi_sock_waitset_wait (struct ddsi_sock_waitset * ws)
{
  unsigned u;
#if !_WIN32
  int fdmax;
#endif
  fd_set * rdset = NULL;
  struct ddsi_sock_waitset_ctx * ctx = &ws->ctx;
  ddsi_sock_waitset_set_t * dst = &ctx->set;
  ddsi_sock_waitset_set_t * src = &ws->set;

  ddsrt_mutex_lock (&ws->mutex);

#if !_WIN32
  fdmax = ws->fdmax_plus_1;
#endif

  /* Copy context to working context */

  while (dst->sz < src->sz)
  {
    ddsi_sock_waitset_grow (dst);
  }
  dst->n = src->n;

  for (u = 0; u < src->sz; u++)
  {
    dst->conns[u] = src->conns[u];
    dst->fds[u] = src->fds[u];
  }

  ddsrt_mutex_unlock (&ws->mutex);

  /* Copy file descriptors into select read set */

  rdset = &ctx->rdset;
  FD_ZERO (rdset);
#if !defined(LWIP_SOCKET)
  for (u = 0; u < dst->n; u++)
  {
    FD_SET (dst->fds[u], rdset);
  }
#else
  for (u = 1; u < dst->n; u++)
  {
    DDSRT_WARNING_GNUC_OFF(sign-conversion)
    FD_SET (dst->fds[u], rdset);
    DDSRT_WARNING_GNUC_ON(sign-conversion)
  }
#endif /* !defined(LWIP_SOCKET) */

  dds_return_t rc;
  do
  {
    rc = ddsrt_select (fdmax, rdset, NULL, NULL, DDS_INFINITY);
    if (rc < 0 && rc != DDS_RETCODE_INTERRUPTED && rc != DDS_RETCODE_TRY_AGAIN)
    {
      DDS_WARNING("ddsi_sock_waitset_wait: select failed, retcode = %"PRId32, rc);
      break;
    }
  } while (rc < 0);

  if (rc > 0)
  {
    /* this simply skips the trigger fd */
    ctx->index = 1;
#if !defined(LWIP_SOCKET) 
    if (FD_ISSET (dst->fds[0], rdset))
    {
      char buf;
      int n1;
#if defined (_WIN32)
      n1 = recv (dst->fds[0], &buf, 1, 0);
#else
      n1 = (int) read (dst->fds[0], &buf, 1);
#endif
      if (n1 != 1)
      {
        DDS_WARNING("ddsi_sock_waitset_wait: read failed on trigger pipe\n");
        assert (0);
      }
    }
#endif /* !defined(LWIP_SOCKET) */
    return ctx;
  }

  return NULL;
}

#if defined(LWIP_SOCKET)
DDSRT_WARNING_GNUC_OFF(sign-conversion)
#endif

int ddsi_sock_waitset_next_event (struct ddsi_sock_waitset_ctx * ctx, struct ddsi_tran_conn ** conn)
{
  while (ctx->index < ctx->set.n)
  {
    unsigned idx = ctx->index++;
    ddsrt_socket_t fd = ctx->set.fds[idx];
#if !defined(LWIP_SOCKET) /* && !defined(__ZEPHYR__) */
    assert(idx > 0);
#endif
    if (FD_ISSET (fd, &ctx->rdset))
    {
      *conn = ctx->set.conns[idx];

      return (int) (idx - 1);
    }
  }
  return -1;
}

#if defined(LWIP_SOCKET)
DDSRT_WARNING_GNUC_ON(sign-conversion)
#endif

#else
#error "no mode selected"
#endif
