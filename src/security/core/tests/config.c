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

#include "dds/dds.h"
#include "CUnit/Test.h"
#include "dds/version.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/security/dds_security_api_defs.h"
#include "common/config_env.h"
#include "common/test_identity.h"

#define PROPLIST(init_auth, fin_auth, init_crypto, fin_crypto, init_ac, fin_ac, perm_ca, gov, perm, pre_str, post_str, binprops)         \
  "property_list={" pre_str                                             \
  "0:\"dds.sec.auth.library.path\":\""WRAPPERLIB_PATH("dds_security_authentication_wrapper")"\","                         \
  "0:\"dds.sec.auth.library.init\":\""init_auth"\","            \
  "0:\"dds.sec.auth.library.finalize\":\""fin_auth"\","    \
  "0:\"dds.sec.crypto.library.path\":\""WRAPPERLIB_PATH("dds_security_cryptography_wrapper")"\","                     \
  "0:\"dds.sec.crypto.library.init\":\""init_crypto"\","                  \
  "0:\"dds.sec.crypto.library.finalize\":\""fin_crypto"\","          \
  "0:\"dds.sec.access.library.path\":\""WRAPPERLIB_PATH("dds_security_access_control_wrapper")"\","                         \
  "0:\"dds.sec.access.library.init\":\""init_ac"\","          \
  "0:\"dds.sec.access.library.finalize\":\""fin_ac"\","  \
  "0:\"dds.sec.auth.identity_ca\":\"" TEST_IDENTITY_CA_CERTIFICATE_DUMMY "\","  \
  "0:\"dds.sec.auth.private_key\":\"" TEST_IDENTITY_PRIVATE_KEY_DUMMY "\","     \
  "0:\"dds.sec.auth.identity_certificate\":\"" TEST_IDENTITY_CERTIFICATE_DUMMY "\"," \
  "0:\"dds.sec.access.permissions_ca\":\""perm_ca"\","    \
  "0:\"dds.sec.access.governance\":\""gov"\","            \
  "0:\"dds.sec.access.permissions\":\""perm"\""           \
  post_str "}:{" binprops "}"
#define PARTICIPANT_QOS(init_auth, fin_auth, init_crypto, fin_crypto, init_ac, fin_ac, perm_ca, gov, perm, pre_str, post_str, binprops)  \
  "PARTICIPANT * QOS={*" PROPLIST (init_auth, fin_auth, init_crypto, fin_crypto, init_ac, fin_ac, perm_ca, gov, perm, pre_str, post_str, binprops) "*"
#define PARTICIPANT_QOS_ALL_OK(pre_str, post_str, binprops)             \
  PARTICIPANT_QOS ("init_test_authentication_all_ok", "finalize_test_authentication_all_ok", \
                   "init_test_cryptography_all_ok", "finalize_test_cryptography_all_ok", \
                   "init_test_access_control_all_ok", "finalize_test_access_control_all_ok", \
                   "file:Permissions_CA.pem", "file:Governance.p7s", "file:Permissions.p7s", \
                   pre_str, post_str, binprops)

static const char *default_config =
    "<Domain id=\"any\">"
    "  <Discovery>"
    "    <Tag>${CYCLONEDDS_PID}</Tag>"
    "  </Discovery>"
    "  <Tracing><Verbosity>finest</></>"
    "</Domain>";


/*
 * The 'found' variable will contain flags related to the expected log
 * messages that were received.
 * Using flags will allow to show that when message isn't received,
 * which one it was.
 */
static uint32_t found;

static void logger(void *ptr, const dds_log_data_t *data)
{
  char **expected = (char**)ptr;
  fputs (data->message, stdout);
  for (uint32_t i = 0; expected[i] != NULL; i++) {
    if (ddsi2_patmatch(expected[i], data->message)) {
      found |= (uint32_t)(1 << i);
    }
  }
}

static void set_logger_exp(const void * log_expected)
{
  found = 0;
  dds_set_log_mask(DDS_LC_FATAL|DDS_LC_ERROR|DDS_LC_WARNING|DDS_LC_CONFIG);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);
}

