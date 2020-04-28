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
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds__entity.h"

#include "dds/security/dds_security_api.h"

#include "common/config_env.h"
#include "common/access_control_wrapper.h"
#include "common/cryptography_wrapper.h"
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
    "      <Library finalizeFunction=\"finalize_test_authentication_wrapped\" initFunction=\"init_test_authentication_wrapped\" path=\"" WRAPPERLIB_PATH("dds_security_authentication_wrapper") "\"/>"
    "      <IdentityCertificate>data:,${TEST_IDENTITY_CERTIFICATE}</IdentityCertificate>"
    "      <PrivateKey>data:,${TEST_IDENTITY_PRIVATE_KEY}</PrivateKey>"
    "      <IdentityCA>data:,${TEST_IDENTITY_CA_CERTIFICATE}</IdentityCA>"
    "    </Authentication>"
    "    <AccessControl>"
    "      <Library initFunction=\"${ACCESS_CONTROL_INIT:-init_test_access_control_wrapped}\" finalizeFunction=\"${ACCESS_CONTROL_FINI:-finalize_test_access_control_wrapped}\" path=\"" WRAPPERLIB_PATH("dds_security_access_control_wrapper") "\"/>"
    "      ${INCL_GOV:+<Governance><![CDATA[}${TEST_GOVERNANCE}${INCL_GOV:+]]></Governance>}"
    "      ${INCL_PERM_CA:+<PermissionsCA>}${TEST_PERMISSIONS_CA}${INCL_PERM_CA:+</PermissionsCA>}"
    "      ${INCL_PERM:+<Permissions><![CDATA[}${TEST_PERMISSIONS}${INCL_PERM:+]]></Permissions>}"
    "    </AccessControl>"
    "    <Cryptographic>"
    "      <Library initFunction=\"${CRYPTO_INIT:-init_test_cryptography_wrapped}\" finalizeFunction=\"${CRYPTO_INIT:-finalize_test_cryptography_wrapped}\" path=\"" WRAPPERLIB_PATH("dds_security_cryptography_wrapper") "\"/>"
    "    </Cryptographic>"
    "  </DDSSecurity>"
    "</Domain>";

#define MAX_DOMAINS 10
#define DDS_DOMAINID 0

#define PF_F "file:"
#define PF_D "data:,"

static dds_entity_t g_domain[MAX_DOMAINS];
static dds_entity_t g_participant[MAX_DOMAINS];
static uint32_t g_topic_nr = 0;

