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
#ifndef DDSI_IPADDR_H
#define DDSI_IPADDR_H

#include "dds/ddsi/ddsi_tran.h"

#if defined (__cplusplus)
extern "C" {
#endif

enum ddsi_nearby_address_result ddsi_ipaddr_is_nearby_address (const nn_locator_t *loc, const nn_locator_t *ownloc, size_t ninterf, const struct nn_interface *interf);
enum ddsi_locator_from_string_result ddsi_ipaddr_from_string (const struct ddsi_tran_factory *tran, nn_locator_t *loc, const char *str, int32_t kind);
int ddsi_ipaddr_compare (const struct sockaddr *const sa1, const struct sockaddr *const sa2);
char *ddsi_ipaddr_to_string (char *dst, size_t sizeof_dst, const nn_locator_t *loc, int with_port);
void ddsi_ipaddr_to_loc (const struct ddsi_tran_factory *tran, nn_locator_t *dst, const struct sockaddr *src, int32_t kind);
void ddsi_ipaddr_from_loc (struct sockaddr_storage *dst, const nn_locator_t *src);

#if defined (__cplusplus)
}
#endif

#endif
