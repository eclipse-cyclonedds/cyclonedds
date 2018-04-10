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
#ifndef _DDSI_TRAN_H_
#define _DDSI_TRAN_H_

/* DDSI Transport module */

#include "ddsi/q_globals.h"
#include "ddsi/q_protocol.h"

/* Types supporting handles */

#define DDSI_TRAN_CONN 1
#define DDSI_TRAN_LISTENER 2

/* Flags */

#define DDSI_TRAN_ON_CONNECT 0x0001

/* Core types */

typedef struct ddsi_tran_base * ddsi_tran_base_t;
typedef struct ddsi_tran_conn * ddsi_tran_conn_t;
typedef struct ddsi_tran_listener * ddsi_tran_listener_t;
typedef struct ddsi_tran_factory * ddsi_tran_factory_t;
typedef struct ddsi_tran_qos * ddsi_tran_qos_t;

/* Function pointer types */

typedef ssize_t (*ddsi_tran_read_fn_t) (ddsi_tran_conn_t , unsigned char *, size_t);
typedef ssize_t (*ddsi_tran_write_fn_t) (ddsi_tran_conn_t, const struct msghdr *, size_t, uint32_t);
typedef int (*ddsi_tran_locator_fn_t) (ddsi_tran_base_t, nn_locator_t *);
typedef bool (*ddsi_tran_supports_fn_t) (int32_t);
typedef os_handle (*ddsi_tran_handle_fn_t) (ddsi_tran_base_t);
typedef int (*ddsi_tran_listen_fn_t) (ddsi_tran_listener_t);
typedef void (*ddsi_tran_free_fn_t) (void);
typedef void (*ddsi_tran_peer_locator_fn_t) (ddsi_tran_conn_t, nn_locator_t *);
typedef ddsi_tran_conn_t (*ddsi_tran_accept_fn_t) (ddsi_tran_listener_t);
typedef ddsi_tran_conn_t (*ddsi_tran_create_conn_fn_t) (uint32_t, ddsi_tran_qos_t);
typedef ddsi_tran_listener_t (*ddsi_tran_create_listener_fn_t) (int port, ddsi_tran_qos_t);
typedef void (*ddsi_tran_release_conn_fn_t) (ddsi_tran_conn_t);
typedef void (*ddsi_tran_close_conn_fn_t) (ddsi_tran_conn_t);
typedef void (*ddsi_tran_unblock_listener_fn_t) (ddsi_tran_listener_t);
typedef void (*ddsi_tran_release_listener_fn_t) (ddsi_tran_listener_t);
typedef int (*ddsi_tran_join_mc_fn_t) (ddsi_tran_conn_t, const nn_locator_t *srcip, const nn_locator_t *mcip);
typedef int (*ddsi_tran_leave_mc_fn_t) (ddsi_tran_conn_t, const nn_locator_t *srcip, const nn_locator_t *mcip);

/* Data types */

struct ddsi_tran_base
{
  /* Data */

  uint32_t m_port;
  uint32_t m_trantype;
  bool m_multicast;

  /* Functions */

  ddsi_tran_locator_fn_t m_locator_fn;
  ddsi_tran_handle_fn_t m_handle_fn;
};

struct ddsi_tran_conn
{
  struct ddsi_tran_base m_base;

  /* Functions */

  ddsi_tran_read_fn_t m_read_fn;
  ddsi_tran_write_fn_t m_write_fn;
  ddsi_tran_peer_locator_fn_t m_peer_locator_fn;

  /* Data */

  bool m_server;
  bool m_connless;
  bool m_stream;
  bool m_closed;
  os_atomic_uint32_t m_count;

  /* Relationships */

  ddsi_tran_factory_t m_factory;
  ddsi_tran_listener_t m_listener;
  ddsi_tran_conn_t m_conn;
};

struct ddsi_tran_listener
{
  struct ddsi_tran_base m_base;

  /* Functions */

  ddsi_tran_listen_fn_t m_listen_fn;
  ddsi_tran_accept_fn_t m_accept_fn;

  /* Relationships */