static void access_control_init(
  size_t n_nodes,
  const char * id_certs[], const char * id_keys[], const char * id_ca[], bool exp_pp_fail[],
  const char * ac_init_fns[], const char * ac_fini_fns[],
  bool incl_gov[], const char * gov[],
  bool incl_perm[], const char * perm[],
  bool incl_ca[], const char * ca[])
{
  CU_ASSERT_FATAL (n_nodes <= MAX_DOMAINS);
  for (size_t i = 0; i < n_nodes; i++)
  {
    struct kvp config_vars[] = {
      { "TEST_IDENTITY_CERTIFICATE", id_certs[i], 1 },
      { "TEST_IDENTITY_PRIVATE_KEY", id_keys[i], 1 },
      { "TEST_IDENTITY_CA_CERTIFICATE", id_ca[i], 1 },
      { "ACCESS_CONTROL_INIT", ac_init_fns ? ac_init_fns[i] : NULL, 1 },
      { "ACCESS_CONTROL_FINI", ac_fini_fns ? ac_fini_fns[i] : NULL, 1 },
      { "INCL_GOV", incl_gov[i] ? "1" : "", 2 },
      { "INCL_PERM", incl_perm[i] ? "1" : "", 2 },
      { "INCL_PERM_CA", incl_ca[i] ? "1" : "", 2 },
      { "TEST_GOVERNANCE", gov[i], 1 },
      { "TEST_PERMISSIONS", perm[i], 1 },
      { "TEST_PERMISSIONS_CA", ca[i], 1 },
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

static void access_control_fini(size_t n, void * res[], size_t nres)
{
  for (size_t i = 0; i < n; i++)
    CU_ASSERT_EQUAL_FATAL (dds_delete (g_domain[i]), DDS_RETCODE_OK);
  if (res != NULL)
  {
    for (size_t i = 0; i < nres; i++)
      ddsrt_free (res[i]);
  }
}

static DDS_Security_DatawriterCryptoHandle get_builtin_writer_crypto_handle(dds_entity_t participant, unsigned entityid)
{
  DDS_Security_DatawriterCryptoHandle crypto_handle;
  struct dds_entity *pp_entity;
  struct participant *pp;
  struct writer *wr;
  CU_ASSERT_EQUAL_FATAL(dds_entity_pin(participant, &pp_entity), 0);
  thread_state_awake(lookup_thread_state(), &pp_entity->m_domain->gv);
  pp = entidx_lookup_participant_guid(pp_entity->m_domain->gv.entity_index, &pp_entity->m_guid);
  wr = get_builtin_writer(pp, entityid);
  CU_ASSERT_FATAL(wr != NULL);
  assert(wr != NULL); /* for Clang's static analyzer */
  crypto_handle = wr->sec_attr->crypto_handle;
  thread_state_asleep(lookup_thread_state());
  dds_entity_unpin(pp_entity);
  return crypto_handle;
}

// static DDS_Security_DatawriterCryptoHandle get_writer_crypto_handle(dds_entity_t writer)
// {
//   DDS_Security_DatawriterCryptoHandle crypto_handle;
//   struct dds_entity *wr_entity;
//   struct writer *wr;
//   CU_ASSERT_EQUAL_FATAL(dds_entity_pin(writer, &wr_entity), 0);
//   thread_state_awake(lookup_thread_state(), &wr_entity->m_domain->gv);
//   wr = entidx_lookup_writer_guid(wr_entity->m_domain->gv.entity_index, &wr_entity->m_guid);
//   CU_ASSERT_FATAL(wr != NULL);
//   assert(wr != NULL); /* for Clang's static analyzer */
//   crypto_handle = wr->sec_attr->crypto_handle;
//   thread_state_asleep(lookup_thread_state());
//   dds_entity_unpin(wr_entity);
//   return crypto_handle;
// }


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
  bool has_gov = incl_empty_els || strlen (gov);
  bool has_perm = incl_empty_els || strlen (perm);
  bool has_ca = incl_empty_els || strlen (ca);
  access_control_init (
      2,
      (const char *[]) { TEST_IDENTITY1_CERTIFICATE, TEST_IDENTITY1_CERTIFICATE },
      (const char *[]) { TEST_IDENTITY1_PRIVATE_KEY, TEST_IDENTITY1_PRIVATE_KEY },
      (const char *[]) { TEST_IDENTITY_CA1_CERTIFICATE, TEST_IDENTITY_CA1_CERTIFICATE },
      (bool []) { exp_fail, exp_fail }, NULL, NULL,
      (bool []) { has_gov, has_gov }, (const char *[]) { gov, gov },
      (bool []) { has_perm, has_perm }, (const char *[]) { perm, perm },
      (bool []) { has_ca, has_ca }, (const char *[]) { ca, ca });
  access_control_fini (2, NULL, 0);
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
    /*                       |      |      |      |     */"node 1 4s valid, write/read for 10s",
    /*                       |      |      |      |      |     */"node 2 4s valid, write/read for 10s"),
    CU_DataPoints(int32_t,   0,     -M(1), 0,     0,     0,     0),     /* node 1 permissions not before (offset from local time) */
    CU_DataPoints(int32_t,   M(1),  0,     S(1),  D(1),  S(4),  D(1)),  /* node 1 permissions not after (offset from local time) */
    CU_DataPoints(int32_t,   0,     -M(1), 0,     -D(1), 0,     0),     /* node 2 permissions not before (offset from local time) */
    CU_DataPoints(int32_t,   M(1),  0,     S(1),  0,     D(1),  S(4)),  /* node 2 permissions not after (offset from local time) */
    CU_DataPoints(uint32_t,  0,     0,     1100,  0,     0,     0),     /* delay (ms) after generating permissions */
    CU_DataPoints(bool,      false, true,  true,  false, false, false), /* expect pp 1 create failure */
    CU_DataPoints(bool,      false, true,  true,  true,  false, false), /* expect pp 2 create failure */
    CU_DataPoints(uint32_t,  1,     0,     0,     0,     10000, 10000), /* write/read data during x ms */
    CU_DataPoints(bool,      false, false, false, false, true,  true),  /* expect read data failure */
};
#undef S
#undef D
#undef H
#undef M
CU_Theory(
  (const char * test_descr,
    int32_t perm1_not_before, int32_t perm1_not_after, int32_t perm2_not_before, int32_t perm2_not_after,
    uint32_t delay_perm, bool exp_pp1_fail, bool exp_pp2_fail, uint32_t write_read_dur, bool exp_read_fail),
  ddssec_access_control, permissions_expiry, .timeout=30)
{
  print_test_msg ("running test permissions_expiry: %s\n", test_descr);

  char topic_name[100];
  create_topic_name ("ddssec_access_control_", g_topic_nr++, topic_name, sizeof (topic_name));

  /* create ca and id1/id2 certs that will not expire during this test */
  char *ca, *id1, *id2, *id1_subj, *id2_subj;
  ca = generate_ca ("ca1", TEST_IDENTITY_CA1_PRIVATE_KEY, 0, 3600);
  id1 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id1", TEST_IDENTITY1_PRIVATE_KEY, 0, 3600, &id1_subj);
  id2 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id2", TEST_IDENTITY1_PRIVATE_KEY, 0, 3600, &id2_subj);

  /* localtime will be converted to gmtime in get_permissions_grant */
  dds_time_t now = dds_time ();
  char * perm_topic = get_permissions_topic (topic_name);
  char * grants[] = {
    get_permissions_grant ("id1", id1_subj, NULL, now + DDS_SECS(perm1_not_before), now + DDS_SECS(perm1_not_after), perm_topic, perm_topic, NULL),
    get_permissions_grant ("id2", id2_subj, NULL, now + DDS_SECS(perm2_not_before), now + DDS_SECS(perm2_not_after), perm_topic, perm_topic, NULL) };
  char * perm_config = get_permissions_config (grants, 2, true);
  dds_sleepfor (DDS_MSECS (delay_perm));

  const char * def_gov = PF_F COMMON_ETC_PATH("default_governance.p7s");
  const char * def_perm_ca = PF_F COMMON_ETC_PATH("default_permissions_ca.pem");
  access_control_init (
      2,
      (const char *[]) { id1, id2 },
      (const char *[]) { TEST_IDENTITY1_PRIVATE_KEY, TEST_IDENTITY1_PRIVATE_KEY },
      (const char *[]) { ca, ca },
      (bool []) { exp_pp1_fail, exp_pp2_fail }, NULL, NULL,
      (bool []) { true, true }, (const char *[]) { def_gov, def_gov },
      (bool []) { true, true }, (const char *[]) { perm_config, perm_config },
      (bool []) { true, true }, (const char *[]) { def_perm_ca, def_perm_ca });

  if (write_read_dur > 0)
  {
    dds_entity_t wr = 0, rd = 0;
    dds_entity_t pub, sub;
    dds_entity_t topic0, topic1;
    rd_wr_init (g_participant[0], &pub, &topic0, &wr, g_participant[1], &sub, &topic1, &rd, topic_name);
    sync_writer_to_readers(g_participant[0], wr, 1, DDS_SECS(2));
    write_read_for (wr, g_participant[1], rd, DDS_MSECS (write_read_dur), false, exp_read_fail);
  }

  access_control_fini (2, (void * []) { perm_topic, grants[0], grants[1], perm_config, ca, id1_subj, id2_subj, id1, id2 }, 9);
}


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
  const char *id[N_NODES], *pk[N_NODES], *ca_list[N_NODES], *gov[N_NODES], *perm_conf[N_NODES], *perm_ca[N_NODES];
  char * id_subj[N_NODES], *grants[N_NODES];
  bool exp_fail[N_NODES], incl_el[N_NODES];

  for (int i = 0; i < N_NODES; i++)
  {
    char *id_name;
    ddsrt_asprintf (&id_name, "id_%d", i);
    pk[i] = TEST_IDENTITY1_PRIVATE_KEY;
    ca_list[i] = ca;
    id[i] = generate_identity (ca_list[i], TEST_IDENTITY_CA1_PRIVATE_KEY, id_name, pk[i], 0, 3600, &id_subj[i]);
    exp_fail[i] = false;
    gov[i] = PF_F COMMON_ETC_PATH ("default_governance.p7s");
    perm_ca[i] = PF_F COMMON_ETC_PATH ("default_permissions_ca.pem");
    incl_el[i] = true;
    dds_duration_t v = DDS_SECS(i < N_RD ? 3600 : PERM_EXP_BASE + 2 * i); /* readers should not expire */
    dds_time_t t_exp = ddsrt_time_add_duration (t_perm, v);
    if (i >= N_RD)
      print_test_msg ("w[%d] grant expires at %d.%06d\n", i - N_RD, (int32_t) (t_exp / DDS_NSECS_IN_SEC), (int32_t) (t_exp % DDS_NSECS_IN_SEC) / 1000);
    grants[i] = get_permissions_grant (id_name, id_subj[i], NULL, t_perm, t_exp, perm_topic, perm_topic, NULL);
    ddsrt_free (id_name);
  }

  char * perm_config_str = get_permissions_config (grants, N_NODES, true);
  for (int i = 0; i < N_NODES; i++)
    perm_conf[i] = perm_config_str;

  access_control_init (
      N_NODES,
      id, pk, ca_list, exp_fail, NULL, NULL,
      incl_el, gov, incl_el, perm_conf, incl_el, perm_ca);

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
    sync_writer_to_readers (g_participant[i + N_RD], wr[i], N_RD, DDS_SECS(2));
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

  access_control_fini (N_NODES, NULL, 0);

  for (int i = 0; i < N_NODES; i++)
  {
    ddsrt_free (grants[i]);
    ddsrt_free (id_subj[i]);
    ddsrt_free ((char *)id[i]);
  }
  ddsrt_free (ca);
  ddsrt_free (perm_topic);
  ddsrt_free (perm_config_str);
}
#undef N_RD
#undef N_WR
#undef N_NODES
#undef PERM_EXP_BASE

