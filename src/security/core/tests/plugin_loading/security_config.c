/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
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
#include "config_env.h"

#include "dds/version.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/q_xqos.h"

#include "dds/security/dds_security_api_defs.h"

#define MOCKLIB_PATH(name) \
  CONFIG_PLUGIN_MOCK_DIR CONFIG_LIB_SEP CONFIG_LIB_PREFIX name CONFIG_LIB_SUFFIX
#define MOCKLIB_ELEM_AUTH(name) \
  "<Library path=\"" MOCKLIB_PATH(name) "\"" \
  " initFunction=\"init_authentication\"" \
  " finalizeFunction=\"finalize_authentication\" />"
#define MOCKLIB_ELEM_CRYPTO(name) \
  "<Library path=\"" MOCKLIB_PATH(name) "\"" \
  " initFunction=\"init_crypto\"" \
  " finalizeFunction=\"finalize_crypto\" />"
#define MOCKLIB_ELEM_ACCESS_CONTROL(name) \
  "<Library path=\"" MOCKLIB_PATH(name) "\"" \
  " initFunction=\"init_access_control\"" \
  " finalizeFunction=\"finalize_access_control\" />"

#define URI_VARIABLE DDS_PROJECT_NAME_NOSPACE_CAPS"_URI"

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
  for (uint32_t i = 0; expected[i] != NULL; i++) {
    if (ddsi2_patmatch(expected[i], data->message)) {
      found |= (uint32_t)(1 << i);
    }
  }
}


CU_Test(ddsc_security_config, empty, .init = ddsrt_init, .fini = ddsrt_fini)
{
  /* Expected traces when creating participant with an empty security element.  We need to
     test this one here to be sure that it refuses to start when security is configured
     but the implementation doesn't include support for it. */
  const char *log_expected[] = {
    "config: //CycloneDDS/Domain/DDSSecurity/Authentication/IdentityCertificate/#text: element missing in configuration*",
    "config: //CycloneDDS/Domain/DDSSecurity/Authentication/IdentityCA/#text: element missing in configuration*",
    "config: //CycloneDDS/Domain/DDSSecurity/Authentication/PrivateKey/#text: element missing in configuration*",
    NULL
  };

  dds_entity_t participant;

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_FATAL|DDS_LC_ERROR|DDS_LC_WARNING|DDS_LC_CONFIG);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create participant with an empty security element. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, "<DDSSecurity/>");
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  ddsrt_setenv(URI_VARIABLE, "");
  CU_ASSERT_FATAL(participant < 0);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);

  /* All traces should have been provided. */
  CU_ASSERT_FATAL(found == 0x7);
}

CU_Test(ddsc_security_config, non, .init = ddsrt_init, .fini = ddsrt_fini)
{
  /* There shouldn't be traces that mention security. */
  const char *log_expected[] = {
    "*Security*",
    NULL
  };

  dds_entity_t participant;

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_FATAL|DDS_LC_ERROR|DDS_LC_WARNING|DDS_LC_CONFIG);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create participant with an empty security element. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, "<Tracing><Verbosity>finest</></>");
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  ddsrt_setenv(URI_VARIABLE, "");
  CU_ASSERT_FATAL(participant > 0);
  dds_delete(participant);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);

  /* No security traces should have been provided. */
  CU_ASSERT_FATAL(found == 0x0);
}

CU_Test(ddsc_security_config, missing, .init = ddsrt_init, .fini = ddsrt_fini)
{
  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
    "config: //CycloneDDS/Domain/DDSSecurity/Authentication/IdentityCertificate/#text: element missing in configuration*",
    "config: //CycloneDDS/Domain/DDSSecurity/Authentication/IdentityCA/#text: element missing in configuration*",
    "config: //CycloneDDS/Domain/DDSSecurity/Authentication/PrivateKey/#text: element missing in configuration*",
    NULL
  };

  /* IdentityCertificate, IdentityCA and PrivateKey values or elements are missing. */
  const char *sec_config =
    "<Tracing><Verbosity>finest</></>"
    "<DDSSecurity>"
    "  <Authentication>"
    "    "MOCKLIB_ELEM_AUTH("dds_security_authentication_all_ok")
    "    <IdentityCertificate></IdentityCertificate>"
    "    <PrivateKey></PrivateKey>"
    "    <Password>testtext_Password_testtext</Password>"
    "  </Authentication>"
    "  <Cryptographic>"
    "    "MOCKLIB_ELEM_CRYPTO("dds_security_cryptography_all_ok")
    "  </Cryptographic>"
    "  <AccessControl>"
    "    "MOCKLIB_ELEM_ACCESS_CONTROL("dds_security_access_control_all_ok")
    "    <Governance>file:Governance.p7s</Governance>"
    "    <PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
    "    <Permissions>file:Permissions.p7s</Permissions>"
    "  </AccessControl>"
    "</DDSSecurity>";

  dds_entity_t participant;

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_FATAL|DDS_LC_ERROR|DDS_LC_WARNING|DDS_LC_CONFIG);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create participant with an empty security element. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, sec_config);
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  ddsrt_setenv(URI_VARIABLE, "");
  CU_ASSERT_FATAL(participant < 0);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);
  /* All traces should have been provided. */
  CU_ASSERT_FATAL(found == 0x7);
}

