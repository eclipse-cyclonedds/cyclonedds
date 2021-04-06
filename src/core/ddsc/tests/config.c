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
#include "config_env.h"

#include "dds/version.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/ddsi_xqos.h"

#include "test_common.h"

#define FORCE_ENV

static void config__check_env (const char *env_variable, const char *expected_value)
{
  const char *env_uri = NULL;
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
  config__check_env ("CYCLONEDDS_URI", CONFIG_ENV_SIMPLE_UDP);
  config__check_env ("MAX_PARTICIPANTS", CONFIG_ENV_MAX_PARTICIPANTS);
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL (participant> 0);
  dds_delete (participant);
}

CU_Test (ddsc_config, user_config, .init = ddsrt_init, .fini = ddsrt_fini)
{
  dds_entity_t domain;
  domain = dds_create_domain (1,
                              "<CycloneDDS><Domain><Id>any</Id></Domain>"
                              "<DDSI2E><Internal><MaxParticipants>2</MaxParticipants></Internal></DDSI2E>"
                              "</CycloneDDS>");
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

CU_Test(ddsc_security_config, empty, .init = ddsrt_init, .fini = ddsrt_fini)
{
  /* Expected traces when creating participant with an empty security element.  We need to
     test this one here to be sure that it refuses to start when security is configured
     but the implementation doesn't include support for it. */
  const char *log_expected[] = {
#ifndef DDS_HAS_SECURITY
    "config: //CycloneDDS/Domain: Security: unknown element*",
#else
    "config: //CycloneDDS/Domain/Security/Authentication/IdentityCertificate/#text: element missing in configuration*",
    "config: //CycloneDDS/Domain/Security/Authentication/IdentityCA/#text: element missing in configuration*",
    "config: //CycloneDDS/Domain/Security/Authentication/PrivateKey/#text: element missing in configuration*",
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
  ddsrt_setenv("CYCLONEDDS_URI", "<Security/>");
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  ddsrt_setenv("CYCLONEDDS_URI", "");
  CU_ASSERT_FATAL(participant < 0);
  dds_set_log_sink(NULL, NULL);
  dds_set_trace_sink(NULL, NULL);

  /* All traces should have been provided. */
#ifndef DDS_HAS_SECURITY
  CU_ASSERT_FATAL(found == 0x1);
#else
  CU_ASSERT_FATAL(found == 0x7);
#endif
}

CU_Test(ddsc_security_qos, empty, .init = ddsrt_init, .fini = ddsrt_fini)
{
  /* Expected traces when creating participant with some (not all) security QoS
     settings.  We need to test this one here to be sure that it also refuses to
     start when security is configured but the implementation doesn't include
     support for it. */
  const char *log_expected[] = {
#ifdef DDS_HAS_SECURITY
    "new_participant(*): using security settings from QoS*",
    "new_participant(*): required security property * missing*",
#endif
    NULL
  };

  /* Set up the trace sinks to detect the config parsing. */
  dds_set_log_mask (DDS_LC_FATAL|DDS_LC_ERROR|DDS_LC_WARNING|DDS_LC_CONFIG);
  dds_set_log_sink (&logger, (void *) log_expected);
  dds_set_trace_sink (&logger, (void *) log_expected);

  /* Create participant with incomplete/nonsensical security configuration: this should always fail */
  found = 0;
  dds_qos_t *qos = dds_create_qos ();
  dds_qset_prop (qos, "dds.sec.nonsense", "");
  dds_entity_t domain = dds_create_domain (0, "<Tracing><Category>trace</Category>");
  CU_ASSERT_FATAL (domain > 0);
  dds_entity_t participant = dds_create_participant (0, qos, NULL);
  dds_delete_qos (qos);
  CU_ASSERT_FATAL (participant < 0);
  (void) dds_delete (domain);
  dds_set_log_sink (NULL, NULL);
  dds_set_trace_sink (NULL, NULL);

  /* All traces should have been provided. */
#ifndef DDS_HAS_SECURITY
  CU_ASSERT_FATAL (found == 0x0);
#else
  CU_ASSERT_FATAL (found == 0x3);
#endif
}
