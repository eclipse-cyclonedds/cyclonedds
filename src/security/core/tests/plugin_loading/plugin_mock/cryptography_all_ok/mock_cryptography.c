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
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "mock_cryptography.h"
/**
 * Implementation structure for storing encapsulated members of the instance
 * while giving only the interface definition to user
 */

typedef struct dds_security_cryptography_impl {
  dds_security_cryptography base;
  int member;
} dds_security_cryptography_impl;


dds_security_crypto_key_exchange* dds_security_crypto_key_exchange__alloc(void);
void dds_security_crypto_key_exchange__dealloc(
    dds_security_crypto_key_exchange* instance);

dds_security_crypto_key_factory* dds_security_crypto_key_factory__alloc(void);
void dds_security_crypto_key_factory__dealloc(
    dds_security_crypto_key_factory* instance);

/**
 * CryptoTransform Interface
 */

/*
 * Allocation function for implementer structure (with internal variables) transparently.
 *
 */

dds_security_crypto_transform* dds_security_crypto_transform__alloc(void);
void dds_security_crypto_transform__dealloc(
    dds_security_crypto_transform* instance);


int32_t init_crypto(  const char *argument, void **context)
{

  dds_security_cryptography_impl *cryptography;

  dds_security_crypto_key_exchange *crypto_key_exchange;
  dds_security_crypto_key_factory *crypto_key_factory;
  dds_security_crypto_transform *crypto_transform;


  DDSRT_UNUSED_ARG(argument);

  //allocate new instance
  cryptography = (dds_security_cryptography_impl*) ddsrt_malloc(
      sizeof(dds_security_cryptography_impl));

  //assign the sub components
  crypto_key_exchange = dds_security_crypto_key_exchange__alloc();
  crypto_key_factory = dds_security_crypto_key_factory__alloc();
  crypto_transform = dds_security_crypto_transform__alloc();


  cryptography->base.crypto_key_exchange = crypto_key_exchange;
  cryptography->base.crypto_key_factory = crypto_key_factory;
  cryptography->base.crypto_transform = crypto_transform;

  //return the instance
  *context = cryptography;
  return 0;
}

int32_t finalize_crypto( void *instance)
{

  dds_security_cryptography_impl* instance_impl =
      (dds_security_cryptography_impl*) instance;

  //deallocate components
  dds_security_crypto_key_exchange__dealloc(
      instance_impl->base.crypto_key_exchange);
  dds_security_crypto_key_factory__dealloc(
      instance_impl->base.crypto_key_factory);
  dds_security_crypto_transform__dealloc(instance_impl->base.crypto_transform);
  //deallocate cryptography
  ddsrt_free(instance_impl);

  return 0;
}






/**
 * Implementation structure for storing encapsulated members of the instance
 * while giving only the interface definition to user
 */

typedef struct dds_security_crypto_key_exchange_impl {
  dds_security_crypto_key_exchange base;
  int member;
} dds_security_crypto_key_exchange_impl;

/**
 * Function implementations
 */
static DDS_Security_boolean create_local_participant_crypto_tokens(
     dds_security_crypto_key_exchange *instance,
    DDS_Security_ParticipantCryptoTokenSeq *local_participant_crypto_tokens,
     const DDS_Security_ParticipantCryptoHandle local_participant_crypto,
     const DDS_Security_ParticipantCryptoHandle remote_participant_crypto,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(local_participant_crypto_tokens);
  DDSRT_UNUSED_ARG(local_participant_crypto);
  DDSRT_UNUSED_ARG(remote_participant_crypto);
  DDSRT_UNUSED_ARG(ex);
  return true;

}

static DDS_Security_boolean set_remote_participant_crypto_tokens(
     dds_security_crypto_key_exchange *instance,
     const DDS_Security_ParticipantCryptoHandle local_participant_crypto,
     const DDS_Security_ParticipantCryptoHandle remote_participant_crypto,
     const DDS_Security_ParticipantCryptoTokenSeq *remote_participant_tokens,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(local_participant_crypto);
  DDSRT_UNUSED_ARG(remote_participant_crypto);
  DDSRT_UNUSED_ARG(remote_participant_tokens);
  DDSRT_UNUSED_ARG(ex);

  return true;
}

