// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__CONFIG_IMPL_H
#define DDSI__CONFIG_IMPL_H

#include <stdio.h>

#include "dds/ddsrt/attributes.h"
#include "dds/ddsi/ddsi_config.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_cfgst;
struct ddsrt_log_cfg;

/** @component config */
void ddsi_config_print_cfgst (struct ddsi_cfgst *cfgst, const struct ddsrt_log_cfg *logcfg);

/** @component config */
void ddsi_config_print_rawconfig (const struct ddsi_config *cfg, const struct ddsrt_log_cfg *logcfg);

/** @component config */
void ddsi_config_free_source_info (struct ddsi_cfgst *cfgst);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__CONFIG_IMPL_H */