static void reset_logger()
{
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);
}

/* Expected traces when creating domain with an empty security element.  We need to
   test this one here to be sure that it refuses to start when security is configured
   but the implementation doesn't include support for it. */
CU_Test(ddssec_config, empty, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain;
  const char *log_expected[] = {
    "config: //CycloneDDS/Domain/Security/Authentication/IdentityCertificate/#text: element missing in configuration*",
    "config: //CycloneDDS/Domain/Security/Authentication/IdentityCA/#text: element missing in configuration*",
    "config: //CycloneDDS/Domain/Security/Authentication/PrivateKey/#text: element missing in configuration*",
    NULL
  };

  const char *sec_config =
    "<Domain id=\"any\">"
    "  <Discovery>"
    "    <Tag>${CYCLONEDDS_PID}</Tag>"
    "  </Discovery>"
    "  <Tracing><Verbosity>config</></>"
    "  <Security />"
    "</Domain>";

  set_logger_exp(log_expected);
  domain = dds_create_domain(0, sec_config);
  CU_ASSERT_EQUAL_FATAL(domain, DDS_RETCODE_ERROR);
  reset_logger();

  /* All traces should have been provided. */
  CU_ASSERT_FATAL(found == 0x7);
}

/* Create domain without security element, there shouldn't
   be traces that mention security. */
CU_Test(ddssec_config, non, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain;
  const char *log_expected[] = {
    "*Security*",
    NULL
  };

  set_logger_exp(log_expected);
  domain = dds_create_domain(0, default_config);
  CU_ASSERT_FATAL(domain > 0);
  dds_delete(domain);
  reset_logger();

  /* No security traces should have been provided. */
  CU_ASSERT_FATAL(found == 0x0);
}

/* Expected traces when creating domain with the security elements. */
CU_Test(ddssec_config, missing, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain;
  const char *log_expected[] = {
    "config: //CycloneDDS/Domain/Security/Authentication/IdentityCertificate/#text: element missing in configuration*",
    "config: //CycloneDDS/Domain/Security/Authentication/IdentityCA/#text: element missing in configuration*",
    "config: //CycloneDDS/Domain/Security/Authentication/PrivateKey/#text: element missing in configuration*",
    NULL
  };

  /* IdentityCertificate, IdentityCA and PrivateKey values or elements are missing. */
  const char *sec_config =
    "<Domain id=\"any\">"
    "  <Discovery>"
    "    <Tag>${CYCLONEDDS_PID}</Tag>"
    "  </Discovery>"
    "  <Tracing><Verbosity>finest</></>"
    "  <Security>"
    "    <Authentication>"
    "      <Library initFunction=\"init_test_authentication_all_ok\" finalizeFunction=\"finalize_test_authentication_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_authentication_wrapper") "\"/>"
    "      <IdentityCertificate></IdentityCertificate>"
    "      <PrivateKey></PrivateKey>"
    "      <Password>testtext_Password_testtext</Password>"
    "    </Authentication>"
    "  </Security>"
    "</Domain>";

  set_logger_exp(log_expected);
  domain = dds_create_domain(0, sec_config);
  CU_ASSERT_EQUAL_FATAL(domain, DDS_RETCODE_ERROR);
  reset_logger();

  /* All traces should have been provided. */
  CU_ASSERT_FATAL(found == 0x7);
}

