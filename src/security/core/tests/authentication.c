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

#include "dds/version.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/environ.h"
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
    "<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"
    "<Domain id=\"any\">"
    "  <Tracing><Verbosity>finest</></>"
    "  <DDSSecurity>"
    "    <Authentication>"
    "      <Library finalizeFunction=\"finalize_test_authentication_wrapped\" initFunction=\"init_test_authentication_wrapped\" path=\"" WRAPPERLIB_PATH("dds_security_authentication_wrapper") "\"/>"
    "      <IdentityCertificate>${TEST_IDENTITY_CERTIFICATE}</IdentityCertificate>"
    "      <PrivateKey>${TEST_IDENTITY_PRIVATE_KEY}</PrivateKey>"
    "      <IdentityCA>${TEST_IDENTITY_CA_CERTIFICATE}</IdentityCA>"
    "      <Password>testtext_Password_testtext</Password>"
    "      <TrustedCADirectory>.</TrustedCADirectory>"
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

static dds_entity_t g_domain1 = 0;
static dds_entity_t g_participant1 = 0;

static dds_entity_t g_domain2 = 0;
static dds_entity_t g_participant2 = 0;

static void authentication_init(void)
{
  struct kvp governance_vars[] = {
    { "ALLOW_UNAUTH_PP", "false" },
    { "ENABLE_JOIN_AC", "true" },
    { NULL, NULL }
  };
  char * gov_config_signed = get_governance_config (governance_vars);

  struct kvp config_vars1[] = {
    { "GOVERNANCE_DATA", gov_config_signed },
    { "TEST_IDENTITY_CERTIFICATE", TEST_IDENTITY_CERTIFICATE },
    { "TEST_IDENTITY_PRIVATE_KEY", TEST_IDENTITY_PRIVATE_KEY },
    { "TEST_IDENTITY_CA_CERTIFICATE", TEST_IDENTITY_CA_CERTIFICATE },
    { NULL, NULL }
  };
  struct kvp config_vars2[] = {
    { "GOVERNANCE_DATA", gov_config_signed },
    { "TEST_IDENTITY_CERTIFICATE", TEST_IDENTITY2_CERTIFICATE },
    { "TEST_IDENTITY_PRIVATE_KEY", TEST_IDENTITY2_PRIVATE_KEY },
    { "TEST_IDENTITY_CA_CERTIFICATE", TEST_IDENTITY_CA2_CERTIFICATE },
    { NULL, NULL }
  };

  char *conf1 = ddsrt_expand_vars (config, &expand_lookup_vars_env, config_vars1);
  char *conf2 = ddsrt_expand_vars (config, &expand_lookup_vars_env, config_vars2);
  g_domain1 = dds_create_domain (DDS_DOMAINID1, conf1);
  g_domain2 = dds_create_domain (DDS_DOMAINID2, conf2);
  dds_free (conf1);
  dds_free (conf2);
  ddsrt_free (gov_config_signed);

  CU_ASSERT_FATAL ((g_participant1 = dds_create_participant (DDS_DOMAINID1, NULL, NULL)) > 0);
  CU_ASSERT_FATAL ((g_participant2 = dds_create_participant (DDS_DOMAINID2, NULL, NULL)) > 0);
}

static void authentication_fini(void)
{
  CU_ASSERT_EQUAL_FATAL (dds_delete (g_participant1), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL (dds_delete (g_participant2), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL (dds_delete (g_domain1), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL (dds_delete (g_domain2), DDS_RETCODE_OK);
}

CU_Test(ddssec_authentication, different_ca, .init = authentication_init, .fini = authentication_fini)
{
  validate_handshake (DDS_DOMAINID1, true, NULL, true, "error: unable to get local issuer certificate");
  validate_handshake (DDS_DOMAINID2, true, NULL, true, "error: unable to get local issuer certificate");
}
