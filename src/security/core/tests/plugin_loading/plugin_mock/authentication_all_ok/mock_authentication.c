/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include "dds/security/dds_security_api.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/misc.h"
#include "mock_authentication.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/**
 * Implementation structure for storing encapsulated members of the instance
 * while giving only the interface definition to user
 */

typedef struct dds_security_authentication_impl {
  dds_security_authentication base;
  int id; //sample internal member
} dds_security_authentication_impl;

DDS_Security_ValidationResult_t validate_local_identity(
    dds_security_authentication *instance,
     DDS_Security_IdentityHandle *local_identity_handle,
     DDS_Security_GUID_t *adjusted_participant_guid,
    const DDS_Security_DomainId domain_id,
    const DDS_Security_Qos *participant_qos,
    const DDS_Security_GUID_t *candidate_participant_guid,
     DDS_Security_SecurityException *ex)
{

  unsigned i;
  DDS_Security_ValidationResult_t result=DDS_SECURITY_VALIDATION_OK;
  dds_security_authentication_impl *implementation =
      (dds_security_authentication_impl *) instance;
  char *identity_ca = NULL;
  char *identity_certificate = NULL;
  char *private_key = NULL;

  DDSRT_UNUSED_ARG(local_identity_handle);
  DDSRT_UNUSED_ARG(adjusted_participant_guid);
  DDSRT_UNUSED_ARG(domain_id);
  DDSRT_UNUSED_ARG(participant_qos);
  DDSRT_UNUSED_ARG(candidate_participant_guid);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);

  implementation->id = 2;

  memcpy(adjusted_participant_guid, candidate_participant_guid, sizeof(DDS_Security_GUID_t));

  for( i=0; i< participant_qos->property.value._length; i++)
  {

    //printf("%s: %s",participant_qos->property.value._buffer[i].name, participant_qos->property.value._buffer[i].value);
    printf("%s\n",participant_qos->property.value._buffer[i].name);
    if( strcmp(participant_qos->property.value._buffer[i].name, "dds.sec.auth.private_key") == 0)
    {
      private_key = participant_qos->property.value._buffer[i].value;
    } else if( strcmp(participant_qos->property.value._buffer[i].name, "dds.sec.auth.identity_ca") == 0)
    {
      identity_ca = participant_qos->property.value._buffer[i].value;
    } else if( strcmp(participant_qos->property.value._buffer[i].name, "dds.sec.auth.identity_certificate") == 0)
    {
      identity_certificate = participant_qos->property.value._buffer[i].value;
    }
  }

  if( strcmp(identity_certificate, test_identity_certificate) != 0){

    result = DDS_SECURITY_VALIDATION_FAILED;
    printf("FAILED: Could not get identity_certificate value properly\n");
  }
  else if( strcmp(identity_ca, test_ca_certificate) != 0){
    printf("FAILED: Could not get identity_ca value properly\n");
    result = DDS_SECURITY_VALIDATION_FAILED;
  }else if( strcmp(private_key, test_privatekey) != 0){
    printf("FAILED: Could not get private_key value properly\n");
    result = DDS_SECURITY_VALIDATION_FAILED;
  }

  if( result == DDS_SECURITY_VALIDATION_OK )
  {
    printf("DDS_SECURITY_VALIDATION_OK\n");
  }


  return result;
}

DDS_Security_boolean get_identity_token(dds_security_authentication *instance,
     DDS_Security_IdentityToken *identity_token,
    const DDS_Security_IdentityHandle handle,
     DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(identity_token);
  DDSRT_UNUSED_ARG(handle);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);

  memset(identity_token, 0, sizeof(*identity_token));

  return true;
}

DDS_Security_boolean get_identity_status_token(
    dds_security_authentication *instance,
     DDS_Security_IdentityStatusToken *identity_status_token,
    const DDS_Security_IdentityHandle handle,
     DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(identity_status_token);
  DDSRT_UNUSED_ARG(handle);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);

  return true;
}

DDS_Security_boolean set_permissions_credential_and_token(
    dds_security_authentication *instance,
    const DDS_Security_IdentityHandle handle,
    const DDS_Security_PermissionsCredentialToken *permissions_credential,
    const DDS_Security_PermissionsToken *permissions_token,
     DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(handle);
  DDSRT_UNUSED_ARG(permissions_credential);
  DDSRT_UNUSED_ARG(permissions_token);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);

  return true;
}

DDS_Security_ValidationResult_t validate_remote_identity(
    dds_security_authentication *instance,
     DDS_Security_IdentityHandle *remote_identity_handle,
     DDS_Security_AuthRequestMessageToken *local_auth_request_token,
    const DDS_Security_AuthRequestMessageToken *remote_auth_request_token,
    const DDS_Security_IdentityHandle local_identity_handle,
    const DDS_Security_IdentityToken *remote_identity_token,
    const DDS_Security_GUID_t *remote_participant_guid,
     DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(remote_identity_handle);
  DDSRT_UNUSED_ARG(local_auth_request_token);
  DDSRT_UNUSED_ARG(remote_auth_request_token);
  DDSRT_UNUSED_ARG(local_identity_handle);
  DDSRT_UNUSED_ARG(remote_identity_token);
  DDSRT_UNUSED_ARG(remote_participant_guid);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);

  return DDS_SECURITY_VALIDATION_OK;
}

