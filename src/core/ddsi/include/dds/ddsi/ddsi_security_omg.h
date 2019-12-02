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
#include "dds/ddsi/q_xqos.h"

#if defined (__cplusplus)
extern "C" {
#endif

#ifdef DDSI_INCLUDE_SECURITY

struct ddsi_hsdmin;

struct participant_sec_attributes;
struct proxy_participant_sec_attributes;
struct writer_sec_attributes;
struct reader_sec_attributes;

/**
 * @brief Return a reference to the handshake administration.
 *
 * @param[in] pp The participant.
 *
 * @returns pointer to the handshake administation.
 */
struct ddsi_hsadmin * q_omg_security_get_handhake_admin(const struct participant *pp);

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
 * @brief Check if security is enabled for the proxy participant.
 *
 * @param[in] proxypp  Proxy participant to check if it is secure.
 *
 * @returns bool
 * @retval true   Proxy participant is secure
 * @retval false  Proxy participant is not secure
 */
bool q_omg_proxy_participant_is_secure(const struct proxy_participant *proxypp);

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
 * @brief Check security if it is allowed to create the participant.
 *
 * When security is enabled for this participant it is checked if the
 * participant is authenticated by checking the provided security
 * certificates. When that is ok the participant is registered with
 * access control and with cryptography. When that is all successful
 * this function return true;
 *
 * @param[in] pp         The participant to check if alloweed by security.
 * #param[in] domain_id  The domain_id
 *
 * @returns bool
 * @retval true   Participant is allowed
 * @retval false  Participant is not allowed
 */
bool
q_omg_security_check_create_participant(
    struct participant *pp,
    uint32_t domain_id);

/**
 * @brief Remove the participant from the security plugins.
 *
 * When the participant was registered with the security
 * plugins then this function will release the allocated
 * security resources.
 *
 * @param[in] pp  Participant to remove.
 */
void
q_omg_security_deregister_participant(
    struct participant *pp);

/**
 * @brief Get the identity handle associate with this participant.
 *
 * This function returns the identity handle that was created
 * when the participant was authenticated. This handle corresponds
 * with the handle returned by calling validate_local_identity on
 * the authentication plugin.
 *
 * @param[in] pp  Participant to check if it is secure.
 *
 * @returns int64_t
 * @retval !0 Identity handle associated with the participant.
 * @retval 0  Invalid handle the participant was not registered
 */
int64_t
q_omg_security_get_local_participant_handle(
    struct participant *pp);

/**
 * @brief Get security info flags of the given participant.
 *
 * @param[in]  pp    Participant to get the security info from.
 * @param[out] info  The security info.
 *
 * @returns bool
 * @retval true   Security info set.
 * @retval false  Security info not set.
 */
bool q_omg_get_participant_security_info(struct participant *pp, nn_security_info_t *info);

/**
 * @brief Check if security allows to create the topic.
 *
 * This function checks with access control if is allowed to create
 * this topic for the specified domain.
 *
 * @param[in] pp          Participant on which the topic is being created.
 * @param[in] domain_id   The corresponding domain_id.
 * @param[in] topic_name  The name of the  topic.
 * @param[in] qos         The topic QoS used.
 *
 * @returns bool
 * @retval true   Creation of the topic is allowed
 * @retval false  Otherwise.
 */
bool
q_omg_security_check_create_topic(
    struct participant *pp,
    uint32_t domain_id,
    const char *topic_name,
    const struct dds_qos *qos);

/**
 * @brief Check if security allows to create the writer.
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
bool
q_omg_security_check_create_writer(
    struct participant *pp,
    uint32_t domain_id,
    const char *topic_name,
    const struct dds_qos *writer_qos);

/**
 * @brief Register the writer with security.
 *
 * This function registers the writer with security
 * when the associated participant has security enabled.
 * The security settings associated with this writer are determined
 * and the writer is registered with cryptography when needed by
 * the security settings which indicate if payload protection and or
 * submessage protection is enabled for this writer.
 *
 * @param[in] wr  The writer to register.
 */
void
q_omg_security_register_writer(
    struct writer *wr);

/**
 * @brief Remove the writer from security.
 *
 * When the writer was registered with security then this function
 * will remove the writer from security which will free the allocated
 * security resource created for this writer.
 *
 * @param[in] wr  The writer to remove.
 */
void
q_omg_security_deregister_writer(
    struct writer *wr);

/**
 * @brief Check if security allows to create the reader.
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
bool
q_omg_security_check_create_reader(
    struct participant *pp,
    uint32_t domain_id,
    const char *topic_name,
    const struct dds_qos *reader_qos);

/**
 * @brief Register the reader with security.
 *
 * This function registers the reader with security
 * when the associated participant has security enabled.
 * The security settings associated with this reader are determined
 * and the reader is registered with cryptography when needed by
 * the security settings which indicate if submessage protection is
 *  enabled for this reader.
 *
 * @param[in] rd  The reader to register.
 */
void
q_omg_security_register_reader(
    struct reader *rd);

/**
 * @brief Remove the reader from security.
 *
 * When the reader was registered with security then this function
 * will remove the reader from security which will free the allocated
 * security resource created for this reader.
 *
 * @param[in] rd  The reader to remove.
 */
void
q_omg_security_deregister_reader(
    struct reader *rd);

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
 * @brief Register participant with security plugin and check if the
 *        participant is allowed by security.
 *
 * This function will register the participant with the authentication
 * plugin which will check if the provided security QoS parameters are
 * correct, e.g. is the provided certificate valid, etc.
 * When that is successful it is checked with access control if the
 * participant has the correct permissions and is allowed to be created.
 *
 * @param[in] pp        The participant.
 * @param[in] domain_id The domain id.
 *
 * @returns bool
 * @retval true   The security check on the participant succeeded.
 * @retval false  The security check on the participant failed.
 */
bool q_omg_security_check_create_participant(struct participant *pp, uint32_t domain_id);

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
 * @brief Get the cypto handle associated with the proxy participant.
 *
 * This function returns the handle which is the association between
 * the proxy participant and the crypto plugin. This handle is created
 * when the proxy participant is registered with the crypto plugin.
 *
 * @param[in] proxypp            The proxy participant.
 *
 * @returns handle
 * @retval !0  Valid cypto handle associated with the proxy participant.
 * @retval 0   Otherwise.
 */
int64_t q_omg_security_get_remote_participant_handle(struct proxy_participant *proxypp);

/**
 * @brief Set the crypto tokens used for the encryption and decryption of RTPS messages.
 *
 * The remote participant  will send the crypto tokens when the security settings determine that the
 * communication between the participants must be secure. These tokens are used for the necryption and
 * decryption of complete RTPS messages. When these tokens are received this function will register these tokens
 * with the crypto plugin. The crypto plugin will return a crypto handle that will be used to associate the
 * stored tokens with the remote participant.
 *
 * @param[in] pp        The local participant.
 * @param[in] proxypp   The remote participant.
 * @param[in] tokens    The crypto token received from the remote participant for the local participant.
 */
void q_omg_security_set_participant_crypto_tokens(struct participant *pp, struct proxy_participant *proxypp, const nn_dataholderseq_t *tokens);

/**
 * @brief Determine the security settings associated with the remote participant.
 *
 * From the security information contained in the parameter list from the remote participant
 * the corresponding security settings are determined and returned in the info parameter.
 *
 * @param[in] proxypp   The remoate participant.
 * @param[in] plist     The parameter list from the remote writer.
 * @param[out] info     The security settings associated with the remote writer.
 */
void q_omg_get_proxy_participant_security_info(struct proxy_participant *proxypp, const nn_plist_t *plist, nn_security_info_t *info);

/**
 * @brief Determine the security settings associated with the remote writer.
 *
 * From the security information contained in the parameter list from the remote writer
 * the corresponding security settings are determined and returned in the info parameter.
 *
 * @param[in] pwr       The remoate writer.
 * @param[in] plist     The parameter list from the remote writer.
 * @param[out] info     The security settings associated with the remote writer.
 */
void q_omg_get_proxy_writer_security_info(struct proxy_writer *pwr, const nn_plist_t *plist, nn_security_info_t *info);

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
 * @brief Release the security information associated with the match between a reader and
 * a remote writer.
 *
 * This function releases the security resources that were allocated for this reader and remote
 * writer match. For example it will release the security tokens that where associated with this
 * reader and the remote writer.
 *
 * @param[in] pwr   The remote writer.
 * @param[in] rd    The local reader.
 * @param[in] match The match information between the reader and the remote writer.
 */
void q_omg_security_deregister_remote_writer_match(struct proxy_writer *pwr, struct reader *rd, struct rd_pwr_match *match);

/**
 * @brief Set the crypto tokens used for the secure communication from the remote writer to the reader.
 *
 * The remote writer instance will send the crypto tokens when the security settings determine that the
 * communication between the remote writer and the reader must be secure. When these tokens are received
 * this function will register these tokens with the crypto plugin and set the corresponding crypto handle returned
 * by the crypto plugin which is then used for decrypting messages received from that remote writer to the reader.
 *
 * @param[in] rd        The local reader.
 * @param[in] pwr_guid  The guid of the remote writer.
 * @param[in] tokens    The crypto token received from the remote writer for the reader.
 */
void q_omg_security_set_remote_writer_crypto_tokens(struct reader *rd, const ddsi_guid_t *pwr_guid, const nn_dataholderseq_t *tokens);

/**
 * @brief Determine the security settings associated with the remote reader.
 *
 * From the security information contained in the parameter list from the remote reader
 * the corresponding security settings are determined and returned in the info parameter.
 *
 * @param[in] prd       The remoate reader.
 * @param[in] plist     The parameter list from the remote reader.
 * @param[out] info     The security settings associated with the remote reader.
 */
void q_omg_get_proxy_reader_security_info(struct proxy_reader *prd, const nn_plist_t *plist, nn_security_info_t *info);

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

/**
 * @brief Release the security information associated with the match between a writer and
 * a remote reader.
 *
 * This function releases the security resources that were allocated for this writer and remote
 * reader match. For example it will release the security tokens that where associated with this
 * writer and the remote reader.
 *
 * @param[in] prd  The remote reader..
 * @param[in] wr   The local writer.
 * @param[in] match The match information between the writer and the remote reader.
 */
void q_omg_security_deregister_remote_reader_match(struct proxy_reader *prd, struct writer *wr, struct wr_prd_match *match);

/**
 * @brief Set the crypto tokens used for the secure communication from the remote reader to the writer.
 *
 * The remote reader instance will send the crypto tokens when the security settings determine that the
 * communication between the remote reader and the writer must be secure. When these tokens are received
 * this function will register these tokens with the crypto plugin and set the corresponding crypto handle returned
 * by the crypto plugin which is then used for decrypting messages received from that remote reader to the writer.
 *
 * @param[in] wr        The local writer.
 * @param[in] prd_guid  The guid of the remote reader.
 * @param[in] tokens    The crypto token received from the remote reader for the writer.
 */
void q_omg_security_set_remote_reader_crypto_tokens(struct writer *wr, const ddsi_guid_t *prd_guid, const nn_dataholderseq_t *tokens);



#else /* DDSI_INCLUDE_SECURITY */

#include "dds/ddsi/q_unused.h"

inline bool
q_omg_participant_is_secure(
  UNUSED_ARG(const struct participant *pp))
{
  return false;
}

inline bool
q_omg_proxy_participant_is_secure(
  UNUSED_ARG(const struct proxy_participant *proxypp))
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

inline bool
q_omg_security_check_create_participant(
    UNUSED_ARG(struct participant *pp),
    UNUSED_ARG(uint32_t domain_id))
{
  return true;
}

inline void
q_omg_security_deregister_participant(
    UNUSED_ARG(struct participant *pp))
{
}

inline bool
q_omg_security_check_create_topic(
    UNUSED_ARG(struct participant *pp),
    UNUSED_ARG(uint32_t domain_id),
    UNUSED_ARG(const char *topic_name),
    UNUSED_ARG(const struct dds_qos *qos))
{
  return true;
}

inline int64_t
q_omg_security_get_local_participant_handle(
    UNUSED_ARG(struct participant *pp))
{
  return 0;
}

inline bool
q_omg_security_check_create_writer(
    UNUSED_ARG(struct participant *pp),
    UNUSED_ARG(uint32_t domain_id),
    UNUSED_ARG(const char *topic_name),
    UNUSED_ARG(const struct dds_qos *writer_qos))
{
  return true;
}

inline void
q_omg_security_register_writer(
    UNUSED_ARG(struct writer *wr))
{
}

inline void
q_omg_security_deregister_writer(
    UNUSED_ARG(struct writer *wr))
{
}

inline bool
q_omg_security_check_create_reader(
    UNUSED_ARG(struct participant *pp),
    UNUSED_ARG(uint32_t domain_id),
    UNUSED_ARG(const char *topic_name),
    UNUSED_ARG(const struct dds_qos *reader_qos))
{
  return true;
}

inline void
q_omg_security_register_reader(
    UNUSED_ARG(struct reader *rd))
{
}

inline void
q_omg_security_deregister_reader(
    UNUSED_ARG(struct reader *rd))
{
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

inline int64_t q_omg_security_get_remote_participant_handle(UNUSED_ARG(struct proxy_participant *proxypp))
{
  return 0;
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

inline void q_omg_get_proxy_writer_security_info(UNUSED_ARG(struct proxy_writer *pwr), UNUSED_ARG(const nn_plist_t *plist), UNUSED_ARG(nn_security_info_t *info))
{
}

inline bool
q_omg_security_check_remote_writer_permissions(UNUSED_ARG(const struct proxy_writer *pwr), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(struct participant *pp))
{
  return true;
}

inline void q_omg_security_deregister_remote_writer_match(UNUSED_ARG(struct proxy_writer *pwr), UNUSED_ARG(struct reader *rd), UNUSED_ARG(struct rd_pwr_match *match))
{
}

inline void q_omg_get_proxy_reader_security_info(UNUSED_ARG(struct proxy_reader *prd), UNUSED_ARG(const nn_plist_t *plist), UNUSED_ARG(nn_security_info_t *info))
{
}

inline bool
q_omg_security_check_remote_reader_permissions(UNUSED_ARG(const struct proxy_reader *prd), UNUSED_ARG(uint32_t domain_id), UNUSED_ARG(struct participant *pp))
{
  return true;
}

inline void q_omg_security_deregister_remote_reader_match(UNUSED_ARG(struct proxy_reader *prd), UNUSED_ARG(struct writer *wr), UNUSED_ARG(struct wr_prd_match *match))
{
}

#endif /* DDSI_INCLUDE_SECURITY */

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_OMG_SECURITY_H */
