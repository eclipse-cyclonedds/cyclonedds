// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__FAKENET_H
#define DDSI__FAKENET_H

#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsi/ddsi_config.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_tran_factory;

/** @component fake_network */
int ddsi_fakenet_load_xml_string (const char *xml);

/** @component fake_network */
int ddsi_fakenet_load_xml_file (const char *path);

/** @component fake_network */
int ddsi_fakenet_load_default (void);

/** @component fake_network */
int ddsi_fakenet_load_real_interfaces (void);

/** @component fake_network */
int ddsi_fakenet_ensure_xml_file (const char *path);

/** @component fake_network */
int ddsi_fakenet_ensure_default (void);

/** @component fake_network */
int ddsi_fakenet_ensure_real_interfaces (void);

/** @component fake_network */
void ddsi_fakenet_clear (void);

/** @component fake_network */
int ddsi_fakenet_set_host (const char *name);

/** @component fake_network */
int ddsi_fakenet_enumerate_interfaces (struct ddsi_tran_factory *fact, enum ddsi_transport_selector transport_selector, ddsrt_ifaddrs_t **ifs);

/** @component fake_network */
dds_return_t ddsi_fakenet_socket (ddsrt_socket_t *sockptr, int domain, int type, int protocol);

/** @component fake_network */
dds_return_t ddsi_fakenet_bind (ddsrt_socket_t sock, const struct sockaddr *addr, socklen_t addrlen);

/** @component fake_network */
dds_return_t ddsi_fakenet_getsockname (ddsrt_socket_t sock, struct sockaddr *addr, socklen_t *addrlen);

/** @component fake_network */
dds_return_t ddsi_fakenet_sendmsg (ddsrt_socket_t sock, const ddsrt_msghdr_t *msg, int flags, size_t *sent);

/** @component fake_network */
dds_return_t ddsi_fakenet_recvmsg (const ddsrt_socket_ext_t *sockext, ddsrt_msghdr_t *msg, int flags, size_t *rcvd);

/** @component fake_network */
dds_return_t ddsi_fakenet_getsockopt (ddsrt_socket_t sock, int32_t level, int32_t optname, void *optval, socklen_t *optlen);

/** @component fake_network */
dds_return_t ddsi_fakenet_setsockopt (ddsrt_socket_t sock, int32_t level, int32_t optname, const void *optval, socklen_t optlen);

/** @component fake_network */
dds_return_t ddsi_fakenet_setsockreuse (ddsrt_socket_t sock, bool reuse);

/** @component fake_network */
dds_return_t ddsi_fakenet_close (ddsrt_socket_t sock);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__FAKENET_H */