static DDS_Security_boolean create_local_datawriter_crypto_tokens(
     dds_security_crypto_key_exchange *instance,
    DDS_Security_DatawriterCryptoTokenSeq *local_datawriter_crypto_tokens,
     const DDS_Security_DatawriterCryptoHandle local_datawriter_crypto,
     const DDS_Security_DatareaderCryptoHandle remote_datareader_crypto,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(local_datawriter_crypto_tokens);
  DDSRT_UNUSED_ARG(local_datawriter_crypto);
  DDSRT_UNUSED_ARG(remote_datareader_crypto);
  DDSRT_UNUSED_ARG(ex);

  return true;
}

static DDS_Security_boolean set_remote_datawriter_crypto_tokens(
     dds_security_crypto_key_exchange *instance,
     const DDS_Security_DatareaderCryptoHandle local_datareader_crypto,
     const DDS_Security_DatawriterCryptoHandle remote_datawriter_crypto,
     const DDS_Security_DatawriterCryptoTokenSeq *remote_datawriter_tokens,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(local_datareader_crypto);
  DDSRT_UNUSED_ARG(remote_datawriter_crypto);
  DDSRT_UNUSED_ARG(remote_datawriter_tokens);
  DDSRT_UNUSED_ARG(ex);

  return true;
}

static DDS_Security_boolean create_local_datareader_crypto_tokens(
     dds_security_crypto_key_exchange *instance,
    DDS_Security_DatareaderCryptoTokenSeq *local_datareader_cryto_tokens,
     const DDS_Security_DatareaderCryptoHandle local_datareader_crypto,
     const DDS_Security_DatawriterCryptoHandle remote_datawriter_crypto,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(local_datareader_cryto_tokens);
  DDSRT_UNUSED_ARG(local_datareader_crypto);
  DDSRT_UNUSED_ARG(remote_datawriter_crypto);
  DDSRT_UNUSED_ARG(ex);

  return true;
}

static DDS_Security_boolean set_remote_datareader_crypto_tokens(
     dds_security_crypto_key_exchange *instance,
     const DDS_Security_DatawriterCryptoHandle local_datawriter_crypto,
     const DDS_Security_DatareaderCryptoHandle remote_datareader_crypto,
     const DDS_Security_DatareaderCryptoTokenSeq *remote_datareader_tokens,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(local_datawriter_crypto);
  DDSRT_UNUSED_ARG(remote_datareader_crypto);
  DDSRT_UNUSED_ARG(remote_datareader_tokens);
  DDSRT_UNUSED_ARG(ex);

  return true;
}

static DDS_Security_boolean return_crypto_tokens(
     dds_security_crypto_key_exchange *instance,
     DDS_Security_CryptoTokenSeq *crypto_tokens,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(crypto_tokens);
  DDSRT_UNUSED_ARG(ex);

  return true;
}

dds_security_crypto_key_exchange* dds_security_crypto_key_exchange__alloc(void)
{
  dds_security_crypto_key_exchange_impl *instance;
  instance = (dds_security_crypto_key_exchange_impl*) ddsrt_malloc(
      sizeof(dds_security_crypto_key_exchange_impl));

  instance->base.create_local_participant_crypto_tokens =
      &create_local_participant_crypto_tokens;

  instance->base.set_remote_participant_crypto_tokens =
      &set_remote_participant_crypto_tokens;

  instance->base.create_local_datawriter_crypto_tokens =
      &create_local_datawriter_crypto_tokens;

  instance->base.set_remote_datawriter_crypto_tokens =
      &set_remote_datawriter_crypto_tokens;

  instance->base.create_local_datareader_crypto_tokens =
      &create_local_datareader_crypto_tokens;

  instance->base.set_remote_datareader_crypto_tokens =
      &set_remote_datareader_crypto_tokens;

  instance->base.return_crypto_tokens = &return_crypto_tokens;

  return (dds_security_crypto_key_exchange*) instance;
}

void dds_security_crypto_key_exchange__dealloc(
    dds_security_crypto_key_exchange* instance)
{

  ddsrt_free((dds_security_crypto_key_exchange_impl*) instance);
}



/**
 * Implementation structure for storing encapsulated members of the instance
 * while giving only the interface definition to user
 */

typedef struct dds_security_crypto_key_factory_impl {
  dds_security_crypto_key_factory base;
  int member;
} dds_security_crypto_key_factory_impl;

/**
 * Function implementations
 */

static DDS_Security_ParticipantCryptoHandle register_local_participant(
     dds_security_crypto_key_factory *instance,
     const DDS_Security_IdentityHandle participant_identity,
     const DDS_Security_PermissionsHandle participant_permissions,
     const DDS_Security_PropertySeq *participant_properties,
     const DDS_Security_ParticipantSecurityAttributes *participant_security_attributes,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(participant_identity);
  DDSRT_UNUSED_ARG(participant_permissions);
  DDSRT_UNUSED_ARG(participant_properties);
  DDSRT_UNUSED_ARG(participant_security_attributes);
  DDSRT_UNUSED_ARG(ex);
  return 0;
}

