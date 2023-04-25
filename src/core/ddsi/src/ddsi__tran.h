// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__TRAN_H
#define DDSI__TRAN_H

/* DDSI Transport module */

#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsi/ddsi_locator.h"
#include "dds/ddsi/ddsi_config.h"
#include "dds/ddsi/ddsi_tran.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_network_interface;
struct ddsi_domaingv;

/* Types supporting handles */
#define DDSI_TRAN_CONN 1
#define DDSI_TRAN_LISTENER 2

/* Flags */
#define DDSI_TRAN_ON_CONNECT 0x0001

enum ddsi_tran_qos_purpose {
  DDSI_TRAN_QOS_XMIT_UC, // will send unicast only
  DDSI_TRAN_QOS_XMIT_MC, // may send unicast or multicast
  DDSI_TRAN_QOS_RECV_UC, // will be used for receiving unicast
  DDSI_TRAN_QOS_RECV_MC  // will be used for receiving multicast
};

/* Function pointer types */
typedef ssize_t (*ddsi_tran_read_fn_t) (struct ddsi_tran_conn *, unsigned char *, size_t, bool, ddsi_locator_t *);
typedef ssize_t (*ddsi_tran_write_fn_t) (struct ddsi_tran_conn *, const ddsi_locator_t *, size_t, const ddsrt_iovec_t *, uint32_t);
typedef int (*ddsi_tran_locator_fn_t) (struct ddsi_tran_factory *, struct ddsi_tran_base *, ddsi_locator_t *);
typedef bool (*ddsi_tran_supports_fn_t) (const struct ddsi_tran_factory *, int32_t);
typedef ddsrt_socket_t (*ddsi_tran_handle_fn_t) (struct ddsi_tran_base *);
typedef int (*ddsi_tran_listen_fn_t) (struct ddsi_tran_listener *);
typedef void (*ddsi_tran_free_fn_t) (struct ddsi_tran_factory *);
typedef void (*ddsi_tran_peer_locator_fn_t) (struct ddsi_tran_conn *, ddsi_locator_t *);
typedef void (*ddsi_tran_disable_multiplexing_fn_t) (struct ddsi_tran_conn *);
typedef struct ddsi_tran_conn * (*ddsi_tran_accept_fn_t) (struct ddsi_tran_listener *);
typedef dds_return_t (*ddsi_tran_create_conn_fn_t) (struct ddsi_tran_conn **conn, struct ddsi_tran_factory * fact, uint32_t, const struct ddsi_tran_qos *);
typedef dds_return_t (*ddsi_tran_create_listener_fn_t) (struct ddsi_tran_listener **listener, struct ddsi_tran_factory * fact, uint32_t port, const struct ddsi_tran_qos *);
typedef void (*ddsi_tran_release_conn_fn_t) (struct ddsi_tran_conn *);
typedef void (*ddsi_tran_close_conn_fn_t) (struct ddsi_tran_conn *);
typedef void (*ddsi_tran_unblock_listener_fn_t) (struct ddsi_tran_listener *);
typedef void (*ddsi_tran_release_listener_fn_t) (struct ddsi_tran_listener *);
typedef int (*ddsi_tran_join_mc_fn_t) (struct ddsi_tran_conn *, const ddsi_locator_t *srcip, const ddsi_locator_t *mcip, const struct ddsi_network_interface *interf);
typedef int (*ddsi_tran_leave_mc_fn_t) (struct ddsi_tran_conn *, const ddsi_locator_t *srcip, const ddsi_locator_t *mcip, const struct ddsi_network_interface *interf);
typedef int (*ddsi_is_loopbackaddr_fn_t) (const struct ddsi_tran_factory *tran, const ddsi_locator_t *loc);
typedef int (*ddsi_is_mcaddr_fn_t) (const struct ddsi_tran_factory *tran, const ddsi_locator_t *loc);
typedef int (*ddsi_is_ssm_mcaddr_fn_t) (const struct ddsi_tran_factory *tran, const ddsi_locator_t *loc);
typedef int (*ddsi_is_valid_port_fn_t) (const struct ddsi_tran_factory *tran, uint32_t port);
typedef uint32_t (*ddsi_receive_buffer_size_fn_t) (const struct ddsi_tran_factory *fact);

