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

#include "dds/version.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "ddsi__misc.h"

#include "dds/security/dds_security_api.h"

#include "common/config_env.h"
#include "common/authentication_wrapper.h"
#include "common/cryptography_wrapper.h"
#include "common/plugin_wrapper_msg_q.h"
#include "common/test_utils.h"
#include "common/test_identity.h"
#include "common/security_config_test_utils.h"

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
    "      <Library initFunction=\"${AUTH_INIT}\" finalizeFunction=\"${AUTH_FINI}\" path=\"" WRAPPERLIB_PATH("dds_security_authentication_wrapper") "\"/>"
    "      <IdentityCertificate>data:,"TEST_IDENTITY1_CERTIFICATE"</IdentityCertificate>"
    "      <PrivateKey>data:,"TEST_IDENTITY1_PRIVATE_KEY"</PrivateKey>"
    "      <IdentityCA>data:,"TEST_IDENTITY_CA1_CERTIFICATE"</IdentityCA>"
    "    </Authentication>"
    "    <AccessControl>"
    "      <Library finalizeFunction=\"finalize_access_control\" initFunction=\"init_access_control\"/>"
    "      <Governance>file:" COMMON_ETC_PATH("default_governance.p7s") "</Governance>"
    "      <PermissionsCA>file:" COMMON_ETC_PATH("default_permissions_ca.pem") "</PermissionsCA>"
    "      <Permissions>file:" COMMON_ETC_PATH("default_permissions.p7s") "</Permissions>"
    "    </AccessControl>"
    "    <Cryptographic>"
    "      <Library initFunction=\"${CRYPTO_INIT}\" finalizeFunction=\"${CRYPTO_FINI}\" path=\"" WRAPPERLIB_PATH("dds_security_cryptography_wrapper") "\"/>"
    "    </Cryptographic>"
    "  </Security>"
    "</Domain>";

#define DDS_DOMAINID1 0
#define DDS_DOMAINID2 1

static dds_entity_t g_domain1 = 0;
static dds_entity_t g_participant1 = 0;

static dds_entity_t g_domain2 = 0;
static dds_entity_t g_participant2 = 0;

static uint32_t g_topic_nr = 0;
static dds_entity_t g_pub = 0, g_pub_tp = 0, g_wr = 0, g_sub = 0, g_sub_tp = 0, g_rd = 0;

static void handshake_init(const char * auth_init, const char * auth_fini, const char * crypto_init, const char * crypto_fini)
{
  struct kvp config_vars[] = {
    { "AUTH_INIT", auth_init, 1},
    { "AUTH_FINI", auth_fini, 1},
    { "CRYPTO_INIT", crypto_init, 1 },
    { "CRYPTO_FINI", crypto_fini, 1 },
    { NULL, NULL, 0 }
  };

  char *conf = ddsrt_expand_vars_sh (config, &expand_lookup_vars_env, config_vars);
  int32_t unmatched = expand_lookup_unmatched (config_vars);
  CU_ASSERT_EQUAL_FATAL (unmatched, 0);
  g_domain1 = dds_create_domain (DDS_DOMAINID1, conf);
  g_domain2 = dds_create_domain (DDS_DOMAINID2, conf);
  dds_free (conf);

  g_participant1 = dds_create_participant (DDS_DOMAINID1, NULL, NULL);
  CU_ASSERT_FATAL (g_participant1 > 0);
  g_participant2 = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
  CU_ASSERT_FATAL (g_participant2 > 0);
}

static void handshake_fini(void)
{
  dds_return_t ret = dds_delete (g_domain1);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_delete (g_domain2);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
}

/* Happy-day test for the security handshake, that tests succesfull handshake for
   two participants using the same typical security settings. */
CU_Test(ddssec_handshake, happy_day)
{
  struct Handshake *hs_list;
  int nhs;

  handshake_init (
    "init_test_authentication_wrapped", "finalize_test_authentication_wrapped",
    "init_test_cryptography_wrapped", "finalize_test_cryptography_wrapped");

  validate_handshake (DDS_DOMAINID1, false, NULL, &hs_list, &nhs, DDS_SECS(2));
  CU_ASSERT_EQUAL_FATAL (nhs, 1);
  for (int n = 0; n < nhs; n++)
    validate_handshake_result (&hs_list[n], false, NULL, false, NULL);
  handshake_list_fini (hs_list, nhs);

  validate_handshake (DDS_DOMAINID2, false, NULL, &hs_list, &nhs, DDS_SECS(2));
  CU_ASSERT_EQUAL_FATAL (nhs, 1);
  for (int n = 0; n < nhs; n++)
    validate_handshake_result (&hs_list[n], false, NULL, false, NULL);
  handshake_list_fini (hs_list, nhs);

  handshake_fini ();
}

