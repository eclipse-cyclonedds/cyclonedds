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
#include <stddef.h>
#include <assert.h>
#include <string.h>

#include "ddsi_eth.h"
#include "ddsi/ddsi_tran.h"
#include "ddsi/ddsi_tcp.h"
#include "ddsi/ddsi_ipaddr.h"
#include "util/ut_avl.h"
#include "ddsi/q_nwif.h"
#include "ddsi/q_config.h"
#include "ddsi/q_log.h"
#include "ddsi/q_entity.h"
#include "os/os.h"

#define INVALID_PORT (~0u)

typedef struct ddsi_tran_factory * ddsi_tcp_factory_g_t;
static os_atomic_uint32_t ddsi_tcp_init_g = OS_ATOMIC_UINT32_INIT(0);

#ifdef DDSI_INCLUDE_SSL
static struct ddsi_ssl_plugins ddsi_tcp_ssl_plugin;
#endif

static const char * ddsi_name = "tcp";

/*
  ddsi_tcp_conn: TCP connection for reading and writing. Mutex prevents concurrent
  writes to socket. Is reference counted. Peer port is actually contained in peer
  address but is extracted for convenience and for faster cache lookup
  (see ddsi_tcp_cmp_entry). Where connection is server side socket (for bi-dir)
  is flagged as such to avoid connection attempts and for same reason, on failure,
  is not removed from cache but simply flagged as failed (may be subsequently
  replaced). Similarly server side sockets are not closed as are also used in socket
  wait set that manages their lifecycle.
*/

typedef struct ddsi_tcp_conn
{
  struct ddsi_tran_conn m_base;
  os_sockaddr_storage m_peer_addr;
  uint32_t m_peer_port;
  os_mutex m_mutex;
  os_socket m_sock;
#ifdef DDSI_INCLUDE_SSL
  SSL * m_ssl;
#endif
}
* ddsi_tcp_conn_t;

typedef struct ddsi_tcp_listener
{
  struct ddsi_tran_listener m_base;
  os_socket m_sock;
#ifdef DDSI_INCLUDE_SSL
  BIO * m_bio;
#endif
}
* ddsi_tcp_listener_t;

/* Stateless singleton instance handed out as client connection */

static struct ddsi_tcp_conn ddsi_tcp_conn_client;

static int ddsi_tcp_cmp_conn (const struct ddsi_tcp_conn *c1, const struct ddsi_tcp_conn *c2)
{
  const os_sockaddr *a1s = (os_sockaddr *)&c1->m_peer_addr;
  const os_sockaddr *a2s = (os_sockaddr *)&c2->m_peer_addr;
  if (a1s->sa_family != a2s->sa_family)
   return (a1s->sa_family < a2s->sa_family) ? -1 : 1;
  else if (c1->m_peer_port != c2->m_peer_port)
    return (c1->m_peer_port < c2->m_peer_port) ? -1 : 1;
  return ddsi_ipaddr_compare (a1s, a2s);
}

static int ddsi_tcp_cmp_conn_wrap (const void *a, const void *b)
{
  return ddsi_tcp_cmp_conn (a, b);
}

typedef struct ddsi_tcp_node
{
  ut_avlNode_t m_avlnode;
  ddsi_tcp_conn_t m_conn;
}
* ddsi_tcp_node_t;

static const ut_avlTreedef_t ddsi_tcp_treedef = UT_AVL_TREEDEF_INITIALIZER_INDKEY
(
  offsetof (struct ddsi_tcp_node, m_avlnode),
  offsetof (struct ddsi_tcp_node, m_conn),
  ddsi_tcp_cmp_conn_wrap,
  0
);

static os_mutex ddsi_tcp_cache_lock_g;
static ut_avlTree_t ddsi_tcp_cache_g;
static struct ddsi_tran_factory ddsi_tcp_factory_g;

static ddsi_tcp_conn_t ddsi_tcp_new_conn (os_socket, bool, os_sockaddr *);

static char *sockaddr_to_string_with_port (char *dst, size_t sizeof_dst, const os_sockaddr *src)
{
  nn_locator_t loc;
  ddsi_ipaddr_to_loc(&loc, src, src->sa_family == AF_INET ? NN_LOCATOR_KIND_TCPv4 : NN_LOCATOR_KIND_TCPv6);
  ddsi_locator_to_string(dst, sizeof_dst, &loc);
  return dst;
}

/* Connection cache dump routine for debugging

static void ddsi_tcp_cache_dump (void)
{
  char buff[64];
  ut_avlIter_t iter;
  ddsi_tcp_node_t n;
  unsigned i = 0;

  n = ut_avlIterFirst (&ddsi_tcp_treedef, &ddsi_tcp_cache_g, &iter);
  while (n)
  {
    os_sockaddrAddressPortToString ((const os_sockaddr *) &n->m_conn->m_peer_addr, buff, sizeof (buff));
    DDS_TRACE
    (
      DDS_LC_TCP,
      "%s cache #%d: %s sock %d port %u peer %s\n",
      ddsi_name, i++, n->m_conn->m_base.m_server ? "server" : "client",
      n->m_conn->m_sock, n->m_conn->m_base.m_base.m_port, buff
    );
    n = ut_avlIterNext (&iter);
  }
}
*/

static unsigned short get_socket_port (os_socket socket)
{
  os_sockaddr_storage addr;
  socklen_t addrlen = sizeof (addr);
  if (getsockname (socket, (os_sockaddr *) &addr, &addrlen) < 0)
  {
    int err = os_getErrno();
    DDS_ERROR("ddsi_tcp_get_socket_port: getsockname errno %d\n", err);
    return 0;
  }
  return os_sockaddr_get_port((os_sockaddr *)&addr);
}