#define na false
CU_TheoryDataPoints(ddssec_access_control, hooks) = {
    CU_DataPoints(const char *,
    /*                 */"init_test_access_control_local_participant_not_allowed",
    /*                  |     */"init_test_access_control_local_topic_not_allowed",
    /*                  |      |     */"init_test_access_control_local_publishing_not_allowed",
    /*                  |      |      |      */"init_test_access_control_local_subscribing_not_allowed",
    /*                  |      |      |      |     */"init_test_access_control_remote_permissions_invalidate",
    /*                  |      |      |      |      |     */"init_test_access_control_remote_participant_not_allowed",
    /*                  |      |      |      |      |      |     */"init_test_access_control_remote_topic_not_allowed",
    /*                  |      |      |      |      |      |      |     */"init_test_access_control_remote_writer_not_allowed",
    /*                  |      |      |      |      |      |      |      |     */"init_test_access_control_remote_reader_not_allowed",
    /*                  |      |      |      |      |      |      |      |      |     */"init_test_access_control_remote_reader_relay_only"),
    CU_DataPoints(bool, true,  false, false, false, false, false, false, false, false, false),  // exp_pp_fail
    CU_DataPoints(bool, na,    true,  false, false, false, false, false, false, false, false),  // exp_local_topic_fail
    CU_DataPoints(bool, na,    false, false, false, false, false, false, false, false, false),  // exp_remote_topic_fail
    CU_DataPoints(bool, na,    na,    true,  false, false, false, false, false, false, false),  // exp_wr_fail
    CU_DataPoints(bool, na,    na,    false, true,  false, false, false, false, false, false),  // exp_rd_fail
    CU_DataPoints(bool, na,    na,    na,    na,    true,  true,  true,  false, true,  true),   // exp_wr_rd_sync_fail
    CU_DataPoints(bool, na,    na,    false, na,    true,  true,  true,  true,  false, false),  // exp_rd_wr_sync_fail
};
#undef na
CU_Theory(
  (const char * init_fn, bool exp_pp_fail, bool exp_local_topic_fail, bool exp_remote_topic_fail, bool exp_wr_fail, bool exp_rd_fail, bool exp_wr_rd_sync_fail, bool exp_rd_wr_sync_fail),
  ddssec_access_control, hooks, .timeout=40)
{
  print_test_msg ("running test access_control_hooks: %s\n", init_fn);

  const char * def_gov = PF_F COMMON_ETC_PATH("default_governance.p7s");
  const char * def_perm = PF_F COMMON_ETC_PATH("default_permissions.p7s");
  const char * def_perm_ca = PF_F COMMON_ETC_PATH("default_permissions_ca.pem");

  access_control_init (
      2,
      (const char *[]) { TEST_IDENTITY1_CERTIFICATE, TEST_IDENTITY1_CERTIFICATE },
      (const char *[]) { TEST_IDENTITY1_PRIVATE_KEY, TEST_IDENTITY1_PRIVATE_KEY },
      (const char *[]) { TEST_IDENTITY_CA1_CERTIFICATE, TEST_IDENTITY_CA1_CERTIFICATE },
      (bool []) { exp_pp_fail, false },
      (const char *[]) { init_fn, "init_test_access_control_wrapped" }, (const char *[]) { "finalize_test_access_control_not_allowed", "finalize_test_access_control_wrapped" },
      (bool []) { true, true, true }, (const char *[]) { def_gov, def_gov },
      (bool []) { true, true, true }, (const char *[]) { def_perm, def_perm },
      (bool []) { true, true, true }, (const char *[]) { def_perm_ca, def_perm_ca });

  if (!exp_pp_fail)
  {
    dds_entity_t lwr = 0, rwr = 0, lrd = 0, rrd = 0;
    dds_entity_t ltopic[2], rtopic[2];
    dds_entity_t lpub, lsub, rpub, rsub;
    char topic_name[100];

    // Local writer, remote reader
    create_topic_name (AC_WRAPPER_TOPIC_PREFIX, g_topic_nr++, topic_name, sizeof (topic_name));
    rd_wr_init_fail (
      g_participant[0], &lpub, &ltopic[0], &lwr,
      g_participant[1], &rsub, &rtopic[0], &rrd,
      topic_name, exp_local_topic_fail, exp_wr_fail, exp_remote_topic_fail, false);
    if (!exp_local_topic_fail && !exp_remote_topic_fail && !exp_wr_fail)
      sync_writer_to_readers (g_participant[0], lwr, exp_wr_rd_sync_fail ? 0 : 1, DDS_SECS(2));

    // Local reader, remote writer
    create_topic_name (AC_WRAPPER_TOPIC_PREFIX, g_topic_nr++, topic_name, sizeof (topic_name));
    rd_wr_init_fail (
      g_participant[1], &rpub, &rtopic[1], &rwr,
      g_participant[0], &lsub, &ltopic[1], &lrd,
      topic_name, exp_remote_topic_fail, false, exp_local_topic_fail, exp_rd_fail);
    if (!exp_local_topic_fail && !exp_remote_topic_fail && !exp_rd_fail)
      sync_reader_to_writers (g_participant[0], lrd, exp_rd_wr_sync_fail ? 0 : 1, DDS_SECS(2));
  }

  access_control_fini (2, NULL, 0);
}

