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
//
// Created by kurtulus on 29-11-17.
//
//#include "os/os_public.h"
//#include "os/os_decl_attributes_sal.h"

#include "ddsc/dds.h"
#include "dds_security_builtintopics.h"
#include "dds_security_interface_types.h"

#ifndef DDSC_SECURITY_H
#define DDSC_SECURITY_H

//Note – It is recommended that native types be mapped to equivalent type
// names in each programming language, subject to the normal mapping rules for type names in that language

/**
 * Authentication Component
 */

typedef struct dds_security_authentication dds_security_authentication;

/**
 * AuthenticationListener interface
 */

typedef bool
(*dds_security_authentication_listener_on_revoke_identity)
        (void *listener_data,
         _In_ const dds_security_authentication *plugin,
         _In_ const DDS_Security_IdentityHandle handle,
         _Inout_ DDS_Security_SecurityException *ex
        );

typedef bool
(*dds_security_authentication_listener_on_status_changed)
        (void *listener_data,
         _In_ const dds_security_authentication *plugin,
         _In_ const DDS_Security_IdentityHandle handle,
         _In_ const DDS_Security_AuthStatusKind status_kind,
         _Inout_ DDS_Security_SecurityException *ex
        );


typedef struct dds_security_authentication_listener
{
  void *listener_data;

  dds_security_authentication_listener_on_revoke_identity on_revoke_identity;

  dds_security_authentication_listener_on_status_changed on_status_changed;
} dds_security_authentication_listener;

#define dds_security_authentication_listener__alloc() \
((dds_security_authentication_listener*) dds_alloc (sizeof (dds_security_authentication_listener)));


typedef DDS_Security_ValidationResult_t
(*dds_security_authentication_validate_local_identity)
        (void *listener_data,
         _Inout_ DDS_Security_IdentityHandle *local_identity_handle,
         _Inout_ DDS_Security_GUID_t *adjusted_participant_guid,
         _In_ const dds_domainid_t domain_id,
         _In_ const DDS_Security_DomainParticipantQos *participant_qos,
         _In_ const DDS_Security_GUID_t *candidate_participant_guid,
         _Inout_ DDS_Security_SecurityException *ex
        );


typedef bool
(*dds_security_authentication_get_identity_token)
        (void *listener_data,
         _Inout_ DDS_Security_IdentityToken *identity_token,
         _In_ const DDS_Security_IdentityHandle handle,
         _Inout_ DDS_Security_SecurityException *ex);


typedef bool
(*dds_security_authentication_get_identity_status_token)
        (void *listener_data,
         _Inout_ DDS_Security_IdentityStatusToken *identity_status_token,
         _In_ const DDS_Security_IdentityHandle handle,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_authentication_set_permissions_credential_and_token)
        (void *listener_data,
         _In_ const DDS_Security_IdentityHandle handle,
         _In_ const DDS_Security_PermissionsCredentialToken *permissions_credential,
         _In_ const DDS_Security_PermissionsToken *permissions_token,
         _Inout_ DDS_Security_SecurityException *ex);


typedef DDS_Security_ValidationResult_t
(*dds_security_authentication_validate_remote_identity)
        (void *listener_data,
         _Inout_ DDS_Security_IdentityHandle *remote_identity_handle,
         _Inout_ DDS_Security_AuthRequestMessageToken *local_auth_request_token,
         _In_ const DDS_Security_AuthRequestMessageToken *remote_auth_request_token,
         _In_ const DDS_Security_IdentityHandle local_identity_handle,
         _In_ const DDS_Security_IdentityToken *remote_identity_token,
         _In_ const DDS_Security_GUID_t *remote_participant_guid,
         _Inout_ DDS_Security_SecurityException *ex);


typedef DDS_Security_ValidationResult_t
(*dds_security_authentication_begin_handshake_request)
        (void *listener_data,
         _Inout_ DDS_Security_HandshakeHandle *handshake_handle,
         _Inout_ DDS_Security_HandshakeMessageToken *handshake_message,
         _In_ const DDS_Security_HandshakeMessageToken *handshake_message_in,
         _In_ const DDS_Security_IdentityHandle initiator_identity_handle,
         _In_ const DDS_Security_IdentityHandle replier_identity_handle,
         _In_ const DDS_OctetSeq *serialized_local_participant_data,
         _Inout_ DDS_Security_SecurityException *ex);


