// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_OMG_SECURITY_H
#define DDSI_OMG_SECURITY_H

#include "dds/features.h"

#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/avl.h"

#include "dds/ddsi/ddsi_participant.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_xqos.h"

#if defined (__cplusplus)
extern "C" {
#endif

#ifdef DDS_HAS_SECURITY

/**
 * @brief Check if security is enabled for the participant.
 * @component security_entity
 *
 * @param[in] pp  Participant to check if it is secure.
 *
 * @returns bool  True if participant is secure
 */
bool ddsi_omg_participant_is_secure(const struct ddsi_participant *pp);

/**
 * @brief Check if security allows to create the topic.
 * @component security_entity
 *
 * This function checks with access control if is allowed to create
 * this topic for the specified domain.
 *
 * @param[in] gv          The domain global information.
 * @param[in] pp_guid     The participant guid.
 * @param[in] topic_name  The name of the  topic.
 * @param[in] qos         The topic QoS used.
 *
 * @returns bool
 * @retval true   Creation of the topic is allowed
 * @retval false  Otherwise.
 */
bool ddsi_omg_security_check_create_topic (const struct ddsi_domaingv *gv, const ddsi_guid_t *pp_guid, const char *topic_name, const struct dds_qos *qos);

/**
 * @brief Check if security allows to create the reader.
 * @component security_entity
 *
 * This function checks with access control if is allowed to create
 * this reader for the specified domain.
 *
 * @param[in] pp          Participant on which the topic is being created.
 * @param[in] domain_id   The corresponding domain_id.
 * @param[in] topic_name  The name of the topic.
 * @param[in] reader_qos  The reader QoS used.
 *
 * @returns bool
 * @retval true   Creation of the writer is allowed
 * @retval false  Otherwise.
 */
bool ddsi_omg_security_check_create_reader (struct ddsi_participant *pp, uint32_t domain_id, const char *topic_name, const struct dds_qos *reader_qos);

/**
 * @brief Check if security allows to create the writer.
 * @component security_entity
 *
 * This function checks with access control if is allowed to create
 * this writer for the specified domain.
 *
 * @param[in] pp          Participant on which the topic is being created.
 * @param[in] domain_id   The corresponding domain_id.
 * @param[in] topic_name  The name of the topic.
 * @param[in] writer_qos  The writer QoS used.
 *
 * @returns bool
 * @retval true   Creation of the writer is allowed
 * @retval false  Otherwise.
 */
bool ddsi_omg_security_check_create_writer (struct ddsi_participant *pp, uint32_t domain_id, const char *topic_name, const struct dds_qos *writer_qos);

#else /* DDS_HAS_SECURITY */

#include "dds/ddsi/ddsi_unused.h"

/** @component security_entity */
inline bool ddsi_omg_participant_is_secure(UNUSED_ARG(const struct ddsi_participant *pp))
{
  return false;
}

/** @component security_entity */
inline bool ddsi_omg_security_check_create_topic (UNUSED_ARG(const struct ddsi_domaingv *gv), UNUSED_ARG(const ddsi_guid_t *pp_guid), UNUSED_ARG(const char *topic_name), UNUSED_ARG(const struct dds_qos *qos))
{
  return true;
}

/** @component security_entity */
inline bool ddsi_omg_security_check_create_reader (UNUSED_ARG(struct ddsi_participant *pp), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(const char *topic_name), UNUSED_ARG(const struct dds_qos *reader_qos))
{
  return true;
}

/** @component security_entity */
inline bool ddsi_omg_security_check_create_writer (UNUSED_ARG(struct ddsi_participant *pp), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(const char *topic_name), UNUSED_ARG(const struct dds_qos *writer_qos))
{
  return true;
}

#endif /* DDS_HAS_SECURITY */

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_OMG_SECURITY_H */
