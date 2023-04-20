// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

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
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsi/ddsi_participant.h"
#include "ddsi__misc.h"
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
#ifdef DDS_HAS_SHM
    "  <SharedMemory>"
    "    <Enable>false</Enable>"
    "  </SharedMemory>"
#endif
    "  <Security>"
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
    "  </Security>"
    "</Domain>";

#define MAX_DOMAINS 10
#define DDS_DOMAINID 0

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
    print_test_msg ("init domain %"PRIuSIZE"\n", i);
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
/* Testing configuration parameters for the access control security plugin,
   using configuration from file. The test cases include using non-existing
   files, empty configuration files, mixing configudation from file and inline
   in the cyclone XML configuration. */
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
/* Testing expiry of the (signed) permissions XML. Test cases include using
   permissions config that is valid for 1 minute, was valid in the past minute,
   expires before data is written, expires during writing data. */
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
  char * rules_xml = get_permissions_rules (NULL, topic_name, topic_name, NULL, NULL);
  char * grants[] = {
    get_permissions_grant ("id1", id1_subj, now + DDS_SECS(perm1_not_before), now + DDS_SECS(perm1_not_after), rules_xml, NULL),
    get_permissions_grant ("id2", id2_subj, now + DDS_SECS(perm2_not_before), now + DDS_SECS(perm2_not_after), rules_xml, NULL) };
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
    sync_writer_to_readers(g_participant[0], wr, 1, dds_time() + DDS_SECS(2));
    write_read_for (wr, g_participant[1], rd, DDS_MSECS (write_read_dur), false, exp_read_fail);
  }

  access_control_fini (2, (void * []) { rules_xml, grants[0], grants[1], perm_config, ca, id1_subj, id2_subj, id1, id2 }, 9);
}


static dds_time_t ceiling_sec (dds_time_t t)
{
  const dds_time_t s = DDS_SECS (1);
  const dds_time_t t1 = t + s-1;
  return t1 - (t1 % s);
}

#define N_WR 3
#define N_NODES (1 + N_WR)
#define PERM_EXP_BASE 3
#define PERM_EXP_INCR 2
/* Tests permissions configuration expiry using multiple writers, to validate
   that a reader keeps receiving data from writers that have valid permissions config */
