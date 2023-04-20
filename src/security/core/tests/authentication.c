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
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "ddsi__misc.h"

#include "dds/security/dds_security_api.h"

#include "common/config_env.h"
#include "common/authentication_wrapper.h"
#include "common/test_utils.h"
#include "common/security_config_test_utils.h"
#include "common/test_identity.h"
#include "common/cert_utils.h"

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
    "      ${TRUSTED_CA_DIR:+<TrustedCADir>}${TRUSTED_CA_DIR}${TRUSTED_CA_DIR:+</TrustedCADir>}"
    "      ${CRL:+<CRL>}${CRL}${CRL:+</CRL>}"
    "    </Authentication>"
    "    <AccessControl>"
    "      <Library finalizeFunction=\"finalize_access_control\" initFunction=\"init_access_control\"/>"
    "      <Governance><![CDATA[${GOVERNANCE_CONFIG}]]></Governance>"
    "      <PermissionsCA>file:" COMMON_ETC_PATH("default_permissions_ca.pem") "</PermissionsCA>"
    "      <Permissions><![CDATA[${PERMISSIONS_CONFIG}]]></Permissions>"
    "    </AccessControl>"
    "    <Cryptographic>"
    "      <Library finalizeFunction=\"finalize_crypto\" initFunction=\"init_crypto\"/>"
    "    </Cryptographic>"
    "  </Security>"
    "</Domain>";

static const char *config_non_secure =
    "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}"
    "<Domain id=\"any\">"
    "  <Discovery>"
    "    <ExternalDomainId>0</ExternalDomainId>"
    "    <Tag>\\${CYCLONEDDS_PID}</Tag>"
    "  </Discovery>"
    "</Domain>";


#define DDS_DOMAINID1 0
#define DDS_DOMAINID2 1

#define DEF_PERM_CONF "file:" COMMON_ETC_PATH("default_permissions.p7s")
#define DEF_GOV_CONF "file:" COMMON_ETC_PATH("default_governance.p7s")

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

static void init_domain_pp (bool pp_secure, bool exp_pp_fail,
    dds_domainid_t domain_id, const char * id_cert, const char * id_key, const char * id_ca, const char * gov_config, const char * perm_config, const char * trusted_ca_dir, const char * crl,
    dds_entity_t * domain, dds_entity_t * pp)
{
  char *conf;
  if (pp_secure)
  {
    struct kvp config_vars[] =
    {
      { "TEST_IDENTITY_CERTIFICATE", id_cert, 1 },
      { "TEST_IDENTITY_PRIVATE_KEY", id_key, 1 },
      { "TEST_IDENTITY_CA_CERTIFICATE", id_ca, 1 },
      { "TRUSTED_CA_DIR", trusted_ca_dir, 3 },
      { "CRL", crl, 3 },
      { "PERMISSIONS_CONFIG", perm_config, 1 },
      { "GOVERNANCE_CONFIG", gov_config, 1 },
      { NULL, NULL, 0 }
    };
    conf = ddsrt_expand_vars_sh (config, &expand_lookup_vars_env, config_vars);
    CU_ASSERT_EQUAL_FATAL (expand_lookup_unmatched (config_vars), 0);
  }
  else
  {
    struct kvp config_vars[] = { { NULL, NULL, 0 } };
    conf = ddsrt_expand_vars_sh (config_non_secure, &expand_lookup_vars_env, config_vars);
    CU_ASSERT_EQUAL_FATAL (expand_lookup_unmatched (config_vars), 0);
  }
  *domain = dds_create_domain (domain_id, conf);
  *pp = dds_create_participant (domain_id, NULL, NULL);
  CU_ASSERT_EQUAL_FATAL (exp_pp_fail, *pp <= 0);
  ddsrt_free (conf);
}

static void authentication_init(
    bool pp1_secure, const char * id1_cert, const char * id1_key, const char * id1_ca, bool exp_pp1_fail, const char * gov_conf1, const char * perm_conf1,
    bool pp2_secure, const char * id2_cert, const char * id2_key, const char * id2_ca, bool exp_pp2_fail, const char * gov_conf2, const char * perm_conf2,
    const char * trusted_ca_dir, const char * crl)
{
  init_domain_pp (pp1_secure, exp_pp1_fail, DDS_DOMAINID1, id1_cert, id1_key, id1_ca, gov_conf1, perm_conf1, trusted_ca_dir, crl, &g_domain1, &g_participant1);
  init_domain_pp (pp2_secure, exp_pp2_fail, DDS_DOMAINID2, id2_cert, id2_key, id2_ca, gov_conf2, perm_conf2, trusted_ca_dir, crl, &g_domain2, &g_participant2);
}

