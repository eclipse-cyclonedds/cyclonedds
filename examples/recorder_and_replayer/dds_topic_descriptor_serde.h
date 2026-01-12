/*
 * Copyright(c) 2025 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#ifndef DDS_TOPIC_DESCRIPTOR_SERDE_H
#define DDS_TOPIC_DESCRIPTOR_SERDE_H

#include <stddef.h>

#include "dds/ddsc/dds_public_impl.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief Calculate the serialized size of a topic descriptor
 * @param[in] desc Topic descriptor to measure
 * @return Size in bytes needed to serialize the descriptor
 */
size_t dds_topic_descriptor_serialized_size (const dds_topic_descriptor_t *desc);

/**
 * @brief Serialize a topic descriptor to a buffer
 * @param[in] desc Topic descriptor to serialize
 * @param[out] buffer Buffer to write serialized data
 * @param[in] buffer_size Size of the buffer
 * @return Number of bytes written, or 0 on error
 */
size_t dds_topic_descriptor_serialize (const dds_topic_descriptor_t *desc, void *buffer, size_t buffer_size);

/**
 * @brief Deserialize a topic descriptor from a buffer
 * @param[in] buffer Buffer containing serialized topic descriptor
 * @param[in] buffer_size Size of the buffer
 * @return Newly allocated topic descriptor, or NULL on error
 */
dds_topic_descriptor_t *dds_topic_descriptor_deserialize (const void *buffer, size_t buffer_size);

/**
 * @brief Free a deserialized topic descriptor
 * @param[in] desc Topic descriptor to free (can be NULL)
 */
void dds_topic_descriptor_free (dds_topic_descriptor_t *desc);

#if defined(__cplusplus)
}
#endif

#endif /* DDS_TOPIC_DESCRIPTOR_SERDE_H */
