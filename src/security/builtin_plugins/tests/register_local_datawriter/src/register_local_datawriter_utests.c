// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/types.h"
#include "dds/ddsrt/environ.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_serialize.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/core/dds_security_shared_secret.h"
#include "dds/security/openssl_support.h"
#include "CUnit/CUnit.h"
#include "CUnit/Test.h"
#include "common/src/loader.h"
#include "common/src/crypto_helper.h"
#include "crypto_objects.h"

#if OPENSLL_VERSION_NUMBER >= 0x10002000L
#define AUTH_INCLUDE_EC
#endif

#define TEST_SHARED_SECRET_SIZE 32

static struct plugins_hdl *plugins = NULL;
static DDS_Security_SharedSecretHandle shared_secret_handle = DDS_SECURITY_HANDLE_NIL;
static dds_security_cryptography *crypto = NULL;
static DDS_Security_ParticipantCryptoHandle local_participant_crypto_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_ParticipantCryptoHandle remote_participant_crypto_handle = DDS_SECURITY_HANDLE_NIL;

static void prepare_participant_security_attributes(DDS_Security_ParticipantSecurityAttributes *attributes)
{
  memset(attributes, 0, sizeof(DDS_Security_ParticipantSecurityAttributes));
  attributes->allow_unauthenticated_participants = false;
  attributes->is_access_protected = false;
  attributes->is_discovery_protected = false;
  attributes->is_liveliness_protected = false;
  attributes->is_rtps_protected = true;
  attributes->plugin_participant_attributes = DDS_SECURITY_PARTICIPANT_ATTRIBUTES_FLAG_IS_VALID;
  attributes->plugin_participant_attributes |= DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_ENCRYPTED;
}

static void reset_exception(DDS_Security_SecurityException *ex)
{
  ex->code = 0;
  ex->minor_code = 0;
  ddsrt_free(ex->message);
  ex->message = NULL;
}

static void suite_register_local_datawriter_init(void)
{
  DDS_Security_IdentityHandle participant_identity = 5; //valid dummy value
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PropertySeq participant_properties;
  DDS_Security_PermissionsHandle remote_participant_permissions = 5; //valid dummy value
  DDS_Security_SharedSecretHandleImpl *shared_secret_handle_impl;
  DDS_Security_PermissionsHandle participant_permissions = 3; //valid dummy value
  DDS_Security_ParticipantSecurityAttributes participant_security_attributes;

  CU_ASSERT_FATAL ((plugins = load_plugins(
                        NULL    /* Access Control */,
                        NULL    /* Authentication */,
                        &crypto /* Cryptograpy    */,
                        NULL)) != NULL);

  /* prepare test shared secret handle */
  shared_secret_handle_impl = ddsrt_malloc(sizeof(DDS_Security_SharedSecretHandleImpl));
  shared_secret_handle_impl->shared_secret = ddsrt_malloc(TEST_SHARED_SECRET_SIZE * sizeof(DDS_Security_octet));
  shared_secret_handle_impl->shared_secret_size = TEST_SHARED_SECRET_SIZE;
  for (int i = 0; i < shared_secret_handle_impl->shared_secret_size; i++)
  {
    shared_secret_handle_impl->shared_secret[i] = (unsigned char)(i % 20);
  }
  for (int i = 0; i < 32; i++)
  {
    shared_secret_handle_impl->challenge1[i] = (unsigned char)(i % 15);
    shared_secret_handle_impl->challenge2[i] = (unsigned char)(i % 12);
  }
  shared_secret_handle = (DDS_Security_SharedSecretHandle)shared_secret_handle_impl;

  /* Check if we actually have the validate_local_identity() function. */
  CU_ASSERT_FATAL (crypto != NULL && crypto->crypto_key_factory != NULL && crypto->crypto_key_factory->register_local_participant != NULL);

  memset(&exception, 0, sizeof(DDS_Security_SecurityException));
  memset(&participant_properties, 0, sizeof(participant_properties));
  prepare_participant_security_attributes(&participant_security_attributes);
  local_participant_crypto_handle = crypto->crypto_key_factory->register_local_participant(
      crypto->crypto_key_factory,
      participant_identity,
      participant_permissions,
      &participant_properties,
      &participant_security_attributes,
      &exception);
  CU_ASSERT_FATAL (local_participant_crypto_handle != DDS_SECURITY_HANDLE_NIL);

  /* Now call the function. */
  remote_participant_crypto_handle = crypto->crypto_key_factory->register_matched_remote_participant(
      crypto->crypto_key_factory,
      local_participant_crypto_handle,
      participant_identity,
      remote_participant_permissions,
      shared_secret_handle,
      &exception);
  CU_ASSERT_FATAL (remote_participant_crypto_handle != DDS_SECURITY_HANDLE_NIL);
  ddsrt_free(shared_secret_handle_impl->shared_secret);
  ddsrt_free(shared_secret_handle_impl);
}