static void ddsi_tcp_conn_set_socket (ddsi_tcp_conn_t conn, os_socket sock)
{
  conn->m_sock = sock;
  conn->m_base.m_base.m_port = (sock == OS_INVALID_SOCKET) ? INVALID_PORT : get_socket_port (sock);
}

static void ddsi_tcp_sock_free (os_socket sock, const char * msg)
{
  if (sock != OS_INVALID_SOCKET)
  {
    if (msg)
    {
      DDS_LOG(DDS_LC_TCP, "%s %s free socket %"PRIsock"\n", ddsi_name, msg, sock);
    }
    os_sockFree (sock);
  }
}

static void ddsi_tcp_sock_new (os_socket * sock, unsigned short port)
{
  if (make_socket (sock, port, true, true) != 0)
  {
    *sock = OS_INVALID_SOCKET;
  }
}

static void ddsi_tcp_node_free (void * ptr)
{
  ddsi_tcp_node_t node = (ddsi_tcp_node_t) ptr;
  ddsi_conn_free ((ddsi_tran_conn_t) node->m_conn);
  os_free (node);
}

static void ddsi_tcp_conn_connect (ddsi_tcp_conn_t conn, const struct msghdr * msg)
{
  int ret;
  char buff[DDSI_LOCSTRLEN];
  os_socket sock;

  ddsi_tcp_sock_new (&sock, 0);
  if (sock != OS_INVALID_SOCKET)
  {
    /* Attempt to connect, expected that may fail */

    do
    {
      ret = connect (sock, msg->msg_name, msg->msg_namelen);
    }
    while ((ret == -1) && (os_getErrno() == os_sockEINTR));

    if (ret != 0)
    {
      ddsi_tcp_sock_free (sock, NULL);
      return;
    }
    ddsi_tcp_conn_set_socket (conn, sock);

#ifdef DDSI_INCLUDE_SSL
    if (ddsi_tcp_ssl_plugin.connect)
    {
      conn->m_ssl = (ddsi_tcp_ssl_plugin.connect) (sock);
      if (conn->m_ssl == NULL)
      {
        ddsi_tcp_conn_set_socket (conn, OS_INVALID_SOCKET);
        return;
      }
    }
#endif

    sockaddr_to_string_with_port(buff, sizeof(buff), (os_sockaddr *) msg->msg_name);
    DDS_LOG(DDS_LC_TCP, "%s connect socket %"PRIsock" port %u to %s\n", ddsi_name, sock, get_socket_port (sock), buff);

    /* Also may need to receive on connection so add to waitset */

    os_sockSetNonBlocking (conn->m_sock, true);

    assert (gv.n_recv_threads > 0);
    assert (gv.recv_threads[0].arg.mode == RTM_MANY);
    os_sockWaitsetAdd (gv.recv_threads[0].arg.u.many.ws, &conn->m_base);
    os_sockWaitsetTrigger (gv.recv_threads[0].arg.u.many.ws);
  }
}

static void ddsi_tcp_cache_add (ddsi_tcp_conn_t conn, ut_avlIPath_t * path)
{
  const char * action = "added";
  ddsi_tcp_node_t node;
  char buff[DDSI_LOCSTRLEN];

  os_atomic_inc32 (&conn->m_base.m_count);

  /* If path set, then cache does not contain connection */
  if (path)
  {
    node = os_malloc (sizeof (*node));
    node->m_conn = conn;
    ut_avlInsertIPath (&ddsi_tcp_treedef, &ddsi_tcp_cache_g, node, path);
  }
  else
  {
    node = ut_avlLookup (&ddsi_tcp_treedef, &ddsi_tcp_cache_g, conn);
    if (node)
    {
      /* Replace connection in cache */

      ddsi_conn_free ((ddsi_tran_conn_t) node->m_conn);
      node->m_conn = conn;
      action = "updated";
    }
    else
    {
      node = os_malloc (sizeof (*node));
      node->m_conn = conn;
      ut_avlInsert (&ddsi_tcp_treedef, &ddsi_tcp_cache_g, node);
    }
  }

  sockaddr_to_string_with_port(buff, sizeof(buff), (os_sockaddr *)&conn->m_peer_addr);
  DDS_LOG(DDS_LC_TCP, "%s cache %s %s socket %"PRIsock" to %s\n", ddsi_name, action, conn->m_base.m_server ? "server" : "client", conn->m_sock, buff);
}

static void ddsi_tcp_cache_remove (ddsi_tcp_conn_t conn)
{
  char buff[DDSI_LOCSTRLEN];
  ddsi_tcp_node_t node;
  ut_avlDPath_t path;

  os_mutexLock (&ddsi_tcp_cache_lock_g);
  node = ut_avlLookupDPath (&ddsi_tcp_treedef, &ddsi_tcp_cache_g, conn, &path);
  if (node)
  {
    sockaddr_to_string_with_port(buff, sizeof(buff), (os_sockaddr *)&conn->m_peer_addr);
    DDS_LOG(DDS_LC_TCP, "%s cache removed socket %"PRIsock" to %s\n", ddsi_name, conn->m_sock, buff);
    ut_avlDeleteDPath (&ddsi_tcp_treedef, &ddsi_tcp_cache_g, node, &path);
    ddsi_tcp_node_free (node);
  }
  os_mutexUnlock (&ddsi_tcp_cache_lock_g);
}