/* Expected traces when creating domain with the security elements. */
CU_Test(ddssec_config, all, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain, participant;
  const char *log_expected[] = {
    "config: Domain/Security/Authentication/Library/#text: "WRAPPERLIB_PATH("dds_security_authentication_wrapper")"*",
    "config: Domain/Security/Authentication/Library[@path]: "WRAPPERLIB_PATH("dds_security_authentication_wrapper")"*",
    "config: Domain/Security/Authentication/Library[@initFunction]: init_test_authentication_all_ok*",
    "config: Domain/Security/Authentication/Library[@finalizeFunction]: finalize_test_authentication_all_ok*",
    "config: Domain/Security/Authentication/IdentityCertificate/#text: "TEST_IDENTITY_CERTIFICATE_DUMMY"*",
    "config: Domain/Security/Authentication/IdentityCA/#text: "TEST_IDENTITY_CA_CERTIFICATE_DUMMY"*",
    "config: Domain/Security/Authentication/PrivateKey/#text: "TEST_IDENTITY_PRIVATE_KEY_DUMMY"*",
    "config: Domain/Security/Authentication/Password/#text: testtext_Password_testtext*",
    "config: Domain/Security/Authentication/TrustedCADirectory/#text: testtext_Dir_testtext*",
    "config: Domain/Security/AccessControl/Library/#text: "WRAPPERLIB_PATH("dds_security_access_control_wrapper")"*",
    "config: Domain/Security/AccessControl/Library[@path]: "WRAPPERLIB_PATH("dds_security_access_control_wrapper")"*",
    "config: Domain/Security/AccessControl/Library[@initFunction]: init_test_access_control_all_ok*",
    "config: Domain/Security/AccessControl/Library[@finalizeFunction]: finalize_test_access_control_all_ok*",
    "config: Domain/Security/AccessControl/PermissionsCA/#text: file:Permissions_CA.pem*",
    "config: Domain/Security/AccessControl/Governance/#text: file:Governance.p7s*",
    "config: Domain/Security/AccessControl/Permissions/#text: file:Permissions.p7s*",
    "config: Domain/Security/Cryptographic/Library/#text: "WRAPPERLIB_PATH("dds_security_cryptography_wrapper")"*",
    "config: Domain/Security/Cryptographic/Library[@path]: "WRAPPERLIB_PATH("dds_security_cryptography_wrapper")"*",
    "config: Domain/Security/Cryptographic/Library[@initFunction]: init_test_cryptography_all_ok*",
    "config: Domain/Security/Cryptographic/Library[@finalizeFunction]: finalize_test_cryptography_all_ok*",
    /* The config should have been parsed into the participant QoS. */
    PARTICIPANT_QOS_ALL_OK ("", ",0:\"dds.sec.auth.password\":\"testtext_Password_testtext\",0:\"dds.sec.auth.trusted_ca_dir\":\"testtext_Dir_testtext\"", ""),
    NULL
  };

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
    "      <Governance>file:Governance.p7s</Governance>"
    "      <PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
    "      <Permissions>file:Permissions.p7s</Permissions>"
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

  /* All traces should have been provided. */
  printf("found: %x\n", found);
  CU_ASSERT_FATAL(found == 0x1fffff);
}

