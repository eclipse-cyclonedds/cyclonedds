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
#include <ctype.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsi/ddsi_tran.h"
#include "dds/ddsi/ddsi_ipaddr.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/ddsi_domaingv.h"

DDS_EXPORT extern inline uint32_t ddsi_conn_type (const struct ddsi_tran_conn *conn);
DDS_EXPORT extern inline uint32_t ddsi_conn_port (const struct ddsi_tran_conn *conn);
DDS_EXPORT extern inline dds_return_t ddsi_factory_create_listener (ddsi_tran_listener_t *listener, ddsi_tran_factory_t factory, uint32_t port, const struct ddsi_tran_qos *qos);
DDS_EXPORT extern inline bool ddsi_factory_supports (const struct ddsi_tran_factory *factory, int32_t kind);
DDS_EXPORT extern inline int ddsi_is_valid_port (const struct ddsi_tran_factory *factory, uint32_t port);
DDS_EXPORT extern inline uint32_t ddsi_receive_buffer_size (const struct ddsi_tran_factory *factory);
DDS_EXPORT extern inline ddsrt_socket_t ddsi_conn_handle (ddsi_tran_conn_t conn);
DDS_EXPORT extern inline int ddsi_conn_locator (ddsi_tran_conn_t conn, ddsi_locator_t * loc);
DDS_EXPORT extern inline ddsrt_socket_t ddsi_tran_handle (ddsi_tran_base_t base);
DDS_EXPORT extern inline dds_return_t ddsi_factory_create_conn (ddsi_tran_conn_t *conn, ddsi_tran_factory_t factory, uint32_t port, const struct ddsi_tran_qos *qos);
DDS_EXPORT extern inline int ddsi_listener_locator (ddsi_tran_listener_t listener, ddsi_locator_t * loc);
DDS_EXPORT extern inline int ddsi_listener_listen (ddsi_tran_listener_t listener);
DDS_EXPORT extern inline ddsi_tran_conn_t ddsi_listener_accept (ddsi_tran_listener_t listener);
DDS_EXPORT extern inline ssize_t ddsi_conn_read (ddsi_tran_conn_t conn, unsigned char * buf, size_t len, bool allow_spurious, ddsi_locator_t *srcloc);
DDS_EXPORT extern inline ssize_t ddsi_conn_write (ddsi_tran_conn_t conn, const ddsi_locator_t *dst, size_t niov, const ddsrt_iovec_t *iov, uint32_t flags);

void ddsi_factory_add (struct ddsi_domaingv *gv, ddsi_tran_factory_t factory)
{
  // Initially add factories in a disabled state.  We'll enable the ones we
  // actually want to use for DDS explicitly, as a way to work around using
  // the TCP support code for the "debmon" thing.
  factory->m_enable = false;

  factory->m_factory = gv->ddsi_tran_factories;
  gv->ddsi_tran_factories = factory;
}

void ddsi_tran_factories_fini (struct ddsi_domaingv *gv)
{
  ddsi_tran_factory_t factory;
  while ((factory = gv->ddsi_tran_factories) != NULL)
  {
    /* Keep the factory in the list for the duration of "factory_free" so that
       conversion of locator kind to factory remains possible. */
    ddsi_tran_factory_t next = factory->m_factory;
    ddsi_factory_free (factory);
    gv->ddsi_tran_factories = next;
  }
}

static bool type_is_numeric (const char *type, size_t len, int32_t *value)
{
  /* returns false if there are non-digits present or if the value is out of range */
  *value = 0;
  for (size_t i = 0; i < len; i++)
  {
    if (!isdigit ((unsigned char) type[i]))
      return false;
    int32_t d = (unsigned char) type[i] - '0';
    if (*value > INT32_MAX / 10 || 10 * *value > INT32_MAX - d)
      return false;
    *value = 10 * *value + d;
  }
  return true;
}

static ddsi_tran_factory_t ddsi_factory_find_with_len (const struct ddsi_domaingv *gv, const char *type, size_t len)
{
  int32_t loc_kind;
  if (type_is_numeric (type, len, &loc_kind))
    return ddsi_factory_find_supported_kind (gv, loc_kind);
  else
  {
    for (ddsi_tran_factory_t f = gv->ddsi_tran_factories; f; f = f->m_factory)
    {
      if (strncmp (f->m_typename, type, len) == 0 && f->m_typename[len] == 0)
        return f;
    }
  }
  return NULL;
}

