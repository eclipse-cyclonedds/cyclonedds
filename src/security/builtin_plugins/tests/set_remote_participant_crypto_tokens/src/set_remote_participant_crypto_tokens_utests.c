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

static DDS_Security_IdentityHandle local_participant_identity = 1;
static DDS_Security_IdentityHandle remote_participant_identity = 2;

static DDS_Security_ParticipantCryptoHandle local_crypto_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_ParticipantCryptoHandle remote_crypto_handle = DDS_SECURITY_HANDLE_NIL;

static DDS_Security_SharedSecretHandleImpl *shared_secret_handle_impl = NULL;
static DDS_Security_SharedSecretHandle shared_secret_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_ParticipantCryptoTokenSeq tokens;

static void allocate_shared_secret(void)
{
  shared_secret_handle_impl = ddsrt_malloc(sizeof(DDS_Security_SharedSecretHandleImpl));

  shared_secret_handle_impl->shared_secret = ddsrt_malloc(TEST_SHARED_SECRET_SIZE * sizeof(unsigned char));
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

static int register_local_participant(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PermissionsHandle participant_permissions = 3; //valid dummy value
  DDS_Security_PropertySeq participant_properties;
  DDS_Security_ParticipantSecurityAttributes participant_security_attributes;

  memset(&participant_properties, 0, sizeof(participant_properties));
  memset(&tokens, 0, sizeof(tokens));

  prepare_participant_security_attributes(&participant_security_attributes);

  local_crypto_handle =
      crypto->crypto_key_factory->register_local_participant(
          crypto->crypto_key_factory,
          local_participant_identity,
          participant_permissions,
          &participant_properties,
          &participant_security_attributes,
          &exception);

  if (local_crypto_handle == DDS_SECURITY_HANDLE_NIL)
    printf("register_local_participant: %s\n", exception.message ? exception.message : "Error message missing");

  return local_crypto_handle ? 0 : -1;
}

static int register_remote_participant(void)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PermissionsHandle remote_participant_permissions = 5;

  remote_crypto_handle =
      crypto->crypto_key_factory->register_matched_remote_participant(
          crypto->crypto_key_factory,
          local_crypto_handle,
          remote_participant_identity,
          remote_participant_permissions,
          shared_secret_handle,
          &exception);

  if (remote_crypto_handle == DDS_SECURITY_HANDLE_NIL)
    printf("register_matched_remote_participant: %s\n", exception.message ? exception.message : "Error message missing");

  return remote_crypto_handle ? 0 : -1;
}

static int create_crypto_tokens(void)
{
  DDS_Security_boolean status;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  status = crypto->crypto_key_exchange->create_local_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      &tokens,
      local_crypto_handle,
      remote_crypto_handle,
      &exception);
  if (!status)
    printf("register_matched_remote_participant: %s\n", exception.message ? exception.message : "Error message missing");

  return status ? 0 : -1;
}

static void unregister_participants(void)
{
  DDS_Security_boolean status;
  DDS_Security_SecurityException exception = {NULL, 0, 0};

  if (tokens._length != 0)
  {
    status = crypto->crypto_key_exchange->return_crypto_tokens(crypto->crypto_key_exchange, &tokens, &exception);
    if (!status)
      printf("return_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");
  }

  if (local_crypto_handle)
  {
    status = crypto->crypto_key_factory->unregister_participant(crypto->crypto_key_factory, local_crypto_handle, &exception);
    if (!status)
      printf("unregister_participant: %s\n", exception.message ? exception.message : "Error message missing");
  }

  if (remote_crypto_handle)
  {
    status = crypto->crypto_key_factory->unregister_participant(crypto->crypto_key_factory, remote_crypto_handle, &exception);
    if (!status)
      printf("unregister_participant: %s\n", exception.message ? exception.message : "Error message missing");
  }
}

static void suite_set_remote_participant_crypto_tokens_init(void)
{
  allocate_shared_secret();

  CU_ASSERT_FATAL ((plugins = load_plugins(
                      NULL    /* Access Control */,
                      NULL    /* Authentication */,
                      &crypto /* Cryptograpy    */,
                      NULL)) != NULL);
  CU_ASSERT_EQUAL_FATAL (register_local_participant(), 0);
  CU_ASSERT_EQUAL_FATAL (register_remote_participant(), 0);
  CU_ASSERT_EQUAL_FATAL (create_crypto_tokens(), 0);
}

static void suite_set_remote_participant_crypto_tokens_fini(void)
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