CU_Test(ddsc_security_config, all, .init = ddsrt_init, .fini = ddsrt_fini)
{
  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
    "config: Domain/DDSSecurity/Authentication/Library/#text: "MOCKLIB_PATH("dds_security_authentication_all_ok")"*",
    "config: Domain/DDSSecurity/Authentication/Library[@path]: "MOCKLIB_PATH("dds_security_authentication_all_ok")"*",
    "config: Domain/DDSSecurity/Authentication/Library[@initFunction]: init_authentication*",
    "config: Domain/DDSSecurity/Authentication/Library[@finalizeFunction]: finalize_authentication*",
    "config: Domain/DDSSecurity/Authentication/IdentityCertificate/#text: testtext_IdentityCertificate_testtext*",
    "config: Domain/DDSSecurity/Authentication/IdentityCA/#text: testtext_IdentityCA_testtext*",
    "config: Domain/DDSSecurity/Authentication/PrivateKey/#text: testtext_PrivateKey_testtext*",
    "config: Domain/DDSSecurity/Authentication/Password/#text: testtext_Password_testtext*",
    "config: Domain/DDSSecurity/Authentication/TrustedCADirectory/#text: testtext_Dir_testtext*",
    "config: Domain/DDSSecurity/AccessControl/Library/#text: "MOCKLIB_PATH("dds_security_access_control_all_ok")"*",
    "config: Domain/DDSSecurity/AccessControl/Library[@path]: "MOCKLIB_PATH("dds_security_access_control_all_ok")"*",
    "config: Domain/DDSSecurity/AccessControl/Library[@initFunction]: init_access_control*",
    "config: Domain/DDSSecurity/AccessControl/Library[@finalizeFunction]: finalize_access_control*",
    "config: Domain/DDSSecurity/AccessControl/PermissionsCA/#text: file:Permissions_CA.pem*",
    "config: Domain/DDSSecurity/AccessControl/Governance/#text: file:Governance.p7s*",
    "config: Domain/DDSSecurity/AccessControl/Permissions/#text: file:Permissions.p7s*",
    "config: Domain/DDSSecurity/Cryptographic/Library/#text: "MOCKLIB_PATH("dds_security_cryptography_all_ok")"*",
    "config: Domain/DDSSecurity/Cryptographic/Library[@path]: "MOCKLIB_PATH("dds_security_cryptography_all_ok")"*",
    "config: Domain/DDSSecurity/Cryptographic/Library[@initFunction]: init_crypto*",
    "config: Domain/DDSSecurity/Cryptographic/Library[@finalizeFunction]: finalize_crypto*",
    /* The config should have been parsed into the participant QoS. */
    "PARTICIPANT * QOS={*property_list={value={{dds.sec.auth.library.path,"MOCKLIB_PATH("dds_security_authentication_all_ok")",0},"
    "{dds.sec.auth.library.init,init_authentication,0},"
    "{dds.sec.auth.library.finalize,finalize_authentication,0},"
    "{dds.sec.crypto.library.path,"MOCKLIB_PATH("dds_security_cryptography_all_ok")",0},"
    "{dds.sec.crypto.library.init,init_crypto,0},"
    "{dds.sec.crypto.library.finalize,finalize_crypto,0},"
    "{dds.sec.access.library.path,"MOCKLIB_PATH("dds_security_access_control_all_ok")",0},"
    "{dds.sec.access.library.init,init_access_control,0},"
    "{dds.sec.access.library.finalize,finalize_access_control,0},"
    "{dds.sec.auth.identity_ca,testtext_IdentityCA_testtext,0},"
    "{dds.sec.auth.private_key,testtext_PrivateKey_testtext,0},"
    "{dds.sec.auth.identity_certificate,testtext_IdentityCertificate_testtext,0},"
    "{dds.sec.access.permissions_ca,file:Permissions_CA.pem,0},"
    "{dds.sec.access.governance,file:Governance.p7s,0},"
    "{dds.sec.access.permissions,file:Permissions.p7s,0},"
    "{dds.sec.auth.password,testtext_Password_testtext,0},"
    "{dds.sec.auth.trusted_ca_dir,testtext_Dir_testtext,0}}binary_value={}}*}*",
    NULL
  };
  const char *sec_config =
    "<"DDS_PROJECT_NAME">"
    "  <Domain id=\"any\">"
    "    <Tracing><Verbosity>finest</></>"
    "    <DDSSecurity>"
    "      <Authentication>"
    "        "MOCKLIB_ELEM_AUTH("dds_security_authentication_all_ok")
    "        <IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
    "        <IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
    "        <PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
    "        <Password>testtext_Password_testtext</Password>"
    "        <TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
    "      </Authentication>"
    "      <Cryptographic>"
    "        "MOCKLIB_ELEM_CRYPTO("dds_security_cryptography_all_ok")
    "      </Cryptographic>"
    "      <AccessControl>"
    "        "MOCKLIB_ELEM_ACCESS_CONTROL("dds_security_access_control_all_ok")
    "        <Governance>file:Governance.p7s</Governance>"
    "        <PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
    "        <Permissions>file:Permissions.p7s</Permissions>"
    "      </AccessControl>"
    "    </DDSSecurity>"
    "  </Domain>"
    "</"DDS_PROJECT_NAME">";

  dds_entity_t participant;

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_FATAL|DDS_LC_ERROR|DDS_LC_WARNING|DDS_LC_CONFIG);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create participant with security elements. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, sec_config);
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);
  ddsrt_setenv(URI_VARIABLE, "");
  dds_delete(participant);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);

  /* All traces should have been provided. */
  CU_ASSERT_FATAL(found == 0x1fffff);
}

