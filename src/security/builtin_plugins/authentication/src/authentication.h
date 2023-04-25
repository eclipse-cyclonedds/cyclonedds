// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef SECURITY_BUILTIN_PLUGINS_AUTHENTICATION_H_
#define SECURITY_BUILTIN_PLUGINS_AUTHENTICATION_H_

#include "dds/ddsrt/atomics.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/export.h"

SECURITY_EXPORT int32_t init_authentication(const char *argument, void **context, struct ddsi_domaingv *gv);
SECURITY_EXPORT int32_t finalize_authentication(void *context);

DDS_Security_ValidationResult_t validate_local_identity(dds_security_authentication *instance, DDS_Security_IdentityHandle *local_identity_handle, DDS_Security_GUID_t *adjusted_participant_guid,
    const DDS_Security_DomainId domain_id, const DDS_Security_Qos *participant_qos, const DDS_Security_GUID_t *candidate_participant_guid, DDS_Security_SecurityException *ex);
DDS_Security_boolean get_identity_token(dds_security_authentication *instance, DDS_Security_IdentityToken *identity_token, const DDS_Security_IdentityHandle handle, DDS_Security_SecurityException *ex);
DDS_Security_boolean set_permissions_credential_and_token(dds_security_authentication *instance, const DDS_Security_IdentityHandle handle, const DDS_Security_PermissionsCredentialToken *permissions_credential,
    const DDS_Security_PermissionsToken *permissions_token, DDS_Security_SecurityException *ex);
DDS_Security_ValidationResult_t validate_remote_identity(dds_security_authentication *instance, DDS_Security_IdentityHandle *remote_identity_handle, DDS_Security_AuthRequestMessageToken *local_auth_request_token,
    const DDS_Security_AuthRequestMessageToken *remote_auth_request_token, const DDS_Security_IdentityHandle local_identity_handle, const DDS_Security_IdentityToken *remote_identity_token,
    const DDS_Security_GUID_t *remote_participant_guid, DDS_Security_SecurityException *ex);
DDS_Security_ValidationResult_t begin_handshake_request(dds_security_authentication *instance, DDS_Security_HandshakeHandle *handshake_handle, DDS_Security_HandshakeMessageToken *handshake_message,
    const DDS_Security_IdentityHandle initiator_identity_handle, const DDS_Security_IdentityHandle replier_identity_handle, const DDS_Security_OctetSeq *serialized_local_participant_data, DDS_Security_SecurityException *ex);
DDS_Security_ValidationResult_t begin_handshake_reply(dds_security_authentication *instance, DDS_Security_HandshakeHandle *handshake_handle, DDS_Security_HandshakeMessageToken *handshake_message_out,
    const DDS_Security_HandshakeMessageToken *handshake_message_in, const DDS_Security_IdentityHandle initiator_identity_handle, const DDS_Security_IdentityHandle replier_identity_handle,
    const DDS_Security_OctetSeq *serialized_local_participant_data, DDS_Security_SecurityException *ex);
DDS_Security_ValidationResult_t process_handshake(dds_security_authentication *instance, DDS_Security_HandshakeMessageToken *handshake_message_out, const DDS_Security_HandshakeMessageToken *handshake_message_in,
    const DDS_Security_HandshakeHandle handshake_handle, DDS_Security_SecurityException *ex);
DDS_Security_SharedSecretHandle get_shared_secret(dds_security_authentication *instance, const DDS_Security_HandshakeHandle handshake_handle, DDS_Security_SecurityException *ex);
DDS_Security_boolean get_authenticated_peer_credential_token(dds_security_authentication *instance, DDS_Security_AuthenticatedPeerCredentialToken *peer_credential_token,
    const DDS_Security_HandshakeHandle handshake_handle, DDS_Security_SecurityException *ex);
DDS_Security_boolean get_identity_status_token(dds_security_authentication *instance, DDS_Security_IdentityStatusToken *identity_status_token, const DDS_Security_IdentityHandle handle, DDS_Security_SecurityException *ex);
DDS_Security_boolean set_listener(dds_security_authentication *instance, const dds_security_authentication_listener *listener, DDS_Security_SecurityException *ex);
DDS_Security_boolean return_identity_token(dds_security_authentication *instance, const DDS_Security_IdentityToken *token, DDS_Security_SecurityException *ex);
DDS_Security_boolean return_identity_status_token(dds_security_authentication *instance, const DDS_Security_IdentityStatusToken *token, DDS_Security_SecurityException *ex);
DDS_Security_boolean return_authenticated_peer_credential_token(dds_security_authentication *instance, const DDS_Security_AuthenticatedPeerCredentialToken *peer_credential_token, DDS_Security_SecurityException *ex);
DDS_Security_boolean return_handshake_handle(dds_security_authentication *instance, const DDS_Security_HandshakeHandle handshake_handle, DDS_Security_SecurityException *ex);
DDS_Security_boolean return_identity_handle(dds_security_authentication *instance, const DDS_Security_IdentityHandle identity_handle, DDS_Security_SecurityException *ex);
DDS_Security_boolean return_sharedsecret_handle(dds_security_authentication *instance, const DDS_Security_SharedSecretHandle sharedsecret_handle, DDS_Security_SecurityException *ex);

#endif /* SECURITY_BUILTIN_PLUGINS_AUTHENTICATION_H_ */
