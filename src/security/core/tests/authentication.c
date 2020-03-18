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
#include "dds/ddsrt/string.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/ddsi_xqos.h"

#include "dds/security/dds_security_api.h"

#include "common/config_env.h"
#include "common/authentication_wrapper.h"
#include "common/handshake_test_utils.h"
#include "common/security_config_test_utils.h"
#include "common/test_identity.h"
#include "common/cert_utils.h"
#include "common/security_config_test_utils.h"

#include "SecurityCoreTests.h"


#define ID1 TEST_IDENTITY1_CERTIFICATE
#define ID1K TEST_IDENTITY1_PRIVATE_KEY
#define ID2 TEST_IDENTITY2_CERTIFICATE
#define ID2K TEST_IDENTITY2_PRIVATE_KEY
#define ID3 TEST_IDENTITY3_CERTIFICATE
#define ID3K TEST_IDENTITY3_PRIVATE_KEY
#define CA1 TEST_IDENTITY_CA1_CERTIFICATE
#define CA1K TEST_IDENTITY_CA1_PRIVATE_KEY
#define CA2 TEST_IDENTITY_CA2_CERTIFICATE
#define CA2K TEST_IDENTITY_CA2_PRIVATE_KEY

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
    "      ${TRUSTED_CA_DIR:+<TrustedCADir>}${TRUSTED_CA_DIR}${TRUSTED_CA_DIR:+</TrustedCADir>}"
    "    </Authentication>"
    "    <AccessControl>"
    "      <Library finalizeFunction=\"finalize_access_control\" initFunction=\"init_access_control\"/>"
    "      <Governance><![CDATA[${GOVERNANCE_CONFIG}]]></Governance>"
    "      <PermissionsCA>file:" COMMON_ETC_PATH("default_permissions_ca.pem") "</PermissionsCA>"
    "      <Permissions><![CDATA[${PERM_CONFIG}]]></Permissions>"
    "    </AccessControl>"
    "    <Cryptographic>"
    "      <Library finalizeFunction=\"finalize_crypto\" initFunction=\"init_crypto\"/>"
    "    </Cryptographic>"
    "  </DDSSecurity>"
    "</Domain>";

#define DDS_DOMAINID1 0
#define DDS_DOMAINID2 1

#define DEF_PERM_CONF "file:" COMMON_ETC_PATH("default_permissions.p7s")

static dds_entity_t g_domain1;
static dds_entity_t g_participant1;
static dds_entity_t g_pub;
static dds_entity_t g_pub_tp;
static dds_entity_t g_wr;

static dds_entity_t g_domain2;
static dds_entity_t g_participant2;
static dds_entity_t g_sub;
static dds_entity_t g_sub_tp;
static dds_entity_t g_rd;

static uint32_t g_topic_nr = 0;

