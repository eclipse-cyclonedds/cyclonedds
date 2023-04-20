// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__SHM_QOS_H
#define DDS__SHM_QOS_H

#if defined (__cplusplus)
extern "C" {
#endif

struct dds_qos;
struct dds_topic;

/**
 * @brief Check whether the DDS QoS settings and topic are compatible with Iceoryx
 * @component iceoryx_support
 *
 * @param[in] qos the QoS for which to check compatibility
 * @param[in] tp topic
 * @param[in] check_durability_service whether to include this one
 *
 * @return true iff compatible
 */
bool dds_shm_compatible_qos_and_topic (const struct dds_qos *qos, const struct dds_topic *tp, bool check_durability_service);

/**
 * @brief Construct a string representation partition & topic string for use in Iceoryx
 * @component iceoryx_support
 *
 * @param[in] qos the QoS from which to get the partition
 * @param[in] tp the topic
 *
 * @return an allocated, null-terminated string if partition is compatible and sufficent memory is available
 */
char *dds_shm_partition_topic (const struct dds_qos *qos, const struct dds_topic *tp);

#if defined (__cplusplus)
}
#endif

#endif /* DDS__SHM_MONITOR_H */