/* Expected traces when creating participant with the security elements. */
CU_Test(ddssec_config, security, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain, participant;
  const char *log_expected[] = {
    "config: Domain/Security/Authentication/Library/#text: "WRAPPERLIB_PATH("dds_security_authentication_wrapper")"*",
    "config: Domain/Security/Authentication/Library[@path]: "WRAPPERLIB_PATH("dds_security_authentication_wrapper")"*",
    "config: Domain/Security/Authentication/Library[@initFunction]: init_test_authentication_all_ok*",
    "config: Domain/Security/Authentication/Library[@finalizeFunction]: finalize_test_authentication_all_ok*",
    "config: Domain/Security/Authentication/IdentityCertificate/#text: "TEST_IDENTITY_CERTIFICATE_DUMMY"*",
    "config: Domain/Security/Authentication/IdentityCA/#text: "TEST_IDENTITY_CA_CERTIFICATE_DUMMY"*",
    "config: Domain/Security/Authentication/PrivateKey/#text: "TEST_IDENTITY_PRIVATE_KEY_DUMMY"*",
    "config: Domain/Security/Authentication/Password/#text:  {}*",
    "config: Domain/Security/Authentication/TrustedCADirectory/#text:  {}*",
    "config: Domain/Security/AccessControl/Library/#text: "WRAPPERLIB_PATH("dds_security_access_control_wrapper")"*",
    "config: Domain/Security/AccessControl/Library[@path]: "WRAPPERLIB_PATH("dds_security_access_control_wrapper")"*",
    "config: Domain/Security/AccessControl/Library[@initFunction]: init_test_access_control_all_ok*",
    "config: Domain/Security/AccessControl/Library[@finalizeFunction]: finalize_test_access_control_all_ok*",
    "config: Domain/Security/AccessControl/PermissionsCA/#text: file:Permissions_CA.pem*",
    "config: Domain/Security/AccessControl/Governance/#text: file:Governance.p7s*",
    "config: Domain/Security/AccessControl/Permissions/#text: file:Permissions.p7s*",
    "config: Domain/Security/Cryptographic/Library/#text: "WRAPPERLIB_PATH("dds_security_cryptography_wrapper")"*",
    "config: Domain/Security/Cryptographic/Library[@path]: "WRAPPERLIB_PATH("dds_security_cryptography_wrapper")"*",
    "config: Domain/Security/Cryptographic/Library[@initFunction]: init_test_cryptography_all_ok*",
    "config: Domain/Security/Cryptographic/Library[@finalizeFunction]: finalize_test_cryptography_all_ok*",
    /* The config should have been parsed into the participant QoS. */
    PARTICIPANT_QOS_ALL_OK ("", ",0:\"dds.sec.auth.password\":\"\",0:\"dds.sec.auth.trusted_ca_dir\":\"\"", ""),
    NULL
  };

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
    "    </Authentication>"
    "    <Cryptographic>"
    "      <Library initFunction=\"init_test_cryptography_all_ok\" finalizeFunction=\"finalize_test_cryptography_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_cryptography_wrapper") "\"/>"
    "    </Cryptographic>"
    "    <AccessControl>"
    "      <Library initFunction=\"init_test_access_control_all_ok\" finalizeFunction=\"finalize_test_access_control_all_ok\" path=\"" WRAPPERLIB_PATH("dds_security_access_control_wrapper") "\"/>"
    "      <Governance>file:Governance.p7s</Governance>"
    "      <PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
    "      <Permissions>file:Permissions.p7s</Permissions>"
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

  /* All traces should have been provided. */
  CU_ASSERT_FATAL(found == 0x1fffff);
}

/* Expected traces when creating domain with the security elements. */
CU_Test(ddssec_config, deprecated, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain, participant;
  const char *log_expected[] = {
    "config: Domain/Security/Authentication/Library/#text: "WRAPPERLIB_PATH("dds_security_authentication_wrapper")"*",
    "config: Domain/Security/Authentication/Library[@path]: "WRAPPERLIB_PATH("dds_security_authentication_wrapper")"*",
    "config: Domain/Security/Authentication/Library[@initFunction]: init_test_authentication_all_ok*",
    "config: Domain/Security/Authentication/Library[@finalizeFunction]: finalize_test_authentication_all_ok*",
    "config: Domain/Security/Authentication/IdentityCertificate/#text: "TEST_IDENTITY_CERTIFICATE_DUMMY"*",
    "config: Domain/Security/Authentication/IdentityCA/#text: "TEST_IDENTITY_CA_CERTIFICATE_DUMMY"*",
    "config: Domain/Security/Authentication/PrivateKey/#text: "TEST_IDENTITY_PRIVATE_KEY_DUMMY"*",
    "config: Domain/Security/Authentication/Password/#text: testtext_Password_testtext*",
    "config: Domain/Security/Authentication/TrustedCADirectory/#text: testtext_Dir_testtext*",
    "config: Domain/Security/AccessControl/Library/#text: "WRAPPERLIB_PATH("dds_security_access_control_wrapper")"*",
    "config: Domain/Security/AccessControl/Library[@path]: "WRAPPERLIB_PATH("dds_security_access_control_wrapper")"*",
    "config: Domain/Security/AccessControl/Library[@initFunction]: init_test_access_control_all_ok*",
    "config: Domain/Security/AccessControl/Library[@finalizeFunction]: finalize_test_access_control_all_ok*",
    "config: Domain/Security/AccessControl/PermissionsCA/#text: file:Permissions_CA.pem*",
    "config: Domain/Security/AccessControl/Governance/#text: file:Governance.p7s*",
    "config: Domain/Security/AccessControl/Permissions/#text: file:Permissions.p7s*",
    "config: Domain/Security/Cryptographic/Library/#text: "WRAPPERLIB_PATH("dds_security_cryptography_wrapper")"*",
    "config: Domain/Security/Cryptographic/Library[@path]: "WRAPPERLIB_PATH("dds_security_cryptography_wrapper")"*",
    "config: Domain/Security/Cryptographic/Library[@initFunction]: init_test_cryptography_all_ok*",
    "config: Domain/Security/Cryptographic/Library[@finalizeFunction]: finalize_test_cryptography_all_ok*",
    /* The config should have been parsed into the participant QoS. */
    PARTICIPANT_QOS_ALL_OK ("", ",0:\"dds.sec.auth.password\":\"testtext_Password_testtext\",0:\"dds.sec.auth.trusted_ca_dir\":\"testtext_Dir_testtext\"", ""),
    NULL
  };

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
    "      <Governance>file:Governance.p7s</Governance>"
    "      <PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
    "      <Permissions>file:Permissions.p7s</Permissions>"
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

  /* All traces should have been provided. */
  CU_ASSERT_FATAL(found == 0x1fffff);
}

