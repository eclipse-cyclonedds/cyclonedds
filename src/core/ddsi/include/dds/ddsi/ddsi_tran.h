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

#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsi/q_protocol.h"
#include "dds/ddsi/q_config.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct nn_interface;

/* Types supporting handles */

#define DDSI_TRAN_CONN 1
#define DDSI_TRAN_LISTENER 2

/* Flags */

#define DDSI_TRAN_ON_CONNECT 0x0001

#if DDSRT_HAVE_IPV6 == 1
# define DDSI_LOCATORSTRLEN INET6_ADDRSTRLEN_EXTENDED
#else
# define DDSI_LOCATORSTRLEN INET_ADDRSTRLEN_EXTENDED
#endif

/* Core types */

typedef struct ddsi_tran_base * ddsi_tran_base_t;
typedef struct ddsi_tran_conn * ddsi_tran_conn_t;
typedef struct ddsi_tran_listener * ddsi_tran_listener_t;
typedef struct ddsi_tran_factory * ddsi_tran_factory_t;
typedef struct ddsi_tran_qos ddsi_tran_qos_t;

/* Function pointer types */

typedef ssize_t (*ddsi_tran_read_fn_t) (ddsi_tran_conn_t, unsigned char *, size_t, bool, nn_locator_t *);
typedef ssize_t (*ddsi_tran_write_fn_t) (ddsi_tran_conn_t, const nn_locator_t *, size_t, const ddsrt_iovec_t *, uint32_t);
typedef int (*ddsi_tran_locator_fn_t) (ddsi_tran_factory_t, ddsi_tran_base_t, nn_locator_t *);
typedef bool (*ddsi_tran_supports_fn_t) (const struct ddsi_tran_factory *, int32_t);
typedef ddsrt_socket_t (*ddsi_tran_handle_fn_t) (ddsi_tran_base_t);
typedef int (*ddsi_tran_listen_fn_t) (ddsi_tran_listener_t);
typedef void (*ddsi_tran_free_fn_t) (ddsi_tran_factory_t);
typedef void (*ddsi_tran_peer_locator_fn_t) (ddsi_tran_conn_t, nn_locator_t *);
typedef void (*ddsi_tran_disable_multiplexing_fn_t) (ddsi_tran_conn_t);
typedef ddsi_tran_conn_t (*ddsi_tran_accept_fn_t) (ddsi_tran_listener_t);
typedef dds_return_t (*ddsi_tran_create_conn_fn_t) (ddsi_tran_conn_t *conn, ddsi_tran_factory_t fact, uint32_t, const struct ddsi_tran_qos *);
typedef dds_return_t (*ddsi_tran_create_listener_fn_t) (ddsi_tran_listener_t *listener, ddsi_tran_factory_t fact, uint32_t port, const struct ddsi_tran_qos *);
typedef void (*ddsi_tran_release_conn_fn_t) (ddsi_tran_conn_t);
typedef void (*ddsi_tran_close_conn_fn_t) (ddsi_tran_conn_t);
typedef void (*ddsi_tran_unblock_listener_fn_t) (ddsi_tran_listener_t);
typedef void (*ddsi_tran_release_listener_fn_t) (ddsi_tran_listener_t);
typedef int (*ddsi_tran_join_mc_fn_t) (ddsi_tran_conn_t, const nn_locator_t *srcip, const nn_locator_t *mcip, const struct nn_interface *interf);
typedef int (*ddsi_tran_leave_mc_fn_t) (ddsi_tran_conn_t, const nn_locator_t *srcip, const nn_locator_t *mcip, const struct nn_interface *interf);
typedef int (*ddsi_is_mcaddr_fn_t) (const struct ddsi_tran_factory *tran, const nn_locator_t *loc);
typedef int (*ddsi_is_ssm_mcaddr_fn_t) (const struct ddsi_tran_factory *tran, const nn_locator_t *loc);
typedef int (*ddsi_is_valid_port_fn_t) (const struct ddsi_tran_factory *tran, uint32_t port);
typedef uint32_t (*ddsi_receive_buffer_size_fn_t) (const struct ddsi_tran_factory *fact);

enum ddsi_nearby_address_result {
  DNAR_DISTANT,
  DNAR_LOCAL,
  DNAR_SAME
};

typedef enum ddsi_nearby_address_result (*ddsi_is_nearby_address_fn_t) (const nn_locator_t *loc, const nn_locator_t *ownloc, size_t ninterf, const struct nn_interface *interf);