static void authentication_init(
    const char * id1_cert, const char * id1_key, const char * id1_ca,
    const char * id2_cert, const char * id2_key, const char * id2_ca,
    const char * trusted_ca_dir, const char * perm_config,
    bool exp_pp1_fail, bool exp_pp2_fail)
{
  if (perm_config == NULL)
    perm_config = DEF_PERM_CONF;

  struct kvp governance_vars[] = {
    { "DISCOVERY_PROTECTION_KIND", "NONE", 1 },
    { "LIVELINESS_PROTECTION_KIND", "NONE", 1 },
    { "RTPS_PROTECTION_KIND", "NONE", 1 },
    { "METADATA_PROTECTION_KIND", "NONE", 1 },
    { "DATA_PROTECTION_KIND", "NONE", 1 },
    { NULL, NULL, 0 }
  };
  char * gov_config_signed = get_governance_config (governance_vars, true);

  struct kvp config_vars1[] = {
    { "TEST_IDENTITY_CERTIFICATE", id1_cert, 1 },
    { "TEST_IDENTITY_PRIVATE_KEY", id1_key, 1 },
    { "TEST_IDENTITY_CA_CERTIFICATE", id1_ca, 1 },
    { "TRUSTED_CA_DIR", trusted_ca_dir, 3 },
    { "PERM_CONFIG", perm_config, 1 },
    { "GOVERNANCE_CONFIG", gov_config_signed, 1 },
    { NULL, NULL, 0 }
  };

  struct kvp config_vars2[] = {
    { "TEST_IDENTITY_CERTIFICATE", id2_cert, 1 },
    { "TEST_IDENTITY_PRIVATE_KEY", id2_key, 1 },
    { "TEST_IDENTITY_CA_CERTIFICATE", id2_ca, 1 },
    { "TRUSTED_CA_DIR", trusted_ca_dir, 3 },
    { "PERM_CONFIG", perm_config, 1 },
    { "GOVERNANCE_CONFIG", gov_config_signed, 1 },
    { NULL, NULL, 0 }
  };

  char *conf1 = ddsrt_expand_vars_sh (config, &expand_lookup_vars_env, config_vars1);
  char *conf2 = ddsrt_expand_vars_sh (config, &expand_lookup_vars_env, config_vars2);
  CU_ASSERT_EQUAL_FATAL (expand_lookup_unmatched (config_vars1), 0);
  CU_ASSERT_EQUAL_FATAL (expand_lookup_unmatched (config_vars2), 0);
  g_domain1 = dds_create_domain (DDS_DOMAINID1, conf1);
  g_domain2 = dds_create_domain (DDS_DOMAINID2, conf2);
  g_participant1 = dds_create_participant (DDS_DOMAINID1, NULL, NULL);
  g_participant2 = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
  CU_ASSERT_EQUAL_FATAL (exp_pp1_fail, g_participant1 <= 0);
  CU_ASSERT_EQUAL_FATAL (exp_pp2_fail, g_participant2 <= 0);

  ddsrt_free (gov_config_signed);
  ddsrt_free (conf1);
  ddsrt_free (conf2);
}

static char *create_topic_name(const char *prefix, uint32_t nr, char *name, size_t size)
{
  ddsrt_pid_t pid = ddsrt_getpid ();
  ddsrt_tid_t tid = ddsrt_gettid ();
  (void)snprintf(name, size, "%s%d_pid%" PRIdPID "_tid%" PRIdTID "", prefix, nr, pid, tid);
  return name;
}

static void sync_writer_to_reader()
{
  dds_attach_t triggered;
  dds_return_t ret;
  dds_entity_t waitset_wr = dds_create_waitset (g_participant1);
  CU_ASSERT_FATAL (waitset_wr > 0);
  dds_publication_matched_status_t pub_matched;

  /* Sync writer to reader. */
  ret = dds_waitset_attach (waitset_wr, g_wr, g_wr);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  while (true)
  {
    ret = dds_waitset_wait (waitset_wr, &triggered, 1, DDS_SECS(5));
    CU_ASSERT_FATAL (ret >= 1);
    CU_ASSERT_EQUAL_FATAL (g_wr, (dds_entity_t)(intptr_t) triggered);
    ret = dds_get_publication_matched_status(g_wr, &pub_matched);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
    if (pub_matched.total_count >= 1)
      break;
  };
  dds_delete (waitset_wr);
}

static void reader_wait_for_data()
{
  dds_attach_t triggered;
  dds_return_t ret;
  dds_entity_t waitset_rd = dds_create_waitset (g_participant2);
  CU_ASSERT_FATAL (waitset_rd > 0);

  ret = dds_waitset_attach (waitset_rd, g_rd, g_rd);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_waitset_wait (waitset_rd, &triggered, 1, DDS_SECS(5));
  CU_ASSERT_EQUAL_FATAL (ret, 1);
  CU_ASSERT_EQUAL_FATAL (g_rd, (dds_entity_t)(intptr_t)triggered);
  dds_delete (waitset_rd);
}