/* Expected traces when creating participant with the security elements. */
CU_Test(ddssec_config, qos, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain, participant;
  dds_qos_t * qos;
  const char *log_expected[] = {
    /* The config should have been parsed into the participant QoS. */
    PARTICIPANT_QOS_ALL_OK ("", ",0:\"dds.sec.auth.password\":\"testtext_Password_testtext\",0:\"dds.sec.auth.trusted_ca_dir\":\"file:/test/dir\"", ""),
    NULL
  };

  /* Create the qos */
  CU_ASSERT_FATAL((qos = dds_create_qos()) != NULL);
  dds_qset_prop(qos, "dds.sec.auth.library.path", ""WRAPPERLIB_PATH("dds_security_authentication_wrapper")"");
  dds_qset_prop(qos, "dds.sec.auth.library.init", "init_test_authentication_all_ok");
  dds_qset_prop(qos, "dds.sec.auth.library.finalize", "finalize_test_authentication_all_ok");
  dds_qset_prop(qos, "dds.sec.crypto.library.path", ""WRAPPERLIB_PATH("dds_security_cryptography_wrapper")"");
  dds_qset_prop(qos, "dds.sec.crypto.library.init", "init_test_cryptography_all_ok");
  dds_qset_prop(qos, "dds.sec.crypto.library.finalize", "finalize_test_cryptography_all_ok");
  dds_qset_prop(qos, "dds.sec.access.library.path", ""WRAPPERLIB_PATH("dds_security_access_control_wrapper")"");
  dds_qset_prop(qos, "dds.sec.access.library.init", "init_test_access_control_all_ok");
  dds_qset_prop(qos, "dds.sec.access.library.finalize", "finalize_test_access_control_all_ok");
  dds_qset_prop(qos, "dds.sec.auth.identity_ca", ""TEST_IDENTITY_CA_CERTIFICATE_DUMMY"");
  dds_qset_prop(qos, "dds.sec.auth.private_key", ""TEST_IDENTITY_PRIVATE_KEY_DUMMY"");
  dds_qset_prop(qos, "dds.sec.auth.identity_certificate", ""TEST_IDENTITY_CERTIFICATE_DUMMY"");
  dds_qset_prop(qos, "dds.sec.access.permissions_ca", "file:Permissions_CA.pem");
  dds_qset_prop(qos, "dds.sec.access.governance", "file:Governance.p7s");
  dds_qset_prop(qos, "dds.sec.access.permissions", "file:Permissions.p7s");
  dds_qset_prop(qos, "dds.sec.auth.password", "testtext_Password_testtext");
  dds_qset_prop(qos, "dds.sec.auth.trusted_ca_dir", "file:/test/dir");

  set_logger_exp(log_expected);
  domain = dds_create_domain(0, default_config);
  CU_ASSERT_FATAL(domain > 0);
  participant = dds_create_participant(0, qos, NULL);
  CU_ASSERT_FATAL(participant > 0);
  dds_delete(participant);
  dds_delete_qos(qos);
  dds_delete(domain);
  reset_logger();

  /* All traces should have been provided. */
  CU_ASSERT_FATAL(found == 0x1);
}

