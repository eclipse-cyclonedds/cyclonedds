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
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "ddsi__misc.h"
#include "dds/security/dds_security_api.h"

#include "common/config_env.h"
#include "common/test_identity.h"
#include "common/test_utils.h"
#include "common/security_config_test_utils.h"
#include "common/cryptography_wrapper.h"

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
    "      <Library finalizeFunction=\"finalize_authentication\" initFunction=\"init_authentication\" />"
    "      <IdentityCertificate>data:,"TEST_IDENTITY1_CERTIFICATE"</IdentityCertificate>"
    "      <PrivateKey>data:,"TEST_IDENTITY1_PRIVATE_KEY"</PrivateKey>"
    "      <IdentityCA>data:,"TEST_IDENTITY_CA1_CERTIFICATE"</IdentityCA>"
    "      <Password></Password>"
    "      <TrustedCADirectory>.</TrustedCADirectory>"
    "    </Authentication>"
    "    <AccessControl>"
    "      <Library finalizeFunction=\"finalize_access_control\" initFunction=\"init_access_control\"/>"
    "      <Governance><![CDATA[data:,${GOVERNANCE_DATA}]]></Governance>"
    "      <PermissionsCA>file:" COMMON_ETC_PATH("default_permissions_ca.pem") "</PermissionsCA>"
    "      <Permissions>file:" COMMON_ETC_PATH("default_permissions.p7s") "</Permissions>"
    "    </AccessControl>"
    "    <Cryptographic>"
    "      <Library finalizeFunction=\"finalize_test_cryptography_wrapped\" initFunction=\"init_test_cryptography_wrapped\" path=\"" WRAPPERLIB_PATH("dds_security_cryptography_wrapper") "\"/>"
    "    </Cryptographic>"
    "  </Security>"
    "</Domain>";

#define DDS_DOMAINID_PUB 0
#define DDS_DOMAINID_SUB 10

#define MAX_DOMAINS 10
#define MAX_PARTICIPANTS 10

uint32_t g_topic_nr = 0;

static dds_entity_t g_pub_domains[MAX_DOMAINS];
static dds_entity_t g_pub_participants[MAX_DOMAINS * MAX_PARTICIPANTS];
static dds_entity_t g_pub_publishers[MAX_DOMAINS * MAX_PARTICIPANTS];

static dds_entity_t g_sub_domains[MAX_DOMAINS];
static dds_entity_t g_sub_participants[MAX_DOMAINS * MAX_PARTICIPANTS];
static dds_entity_t g_sub_subscribers[MAX_DOMAINS * MAX_PARTICIPANTS];

struct domain_sec_config {
  DDS_Security_ProtectionKind discovery_pk;
  DDS_Security_ProtectionKind liveliness_pk;
  DDS_Security_ProtectionKind rtps_pk;
  DDS_Security_ProtectionKind metadata_pk;
  DDS_Security_BasicProtectionKind payload_pk;
  const char * payload_secret;
  const char * pp_userdata_secret;
  const char * groupdata_secret;
  const char * ep_userdata_secret;
};

typedef void (*set_crypto_params_fn)(struct dds_security_cryptography_impl *, const struct domain_sec_config *);
typedef dds_entity_t (*pubsub_create_fn)(dds_entity_t, const dds_qos_t *qos, const dds_listener_t *listener);
typedef dds_entity_t (*ep_create_fn)(dds_entity_t, dds_entity_t, const dds_qos_t *qos, const dds_listener_t *listener);

const char * g_pp_secret = "ppsecret";
const char * g_groupdata_secret = "groupsecret";
const char * g_ep_secret = "epsecret";

static dds_qos_t *get_qos(void)
{
  dds_qos_t * qos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, -1);
  dds_qset_durability (qos, DDS_DURABILITY_TRANSIENT_LOCAL);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  dds_qset_userdata (qos, g_ep_secret, strlen (g_ep_secret));
  return qos;
}