static void suite_register_local_datawriter_fini(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};

  crypto->crypto_key_factory->unregister_participant(crypto->crypto_key_factory, remote_participant_crypto_handle, &exception);
  reset_exception(&exception);

  crypto->crypto_key_factory->unregister_participant(crypto->crypto_key_factory, local_participant_crypto_handle, &exception);
  reset_exception(&exception);

  unload_plugins(plugins);
  shared_secret_handle = DDS_SECURITY_HANDLE_NIL;
  crypto = NULL;
  local_participant_crypto_handle = DDS_SECURITY_HANDLE_NIL;
  remote_participant_crypto_handle = DDS_SECURITY_HANDLE_NIL;
}

static void prepare_endpoint_security_attributes(DDS_Security_EndpointSecurityAttributes *attributes)
{
  memset(attributes, 0, sizeof(DDS_Security_EndpointSecurityAttributes));
  attributes->is_discovery_protected = true;
  attributes->is_submessage_protected = true;

  attributes->plugin_endpoint_attributes |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
}

CU_Test(ddssec_builtin_register_local_datawriter, happy_day, .init = suite_register_local_datawriter_init, .fini = suite_register_local_datawriter_fini)
{
  DDS_Security_DatawriterCryptoHandle result;

  /* Dummy (even un-initialized) data for now. */
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PropertySeq datawriter_properties;
  DDS_Security_EndpointSecurityAttributes datawriter_security_attributes;
  local_datawriter_crypto *writer_crypto;

  /* Check if we actually have the function. */
  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_factory != NULL);
  assert(crypto->crypto_key_factory != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_factory->register_local_datawriter != NULL);
  assert(crypto->crypto_key_factory->register_local_datawriter != 0);

  memset(&exception, 0, sizeof(DDS_Security_SecurityException));
  memset(&datawriter_properties, 0, sizeof(datawriter_properties));
  prepare_endpoint_security_attributes(&datawriter_security_attributes);

  datawriter_security_attributes.is_payload_protected = true;
  datawriter_security_attributes.plugin_endpoint_attributes |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_PAYLOAD_ENCRYPTED;

  /* Now call the function. */
  result = crypto->crypto_key_factory->register_local_datawriter(
      crypto->crypto_key_factory,
      local_participant_crypto_handle,
      &datawriter_properties,
      &datawriter_security_attributes,
      &exception);

  if (exception.code != 0)
    printf("register_local_datawriter: %s\n", exception.message ? exception.message : "Error message missing");

  /* A valid handle to be returned */
  CU_ASSERT(result != 0);
  CU_ASSERT(exception.code == DDS_SECURITY_ERR_OK_CODE);
  assert(result != 0); // for Clang's static analyzer

  /* NOTE: It would be better to check if the keys have been generated but there is no interface to get them from handle */
  writer_crypto = (local_datawriter_crypto *)result;

  CU_ASSERT_FATAL(writer_crypto->writer_key_material_message != NULL);
  CU_ASSERT_FATAL(writer_crypto->writer_key_material_payload != NULL);

  CU_ASSERT(master_salt_not_empty(writer_crypto->writer_key_material_message));
  CU_ASSERT(master_key_not_empty(writer_crypto->writer_key_material_message));

  CU_ASSERT(master_salt_not_empty(writer_crypto->writer_key_material_payload));
  CU_ASSERT(master_key_not_empty(writer_crypto->writer_key_material_payload));

  CU_ASSERT(writer_crypto->metadata_protectionKind == DDS_SECURITY_PROTECTION_KIND_ENCRYPT);
  CU_ASSERT(writer_crypto->data_protectionKind == DDS_SECURITY_BASICPROTECTION_KIND_ENCRYPT);

  reset_exception(&exception);
}