/* Expected traces when creating participant with the security elements. */
CU_Test(ddssec_config, qos_props, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain, participant;
  dds_qos_t * qos;
  const char *log_expected[] = {
    /* The config should have been parsed into the participant QoS. */
    PARTICIPANT_QOS_ALL_OK ("", ",0:\"dds.sec.auth.password\":\"testtext_Password_testtext\",0:\"dds.sec.auth.trusted_ca_dir\":\"file:/test/dir\",0:\"test.prop1\":\"testtext_value1_testtext\",0:\"test.prop2\":\"testtext_value2_testtext\"",
                            "0:\"test.bprop1\":3<1,2,3>"),
    NULL
  };

  /* Create the qos */
  unsigned char bvalue[3] = { 0x01, 0x02, 0x03 };
  CU_ASSERT_FATAL((qos = dds_create_qos()) != NULL);
  dds_qset_prop(qos, "dds.sec.auth.library.path", ""WRAPPERLIB_PATH("dds_security_authentication_wrapper")"");
  dds_qset_prop(qos, "dds.sec.auth.library.init", "init_test_authentication_all_ok");
  dds_qset_prop(qos, "dds.sec.auth.library.finalize", "finalize_test_authentication_all_ok");
  dds_qset_prop(qos, "dds.sec.crypto.library.path", ""WRAPPERLIB_PATH("dds_security_cryptography_wrapper")"");
  dds_qset_prop(qos, "dds.sec.crypto.library.init", "init_test_cryptography_all_ok");
  dds_qset_prop(qos, "dds.sec.crypto.library.finalize", "finalize_test_cryptography_all_ok");
  dds_qset_prop(qos, "dds.sec.access.library.path", ""WRAPPERLIB_PATH("dds_security_access_control_wrapper")"");
  dds_qset_prop(qos, "dds.sec.access.library.init", "init_test_access_control_all_ok");
  dds_qset_prop(qos, "dds.sec.access.library.finalize", "finalize_test_access_control_all_ok");
  dds_qset_prop(qos, "dds.sec.auth.identity_ca", TEST_IDENTITY_CA_CERTIFICATE_DUMMY);
  dds_qset_prop(qos, "dds.sec.auth.private_key", TEST_IDENTITY_PRIVATE_KEY_DUMMY);
  dds_qset_prop(qos, "dds.sec.auth.identity_certificate", TEST_IDENTITY_CERTIFICATE_DUMMY);
  dds_qset_prop(qos, "dds.sec.access.permissions_ca", "file:Permissions_CA.pem");
  dds_qset_prop(qos, "dds.sec.access.governance", "file:Governance.p7s");
  dds_qset_prop(qos, "dds.sec.access.permissions", "file:Permissions.p7s");
  dds_qset_prop(qos, "dds.sec.auth.password", "testtext_Password_testtext");
  dds_qset_prop(qos, "dds.sec.auth.trusted_ca_dir", "file:/test/dir");
  dds_qset_prop(qos, "test.prop1", "testtext_value1_testtext");
  dds_qset_prop(qos, "test.prop2", "testtext_value2_testtext");
  dds_qset_bprop(qos, "test.bprop1", bvalue, 3);

  set_logger_exp(log_expected);
  domain = dds_create_domain(0, default_config);
  CU_ASSERT_FATAL(domain > 0);
  participant = dds_create_participant(0, qos, NULL);
  CU_ASSERT_FATAL(participant > 0);
  dds_delete(participant);
  dds_delete_qos(qos);
  dds_delete(domain);
  reset_logger();

  /* All traces should have been provided. */
  CU_ASSERT_FATAL(found == 0x1);
}