static dds_entity_t create_pp (dds_domainid_t domain_id, const struct domain_sec_config * domain_config, set_crypto_params_fn set_crypto_params)
{
  dds_qos_t *qos = dds_create_qos ();
  dds_qset_userdata (qos, g_pp_secret, strlen (g_pp_secret));
  dds_entity_t pp = dds_create_participant (domain_id, qos, NULL);
  CU_ASSERT_FATAL (pp > 0);
  dds_delete_qos (qos);
  struct dds_security_cryptography_impl * crypto_context = get_cryptography_context (pp);
  CU_ASSERT_FATAL (crypto_context != NULL);
  assert (set_crypto_params);
  set_crypto_params (crypto_context, domain_config);
  return pp;
}


static void create_dom_pp_pubsub(dds_domainid_t domain_id_base, const char * domain_conf, const struct domain_sec_config * domain_sec_config,
    size_t n_dom, size_t n_pp, dds_entity_t * doms, dds_entity_t * pps, dds_entity_t * pubsubs, pubsub_create_fn pubsub_create, set_crypto_params_fn set_crypto_params)
{
  for (size_t d = 0; d < n_dom; d++)
  {
    doms[d] = dds_create_domain (domain_id_base + (uint32_t)d, domain_conf);
    CU_ASSERT_FATAL (doms[d] > 0);
    for (size_t p = 0; p < n_pp; p++)
    {
      size_t pp_index = d * n_pp + p;
      pps[pp_index] = create_pp (domain_id_base + (uint32_t)d, domain_sec_config, set_crypto_params);
      dds_qos_t *qos = dds_create_qos ();
      dds_qset_groupdata (qos, g_groupdata_secret, strlen (g_groupdata_secret));
      pubsubs[pp_index] = pubsub_create (pps[pp_index], qos, NULL);
      CU_ASSERT_FATAL (pubsubs[pp_index] > 0);
      dds_delete_qos (qos);
    }
  }
}

static void test_init(const struct domain_sec_config * domain_config, size_t n_sub_domains, size_t n_sub_participants, size_t n_pub_domains, size_t n_pub_participants, set_crypto_params_fn set_crypto_params)
{
  assert (n_sub_domains < MAX_DOMAINS);
  assert (n_sub_participants < MAX_PARTICIPANTS);
  assert (n_pub_domains < MAX_DOMAINS);
  assert (n_pub_participants < MAX_PARTICIPANTS);

  char * gov_topic_rule = get_governance_topic_rule ("*", true, true, true, true, domain_config->metadata_pk, domain_config->payload_pk);
  char * gov_config_signed = get_governance_config (false, true, domain_config->discovery_pk, domain_config->liveliness_pk, domain_config->rtps_pk, gov_topic_rule, false);

  struct kvp config_vars[] = {
    { "GOVERNANCE_DATA", gov_config_signed, 1 },
    { NULL, NULL, 0 }
  };

  char *conf_pub = ddsrt_expand_vars_sh (config, &expand_lookup_vars_env, config_vars);
  create_dom_pp_pubsub (DDS_DOMAINID_PUB, conf_pub, domain_config, n_pub_domains, n_pub_participants,
      g_pub_domains, g_pub_participants, g_pub_publishers, &dds_create_publisher, set_crypto_params);
  dds_free (conf_pub);

  char *conf_sub = ddsrt_expand_vars_sh (config, &expand_lookup_vars_env, config_vars);
  create_dom_pp_pubsub (DDS_DOMAINID_SUB, conf_sub, domain_config, n_sub_domains, n_sub_participants,
      g_sub_domains, g_sub_participants, g_sub_subscribers, &dds_create_subscriber, set_crypto_params);
  dds_free (conf_sub);

  dds_free (gov_config_signed);
  dds_free (gov_topic_rule);
}

static void test_fini(size_t n_sub_domain, size_t n_pub_domain)
{
  dds_return_t ret;
  for (size_t d = 0; d < n_pub_domain; d++)
  {
    ret = dds_delete (g_pub_domains[d]);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  }
  for (size_t d = 0; d < n_sub_domain; d++)
  {
    ret = dds_delete (g_sub_domains[d]);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  }
  printf("Test finished\n");
}

