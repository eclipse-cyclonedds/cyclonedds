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
#ifndef DDSI_XMSG_H
#define DDSI_XMSG_H

#include <stddef.h>

#include "dds/features.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_domaingv;
struct ddsi_xpack;

/* XPACK */
struct ddsi_xpack * ddsi_xpack_new (struct ddsi_domaingv *gv, uint32_t bw_limit, bool async_mode);
void ddsi_xpack_free (struct ddsi_xpack *xp);
void ddsi_xpack_send (struct ddsi_xpack *xp, bool immediately /* unused */);

/* SENDQ */
void ddsi_xpack_sendq_init (struct ddsi_domaingv *gv);
void ddsi_xpack_sendq_start (struct ddsi_domaingv *gv);

#if defined (__cplusplus)
}
#endif
#endif /* DDSI_XMSG_H */