typedef DDS_Security_ValidationResult_t
(*dds_security_authentication_begin_handshake_reply)
        (void *listener_data,
         _Inout_ DDS_Security_HandshakeHandle *handshake_handle,
         _Inout_ DDS_Security_HandshakeMessageToken *handshake_message_out,
         _In_ const DDS_Security_HandshakeMessageToken *handshake_message_in,
         _In_ const DDS_Security_IdentityHandle initiator_identity_handle,
         _In_ const DDS_Security_IdentityHandle replier_identity_handle,
         _In_ const DDS_OctetSeq *serialized_local_participant_data,
         _Inout_ DDS_Security_SecurityException *ex);

typedef DDS_Security_ValidationResult_t
(*dds_security_authentication_process_handshake)
        (void *listener_data,
         _Inout_ DDS_Security_HandshakeMessageToken *handshake_message_out,
         _In_ const DDS_Security_HandshakeMessageToken *handshake_message_in,
         _In_ const DDS_Security_HandshakeHandle handshake_handle,
         _Inout_ DDS_Security_SecurityException *ex);

typedef DDS_Security_SharedSecretHandle
(*dds_security_authentication_get_shared_secret)
        (void *listener_data,
         _In_ const DDS_Security_HandshakeHandle handshake_handle,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_authentication_get_authenticated_peer_credential_token)
        (void *listener_data,
         _Inout_ DDS_Security_AuthenticatedPeerCredentialToken *peer_credential_token,
         _In_ const DDS_Security_HandshakeHandle handshake_handle,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_authentication_set_listener)
        (void *listener_data,
         _In_ const dds_security_authentication_listener *listener,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_authentication_return_identity_token)
        (void *listener_data,
         _In_ const DDS_Security_IdentityToken *token,
         _Inout_ DDS_Security_SecurityException *ex);


typedef bool
(*dds_security_authentication_return_identity_status_token)
        (void *listener_data,
         _In_ const DDS_Security_IdentityStatusToken *token,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_authentication_return_authenticated_peer_credential_token)
        (void *listener_data,
         _In_ const DDS_Security_AuthenticatedPeerCredentialToken *peer_credential_token,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_authentication_return_handshake_handle)
        (void *listener_data,
         _In_ const DDS_Security_HandshakeHandle handshake_handle,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_authentication_return_identity_handle)
        (void *listener_data,
         _In_ const DDS_Security_IdentityHandle identity_handle,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_authentication_return_sharedsecret_handle)
        (void *listener_data,
         _In_ const DDS_Security_SharedSecretHandle sharedsecret_handle,
         _Inout_ DDS_Security_SecurityException *ex);


struct dds_security_authentication
{

  dds_security_authentication_validate_local_identity validate_local_identity;

  dds_security_authentication_get_identity_token get_identity_token;

  dds_security_authentication_get_identity_status_token get_identity_status_token;

  dds_security_authentication_set_permissions_credential_and_token set_permissions_credential_and_token;

  dds_security_authentication_validate_remote_identity validate_remote_identity;

  dds_security_authentication_begin_handshake_request begin_handshake_request;

  dds_security_authentication_begin_handshake_reply begin_handshake_reply;

  dds_security_authentication_process_handshake process_handshake;

  dds_security_authentication_get_shared_secret get_shared_secret;

  dds_security_authentication_get_authenticated_peer_credential_token get_authenticated_peer_credential_token;

  dds_security_authentication_set_listener set_listener;

  dds_security_authentication_return_identity_token return_identity_token;

  dds_security_authentication_return_identity_status_token return_identity_status_token;

  dds_security_authentication_return_authenticated_peer_credential_token return_authenticated_peer_credential_token;

  dds_security_authentication_return_handshake_handle return_handshake_handle;

  dds_security_authentication_return_identity_handle return_identity_handle;

  dds_security_authentication_return_sharedsecret_handle return_sharedsecret_handle;
};

#define dds_security_authentication__alloc() \
((dds_security_authentication*) dds_alloc (sizeof (dds_security_authentication)));


/**
 * AccessControl Component
 */

typedef struct dds_security_access_control dds_security_access_control;

