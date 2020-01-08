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

#ifdef DDSI_INCLUDE_SECURITY
#include "dds/security/dds_security_api_defs.h"
#endif

#define FORCE_ENV

#define URI_VARIABLE DDS_PROJECT_NAME_NOSPACE_CAPS"_URI"
#define MAX_PARTICIPANTS_VARIABLE "MAX_PARTICIPANTS"

static void config__check_env (const char *env_variable, const char *expected_value)
{
  char *env_uri = NULL;
  ddsrt_getenv (env_variable, &env_uri);
#ifdef FORCE_ENV
  {
    bool env_ok;

    if (env_uri == NULL)
      env_ok = false;
    else if (strncmp (env_uri, expected_value, strlen (expected_value)) != 0)
      env_ok = false;
    else
      env_ok = true;

    if (!env_ok)
    {
      dds_return_t r = ddsrt_setenv (env_variable, expected_value);
      CU_ASSERT_EQUAL_FATAL (r, DDS_RETCODE_OK);
    }
  }
#else
  CU_ASSERT_PTR_NOT_NULL_FATAL (env_uri);
  CU_ASSERT_STRING_EQUAL_FATAL (env_uri, expected_value);
#endif /* FORCE_ENV */
}

CU_Test (ddsc_config, simple_udp, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t participant;
  config__check_env (URI_VARIABLE, CONFIG_ENV_SIMPLE_UDP);
  config__check_env (MAX_PARTICIPANTS_VARIABLE, CONFIG_ENV_MAX_PARTICIPANTS);
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL (participant> 0);
  dds_delete (participant);
}

CU_Test (ddsc_config, user_config, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain;
  domain = dds_create_domain (1,
                              "<"DDS_PROJECT_NAME"><Domain><Id>any</Id></Domain>"
                              "<DDSI2E><Internal><MaxParticipants>2</MaxParticipants></Internal></DDSI2E>"
                              "</"DDS_PROJECT_NAME">");
  CU_ASSERT_FATAL (domain > 0);

  dds_entity_t participant_1 = dds_create_participant (1, NULL, NULL);
  CU_ASSERT_FATAL(participant_1 > 0);

  dds_entity_t participant_2 = dds_create_participant (1, NULL, NULL);
  CU_ASSERT_FATAL(participant_2 > 0);

  dds_entity_t participant_3 = dds_create_participant (1, NULL, NULL);
  CU_ASSERT(participant_3 < 0);

  dds_delete (domain);
}

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