#define na false
CU_TheoryDataPoints(ddssec_access_control, join_access_control) = {
    CU_DataPoints(const char *,
    /*                 */"no join access control",
    /*                  |     */"join access control pp1, valid",
    /*                  |      |     */"join access control pp1 and pp2, valid",
    /*                  |      |      |      */"join access control pp1, invalid",
    /*                  |      |      |      |     */"join access control pp1 and pp2, invalid"),
    CU_DataPoints(bool, false, true,  true,  true,  true), /* join access control pp 1 enabled */
    CU_DataPoints(bool, false, false, true,  false, true), /* join access control pp 2 enabled */
    CU_DataPoints(bool, false, false, false, true,  true), /* permissions pp 1 invalid */
    CU_DataPoints(bool, false, false, false, false, true), /* permissions pp 2 invalid */
    CU_DataPoints(bool, false, false, false, true,  true), /* expect pp 1 create failure */
    CU_DataPoints(bool, false, false, false, false, true), /* expect pp 2 create failure */
    CU_DataPoints(bool, false, false, false, na,    na),   /* expect handshake failure */
};
#undef na
CU_Theory(
  (const char * test_descr, bool join_ac_pp1, bool join_ac_pp2, bool perm_inv_pp1, bool perm_inv_pp2, bool exp_pp1_fail, bool exp_pp2_fail, bool exp_hs_fail),
  ddssec_access_control, join_access_control, .timeout=30)
{
  print_test_msg ("running test join_access_control: %s\n", test_descr);

  char topic_name[100];
  create_topic_name ("ddssec_access_control_", g_topic_nr++, topic_name, sizeof (topic_name));

  /* create ca and id1/id2 certs that will not expire during this test */
  char *ca, *id1, *id2, *id1_subj, *id2_subj;
  ca = generate_ca ("ca1", TEST_IDENTITY_CA1_PRIVATE_KEY, 0, 3600);
  id1 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id1", TEST_IDENTITY1_PRIVATE_KEY, 0, 3600, &id1_subj);
  id2 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id2", TEST_IDENTITY1_PRIVATE_KEY, 0, 3600, &id2_subj);

  /* localtime will be converted to gmtime in get_permissions_grant */
  dds_time_t now = dds_time ();
  char * perm_topic = get_permissions_topic (topic_name);
  char * grants[] = {
    get_permissions_grant ("id1", id1_subj, perm_inv_pp1 ? "99" : NULL, now, now + DDS_SECS(3600), perm_topic, perm_topic, NULL),
    get_permissions_grant ("id2", id2_subj, perm_inv_pp2 ? "99" : NULL, now, now + DDS_SECS(3600), perm_topic, perm_topic, NULL) };
  char * perm_config = get_permissions_config (grants, 2, true);

  char * gov_topic_rule = get_governance_topic_rule ("*", false, false, true, true, "NONE", "NONE");
  char * gov_config_pp1 = get_governance_config (false, join_ac_pp1, NULL, NULL, NULL, gov_topic_rule, true);
  char * gov_config_pp2 = get_governance_config (false, join_ac_pp2, NULL, NULL, NULL, gov_topic_rule, true);
  const char * def_perm_ca = PF_F COMMON_ETC_PATH("default_permissions_ca.pem");

  access_control_init (
      2,
      (const char *[]) { id1, id2 },
      (const char *[]) { TEST_IDENTITY1_PRIVATE_KEY, TEST_IDENTITY1_PRIVATE_KEY },
      (const char *[]) { ca, ca },
      (bool []) { exp_pp1_fail, exp_pp2_fail }, NULL, NULL,
      (bool []) { true, true }, (const char *[]) { gov_config_pp1, gov_config_pp2 },
      (bool []) { true, true }, (const char *[]) { perm_config, perm_config },
      (bool []) { true, true }, (const char *[]) { def_perm_ca, def_perm_ca });

  if (!exp_pp1_fail && !exp_pp2_fail)
    validate_handshake (DDS_DOMAINID, exp_hs_fail, NULL, NULL, NULL, DDS_SECS(2));

  access_control_fini (2, (void * []) { gov_config_pp1, gov_config_pp2, gov_topic_rule, perm_topic, grants[0], grants[1], perm_config, ca, id1_subj, id2_subj, id1, id2 }, 12);
}