/**
 * AccessControlListener Interface
 * */



typedef bool
(*dds_security_access_control_listener_on_revoke_permissions)
        (void *listener_data,
         _In_ const dds_security_access_control *plugin,
         _In_ const DDS_Security_PermissionsHandle handle);

typedef struct dds_security_access_control_listener
{
  dds_security_access_control_listener_on_revoke_permissions on_revoke_permissions;
} dds_security_access_control_listener;


/**
 * AccessControl Interface
 */

typedef DDS_Security_PermissionsHandle
(*dds_security_access_control_validate_local_permissions)
        (void *listener_data,
         _In_ const dds_security_authentication *auth_plugin,
         _In_ const DDS_Security_IdentityHandle identity,
         _In_ const dds_domainid_t domain_id,
         _In_ const DDS_Security_DomainParticipantQos *participant_qos,
         _Inout_ DDS_Security_SecurityException *ex);

typedef DDS_Security_PermissionsHandle
(*dds_security_access_control_validate_remote_permissions)
        (void *listener_data,
         _In_ const dds_security_authentication *auth_plugin,
         _In_ const DDS_Security_IdentityHandle local_identity_handle,
         _In_ const DDS_Security_IdentityHandle remote_identity_handle,
         _In_ const DDS_Security_PermissionsToken *remote_permissions_token,
         _In_ const DDS_Security_AuthenticatedPeerCredentialToken *remote_credential_token,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_access_control_check_create_participant)
        (void *listener_data,
         _In_ const DDS_Security_PermissionsHandle permissions_handle,
         _In_ const dds_domainid_t domain_id,
         _In_ const dds_qos_t **participant_qos,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_access_control_check_create_datawriter)
        (void *listener_data,
         _In_ const DDS_Security_PermissionsHandle permissions_handle,
         _In_ const dds_domainid_t domain_id,
         _In_ const char *topic_name,
         _In_ const dds_qos_t *writer_qos,
         _In_ const DDS_PartitionQosPolicy *partition,
         _In_ const DDS_Security_DataTags *data_tag,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_access_control_check_create_datareader)
        (void *listener_data,
         _In_ const DDS_Security_PermissionsHandle permissions_handle,
         _In_ const dds_domainid_t domain_id,
         _In_ const char *topic_name,
         _In_ const dds_qos_t *reader_qos,
         _In_ const DDS_PartitionQosPolicy *partition,
         _In_ const DDS_Security_DataTags *data_tag,
         _Inout_ DDS_Security_SecurityException *ex);


typedef bool
(*dds_security_access_control_check_create_topic)
        (void *listener_data,
         _In_ const DDS_Security_PermissionsHandle permissions_handle,
         _In_ const dds_domainid_t domain_id,
         _In_ const char *topic_name,
         _In_ const DDS_TopicQos *qos,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_access_control_check_local_datawriter_register_instance)
        (void *listener_data,
         _In_ const DDS_Security_PermissionsHandle permissions_handle,
         _In_ const dds_entity_t *writer,
         _In_ const DDS_Security_DynamicData *key,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_access_control_check_local_datawriter_dispose_instance)
        (void *listener_data,
         _In_ const DDS_Security_PermissionsHandle permissions_handle,
         _In_ const dds_entity_t *writer,
         _In_ const DDS_Security_DynamicData key,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_access_control_check_remote_participant)
        (void *listener_data,
         _In_ const DDS_Security_PermissionsHandle permissions_handle,
         _In_ const dds_domainid_t domain_id,
         _In_ const DDS_Security_ParticipantBuiltinTopicDataSecure *participant_data,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_access_control_check_remote_datawriter)
        (void *listener_data,
         _In_ const DDS_Security_PermissionsHandle permissions_handle,
         _In_ const dds_domainid_t domain_id,
         _In_ const DDS_Security_PublicationBuiltinTopicDataSecure *publication_data,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_access_control_check_remote_datareader)
        (void *listener_data,
         _In_ const DDS_Security_PermissionsHandle permissions_handle,
         _In_ const dds_domainid_t domain_id,
         _In_ const DDS_Security_SubscriptionBuiltinTopicDataSecure *subscription_data,
         _Inout_ bool *relay_only,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_access_control_check_remote_topic)
        (void *listener_data,
         _In_ const DDS_Security_PermissionsHandle permissions_handle,
         _In_ const dds_domainid_t domain_id,
         _In_ const DDS_TopicBuiltinTopicData *topic_data,
         _Inout_ DDS_Security_SecurityException *ex);