/*
  ddsi_tcp_cache_find: Find existing connection to target, or if possible
  create new connection.
*/

static ddsi_tcp_conn_t ddsi_tcp_cache_find (const struct msghdr * msg)
{
  ut_avlIPath_t path;
  ddsi_tcp_node_t node;
  struct ddsi_tcp_conn key;
  ddsi_tcp_conn_t ret = NULL;

  memset (&key, 0, sizeof (key));
  key.m_peer_port = os_sockaddr_get_port (msg->msg_name);
  memcpy (&key.m_peer_addr, msg->msg_name, msg->msg_namelen);

  /* Check cache for existing connection to target */

  os_mutexLock (&ddsi_tcp_cache_lock_g);
  node = ut_avlLookupIPath (&ddsi_tcp_treedef, &ddsi_tcp_cache_g, &key, &path);
  if (node)
  {
    if (node->m_conn->m_base.m_closed)
    {
      ut_avlDelete (&ddsi_tcp_treedef, &ddsi_tcp_cache_g, node);
      ddsi_tcp_node_free (node);
    }
    else
    {
      ret = node->m_conn;
    }
  }
  if (ret == NULL)
  {
    ret = ddsi_tcp_new_conn (OS_INVALID_SOCKET, false, (os_sockaddr *)&key.m_peer_addr);
    ddsi_tcp_cache_add (ret, &path);
  }
  os_mutexUnlock (&ddsi_tcp_cache_lock_g);

  return ret;
}

static ssize_t ddsi_tcp_conn_read_plain (ddsi_tcp_conn_t tcp, void * buf, size_t len, int * err)
{
OS_WARNING_MSVC_OFF(4267);
  ssize_t ret = recv (tcp->m_sock, buf, len, 0);
  *err = (ret == -1) ? os_getErrno () : 0;
  return ret;
OS_WARNING_MSVC_ON(4267);
}

#ifdef DDSI_INCLUDE_SSL
static ssize_t ddsi_tcp_conn_read_ssl (ddsi_tcp_conn_t tcp, void * buf, size_t len, int * err)
{
  return (ddsi_tcp_ssl_plugin.read) (tcp->m_ssl, buf, len, err);
}
#endif

static bool ddsi_tcp_select (os_socket sock, bool read, size_t pos)
{
  int ret;
  fd_set fds;
  os_time timeout;
  fd_set * rdset = read ? &fds : NULL;
  fd_set * wrset = read ? NULL : &fds;
  int64_t tval = read ? config.tcp_read_timeout : config.tcp_write_timeout;

  FD_ZERO (&fds);
  FD_SET (sock, &fds);
  timeout.tv_sec = (int) (tval / T_SECOND);
  timeout.tv_nsec = (int) (tval % T_SECOND);

  DDS_LOG(DDS_LC_TCP, "%s blocked %s: sock %d\n", ddsi_name, read ? "read" : "write", (int) sock);
  do
  {
    ret = os_sockSelect ((int32_t)sock + 1, rdset, wrset, NULL, &timeout); /* The variable "sock" with os_socket type causes the possible loss of data. So type casting done */
  }
  while (ret == -1 && os_getErrno () == os_sockEINTR);

  if (ret <= 0)
  {
    DDS_WARNING
    (
      "%s abandoning %s on blocking socket %d after %"PRIuSIZE" bytes\n",
      ddsi_name, read ? "read" : "write", (int) sock, pos
    );
  }

  return (ret > 0);
}

static int err_is_AGAIN_or_WOULDBLOCK (int err)
{
  if (err == os_sockEAGAIN)
    return 1;
  if (err == os_sockEWOULDBLOCK)
    return 1;
  return 0;
}

static ssize_t ddsi_tcp_conn_read (ddsi_tran_conn_t conn, unsigned char * buf, size_t len, bool allow_spurious, nn_locator_t *srcloc)
{
  ddsi_tcp_conn_t tcp = (ddsi_tcp_conn_t) conn;
  ssize_t (*rd) (ddsi_tcp_conn_t, void *, size_t, int * err) = ddsi_tcp_conn_read_plain;
  size_t pos = 0;
  ssize_t n;
  int err;

#ifdef DDSI_INCLUDE_SSL
  if (ddsi_tcp_ssl_plugin.read)
  {
    rd = ddsi_tcp_conn_read_ssl;
  }
#endif

  while (true)
  {
    n = rd (tcp, (char *) buf + pos, len - pos, &err);
    if (n > 0)
    {
      pos += (size_t) n;
      if (pos == len)
      {
        if (srcloc)
        {
          ddsi_ipaddr_to_loc(srcloc, (os_sockaddr *)&tcp->m_peer_addr, tcp->m_peer_addr.ss_family == AF_INET ? NN_LOCATOR_KIND_TCPv4 : NN_LOCATOR_KIND_TCPv6);
        }
        return (ssize_t) pos;
      }
    }
    else if (n == 0)
    {
      DDS_LOG(DDS_LC_TCP, "%s read: sock %"PRIsock" closed-by-peer\n", ddsi_name, tcp->m_sock);
      break;
    }
    else
    {
      if (err != os_sockEINTR)
      {
        if (err_is_AGAIN_or_WOULDBLOCK (err))
        {
          if (allow_spurious && pos == 0)
            return 0;
          else if (ddsi_tcp_select (tcp->m_sock, true, pos) == false)
            break;
        }
        else
        {
          DDS_LOG(DDS_LC_TCP, "%s read: sock %"PRIsock" error %d\n", ddsi_name, tcp->m_sock, err);
          break;
        }
      }
    }
  }

  ddsi_tcp_cache_remove (tcp);
  return -1;
}