CU_Test(ddssec_builtin_register_local_datawriter, builtin_endpoint, .init = suite_register_local_datawriter_init, .fini = suite_register_local_datawriter_fini)
{
  DDS_Security_DatawriterCryptoHandle result;

  /* Dummy (even un-initialized) data for now. */
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PropertySeq datawriter_properties;
  DDS_Security_EndpointSecurityAttributes datawriter_security_attributes;
  local_datawriter_crypto *writer_crypto;

  memset(&exception, 0, sizeof(DDS_Security_SecurityException));
  memset(&datawriter_properties, 0, sizeof(datawriter_properties));

  prepare_endpoint_security_attributes(&datawriter_security_attributes);
  datawriter_security_attributes.is_payload_protected = true;
  datawriter_security_attributes.plugin_endpoint_attributes |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_PAYLOAD_ENCRYPTED;

  /* Check if we actually have the function. */
  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_factory != NULL);
  assert(crypto->crypto_key_factory != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_factory->register_local_datawriter != NULL);
  assert(crypto->crypto_key_factory->register_local_datawriter != 0);

  datawriter_properties._buffer = DDS_Security_PropertySeq_allocbuf(1);
  datawriter_properties._length = datawriter_properties._maximum = 1;
  datawriter_properties._buffer[0].name = ddsrt_strdup(DDS_SEC_PROP_BUILTIN_ENDPOINT_NAME);
  datawriter_properties._buffer[0].value = ddsrt_strdup("BuiltinSecureEndpointName");

  /* Now call the function. */
  result = crypto->crypto_key_factory->register_local_datawriter(
      crypto->crypto_key_factory,
      local_participant_crypto_handle,
      &datawriter_properties,
      &datawriter_security_attributes,
      &exception);

  if (exception.code != 0)
    printf("register_local_datawriter: %s\n", exception.message ? exception.message : "Error message missing");

  /* A valid handle to be returned */
  CU_ASSERT_FATAL(result != 0);
  CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_OK_CODE);
  assert(result != 0); // for Clang's static analyzer

  /* NOTE: It would be better to check if the keys have been generated but there is no interface to get them from handle */
  writer_crypto = (local_datawriter_crypto *)result;

  CU_ASSERT_FATAL(writer_crypto->writer_key_material_message != NULL);
  CU_ASSERT_FATAL(writer_crypto->writer_key_material_payload != NULL);

  CU_ASSERT(master_salt_not_empty(writer_crypto->writer_key_material_message));
  CU_ASSERT(master_key_not_empty(writer_crypto->writer_key_material_message));

  CU_ASSERT(master_salt_not_empty(writer_crypto->writer_key_material_payload));
  CU_ASSERT(master_key_not_empty(writer_crypto->writer_key_material_payload));

  CU_ASSERT_FATAL(writer_crypto->metadata_protectionKind == DDS_SECURITY_PROTECTION_KIND_ENCRYPT);
  CU_ASSERT_FATAL(writer_crypto->data_protectionKind == DDS_SECURITY_BASICPROTECTION_KIND_ENCRYPT);
  CU_ASSERT_FATAL(writer_crypto->is_builtin_participant_volatile_message_secure_writer == false);

  reset_exception(&exception);
  DDS_Security_PropertySeq_deinit(&datawriter_properties);
}

