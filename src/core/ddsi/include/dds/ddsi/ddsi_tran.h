/*
 * Copyright(c) 2006 to 2021 ZettaScale Technology and others
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
#include "dds/ddsi/ddsi_locator.h"
#include "dds/ddsi/ddsi_config.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* Core types */
typedef struct ddsi_tran_base * ddsi_tran_base_t;
typedef struct ddsi_tran_conn * ddsi_tran_conn_t;
typedef struct ddsi_tran_listener * ddsi_tran_listener_t;
typedef struct ddsi_tran_factory * ddsi_tran_factory_t;
typedef struct ddsi_tran_qos ddsi_tran_qos_t;

/*  8 for transport/
    1 for [
   48 for IPv6 hex digits (3*16) + separators
    2 for ]:
   10 for port (DDSI loc has signed 32-bit)
   11 for @ifindex
    1 for terminator
   --
   81
*/
#define DDSI_LOCSTRLEN 81

char *ddsi_xlocator_to_string (char *dst, size_t sizeof_dst, const ddsi_xlocator_t *loc);
char *ddsi_locator_to_string (char *dst, size_t sizeof_dst, const ddsi_locator_t *loc);

char *ddsi_xlocator_to_string_no_port (char *dst, size_t sizeof_dst, const ddsi_xlocator_t *loc);
char *ddsi_locator_to_string_no_port (char *dst, size_t sizeof_dst, const ddsi_locator_t *loc);

#if defined (__cplusplus)
}
#endif

#endif