#define na false
CU_TheoryDataPoints(ddssec_access_control, discovery_protection) = {
    CU_DataPoints(const char *,
    /*                                        */"disabled",
    /*                                         |     */"enabled, protection kind none",
    /*                                         |      |     */"disabled, protection kind encrypt",
    /*                                         |      |      |     */"enabled, protection kind encrypt",
    /*                                         |      |      |      |     */"enabled, protection kind sign",
    /*                                         |      |      |      |      |     */"enabled, protection kind encrypt-with-origin_auth",
    /*                                         |      |      |      |      |      |      */"enabled for node 1, disabled for node 2",
    /*                                         |      |      |      |      |      |       |     */"node 1 and node 2 different protection kinds"),
    CU_DataPoints(bool,                        false, true,  false, true,  true,  true,   true,  true),   /* enable_discovery_protection for pp 1 */
    CU_DataPoints(bool,                        false, true,  false, true,  true,  true,   false, true),   /* enable_discovery_protection for pp 2 */
    CU_DataPoints(DDS_Security_ProtectionKind, PK_N,  PK_N,  PK_E,  PK_E,  PK_S,  PK_EOA, PK_E,  PK_E),   /* discovery_protection_kind pp 1 */
    CU_DataPoints(DDS_Security_ProtectionKind, PK_N,  PK_N,  PK_E,  PK_E,  PK_S,  PK_EOA, PK_N,  PK_S),   /* discovery_protection_kind pp 2 */
    CU_DataPoints(bool,                        false, false, false, false, false, false,  true,  true),   /* expect rd-wr match fail */
    CU_DataPoints(bool,                        false, false, true,  true,  true,  true,   true,  true),   /* expect SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER of pp 1 to have a crypto handle */
    CU_DataPoints(bool,                        na,    na,    true,  true,  true,  true,   false, false),  /* expect encode_datawriter_submessage for SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER of pp 1 */
};
#undef na
CU_Theory(
  (const char * test_descr, bool enable_discovery_protection_pp1, bool enable_discovery_protection_pp2,
    DDS_Security_ProtectionKind discovery_protection_kind_pp1, DDS_Security_ProtectionKind discovery_protection_kind_pp2,
    bool exp_rd_wr_match_fail, bool exp_secure_pub_wr_handle, bool exp_secure_pub_wr_encode_decode),
  ddssec_access_control, discovery_protection, .timeout=30)
{
  print_test_msg ("running test discovery_protection: %s\n", test_descr);

  char topic_name[100];
  create_topic_name ("ddssec_access_control_", g_topic_nr++, topic_name, sizeof (topic_name));

  /* create ca and id1/id2 certs that will not expire during this test */
  char *ca, *id1, *id2, *id1_subj, *id2_subj;
  ca = generate_ca ("ca1", TEST_IDENTITY_CA1_PRIVATE_KEY, 0, 3600);
  id1 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id1", TEST_IDENTITY1_PRIVATE_KEY, 0, 3600, &id1_subj);
  id2 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id2", TEST_IDENTITY1_PRIVATE_KEY, 0, 3600, &id2_subj);

  /* localtime will be converted to gmtime in get_permissions_grant */
  dds_time_t now = dds_time ();
  char * perm_topic = get_permissions_topic (topic_name);
  char * grants[] = {
    get_permissions_grant ("id1", id1_subj, NULL, now, now + DDS_SECS(3600), perm_topic, perm_topic, NULL),
    get_permissions_grant ("id2", id2_subj, NULL, now, now + DDS_SECS(3600), perm_topic, perm_topic, NULL) };
  char * perm_config = get_permissions_config (grants, 2, true);

  char * gov_topic_rule1 = get_governance_topic_rule (topic_name, enable_discovery_protection_pp1, false, true, true, "ENCRYPT", "NONE");
  char * gov_topic_rule2 = get_governance_topic_rule (topic_name, enable_discovery_protection_pp2, false, true, true, "ENCRYPT", "NONE");
  char * gov_config1 = get_governance_config (false, true, pk_to_str (discovery_protection_kind_pp1), NULL, "ENCRYPT", gov_topic_rule1, true);
  char * gov_config2 = get_governance_config (false, true, pk_to_str (discovery_protection_kind_pp2), NULL, "ENCRYPT", gov_topic_rule2, true);
  const char * def_perm_ca = PF_F COMMON_ETC_PATH("default_permissions_ca.pem");

  access_control_init (
      2,
      (const char *[]) { id1, id2 },
      (const char *[]) { TEST_IDENTITY1_PRIVATE_KEY, TEST_IDENTITY1_PRIVATE_KEY },
      (const char *[]) { ca, ca },
      (bool []) { false, false }, NULL, NULL,
      (bool []) { true, true }, (const char *[]) { gov_config1, gov_config2 },
      (bool []) { true, true }, (const char *[]) { perm_config, perm_config },
      (bool []) { true, true }, (const char *[]) { def_perm_ca, def_perm_ca });
  validate_handshake (DDS_DOMAINID, false, NULL, NULL, NULL, DDS_SECS(2));

  dds_entity_t pub, sub, pub_tp, sub_tp, wr, rd;
  rd_wr_init (g_participant[0], &pub, &pub_tp, &wr, g_participant[1], &sub, &sub_tp, &rd, topic_name);
  sync_writer_to_readers (g_participant[0], wr, exp_rd_wr_match_fail ? 0 : 1, DDS_SECS(2));
  if (!exp_rd_wr_match_fail)
    write_read_for (wr, g_participant[1], rd, DDS_MSECS (100), false, false);

  DDS_Security_DatawriterCryptoHandle secure_pub_wr_handle = get_builtin_writer_crypto_handle (g_participant[0], NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER);
  print_test_msg ("crypto handle for SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER: %ld\n", secure_pub_wr_handle);
  CU_ASSERT_EQUAL_FATAL (exp_secure_pub_wr_handle, secure_pub_wr_handle != 0);

  struct dds_security_cryptography_impl * crypto_context_pub = get_crypto_context (g_participant[0]);
  CU_ASSERT_FATAL (crypto_context_pub != NULL);

  struct crypto_encode_decode_data *log = get_encode_decode_log (crypto_context_pub, ENCODE_DATAWRITER_SUBMESSAGE, secure_pub_wr_handle);
  CU_ASSERT_EQUAL_FATAL (exp_secure_pub_wr_handle && exp_secure_pub_wr_encode_decode, log != NULL);
  if (log != NULL)
  {
    print_test_msg ("encode_datawriter_submessage count for SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER: %u\n", log->count);
    CU_ASSERT_FATAL (log->count > 0);
    ddsrt_free (log);
  }

  access_control_fini (2, (void * []) { gov_config1, gov_config2, gov_topic_rule1, gov_topic_rule2, perm_topic, grants[0], grants[1], perm_config, ca, id1_subj, id2_subj, id1, id2 }, 13);
}

