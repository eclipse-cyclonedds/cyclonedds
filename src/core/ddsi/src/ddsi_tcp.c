// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stddef.h>
#include <assert.h>
#include <string.h>

#include "dds/features.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_log.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__eth.h"
#include "ddsi__tran.h"
#include "ddsi__tcp.h"
#include "ddsi__ipaddr.h"
#include "ddsi__entity.h"
#include "ddsi__ssl.h"
#include "ddsi__proxy_participant.h"
#include "ddsi__sockwaitset.h"

#define INVALID_PORT (~0u)

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

union addr {
  struct sockaddr a;
  struct sockaddr_in a4;
#if DDSRT_HAVE_IPV6
  struct sockaddr_in6 a6;
#endif
};

typedef struct ddsi_tcp_conn {
  struct ddsi_tran_conn m_base;
  union addr m_peer_addr;
  uint32_t m_peer_port;
  ddsrt_mutex_t m_mutex;
  ddsrt_socket_t m_sock;
#ifdef DDS_HAS_SSL
  SSL * m_ssl;
#endif
} *ddsi_tcp_conn_t;

typedef struct ddsi_tcp_listener {
  struct ddsi_tran_listener m_base;
  ddsrt_socket_t m_sock;
#ifdef DDS_HAS_SSL
  BIO * m_bio;
#endif
} *ddsi_tcp_listener_t;

struct ddsi_tran_factory_tcp {
  struct ddsi_tran_factory fact;
  int32_t m_kind;
  ddsrt_mutex_t ddsi_tcp_cache_lock_g;
  ddsrt_avl_tree_t ddsi_tcp_cache_g;
  struct ddsi_tcp_conn ddsi_tcp_conn_client;
#ifdef DDS_HAS_SSL
  struct ddsi_ssl_plugins ddsi_tcp_ssl_plugin;
#endif
};