static ssize_t ddsi_tcp_conn_write_plain (ddsi_tcp_conn_t conn, const void * buf, size_t len, int * err)
{
  ssize_t ret;
  int sendflags = 0;

#ifdef MSG_NOSIGNAL
  sendflags |= MSG_NOSIGNAL;
#endif
OS_WARNING_MSVC_OFF(4267);
  ret = send (conn->m_sock, buf, len, sendflags);
  *err = (ret == -1) ? os_getErrno () : 0;
  return ret;
  OS_WARNING_MSVC_ON(4267);
}

#ifdef DDSI_INCLUDE_SSL
static ssize_t ddsi_tcp_conn_write_ssl (ddsi_tcp_conn_t conn, const void * buf, size_t len, int * err)
{
  return (ddsi_tcp_ssl_plugin.write) (conn->m_ssl, buf, len, err);
}
#endif

static ssize_t ddsi_tcp_block_write
(
  ssize_t (*wr) (ddsi_tcp_conn_t, const void *, size_t, int *),
  ddsi_tcp_conn_t conn,
  const void * buf,
  size_t sz
)
{
  /* Write all bytes of buf even in the presence of signals,
     partial writes and blocking (typically write buffer full) */

  size_t pos = 0;
  ssize_t n;
  int err;

  while (pos != sz)
  {
    n = (wr) (conn, (const char *) buf + pos, sz - pos, &err);
    if (n > 0)
    {
      pos += (size_t) n;
    }
    else if (n == -1)
    {
      if (err != os_sockEINTR)
      {
        if (err_is_AGAIN_or_WOULDBLOCK (err))
        {
          if (ddsi_tcp_select (conn->m_sock, false, pos) == false)
          {
            break;
          }
        }
        else
        {
          DDS_LOG(DDS_LC_TCP, "%s write: sock %"PRIsock" error %d\n", ddsi_name, conn->m_sock, err);
          break;
        }
      }
    }
  }

  return (pos == sz) ? (ssize_t) pos : -1;
}

static size_t iovlen_sum (size_t niov, const os_iovec_t *iov)
{
  size_t tot = 0;
  while (niov--) {
    tot += iov++->iov_len;
  }
  return tot;
}

static void set_msghdr_iov (struct msghdr *mhdr, os_iovec_t *iov, size_t iovlen)
{
  mhdr->msg_iov = iov;
  mhdr->msg_iovlen = (os_msg_iovlen_t)iovlen;
}

