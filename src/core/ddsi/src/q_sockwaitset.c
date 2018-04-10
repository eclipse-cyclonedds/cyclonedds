/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#if defined (WIN32) || defined (OSPL_LINUX)
#define FD_SETSIZE 4096
#endif

#include <assert.h>
#include <stdlib.h>

#include "os/os.h"

#include "ddsi/q_sockwaitset.h"
#include "ddsi/q_config.h"
#include "ddsi/q_log.h"
#include "ddsi/ddsi_tran.h"

#define WAITSET_DELTA 8

#ifdef __VXWORKS__
#include <pipeDrv.h>
#include <ioLib.h>
#include <string.h>
#include <selectLib.h>
#define OSPL_PIPENAMESIZE 26
#endif

#ifdef WINCE

struct os_sockWaitsetCtx
{
  ddsi_tran_conn_t conns[MAXIMUM_WAIT_OBJECTS]; /* connections and listeners */
  WSAEVENT events[MAXIMUM_WAIT_OBJECTS];        /* events associated with sockets */
  int index; /* last wakeup index, or -1 */
  unsigned n; /* sockets/events [0 .. n-1] are occupied */
};

struct os_sockWaitset
{
  os_mutex mutex;  /* concurrency guard */
  struct os_sockWaitsetCtx ctx;
  struct os_sockWaitsetCtx ctx0;
};

os_sockWaitset os_sockWaitsetNew (void)
{
  os_sockWaitset ws = os_malloc (sizeof (*ws));
  ws->ctx.conns[0] = NULL;
  ws->ctx.events[0] = WSACreateEvent ();
  ws->ctx.n = 1;
  ws->ctx.index = -1;
  os_mutexInit (&ws->mutex);
  return ws;
}

void os_sockWaitsetFree (os_sockWaitset ws)
{
  unsigned i;

  for (i = 0; i < ws->ctx.n; i++)
  {
    WSACloseEvent (ws->ctx.events[i]);
  }
  os_mutexDestroy (&ws->mutex);
  os_free (ws);
}

void os_sockWaitsetPurge (os_sockWaitset ws, unsigned index)
{
  unsigned i;

  os_mutexLock (&ws->mutex);
  for (i = index + 1; i < ws->ctx.n; i++)
  {
    ws->ctx.conns[i] = NULL;
    if (!WSACloseEvent (ws->ctx.events[i]))
    {
      NN_WARNING ("os_sockWaitsetPurge: WSACloseEvent (%x failed, error %d", (os_uint32) ws->ctx.events[i], os_getErrno ());
    }
  }
  ws->ctx.n = index + 1;
  os_mutexUnlock (&ws->mutex);
}

void os_sockWaitsetRemove (os_sockWaitset ws, ddsi_tran_conn_t conn)
{
  unsigned i;

  os_mutexLock (&ws->mutex);
  for (i = 0; i < ws->ctx.n; i++)
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
  os_mutexUnlock (&ws->mutex);
}

void os_sockWaitsetTrigger (os_sockWaitset ws)
{
  if (! WSASetEvent (ws->ctx.events[0]))
  {
    NN_WARNING ("os_sockWaitsetTrigger: WSASetEvent(%x) failed, error %d", (os_uint32) ws->ctx.events[0], os_getErrno ());
  }
}

void os_sockWaitsetAdd (os_sockWaitset ws, ddsi_tran_conn_t conn)
{
  WSAEVENT ev;
  os_socket sock = ddsi_conn_handle (conn);
  unsigned idx;

  os_mutexLock (&ws->mutex);

  for (idx = 0; idx < ws->ctx.n; idx++)
  {
    if (ws->ctx.conns[idx] == conn)
      break;
  }
  if (idx == ws->ctx.n)
  {
    assert (ws->n < MAXIMUM_WAIT_OBJECTS);

    ev = WSACreateEvent ();
    assert (ev != WSA_INVALID_EVENT);

    if (WSAEventSelect (sock, ev, FD_READ) == SOCKET_ERROR)
    {
      NN_WARNING ("os_sockWaitsetAdd: WSAEventSelect(%x,%x) failed, error %d", (os_uint32) sock, (os_uint32) ev, os_getErrno ());
      WSACloseEvent (ev);
      assert (0);
    }
    ws->ctx.conns[ws->ctx.n] = conn;
    ws->ctx.events[ws->ctx.n] = ev;
    ws->ctx.n++;
  }

  os_mutexUnlock (&ws->mutex);
}