static void rd_wr_init()
{
  char name[100];
  dds_qos_t * qos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, -1);
  dds_qset_durability (qos, DDS_DURABILITY_TRANSIENT_LOCAL);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);

  create_topic_name("ddssec_authentication_", g_topic_nr++, name, sizeof (name));
  g_pub = dds_create_publisher (g_participant1, NULL, NULL);
  CU_ASSERT_FATAL (g_pub > 0);
  g_sub = dds_create_subscriber (g_participant2, NULL, NULL);
  CU_ASSERT_FATAL (g_sub > 0);
  g_pub_tp = dds_create_topic (g_participant1, &SecurityCoreTests_Type1_desc, name, NULL, NULL);
  CU_ASSERT_FATAL (g_pub_tp > 0);
  g_sub_tp = dds_create_topic (g_participant2, &SecurityCoreTests_Type1_desc, name, NULL, NULL);
  CU_ASSERT_FATAL (g_sub_tp > 0);
  g_wr = dds_create_writer (g_pub, g_pub_tp, qos, NULL);
  CU_ASSERT_FATAL (g_wr > 0);
  dds_set_status_mask (g_wr, DDS_PUBLICATION_MATCHED_STATUS);
  g_rd = dds_create_reader (g_sub, g_sub_tp, qos, NULL);
  CU_ASSERT_FATAL (g_rd > 0);
  dds_set_status_mask (g_rd, DDS_DATA_AVAILABLE_STATUS);
  sync_writer_to_reader();
  dds_delete_qos (qos);
}

static void write_read(dds_duration_t dur, bool exp_write_fail, bool exp_read_fail)
{
  SecurityCoreTests_Type1 sample = { 1, 1 };
  SecurityCoreTests_Type1 rd_sample;
  void * samples[] = { &rd_sample };
  dds_sample_info_t info[1];
  dds_return_t ret;
  dds_time_t tend = dds_time () + dur;
  bool write_fail = false, read_fail = false;

  rd_wr_init ();
  do
  {
    ret = dds_write (g_wr, &sample);
    if (ret != DDS_RETCODE_OK)
      write_fail = true;
    while (true)
    {
      if ((ret = dds_take (g_rd, samples, info, 1, 1)) == 0)
      {
        reader_wait_for_data ();
        continue;
      }
      else if (ret < 0)
      {
        read_fail = true;
        break;
      }
      CU_ASSERT_EQUAL_FATAL (ret, 1);
      break;
    }
    dds_sleepfor (DDS_MSECS (1));
  }
  while (dds_time() < tend && !write_fail && !read_fail);
  CU_ASSERT_EQUAL_FATAL (write_fail, exp_write_fail);
  CU_ASSERT_EQUAL_FATAL (read_fail, exp_read_fail);
}

