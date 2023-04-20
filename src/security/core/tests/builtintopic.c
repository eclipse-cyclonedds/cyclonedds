// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdlib.h>
#include <assert.h>

#include "dds/dds.h"
#include "CUnit/Test.h"
#include "CUnit/Theory.h"

#include "dds/version.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/security/dds_security_api.h"
#include "ddsi__misc.h"

#include "common/config_env.h"
#include "common/test_identity.h"
#include "common/test_utils.h"
#include "common/security_config_test_utils.h"
#include "common/cryptography_wrapper.h"
#include "common/test_identity.h"
#include "common/cert_utils.h"

static const char *config =
    "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}"
    "<Domain id=\"any\">"
    "  <Discovery>"
    "    <ExternalDomainId>0</ExternalDomainId>"
    "    <Tag>\\${CYCLONEDDS_PID}</Tag>"
    "  </Discovery>"
    "  <Security>"
    "    <Authentication>"
    "      <Library finalizeFunction=\"finalize_test_authentication_wrapped\" initFunction=\"init_test_authentication_wrapped\" path=\"" WRAPPERLIB_PATH("dds_security_authentication_wrapper") "\"/>"
    "      <IdentityCertificate>data:," TEST_IDENTITY1_CERTIFICATE "</IdentityCertificate>"
    "      <PrivateKey>data:," TEST_IDENTITY1_PRIVATE_KEY "</PrivateKey>"
    "      <IdentityCA>data:," TEST_IDENTITY_CA1_CERTIFICATE "</IdentityCA>"
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
    "  </Security>"
    "</Domain>";

CU_Test(ddssec_builtintopic, participant_iid)
{
  static struct kvp config_vars[] = { { NULL, NULL, 0 } };
  char *conf;
  conf = ddsrt_expand_vars_sh (config, &expand_lookup_vars_env, config_vars);
  CU_ASSERT_EQUAL_FATAL (expand_lookup_unmatched (config_vars), 0);
  dds_entity_t domain = dds_create_domain (0, conf);
  CU_ASSERT_FATAL (domain > 0);
  dds_entity_t pp = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (pp > 0);
  ddsrt_free (conf);

  dds_return_t rc;
  dds_guid_t guid;
  dds_instance_handle_t iid;
  rc = dds_get_guid (pp, &guid);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_get_instance_handle (pp, &iid);
  CU_ASSERT_FATAL (rc == 0);

  dds_entity_t rd = dds_create_reader (pp, DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, NULL, NULL);

  // Should be able to find it by GUID; instance id must match
  {
    dds_instance_handle_t iid1;
    iid1 = dds_lookup_instance (rd, &guid);
    CU_ASSERT_FATAL (iid1 == iid);
  }

  // Should be able to find it by instance; GUID must match
  {
    dds_sample_info_t si;
    void *raw = NULL;
    int32_t n = dds_take_instance (rd, &raw, &si, 1, 1, iid);
    CU_ASSERT_FATAL (n == 1);
    CU_ASSERT_FATAL (si.valid_data);
    const dds_builtintopic_participant_t *s = raw;
    CU_ASSERT_FATAL (memcmp (&s->key, &guid, sizeof (guid)) == 0);
    dds_return_loan (rd, &raw, 1);
  }

  // There should be no other instances
  {
    dds_sample_info_t si;
    void *raw = NULL;
    int32_t n = dds_take_instance (rd, &raw, &si, 1, 1, iid);
    CU_ASSERT_FATAL (n == 0);
  }

  dds_delete (domain);
}
