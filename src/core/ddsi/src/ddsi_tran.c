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
#include <string.h>
#include <ctype.h>
#include "os/os.h"
#include "ddsi/ddsi_tran.h"
#include "ddsi/q_config.h"
#include "ddsi/q_log.h"

static ddsi_tran_factory_t ddsi_tran_factories = NULL;

extern inline uint32_t ddsi_conn_type (ddsi_tran_conn_t conn);
extern inline uint32_t ddsi_conn_port (ddsi_tran_conn_t conn);
extern inline ddsi_tran_listener_t ddsi_factory_create_listener (ddsi_tran_factory_t factory, int port, ddsi_tran_qos_t qos);
extern inline bool ddsi_factory_supports (ddsi_tran_factory_t factory, int32_t kind);
extern inline os_socket ddsi_conn_handle (ddsi_tran_conn_t conn);
extern inline int ddsi_conn_locator (ddsi_tran_conn_t conn, nn_locator_t * loc);
extern inline os_socket ddsi_tran_handle (ddsi_tran_base_t base);
extern inline ddsi_tran_conn_t ddsi_factory_create_conn (ddsi_tran_factory_t factory, uint32_t port, ddsi_tran_qos_t qos);
extern inline int ddsi_tran_locator (ddsi_tran_base_t base, nn_locator_t * loc);
extern inline int ddsi_listener_locator (ddsi_tran_listener_t listener, nn_locator_t * loc);
extern inline int ddsi_listener_listen (ddsi_tran_listener_t listener);
extern inline ddsi_tran_conn_t ddsi_listener_accept (ddsi_tran_listener_t listener);
extern inline ssize_t ddsi_conn_read (ddsi_tran_conn_t conn, unsigned char * buf, size_t len, bool allow_spurious, nn_locator_t *srcloc);
extern inline ssize_t ddsi_conn_write (ddsi_tran_conn_t conn, const nn_locator_t *dst, size_t niov, const os_iovec_t *iov, uint32_t flags);

void ddsi_factory_add (ddsi_tran_factory_t factory)
{
  factory->m_factory = ddsi_tran_factories;
  ddsi_tran_factories = factory;
}

ddsi_tran_factory_t ddsi_factory_find (const char * type)
{
  /* FIXME: should speed up */
  ddsi_tran_factory_t factory = ddsi_tran_factories;

  while (factory)
  {
    if (strcmp (factory->m_typename, type) == 0)
    {
      break;
    }
    factory = factory->m_factory;
  }

  return factory;
}

void ddsi_tran_factories_fini (void)
{
  ddsi_tran_factory_t factory;
  while ((factory = ddsi_tran_factories) != NULL)
  {
    /* Keep the factory in the list for the duration of "factory_free" so that
       conversion of locator kind to factory remains possible. */
    ddsi_tran_factory_t next = factory->m_factory;
    ddsi_factory_free (factory);
    ddsi_tran_factories = next;
  }
}

static ddsi_tran_factory_t ddsi_factory_find_with_len (const char * type, size_t len)
{
  /* FIXME: should speed up */
  ddsi_tran_factory_t factory = ddsi_tran_factories;

  while (factory)
  {
    if (strncmp (factory->m_typename, type, len) == 0 && factory->m_typename[len] == 0)
    {
      break;
    }
    factory = factory->m_factory;
  }

  return factory;
}

ddsi_tran_factory_t ddsi_factory_find_supported_kind (int32_t kind)
{
  /* FIXME: MUST speed up */
  ddsi_tran_factory_t factory;
  for (factory = ddsi_tran_factories; factory; factory = factory->m_factory) {
    if (factory->m_supports_fn(kind)) {
      return factory;
    }
  }
  return NULL;
}