CU_Test(ddsc_config, security_non, .init = ddsrt_init, .fini = ddsrt_fini) {

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

CU_Test(ddsc_config, security_empty, .init = ddsrt_init, .fini = ddsrt_fini) {

  /* Expected traces when creating participant with an empty security element. */
  const char *log_expected[] = {
#ifndef DDSI_INCLUDE_SECURITY
    "config: //CycloneDDS/Domain: DDSSecurity: unknown element*",
#else
    "config: //CycloneDDS/Domain/DDSSecurity/Authentication/IdentityCertificate/#text: element missing in configuration*",
    "config: //CycloneDDS/Domain/DDSSecurity/Authentication/IdentityCA/#text: element missing in configuration*",
    "config: //CycloneDDS/Domain/DDSSecurity/Authentication/PrivateKey/#text: element missing in configuration*",
#endif
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
#ifndef DDSI_INCLUDE_SECURITY
  CU_ASSERT_FATAL(found == 0x1);
#else
  CU_ASSERT_FATAL(found == 0x7);
#endif
}

CU_Test(ddsc_config, security_missing, .init = ddsrt_init, .fini = ddsrt_fini) {

  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
#ifndef DDSI_INCLUDE_SECURITY
    "config: //CycloneDDS/Domain: DDSSecurity: unknown element*",
#else
    "config: //CycloneDDS/Domain/DDSSecurity/Authentication/IdentityCertificate/#text: element missing in configuration*",
    "config: //CycloneDDS/Domain/DDSSecurity/Authentication/IdentityCA/#text: element missing in configuration*",
    "config: //CycloneDDS/Domain/DDSSecurity/Authentication/PrivateKey/#text: element missing in configuration*",
#endif
      NULL
    };

  /* IdentityCertificate, IdentityCA and PrivateKey values or elements are missing. */
  const char *sec_config =
    "<Tracing><Verbosity>finest</></>"
    "<DDSSecurity>"
      "<Authentication>"
        "<Library path=\""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX"\" initFunction=\"init_authentication\" finalizeFunction=\"finalize_authentication\" />"
        "<IdentityCertificate></IdentityCertificate>"
        "<PrivateKey></PrivateKey>"
        "<Password>testtext_Password_testtext</Password>"
      "</Authentication>"
      "<Cryptographic>"
        "<Library path=\""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX"\" initFunction=\"init_crypto\" finalizeFunction=\"finalize_crypto\"/>"
      "</Cryptographic>"
      "<AccessControl>"
        "<Library path=\""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX"\" initFunction=\"init_access_control\" finalizeFunction=\"finalize_access_control\"/>"
        "<Governance>file:Governance.p7s</Governance>"
        "<PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
        "<Permissions>file:Permissions.p7s</Permissions>"
      "</AccessControl>"
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
#ifndef DDSI_INCLUDE_SECURITY
  CU_ASSERT_FATAL(found == 0x1);
#else
  CU_ASSERT_FATAL(found == 0x7);
#endif
}

CU_Test(ddsc_config, security_all, .init = ddsrt_init, .fini = ddsrt_fini) {

  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
#ifndef DDSI_INCLUDE_SECURITY
    "config: //CycloneDDS/Domain: DDSSecurity: unknown element*",
#else
    "config: Domain/DDSSecurity/Authentication/Library/#text: "CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX"*",
    "config: Domain/DDSSecurity/Authentication/Library[@path]: "CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX"*",
    "config: Domain/DDSSecurity/Authentication/Library[@initFunction]: init_authentication*",
    "config: Domain/DDSSecurity/Authentication/Library[@finalizeFunction]: finalize_authentication*",
    "config: Domain/DDSSecurity/Authentication/IdentityCertificate/#text: testtext_IdentityCertificate_testtext*",
    "config: Domain/DDSSecurity/Authentication/IdentityCA/#text: testtext_IdentityCA_testtext*",
    "config: Domain/DDSSecurity/Authentication/PrivateKey/#text: testtext_PrivateKey_testtext*",
    "config: Domain/DDSSecurity/Authentication/Password/#text: testtext_Password_testtext*",
    "config: Domain/DDSSecurity/Authentication/TrustedCADirectory/#text: testtext_Dir_testtext*",
    "config: Domain/DDSSecurity/AccessControl/Library/#text: "CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX"*",
    "config: Domain/DDSSecurity/AccessControl/Library[@path]: "CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX"*",
    "config: Domain/DDSSecurity/AccessControl/Library[@initFunction]: init_access_control*",
    "config: Domain/DDSSecurity/AccessControl/Library[@finalizeFunction]: finalize_access_control*",
    "config: Domain/DDSSecurity/AccessControl/PermissionsCA/#text: file:Permissions_CA.pem*",
    "config: Domain/DDSSecurity/AccessControl/Governance/#text: file:Governance.p7s*",
    "config: Domain/DDSSecurity/AccessControl/Permissions/#text: file:Permissions.p7s*",
    "config: Domain/DDSSecurity/Cryptographic/Library/#text: "CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX"*",
    "config: Domain/DDSSecurity/Cryptographic/Library[@path]: "CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX"*",
    "config: Domain/DDSSecurity/Cryptographic/Library[@initFunction]: init_crypto*",
    "config: Domain/DDSSecurity/Cryptographic/Library[@finalizeFunction]: finalize_crypto*",
    /* The config should have been parsed into the participant QoS. */
    "PARTICIPANT * QOS={*property_list={value={{dds.sec.auth.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX",0},"
      "{dds.sec.auth.library.init,init_authentication,0},"
      "{dds.sec.auth.library.finalize,finalize_authentication,0},"
      "{dds.sec.crypto.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX",0},"
      "{dds.sec.crypto.library.init,init_crypto,0},"
      "{dds.sec.crypto.library.finalize,finalize_crypto,0},"
      "{dds.sec.access.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX",0},"
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

