/*
 * Copyright(c) 2020 to 2021 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSI_VNET_H
#define DDSI_VNET_H

#if defined (__cplusplus)
extern "C" {
#endif

int ddsi_vnet_init (struct ddsi_domaingv *gv, const char *name, int32_t locator_kind);

#if defined (__cplusplus)
}
#endif

#endif
