// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

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
#include "crypto_objects.h"
#include "crypto_tokens.h"

#define TEST_SHARED_SECRET_SIZE 32

static struct plugins_hdl *plugins = NULL;
static dds_security_cryptography *crypto = NULL;

static DDS_Security_ParticipantCryptoHandle local_crypto_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_ParticipantCryptoHandle remote_crypto_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_SharedSecretHandleImpl *shared_secret_handle_impl = NULL;
static DDS_Security_SharedSecretHandle shared_secret_handle = DDS_SECURITY_HANDLE_NIL;

static void allocate_shared_secret(void)
{
  int32_t i;

  shared_secret_handle_impl = ddsrt_malloc(sizeof(DDS_Security_SharedSecretHandleImpl));

  shared_secret_handle_impl->shared_secret = ddsrt_malloc(TEST_SHARED_SECRET_SIZE * sizeof(DDS_Security_octet));
  shared_secret_handle_impl->shared_secret_size = TEST_SHARED_SECRET_SIZE;

  for (i = 0; i < shared_secret_handle_impl->shared_secret_size; i++)
  {
    shared_secret_handle_impl->shared_secret[i] = (unsigned char)(i % 20);
  }

  for (i = 0; i < 32; i++)
  {
    shared_secret_handle_impl->challenge1[i] = (unsigned char)(i % 15);
    shared_secret_handle_impl->challenge2[i] = (unsigned char)(i % 12);
  }

  shared_secret_handle = (DDS_Security_SharedSecretHandle)shared_secret_handle_impl;
}

static void deallocate_shared_secret(void)
{
  ddsrt_free(shared_secret_handle_impl->shared_secret);
  ddsrt_free(shared_secret_handle_impl);
}

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

static int register_participants(void)
{
  int r = 0;
  DDS_Security_IdentityHandle participant_identity = 5; /* valid dummy value */
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PermissionsHandle participant_permissions = 3;        /* valid but dummy value */
  DDS_Security_PermissionsHandle remote_participant_permissions = 5; /*valid dummy value */
  DDS_Security_PropertySeq participant_properties;
  DDS_Security_ParticipantSecurityAttributes participant_security_attributes;

  memset(&exception, 0, sizeof(DDS_Security_SecurityException));
  memset(&participant_properties, 0, sizeof(participant_properties));
  prepare_participant_security_attributes(&participant_security_attributes);

  local_crypto_handle =
      crypto->crypto_key_factory->register_local_participant(
          crypto->crypto_key_factory,
          participant_identity,
          participant_permissions,
          &participant_properties,
          &participant_security_attributes,
          &exception);

  if (local_crypto_handle == DDS_SECURITY_HANDLE_NIL)
  {
    r = -1;
    printf("register_local_participant: %s\n", exception.message ? exception.message : "Error message missing");
  }

  if (r == 0)
  {
    remote_crypto_handle =
        crypto->crypto_key_factory->register_matched_remote_participant(
            crypto->crypto_key_factory,
            local_crypto_handle,
            participant_identity,
            remote_participant_permissions,
            shared_secret_handle,
            &exception);
    if (remote_crypto_handle == DDS_SECURITY_HANDLE_NIL)
    {
      r = -1;
      printf("register_matched_remote_participant: %s\n", exception.message ? exception.message : "Error message missing");
    }
  }

  return r;
}

static void unregister_participants(void)
{
  DDS_Security_boolean status;
  DDS_Security_SecurityException exception = {NULL, 0, 0};

  if (local_crypto_handle)
  {
    status = crypto->crypto_key_factory->unregister_participant(crypto->crypto_key_factory, local_crypto_handle, &exception);
    if (!status)
    {
      printf("unregister_participant: %s\n", exception.message ? exception.message : "Error message missing");
    }
  }

  if (remote_crypto_handle)
  {
    status = crypto->crypto_key_factory->unregister_participant(crypto->crypto_key_factory, remote_crypto_handle, &exception);
    if (!status)
    {
      printf("unregister_participant: %s\n", exception.message ? exception.message : "Error message missing");
    }
  }
}

static void suite_create_local_participant_crypto_tokens_init(void)
{
  CU_ASSERT_FATAL ((plugins = load_plugins(
                      NULL    /* Access Control */,
                      NULL    /* Authentication */,
                      &crypto /* Cryptograpy    */,
                      NULL)) != NULL);
  allocate_shared_secret();
  CU_ASSERT_EQUAL_FATAL (register_participants(), 0);
}

static void suite_create_local_participant_crypto_tokens_fini(void)
{
  unregister_participants();
  deallocate_shared_secret();
  unload_plugins(plugins);
}