static DDS_Security_ParticipantCryptoHandle register_matched_remote_participant(
     dds_security_crypto_key_factory *instance,
     const DDS_Security_ParticipantCryptoHandle local_participant_crypto_handle,
     const DDS_Security_IdentityHandle remote_participant_identity,
     const DDS_Security_PermissionsHandle remote_participant_permissions,
     const DDS_Security_SharedSecretHandle shared_secret,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(local_participant_crypto_handle);
  DDSRT_UNUSED_ARG(remote_participant_identity);
  DDSRT_UNUSED_ARG(remote_participant_permissions);
  DDSRT_UNUSED_ARG(shared_secret);
  DDSRT_UNUSED_ARG(ex);
  return 0;
}

static DDS_Security_DatawriterCryptoHandle register_local_datawriter(
     dds_security_crypto_key_factory *instance,
     const DDS_Security_ParticipantCryptoHandle participant_crypto,
     const DDS_Security_PropertySeq *datawriter_properties,
     const DDS_Security_EndpointSecurityAttributes *datawriter_security_attributes,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(participant_crypto);
  DDSRT_UNUSED_ARG(datawriter_properties);
  DDSRT_UNUSED_ARG(datawriter_security_attributes);
  DDSRT_UNUSED_ARG(ex);
  return 0;
}

static DDS_Security_DatareaderCryptoHandle register_matched_remote_datareader(
     dds_security_crypto_key_factory *instance,
     const DDS_Security_DatawriterCryptoHandle local_datawritert_crypto_handle,
     const DDS_Security_ParticipantCryptoHandle remote_participant_crypto,
     const DDS_Security_SharedSecretHandle shared_secret,
     const DDS_Security_boolean relay_only,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(local_datawritert_crypto_handle);
  DDSRT_UNUSED_ARG(remote_participant_crypto);
  DDSRT_UNUSED_ARG(shared_secret);
  DDSRT_UNUSED_ARG(relay_only);
  DDSRT_UNUSED_ARG(ex);
  return 0;
}

static DDS_Security_DatareaderCryptoHandle register_local_datareader(
     dds_security_crypto_key_factory *instance,
     const DDS_Security_ParticipantCryptoHandle participant_crypto,
     const DDS_Security_PropertySeq *datareader_properties,
     const DDS_Security_EndpointSecurityAttributes *datareader_security_attributes,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(participant_crypto);
  DDSRT_UNUSED_ARG(datareader_properties);
  DDSRT_UNUSED_ARG(datareader_security_attributes);
  DDSRT_UNUSED_ARG(ex);

  return 0;
}

static DDS_Security_DatawriterCryptoHandle register_matched_remote_datawriter(
     dds_security_crypto_key_factory *instance,
     const DDS_Security_DatareaderCryptoHandle local_datareader_crypto_handle,
     const DDS_Security_ParticipantCryptoHandle remote_participant_crypt,
     const DDS_Security_SharedSecretHandle shared_secret,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(local_datareader_crypto_handle);
  DDSRT_UNUSED_ARG(remote_participant_crypt);
  DDSRT_UNUSED_ARG(shared_secret);
  DDSRT_UNUSED_ARG(ex);
  return true;
}

static DDS_Security_boolean unregister_participant(
     dds_security_crypto_key_factory *instance,
     const DDS_Security_ParticipantCryptoHandle participant_crypto_handle,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(participant_crypto_handle);
  DDSRT_UNUSED_ARG(ex);
  return true;
}

static DDS_Security_boolean unregister_datawriter(
     dds_security_crypto_key_factory *instance,
     const DDS_Security_DatawriterCryptoHandle datawriter_crypto_handle,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(datawriter_crypto_handle);
  DDSRT_UNUSED_ARG(ex);
  return true;
}

static DDS_Security_boolean unregister_datareader(
     dds_security_crypto_key_factory *instance,
     const DDS_Security_DatareaderCryptoHandle datareader_crypto_handle,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(datareader_crypto_handle);
  DDSRT_UNUSED_ARG(ex);
  return true;
}