typedef bool
(*dds_security_access_control_check_local_datawriter_match)
        (void *listener_data,
         _In_ const DDS_Security_PermissionsHandle writer_permissions_handle,
         _In_ const DDS_Security_PermissionsHandle reader_permissions_handle,
         _In_ const DDS_Security_PublicationBuiltinTopicDataSecure *publication_data,
         _In_ const DDS_Security_SubscriptionBuiltinTopicDataSecure *subscription_data,
         _Inout_ DDS_Security_SecurityException *ex);


typedef bool
(*dds_security_access_control_check_local_datareader_match)
        (void *listener_data,
         _In_ const DDS_Security_PermissionsHandle reader_permissions_handle,
         _In_ const DDS_Security_PermissionsHandle writer_permissions_handle,
         _In_ const DDS_Security_SubscriptionBuiltinTopicDataSecure *subscription_data,
         _In_ const DDS_Security_PublicationBuiltinTopicDataSecure *publication_data,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_access_control_check_remote_datawriter_register_instance)
        (void *listener_data,
         _In_ const DDS_Security_PermissionsHandle permissions_handle,
         _In_ const dds_entity_t *reader,
         _In_ const dds_instance_handle_t publication_handle,
         _In_ const DDS_Security_DynamicData key,
         _In_ const dds_instance_handle_t instance_handle,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_access_control_check_remote_datawriter_dispose_instance)
        (void *listener_data,
         _In_ const DDS_Security_PermissionsHandle permissions_handle,
         _In_ const dds_entity_t *reader,
         _In_ const dds_instance_handle_t publication_handle,
         _In_ const DDS_Security_DynamicData key,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_access_control_get_permissions_token)
        (void *listener_data,
         _Inout_ DDS_Security_PermissionsToken *permissions_token,
         _In_ const DDS_Security_PermissionsHandle handle,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_access_control_get_permissions_credential_token)
        (void *listener_data,
         _Inout_ DDS_Security_PermissionsCredentialToken *permissions_credential_token,
         _In_ const DDS_Security_PermissionsHandle handle,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_access_control_set_listener)
        (void *listener_data,
         _In_ const dds_security_access_control_listener *listener,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_access_control_return_permissions_token)
        (void *listener_data,
         _In_ const DDS_Security_PermissionsToken *token,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_access_control_return_permissions_credential_token)
        (void *listener_data,
         _In_ const DDS_Security_PermissionsCredentialToken *permissions_credential_token,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_access_control_get_participant_sec_attributes)
        (void *listener_data,
         _In_ const DDS_Security_PermissionsHandle permissions_handle,
         _Inout_ DDS_Security_ParticipantSecurityAttributes *attributes,
         _Inout_ DDS_Security_SecurityException *ex);


typedef bool
(*dds_security_access_control_get_topic_sec_attributes)
        (void *listener_data,
         _In_ const DDS_Security_PermissionsHandle permissions_handle,
         _In_ const char *topic_name,
         _Inout_ DDS_Security_TopicSecurityAttributes *attributes,
         _Inout_ DDS_Security_SecurityException *ex);


typedef bool
(*dds_security_access_control_get_datawriter_sec_attributes)
        (void *listener_data,
         _In_ const DDS_Security_PermissionsHandle permissions_handle,
         _In_ const DDS_PartitionQosPolicy *partition,
         _In_ const DDS_Security_DataTagQosPolicy *data_tag,
         _Inout_ DDS_Security_EndpointSecurityAttributes *attributes,
         _Inout_ DDS_Security_SecurityException *ex);


typedef bool
(*dds_security_access_control_get_datareader_sec_attributes)
        (void *listener_data,
         _In_ const DDS_Security_PermissionsHandle permissions_handle,
         _In_ const DDS_PartitionQosPolicy *partition,
         _In_ const DDS_Security_DataTagQosPolicy *data_tag,
         _Inout_ DDS_Security_EndpointSecurityAttributes *attributes,
         _Inout_ DDS_Security_SecurityException *ex);