DDS_Security_ValidationResult_t begin_handshake_request(
    dds_security_authentication *instance,
     DDS_Security_HandshakeHandle *handshake_handle,
     DDS_Security_HandshakeMessageToken *handshake_message,
    const DDS_Security_IdentityHandle initiator_identity_handle,
    const DDS_Security_IdentityHandle replier_identity_handle,
    const DDS_Security_OctetSeq *serialized_local_participant_data,
     DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(handshake_handle);
  DDSRT_UNUSED_ARG(handshake_message);
  DDSRT_UNUSED_ARG(initiator_identity_handle);
  DDSRT_UNUSED_ARG(replier_identity_handle);
  DDSRT_UNUSED_ARG(serialized_local_participant_data);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);

  return DDS_SECURITY_VALIDATION_OK;
}

DDS_Security_ValidationResult_t begin_handshake_reply(
    dds_security_authentication *instance,
     DDS_Security_HandshakeHandle *handshake_handle,
     DDS_Security_HandshakeMessageToken *handshake_message_out,
    const DDS_Security_HandshakeMessageToken *handshake_message_in,
    const DDS_Security_IdentityHandle initiator_identity_handle,
    const DDS_Security_IdentityHandle replier_identity_handle,
    const DDS_Security_OctetSeq *serialized_local_participant_data,
     DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(handshake_handle);
  DDSRT_UNUSED_ARG(handshake_message_out);
  DDSRT_UNUSED_ARG(handshake_message_in);
  DDSRT_UNUSED_ARG(initiator_identity_handle);
  DDSRT_UNUSED_ARG(replier_identity_handle);
  DDSRT_UNUSED_ARG(serialized_local_participant_data);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);

  return DDS_SECURITY_VALIDATION_OK;
}

DDS_Security_ValidationResult_t process_handshake(
    dds_security_authentication *instance,
     DDS_Security_HandshakeMessageToken *handshake_message_out,
    const DDS_Security_HandshakeMessageToken *handshake_message_in,
    const DDS_Security_HandshakeHandle handshake_handle,
     DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(handshake_message_out);
  DDSRT_UNUSED_ARG(handshake_message_in);
  DDSRT_UNUSED_ARG(handshake_handle);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);

  return DDS_SECURITY_VALIDATION_OK;
}

DDS_Security_SharedSecretHandle get_shared_secret(
    dds_security_authentication *instance,
    const DDS_Security_HandshakeHandle handshake_handle,
     DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(handshake_handle);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);

  return 0;
}

DDS_Security_boolean get_authenticated_peer_credential_token(
    dds_security_authentication *instance,
     DDS_Security_AuthenticatedPeerCredentialToken *peer_credential_token,
    const DDS_Security_HandshakeHandle handshake_handle,
     DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(peer_credential_token);
  DDSRT_UNUSED_ARG(handshake_handle);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);

  return true;
}

DDS_Security_boolean set_listener(dds_security_authentication *instance,
    const dds_security_authentication_listener *listener,
     DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(listener);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);

  return true;
}

DDS_Security_boolean return_identity_token(dds_security_authentication *instance,
    const DDS_Security_IdentityToken *token,
     DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(token);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);

  return true;
}

DDS_Security_boolean return_identity_status_token(
    dds_security_authentication *instance,
    const DDS_Security_IdentityStatusToken *token,
     DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(token);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);

  return true;
}

DDS_Security_boolean return_authenticated_peer_credential_token(
    dds_security_authentication *instance,
    const DDS_Security_AuthenticatedPeerCredentialToken *peer_credential_token,
     DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(peer_credential_token);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);

  return true;
}

DDS_Security_boolean return_handshake_handle(dds_security_authentication *instance,
    const DDS_Security_HandshakeHandle handshake_handle,
     DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(handshake_handle);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);

  return true;
}

DDS_Security_boolean return_identity_handle(dds_security_authentication *instance,
    const DDS_Security_IdentityHandle identity_handle,
     DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(identity_handle);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);

  return true;
}

DDS_Security_boolean return_sharedsecret_handle(
    dds_security_authentication *instance,
    const DDS_Security_SharedSecretHandle sharedsecret_handle,
     DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(sharedsecret_handle);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);

  return true;
}

int32_t init_authentication( const char *argument,  void **context)
{

  dds_security_authentication_impl *authentication;

  DDSRT_UNUSED_ARG(argument);

  //allocate implementation wrapper
  authentication = (dds_security_authentication_impl*) ddsrt_malloc(
      sizeof(dds_security_authentication_impl));
  memset(authentication, 0, sizeof(dds_security_authentication_impl));

  //assign the interface functions
  authentication->base.validate_local_identity = &validate_local_identity;

  authentication->base.get_identity_token = &get_identity_token;

  authentication->base.get_identity_status_token = &get_identity_status_token;

  authentication->base.set_permissions_credential_and_token =
      &set_permissions_credential_and_token;

  authentication->base.validate_remote_identity = &validate_remote_identity;

  authentication->base.begin_handshake_request = &begin_handshake_request;

  authentication->base.begin_handshake_reply = &begin_handshake_reply;

  authentication->base.process_handshake = &process_handshake;

  authentication->base.get_shared_secret = &get_shared_secret;

  authentication->base.get_authenticated_peer_credential_token =
      &get_authenticated_peer_credential_token;

  authentication->base.set_listener = &set_listener;

  authentication->base.return_identity_token = &return_identity_token;

  authentication->base.return_identity_status_token =
      &return_identity_status_token;

  authentication->base.return_authenticated_peer_credential_token =
      &return_authenticated_peer_credential_token;

  authentication->base.return_handshake_handle = &return_handshake_handle;

  authentication->base.return_identity_handle = &return_identity_handle;

  authentication->base.return_sharedsecret_handle = &return_sharedsecret_handle;

  //prepare implementation wrapper
  authentication->id = 1;

  //return the instance
  *context = authentication;
  return 0;
}

int32_t finalize_authentication(void *instance)
{

  ddsrt_free((dds_security_authentication_impl*) instance);

  return 0;
}