#endif
    NULL
  };
  const char *sec_config =
    "<"DDS_PROJECT_NAME">"
      "<Domain id=\"any\">"
      "<Tracing><Verbosity>finest</></>"
      "<DDSSecurity>"
      "<Authentication>"
        "<Library path=\""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX"\" initFunction=\"init_authentication\" finalizeFunction=\"finalize_authentication\" />"
        "<IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
        "<IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
        "<PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
        "<Password>testtext_Password_testtext</Password>"
        "<TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
      "</Authentication>"
      "<Cryptographic>"
        "<Library path=\""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX"\" initFunction=\"init_crypto\" finalizeFunction=\"finalize_crypto\"/>"
      "</Cryptographic>"
      "<AccessControl>"
        "<Library path=\""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX"\" initFunction=\"init_access_control\" finalizeFunction=\"finalize_access_control\"/>"
      "<Governance>file:Governance.p7s</Governance>"
      "<PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
      "<Permissions>file:Permissions.p7s</Permissions>"
      "</AccessControl>"
      "</DDSSecurity>"
    "</Domain>"
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
    ddsrt_setenv(URI_VARIABLE, "");
    dds_delete(participant);
    dds_set_log_sink(NULL, NULL);
    dds_set_trace_sink(NULL, NULL);

    /* All traces should have been provided. */
#ifndef DDSI_INCLUDE_SECURITY
    CU_ASSERT_FATAL(found == 0x1);
#else
    CU_ASSERT_FATAL(found == 0x1fffff);
#endif
}

CU_Test(ddsc_config, security, .init = ddsrt_init, .fini = ddsrt_fini) {

    /* Expected traces when creating participant with the security elements. */
    const char *log_expected[] = {
#ifndef DDSI_INCLUDE_SECURITY
      "config: //CycloneDDS/Domain: DDSSecurity: unknown element*",
#else
      "config: Domain/DDSSecurity/Authentication/Library/#text: "CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX"*",
      "config: Domain/DDSSecurity/Authentication/Library[@path]: "CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX"*",
      "config: Domain/DDSSecurity/Authentication/Library[@initFunction]: init_authentication*",
      "config: Domain/DDSSecurity/Authentication/Library[@finalizeFunction]: finalize_authentication*",
      "config: Domain/DDSSecurity/Authentication/IdentityCertificate/#text: testtext_IdentityCertificate_testtext*",
      "config: Domain/DDSSecurity/Authentication/IdentityCA/#text: testtext_IdentityCA_testtext*",
      "config: Domain/DDSSecurity/Authentication/PrivateKey/#text: testtext_PrivateKey_testtext*",
      "config: Domain/DDSSecurity/Authentication/Password/#text:  {}*",
      "config: Domain/DDSSecurity/Authentication/TrustedCADirectory/#text:  {}*",
      "config: Domain/DDSSecurity/AccessControl/Library/#text: "CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX"*",
      "config: Domain/DDSSecurity/AccessControl/Library[@path]: "CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX"*",
      "config: Domain/DDSSecurity/AccessControl/Library[@initFunction]: init_access_control*",
      "config: Domain/DDSSecurity/AccessControl/Library[@finalizeFunction]: finalize_access_control*",
      "config: Domain/DDSSecurity/AccessControl/PermissionsCA/#text: file:Permissions_CA.pem*",
      "config: Domain/DDSSecurity/AccessControl/Governance/#text: file:Governance.p7s*",
      "config: Domain/DDSSecurity/AccessControl/Permissions/#text: file:Permissions.p7s*",
      "config: Domain/DDSSecurity/Cryptographic/Library/#text: "CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX"*",
      "config: Domain/DDSSecurity/Cryptographic/Library[@path]: "CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX"*",
      "config: Domain/DDSSecurity/Cryptographic/Library[@initFunction]: init_crypto*",
      "config: Domain/DDSSecurity/Cryptographic/Library[@finalizeFunction]: finalize_crypto*",
      /* The config should have been parsed into the participant QoS. */
      "PARTICIPANT * QOS={*property_list={value={{dds.sec.auth.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX",0},"
        "{dds.sec.auth.library.init,init_authentication,0},"
        "{dds.sec.auth.library.finalize,finalize_authentication,0},"
        "{dds.sec.crypto.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX",0},"
        "{dds.sec.crypto.library.init,init_crypto,0},"
        "{dds.sec.crypto.library.finalize,finalize_crypto,0},"
        "{dds.sec.access.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX",0},"
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
#endif
    NULL
  };

  const char *sec_config =
    "<Tracing><Verbosity>finest</></>"
    "<DDSSecurity>"
      "<Authentication>"
        "<Library path=\""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX"\" initFunction=\"init_authentication\" finalizeFunction=\"finalize_authentication\" />"
        "<IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
        "<IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
        "<PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
      "</Authentication>"
      "<Cryptographic>"
        "<Library path=\""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX"\" initFunction=\"init_crypto\" finalizeFunction=\"finalize_crypto\"/>"
      "</Cryptographic>"
      "<AccessControl>"
        "<Library path=\""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX"\" initFunction=\"init_access_control\" finalizeFunction=\"finalize_access_control\"/>"
        "<Governance>file:Governance.p7s</Governance>"
        "<PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
        "<Permissions>file:Permissions.p7s</Permissions>"
      "</AccessControl>"
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
    ddsrt_setenv(URI_VARIABLE, "");
    dds_delete(participant);
    dds_set_log_sink(NULL, NULL);
    dds_set_trace_sink(NULL, NULL);

    /* All traces should have been provided. */
