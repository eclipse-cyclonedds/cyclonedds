// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__ETH_H
#define DDSI__ETH_H

#include "dds/ddsi/ddsi_tran.h"

#if defined (__cplusplus)
extern "C" {
#endif

/** @component ethernet */
int ddsi_eth_enumerate_interfaces(struct ddsi_tran_factory * fact, enum ddsi_transport_selector transport_selector, ddsrt_ifaddrs_t **ifs);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__ETH_H */