  ddsi_tran_conn_t m_connections;
  ddsi_tran_factory_t m_factory;
  ddsi_tran_listener_t m_listener;
};

struct ddsi_tran_factory
{
  /* Functions */

  ddsi_tran_create_conn_fn_t m_create_conn_fn;
  ddsi_tran_create_listener_fn_t m_create_listener_fn;
  ddsi_tran_release_conn_fn_t m_release_conn_fn;
  ddsi_tran_close_conn_fn_t m_close_conn_fn;
  ddsi_tran_unblock_listener_fn_t m_unblock_listener_fn;
  ddsi_tran_release_listener_fn_t m_release_listener_fn;
  ddsi_tran_supports_fn_t m_supports_fn;
  ddsi_tran_free_fn_t m_free_fn;
  ddsi_tran_join_mc_fn_t m_join_mc_fn;
  ddsi_tran_leave_mc_fn_t m_leave_mc_fn;

  /* Data */

  int32_t m_kind;
  const char * m_typename;
  bool m_connless;
  bool m_stream;

  /* Relationships */

  ddsi_tran_factory_t m_factory;
};

struct ddsi_tran_qos
{
  /* QoS Data */

  bool m_multicast;
  int m_diffserv;
};

/* Functions and pseudo functions (macro wrappers) */

#define ddsi_tran_type(b) (((ddsi_tran_base_t) (b))->m_trantype)
#define ddsi_tran_port(b) (((ddsi_tran_base_t) (b))->m_port)
int ddsi_tran_locator (ddsi_tran_base_t base, nn_locator_t * loc);
void ddsi_tran_free (ddsi_tran_base_t base);
void ddsi_tran_free_qos (ddsi_tran_qos_t qos);
ddsi_tran_qos_t ddsi_tran_create_qos (void);
os_handle ddsi_tran_handle (ddsi_tran_base_t base);

#define ddsi_factory_create_listener(f,p,q) (((f)->m_create_listener_fn) ((p), (q)))
#define ddsi_factory_supports(f,k) (((f)->m_supports_fn) (k))

ddsi_tran_conn_t ddsi_factory_create_conn
(
  ddsi_tran_factory_t factory,
  uint32_t port,
  ddsi_tran_qos_t qos
);
void ddsi_tran_factories_fini (void);
void ddsi_factory_add (ddsi_tran_factory_t factory);
void ddsi_factory_free (ddsi_tran_factory_t factory);
ddsi_tran_factory_t ddsi_factory_find (const char * type);
void ddsi_factory_conn_init (ddsi_tran_factory_t factory, ddsi_tran_conn_t conn);

#define ddsi_conn_handle(c) (ddsi_tran_handle (&(c)->m_base))
#define ddsi_conn_locator(c,l) (ddsi_tran_locator (&(c)->m_base,(l)))
OSAPI_EXPORT ssize_t ddsi_conn_write (ddsi_tran_conn_t conn, const struct msghdr * msg, size_t len, uint32_t flags);
ssize_t ddsi_conn_read (ddsi_tran_conn_t conn, unsigned char * buf, size_t len);
bool ddsi_conn_peer_locator (ddsi_tran_conn_t conn, nn_locator_t * loc);
void ddsi_conn_add_ref (ddsi_tran_conn_t conn);
void ddsi_conn_free (ddsi_tran_conn_t conn);

int ddsi_conn_join_mc (ddsi_tran_conn_t conn, const nn_locator_t *srcip, const nn_locator_t *mcip);
int ddsi_conn_leave_mc (ddsi_tran_conn_t conn, const nn_locator_t *srcip, const nn_locator_t *mcip);

#define ddsi_listener_locator(s,l) (ddsi_tran_locator (&(s)->m_base,(l)))
ddsi_tran_conn_t ddsi_listener_accept (ddsi_tran_listener_t listener);
int ddsi_listener_listen (ddsi_tran_listener_t listener);
void ddsi_listener_unblock (ddsi_tran_listener_t listener);
void ddsi_listener_free (ddsi_tran_listener_t listener);

#endif