os_sockWaitsetCtx os_sockWaitsetWait (os_sockWaitset ws)
{
  unsigned idx;

  assert (ws->index == -1);

  os_mutexLock (&ws->mutex);
  ws->ctx0 = ws->ctx;
  os_mutexUnlock (&ws->mutex);

  if ((idx = WSAWaitForMultipleEvents (ws->ctx0.n, ws->ctx0.events, FALSE, WSA_INFINITE, FALSE)) == WSA_WAIT_FAILED)
  {
    NN_WARNING ("os_sockWaitsetWait: WSAWaitForMultipleEvents(%d,...,0,0,0) failed, error %d", ws->ctx0.n, os_getErrno ());
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
    NN_WARNING ("os_sockWaitsetWait: WSAWaitForMultipleEvents(%d,...,0,0,0) returned unexpected WAIT_IO_COMPLETION", ws->ctx0.n);
  }
  else
  {
    NN_WARNING ("os_sockWaitsetWait: WSAWaitForMultipleEvents(%d,...,0,0,0) returned unrecognised %d", ws->ctx0.n, idx);
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

int os_sockWaitsetNextEvent (os_sockWaitsetCtx ctx, ddsi_tran_conn_t * conn)
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
    os_handle handle;

    ctx->index = -1;
    handle = ddsi_conn_handle (ctx->conns[idx]);
    if (WSAEnumNetworkEvents (handle, ctx->events[idx], &nwev) == SOCKET_ERROR)
    {
      int err = os_getErrno ();
      if (err != WSAENOTSOCK)
      {
        /* May have a wakeup and a close in parallel, so the handle
           need not exist anymore. */
        NN_ERROR ("os_sockWaitsetNextEvent: WSAEnumNetworkEvents(%x,%x,...) failed, error %d", (os_uint32) handle, (os_uint32) ctx->events[idx], err);
      }
      return -1;
    }

    *conn = ctx->conns[idx];
    return idx - 1;
  }
}

#else /* WINCE */

#if defined (_WIN32)