typedef bool
(*dds_security_access_control_return_participant_sec_attributes)
        (void *listener_data,
         _In_ const DDS_Security_ParticipantSecurityAttributes *attributes,
         _Inout_ DDS_Security_SecurityException *ex);


typedef bool
(*dds_security_access_control_return_datawriter_sec_attributes)
        (void *listener_data,
         _In_ const DDS_Security_EndpointSecurityAttributes *attributes,
         _Inout_ DDS_Security_SecurityException *ex);


typedef bool
(*dds_security_access_control_return_datareader_sec_attributes)
        (void *listener_data,
         _In_ const DDS_Security_EndpointSecurityAttributes *attributes,
         _Inout_ DDS_Security_SecurityException *ex);


struct dds_security_access_control
{
  dds_security_access_control_validate_local_permissions validate_local_permissions;

  dds_security_access_control_validate_remote_permissions validate_remote_permissions;

  dds_security_access_control_check_create_participant check_create_participant;

  dds_security_access_control_check_create_datawriter check_create_datawriter;

  dds_security_access_control_check_create_datareader check_create_datareader;

  dds_security_access_control_check_create_topic check_create_topic;

  dds_security_access_control_check_local_datawriter_register_instance check_local_datawriter_register_instance;

  dds_security_access_control_check_local_datawriter_dispose_instance check_local_datawriter_dispose_instance;

  dds_security_access_control_check_remote_participant check_remote_participant;

  dds_security_access_control_check_remote_datawriter check_remote_datawriter;

  dds_security_access_control_check_remote_datareader check_remote_datareader;

  dds_security_access_control_check_remote_topic check_remote_topic;

  dds_security_access_control_check_local_datawriter_match check_local_datawriter_match;

  dds_security_access_control_check_local_datareader_match check_local_datareader_match;

  dds_security_access_control_check_remote_datawriter_register_instance check_remote_datawriter_register_instance;

  dds_security_access_control_check_remote_datawriter_dispose_instance check_remote_datawriter_dispose_instance;

  dds_security_access_control_get_permissions_token get_permissions_token;

  dds_security_access_control_get_permissions_credential_token get_permissions_credential_token;

  dds_security_access_control_set_listener set_listener;

  dds_security_access_control_return_permissions_token return_permissions_token;

  dds_security_access_control_return_permissions_credential_token return_permissions_credential_token;

  dds_security_access_control_get_participant_sec_attributes get_participant_sec_attributes;

  dds_security_access_control_get_topic_sec_attributes get_topic_sec_attributes;

  dds_security_access_control_get_datawriter_sec_attributes get_datawriter_sec_attributes;

  dds_security_access_control_get_datareader_sec_attributes get_datareader_sec_attributes;

  dds_security_access_control_return_participant_sec_attributes return_participant_sec_attributes;

  dds_security_access_control_return_datawriter_sec_attributes return_datawriter_sec_attributes;

  dds_security_access_control_return_datareader_sec_attributes return_datareader_sec_attributes;

};

struct dds_security_access_control *dds_security_access_control__alloc(void);

/**
 * Crypto Component
 */

/**
 * CryptoKeyFactory interface
 */

typedef DDS_Security_ParticipantCryptoHandle
(*dds_security_crypto_key_factory_register_local_participant)
        (void *listener_data,
         _In_ const DDS_Security_IdentityHandle participant_identity,
         _In_ const DDS_Security_PermissionsHandle participant_permissions,
         _In_ const DDS_Security_PropertySeq *participant_properties,
         _In_ const DDS_Security_ParticipantSecurityAttributes *participant_security_attributes,
         _Inout_ DDS_Security_SecurityException *ex);

typedef DDS_Security_ParticipantCryptoHandle
(*dds_security_crypto_key_factory_register_matched_remote_participant)
        (void *listener_data,
         _In_ const DDS_Security_ParticipantCryptoHandle local_participant_crypto_handle,
         _In_ const DDS_Security_IdentityHandle remote_participant_identity,
         _In_ const DDS_Security_PermissionsHandle remote_participant_permissions,
         _In_ const DDS_Security_SharedSecretHandle shared_secret,
         _Inout_ DDS_Security_SecurityException *ex);