/* Expect qos settings used when creating participant with config security elements and qos. */
CU_Test(ddssec_config, config_qos, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain, participant;
  dds_qos_t * qos;
  const char *log_expected[] = {
    /* The security settings from qos properties should have been parsed into the participant QoS. */
    "new_participant(*): using security settings from QoS*",
    PARTICIPANT_QOS ("init_test_authentication_all_ok", "finalize_test_authentication_all_ok", \
                   "init_test_cryptography_all_ok", "finalize_test_cryptography_all_ok", \
                   "init_test_access_control_all_ok", "finalize_test_access_control_all_ok", \
                   "file:QOS_Permissions_CA.pem", "file:QOS_Governance.p7s", "file:QOS_Permissions.p7s", \
                   "", "", ""),
    NULL
  };

  const char *sec_config =
    "<Domain id=\"any\">"
    "  <Discovery>"
    "    <Tag>${CYCLONEDDS_PID}</Tag>"
    "  </Discovery>"
    "  <Tracing><Verbosity>finest</></>"
    "  <Security>"
    "    <Authentication>"
    "      <IdentityCertificate>"TEST_IDENTITY_CERTIFICATE_DUMMY"</IdentityCertificate>"
    "      <IdentityCA>"TEST_IDENTITY_CA_CERTIFICATE_DUMMY"</IdentityCA>"
    "      <PrivateKey>"TEST_IDENTITY_PRIVATE_KEY_DUMMY"</PrivateKey>"
    "    </Authentication>"
    "    <AccessControl>"
    "      <Governance>file:Governance.p7s</Governance>"
    "      <PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
    "      <Permissions>file:Permissions.p7s</Permissions>"
    "    </AccessControl>"
    "  </Security>"
    "</Domain>";

  CU_ASSERT_FATAL((qos = dds_create_qos()) != NULL);
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_PATH, WRAPPERLIB_PATH("dds_security_authentication_wrapper"));
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_INIT, "init_test_authentication_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_FINALIZE, "finalize_test_authentication_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_PATH, WRAPPERLIB_PATH("dds_security_cryptography_wrapper"));
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_INIT, "init_test_cryptography_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_FINALIZE, "finalize_test_cryptography_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_PATH, WRAPPERLIB_PATH("dds_security_access_control_wrapper"));
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_INIT, "init_test_access_control_all_ok");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_FINALIZE, "finalize_test_access_control_all_ok");
  dds_qset_prop(qos, "dds.sec.auth.identity_ca", TEST_IDENTITY_CA_CERTIFICATE_DUMMY);
  dds_qset_prop(qos, "dds.sec.auth.private_key", TEST_IDENTITY_PRIVATE_KEY_DUMMY);
  dds_qset_prop(qos, "dds.sec.auth.identity_certificate", TEST_IDENTITY_CERTIFICATE_DUMMY);
  dds_qset_prop(qos, "dds.sec.access.permissions_ca", "file:QOS_Permissions_CA.pem");
  dds_qset_prop(qos, "dds.sec.access.governance", "file:QOS_Governance.p7s");
  dds_qset_prop(qos, "dds.sec.access.permissions", "file:QOS_Permissions.p7s");

  set_logger_exp(log_expected);
  domain = dds_create_domain(0, sec_config);
  CU_ASSERT_FATAL(domain > 0);
  participant = dds_create_participant(0, qos, NULL);
  CU_ASSERT_FATAL (participant > 0);
  dds_delete(participant);
  dds_delete_qos(qos);
  dds_delete(domain);
  reset_logger();

  /* All traces should have been provided. */
  CU_ASSERT_FATAL(found == 0x3);
}

/* Expect config used when creating participant with config security elements and
   qos containing only non-security properties. */