enum ddsi_nearby_address_result {
  DNAR_UNREACHABLE, /**< no way to reach this address */
  DNAR_DISTANT,     /**< address does not match one of the enabled interfaces */
  DNAR_LOCAL,       /**< address is of some other host on one of the enabled interfaces */
  DNAR_SELF         /**< address is of this host */
};

typedef enum ddsi_nearby_address_result (*ddsi_is_nearby_address_fn_t) (const ddsi_locator_t *loc, size_t ninterf, const struct ddsi_network_interface *interf, size_t *interf_idx);

enum ddsi_locator_from_string_result {
  AFSR_OK,      /* conversion succeeded */
  AFSR_INVALID, /* bogus input */
  AFSR_UNKNOWN, /* transport or hostname lookup failure */
  AFSR_MISMATCH /* recognised format, but mismatch with expected (e.g., IPv4/IPv6) */
};

typedef enum ddsi_locator_from_string_result (*ddsi_locator_from_string_fn_t) (const struct ddsi_tran_factory *tran, ddsi_locator_t *loc, const char *str);

typedef int (*ddsi_locator_from_sockaddr_fn_t) (const struct ddsi_tran_factory *tran, ddsi_locator_t *loc, const struct sockaddr *sockaddr);

typedef char * (*ddsi_locator_to_string_fn_t) (char *dst, size_t sizeof_dst, const ddsi_locator_t *loc, struct ddsi_tran_conn * conn, int with_port);

typedef int (*ddsi_enumerate_interfaces_fn_t) (struct ddsi_tran_factory * tran, enum ddsi_transport_selector transport_selector, ddsrt_ifaddrs_t **interfs);

/* Data types */
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

  const struct ddsi_network_interface *m_interf;
  struct ddsi_tran_factory * m_factory;
  struct ddsi_tran_listener * m_listener;
  struct ddsi_tran_conn * m_conn;
};

struct ddsi_tran_listener
{
  struct ddsi_tran_base m_base;

  /* Functions */
  ddsi_tran_listen_fn_t m_listen_fn;
  ddsi_tran_accept_fn_t m_accept_fn;
  ddsi_tran_locator_fn_t m_locator_fn;

  /* Relationships */
  struct ddsi_tran_conn * m_connections;
  struct ddsi_tran_factory * m_factory;
  struct ddsi_tran_listener * m_listener;
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
  ddsi_is_loopbackaddr_fn_t m_is_loopbackaddr_fn;
  ddsi_is_mcaddr_fn_t m_is_mcaddr_fn;
  ddsi_is_ssm_mcaddr_fn_t m_is_ssm_mcaddr_fn;
  ddsi_is_nearby_address_fn_t m_is_nearby_address_fn;
  ddsi_locator_from_string_fn_t m_locator_from_string_fn;
  ddsi_locator_to_string_fn_t m_locator_to_string_fn;
  ddsi_enumerate_interfaces_fn_t m_enumerate_interfaces_fn;
  ddsi_is_valid_port_fn_t m_is_valid_port_fn;
  ddsi_receive_buffer_size_fn_t m_receive_buffer_size_fn;
  ddsi_locator_from_sockaddr_fn_t m_locator_from_sockaddr_fn;

  /* Data */

  /// Transport name, also used as prefix in string representation of locator (e.g., udp/1.2.3.4)
  const char *m_typename;

  /// Whether this is a connection-oriented transport like TCP (false), where a socket communicates
  /// with one other socket after connecting; or whether it can send to any address at any time like
  /// UDP (true).
  bool m_connless;

  /// Whether this transport deals with byte streams (TCP, true) or with datagrams (UDP, false). A
  /// byte stream forces the upper layer to do add some framing.
  bool m_stream;

