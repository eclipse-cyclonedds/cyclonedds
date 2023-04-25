// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdlib.h>
#include "CUnit/Test.h"
#include "dds/dds.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/heap.h"
#include "ddsi__misc.h"
#include "dds/security/dds_security_api_defs.h"
#include "common/config_env.h"
#include "common/test_identity.h"

static uint32_t found;

static const char *default_config =
    "<Domain id=\"any\">"
    "  <Discovery>"
    "    <Tag>${CYCLONEDDS_PID}</Tag>"
    "  </Discovery>"
    "  <Tracing><Verbosity>finest</></>"
    "</Domain>";

static void logger(void *ptr, const dds_log_data_t *data)
{
  char **expected = (char **)ptr;
  fputs(data->message, stdout);
  for (uint32_t i = 0; expected[i] != NULL; i++)
  {
    if (ddsi_patmatch(expected[i], data->message))
    {
      found |= (uint32_t)(1 << i);
    }
  }
}

static void set_logger_exp(const void *log_expected)
{
  found = 0;
  dds_set_log_mask(DDS_LC_FATAL | DDS_LC_ERROR | DDS_LC_WARNING | DDS_LC_CONFIG);
  dds_set_log_sink(&logger, (void *)log_expected);
  dds_set_trace_sink(&logger, (void *)log_expected);
}

static void reset_logger(void)
{
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);
}

CU_Test(ddssec_security_plugin_loading, all_ok, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain, participant;
  const char *log_expected[] = {
      "DDS Security plugins have been loaded*",
      NULL};

  const char *sec_config =
      "<Domain id=\"any\">"
      "  <Discovery>"
      "    <Tag>${CYCLONEDDS_PID}</Tag>"
      "  </Discovery>"
      "  <Tracing><Verbosity>finest</></>"
      "  <Security>"
      "    <Authentication>"
      "      <Library initFunction=\"init_test_authentication_all_ok\" finalizeFunction=\"finalize_test_authentication_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_authentication_wrapper") "\"/>"
      "      <IdentityCertificate>"TEST_IDENTITY_CERTIFICATE_DUMMY"</IdentityCertificate>"
      "      <IdentityCA>"TEST_IDENTITY_CA_CERTIFICATE_DUMMY"</IdentityCA>"
      "      <PrivateKey>"TEST_IDENTITY_PRIVATE_KEY_DUMMY"</PrivateKey>"
      "      <Password>testtext_Password_testtext</Password>"
      "      <TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
      "    </Authentication>"
      "    <Cryptographic>"
      "      <Library initFunction=\"init_test_cryptography_all_ok\" finalizeFunction=\"finalize_test_cryptography_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_cryptography_wrapper") "\"/>"
      "    </Cryptographic>"
      "    <AccessControl>"
      "      <Library initFunction=\"init_test_access_control_all_ok\" finalizeFunction=\"finalize_test_access_control_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_access_control_wrapper") "\"/>"
      "      <Governance></Governance>"
      "      <PermissionsCA></PermissionsCA>"
      "      <Permissions></Permissions>"
      "    </AccessControl>"
      "  </Security>"
      "</Domain>";

  set_logger_exp(log_expected);
  domain = dds_create_domain(0, sec_config);
  CU_ASSERT_FATAL(domain > 0);
  participant = dds_create_participant(0, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);
  dds_delete(participant);
  dds_delete(domain);
  reset_logger();

  CU_ASSERT_FATAL(found == 0x1);
}