static void test_encoding_mismatch(
    bool exp_hs_fail, bool exp_rd_wr_fail,
    DDS_Security_ProtectionKind rtps_pk1, DDS_Security_ProtectionKind rtps_pk2,
    DDS_Security_ProtectionKind discovery_pk1, DDS_Security_ProtectionKind discovery_pk2,
    DDS_Security_ProtectionKind liveliness_pk1, DDS_Security_ProtectionKind liveliness_pk2,
    DDS_Security_ProtectionKind metadata_pk1, DDS_Security_ProtectionKind metadata_pk2,
    DDS_Security_BasicProtectionKind payload_pk1, DDS_Security_BasicProtectionKind payload_pk2)
{
  char topic_name[100];
  create_topic_name ("ddssec_access_control_", g_topic_nr++, topic_name, sizeof (topic_name));

  /* create ca and id1/id2 certs that will not expire during this test */
  char *ca, *id1, *id2, *id1_subj, *id2_subj;
  ca = generate_ca ("ca1", TEST_IDENTITY_CA1_PRIVATE_KEY, 0, 3600);
  id1 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id1", TEST_IDENTITY1_PRIVATE_KEY, 0, 3600, &id1_subj);
  id2 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id2", TEST_IDENTITY1_PRIVATE_KEY, 0, 3600, &id2_subj);

  /* localtime will be converted to gmtime in get_permissions_grant */
  dds_time_t now = dds_time ();
  char * perm_topic = get_permissions_topic (topic_name);
  char * grants[] = {
    get_permissions_grant ("id1", id1_subj, NULL, now, now + DDS_SECS(3600), perm_topic, perm_topic, NULL),
    get_permissions_grant ("id2", id2_subj, NULL, now, now + DDS_SECS(3600), perm_topic, perm_topic, NULL) };
  char * perm_config = get_permissions_config (grants, 2, true);

  char * gov_topic_rule1 = get_governance_topic_rule (topic_name, true, true, true, true, pk_to_str (metadata_pk1), bpk_to_str (payload_pk1));
  char * gov_topic_rule2 = get_governance_topic_rule (topic_name, true, true, true, true, pk_to_str (metadata_pk2), bpk_to_str (payload_pk2));
  char * gov_config1 = get_governance_config (false, true, pk_to_str (discovery_pk1), pk_to_str (liveliness_pk1), pk_to_str (rtps_pk1), gov_topic_rule1, true);
  char * gov_config2 = get_governance_config (false, true, pk_to_str (discovery_pk2), pk_to_str (liveliness_pk2), pk_to_str (rtps_pk2), gov_topic_rule2, true);
  const char * def_perm_ca = PF_F COMMON_ETC_PATH("default_permissions_ca.pem");

  access_control_init (
      2,
      (const char *[]) { id1, id2 },
      (const char *[]) { TEST_IDENTITY1_PRIVATE_KEY, TEST_IDENTITY1_PRIVATE_KEY },
      (const char *[]) { ca, ca },
      (bool []) { false, false }, NULL, NULL,
      (bool []) { true, true }, (const char *[]) { gov_config1, gov_config2 },
      (bool []) { true, true }, (const char *[]) { perm_config, perm_config },
      (bool []) { true, true }, (const char *[]) { def_perm_ca, def_perm_ca });

  struct Handshake *hs_list;
  int nhs;
  validate_handshake (DDS_DOMAINID, false, NULL, &hs_list, &nhs, DDS_MSECS(500));
  CU_ASSERT_EQUAL_FATAL (exp_hs_fail, nhs < 1);
  handshake_list_fini (hs_list, nhs);

  if (!exp_hs_fail)
  {
    dds_entity_t pub, sub, pub_tp, sub_tp, wr, rd;
    rd_wr_init (g_participant[0], &pub, &pub_tp, &wr, g_participant[1], &sub, &sub_tp, &rd, topic_name);
    sync_writer_to_readers (g_participant[0], wr, exp_rd_wr_fail ? 0 : 1, DDS_SECS(1));
  }

  access_control_fini (2, (void * []) { gov_config1, gov_config2, gov_topic_rule1, gov_topic_rule2, perm_topic, grants[0], grants[1], perm_config, ca, id1_subj, id2_subj, id1, id2 }, 13);
}

