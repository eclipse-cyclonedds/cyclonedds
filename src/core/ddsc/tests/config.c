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

#include "dds/dds.h"
#include "config_env.h"

#include "dds/version.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/heap.h"
#include "ddsi__misc.h"
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

CU_Test (ddsc_config, ignoredpartition, .init = ddsrt_init, .fini = ddsrt_fini)
{
#ifndef DDS_HAS_NETWORK_PARTITIONS
  CU_PASS("no network partitions in build");
#else
  char tpname_ignore[100];
  create_unique_topic_name ("ddsc_config_ignoredpartition_ignore", tpname_ignore, sizeof (tpname_ignore));
  char tpname_normal[100];
  create_unique_topic_name ("ddsc_config_ignoredpartition_normal", tpname_normal, sizeof (tpname_normal));

  const char *cyclonedds_uri;
  if (ddsrt_getenv ("CYCLONEDDS_URI", &cyclonedds_uri) != DDS_RETCODE_OK)
    cyclonedds_uri = "";
  char *config;
  (void) ddsrt_asprintf (&config, "%s,"
                         "<Discovery>"
                         "  <ExternalDomainId>0</ExternalDomainId>"
                         "</Discovery>"
                         "<Partitioning>"
                         "  <IgnoredPartitions>"
                         "    <IgnoredPartition DCPSPartitionTopic=\".%s\"/>"
                         "  </IgnoredPartitions>"
                         "</Partitioning>",
                         cyclonedds_uri, tpname_ignore);
  dds_entity_t domw = dds_create_domain (0, config);
  CU_ASSERT_FATAL (domw > 0);
  dds_entity_t domr = dds_create_domain (1, config);
  CU_ASSERT_FATAL (domr > 0);
  ddsrt_free (config);

  dds_entity_t dpw = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (dpw > 0);
  dds_entity_t dpr = dds_create_participant (1, NULL, NULL);
  CU_ASSERT_FATAL (dpr > 0);

  dds_entity_t tpw_i = dds_create_topic (dpw, &Space_Type1_desc, tpname_ignore, NULL, NULL);
  CU_ASSERT_FATAL (tpw_i > 0);
  dds_entity_t tpw_n = dds_create_topic (dpw, &Space_Type1_desc, tpname_normal, NULL, NULL);
  CU_ASSERT_FATAL (tpw_n > 0);

  dds_entity_t tpr_i = dds_create_topic (dpr, &Space_Type1_desc, tpname_ignore, NULL, NULL);
  CU_ASSERT_FATAL (tpr_i > 0);
  dds_entity_t tpr_n = dds_create_topic (dpr, &Space_Type1_desc, tpname_normal, NULL, NULL);
  CU_ASSERT_FATAL (tpr_n > 0);

  dds_qos_t *qos = dds_create_qos ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  dds_entity_t wr_i = dds_create_writer (dpw, tpw_i, qos, NULL);
  CU_ASSERT_FATAL (wr_i > 0);
  dds_entity_t wr_n = dds_create_writer (dpw, tpw_n, qos, NULL);
  CU_ASSERT_FATAL (wr_i > 0);
  dds_entity_t rd_i = dds_create_reader (dpr, tpr_i, qos, NULL);
  CU_ASSERT_FATAL (rd_i > 0);
  dds_entity_t rd_n = dds_create_reader (dpr, tpr_n, qos, NULL);
  CU_ASSERT_FATAL (rd_n > 0);

  dds_return_t rc;
  rc = dds_set_status_mask (wr_i, DDS_PUBLICATION_MATCHED_STATUS);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_set_status_mask (wr_n, DDS_PUBLICATION_MATCHED_STATUS);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_set_status_mask (rd_i, DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_DATA_AVAILABLE_STATUS);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_set_status_mask (rd_n, DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_DATA_AVAILABLE_STATUS);
  CU_ASSERT_FATAL (rc == 0);

  // Wait for a match on the "normal" topic, with standard DDSI discovery, that implies
  // the ignored ones would have been discovered
  dds_entity_t ws = dds_create_waitset (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (ws > 0);
  rc = dds_waitset_attach (ws, wr_n, 1);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_waitset_attach (ws, rd_n, 2);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_waitset_attach (ws, wr_i, 4);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_waitset_attach (ws, rd_i, 8);
  CU_ASSERT_FATAL (rc == 0);

  unsigned waitfor;
  waitfor = 1 + 2; // there shouldn't be any events on the ignored ones
  while (waitfor != 0)
  {
    dds_attach_t xs[4];
    rc = dds_waitset_wait (ws, xs, sizeof (xs) / sizeof (xs[0]), DDS_INFINITY);
    CU_ASSERT_FATAL (rc >= 0);
    for (int32_t i = 0; i < rc; i++)
      waitfor &= ~(unsigned)xs[i];
  }

  uint32_t status;
  rc = dds_take_status (wr_i, &status, DDS_PUBLICATION_MATCHED_STATUS);
  CU_ASSERT_FATAL (rc == 0);
  CU_ASSERT_FATAL (status == 0);
  rc = dds_take_status (rd_i, &status, DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_DATA_AVAILABLE_STATUS);
  CU_ASSERT_FATAL (rc == 0);
  CU_ASSERT_FATAL (status == 0);
  rc = dds_take_status (wr_n, &status, DDS_PUBLICATION_MATCHED_STATUS);
  CU_ASSERT_FATAL (rc == 0);
  CU_ASSERT_FATAL (status == DDS_PUBLICATION_MATCHED_STATUS);
  rc = dds_take_status (rd_n, &status, DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_DATA_AVAILABLE_STATUS);
  CU_ASSERT_FATAL (rc == 0);
  CU_ASSERT_FATAL (status == DDS_SUBSCRIPTION_MATCHED_STATUS);

  // add a reader for the ignored topic on the writing participant for checking data still arrives locally
  dds_entity_t rd_i_wr = dds_create_reader (dpw, tpw_i, qos, NULL);
  CU_ASSERT_FATAL (rd_i_wr > 0);
  rc = dds_set_status_mask (rd_i_wr, DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_DATA_AVAILABLE_STATUS);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_take_status (wr_i, &status, DDS_PUBLICATION_MATCHED_STATUS);
  CU_ASSERT_FATAL (rc == 0);
  CU_ASSERT_FATAL (status == DDS_PUBLICATION_MATCHED_STATUS);
  rc = dds_take_status (rd_i_wr, &status, DDS_SUBSCRIPTION_MATCHED_STATUS);
  CU_ASSERT_FATAL (rc == 0);
  CU_ASSERT_FATAL (status == DDS_SUBSCRIPTION_MATCHED_STATUS);
  rc = dds_waitset_attach (ws, rd_i_wr, 16);
  CU_ASSERT_FATAL (rc == 0);

  const Space_Type1 s_i = { 301876963, 1211346953, 447421619 };
  const Space_Type1 s_n = { 127347047, 1130047829, 1446097241 };
  rc = dds_write (wr_i, &s_i);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_write (wr_n, &s_n);
  CU_ASSERT_FATAL (rc == 0);

  waitfor = 2 | 16; // there should only be DATA_AVAILABLE event on rd_n & rd_i_wr
  while (waitfor != 0)
  {
    dds_attach_t xs[5];
    rc = dds_waitset_wait (ws, xs, sizeof (xs) / sizeof (xs[0]), DDS_INFINITY);
    CU_ASSERT_FATAL (rc >= 0);
    for (int32_t i = 0; i < rc; i++)
      waitfor &= ~(unsigned)xs[i];
  }

  rc = dds_take_status (wr_i, &status, DDS_PUBLICATION_MATCHED_STATUS);
  CU_ASSERT_FATAL (rc == 0);
  CU_ASSERT_FATAL (status == 0);
  rc = dds_take_status (rd_i, &status, DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_DATA_AVAILABLE_STATUS);
  CU_ASSERT_FATAL (rc == 0);
  CU_ASSERT_FATAL (status == 0);
  rc = dds_take_status (wr_n, &status, DDS_PUBLICATION_MATCHED_STATUS);
  CU_ASSERT_FATAL (rc == 0);
  CU_ASSERT_FATAL (status == 0);
  rc = dds_take_status (rd_n, &status, DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_DATA_AVAILABLE_STATUS);
  CU_ASSERT_FATAL (rc == 0);
  CU_ASSERT_FATAL (status == DDS_DATA_AVAILABLE_STATUS);
  rc = dds_take_status (rd_i_wr, &status, DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_DATA_AVAILABLE_STATUS);
  CU_ASSERT_FATAL (rc == 0);
  CU_ASSERT_FATAL (status == DDS_DATA_AVAILABLE_STATUS);

  Space_Type1 sample;
  dds_sample_info_t si;
  void *raw = &sample;
  int32_t n;

  n = dds_take (rd_n, &raw, &si, 1, 1);
  CU_ASSERT_FATAL (n == 1);
  CU_ASSERT_FATAL (sample.long_1 == s_n.long_1);
  CU_ASSERT_FATAL (sample.long_2 == s_n.long_2);
  CU_ASSERT_FATAL (sample.long_3 == s_n.long_3);

  n = dds_take (rd_i_wr, &raw, &si, 1, 1);
  CU_ASSERT_FATAL (n == 1);
  CU_ASSERT_FATAL (sample.long_1 == s_i.long_1);
  CU_ASSERT_FATAL (sample.long_2 == s_i.long_2);
  CU_ASSERT_FATAL (sample.long_3 == s_i.long_3);

  dds_delete_qos (qos);
  dds_delete (DDS_CYCLONEDDS_HANDLE);
#endif
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
      if (ddsi_patmatch(expected[i], data->message)) {
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
    "ddsi_new_participant(*): using security settings from QoS*",
    "ddsi_new_participant(*): required security property * missing*",
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

CU_Test(ddsc_config, invalid_envvar, .init = ddsrt_init, .fini = ddsrt_fini)
{
  const char *log_expected[] = {
    "*invalid expansion*",
    NULL
  };

  dds_set_log_mask (DDS_LC_FATAL|DDS_LC_ERROR|DDS_LC_WARNING|DDS_LC_CONFIG);
  dds_set_log_sink (&logger, (void *) log_expected);
  dds_set_trace_sink (&logger, (void *) log_expected);

  found = 0;
  dds_entity_t domain;
  domain = dds_create_domain (0, "<Discovery><Tag>${INVALID_EXPANSION</Tag></Discovery>");
  CU_ASSERT_FATAL (domain < 0);
  CU_ASSERT_FATAL (found == 0x1);

  found = 0;
  domain = dds_create_domain (0, "<Discovery><Peers><Peer address=\"${INVALID_EXPANSION\"/></Peers></Discovery>");
  CU_ASSERT_FATAL (domain < 0);
  CU_ASSERT_FATAL (found == 0x1);

  dds_set_log_sink (NULL, NULL);
  dds_set_trace_sink (NULL, NULL);
}

CU_Test(ddsc_config, too_deep_nesting, .init = ddsrt_init, .fini = ddsrt_fini)
{
  const char *log_expected[] = {
    "*too deeply nested*",
    NULL
  };

  dds_set_log_mask (DDS_LC_FATAL|DDS_LC_ERROR|DDS_LC_WARNING|DDS_LC_CONFIG);
  dds_set_log_sink (&logger, (void *) log_expected);
  dds_set_trace_sink (&logger, (void *) log_expected);

  found = 0;
  dds_entity_t domain = dds_create_domain (0, "<A><B><C><D><E><F><G><H><I><J><K><L>");
  CU_ASSERT_FATAL (domain < 0);
  CU_ASSERT_FATAL (found == 0x1);

  dds_set_log_sink (NULL, NULL);
  dds_set_trace_sink (NULL, NULL);
}

CU_Test(ddsc_config, multiple_domains, .init = ddsrt_init, .fini = ddsrt_fini)
{
  static const char *config = "\
<CycloneDDS>\
  <Domain id=\"any\">\
    <Tracing>\
      <Category>config</Category>\
    </Tracing>\
    <Compatibility>\
      <StandardsConformance>strict</StandardsConformance>\
    </Compatibility>\
  </Domain>\
  <Domain id=\"53\">\
    <Discovery>\
      <Tag>W</Tag>\
    </Discovery>\
  </Domain>\
  <Domain id=\"57\">\
    <Discovery>\
      <Tag>A</Tag>\
    </Discovery>\
  </Domain>\
</CycloneDDS>\
";
  const char *exp[][4] = {
    {
      "*config: Domain/Discovery/Tag/#text: W {1}*",
      "*config: Domain/Compatibility/StandardsConformance/#text: strict {0}*",
      "*config: Domain[@Id]: 53 {0,1,2}*",
      NULL
    },
    {
      "*config: Domain/Discovery/Tag/#text:  {}*",
      "*config: Domain/Compatibility/StandardsConformance/#text: strict {0}*",
      "*config: Domain[@Id]: 54 {0,1}*",
      NULL
    },
    {
      "*config: Domain/Discovery/Tag/#text: A {1}*",
      "*config: Domain/Compatibility/StandardsConformance/#text: strict {0}*",
      "*config: Domain[@Id]: 57 {0,1}*",
      NULL
    }
  };
  dds_entity_t doms[3];

  dds_set_log_mask (DDS_LC_FATAL|DDS_LC_ERROR|DDS_LC_WARNING|DDS_LC_CONFIG);

  dds_set_log_sink (&logger, (void *) exp[0]);
  dds_set_trace_sink (&logger, (void *) exp[0]);
  found = 0;
  doms[0] = dds_create_domain (53, config);
  CU_ASSERT_FATAL (doms[0] > 0);
  printf ("found = %d\n", found);
  CU_ASSERT_FATAL (found == 7);

  dds_set_log_sink (&logger, (void *) exp[1]);
  dds_set_trace_sink (&logger, (void *) exp[1]);
  found = 0;
  doms[1] = dds_create_domain (54, config);
  CU_ASSERT_FATAL (doms[1] > 0 && doms[1] != doms[0]);
  printf ("found = %d\n", found);
  CU_ASSERT_FATAL (found == 7);

  dds_set_log_sink (&logger, (void *) exp[2]);
  dds_set_trace_sink (&logger, (void *) exp[2]);
  found = 0;
  doms[2] = dds_create_domain (57, config);
  CU_ASSERT_FATAL (doms[2] > 0 && doms[2] != doms[1] && doms[2] != doms[0]);
  printf ("found = %d\n", found);
  CU_ASSERT_FATAL (found == 7);

  for (int i = 0; i < 3; i++)
  {
    const dds_return_t rc = dds_delete (doms[i]);
    CU_ASSERT_FATAL (rc == 0);
  }

  dds_set_log_sink (NULL, NULL);
  dds_set_trace_sink (NULL, NULL);
}

CU_Test(ddsc_config, bad_configs_listelems)
{
  // The first one is thanks to OSS-Fuzz, the fact that it is so easy
  // to forget an initialisation that can trigger this means it is
  // worthwhile trying a few more case
  const char *configs[] = {
    "<Partitioning><NetworkPartitions><NetworkPartition",
    "<Partitioning><PartitionMappings><PartitionMapping",
    "<Partitioning><IgnoredPartitions><IgnoredPartition",
    "<Threads><Thread",
    "<NetworkInterfaces><NetworkInterface",
    "<Discovery><Peers><Peer",
    NULL
  };
  for (int i = 0; configs[i]; i++)
  {
    CU_ASSERT_FATAL (dds_create_domain (0, configs[i]) < 0);
  }
}
