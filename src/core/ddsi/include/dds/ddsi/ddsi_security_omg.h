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

#include "dds/ddsi/q_plist.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_globals.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/types.h"
#include "dds/ddsrt/sync.h"


#if defined (__cplusplus)
extern "C" {
#endif

typedef enum {
  NN_RTPS_MSG_STATE_ERROR,
  NN_RTPS_MSG_STATE_PLAIN,
  NN_RTPS_MSG_STATE_ENCODED
} nn_rtps_msg_state_t;

#ifdef DDSI_INCLUDE_SECURITY

#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_plugins.h"

typedef struct nn_msg_sec_info {
  int64_t src_pp_handle;
  int64_t dst_pp_handle;
  bool use_rtps_encoding;
} nn_msg_sec_info_t;




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
 * @brief Get the security handle of the given local participant.
 *
 * @param[in] pp  Participant to check if it is secure.
 *
 * @returns int64_t
 * @retval Local participant security handle
 */
int64_t q_omg_security_get_local_participant_handle(struct participant *pp);

/**
 * @brief Get the security handle of the given remote participant.
 *
 * @param[in] proxypp  Participant to check if it is secure.
 *
 * @returns int64_t
 * @retval Remote participant security handle
 */
int64_t q_omg_security_get_remote_participant_handle(struct proxy_participant *proxypp);

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
bool
is_proxy_participant_deletion_allowed(
  struct q_globals * const gv,
  const struct ddsi_guid *guid,
  const ddsi_entityid_t pwr_entityid);

/**
 * @brief Determine if the messages, related to the given remote
 * entity, are RTPS protected or not.
 *
 * @param[in] proxy_pp  Related proxy participant.
 * @param[in] entityid  ID of the entity to check.
 *
 * @returns bool
 * @retval true   The entity messages are RTPS protected.
 * @retval false  The entity messages are not RTPS protected.
 */
bool
q_omg_security_is_remote_rtps_protected(
  const struct proxy_participant *proxy_pp,
  ddsi_entityid_t entityid);

/**
 * @brief Determine if the messages, related to the given local
 * entity, are RTPS protected or not.
 *
 * @param[in] pp        Related participant.
 * @param[in] entityid  ID of the entity to check.
 *
 * @returns bool
 * @retval true   The entity messages are RTPS protected.
 * @retval false  The entity messages are not RTPS protected.
 */
bool
q_omg_security_is_local_rtps_protected(
  const struct participant *pp,
  ddsi_entityid_t entityid);

/**
 * @brief Set security information, depending on plist, into the given
 * proxy participant.
 *
 * @param[in] proxypp  Proxy participant to set security info on.
 * @param[in] plist    Paramater list, possibly contains security info.
 */
void
set_proxy_participant_security_info(
  struct proxy_participant *proxypp,
  const nn_plist_t *plist);

/**
 * @brief Set security information, depending on plist and proxy participant,
 * into the given proxy reader.
 *
 * @param[in] prd      Proxy reader to set security info on.
 * @param[in] plist    Paramater list, possibly contains security info.
 */
void
set_proxy_reader_security_info(
  struct proxy_reader *prd,
  const nn_plist_t *plist);

/**
 * @brief Set security information, depending on plist and proxy participant,
 * into the given proxy writer.
 *
 * @param[in] pwr      Proxy writer to set security info on.
 * @param[in] plist    Paramater list, possibly contains security info.
 */
void
set_proxy_writer_security_info(
  struct proxy_writer *pwr,
  const nn_plist_t *plist);

/**
 * @brief Encode RTPS message.
 *
 * @param[in]     src_handle  Security handle of data source.
 * @param[in]     src_guid    GUID of the entity data source.
 * @param[in]     src_buf     Original RTPS message.
 * @param[in]     src_len     Original RTPS message size.
 * @param[out]    dst_buf     Encoded RTPS message.
 * @param[out]    dst_len     Encoded RTPS message size.
 * @param[in]     dst_handle  Security handle of data destination.
 *
 * @returns bool
 * @retval true   Encoding succeeded.
 * @retval false  Encoding failed.
 */
bool
q_omg_security_encode_rtps_message(
  int64_t                 src_handle,
  ddsi_guid_t            *src_guid,
  const unsigned char    *src_buf,
  size_t                  src_len,
  unsigned char         **dst_buf,
  size_t                 *dst_len,
  int64_t                 dst_handle);

/**
 * @brief Encode payload when necessary.
 *
 * When encoding is necessary, *buf will be allocated and the vec contents
 * will change to point to that buffer.
 * It is expected that the vec contents is always aliased.
 *
 * If no encoding is necessary, nothing changes.
 *
 * encoding(    not needed) -> return( true), vec(untouched), buf(NULL)
 * encoding(needed&success) -> return( true), vec( buf(new))
 * encoding(needed&failure) -> return(false), vec(untouched), buf(NULL)
 *
 * @param[in]     wr   Writer that writes the payload.
 * @param[in,out] vec  An iovec that contains the payload.
 * @param[out]    buf  Buffer to contain the encoded payload.
 *
 * @returns bool
 * @retval true   Encoding succeeded or not necessary. Either way, vec
 *                contains the payload that should be send.
 * @retval false  Encoding was necessary, but failed.
 */
bool
encode_payload(
  struct writer *wr,
  ddsrt_iovec_t *vec,
  unsigned char **buf);

/**
 * @brief Decode the payload of a Data submessage.
 *
 * When decoding is necessary, the payloadp memory will be replaced
 * by the decoded payload. This means that the original submessage
 * now contains payload that can be deserialized.
 *
 * If no decoding is necessary, nothing changes.
 *
 * @param[in]     gv          Global information.
 * @param[in]     sampleinfo  Sample information.
 * @param[in,out] payloadp    Pointer to payload memory.
 * @param[in]     payloadsz   Size of payload.
 * @param[in,out] submsg_len  Size of submessage.
 *
 * @returns bool
 * @retval true   Decoding succeeded or not necessary. Either way, payloadp
 *                contains the data that should be deserialized.
 * @retval false  Decoding was necessary, but failed.
 */
bool
decode_Data(
  const struct q_globals *gv,
  struct nn_rsample_info *sampleinfo,
  unsigned char *payloadp,
  uint32_t payloadsz,
  size_t *submsg_len);

/**
 * @brief Decode the payload of a DataFrag submessage.
 *
 * When decoding is necessary, the payloadp memory will be replaced
 * by the decoded payload. This means that the original submessage
 * now contains payload that can be deserialized.
 *
 * If no decoding is necessary, nothing changes.
 *
 * @param[in]     gv          Global information.
 * @param[in]     sampleinfo  Sample information.
 * @param[in,out] payloadp    Pointer to payload memory.
 * @param[in]     payloadsz   Size of payload.
 * @param[in,out] submsg_len  Size of submessage.
 *
 * @returns bool
 * @retval true   Decoding succeeded or not necessary. Either way, payloadp
 *                contains the data that should be deserialized.
 * @retval false  Decoding was necessary, but failed.
 */
bool
decode_DataFrag(
  const struct q_globals *gv,
  struct nn_rsample_info *sampleinfo,
  unsigned char *payloadp,
  uint32_t payloadsz,
  size_t *submsg_len);

/**
 * @brief Encode datareader submessage when necessary.
 *
 * When encoding is necessary, the original submessage will be replaced
 * by a new encoded submessage.
 * If the encoding fails, the original submessage will be removed.
 *
 * If no encoding is necessary, nothing changes.
 *
 * @param[in,out] msg       Complete message.
 * @param[in,out] sm_marker Submessage location within message.
 * @param[in]     pwr       Writer for which the message is intended.
 * @param[in]     rd_guid   Origin reader guid.
 */
void
encode_datareader_submsg(
  struct nn_xmsg *msg,
  struct nn_xmsg_marker sm_marker,
  struct proxy_writer *pwr,
  const struct ddsi_guid *rd_guid);

/**
 * @brief Encode datawriter submessage when necessary.
 *
 * When encoding is necessary, the original submessage will be replaced
 * by a new encoded submessage.
 * If the encoding fails, the original submessage will be removed.
 *
 * If no encoding is necessary, nothing changes.
 *
 * @param[in,out] msg       Complete message.
 * @param[in,out] sm_marker Submessage location within message.
 * @param[in]     wr        Origin writer guid.
 */
void
encode_datawriter_submsg(
  struct nn_xmsg *msg,
  struct nn_xmsg_marker sm_marker,
  struct writer *wr);

/**
 * @brief Check if given submessage is properly decoded.
 *
 * When decoding is necessary, it should be checked if a plain submessage was
 * actually decoded. Otherwise data can be injected just by inserting a plain
 * submessage directly.
 *
 * @param[in] e         Entity information.
 * @param[in] c         Proxy endpoint information.
 * @param[in] proxypp   Related proxy participant.
 * @param[in] rst       Receiver information.
 * @param[in] prev_smid Previously handled submessage ID.
 *
 * @returns bool
 * @retval true   Decoding succeeded or was not necessary.
 * @retval false  Decoding was necessary, but not detected.
 */
bool
validate_msg_decoding(
  const struct entity_common *e,
  const struct proxy_endpoint_common *c,
  const struct proxy_participant *proxypp,
  const struct receiver_state *rst,
  SubmessageKind_t prev_smid);

/**
 * @brief Decode not only SecPrefix, but also the SecBody and SecPostfix
 * sub-messages.
 *
 * When encrypted, the original SecBody will be replaced by the decrypted
 * submessage. Then the normal sequence can continue as if there was no
 * encrypted data.
 *
 * @param[in]     rst         Receiver information.
 * @param[in,out] submsg      Pointer to SecPrefix/(SecBody|Submsg)/SecPostfix.
 * @param[in]     submsg_size Size of SecPrefix submessage.
 * @param[in]     msg_end     End of the complete message.
 * @param[in]     src_prefix  Prefix of the source entity.
 * @param[in]     dst_prefix  Prefix of the destination entity.
 * @param[in]     byteswap    Do the bytes need swapping?
 *
 * @returns bool
 * @retval true   Decoding succeeded.
 * @retval false  Decoding failed.
 */
bool
decode_SecPrefix(
  const struct receiver_state *rst,
  unsigned char *submsg,
  size_t submsg_size,
  unsigned char * const msg_end,
  const ddsi_guid_prefix_t * const src_prefix,
  const ddsi_guid_prefix_t * const dst_prefix,
  int byteswap);

/**
 * @brief Decode the RTPS message.
 *
 * When encrypted, the original buffers and information will be replaced
 * by the decrypted RTPS message. Then the normal sequence can continue
 * as if there was no encrypted data.
 *
 * @param[in]     ts1         Thread information.
 * @param[in]     gv          Global information.
 * @param[in,out] rmsg        Message information.
 * @param[in,out] hdr         Message header.
 * @param[in,out] buff        Message buffer.
 * @param[in,out] sz          Message size.
 * @param[in]     rbpool      Buffers pool.
 * @param[in]     isstream    Is message a stream variant?
 *
 * @returns nn_rtps_msg_state_t
 * @retval NN_RTPS_MSG_STATE_PLAIN    No decoding was necessary.
 * @retval NN_RTPS_MSG_STATE_ENCODED  Decoding succeeded.
 * @retval NN_RTPS_MSG_STATE_ERROR    Decoding failed.
 */
nn_rtps_msg_state_t
decode_rtps_message(
  struct thread_state1 * const ts1,
  struct q_globals *gv,
  struct nn_rmsg **rmsg,
  Header_t **hdr,
  unsigned char **buff,
  ssize_t *sz,
  struct nn_rbufpool *rbpool,
  bool isstream);

/**
 * @brief Send the RTPS message securely.
 *
 * @param[in]     conn          Connection to use.
 * @param[in]     dst           Possible destination information.
 * @param[in]     niov          Number of io vectors.
 * @param[in]     iov           Array of io vectors.
 * @param[in]     flags         Connection write flags.
 * @param[in,out] msg_len       Submessage containing length.
 * @param[in]     dst_one       Is there only one specific destination?
 * @param[in]     sec_info      Security information for handles.
 * @param[in]     conn_write_cb Function to call to do the actual writing.
 *
 * @returns ssize_t
 * @retval negative/zero    Something went wrong.
 * @retval positive         Secure writing succeeded.
 */
ssize_t
secure_conn_write(
    ddsi_tran_conn_t conn,
    const nn_locator_t *dst,
    size_t niov,
    const ddsrt_iovec_t *iov,
    uint32_t flags,
    MsgLen_t *msg_len,
    bool dst_one,
    nn_msg_sec_info_t *sec_info,
    ddsi_tran_write_fn_t conn_write_cb);


/**
 * @brief Loads the security plugins with the given configuration.
 *        This function tries to load the plugins only once. Returns the same
 *        result on subsequent calls.
 *        It logs the reason and returns error if can not load a plugin.
 *
 * @param[in] qos   Participant qos which owns the Property list
 *                             that contains security configurations and
 *                             plugin properties that are required for loading libraries
 * @returns dds_return_t
 * @retval DDS_RETCODE_OK   All plugins are successfully loaded
 * @retval DDS_RETCODE_ERROR  One or more security plugins are not loaded.
 */
dds_return_t q_omg_security_load( struct dds_security_context *security_context, const dds_qos_t *qos );


void q_omg_security_init( struct dds_security_context **sc, const struct ddsrt_log_cfg *logcfg);

void q_omg_security_deinit( struct dds_security_context **sc);

bool q_omg_is_security_loaded(  struct dds_security_context *sc );


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
is_proxy_participant_deletion_allowed(
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
q_omg_security_check_create_participant(UNUSED_ARG(struct participant *pp), UNUSED_ARG(uint32_t domain_id))
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

inline bool q_omg_security_match_remote_writer_enabled(UNUSED_ARG(struct reader *rd), UNUSED_ARG(struct proxy_writer *pwr))
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

inline void
set_proxy_participant_security_info(
  UNUSED_ARG(struct proxy_participant *prd),
  UNUSED_ARG(const nn_plist_t *plist))
{
}

inline void
set_proxy_reader_security_info(
  UNUSED_ARG(struct proxy_reader *prd),
  UNUSED_ARG(const nn_plist_t *plist))
{
}

inline void
set_proxy_writer_security_info(
  UNUSED_ARG(struct proxy_writer *pwr),
  UNUSED_ARG(const nn_plist_t *plist))
{
}

inline bool
decode_Data(
  UNUSED_ARG(const struct q_globals *gv),
  UNUSED_ARG(struct nn_rsample_info *sampleinfo),
  UNUSED_ARG(unsigned char *payloadp),
  UNUSED_ARG(uint32_t payloadsz),
  UNUSED_ARG(size_t *submsg_len))
{
  return true;
}

inline bool
decode_DataFrag(
  UNUSED_ARG(const struct q_globals *gv),
  UNUSED_ARG(struct nn_rsample_info *sampleinfo),
  UNUSED_ARG(unsigned char *payloadp),
  UNUSED_ARG(uint32_t payloadsz),
  UNUSED_ARG(size_t *submsg_len))
{
  return true;
}

inline void
encode_datareader_submsg(
  UNUSED_ARG(struct nn_xmsg *msg),
  UNUSED_ARG(struct nn_xmsg_marker sm_marker),
  UNUSED_ARG(struct proxy_writer *pwr),
  UNUSED_ARG(const struct ddsi_guid *rd_guid))
{
}

inline void
encode_datawriter_submsg(
  UNUSED_ARG(struct nn_xmsg *msg),
  UNUSED_ARG(struct nn_xmsg_marker sm_marker),
  UNUSED_ARG(struct writer *wr))
{
}

inline bool
validate_msg_decoding(
  UNUSED_ARG(const struct entity_common *e),
  UNUSED_ARG(const struct proxy_endpoint_common *c),
  UNUSED_ARG(struct proxy_participant *proxypp),
  UNUSED_ARG(struct receiver_state *rst),
  UNUSED_ARG(SubmessageKind_t prev_smid))
{
  return true;
}

inline int
decode_SecPrefix(
  UNUSED_ARG(struct receiver_state *rst),
  UNUSED_ARG(unsigned char *submsg),
  UNUSED_ARG(size_t submsg_size),
  UNUSED_ARG(unsigned char * const msg_end),
  UNUSED_ARG(const ddsi_guid_prefix_t * const src_prefix),
  UNUSED_ARG(const ddsi_guid_prefix_t * const dst_prefix),
  UNUSED_ARG(int byteswap))
{
  /* Just let the parsing ignore the security sub-messages. */
  return true;
}

inline nn_rtps_msg_state_t
decode_rtps_message(
  UNUSED_ARG(struct thread_state1 * const ts1),
  UNUSED_ARG(struct q_globals *gv),
  UNUSED_ARG(struct nn_rmsg **rmsg),
  UNUSED_ARG(Header_t **hdr),
  UNUSED_ARG(unsigned char **buff),
  UNUSED_ARG(ssize_t *sz),
  UNUSED_ARG(struct nn_rbufpool *rbpool),
  UNUSED_ARG(bool isstream))
{
  return NN_RTPS_MSG_STATE_PLAIN;
}

inline dds_return_t q_omg_security_load( UNUSED_ARG( struct dds_security_context *security_context ), UNUSED_ARG( const dds_qos_t *property_seq) )
{
  return DDS_RETCODE_ERROR;
}

inline void q_omg_security_init( UNUSED_ARG( struct dds_security_context *sc) ) {}

inline void q_omg_security_deinit( UNUSED_ARG( struct dds_security_context *sc) ) {}

inline bool q_omg_is_security_loaded(  UNUSED_ARG( struct dds_security_context *sc )) { return false; }

#endif /* DDSI_INCLUDE_SECURITY */

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_OMG_SECURITY_H */