CU_Test(ddssec_config, other_prop, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain, participant;
  dds_qos_t * qos;
  const char *log_expected[] = {
    /* The security settings from config should have been parsed into the participant QoS. */
    PARTICIPANT_QOS_ALL_OK ("0:\"test.dds.sec.prop1\":\"testtext_value1_testtext\",", ",0:\"dds.sec.auth.password\":\"testtext_Password_testtext\",0:\"dds.sec.auth.trusted_ca_dir\":\"testtext_Dir_testtext\"", ""),
    NULL
  };

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
    "      <Governance>file:Governance.p7s</Governance>"
    "      <PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
    "      <Permissions>file:Permissions.p7s</Permissions>"
    "    </AccessControl>"
    "  </Security>"
    "</Domain>";

  CU_ASSERT_FATAL((qos = dds_create_qos()) != NULL);
  dds_qset_prop(qos, "test.dds.sec.prop1", "testtext_value1_testtext");

  set_logger_exp(log_expected);
  domain = dds_create_domain(0, sec_config);
  CU_ASSERT_FATAL(domain > 0);
  participant = dds_create_participant(0, qos, NULL);
  CU_ASSERT_FATAL (participant > 0);
  dds_delete(participant);
  dds_delete_qos(qos);
  dds_delete(domain);
  reset_logger();

  /* All traces should have been provided. */
  CU_ASSERT_FATAL(found == 0x1);
}

/* Expected traces when creating participant with the security elements. */
CU_Test(ddssec_config, qos_invalid, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain, participant;
  dds_qos_t * qos;
  const char *log_expected[] = {
    /* The config should have been parsed into the participant QoS. */
    "new_participant(*): using security settings from QoS*",
    "new_participant(*): required security property dds.sec.auth.identity_ca missing in Property QoS*",
    "new_participant(*): required security property dds.sec.auth.private_key missing in Property QoS*",
    "new_participant(*): required security property dds.sec.auth.identity_certificate missing in Property QoS*",
    "new_participant(*): required security property dds.sec.access.permissions_ca missing in Property QoS*",
    "new_participant(*): required security property dds.sec.access.governance missing in Property QoS*",
    "new_participant(*): required security property dds.sec.access.permissions missing in Property QoS*",
    "new_participant(*): required security property dds.sec.auth.library.path missing in Property QoS*",
    "new_participant(*): required security property dds.sec.auth.library.init missing in Property QoS*",
    "new_participant(*): required security property dds.sec.auth.library.finalize missing in Property QoS*",
    "new_participant(*): required security property dds.sec.crypto.library.path missing in Property QoS*",
    "new_participant(*): required security property dds.sec.crypto.library.init missing in Property QoS*",
    "new_participant(*): required security property dds.sec.crypto.library.finalize missing in Property QoS*",
    "new_participant(*): required security property dds.sec.access.library.path missing in Property QoS*",
    "new_participant(*): required security property dds.sec.access.library.init missing in Property QoS*",
    "new_participant(*): required security property dds.sec.access.library.finalize missing in Property QoS*",
    NULL
  };

  const char *sec_config =
    "<Domain id=\"any\">"
    "  <Discovery>"
    "    <Tag>${CYCLONEDDS_PID}</Tag>"
    "  </Discovery>"
    "  <Tracing><Verbosity>finest</></>"
    "  <Security>"
    "    <Authentication>"
    "      <IdentityCertificate>"TEST_IDENTITY_CERTIFICATE_DUMMY"</IdentityCertificate>"
    "      <IdentityCA>"TEST_IDENTITY_CA_CERTIFICATE_DUMMY"</IdentityCA>"
    "      <PrivateKey>"TEST_IDENTITY_PRIVATE_KEY_DUMMY"</PrivateKey>"
    "    </Authentication>"
    "    <AccessControl>"
    "      <Governance>file:Governance.p7s</Governance>"
    "      <PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
    "      <Permissions>file:Permissions.p7s</Permissions>"
    "    </AccessControl>"
    "  </Security>"
    "</Domain>";

  set_logger_exp(log_expected);

  CU_ASSERT_FATAL((qos = dds_create_qos()) != NULL);
  dds_qset_prop(qos, "dds.sec.dummy", "testtext_dummy_testtext");

  /* Create participant with security config in qos. */
  domain = dds_create_domain(0, sec_config);
  CU_ASSERT_FATAL(domain > 0);
  participant = dds_create_participant(0, qos, NULL);
  CU_ASSERT_EQUAL_FATAL(participant, DDS_RETCODE_ERROR);
  dds_delete_qos(qos);
  dds_delete(domain);
  reset_logger();

  /* All traces should have been provided. */
  CU_ASSERT_FATAL(found == 0xffff);
}
