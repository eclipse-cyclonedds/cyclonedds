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
#include "common/plugin_wrapper_msg_q.h"
#include "common/handshake_test_utils.h"
#include "common/test_identity.h"

static const char *config =
    "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}"
    "<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"
    "<Domain id=\"any\">"
    "  <Tracing><Verbosity>finest</></>"
    "  <DDSSecurity>"
    "    <Authentication>"
    "      <Library finalizeFunction=\"finalize_test_authentication_wrapped\" initFunction=\"init_test_authentication_wrapped\" path=\"" WRAPPERLIB_PATH("dds_security_authentication_wrapper") "\"/>"
    "      <IdentityCertificate>"TEST_IDENTITY_CERTIFICATE"</IdentityCertificate>"
    "      <PrivateKey>"TEST_IDENTITY_PRIVATE_KEY"</PrivateKey>"
    "      <IdentityCA>"TEST_IDENTITY_CA_CERTIFICATE"</IdentityCA>"
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

#define DDS_DOMAINID_PART1 0
#define DDS_DOMAINID_PART2 1

static dds_entity_t g_part1_domain = 0;
static dds_entity_t g_part1_participant = 0;

static dds_entity_t g_part2_domain = 0;
static dds_entity_t g_part2_participant = 0;

static void handshake_init(void)
{
  /* Domains for pub and sub use a different domain id, but the portgain setting
   * in configuration is 0, so that both domains will map to the same port number.
   * This allows to create two domains in a single test process. */
  char *conf_part1 = ddsrt_expand_envvars(config, DDS_DOMAINID_PART1);
  char *conf_part2 = ddsrt_expand_envvars(config, DDS_DOMAINID_PART2);
  g_part1_domain = dds_create_domain(DDS_DOMAINID_PART1, conf_part1);
  g_part2_domain = dds_create_domain(DDS_DOMAINID_PART2, conf_part2);
  dds_free(conf_part1);
  dds_free(conf_part2);

  CU_ASSERT_FATAL((g_part1_participant = dds_create_participant(DDS_DOMAINID_PART1, NULL, NULL)) > 0);
  CU_ASSERT_FATAL((g_part2_participant = dds_create_participant(DDS_DOMAINID_PART2, NULL, NULL)) > 0);
}

static void handshake_fini(void)
{
  CU_ASSERT_EQUAL_FATAL(dds_delete(g_part1_participant), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(g_part2_participant), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(g_part1_domain), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(dds_delete(g_part2_domain), DDS_RETCODE_OK);
}

CU_Test(ddssec_handshake, happy_day, .init = handshake_init, .fini = handshake_fini)
{
  validate_handshake(DDS_DOMAINID_PART1, false, NULL, false, NULL);
  validate_handshake(DDS_DOMAINID_PART2, false, NULL, false, NULL);
}
