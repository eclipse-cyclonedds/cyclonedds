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
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "ddsi__misc.h"

#include "dds/security/dds_security_api.h"

#include "common/config_env.h"
#include "common/cryptography_wrapper.h"
#include "common/test_utils.h"
#include "common/security_config_test_utils.h"
#include "common/test_identity.h"
#include "common/cert_utils.h"

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
    "      <Library finalizeFunction=\"finalize_authentication\" initFunction=\"init_authentication\"/>"
    "      <IdentityCertificate>data:,${TEST_IDENTITY_CERTIFICATE}</IdentityCertificate>"
    "      <PrivateKey>data:,${TEST_IDENTITY_PRIVATE_KEY}</PrivateKey>"
    "      <IdentityCA>data:,${TEST_IDENTITY_CA_CERTIFICATE}</IdentityCA>"
    "    </Authentication>"
    "    <AccessControl>"
    "      <Library finalizeFunction=\"finalize_access_control\" initFunction=\"init_access_control\"/>"
    "      <Governance><![CDATA[${GOVERNANCE_CONFIG}]]></Governance>"
    "      <PermissionsCA>file:" COMMON_ETC_PATH("default_permissions_ca.pem") "</PermissionsCA>"
    "      <Permissions><![CDATA[${PERMISSIONS_CONFIG}]]></Permissions>"
    "    </AccessControl>"
    "    <Cryptographic>"
    "      <Library initFunction=\"${CRYPTO_INIT:-init_test_cryptography_wrapped}\" finalizeFunction=\"${CRYPTO_FINI:-finalize_test_cryptography_wrapped}\" path=\"" WRAPPERLIB_PATH("dds_security_cryptography_wrapper") "\"/>"
    "    </Cryptographic>"
    "  </Security>"
    "</Domain>";

#define DDS_DOMAINID1 0
#define DDS_DOMAINID2 1

static dds_entity_t g_domain1;
static dds_entity_t g_participant1;
static dds_entity_t g_domain2;
static dds_entity_t g_participant2;

static uint32_t g_topic_nr = 0;

static void init_domain_pp (dds_domainid_t domain_id, const char *id_cert, const char * id_key, const char * id_ca,
    const char * gov_config, const char * perm_config, const char * crypto_init, const char * crypto_fini, dds_entity_t *domain, dds_entity_t *pp)
{
  struct kvp config_vars[] =
  {
    { "TEST_IDENTITY_CERTIFICATE", id_cert, 1 },
    { "TEST_IDENTITY_PRIVATE_KEY", id_key, 1 },
    { "TEST_IDENTITY_CA_CERTIFICATE", id_ca, 1 },
    { "PERMISSIONS_CONFIG", perm_config, 1 },
    { "GOVERNANCE_CONFIG", gov_config, 1 },
    { "CRYPTO_INIT", crypto_init, 1 },
    { "CRYPTO_FINI", crypto_fini, 1 },
    { NULL, NULL, 0 }
  };
  char *conf = ddsrt_expand_vars_sh (config, &expand_lookup_vars_env, config_vars);
  CU_ASSERT_EQUAL_FATAL (expand_lookup_unmatched (config_vars), 0);
  *domain = dds_create_domain (domain_id, conf);
  *pp = dds_create_participant (domain_id, NULL, NULL);
  CU_ASSERT_FATAL (*pp > 0);
  ddsrt_free (conf);
}

static void crypto_init (
    const char * gov_config1, const char * perm_config1, const char * id_cert1, const char * id_key1, const char * crypto_init1, const char * crypto_fini1,
    const char * gov_config2, const char * perm_config2, const char * id_cert2, const char * id_key2, const char * crypto_init2, const char * crypto_fini2,
    const char * id_ca)
{
  init_domain_pp (DDS_DOMAINID1, id_cert1, id_key1, id_ca, gov_config1, perm_config1, crypto_init1, crypto_fini1, &g_domain1, &g_participant1);
  init_domain_pp (DDS_DOMAINID2, id_cert2, id_key2, id_ca, gov_config2, perm_config2, crypto_init2, crypto_fini2, &g_domain2, &g_participant2);
}