CU_Test(ddssec_access_control, permissions_expiry_multiple, .timeout=20)
{
  char topic_name[100];
  create_topic_name ("ddssec_access_control_", g_topic_nr++, topic_name, sizeof (topic_name));

  const dds_time_t t_perm = dds_time ();
  dds_time_t t_exp_wr[N_WR];
  char *ca = generate_ca ("ca1", TEST_IDENTITY_CA1_PRIVATE_KEY, 0, 3600);
  char *rules_xml = get_permissions_rules (NULL, topic_name, topic_name, NULL, NULL);

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
    const dds_duration_t v = DDS_SECS(i == 0 ? 3600 : PERM_EXP_BASE + PERM_EXP_INCR * i); /* reader should not expire */
    assert ((v % DDS_SECS(1)) == 0);
    const dds_time_t t_exp = ddsrt_time_add_duration (ceiling_sec (t_perm), v);
    if (i >= 1)
    {
      print_test_msg ("w[%d] grant expires at %d.%06d\n", i - 1, (int32_t) (t_exp / DDS_NSECS_IN_SEC), (int32_t) (t_exp % DDS_NSECS_IN_SEC) / 1000);
      t_exp_wr[i - 1] = t_exp;
    }
    grants[i] = get_permissions_grant (id_name, id_subj[i], t_perm, t_exp, rules_xml, NULL);
    ddsrt_free (id_name);
  }

  char * perm_config_str = get_permissions_config (grants, N_NODES, true);
  for (int i = 0; i < N_NODES; i++)
    perm_conf[i] = perm_config_str;

  access_control_init (
      N_NODES,
      id, pk, ca_list, exp_fail, NULL, NULL,
      incl_el, gov, incl_el, perm_conf, incl_el, perm_ca);

  // create 1 reader
  dds_qos_t * rdqos = get_default_test_qos ();
  dds_entity_t sub = dds_create_subscriber (g_participant[0], NULL, NULL);
  CU_ASSERT_FATAL (sub > 0);
  dds_entity_t sub_tp = dds_create_topic (g_participant[0], &SecurityCoreTests_Type1_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (sub_tp > 0);
  dds_entity_t rd = dds_create_reader (sub, sub_tp, rdqos, NULL);
  CU_ASSERT_FATAL (rd > 0);
  dds_set_status_mask (rd, DDS_SUBSCRIPTION_MATCHED_STATUS);
  dds_delete_qos (rdqos);

  // create N_WR writers
  dds_qos_t * wrqos = get_default_test_qos ();
  dds_entity_t wr[N_WR];
  for (int i = 0; i < N_WR; i++)
  {
    dds_entity_t pub = dds_create_publisher (g_participant[i + 1], NULL, NULL);
    CU_ASSERT_FATAL (pub > 0);
    dds_entity_t pub_tp = dds_create_topic (g_participant[i + 1], &SecurityCoreTests_Type1_desc, topic_name, NULL, NULL);
    CU_ASSERT_FATAL (pub_tp > 0);
    wr[i] = dds_create_writer (pub, pub_tp, wrqos, NULL);
    CU_ASSERT_FATAL (wr[i] > 0);
    dds_set_status_mask (wr[i], DDS_PUBLICATION_MATCHED_STATUS);
  }
  dds_delete_qos (wrqos);

  // deadline matches planned start of first run
  const dds_time_t sync_abstimeout = t_exp_wr[0] - DDS_SECS(1);
  for (int i = 0; i < N_WR; i++)
    sync_writer_to_readers (g_participant[i + 1], wr[i], 1, sync_abstimeout);
  sync_reader_to_writers (g_participant[0], rd, N_WR, sync_abstimeout);

  // write data
  SecurityCoreTests_Type1 sample = { 1, 1 };
  dds_return_t ret;
  dds_entity_t ws = dds_create_waitset (g_participant[0]);
  dds_entity_t gcond = dds_create_guardcondition (g_participant[0]);
  dds_set_guardcondition (gcond, false);
  dds_waitset_attach (ws, gcond, 0);

  dds_set_status_mask (rd, DDS_DATA_AVAILABLE_STATUS);
  for (int run = 0; run < N_WR; run++)
  {
    // wait until 1s before the next permission expiry
    dds_waitset_wait_until (ws, NULL, 0, t_exp_wr[run] - DDS_SECS (1));
    print_test_msg ("run %d\n", run);
    for (int w = 0; w < N_WR; w++)
    {
      sample.id = w;
      ret = dds_write (wr[w], &sample);
      print_test_msg ("write %d\n", w);
      CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
    }
  }

  // wait until last pp's permissions are expired, with a
  // bit of margin for the expiration to be handled by the
  // reader domain
  dds_waitset_wait_until (ws, NULL, 0, t_exp_wr[N_WR - 1] + DDS_SECS (1));

  // check received data
  SecurityCoreTests_Type1 * data = ddsrt_calloc (N_WR, sizeof (*data));
  dds_sample_info_t rd_info[N_WR];
  static void * rd_samples[N_WR];
  for (int i = 0; i < N_WR; i++)
    rd_samples[i] = &data[i];

  for (int w = 0; w < N_WR; w++)
  {
    sample.id = w;
    dds_instance_handle_t ih = dds_lookup_instance(rd, &sample);
    CU_ASSERT_NOT_EQUAL_FATAL(ih, DDS_HANDLE_NIL);
    ret = dds_take_instance (rd, rd_samples, rd_info, N_WR, N_WR, ih);
    print_test_msg ("samples from writer %d: %d\n", w, ret);
    CU_ASSERT_EQUAL_FATAL (ret, w + 1);
    print_test_msg ("writer %d instance state: %d\n", w, rd_info[w].instance_state);
    CU_ASSERT_EQUAL_FATAL (rd_info[w].instance_state, DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE);
    print_test_msg ("writer %d valid data: %d\n", w, rd_info[w].valid_data);
    CU_ASSERT_EQUAL_FATAL (rd_info[w].valid_data, true);
  }

  access_control_fini (N_NODES, (void * []) { ca, rules_xml, perm_config_str, data }, 4);
  for (int i = 0; i < N_NODES; i++)
  {
    ddsrt_free (grants[i]);
    ddsrt_free (id_subj[i]);
    ddsrt_free ((char *)id[i]);
  }
}
#undef N_WR
#undef N_NODES
#undef PERM_EXP_BASE
#undef PERM_EXP_INCR

#define na false
CU_TheoryDataPoints(ddssec_access_control, hooks) = {
    CU_DataPoints(const char *,
    /*                 */"init_test_access_control_local_participant_not_allowed",
    /*                  |     */"init_test_access_control_local_permissions_not_allowed",
    /*                  |      |     */"init_test_access_control_local_topic_not_allowed",
    /*                  |      |      |     */"init_test_access_control_local_writer_not_allowed",
    /*                  |      |      |      |      */"init_test_access_control_local_reader_not_allowed",
    /*                  |      |      |      |      |     */"init_test_access_control_remote_permissions_not_allowed",
    /*                  |      |      |      |      |      |     */"init_test_access_control_remote_participant_not_allowed",
    /*                  |      |      |      |      |      |      |     */"init_test_access_control_remote_topic_not_allowed",
    /*                  |      |      |      |      |      |      |      |     */"init_test_access_control_remote_writer_not_allowed",
    /*                  |      |      |      |      |      |      |      |      |     */"init_test_access_control_remote_reader_not_allowed",
    /*                  |      |      |      |      |      |      |      |      |      |     */"init_test_access_control_remote_reader_relay_only"),
    CU_DataPoints(bool, true,  true,  false, false, false, false, false, false, false, false, false),  // exp_pp_fail
    CU_DataPoints(bool, na,    na,    true,  false, false, false, false, false, false, false, false),  // exp_local_topic_fail
    CU_DataPoints(bool, na,    na,    false, false, false, false, false, false, false, false, false),  // exp_remote_topic_fail
    CU_DataPoints(bool, na,    na,    na,    true,  false, false, false, false, false, false, false),  // exp_wr_fail
    CU_DataPoints(bool, na,    na,    na,    false, true,  false, false, false, false, false, false),  // exp_rd_fail
    CU_DataPoints(bool, na,    na,    na,    na,    na,    true,  true,  true,  false, true,  true),   // exp_wr_rd_sync_fail
    CU_DataPoints(bool, na,    na,    na,    false, na,    true,  true,  true,  true,  false, false),  // exp_rd_wr_sync_fail
};
#undef na
/* Test that the security implementation in DDSI is correctly handling denial of
   creating enities, e.g. local participant not allowed, local writer not allowed,
   remote topic not allowed, etc. This test is initializing the wrapper plugin in a
   not-allowed mode to force denial of a specified entity. */