static void authentication_fini(bool delete_pp1, bool delete_pp2)
{
  if (delete_pp1)
    CU_ASSERT_EQUAL_FATAL (dds_delete (g_participant1), DDS_RETCODE_OK);
  if (delete_pp2)
    CU_ASSERT_EQUAL_FATAL (dds_delete (g_participant2), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL (dds_delete (g_domain1), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL (dds_delete (g_domain2), DDS_RETCODE_OK);
}

#define FM_CA "error: unable to get local issuer certificate"
#define FM_INVK "Failed to finalize verify context"
CU_TheoryDataPoints(ddssec_authentication, id_ca_certs) = {
    CU_DataPoints(const char *,
    /*                         */"valid ID1-ID1",
    /*                          |     */"non-trusted remote CA ID1(CA1)-ID2(CA2)",
    /*                          |      |     */"valid ID1-ID3",
    /*                          |      |      |     */"invalid ca ID1(CA1)-ID1(CA2)",
    /*                          |      |      |      |     */"invalid remote key ID3(K1)"),
    CU_DataPoints(const char *, ID1,   ID2,   ID3,   ID1,   ID3),      /* Identity for domain 2 */
    CU_DataPoints(const char *, ID1K,  ID2K,  ID3K,  ID1K,  ID1K),     /* Private key for domain 2 identity */
    CU_DataPoints(const char *, CA1,   CA2,   CA1,   CA2,   CA1),      /* CA for domain 2 identity */
    CU_DataPoints(bool,         false, false, false, false, false),    /* expect create participant1 fails */
    CU_DataPoints(bool,         false, false, false, true,  false),    /* expect create participant2 fails */
    CU_DataPoints(bool,         false, false, false, true,  false),    /* expect validate local failed for domain 2 */
    CU_DataPoints(const char *, NULL,  NULL,  NULL,  FM_CA, NULL),     /* expected error message for validate local failed */
    CU_DataPoints(bool,         false, true,  false, false, true),     /* expect handshake request failed */
    CU_DataPoints(const char *, NULL,  NULL,  NULL,  NULL,  FM_INVK),  /* expected error message for handshake request failed */
    CU_DataPoints(bool,         false, true,  false, true,  true),     /* expect handshake reply failed */
    CU_DataPoints(const char *, NULL,  FM_CA, NULL,  FM_CA, NULL)      /* expected error message for handshake reply failed */
};
#undef FM_CA
#undef FM_INVK

static void validate_hs(struct Handshake *hs, bool exp_fail_hs_req, const char * fail_hs_req_msg, bool exp_fail_hs_reply, const char * fail_hs_reply_msg)
{
  DDS_Security_ValidationResult_t exp_result = hs->node_type == HSN_REQUESTER ? DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE : DDS_SECURITY_VALIDATION_OK;
  if (hs->node_type == HSN_REQUESTER)
  {
    CU_ASSERT_EQUAL_FATAL (hs->finalResult, exp_fail_hs_req ? DDS_SECURITY_VALIDATION_FAILED : exp_result);
    if (exp_fail_hs_req)
    {
      if (fail_hs_req_msg == NULL)
      {
        CU_ASSERT_EQUAL_FATAL (hs->err_msg, NULL);
      }
      else
      {
        CU_ASSERT_FATAL (hs->err_msg != NULL);
        CU_ASSERT_FATAL (strstr(hs->err_msg, fail_hs_req_msg) != NULL);
      }
    }
  }
  else if (hs->node_type == HSN_REPLIER)
  {
    CU_ASSERT_EQUAL_FATAL (hs->finalResult, exp_fail_hs_reply ? DDS_SECURITY_VALIDATION_FAILED : exp_result);
    if (exp_fail_hs_reply)
    {
      if (fail_hs_reply_msg == NULL)
      {
        CU_ASSERT_EQUAL_FATAL (hs->err_msg, NULL);
      }
      else
      {
        CU_ASSERT_FATAL (hs->err_msg != NULL);
        CU_ASSERT_FATAL (strstr(hs->err_msg, fail_hs_reply_msg) != NULL);
      }
    }
  }
}

CU_Theory((const char * test_descr, const char * id2, const char *key2, const char *ca2,
    bool exp_fail_pp1, bool exp_fail_pp2,
    bool exp_fail_local, const char * fail_local_msg,
    bool exp_fail_hs_req, const char * fail_hs_req_msg,
    bool exp_fail_hs_reply, const char * fail_hs_reply_msg),
    ddssec_authentication, id_ca_certs)
{
  struct Handshake *hs_list;
  int nhs;
  printf("running test id_ca_certs: %s\n", test_descr);
  authentication_init (ID1, ID1K, CA1, id2, key2, ca2, NULL, NULL, exp_fail_pp1, exp_fail_pp2);

  // Domain 1
  validate_handshake (DDS_DOMAINID1, false, NULL, &hs_list, &nhs);
  for (int n = 0; n < nhs; n++)
    validate_hs (&hs_list[n], exp_fail_hs_req, fail_hs_req_msg, exp_fail_hs_reply, fail_hs_reply_msg);
  handshake_list_fini (hs_list, nhs);

  // Domain 2
  validate_handshake (DDS_DOMAINID2, exp_fail_local, fail_local_msg, &hs_list, &nhs);
  for (int n = 0; n < nhs; n++)
    validate_hs (&hs_list[n], exp_fail_hs_req, fail_hs_req_msg, exp_fail_hs_reply, fail_hs_reply_msg);
  handshake_list_fini (hs_list, nhs);

  authentication_fini (!exp_fail_pp1, !exp_fail_pp2);
}

CU_TheoryDataPoints(ddssec_authentication, trusted_ca_dir) = {
    CU_DataPoints(const char *, "",    ".",   "/nonexisting", NULL),
    CU_DataPoints(bool,         false, false, true,           false)
};
CU_Theory((const char * ca_dir, bool exp_fail), ddssec_authentication, trusted_ca_dir)
{
  printf("Testing custom CA dir: %s\n", ca_dir);
  authentication_init (ID1, ID1K, CA1, ID1, ID1K, CA1, ca_dir, NULL, exp_fail, exp_fail);
  if (!exp_fail)
  {
    validate_handshake_nofail (DDS_DOMAINID1);
    validate_handshake_nofail (DDS_DOMAINID2);
  }
  authentication_fini (!exp_fail, !exp_fail);
}

#define M(n) ((n)*60)
#define H(n) (M(n)*60)
#define D(n) (H(n)*24)
CU_TheoryDataPoints(ddssec_authentication, expired_cert) = {
    CU_DataPoints(const char *,
    /*                      */"all valid 1d",
    /*                       |     */"ca expired",
    /*                       |      |     */"id1 expired",
    /*                       |      |      |     */"id2 expired",
    /*                       |      |      |      |     */"ca and id1 1min valid",
    /*                       |      |      |      |      |     */"id1 and id2 1s valid, delay 1100ms",
    /*                       |      |      |      |      |      |     */"id1 valid after 1s, delay 1100ms",
    /*                       |      |      |      |      |      |      |    *//*"ca and id1 expire during session"*/),
    CU_DataPoints(int32_t,   0,     -M(1), 0,     0,     0,     0,     0,     /*0*/     ),   /* CA1 not before */
    CU_DataPoints(int32_t,   D(1),  0,     D(1),  D(1),  M(1),  D(1),  D(1),  /*2*/     ),  /* CA1 not after */
    CU_DataPoints(int32_t,   0,     0,     -D(1), 0,     0,     0,     1,     /*0*/     ),  /* ID1 not before */
    CU_DataPoints(int32_t,   D(1),  D(1),  0,     D(1),  M(1),  1,     D(1),  /*2*/     ),  /* ID1 not after */
    CU_DataPoints(bool,      false, true,  true,  false, false, true,  false, /*false*/ ),  /* expect validate local ID1 fail */
    CU_DataPoints(int32_t,   0,     0,     0,     -D(1), 0,     0,     0,     /*0*/     ),  /* ID2 not before */
    CU_DataPoints(int32_t,   D(1),  D(1),  D(1),  0,     D(1),  1,     D(1),  /*D(1)*/  ),  /* ID2 not after */
    CU_DataPoints(bool,      false, true,  false, true,  false, true,  false, /*false*/ ),  /* expect validate local ID2 fail */
    CU_DataPoints(uint32_t,  0,     0,     0,     0,     0,     1100,  1100,  /*0*/     ),  /* delay (ms) after generating certificate */
    CU_DataPoints(uint32_t,  1,     0,     0,     0,     1,     0,     1,     /*3500*/  ),  /* write/read data during x ms */
    CU_DataPoints(bool,      false, false, false, false, false, false, false, /*true*/  ),  /* expect read data failure */
};
CU_Theory(
  (const char * test_descr, int32_t ca_not_before, int32_t ca_not_after,
    int32_t id1_not_before, int32_t id1_not_after, bool id1_local_fail,
    int32_t id2_not_before, int32_t id2_not_after, bool id2_local_fail,
    uint32_t delay, uint32_t write_read_dur, bool exp_read_fail),
  ddssec_authentication, expired_cert)
{
  char *ca, *id1, *id2, *id1_subj, *id2_subj;
  printf("running test expired_cert: %s\n", test_descr);
  ca = generate_ca ("ca1", CA1K, ca_not_before, ca_not_after);
  id1 = generate_identity (ca, CA1K, "id1", ID1K, id1_not_before, id1_not_after, &id1_subj);
  id2 = generate_identity (ca, CA1K, "id2", ID1K, id2_not_before, id2_not_after, &id2_subj);
  dds_sleepfor (DDS_MSECS (delay));

  char * grants[] = { get_permissions_grant ("id1", id1_subj), get_permissions_grant ("id2", id2_subj) };
  char * perm_config = get_permissions_config (grants, 2, true);
  authentication_init (id1, ID1K, ca, id2, ID1K, ca, NULL, perm_config, id1_local_fail, id2_local_fail);
  validate_handshake (DDS_DOMAINID1, id1_local_fail, NULL, NULL, NULL);
  validate_handshake (DDS_DOMAINID2, id2_local_fail, NULL, NULL, NULL);
  if (write_read_dur > 0)
    write_read (DDS_MSECS (write_read_dur), false, exp_read_fail);
  authentication_fini (!id1_local_fail, !id2_local_fail);
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