static void crypto_fini (void * res[], size_t nres)
{
  CU_ASSERT_EQUAL_FATAL (dds_delete (g_domain1), DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL (dds_delete (g_domain2), DDS_RETCODE_OK);
  if (res != NULL)
  {
    for (size_t i = 0; i < nres; i++)
      ddsrt_free (res[i]);
  }
}

CU_TheoryDataPoints(ddssec_crypto, inject_plain_data) = {
    CU_DataPoints(const char *,
    /*                                             */"payload encrypt",
    /*                                              |     */"payload sign",
    /*                                              |      |     */"submessage encrypt",
    /*                                              |      |      |     */"submessage sign",
    /*                                              |      |      |      |     */"rtps encrypt",
    /*                                              |      |      |      |      |     */"rtps sign"),
    CU_DataPoints(DDS_Security_BasicProtectionKind, BPK_E, BPK_S, BPK_N, BPK_N, BPK_N, BPK_N),  /* payload protection */
    CU_DataPoints(DDS_Security_ProtectionKind,      PK_N,  PK_N,  PK_E,  PK_S,  PK_N,  PK_N),   /* submessage protection */
    CU_DataPoints(DDS_Security_ProtectionKind,      PK_N,  PK_N,  PK_N,  PK_N,  PK_E,  PK_S),   /* rtps protection */
};
/* This test validates that non-encrypted data will not be received by a reader that has protection
   enabled for rtps/submsg/payload. The test uses a crypto plugin wrapper mode that force the plugin
   to write plain data in the encoded output buffer to DDSI, ignoring the security attributes for the
   reader and writer. */
CU_Theory((const char * test_descr, DDS_Security_BasicProtectionKind payload_pk, DDS_Security_ProtectionKind submsg_pk, DDS_Security_ProtectionKind rtps_pk),
  ddssec_crypto, inject_plain_data, .timeout=30)
{
  print_test_msg ("running test inject_plain_data: %s\n", test_descr);

  char topic_name[100];
  create_topic_name ("ddssec_crypto_", g_topic_nr++, topic_name, sizeof (topic_name));

  char *ca, *id1, *id1_subj, *id2, *id2_subj;
  ca = generate_ca ("ca1", TEST_IDENTITY_CA1_PRIVATE_KEY, 0, 3600);
  id1 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id1", TEST_IDENTITY1_PRIVATE_KEY, 0, 3600, &id1_subj);
  id2 = generate_identity (ca, TEST_IDENTITY_CA1_PRIVATE_KEY, "id2", TEST_IDENTITY1_PRIVATE_KEY, 0, 3600, &id2_subj);

  char * grants[] = {
      get_permissions_default_grant ("id1", id1_subj, topic_name),
      get_permissions_default_grant ("id2", id2_subj, topic_name) };
  char * perm_config = get_permissions_config (grants, 2, true);

  char * gov_topic_rule = get_governance_topic_rule (topic_name, false, false, false, false, submsg_pk, payload_pk);
  char * gov_config = get_governance_config (false, true, PK_N, PK_N, rtps_pk, gov_topic_rule, true);

  crypto_init (
    gov_config, perm_config, id1, TEST_IDENTITY1_PRIVATE_KEY, "init_test_cryptography_plain_data", "finalize_test_cryptography_plain_data",
    gov_config, perm_config, id2, TEST_IDENTITY1_PRIVATE_KEY, "init_test_cryptography_wrapped", "finalize_test_cryptography_wrapped",
    ca);

  dds_entity_t pub, sub, pub_tp, sub_tp, wr, rd;
  rd_wr_init (g_participant1, &pub, &pub_tp, &wr, g_participant2, &sub, &sub_tp, &rd, topic_name);

  /* set forced plain data for payload/submsg/rtps */
  DDS_Security_DatawriterCryptoHandle wr_handle = get_writer_crypto_handle (wr);
  struct dds_security_cryptography_impl * crypto_impl = get_cryptography_context (g_participant1);
  set_force_plain_data (crypto_impl, wr_handle, rtps_pk != PK_N, submsg_pk != PK_N, payload_pk != BPK_N);

  /* sync and write/take sample */
  sync_writer_to_readers (g_participant1, wr, 1, dds_time() + DDS_SECS (2));
  write_read_for (wr, g_participant2, rd, DDS_MSECS (10), false, true);

  /* reset forced plain data */
  set_force_plain_data (crypto_impl, wr_handle, false, false, false);

  crypto_fini ((void * []) { gov_config, gov_topic_rule, grants[0], grants[1], perm_config, ca, id1_subj, id1, id2_subj, id2 }, 10);
}
