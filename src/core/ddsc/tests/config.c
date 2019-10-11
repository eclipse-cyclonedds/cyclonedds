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

#define FORCE_ENV

#define URI_VARIABLE DDS_PROJECT_NAME_NOSPACE_CAPS"_URI"
#define MAX_PARTICIPANTS_VARIABLE "MAX_PARTICIPANTS"

static void config__check_env(
    const char * env_variable,
    const char * expected_value)
{
    char * env_uri = NULL;
    ddsrt_getenv(env_variable, &env_uri);
#if 0
    const char * const env_not_set = "Environment variable '%s' isn't set. This needs to be set to '%s' for this test to run.";
    const char * const env_not_as_expected = "Environment variable '%s' has an unexpected value: '%s' (expected: '%s')";
#endif

#ifdef FORCE_ENV
    {
        bool env_ok;

        if ( env_uri == NULL ) {
            env_ok = false;
        } else if ( strncmp(env_uri, expected_value, strlen(expected_value)) != 0 ) {
            env_ok = false;
        } else {
            env_ok = true;
        }

        if ( !env_ok ) {
            dds_return_t r;

            r = ddsrt_setenv(env_variable, expected_value);
            CU_ASSERT_EQUAL_FATAL(r, DDS_RETCODE_OK);
        }
    }
#else
    CU_ASSERT_PTR_NOT_NULL_FATAL(env_uri);
    CU_ASSERT_STRING_EQUAL_FATAL(env_uri, expected_value);
#endif /* FORCE_ENV */

}

CU_Test(ddsc_config, simple_udp, .init = ddsrt_init, .fini = ddsrt_fini) {

    dds_entity_t participant;

    config__check_env(URI_VARIABLE, CONFIG_ENV_SIMPLE_UDP);
    config__check_env(MAX_PARTICIPANTS_VARIABLE, CONFIG_ENV_MAX_PARTICIPANTS);

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);

    CU_ASSERT_FATAL(participant> 0);

    dds_delete(participant);
}

CU_Test(ddsc_config, user_config, .init = ddsrt_init, .fini = ddsrt_fini) {

    CU_ASSERT_FATAL(dds_create_domain(1,
         "<"DDS_PROJECT_NAME"><Domain><Id>any</Id></Domain>"
           "<DDSI2E><Internal><MaxParticipants>2</MaxParticipants></Internal></DDSI2E>"
         "</"DDS_PROJECT_NAME">") == DDS_RETCODE_OK);

    dds_entity_t participant_1;
    dds_entity_t participant_2;
    dds_entity_t participant_3;

    participant_1 = dds_create_participant(1, NULL, NULL);

    CU_ASSERT_FATAL(participant_1 > 0);

    participant_2 = dds_create_participant(1, NULL, NULL);

    CU_ASSERT_FATAL(participant_2 > 0);

    participant_3 = dds_create_participant(1, NULL, NULL);

    CU_ASSERT(participant_3 <= 0);

    dds_delete(participant_3);
    dds_delete(participant_2);
    dds_delete(participant_1);
}

