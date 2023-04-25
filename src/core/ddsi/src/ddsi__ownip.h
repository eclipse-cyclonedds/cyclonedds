// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__OWNIP_H
#define DDSI__OWNIP_H

#include <stdbool.h>

#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsi/ddsi_locator.h"
#include "dds/ddsi/ddsi_ownip.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_domaingv;

/** @component network_if_selection */
int ddsi_find_own_ip (struct ddsi_domaingv *gv);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__OWNIP_H */