  /// Whether this transport is enabled for DDS communications. Only locators mapping handled
  /// by enabled transports are taken into account when parsing discovery data.
  ///
  /// The usefulness of disabled transports is (currently) limited to running in UDP mode while using
  /// the TCP transport code as a portable means for providing a debug interface.
  bool m_enable;

  /// Whether this transport should be included in SPDP advertisements. Not all transports are
  /// created equally: those that only provide a representation for an integrated pub-sub messaging
  /// system that can be used to by-pass RTPS should not by included in anything related to SPDP.
  bool m_enable_spdp;

  /// Default SPDP address for this transport (the spec only gives an UDPv4 default one), NULL if
  /// no default address exists.
  const char *m_default_spdp_address;

  struct ddsi_domaingv *gv;

  /* Relationships */
  struct ddsi_tran_factory * m_factory;
};

struct ddsi_tran_qos
{
  enum ddsi_tran_qos_purpose m_purpose;
  int m_diffserv;
  struct ddsi_network_interface *m_interface; // only for purpose = XMIT
};

/** @component transport */
void ddsi_tran_factories_fini (struct ddsi_domaingv *gv);

/** @component transport */
void ddsi_factory_add (struct ddsi_domaingv *gv, struct ddsi_tran_factory * factory);

/** @component transport */
void ddsi_factory_free (struct ddsi_tran_factory * factory);

/** @component transport */
struct ddsi_tran_factory * ddsi_factory_find (const struct ddsi_domaingv *gv, const char * type);

/** @component transport */
struct ddsi_tran_factory * ddsi_factory_find_supported_kind (const struct ddsi_domaingv *gv, int32_t kind);

/** @component transport */
void ddsi_factory_conn_init (const struct ddsi_tran_factory *factory, const struct ddsi_network_interface *interf, struct ddsi_tran_conn * conn);

/** @component transport */
inline bool ddsi_factory_supports (const struct ddsi_tran_factory *factory, int32_t kind) {
  return factory->m_supports_fn (factory, kind);
}

/** @component transport */
inline int ddsi_is_valid_port (const struct ddsi_tran_factory *factory, uint32_t port) {
  return factory->m_is_valid_port_fn (factory, port);
}

/** @component transport */
inline uint32_t ddsi_receive_buffer_size (const struct ddsi_tran_factory *factory) {
  return factory->m_receive_buffer_size_fn (factory);
}

/** @component transport */
inline dds_return_t ddsi_factory_create_conn (struct ddsi_tran_conn **conn, struct ddsi_tran_factory * factory, uint32_t port, const struct ddsi_tran_qos *qos) {
  *conn = NULL;
  if ((qos->m_interface != NULL) != (qos->m_purpose == DDSI_TRAN_QOS_XMIT_UC || qos->m_purpose == DDSI_TRAN_QOS_XMIT_MC))
    return DDS_RETCODE_BAD_PARAMETER;
  if (!ddsi_is_valid_port (factory, port))
    return DDS_RETCODE_BAD_PARAMETER;
  return factory->m_create_conn_fn (conn, factory, port, qos);
}

/** @component transport */
inline dds_return_t ddsi_factory_create_listener (struct ddsi_tran_listener **listener, struct ddsi_tran_factory * factory, uint32_t port, const struct ddsi_tran_qos *qos) {
  *listener = NULL;
  if (!ddsi_is_valid_port (factory, port))
    return DDS_RETCODE_BAD_PARAMETER;
  return factory->m_create_listener_fn (listener, factory, port, qos);
}

/** @component transport */
void ddsi_tran_free (struct ddsi_tran_base * base);

/** @component transport */
inline ddsrt_socket_t ddsi_tran_handle (struct ddsi_tran_base * base) {
  return base->m_handle_fn (base);
}

/** @component transport */
inline ddsrt_socket_t ddsi_conn_handle (struct ddsi_tran_conn * conn) {
  return conn->m_base.m_handle_fn (&conn->m_base);
}

