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
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/sync.h"

#include "dds/ddsi/q_sockwaitset.h"
#include "dds/ddsi/ddsi_config_impl.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/ddsi_tran.h"

#define WAITSET_DELTA 8

#define MODE_KQUEUE 1
#define MODE_SELECT 2
#define MODE_WFMEVS 3

#if defined __APPLE__
#define MODE_SEL MODE_KQUEUE
#elif defined WINCE
#define MODE_SEL MODE_WFMEVS
#else
#define MODE_SEL MODE_SELECT    /* FreeRTOS always use select */
#endif


#ifdef DDSRT_WITH_FREERTOSTCP
# warning " *** FreeRTOS-Plus-TCP debug include tree "

#else
#if !_WIN32 && !LWIP_SOCKET

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

#endif /* !_WIN32 && !LWIP_SOCKET */
#endif

typedef struct os_sockWaitsetSet
{
  ddsi_tran_conn_t * conns;  /* connections in set */
  ddsrt_socket_t * fds;           /* file descriptors in set */
  unsigned sz;               /* max number of fds in context */
  unsigned n;                /* actual number of fds in context */
} os_sockWaitsetSet;

struct os_sockWaitsetCtx
{
  os_sockWaitsetSet set;     /* set of connections and descriptors */
  unsigned index;            /* cursor for enumerating */
  ddsrt_fd_set_t rdset;      /* read file descriptors set */
};