CU_Test(ddsc_config, incorrect_config, .init = ddsrt_init, .fini = ddsrt_fini) {

    CU_ASSERT_FATAL(dds_create_domain(1, NULL) == DDS_RETCODE_BAD_PARAMETER);
    CU_ASSERT_FATAL(dds_create_domain(1, "<CycloneDDS incorrect XML") != DDS_RETCODE_OK);
    CU_ASSERT_FATAL(dds_create_domain(DDS_DOMAIN_DEFAULT,
         "<"DDS_PROJECT_NAME"><Domain><Id>any</Id></Domain>"
           "<DDSI2E><Internal><MaxParticipants>2</MaxParticipants></Internal></DDSI2E>"
         "</"DDS_PROJECT_NAME">") == DDS_RETCODE_BAD_PARAMETER);
    CU_ASSERT_FATAL(dds_create_domain(2,
         "<"DDS_PROJECT_NAME"><Domain><Id>any</Id></Domain>"
           "<DDSI2E><Internal><MaxParticipants>2</MaxParticipants></Internal></DDSI2E>"
         "</"DDS_PROJECT_NAME">") == DDS_RETCODE_OK);
    CU_ASSERT_FATAL(dds_create_domain(2, "") == DDS_RETCODE_PRECONDITION_NOT_MET);
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
          "<Library path=\"dds_security_auth\" initFunction=\"init_authentication\" finalizeFunction=\"finalize_authentication\" />"
          "<IdentityCertificate></IdentityCertificate>"
          "<PrivateKey></PrivateKey>"
          "<Password>testtext_Password_testtext</Password>"
        "</Authentication>"
          "<Cryptographic>"
            "<Library path=\"dds_security_crypto\" initFunction=\"init_crypto\" finalizeFunction=\"finalize_crypto\"/>"
          "</Cryptographic>"
        "<AccessControl>"
          "<Library path=\"dds_security_ac\" initFunction=\"init_ac\" finalizeFunction=\"finalize_ac\"/>"
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
      "config: Domain/DDSSecurity/Authentication/Library/#text: dds_security_auth*",
      "config: Domain/DDSSecurity/Authentication/Library[@path]: dds_security_auth*",
      "config: Domain/DDSSecurity/Authentication/Library[@initFunction]: init_authentication*",
      "config: Domain/DDSSecurity/Authentication/Library[@finalizeFunction]: finalize_authentication*",
      "config: Domain/DDSSecurity/Authentication/IdentityCertificate/#text: testtext_IdentityCertificate_testtext*",
      "config: Domain/DDSSecurity/Authentication/IdentityCA/#text: testtext_IdentityCA_testtext*",
      "config: Domain/DDSSecurity/Authentication/PrivateKey/#text: testtext_PrivateKey_testtext*",
      "config: Domain/DDSSecurity/Authentication/Password/#text: testtext_Password_testtext*",
      "config: Domain/DDSSecurity/Authentication/TrustedCADirectory/#text: testtext_Dir_testtext*",
      "config: Domain/DDSSecurity/AccessControl/Library/#text: dds_security_ac*",
      "config: Domain/DDSSecurity/AccessControl/Library[@path]: dds_security_ac*",
      "config: Domain/DDSSecurity/AccessControl/Library[@initFunction]: init_ac*",
      "config: Domain/DDSSecurity/AccessControl/Library[@finalizeFunction]: finalize_ac*",
      "config: Domain/DDSSecurity/AccessControl/PermissionsCA/#text: file:Permissions_CA.pem*",
      "config: Domain/DDSSecurity/AccessControl/Governance/#text: file:Governance.p7s*",
      "config: Domain/DDSSecurity/AccessControl/Permissions/#text: file:Permissions.p7s*",
      "config: Domain/DDSSecurity/Cryptographic/Library/#text: dds_security_crypto*",
      "config: Domain/DDSSecurity/Cryptographic/Library[@path]: dds_security_crypto*",
      "config: Domain/DDSSecurity/Cryptographic/Library[@initFunction]: init_crypto*",
      "config: Domain/DDSSecurity/Cryptographic/Library[@finalizeFunction]: finalize_crypto*",
      /* The config should have been parsed into the participant QoS. */
      "PARTICIPANT * QOS={*property_list={value={{dds.sec.auth.identity_ca,testtext_IdentityCA_testtext,0},{dds.sec.auth.private_key,testtext_PrivateKey_testtext,0},{dds.sec.auth.identity_certificate,testtext_IdentityCertificate_testtext,0},{dds.sec.access.permissions_ca,file:Permissions_CA.pem,0},{dds.sec.access.governance,file:Governance.p7s,0},{dds.sec.access.permissions,file:Permissions.p7s,0},{dds.sec.auth.password,testtext_Password_testtext,0},{dds.sec.auth.trusted_ca_dir,testtext_Dir_testtext,0}}binary_value={}}*}*",
#endif
      NULL
    };

    const char *sec_config =
      "<"DDS_PROJECT_NAME">"
        "<Domain id=\"any\">"
          "<Tracing><Verbosity>finest</></>"
          "<DDSSecurity>"
            "<Authentication>"
              "<Library path=\"dds_security_auth\" initFunction=\"init_authentication\" finalizeFunction=\"finalize_authentication\" />"
              "<IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
              "<IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
              "<PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
              "<Password>testtext_Password_testtext</Password>"
              "<TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
            "</Authentication>"
            "<Cryptographic>"
              "<Library path=\"dds_security_crypto\" initFunction=\"init_crypto\" finalizeFunction=\"finalize_crypto\"/>"
            "</Cryptographic>"
            "<AccessControl>"
              "<Library path=\"dds_security_ac\" initFunction=\"init_ac\" finalizeFunction=\"finalize_ac\"/>"
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
      "config: Domain/DDSSecurity/Authentication/Library/#text: dds_security_auth*",
      "config: Domain/DDSSecurity/Authentication/Library[@path]: dds_security_auth*",
      "config: Domain/DDSSecurity/Authentication/Library[@initFunction]: init_authentication*",
      "config: Domain/DDSSecurity/Authentication/Library[@finalizeFunction]: finalize_authentication*",
      "config: Domain/DDSSecurity/Authentication/IdentityCertificate/#text: testtext_IdentityCertificate_testtext*",
      "config: Domain/DDSSecurity/Authentication/IdentityCA/#text: testtext_IdentityCA_testtext*",
      "config: Domain/DDSSecurity/Authentication/PrivateKey/#text: testtext_PrivateKey_testtext*",
      "config: Domain/DDSSecurity/Authentication/Password/#text:  {}*",
      "config: Domain/DDSSecurity/Authentication/TrustedCADirectory/#text:  {}*",
      "config: Domain/DDSSecurity/AccessControl/Library/#text: dds_security_ac*",
      "config: Domain/DDSSecurity/AccessControl/Library[@path]: dds_security_ac*",
      "config: Domain/DDSSecurity/AccessControl/Library[@initFunction]: init_ac*",
      "config: Domain/DDSSecurity/AccessControl/Library[@finalizeFunction]: finalize_ac*",
      "config: Domain/DDSSecurity/AccessControl/PermissionsCA/#text: file:Permissions_CA.pem*",
      "config: Domain/DDSSecurity/AccessControl/Governance/#text: file:Governance.p7s*",
      "config: Domain/DDSSecurity/AccessControl/Permissions/#text: file:Permissions.p7s*",
      "config: Domain/DDSSecurity/Cryptographic/Library/#text: dds_security_crypto*",
      "config: Domain/DDSSecurity/Cryptographic/Library[@path]: dds_security_crypto*",
      "config: Domain/DDSSecurity/Cryptographic/Library[@initFunction]: init_crypto*",
      "config: Domain/DDSSecurity/Cryptographic/Library[@finalizeFunction]: finalize_crypto*",
      /* The config should have been parsed into the participant QoS. */
      "PARTICIPANT * QOS={*property_list={value={{dds.sec.auth.identity_ca,testtext_IdentityCA_testtext,0},{dds.sec.auth.private_key,testtext_PrivateKey_testtext,0},{dds.sec.auth.identity_certificate,testtext_IdentityCertificate_testtext,0},{dds.sec.access.permissions_ca,file:Permissions_CA.pem,0},{dds.sec.access.governance,file:Governance.p7s,0},{dds.sec.access.permissions,file:Permissions.p7s,0}}binary_value={}}*}*",