static int ddsi_tcp_cmp_conn (const struct ddsi_tcp_conn *c1, const struct ddsi_tcp_conn *c2)
{
  const struct sockaddr *a1s = &c1->m_peer_addr.a;
  const struct sockaddr *a2s = &c2->m_peer_addr.a;
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

typedef struct ddsi_tcp_node {
  ddsrt_avl_node_t m_avlnode;
  ddsi_tcp_conn_t m_conn;
} * ddsi_tcp_node_t;

static const ddsrt_avl_treedef_t ddsi_tcp_treedef = DDSRT_AVL_TREEDEF_INITIALIZER_INDKEY
(
  offsetof (struct ddsi_tcp_node, m_avlnode),
  offsetof (struct ddsi_tcp_node, m_conn),
  ddsi_tcp_cmp_conn_wrap,
  0
);

static ddsi_tcp_conn_t ddsi_tcp_new_conn (struct ddsi_tran_factory_tcp *fact, const struct ddsi_network_interface *interf, ddsrt_socket_t, bool, struct sockaddr *);

static char *sockaddr_to_string_with_port (char *dst, size_t sizeof_dst, const struct sockaddr *src)
{
  ddsi_locator_t loc;
  ddsi_ipaddr_to_loc(&loc, src, src->sa_family == AF_INET ? DDSI_LOCATOR_KIND_TCPv4 : DDSI_LOCATOR_KIND_TCPv6);
  ddsi_locator_to_string(dst, sizeof_dst, &loc);
  return dst;
}

/* Connection cache dump routine for debugging

static void ddsi_tcp_cache_dump (void)
{
  char buff[64];
  ddsrt_avl_iter_t iter;
  ddsi_tcp_node_t n;
  unsigned i = 0;

  n = ddsrt_avl_iter_first (&ddsi_tcp_treedef, &ddsi_tcp_cache_g, &iter);
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
    n = ddsrt_avl_iter_next (&iter);
  }
}
*/

static uint16_t get_socket_port (struct ddsi_domaingv const * const gv, ddsrt_socket_t socket)
{
  union addr addr;
  socklen_t addrlen = sizeof (addr);
  dds_return_t ret;

  ret = ddsrt_getsockname(socket, &addr.a, &addrlen);
  if (ret != DDS_RETCODE_OK) {
    GVERROR ("ddsi_tcp_get_socket_port: ddsrt_getsockname retcode %"PRId32"\n", ret);
    return 0;
  }
  return ddsrt_sockaddr_get_port (&addr.a);
}

static void ddsi_tcp_conn_set_socket (ddsi_tcp_conn_t conn, ddsrt_socket_t sock)
{
  struct ddsi_domaingv const * const gv = conn->m_base.m_base.gv;
  conn->m_sock = sock;
  conn->m_base.m_base.m_port = (sock == DDSRT_INVALID_SOCKET) ? INVALID_PORT : get_socket_port (gv, sock);
}

static void ddsi_tcp_sock_free (struct ddsi_domaingv const * const gv, ddsrt_socket_t sock, const char *msg)
{
  if (sock != DDSRT_INVALID_SOCKET)
  {
    if (msg)
      GVLOG (DDS_LC_TCP, "tcp %s free socket %"PRIdSOCK"\n", msg, sock);
    ddsrt_close (sock);
  }
}

static dds_return_t ddsi_tcp_sock_new (struct ddsi_tran_factory_tcp * const fact, ddsrt_socket_t *sock, uint16_t port)
{
  struct ddsi_domaingv const * const gv = fact->fact.gv;
  union addr socketname;
  dds_return_t rc;
#if defined SO_NOSIGPIPE || defined TCP_NODELAY
  const int one = 1;
#endif

  memset (&socketname, 0, sizeof (socketname));
  switch (fact->m_kind)
  {
    case DDSI_LOCATOR_KIND_TCPv4:
      socketname.a4.sin_family = AF_INET;
      socketname.a4.sin_addr.s_addr = htonl (INADDR_ANY);
      socketname.a4.sin_port = htons (port);
      break;
#if DDSRT_HAVE_IPV6
    case DDSI_LOCATOR_KIND_TCPv6:
      socketname.a6.sin6_family = AF_INET6;
      socketname.a6.sin6_addr = ddsrt_in6addr_any;
      socketname.a6.sin6_port = htons (port);
      break;
#endif
    default:
      DDS_FATAL ("ddsi_tcp_sock_new: unsupported kind %"PRId32"\n", fact->m_kind);
  }
  if ((rc = ddsrt_socket (sock, socketname.a.sa_family, SOCK_STREAM, 0)) != DDS_RETCODE_OK)
  {
    GVERROR ("ddsi_tcp_sock_new: failed to create socket: %s\n", dds_strretcode (rc));
    goto fail;
  }

  /* If we're binding to a port number, allow others to bind to the same port */
  if (port && (rc = ddsrt_setsockreuse (*sock, true)) != DDS_RETCODE_OK) {
    if (rc != DDS_RETCODE_UNSUPPORTED) {
      GVERROR ("ddsi_tcp_sock_new: failed to enable port reuse: %s\n", dds_strretcode(rc));
      goto fail_w_socket;
    } else {
      // If the network stack doesn't support it, do make it fairly easy to find out,
      // but don't always print it to stderr because it would likely be more annoying
      // than helpful.
      GVLOG (DDS_LC_CONFIG, "ddsi_tcp_sock_new: port reuse not supported by network stack\n");
    }
  }

  if ((rc = ddsrt_bind (*sock, &socketname.a, ddsrt_sockaddr_get_size (&socketname.a))) != DDS_RETCODE_OK)
  {
    GVERROR ("ddsi_tcp_sock_new: failed to bind to ANY:%"PRIu16": %s\n", port,
             (rc == DDS_RETCODE_PRECONDITION_NOT_MET) ? "address in use" : dds_strretcode (rc));
    goto fail_w_socket;
  }

#ifdef SO_NOSIGPIPE
  if (ddsrt_setsockopt (*sock, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof (one)) != DDS_RETCODE_OK)
  {
    GVERROR ("ddsi_tcp_sock_new: failed to set NOSIGPIPE: %s\n", dds_strretcode (rc));
    goto fail_w_socket;
  }
#endif
#ifdef TCP_NODELAY
  if (gv->config.tcp_nodelay && (rc = ddsrt_setsockopt (*sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof (one))) != DDS_RETCODE_OK)
  {
    GVERROR ("ddsi_tcp_sock_new: failed to set NODELAY: %s\n", dds_strretcode (rc));
    goto fail_w_socket;
  }
#endif
  return DDS_RETCODE_OK;

fail_w_socket:
  ddsrt_close (*sock);
fail:
  *sock = DDSRT_INVALID_SOCKET;
  return rc;
}

static void ddsi_tcp_node_free (void * ptr)
{
  ddsi_tcp_node_t node = (ddsi_tcp_node_t) ptr;
  ddsi_conn_free ((struct ddsi_tran_conn *) node->m_conn);
  ddsrt_free (node);
}

static void ddsi_tcp_conn_connect (ddsi_tcp_conn_t conn, const ddsrt_msghdr_t * msg)
{
  struct ddsi_tran_factory_tcp * const fact = (struct ddsi_tran_factory_tcp *) conn->m_base.m_factory;
  struct ddsi_domaingv const * const gv = fact->fact.gv;
  char buff[DDSI_LOCSTRLEN];
  ddsrt_socket_t sock;
  dds_return_t ret;

  if (ddsi_tcp_sock_new (fact, &sock, 0) != DDS_RETCODE_OK)
  {
    /* error messages are logged by ddsi_tcp_sock_new */
    return;
  }

  /* Attempt to connect, expected that may fail */
  do {
    ret = ddsrt_connect(sock, msg->msg_name, msg->msg_namelen);
  } while (ret == DDS_RETCODE_INTERRUPTED);
  if (ret != DDS_RETCODE_OK)
    goto fail_w_socket;

  ddsi_tcp_conn_set_socket (conn, sock);
#ifdef DDS_HAS_SSL
  if (fact->ddsi_tcp_ssl_plugin.connect)
  {
    conn->m_ssl = (fact->ddsi_tcp_ssl_plugin.connect) (conn->m_base.m_base.gv, sock);
    if (conn->m_ssl == NULL)
    {
      ddsi_tcp_conn_set_socket (conn, DDSRT_INVALID_SOCKET);
      goto fail_w_socket;
    }
  }
#endif

  sockaddr_to_string_with_port(buff, sizeof(buff), (struct sockaddr *) msg->msg_name);
  GVLOG (DDS_LC_TCP, "tcp connect socket %"PRIdSOCK" port %u to %s\n", sock, get_socket_port (gv, sock), buff);

  /* Also may need to receive on connection so add to waitset */

  (void)ddsrt_setsocknonblocking(conn->m_sock, true);

  assert (conn->m_base.m_base.gv->n_recv_threads > 0);
  assert (conn->m_base.m_base.gv->recv_threads[0].arg.mode == DDSI_RTM_MANY);
  ddsi_sock_waitset_add (conn->m_base.m_base.gv->recv_threads[0].arg.u.many.ws, &conn->m_base);
  ddsi_sock_waitset_trigger (conn->m_base.m_base.gv->recv_threads[0].arg.u.many.ws);
  return;

fail_w_socket:
  ddsi_tcp_sock_free (gv, sock, NULL);
}

static void ddsi_tcp_cache_add (struct ddsi_tran_factory_tcp *fact, ddsi_tcp_conn_t conn, ddsrt_avl_ipath_t * path)
{
  struct ddsi_domaingv * const gv = fact->fact.gv;
  const char * action = "added";
  ddsi_tcp_node_t node;
  char buff[DDSI_LOCSTRLEN];

  ddsrt_atomic_inc32 (&conn->m_base.m_count);

  /* If path set, then cache does not contain connection */
  if (path)
  {
    node = ddsrt_malloc (sizeof (*node));
    node->m_conn = conn;
    ddsrt_avl_insert_ipath (&ddsi_tcp_treedef, &fact->ddsi_tcp_cache_g, node, path);
  }
  else
  {
    node = ddsrt_avl_lookup (&ddsi_tcp_treedef, &fact->ddsi_tcp_cache_g, conn);
    if (node)
    {
      /* Replace connection in cache */

      ddsi_conn_free ((struct ddsi_tran_conn *) node->m_conn);
      node->m_conn = conn;
      action = "updated";
    }
    else
    {
      node = ddsrt_malloc (sizeof (*node));
      node->m_conn = conn;
      ddsrt_avl_insert (&ddsi_tcp_treedef, &fact->ddsi_tcp_cache_g, node);
    }
  }

  sockaddr_to_string_with_port(buff, sizeof(buff), &conn->m_peer_addr.a);
  GVLOG (DDS_LC_TCP, "tcp cache %s %s socket %"PRIdSOCK" to %s\n", action, conn->m_base.m_server ? "server" : "client", conn->m_sock, buff);
}

static void ddsi_tcp_cache_remove (ddsi_tcp_conn_t conn)
{
  struct ddsi_tran_factory_tcp * const fact = (struct ddsi_tran_factory_tcp *) conn->m_base.m_factory;
  struct ddsi_domaingv * const gv = fact->fact.gv;
  char buff[DDSI_LOCSTRLEN];
  ddsi_tcp_node_t node;
  ddsrt_avl_dpath_t path;

  ddsrt_mutex_lock (&fact->ddsi_tcp_cache_lock_g);
  node = ddsrt_avl_lookup_dpath (&ddsi_tcp_treedef, &fact->ddsi_tcp_cache_g, conn, &path);
  if (node)
  {
    sockaddr_to_string_with_port(buff, sizeof(buff), &conn->m_peer_addr.a);
    GVLOG (DDS_LC_TCP, "tcp cache removed socket %"PRIdSOCK" to %s\n", conn->m_sock, buff);
    ddsrt_avl_delete_dpath (&ddsi_tcp_treedef, &fact->ddsi_tcp_cache_g, node, &path);
    ddsi_tcp_node_free (node);
  }
  ddsrt_mutex_unlock (&fact->ddsi_tcp_cache_lock_g);
}

/*
  ddsi_tcp_cache_find: Find existing connection to target, or if possible
  create new connection.
*/

static ddsi_tcp_conn_t ddsi_tcp_cache_find (struct ddsi_tran_factory_tcp *fact, const ddsrt_msghdr_t * msg)
{
  ddsrt_avl_ipath_t path;
  ddsi_tcp_node_t node;
  struct ddsi_tcp_conn key;
  ddsi_tcp_conn_t ret = NULL;

  memset (&key, 0, sizeof (key));
  key.m_peer_port = ddsrt_sockaddr_get_port (msg->msg_name);
  memcpy (&key.m_peer_addr, msg->msg_name, (size_t)msg->msg_namelen);

  /* Check cache for existing connection to target */

  ddsrt_mutex_lock (&fact->ddsi_tcp_cache_lock_g);
  node = ddsrt_avl_lookup_ipath (&ddsi_tcp_treedef, &fact->ddsi_tcp_cache_g, &key, &path);
  if (node)
  {
    if (node->m_conn->m_base.m_closed)
    {
      ddsrt_avl_delete (&ddsi_tcp_treedef, &fact->ddsi_tcp_cache_g, node);
      ddsi_tcp_node_free (node);
    }
    else
    {
      ret = node->m_conn;
    }
  }
  if (ret == NULL)
  {
    ret = ddsi_tcp_new_conn (fact, NULL, DDSRT_INVALID_SOCKET, false, &key.m_peer_addr.a);
    ddsi_tcp_cache_add (fact, ret, &path);
  }
  ddsrt_mutex_unlock (&fact->ddsi_tcp_cache_lock_g);

  return ret;
}

static ssize_t ddsi_tcp_conn_read_plain (ddsi_tcp_conn_t tcp, void * buf, size_t len, dds_return_t *rc)
{
  ssize_t rcvd = -1;

  assert(rc != NULL);
  *rc = ddsrt_recv(tcp->m_sock, buf, len, 0, &rcvd);

  return (*rc == DDS_RETCODE_OK ? rcvd : -1);
}

#ifdef DDS_HAS_SSL
static ssize_t ddsi_tcp_conn_read_ssl (ddsi_tcp_conn_t tcp, void * buf, size_t len, dds_return_t *rc)
{
  struct ddsi_tran_factory_tcp * const fact = (struct ddsi_tran_factory_tcp *) tcp->m_base.m_factory;
  return (fact->ddsi_tcp_ssl_plugin.read) (tcp->m_ssl, buf, len, rc);
}
#endif

static bool ddsi_tcp_select (struct ddsi_domaingv const * const gv, ddsrt_socket_t sock, bool read, size_t pos, int64_t timeout)
{
  dds_return_t rc;
  fd_set fds;
  fd_set *rdset = read ? &fds : NULL;
  fd_set *wrset = read ? NULL : &fds;
  int64_t tval = timeout;

  FD_ZERO (&fds);
#if LWIP_SOCKET == 1
  DDSRT_WARNING_GNUC_OFF(sign-conversion)
#endif
  FD_SET (sock, &fds);
#if LWIP_SOCKET == 1
  DDSRT_WARNING_GNUC_ON(sign-conversion)
#endif

  GVLOG (DDS_LC_TCP, "tcp blocked %s: sock %d\n", read ? "read" : "write", (int) sock);
  do {
    rc = ddsrt_select (sock + 1, rdset, wrset, NULL, tval);
  } while (rc == DDS_RETCODE_INTERRUPTED);

  if (rc < 0)
  {
    GVWARNING ("tcp abandoning %s on blocking socket %d after %"PRIuSIZE" bytes\n", read ? "read" : "write", (int) sock, pos);
  }

  return (rc > 0);
}

static int32_t addrfam_to_locator_kind (int af)
{
  assert (af == AF_INET || af == AF_INET6);
  return (af == AF_INET) ? DDSI_LOCATOR_KIND_TCPv4 : DDSI_LOCATOR_KIND_TCPv6;
}

static ssize_t ddsi_tcp_conn_read (struct ddsi_tran_conn * conn, unsigned char *buf, size_t len, bool allow_spurious, ddsi_locator_t *srcloc)
{
  struct ddsi_tran_factory_tcp * const fact = (struct ddsi_tran_factory_tcp *) conn->m_factory;
  struct ddsi_domaingv const * const gv = fact->fact.gv;
  dds_return_t rc;
  ddsi_tcp_conn_t tcp = (ddsi_tcp_conn_t) conn;
  ssize_t (*rd) (ddsi_tcp_conn_t, void *, size_t, dds_return_t * err) = ddsi_tcp_conn_read_plain;
  size_t pos = 0;
  ssize_t n;

#ifdef DDS_HAS_SSL
  if (fact->ddsi_tcp_ssl_plugin.read)
  {
    rd = ddsi_tcp_conn_read_ssl;
  }
#endif

  while (true)
  {
    n = rd (tcp, (char *) buf + pos, len - pos, &rc);
    if (n > 0)
    {
      pos += (size_t) n;
      if (pos == len)
      {
        if (srcloc)
        {
          const int32_t kind = addrfam_to_locator_kind (tcp->m_peer_addr.a.sa_family);
          ddsi_ipaddr_to_loc(srcloc, &tcp->m_peer_addr.a, kind);
        }
        return (ssize_t) pos;
      }
    }
    else if (n == 0)
    {
      GVLOG (DDS_LC_TCP, "tcp read: sock %"PRIdSOCK" closed-by-peer\n", tcp->m_sock);
      break;
    }
    else
    {
      if (rc != DDS_RETCODE_INTERRUPTED)
      {
        if (rc == DDS_RETCODE_TRY_AGAIN)
        {
          if (allow_spurious && pos == 0)
            return 0;
          const int64_t timeout = gv->config.tcp_read_timeout;
          if (ddsi_tcp_select (gv, tcp->m_sock, true, pos, timeout) == false)
            break;
        }
        else
        {
          GVLOG (DDS_LC_TCP, "tcp read: sock %"PRIdSOCK" error %"PRId32"\n", tcp->m_sock, rc);
          break;
        }
      }
    }
  }

  ddsi_tcp_cache_remove (tcp);
  return -1;
}

static ssize_t ddsi_tcp_conn_write_plain (ddsi_tcp_conn_t conn, const void * buf, size_t len, dds_return_t *rc)
{
  ssize_t sent = -1;
  int sendflags = 0;

#ifdef MSG_NOSIGNAL
  sendflags |= MSG_NOSIGNAL;
#endif
  *rc = ddsrt_send(conn->m_sock, buf, len, sendflags, &sent);

  return (*rc == DDS_RETCODE_OK ? sent : -1);
}

#ifdef DDS_HAS_SSL
static ssize_t ddsi_tcp_conn_write_ssl (ddsi_tcp_conn_t conn, const void * buf, size_t len, dds_return_t *rc)
{
  struct ddsi_tran_factory_tcp * const fact = (struct ddsi_tran_factory_tcp *) conn->m_base.m_factory;
  return (fact->ddsi_tcp_ssl_plugin.write) (conn->m_ssl, buf, len, rc);
}
#endif

static ssize_t ddsi_tcp_block_write (ssize_t (*wr) (ddsi_tcp_conn_t, const void *, size_t, dds_return_t *), ddsi_tcp_conn_t conn, const void * buf, size_t sz)
{
  /* Write all bytes of buf even in the presence of signals,
     partial writes and blocking (typically write buffer full) */
  struct ddsi_domaingv const * const gv = conn->m_base.m_base.gv;
  dds_return_t rc;
  size_t pos = 0;
  ssize_t n = -1;

  while (pos != sz)
  {
    n = (wr) (conn, (const char *) buf + pos, sz - pos, &rc);
    if (n > 0)
    {
      pos += (size_t) n;
    }
    else if (n == -1)
    {
      if (rc != DDS_RETCODE_INTERRUPTED)
      {
        if (rc == DDS_RETCODE_TRY_AGAIN)
        {
          const int64_t timeout = gv->config.tcp_write_timeout;
          if (ddsi_tcp_select (gv, conn->m_sock, false, pos, timeout) == false)
          {
            break;
          }
        }
        else
        {
          GVLOG (DDS_LC_TCP, "tcp write: sock %"PRIdSOCK" error %"PRId32"\n", conn->m_sock, rc);
          break;
        }
      }
    }
  }

  return (pos == sz) ? (ssize_t) pos : -1;
}

static size_t iovlen_sum (size_t niov, const ddsrt_iovec_t *iov)
{
  size_t tot = 0;
  while (niov--)
    tot += iov++->iov_len;
  return tot;
}

static void set_msghdr_iov (ddsrt_msghdr_t *mhdr, ddsrt_iovec_t *iov, size_t iovlen)
{
  mhdr->msg_iov = iov;
  mhdr->msg_iovlen = (ddsrt_msg_iovlen_t)iovlen;
}

static ssize_t ddsi_tcp_conn_write (struct ddsi_tran_conn * base, const ddsi_locator_t *dst, size_t niov, const ddsrt_iovec_t *iov, uint32_t flags)
{
  struct ddsi_tran_factory_tcp * const fact = (struct ddsi_tran_factory_tcp *) base->m_factory;
  struct ddsi_domaingv const * const gv = fact->fact.gv;
#ifdef DDS_HAS_SSL
  char msgbuf[4096]; /* stack buffer for merging smallish writes without requiring allocations */
  ddsrt_iovec_t iovec; /* iovec used for msgbuf */
#endif
  ssize_t ret = -1;
  size_t len;
  ddsi_tcp_conn_t conn;
  int piecewise;
  bool connect = false;
  ddsrt_msghdr_t msg;
  union {
    struct sockaddr_storage x;
    union addr a;
  } dstaddr;
  assert(niov <= INT_MAX);
  ddsi_ipaddr_from_loc(&dstaddr.x, dst);
  memset(&msg, 0, sizeof(msg));
  set_msghdr_iov (&msg, (ddsrt_iovec_t *) iov, niov);
  msg.msg_name = &dstaddr;
  msg.msg_namelen = ddsrt_sockaddr_get_size(&dstaddr.a.a);
#if DDSRT_MSGHDR_FLAGS
  msg.msg_flags = (int) flags;
#endif
  len = iovlen_sum (niov, iov);
  (void) base;

  conn = ddsi_tcp_cache_find (fact, &msg);
  if (conn == NULL)
  {
    return -1;
  }

  ddsrt_mutex_lock (&conn->m_mutex);

  /* If not connected attempt to conect */

  if (conn->m_sock == DDSRT_INVALID_SOCKET)
  {
    assert (!conn->m_base.m_server);
    ddsi_tcp_conn_connect (conn, &msg);
    if (conn->m_sock == DDSRT_INVALID_SOCKET)
    {
      ddsrt_mutex_unlock (&conn->m_mutex);
      return -1;
    }
    connect = true;
  }

  /* Check if filtering out message from existing connections */

  if (!connect && ((flags & DDSI_TRAN_ON_CONNECT) != 0))
  {
    GVLOG (DDS_LC_TCP, "tcp write: sock %"PRIdSOCK" message filtered\n", conn->m_sock);
    ddsrt_mutex_unlock (&conn->m_mutex);
    return (ssize_t) len;
  }

#ifdef DDS_HAS_SSL
  if (gv->config.ssl_enable)
  {
    /* SSL doesn't have sendmsg, ret = 0 so writing starts at first byte.
       Rumor is that it is much better to merge small writes, which do here
       rather in than in SSL-specific code for simplicity - perhaps ought
       to move this copying into xpack_send */
    if (msg.msg_iovlen > 1)
    {
      int i;
      char * ptr;
      iovec.iov_len = (ddsrt_iov_len_t) len;
      iovec.iov_base = (len <= sizeof (msgbuf)) ? msgbuf : ddsrt_malloc (len);
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
    dds_return_t rc;
#ifdef MSG_NOSIGNAL
    sendflags |= MSG_NOSIGNAL;
#endif
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    do
    {
      rc = ddsrt_sendmsg (conn->m_sock, &msg, sendflags, &ret);
    }
    while (rc == DDS_RETCODE_INTERRUPTED);
    if (ret == -1)
    {
      if (rc == DDS_RETCODE_TRY_AGAIN)
      {
        piecewise = 1;
        ret = 0;
      }
      else
      {
        piecewise = 0;
        switch (rc)
        {
          case DDS_RETCODE_NO_CONNECTION:
          case DDS_RETCODE_ILLEGAL_OPERATION:
            GVLOG (DDS_LC_TCP, "tcp write: sock %"PRIdSOCK" DDS_RETCODE_NO_CONNECTION\n", conn->m_sock);
            break;
          default:
            if (! conn->m_base.m_closed && (conn->m_sock != DDSRT_INVALID_SOCKET))
              GVWARNING ("tcp write failed on socket %"PRIdSOCK" with errno %"PRId32"\n", conn->m_sock, rc);
            break;
        }
      }
    }
    else
    {
      if (ret == 0)
      {
        GVLOG (DDS_LC_TCP, "tcp write: sock %"PRIdSOCK" eof\n", conn->m_sock);
      }
      piecewise = (ret > 0 && (size_t) ret < len);
    }
  }

  if (piecewise)
  {
    ssize_t (*wr) (ddsi_tcp_conn_t, const void *, size_t, dds_return_t *) = ddsi_tcp_conn_write_plain;
    int i = 0;
#ifdef DDS_HAS_SSL
    if (fact->ddsi_tcp_ssl_plugin.write)
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

#ifdef DDS_HAS_SSL
  /* If allocated memory for merging original fragments into a single buffer, free it */
  DDSRT_WARNING_MSVC_OFF(28199)
  if (msg.msg_iov == &iovec && iovec.iov_base != msgbuf)
  {
    ddsrt_free (iovec.iov_base);
  }
  DDSRT_WARNING_MSVC_ON(28199)
#endif

  ddsrt_mutex_unlock (&conn->m_mutex);

  if (ret == -1)
  {
    ddsi_tcp_cache_remove (conn);
  }

  return ((size_t) ret == len) ? ret : -1;
}

static ddsrt_socket_t ddsi_tcp_conn_handle (struct ddsi_tran_base * base)
{
  return ((ddsi_tcp_conn_t) base)->m_sock;
}

ddsrt_attribute_no_sanitize (("thread"))
static bool ddsi_tcp_supports (const struct ddsi_tran_factory *fact_cmn, int32_t kind)
{
  struct ddsi_tran_factory_tcp * const fact = (struct ddsi_tran_factory_tcp *) fact_cmn;
  return kind == fact->m_kind;
}

static int ddsi_tcp_locator (struct ddsi_tran_factory *fact_cmn, struct ddsi_tran_base * base, ddsi_locator_t *loc)
{
  struct ddsi_tran_factory_tcp * const fact = (struct ddsi_tran_factory_tcp *) fact_cmn;
  loc->kind = fact->m_kind;
  memcpy(loc->address, base->gv->interfaces[0].loc.address, sizeof(loc->address));
  loc->port = base->m_port;
  return 0;
}

static dds_return_t ddsi_tcp_create_conn (struct ddsi_tran_conn **conn_out, struct ddsi_tran_factory *fact_cmn, uint32_t port, const struct ddsi_tran_qos *qos)
{
  struct ddsi_tran_factory_tcp * const fact = (struct ddsi_tran_factory_tcp *) fact_cmn;
  (void) qos;
  (void) port;
  struct ddsi_domaingv const * const gv = fact->fact.gv;
  struct ddsi_network_interface const * const intf = qos->m_interface ? qos->m_interface : &gv->interfaces[0];

  fact->ddsi_tcp_conn_client.m_base.m_interf = intf;
  *conn_out = &fact->ddsi_tcp_conn_client.m_base;
  return DDS_RETCODE_OK;
}

static int ddsi_tcp_listen (struct ddsi_tran_listener * listener)
{
#ifdef DDS_HAS_SSL
  struct ddsi_tran_factory_tcp * const fact = (struct ddsi_tran_factory_tcp *) listener->m_factory;
#endif
  ddsi_tcp_listener_t tl = (ddsi_tcp_listener_t) listener;
  int ret = listen (tl->m_sock, 4);

#ifdef DDS_HAS_SSL
  if ((ret == 0) && fact->ddsi_tcp_ssl_plugin.listen)
  {
    tl->m_bio = (fact->ddsi_tcp_ssl_plugin.listen) (tl->m_sock);
  }
#endif

  return ret;
}

static struct ddsi_tran_conn * ddsi_tcp_accept (struct ddsi_tran_listener * listener)
{
  struct ddsi_tran_factory_tcp * const fact = (struct ddsi_tran_factory_tcp *) listener->m_factory;
  struct ddsi_domaingv const * const gv = fact->fact.gv;
  ddsi_tcp_listener_t tl = (ddsi_tcp_listener_t) listener;
  ddsi_tcp_conn_t tcp = NULL;
  ddsrt_socket_t sock = DDSRT_INVALID_SOCKET;
  union addr addr;
  socklen_t addrlen = sizeof (addr);
  char buff[DDSI_LOCSTRLEN];
  dds_return_t rc = DDS_RETCODE_OK;
#ifdef DDS_HAS_SSL
  SSL * ssl = NULL;
#endif

  memset (&addr, 0, sizeof(addr));
  do {
#ifdef DDS_HAS_SSL
    if (fact->ddsi_tcp_ssl_plugin.accept)
    {
      ssl = (fact->ddsi_tcp_ssl_plugin.accept) (listener->m_base.gv, tl->m_bio, &sock);
      if (ssl == NULL) {
        assert(sock == DDSRT_INVALID_SOCKET);
        rc = DDS_RETCODE_ERROR;
      }
    }
    else
#endif
    {
      rc = ddsrt_accept(tl->m_sock, NULL, NULL, &sock);
    }
    if (!ddsrt_atomic_ld32(&gv->rtps_keepgoing))
    {
      ddsi_tcp_sock_free (gv, sock, NULL);
      return NULL;
    }
  } while (rc == DDS_RETCODE_INTERRUPTED || rc == DDS_RETCODE_TRY_AGAIN);

  if (sock == DDSRT_INVALID_SOCKET)
  {
    (void)ddsrt_getsockname (tl->m_sock, &addr.a, &addrlen);
    sockaddr_to_string_with_port(buff, sizeof(buff), &addr.a);
    GVLOG ((rc == DDS_RETCODE_OK) ? DDS_LC_ERROR : DDS_LC_FATAL, "tcp accept failed on socket %"PRIdSOCK" at %s retcode %"PRId32"\n", tl->m_sock, buff, rc);
  }
  else if (getpeername (sock, &addr.a, &addrlen) == -1)
  {
    GVWARNING ("tcp accepted new socket %"PRIdSOCK" on socket %"PRIdSOCK" but no peer address, errno %"PRId32"\n", sock, tl->m_sock, rc);
    ddsrt_close (sock);
  }
  else
  {
    sockaddr_to_string_with_port(buff, sizeof(buff), &addr.a);
    GVLOG (DDS_LC_TCP, "tcp accept new socket %"PRIdSOCK" on socket %"PRIdSOCK" from %s\n", sock, tl->m_sock, buff);

    (void)ddsrt_setsocknonblocking (sock, true);
    tcp = ddsi_tcp_new_conn (fact, NULL, sock, true, &addr.a);
#ifdef DDS_HAS_SSL
    tcp->m_ssl = ssl;
#endif
    tcp->m_base.m_listener = listener;
    tcp->m_base.m_conn = listener->m_connections;
    listener->m_connections = &tcp->m_base;

    /* Add connection to cache for bi-dir */

    ddsrt_mutex_lock (&fact->ddsi_tcp_cache_lock_g);
    ddsi_tcp_cache_add (fact, tcp, NULL);
    ddsrt_mutex_unlock (&fact->ddsi_tcp_cache_lock_g);
  }
  return tcp ? &tcp->m_base : NULL;
}

static ddsrt_socket_t ddsi_tcp_listener_handle (struct ddsi_tran_base * base)
{
  return ((ddsi_tcp_listener_t) base)->m_sock;
}

/*
  ddsi_tcp_conn_address: This function is called when an entity had been discovered
  with an empty locator list and the locator is being set to the address of the
  caller (supporting call back over NAT).
*/

static void addr_to_loc (ddsi_locator_t *loc, const union addr *addr)
{
  ddsi_ipaddr_to_loc (loc, &addr->a, addrfam_to_locator_kind (addr->a.sa_family));
}

static void ddsi_tcp_conn_peer_locator (struct ddsi_tran_conn * conn, ddsi_locator_t * loc)
{
  struct ddsi_domaingv const * const gv = conn->m_base.gv;
  char buff[DDSI_LOCSTRLEN];
  ddsi_tcp_conn_t tc = (ddsi_tcp_conn_t) conn;
  assert (tc->m_sock != DDSRT_INVALID_SOCKET);
  addr_to_loc (loc, &tc->m_peer_addr);
  ddsi_locator_to_string(buff, sizeof(buff), loc);
  GVLOG (DDS_LC_TCP, "(tcp EP:%s)", buff);
}

static void ddsi_tcp_base_init (const struct ddsi_tran_factory_tcp *fact, const struct ddsi_network_interface *interf, struct ddsi_tran_conn *base)
{
  ddsi_factory_conn_init (&fact->fact, interf, base);
  base->m_base.m_trantype = DDSI_TRAN_CONN;
  base->m_base.m_handle_fn = ddsi_tcp_conn_handle;
  base->m_read_fn = ddsi_tcp_conn_read;
  base->m_write_fn = ddsi_tcp_conn_write;
  base->m_peer_locator_fn = ddsi_tcp_conn_peer_locator;
  base->m_disable_multiplexing_fn = 0;
  base->m_locator_fn = ddsi_tcp_locator;
}

static ddsi_tcp_conn_t ddsi_tcp_new_conn (struct ddsi_tran_factory_tcp *fact, const struct ddsi_network_interface *interf, ddsrt_socket_t sock, bool server, struct sockaddr * peer)
{
  ddsi_tcp_conn_t conn = ddsrt_malloc (sizeof (*conn));

  memset (conn, 0, sizeof (*conn));
  ddsi_tcp_base_init (fact, interf, &conn->m_base);
  ddsrt_mutex_init (&conn->m_mutex);
  conn->m_sock = DDSRT_INVALID_SOCKET;
  (void)memcpy(&conn->m_peer_addr, peer, (size_t)ddsrt_sockaddr_get_size(peer));
  conn->m_peer_port = ddsrt_sockaddr_get_port (peer);
  conn->m_base.m_server = server;
  conn->m_base.m_base.m_port = INVALID_PORT;
  ddsi_tcp_conn_set_socket (conn, sock);

  return conn;
}

static dds_return_t ddsi_tcp_create_listener (struct ddsi_tran_listener **listener_out, struct ddsi_tran_factory * fact, uint32_t port, const struct ddsi_tran_qos *qos)
{
  struct ddsi_tran_factory_tcp * const fact_tcp = (struct ddsi_tran_factory_tcp *) fact;
  struct ddsi_domaingv const * const gv = fact_tcp->fact.gv;
  ddsrt_socket_t sock;
  (void) qos;

  if (ddsi_tcp_sock_new (fact_tcp, &sock, (uint16_t) port) != DDS_RETCODE_OK)
    return DDS_RETCODE_ERROR;

  char buff[DDSI_LOCSTRLEN];
  union addr addr;
  socklen_t addrlen = sizeof (addr);
  dds_return_t ret;
  if ((ret = ddsrt_getsockname (sock, &addr.a, &addrlen)) != DDS_RETCODE_OK)
  {
    GVERROR ("ddsi_tcp_create_listener: ddsrt_getsockname returned %"PRId32"\n", ret);
    ddsi_tcp_sock_free (gv, sock, NULL);
    return DDS_RETCODE_ERROR;
  }
  sockaddr_to_string_with_port (buff, sizeof (buff), &addr.a);
  GVLOG (DDS_LC_TCP, "tcp create listener socket %"PRIdSOCK" on %s\n", sock, buff);

  ddsi_tcp_listener_t tl = ddsrt_malloc (sizeof (*tl));
  memset (tl, 0, sizeof (*tl));

  tl->m_sock = sock;

  tl->m_base.m_base.gv = fact->gv;
  tl->m_base.m_listen_fn = ddsi_tcp_listen;
  tl->m_base.m_accept_fn = ddsi_tcp_accept;
  tl->m_base.m_factory = fact;

  tl->m_base.m_base.m_port = get_socket_port (gv, sock);
  tl->m_base.m_base.m_trantype = DDSI_TRAN_LISTENER;
  tl->m_base.m_base.m_handle_fn = ddsi_tcp_listener_handle;
  tl->m_base.m_locator_fn = ddsi_tcp_locator;
  *listener_out = &tl->m_base;
  return DDS_RETCODE_OK;
}

static void ddsi_tcp_conn_delete (ddsi_tcp_conn_t conn)
{
  struct ddsi_tran_factory_tcp * const fact = (struct ddsi_tran_factory_tcp *) conn->m_base.m_factory;
  struct ddsi_domaingv const * const gv = fact->fact.gv;
  char buff[DDSI_LOCSTRLEN];
  sockaddr_to_string_with_port(buff, sizeof(buff), &conn->m_peer_addr.a);
  GVLOG (DDS_LC_TCP, "tcp free %s connection on socket %"PRIdSOCK" to %s\n", conn->m_base.m_server ? "server" : "client", conn->m_sock, buff);

#ifdef DDS_HAS_SSL
  if (fact->ddsi_tcp_ssl_plugin.ssl_free)
  {
    (fact->ddsi_tcp_ssl_plugin.ssl_free) (conn->m_ssl);
  }
  else
#endif
  {
    ddsi_tcp_sock_free (gv, conn->m_sock, "connection");
  }
  ddsrt_mutex_destroy (&conn->m_mutex);
  ddsrt_free (conn);
}

static void ddsi_tcp_close_conn (struct ddsi_tran_conn * tc)
{
  struct ddsi_tran_factory_tcp * const fact_tcp = (struct ddsi_tran_factory_tcp *) tc->m_factory;
  struct ddsi_domaingv * const gv = fact_tcp->fact.gv;
  if (tc != &fact_tcp->ddsi_tcp_conn_client.m_base)
  {
    char buff[DDSI_LOCSTRLEN];
    ddsi_xlocator_t loc;
    ddsi_tcp_conn_t conn = (ddsi_tcp_conn_t) tc;
    sockaddr_to_string_with_port(buff, sizeof(buff), &conn->m_peer_addr.a);
    GVLOG (DDS_LC_TCP, "tcp close %s connection on socket %"PRIdSOCK" to %s\n", conn->m_base.m_server ? "server" : "client", conn->m_sock, buff);
    (void) shutdown (conn->m_sock, 2);
    ddsi_ipaddr_to_loc(&loc.c, &conn->m_peer_addr.a, addrfam_to_locator_kind(conn->m_peer_addr.a.sa_family));
    loc.c.port = conn->m_peer_port;
    loc.conn = tc;
    ddsi_purge_proxy_participants (gv, &loc, conn->m_base.m_server);
  }
}

static void ddsi_tcp_release_conn (struct ddsi_tran_conn * conn)
{
  struct ddsi_tran_factory_tcp * const fact_tcp = (struct ddsi_tran_factory_tcp *) conn->m_factory;
  if (conn != &fact_tcp->ddsi_tcp_conn_client.m_base)
  {
    ddsi_tcp_conn_delete ((ddsi_tcp_conn_t) conn);
  }
}

static void ddsi_tcp_unblock_listener (struct ddsi_tran_listener * listener)
{
  struct ddsi_tran_factory_tcp * const fact_tcp = (struct ddsi_tran_factory_tcp *) listener->m_factory;
  struct ddsi_domaingv const * const gv = fact_tcp->fact.gv;
  ddsi_tcp_listener_t tl = (ddsi_tcp_listener_t) listener;
  ddsrt_socket_t sock;
  dds_return_t ret;

  /* Connect to own listener socket to wake listener from blocking 'accept()' */
  if (ddsi_tcp_sock_new (fact_tcp, &sock, 0) != DDS_RETCODE_OK)
    goto fail;

  union addr addr;
  socklen_t addrlen = sizeof (addr);
  if ((ret = ddsrt_getsockname (tl->m_sock, &addr.a, &addrlen)) != DDS_RETCODE_OK)
  {
    GVWARNING ("tcp failed to get listener address error %"PRId32"\n", ret);
    goto fail_w_socket;
  }
  switch (addr.a.sa_family)
  {
    case AF_INET:
      if (addr.a4.sin_addr.s_addr == htonl (INADDR_ANY))
        addr.a4.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
      break;
#if DDSRT_HAVE_IPV6
    case AF_INET6:
      if (memcmp (&addr.a6.sin6_addr, &ddsrt_in6addr_any, sizeof (addr.a6.sin6_addr)) == 0)
        addr.a6.sin6_addr = ddsrt_in6addr_loopback;
      break;
#endif
  }

  do {
    ret = ddsrt_connect (sock, &addr.a, ddsrt_sockaddr_get_size (&addr.a));
  } while (ret == DDS_RETCODE_INTERRUPTED);
  if (ret != DDS_RETCODE_OK)
  {
    char buff[DDSI_LOCSTRLEN];
    sockaddr_to_string_with_port (buff, sizeof (buff), &addr.a);
    GVWARNING ("tcp failed to connect to own listener (%s) error %"PRId32"\n", buff, ret);
  }

fail_w_socket:
  ddsi_tcp_sock_free (gv, sock, NULL);
fail:
  return;
}

static void ddsi_tcp_release_listener (struct ddsi_tran_listener * listener)
{
  ddsi_tcp_listener_t tl = (ddsi_tcp_listener_t) listener;
  struct ddsi_domaingv const * const gv = tl->m_base.m_base.gv;
#ifdef DDS_HAS_SSL
  struct ddsi_tran_factory_tcp * const fact = (struct ddsi_tran_factory_tcp *) listener->m_factory;
  if (fact->ddsi_tcp_ssl_plugin.bio_vfree)
  {
    (fact->ddsi_tcp_ssl_plugin.bio_vfree) (tl->m_bio);
  }
#endif
  ddsi_tcp_sock_free (gv, tl->m_sock, "listener");
  ddsrt_free (tl);
}

static void ddsi_tcp_release_factory (struct ddsi_tran_factory *fact_cmn)
{
  struct ddsi_tran_factory_tcp * const fact = (struct ddsi_tran_factory_tcp *) fact_cmn;
  struct ddsi_domaingv const * const gv = fact->fact.gv;
  ddsrt_avl_free (&ddsi_tcp_treedef, &fact->ddsi_tcp_cache_g, ddsi_tcp_node_free);
  ddsrt_mutex_destroy (&fact->ddsi_tcp_cache_lock_g);
#ifdef DDS_HAS_SSL
  if (fact->ddsi_tcp_ssl_plugin.fini)
  {
    (fact->ddsi_tcp_ssl_plugin.fini) ();
  }
#endif
  GVLOG (DDS_LC_CONFIG, "tcp de-initialized\n");
  ddsrt_free (fact);
}

static enum ddsi_locator_from_string_result ddsi_tcp_address_from_string (const struct ddsi_tran_factory *fact_cmn, ddsi_locator_t *loc, const char *str)
{
  struct ddsi_tran_factory_tcp * const fact = (struct ddsi_tran_factory_tcp *) fact_cmn;
  return ddsi_ipaddr_from_string(loc, str, fact->m_kind);
}

static int ddsi_tcp_is_loopbackaddr (const struct ddsi_tran_factory *tran, const ddsi_locator_t *loc)
{
  (void) tran;
  switch (loc->kind)
  {
    case DDSI_LOCATOR_KIND_UDPv4: {
      return loc->address[12] == 127;
    }
#if DDSRT_HAVE_IPV6
    case DDSI_LOCATOR_KIND_UDPv6: {
      const struct in6_addr *ipv6 = (const struct in6_addr *) loc->address;
      return IN6_IS_ADDR_LOOPBACK (ipv6);
    }
#endif
    default: {
      return 0;
    }
  }
}

static int ddsi_tcp_is_mcaddr (const struct ddsi_tran_factory *tran, const ddsi_locator_t *loc)
{
  (void) tran;
  (void) loc;
  return 0;
}

static int ddsi_tcp_is_ssm_mcaddr (const struct ddsi_tran_factory *tran, const ddsi_locator_t *loc)
{
  (void) tran;
  (void) loc;
  return 0;
}

static enum ddsi_nearby_address_result ddsi_tcp_is_nearby_address (const ddsi_locator_t *loc, size_t ninterf, const struct ddsi_network_interface interf[], size_t *interf_idx)
{
  return ddsi_ipaddr_is_nearby_address(loc, ninterf, interf, interf_idx);
}

static int ddsi_tcp_is_valid_port (const struct ddsi_tran_factory *fact, uint32_t port)
{
  (void) fact;
  return (port <= 65535);
}

static uint32_t ddsi_tcp_receive_buffer_size (const struct ddsi_tran_factory *fact)
{
  (void) fact;
  return 0;
}

static char *ddsi_tcp_locator_to_string (char *dst, size_t sizeof_dst, const ddsi_locator_t *loc, struct ddsi_tran_conn * conn, int with_port)
{
  (void) conn;
  return ddsi_ipaddr_to_string(dst, sizeof_dst, loc, with_port, NULL);
}

static int ddsi_tcp_locator_from_sockaddr (const struct ddsi_tran_factory *tran_cmn, ddsi_locator_t *loc, const struct sockaddr *sockaddr)
{
  struct ddsi_tran_factory_tcp * const tran = (struct ddsi_tran_factory_tcp *) tran_cmn;
  switch (sockaddr->sa_family)
  {
    case AF_INET:
      if (tran->m_kind != DDSI_LOCATOR_KIND_TCPv4)
        return -1;
      break;
    case AF_INET6:
      if (tran->m_kind != DDSI_LOCATOR_KIND_TCPv6)
        return -1;
      break;
  }
  ddsi_ipaddr_to_loc (loc, sockaddr, tran->m_kind);
  return 0;
}

int ddsi_tcp_init (struct ddsi_domaingv *gv)
{
  struct ddsi_tran_factory_tcp *fact = ddsrt_malloc (sizeof (*fact));

  memset (fact, 0, sizeof (*fact));
  fact->m_kind = DDSI_LOCATOR_KIND_TCPv4;
  fact->fact.gv = gv;
  fact->fact.m_typename = "tcp";
  fact->fact.m_default_spdp_address = NULL;
  fact->fact.m_stream = true;
  fact->fact.m_connless = false;
  fact->fact.m_enable_spdp = true;
  fact->fact.m_supports_fn = ddsi_tcp_supports;
  fact->fact.m_create_listener_fn = ddsi_tcp_create_listener;
  fact->fact.m_create_conn_fn = ddsi_tcp_create_conn;
  fact->fact.m_release_conn_fn = ddsi_tcp_release_conn;
  fact->fact.m_close_conn_fn = ddsi_tcp_close_conn;
  fact->fact.m_unblock_listener_fn = ddsi_tcp_unblock_listener;
  fact->fact.m_release_listener_fn = ddsi_tcp_release_listener;
  fact->fact.m_free_fn = ddsi_tcp_release_factory;
  fact->fact.m_locator_from_string_fn = ddsi_tcp_address_from_string;
  fact->fact.m_locator_to_string_fn = ddsi_tcp_locator_to_string;
  fact->fact.m_enumerate_interfaces_fn = ddsi_eth_enumerate_interfaces;
  fact->fact.m_is_loopbackaddr_fn = ddsi_tcp_is_loopbackaddr;
  fact->fact.m_is_mcaddr_fn = ddsi_tcp_is_mcaddr;
  fact->fact.m_is_ssm_mcaddr_fn = ddsi_tcp_is_ssm_mcaddr;
  fact->fact.m_is_nearby_address_fn = ddsi_tcp_is_nearby_address;
  fact->fact.m_is_valid_port_fn = ddsi_tcp_is_valid_port;
  fact->fact.m_receive_buffer_size_fn = ddsi_tcp_receive_buffer_size;
  fact->fact.m_locator_from_sockaddr_fn = ddsi_tcp_locator_from_sockaddr;

#if DDSRT_HAVE_IPV6
  if (gv->config.transport_selector == DDSI_TRANS_TCP6)
  {
    fact->m_kind = DDSI_LOCATOR_KIND_TCPv6;
    fact->fact.m_typename = "tcp6";
  }
#endif

  ddsi_factory_add (gv, &fact->fact);

  memset (&fact->ddsi_tcp_conn_client, 0, sizeof (fact->ddsi_tcp_conn_client));
  ddsi_tcp_base_init (fact, NULL, &fact->ddsi_tcp_conn_client.m_base);

#ifdef DDS_HAS_SSL
  if (gv->config.ssl_enable)
  {
    ddsi_ssl_config_plugin (&fact->ddsi_tcp_ssl_plugin);
    if (! fact->ddsi_tcp_ssl_plugin.init (gv))
    {
      GVERROR ("Failed to initialize OpenSSL\n");
      return -1;
    }
  }
#endif

  ddsrt_avl_init (&ddsi_tcp_treedef, &fact->ddsi_tcp_cache_g);
  ddsrt_mutex_init (&fact->ddsi_tcp_cache_lock_g);

  GVLOG (DDS_LC_CONFIG, "tcp initialized\n");
  return 0;
}