CU_Test(ddssec_security_plugin_loading, missing_finalize, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain, participant;
  const char *log_expected[] = {
      "Could not find the function: finalize_test_authentication_NON_EXISTING_FUNC*",
      "Could not load Authentication plugin*",
      NULL};

  const char *sec_config =
      "<Domain id=\"any\">"
      "  <Discovery>"
      "    <Tag>${CYCLONEDDS_PID}</Tag>"
      "  </Discovery>"
      "  <Tracing><Verbosity>warning</></>"
      "  <Security>"
      "    <Authentication>"
      "      <Library initFunction=\"init_test_authentication_all_ok\" finalizeFunction=\"finalize_test_authentication_NON_EXISTING_FUNC\" path=\"" WRAPPERLIB_PATH("dds_security_authentication_wrapper") "\"/>"
      "      <IdentityCertificate>"TEST_IDENTITY_CERTIFICATE_DUMMY"</IdentityCertificate>"
      "      <IdentityCA>"TEST_IDENTITY_CA_CERTIFICATE_DUMMY"</IdentityCA>"
      "      <PrivateKey>"TEST_IDENTITY_PRIVATE_KEY_DUMMY"</PrivateKey>"
      "      <Password>testtext_Password_testtext</Password>"
      "      <TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
      "    </Authentication>"
      "    <Cryptographic>"
      "      <Library initFunction=\"init_test_cryptography_all_ok\" finalizeFunction=\"finalize_test_cryptography_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_cryptography_wrapper") "\"/>"
      "    </Cryptographic>"
      "    <AccessControl>"
      "      <Library initFunction=\"init_test_access_control_all_ok\" finalizeFunction=\"finalize_test_access_control_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_access_control_wrapper") "\"/>"
      "      <Governance></Governance>"
      "      <PermissionsCA></PermissionsCA>"
      "      <Permissions></Permissions>"
      "    </AccessControl>"
      "  </Security>"
      "</Domain>";

  set_logger_exp(log_expected);
  domain = dds_create_domain(0, sec_config);
  CU_ASSERT_FATAL(domain > 0);
  participant = dds_create_participant(0, NULL, NULL);
  CU_ASSERT_EQUAL_FATAL(participant, DDS_RETCODE_ERROR);
  dds_delete(domain);
  reset_logger();

  CU_ASSERT_FATAL(found == 0x3);
}

CU_Test(ddssec_security_plugin_loading, authentication_missing_function, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain, participant;
  const char *log_expected[] = {
      "Could not find the function for Authentication: get_shared_secret*",
      "Could not load security*",
      NULL};

  const char *sec_config =
      "<Domain id=\"any\">"
      "  <Discovery>"
      "    <Tag>${CYCLONEDDS_PID}</Tag>"
      "  </Discovery>"
      "  <Tracing><Verbosity>warning</></>"
      "  <Security>"
      "    <Authentication>"
      "      <Library initFunction=\"init_test_authentication_missing_func\" finalizeFunction=\"finalize_test_authentication_missing_func\" path=\"" WRAPPERLIB_PATH("dds_security_authentication_wrapper") "\"/>"
      "      <IdentityCertificate>"TEST_IDENTITY_CERTIFICATE_DUMMY"</IdentityCertificate>"
      "      <IdentityCA>"TEST_IDENTITY_CA_CERTIFICATE_DUMMY"</IdentityCA>"
      "      <PrivateKey>"TEST_IDENTITY_PRIVATE_KEY_DUMMY"</PrivateKey>"
      "      <Password>testtext_Password_testtext</Password>"
      "      <TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
      "    </Authentication>"
      "    <Cryptographic>"
      "      <Library initFunction=\"init_test_cryptography_all_ok\" finalizeFunction=\"finalize_test_cryptography_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_cryptography_wrapper") "\"/>"
      "    </Cryptographic>"
      "    <AccessControl>"
      "      <Library initFunction=\"init_test_access_control_all_ok\" finalizeFunction=\"finalize_test_access_control_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_access_control_wrapper") "\"/>"
      "      <Governance></Governance>"
      "      <PermissionsCA></PermissionsCA>"
      "      <Permissions></Permissions>"
      "    </AccessControl>"
      "  </Security>"
      "</Domain>";

  set_logger_exp(log_expected);
  domain = dds_create_domain(0, sec_config);
  CU_ASSERT_FATAL(domain > 0);
  participant = dds_create_participant(0, NULL, NULL);
  CU_ASSERT_EQUAL_FATAL(participant, DDS_RETCODE_ERROR);
  dds_delete(domain);
  reset_logger();

  CU_ASSERT_FATAL(found == 0x3);
}

