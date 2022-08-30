/*
 * Copyright(c) 2022 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDS_DDSI_NWPART_H
#define DDS_DDSI_NWPART_H

#include <stdint.h>
#include "dds/features.h"
#include "dds/ddsrt/attributes.h"

#if defined (__cplusplus)
extern "C" {
#endif

void free_config_networkpartition_addresses (struct ddsi_domaingv *gv) ddsrt_nonnull_all;
int convert_network_partition_config (struct ddsi_domaingv *gv, uint32_t port_data_uc)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;
bool ddsi_is_ignored_partition (const struct ddsi_domaingv *gv, const struct dds_qos *xqos, const char *topic_name)
  ddsrt_nonnull_all;

#ifdef DDS_HAS_NETWORK_PARTITIONS

struct dds_xqos;
struct ddsi_config;
struct ddsi_domaingv;
struct ddsi_config_partitionmapping_listelem;
struct ddsrt_log_cfg;

const struct ddsi_config_partitionmapping_listelem *find_partitionmapping (const struct ddsi_config *cfg, const char *partition, const char *topic)
  ddsrt_nonnull_all;
const struct ddsi_config_networkpartition_listelem *ddsi_get_partition_from_mapping (const struct ddsrt_log_cfg *logcfg, const struct ddsi_config *config, const struct dds_qos *xqos, const char *topic_name)
  ddsrt_nonnull_all;

#endif // DDS_HAS_NETWORK_PARTITIONS

#if defined (__cplusplus)
}
#endif

#endif