static ssize_t ddsi_tcp_conn_write (ddsi_tran_conn_t base, const nn_locator_t *dst, size_t niov, const os_iovec_t *iov, uint32_t flags)
{
#ifdef DDSI_INCLUDE_SSL
  char msgbuf[4096]; /* stack buffer for merging smallish writes without requiring allocations */
  os_iovec_t iovec; /* iovec used for msgbuf */
#endif
  ssize_t ret;
  size_t len;
  ddsi_tcp_conn_t conn;
  int piecewise;
  bool connect = false;
  struct msghdr msg;
  os_sockaddr_storage dstaddr;
  assert(niov <= INT_MAX);
  ddsi_ipaddr_from_loc(&dstaddr, dst);
  memset(&msg, 0, sizeof(msg));
  set_msghdr_iov (&msg, (os_iovec_t *) iov, niov);
  msg.msg_name = &dstaddr;
  msg.msg_namelen = (socklen_t) os_sockaddr_get_size((os_sockaddr *) &dstaddr);
#if OS_MSGHDR_FLAGS
  msg.msg_flags = (int) flags;
#endif
  len = iovlen_sum (niov, iov);
  (void) base;

  conn = ddsi_tcp_cache_find (&msg);
  if (conn == NULL)
  {
    return -1;
  }

  os_mutexLock (&conn->m_mutex);

  /* If not connected attempt to conect */

  if ((conn->m_sock == OS_INVALID_SOCKET) && ! conn->m_base.m_server)
  {
    ddsi_tcp_conn_connect (conn, &msg);
    if (conn->m_sock == OS_INVALID_SOCKET)
    {
      os_mutexUnlock (&conn->m_mutex);
      return -1;
    }
    connect = true;
  }

  /* Check if filtering out message from existing connections */

  if (!connect && ((flags & DDSI_TRAN_ON_CONNECT) != 0))
  {
    DDS_LOG(DDS_LC_TCP, "%s write: sock %"PRIsock" message filtered\n", ddsi_name, conn->m_sock);
    os_mutexUnlock (&conn->m_mutex);
    return (ssize_t) len;
  }

#ifdef DDSI_INCLUDE_SSL
  if (config.ssl_enable)
  {
    /* SSL doesn't have sendmsg, ret = 0 so writing starts at first byte.
       Rumor is that it is much better to merge small writes, which do here
       rather in than in SSL-specific code for simplicity - perhaps ought
       to move this copying into xpack_send */
    if (msg.msg_iovlen > 1)
    {
      int i;
      char * ptr;
      iovec.iov_len = (os_iov_len_t) len;
      iovec.iov_base = (len <= sizeof (msgbuf)) ? msgbuf : os_malloc (len);
      ptr = iovec.iov_base;
      for (i = 0; i < (int) msg.msg_iovlen; i++)
      {
        memcpy (ptr, msg.msg_iov[i].iov_base, msg.msg_iov[i].iov_len);
        ptr += msg.msg_iov[i].iov_len;
      }
      msg.msg_iov = &iovec;
      msg.msg_iovlen = 1;
    }
    piecewise = 1;
    ret = 0;
  }
  else
#endif
  {
    int sendflags = 0;
    int err;
#ifdef MSG_NOSIGNAL
    sendflags |= MSG_NOSIGNAL;
#endif
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    do
    {
      ret = sendmsg (conn->m_sock, &msg, sendflags);
      err = (ret == -1) ? os_getErrno () : 0;
    }
    while ((ret == -1) && (err == os_sockEINTR));
    if (ret == -1)
    {
      if (err_is_AGAIN_or_WOULDBLOCK (err))
      {
        piecewise = 1;
        ret = 0;
      }
      else
      {
        piecewise = 0;
        switch (err)
        {
          case os_sockECONNRESET:
#ifdef os_sockEPIPE
          case os_sockEPIPE:
#endif
            DDS_LOG(DDS_LC_TCP, "%s write: sock %"PRIsock" ECONNRESET\n", ddsi_name, conn->m_sock);
            break;
          default:
            if (! conn->m_base.m_closed && (conn->m_sock != OS_INVALID_SOCKET))
              DDS_WARNING("%s write failed on socket %"PRIsock" with errno %d\n", ddsi_name, conn->m_sock, err);
            break;
        }
      }
    }
    else
    {
      if (ret == 0)
      {
        DDS_LOG(DDS_LC_TCP, "%s write: sock %"PRIsock" eof\n", ddsi_name, conn->m_sock);
      }
      piecewise = (ret > 0 && (size_t) ret < len);
    }
  }

  if (piecewise)
  {
    ssize_t (*wr) (ddsi_tcp_conn_t, const void *, size_t, int *) = ddsi_tcp_conn_write_plain;
    int i = 0;
#ifdef DDSI_INCLUDE_SSL
    if (ddsi_tcp_ssl_plugin.write)
    {
      wr = ddsi_tcp_conn_write_ssl;
    }
#endif

    assert (msg.msg_iov[i].iov_len > 0);
    while (ret >= (ssize_t) msg.msg_iov[i].iov_len)
    {
      ret -= (ssize_t) msg.msg_iov[i++].iov_len;
    }
    assert (i < (int) msg.msg_iovlen);
    ret = ddsi_tcp_block_write (wr, conn, (const char *) msg.msg_iov[i].iov_base + ret, msg.msg_iov[i].iov_len - (size_t) ret);
    while (ret > 0 && ++i < (int) msg.msg_iovlen)
    {
      ret = ddsi_tcp_block_write (wr, conn, msg.msg_iov[i].iov_base, msg.msg_iov[i].iov_len);
    }
  }

#ifdef DDSI_INCLUDE_SSL
  /* If allocated memory for merging original fragments into a single buffer, free it */
  if (msg.msg_iov == &iovec && iovec.iov_base != msgbuf)
  {
    os_free (iovec.iov_base);
  }
#endif

  os_mutexUnlock (&conn->m_mutex);

  if (ret == -1)
  {
    ddsi_tcp_cache_remove (conn);
  }

  return ((size_t) ret == len) ? ret : -1;
}

static os_socket ddsi_tcp_conn_handle (ddsi_tran_base_t base)
{
  return ((ddsi_tcp_conn_t) base)->m_sock;
}

static bool ddsi_tcp_supports (int32_t kind)
{
  return kind == ddsi_tcp_factory_g.m_kind;
}

static int ddsi_tcp_locator (ddsi_tran_base_t base, nn_locator_t *loc)
{
  loc->kind = ddsi_tcp_factory_g.m_kind;
  memcpy(loc->address, gv.extloc.address, sizeof(loc->address));
  loc->port = base->m_port;
  return 0;
}

static ddsi_tran_conn_t ddsi_tcp_create_conn (uint32_t port, ddsi_tran_qos_t qos)
{
  (void) qos;
  (void) port;
  return &ddsi_tcp_conn_client.m_base;
}

static int ddsi_tcp_listen (ddsi_tran_listener_t listener)
{
  ddsi_tcp_listener_t tl = (ddsi_tcp_listener_t) listener;
  int ret = listen (tl->m_sock, 4);

#ifdef DDSI_INCLUDE_SSL
  if ((ret == 0) && ddsi_tcp_ssl_plugin.listen)
  {
    tl->m_bio = (ddsi_tcp_ssl_plugin.listen) (tl->m_sock);
  }
#endif

  return ret;
}