#ifndef DDSI_INCLUDE_SECURITY
    CU_ASSERT_FATAL(found == 0x1);
#else
    CU_ASSERT_FATAL(found == 0x1fffff);
#endif
}

CU_Test(ddsc_config, security_deprecated, .init = ddsrt_init, .fini = ddsrt_fini) {

    /* Expected traces when creating participant with the security elements. */
    const char *log_expected[] = {
#ifndef DDSI_INCLUDE_SECURITY
      "config: //CycloneDDS/Domain: DDSSecurity: unknown element*",
#else
    "config: Domain/DDSSecurity/Authentication/Library/#text: "CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX"*",
    "config: Domain/DDSSecurity/Authentication/Library[@path]: "CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX"*",
    "config: Domain/DDSSecurity/Authentication/Library[@initFunction]: init_authentication*",
    "config: Domain/DDSSecurity/Authentication/Library[@finalizeFunction]: finalize_authentication*",
    "config: Domain/DDSSecurity/Authentication/IdentityCertificate/#text: testtext_IdentityCertificate_testtext*",
    "config: Domain/DDSSecurity/Authentication/IdentityCA/#text: testtext_IdentityCA_testtext*",
    "config: Domain/DDSSecurity/Authentication/PrivateKey/#text: testtext_PrivateKey_testtext*",
    "config: Domain/DDSSecurity/Authentication/Password/#text: testtext_Password_testtext*",
    "config: Domain/DDSSecurity/Authentication/TrustedCADirectory/#text: testtext_Dir_testtext*",
    "config: Domain/DDSSecurity/AccessControl/Library/#text: "CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX"*",
    "config: Domain/DDSSecurity/AccessControl/Library[@path]: "CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX"*",
    "config: Domain/DDSSecurity/AccessControl/Library[@initFunction]: init_access_control*",
    "config: Domain/DDSSecurity/AccessControl/Library[@finalizeFunction]: finalize_access_control*",
    "config: Domain/DDSSecurity/AccessControl/PermissionsCA/#text: file:Permissions_CA.pem*",
    "config: Domain/DDSSecurity/AccessControl/Governance/#text: file:Governance.p7s*",
    "config: Domain/DDSSecurity/AccessControl/Permissions/#text: file:Permissions.p7s*",
    "config: Domain/DDSSecurity/Cryptographic/Library/#text: "CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX"*",
    "config: Domain/DDSSecurity/Cryptographic/Library[@path]: "CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX"*",
    "config: Domain/DDSSecurity/Cryptographic/Library[@initFunction]: init_crypto*",
    "config: Domain/DDSSecurity/Cryptographic/Library[@finalizeFunction]: finalize_crypto*",
    /* The config should have been parsed into the participant QoS. */
    "PARTICIPANT * QOS={*property_list={value={"
      "{dds.sec.auth.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX",0},"
      "{dds.sec.auth.library.init,init_authentication,0},"
      "{dds.sec.auth.library.finalize,finalize_authentication,0},"
      "{dds.sec.crypto.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX",0},"
      "{dds.sec.crypto.library.init,init_crypto,0},"
      "{dds.sec.crypto.library.finalize,finalize_crypto,0},"
      "{dds.sec.access.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX",0},"
      "{dds.sec.access.library.init,init_access_control,0},{dds.sec.access.library.finalize,finalize_access_control,0},{dds.sec.auth.identity_ca,testtext_IdentityCA_testtext,0},"
      "{dds.sec.auth.private_key,testtext_PrivateKey_testtext,0},"
      "{dds.sec.auth.identity_certificate,testtext_IdentityCertificate_testtext,0},"
      "{dds.sec.access.permissions_ca,file:Permissions_CA.pem,0},"
      "{dds.sec.access.governance,file:Governance.p7s,0},"
      "{dds.sec.access.permissions,file:Permissions.p7s,0},"
      "{dds.sec.auth.password,testtext_Password_testtext,0},"
      "{dds.sec.auth.trusted_ca_dir,testtext_Dir_testtext,0}}binary_value={}}*}*",