CU_Theory(
  (const char * init_fn, bool exp_pp_fail, bool exp_local_topic_fail, bool exp_remote_topic_fail, bool exp_wr_fail, bool exp_rd_fail, bool exp_wr_rd_sync_fail, bool exp_rd_wr_sync_fail),
  ddssec_access_control, hooks, .timeout=60)
{
  for (int i = 0; i <= 1; i++)
  {
    bool discovery_protection = (i == 0);
    print_test_msg ("running test access_control_hooks: %s with discovery protection %s\n", init_fn, discovery_protection ? "enabled" : "disabled");

    char * gov_topic_rule = get_governance_topic_rule ("*", discovery_protection, false, true, true, PK_N, BPK_N);
    char * gov_config = get_governance_config (false, true, PK_E, PK_N, PK_N, gov_topic_rule, true);

    const char * def_perm = PF_F COMMON_ETC_PATH("default_permissions.p7s");
    const char * def_perm_ca = PF_F COMMON_ETC_PATH("default_permissions_ca.pem");

    access_control_init (
        2,
        (const char *[]) { TEST_IDENTITY1_CERTIFICATE, TEST_IDENTITY1_CERTIFICATE },
        (const char *[]) { TEST_IDENTITY1_PRIVATE_KEY, TEST_IDENTITY1_PRIVATE_KEY },
        (const char *[]) { TEST_IDENTITY_CA1_CERTIFICATE, TEST_IDENTITY_CA1_CERTIFICATE },
        (bool []) { exp_pp_fail, false },
        (const char *[]) { init_fn, "init_test_access_control_wrapped" }, (const char *[]) { "finalize_test_access_control_not_allowed", "finalize_test_access_control_wrapped" },
        (bool []) { true, true, true }, (const char *[]) { gov_config, gov_config },
        (bool []) { true, true, true }, (const char *[]) { def_perm, def_perm },
        (bool []) { true, true, true }, (const char *[]) { def_perm_ca, def_perm_ca });

    if (!exp_pp_fail)
    {
      const dds_time_t sync_abstimeout = dds_time () + DDS_SECS (2);

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

      // Local reader, remote writer
      create_topic_name (AC_WRAPPER_TOPIC_PREFIX, g_topic_nr++, topic_name, sizeof (topic_name));
      rd_wr_init_fail (
        g_participant[1], &rpub, &rtopic[1], &rwr,
        g_participant[0], &lsub, &ltopic[1], &lrd,
        topic_name, exp_remote_topic_fail, false, exp_local_topic_fail, exp_rd_fail);

      if (!exp_local_topic_fail && !exp_remote_topic_fail && !exp_wr_fail)
        sync_writer_to_readers (g_participant[0], lwr, exp_wr_rd_sync_fail ? 0 : 1, sync_abstimeout);
      if (!exp_local_topic_fail && !exp_remote_topic_fail && !exp_rd_fail)
        sync_reader_to_writers (g_participant[0], lrd, exp_rd_wr_sync_fail ? 0 : 1, sync_abstimeout);
    }

    access_control_fini (2, (void * []) { gov_topic_rule, gov_config }, 2);
  }
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
/* Testing handshake result using join access control setting enabled/disabled and
   valid/invalid permissions for 2 participants. */
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

  dds_time_t now = dds_time ();
  char * rules1_xml = get_permissions_rules (perm_inv_pp1 ? "99" : NULL, topic_name, topic_name, NULL, NULL);
  char * rules2_xml = get_permissions_rules (perm_inv_pp2 ? "99" : NULL, topic_name, topic_name, NULL, NULL);
  char * grants[] = {
    get_permissions_grant ("id1", id1_subj, now, now + DDS_SECS(3600), rules1_xml, NULL),
    get_permissions_grant ("id2", id2_subj, now, now + DDS_SECS(3600), rules2_xml, NULL) };
  char * perm_config = get_permissions_config (grants, 2, true);

  char * gov_topic_rule = get_governance_topic_rule ("*", false, false, true, true, PK_N, BPK_N);
  char * gov_config_pp1 = get_governance_config (false, join_ac_pp1, PK_N, PK_N, PK_N, gov_topic_rule, true);
  char * gov_config_pp2 = get_governance_config (false, join_ac_pp2, PK_N, PK_N, PK_N, gov_topic_rule, true);
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

  access_control_fini (2, (void * []) { gov_config_pp1, gov_config_pp2, gov_topic_rule, rules1_xml, rules2_xml, grants[0], grants[1], perm_config, ca, id1_subj, id2_subj, id1, id2 }, 13);
}