static DDS_Security_ProtectionKind pk[] = { PK_N, PK_S, PK_E, PK_SOA, PK_EOA };
static DDS_Security_BasicProtectionKind bpk[] = { BPK_N, BPK_S, BPK_E };

CU_Test(ddssec_access_control, encoding_mismatch_rtps, .timeout=30)
{
  for (size_t pk1 = 0; pk1 < sizeof (pk) / sizeof (pk[0]); pk1++)
    for (size_t pk2 = pk1 + 1; pk2 < sizeof (pk) / sizeof (pk[0]); pk2++)
      test_encoding_mismatch (pk1 != pk2, false, pk[pk1], pk[pk2], PK_N, PK_N, PK_N, PK_N, PK_N, PK_N, BPK_N, BPK_N);
}

CU_Test(ddssec_access_control, encoding_mismatch_discovery, .timeout=30)
{
  for (size_t pk1 = 0; pk1 < sizeof (pk) / sizeof (pk[0]); pk1++)
    for (size_t pk2 = pk1 + 1; pk2 < sizeof (pk) / sizeof (pk[0]); pk2++)
      test_encoding_mismatch (pk1 != pk2, false, PK_N, PK_N, pk[pk1], pk[pk2], PK_N, PK_N, PK_N, PK_N, BPK_N, BPK_N);
}

