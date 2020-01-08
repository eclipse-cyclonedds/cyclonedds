/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
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
#include <dds/security/dds_security_api_defs.h>

#include "dds/dds.h"
#include "CUnit/Test.h"
#include "config_env.h"

#include "dds/version.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/heap.h"
//#include "dds/ddsi/ddsi_security_omg.h"

#define FORCE_ENV

#define URI_VARIABLE DDS_PROJECT_NAME_NOSPACE_CAPS"_URI"
#define MAX_PARTICIPANTS_VARIABLE "MAX_PARTICIPANTS"
static bool print_log=true;

static int patmatch (const char *pat, const char *str)
{
  while (*pat)
  {
    if (*pat == '?')
    {
      /* any character will do */
      if (*str++ == 0)
      {
        return 0;
      }
      pat++;
    }
    else if (*pat == '*')
    {
      /* collapse a sequence of wildcards, requiring as many
       characters in str as there are ?s in the sequence */
      while (*pat == '*' || *pat == '?')
      {
        if (*pat == '?' && *str++ == 0)
        {
          return 0;
        }
        pat++;
      }
      /* try matching on all positions where str matches pat */
      while (*str)
      {
        if (*str == *pat && patmatch (pat+1, str+1))
        {
          return 1;
        }
        str++;
      }
      return *pat == 0;
    }
    else
    {
      /* only an exact match */
      if (*str++ != *pat++)
      {
        return 0;
      }
    }
  }
  return *str == 0;
}


/*
 * The 'found' variable will contain flags related to the expected log
 * messages that were received.
 * Using flags will allow to show that when message isn't received,
 * which one it was.
 */
static uint32_t found;
static void logger(void *ptr, const dds_log_data_t *data) {
  char **expected = (char **) ptr;
  if (print_log) {
    printf("%s\n", data->message);
  }
  for (uint32_t i = 0; expected[i] != NULL; i++) {
      if (patmatch(expected[i], data->message)) {
          found |= (uint32_t)(1 << i);
      }
  }
}


CU_Test(ddssec_security_plugin_loading, all_ok, .init = ddsrt_init, .fini = ddsrt_fini) {

    /* Expected traces when creating participant with the security elements. */
    const char *log_expected[] = {
        "DDS Security plugins have been loaded*",
        NULL
    };

    const char *sec_config =
      "<"DDS_PROJECT_NAME">"
        "<Domain id=\"any\">"
          "<Tracing><Verbosity>finest</></>"
          "<DDSSecurity>"
            "<Authentication>"
              "<Library path=\"dds_security_authentication_all_ok\" initFunction=\"init_authentication\" finalizeFunction=\"finalize_authentication\" />"
              "<IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
              "<IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
              "<PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
              "<Password>testtext_Password_testtext</Password>"
              "<TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
            "</Authentication>"
            "<Cryptographic>"
              "<Library path=\"dds_security_cryptography_all_ok\" initFunction=\"init_crypto\" finalizeFunction=\"finalize_crypto\"/>"
            "</Cryptographic>"
            "<AccessControl>"
              "<Library path=\"dds_security_access_control_all_ok\" initFunction=\"init_access_control\" finalizeFunction=\"finalize_access_control\"/>"
              "<Governance>file:Governance.p7s</Governance>"
              "<PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
              "<Permissions>file:Permissions.p7s</Permissions>"
            "</AccessControl>"
          "</DDSSecurity>"
        "</Domain>"
      "</"DDS_PROJECT_NAME">";


    dds_entity_t participant;

    /* Set up the trace sinks to detect the config parsing. */
    dds_set_log_mask(DDS_LC_FATAL|DDS_LC_ERROR|DDS_LC_WARNING|DDS_LC_INFO| DDS_LC_TRACE);
    dds_set_log_sink(&logger, (void*)log_expected);
    dds_set_trace_sink(&logger, (void*)log_expected);

    /* Create participant with security elements. */
    found = 0;
    ddsrt_setenv(URI_VARIABLE, sec_config);
    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    dds_set_log_sink(NULL,NULL);
    dds_set_trace_sink(NULL,NULL);
    ddsrt_setenv(URI_VARIABLE, "");
    CU_ASSERT_FATAL(found == 0x1);


    dds_delete(participant);

}