#define na false
CU_TheoryDataPoints(ddssec_access_control, discovery_liveliness_protection) = {
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

enum test_discovery_liveliness
{
  TEST_DISCOVERY,
  TEST_LIVELINESS
};

static void test_discovery_liveliness_protection(enum test_discovery_liveliness kind,
    bool enable_protection_pp1, bool enable_protection_pp2,
    DDS_Security_ProtectionKind protection_kind_pp1, DDS_Security_ProtectionKind protection_kind_pp2,
    bool exp_rd_wr_match_fail, bool exp_secure_pub_wr_handle, bool exp_secure_pub_wr_encode_decode)
{
  bool dp = kind == TEST_DISCOVERY, lp = kind == TEST_LIVELINESS;
  char topic_name[100];
  create_topic_name ("ddssec_access_control_", g_topic_nr++, topic_name, sizeof (topic_name));

  /* create ca and id1/id2 certs that will not expire during this test */
  char *ca, *id1, *id2, *id1_subj, *id2_subj;
  ca = generate_ca ("ca1", TEST_IDENTITY_CA1_PRIVATE_KEY, 0, 3600);
  id1 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id1", TEST_IDENTITY1_PRIVATE_KEY, 0, 3600, &id1_subj);
  id2 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id2", TEST_IDENTITY1_PRIVATE_KEY, 0, 3600, &id2_subj);

  char * grants[] = {
    get_permissions_default_grant ("id1", id1_subj, topic_name),
    get_permissions_default_grant ("id2", id2_subj, topic_name) };
  char * perm_config = get_permissions_config (grants, 2, true);

  char * gov_topic_rule1 = get_governance_topic_rule (topic_name, dp && enable_protection_pp1, lp && enable_protection_pp1, true, true, PK_E, BPK_N);
  char * gov_topic_rule2 = get_governance_topic_rule (topic_name, dp && enable_protection_pp2, lp && enable_protection_pp2, true, true, PK_E, BPK_N);
  char * gov_config1 = get_governance_config (false, true, dp ? protection_kind_pp1 : PK_N, lp ? protection_kind_pp1 : PK_N, PK_E, gov_topic_rule1, true);
  char * gov_config2 = get_governance_config (false, true, dp ? protection_kind_pp2 : PK_N, lp ? protection_kind_pp2 : PK_N, PK_E, gov_topic_rule2, true);
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
  sync_writer_to_readers (g_participant[0], wr, exp_rd_wr_match_fail ? 0 : 1, dds_time() + DDS_SECS(2));
  if (!exp_rd_wr_match_fail)
    write_read_for (wr, g_participant[1], rd, DDS_MSECS (100), false, false);

  unsigned builtin_wr = dp ? DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER : DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER;
  const char * builtin_wr_descr = dp ? "SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER" : "P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER";
  DDS_Security_DatawriterCryptoHandle secure_pub_wr_handle = get_builtin_writer_crypto_handle (g_participant[0], builtin_wr);
  print_test_msg ("crypto handle for %s: %ld\n", builtin_wr_descr, secure_pub_wr_handle);
  CU_ASSERT_EQUAL_FATAL (exp_secure_pub_wr_handle, secure_pub_wr_handle != 0);

  struct dds_security_cryptography_impl * crypto_context_pub = get_cryptography_context (g_participant[0]);
  CU_ASSERT_FATAL (crypto_context_pub != NULL);

  struct crypto_encode_decode_data *log = get_encode_decode_log (crypto_context_pub, ENCODE_DATAWRITER_SUBMESSAGE, secure_pub_wr_handle);
  CU_ASSERT_EQUAL_FATAL (exp_secure_pub_wr_handle && exp_secure_pub_wr_encode_decode, log != NULL);
  if (log != NULL)
  {
    print_test_msg ("encode_datawriter_submessage count for %s: %u\n", builtin_wr_descr, log->count);
    CU_ASSERT_FATAL (log->count > 0);
    ddsrt_free (log);
  }

  access_control_fini (2, (void * []) { gov_config1, gov_config2, gov_topic_rule1, gov_topic_rule2, grants[0], grants[1], perm_config, ca, id1_subj, id2_subj, id1, id2 }, 12);
}
/* Testing discovery and liveliness protection by checking that encode_datawriter_submessage
   is called for SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER and/or P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER
   depending on the discovery and liveliness protection settings in security configuration. */
CU_Theory(
  (const char * test_descr, bool enable_discovery_protection_pp1, bool enable_discovery_protection_pp2,
    DDS_Security_ProtectionKind discovery_protection_kind_pp1, DDS_Security_ProtectionKind discovery_protection_kind_pp2,
    bool exp_rd_wr_match_fail, bool exp_secure_pub_wr_handle, bool exp_secure_pub_wr_encode_decode),
  ddssec_access_control, discovery_liveliness_protection, .timeout=40)
{
  enum test_discovery_liveliness kinds[2] = { TEST_DISCOVERY, TEST_LIVELINESS };
  for (size_t i = 0; i < sizeof (kinds) / sizeof (kinds[0]); i++)
  {
    print_test_msg ("running test %s_protection: %s\n", kinds[i] == TEST_DISCOVERY ? "discovery" : "liveliness", test_descr);
    test_discovery_liveliness_protection (kinds[i], enable_discovery_protection_pp1, enable_discovery_protection_pp2,
      discovery_protection_kind_pp1, discovery_protection_kind_pp2, exp_rd_wr_match_fail, exp_secure_pub_wr_handle, exp_secure_pub_wr_encode_decode);
  }
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

  char * grants[] = {
    get_permissions_default_grant ("id1", id1_subj, topic_name),
    get_permissions_default_grant ("id2", id2_subj, topic_name) };
  char * perm_config = get_permissions_config (grants, 2, true);

  char * gov_topic_rule1 = get_governance_topic_rule (topic_name, true, true, true, true, metadata_pk1, payload_pk1);
  char * gov_topic_rule2 = get_governance_topic_rule (topic_name, true, true, true, true, metadata_pk2, payload_pk2);
  char * gov_config1 = get_governance_config (false, true, discovery_pk1, liveliness_pk1, rtps_pk1, gov_topic_rule1, true);
  char * gov_config2 = get_governance_config (false, true, discovery_pk2, liveliness_pk2, rtps_pk2, gov_topic_rule2, true);
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
  validate_handshake (DDS_DOMAINID, false, NULL, &hs_list, &nhs, DDS_SECS(2));
  CU_ASSERT_EQUAL_FATAL (exp_hs_fail, nhs < 1);
  handshake_list_fini (hs_list, nhs);

  if (!exp_hs_fail)
  {
    dds_entity_t pub, sub, pub_tp, sub_tp, wr, rd;
    rd_wr_init (g_participant[0], &pub, &pub_tp, &wr, g_participant[1], &sub, &sub_tp, &rd, topic_name);
    sync_writer_to_readers (g_participant[0], wr, exp_rd_wr_fail ? 0 : 1, dds_time() + DDS_SECS(1));
  }

  access_control_fini (2, (void * []) { gov_config1, gov_config2, gov_topic_rule1, gov_topic_rule2, grants[0], grants[1], perm_config, ca, id1_subj, id2_subj, id1, id2 }, 12);
}

/* Testing handshake result for any combination of protection kind values for rtps, discovery,
   liveliness, metadata (submsg) and payload encoding. In all cases where there is an encoding
   mismatch, the security handshake is expect to fail */
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


static void test_readwrite_protection (
  bool allow_pub, bool allow_sub, bool deny_pub, bool deny_sub, bool write_ac, bool read_ac,
  bool exp_pub_pp_fail, bool exp_pub_tp_fail, bool exp_wr_fail,
  bool exp_sub_pp_fail, bool exp_sub_tp_fail, bool exp_rd_fail,
  bool exp_sync_fail, const char * default_policy)
{
  print_test_msg ("running test readwrite_protection: \n");
  print_test_msg ("- allow_pub=%d | allow_sub=%d | deny_pub=%d | deny_sub=%d | write_ac=%d | read_ac=%d | default_policy=%s\n", allow_pub, allow_sub, deny_pub, deny_sub, write_ac, read_ac, default_policy);
  print_test_msg ("- exp_pub_pp_fail=%d | exp_pub_tp_fail=%d | exp_wr_fail=%d | exp_sub_pp_fail=%d | exp_sub_tp_fail=%d | exp_rd_fail=%d | exp_sync_fail=%d\n", exp_pub_pp_fail, exp_pub_tp_fail, exp_wr_fail, exp_sub_pp_fail, exp_sub_tp_fail, exp_rd_fail, exp_sync_fail);

  char topic_name[100];
  create_topic_name ("ddssec_access_control_", g_topic_nr++, topic_name, sizeof (topic_name));

  /* create ca and id1/id2 certs that will not expire during this test */
  char *ca, *id1, *id2, *id1_subj, *id2_subj;
  ca = generate_ca ("ca1", TEST_IDENTITY_CA1_PRIVATE_KEY, 0, 3600);
  id1 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id1", TEST_IDENTITY1_PRIVATE_KEY, 0, 3600, &id1_subj);
  id2 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id2", TEST_IDENTITY1_PRIVATE_KEY, 0, 3600, &id2_subj);

  dds_time_t now = dds_time ();
  char * rules_xml = get_permissions_rules (NULL, allow_pub ? topic_name : NULL, allow_sub ? topic_name : NULL, deny_pub ? topic_name : NULL, deny_sub ? topic_name : NULL);
  char * grants[] = {
    get_permissions_grant ("id1", id1_subj, now, now + DDS_SECS(3600), rules_xml, default_policy),
    get_permissions_grant ("id2", id2_subj, now, now + DDS_SECS(3600), rules_xml, default_policy) };
  char * perm_config = get_permissions_config (grants, 2, true);

  char * gov_topic_rule = get_governance_topic_rule (topic_name, false, false, read_ac, write_ac, PK_E, BPK_E);
  char * gov_config = get_governance_config (false, true, PK_N, PK_N, PK_N, gov_topic_rule, true);
  const char * def_perm_ca = PF_F COMMON_ETC_PATH("default_permissions_ca.pem");

  access_control_init (
      2,
      (const char *[]) { id1, id2 },
      (const char *[]) { TEST_IDENTITY1_PRIVATE_KEY, TEST_IDENTITY1_PRIVATE_KEY },
      (const char *[]) { ca, ca },
      (bool []) { exp_pub_pp_fail, exp_sub_pp_fail }, NULL, NULL,
      (bool []) { true, true }, (const char *[]) { gov_config, gov_config },
      (bool []) { true, true }, (const char *[]) { perm_config, perm_config },
      (bool []) { true, true }, (const char *[]) { def_perm_ca, def_perm_ca });

  if (!exp_pub_pp_fail && !exp_sub_pp_fail)
  {
    dds_entity_t pub, sub, pub_tp, sub_tp, wr, rd;
    validate_handshake_nofail (DDS_DOMAINID, DDS_SECS(2));
    rd_wr_init_fail (g_participant[0], &pub, &pub_tp, &wr, g_participant[1], &sub, &sub_tp, &rd, topic_name, exp_pub_tp_fail, exp_wr_fail, exp_sub_tp_fail, exp_rd_fail);
    if (!exp_pub_tp_fail && !exp_wr_fail && !exp_sub_tp_fail && !exp_rd_fail)
      sync_writer_to_readers (g_participant[0], wr, exp_sync_fail ? 0 : 1, dds_time() + DDS_SECS(1));
  }

  access_control_fini (2, (void * []) { gov_config, gov_topic_rule, rules_xml, grants[0], grants[1], perm_config, ca, id1_subj, id2_subj, id1, id2 }, 11);
}
/* Test read/write access control by running test cases with different combinations
   of allow and deny rules for publishing and subscribing on a topic, and check correct
   working of the default policy. */
CU_Test(ddssec_access_control, readwrite_protection, .timeout=60)
{
  for (int allow_pub = 0; allow_pub <= 1; allow_pub++)
    for (int allow_sub = 0; allow_sub <= 1; allow_sub++)
      for (int deny_pub = 0; deny_pub <= 1 && !(allow_pub && deny_pub); deny_pub++)
        for (int deny_sub = 0; deny_sub <= 1 && !(allow_sub && deny_sub); deny_sub++)
          for (int write_ac = 0; write_ac <= 1; write_ac++)
            for (int read_ac = 0; read_ac <= 1; read_ac++)
              for (int default_deny = 0; default_deny <= 1; default_deny++)
              {
                /* creating local writer/reader fails if write_ac/read_ac enabled and no allow-pub/sub or a deny-pub/sub rule exists */
                bool exp_wr_fail = write_ac && !allow_pub && (deny_pub || default_deny);
                bool exp_rd_fail = read_ac && !allow_sub && (deny_sub || default_deny);
                /* if both read_ac and write_ac are enabled, and pub and sub not allowed, topic creation should fail */
                bool exp_tp_fail = write_ac && read_ac && !allow_pub && !allow_sub && (deny_pub || deny_sub || default_deny);
                /* participant creation should fail under same conditions as topic creation (as opposed to the DDS Security spec,
                  table 63, that states that participant creation fails when there is not any topic that has enable_read/write_ac
                  set to false and join_ac is enabled; it seems that the allow_rule condition is missing there) */
                bool exp_pp_fail = write_ac && read_ac && !allow_pub && !allow_sub && default_deny;
                /* join-ac is enabled in this test, so check_remote_pp fails (and therefore the reader/writer sync) in case not any allow rule exists */
                bool exp_sync_fail = !allow_pub && !allow_sub && default_deny;

                test_readwrite_protection (allow_pub, allow_sub, deny_pub, deny_sub, write_ac, read_ac,
                  exp_pp_fail, exp_tp_fail, exp_wr_fail, exp_pp_fail, exp_tp_fail, exp_rd_fail, exp_sync_fail, default_deny ? "DENY" : "ALLOW");
              }
}

/* Check that communication for a topic that is allowed in the permissions config
   keeps working in case the publisher also creates a writer for a non-allowed topic */
CU_Test(ddssec_access_control, denied_topic)
{
  char topic_name[100], denied_topic_name[100];
  create_topic_name ("ddssec_access_control_", g_topic_nr++, topic_name, sizeof (topic_name));
  create_topic_name ("ddssec_access_control_", g_topic_nr++, denied_topic_name, sizeof (denied_topic_name));

  char *ca, *id1, *id2, *id1_subj, *id2_subj;
  ca = generate_ca ("ca1", TEST_IDENTITY_CA1_PRIVATE_KEY, 0, 3600);
  id1 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id1", TEST_IDENTITY1_PRIVATE_KEY, 0, 3600, &id1_subj);
  id2 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id2", TEST_IDENTITY1_PRIVATE_KEY, 0, 3600, &id2_subj);

   dds_time_t now = dds_time ();
  char * sub_rules_xml = get_permissions_rules (NULL, NULL, NULL, denied_topic_name, denied_topic_name);
  char * grants_pub[] = { get_permissions_grant ("id1", id1_subj, now, now + DDS_SECS(3600), NULL, "ALLOW") };
  char * grants_sub[] = { get_permissions_grant ("id2", id2_subj, now, now + DDS_SECS(3600), sub_rules_xml, "ALLOW") };
  char * perm_config_pub = get_permissions_config (grants_pub, 1, true);
  char * perm_config_sub = get_permissions_config (grants_sub, 1, true);

  char * gov_topic_rule = get_governance_topic_rule (NULL, true, true, true, true, PK_E, BPK_E);
  char * gov_config = get_governance_config (false, true, PK_E, PK_E, PK_E, gov_topic_rule, true);
  const char * def_perm_ca = PF_F COMMON_ETC_PATH("default_permissions_ca.pem");

  access_control_init (
      2,
      (const char *[]) { id1, id2 },
      (const char *[]) { TEST_IDENTITY1_PRIVATE_KEY, TEST_IDENTITY1_PRIVATE_KEY },
      (const char *[]) { ca, ca },
      (bool []) { false, false },
      NULL, NULL,
      (bool []) { true, true }, (const char *[]) { gov_config, gov_config },
      (bool []) { true, true }, (const char *[]) { perm_config_pub, perm_config_sub },
      (bool []) { true, true }, (const char *[]) { def_perm_ca, def_perm_ca });

  dds_entity_t pub, sub, pub_tp, sub_tp, wr, rd;
  rd_wr_init (g_participant[0], &pub, &pub_tp, &wr, g_participant[1], &sub, &sub_tp, &rd, topic_name);

  const dds_time_t sync_abstimeout = dds_time () + DDS_SECS (2);
  sync_writer_to_readers (g_participant[0], wr, 1, sync_abstimeout);
  sync_reader_to_writers (g_participant[1], rd, 1, sync_abstimeout);

  /* Create a topic that is denied in the subscriber pp security config */
  dds_entity_t denied_pub_tp = dds_create_topic (g_participant[0], &SecurityCoreTests_Type1_desc, denied_topic_name, NULL, NULL);
  CU_ASSERT_FATAL (denied_pub_tp > 0);
  dds_qos_t * qos = get_default_test_qos ();
  dds_entity_t denied_tp_wr = dds_create_writer (pub, denied_pub_tp, qos, NULL);
  CU_ASSERT_FATAL (denied_tp_wr > 0);

  /* Check that creating denied topic for subscriber fails */
  dds_entity_t denied_sub_tp = dds_create_topic (g_participant[1], &SecurityCoreTests_Type1_desc, denied_topic_name, NULL, NULL);
  CU_ASSERT_FATAL (denied_sub_tp == DDS_RETCODE_NOT_ALLOWED_BY_SECURITY);

  /* Check if communication for allowed topic is still working */
  write_read_for (wr, g_participant[1], rd, DDS_MSECS (10), false, false);

  dds_delete_qos (qos);
  access_control_fini (2, (void * []) { gov_config, gov_topic_rule, sub_rules_xml, grants_pub[0], grants_sub[0], perm_config_pub, perm_config_sub, ca, id1_subj, id2_subj, id1, id2 }, 12);
}

/* Check that partitons are taken into account */
static void partition_test (const char **parts_allowed, const char **parts_denied, const char **parts_used, bool exp_deny)
{
  char topic_name[100];

  create_topic_name ("ddssec_access_control_", g_topic_nr++, topic_name, sizeof (topic_name));

  char *ca, *id1, *id2, *id1_subj, *id2_subj;
  ca = generate_ca ("ca1", TEST_IDENTITY_CA1_PRIVATE_KEY, 0, 3600);
  id1 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id1", TEST_IDENTITY1_PRIVATE_KEY, 0, 3600, &id1_subj);
  id2 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id2", TEST_IDENTITY1_PRIVATE_KEY, 0, 3600, &id2_subj);

  dds_time_t now = dds_time ();
  char * pubsub_rules_xml = get_permissions_rules_w_partitions (NULL, topic_name, topic_name, parts_allowed, topic_name, topic_name, parts_denied);
  char * grants_pub[] = { get_permissions_grant ("id1", id1_subj, now, now + DDS_SECS(3600), pubsub_rules_xml, "ALLOW") };
  char * grants_sub[] = { get_permissions_grant ("id2", id2_subj, now, now + DDS_SECS(3600), pubsub_rules_xml, "ALLOW") };
  char * perm_config_pub = get_permissions_config (grants_pub, 1, true);
  char * perm_config_sub = get_permissions_config (grants_sub, 1, true);

  char * gov_topic_rule = get_governance_topic_rule (NULL, true, true, true, true, PK_E, BPK_E);
  char * gov_config = get_governance_config (false, true, PK_E, PK_E, PK_E, gov_topic_rule, true);
  const char * def_perm_ca = PF_F COMMON_ETC_PATH("default_permissions_ca.pem");

  access_control_init (
      2,
      (const char *[]) { id1, id2 },
      (const char *[]) { TEST_IDENTITY1_PRIVATE_KEY, TEST_IDENTITY1_PRIVATE_KEY },
      (const char *[]) { ca, ca },
      (bool []) { false, false },
      NULL, NULL,
      (bool []) { true, true }, (const char *[]) { gov_config, gov_config },
      (bool []) { true, true }, (const char *[]) { perm_config_pub, perm_config_sub },
      (bool []) { true, true }, (const char *[]) { def_perm_ca, def_perm_ca });

  dds_entity_t pub, sub, pub_tp, sub_tp, wr, rd;
  rd_wr_init_w_partitions_fail (g_participant[0], &pub, &pub_tp, &wr,
                                g_participant[1], &sub, &sub_tp, &rd,
                                topic_name, parts_used,
                                false, exp_deny,
                                false, exp_deny);
  access_control_fini (2, (void * []) { gov_config, gov_topic_rule, pubsub_rules_xml, grants_pub[0], grants_sub[0], perm_config_pub, perm_config_sub, ca, id1_subj, id2_subj, id1, id2 }, 12);
}

CU_Test(ddssec_access_control, partition)
{
  const struct parts {
    const char **allow;
    const char **deny;
    const char **use;
    bool exp_deny;
  } parts[] = {
#define PS(...) (const char *[]){__VA_ARGS__,0}
    { NULL, PS("*"), NULL, false }, // default case
    { PS(""), PS("*"), PS(""), false }, // default partition
    { PS(""), PS("*"), PS("x"), true },
    { PS("x"), PS("*"), PS("x"), false },
    { PS("", "x"), PS("*"), PS(""), false },
    { PS("", "x"), PS("*"), PS("x"), false },
    { PS("", "x"), PS("*"), PS("y"), true },
    { PS("", "x"), PS("*"), PS("", "x"), false },
    { PS("", "x"), PS("*"), PS("x", ""), false },
    { PS("", "x"), PS("*"), PS("x", "y"), true },
    { PS("??"), PS("*"), PS("xy"), false },
    { PS("??"), PS("*"), PS("x", "y"), true },
    { PS("??"), PS("*"), PS("xy", "y"), true },

    { PS("Q"), NULL, PS(""), true }, // default partition
    { PS("Q"), PS(""), PS(""), true },
    { PS("Q"), PS("", "x"), PS(""), true },
    { PS("Q"), PS("", "x"), PS("x"), true },
    { PS("Q"), PS("", "x"), PS("", "x"), true },
    { PS("Q"), PS("", "x"), PS("x", "y"), true },
    { PS("Q"), PS("", "x"), PS("y"), false },
#undef PS
  };
  // The generated configuration has the allow rule comes first,
  // then the deny rule and finally a default of allow
  for (size_t i = 0; i < sizeof (parts) / sizeof (parts[0]); i++)
  {
    printf ("======== ALLOW:{");
    for (int j = 0; parts[i].allow && parts[i].allow[j]; j++)
      printf (" \"%s\"", parts[i].allow[j]);
    printf (" } DENY:{");
    for (int j = 0; parts[i].deny && parts[i].deny[j]; j++)
      printf (" \"%s\"", parts[i].deny[j]);
    printf (" } USE:{");
    for (int j = 0; parts[i].use && parts[i].use[j]; j++)
      printf (" \"%s\"", parts[i].use[j]);
    printf (" } EXP_DENY: %s\n", parts[i].exp_deny ? "true" : "false");
    partition_test (parts[i].allow, parts[i].deny, parts[i].use, parts[i].exp_deny);
  }
}
