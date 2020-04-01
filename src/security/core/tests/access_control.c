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
#include "dds/ddsrt/io.h"
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
#include "common/test_utils.h"
#include "common/cert_utils.h"
#include "SecurityCoreTests.h"

static const char *config =
    "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}"
    "<Domain id=\"any\">"
    "  <Discovery>"
    "    <ExternalDomainId>0</ExternalDomainId>"
    "    <Tag>\\${CYCLONEDDS_PID}</Tag>"
    "  </Discovery>"
    "  <DDSSecurity>"
    "    <Authentication>"
    "      <Library finalizeFunction=\"finalize_authentication\" initFunction=\"init_authentication\"/>"
    "      <IdentityCertificate>data:,${TEST_IDENTITY_CERTIFICATE}</IdentityCertificate>"
    "      <PrivateKey>data:,${TEST_IDENTITY_PRIVATE_KEY}</PrivateKey>"
    "      <IdentityCA>data:,${TEST_IDENTITY_CA_CERTIFICATE}</IdentityCA>"
    "    </Authentication>"
    "    <AccessControl>"
    "      <Library finalizeFunction=\"finalize_access_control\" initFunction=\"init_access_control\"/>"
    "      ${INCL_GOV:+<Governance><![CDATA[}${TEST_GOVERNANCE}${INCL_GOV:+]]></Governance>}"
    "      ${INCL_PERM_CA:+<PermissionsCA>}${TEST_PERMISSIONS_CA}${INCL_PERM_CA:+</PermissionsCA>}"
    "      ${INCL_PERM:+<Permissions><![CDATA[}${TEST_PERMISSIONS}${INCL_PERM:+]]></Permissions>}"
    "    </AccessControl>"
    "    <Cryptographic>"
    "      <Library finalizeFunction=\"finalize_crypto\" initFunction=\"init_crypto\"/>"
    "    </Cryptographic>"
    "  </DDSSecurity>"
    "</Domain>";

#define MAX_DOMAINS 10
#define DDS_DOMAINID

static dds_entity_t g_domain[MAX_DOMAINS];
static dds_entity_t g_participant[MAX_DOMAINS];
static dds_entity_t g_pubsub[MAX_DOMAINS];
static dds_entity_t g_topic[MAX_DOMAINS];
static uint32_t g_topic_nr = 0;

static void access_control_init(
  const char * id_certs[], const char * id_keys[], const char * id_ca[], bool exp_pp_fail[], size_t n_nodes,
  bool incl_gov, const char * gov,
  bool incl_perm, const char * perm,
  bool incl_ca, const char * ca)
{
  CU_ASSERT_FATAL (n_nodes <= MAX_DOMAINS);
  for (size_t i = 0; i < n_nodes; i++)
  {
    struct kvp config_vars[] = {
      { "TEST_IDENTITY_CERTIFICATE", id_certs[i], 1 },
      { "TEST_IDENTITY_PRIVATE_KEY", id_keys[i], 1 },
      { "TEST_IDENTITY_CA_CERTIFICATE", id_ca[i], 1 },
      { "INCL_GOV", incl_gov ? "1" : "", 2 },
      { "INCL_PERM", incl_perm ? "1" : "", 2 },
      { "INCL_PERM_CA", incl_ca ? "1" : "", 2 },
      { "TEST_GOVERNANCE", gov, 1 },
      { "TEST_PERMISSIONS", perm, 1 },
      { "TEST_PERMISSIONS_CA", ca, 1 },
      { NULL, NULL, 0 }
    };
    char *conf = ddsrt_expand_vars_sh (config, &expand_lookup_vars_env, config_vars);
    CU_ASSERT_EQUAL_FATAL (expand_lookup_unmatched (config_vars), 0);
    g_domain[i] = dds_create_domain (DDS_DOMAINID + (dds_domainid_t)i, conf);
    dds_free (conf);
    g_participant[i] = dds_create_participant (DDS_DOMAINID + (dds_domainid_t)i, NULL, NULL);
    CU_ASSERT_EQUAL_FATAL (exp_pp_fail[i], g_participant[i] <= 0);
  }
}

