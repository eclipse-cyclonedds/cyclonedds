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
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/ddsi_xqos.h"

#include "dds/security/dds_security_api.h"

#include "common/config_env.h"
#include "common/authentication_wrapper.h"
#include "common/test_utils.h"
#include "common/security_config_test_utils.h"
#include "common/test_identity.h"
#include "common/cert_utils.h"
#include "common/security_config_test_utils.h"

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
#define FM_INVK "Failed to finalize digest context"
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
    ddssec_authentication, id_ca_certs, .timeout=30)
{
  struct Handshake *hs_list;
  int nhs;
  print_test_msg ("running test id_ca_certs: %s\n", test_descr);
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
  print_test_msg ("Testing custom CA dir: %s\n", ca_dir);
  authentication_init (ID1, ID1K, CA1, ID1, ID1K, CA1, ca_dir, NULL, exp_fail, exp_fail);
  if (!exp_fail)
  {
    validate_handshake_nofail (DDS_DOMAINID1);
    validate_handshake_nofail (DDS_DOMAINID2);
  }
  authentication_fini (!exp_fail, !exp_fail);
}

#define S(n) (n)
#define M(n) (S(n)*60)
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
    /*                       |      |      |      |      |      |      |     */"id1 expires during session"),
    CU_DataPoints(int32_t,   0,     -M(1), 0,     0,     0,     0,     0,     0     ),  /* CA1 not before */
    CU_DataPoints(int32_t,   D(1),  0,     D(1),  D(1),  M(1),  D(1),  D(1),  D(1)  ),  /* CA1 not after (offset from local time) */
    CU_DataPoints(int32_t,   0,     0,     -D(1), 0,     0,     0,     S(1),  0     ),  /* ID1 not before (offset from local time) */
    CU_DataPoints(int32_t,   D(1),  D(1),  0,     D(1),  M(1),  S(1),  D(1),  S(4)  ),  /* ID1 not after (offset from local time) */
    CU_DataPoints(bool,      false, true,  true,  false, false, true,  false, false ),  /* expect validate local ID1 fail */
    CU_DataPoints(int32_t,   0,     0,     0,     -D(1), 0,     0,     0,     0     ),  /* ID2 not before (offset from local time) */
    CU_DataPoints(int32_t,   D(1),  D(1),  D(1),  0,     D(1),  S(1),  D(1),  D(1)  ),  /* ID2 not after (offset from local time) */
    CU_DataPoints(bool,      false, true,  false, true,  false, true,  false, false ),  /* expect validate local ID2 fail */
    CU_DataPoints(uint32_t,  0,     0,     0,     0,     0,     1100,  1100,  0     ),  /* delay (ms) after generating certificate */
    CU_DataPoints(uint32_t,  1,     0,     0,     0,     1,     0,     1,     10000 ),  /* write/read data during x ms */
    CU_DataPoints(bool,      false, false, false, false, false, false, false, true  ),  /* expect read data failure */
};
CU_Theory(
  (const char * test_descr, int32_t ca_not_before, int32_t ca_not_after,
    int32_t id1_not_before, int32_t id1_not_after, bool id1_local_fail,
    int32_t id2_not_before, int32_t id2_not_after, bool id2_local_fail,
    uint32_t delay, uint32_t write_read_dur, bool exp_read_fail),
  ddssec_authentication, expired_cert, .timeout=30)
{
  print_test_msg ("running test expired_cert: %s\n", test_descr);

  char topic_name[100];
  create_topic_name("ddssec_authentication_", g_topic_nr++, topic_name, sizeof (topic_name));

  print_test_msg ("generate ids (id1: %d-%d, id2: %d-%d):\n", id1_not_before, id1_not_after, id2_not_before, id2_not_after);
  char *ca, *id1, *id2, *id1_subj, *id2_subj;
  ca = generate_ca ("ca1", CA1K, ca_not_before, ca_not_after);
  id1 = generate_identity (ca, CA1K, "id1", ID1K, id1_not_before, id1_not_after, &id1_subj);
  id2 = generate_identity (ca, CA1K, "id2", ID1K, id2_not_before, id2_not_after, &id2_subj);
  dds_sleepfor (DDS_MSECS (delay));

  dds_time_t now = dds_time ();
  char * grants[] = {
    get_permissions_grant ("id1", id1_subj, now - DDS_SECS(D(1)), now + DDS_SECS(D(1)), NULL, NULL, NULL),
    get_permissions_grant ("id2", id2_subj, now - DDS_SECS(D(1)), now + DDS_SECS(D(1)), NULL, NULL, NULL) };
  char * perm_config = get_permissions_config (grants, 2, true);
  authentication_init (id1, ID1K, ca, id2, ID1K, ca, NULL, perm_config, id1_local_fail, id2_local_fail);
  validate_handshake (DDS_DOMAINID1, id1_local_fail, NULL, NULL, NULL);
  validate_handshake (DDS_DOMAINID2, id2_local_fail, NULL, NULL, NULL);
  if (write_read_dur > 0)
  {
    rd_wr_init (g_participant1, &g_pub, &g_pub_tp, &g_wr, g_participant2, &g_sub, &g_sub_tp, &g_rd, topic_name);
    write_read_for (g_wr, g_participant2, g_rd, DDS_MSECS (write_read_dur), false, exp_read_fail);
  }
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