CU_Test(ddssec_security_plugin_loading, access_control_missing_function, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain, participant;
  const char *log_expected[] = {
      "Could not find the function for Access Control: check_create_datareader*",
      "Could not load security*",
      NULL};

  const char *sec_config =
      "<Domain id=\"any\">"
      "  <Discovery>"
      "    <Tag>${CYCLONEDDS_PID}</Tag>"
      "  </Discovery>"
      "  <Tracing><Verbosity>warning</></>"
      "  <Security>"
      "    <Authentication>"
      "      <Library initFunction=\"init_test_authentication_all_ok\" finalizeFunction=\"finalize_test_authentication_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_authentication_wrapper") "\"/>"
      "      <IdentityCertificate>"TEST_IDENTITY_CERTIFICATE_DUMMY"</IdentityCertificate>"
      "      <IdentityCA>"TEST_IDENTITY_CA_CERTIFICATE_DUMMY"</IdentityCA>"
      "      <PrivateKey>"TEST_IDENTITY_PRIVATE_KEY_DUMMY"</PrivateKey>"
      "      <Password>testtext_Password_testtext</Password>"
      "      <TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
      "    </Authentication>"
      "    <Cryptographic>"
      "      <Library initFunction=\"init_test_cryptography_all_ok\" finalizeFunction=\"finalize_test_cryptography_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_cryptography_wrapper") "\"/>"
      "    </Cryptographic>"
      "    <AccessControl>"
      "      <Library initFunction=\"init_test_access_control_missing_func\" finalizeFunction=\"finalize_test_access_control_missing_func\" path=\"" WRAPPERLIB_PATH("dds_security_access_control_wrapper") "\"/>"
      "      <Governance></Governance>"
      "      <PermissionsCA></PermissionsCA>"
      "      <Permissions></Permissions>"
      "    </AccessControl>"
      "  </Security>"
      "</Domain>";

  set_logger_exp(log_expected);
  domain = dds_create_domain(0, sec_config);
  CU_ASSERT_FATAL(domain > 0);
  participant = dds_create_participant(0, NULL, NULL);
  CU_ASSERT_EQUAL_FATAL(participant, DDS_RETCODE_ERROR);
  dds_delete(domain);
  reset_logger();

  CU_ASSERT_FATAL(found == 0x3);
}

CU_Test(ddssec_security_plugin_loading, cryptography_missing_function, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain, participant;
  const char *log_expected[] = {
      "Could not find the function for Cryptographic: set_remote_participant_crypto_tokens*",
      "Could not load security*",
      NULL};

  const char *sec_config =
      "<Domain id=\"any\">"
      "  <Discovery>"
      "    <Tag>${CYCLONEDDS_PID}</Tag>"
      "  </Discovery>"
      "  <Tracing><Verbosity>warning</></>"
      "  <Security>"
      "    <Authentication>"
      "      <Library initFunction=\"init_test_authentication_all_ok\" finalizeFunction=\"finalize_test_authentication_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_authentication_wrapper") "\"/>"
      "      <IdentityCertificate>"TEST_IDENTITY_CERTIFICATE_DUMMY"</IdentityCertificate>"
      "      <IdentityCA>"TEST_IDENTITY_CA_CERTIFICATE_DUMMY"</IdentityCA>"
      "      <PrivateKey>"TEST_IDENTITY_PRIVATE_KEY_DUMMY"</PrivateKey>"
      "      <Password>testtext_Password_testtext</Password>"
      "      <TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
      "    </Authentication>"
      "    <Cryptographic>"
      "      <Library initFunction=\"init_test_cryptography_missing_func\" finalizeFunction=\"finalize_test_cryptography_missing_func\" path=\"" WRAPPERLIB_PATH("dds_security_cryptography_wrapper") "\"/>"
      "    </Cryptographic>"
      "    <AccessControl>"
      "      <Library initFunction=\"init_test_access_control_all_ok\" finalizeFunction=\"finalize_test_access_control_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_access_control_wrapper") "\"/>"
      "      <Governance></Governance>"
      "      <PermissionsCA></PermissionsCA>"
      "      <Permissions></Permissions>"
      "    </AccessControl>"
      "  </Security>"
      "</Domain>";

  set_logger_exp(log_expected);
  domain = dds_create_domain(0, sec_config);
  CU_ASSERT_FATAL(domain > 0);
  participant = dds_create_participant(0, NULL, NULL);
  CU_ASSERT_EQUAL_FATAL(participant, DDS_RETCODE_ERROR);
  dds_delete(domain);
  reset_logger();

  CU_ASSERT_FATAL(found == 0x3);
}