dds_security_crypto_key_factory* dds_security_crypto_key_factory__alloc(void)
{
  dds_security_crypto_key_factory_impl *instance;
  instance = (dds_security_crypto_key_factory_impl*) ddsrt_malloc(
      sizeof(dds_security_crypto_key_factory_impl));

  instance->base.register_local_participant = &register_local_participant;

  instance->base.register_matched_remote_participant =
      &register_matched_remote_participant;

  instance->base.register_local_datawriter = &register_local_datawriter;

  instance->base.register_matched_remote_datareader =
      &register_matched_remote_datareader;

  instance->base.register_local_datareader = &register_local_datareader;

  instance->base.register_matched_remote_datawriter =
      &register_matched_remote_datawriter;

  instance->base.unregister_participant = &unregister_participant;

  instance->base.unregister_datawriter = &unregister_datawriter;

  instance->base.unregister_datareader = &unregister_datareader;

  return (dds_security_crypto_key_factory*) instance;
}

void dds_security_crypto_key_factory__dealloc(
    dds_security_crypto_key_factory* instance)
{

  ddsrt_free((dds_security_crypto_key_factory_impl*) instance);
}



/**
 * Implementation structure for storing encapsulated members of the instance
 * while giving only the interface definition to user
 */

typedef struct dds_security_crypto_transform_impl {
  dds_security_crypto_transform base;
  int member;
} dds_security_crypto_transform_impl;

/**
 * Function implementations
 */
static DDS_Security_boolean encode_serialized_payload(
     dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *encoded_buffer,
    DDS_Security_OctetSeq *extra_inline_qos,
     const DDS_Security_OctetSeq *plain_buffer,
     const DDS_Security_DatawriterCryptoHandle sending_datawriter_crypto,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(encoded_buffer);
  DDSRT_UNUSED_ARG(extra_inline_qos);
  DDSRT_UNUSED_ARG(plain_buffer);
  DDSRT_UNUSED_ARG(sending_datawriter_crypto);
  DDSRT_UNUSED_ARG(ex);

  return true;
}

static DDS_Security_boolean encode_datawriter_submessage(
     dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *encoded_rtps_submessage,
     const DDS_Security_OctetSeq *plain_rtps_submessage,
     const DDS_Security_DatawriterCryptoHandle sending_datawriter_crypto,
     const DDS_Security_DatareaderCryptoHandleSeq *receiving_datareader_crypto_list,
    int32_t *receiving_datareader_crypto_list_index,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(encoded_rtps_submessage);
  DDSRT_UNUSED_ARG(plain_rtps_submessage);
  DDSRT_UNUSED_ARG(sending_datawriter_crypto);
  DDSRT_UNUSED_ARG(receiving_datareader_crypto_list);
  DDSRT_UNUSED_ARG(receiving_datareader_crypto_list_index);
  DDSRT_UNUSED_ARG(ex);

  return true;
}

static DDS_Security_boolean encode_datareader_submessage(
     dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *encoded_rtps_submessage,
     const DDS_Security_OctetSeq *plain_rtps_submessage,
     const DDS_Security_DatareaderCryptoHandle sending_datareader_crypto,
     const DDS_Security_DatawriterCryptoHandleSeq *receiving_datawriter_crypto_list,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(encoded_rtps_submessage);
  DDSRT_UNUSED_ARG(plain_rtps_submessage);
  DDSRT_UNUSED_ARG(sending_datareader_crypto);
  DDSRT_UNUSED_ARG(receiving_datawriter_crypto_list);
  DDSRT_UNUSED_ARG(ex);

  return true;
}

static DDS_Security_boolean encode_rtps_message(  dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *encoded_rtps_message,
     const DDS_Security_OctetSeq *plain_rtps_message,
     const DDS_Security_ParticipantCryptoHandle sending_participant_crypto,
     const DDS_Security_ParticipantCryptoHandleSeq *receiving_participant_crypto_list,
    int32_t *receiving_participant_crypto_list_index,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(encoded_rtps_message);
  DDSRT_UNUSED_ARG(plain_rtps_message);
  DDSRT_UNUSED_ARG(sending_participant_crypto);
  DDSRT_UNUSED_ARG(receiving_participant_crypto_list);
  DDSRT_UNUSED_ARG(receiving_participant_crypto_list_index);
  DDSRT_UNUSED_ARG(ex);

  return true;
}

static DDS_Security_boolean decode_rtps_message(  dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *plain_buffer,  const DDS_Security_OctetSeq *encoded_buffer,
     const DDS_Security_ParticipantCryptoHandle receiving_participant_crypto,
     const DDS_Security_ParticipantCryptoHandle sending_participant_crypto,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(plain_buffer);
  DDSRT_UNUSED_ARG(encoded_buffer);
  DDSRT_UNUSED_ARG(receiving_participant_crypto);
  DDSRT_UNUSED_ARG(sending_participant_crypto);
  DDSRT_UNUSED_ARG(ex);

  return true;
}