#endif
      NULL
    };

    const char *sec_config =
      "<Tracing><Verbosity>finest</></>"
      "<DDSSecurity>"
        "<Authentication>"
          "<Library path=\"dds_security_auth\" initFunction=\"init_authentication\" finalizeFunction=\"finalize_authentication\" />"
          "<IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
          "<IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
          "<PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
        "</Authentication>"
          "<Cryptographic>"
            "<Library path=\"dds_security_crypto\" initFunction=\"init_crypto\" finalizeFunction=\"finalize_crypto\"/>"
          "</Cryptographic>"
        "<AccessControl>"
          "<Library path=\"dds_security_ac\" initFunction=\"init_ac\" finalizeFunction=\"finalize_ac\"/>"
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
      "config: Domain/DDSSecurity/Authentication/Library/#text: dds_security_auth*",
      "config: Domain/DDSSecurity/Authentication/Library[@path]: dds_security_auth*",
      "config: Domain/DDSSecurity/Authentication/Library[@initFunction]: init_authentication*",
      "config: Domain/DDSSecurity/Authentication/Library[@finalizeFunction]: finalize_authentication*",
      "config: Domain/DDSSecurity/Authentication/IdentityCertificate/#text: testtext_IdentityCertificate_testtext*",
      "config: Domain/DDSSecurity/Authentication/IdentityCA/#text: testtext_IdentityCA_testtext*",
      "config: Domain/DDSSecurity/Authentication/PrivateKey/#text: testtext_PrivateKey_testtext*",
      "config: Domain/DDSSecurity/Authentication/Password/#text: testtext_Password_testtext*",
      "config: Domain/DDSSecurity/Authentication/TrustedCADirectory/#text: testtext_Dir_testtext*",
      "config: Domain/DDSSecurity/AccessControl/Library/#text: dds_security_ac*",
      "config: Domain/DDSSecurity/AccessControl/Library[@path]: dds_security_ac*",
      "config: Domain/DDSSecurity/AccessControl/Library[@initFunction]: init_ac*",
      "config: Domain/DDSSecurity/AccessControl/Library[@finalizeFunction]: finalize_ac*",
      "config: Domain/DDSSecurity/AccessControl/PermissionsCA/#text: file:Permissions_CA.pem*",
      "config: Domain/DDSSecurity/AccessControl/Governance/#text: file:Governance.p7s*",
      "config: Domain/DDSSecurity/AccessControl/Permissions/#text: file:Permissions.p7s*",
      "config: Domain/DDSSecurity/Cryptographic/Library/#text: dds_security_crypto*",
      "config: Domain/DDSSecurity/Cryptographic/Library[@path]: dds_security_crypto*",
      "config: Domain/DDSSecurity/Cryptographic/Library[@initFunction]: init_crypto*",
      "config: Domain/DDSSecurity/Cryptographic/Library[@finalizeFunction]: finalize_crypto*",
      /* The config should have been parsed into the participant QoS. */
      "PARTICIPANT * QOS={*property_list={value={{dds.sec.auth.identity_ca,testtext_IdentityCA_testtext,0},{dds.sec.auth.private_key,testtext_PrivateKey_testtext,0},{dds.sec.auth.identity_certificate,testtext_IdentityCertificate_testtext,0},{dds.sec.access.permissions_ca,file:Permissions_CA.pem,0},{dds.sec.access.governance,file:Governance.p7s,0},{dds.sec.access.permissions,file:Permissions.p7s,0},{dds.sec.auth.password,testtext_Password_testtext,0},{dds.sec.auth.trusted_ca_dir,testtext_Dir_testtext,0}}binary_value={}}*}*",
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
              "<Library path=\"dds_security_auth\" initFunction=\"init_authentication\" finalizeFunction=\"finalize_authentication\" />"
              "<IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
              "<IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
              "<PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
              "<Password>testtext_Password_testtext</Password>"
              "<TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
            "</Authentication>"
            "<Cryptographic>"
              "<Library path=\"dds_security_crypto\" initFunction=\"init_crypto\" finalizeFunction=\"finalize_crypto\"/>"
            "</Cryptographic>"
            "<AccessControl>"
              "<Library path=\"dds_security_ac\" initFunction=\"init_ac\" finalizeFunction=\"finalize_ac\"/>"
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

    /* All traces should have been provided. */
#ifndef DDSI_INCLUDE_SECURITY
    CU_ASSERT_FATAL(found == 0x1);
#else
    CU_ASSERT_FATAL(found == 0x1fffff);
#endif
}
