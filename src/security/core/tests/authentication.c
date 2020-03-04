/*
 * Copyright(c) 2006 to 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stdlib.h>
#include <assert.h>

#include "dds/dds.h"
#include "CUnit/Test.h"
#include "CUnit/Theory.h"

#include "dds/version.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/ddsi_xqos.h"

#include "dds/security/dds_security_api.h"

#include "common/config_env.h"
#include "common/authentication_wrapper.h"
#include "common/handshake_test_utils.h"
#include "common/security_config_test_utils.h"
#include "common/test_identity.h"

static const char *config =
    "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}"
    "<Domain id=\"any\">"
    "  <Discovery>"
    "    <ExternalDomainId>0</ExternalDomainId>"
    "    <Tag>\\${CYCLONEDDS_PID}</Tag>"
    "  </Discovery>"
    "  <Tracing><Verbosity>config</></>"
    "  <DDSSecurity>"
    "    <Authentication>"
    "      <Library finalizeFunction=\"finalize_test_authentication_wrapped\" initFunction=\"init_test_authentication_wrapped\" path=\"" WRAPPERLIB_PATH("dds_security_authentication_wrapper") "\"/>"
    "      <IdentityCertificate>${TEST_IDENTITY_CERTIFICATE}</IdentityCertificate>"
    "      <PrivateKey>${TEST_IDENTITY_PRIVATE_KEY}</PrivateKey>"
    "      <IdentityCA>${TEST_IDENTITY_CA_CERTIFICATE}</IdentityCA>"
    "      ${TRUSTED_CA_DIR:+<TrustedCADir>}${TRUSTED_CA_DIR}${TRUSTED_CA_DIR:+</TrustedCADir>}"
    "    </Authentication>"
    "    <AccessControl>"
    "      <Library finalizeFunction=\"finalize_access_control\" initFunction=\"init_access_control\"/>"
    "      <Governance>file:" COMMON_ETC_PATH("default_governance.p7s") "</Governance>"
    "      <PermissionsCA>file:" COMMON_ETC_PATH("default_permissions_ca.pem") "</PermissionsCA>"
    "      <Permissions>file:" COMMON_ETC_PATH("default_permissions.p7s") "</Permissions>"
    "    </AccessControl>"
    "    <Cryptographic>"
    "      <Library finalizeFunction=\"finalize_crypto\" initFunction=\"init_crypto\"/>"
    "    </Cryptographic>"
    "  </DDSSecurity>"
    "</Domain>";

#define DDS_DOMAINID1 0
#define DDS_DOMAINID2 1
#define MAX_ADDITIONAL_CONF 255

static dds_entity_t g_domain1 = 0;
static dds_entity_t g_participant1 = 0;

static dds_entity_t g_domain2 = 0;
static dds_entity_t g_participant2 = 0;

static void authentication_init(bool different_ca, const char * trusted_ca_dir, bool exp_pp_fail)
{
  struct kvp config_vars1[] = {
    { "TEST_IDENTITY_CERTIFICATE", TEST_IDENTITY_CERTIFICATE, 1 },
    { "TEST_IDENTITY_PRIVATE_KEY", TEST_IDENTITY_PRIVATE_KEY, 1 },
    { "TEST_IDENTITY_CA_CERTIFICATE", TEST_IDENTITY_CA_CERTIFICATE, 1 },
    { "TRUSTED_CA_DIR", trusted_ca_dir, 3 },
    { NULL, NULL, 0 }
  };

  struct kvp config_vars2[] = {
    { "TEST_IDENTITY_CERTIFICATE", TEST_IDENTITY2_CERTIFICATE, 1 },
    { "TEST_IDENTITY_PRIVATE_KEY", TEST_IDENTITY2_PRIVATE_KEY, 1 },
    { "TEST_IDENTITY_CA_CERTIFICATE", TEST_IDENTITY_CA2_CERTIFICATE, 1 },
    { "TRUSTED_CA_DIR", trusted_ca_dir, 3 },
    { NULL, NULL, 0 }
  };

  char *conf1 = ddsrt_expand_vars_sh (config, &expand_lookup_vars_env, config_vars1);
  char *conf2 = ddsrt_expand_vars_sh (config, &expand_lookup_vars_env, config_vars2);
  CU_ASSERT_EQUAL_FATAL (expand_lookup_unmatched (config_vars1), 0);
  CU_ASSERT_EQUAL_FATAL (expand_lookup_unmatched (config_vars2), 0);
  g_domain1 = dds_create_domain (DDS_DOMAINID1, conf1);
  g_domain2 = dds_create_domain (DDS_DOMAINID2, different_ca ? conf2 : conf1);
  dds_free (conf1);
  dds_free (conf2);

  g_participant1 = dds_create_participant (DDS_DOMAINID1, NULL, NULL);
  g_participant2 = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
  if (exp_pp_fail)
  {
    CU_ASSERT_FATAL (g_participant1 <= 0);
    CU_ASSERT_FATAL (g_participant2 <= 0);
  }
  else
  {
    CU_ASSERT_FATAL (g_participant1 > 0);
    CU_ASSERT_FATAL (g_participant2 > 0);
  }
}

static void authentication_fini(bool delete_pp)
{
  if (delete_pp)
  {
    CU_ASSERT_EQUAL_FATAL (dds_delete (g_participant1), DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL (dds_delete (g_participant2), DDS_RETCODE_OK);
  }
  CU_ASSERT_EQUAL_FATAL (dds_delete (g_domain1), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL (dds_delete (g_domain2), DDS_RETCODE_OK);
}

CU_Test(ddssec_authentication, different_ca)
{
  authentication_init (true, NULL, false);
  validate_handshake (DDS_DOMAINID1, true, NULL, true, "error: unable to get local issuer certificate");
  validate_handshake (DDS_DOMAINID2, true, NULL, true, "error: unable to get local issuer certificate");
  authentication_fini (true);
}


CU_TheoryDataPoints(ddssec_authentication, trusted_ca_dir) = {
    CU_DataPoints(const char *, "",    ".",   "/nonexisting", NULL),
    CU_DataPoints(bool,         false, false, true,           false)
};

CU_Theory((const char * ca_dir, bool exp_fail), ddssec_authentication, trusted_ca_dir)
{
  printf("Testing custom CA dir: %s\n", ca_dir);
  authentication_init (false, ca_dir, exp_fail);
  if (!exp_fail)
  {
    validate_handshake (DDS_DOMAINID1, false, NULL, false, NULL);
    validate_handshake (DDS_DOMAINID2, false, NULL, false, NULL);
  }
  authentication_fini (!exp_fail);
}