CU_Test(ddsc_security_config, security, .init = ddsrt_init, .fini = ddsrt_fini)
{
  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
    "config: Domain/DDSSecurity/Authentication/Library/#text: "MOCKLIB_PATH("dds_security_authentication_all_ok")"*",
    "config: Domain/DDSSecurity/Authentication/Library[@path]: "MOCKLIB_PATH("dds_security_authentication_all_ok")"*",
    "config: Domain/DDSSecurity/Authentication/Library[@initFunction]: init_authentication*",
    "config: Domain/DDSSecurity/Authentication/Library[@finalizeFunction]: finalize_authentication*",
    "config: Domain/DDSSecurity/Authentication/IdentityCertificate/#text: testtext_IdentityCertificate_testtext*",
    "config: Domain/DDSSecurity/Authentication/IdentityCA/#text: testtext_IdentityCA_testtext*",
    "config: Domain/DDSSecurity/Authentication/PrivateKey/#text: testtext_PrivateKey_testtext*",
    "config: Domain/DDSSecurity/Authentication/Password/#text:  {}*",
    "config: Domain/DDSSecurity/Authentication/TrustedCADirectory/#text:  {}*",
    "config: Domain/DDSSecurity/AccessControl/Library/#text: "MOCKLIB_PATH("dds_security_access_control_all_ok")"*",
    "config: Domain/DDSSecurity/AccessControl/Library[@path]: "MOCKLIB_PATH("dds_security_access_control_all_ok")"*",
    "config: Domain/DDSSecurity/AccessControl/Library[@initFunction]: init_access_control*",
    "config: Domain/DDSSecurity/AccessControl/Library[@finalizeFunction]: finalize_access_control*",
    "config: Domain/DDSSecurity/AccessControl/PermissionsCA/#text: file:Permissions_CA.pem*",
    "config: Domain/DDSSecurity/AccessControl/Governance/#text: file:Governance.p7s*",
    "config: Domain/DDSSecurity/AccessControl/Permissions/#text: file:Permissions.p7s*",
    "config: Domain/DDSSecurity/Cryptographic/Library/#text: "MOCKLIB_PATH("dds_security_cryptography_all_ok")"*",
    "config: Domain/DDSSecurity/Cryptographic/Library[@path]: "MOCKLIB_PATH("dds_security_cryptography_all_ok")"*",
    "config: Domain/DDSSecurity/Cryptographic/Library[@initFunction]: init_crypto*",
    "config: Domain/DDSSecurity/Cryptographic/Library[@finalizeFunction]: finalize_crypto*",
    /* The config should have been parsed into the participant QoS. */
    "PARTICIPANT * QOS={*property_list={value={{dds.sec.auth.library.path,"MOCKLIB_PATH("dds_security_authentication_all_ok")",0},"
    "{dds.sec.auth.library.init,init_authentication,0},"
    "{dds.sec.auth.library.finalize,finalize_authentication,0},"
    "{dds.sec.crypto.library.path,"MOCKLIB_PATH("dds_security_cryptography_all_ok")",0},"
    "{dds.sec.crypto.library.init,init_crypto,0},"
    "{dds.sec.crypto.library.finalize,finalize_crypto,0},"
    "{dds.sec.access.library.path,"MOCKLIB_PATH("dds_security_access_control_all_ok")",0},"
    "{dds.sec.access.library.init,init_access_control,0},"
    "{dds.sec.access.library.finalize,finalize_access_control,0},"
    "{dds.sec.auth.identity_ca,testtext_IdentityCA_testtext,0},"
    "{dds.sec.auth.private_key,testtext_PrivateKey_testtext,0},"
    "{dds.sec.auth.identity_certificate,testtext_IdentityCertificate_testtext,0},"
    "{dds.sec.access.permissions_ca,file:Permissions_CA.pem,0},"
    "{dds.sec.access.governance,file:Governance.p7s,0},"
    "{dds.sec.access.permissions,file:Permissions.p7s,0},"
    "{dds.sec.auth.password,,0},"
    "{dds.sec.auth.trusted_ca_dir,,0}}binary_value={}}*}*",
    NULL
  };

  const char *sec_config =
    "<Tracing><Verbosity>finest</></>"
    "<DDSSecurity>"
    "  <Authentication>"
    "    "MOCKLIB_ELEM_AUTH("dds_security_authentication_all_ok")
    "    <IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
    "    <IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
    "    <PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
    "  </Authentication>"
    "  <Cryptographic>"
    "    "MOCKLIB_ELEM_CRYPTO("dds_security_cryptography_all_ok")
    "  </Cryptographic>"
    "  <AccessControl>"
    "    "MOCKLIB_ELEM_ACCESS_CONTROL("dds_security_access_control_all_ok")
    "    <Governance>file:Governance.p7s</Governance>"
    "    <PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
    "    <Permissions>file:Permissions.p7s</Permissions>"
    "  </AccessControl>"
    "</DDSSecurity>";

  dds_entity_t participant;

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_FATAL|DDS_LC_ERROR|DDS_LC_WARNING|DDS_LC_CONFIG);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create participant with security elements. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, sec_config);
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);
  ddsrt_setenv(URI_VARIABLE, "");
  dds_delete(participant);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);

  /* All traces should have been provided. */
  CU_ASSERT_FATAL(found == 0x1fffff);
}