CU_Test(ddssec_builtin_set_remote_participant_crypto_tokens, happy_day, .init = suite_set_remote_participant_crypto_tokens_init, .fini = suite_set_remote_participant_crypto_tokens_fini)
{
  DDS_Security_boolean result;

  /* Dummy (even un-initialized) data for now. */
  DDS_Security_SecurityException exception = {NULL, 0, 0};

  /* Check if we actually have the validate_local_identity() function. */
  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange != NULL);
  assert(crypto->crypto_key_exchange != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange->set_remote_participant_crypto_tokens != NULL);
  assert(crypto->crypto_key_exchange->set_remote_participant_crypto_tokens != 0);

  memset(&exception, 0, sizeof(DDS_Security_SecurityException));

  /* Now call the function. */
  result = crypto->crypto_key_exchange->set_remote_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      local_crypto_handle,
      remote_crypto_handle,
      &tokens,
      &exception);
  if (!result)
    printf("set_local_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT_FATAL(result);
  CU_ASSERT(exception.code == 0);
  CU_ASSERT(exception.message == NULL);
  reset_exception(&exception);
}

CU_Test(ddssec_builtin_set_remote_participant_crypto_tokens, invalid_args, .init = suite_set_remote_participant_crypto_tokens_init, .fini = suite_set_remote_participant_crypto_tokens_fini)
{
  DDS_Security_boolean result;

  /* Dummy (even un-initialized) data for now. */
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PropertySeq participant_properties;

  /* Check if we actually have the validate_local_identity() function. */
  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange != NULL);
  assert(crypto->crypto_key_exchange != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange->create_local_participant_crypto_tokens != NULL);
  assert(crypto->crypto_key_exchange->create_local_participant_crypto_tokens != 0);

  memset(&exception, 0, sizeof(DDS_Security_SecurityException));
  memset(&participant_properties, 0, sizeof(participant_properties));

  /* invalid token seq = NULL */
  result = crypto->crypto_key_exchange->set_remote_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      local_crypto_handle,
      remote_crypto_handle,
      NULL,
      &exception);
  if (!result)
    printf("set_local_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);

  /* invalid local_crypto_handle = DDS_SECURITY_HANDLE_NIL */
  result = crypto->crypto_key_exchange->set_remote_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      0,
      remote_crypto_handle,
      &tokens,
      &exception);
  if (!result)
    printf("set_remote_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);

  /* invalid remote_crypto_handle = DDS_SECURITY_HANDLE_NIL */
  result = crypto->crypto_key_exchange->set_remote_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      local_crypto_handle,
      0,
      &tokens,
      &exception);
  if (!result)
    printf("set_remote_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);

  /* invalid local_crypto_handle = 1 */
  result = crypto->crypto_key_exchange->set_remote_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      1,
      remote_crypto_handle,
      &tokens,
      &exception);
  if (!result)
    printf("set_remote_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);

  /* invalid remote_crypto_handle = 1 */
  result = crypto->crypto_key_exchange->set_remote_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      local_crypto_handle,
      1,
      &tokens,
      &exception);
  if (!result)
    printf("set_remote_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);
}