static void authentication_fini(bool delete_pp1, bool delete_pp2, void * res[], size_t nres)
{
  if (delete_pp1)
    CU_ASSERT_EQUAL_FATAL (dds_delete (g_participant1), DDS_RETCODE_OK);
  if (delete_pp2)
    CU_ASSERT_EQUAL_FATAL (dds_delete (g_participant2), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL (dds_delete (g_domain1), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL (dds_delete (g_domain2), DDS_RETCODE_OK);
  if (res != NULL)
  {
    for (size_t i = 0; i < nres; i++)
      ddsrt_free (res[i]);
  }
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
/* Test the security handshake result in test-cases using identity CA's that match and do not
   match (i.e. a different CA that was not used for creating the identity) the identities used
   in the participants security configuration. */
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
  authentication_init (
      true, ID1, ID1K, CA1, exp_fail_pp1, DEF_GOV_CONF, DEF_PERM_CONF,
      true, id2, key2, ca2, exp_fail_pp2, DEF_GOV_CONF, DEF_PERM_CONF,
      NULL, NULL);

  // Domain 1
  validate_handshake (DDS_DOMAINID1, false, NULL, &hs_list, &nhs, DDS_SECS(2));
  for (int n = 0; n < nhs; n++)
    validate_handshake_result (&hs_list[n], exp_fail_hs_req, fail_hs_req_msg, exp_fail_hs_reply, fail_hs_reply_msg);
  handshake_list_fini (hs_list, nhs);

  // Domain 2
  validate_handshake (DDS_DOMAINID2, exp_fail_local, fail_local_msg, &hs_list, &nhs, DDS_SECS(2));
  for (int n = 0; n < nhs; n++)
    validate_handshake_result (&hs_list[n], exp_fail_hs_req, fail_hs_req_msg, exp_fail_hs_reply, fail_hs_reply_msg);
  handshake_list_fini (hs_list, nhs);

  authentication_fini (!exp_fail_pp1, !exp_fail_pp2, NULL, 0);
}

CU_TheoryDataPoints(ddssec_authentication, trusted_ca_dir) = {
    CU_DataPoints(const char *, "",    ".",   "/nonexisting", NULL),
    CU_DataPoints(bool,         false, false, true,           false)
};
/* Test correct and incorrect values for the trusted CA directory in the
   authentication plugin configuration */
CU_Theory((const char * ca_dir, bool exp_fail), ddssec_authentication, trusted_ca_dir)
{
  print_test_msg ("Testing custom CA dir: %s\n", ca_dir);
  authentication_init (
      true, ID1, ID1K, CA1, exp_fail, DEF_GOV_CONF, DEF_PERM_CONF,
      true, ID1, ID1K, CA1, exp_fail, DEF_GOV_CONF, DEF_PERM_CONF,
      ca_dir, NULL);
  if (!exp_fail)
  {
    validate_handshake_nofail (DDS_DOMAINID1, DDS_SECS (2));
    validate_handshake_nofail (DDS_DOMAINID2, DDS_SECS (2));
  }
  authentication_fini (!exp_fail, !exp_fail, NULL, 0);
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
/* Test the security handshake result and check communication for scenarios using
   valid identities, identities that are expired and identities that are not yet valid.
   A test case using an identity that expires during the test is also included. */
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

  char * grants[] = {
    get_permissions_default_grant ("id1", id1_subj, topic_name),
    get_permissions_default_grant ("id2", id2_subj, topic_name) };
  char * perm_config = get_permissions_config (grants, 2, true);
  authentication_init (
    true, id1, ID1K, ca, id1_local_fail, DEF_GOV_CONF, perm_config,
    true, id2, ID1K, ca, id2_local_fail, DEF_GOV_CONF, perm_config,
    NULL, NULL);
  validate_handshake (DDS_DOMAINID1, id1_local_fail, NULL, NULL, NULL, DDS_SECS(2));
  validate_handshake (DDS_DOMAINID2, id2_local_fail, NULL, NULL, NULL, DDS_SECS(2));
  if (write_read_dur > 0)
  {
    rd_wr_init (g_participant1, &g_pub, &g_pub_tp, &g_wr, g_participant2, &g_sub, &g_sub_tp, &g_rd, topic_name);
    sync_writer_to_readers(g_participant1, g_wr, 1, dds_time() + DDS_SECS(2));
    write_read_for (g_wr, g_participant2, g_rd, DDS_MSECS (write_read_dur), false, exp_read_fail);
  }
  authentication_fini (!id1_local_fail, !id2_local_fail, (void * []){ grants[0], grants[1], perm_config, ca, id1_subj, id2_subj, id1, id2 }, 8);
}
#undef D
#undef H
#undef M

/* Test communication for a non-secure participant with a secure participant that
   allows unauthenticated nodes in its governance configuration. Samples for a secured
   topic should not be received by a reader in the non-secure participant; samples for
   a non-secure topic should. */
CU_Test(ddssec_authentication, unauthenticated_pp)
{
  char topic_name_secure[100];
  char topic_name_plain[100];
  create_topic_name ("ddssec_authentication_secure_", g_topic_nr++, topic_name_secure, sizeof (topic_name_secure));
  create_topic_name ("ddssec_authentication_plain_", g_topic_nr++, topic_name_plain, sizeof (topic_name_plain));

  /* create ca and id1 cert that will not expire during this test */
  char *ca, *id1, *id1_subj;
  ca = generate_ca ("ca1", TEST_IDENTITY_CA1_PRIVATE_KEY, 0, 3600);
  id1 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id1", TEST_IDENTITY1_PRIVATE_KEY, 0, 3600, &id1_subj);

  char * grants[] = { get_permissions_default_grant ("id1", id1_subj, topic_name_secure) };
  char * perm_config = get_permissions_config (grants, 1, true);

  char * topic_rule_sec = get_governance_topic_rule (topic_name_secure, true, true, true, true, PK_E, BPK_N);
  char * topic_rule_plain = get_governance_topic_rule (topic_name_plain, false, false, false, false, PK_N, BPK_N);
  char * gov_topic_rules;
  ddsrt_asprintf(&gov_topic_rules, "%s%s", topic_rule_sec, topic_rule_plain);
  char * gov_config = get_governance_config (true, true, PK_N, PK_N, PK_N, gov_topic_rules, true);

  authentication_init (
    true, id1, TEST_IDENTITY1_PRIVATE_KEY, ca, false, gov_config, perm_config,
    false, NULL, NULL, NULL, false, NULL, NULL,
    NULL, NULL);

  print_test_msg ("writing sample for plain topic\n");
  dds_entity_t pub, sub, pub_tp, sub_tp, wr, rd;
  rd_wr_init (g_participant1, &pub, &pub_tp, &wr, g_participant2, &sub, &sub_tp, &rd, topic_name_plain);
  sync_writer_to_readers(g_participant1, wr, 1, dds_time() + DDS_SECS(5));
  write_read_for (wr, g_participant2, rd, DDS_MSECS (10), false, false);

  print_test_msg ("writing sample for secured topic\n");
  dds_entity_t spub, ssub, spub_tp, ssub_tp, swr, srd;
  rd_wr_init (g_participant1, &spub, &spub_tp, &swr, g_participant2, &ssub, &ssub_tp, &srd, topic_name_secure);
  sync_writer_to_readers(g_participant1, swr, 0, dds_time() + DDS_SECS(2));
  write_read_for (swr, g_participant2, srd, DDS_MSECS (10), false, true);

  authentication_fini (true, true, (void * []) { gov_config, gov_topic_rules, topic_rule_sec, topic_rule_plain, grants[0], perm_config, ca, id1_subj, id1 }, 9);
}

CU_TheoryDataPoints(ddssec_authentication, crl) = {
    CU_DataPoints(const char *, "",    "/nonexisting", TEST_CRL, NULL),
    CU_DataPoints(bool,         false, true,           true,     false),
};
/* Test correct and incorrect values for the trusted CA directory in the
   authentication plugin configuration */
CU_Theory((const char * crl, bool exp_fail), ddssec_authentication, crl)
{
  print_test_msg ("Testing CRL: %s\n", crl);
  authentication_init (
      true, TEST_REVOKED_IDENTITY_CERTIFICATE, TEST_REVOKED_IDENTITY_PRIVATE_KEY, CA1, exp_fail, DEF_GOV_CONF, DEF_PERM_CONF,
      true, TEST_REVOKED_IDENTITY_CERTIFICATE, TEST_REVOKED_IDENTITY_PRIVATE_KEY, CA1, exp_fail, DEF_GOV_CONF, DEF_PERM_CONF,
      NULL, crl);
  if (!exp_fail)
  {
    validate_handshake_nofail (DDS_DOMAINID1, DDS_SECS (2));
    validate_handshake_nofail (DDS_DOMAINID2, DDS_SECS (2));
  }
  authentication_fini (!exp_fail, !exp_fail, NULL, 0);
}