typedef DDS_Security_DatawriterCryptoHandle
(*dds_security_crypto_key_factory_register_local_datawriter)
        (void *listener_data,
         _In_ const DDS_Security_ParticipantCryptoHandle participant_crypto,
         _In_ const DDS_Security_PropertySeq *datawriter_properties,
         _In_ const DDS_Security_EndpointSecurityAttributes *datawriter_security_attributes,
         _Inout_ DDS_Security_SecurityException *ex);

typedef DDS_Security_DatareaderCryptoHandle
(*dds_security_crypto_key_factory_register_matched_remote_datareader)
        (void *listener_data,
         _In_ const DDS_Security_DatawriterCryptoHandle local_datawritert_crypto_handle,
         _In_ const DDS_Security_ParticipantCryptoHandle remote_participant_crypto,
         _In_ const DDS_Security_SharedSecretHandle shared_secret,
         _In_ const bool relay_only,
         _Inout_ DDS_Security_SecurityException *ex);


typedef DDS_Security_DatareaderCryptoHandle
(*dds_security_crypto_key_factory_register_local_datareader)
        (void *listener_data,
         _In_ const DDS_Security_ParticipantCryptoHandle *participant_crypto,
         _In_ const DDS_Security_PropertySeq *datareader_properties,
         _In_ const DDS_Security_EndpointSecurityAttributes *datareader_security_attributes,
         _Inout_ DDS_Security_SecurityException *ex);

typedef DDS_Security_DatawriterCryptoHandle
(*dds_security_crypto_key_factory_register_matched_remote_datawriter)
        (void *listener_data,
         _In_ const DDS_Security_DatareaderCryptoHandle local_datareader_crypto_handle,
         _In_ const DDS_Security_ParticipantCryptoHandle remote_participant_crypt,
         _In_ const DDS_Security_SharedSecretHandle shared_secret,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_crypto_key_factory_unregister_participant)
        (void *listener_data,
         _In_ const DDS_Security_ParticipantCryptoHandle participant_crypto_handle,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_crypto_key_factory_unregister_datawriter)
        (void *listener_data,
         _In_ const DDS_Security_DatawriterCryptoHandle datawriter_crypto_handle,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_crypto_key_factory_unregister_datareader)
        (void *listener_data,
         _In_ const DDS_Security_DatareaderCryptoHandle datareader_crypto_handle,
         _Inout_ DDS_Security_SecurityException *ex);

typedef struct dds_security_crypto_key_factory
{

  dds_security_crypto_key_factory_register_local_participant register_local_participant;

  dds_security_crypto_key_factory_register_matched_remote_participant register_matched_remote_participant;

  dds_security_crypto_key_factory_register_local_datawriter register_local_datawriter;

  dds_security_crypto_key_factory_register_matched_remote_datareader register_matched_remote_datareader;

  dds_security_crypto_key_factory_register_local_datareader register_local_datareader;

  dds_security_crypto_key_factory_register_matched_remote_datawriter register_matched_remote_datawriter;

  dds_security_crypto_key_factory_unregister_participant unregister_participant;

  dds_security_crypto_key_factory_unregister_datawriter unregister_datawriter;

  dds_security_crypto_key_factory_unregister_datareader unregister_datareader;
} dds_security_crypto_key_factory;

#define dds_security_crypto_key_factory__alloc() \
((dds_security_crypto_key_factory*) dds_alloc (sizeof (dds_security_crypto_key_factory)));

/**
 * CryptoKeyExchange Interface
 */
