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

#include "dds/ddsrt/attributes.h"
#include "dds/ddsi/ddsi_cfgtype.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct cfgst;
struct ddsrt_log_cfg;

struct cfgst *config_init (const char *config, struct config *cfg, uint32_t domid) ddsrt_nonnull((1,2));
void config_print_cfgst (struct cfgst *cfgst, const struct ddsrt_log_cfg *logcfg);
void config_free_source_info (struct cfgst *cfgst);
void config_fini (struct cfgst *cfgst);

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
struct ddsi_config_partitionmapping_listelem *find_partitionmapping (const struct config *cfg, const char *partition, const char *topic);
int is_ignored_partition (const struct config *cfg, const char *partition, const char *topic);
#endif
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
struct config_channel_listelem *find_channel (const struct config *cfg, nn_transport_priority_qospolicy_t transport_priority);
#endif

#if defined (__cplusplus)
}
#endif

#endif /* NN_CONFIG_H */
