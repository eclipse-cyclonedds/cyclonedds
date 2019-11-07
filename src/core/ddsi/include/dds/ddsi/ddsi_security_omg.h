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
#ifndef DDSI_OMG_SECURITY_H
#define DDSI_OMG_SECURITY_H

#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_plist.h"
#include "dds/ddsi/q_globals.h"

#if defined (__cplusplus)
extern "C" {
#endif

#ifdef DDSI_INCLUDE_SECURITY

/**
 * @brief Check if security is enabled for the participant.
 *
 * @param[in] pp  Participant to check if it is secure.
 *
 * @returns bool
 * @retval true   Participant is secure
 * @retval false  Participant is not secure
 */
bool q_omg_participant_is_secure(const struct participant *pp);

/**
 * @brief Get security info flags of the given writer.
 *
 * @param[in]  wr    Writer to get the security info from.
 * @param[out] info  The security info.
 *
 * @returns bool
 * @retval true   Security info set.
 * @retval false  Security info not set (probably unsecure writer).
 */
bool q_omg_get_writer_security_info(const struct writer *wr, nn_security_info_t *info);

/**
 * @brief Get security info flags of the given reader.
 *
 * @param[in]  rd    Reader to get the security info from.
 * @param[out] info  The security info.
 *
 * @returns bool
 * @retval true   Security info set.
 * @retval false  Security info not set (probably unsecure reader).
 */
bool q_omg_get_reader_security_info(const struct reader *rd, nn_security_info_t *info);

/**
 * @brief Return the builtin writer id for this readers' discovery.
 *
 * Return builtin entity id of the writer to use for the subscription
 * discovery information.
 * Depending on whether the discovery is protected or not (for the
 * given reader), either the default writer or protected writer needs
 * to be used.
 *
 * @param[in] rd Reader to determine the subscription writer from.
 *
 * @returns unsigned
 * @retval NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER
 * @retval NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER
 */
unsigned determine_subscription_writer(const struct reader *rd);

/**
 * @brief Return the builtin writer id for this writers' discovery.
 *
 * Return builtin entity id of the writer to use for the publication
 * discovery information.
 * Depending on whether the discovery is protected or not (for the
 * given writer), either the default writer or protected writer needs
 * to be used.
 *
 * @param[in] wr Writer to determine the publication writer from.
 *
 * @returns unsigned
 * @retval NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER
 * @retval NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER
 */
unsigned determine_publication_writer(const struct writer *wr);

/**
 * @brief Determine if the proxy participant is allowed to be deleted
 *        by the given writer.
 *
 * If an proxy participant is authenticated, it is only allowed to
 * to deleted when a dispose is received from the proper protected
 * discovery writer.
 *
 * @param[in] gv           Used for tracing.
 * @param[in] guid         Guid of the proxy participant to be deleted.
 * @param[in] pwr_entityid Writer that send the dispose.
 *
 * @returns bool
 * @retval true   The proxy participant may be deleted.
 * @retval false  The proxy participant may not be deleted by this writer.
 */
bool allow_proxy_participant_deletion(struct q_globals * const gv, const struct ddsi_guid *guid, const ddsi_entityid_t pwr_entityid);

/**
 * @brief Check if the participant and the proxy participant
 *        have compatible security info settings.
 *
 * Associated with a secure participant is the ParticipantSecurityInfo parameter.
 * This parameter contains the setting of the security attributes and the associated
 * plugin security attributes of the secure participant.
 * This function will check if the received ParticipantSecurityInfo parameter is
 * compatible with the local ParticipantSecurityInfo parameter.
 *
 * @param[in] pp      The participant.
 * @param[in] proxypp The proxy participant.
 *
 * @returns bool
 * @retval true   The participant and the proxy participant have compatible
 *                security info settings.
 * @retval false  Otherwise.
 */
bool q_omg_is_similar_participant_security_info(struct participant *pp, struct proxy_participant *proxypp);

/**
 * @brief Check if the participant allows communication with unauthenticated
 *        participants
 *
 * @param[in] pp  The participant.
 *
 * @returns bool
 * @retval true   The participant allows unauthenticated communication
 * @retval false  Otherwise.
 */
bool q_omg_participant_allow_unauthenticated(struct participant *pp);

/**
 * @brief Initialize the proxy participant security attributes
 *
 * @param[in] proxypp  The proxy participant.
 *
 */
void q_omg_security_init_remote_participant(struct proxy_participant *proxypp);

/**
 * @brief Check the if the proxy participant is allowed by checking the security permissions.
 *
 * The access control plugin is ask to verify if the proxy participant is allowed to
 * communicate with the local participant. When the proxy participant is allowed the
 * function will return a valid permission handle which is provided by the access control plugin.
 *
 * @param[in] domain_id The domain id
 * @param[in] pp        The participant
 * @param[in] proxypp   The proxy participant
 *
 * @returns permission handle
 * @retval !0    The proxy participant is allowed
 * @retval 0     The proxy participant is not allowed.
 */
int64_t q_omg_security_check_remote_participant_permissions(uint32_t domain_id, struct participant *pp, struct proxy_participant *proxypp);

/**
 * @brief Registers the matched proxy participant with the crypto plugin
 *
 * When the proxy participant is authenticated and allowed by access control then the match between the local and
 * the remote participant must be registered with the cypto factory provided by the crypto plugin. The
 * shared secret handle obtained from the authentication phase and the permission handle returned when validating
 * the proxy participant with access control plugin have to be provided.
 *
 *
 * @param[in] pp                 The participant.
 * @param[in] proxypp            The proxy participant.
 * @param[in] shared_secret      The shared_secret handle.
 * @param[in] proxy_permissions  The permission handle associated with the proxy participant.
 */
void q_omg_security_register_remote_participant(struct participant *pp, struct proxy_participant *proxypp, int64_t shared_secret, int64_t proxy_permissions);

/**
 * @brief Removes a registered proxy participant from administation of the authentication,
 *        access control and crypto plugins.
 *
 * @param[in] proxypp            The proxy participant.
 */
void q_omg_security_deregister_remote_participant(struct proxy_participant *proxypp);

/**
 * @brief Generate and send the crypto tokens needed for encoding RTPS messages.
 *
 * When the security settings indicate that RTPS message encoding or signing is
 * configured for the participant then this function will ask the cypto echange for
 * the corresponding cypto tokens and send these to the proxy participant.
 *
 * @param[in] pp                 The participant.
 * @param[in] proxypp            The proxy participant.
 */
void q_omg_security_participant_send_tokens(struct participant *pp, struct proxy_participant *proxypp);

/**
 * @brief Check if the remote writer is allowed to communicate with endpoints of the
 *        local participant.
 *
 * This function will check with the access control plugin if the remote writer
 * is allowed to communicate with this participant.
 *
 * @param[in] pwr       The remote writer.
 * @param[in] domain_id The domain id.
 * @param[in] pp        The local participant.
 *
 * @returns bool
 * @retval true   The remote writer is allowed to communicate.
 * @retval false  Otherwise.
 */
bool q_omg_security_check_remote_writer_permissions(const struct proxy_writer *pwr, uint32_t domain_id, struct participant *pp);

/**
 * @brief Check if the remote reader is allowed to communicate with endpoints of the
 *        local participant.
 *
 * This function will check with the access control plugin if the remote reader
 * is allowed to communicate with this participant.
 *
 * @param[in] prd       The remote reader.
 * @param[in] domain_id The domain id.
 * @param[in] pp        The local participant.
 *
 * @returns bool
 * @retval true   The remote reader is allowed to communicate.
 * @retval false  Otherwise.
 */
bool q_omg_security_check_remote_reader_permissions(const struct proxy_reader *prd, uint32_t domain_id, struct participant *pp);

/**
 * @brief Check it the remote writer is allowed to communicate with the local reader.
 *
 * When a remote writer is allowed by access control it has to be checked if the remote
 * writer is allowed to communicate with a particular local reader. This function will
 * check if the provided security end-point attributes are compatible, When the security
 * attributes are compatible then the function will register the reader and remote writer
 * match with the crypto factory and will also ask the crypto exchange to generate the
 * crypto tokens associate with the local reader which will be sent to the remote entity.
 * Note that the reader crypto tokens are used to encrypt the reader specific submessages
 * when submessage encoding or signing is configured.
 *
 * @param[in] rd   The local reader.
 * @param[in] pwr  The remote writer.
 *
 * @returns bool
 * @retval true   The local reader and remote writer are allowed to communicate.
 * @retval false  Otherwise.
 */
bool q_omg_security_match_remote_writer_enabled(struct reader *rd, struct proxy_writer *pwr);

/**
 * @brief Check it the local writer is allowed to communicate with the remote reader.
 *
 * When a remote reader is allowed by access control it has to be checked if the local
 * writer is allowed to communicate with a particular local writer. This function will
 * check if the provided security end-point attributes are compatible, When the security
 * attributes are compatible then the function will register the writer and remote reader
 * match with the crypto factory and will also ask the crypto exchange to generate the
 * crypto tokens associate with the local writer which will be sent to the remote entity.
 * Note that the writer crypto tokens are used to encrypt the writer specific submessages
 * when submessage encoding or signing is configured and also the crypto tokens used
 * for encoding the payload of data or datafrag messages.
 *
 * @param[in] wr   The local writer.
 * @param[in] prd  The remote reader.
 *
 * @returns bool
 * @retval true   The local writer and remote reader are allowed to communicate.
 * @retval false  Otherwise.
 */
bool q_omg_security_match_remote_reader_enabled(struct writer *wr, struct proxy_reader *prd);










#else /* DDSI_INCLUDE_SECURITY */

#include "dds/ddsi/q_unused.h"

inline bool
q_omg_participant_is_secure(
  UNUSED_ARG(const struct participant *pp))
{
  return false;
}

inline unsigned
determine_subscription_writer(
  UNUSED_ARG(const struct reader *rd))
{
  return NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER;
}

inline unsigned
determine_publication_writer(
  UNUSED_ARG(const struct writer *wr))
{
  return NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER;
}

inline bool
allow_proxy_participant_deletion(
  UNUSED_ARG(struct q_globals * const gv),
  UNUSED_ARG(const struct ddsi_guid *guid),
  UNUSED_ARG(const ddsi_entityid_t pwr_entityid))
{
  return true;
}

inline bool q_omg_is_similar_participant_security_info(UNUSED_ARG(struct participant *pp), UNUSED_ARG(struct proxy_participant *proxypp))
{
  return true;
}

inline bool q_omg_participant_allow_unauthenticated(UNUSED_ARG(struct participant *pp))
{
  return true;
}

inline void q_omg_security_init_remote_participant(UNUSED_ARG(struct proxy_participant *proxypp))
{
}

inline int64_t q_omg_security_check_remote_participant_permissions(UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(struct participant *pp), UNUSED_ARG(struct proxy_participant *proxypp))
{
  return 0LL;
}

inline void q_omg_security_register_remote_participant(UNUSED_ARG(struct participant *pp), UNUSED_ARG(struct proxy_participant *proxypp), UNUSED_ARG(int64_t shared_secret), UNUSED_ARG(int64_t proxy_permissions))
{
}

inline void q_omg_security_deregister_remote_participant(UNUSED_ARG(struct proxy_participant *proxypp))
{
}

inline void q_omg_security_participant_send_tokens(UNUSED_ARG(struct participant *pp), UNUSED_ARG(struct proxy_participant *proxypp))
{
}

inline bool
q_omg_security_match_remote_writer_enabled(UNUSED_ARG(struct reader *rd), UNUSED_ARG(struct proxy_writer *pwr))
{
  return true;
}

inline bool q_omg_security_match_remote_reader_enabled(UNUSED_ARG(struct writer *wr), UNUSED_ARG(struct proxy_reader *prd))
{
  return true;
}

inline bool
q_omg_security_check_remote_writer_permissions(UNUSED_ARG(const struct proxy_writer *pwr), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(struct participant *pp))
{
  return true;
}

inline bool
q_omg_security_check_remote_reader_permissions(UNUSED_ARG(const struct proxy_reader *prd), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(struct participant *pp))
{
  return true;
}

#endif /* DDSI_INCLUDE_SECURITY */

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_OMG_SECURITY_H */
