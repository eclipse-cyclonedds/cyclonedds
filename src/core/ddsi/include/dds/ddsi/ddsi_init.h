// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_INIT_H
#define DDSI_INIT_H

#include <stdbool.h>
#include "dds/export.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_cfgst;
struct ddsi_domaingv;

/** @component ddsi_init */
int ddsi_config_prep (struct ddsi_domaingv *gv, struct ddsi_cfgst *cfgst);

/** @component ddsi_init */
int ddsi_init (struct ddsi_domaingv *gv);

/** @component ddsi_init */
int ddsi_start (struct ddsi_domaingv *gv);

/** @component ddsi_init */
void ddsi_stop (struct ddsi_domaingv *gv);

/** @component ddsi_init */
void ddsi_fini (struct ddsi_domaingv *gv);

/** @component ddsi_generic_entity */
void ddsi_set_deafmute (struct ddsi_domaingv *gv, bool deaf, bool mute, int64_t reset_after);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_INIT_H */