CU_Test(ddssec_security_plugin_loading, no_library_in_path, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain, participant;
  const char *log_expected[] = {
      "Could not load Authentication library: " WRAPPERLIB_PATH("dds_security_authentication_wrapper_INVALID") ": cannot open shared object file: No such file or directory*",
      "Could not load Authentication library: dlopen(" WRAPPERLIB_PATH("dds_security_authentication_wrapper_INVALID") "*",
      "Could not load Authentication library: The specified module could not be found.*",
      "Could not load Authentication plugin*",
      "Could not load security*",
      NULL};

  const char *sec_config =
      "<Domain id=\"any\">"
      "  <Discovery>"
      "    <Tag>${CYCLONEDDS_PID}</Tag>"
      "  </Discovery>"
      "  <Tracing><Verbosity>warning</></>"
      "  <Security>"
      "    <Authentication>"
      "      <Library initFunction=\"init_test_authentication_all_ok\" finalizeFunction=\"finalize_test_authentication_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_authentication_wrapper_INVALID") "\"/>"
      "      <IdentityCertificate>"TEST_IDENTITY_CERTIFICATE_DUMMY"</IdentityCertificate>"
      "      <IdentityCA>"TEST_IDENTITY_CA_CERTIFICATE_DUMMY"</IdentityCA>"
      "      <PrivateKey>"TEST_IDENTITY_PRIVATE_KEY_DUMMY"</PrivateKey>"
      "      <Password>testtext_Password_testtext</Password>"
      "      <TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
      "    </Authentication>"
      "    <Cryptographic>"
      "      <Library initFunction=\"init_test_cryptography_all_ok\" finalizeFunction=\"finalize_test_cryptography_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_cryptography_wrapper") "\"/>"
      "    </Cryptographic>"
      "    <AccessControl>"
      "      <Library initFunction=\"init_test_access_control_all_ok\" finalizeFunction=\"finalize_test_access_control_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_access_control_wrapper") "\"/>"
      "      <Governance></Governance>"
      "      <PermissionsCA></PermissionsCA>"
      "      <Permissions></Permissions>"
      "    </AccessControl>"
      "  </Security>"
      "</Domain>";

  set_logger_exp(log_expected);
  domain = dds_create_domain(0, sec_config);
  CU_ASSERT_FATAL(domain > 0);
  participant = dds_create_participant(0, NULL, NULL);
  CU_ASSERT_EQUAL_FATAL(participant, DDS_RETCODE_ERROR);
  dds_delete(domain);
  reset_logger();

  CU_ASSERT_FATAL(found == 0x19 || found == 0x1a || found == 0x1c);
}