static ddsi_tran_conn_t ddsi_tcp_accept (ddsi_tran_listener_t listener)
{
  ddsi_tcp_listener_t tl = (ddsi_tcp_listener_t) listener;
  ddsi_tcp_conn_t tcp = NULL;
  os_socket sock = OS_INVALID_SOCKET;
  os_sockaddr_storage addr;
  socklen_t addrlen = sizeof (addr);
  char buff[DDSI_LOCSTRLEN];
  int err = 0;
#ifdef DDSI_INCLUDE_SSL
  SSL * ssl = NULL;
#endif

  memset (&addr, 0, addrlen);
  do
  {
#ifdef DDSI_INCLUDE_SSL
    if (ddsi_tcp_ssl_plugin.accept)
    {
      ssl = (ddsi_tcp_ssl_plugin.accept) (tl->m_bio, &sock);
    }
    else
#endif
    {
      sock = accept (tl->m_sock, NULL, NULL);
    }
    if (! gv.rtps_keepgoing)
    {
      ddsi_tcp_sock_free (sock, NULL);
      return NULL;
    }
    err = (sock == OS_INVALID_SOCKET) ? os_getErrno () : 0;
  }
  while ((err == os_sockEINTR) || (err == os_sockEAGAIN) || (err == os_sockEWOULDBLOCK));

  if (sock == OS_INVALID_SOCKET)
  {
    getsockname (tl->m_sock, (struct sockaddr *) &addr, &addrlen);
    sockaddr_to_string_with_port(buff, sizeof(buff), (os_sockaddr *)&addr);
    DDS_LOG((err == 0) ? DDS_LC_ERROR : DDS_LC_FATAL, "%s accept failed on socket %"PRIsock" at %s errno %d\n", ddsi_name, tl->m_sock, buff, err);
  }
  else if (getpeername (sock, (struct sockaddr *) &addr, &addrlen) == -1)
  {
    DDS_WARNING("%s accepted new socket %"PRIsock" on socket %"PRIsock" but no peer address, errno %d\n", ddsi_name, sock, tl->m_sock, os_getErrno());
    os_sockFree (sock);
  }
  else
  {
    sockaddr_to_string_with_port(buff, sizeof(buff), (os_sockaddr *)&addr);
    DDS_LOG(DDS_LC_TCP, "%s accept new socket %"PRIsock" on socket %"PRIsock" from %s\n", ddsi_name, sock, tl->m_sock, buff);

    os_sockSetNonBlocking (sock, true);
    tcp = ddsi_tcp_new_conn (sock, true, (os_sockaddr *)&addr);
#ifdef DDSI_INCLUDE_SSL
    tcp->m_ssl = ssl;
#endif
    tcp->m_base.m_listener = listener;
    tcp->m_base.m_conn = listener->m_connections;
    listener->m_connections = &tcp->m_base;

    /* Add connection to cache for bi-dir */

    os_mutexLock (&ddsi_tcp_cache_lock_g);
    ddsi_tcp_cache_add (tcp, NULL);
    os_mutexUnlock (&ddsi_tcp_cache_lock_g);
  }
  return tcp ? &tcp->m_base : NULL;
}

static os_socket ddsi_tcp_listener_handle (ddsi_tran_base_t base)
{
  return ((ddsi_tcp_listener_t) base)->m_sock;
}

/*
  ddsi_tcp_conn_address: This function is called when an entity had been discovered
  with an empty locator list and the locator is being set to the address of the
  caller (supporting call back over NAT).
*/

static void ddsi_tcp_conn_peer_locator (ddsi_tran_conn_t conn, nn_locator_t * loc)
{
  char buff[DDSI_LOCSTRLEN];
  ddsi_tcp_conn_t tc = (ddsi_tcp_conn_t) conn;
  assert (tc->m_sock != OS_INVALID_SOCKET);
  ddsi_ipaddr_to_loc (loc, (os_sockaddr *)&tc->m_peer_addr, tc->m_peer_addr.ss_family == AF_INET ? NN_LOCATOR_KIND_TCPv4 : NN_LOCATOR_KIND_TCPv6);
  ddsi_locator_to_string(buff, sizeof(buff), loc);
  DDS_LOG(DDS_LC_TCP, "(%s EP:%s)", ddsi_name, buff);
}

static void ddsi_tcp_base_init (struct ddsi_tran_conn * base)
{
  ddsi_factory_conn_init (&ddsi_tcp_factory_g, base);
  base->m_base.m_trantype = DDSI_TRAN_CONN;
  base->m_base.m_handle_fn = ddsi_tcp_conn_handle;
  base->m_base.m_locator_fn = ddsi_tcp_locator;
  base->m_read_fn = ddsi_tcp_conn_read;
  base->m_write_fn = ddsi_tcp_conn_write;
  base->m_peer_locator_fn = ddsi_tcp_conn_peer_locator;
  base->m_disable_multiplexing_fn = 0;
}

static ddsi_tcp_conn_t ddsi_tcp_new_conn (os_socket sock, bool server, os_sockaddr * peer)
{
  ddsi_tcp_conn_t conn = (ddsi_tcp_conn_t) os_malloc (sizeof (*conn));

  memset (conn, 0, sizeof (*conn));
  ddsi_tcp_base_init (&conn->m_base);
  os_mutexInit (&conn->m_mutex);
  conn->m_sock = OS_INVALID_SOCKET;
  (void)memcpy(&conn->m_peer_addr, peer, os_sockaddr_get_size(peer));
  conn->m_peer_port = os_sockaddr_get_port (peer);
  conn->m_base.m_server = server;
  conn->m_base.m_base.m_port = INVALID_PORT;
  ddsi_tcp_conn_set_socket (conn, sock);

  return conn;
}