CU_Test(ddssec_security_plugin_loading, missing_finalize, .init = ddsrt_init, .fini = ddsrt_fini) {

    /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
      "Could not find the function: finalize_authentication*",
      "Could not load Authentication plugin*",
      NULL
  };

  const char *sec_config =
    "<"DDS_PROJECT_NAME">"
      "<Domain id=\"any\">"
        "<Tracing><Verbosity>finest</></>"
        "<DDSSecurity>"
          "<Authentication>"
            "<Library path=\"dds_security_authentication_finalize_error\" initFunction=\"init_authentication\" finalizeFunction=\"finalize_authentication\" />"
            "<IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
            "<IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
            "<PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
            "<Password>testtext_Password_testtext</Password>"
            "<TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
          "</Authentication>"
          "<Cryptographic>"
            "<Library path=\"dds_security_cryptography_all_ok\" initFunction=\"init_crypto\" finalizeFunction=\"finalize_crypto\"/>"
          "</Cryptographic>"
          "<AccessControl>"
            "<Library path=\"dds_security_access_control_all_ok\" initFunction=\"init_access_control\" finalizeFunction=\"finalize_access_control\"/>"
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
    dds_set_log_sink( NULL, NULL );
    dds_set_trace_sink(NULL, NULL);
    ddsrt_setenv(URI_VARIABLE, "");
#ifdef PR304_MERGED
    /* It is better dds to return DDS_RETCODE_NOT_ALLOWED_BY_SECURITY instead of DDS_RETCODE_ERROR
    CU_ASSERT_FATAL( participant == DDS_RETCODE_NOT_ALLOWED_BY_SECURITY ); */
    CU_ASSERT_FATAL( participant == DDS_RETCODE_ERROR );
#else
    dds_delete(participant);
#endif
    CU_ASSERT_FATAL(found == 0x3);

}


CU_Test(ddssec_security_plugin_loading, authentication_missing_function, .init = ddsrt_init, .fini = ddsrt_fini) {

  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
          "Could not find the function for Authentication: get_shared_secret*",
          "Could not load security*",
          NULL
  };

  const char *sec_config =
          "<"DDS_PROJECT_NAME">"
            "<Domain id=\"any\">"
            "<Tracing><Verbosity>finest</></>"
            "<DDSSecurity>"
              "<Authentication>"
                "<Library path=\"dds_security_authentication_missing_function\" initFunction=\"init_authentication\" finalizeFunction=\"finalize_authentication\" />"
                "<IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
                "<IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
                "<PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
                "<Password>testtext_Password_testtext</Password>"
                "<TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
              "</Authentication>"
              "<Cryptographic>"
                "<Library path=\"dds_security_cryptography_all_ok\" initFunction=\"init_crypto\" finalizeFunction=\"finalize_crypto\"/>"
              "</Cryptographic>"
              "<AccessControl>"
                "<Library path=\"dds_security_access_control_all_ok\" initFunction=\"init_access_control\" finalizeFunction=\"finalize_access_control\"/>"
                "<Governance>file:Governance.p7s</Governance>"
                "<PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
                "<Permissions>file:Permissions.p7s</Permissions>"
              "</AccessControl>"
            "</DDSSecurity>"
          "</Domain>"
          "</"DDS_PROJECT_NAME">";


  dds_entity_t participant;

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_ERROR);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create participant with security elements. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, sec_config);
  print_log = true;
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);
  ddsrt_setenv(URI_VARIABLE, "");
#ifdef PR304_MERGED
  /* It is better dds to return DDS_RETCODE_NOT_ALLOWED_BY_SECURITY instead of DDS_RETCODE_ERROR
    CU_ASSERT_FATAL( participant == DDS_RETCODE_NOT_ALLOWED_BY_SECURITY ); */
    CU_ASSERT_FATAL( participant == DDS_RETCODE_ERROR );
#else
  dds_delete(participant);
#endif
  CU_ASSERT_FATAL(found == 0x3);


}