static void create_eps (dds_entity_t **endpoints, dds_entity_t **topics, size_t n_dom, size_t n_pp, size_t n_eps, const char * topic_name, const dds_topic_descriptor_t *topic_descriptor,
    const dds_entity_t * pps, const dds_qos_t * qos, ep_create_fn ep_create, unsigned status_mask)
{
  *topics = ddsrt_malloc (n_dom * n_pp * sizeof (dds_entity_t));
  *endpoints = ddsrt_malloc (n_dom * n_pp * n_eps * sizeof (dds_entity_t));
  for (size_t d = 0; d < n_dom; d++)
  {
    for (size_t p = 0; p < n_pp; p++)
    {
      size_t pp_index = d * n_pp + p;
      (*topics)[pp_index] = dds_create_topic (pps[pp_index], topic_descriptor, topic_name, NULL, NULL);
      CU_ASSERT_FATAL ((*topics)[pp_index] > 0);
      for (size_t e = 0; e < n_eps; e++)
      {
        size_t ep_index = pp_index * n_eps + e;
        (*endpoints)[ep_index] = ep_create (pps[pp_index], (*topics)[pp_index], qos, NULL);
        CU_ASSERT_FATAL ((*endpoints)[ep_index] > 0);
        dds_return_t ret = dds_set_status_mask ((*endpoints)[ep_index], status_mask);
        CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
      }
    }
  }
}

static void free_eps(dds_entity_t *endpoints, dds_entity_t *topics)
{
  ddsrt_free (endpoints);
  ddsrt_free (topics);
}

static void test_write_read(struct domain_sec_config *domain_config,
    size_t n_sub_domains, size_t n_sub_participants, size_t n_readers,
    size_t n_pub_domains, size_t n_pub_participants, size_t n_writers,
    set_crypto_params_fn set_crypto_params)
{
  dds_entity_t *writers, *readers, *writer_topics, *reader_topics;
  dds_qos_t *qos;
  SecurityCoreTests_Type1 sample = { 0, 1 };
  SecurityCoreTests_Type1 rd_sample;
  void * samples[] = { &rd_sample };
  dds_sample_info_t info[1];
  dds_return_t ret;
  char name[100];

  printf("Testing: %"PRIuSIZE" subscriber domains, %"PRIuSIZE" pp per domain, %"PRIuSIZE" rd per pp; %"PRIuSIZE" publishing domains, %"PRIuSIZE" pp per domain, %"PRIuSIZE" wr per pp\n",
      n_sub_domains, n_sub_participants, n_readers, n_pub_domains, n_pub_participants, n_writers);
  test_init(domain_config, n_sub_domains, n_sub_participants, n_pub_domains, n_pub_participants, set_crypto_params);

  create_topic_name("ddssec_secure_communication_", g_topic_nr++, name, sizeof name);

  qos = get_qos ();
  create_eps (&writers, &writer_topics, n_pub_domains, n_pub_participants, n_writers, name, &SecurityCoreTests_Type1_desc, g_pub_participants, qos, &dds_create_writer, DDS_PUBLICATION_MATCHED_STATUS);
  create_eps (&readers, &reader_topics, n_sub_domains, n_sub_participants, n_readers, name, &SecurityCoreTests_Type1_desc, g_sub_participants, qos, &dds_create_reader, DDS_DATA_AVAILABLE_STATUS);

  const dds_time_t sync_abstimeout = dds_time() + DDS_SECS(5);
  for (size_t d = 0; d < n_pub_domains; d++)
  {
    for (size_t p = 0; p < n_pub_participants; p++)
    {
      size_t pp_index = d * n_pub_participants + p;
      for (size_t w = 0; w < n_writers; w++)
      {
        size_t wr_index = pp_index * n_writers + w;
        sync_writer_to_readers (g_pub_participants[pp_index], writers[wr_index], (uint32_t)(n_sub_domains * n_sub_participants * n_readers), sync_abstimeout);
        sample.id = (int32_t) wr_index;
        printf("writer %"PRId32" writing sample %d\n", writers[wr_index], sample.id);
        ret = dds_write (writers[wr_index], &sample);
        CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
      }
    }
  }

  for (size_t d = 0; d < n_sub_domains; d++)
  {
    for (size_t p = 0; p < n_sub_participants; p++)
    {
      size_t pp_index = d * n_sub_participants + p;
      for (size_t r = 0; r < n_readers; r++)
      {
        size_t rd_index = pp_index * n_readers + r;
        size_t n_samples = n_pub_domains * n_pub_participants * n_writers;
        while (n_samples > 0)
        {
          ret = dds_take (readers[rd_index], samples, info, 1, 1);
          if (ret == 0)
          {
            reader_wait_for_data (g_sub_participants[pp_index], readers[rd_index], DDS_SECS(5));
            continue;
          }
          printf("reader %"PRId32" received sample %d\n", readers[rd_index], rd_sample.id);
          CU_ASSERT_EQUAL_FATAL (ret, 1);
          CU_ASSERT_EQUAL_FATAL (rd_sample.value, 1);
          n_samples--;
        }
      }
    }
  }

  /* Cleanup */
  dds_delete_qos (qos);
  test_fini (n_sub_domains, n_pub_domains);
  free_eps (readers, reader_topics);
  free_eps (writers, writer_topics);
}