static ddsi_tran_listener_t ddsi_tcp_create_listener (int port, ddsi_tran_qos_t qos)
{
  char buff[DDSI_LOCSTRLEN];
  os_socket sock;
  os_sockaddr_storage addr;
  socklen_t addrlen = sizeof (addr);
  ddsi_tcp_listener_t tl = NULL;

  (void) qos;

  ddsi_tcp_sock_new (&sock, (unsigned short) port);

  if (sock != OS_INVALID_SOCKET)
  {
    tl = (ddsi_tcp_listener_t) os_malloc (sizeof (*tl));
    memset (tl, 0, sizeof (*tl));

    tl->m_sock = sock;

    tl->m_base.m_listen_fn = ddsi_tcp_listen;
    tl->m_base.m_accept_fn = ddsi_tcp_accept;
    tl->m_base.m_factory = &ddsi_tcp_factory_g;

    tl->m_base.m_base.m_port = get_socket_port (sock);
    tl->m_base.m_base.m_trantype = DDSI_TRAN_LISTENER;
    tl->m_base.m_base.m_handle_fn = ddsi_tcp_listener_handle;
    tl->m_base.m_base.m_locator_fn = ddsi_tcp_locator;

    if (getsockname (sock, (os_sockaddr *) &addr, &addrlen) == -1)
    {
      int err = os_getErrno ();
      DDS_ERROR("ddsi_tcp_create_listener: getsockname errno %d\n", err);
      ddsi_tcp_sock_free (sock, NULL);
      os_free (tl);
      return NULL;
    }

    sockaddr_to_string_with_port(buff, sizeof(buff), (os_sockaddr *)&addr);
    DDS_LOG(DDS_LC_TCP, "%s create listener socket %"PRIsock" on %s\n", ddsi_name, sock, buff);
  }

  return tl ? &tl->m_base : NULL;
}

static void ddsi_tcp_conn_delete (ddsi_tcp_conn_t conn)
{
  char buff[DDSI_LOCSTRLEN];
  sockaddr_to_string_with_port(buff, sizeof(buff), (os_sockaddr *)&conn->m_peer_addr);
  DDS_LOG(DDS_LC_TCP, "%s free %s connnection on socket %"PRIsock" to %s\n", ddsi_name, conn->m_base.m_server ? "server" : "client", conn->m_sock, buff);

#ifdef DDSI_INCLUDE_SSL
  if (ddsi_tcp_ssl_plugin.ssl_free)
  {
    (ddsi_tcp_ssl_plugin.ssl_free) (conn->m_ssl);
  }
  else
#endif
  {
    ddsi_tcp_sock_free (conn->m_sock, "connection");
  }
  os_mutexDestroy (&conn->m_mutex);
  os_free (conn);
}

static void ddsi_tcp_close_conn (ddsi_tran_conn_t tc)
{
  if (tc != &ddsi_tcp_conn_client.m_base)
  {
    char buff[DDSI_LOCSTRLEN];
    nn_locator_t loc;
    ddsi_tcp_conn_t conn = (ddsi_tcp_conn_t) tc;
    sockaddr_to_string_with_port(buff, sizeof(buff), (os_sockaddr *)&conn->m_peer_addr);
    DDS_LOG(DDS_LC_TCP, "%s close %s connnection on socket %"PRIsock" to %s\n", ddsi_name, conn->m_base.m_server ? "server" : "client", conn->m_sock, buff);
    (void) shutdown (conn->m_sock, 2);
    ddsi_ipaddr_to_loc(&loc, (os_sockaddr *)&conn->m_peer_addr, conn->m_peer_addr.ss_family == AF_INET ? NN_LOCATOR_KIND_TCPv4 : NN_LOCATOR_KIND_TCPv6);
    loc.port = conn->m_peer_port;
    purge_proxy_participants (&loc, conn->m_base.m_server);
  }
}

static void ddsi_tcp_release_conn (ddsi_tran_conn_t conn)
{
  if (conn != &ddsi_tcp_conn_client.m_base)
  {
    ddsi_tcp_conn_delete ((ddsi_tcp_conn_t) conn);
  }
}

static void ddsi_tcp_unblock_listener (ddsi_tran_listener_t listener)
{
  ddsi_tcp_listener_t tl = (ddsi_tcp_listener_t) listener;
  os_socket sock;
  int ret;

  /* Connect to own listener socket to wake listener from blocking 'accept()' */
  ddsi_tcp_sock_new (&sock, 0);
  if (sock != OS_INVALID_SOCKET)
  {
    os_sockaddr_storage addr;
    socklen_t addrlen = sizeof (addr);
    if (getsockname (tl->m_sock, (os_sockaddr *) &addr, &addrlen) == -1)
      DDS_WARNING("%s failed to get listener address error %d\n", ddsi_name, os_getErrno());
    else
    {
      switch (addr.ss_family) {
        case AF_INET:
          {
            os_sockaddr_in *socketname = (os_sockaddr_in*)&addr;
            if (socketname->sin_addr.s_addr == htonl (INADDR_ANY)) {
              socketname->sin_addr.s_addr = htonl (INADDR_LOOPBACK);
            }
          }
          break;
#if OS_SOCKET_HAS_IPV6
        case AF_INET6:
          {
            os_sockaddr_in6 *socketname = (os_sockaddr_in6*)&addr;
            if (memcmp(&socketname->sin6_addr, &os_in6addr_any, sizeof(socketname->sin6_addr)) == 0) {
                socketname->sin6_addr = os_in6addr_loopback;
            }
          }
          break;
#endif
      }
      do
      {
        ret = connect (sock, (struct sockaddr *) &addr, (unsigned) os_sockaddr_get_size((os_sockaddr *)&addr));
      }
      while ((ret == -1) && (os_getErrno() == os_sockEINTR));
      if (ret == -1)
      {
        char buff[DDSI_LOCSTRLEN];
        sockaddr_to_string_with_port(buff, sizeof(buff), (os_sockaddr *)&addr);
        DDS_WARNING("%s failed to connect to own listener (%s) error %d\n", ddsi_name, buff, os_getErrno());
      }
    }
    ddsi_tcp_sock_free (sock, NULL);
  }
}