ddsi_tran_factory_t ddsi_factory_find (const struct ddsi_domaingv *gv, const char *type)
{
  return ddsi_factory_find_with_len (gv, type, strlen (type));
}

ddsrt_attribute_no_sanitize (("thread"))
ddsi_tran_factory_t ddsi_factory_find_supported_kind (const struct ddsi_domaingv *gv, int32_t kind)
{
  /* FIXME: MUST speed up */
  ddsi_tran_factory_t factory;
  for (factory = gv->ddsi_tran_factories; factory; factory = factory->m_factory) {
    if (factory->m_supports_fn(factory, kind)) {
      return factory;
    }
  }
  return NULL;
}

void ddsi_factory_free (ddsi_tran_factory_t factory)
{
  if (factory && factory->m_free_fn)
  {
    (factory->m_free_fn) (factory);
  }
}

void ddsi_conn_free (ddsi_tran_conn_t conn)
{
  if (conn)
  {
    if (! conn->m_closed)
    {
      conn->m_closed = true;
      /* FIXME: rethink the socket waitset & the deleting of entries; the biggest issue is TCP handling that can open & close sockets at will and yet expects the waitset to wake up at the apprioriate times.  (This pretty much works with the select-based version, but not the kqueue-based one.)  TCP code can also have connections without a socket ...  Calling sockWaitsetRemove here (where there shouldn't be any knowledge of it) at least ensures that it is removed in time and that there can't be aliasing of connections and sockets.   */
      if (ddsi_conn_handle (conn) != DDSRT_INVALID_SOCKET)
      {
        for (uint32_t i = 0; i < conn->m_base.gv->n_recv_threads; i++)
        {
          if (!conn->m_base.gv->recv_threads[i].ts)
            assert (!ddsrt_atomic_ld32 (&conn->m_base.gv->rtps_keepgoing));
          else
          {
            switch (conn->m_base.gv->recv_threads[i].arg.mode)
            {
              case RTM_MANY:
                os_sockWaitsetRemove (conn->m_base.gv->recv_threads[i].arg.u.many.ws, conn);
                break;
              case RTM_SINGLE:
                if (conn->m_base.gv->recv_threads[i].arg.u.single.conn == conn)
                  abort();
                break;
            }
          }
        }
      }
      if (conn->m_factory->m_close_conn_fn)
      {
        (conn->m_factory->m_close_conn_fn) (conn);
      }
    }
    if (ddsrt_atomic_dec32_ov (&conn->m_count) == 1)
    {
      (conn->m_factory->m_release_conn_fn) (conn);
    }
  }
}

void ddsi_conn_add_ref (ddsi_tran_conn_t conn)
{
  ddsrt_atomic_inc32 (&conn->m_count);
}

void ddsi_factory_conn_init (const struct ddsi_tran_factory *factory, const struct nn_interface *interf, ddsi_tran_conn_t conn)
{
  ddsrt_atomic_st32 (&conn->m_count, 1);
  conn->m_connless = factory->m_connless;
  conn->m_stream = factory->m_stream;
  conn->m_factory = (struct ddsi_tran_factory *) factory;
  conn->m_interf = interf;
  conn->m_base.gv = factory->gv;
}

void ddsi_conn_disable_multiplexing (ddsi_tran_conn_t conn)
{
  if (conn->m_disable_multiplexing_fn)
    (conn->m_disable_multiplexing_fn) (conn);
}

bool ddsi_conn_peer_locator (ddsi_tran_conn_t conn, ddsi_locator_t * loc)
{
  if (conn->m_peer_locator_fn)
  {
    (conn->m_peer_locator_fn) (conn, loc);
    return true;
  }
  return false;
}

int ddsi_conn_join_mc (ddsi_tran_conn_t conn, const ddsi_locator_t *srcloc, const ddsi_locator_t *mcloc, const struct nn_interface *interf)
{
  return conn->m_factory->m_join_mc_fn (conn, srcloc, mcloc, interf);
}

int ddsi_conn_leave_mc (ddsi_tran_conn_t conn, const ddsi_locator_t *srcloc, const ddsi_locator_t *mcloc, const struct nn_interface *interf)
{
  return conn->m_factory->m_leave_mc_fn (conn, srcloc, mcloc, interf);
}

void ddsi_tran_free (ddsi_tran_base_t base)
{
  if (base)
  {
    if (base->m_trantype == DDSI_TRAN_CONN)
    {
      ddsi_conn_free ((ddsi_tran_conn_t) base);
    }
    else
    {
      ddsi_listener_unblock ((ddsi_tran_listener_t) base);
      ddsi_listener_free ((ddsi_tran_listener_t) base);
    }
  }
}