enum ddsi_locator_from_string_result {
  AFSR_OK,      /* conversion succeeded */
  AFSR_INVALID, /* bogus input */
  AFSR_UNKNOWN, /* transport or hostname lookup failure */
  AFSR_MISMATCH /* recognised format, but mismatch with expected (e.g., IPv4/IPv6) */
};

typedef enum ddsi_locator_from_string_result (*ddsi_locator_from_string_fn_t) (const struct ddsi_tran_factory *tran, nn_locator_t *loc, const char *str);

typedef char * (*ddsi_locator_to_string_fn_t) (char *dst, size_t sizeof_dst, const nn_locator_t *loc, int with_port);

typedef int (*ddsi_enumerate_interfaces_fn_t) (ddsi_tran_factory_t tran, enum transport_selector transport_selector, ddsrt_ifaddrs_t **interfs);

/* Data types */
struct ddsi_domaingv;
struct ddsi_tran_base
{
  /* Data */

  uint32_t m_port;
  uint32_t m_trantype;
  bool m_multicast;
  struct ddsi_domaingv *gv;

  /* Functions */

  ddsi_tran_handle_fn_t m_handle_fn;
};

struct ddsi_tran_conn
{
  struct ddsi_tran_base m_base;

  /* Functions */

  ddsi_tran_read_fn_t m_read_fn;
  ddsi_tran_write_fn_t m_write_fn;
  ddsi_tran_peer_locator_fn_t m_peer_locator_fn;
  ddsi_tran_disable_multiplexing_fn_t m_disable_multiplexing_fn;
  ddsi_tran_locator_fn_t m_locator_fn;

  /* Data */

  bool m_server;
  bool m_connless;
  bool m_stream;
  bool m_closed;
  ddsrt_atomic_uint32_t m_count;

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
  ddsi_tran_locator_fn_t m_locator_fn;

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
  ddsi_is_mcaddr_fn_t m_is_mcaddr_fn;
  ddsi_is_ssm_mcaddr_fn_t m_is_ssm_mcaddr_fn;
  ddsi_is_nearby_address_fn_t m_is_nearby_address_fn;
  ddsi_locator_from_string_fn_t m_locator_from_string_fn;
  ddsi_locator_to_string_fn_t m_locator_to_string_fn;
  ddsi_enumerate_interfaces_fn_t m_enumerate_interfaces_fn;
  ddsi_is_valid_port_fn_t m_is_valid_port_fn;
  ddsi_receive_buffer_size_fn_t m_receive_buffer_size_fn;

  /* Data */

  int32_t m_kind;
  const char *m_typename;
  const char *m_default_spdp_address;
  bool m_connless;
  bool m_stream;
  struct ddsi_domaingv *gv;

  /* Relationships */

  ddsi_tran_factory_t m_factory;
};

enum ddsi_tran_qos_purpose {
  DDSI_TRAN_QOS_XMIT,
  DDSI_TRAN_QOS_RECV_UC,
  DDSI_TRAN_QOS_RECV_MC
};

struct ddsi_tran_qos
{
  enum ddsi_tran_qos_purpose m_purpose;
  int m_diffserv;
};

void ddsi_tran_factories_fini (struct ddsi_domaingv *gv);
void ddsi_factory_add (struct ddsi_domaingv *gv, ddsi_tran_factory_t factory);
DDS_EXPORT void ddsi_factory_free (ddsi_tran_factory_t factory);
DDS_EXPORT ddsi_tran_factory_t ddsi_factory_find (const struct ddsi_domaingv *gv, const char * type);
ddsi_tran_factory_t ddsi_factory_find_supported_kind (const struct ddsi_domaingv *gv, int32_t kind);
void ddsi_factory_conn_init (const struct ddsi_tran_factory *factory, ddsi_tran_conn_t conn);

inline bool ddsi_factory_supports (const struct ddsi_tran_factory *factory, int32_t kind) {
  return factory->m_supports_fn (factory, kind);
}
inline int ddsi_is_valid_port (const struct ddsi_tran_factory *factory, uint32_t port) {
  return factory->m_is_valid_port_fn (factory, port);
}
inline uint32_t ddsi_receive_buffer_size (const struct ddsi_tran_factory *factory) {
  return factory->m_receive_buffer_size_fn (factory);
}
inline dds_return_t ddsi_factory_create_conn (ddsi_tran_conn_t *conn, ddsi_tran_factory_t factory, uint32_t port, const struct ddsi_tran_qos *qos) {
  *conn = NULL;
  if (!ddsi_is_valid_port (factory, port))
    return DDS_RETCODE_BAD_PARAMETER;
  return factory->m_create_conn_fn (conn, factory, port, qos);
}
inline dds_return_t ddsi_factory_create_listener (ddsi_tran_listener_t *listener, ddsi_tran_factory_t factory, uint32_t port, const struct ddsi_tran_qos *qos) {
  *listener = NULL;
  if (!ddsi_is_valid_port (factory, port))
    return DDS_RETCODE_BAD_PARAMETER;
  return factory->m_create_listener_fn (listener, factory, port, qos);
}