static void ddsi_tcp_release_listener (ddsi_tran_listener_t listener)
{
  ddsi_tcp_listener_t tl = (ddsi_tcp_listener_t) listener;
#ifdef DDSI_INCLUDE_SSL
  if (ddsi_tcp_ssl_plugin.bio_vfree)
  {
    (ddsi_tcp_ssl_plugin.bio_vfree) (tl->m_bio);
  }
#endif
  ddsi_tcp_sock_free (tl->m_sock, "listener");
  os_free (tl);
}

static void ddsi_tcp_release_factory (void)
{
  if (os_atomic_dec32_nv (&ddsi_tcp_init_g) == 0) {
    ut_avlFree (&ddsi_tcp_treedef, &ddsi_tcp_cache_g, ddsi_tcp_node_free);
    os_mutexDestroy (&ddsi_tcp_cache_lock_g);
#ifdef DDSI_INCLUDE_SSL
    if (ddsi_tcp_ssl_plugin.fini)
    {
      (ddsi_tcp_ssl_plugin.fini) ();
    }
#endif
    DDS_LOG(DDS_LC_CONFIG, "tcp de-initialized\n");
  }
}

static enum ddsi_locator_from_string_result ddsi_tcp_address_from_string (ddsi_tran_factory_t tran, nn_locator_t *loc, const char *str)
{
  return ddsi_ipaddr_from_string(tran, loc, str, ddsi_tcp_factory_g.m_kind);
}

static int ddsi_tcp_is_mcaddr (const ddsi_tran_factory_t tran, const nn_locator_t *loc)
{
  (void) tran;
  (void) loc;
  return 0;
}

static int ddsi_tcp_is_ssm_mcaddr (const ddsi_tran_factory_t tran, const nn_locator_t *loc)
{
  (void) tran;
  (void) loc;
  return 0;
}

static enum ddsi_nearby_address_result ddsi_tcp_is_nearby_address (ddsi_tran_factory_t tran, const nn_locator_t *loc, size_t ninterf, const struct nn_interface interf[])
{
  return ddsi_ipaddr_is_nearby_address(tran, loc, ninterf, interf);
}

int ddsi_tcp_init (void)
{
  if (os_atomic_inc32_nv (&ddsi_tcp_init_g) == 1)
  {
    memset (&ddsi_tcp_factory_g, 0, sizeof (ddsi_tcp_factory_g));
    ddsi_tcp_factory_g.m_kind = NN_LOCATOR_KIND_TCPv4;
    ddsi_tcp_factory_g.m_typename = "tcp";
    ddsi_tcp_factory_g.m_stream = true;
    ddsi_tcp_factory_g.m_connless = false;
    ddsi_tcp_factory_g.m_supports_fn = ddsi_tcp_supports;
    ddsi_tcp_factory_g.m_create_listener_fn = ddsi_tcp_create_listener;
    ddsi_tcp_factory_g.m_create_conn_fn = ddsi_tcp_create_conn;
    ddsi_tcp_factory_g.m_release_conn_fn = ddsi_tcp_release_conn;
    ddsi_tcp_factory_g.m_close_conn_fn = ddsi_tcp_close_conn;
    ddsi_tcp_factory_g.m_unblock_listener_fn = ddsi_tcp_unblock_listener;
    ddsi_tcp_factory_g.m_release_listener_fn = ddsi_tcp_release_listener;
    ddsi_tcp_factory_g.m_free_fn = ddsi_tcp_release_factory;
    ddsi_tcp_factory_g.m_locator_from_string_fn = ddsi_tcp_address_from_string;
    ddsi_tcp_factory_g.m_locator_to_string_fn = ddsi_ipaddr_to_string;
    ddsi_tcp_factory_g.m_enumerate_interfaces_fn = ddsi_eth_enumerate_interfaces;
    ddsi_tcp_factory_g.m_is_mcaddr_fn = ddsi_tcp_is_mcaddr;
    ddsi_tcp_factory_g.m_is_ssm_mcaddr_fn = ddsi_tcp_is_ssm_mcaddr;
    ddsi_tcp_factory_g.m_is_nearby_address_fn = ddsi_tcp_is_nearby_address;
    ddsi_factory_add (&ddsi_tcp_factory_g);

#if OS_SOCKET_HAS_IPV6
    if (config.transport_selector == TRANS_TCP6)
    {
      ddsi_tcp_factory_g.m_kind = NN_LOCATOR_KIND_TCPv6;
      ddsi_tcp_factory_g.m_typename = "tcp6";
    }
#endif

    memset (&ddsi_tcp_conn_client, 0, sizeof (ddsi_tcp_conn_client));
    ddsi_tcp_base_init (&ddsi_tcp_conn_client.m_base);

#ifdef DDSI_INCLUDE_SSL
    if (config.ssl_enable)
    {
      ddsi_name = "tcp/ssl";
      ddsi_ssl_config_plugin (&ddsi_tcp_ssl_plugin);
      if (! ddsi_tcp_ssl_plugin.init ())
      {
        DDS_ERROR("Failed to initialize OpenSSL\n");
        return -1;
      }
    }
#endif

    ut_avlInit (&ddsi_tcp_treedef, &ddsi_tcp_cache_g);
    os_mutexInit (&ddsi_tcp_cache_lock_g);

    DDS_LOG(DDS_LC_CONFIG, "%s initialized\n", ddsi_name);
  }
  return 0;
}