static void set_encryption_parameters_basic(struct dds_security_cryptography_impl * crypto_context, const struct domain_sec_config *domain_config)
{
  set_protection_kinds (crypto_context, domain_config->rtps_pk, domain_config->metadata_pk, domain_config->payload_pk);
}

static void set_encryption_parameters_secret(struct dds_security_cryptography_impl * crypto_context, const struct domain_sec_config *domain_config)
{
  set_encrypted_secret (crypto_context, domain_config->payload_secret);
  set_encryption_parameters_basic (crypto_context, domain_config);
}

static void set_encryption_parameters_disc(struct dds_security_cryptography_impl * crypto_context, const struct domain_sec_config *domain_config)
{
  set_entity_data_secret (crypto_context, domain_config->pp_userdata_secret, domain_config->groupdata_secret, domain_config->ep_userdata_secret);
  set_encryption_parameters_basic (crypto_context, domain_config);
  set_disc_protection_kinds (crypto_context, domain_config->discovery_pk, domain_config->liveliness_pk);
}

static void test_discovery_liveliness_protection(DDS_Security_ProtectionKind discovery_pk, DDS_Security_ProtectionKind liveliness_pk)
{
  struct domain_sec_config domain_config = { discovery_pk, liveliness_pk, PK_N, PK_N, BPK_N, NULL };
  /* FIXME: add more asserts in wrapper or test instead of just testing communication */
  test_write_read (&domain_config, 1, 1, 1, 1, 1, 1, set_encryption_parameters_disc);
}

static void test_data_protection_kind(DDS_Security_ProtectionKind rtps_pk, DDS_Security_ProtectionKind metadata_pk, DDS_Security_BasicProtectionKind payload_pk)
{
  struct domain_sec_config domain_config = { PK_N, PK_N, rtps_pk, metadata_pk, payload_pk, NULL };
  test_write_read (&domain_config, 1, 1, 1, 1, 1, 1, set_encryption_parameters_basic);
}

static void test_multiple_readers(size_t n_dom, size_t n_pp, size_t n_rd, DDS_Security_ProtectionKind metadata_pk, DDS_Security_BasicProtectionKind payload_pk)
{
  struct domain_sec_config domain_config = { PK_N, PK_N, PK_N, metadata_pk, payload_pk, NULL };
  test_write_read (&domain_config, n_dom, n_pp, n_rd, 1, 1, 1, set_encryption_parameters_basic);
}

static void test_multiple_writers(size_t n_rd_dom, size_t n_rd, size_t n_wr_dom, size_t n_wr, DDS_Security_ProtectionKind metadata_pk)
{
  struct domain_sec_config domain_config = { PK_N, PK_N, PK_N, metadata_pk, BPK_N, NULL };
  test_write_read (&domain_config, n_rd_dom, 1, n_rd, n_wr_dom, 1, n_wr, set_encryption_parameters_basic);
}