CU_Test(ddssec_security_plugin_loading, init_error, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain, participant;
  const char *log_expected[] = {
      "Error occurred while initializing Authentication plugin*",
      "Could not load Authentication plugin*",
      "Could not load security*",
      NULL};

  const char *sec_config =
      "<Domain id=\"any\">"
      "  <Discovery>"
      "    <Tag>${CYCLONEDDS_PID}</Tag>"
      "  </Discovery>"
      "  <Tracing><Verbosity>warning</></>"
      "  <Security>"
      "    <Authentication>"
      "      <Library initFunction=\"init_test_authentication_init_error\" finalizeFunction=\"finalize_test_authentication_init_error\" path=\"" WRAPPERLIB_PATH("dds_security_authentication_wrapper") "\"/>"
      "      <IdentityCertificate>"TEST_IDENTITY_CERTIFICATE_DUMMY"</IdentityCertificate>"
      "      <IdentityCA>"TEST_IDENTITY_CA_CERTIFICATE_DUMMY"</IdentityCA>"
      "      <PrivateKey>"TEST_IDENTITY_PRIVATE_KEY_DUMMY"</PrivateKey>"
      "      <Password>testtext_Password_testtext</Password>"
      "      <TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
      "    </Authentication>"
      "    <Cryptographic>"
      "      <Library initFunction=\"init_test_cryptography_all_ok\" finalizeFunction=\"finalize_test_cryptography_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_cryptography_wrapper") "\"/>"
      "    </Cryptographic>"
      "    <AccessControl>"
      "      <Library initFunction=\"init_test_access_control_all_ok\" finalizeFunction=\"finalize_test_access_control_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_access_control_wrapper") "\"/>"
      "      <Governance></Governance>"
      "      <PermissionsCA></PermissionsCA>"
      "      <Permissions></Permissions>"
      "    </AccessControl>"
      "  </Security>"
      "</Domain>";

  set_logger_exp(log_expected);
  domain = dds_create_domain(0, sec_config);
  CU_ASSERT_FATAL(domain > 0);
  participant = dds_create_participant(0, NULL, NULL);
  CU_ASSERT_EQUAL_FATAL(participant, DDS_RETCODE_ERROR);
  dds_delete(domain);
  reset_logger();

  CU_ASSERT_FATAL(found == 0x7);
}

CU_Test(ddssec_security_plugin_loading, all_ok_with_props, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain, participant;
  dds_qos_t *qos;
  const char *log_expected[] = {
      "DDS Security plugins have been loaded*",
      NULL};

  unsigned char bvalue[3] = {0x01, 0x02, 0x03};
  CU_ASSERT_FATAL((qos = dds_create_qos()) != NULL);
  dds_qset_prop(qos, "test.prop1", "testtext_value1_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_IDENTITY_CA, "testtext_IdentityCA_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_PRIV_KEY, "testtext_PrivateKey_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_IDENTITY_CERT, "testtext_IdentityCertificate_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_PERMISSIONS_CA, "file:");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_GOVERNANCE, "file:");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_PERMISSIONS, "file:");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_PASSWORD, "testtext_Password_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_TRUSTED_CA_DIR, "file:/test/dir");

  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_PATH, WRAPPERLIB_PATH("dds_security_authentication_wrapper"));
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_INIT, "init_test_authentication_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_FINALIZE, "finalize_test_authentication_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_PATH, WRAPPERLIB_PATH("dds_security_cryptography_wrapper"));
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_INIT, "init_test_cryptography_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_FINALIZE, "finalize_test_cryptography_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_PATH, WRAPPERLIB_PATH("dds_security_access_control_wrapper"));
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_INIT, "init_test_access_control_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_FINALIZE, "finalize_test_access_control_all_ok");

  dds_qset_prop(qos, "test.prop2", "testtext_value2_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_IDENTITY_CA, "testtext_IdentityCA_testtext");
  dds_qset_bprop(qos, "test.bprop1", bvalue, 3);

  set_logger_exp(log_expected);
  domain = dds_create_domain(0, default_config);
  CU_ASSERT_FATAL(domain > 0);
  participant = dds_create_participant(0, qos, NULL);
  CU_ASSERT_FATAL(participant > 0);
  dds_delete(participant);
  dds_delete(domain);
  dds_delete_qos(qos);
  reset_logger();

  CU_ASSERT_FATAL(found == 0x1);
}