static void reset_exception(DDS_Security_SecurityException *ex)
{
  ex->code = 0;
  ex->minor_code = 0;
  ddsrt_free(ex->message);
  ex->message = NULL;
}

static DDS_Security_boolean check_token_validity(const DDS_Security_ParticipantCryptoTokenSeq *tokens)
{
  DDS_Security_boolean status = true;
  uint32_t i;

  if (tokens->_length == 0 || tokens->_buffer == NULL)
  {
    status = false;
  }

  for (i = 0; status && (i < tokens->_length); i++)
  {
    status = (tokens->_buffer[i].class_id != NULL) &&
             (strcmp(DDS_CRYPTOTOKEN_CLASS_ID, tokens->_buffer[i].class_id) == 0) &&
             (tokens->_buffer[i].binary_properties._length == 1) &&
             (tokens->_buffer[i].binary_properties._buffer != NULL) &&
             (tokens->_buffer[i].binary_properties._buffer[0].name != NULL) &&
             (strcmp(DDS_CRYPTOTOKEN_PROP_KEYMAT, tokens->_buffer[i].binary_properties._buffer[0].name) == 0) &&
             (tokens->_buffer[i].binary_properties._buffer[0].value._length > 0) &&
             (tokens->_buffer[i].binary_properties._buffer[0].value._buffer != NULL);
  }

  return status;
}

CU_Test(ddssec_builtin_create_local_participant_crypto_tokens, happy_day, .init = suite_create_local_participant_crypto_tokens_init, .fini = suite_create_local_participant_crypto_tokens_fini)
{
  DDS_Security_boolean result;

  /* Dummy (even un-initialized) data for now. */
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_ParticipantCryptoTokenSeq tokens;

  /* Check if we actually have the validate_local_identity() function. */
  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange != NULL);
  assert(crypto->crypto_key_exchange != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange->create_local_participant_crypto_tokens != NULL);
  assert(crypto->crypto_key_exchange->create_local_participant_crypto_tokens != 0);

  memset(&exception, 0, sizeof(DDS_Security_SecurityException));
  memset(&tokens, 0, sizeof(tokens));

  /* Now call the function. */
  result = crypto->crypto_key_exchange->create_local_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      &tokens,
      local_crypto_handle,
      remote_crypto_handle,
      &exception);
  if (!result)
  {
    printf("create_local_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT_FATAL(result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);

  reset_exception(&exception);

  CU_ASSERT(check_token_validity(&tokens));

  result = crypto->crypto_key_exchange->return_crypto_tokens(crypto->crypto_key_exchange, &tokens, &exception);

  if (!result)
  {
    printf("return_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT_FATAL(result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);

  reset_exception(&exception);
}

CU_Test(ddssec_builtin_create_local_participant_crypto_tokens, invalid_args, .init = suite_create_local_participant_crypto_tokens_init, .fini = suite_create_local_participant_crypto_tokens_fini)
{
  DDS_Security_boolean result;

  /* Dummy (even un-initialized) data for now. */
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_ParticipantCryptoTokenSeq tokens;

  /* Check if we actually have the validate_local_identity() function. */
  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange != NULL);
  assert(crypto->crypto_key_exchange != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange->create_local_participant_crypto_tokens != NULL);
  assert(crypto->crypto_key_exchange->create_local_participant_crypto_tokens != 0);

  memset(&exception, 0, sizeof(DDS_Security_SecurityException));
  memset(&tokens, 0, sizeof(tokens));

  /* invalid token seq = NULL */
  result = crypto->crypto_key_exchange->create_local_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      NULL,
      local_crypto_handle,
      remote_crypto_handle,
      &exception);
  if (!result)
  {
    printf("create_local_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* invalid local_crypto_handle = DDS_SECURITY_HANDLE_NIL */
  result = crypto->crypto_key_exchange->create_local_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      &tokens,
      0,
      remote_crypto_handle,
      &exception);
  if (!result)
  {
    printf("create_local_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* invalid remote_crypto_handle = DDS_SECURITY_HANDLE_NIL */
  result = crypto->crypto_key_exchange->create_local_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      &tokens,
      local_crypto_handle,
      0,
      &exception);
  if (!result)
  {
    printf("create_local_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* invalid local_crypto_handle = 1 */
  result = crypto->crypto_key_exchange->create_local_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      &tokens,
      1,
      remote_crypto_handle,
      &exception);
  if (!result)
  {
    printf("create_local_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);

  /* invalid remote_crypto_handle = 1 */
  result = crypto->crypto_key_exchange->create_local_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      &tokens,
      local_crypto_handle,
      1,
      &exception);
  if (!result)
  {
    printf("create_local_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");
  }

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);

  reset_exception(&exception);
}