typedef bool
(*dds_security_crypto_key_exchange_create_local_participant_crypto_tokens)
        (void *listener_data,
         _Inout_ DDS_Security_ParticipantCryptoTokenSeq *local_participant_crypto_tokens,
         _In_ const DDS_Security_ParticipantCryptoHandle local_participant_crypto,
         _In_ const DDS_Security_ParticipantCryptoHandle remote_participant_crypto,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_crypto_key_exchange_set_remote_participant_crypto_tokens)
        (void *listener_data,
         _In_ const DDS_Security_ParticipantCryptoHandle local_participant_crypto,
         _In_ const DDS_Security_ParticipantCryptoHandle remote_participant_crypto,
         _In_ const DDS_Security_ParticipantCryptoTokenSeq *remote_participant_tokens,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_crypto_key_exchange_create_local_datawriter_crypto_tokens)
        (void *listener_data,
         _Inout_ DDS_Security_DatawriterCryptoTokenSeq *local_datawriter_crypto_tokens,
         _In_ const DDS_Security_DatawriterCryptoHandle local_datawriter_crypto,
         _In_ const DDS_Security_DatareaderCryptoHandle remote_datareader_crypto,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_crypto_key_exchange_set_remote_datawriter_crypto_tokens)
        (void *listener_data,
         _In_ const DDS_Security_DatareaderCryptoHandle local_datareader_crypto,
         _In_ const DDS_Security_DatawriterCryptoHandle remote_datawriter_crypto,
         _In_ const DDS_Security_DatawriterCryptoTokenSeq *remote_datawriter_tokens,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_crypto_key_exchange_create_local_datareader_crypto_tokens)
        (void *listener_data,
         _Inout_ DDS_Security_DatareaderCryptoTokenSeq *local_datareader_cryto_tokens,
         _In_ const DDS_Security_DatareaderCryptoHandle local_datareader_crypto,
         _In_ const DDS_Security_DatawriterCryptoHandle remote_datawriter_crypto,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_crypto_key_exchange_set_remote_datareader_crypto_tokens)
        (void *listener_data,
         _In_ const DDS_Security_DatawriterCryptoHandle local_datawriter_crypto,
         _In_ const DDS_Security_DatareaderCryptoHandle remote_datareader_crypto,
         _In_ const DDS_Security_DatareaderCryptoTokenSeq *remote_datareader_tokens,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_crypto_key_exchange_return_crypto_tokens)
        (void *listener_data,
         _In_ const DDS_Security_CryptoTokenSeq *crypto_tokens,
         _Inout_ DDS_Security_SecurityException *ex);

typedef struct dds_security_crypto_key_exchange
{
  dds_security_crypto_key_exchange_create_local_participant_crypto_tokens create_local_participant_crypto_tokens;

  dds_security_crypto_key_exchange_set_remote_participant_crypto_tokens set_remote_participant_crypto_tokens;

  dds_security_crypto_key_exchange_create_local_datawriter_crypto_tokens create_local_datawriter_crypto_tokens;

  dds_security_crypto_key_exchange_set_remote_datawriter_crypto_tokens set_remote_datawriter_crypto_tokens;

  dds_security_crypto_key_exchange_create_local_datareader_crypto_tokens create_local_datareader_crypto_tokens;

  dds_security_crypto_key_exchange_set_remote_datareader_crypto_tokens set_remote_datareader_crypto_tokens;

  dds_security_crypto_key_exchange_return_crypto_tokens return_crypto_tokens;
} dds_security_crypto_key_exchange;

#define dds_security_crypto_key_exchange__alloc() \
((dds_security_crypto_key_exchange*) dds_alloc (sizeof (dds_security_crypto_key_exchange)));

/**
 * CryptoTransform Interface
 */

typedef bool
(*dds_security_crypto_transform_encode_serialized_payload)
        (void *listener_data,
         _Inout_ DDS_OctetSeq *encoded_buffer,
         _Inout_ DDS_OctetSeq *extra_inline_qos,
         _In_ const DDS_OctetSeq *plain_buffer,
         _In_ const DDS_Security_DatawriterCryptoHandle sending_datawriter_crypto,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_crypto_transform_encode_datawriter_submessage)
        (void *listener_data,
         _Inout_ DDS_OctetSeq *encoded_rtps_submessage,
         _In_ const DDS_OctetSeq *plain_rtps_submessage,
         _In_ const DDS_Security_DatawriterCryptoHandle sending_datawriter_crypto,
         _In_ const DDS_Security_DatareaderCryptoHandleSeq *receiving_datareader_crypto_list,
         _Inout_ int32_t *receiving_datareader_crypto_list_index,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_crypto_transform_encode_datareader_submessage)
        (void *listener_data,
         _Inout_ DDS_OctetSeq *encoded_rtps_submessage,
         _In_ const DDS_OctetSeq *plain_rtps_submessage,
         _In_ const DDS_Security_DatareaderCryptoHandle sending_datareader_crypto,
         _In_ const DDS_Security_DatawriterCryptoHandleSeq *receiving_datawriter_crypto_list,
         _Inout_ DDS_Security_SecurityException *ex);