CU_Test(ddssec_security_plugin_loading, missing_plugin_property_with_props, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain, participant;
  dds_qos_t *qos;
  const char *log_expected[] = {
      "*using security settings from QoS*",
      "*required security property " DDS_SEC_PROP_AUTH_LIBRARY_INIT " missing in Property QoS*",
      NULL};

  unsigned char bvalue[3] = {0x01, 0x02, 0x03};
  CU_ASSERT_FATAL((qos = dds_create_qos()) != NULL);
  dds_qset_prop(qos, "test.prop1", "testtext_value1_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_IDENTITY_CA, "testtext_IdentityCA_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_PRIV_KEY, "testtext_PrivateKey_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_IDENTITY_CERT, "testtext_IdentityCertificate_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_PERMISSIONS_CA, "file:");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_GOVERNANCE, "file:");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_PERMISSIONS, "file:");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_PASSWORD, "testtext_Password_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_TRUSTED_CA_DIR, "file:/test/dir");

  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_PATH, "dds_security_authentication_all_ok");
  // missing: dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_INIT, "init_authentication");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_FINALIZE, "finalize_authentication");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_PATH, "dds_security_cryptography_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_INIT, "init_crypto");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_FINALIZE, "finalize_crypto");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_PATH, "dds_security_access_control_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_INIT, "init_access_control");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_FINALIZE, "finalize_access_control");

  dds_qset_prop(qos, "test.prop2", "testtext_value2_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_IDENTITY_CA, "testtext_IdentityCA_testtext");
  dds_qset_bprop(qos, "test.bprop1", bvalue, 3);

  set_logger_exp(log_expected);
  domain = dds_create_domain(0, default_config);
  CU_ASSERT_FATAL(domain > 0);
  participant = dds_create_participant(0, qos, NULL);
  CU_ASSERT_EQUAL_FATAL(participant, DDS_RETCODE_ERROR);
  dds_delete_qos(qos);
  dds_delete(domain);
  reset_logger();

  CU_ASSERT_FATAL(found == 0x3);
}

CU_Test(ddssec_security_plugin_loading, empty_plugin_property_with_props, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain, participant;
  dds_qos_t *qos;
  const char *log_expected[] = {
      "*using security settings from QoS*",
      "*required security property " DDS_SEC_PROP_AUTH_LIBRARY_FINALIZE " missing in Property QoS*",
      NULL};

  unsigned char bvalue[3] = {0x01, 0x02, 0x03};
  CU_ASSERT_FATAL((qos = dds_create_qos()) != NULL);
  dds_qset_prop(qos, "test.prop1", "testtext_value1_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_IDENTITY_CA, "testtext_IdentityCA_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_PRIV_KEY, "testtext_PrivateKey_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_IDENTITY_CERT, "testtext_IdentityCertificate_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_PERMISSIONS_CA, "file:Permissions_CA.pem");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_GOVERNANCE, "file:");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_PERMISSIONS, "file:");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_PASSWORD, "testtext_Password_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_TRUSTED_CA_DIR, "file:/test/dir");

  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_PATH, "dds_security_authentication_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_INIT, "init_authentication");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_FINALIZE, "");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_PATH, "dds_security_cryptography_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_INIT, "init_crypto");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_FINALIZE, "finalize_crypto");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_PATH, "dds_security_access_control_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_INIT, "init_access_control");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_FINALIZE, "finalize_access_control");

  dds_qset_prop(qos, "test.prop2", "testtext_value2_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_IDENTITY_CA, "testtext_IdentityCA_testtext");
  dds_qset_bprop(qos, "test.bprop1", bvalue, 3);

  set_logger_exp(log_expected);
  domain = dds_create_domain(0, default_config);
  CU_ASSERT_FATAL(domain > 0);
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, qos, NULL);
  CU_ASSERT_EQUAL_FATAL(participant, DDS_RETCODE_ERROR);
  dds_delete_qos(qos);
  dds_delete(domain);
  reset_logger();

  CU_ASSERT_FATAL(found == 0x3);
}