CU_Test(ddsc_security_config, deprecated, .init = ddsrt_init, .fini = ddsrt_fini)
{
  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
    "config: Domain/DDSSecurity/Authentication/Library/#text: "MOCKLIB_PATH("dds_security_authentication_all_ok")"*",
    "config: Domain/DDSSecurity/Authentication/Library[@path]: "MOCKLIB_PATH("dds_security_authentication_all_ok")"*",
    "config: Domain/DDSSecurity/Authentication/Library[@initFunction]: init_authentication*",
    "config: Domain/DDSSecurity/Authentication/Library[@finalizeFunction]: finalize_authentication*",
    "config: Domain/DDSSecurity/Authentication/IdentityCertificate/#text: testtext_IdentityCertificate_testtext*",
    "config: Domain/DDSSecurity/Authentication/IdentityCA/#text: testtext_IdentityCA_testtext*",
    "config: Domain/DDSSecurity/Authentication/PrivateKey/#text: testtext_PrivateKey_testtext*",
    "config: Domain/DDSSecurity/Authentication/Password/#text: testtext_Password_testtext*",
    "config: Domain/DDSSecurity/Authentication/TrustedCADirectory/#text: testtext_Dir_testtext*",
    "config: Domain/DDSSecurity/AccessControl/Library/#text: "MOCKLIB_PATH("dds_security_access_control_all_ok")"*",
    "config: Domain/DDSSecurity/AccessControl/Library[@path]: "MOCKLIB_PATH("dds_security_access_control_all_ok")"*",
    "config: Domain/DDSSecurity/AccessControl/Library[@initFunction]: init_access_control*",
    "config: Domain/DDSSecurity/AccessControl/Library[@finalizeFunction]: finalize_access_control*",
    "config: Domain/DDSSecurity/AccessControl/PermissionsCA/#text: file:Permissions_CA.pem*",
    "config: Domain/DDSSecurity/AccessControl/Governance/#text: file:Governance.p7s*",
    "config: Domain/DDSSecurity/AccessControl/Permissions/#text: file:Permissions.p7s*",
    "config: Domain/DDSSecurity/Cryptographic/Library/#text: "MOCKLIB_PATH("dds_security_cryptography_all_ok")"*",
    "config: Domain/DDSSecurity/Cryptographic/Library[@path]: "MOCKLIB_PATH("dds_security_cryptography_all_ok")"*",
    "config: Domain/DDSSecurity/Cryptographic/Library[@initFunction]: init_crypto*",
    "config: Domain/DDSSecurity/Cryptographic/Library[@finalizeFunction]: finalize_crypto*",
    /* The config should have been parsed into the participant QoS. */
    "PARTICIPANT * QOS={*property_list={value={"
    "{dds.sec.auth.library.path,"MOCKLIB_PATH("dds_security_authentication_all_ok")",0},"
    "{dds.sec.auth.library.init,init_authentication,0},"
    "{dds.sec.auth.library.finalize,finalize_authentication,0},"
    "{dds.sec.crypto.library.path,"MOCKLIB_PATH("dds_security_cryptography_all_ok")",0},"
    "{dds.sec.crypto.library.init,init_crypto,0},"
    "{dds.sec.crypto.library.finalize,finalize_crypto,0},"
    "{dds.sec.access.library.path,"MOCKLIB_PATH("dds_security_access_control_all_ok")",0},"
    "{dds.sec.access.library.init,init_access_control,0},{dds.sec.access.library.finalize,finalize_access_control,0},{dds.sec.auth.identity_ca,testtext_IdentityCA_testtext,0},"
    "{dds.sec.auth.private_key,testtext_PrivateKey_testtext,0},"
    "{dds.sec.auth.identity_certificate,testtext_IdentityCertificate_testtext,0},"
    "{dds.sec.access.permissions_ca,file:Permissions_CA.pem,0},"
    "{dds.sec.access.governance,file:Governance.p7s,0},"
    "{dds.sec.access.permissions,file:Permissions.p7s,0},"
    "{dds.sec.auth.password,testtext_Password_testtext,0},"
    "{dds.sec.auth.trusted_ca_dir,testtext_Dir_testtext,0}}binary_value={}}*}*",
    NULL
  };

  const char *sec_config =
    "<"DDS_PROJECT_NAME">"
    "  <Domain id=\"any\">"
    "    <DDSSecurity>"
    "      <Authentication>"
    "        "MOCKLIB_ELEM_AUTH("dds_security_authentication_all_ok")
    "        <IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
    "        <IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
    "        <PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
    "        <Password>testtext_Password_testtext</Password>"
    "        <TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
    "      </Authentication>"
    "      <Cryptographic>"
    "        "MOCKLIB_ELEM_CRYPTO("dds_security_cryptography_all_ok")
    "      </Cryptographic>"
    "      <AccessControl>"
    "        "MOCKLIB_ELEM_ACCESS_CONTROL("dds_security_access_control_all_ok")
    "        <Governance>file:Governance.p7s</Governance>"
    "        <PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
    "        <Permissions>file:Permissions.p7s</Permissions>"
    "      </AccessControl>"
    "    </DDSSecurity>"
    "    <Tracing><Verbosity>finest</></>"
    "  </Domain>"
    "</"DDS_PROJECT_NAME">";

  dds_entity_t participant;

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_FATAL|DDS_LC_ERROR|DDS_LC_WARNING|DDS_LC_CONFIG);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create participant with security elements. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, sec_config);
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);
  ddsrt_setenv(URI_VARIABLE, "");
  dds_delete(participant);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);

  /* All traces should have been provided. */
  CU_ASSERT_FATAL(found == 0x1fffff);
}