static DDS_Security_boolean preprocess_secure_submsg(
     dds_security_crypto_transform *instance,
    DDS_Security_DatawriterCryptoHandle *datawriter_crypto,
    DDS_Security_DatareaderCryptoHandle *datareader_crypto,
    DDS_Security_SecureSubmessageCategory_t *secure_submessage_category,
     const DDS_Security_OctetSeq *encoded_rtps_submessage,
     const DDS_Security_ParticipantCryptoHandle receiving_participant_crypto,
     const DDS_Security_ParticipantCryptoHandle sending_participant_crypto,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(datawriter_crypto);
  DDSRT_UNUSED_ARG(datareader_crypto);
  DDSRT_UNUSED_ARG(secure_submessage_category);
  DDSRT_UNUSED_ARG(encoded_rtps_submessage);
  DDSRT_UNUSED_ARG(receiving_participant_crypto);
  DDSRT_UNUSED_ARG(sending_participant_crypto);
  DDSRT_UNUSED_ARG(ex);

  return true;
}

static DDS_Security_boolean decode_datawriter_submessage(
     dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *plain_rtps_submessage,
     const DDS_Security_OctetSeq *encoded_rtps_submessage,
     const DDS_Security_DatareaderCryptoHandle receiving_datareader_crypto,
     const DDS_Security_DatawriterCryptoHandle sending_datawriter_crypto,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(plain_rtps_submessage);
  DDSRT_UNUSED_ARG(encoded_rtps_submessage);
  DDSRT_UNUSED_ARG(receiving_datareader_crypto);
  DDSRT_UNUSED_ARG(sending_datawriter_crypto);
  DDSRT_UNUSED_ARG(ex);

  return true;
}

static DDS_Security_boolean decode_datareader_submessage(
     dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *plain_rtps_message,
     const DDS_Security_OctetSeq *encoded_rtps_message,
     const DDS_Security_DatawriterCryptoHandle receiving_datawriter_crypto,
     const DDS_Security_DatareaderCryptoHandle sending_datareader_crypto,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(plain_rtps_message);
  DDSRT_UNUSED_ARG(encoded_rtps_message);
  DDSRT_UNUSED_ARG(receiving_datawriter_crypto);
  DDSRT_UNUSED_ARG(sending_datareader_crypto);
  DDSRT_UNUSED_ARG(ex);

  return true;
}

static DDS_Security_boolean decode_serialized_payload(
     dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *plain_buffer,  const DDS_Security_OctetSeq *encoded_buffer,
     const DDS_Security_OctetSeq *inline_qos,
     const DDS_Security_DatareaderCryptoHandle receiving_datareader_crypto,
     const DDS_Security_DatawriterCryptoHandle sending_datawriter_crypto,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(instance);
  DDSRT_UNUSED_ARG(plain_buffer);
  DDSRT_UNUSED_ARG(encoded_buffer);
  DDSRT_UNUSED_ARG(inline_qos);
  DDSRT_UNUSED_ARG(receiving_datareader_crypto);
  DDSRT_UNUSED_ARG(sending_datawriter_crypto);
  DDSRT_UNUSED_ARG(ex);

  return true;
}

dds_security_crypto_transform* dds_security_crypto_transform__alloc(void)
{
  dds_security_crypto_transform_impl *instance;
  instance = (dds_security_crypto_transform_impl*) ddsrt_malloc(
      sizeof(dds_security_crypto_transform_impl));

  memset( instance, 0, sizeof(dds_security_crypto_transform_impl));

  instance->base.encode_datawriter_submessage = &encode_datawriter_submessage;

  instance->base.encode_datareader_submessage = &encode_datareader_submessage;

  instance->base.encode_rtps_message = &encode_rtps_message;

  instance->base.decode_rtps_message = &decode_rtps_message;

  instance->base.preprocess_secure_submsg = &preprocess_secure_submsg;

  instance->base.decode_datawriter_submessage = &decode_datawriter_submessage;

  instance->base.decode_datareader_submessage = &decode_datareader_submessage;

  instance->base.decode_serialized_payload = &decode_serialized_payload;

  instance->base.encode_serialized_payload = &encode_serialized_payload;

  return (dds_security_crypto_transform*) instance;
}

void dds_security_crypto_transform__dealloc(
    dds_security_crypto_transform* instance)
{

  ddsrt_free((dds_security_crypto_transform_impl*) instance);
}