/** @component transport */
inline uint32_t ddsi_conn_type (const struct ddsi_tran_conn *conn) {
  return conn->m_base.m_trantype;
}

/** @component transport */
inline uint32_t ddsi_conn_port (const struct ddsi_tran_conn *conn) {
  return conn->m_base.m_port;
}

/** @component transport */
inline int ddsi_conn_locator (struct ddsi_tran_conn * conn, ddsi_locator_t * loc) {
  return conn->m_locator_fn (conn->m_factory, &conn->m_base, loc);
}

/** @component transport */
inline ssize_t ddsi_conn_write (struct ddsi_tran_conn * conn, const ddsi_locator_t *dst, size_t niov, const ddsrt_iovec_t *iov, uint32_t flags) {
  return conn->m_closed ? -1 : (conn->m_write_fn) (conn, dst, niov, iov, flags);
}

/** @component transport */
inline ssize_t ddsi_conn_read (struct ddsi_tran_conn * conn, unsigned char * buf, size_t len, bool allow_spurious, ddsi_locator_t *srcloc) {
  return conn->m_closed ? -1 : conn->m_read_fn (conn, buf, len, allow_spurious, srcloc);
}

/** @component transport */
bool ddsi_conn_peer_locator (struct ddsi_tran_conn * conn, ddsi_locator_t * loc);

/** @component transport */
void ddsi_conn_disable_multiplexing (struct ddsi_tran_conn * conn);

/** @component transport */
void ddsi_conn_add_ref (struct ddsi_tran_conn * conn);

/** @component transport */
void ddsi_conn_free (struct ddsi_tran_conn * conn);

/** @component transport */
int ddsi_conn_join_mc (struct ddsi_tran_conn * conn, const ddsi_locator_t *srcip, const ddsi_locator_t *mcip, const struct ddsi_network_interface *interf);

/** @component transport */
int ddsi_conn_leave_mc (struct ddsi_tran_conn * conn, const ddsi_locator_t *srcip, const ddsi_locator_t *mcip, const struct ddsi_network_interface *interf);

/** @component transport */
int ddsi_is_loopbackaddr (const struct ddsi_domaingv *gv, const ddsi_locator_t *loc);

/** @component transport */
int ddsi_is_mcaddr (const struct ddsi_domaingv *gv, const ddsi_locator_t *loc);

/** @component transport */
int ddsi_is_ssm_mcaddr (const struct ddsi_domaingv *gv, const ddsi_locator_t *loc);

/** @component transport */
enum ddsi_nearby_address_result ddsi_is_nearby_address (const struct ddsi_domaingv *gv, const ddsi_locator_t *loc, size_t ninterf, const struct ddsi_network_interface *interf, size_t *interf_idx);


/** @component transport */
enum ddsi_locator_from_string_result ddsi_locator_from_string (const struct ddsi_domaingv *gv, ddsi_locator_t *loc, const char *str, struct ddsi_tran_factory * default_factory);


/** @component transport */
int ddsi_locator_from_sockaddr (const struct ddsi_tran_factory *tran, ddsi_locator_t *loc, const struct sockaddr *sockaddr);


/** @component transport */
int ddsi_enumerate_interfaces (struct ddsi_tran_factory * factory, enum ddsi_transport_selector transport_selector, ddsrt_ifaddrs_t **interfs);

/** @component transport */
inline int ddsi_listener_locator (struct ddsi_tran_listener * listener, ddsi_locator_t *loc) {
  return listener->m_locator_fn (listener->m_factory, &listener->m_base, loc);
}

/** @component transport */
inline int ddsi_listener_listen (struct ddsi_tran_listener * listener) {
  return listener->m_listen_fn (listener);
}

/** @component transport */
inline struct ddsi_tran_conn * ddsi_listener_accept (struct ddsi_tran_listener * listener) {
  return listener->m_accept_fn (listener);
}

/** @component transport */
void ddsi_listener_unblock (struct ddsi_tran_listener * listener);

/** @component transport */
void ddsi_listener_free (struct ddsi_tran_listener * listener);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__TRAN_H */
