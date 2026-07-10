// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

// ddsi_udp.c needs these before any socket-related system header is included.
#ifndef __APPLE_USE_RFC_3542
#define __APPLE_USE_RFC_3542
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "ddsi__fakeudp.h"
#include "ddsi__fakenet.h"

#define ddsi_udp_init ddsi_fakeudp_init
#define ddsi_eth_enumerate_interfaces ddsi_fakenet_enumerate_interfaces
#define ddsrt_socket ddsi_fakenet_socket
#define ddsrt_close ddsi_fakenet_close
#define ddsrt_bind ddsi_fakenet_bind
#define ddsrt_getsockname ddsi_fakenet_getsockname
#define ddsrt_sendmsg ddsi_fakenet_sendmsg
#define ddsrt_recvmsg ddsi_fakenet_recvmsg
#define ddsrt_getsockopt ddsi_fakenet_getsockopt
#define ddsrt_setsockopt ddsi_fakenet_setsockopt
#define ddsrt_setsockreuse ddsi_fakenet_setsockreuse

#include "ddsi_udp.c"