CU_Test(ddsc_security_config, qos, .init = ddsrt_init, .fini = ddsrt_fini)
{
  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
    /* The config should have been parsed into the participant QoS. */
    "PARTICIPANT * QOS={*property_list={value={"
    "{dds.sec.auth.identity_ca,testtext_IdentityCA_testtext,0},"
    "{dds.sec.auth.private_key,testtext_PrivateKey_testtext,0},"
    "{dds.sec.auth.identity_certificate,testtext_IdentityCertificate_testtext,0},"
    "{dds.sec.access.permissions_ca,file:Permissions_CA.pem,0},"
    "{dds.sec.access.governance,file:Governance.p7s,0},"
    "{dds.sec.access.permissions,file:Permissions.p7s,0},"
    "{dds.sec.auth.password,testtext_Password_testtext,0},"
    "{dds.sec.auth.trusted_ca_dir,file:/test/dir,0},"
    "{dds.sec.auth.library.path,"MOCKLIB_PATH("dds_security_authentication_all_ok")",0},"
    "{dds.sec.auth.library.init,init_authentication,0},"
    "{dds.sec.auth.library.finalize,finalize_authentication,0},"
    "{dds.sec.crypto.library.path,"MOCKLIB_PATH("dds_security_cryptography_all_ok")",0},"
    "{dds.sec.crypto.library.init,init_crypto,0},"
    "{dds.sec.crypto.library.finalize,finalize_crypto,0},"
    "{dds.sec.access.library.path,"MOCKLIB_PATH("dds_security_access_control_all_ok")",0},"
    "{dds.sec.access.library.init,init_access_control,0},"
    "{dds.sec.access.library.finalize,finalize_access_control,0}}binary_value={}}*}*",
    NULL
  };

  dds_entity_t participant;
  dds_qos_t * qos;

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_FATAL|DDS_LC_ERROR|DDS_LC_WARNING|DDS_LC_CONFIG);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create the qos */
  CU_ASSERT_FATAL((qos = dds_create_qos()) != NULL);
  dds_qset_prop(qos, "dds.sec.auth.identity_ca", "testtext_IdentityCA_testtext");
  dds_qset_prop(qos, "dds.sec.auth.private_key", "testtext_PrivateKey_testtext");
  dds_qset_prop(qos, "dds.sec.auth.identity_certificate", "testtext_IdentityCertificate_testtext");
  dds_qset_prop(qos, "dds.sec.access.permissions_ca", "file:Permissions_CA.pem");
  dds_qset_prop(qos, "dds.sec.access.governance", "file:Governance.p7s");
  dds_qset_prop(qos, "dds.sec.access.permissions", "file:Permissions.p7s");
  dds_qset_prop(qos, "dds.sec.auth.password", "testtext_Password_testtext");
  dds_qset_prop(qos, "dds.sec.auth.trusted_ca_dir", "file:/test/dir");
  dds_qset_prop(qos, "dds.sec.auth.library.path", ""MOCKLIB_PATH("dds_security_authentication_all_ok")"");
  dds_qset_prop(qos, "dds.sec.auth.library.init", "init_authentication");
  dds_qset_prop(qos, "dds.sec.auth.library.finalize", "finalize_authentication");
  dds_qset_prop(qos, "dds.sec.crypto.library.path", ""MOCKLIB_PATH("dds_security_cryptography_all_ok")"");
  dds_qset_prop(qos, "dds.sec.crypto.library.init", "init_crypto");
  dds_qset_prop(qos, "dds.sec.crypto.library.finalize", "finalize_crypto");
  dds_qset_prop(qos, "dds.sec.access.library.path", ""MOCKLIB_PATH("dds_security_access_control_all_ok")"");
  dds_qset_prop(qos, "dds.sec.access.library.init", "init_access_control");
  dds_qset_prop(qos, "dds.sec.access.library.finalize", "finalize_access_control");

  /* Create participant with security config in qos. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, "<Tracing><Verbosity>finest</></>");
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, qos, NULL);
  CU_ASSERT_FATAL(participant > 0);
  ddsrt_setenv(URI_VARIABLE, "");
  dds_delete(participant);
  dds_delete_qos(qos);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);

  /* All traces should have been provided. */
  CU_ASSERT_FATAL(found == 0x1);
}