#endif
    NULL
  };

  const char *sec_config =
    "<"DDS_PROJECT_NAME">"
      "<Domain>"
      "<Id>any</Id>"
      "</Domain>"
      "<DDSI2E>"
        "<DDSSecurity>"
          "<Authentication>"
            "<Library path=\""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX"\" initFunction=\"init_authentication\" finalizeFunction=\"finalize_authentication\" />"
            "<IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
            "<IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
            "<PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
            "<Password>testtext_Password_testtext</Password>"
            "<TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
          "</Authentication>"
          "<Cryptographic>"
            "<Library path=\""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX"\" initFunction=\"init_crypto\" finalizeFunction=\"finalize_crypto\"/>"
          "</Cryptographic>"
          "<AccessControl>"
            "<Library path=\""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX"\" initFunction=\"init_access_control\" finalizeFunction=\"finalize_access_control\"/>"
            "<Governance>file:Governance.p7s</Governance>"
            "<PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
            "<Permissions>file:Permissions.p7s</Permissions>"
          "</AccessControl>"
        "</DDSSecurity>"
        "<Tracing><Verbosity>finest</></>"
      "</DDSI2E>"
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
  ddsrt_setenv(URI_VARIABLE, "");
  dds_delete(participant);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);

  /* All traces should have been provided. */
#ifndef DDSI_INCLUDE_SECURITY
  CU_ASSERT_FATAL(found == 0x1);
#else
  CU_ASSERT_FATAL(found == 0x1fffff);
#endif
}

CU_Test(ddsc_config, security_qos, .init = ddsrt_init, .fini = ddsrt_fini)
{
  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
#ifdef DDSI_INCLUDE_SECURITY
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
    "{dds.sec.auth.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX",0},"
    "{dds.sec.auth.library.init,init_authentication,0},"
    "{dds.sec.auth.library.finalize,finalize_authentication,0},"
    "{dds.sec.crypto.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX",0},"
    "{dds.sec.crypto.library.init,init_crypto,0},"
    "{dds.sec.crypto.library.finalize,finalize_crypto,0},"
    "{dds.sec.access.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX",0},"
    "{dds.sec.access.library.init,init_access_control,0},"
    "{dds.sec.access.library.finalize,finalize_access_control,0}}binary_value={}}*}*",
  #endif
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
  dds_qset_prop(qos, "dds.sec.auth.library.path", ""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX"");
  dds_qset_prop(qos, "dds.sec.auth.library.init", "init_authentication");
  dds_qset_prop(qos, "dds.sec.auth.library.finalize", "finalize_authentication");
  dds_qset_prop(qos, "dds.sec.crypto.library.path", ""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX"");
  dds_qset_prop(qos, "dds.sec.crypto.library.init", "init_crypto");
  dds_qset_prop(qos, "dds.sec.crypto.library.finalize", "finalize_crypto");
  dds_qset_prop(qos, "dds.sec.access.library.path", ""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX"");
  dds_qset_prop(qos, "dds.sec.access.library.init", "init_access_control");
  dds_qset_prop(qos, "dds.sec.access.library.finalize", "finalize_access_control");

  /* Create participant with security config in qos. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, "<Tracing><Verbosity>finest</></>");
  CU_ASSERT_FATAL ((participant = dds_create_participant(DDS_DOMAIN_DEFAULT, qos, NULL)) > 0);
  ddsrt_setenv(URI_VARIABLE, "");
  dds_delete(participant);
  dds_delete_qos(qos);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);

  /* All traces should have been provided. */
#ifndef DDSI_INCLUDE_SECURITY
  CU_ASSERT_FATAL(found == 0);
#else
  CU_ASSERT_FATAL(found == 0x1);
#endif
}