CU_Test(ddssec_security_plugin_loading, access_control_missing_function, .init = ddsrt_init, .fini = ddsrt_fini) {

  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
          "Could not find the function for Access Control: check_create_datareader*",
          "Could not load security*",
          NULL
  };

  const char *sec_config =
          "<"DDS_PROJECT_NAME">"
          "<Domain id=\"any\">"
          "<Tracing><Verbosity>finest</></>"
          "<DDSSecurity>"
          "<Authentication>"
          "<Library path=\"dds_security_authentication_all_ok\" initFunction=\"init_authentication\" finalizeFunction=\"finalize_authentication\" />"
          "<IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
          "<IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
          "<PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
          "<Password>testtext_Password_testtext</Password>"
          "<TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
          "</Authentication>"
          "<Cryptographic>"
          "<Library path=\"dds_security_cryptography_all_ok\" initFunction=\"init_crypto\" finalizeFunction=\"finalize_crypto\"/>"
          "</Cryptographic>"
          "<AccessControl>"
          "<Library path=\"dds_security_access_control_missing_function\" initFunction=\"init_access_control\" finalizeFunction=\"finalize_access_control\"/>"
          "<Governance>file:Governance.p7s</Governance>"
          "<PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
          "<Permissions>file:Permissions.p7s</Permissions>"
          "</AccessControl>"
          "</DDSSecurity>"
          "</Domain>"
          "</"DDS_PROJECT_NAME">";


  dds_entity_t participant;

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_FATAL|DDS_LC_ERROR);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create participant with security elements. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, sec_config);
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  ddsrt_setenv(URI_VARIABLE, "");
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);

#ifdef PR304_MERGED
  /* It is better dds to return DDS_RETCODE_NOT_ALLOWED_BY_SECURITY instead of DDS_RETCODE_ERROR
    CU_ASSERT_FATAL( participant == DDS_RETCODE_NOT_ALLOWED_BY_SECURITY ); */
    CU_ASSERT_FATAL( participant == DDS_RETCODE_ERROR );
#else
  dds_delete(participant);
#endif
  CU_ASSERT_FATAL(found == 0x3);



}


CU_Test(ddssec_security_plugin_loading, cryptography_missing_function, .init = ddsrt_init, .fini = ddsrt_fini) {

  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
          "Could not find the function for Cryptographic: set_remote_participant_crypto_tokens*",
          "Could not load security*",
          NULL
  };

  const char *sec_config =
          "<"DDS_PROJECT_NAME">"
          "<Domain id=\"any\">"
          "<Tracing><Verbosity>finest</></>"
          "<DDSSecurity>"
          "<Authentication>"
          "<Library path=\"dds_security_authentication_all_ok\" initFunction=\"init_authentication\" finalizeFunction=\"finalize_authentication\" />"
          "<IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
          "<IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
          "<PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
          "<Password>testtext_Password_testtext</Password>"
          "<TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
          "</Authentication>"
          "<Cryptographic>"
          "<Library path=\"dds_security_cryptography_missing_function\" initFunction=\"init_crypto\" finalizeFunction=\"finalize_crypto\"/>"
          "</Cryptographic>"
          "<AccessControl>"
          "<Library path=\"dds_security_access_control_all_ok\" initFunction=\"init_access_control\" finalizeFunction=\"finalize_access_control\"/>"
          "<Governance>file:Governance.p7s</Governance>"
          "<PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
          "<Permissions>file:Permissions.p7s</Permissions>"
          "</AccessControl>"
          "</DDSSecurity>"
          "</Domain>"
          "</"DDS_PROJECT_NAME">";


  dds_entity_t participant;

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_FATAL|DDS_LC_ERROR|DDS_LC_WARNING);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create participant with security elements. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, sec_config);
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  ddsrt_setenv(URI_VARIABLE, "");
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);

#ifdef PR304_MERGED
  /* It is better dds to return DDS_RETCODE_NOT_ALLOWED_BY_SECURITY instead of DDS_RETCODE_ERROR
    CU_ASSERT_FATAL( participant == DDS_RETCODE_NOT_ALLOWED_BY_SECURITY ); */
    CU_ASSERT_FATAL( participant == DDS_RETCODE_ERROR );
#else
  dds_delete(participant);
#endif
  CU_ASSERT_FATAL(found == 0x3);



}