/* This test checks that all tokens that are sent to a remote participant are received
   correctly by that participant and the token-data stored in the remote participant
   is equal to the data in the token that was sent. */
CU_Test(ddssec_handshake, check_tokens)
{
  handshake_init (
    "init_test_authentication_wrapped", "finalize_test_authentication_wrapped",
    "init_test_cryptography_store_tokens", "finalize_test_cryptography_store_tokens");
  validate_handshake_nofail (DDS_DOMAINID1, DDS_SECS (2));
  validate_handshake_nofail (DDS_DOMAINID2, DDS_SECS (2));

  char topic_name[100];
  create_topic_name("ddssec_authentication_", g_topic_nr++, topic_name, sizeof (topic_name));
  rd_wr_init (g_participant1, &g_pub, &g_pub_tp, &g_wr, g_participant2, &g_sub, &g_sub_tp, &g_rd, topic_name);
  write_read_for (g_wr, g_participant2, g_rd, DDS_MSECS (100), false, false);

  // Get subscriber and publisher crypto tokens
  struct dds_security_cryptography_impl * crypto_context_pub = get_cryptography_context (g_participant1);
  CU_ASSERT_FATAL (crypto_context_pub != NULL);
  struct ddsrt_circlist *pub_tokens = get_crypto_tokens (crypto_context_pub);

  struct dds_security_cryptography_impl * crypto_context_sub = get_cryptography_context (g_participant2);
  CU_ASSERT_FATAL (crypto_context_sub != NULL);
  struct ddsrt_circlist *sub_tokens = get_crypto_tokens (crypto_context_sub);

  // Find all publisher tokens in subscribers token store
  while (!ddsrt_circlist_isempty (pub_tokens))
  {
    struct ddsrt_circlist_elem *list_elem = ddsrt_circlist_oldest (pub_tokens);
    struct crypto_token_data *token_data = DDSRT_FROM_CIRCLIST (struct crypto_token_data, e, list_elem);
    enum crypto_tokens_type exp_type = TOKEN_TYPE_INVALID;
    for (size_t n = 0; n < token_data->n_tokens; n++)
    {
      switch (token_data->type)
      {
        case LOCAL_PARTICIPANT_TOKENS: exp_type = REMOTE_PARTICIPANT_TOKENS; break;
        case REMOTE_PARTICIPANT_TOKENS: exp_type = LOCAL_PARTICIPANT_TOKENS; break;
        case LOCAL_WRITER_TOKENS: exp_type = REMOTE_WRITER_TOKENS; break;
        case REMOTE_WRITER_TOKENS: exp_type = LOCAL_WRITER_TOKENS; break;
        case LOCAL_READER_TOKENS: exp_type = REMOTE_READER_TOKENS; break;
        case REMOTE_READER_TOKENS: exp_type = LOCAL_READER_TOKENS; break;
        default: CU_FAIL ("Unexpected token type");
      }
      printf("- find token %s #%"PRIuSIZE", len %"PRIuSIZE"\n", get_crypto_token_type_str (token_data->type), n, token_data->data_len[n]);
      struct crypto_token_data *st = find_crypto_token (crypto_context_sub, exp_type, token_data->data[n], token_data->data_len[n]);
      CU_ASSERT_FATAL (st != NULL);
    }
    ddsrt_circlist_remove (pub_tokens, list_elem);
    ddsrt_free (token_data);
  }

  // Cleanup
  while (!ddsrt_circlist_isempty (sub_tokens))
  {
    struct ddsrt_circlist_elem *list_elem = ddsrt_circlist_oldest (sub_tokens);
    ddsrt_circlist_remove (sub_tokens, list_elem);
    ddsrt_free (list_elem);
  }
  ddsrt_free (sub_tokens);
  ddsrt_free (pub_tokens);
  handshake_fini ();
}