CU_Test(ddssec_security_plugin_loading, missing_security_property_with_props, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain, participant;
  dds_qos_t *qos;
  const char *log_expected[] = {
      "*using security settings from QoS*",
      "*required security property " DDS_SEC_PROP_ACCESS_PERMISSIONS " missing in Property QoS*",
      NULL};

  unsigned char bvalue[3] = {0x01, 0x02, 0x03};
  CU_ASSERT_FATAL((qos = dds_create_qos()) != NULL);
  dds_qset_prop(qos, "test.prop1", "testtext_value1_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_IDENTITY_CA, "testtext_IdentityCA_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_PRIV_KEY, "testtext_PrivateKey_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_IDENTITY_CERT, "testtext_IdentityCertificate_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_PERMISSIONS_CA, "file:");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_GOVERNANCE, "file:");
  //dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_PERMISSIONS, "file:");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_PASSWORD, "testtext_Password_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_TRUSTED_CA_DIR, "file:/test/dir");

  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_PATH, "dds_security_authentication_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_INIT, "init_authentication");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_FINALIZE, "finalize_authentication");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_PATH, "dds_security_cryptography_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_INIT, "init_crypto");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_FINALIZE, "finalize_crypto");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_PATH, "dds_security_access_control_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_INIT, "init_access_control");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_FINALIZE, "finalize_access_control");

  dds_qset_prop(qos, "test.prop2", "testtext_value2_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_IDENTITY_CA, "testtext_IdentityCA_testtext");
  dds_qset_bprop(qos, "test.bprop1", bvalue, 3);

  set_logger_exp(log_expected);
  domain = dds_create_domain(0, default_config);
  CU_ASSERT_FATAL(domain > 0);
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, qos, NULL);
  CU_ASSERT_EQUAL_FATAL(participant, DDS_RETCODE_ERROR);
  dds_delete_qos(qos);
  dds_delete(domain);
  reset_logger();

  CU_ASSERT_FATAL(found == 0x3);
}