CU_Test(ddssec_security_plugin_loading, no_library_in_path, .init = ddsrt_init, .fini = ddsrt_fini) {

  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
          "Could not load Authentication library: no_library_in_path: cannot open shared object file: No such file or directory*",
          "Could not load Authentication library: *not*found*",
          "Could not load Authentication plugin*",
          "Could not load security*",
          NULL
  };

  const char *sec_config =
          "<"DDS_PROJECT_NAME">"
            "<Domain id=\"any\">"
              "<Tracing><Verbosity>finest</></>"
              "<DDSSecurity>"
                "<Authentication>"
                  "<Library path=\"no_library_in_path\" initFunction=\"init_authentication\" finalizeFunction=\"finalize_authentication\" />"
                  "<IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
                  "<IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
                  "<PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
                  "<Password>testtext_Password_testtext</Password>"
                  "<TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
                "</Authentication>"
                "<Cryptographic>"
                  "<Library path=\"dds_security_cryptography_all_ok\" initFunction=\"init_crypto\" finalizeFunction=\"finalize_crypto\"/>"
                "</Cryptographic>"
                "<AccessControl>"
                  "<Library path=\"dds_security_access_control_all_ok\" initFunction=\"init_access_control\" finalizeFunction=\"finalize_access_control\"/>"
                  "<Governance>file:Governance.p7s</Governance>"
                  "<PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
                  "<Permissions>file:Permissions.p7s</Permissions>"
                "</AccessControl>"
              "</DDSSecurity>"
            "</Domain>"
          "</"DDS_PROJECT_NAME">";


  dds_entity_t participant;

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_FATAL|DDS_LC_ERROR|DDS_LC_WARNING|DDS_LC_INFO| DDS_LC_TRACE);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create participant with security elements. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, sec_config);
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  ddsrt_setenv(URI_VARIABLE, "");
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);

#ifdef PR304_MERGED
  /* It is better dds to return DDS_RETCODE_NOT_ALLOWED_BY_SECURITY instead of DDS_RETCODE_ERROR
    CU_ASSERT_FATAL( participant == DDS_RETCODE_NOT_ALLOWED_BY_SECURITY ); */
    CU_ASSERT_FATAL( participant == DDS_RETCODE_ERROR );
#else
  dds_delete(participant);
#endif
  
  CU_ASSERT_FATAL(found == 0xd || found == 0xe);

  dds_delete(participant);

}


CU_Test(ddssec_security_plugin_loading, init_error, .init = ddsrt_init, .fini = ddsrt_fini) {

  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
          "Error occured while initializing Authentication plugin*",
          "Could not load Authentication plugin*",
          "Could not load security*",
          NULL
  };

  const char *sec_config =
          "<"DDS_PROJECT_NAME">"
            "<Domain id=\"any\">"
              "<Tracing><Verbosity>finest</></>"
              "<DDSSecurity>"
                "<Authentication>"
                  "<Library path=\"dds_security_authentication_init_error\" initFunction=\"init_authentication\" finalizeFunction=\"finalize_authentication\" />"
                  "<IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
                  "<IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
                  "<PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
                  "<Password>testtext_Password_testtext</Password>"
                  "<TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
                "</Authentication>"
                "<Cryptographic>"
                  "<Library path=\"dds_security_cryptography_all_ok\" initFunction=\"init_crypto\" finalizeFunction=\"finalize_crypto\"/>"
                "</Cryptographic>"
                "<AccessControl>"
                  "<Library path=\"dds_security_access_control_all_ok\" initFunction=\"init_access_control\" finalizeFunction=\"finalize_access_control\"/>"
                  "<Governance>file:Governance.p7s</Governance>"
                  "<PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
                  "<Permissions>file:Permissions.p7s</Permissions>"
                "</AccessControl>"
              "</DDSSecurity>"
            "</Domain>"
          "</"DDS_PROJECT_NAME">";


  dds_entity_t participant;

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_FATAL|DDS_LC_ERROR|DDS_LC_WARNING|DDS_LC_INFO| DDS_LC_TRACE);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create participant with security elements. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, sec_config);
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  ddsrt_setenv(URI_VARIABLE, "");
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);

#ifdef PR304_MERGED
  /* It is better dds to return DDS_RETCODE_NOT_ALLOWED_BY_SECURITY instead of DDS_RETCODE_ERROR
    CU_ASSERT_FATAL( participant == DDS_RETCODE_NOT_ALLOWED_BY_SECURITY ); */
    CU_ASSERT_FATAL( participant == DDS_RETCODE_ERROR );
#else
  dds_delete(participant);
