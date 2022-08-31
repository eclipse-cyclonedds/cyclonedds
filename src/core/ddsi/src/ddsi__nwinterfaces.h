// Copyright(c) 2006 to 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__NWINTERFACES_H
#define DDSI__NWINTERFACES_H

#include <stdbool.h>

#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsi/ddsi_locator.h"
#include "dds/ddsi/ddsi_nwinterfaces.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_domaingv;

/** @component network_if_selection */
int ddsi_gather_network_interfaces (struct ddsi_domaingv *gv);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__NWINTERFACES_H */