struct os_sockWaitset
{
  ddsrt_socket_t pipe[2];             /* pipe used for triggering */
  ddsrt_mutex_t mutex;                /* concurrency guard */
  int fdmax_plus_1;              /* value for first parameter of select() */
  os_sockWaitsetSet set;         /* set of descriptors handled next */
  struct os_sockWaitsetCtx ctx;  /* set of descriptors being handled  */
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
  #ifdef DDSRT_WITH_FREERTOSTCP
  addr.sin_addr = htonl (INADDR_LOOPBACK);
  #else
  addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
  #endif
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
#elif !defined(LWIP_SOCKET)
static int make_pipe (int pfd[2])
{
  return pipe (pfd);
}
#endif

static void os_sockWaitsetNewSet (os_sockWaitsetSet * set)
{
  set->fds = ddsrt_malloc (WAITSET_DELTA * sizeof (*set->fds));
  set->conns = ddsrt_malloc (WAITSET_DELTA * sizeof (*set->conns));
  set->sz = WAITSET_DELTA;
  set->n = 1;
}

static void os_sockWaitsetFreeSet (os_sockWaitsetSet * set)
{
  ddsrt_free (set->fds);
  ddsrt_free (set->conns);
}

static void os_sockWaitsetNewCtx (os_sockWaitsetCtx ctx)
{
  os_sockWaitsetNewSet (&ctx->set);

  #ifdef DDSRT_WITH_FREERTOSTCP
  ctx->rdset = DDSRT_FD_SET_CRATE();
  DDSRT_FD_ZERO (ctx->rdset);
  DDS_WARNING("os_sockWaitsetNewCtx: rdset created! rdset %p \n", ctx->rdset);
  #else
  FD_ZERO (ctx->rdset);
  #endif
}

static void os_sockWaitsetFreeCtx (os_sockWaitsetCtx ctx)
{
  #ifdef DDSRT_WITH_FREERTOSTCP
  DDSRT_FD_SET_DELETE(ctx->rdset);
  #endif
  os_sockWaitsetFreeSet (&ctx->set);
}

os_sockWaitset os_sockWaitsetNew (void)
{
  int result;
  os_sockWaitset ws = ddsrt_malloc (sizeof (*ws));

  os_sockWaitsetNewSet (&ws->set);
  os_sockWaitsetNewCtx (&ws->ctx);

#if ! defined (_WIN32)
  ws->fdmax_plus_1 = 0;
#else
  ws->fdmax_plus_1 = FD_SETSIZE;
#endif

#if defined(LWIP_SOCKET)
  ws->pipe[0] = -1;
  ws->pipe[1] = -1;
  result = 0;
#elif defined(DDSRT_WITH_FREERTOSTCP)
    # warning " *** FreeRTOS-Plus-TCP FreeRTOS_Plus_TCP runtime wrapper ..."
    ws->pipe[0] = -1;
    ws->pipe[1] = -1;
    result = 0;
#else
    # error " *** NO makepipe for FreeRTOS "

  result = make_pipe (ws->pipe);
#endif
  if (result == -1)
  {
    os_sockWaitsetFreeCtx (&ws->ctx);
    os_sockWaitsetFreeSet (&ws->set);
    ddsrt_free (ws);
    return NULL;
  }

#if !defined(LWIP_SOCKET) && !defined(DDSRT_WITH_FREERTOSTCP)
  ws->set.fds[0] = ws->pipe[0];
#else
#warning " *** FreeRTOS-Plus-TCP FreeRTOS_Plus_TCP runtime wrapper ..."
  ws->set.fds[0] = 0;
#endif
  ws->set.conns[0] = NULL;

#if !defined(__VXWORKS__) && !defined(_WIN32) && !defined(LWIP_SOCKET) && !defined(DDSRT_WITH_FREERTOSTCP) && !defined(__QNXNTO__)
  (void) fcntl (ws->pipe[0], F_SETFD, fcntl (ws->pipe[0], F_GETFD) | FD_CLOEXEC);
  (void) fcntl (ws->pipe[1], F_SETFD, fcntl (ws->pipe[1], F_GETFD) | FD_CLOEXEC);
#endif

#if !defined(LWIP_SOCKET) && !defined(DDSRT_WITH_FREERTOSTCP)
  FD_SET (ws->set.fds[0], ws->ctx.rdset);
#endif

#if !defined(_WIN32) && !defined(DDSRT_WITH_FREERTOSTCP)
  ws->fdmax_plus_1 = ws->set.fds[0] + 1;
#endif

  ddsrt_mutex_init (&ws->mutex);

  return ws;
}

static void os_sockWaitsetGrow (os_sockWaitsetSet * set)
{
  set->sz += WAITSET_DELTA;
  set->conns = ddsrt_realloc (set->conns, set->sz * sizeof (*set->conns));
  set->fds = ddsrt_realloc (set->fds, set->sz * sizeof (*set->fds));
}

void os_sockWaitsetFree (os_sockWaitset ws)
{
#if defined(__VXWORKS__) && defined(__RTP__)
  char nameBuf[OSPL_PIPENAMESIZE];
  ioctl (ws->pipe[0], FIOGETNAME, &nameBuf);
#endif

#if defined(DDSRT_WITH_FREERTOSTCP)
# warning " *** FreeRTOS-Plus-TCP runtime tree "
  (void) ddsrt_close (ws->pipe[0]);
  (void) ddsrt_close (ws->pipe[1]);
#else

#if defined(_WIN32)
  closesocket (ws->pipe[0]);
  closesocket (ws->pipe[1]);
#elif !defined(LWIP_SOCKET)
  (void) close (ws->pipe[0]);
  (void) close (ws->pipe[1]);
#endif
#endif

#if defined(__VXWORKS__) && defined(__RTP__)
  pipeDevDelete ((char*) &nameBuf, 0);
#endif
  os_sockWaitsetFreeSet (&ws->set);
  os_sockWaitsetFreeCtx (&ws->ctx);
  ddsrt_mutex_destroy (&ws->mutex);
  ddsrt_free (ws);
}

void os_sockWaitsetTrigger (os_sockWaitset ws)
{
#if defined(LWIP_SOCKET)
  (void)ws;
#elif defined (DDSRT_WITH_FREERTOSTCP)
#warning " *** FreeRTOS-Plus-TCP FreeRTOS_Plus_TCP runtime wrapper ..."
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
    DDS_WARNING("os_sockWaitsetTrigger: write failed on trigger pipe\n");
  }
#endif
}