#endif
  CU_ASSERT_FATAL(found == 0x7);


  dds_delete(participant);

}
CU_Test(ddssec_security_plugin_loading, all_ok_with_props, .init = ddsrt_init, .fini = ddsrt_fini) {
  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
          "DDS Security plugins have been loaded*",
          NULL
  };

  dds_entity_t participant;
  dds_qos_t * qos;
  
  
  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_INFO);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);
  
  /* Create the qos */
  unsigned char bvalue[3] = { 0x01, 0x02, 0x03 };
  CU_ASSERT_FATAL ((qos = dds_create_qos()) != NULL);
  dds_qset_prop (qos, "test.prop1", "testtext_value1_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_IDENTITY_CA, "testtext_IdentityCA_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_PRIV_KEY, "testtext_PrivateKey_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_IDENTITY_CERT, "testtext_IdentityCertificate_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_PERMISSIONS_CA, "file:Permissions_CA.pem");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_GOVERNANCE, "file:Governance.p7s");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_PERMISSIONS, "file:Permissions.p7s");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_PASSWORD, "testtext_Password_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_TRUSTED_CA_DIR, "file:/test/dir");

  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_LIBRARY_PATH, "dds_security_authentication_all_ok");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_LIBRARY_INIT, "init_authentication");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_LIBRARY_FINALIZE, "finalize_authentication");
  dds_qset_prop (qos, DDS_SEC_PROP_CRYPTO_LIBRARY_PATH, "dds_security_cryptography_all_ok");
  dds_qset_prop (qos, DDS_SEC_PROP_CRYPTO_LIBRARY_INIT, "init_crypto");
  dds_qset_prop (qos, DDS_SEC_PROP_CRYPTO_LIBRARY_FINALIZE, "finalize_crypto");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_LIBRARY_PATH, "dds_security_access_control_all_ok");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_LIBRARY_INIT, "init_access_control");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_LIBRARY_FINALIZE, "finalize_access_control");

  dds_qset_prop (qos, "test.prop2", "testtext_value2_testtext");

  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_IDENTITY_CA, "testtext_IdentityCA_testtext");

  dds_qset_bprop (qos, "test.bprop1", bvalue, 3);

  /* Create participant with security config in qos. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, "<Tracing><Verbosity>finest</></>");
  CU_ASSERT_FATAL ((participant = dds_create_participant(DDS_DOMAIN_DEFAULT, qos, NULL)) > 0);
  ddsrt_setenv(URI_VARIABLE, "");
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);

  dds_delete(participant);
  dds_delete_qos(qos);
  CU_ASSERT_FATAL(found == 0x1);
}



CU_Test(ddssec_security_plugin_loading, missing_plugin_property_with_props, .init = ddsrt_init, .fini = ddsrt_fini) {
  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
          "*using security settings from QoS*",
          "*required security property dds.sec.auth.library.init missing in Property QoS*",
          NULL
  };

  dds_entity_t participant;
  dds_qos_t * qos;

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_INFO|DDS_LC_ERROR);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create the qos */
  unsigned char bvalue[3] = { 0x01, 0x02, 0x03 };
  CU_ASSERT_FATAL ((qos = dds_create_qos()) != NULL);
  dds_qset_prop (qos, "test.prop1", "testtext_value1_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_IDENTITY_CA, "testtext_IdentityCA_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_PRIV_KEY, "testtext_PrivateKey_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_IDENTITY_CERT, "testtext_IdentityCertificate_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_PERMISSIONS_CA, "file:Permissions_CA.pem");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_GOVERNANCE, "file:Governance.p7s");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_PERMISSIONS, "file:Permissions.p7s");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_PASSWORD, "testtext_Password_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_TRUSTED_CA_DIR, "file:/test/dir");

  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_LIBRARY_PATH, "dds_security_authentication_all_ok");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_LIBRARY_FINALIZE, "finalize_authentication");
  dds_qset_prop (qos, DDS_SEC_PROP_CRYPTO_LIBRARY_PATH, "dds_security_cryptography_all_ok");
  dds_qset_prop (qos, DDS_SEC_PROP_CRYPTO_LIBRARY_INIT, "init_crypto");
  dds_qset_prop (qos, DDS_SEC_PROP_CRYPTO_LIBRARY_FINALIZE, "finalize_crypto");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_LIBRARY_PATH, "dds_security_access_control_all_ok");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_LIBRARY_INIT, "init_access_control");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_LIBRARY_FINALIZE, "finalize_access_control");

  dds_qset_prop (qos, "test.prop2", "testtext_value2_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_IDENTITY_CA, "testtext_IdentityCA_testtext");
  dds_qset_bprop (qos, "test.bprop1", bvalue, 3);

  /* Create participant with security config in qos. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, "<Tracing><Verbosity>finest</></>");
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, qos, NULL);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);
  ddsrt_setenv(URI_VARIABLE, "");
#ifdef PR304_MERGED
  /* It is better dds to return DDS_RETCODE_NOT_ALLOWED_BY_SECURITY instead of DDS_RETCODE_ERROR
    CU_ASSERT_FATAL( participant == DDS_RETCODE_NOT_ALLOWED_BY_SECURITY ); */
    CU_ASSERT_FATAL( participant == DDS_RETCODE_ERROR );
#else
  dds_delete(participant);
#endif
  CU_ASSERT_FATAL(found == 0x3);
  dds_delete_qos(qos);
}