CU_Test(ddssec_security_plugin_loading, multiple_domains_different_config, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain1, domain2, participant1, participant2, participant3;
  dds_qos_t *qos;
  const char *log_expected[] = {
      "*using security settings from configuration*",
      "*using security settings from QoS*",
      "DDS Security plugins have been loaded*",
      "*security is already loaded for this domain*",
      NULL};

  const char *sec_config =
      "<Domain id=\"1\">"
      "  <Discovery>"
      "    <Tag>${CYCLONEDDS_PID}</Tag>"
      "  </Discovery>"
      "  <Tracing><Verbosity>finest</></>"
      "  <Security>"
      "    <Authentication>"
      "      <Library initFunction=\"init_test_authentication_all_ok\" finalizeFunction=\"finalize_test_authentication_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_authentication_wrapper") "\"/>"
      "      <IdentityCertificate>"TEST_IDENTITY_CERTIFICATE_DUMMY"</IdentityCertificate>"
      "      <IdentityCA>"TEST_IDENTITY_CA_CERTIFICATE_DUMMY"</IdentityCA>"
      "      <PrivateKey>"TEST_IDENTITY_PRIVATE_KEY_DUMMY"</PrivateKey>"
      "      <Password>testtext_Password_testtext</Password>"
      "      <TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
      "    </Authentication>"
      "    <Cryptographic>"
      "      <Library initFunction=\"init_test_cryptography_all_ok\" finalizeFunction=\"finalize_test_cryptography_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_cryptography_wrapper") "\"/>"
      "    </Cryptographic>"
      "    <AccessControl>"
      "      <Library initFunction=\"init_test_access_control_all_ok\" finalizeFunction=\"finalize_test_access_control_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_access_control_wrapper") "\"/>"
      "      <Governance></Governance>"
      "      <PermissionsCA></PermissionsCA>"
      "      <Permissions></Permissions>"
      "    </AccessControl>"
      "  </Security>"
      "</Domain>"
      "<Domain id=\"2\">"
      "  <Tracing><Verbosity>finest</></>"
      "  <Discovery>"
      "    <Tag>${CYCLONEDDS_PID}</Tag>"
      "  </Discovery>"
      "  <Security>"
      "    <Authentication>"
      "      <Library initFunction=\"init_test_authentication_all_ok\" finalizeFunction=\"finalize_test_authentication_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_authentication_wrapper") "\"/>"
      "      <IdentityCertificate>"TEST_IDENTITY_CERTIFICATE_DUMMY"</IdentityCertificate>"
      "      <IdentityCA>"TEST_IDENTITY_CA_CERTIFICATE_DUMMY"</IdentityCA>"
      "      <PrivateKey>"TEST_IDENTITY_PRIVATE_KEY_DUMMY"</PrivateKey>"
      "      <Password>testtext_Password_testtext</Password>"
      "      <TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
      "    </Authentication>"
      "    <Cryptographic>"
      "      <Library initFunction=\"init_test_cryptography_all_ok\" finalizeFunction=\"finalize_test_cryptography_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_cryptography_wrapper") "\"/>"
      "    </Cryptographic>"
      "    <AccessControl>"
      "      <Library initFunction=\"init_test_access_control_all_ok\" finalizeFunction=\"finalize_test_access_control_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_access_control_wrapper") "\"/>"
      "      <Governance></Governance>"
      "      <PermissionsCA></PermissionsCA>"
      "      <Permissions></Permissions>"
      "    </AccessControl>"
      "  </Security>"
      "</Domain>";

  set_logger_exp(log_expected);

  domain1 = dds_create_domain(1, sec_config);
  CU_ASSERT_FATAL(domain1 > 0);
  domain2 = dds_create_domain(2, sec_config);
  CU_ASSERT_FATAL(domain2 > 0);

  /* Create the qos */
  CU_ASSERT_FATAL((qos = dds_create_qos()) != NULL);
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_IDENTITY_CA, "testtext_IdentityCA_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_PRIV_KEY, "testtext_PrivateKey_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_IDENTITY_CERT, "testtext_IdentityCertificate_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_PERMISSIONS_CA, "file:");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_GOVERNANCE, "file:");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_PERMISSIONS, "file:");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_PASSWORD, "testtext_Password_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_TRUSTED_CA_DIR, "file:/test/dir");

  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_PATH, WRAPPERLIB_PATH("dds_security_authentication_wrapper"));
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_INIT, "init_test_authentication_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_FINALIZE, "finalize_test_authentication_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_IDENTITY_CA, "testtext_IdentityCA_testtext");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_PATH, WRAPPERLIB_PATH("dds_security_cryptography_wrapper"));
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_INIT, "init_test_cryptography_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_FINALIZE, "finalize_test_cryptography_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_PATH, WRAPPERLIB_PATH("dds_security_access_control_wrapper"));
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_INIT, "init_test_access_control_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_FINALIZE, "finalize_test_access_control_all_ok");

  participant1 = dds_create_participant(1, NULL, NULL);
  participant2 = dds_create_participant(2, NULL, NULL);
  participant3 = dds_create_participant(2, qos, NULL);
  CU_ASSERT_FATAL(participant1 > 0);
  CU_ASSERT_FATAL(participant2 > 0);
  CU_ASSERT_FATAL(participant3 > 0);
  dds_delete_qos(qos);
  dds_delete(domain1);
  dds_delete(domain2);
  reset_logger();

  CU_ASSERT_FATAL(found == 0xf);
}