CU_Test(ddssec_builtin_set_remote_participant_crypto_tokens, invalid_tokens, .init = suite_set_remote_participant_crypto_tokens_init, .fini = suite_set_remote_participant_crypto_tokens_fini)
{
  DDS_Security_boolean result;

  /* Dummy (even un-initialized) data for now. */
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PropertySeq participant_properties;
  DDS_Security_ParticipantCryptoTokenSeq invalid_tokens;
  DDS_Security_KeyMaterial_AES_GCM_GMAC keymat;

  /* Check if we actually have the validate_local_identity() function. */
  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange != NULL);
  assert(crypto->crypto_key_exchange != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_exchange->create_local_participant_crypto_tokens != NULL);
  assert(crypto->crypto_key_exchange->create_local_participant_crypto_tokens != 0);

  memset(&exception, 0, sizeof(DDS_Security_SecurityException));
  memset(&participant_properties, 0, sizeof(participant_properties));
  memset(&keymat, 0, sizeof(keymat));

  memset(&invalid_tokens, 0, sizeof(invalid_tokens));

  /* empty token sequence */
  result = crypto->crypto_key_exchange->set_remote_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      local_crypto_handle,
      remote_crypto_handle,
      &invalid_tokens,
      &exception);
  if (!result)
    printf("set_local_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);

  /* DDS_Security_ParticipantCryptoTokenSeq with empty token */
  invalid_tokens._length = 1;
  invalid_tokens._buffer = DDS_Security_DataHolderSeq_allocbuf(1);
  result = crypto->crypto_key_exchange->set_remote_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      local_crypto_handle,
      remote_crypto_handle,
      &invalid_tokens,
      &exception);
  if (!result)
    printf("set_local_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);

  /* invalid token class id */
  invalid_tokens._buffer[0].class_id = ddsrt_strdup("invalid class");
  result = crypto->crypto_key_exchange->set_remote_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      local_crypto_handle,
      remote_crypto_handle,
      &invalid_tokens,
      &exception);
  if (!result)
    printf("set_local_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);
  ddsrt_free(invalid_tokens._buffer[0].class_id);
  reset_exception(&exception);

  /* no key material, binary_property missing */
  invalid_tokens._buffer[0].class_id = ddsrt_strdup(DDS_CRYPTOTOKEN_CLASS_ID);
  result = crypto->crypto_key_exchange->set_remote_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      local_crypto_handle,
      remote_crypto_handle,
      &invalid_tokens,
      &exception);
  if (!result)
    printf("set_local_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);

  /* no key material, property is empty */
  invalid_tokens._buffer[0].binary_properties._length = 1;
  invalid_tokens._buffer[0].binary_properties._buffer = DDS_Security_BinaryPropertySeq_allocbuf(1);
  result = crypto->crypto_key_exchange->set_remote_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      local_crypto_handle,
      remote_crypto_handle,
      &invalid_tokens,
      &exception);
  if (!result)
    printf("set_local_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);

  /* invalid property name */
  invalid_tokens._buffer[0].binary_properties._buffer[0].name = ddsrt_strdup("invalid_key_mat_name");
  result = crypto->crypto_key_exchange->set_remote_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      local_crypto_handle,
      remote_crypto_handle,
      &invalid_tokens,
      &exception);
  if (!result)
    printf("set_local_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);
  ddsrt_free(invalid_tokens._buffer[0].binary_properties._buffer[0].name);
  reset_exception(&exception);

  /* invalid property name */
  invalid_tokens._buffer[0].binary_properties._buffer[0].name = ddsrt_strdup(DDS_CRYPTOTOKEN_PROP_KEYMAT);
  result = crypto->crypto_key_exchange->set_remote_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      local_crypto_handle,
      remote_crypto_handle,
      &invalid_tokens,
      &exception);
  if (!result)
    printf("set_local_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);

  /* empty key material */
  invalid_tokens._buffer[0].binary_properties._buffer[0].value._length = 1;
  invalid_tokens._buffer[0].binary_properties._buffer[0].value._maximum = 1;
  invalid_tokens._buffer[0].binary_properties._buffer[0].value._buffer = (unsigned char *)&keymat;
  result = crypto->crypto_key_exchange->set_remote_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      local_crypto_handle,
      remote_crypto_handle,
      &invalid_tokens,
      &exception);
  if (!result)
    printf("set_local_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);

  keymat.transformation_kind[3] = 5;

  /* invalid CryptoTransformKind */
  invalid_tokens._buffer[0].binary_properties._buffer[0].value._length = 1;
  invalid_tokens._buffer[0].binary_properties._buffer[0].value._maximum = 1;
  invalid_tokens._buffer[0].binary_properties._buffer[0].value._buffer = (unsigned char *)&keymat;
  result = crypto->crypto_key_exchange->set_remote_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      local_crypto_handle,
      remote_crypto_handle,
      &invalid_tokens,
      &exception);
  if (!result)
    printf("set_local_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);

  keymat.transformation_kind[3] = 2;
  keymat.master_salt._length = 16;
  keymat.master_salt._buffer = DDS_Security_OctetSeq_allocbuf(32);

  /* invalid master salt */
  invalid_tokens._buffer[0].binary_properties._buffer[0].value._length = 1;
  invalid_tokens._buffer[0].binary_properties._buffer[0].value._maximum = 1;
  invalid_tokens._buffer[0].binary_properties._buffer[0].value._buffer = (unsigned char *)&keymat;
  result = crypto->crypto_key_exchange->set_remote_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      local_crypto_handle,
      remote_crypto_handle,
      &invalid_tokens,
      &exception);
  if (!result)
    printf("set_local_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);

  keymat.master_salt._length = 32;

  /* invalid master salt */
  invalid_tokens._buffer[0].binary_properties._buffer[0].value._length = 1;
  invalid_tokens._buffer[0].binary_properties._buffer[0].value._maximum = 1;
  invalid_tokens._buffer[0].binary_properties._buffer[0].value._buffer = (unsigned char *)&keymat;
  result = crypto->crypto_key_exchange->set_remote_participant_crypto_tokens(
      crypto->crypto_key_exchange,
      local_crypto_handle,
      remote_crypto_handle,
      &invalid_tokens,
      &exception);
  if (!result)
    printf("set_local_participant_crypto_tokens: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(!result);
  CU_ASSERT(exception.code != 0);
  CU_ASSERT(exception.message != NULL);
  reset_exception(&exception);

  invalid_tokens._buffer[0].binary_properties._buffer[0].value._length = 0;
  invalid_tokens._buffer[0].binary_properties._buffer[0].value._maximum = 0;
  invalid_tokens._buffer[0].binary_properties._buffer[0].value._buffer = NULL;
  DDS_Security_ParticipantCryptoTokenSeq_deinit(&invalid_tokens);
  DDS_Security_OctetSeq_deinit(&keymat.master_salt);
}
