// Copyright(c) 2006 to 2020 ZettaScale Technology and others
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
#include "crypto_objects.h"

#if OPENSLL_VERSION_NUMBER >= 0x10002000L
#define AUTH_INCLUDE_EC
#endif

static struct plugins_hdl *plugins = NULL;
static dds_security_cryptography *crypto = NULL;

static void suite_register_local_participant_init(void)
{
  CU_ASSERT_FATAL ((plugins = load_plugins(
                      NULL    /* Access Control */,
                      NULL    /* Authentication */,
                      &crypto /* Cryptograpy    */,
                      NULL)) != NULL);
}

static void suite_register_local_participant_fini(void)
{
  unload_plugins(plugins);
}

static void reset_exception(DDS_Security_SecurityException *ex)
{
  ex->code = 0;
  ex->minor_code = 0;
  ddsrt_free(ex->message);
  ex->message = NULL;
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

CU_Test(ddssec_builtin_register_local_participant, happy_day, .init = suite_register_local_participant_init, .fini = suite_register_local_participant_fini)
{
  DDS_Security_ParticipantCryptoHandle hdl;

  /* Dummy (even un-initialized) data for now. */
  DDS_Security_IdentityHandle participant_identity = 5; //valid dummy value
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_PermissionsHandle participant_permissions = 2; /*dummy but valid */
  DDS_Security_PropertySeq participant_properties;
  DDS_Security_ParticipantSecurityAttributes participant_security_attributes;

  /* Check if we actually have the validate_local_identity() function. */
  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_factory != NULL);
  assert(crypto->crypto_key_factory != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_factory->register_local_participant != NULL);
  assert(crypto->crypto_key_factory->register_local_participant != 0);

  memset(&exception, 0, sizeof(DDS_Security_SecurityException));
  memset(&participant_properties, 0, sizeof(participant_properties));

  prepare_participant_security_attributes(&participant_security_attributes);

  /* Now call the function. */
  hdl = crypto->crypto_key_factory->register_local_participant(
      crypto->crypto_key_factory,
      participant_identity,
      participant_permissions,
      &participant_properties,
      &participant_security_attributes,
      &exception);

  if (exception.code != 0)
    printf("register_local_participant: %s\n", exception.message ? exception.message : "Error message missing");

  /* A valid handle to be returned */
  CU_ASSERT(hdl != DDS_SECURITY_HANDLE_NIL);
  CU_ASSERT(exception.code == DDS_SECURITY_ERR_OK_CODE);

  reset_exception(&exception);

  (void)crypto->crypto_key_factory->unregister_participant(
      crypto->crypto_key_factory,
      hdl,
      &exception);
  reset_exception(&exception);
}

CU_Test(ddssec_builtin_register_local_participant, empty_identity, .init = suite_register_local_participant_init, .fini = suite_register_local_participant_fini)
{
  DDS_Security_ParticipantCryptoHandle result;

  /* Dummy (even un-initialized) data for now. */
  DDS_Security_IdentityHandle participant_identity = 0; //empty identity
  DDS_Security_SecurityException exception = {NULL, 0, 0};

  DDS_Security_PermissionsHandle participant_permissions = 2; /*dummy but valid */
  DDS_Security_PropertySeq participant_properties;
  DDS_Security_ParticipantSecurityAttributes participant_security_attributes;

  /* Check if we actually have the validate_local_identity() function. */
  CU_ASSERT_FATAL(crypto != NULL);
  assert(crypto != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_factory != NULL);
  assert(crypto->crypto_key_factory != NULL);
  CU_ASSERT_FATAL(crypto->crypto_key_factory->register_local_participant != NULL);
  assert(crypto->crypto_key_factory->register_local_participant != 0);

  memset(&exception, 0, sizeof(DDS_Security_SecurityException));
  memset(&participant_properties, 0, sizeof(participant_properties));
  prepare_participant_security_attributes(&participant_security_attributes);

  /* Now call the function. */
  result = crypto->crypto_key_factory->register_local_participant(
      crypto->crypto_key_factory,
      participant_identity,
      participant_permissions,
      &participant_properties,
      &participant_security_attributes,
      &exception);
  if (exception.code != 0)
    printf("register_local_participant: %s\n", exception.message ? exception.message : "Error message missing");

  CU_ASSERT(exception.code == DDS_SECURITY_ERR_IDENTITY_EMPTY_CODE);
  CU_ASSERT_FATAL(exception.message != NULL);
  assert(exception.message != NULL); // for Clang's static analyzer
  CU_ASSERT(!strcmp(exception.message, DDS_SECURITY_ERR_IDENTITY_EMPTY_MESSAGE));
  CU_ASSERT(result == 0);

  reset_exception(&exception);
}