CU_Test(ddsc_config, security_qos_props, .init = ddsrt_init, .fini = ddsrt_fini)
{
  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
#ifdef DDSI_INCLUDE_SECURITY
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
      "{dds.sec.auth.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX",0},"
      "{dds.sec.auth.library.init,init_authentication,0},"
      "{dds.sec.auth.library.finalize,finalize_authentication,0},"
      "{dds.sec.crypto.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX",0},"
      "{dds.sec.crypto.library.init,init_crypto,0},"
      "{dds.sec.crypto.library.finalize,finalize_crypto,0},"
      "{dds.sec.access.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX",0},"
      "{dds.sec.access.library.init,init_access_control,0},"
      "{dds.sec.access.library.finalize,finalize_access_control,0},"
      "{test.prop2,testtext_value2_testtext,0}}"
      "binary_value={{test.bprop1,(3,*),0}}}*}*",

  #endif
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

  dds_qset_prop(qos, "dds.sec.auth.library.path", ""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX"");
  dds_qset_prop(qos, "dds.sec.auth.library.init", "init_authentication");
  dds_qset_prop(qos, "dds.sec.auth.library.finalize", "finalize_authentication");
  dds_qset_prop(qos, "dds.sec.crypto.library.path", ""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX"");
  dds_qset_prop(qos, "dds.sec.crypto.library.init", "init_crypto");
  dds_qset_prop(qos, "dds.sec.crypto.library.finalize", "finalize_crypto");
  dds_qset_prop(qos, "dds.sec.access.library.path", ""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX"");
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
#ifndef DDSI_INCLUDE_SECURITY
  CU_ASSERT_FATAL(found == 0);
#else
  CU_ASSERT_FATAL(found == 0x1);
#endif
}

CU_Test(ddsc_config, security_config_qos, .init = ddsrt_init, .fini = ddsrt_fini)
{
  /* Expect qos settings used when creating participant with config security elements and qos. */
  const char *log_expected[] = {
#ifndef DDSI_INCLUDE_SECURITY
    "config: //CycloneDDS/Domain: DDSSecurity: unknown element*",
#else
    /* The security settings from qos properties should have been parsed into the participant QoS. */
    "new_participant(*): using security settings from QoS*",
    "PARTICIPANT * QOS={*property_list={value={"
      "{dds.sec.auth.identity_ca,testtext_QOS_IdentityCA_testtext,0},"
      "{dds.sec.auth.private_key,testtext_QOS_PrivateKey_testtext,0},"
      "{dds.sec.auth.identity_certificate,testtext_QOS_IdentityCertificate_testtext,0},"
      "{dds.sec.access.permissions_ca,file:QOS_Permissions_CA.pem,0},"
      "{dds.sec.access.governance,file:QOS_Governance.p7s,0},"
      "{dds.sec.access.permissions,file:QOS_Permissions.p7s,0},"
      "{dds.sec.auth.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX",0},"
      "{dds.sec.auth.library.init,init_authentication,0},"
      "{dds.sec.auth.library.finalize,finalize_authentication,0},"
      "{dds.sec.crypto.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX",0},"
      "{dds.sec.crypto.library.init,init_crypto,0},"
      "{dds.sec.crypto.library.finalize,finalize_crypto,0},"
      "{dds.sec.access.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX",0},"
      "{dds.sec.access.library.init,init_access_control,0},"
      "{dds.sec.access.library.finalize,finalize_access_control,0}"
      "}binary_value={}}*}*",
  #endif
    NULL
  };

  const char *sec_config =
    "<Tracing><Verbosity>finest</></>"
    "<DDSSecurity>"
      "<Authentication>"
      "<IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
      "<IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
      "<PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
    "</Authentication>"
    "<AccessControl>"
      "<Governance>file:Governance.p7s</Governance>"
      "<PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
      "<Permissions>file:Permissions.p7s</Permissions>"
    "</AccessControl>"
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
#ifdef DDSI_INCLUDE_SECURITY /*for using with constants coming from API */
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_PATH, ""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX"");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_INIT, "init_authentication");
  dds_qset_prop(qos, DDS_SEC_PROP_AUTH_LIBRARY_FINALIZE, "finalize_authentication");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_PATH, ""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX"");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_INIT, "init_crypto");
  dds_qset_prop(qos, DDS_SEC_PROP_CRYPTO_LIBRARY_FINALIZE, "finalize_crypto");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_PATH, ""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX"");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_INIT, "init_access_control");
  dds_qset_prop(qos, DDS_SEC_PROP_ACCESS_LIBRARY_FINALIZE, "finalize_access_control");