void ddsi_listener_unblock (ddsi_tran_listener_t listener)
{
  if (listener)
  {
    (listener->m_factory->m_unblock_listener_fn) (listener);
  }
}

void ddsi_listener_free (ddsi_tran_listener_t listener)
{
  if (listener)
  {
    (listener->m_factory->m_release_listener_fn) (listener);
  }
}

int ddsi_is_loopbackaddr (const struct ddsi_domaingv *gv, const ddsi_locator_t *loc)
{
  ddsi_tran_factory_t tran = ddsi_factory_find_supported_kind (gv, loc->kind);
  return tran ? tran->m_is_loopbackaddr_fn (tran, loc) : 0;
}

int ddsi_is_mcaddr (const struct ddsi_domaingv *gv, const ddsi_locator_t *loc)
{
  ddsi_tran_factory_t tran = ddsi_factory_find_supported_kind (gv, loc->kind);
  return tran ? tran->m_is_mcaddr_fn (tran, loc) : 0;
}

int ddsi_is_ssm_mcaddr (const struct ddsi_domaingv *gv, const ddsi_locator_t *loc)
{
  ddsi_tran_factory_t tran = ddsi_factory_find_supported_kind(gv, loc->kind);
  if (tran && tran->m_is_ssm_mcaddr_fn != 0)
    return tran->m_is_ssm_mcaddr_fn (tran, loc);
  return 0;
}

enum ddsi_nearby_address_result ddsi_is_nearby_address (const struct ddsi_domaingv *gv, const ddsi_locator_t *loc, size_t ninterf, const struct nn_interface interf[], size_t *interf_idx)
{
  ddsi_tran_factory_t tran = ddsi_factory_find_supported_kind (gv, loc->kind);
  if (tran == NULL)
    return DNAR_DISTANT;
  return tran->m_is_nearby_address_fn (loc, ninterf, interf, interf_idx);
}

enum ddsi_locator_from_string_result ddsi_locator_from_string (const struct ddsi_domaingv *gv, ddsi_locator_t *loc, const char *str, ddsi_tran_factory_t default_factory)
{
  const char *sep = strchr(str, '/');
  ddsi_tran_factory_t tran;
  if (sep == str) {
    return AFSR_INVALID;
  } else if (sep > str) {
    const char *cur = sep;
    while (cur-- > str)
      if (!isalnum((unsigned char)*cur) && *cur != '_')
        return AFSR_INVALID;
    tran = ddsi_factory_find_with_len(gv, str, (size_t)(sep - str));
    if (tran == NULL)
      return AFSR_UNKNOWN;
  } else {
    /* FIXME: am I happy with defaulting it like this? */
    tran = default_factory;
  }
  return tran->m_locator_from_string_fn (tran, loc, sep ? sep + 1 : str);
}

int ddsi_locator_from_sockaddr (const struct ddsi_tran_factory *tran, ddsi_locator_t *loc, const struct sockaddr *sockaddr)
{
  return tran->m_locator_from_sockaddr_fn (tran, loc, sockaddr);
}

static size_t kindstr (char *dst, size_t sizeof_dst, int32_t kind)
{
  char *wellknown;
  switch (kind)
  {
    case NN_LOCATOR_KIND_TCPv4: wellknown = "tcp/"; break;
    case NN_LOCATOR_KIND_TCPv6: wellknown = "tcp6/"; break;
    case NN_LOCATOR_KIND_UDPv4: wellknown = "udp/"; break;
    case NN_LOCATOR_KIND_UDPv6: wellknown = "udp6/"; break;
    default: wellknown = NULL; break;
  };
  if (wellknown)
    return ddsrt_strlcpy (dst, wellknown, sizeof_dst);
  else
  {
    int pos = snprintf (dst, sizeof_dst, "%"PRId32"/", kind);
    return (pos < 0) ? sizeof_dst : (size_t) pos;
  }
}