CU_Test(ddssec_security_plugin_loading, empty_plugin_property_with_props, .init = ddsrt_init, .fini = ddsrt_fini) {
  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
          "*using security settings from QoS*",
          "*required security property dds.sec.auth.library.finalize missing in Property QoS*",
          NULL
  };

  dds_entity_t participant;
  dds_qos_t * qos;

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_INFO|DDS_LC_ERROR);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create the qos */
  unsigned char bvalue[3] = { 0x01, 0x02, 0x03 };
  CU_ASSERT_FATAL ((qos = dds_create_qos()) != NULL);
  dds_qset_prop (qos, "test.prop1", "testtext_value1_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_IDENTITY_CA, "testtext_IdentityCA_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_PRIV_KEY, "testtext_PrivateKey_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_IDENTITY_CERT, "testtext_IdentityCertificate_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_PERMISSIONS_CA, "file:Permissions_CA.pem");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_GOVERNANCE, "file:Governance.p7s");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_PERMISSIONS, "file:Permissions.p7s");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_PASSWORD, "testtext_Password_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_TRUSTED_CA_DIR, "file:/test/dir");

  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_LIBRARY_PATH, "dds_security_authentication_all_ok");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_LIBRARY_INIT, "init_authentication");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_LIBRARY_FINALIZE, "");
  dds_qset_prop (qos, DDS_SEC_PROP_CRYPTO_LIBRARY_PATH, "dds_security_cryptography_all_ok");
  dds_qset_prop (qos, DDS_SEC_PROP_CRYPTO_LIBRARY_INIT, "init_crypto");
  dds_qset_prop (qos, DDS_SEC_PROP_CRYPTO_LIBRARY_FINALIZE, "finalize_crypto");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_LIBRARY_PATH, "dds_security_access_control_all_ok");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_LIBRARY_INIT, "init_access_control");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_LIBRARY_FINALIZE, "finalize_access_control");

  dds_qset_prop (qos, "test.prop2", "testtext_value2_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_IDENTITY_CA, "testtext_IdentityCA_testtext");
  dds_qset_bprop (qos, "test.bprop1", bvalue, 3);

  /* Create participant with security config in qos. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, "<Tracing><Verbosity>finest</></>");
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, qos, NULL);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);
  ddsrt_setenv(URI_VARIABLE, "");
#ifdef PR304_MERGED
  /* It is better dds to return DDS_RETCODE_NOT_ALLOWED_BY_SECURITY instead of DDS_RETCODE_ERROR
    CU_ASSERT_FATAL( participant == DDS_RETCODE_NOT_ALLOWED_BY_SECURITY ); */
    CU_ASSERT_FATAL( participant == DDS_RETCODE_ERROR );
#else
  dds_delete(participant);
#endif
  CU_ASSERT_FATAL(found == 0x3);
  dds_delete_qos(qos);
}


