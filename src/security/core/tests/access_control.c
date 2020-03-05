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
#include "common/access_control_wrapper.h"
#include "common/security_config_test_utils.h"
#include "common/test_identity.h"

static const char *config =
    "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}"
    "<Domain id=\"any\">"
    "  <Discovery>"
    "    <ExternalDomainId>0</ExternalDomainId>"
    "    <Tag>\\${CYCLONEDDS_PID}</Tag>"
    "  </Discovery>"
    "  <DDSSecurity>"
    "    <Authentication>"
    "      <Library finalizeFunction=\"finalize_test_authentication_wrapped\" initFunction=\"init_test_authentication_wrapped\" path=\"" WRAPPERLIB_PATH("dds_security_authentication_wrapper") "\"/>"
    "      <IdentityCertificate>" TEST_IDENTITY_CERTIFICATE "</IdentityCertificate>"
    "      <PrivateKey>" TEST_IDENTITY_PRIVATE_KEY "</PrivateKey>"
    "      <IdentityCA>" TEST_IDENTITY_CA_CERTIFICATE "</IdentityCA>"
    "    </Authentication>"
    "    <AccessControl>"
    "      <Library finalizeFunction=\"finalize_access_control\" initFunction=\"init_access_control\"/>"
    "      <Governance>${TEST_GOVERNANCE}</Governance>"
    "      <PermissionsCA>${TEST_PERMISSIONS_CA}</PermissionsCA>"
    "      <Permissions>${TEST_PERMISSIONS}</Permissions>"
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

static void access_control_init(const char * gov, const char * perm, const char * ca, bool exp_pp_fail)
{
  struct kvp config_vars[] = {
    { "TEST_GOVERNANCE", gov, 1 },
    { "TEST_PERMISSIONS", perm, 1 },
    { "TEST_PERMISSIONS_CA", ca, 1 },
    { NULL, NULL, 0 }
  };

  char *conf = ddsrt_expand_vars_sh (config, &expand_lookup_vars_env, config_vars);
  CU_ASSERT_EQUAL_FATAL (expand_lookup_unmatched (config_vars), 0);
  g_domain1 = dds_create_domain (DDS_DOMAINID1, conf);
  g_domain2 = dds_create_domain (DDS_DOMAINID2, conf);
  dds_free (conf);

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

static void access_control_fini(bool delete_pp)
{
  if (delete_pp)
  {
    CU_ASSERT_EQUAL_FATAL (dds_delete (g_participant1), DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL (dds_delete (g_participant2), DDS_RETCODE_OK);
  }
  CU_ASSERT_EQUAL_FATAL (dds_delete (g_domain1), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL (dds_delete (g_domain2), DDS_RETCODE_OK);
}


#define PF_F "file:"
#define PF_D "data:,"
#define GOV_F PF_F COMMON_ETC_PATH("default_governance.p7s")
#define GOV_FNE PF_F COMMON_ETC_PATH("default_governance_non_existing.p7s")
#define GOV_DI PF_D COMMON_ETC_PATH("default_governance.p7s")
#define PERM_F PF_F COMMON_ETC_PATH("default_permissions.p7s")
#define PERM_FNE PF_F COMMON_ETC_PATH("default_permissions_non_existing.p7s")
#define PERM_DI PF_D COMMON_ETC_PATH("default_permissions.p7s")
#define CA_F PF_F COMMON_ETC_PATH("default_permissions_ca.pem")
#define CA_FNE PF_F COMMON_ETC_PATH("default_permissions_ca_non_existing.pem")
#define CA_DI PF_D COMMON_ETC_PATH("default_permissions_ca.pem")
#define CA_D TEST_PERMISSIONS_CA_CERTIFICATE

CU_TheoryDataPoints(ddssec_access_control, config_parameters) = {
    CU_DataPoints(const char *, GOV_F,  GOV_FNE,  GOV_FNE, GOV_F,    GOV_F,  "",     GOV_F, GOV_F,  GOV_DI, GOV_F),
    CU_DataPoints(const char *, PERM_F, PERM_FNE, PERM_F,  PERM_FNE, PERM_F, PERM_F, "",    PERM_F, PERM_F, PERM_F),
    CU_DataPoints(const char *, CA_F,   CA_FNE,   CA_F,    CA_F,     CA_FNE, CA_F,   CA_F,  "",     CA_F,   CA_D),
    CU_DataPoints(bool,         false,  true,     true,    true,     true,   true,   true,  true,   true,   false)
};
CU_Theory((const char * gov, const char * perm, const char * ca, bool exp_fail), ddssec_access_control, config_parameters)
{
  access_control_init (gov, perm, ca, exp_fail);
  access_control_fini (!exp_fail);
}