CU_Test(ddssec_builtin_register_local_datawriter, special_endpoint_name, .init = suite_register_local_datawriter_init, .fini = suite_register_local_datawriter_fini)
{
  DDS_Security_DatawriterCryptoHandle result;

  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PropertySeq datawriter_properties;
  DDS_Security_EndpointSecurityAttributes datawriter_security_attributes;

  memset(&exception, 0, sizeof(DDS_Security_SecurityException));
  memset(&datawriter_properties, 0, sizeof(datawriter_properties));
  memset(&datawriter_security_attributes, 0, sizeof(datawriter_security_attributes));
  prepare_endpoint_security_attributes(&datawriter_security_attributes);

  /* Check if we actually have the function. */
  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_factory != NULL);
  assert(crypto->crypto_key_factory != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_factory->register_local_datawriter != NULL);
  assert(crypto->crypto_key_factory->register_local_datawriter != 0);

  /*set special endpoint name*/
  datawriter_properties._buffer = DDS_Security_PropertySeq_allocbuf(1);
  datawriter_properties._length = datawriter_properties._maximum = 1;
  datawriter_properties._buffer[0].name = ddsrt_strdup(DDS_SEC_PROP_BUILTIN_ENDPOINT_NAME);
  datawriter_properties._buffer[0].value = ddsrt_strdup("BuiltinParticipantVolatileMessageSecureWriter");

  /* Now call the function. */
  result = crypto->crypto_key_factory->register_local_datawriter(
      crypto->crypto_key_factory,
      local_participant_crypto_handle,
      &datawriter_properties,
      &datawriter_security_attributes,
      &exception);

  if (exception.code != 0)
    printf("register_local_datawriter: %s\n", exception.message ? exception.message : "Error message missing");

  /* A valid handle to be returned */
  CU_ASSERT_FATAL(result != 0);
  CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_OK_CODE);
  assert(result != 0); // for Clang's static analyzer
  CU_ASSERT_FATAL(((local_datawriter_crypto *)result)->is_builtin_participant_volatile_message_secure_writer);

  reset_exception(&exception);
  DDS_Security_PropertySeq_deinit(&datawriter_properties);
}

CU_Test(ddssec_builtin_register_local_datawriter, invalid_participant, .init = suite_register_local_datawriter_init, .fini = suite_register_local_datawriter_fini)
{
  DDS_Security_DatawriterCryptoHandle result;

  /* Dummy (even un-initialized) data for now. */
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PropertySeq datawriter_properties;
  DDS_Security_EndpointSecurityAttributes datawriter_security_attributes;

  /* Check if we actually have the function. */
  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_factory != NULL);
  assert(crypto->crypto_key_factory != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_factory->register_local_datawriter != NULL);
  assert(crypto->crypto_key_factory->register_local_datawriter != 0);

  memset(&exception, 0, sizeof(DDS_Security_SecurityException));
  memset(&datawriter_properties, 0, sizeof(datawriter_properties));
  prepare_endpoint_security_attributes(&datawriter_security_attributes);

  /* Now call the function. */
  result = crypto->crypto_key_factory->register_local_datawriter(
      crypto->crypto_key_factory,
      8, /*non existing handle*/
      &datawriter_properties,
      &datawriter_security_attributes,
      &exception);

  /* Invalid handle should be returned */
  CU_ASSERT(result == 0);

  CU_ASSERT_FATAL(exception.code == DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_CODE);
  CU_ASSERT_NSTRING_EQUAL_FATAL(exception.message, DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE, sizeof(DDS_SECURITY_ERR_INVALID_CRYPTO_HANDLE_MESSAGE));

  reset_exception(&exception);
}
