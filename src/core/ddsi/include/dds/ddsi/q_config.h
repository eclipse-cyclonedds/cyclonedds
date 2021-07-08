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
#ifndef NN_CONFIG_H
#define NN_CONFIG_H

#include <stdio.h>
#include <stdbool.h>

#include "dds/ddsrt/attributes.h"
#include "dds/ddsi/ddsi_config.h"
#include "dds/ddsi/ddsi_domaingv.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct cfgst;
struct ddsrt_log_cfg;

bool config_reload (struct ddsi_domaingv *gv, struct cfgst *cfgst);
struct cfgst *config_init (const char *config, struct ddsi_config *cfg, uint32_t domid) ddsrt_nonnull((1,2));
void config_print_cfgst (struct cfgst *cfgst, const struct ddsrt_log_cfg *logcfg);
void config_print_rawconfig (const struct ddsi_config *cfg, const struct ddsrt_log_cfg *logcfg);
void config_free_source_info (struct cfgst *cfgst);
void config_fini (struct cfgst *cfgst);

#if defined (__cplusplus)
}
#endif

#endif /* NN_CONFIG_H */