void ddsi_factory_free (ddsi_tran_factory_t factory)
{
  if (factory && factory->m_free_fn)
  {
    (factory->m_free_fn) ();
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
      if (ddsi_conn_handle (conn) != OS_INVALID_SOCKET)
      {
        unsigned i;
        for (i = 0; i < gv.n_recv_threads; i++)
        {
          if (!gv.recv_threads[i].ts)
            assert (!gv.rtps_keepgoing);
          else
          {
            switch (gv.recv_threads[i].arg.mode)
            {
              case RTM_MANY:
                os_sockWaitsetRemove (gv.recv_threads[i].arg.u.many.ws, conn);
                break;
              case RTM_SINGLE:
                if (gv.recv_threads[i].arg.u.single.conn == conn)
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
    if (os_atomic_dec32_ov (&conn->m_count) == 1)
    {
      (conn->m_factory->m_release_conn_fn) (conn);
    }
  }
}

void ddsi_conn_add_ref (ddsi_tran_conn_t conn)
{
  os_atomic_inc32 (&conn->m_count);
}

void ddsi_factory_conn_init (ddsi_tran_factory_t factory, ddsi_tran_conn_t conn)
{
  os_atomic_st32 (&conn->m_count, 1);
  conn->m_connless = factory->m_connless;
  conn->m_stream = factory->m_stream;
  conn->m_factory = factory;
}

void ddsi_conn_disable_multiplexing (ddsi_tran_conn_t conn)
{
  if (conn->m_disable_multiplexing_fn) {
    (conn->m_disable_multiplexing_fn) (conn);
  }
}

bool ddsi_conn_peer_locator (ddsi_tran_conn_t conn, nn_locator_t * loc)
{
  if (conn->m_peer_locator_fn)
  {
    (conn->m_peer_locator_fn) (conn, loc);
    return true;
  }
  return false;
}

void ddsi_tran_free_qos (ddsi_tran_qos_t qos)
{
  os_free (qos);
}

int ddsi_conn_join_mc (ddsi_tran_conn_t conn, const nn_locator_t *srcloc, const nn_locator_t *mcloc, const struct nn_interface *interf)
{
  return conn->m_factory->m_join_mc_fn (conn, srcloc, mcloc, interf);
}

int ddsi_conn_leave_mc (ddsi_tran_conn_t conn, const nn_locator_t *srcloc, const nn_locator_t *mcloc, const struct nn_interface *interf)
{
  return conn->m_factory->m_leave_mc_fn (conn, srcloc, mcloc, interf);
}

ddsi_tran_qos_t ddsi_tran_create_qos (void)
{
  ddsi_tran_qos_t qos;
  qos = (ddsi_tran_qos_t) os_malloc (sizeof (*qos));
  memset (qos, 0, sizeof (*qos));
  return qos;
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

int ddsi_is_mcaddr (const nn_locator_t *loc)
{
  ddsi_tran_factory_t tran = ddsi_factory_find_supported_kind (loc->kind);
  return tran ? tran->m_is_mcaddr_fn (tran, loc) : 0;
}

int ddsi_is_ssm_mcaddr (const nn_locator_t *loc)
{
  ddsi_tran_factory_t tran = ddsi_factory_find_supported_kind(loc->kind);
  return tran ? tran->m_is_ssm_mcaddr_fn (tran, loc) : 0;
}

enum ddsi_nearby_address_result ddsi_is_nearby_address (const nn_locator_t *loc, size_t ninterf, const struct nn_interface interf[])
{
  ddsi_tran_factory_t tran = ddsi_factory_find_supported_kind(loc->kind);
  return tran ? tran->m_is_nearby_address_fn (tran, loc, ninterf, interf) : DNAR_DISTANT;
}

enum ddsi_locator_from_string_result ddsi_locator_from_string (nn_locator_t *loc, const char *str)
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
    tran = ddsi_factory_find_with_len(str, (size_t)(sep - str));
    if (tran == NULL)
      return AFSR_UNKNOWN;
  } else {
    /* FIXME: am I happy with defaulting it like this? */
    tran = gv.m_factory;
  }
  return tran->m_locator_from_string_fn (tran, loc, sep ? sep + 1 : str);
}

char *ddsi_locator_to_string (char *dst, size_t sizeof_dst, const nn_locator_t *loc)
{
  /* FIXME: should add a "factory" for INVALID locators */
  if (loc->kind != NN_LOCATOR_KIND_INVALID) {
    ddsi_tran_factory_t tran = ddsi_factory_find_supported_kind(loc->kind);
    int pos = snprintf (dst, sizeof_dst, "%s/", tran->m_typename);
    if (0 < pos && (size_t)pos < sizeof_dst)
      (void) tran->m_locator_to_string_fn (tran, dst + (size_t)pos, sizeof_dst - (size_t)pos, loc, 1);
  } else {
    snprintf (dst, sizeof_dst, "invalid/0:0");
  }
  return dst;
}

char *ddsi_locator_to_string_no_port (char *dst, size_t sizeof_dst, const nn_locator_t *loc)
{
  if (loc->kind != NN_LOCATOR_KIND_INVALID) {
    ddsi_tran_factory_t tran = ddsi_factory_find_supported_kind(loc->kind);
    int pos = snprintf (dst, sizeof_dst, "%s/", tran->m_typename);
    if (0 < pos && (size_t)pos < sizeof_dst)
      (void) tran->m_locator_to_string_fn (tran, dst + (size_t)pos, sizeof_dst - (size_t)pos, loc, 0);
  } else {
    snprintf (dst, sizeof_dst, "invalid/0");
  }
  return dst;
}

int ddsi_enumerate_interfaces (ddsi_tran_factory_t factory, os_ifaddrs_t **interfs)
{
  return factory->m_enumerate_interfaces_fn (factory, interfs);
}