typedef bool
(*dds_security_crypto_transform_encode_rtps_message)
        (void *listener_data,
         _Inout_ DDS_OctetSeq *encoded_rtps_message,
         _In_ const DDS_OctetSeq *plain_rtps_message,
         _In_ const DDS_Security_ParticipantCryptoHandle sending_participant_crypto,
         _In_ const DDS_Security_ParticipantCryptoHandleSeq *receiving_participant_crypto_list,
         _Inout_ int32_t *receiving_participant_crypto_list_index,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_crypto_transform_decode_rtps_message)
        (void *listener_data,
         _Inout_ DDS_OctetSeq *plain_buffer,
         _In_ const DDS_OctetSeq *encoded_buffer,
         _In_ const DDS_Security_ParticipantCryptoHandle receiving_participant_crypto,
         _In_ const DDS_Security_ParticipantCryptoHandle sending_participant_crypto,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_crypto_transform_preprocess_secure_submsg)
        (void *listener_data,
         _Inout_ DDS_Security_DatawriterCryptoHandle *datawriter_crypto,
         _Inout_ DDS_Security_DatareaderCryptoHandle *datareader_crypto,
         _Inout_ DDS_Security_SecureSubmessageCategory_t *secure_submessage_category,
         _In_ const DDS_OctetSeq *encoded_rtps_submessage,
         _In_ const DDS_Security_ParticipantCryptoHandle receiving_participant_crypto,
         _In_ const DDS_Security_ParticipantCryptoHandle sending_participant_crypto,
         _Inout_ DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_crypto_transform_decode_datawriter_submessage)
        (void *listener_data,
         _Inout_ DDS_OctetSeq *plain_rtps_submessage,
         _In_ const DDS_OctetSeq *encoded_rtps_submessage,
         _In_ const DDS_Security_DatareaderCryptoHandle receiving_datareader_crypto,
         _In_ const DDS_Security_DatawriterCryptoHandle sending_datawriter_crypto,
         _In_ const DDS_Security_SecurityException *ex);

typedef bool
(*dds_security_crypto_transform_decode_datareader_submessage)
        (void *listener_data,
         _Inout_ DDS_OctetSeq *plain_rtps_message,
         _In_ const DDS_OctetSeq *encoded_rtps_message,
         _In_ const DDS_Security_DatawriterCryptoHandle receiving_datawriter_crypto,
         _In_ const DDS_Security_DatareaderCryptoHandle sending_datareader_crypto,
         _Inout_ DDS_Security_SecurityException *ex);


typedef bool
(*dds_security_crypto_transform_decode_serialized_payload)
        (void *listener_data,
         _Inout_ DDS_OctetSeq *plain_buffer,
         _In_ const DDS_OctetSeq *encoded_buffer,
         _In_ const DDS_OctetSeq *inline_qos,
         _In_ const DDS_Security_DatareaderCryptoHandle receiving_datareader_crypto,
         _In_ const DDS_Security_DatawriterCryptoHandle sending_datawriter_crypto,
         _Inout_ DDS_Security_SecurityException *ex);


typedef struct dds_security_crypto_transform
{
  dds_security_crypto_transform_encode_serialized_payload encode_serialized_payload;

  dds_security_crypto_transform_encode_datawriter_submessage encode_datawriter_submessage;

  dds_security_crypto_transform_encode_datareader_submessage encode_datareader_submessage;

  dds_security_crypto_transform_encode_rtps_message encode_rtps_message;

  dds_security_crypto_transform_decode_rtps_message decode_rtps_message;

  dds_security_crypto_transform_preprocess_secure_submsg preprocess_secure_submsg;

  dds_security_crypto_transform_decode_datawriter_submessage decode_datawriter_submessage;

  dds_security_crypto_transform_decode_datareader_submessage decode_datareader_submessage;

  dds_security_crypto_transform_decode_serialized_payload decode_serialized_payload;
} dds_security_crypto_transform;

#define dds_security_crypto_key_exchange__alloc() \
((dds_security_crypto_key_exchange*) dds_alloc (sizeof (dds_security_crypto_key_exchange)));

#endif //DDSC_SECURITY_H