static void test_payload_secret(DDS_Security_ProtectionKind rtps_pk, DDS_Security_ProtectionKind metadata_pk, DDS_Security_BasicProtectionKind payload_pk)
{
  dds_entity_t *writers, *readers, *writer_topics, *reader_topics;
  const char * secret = "my_test_secret";
  dds_qos_t *qos;
  SecurityCoreTests_Type2 sample;
  SecurityCoreTests_Type2 rd_sample = {0, NULL};
  void * samples[] = { &rd_sample };
  dds_sample_info_t info[1];
  dds_return_t ret;
  char name[100];
  struct domain_sec_config domain_config = { PK_N, PK_N, rtps_pk, metadata_pk, payload_pk, secret };

  size_t payload_sz = 100 * strlen (secret) + 1;
  sample.id = 1;
  sample.text = ddsrt_malloc (payload_sz);
  for (size_t n = 0; n < 100; n++)
    memcpy (sample.text + n * strlen (secret), secret, strlen (secret));
  sample.text[payload_sz - 1] = '\0';

  test_init (&domain_config, 1, 1, 1, 1, set_encryption_parameters_secret);
  create_topic_name ("ddssec_secure_communication_", g_topic_nr++, name, sizeof name);
  qos = get_qos ();
  create_eps (&writers, &writer_topics, 1, 1, 1, name, &SecurityCoreTests_Type2_desc, g_pub_participants, qos, &dds_create_writer, DDS_PUBLICATION_MATCHED_STATUS);
  create_eps (&readers, &reader_topics, 1, 1, 1, name, &SecurityCoreTests_Type2_desc, g_sub_participants, qos, &dds_create_reader, DDS_DATA_AVAILABLE_STATUS);
  dds_delete_qos (qos);
  sync_writer_to_readers (g_pub_participants[0], writers[0], 1, dds_time() + DDS_SECS(2));
  ret = dds_write (writers[0], &sample);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

  while (true)
  {
    if ((ret = dds_take (readers[0], samples, info, 1, 1)) == 0)
    {
      reader_wait_for_data (g_sub_participants[0], readers[0], DDS_SECS(5));
      continue;
    }
    CU_ASSERT_EQUAL_FATAL (ret, 1);
    break;
  }

  test_fini (1, 1);
  free_eps (readers, reader_topics);
  free_eps (writers, writer_topics);
  ddsrt_free (rd_sample.text);
  ddsrt_free (sample.text);
}

/* Test communication between 2 nodes for all combinations of RTPS, metadata (submsg)
   and payload protection kinds using a single reader and writer */
CU_Test(ddssec_secure_communication, protection_kinds, .timeout = 120)
{
  DDS_Security_ProtectionKind rtps_pk[] = { PK_N, PK_S, PK_E };
  DDS_Security_ProtectionKind metadata_pk[] = { PK_N, PK_S, PK_E };
  DDS_Security_BasicProtectionKind payload_pk[] = { BPK_N, BPK_S, BPK_E };
  for (size_t rtps = 0; rtps < sizeof (rtps_pk) / sizeof (rtps_pk[0]); rtps++)
  {
    for (size_t metadata = 0; metadata < sizeof (metadata_pk) / sizeof (metadata_pk[0]); metadata++)
    {
      for (size_t payload = 0; payload < sizeof (payload_pk) / sizeof (payload_pk[0]); payload++)
      {
        test_data_protection_kind (rtps_pk[rtps], metadata_pk[metadata], payload_pk[payload]);
      }
    }
  }
}

/* Test communication between 2 nodes for all combinations of discovery and
   liveliness protection kinds using a single reader and writer */