void ddsi_tran_free (ddsi_tran_base_t base);
inline ddsrt_socket_t ddsi_tran_handle (ddsi_tran_base_t base) {
  return base->m_handle_fn (base);
}
inline ddsrt_socket_t ddsi_conn_handle (ddsi_tran_conn_t conn) {
  return conn->m_base.m_handle_fn (&conn->m_base);
}
inline uint32_t ddsi_conn_type (const struct ddsi_tran_conn *conn) {
  return conn->m_base.m_trantype;
}
inline uint32_t ddsi_conn_port (const struct ddsi_tran_conn *conn) {
  return conn->m_base.m_port;
}
inline int ddsi_conn_locator (ddsi_tran_conn_t conn, nn_locator_t * loc) {
  return conn->m_locator_fn (conn->m_factory, &conn->m_base, loc);
}
inline ssize_t ddsi_conn_write (ddsi_tran_conn_t conn, const nn_locator_t *dst, size_t niov, const ddsrt_iovec_t *iov, uint32_t flags) {
  return conn->m_closed ? -1 : (conn->m_write_fn) (conn, dst, niov, iov, flags);
}
inline ssize_t ddsi_conn_read (ddsi_tran_conn_t conn, unsigned char * buf, size_t len, bool allow_spurious, nn_locator_t *srcloc) {
  return conn->m_closed ? -1 : conn->m_read_fn (conn, buf, len, allow_spurious, srcloc);
}
bool ddsi_conn_peer_locator (ddsi_tran_conn_t conn, nn_locator_t * loc);
void ddsi_conn_disable_multiplexing (ddsi_tran_conn_t conn);
void ddsi_conn_add_ref (ddsi_tran_conn_t conn);
void ddsi_conn_free (ddsi_tran_conn_t conn);
int ddsi_conn_join_mc (ddsi_tran_conn_t conn, const nn_locator_t *srcip, const nn_locator_t *mcip, const struct nn_interface *interf);
int ddsi_conn_leave_mc (ddsi_tran_conn_t conn, const nn_locator_t *srcip, const nn_locator_t *mcip, const struct nn_interface *interf);
void ddsi_conn_transfer_group_membership (ddsi_tran_conn_t conn, ddsi_tran_conn_t newconn);
int ddsi_conn_rejoin_transferred_mcgroups (ddsi_tran_conn_t conn);
int ddsi_is_mcaddr (const struct ddsi_domaingv *gv, const nn_locator_t *loc);
int ddsi_is_ssm_mcaddr (const struct ddsi_domaingv *gv, const nn_locator_t *loc);
enum ddsi_nearby_address_result ddsi_is_nearby_address (const nn_locator_t *loc, const nn_locator_t *ownloc, size_t ninterf, const struct nn_interface *interf);

DDS_EXPORT enum ddsi_locator_from_string_result ddsi_locator_from_string (const struct ddsi_domaingv *gv, nn_locator_t *loc, const char *str, ddsi_tran_factory_t default_factory);

/*  8 for transport/
    1 for [
   48 for IPv6 hex digits (3*16) + separators
    2 for ]:
   10 for port (DDSI loc has signed 32-bit)
    1 for terminator
   --
   70
*/
#define DDSI_LOCSTRLEN 70

char *ddsi_locator_to_string (char *dst, size_t sizeof_dst, const nn_locator_t *loc);
char *ddsi_locator_to_string_no_port (char *dst, size_t sizeof_dst, const nn_locator_t *loc);

int ddsi_enumerate_interfaces (ddsi_tran_factory_t factory, enum transport_selector transport_selector, ddsrt_ifaddrs_t **interfs);

inline int ddsi_listener_locator (ddsi_tran_listener_t listener, nn_locator_t *loc) {
  return listener->m_locator_fn (listener->m_factory, &listener->m_base, loc);
}
inline int ddsi_listener_listen (ddsi_tran_listener_t listener) {
  return listener->m_listen_fn (listener);
}
inline ddsi_tran_conn_t ddsi_listener_accept (ddsi_tran_listener_t listener) {
  return listener->m_accept_fn (listener);
}
void ddsi_listener_unblock (ddsi_tran_listener_t listener);
void ddsi_listener_free (ddsi_tran_listener_t listener);

#if defined (__cplusplus)
}
#endif

#endif