static void access_control_fini(size_t n)
{
  for (size_t i = 0; i < n; i++)
    CU_ASSERT_EQUAL_FATAL (dds_delete (g_domain[i]), DDS_RETCODE_OK);
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
#define CA_D PF_D TEST_PERMISSIONS_CA_CERTIFICATE

CU_TheoryDataPoints(ddssec_access_control, config_parameters_file) = {
    CU_DataPoints(const char *,
    /*                         */"existing files",
    /*                          |      */"non-existing files",
    /*                          |       |        */"non-existing governance file",
    /*                          |       |         |       */"non-existing permissions file",
    /*                          |       |         |        |        */"non-existing permissions ca file",
    /*                          |       |         |        |         |      */"empty governance",
    /*                          |       |         |        |         |       |      */"empty permissions",
    /*                          |       |         |        |         |       |       |     */"empty permissions ca",
    /*                          |       |         |        |         |       |       |      |      */"all empty",
    /*                          |       |         |        |         |       |       |      |       |     */"invalid governance uri type",
    /*                          |       |         |        |         |       |       |      |       |      |      */"permissions ca type data",
    /*                          |       |         |        |         |       |       |      |       |      |       |      */"no governance element",
    /*                          |       |         |        |         |       |       |      |       |      |       |       |      */"no permissions element",
    /*                          |       |         |        |         |       |       |      |       |      |       |       |       |     */"no permissions ca element"),
    CU_DataPoints(const char *, GOV_F,  GOV_FNE,  GOV_FNE, GOV_F,    GOV_F,  "",     GOV_F, GOV_F,  "",    GOV_DI, GOV_F,  "",     GOV_F, GOV_F),   // Governance config
    CU_DataPoints(const char *, PERM_F, PERM_FNE, PERM_F,  PERM_FNE, PERM_F, PERM_F, "",    PERM_F, "",    PERM_F, PERM_F, PERM_F, "",    PERM_F),  // Permissions config
    CU_DataPoints(const char *, CA_F,   CA_FNE,   CA_F,    CA_F,     CA_FNE, CA_F,   CA_F,  "",     "",    CA_F,   CA_D,   CA_F,   CA_F,  ""),      // Permissions CA
    CU_DataPoints(bool,         true,   true,     true,    true,     true,   true,   true,  true,   true,  true,   true,   false,  false, false),   // include empty config elements
    CU_DataPoints(bool,         false,  true,     true,    true,     true,   true,   true,  true,   false, true,   false,  true,   true,  true)     // expect failure
};
CU_Theory((const char * test_descr, const char * gov, const char * perm, const char * ca, bool incl_empty_els, bool exp_fail),
    ddssec_access_control, config_parameters_file)
{
  print_test_msg ("running test config_parameters_file: %s\n", test_descr);
  access_control_init (
      (const char *[]) { TEST_IDENTITY1_CERTIFICATE, TEST_IDENTITY1_CERTIFICATE },
      (const char *[]) { TEST_IDENTITY1_PRIVATE_KEY, TEST_IDENTITY1_PRIVATE_KEY },
      (const char *[]) { TEST_IDENTITY_CA1_CERTIFICATE, TEST_IDENTITY_CA1_CERTIFICATE },
      (bool []) { exp_fail, exp_fail }, 2,
      incl_empty_els || strlen (gov), gov,
      incl_empty_els || strlen (perm), perm,
      incl_empty_els || strlen (ca), ca);
  access_control_fini (2);
}

#define S(n) (n)
#define M(n) (S(n)*60)
#define H(n) (M(n)*60)
#define D(n) (H(n)*24)
CU_TheoryDataPoints(ddssec_access_control, permissions_expiry) = {
    CU_DataPoints(const char *,
    /*                      */"valid 1 minute from now",
    /*                       |     */"valid -1 minute until now",
    /*                       |      |     */"1s valid, create pp after 1100ms",
    /*                       |      |      |      */"node 2 permissions expired",
    /*                       |      |      |      |     */"node 1 3s valid, write/read for 10s",
    /*                       |      |      |      |      |     */"node 2 3s valid, write/read for 10s"),
    CU_DataPoints(int32_t,   0,     -M(1), 0,     0,     0,     0),     /* node 1 permissions not before (offset from local time) */
    CU_DataPoints(int32_t,   M(1),  0,     S(1),  D(1),  S(4),  D(1)),  /* node 1 permissions not after (offset from local time) */
    CU_DataPoints(int32_t,   0,     -M(1), 0,     -D(1), 0,     0),     /* node 2 permissions not before (offset from local time) */
    CU_DataPoints(int32_t,   M(1),  0,     S(1),  0,     D(1),  S(4)),  /* node 2 permissions not after (offset from local time) */
    CU_DataPoints(uint32_t,  0,     0,     1100,  0,     0,     0),     /* delay (ms) after generating permissions */
    CU_DataPoints(bool,      false, true,  true,  false, false, false), /* expect pp 1 create failure */
    CU_DataPoints(bool,      false, true,  true,  true,  false, false), /* expect pp 2 create failure */
    CU_DataPoints(uint32_t,  1,     0,     0,     0,     10000, 10000),  /* write/read data during x ms */
    CU_DataPoints(bool,      false, false, false, false, true,  true),  /* expect read data failure */
};
CU_Theory(
  (const char * test_descr,
    int32_t perm1_not_before, int32_t perm1_not_after, int32_t perm2_not_before, int32_t perm2_not_after,
    uint32_t delay_perm, bool exp_pp1_fail, bool exp_pp2_fail, uint32_t write_read_dur, bool exp_read_fail),
  ddssec_access_control, permissions_expiry, .timeout=20)
{
  print_test_msg ("running test permissions_expiry: %s\n", test_descr);

  char topic_name[100];
  create_topic_name ("ddssec_access_control_", g_topic_nr++, topic_name, sizeof (topic_name));

  /* create ca and id1/id2 certs that will not expire during this test */
  char *ca, *id1, *id2, *id1_subj, *id2_subj;
  ca = generate_ca ("ca1", TEST_IDENTITY_CA1_PRIVATE_KEY, 0, D(1));
  id1 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id1", TEST_IDENTITY1_PRIVATE_KEY, 0, D(1), &id1_subj);
  id2 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id2", TEST_IDENTITY1_PRIVATE_KEY, 0, D(1), &id2_subj);

  /* localtime will be converted to gmtime in get_permissions_grant */
  dds_time_t now = dds_time ();
  char * perm_topic = get_permissions_topic (topic_name);
  char * grants[] = {
    get_permissions_grant ("id1", id1_subj, now + DDS_SECS(perm1_not_before), now + DDS_SECS(perm1_not_after), perm_topic, perm_topic, NULL),
    get_permissions_grant ("id2", id2_subj, now + DDS_SECS(perm2_not_before), now + DDS_SECS(perm2_not_after), perm_topic, perm_topic, NULL) };
  char * perm_config = get_permissions_config (grants, 2, true);
  dds_sleepfor (DDS_MSECS (delay_perm));

  access_control_init (
      (const char *[]) { id1, id2 },
      (const char *[]) { TEST_IDENTITY1_PRIVATE_KEY, TEST_IDENTITY1_PRIVATE_KEY },
      (const char *[]) { ca, ca },
      (bool []) { exp_pp1_fail, exp_pp2_fail }, 2,
      true, PF_F COMMON_ETC_PATH("default_governance.p7s"),
      true, perm_config,
      true, PF_F COMMON_ETC_PATH("default_permissions_ca.pem"));

  if (write_read_dur > 0)
  {
    dds_entity_t wr = 0, rd = 0;
    rd_wr_init (g_participant[0], &g_pubsub[0], &g_topic[0], &wr, g_participant[1], &g_pubsub[1], &g_topic[1], &rd, topic_name);
    write_read_for (wr, g_participant[1], rd, DDS_MSECS (write_read_dur), false, exp_read_fail);
  }

  access_control_fini (2);

  ddsrt_free (perm_topic);
  ddsrt_free (grants[0]);
  ddsrt_free (grants[1]);
  ddsrt_free (perm_config);
  ddsrt_free (ca);
  ddsrt_free (id1_subj);
  ddsrt_free (id2_subj);
  ddsrt_free (id1);
  ddsrt_free (id2);
}
#undef D
#undef H
#undef M


#define N_RD 1 // N_RD > 1 not yet implemented
#define N_WR 3
#define N_NODES (N_RD + N_WR)
#define PERM_EXP_BASE 3
CU_Test(ddssec_access_control, permissions_expiry_multiple, .timeout=20)
{
  char topic_name[100];
  create_topic_name ("ddssec_access_control_", g_topic_nr++, topic_name, sizeof (topic_name));

  dds_time_t t_perm = dds_time ();
  char *ca = generate_ca ("ca1", TEST_IDENTITY_CA1_PRIVATE_KEY, 0, 3600);
  char *perm_topic = get_permissions_topic (topic_name);

  // 1st node used as reader, other nodes as writer
  print_test_msg ("creating permissions grants\n");
  const char *id[N_NODES], *pk[N_NODES], *ca_list[N_NODES];
  char * id_subj[N_NODES], *grants[N_NODES];
  bool exp_fail[N_NODES];
  for (int i = 0; i < N_NODES; i++)
  {
    char *id_name;
    ddsrt_asprintf (&id_name, "id_%d", i);
    pk[i] = TEST_IDENTITY1_PRIVATE_KEY;
    ca_list[i] = ca;
    id[i] = generate_identity (ca_list[i], TEST_IDENTITY_CA1_PRIVATE_KEY, id_name, pk[i], 0, 3600, &id_subj[i]);
    exp_fail[i] = false;
    dds_duration_t v = DDS_SECS(i < N_RD ? 3600 : PERM_EXP_BASE + 2 * i); /* readers should not expire */
    dds_time_t t_exp = ddsrt_time_add_duration (t_perm, v);
    if (i >= N_RD)
      print_test_msg ("w[%d] grant expires at %d.%06d\n", i - N_RD, (int32_t) (t_exp / DDS_NSECS_IN_SEC), (int32_t) (t_exp % DDS_NSECS_IN_SEC) / 1000);
    grants[i] = get_permissions_grant (id_name, id_subj[i], t_perm, t_exp, perm_topic, perm_topic, NULL);
    ddsrt_free (id_name);
  }

  char * perm_config = get_permissions_config (grants, N_NODES, true);
  access_control_init (
      id, pk, ca_list, exp_fail, N_NODES,
      true, PF_F COMMON_ETC_PATH ("default_governance.p7s"),
      true, perm_config,
      true, PF_F COMMON_ETC_PATH ("default_permissions_ca.pem"));

  dds_qos_t * qos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, -1);
  dds_qset_durability (qos, DDS_DURABILITY_TRANSIENT_LOCAL);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);

  dds_entity_t rd[N_RD];
  for (int i = 0; i < N_RD; i++)
  {
    dds_entity_t sub = dds_create_subscriber (g_participant[i], NULL, NULL);
    CU_ASSERT_FATAL (sub > 0);
    dds_entity_t sub_tp = dds_create_topic (g_participant[i], &SecurityCoreTests_Type1_desc, topic_name, NULL, NULL);
    CU_ASSERT_FATAL (sub_tp > 0);
    rd[i] = dds_create_reader (sub, sub_tp, qos, NULL);
    CU_ASSERT_FATAL (rd[i] > 0);
    dds_set_status_mask (rd[i], DDS_DATA_AVAILABLE_STATUS);
  }

  dds_entity_t wr[N_WR];
  for (int i = 0; i < N_WR; i++)
  {
    dds_entity_t pub = dds_create_publisher (g_participant[i + N_RD], NULL, NULL);
    CU_ASSERT_FATAL (pub > 0);
    dds_entity_t pub_tp = dds_create_topic (g_participant[i + N_RD], &SecurityCoreTests_Type1_desc, topic_name, NULL, NULL);
    CU_ASSERT_FATAL (pub_tp > 0);
    wr[i] = dds_create_writer (pub, pub_tp, qos, NULL);
    CU_ASSERT_FATAL (wr[i] > 0);
    dds_set_status_mask (wr[i], DDS_PUBLICATION_MATCHED_STATUS);
    sync_writer_to_readers (g_participant[i + N_RD], wr[i], N_RD);
  }
  dds_delete_qos (qos);

  SecurityCoreTests_Type1 sample = { 1, 1 };
  SecurityCoreTests_Type1 rd_sample;
  void * samples[] = { &rd_sample };
  dds_sample_info_t info[1];
  dds_return_t ret;

  for (int run = 0; run < N_WR; run++)
  {
    // sleep until 1s after next writer pp permission expires
    dds_duration_t delay = DDS_SECS (PERM_EXP_BASE + 2 * run + 1) - (dds_time () - t_perm);
    if (delay > 0)
      dds_sleepfor (delay);

    print_test_msg ("run %d\n", run);

    for (int w = run; w < N_WR; w++)
    {
      sample.id = w;
      ret = dds_write (wr[w], &sample);
      CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
      print_test_msg ("write %d\n", w);
    }

    // Expect reader to receive data from writers with non-expired permissions
    int n_samples = 0, n_invalid = 0, n_wait = 0;
    while (n_samples + n_invalid < N_WR && n_wait < 5)
    {
      ret = dds_take (rd[0], samples, info, 1, 1);
      CU_ASSERT_FATAL (ret >= 0);
      if (ret == 0)
      {
        reader_wait_for_data (g_participant[0], rd[0], DDS_MSECS (200));
        print_test_msg ("wait for data\n");
        n_wait++;
      }
      else if (info[0].instance_state == DDS_IST_ALIVE)
      {
        print_test_msg ("recv sample %d\n", rd_sample.id);
        n_samples++;
      }
      else
      {
        print_test_msg ("recv inv sample\n");
        n_invalid++;
      }
    }
    CU_ASSERT_EQUAL (n_samples, N_WR - run);
    CU_ASSERT (n_invalid <= run);
  }

  access_control_fini (N_NODES);

  for (int i = 0; i < N_NODES; i++)
  {
    ddsrt_free (grants[i]);
    ddsrt_free (id_subj[i]);
    ddsrt_free ((char *)id[i]);
  }
  ddsrt_free (ca);
  ddsrt_free (perm_topic);
  ddsrt_free (perm_config);
}
#undef N_NODES