CU_Test(ddssec_secure_communication, discovery_liveliness_protection, .timeout = 60)
{
  DDS_Security_ProtectionKind discovery_pk[] = { PK_N, PK_S, PK_E };
  DDS_Security_ProtectionKind liveliness_pk[] = { PK_N, PK_S, PK_E };
  for (size_t disc = 0; disc < sizeof (discovery_pk) / sizeof (discovery_pk[0]); disc++)
  {
    for (size_t liveliness = 0; liveliness < sizeof (liveliness_pk) / sizeof (liveliness_pk[0]); liveliness++)
    {
      test_discovery_liveliness_protection (discovery_pk[disc], liveliness_pk[liveliness]);
    }
  }
}

/* Test that a specific character sequence from the plain data does not appear in
   encrypted payload, submessage or rtps message when protection kind is ENCRYPT*/
CU_Test(ddssec_secure_communication, check_encrypted_secret, .timeout = 60)
{
  DDS_Security_ProtectionKind rtps_pk[] = { PK_N, PK_E, PK_EOA };
  DDS_Security_ProtectionKind metadata_pk[] = { PK_N, PK_E, PK_EOA };
  DDS_Security_BasicProtectionKind payload_pk[] = { BPK_N, BPK_E };
  for (size_t rtps = 0; rtps < sizeof (rtps_pk) / sizeof (rtps_pk[0]); rtps++)
  {
    for (size_t metadata = 0; metadata < sizeof (metadata_pk) / sizeof (metadata_pk[0]); metadata++)
    {
      for (size_t payload = 0; payload < sizeof (payload_pk) / sizeof (payload_pk[0]); payload++)
      {
        test_payload_secret (rtps_pk[rtps], metadata_pk[metadata], payload_pk[payload]);
      }
    }
  }
}

/* Test communication with specific combinations payload and submsg protection
   kinds for 1-3 domains, 1-3 participants per domain and 1-3 readers per participant */
CU_TheoryDataPoints(ddssec_secure_communication, multiple_readers) = {
    CU_DataPoints(size_t, 1, 1, 1, 3), /* number of domains */
    CU_DataPoints(size_t, 1, 3, 1, 3), /* number of participants per domain */
    CU_DataPoints(size_t, 3, 1, 3, 3), /* number of readers per participant */
};
CU_Theory((size_t n_dom, size_t n_pp, size_t n_rd), ddssec_secure_communication, multiple_readers, .timeout = 90, .disabled = false)
{
  DDS_Security_ProtectionKind metadata_pk[] = { PK_N, PK_SOA, PK_EOA };
  DDS_Security_BasicProtectionKind payload_pk[] = { BPK_N, BPK_S, BPK_E };
  for (size_t metadata = 0; metadata < sizeof (metadata_pk) / sizeof (metadata_pk[0]); metadata++)
  {
    for (size_t payload = 0; payload < sizeof (payload_pk) / sizeof (payload_pk[0]); payload++)
    {
      test_multiple_readers (n_dom, n_pp, n_rd, metadata_pk[metadata], payload_pk[payload]);
    }
  }
}

/* Test communication with specific combinations payload and submsg protection
   kinds for 1-2 domains, 1-3 participants per domain, 1-3 readers per participant
   and 1-3 writers per participant */
CU_TheoryDataPoints(ddssec_secure_communication, multiple_readers_writers) = {
    CU_DataPoints(size_t, 1, 1, 2), /* number of reader domains */
    CU_DataPoints(size_t, 1, 3, 3), /* number of readers per domain */
    CU_DataPoints(size_t, 1, 1, 2), /* number of writer domains */
    CU_DataPoints(size_t, 1, 3, 3), /* number of writers per domain */
};
CU_Theory((size_t n_rd_dom, size_t n_rd, size_t n_wr_dom, size_t n_wr), ddssec_secure_communication, multiple_readers_writers, .timeout = 60, .disabled = false)
{
  DDS_Security_ProtectionKind metadata_pk[] = { PK_SOA, PK_EOA };
  for (size_t metadata = 0; metadata < sizeof (metadata_pk) / sizeof (metadata_pk[0]); metadata++)
  {
    test_multiple_writers (n_rd_dom, n_rd, n_wr_dom, n_wr, metadata_pk[metadata]);
  }
}