CU_Test(ddssec_access_control, encoding_mismatch_liveliness, .timeout=30)
{
  for (size_t pk1 = 0; pk1 < sizeof (pk) / sizeof (pk[0]); pk1++)
    for (size_t pk2 = pk1 + 1; pk2 < sizeof (pk) / sizeof (pk[0]); pk2++)
      test_encoding_mismatch (pk1 != pk2, false, PK_N, PK_N, PK_N, PK_N, pk[pk1], pk[pk2], PK_N, PK_N, BPK_N, BPK_N);
}

CU_Test(ddssec_access_control, encoding_mismatch_metadata, .timeout=30)
{
  for (size_t pk1 = 0; pk1 < sizeof (pk) / sizeof (pk[0]); pk1++)
    for (size_t pk2 = pk1 + 1; pk2 < sizeof (pk) / sizeof (pk[0]); pk2++)
      test_encoding_mismatch (false, pk1 != pk2, PK_N, PK_N, PK_N, PK_N, PK_N, PK_N, pk[pk1], pk[pk2], BPK_N, BPK_N);
}

CU_Test(ddssec_access_control, encoding_mismatch_payload, .timeout=30)
{
  for (size_t pk1 = 0; pk1 < sizeof (bpk) / sizeof (bpk[0]); pk1++)
    for (size_t pk2 = pk1 + 1; pk2 < sizeof (bpk) / sizeof (bpk[0]); pk2++)
      test_encoding_mismatch (false, pk1 != pk2, PK_N, PK_N, PK_N, PK_N, PK_N, PK_N, PK_N, PK_N, bpk[pk1], bpk[pk2]);
}