CU_Test(ddsc_security_config, qos_props, .init = ddsrt_init, .fini = ddsrt_fini)
{
  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
    /* The config should have been parsed into the participant QoS. */
    "PARTICIPANT * QOS={*property_list={value={"
    "{test.prop1,testtext_value1_testtext,0},"
    "{dds.sec.auth.identity_ca,testtext_IdentityCA_testtext,0},"
    "{dds.sec.auth.private_key,testtext_PrivateKey_testtext,0},"
    "{dds.sec.auth.identity_certificate,testtext_IdentityCertificate_testtext,0},"
    "{dds.sec.access.permissions_ca,file:Permissions_CA.pem,0},"
    "{dds.sec.access.governance,file:Governance.p7s,0},"
    "{dds.sec.access.permissions,file:Permissions.p7s,0},"
    "{dds.sec.auth.password,testtext_Password_testtext,0},"
    "{dds.sec.auth.trusted_ca_dir,file:/test/dir,0},"
    "{dds.sec.auth.library.path,"MOCKLIB_PATH("dds_security_authentication_all_ok")",0},"
    "{dds.sec.auth.library.init,init_authentication,0},"
    "{dds.sec.auth.library.finalize,finalize_authentication,0},"
    "{dds.sec.crypto.library.path,"MOCKLIB_PATH("dds_security_cryptography_all_ok")",0},"
    "{dds.sec.crypto.library.init,init_crypto,0},"
    "{dds.sec.crypto.library.finalize,finalize_crypto,0},"
    "{dds.sec.access.library.path,"MOCKLIB_PATH("dds_security_access_control_all_ok")",0},"
    "{dds.sec.access.library.init,init_access_control,0},"
    "{dds.sec.access.library.finalize,finalize_access_control,0},"
    "{test.prop2,testtext_value2_testtext,0}}"
    "binary_value={{test.bprop1,(3,*),0}}}*}*",
    NULL
  };

  dds_entity_t participant;
  dds_qos_t * qos;

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_FATAL | DDS_LC_ERROR | DDS_LC_WARNING | DDS_LC_CONFIG);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create the qos */
  unsigned char bvalue[3] = { 0x01, 0x02, 0x03 };
  CU_ASSERT_FATAL((qos = dds_create_qos()) != NULL);
  dds_qset_prop(qos, "test.prop1", "testtext_value1_testtext");
  dds_qset_prop(qos, "dds.sec.auth.identity_ca", "testtext_IdentityCA_testtext");
  dds_qset_prop(qos, "dds.sec.auth.private_key", "testtext_PrivateKey_testtext");
  dds_qset_prop(qos, "dds.sec.auth.identity_certificate", "testtext_IdentityCertificate_testtext");
  dds_qset_prop(qos, "dds.sec.access.permissions_ca", "file:Permissions_CA.pem");
  dds_qset_prop(qos, "dds.sec.access.governance", "file:Governance.p7s");
  dds_qset_prop(qos, "dds.sec.access.permissions", "file:Permissions.p7s");
  dds_qset_prop(qos, "dds.sec.auth.password", "testtext_Password_testtext");
  dds_qset_prop(qos, "dds.sec.auth.trusted_ca_dir", "file:/test/dir");

  dds_qset_prop(qos, "dds.sec.auth.library.path", ""MOCKLIB_PATH("dds_security_authentication_all_ok")"");
  dds_qset_prop(qos, "dds.sec.auth.library.init", "init_authentication");
  dds_qset_prop(qos, "dds.sec.auth.library.finalize", "finalize_authentication");
  dds_qset_prop(qos, "dds.sec.crypto.library.path", ""MOCKLIB_PATH("dds_security_cryptography_all_ok")"");
  dds_qset_prop(qos, "dds.sec.crypto.library.init", "init_crypto");
  dds_qset_prop(qos, "dds.sec.crypto.library.finalize", "finalize_crypto");
  dds_qset_prop(qos, "dds.sec.access.library.path", ""MOCKLIB_PATH("dds_security_access_control_all_ok")"");
  dds_qset_prop(qos, "dds.sec.access.library.init", "init_access_control");
  dds_qset_prop(qos, "dds.sec.access.library.finalize", "finalize_access_control");

  dds_qset_prop(qos, "test.prop2", "testtext_value2_testtext");

  dds_qset_prop(qos, "dds.sec.auth.identity_ca", "testtext_IdentityCA_testtext");

  dds_qset_bprop(qos, "test.bprop1", bvalue, 3);

  /* Create participant with security config in qos. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, "<Tracing><Verbosity>finest</></>");
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, qos, NULL);
  CU_ASSERT_FATAL(participant > 0);
  ddsrt_setenv(URI_VARIABLE, "");
  dds_delete(participant);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);
  dds_delete_qos(qos);

  /* All traces should have been provided. */
  CU_ASSERT_FATAL(found == 0x1);
}