CU_Test(ddssec_security_plugin_loading, missing_security_property_with_props, .init = ddsrt_init, .fini = ddsrt_fini) {
  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
          "*using security settings from QoS*",
          "*required security property dds.sec.access.permissions missing in Property QoS*",
          NULL
  };


  dds_entity_t participant;
  dds_qos_t * qos;

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_INFO|DDS_LC_ERROR);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create the qos */
  unsigned char bvalue[3] = { 0x01, 0x02, 0x03 };
  CU_ASSERT_FATAL ((qos = dds_create_qos()) != NULL);
  dds_qset_prop (qos, "test.prop1", "testtext_value1_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_IDENTITY_CA, "testtext_IdentityCA_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_PRIV_KEY, "testtext_PrivateKey_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_IDENTITY_CERT, "testtext_IdentityCertificate_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_PERMISSIONS_CA, "file:Permissions_CA.pem");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_GOVERNANCE, "file:Governance.p7s");
  /* we ignore permissions for testing
  //dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_PERMISSIONS, "file:Permissions.p7s"); */
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_PASSWORD, "testtext_Password_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_TRUSTED_CA_DIR, "file:/test/dir");

  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_LIBRARY_PATH, "dds_security_authentication_all_ok");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_LIBRARY_INIT, "init_authentication");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_LIBRARY_FINALIZE, "finalize_authentication");
  dds_qset_prop (qos, DDS_SEC_PROP_CRYPTO_LIBRARY_PATH, "dds_security_cryptography_all_ok");
  dds_qset_prop (qos, DDS_SEC_PROP_CRYPTO_LIBRARY_INIT, "init_crypto");
  dds_qset_prop (qos, DDS_SEC_PROP_CRYPTO_LIBRARY_FINALIZE, "finalize_crypto");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_LIBRARY_PATH, "dds_security_access_control_all_ok");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_LIBRARY_INIT, "init_access_control");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_LIBRARY_FINALIZE, "finalize_access_control");

  dds_qset_prop (qos, "test.prop2", "testtext_value2_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_IDENTITY_CA, "testtext_IdentityCA_testtext");
  dds_qset_bprop (qos, "test.bprop1", bvalue, 3);

  /* Create participant with security config in qos. */
  found = 0;
  ddsrt_setenv(URI_VARIABLE, "<Tracing><Verbosity>finest</></>");
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, qos, NULL);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);
  ddsrt_setenv(URI_VARIABLE, "");
#ifdef PR304_MERGED
  /* It is better dds to return DDS_RETCODE_NOT_ALLOWED_BY_SECURITY instead of DDS_RETCODE_ERROR
    CU_ASSERT_FATAL( participant == DDS_RETCODE_NOT_ALLOWED_BY_SECURITY ); */
    CU_ASSERT_FATAL( participant == DDS_RETCODE_ERROR );
#else
  dds_delete(participant);
#endif
  CU_ASSERT_FATAL(found == 0x3);
  dds_delete_qos(qos);
}