int os_sockWaitsetAdd (os_sockWaitset ws, ddsi_tran_conn_t conn)
{
  ddsrt_socket_t handle = ddsi_conn_handle (conn);
  os_sockWaitsetSet * set = &ws->set;
  unsigned idx;
  int ret;

#if ! defined (_WIN32) && ! defined(DDSRT_WITH_FREERTOSTCP)
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
  {  ret = 0; }
  else
  {
    if (set->n == set->sz)
    {  os_sockWaitsetGrow (set); }
#if ! defined (_WIN32) && ! defined(DDSRT_WITH_FREERTOSTCP)
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

void os_sockWaitsetPurge (os_sockWaitset ws, unsigned index)
{
  os_sockWaitsetSet * set = &ws->set;

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

void os_sockWaitsetRemove (os_sockWaitset ws, ddsi_tran_conn_t conn)
{
  os_sockWaitsetSet * set = &ws->set;

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

os_sockWaitsetCtx os_sockWaitsetWait (os_sockWaitset ws)
{
  unsigned u;
#if !_WIN32
  int fdmax;
#endif
  ddsrt_fd_set_t rdset = NULL;
  os_sockWaitsetCtx ctx = &ws->ctx;
  os_sockWaitsetSet * dst = &ctx->set;
  os_sockWaitsetSet * src = &ws->set;

  ddsrt_mutex_lock (&ws->mutex);

#ifdef DDSRT_WITH_FREERTOSTCP
  fdmax = 0;    /* For FreeRTOS_TCP stack, fdmax is stub for api compatible */
#else
 #if !_WIN32
    fdmax = ws->fdmax_plus_1;
 #endif
#endif

  /* Copy context to working context */

  while (dst->sz < src->sz)
  {
    os_sockWaitsetGrow (dst);
  }
  dst->n = src->n;

  for (u = 0; u < src->n; u++)
  {
    dst->conns[u] = src->conns[u];
    dst->fds[u] = src->fds[u];
  }

  ddsrt_mutex_unlock (&ws->mutex);


  /* Copy file descriptors into select read set */
  rdset = ctx->rdset;
#ifdef DDSRT_WITH_FREERTOSTCP
  if (rdset == NULL)
  {
    assert(0);
  }

#endif

  DDSRT_FD_ZERO (rdset);
#if !defined(LWIP_SOCKET) && !defined(DDSRT_WITH_FREERTOSTCP)
  for (u = 0; u < dst->n; u++)
  {
    FD_SET (dst->fds[u], rdset);
  }
#else
  /* fds[0]/conns[0] not using for RTOS */
  for (u = 1; u < dst->n; u++)
  {
    DDSRT_WARNING_GNUC_OFF(sign-conversion)
    DDSRT_FD_SET (dst->fds[u], rdset);
    DDSRT_WARNING_GNUC_ON(sign-conversion)
  }
#endif /* LWIP_SOCKET */

  dds_return_t rc;
  do
  {
    rc = ddsrt_select (fdmax, rdset, NULL, NULL, SELECT_TIMEOUT_MS);
    if (rc < 0 && rc != DDS_RETCODE_INTERRUPTED && rc != DDS_RETCODE_TRY_AGAIN)
    {
      DDS_WARNING("os_sockWaitsetWait: select failed, retcode = %"PRId32, rc);
      break;
    }
  } while (rc < 0);

  if (rc > 0)
  {
    /* set start VALID conn index
     * this simply skips the trigger fd, index0 is INV.
     */
    ctx->index = 1;

    /* to confirm for DDSRT_WITH_FREERTOSTCP
        TODO
        TODO
        TODO
        TODO
        TODO
        TODO
    */
#if ! defined(LWIP_SOCKET) && !defined(DDSRT_WITH_FREERTOSTCP)
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
        DDS_WARNING("os_sockWaitsetWait: read failed on trigger pipe\n");
        assert (0);
      }
    }
#endif /* LWIP_SOCKET */
    return ctx;
  }

  return NULL;
}

int os_sockWaitsetNextEvent (os_sockWaitsetCtx ctx, ddsi_tran_conn_t * conn)
{
  while (ctx->index < ctx->set.n)
  {
    unsigned idx = ctx->index++;
    ddsrt_socket_t fd = ctx->set.fds[idx];
#if ! defined (LWIP_SOCKET) && ! defined(DDSRT_WITH_FREERTOSTCP)
    assert(idx > 0);
#endif

    if (DDSRT_FD_ISSET (fd, ctx->rdset))
    {
      *conn = ctx->set.conns[idx];

      return (int) (idx - 1);
    }
  }
  return -1;
}