CU_Test(ddsc_security_config, config_qos, .init = ddsrt_init, .fini = ddsrt_fini)
{
  /* Expect qos settings used when creating participant with config security elements and qos. */
  const char *log_expected[] = {
    /* The security settings from qos properties should have been parsed into the participant QoS. */
    "new_participant(*): using security settings from QoS*",
    "PARTICIPANT * QOS={*property_list={value={"
    "{dds.sec.auth.identity_ca,testtext_QOS_IdentityCA_testtext,0},"
    "{dds.sec.auth.private_key,testtext_QOS_PrivateKey_testtext,0},"
    "{dds.sec.auth.identity_certificate,testtext_QOS_IdentityCertificate_testtext,0},"
    "{dds.sec.access.permissions_ca,file:QOS_Permissions_CA.pem,0},"
    "{dds.sec.access.governance,file:QOS_Governance.p7s,0},"
    "{dds.sec.access.permissions,file:QOS_Permissions.p7s,0},"
    "{dds.sec.auth.library.path,"MOCKLIB_PATH("dds_security_authentication_all_ok")",0},"
    "{dds.sec.auth.library.init,init_authentication,0},"
    "{dds.sec.auth.library.finalize,finalize_authentication,0},"
    "{dds.sec.crypto.library.path,"MOCKLIB_PATH("dds_security_cryptography_all_ok")",0},"
    "{dds.sec.crypto.library.init,init_crypto,0},"
    "{dds.sec.crypto.library.finalize,finalize_crypto,0},"
    "{dds.sec.access.library.path,"MOCKLIB_PATH("dds_security_access_control_all_ok")",0},"
    "{dds.sec.access.library.init,init_access_control,0},"
    "{dds.sec.access.library.finalize,finalize_access_control,0}"
    "}binary_value={}}*}*",
    NULL
  };

  const char *sec_config =
    "<Tracing><Verbosity>finest</></>"
    "<DDSSecurity>"
    "  <Authentication>"
    "    <IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
    "    <IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
    "    <PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
    "  </Authentication>"
    "  <AccessControl>"
    "    <Governance>file:Governance.p7s</Governance>"
    "    <PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
    "    <Permissions>file:Permissions.p7s</Permissions>"
    "  </AccessControl>"
    "</DDSSecurity>";

  dds_entity_t participant;
  dds_qos_t * qos;

  CU_ASSERT_FATAL((qos = dds_create_qos()) != NULL);
  dds_qset_prop(qos, "dds.sec.auth.identity_ca", "testtext_QOS_IdentityCA_testtext");
  dds_qset_prop(qos, "dds.sec.auth.private_key", "testtext_QOS_PrivateKey_testtext");
  dds_qset_prop(qos, "dds.sec.auth.identity_certificate", "testtext_QOS_IdentityCertificate_testtext");
  dds_qset_prop(qos, "dds.sec.access.permissions_ca", "file:QOS_Permissions_CA.pem");
  dds_qset_prop(qos, "dds.sec.access.governance", "file:QOS_Governance.p7s");
  dds_qset_prop(qos, "dds.sec.access.permissions", "file:QOS_Permissions.p7s");

  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_PATH, ""MOCKLIB_PATH("dds_security_authentication_all_ok")"");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_INIT, "init_authentication");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_FINALIZE, "finalize_authentication");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_PATH, ""MOCKLIB_PATH("dds_security_cryptography_all_ok")"");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_INIT, "init_crypto");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_FINALIZE, "finalize_crypto");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_PATH, ""MOCKLIB_PATH("dds_security_access_control_all_ok")"");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_INIT, "init_access_control");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_FINALIZE, "finalize_access_control");

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_FATAL | DDS_LC_ERROR | DDS_LC_WARNING | DDS_LC_CONFIG);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create participant with security elements. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, sec_config);
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, qos, NULL);
  CU_ASSERT_FATAL (participant > 0);
  ddsrt_setenv(URI_VARIABLE, "");
  dds_delete(participant);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);
  dds_delete_qos(qos);

  /* All traces should have been provided. */
  CU_ASSERT_FATAL(found == 0x3);
}

