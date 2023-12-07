// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

// WARNING This file is only needed for the work around for https://github.com/eclipse-cyclonedds/cyclonedds/issues/74
// Do not include this file in an application! Once issue #74 is solved this header file should be removed.

#ifndef DDS_INTERNAL_API_H
#define DDS_INTERNAL_API_H

#include "dds/export.h"
#include "dds/dds.h"
#include "dds/cdr/dds_cdrstream.h"

#if defined (__cplusplus)
extern "C" {
#endif

// Magic numbers match the set of internal flags we want to use but do not want to expose in the API.
// The implementation contains static assert to ensure this is kept in sync.
#define DDS_PARTICIPANT_FLAGS_NO_DISCOVERY (1u | 2u | 32u)

/**
 * @ingroup internal
 * @component topic
 * @unstable
 * @brief Gets a CDR stream serializer type descriptor from a topic descriptor
 *
 * @param[out] desc Pointer to the target struct that can be filled with the CDR stream topic descriptor
 * @param[in] topic_desc The source topic descriptor
 *
*/
DDS_EXPORT void
dds_cdrstream_desc_from_topic_desc (struct dds_cdrstream_desc *desc, const dds_topic_descriptor_t *topic_desc);

/**
 * @ingroup internal
 * @component participant
 * @unstable
 * @brief Create a participant with the specified GUID
 *
 * @param[in]  domain The domain in which to create the participant (can be DDS_DOMAIN_DEFAULT). DDS_DOMAIN_DEFAULT is for using the domain in the configuration.
 * @param[in]  qos The QoS to set on the new participant (can be NULL).
 * @param[in]  listener Any listener functions associated with the new participant (can be NULL).
 * @param[in]  flags The flags to be used when creating the participant
 * @param[in]  guid The GUID for the new participant
 *
 * @returns A valid participant handle or an error code. @see dds_create_participant for details
 */
DDS_EXPORT dds_entity_t
dds_create_participant_guid (const dds_domainid_t domain, const dds_qos_t *qos, const dds_listener_t *listener, uint32_t flags, const dds_guid_t *guid);

/**
 * @ingroup internal
 * @component writer
 * @unstable
 * @brief Create a writer with the specified GUID
 *
 * @param[in]  participant_or_publisher The participant or publisher on which the writer is being created.
 * @param[in]  topic The topic to write.
 * @param[in]  qos The QoS to set on the new writer (can be NULL).
 * @param[in]  listener Any listener functions associated with the new writer (can be NULL).
 * @param[in]  guid The GUID for the new writer
 *
 * @returns A valid writer handle or an error code. @see dds_create_writer for details
 */
DDS_EXPORT dds_entity_t
dds_create_writer_guid (dds_entity_t participant_or_publisher, dds_entity_t topic, const dds_qos_t *qos, const dds_listener_t *listener, dds_guid_t *guid);

/**
 * @ingroup internal
 * @component reader
 * @unstable
 * @brief Create a reader with the specified GUID
 *
 * @param[in]  participant_or_subscriber The participant or subscriber on which the reader is being created.
 * @param[in]  topic The topic to read.
 * @param[in]  qos The QoS to set on the new reader (can be NULL).
 * @param[in]  listener Any listener functions associated with the new reader (can be NULL).
 * @param[in]  guid The GUID for the new reader
 *
 * @returns A valid reader handle or an error code. @see dds_create_reader for details
 */
DDS_EXPORT dds_entity_t
dds_create_reader_guid (dds_entity_t participant_or_subscriber, dds_entity_t topic, const dds_qos_t *qos, const dds_listener_t *listener, dds_guid_t *guid);

#if defined (__cplusplus)
}
#endif
#endif