static int pipe (os_handle fd[2])
{
  struct sockaddr_in addr;
  socklen_t asize = sizeof (addr);
  os_socket listener = socket (AF_INET, SOCK_STREAM, 0);
  os_socket s1 = socket (AF_INET, SOCK_STREAM, 0);
  os_socket s2 = Q_INVALID_SOCKET;

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
  addr.sin_port = 0;
  if (bind (listener, (struct sockaddr *) &addr, sizeof (addr)) == -1)
  {
    goto fail;
  }
  if (getsockname (listener, (struct sockaddr *) &addr, &asize) == -1)
  {
    goto fail;
  }
  if (listen (listener, 1) == -1)
  {
    goto fail;
  }
  if (connect (s1, (struct sockaddr *) &addr, sizeof (addr)) == -1)
  {
    goto fail;
  }
  if ((s2 = accept (listener, 0, 0)) == -1)
  {
    goto fail;
  }

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

#else

#ifndef __VXWORKS__
#if defined (AIX) || defined (__Lynx__) || defined (__QNX__)
#include <fcntl.h>
#elif ! defined(INTEGRITY)
#include <sys/fcntl.h>
#endif
#endif /* __VXWORKS__ */

#ifndef _WRS_KERNEL
#include <sys/select.h>
#endif
#ifdef __sun
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#endif /* _WIN32 */

typedef struct os_sockWaitsetSet
{
  ddsi_tran_conn_t * conns;  /* connections in set */
  os_handle * fds;           /* file descriptors in set */
  unsigned sz;               /* max number of fds in context */
  unsigned n;                /* actual number of fds in context */
} os_sockWaitsetSet;

struct os_sockWaitsetCtx
{
  os_sockWaitsetSet set;     /* set of connections and descriptors */
  unsigned index;            /* cursor for enumerating */
  fd_set rdset;              /* read file descriptors */
};

struct os_sockWaitset
{
  os_handle pipe[2];             /* pipe used for triggering */
  os_mutex mutex;                /* concurrency guard */
  int fdmax_plus_1;              /* value for first parameter of select() */
  os_sockWaitsetSet set;         /* set of descriptors handled next */
  struct os_sockWaitsetCtx ctx;  /* set of descriptors being handled  */
};

static void os_sockWaitsetNewSet (os_sockWaitsetSet * set)
{
  set->fds = os_malloc (WAITSET_DELTA * sizeof (*set->fds));
  set->conns = os_malloc (WAITSET_DELTA * sizeof (*set->conns));
  set->sz = WAITSET_DELTA;
  set->n = 1;
}

static void os_sockWaitsetNewCtx (os_sockWaitsetCtx ctx)
{
  os_sockWaitsetNewSet (&ctx->set);
  FD_ZERO (&ctx->rdset);
}

os_sockWaitset os_sockWaitsetNew (void)
{
  int result;
  os_sockWaitset ws = os_malloc (sizeof (*ws));

  os_sockWaitsetNewSet (&ws->set);
  os_sockWaitsetNewCtx (&ws->ctx);

#if ! defined (_WIN32)
  ws->fdmax_plus_1 = 0;
#else
  ws->fdmax_plus_1 = FD_SETSIZE;
#endif

#if defined (VXWORKS_RTP) || defined (_WRS_KERNEL)
  {
    char pipename[OSPL_PIPENAMESIZE];
    int pipecount=0;
    do
    {
      snprintf ((char*)&pipename, sizeof(pipename), "/pipe/ospl%d", pipecount++ );
    }
    while ((result = pipeDevCreate ((char*) &pipename, 1, 1)) == -1 && os_getErrno() == EINVAL);
    if (result != -1)
    {
      result = open ((char*) &pipename, O_RDWR, 0644);
      if (result != -1)
      {
        ws->pipe[0] = result;
        result =open ((char*) &pipename, O_RDWR, 0644);
        if (result != -1)
        {
          ws->pipe[1] = result;
        }
        else
        {
          close (ws->pipe[0]);
          pipeDevDelete (pipename, 0);
        }
      }
    }
  }
#else
  result = pipe (ws->pipe);
#endif
  assert (result != -1);
  (void) result;

  ws->set.fds[0] = ws->pipe[0];
  ws->set.conns[0] = NULL;

#if ! defined (VXWORKS_RTP) && ! defined ( _WRS_KERNEL ) && ! defined (_WIN32)
  fcntl (ws->pipe[0], F_SETFD, fcntl (ws->pipe[0], F_GETFD) | FD_CLOEXEC);
  fcntl (ws->pipe[1], F_SETFD, fcntl (ws->pipe[1], F_GETFD) | FD_CLOEXEC);
#endif
  FD_SET (ws->set.fds[0], &ws->ctx.rdset);
#if ! defined (_WIN32)
  ws->fdmax_plus_1 = ws->set.fds[0] + 1;
#endif

  os_mutexInit (&ws->mutex);

  return ws;
}

static void os_sockWaitsetGrow (os_sockWaitsetSet * set)
{
  set->sz += WAITSET_DELTA;
  set->conns = os_realloc (set->conns, set->sz * sizeof (*set->conns));
  set->fds = os_realloc (set->fds, set->sz * sizeof (*set->fds));
}

static void os_sockWaitsetFreeSet (os_sockWaitsetSet * set)
{
  os_free (set->fds);
  os_free (set->conns);
}

static void os_sockWaitsetFreeCtx (os_sockWaitsetCtx ctx)
{
  os_sockWaitsetFreeSet (&ctx->set);
}

void os_sockWaitsetFree (os_sockWaitset ws)
{
#ifdef VXWORKS_RTP
  char nameBuf[OSPL_PIPENAMESIZE];
  ioctl (ws->pipe[0], FIOGETNAME, &nameBuf);
#endif
#if defined (_WIN32)
  closesocket (ws->pipe[0]);
  closesocket (ws->pipe[1]);
#else
  close (ws->pipe[0]);
  close (ws->pipe[1]);
#endif
#ifdef VXWORKS_RTP
  pipeDevDelete ((char*) &nameBuf, 0);
#endif
  os_sockWaitsetFreeSet (&ws->set);
  os_sockWaitsetFreeCtx (&ws->ctx);
  os_mutexDestroy (&ws->mutex);
  os_free (ws);
}

void os_sockWaitsetTrigger (os_sockWaitset ws)
{
  char buf = 0;
  int n;
  int err;

#if defined (_WIN32)
  n = send (ws->pipe[1], &buf, 1, 0);
#else
  n = (int) write (ws->pipe[1], &buf, 1);
#endif
  if (n != 1)
  {
    err = os_getErrno ();
    NN_WARNING ("os_sockWaitsetTrigger: read failed on trigger pipe, errno = %d", err);
  }
}

void os_sockWaitsetAdd (os_sockWaitset ws, ddsi_tran_conn_t conn)
{
  os_handle handle = ddsi_conn_handle (conn);
  os_sockWaitsetSet * set = &ws->set;
  unsigned idx;

  assert (handle >= 0);
#if ! defined (_WIN32)
  assert (handle < FD_SETSIZE);
#endif

  os_mutexLock (&ws->mutex);
  for (idx = 0; idx < set->n; idx++)
  {
    if (set->conns[idx] == conn)
      break;
  }
  if (idx == set->n)
  {
    if (set->n == set->sz)
    {
      os_sockWaitsetGrow (set);
    }
#if ! defined (_WIN32)
    if ((int) handle >= ws->fdmax_plus_1)
    {
      ws->fdmax_plus_1 = handle + 1;
    }
#endif
    set->conns[set->n] = conn;
    set->fds[set->n] = handle;
    set->n++;
  }
  os_mutexUnlock (&ws->mutex);
}

void os_sockWaitsetPurge (os_sockWaitset ws, unsigned index)
{
  unsigned i;
  os_sockWaitsetSet * set = &ws->set;

  os_mutexLock (&ws->mutex);
  if (index + 1 <= set->n)
  {
    for (i = index + 1; i < set->n; i++)
    {
      set->conns[i] = NULL;
      set->fds[i] = 0;
    }
    set->n = index + 1;
  }
  os_mutexUnlock (&ws->mutex);
}

void os_sockWaitsetRemove (os_sockWaitset ws, ddsi_tran_conn_t conn)
{
  unsigned i;
  os_sockWaitsetSet * set = &ws->set;

  os_mutexLock (&ws->mutex);
  for (i = 0; i < set->n; i++)
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
  os_mutexUnlock (&ws->mutex);
}

os_sockWaitsetCtx os_sockWaitsetWait (os_sockWaitset ws)
{
  int n;
  unsigned u;
  int err;
  int fdmax;
  fd_set * rdset = NULL;
  os_sockWaitsetCtx ctx = &ws->ctx;
  os_sockWaitsetSet * dst = &ctx->set;
  os_sockWaitsetSet * src = &ws->set;

  os_mutexLock (&ws->mutex);

  fdmax = ws->fdmax_plus_1;

  /* Copy context to working context */

  while (dst->sz < src->sz)
  {
    os_sockWaitsetGrow (dst);
  }
  dst->n = src->n;

  for (u = 0; u < src->sz; u++)
  {
    dst->conns[u] = src->conns[u];
    dst->fds[u] = src->fds[u];
  }

  os_mutexUnlock (&ws->mutex);

  /* Copy file descriptors into select read set */

  rdset = &ctx->rdset;
  FD_ZERO (rdset);
  for (u = 0; u < dst->n; u++)
  {
    FD_SET (dst->fds[u], rdset);
  }

  do
  {
    n = select (fdmax, rdset, NULL, NULL, NULL);
    if (n < 0)
    {
      err = os_getErrno ();
      if ((err != os_sockEINTR) && (err != os_sockEAGAIN))
      {
        NN_WARNING ("os_sockWaitsetWait: select failed, errno = %d", err);
        break;
      }
    }
  }
  while (n == -1);

  if (n > 0)
  {
    /* this simply skips the trigger fd */
    ctx->index = 1;
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
        err = os_getErrno ();
        NN_WARNING ("os_sockWaitsetWait: read failed on trigger pipe, errno = %d", err);
        assert (0);
      }
    }
    return ctx;
  }

  return NULL;
}

int os_sockWaitsetNextEvent (os_sockWaitsetCtx ctx, ddsi_tran_conn_t * conn)
{
  while (ctx->index < ctx->set.n)
  {
    unsigned idx = ctx->index++;
    os_handle fd = ctx->set.fds[idx];
    assert(idx > 0);
    if (FD_ISSET (fd, &ctx->rdset))
    {
      *conn = ctx->set.conns[idx];

      return (int) (idx - 1);
    }
  }
  return -1;
}
#endif /* WINCE */