#endif
  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_FATAL | DDS_LC_ERROR | DDS_LC_WARNING | DDS_LC_CONFIG);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create participant with security elements. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, sec_config);
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, qos, NULL);
  ddsrt_setenv(URI_VARIABLE, "");
  dds_delete(participant);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);
  dds_delete_qos(qos);

  /* All traces should have been provided. */
#ifndef DDSI_INCLUDE_SECURITY
  CU_ASSERT_FATAL(found == 0x1);
#else
  CU_ASSERT_FATAL(found == 0x3);
#endif
}

CU_Test(ddsc_config, security_other_prop, .init = ddsrt_init, .fini = ddsrt_fini)
{
  /* Expect config used when creating participant with config security elements and
   * qos containing only non-security properties. */
  const char *log_expected[] = {
#ifndef DDSI_INCLUDE_SECURITY
    "config: //CycloneDDS/Domain: DDSSecurity: unknown element*",
#else
    /* The security settings from config should have been parsed into the participant QoS. */
    "PARTICIPANT * QOS={*property_list={value={{test.dds.sec.prop1,testtext_value1_testtext,0},"
      "{dds.sec.auth.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX",0},"
      "{dds.sec.auth.library.init,init_authentication,0},"
      "{dds.sec.auth.library.finalize,finalize_authentication,0},"
      "{dds.sec.crypto.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX",0},"
      "{dds.sec.crypto.library.init,init_crypto,0},"
      "{dds.sec.crypto.library.finalize,finalize_crypto,0},"
      "{dds.sec.access.library.path,"CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX",0},"
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
  #endif
    NULL
  };

  const char *sec_config =
    "<Tracing><Verbosity>finest</></>"
    "<DDSSecurity>"
      "<Authentication>"
        "<Library path=\""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_authentication_all_ok"CONFIG_LIB_SUFFIX"\" initFunction=\"init_authentication\" finalizeFunction=\"finalize_authentication\" />"
        "<IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
        "<IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
        "<PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
        "<Password>testtext_Password_testtext</Password>"
        "<TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
      "</Authentication>"
      "<Cryptographic>"
        "<Library path=\""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_cryptography_all_ok"CONFIG_LIB_SUFFIX"\" initFunction=\"init_crypto\" finalizeFunction=\"finalize_crypto\"/>"
      "</Cryptographic>"
      "<AccessControl>"
        "<Library path=\""CONFIG_PLUGIN_MOCK_DIR""CONFIG_LIB_SEP""CONFIG_LIB_PREFIX"dds_security_access_control_all_ok"CONFIG_LIB_SUFFIX"\" initFunction=\"init_access_control\" finalizeFunction=\"finalize_access_control\"/>"
        "<Governance>file:Governance.p7s</Governance>"
        "<PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
        "<Permissions>file:Permissions.p7s</Permissions>"
      "</AccessControl>"
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
  ddsrt_setenv(URI_VARIABLE, "");
  dds_delete(participant);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);
  dds_delete_qos(qos);

  /* All traces should have been provided. */
#ifndef DDSI_INCLUDE_SECURITY
  CU_ASSERT_FATAL(found == 0x1);
#else
  CU_ASSERT_FATAL(found == 0x1);
#endif
}

CU_Test(ddsc_config, security_qos_invalid, .init = ddsrt_init, .fini = ddsrt_fini)
{
  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
#ifndef DDSI_INCLUDE_SECURITY
    "config: //CycloneDDS/Domain: DDSSecurity: unknown element*",
#else
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
  #endif
    NULL
  };

  const char *sec_config =
    "<Tracing><Verbosity>finest</></>"
    "<DDSSecurity>"
      "<Authentication>"
        "<IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
        "<IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
        "<PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
      "</Authentication>"
      "<AccessControl>"
        "<Governance>file:Governance.p7s</Governance>"
        "<PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
        "<Permissions>file:Permissions.p7s</Permissions>"
      "</AccessControl>"
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
#ifdef DDSI_INCLUDE_SECURITY
  CU_ASSERT_EQUAL_FATAL(participant, DDS_RETCODE_ERROR);
#else
  dds_delete(participant);
#endif
  ddsrt_setenv(URI_VARIABLE, "");

  /* All traces should have been provided. */
#ifndef DDSI_INCLUDE_SECURITY
  CU_ASSERT_FATAL(found == 0x01);
#else
  CU_ASSERT_FATAL(found == 0xffff);
#endif
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);
}