CU_Test(ddsc_security_config, other_prop, .init = ddsrt_init, .fini = ddsrt_fini)
{
  /* Expect config used when creating participant with config security elements and
   * qos containing only non-security properties. */
  const char *log_expected[] = {
    /* The security settings from config should have been parsed into the participant QoS. */
    "PARTICIPANT * QOS={*property_list={value={{test.dds.sec.prop1,testtext_value1_testtext,0},"
    "{dds.sec.auth.library.path,"MOCKLIB_PATH("dds_security_authentication_all_ok")",0},"
    "{dds.sec.auth.library.init,init_authentication,0},"
    "{dds.sec.auth.library.finalize,finalize_authentication,0},"
    "{dds.sec.crypto.library.path,"MOCKLIB_PATH("dds_security_cryptography_all_ok")",0},"
    "{dds.sec.crypto.library.init,init_crypto,0},"
    "{dds.sec.crypto.library.finalize,finalize_crypto,0},"
    "{dds.sec.access.library.path,"MOCKLIB_PATH("dds_security_access_control_all_ok")",0},"
    "{dds.sec.access.library.init,init_access_control,0},"
    "{dds.sec.access.library.finalize,finalize_access_control,0},"
    "{dds.sec.auth.identity_ca,testtext_IdentityCA_testtext,0},"
    "{dds.sec.auth.private_key,testtext_PrivateKey_testtext,0},"
    "{dds.sec.auth.identity_certificate,testtext_IdentityCertificate_testtext,0},"
    "{dds.sec.access.permissions_ca,file:Permissions_CA.pem,0},"
    "{dds.sec.access.governance,file:Governance.p7s,0},"
    "{dds.sec.access.permissions,file:Permissions.p7s,0},"
    "{dds.sec.auth.password,testtext_Password_testtext,0},"
    "{dds.sec.auth.trusted_ca_dir,testtext_Dir_testtext,0}}binary_value={}}*}*",
    NULL
  };

  const char *sec_config =
    "<Tracing><Verbosity>finest</></>"
    "<DDSSecurity>"
    "  <Authentication>"
    "    "MOCKLIB_ELEM_AUTH("dds_security_authentication_all_ok")
    "    <IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
    "    <IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
    "    <PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
    "    <Password>testtext_Password_testtext</Password>"
    "    <TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
    "  </Authentication>"
    "  <Cryptographic>"
    "    "MOCKLIB_ELEM_CRYPTO("dds_security_cryptography_all_ok")
    "  </Cryptographic>"
    "  <AccessControl>"
    "    "MOCKLIB_ELEM_ACCESS_CONTROL("dds_security_access_control_all_ok")
    "    <Governance>file:Governance.p7s</Governance>"
    "    <PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
    "    <Permissions>file:Permissions.p7s</Permissions>"
    "  </AccessControl>"
    "</DDSSecurity>";

  dds_entity_t participant;
  dds_qos_t * qos;

  CU_ASSERT_FATAL((qos = dds_create_qos()) != NULL);
  dds_qset_prop(qos, "test.dds.sec.prop1", "testtext_value1_testtext");

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_FATAL | DDS_LC_ERROR | DDS_LC_WARNING | DDS_LC_CONFIG);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create participant with security elements. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, sec_config);
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, qos, NULL);
  CU_ASSERT_FATAL (participant > 0);
  ddsrt_setenv(URI_VARIABLE, "");
  dds_delete(participant);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);
  dds_delete_qos(qos);

  /* All traces should have been provided. */
  CU_ASSERT_FATAL(found == 0x1);
}

CU_Test(ddsc_security_config, qos_invalid, .init = ddsrt_init, .fini = ddsrt_fini)
{
  /* Expected traces when creating participant with the security elements. */
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
    "<Tracing><Verbosity>finest</></>"
    "<DDSSecurity>"
    "  <Authentication>"
    "    <IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
    "    <IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
    "    <PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
    "  </Authentication>"
    "  <AccessControl>"
    "    <Governance>file:Governance.p7s</Governance>"
    "    <PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
    "    <Permissions>file:Permissions.p7s</Permissions>"
    "  </AccessControl>"
    "</DDSSecurity>";

  dds_entity_t participant;
  dds_qos_t * qos;

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_FATAL | DDS_LC_ERROR | DDS_LC_WARNING | DDS_LC_CONFIG);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create the qos */
  CU_ASSERT_FATAL((qos = dds_create_qos()) != NULL);
  dds_qset_prop(qos, "dds.sec.dummy", "testtext_dummy_testtext");

  /* Create participant with security config in qos. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, sec_config);
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, qos, NULL);
  dds_delete_qos(qos);
  CU_ASSERT_EQUAL_FATAL(participant, DDS_RETCODE_ERROR);
  ddsrt_setenv(URI_VARIABLE, "");

  /* All traces should have been provided. */
  CU_ASSERT_FATAL(found == 0xffff);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);
}