CU_Test(ddssec_security_plugin_loading, multiple_domains_different_config, .init = ddsrt_init, .fini = ddsrt_fini) {
  /* Expected traces when creating participant with the security elements. */
  const char *log_expected[] = {
          "*using security settings from configuration*",
          "*using security settings from QoS*",
          "DDS Security plugins have been loaded*",
          "*security is already loaded for this domain*",
          NULL
  };

  const char *sec_config =
        "<"DDS_PROJECT_NAME">"
          "<Domain id=\"1\">"
            "<Tracing><Verbosity>finest</></>"
            "<DDSSecurity>"
              "<Authentication>"
                "<Library path=\"dds_security_authentication_all_ok\" initFunction=\"init_authentication\" finalizeFunction=\"finalize_authentication\" />"
                "<IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
                "<IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
                "<PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
                "<Password>testtext_Password_testtext</Password>"
                "<TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
              "</Authentication>"
              "<Cryptographic>"
                "<Library path=\"dds_security_cryptography_all_ok\" initFunction=\"init_crypto\" finalizeFunction=\"finalize_crypto\"/>"
              "</Cryptographic>"
              "<AccessControl>"
                "<Library path=\"dds_security_access_control_all_ok\" initFunction=\"init_access_control\" finalizeFunction=\"finalize_access_control\"/>"
                "<Governance>file:Governance.p7s</Governance>"
                "<PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
                "<Permissions>file:Permissions.p7s</Permissions>"
              "</AccessControl>"
            "</DDSSecurity>"
          "</Domain>"
          "<Domain id=\"2\">"
            "<Tracing><Verbosity>finest</></>"
            "<DDSSecurity>"
              "<Authentication>"
                "<Library path=\"dds_security_authentication_invalid\" initFunction=\"init_authentication\" finalizeFunction=\"finalize_authentication\" />"
                "<IdentityCertificate>testtext_IdentityCertificate_testtext</IdentityCertificate>"
                "<IdentityCA>testtext_IdentityCA_testtext</IdentityCA>"
                "<PrivateKey>testtext_PrivateKey_testtext</PrivateKey>"
                "<Password>testtext_Password_testtext</Password>"
                "<TrustedCADirectory>testtext_Dir_testtext</TrustedCADirectory>"
              "</Authentication>"
              "<Cryptographic>"
                "<Library path=\"dds_security_cryptography_invalid\" initFunction=\"init_crypto\" finalizeFunction=\"finalize_crypto\"/>"
              "</Cryptographic>"
              "<AccessControl>"
                "<Library path=\"dds_security_access_control_invalid\" initFunction=\"init_access_control\" finalizeFunction=\"finalize_access_control\"/>"
                "<Governance>file:Governance.p7s</Governance>"
                "<PermissionsCA>file:Permissions_CA.pem</PermissionsCA>"
                "<Permissions>file:Permissions.p7s</Permissions>"
              "</AccessControl>"
            "</DDSSecurity>"
          "</Domain>"
        "</"DDS_PROJECT_NAME">";


  dds_entity_t participant1, participant2, participant3;
  dds_qos_t * qos;

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask(DDS_LC_INFO|DDS_LC_ERROR);
  dds_set_log_sink(&logger, (void*)log_expected);
  dds_set_trace_sink(&logger, (void*)log_expected);

  /* Create the qos */
  unsigned char bvalue[3] = { 0x01, 0x02, 0x03 };
  CU_ASSERT_FATAL ((qos = dds_create_qos()) != NULL);
  dds_qset_prop (qos, "test.prop1", "testtext_value1_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_IDENTITY_CA, "testtext_IdentityCA_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_PRIV_KEY, "testtext_PrivateKey_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_IDENTITY_CERT, "testtext_IdentityCertificate_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_PERMISSIONS_CA, "file:Permissions_CA.pem");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_GOVERNANCE, "file:Governance.p7s");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_PERMISSIONS, "file:Permissions.p7s");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_PASSWORD, "testtext_Password_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_TRUSTED_CA_DIR, "file:/test/dir");

  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_LIBRARY_PATH, "dds_security_authentication_all_ok_other");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_LIBRARY_INIT, "init_authentication");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_LIBRARY_FINALIZE, "finalize_authentication");
  dds_qset_prop (qos, DDS_SEC_PROP_CRYPTO_LIBRARY_PATH, "dds_security_cryptography_all_ok");
  dds_qset_prop (qos, DDS_SEC_PROP_CRYPTO_LIBRARY_INIT, "init_crypto");
  dds_qset_prop (qos, DDS_SEC_PROP_CRYPTO_LIBRARY_FINALIZE, "finalize_crypto");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_LIBRARY_PATH, "dds_security_access_control_all_ok");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_LIBRARY_INIT, "init_access_control");
  dds_qset_prop (qos, DDS_SEC_PROP_ACCESS_LIBRARY_FINALIZE, "finalize_access_control");

  dds_qset_prop (qos, "test.prop2", "testtext_value2_testtext");
  dds_qset_prop (qos, DDS_SEC_PROP_AUTH_IDENTITY_CA, "testtext_IdentityCA_testtext");
  dds_qset_bprop (qos, "test.bprop1", bvalue, 3);

  /* Create participant with security config in qos. */
  found = 0;
  print_log = true;
  ddsrt_setenv(URI_VARIABLE, sec_config);
  participant1 = dds_create_participant(1, NULL, NULL);
  participant2 = dds_create_participant(2, qos, NULL);
  participant3 = dds_create_participant(2, NULL, NULL);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);
  ddsrt_setenv(URI_VARIABLE, "");
#ifdef PR304_MERGED
  /* It is better dds to return DDS_RETCODE_NOT_ALLOWED_BY_SECURITY instead of DDS_RETCODE_ERROR
    CU_ASSERT_FATAL( participant1 == DDS_RETCODE_NOT_ALLOWED_BY_SECURITY ); */
    CU_ASSERT_FATAL( participant1 == DDS_RETCODE_ERROR );
#else
  dds_delete(participant1);
  dds_delete(participant2);
  dds_delete(participant3);
#endif
  CU_ASSERT_FATAL(found == 0xf);
  dds_delete_qos(qos);
}