static char *ddsi_xlocator_to_string_impl (char *dst, size_t sizeof_dst, const ddsi_xlocator_t *loc)
{
  /* FIXME: should add a "factory" for INVALID locators */
  if (loc->c.kind == NN_LOCATOR_KIND_INVALID) {
    (void) snprintf (dst, sizeof_dst, "invalid/0:0");
  } else if (loc->conn != NULL) {
    struct ddsi_tran_factory const * const tran = loc->conn->m_factory;
    int pos = snprintf (dst, sizeof_dst, "%s/", tran->m_typename);
    if (0 < pos && (size_t)pos < sizeof_dst)
      (void) tran->m_locator_to_string_fn (dst + (size_t)pos, sizeof_dst - (size_t)pos, &loc->c, loc->conn, 1);
  } else {
    /* Because IPv4 and IPv6 addresses are so common we special-case and print them in the usual form
       even if they didn't get mapped to a transport.  To indicate that this mapping never took place
       the kind is still printed as a number, not as (udp|tcp)6? */
    switch (loc->c.kind)
    {
      case NN_LOCATOR_KIND_TCPv4:
      case NN_LOCATOR_KIND_TCPv6:
      case NN_LOCATOR_KIND_UDPv4:
      case NN_LOCATOR_KIND_UDPv6: {
        size_t pos = kindstr (dst, sizeof_dst, loc->c.kind);
        if ((size_t)pos < sizeof_dst)
          (void) ddsi_ipaddr_to_string (dst + (size_t)pos, sizeof_dst - (size_t)pos, &loc->c, 1, NULL);
        break;
      }
      default: {
        const unsigned char * const x = loc->c.address;
        (void) snprintf (dst, sizeof_dst, "%"PRId32"/[%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x]:%"PRIu32,
                         loc->c.kind, x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7], x[8], x[9], x[10], x[11], x[12], x[13], x[14], x[15], loc->c.port);
        break;
      }
    }
  }
  return dst;
}

static char *ddsi_xlocator_to_string_no_port_impl (char *dst, size_t sizeof_dst, const ddsi_xlocator_t *loc)
{
  if (loc->c.kind == NN_LOCATOR_KIND_INVALID) {
    (void) snprintf (dst, sizeof_dst, "invalid/0");
  } else if (loc->conn != NULL) {
    struct ddsi_tran_factory const * const tran = loc->conn->m_factory;
    int pos = snprintf (dst, sizeof_dst, "%s/", tran->m_typename);
    if (0 < pos && (size_t)pos < sizeof_dst)
      (void) tran->m_locator_to_string_fn (dst + (size_t)pos, sizeof_dst - (size_t)pos, &loc->c, loc->conn, 0);
  } else {
    switch (loc->c.kind)
    {
      case NN_LOCATOR_KIND_TCPv4:
      case NN_LOCATOR_KIND_TCPv6:
      case NN_LOCATOR_KIND_UDPv4:
      case NN_LOCATOR_KIND_UDPv6: {
        size_t pos = kindstr (dst, sizeof_dst, loc->c.kind);
        if ((size_t)pos < sizeof_dst)
          (void) ddsi_ipaddr_to_string (dst + (size_t)pos, sizeof_dst - (size_t)pos, &loc->c, 0, NULL);
        break;
      }
      default: {
        const unsigned char * const x = loc->c.address;
        (void) snprintf (dst, sizeof_dst, "%"PRId32"/[%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x]",
                         loc->c.kind, x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7], x[8], x[9], x[10], x[11], x[12], x[13], x[14], x[15]);
      }
    }
  }
  return dst;
}

char *ddsi_xlocator_to_string (char *dst, size_t sizeof_dst, const ddsi_xlocator_t *loc)
{
  return ddsi_xlocator_to_string_impl (dst, sizeof_dst, loc);
}

char *ddsi_locator_to_string (char *dst, size_t sizeof_dst, const ddsi_locator_t *loc)
{
  return ddsi_xlocator_to_string_impl (dst, sizeof_dst, &(const ddsi_xlocator_t) {
    .conn = NULL, .c = *loc
  });
}

char *ddsi_xlocator_to_string_no_port (char *dst, size_t sizeof_dst, const ddsi_xlocator_t *loc)
{
  return ddsi_xlocator_to_string_no_port_impl (dst, sizeof_dst, loc);
}

char *ddsi_locator_to_string_no_port (char *dst, size_t sizeof_dst, const ddsi_locator_t *loc)
{
  return ddsi_xlocator_to_string_no_port_impl (dst, sizeof_dst, &(const ddsi_xlocator_t) {
    .conn = NULL, .c = *loc
  });
}

int ddsi_enumerate_interfaces (ddsi_tran_factory_t factory, enum ddsi_transport_selector transport_selector, ddsrt_ifaddrs_t **interfs)
{
  return factory->m_enumerate_interfaces_fn (factory, transport_selector, interfs);
}
