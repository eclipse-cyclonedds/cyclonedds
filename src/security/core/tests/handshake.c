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
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_proxy_participant.h"
#include "dds/ddsi/ddsi_proxy_endpoint.h"
#include "dds/ddsi/ddsi_protocol.h"
#include "dds/ddsi/ddsi_thread.h"
#include "ddsi__misc.h"
#include "ddsi__entity_index.h"
#include "ddsi__security_omg.h"
#include "ddsi__proxy_participant.h"
#include "dds__entity.h"

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
  CU_ASSERT_EQ_FATAL (unmatched, 0);
  g_domain1 = dds_create_domain (DDS_DOMAINID1, conf);
  g_domain2 = dds_create_domain (DDS_DOMAINID2, conf);
  dds_free (conf);

  g_participant1 = dds_create_participant (DDS_DOMAINID1, NULL, NULL);
  CU_ASSERT_GT_FATAL (g_participant1, 0);
  g_participant2 = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
  CU_ASSERT_GT_FATAL (g_participant2, 0);
}

static void handshake_fini(void)
{
  dds_return_t ret = dds_delete (g_domain1);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_delete (g_domain2);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
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
  CU_ASSERT_EQ_FATAL (nhs, 1);
  for (int n = 0; n < nhs; n++)
    validate_handshake_result (&hs_list[n], false, NULL, false, NULL);
  handshake_list_fini (hs_list, nhs);

  validate_handshake (DDS_DOMAINID2, false, NULL, &hs_list, &nhs, DDS_SECS(2));
  CU_ASSERT_EQ_FATAL (nhs, 1);
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
  CU_ASSERT_NEQ_FATAL (crypto_context_pub, NULL);
  struct ddsrt_circlist *pub_tokens = get_crypto_tokens (crypto_context_pub);

  struct dds_security_cryptography_impl * crypto_context_sub = get_cryptography_context (g_participant2);
  CU_ASSERT_NEQ_FATAL (crypto_context_sub, NULL);
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
      CU_ASSERT_NEQ_FATAL (st, NULL);
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

/* ------------------------------------------------------------------------- *
 * Regression test: an unauthenticated plaintext SPDP dispose that merely spoofs
 * the well-known secure-SPDP writer entity id (0xff0101c2) must NOT be able to
 * delete an authenticated proxy participant.
 *
 * ddsi_is_proxy_participant_deletion_allowed() used to unconditionally authorize
 * a participant dispose whenever the (attacker-controlled) RTPS writerId equalled
 * the secure-SPDP writer entity id, without binding that writer to the proxy
 * participant and without requiring that the dispose arrived over a protected
 * channel. With a governance that leaves discovery/rtps protection NONE (still
 * authenticating participants), a single plaintext dispose carrying entity id
 * 0xff0101c2 deleted an authenticated proxy participant and cascaded to all of
 * its endpoints.
 *
 * The fix only trusts the secure-writer entity id when a matched secure-SPDP
 * proxy writer bound to the victim's prefix exists (which guarantees the sample
 * was decrypted/validated by the receive path). This test authenticates two
 * participants in a discovery-protection-NONE domain and asserts the spoofed
 * dispose is refused and the peer survives.
 * ------------------------------------------------------------------------- */

static const char *repro_config =
    "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}"
    "<Domain id=\"any\">"
    "  <Discovery>"
    "    <ExternalDomainId>0</ExternalDomainId>"
    "    <Tag>\\${CYCLONEDDS_PID}</Tag>"
    "  </Discovery>"
    "  <Security>"
    "    <Authentication>"
    "      <Library initFunction=\"${AUTH_INIT}\" finalizeFunction=\"${AUTH_FINI}\" path=\"" WRAPPERLIB_PATH("dds_security_authentication_wrapper") "\"/>"
    "      <IdentityCertificate>data:,"TEST_IDENTITY1_CERTIFICATE"</IdentityCertificate>"
    "      <PrivateKey>data:,"TEST_IDENTITY1_PRIVATE_KEY"</PrivateKey>"
    "      <IdentityCA>data:,"TEST_IDENTITY_CA1_CERTIFICATE"</IdentityCA>"
    "    </Authentication>"
    "    <AccessControl>"
    "      <Library finalizeFunction=\"finalize_access_control\" initFunction=\"init_access_control\"/>"
    "      <Governance><![CDATA[data:,${GOVERNANCE_DATA}]]></Governance>"
    "      <PermissionsCA>file:" COMMON_ETC_PATH("default_permissions_ca.pem") "</PermissionsCA>"
    "      <Permissions>file:" COMMON_ETC_PATH("default_permissions.p7s") "</Permissions>"
    "    </AccessControl>"
    "    <Cryptographic>"
    "      <Library initFunction=\"${CRYPTO_INIT}\" finalizeFunction=\"${CRYPTO_FINI}\" path=\"" WRAPPERLIB_PATH("dds_security_cryptography_wrapper") "\"/>"
    "    </Cryptographic>"
    "  </Security>"
    "</Domain>";

static void repro_init (void)
{
  /* Governance that still authenticates participants (allow_unauthenticated =
     false, join access control enabled) but leaves discovery / liveliness /
     rtps protection at NONE -- a legal configuration. */
  char *gov_topic_rule = get_governance_topic_rule ("*", false, false, true, true, PK_N, BPK_N);
  char *gov_config_signed = get_governance_config (false, true, PK_N, PK_N, PK_N, gov_topic_rule, false);

  struct kvp config_vars[] = {
    { "AUTH_INIT", "init_test_authentication_wrapped", 1 },
    { "AUTH_FINI", "finalize_test_authentication_wrapped", 1 },
    { "CRYPTO_INIT", "init_test_cryptography_wrapped", 1 },
    { "CRYPTO_FINI", "finalize_test_cryptography_wrapped", 1 },
    { "GOVERNANCE_DATA", gov_config_signed, 1 },
    { NULL, NULL, 0 }
  };

  char *conf = ddsrt_expand_vars_sh (repro_config, &expand_lookup_vars_env, config_vars);
  int32_t unmatched = expand_lookup_unmatched (config_vars);
  CU_ASSERT_EQ_FATAL (unmatched, 0);
  g_domain1 = dds_create_domain (DDS_DOMAINID1, conf);
  g_domain2 = dds_create_domain (DDS_DOMAINID2, conf);
  dds_free (conf);
  ddsrt_free (gov_config_signed);
  ddsrt_free (gov_topic_rule);

  g_participant1 = dds_create_participant (DDS_DOMAINID1, NULL, NULL);
  CU_ASSERT_GT_FATAL (g_participant1, 0);
  g_participant2 = dds_create_participant (DDS_DOMAINID2, NULL, NULL);
  CU_ASSERT_GT_FATAL (g_participant2, 0);
}

static struct ddsi_domaingv *repro_get_gv (dds_entity_t pp)
{
  struct dds_entity *e;
  CU_ASSERT_EQ_FATAL (dds_entity_pin (pp, &e), 0);
  struct ddsi_domaingv *gv = &e->m_domain->gv;
  dds_entity_unpin (e);
  return gv;
}

static ddsi_guid_t repro_get_pp_guid (dds_entity_t pp)
{
  struct dds_entity *e;
  CU_ASSERT_EQ_FATAL (dds_entity_pin (pp, &e), 0);
  ddsi_guid_t guid = e->m_guid;
  dds_entity_unpin (e);
  return guid;
}

/* Count proxy endpoints (readers + writers) in gv's entity index whose GUID
   prefix equals the given participant prefix. */
static int repro_count_proxy_endpoints (struct ddsi_domaingv *gv, const ddsi_guid_prefix_t *prefix)
{
  int count = 0;
  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv);
  {
    struct ddsi_entity_enum_proxy_writer epw;
    struct ddsi_proxy_writer *pwr;
    ddsi_entidx_enum_proxy_writer_init (&epw, gv->entity_index);
    while ((pwr = ddsi_entidx_enum_proxy_writer_next (&epw)) != NULL)
      if (memcmp (&pwr->e.guid.prefix, prefix, sizeof (*prefix)) == 0)
        count++;
    ddsi_entidx_enum_proxy_writer_fini (&epw);
  }
  {
    struct ddsi_entity_enum_proxy_reader epr;
    struct ddsi_proxy_reader *prd;
    ddsi_entidx_enum_proxy_reader_init (&epr, gv->entity_index);
    while ((prd = ddsi_entidx_enum_proxy_reader_next (&epr)) != NULL)
      if (memcmp (&prd->e.guid.prefix, prefix, sizeof (*prefix)) == 0)
        count++;
    ddsi_entidx_enum_proxy_reader_fini (&epr);
  }
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  return count;
}

CU_Test(ddssec_handshake, spdp_spoofed_secure_writer_dispose)
{
  repro_init ();

  /* Let both participants complete DDS-Security authentication. */
  validate_handshake_nofail (DDS_DOMAINID1, DDS_SECS (5));
  validate_handshake_nofail (DDS_DOMAINID2, DDS_SECS (5));

  struct ddsi_domaingv *gv1 = repro_get_gv (g_participant1);
  ddsi_guid_t victim_guid = repro_get_pp_guid (g_participant2);

  /* The authenticated peer must be known to participant1 as a proxy participant. */
  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv1);
  struct ddsi_proxy_participant *proxypp = ddsi_entidx_lookup_proxy_participant_guid (gv1->entity_index, &victim_guid);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  CU_ASSERT_NEQ_FATAL (proxypp, NULL);

  int endpoints_before = repro_count_proxy_endpoints (gv1, &victim_guid.prefix);
  printf ("victim proxy participant "PGUIDFMT" known to participant1 with %d proxy endpoints\n",
          PGUID (victim_guid), endpoints_before);
  CU_ASSERT_FATAL (endpoints_before > 0);

  /* A dispose that pretends to come from the *plaintext* SPDP writer is correctly
     refused, because the proxy participant is authenticated: this proves the
     proxy participant really is authenticated. */
  ddsi_entityid_t plain_wr = { .u = DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER };
  ddsi_entityid_t secure_wr = { .u = DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER };

  /* The participant-level authentication association is established in the ddsi
     thread shortly after the handshake's final message; wait for it to settle.
     Once authenticated, a dispose spoofing the *plaintext* SPDP writer is
     refused, which is our signal that the proxy participant is authenticated. */
  bool plain_allowed = true;
  dds_time_t tauth = dds_time () + DDS_SECS (10);
  do {
    ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv1);
    plain_allowed = ddsi_is_proxy_participant_deletion_allowed (gv1, &victim_guid, plain_wr);
    ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
    if (!plain_allowed)
      break;
    dds_sleepfor (DDS_MSECS (50));
  } while (dds_time () < tauth);

  printf ("deletion via plaintext SPDP writer (0x%08x) allowed = %d (expected 0)\n", plain_wr.u, plain_allowed);
  CU_ASSERT (plain_allowed == false);

  /* A dispose that merely *claims* the secure-SPDP writer entity id must NOT be
     honoured: with discovery protection NONE there is no matched secure-SPDP
     proxy writer, so the sample was never decrypted/validated and is just an
     unauthenticated plaintext packet. Before the fix this returned true and
     deleted the authenticated peer (CVE-class single-packet DoS); the fix
     requires a matched secure-SPDP proxy writer bound to the victim prefix. */
  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv1);
  bool secure_allowed = ddsi_is_proxy_participant_deletion_allowed (gv1, &victim_guid, secure_wr);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());

  printf ("deletion via spoofed secure SPDP writer (0x%08x) allowed = %d (expected 0)\n", secure_wr.u, secure_allowed);
  CU_ASSERT (secure_allowed == false);

  /* And a fabricated/unrelated GUID (no proxy writer, no proxy participant) is
     likewise refused: the secure-writer entity id no longer grants a blanket
     authorization without a matching secure proxy writer. */
  ddsi_guid_t bogus_guid = victim_guid;
  bogus_guid.prefix.u[0] ^= 0x5a5a5a5a;
  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv1);
  bool bogus_allowed = ddsi_is_proxy_participant_deletion_allowed (gv1, &bogus_guid, secure_wr);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  printf ("deletion of unrelated GUID via secure writer allowed = %d (expected 0)\n", bogus_allowed);
  CU_ASSERT (bogus_allowed == false);

  /* Drive the full dispose exactly as handle_spdp_dead() does: because deletion
     is (now) refused, the authenticated proxy participant and all of its
     endpoints must survive the spoofed packet. */
  if (secure_allowed)
  {
    ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv1);
    (void) ddsi_delete_proxy_participant_by_guid (gv1, &victim_guid, ddsrt_time_wallclock (), false);
    ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  }

  /* Allow a moment for any (erroneous) deletion to settle before re-checking. */
  dds_sleepfor (DDS_MSECS (200));

  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv1);
  proxypp = ddsi_entidx_lookup_proxy_participant_guid (gv1->entity_index, &victim_guid);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());

  int endpoints_after = repro_count_proxy_endpoints (gv1, &victim_guid.prefix);
  printf ("after single spoofed dispose: proxy participant %s, proxy endpoints %d -> %d\n",
          proxypp == NULL ? "DELETED" : "still present", endpoints_before, endpoints_after);

  CU_ASSERT_NEQ_FATAL (proxypp, NULL);
  CU_ASSERT_EQ_FATAL (endpoints_after, endpoints_before);

  handshake_fini ();
}

/* Companion positive test: with discovery protection enabled (the default
   governance), a dispose from the matched, submessage-protected secure-SPDP
   proxy writer is still honoured -- the fix must not break legitimate secure
   participant teardown. */
CU_Test(ddssec_handshake, spdp_secure_writer_dispose_allowed_when_protected)
{
  handshake_init (
    "init_test_authentication_wrapped", "finalize_test_authentication_wrapped",
    "init_test_cryptography_wrapped", "finalize_test_cryptography_wrapped");

  validate_handshake_nofail (DDS_DOMAINID1, DDS_SECS (5));
  validate_handshake_nofail (DDS_DOMAINID2, DDS_SECS (5));

  struct ddsi_domaingv *gv1 = repro_get_gv (g_participant1);
  ddsi_guid_t peer_guid = repro_get_pp_guid (g_participant2);

  ddsi_entityid_t secure_wr = { .u = DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER };

  /* Wait for the authenticated, discovery-protected secure-SPDP proxy writer to
     be matched; once present, a secure-writer dispose is (legitimately) allowed. */
  bool secure_allowed = false;
  dds_time_t tend = dds_time () + DDS_SECS (10);
  do {
    ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv1);
    secure_allowed = ddsi_is_proxy_participant_deletion_allowed (gv1, &peer_guid, secure_wr);
    ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
    if (secure_allowed)
      break;
    dds_sleepfor (DDS_MSECS (50));
  } while (dds_time () < tend);

  printf ("protected secure SPDP writer dispose allowed = %d (expected 1)\n", secure_allowed);
  CU_ASSERT (secure_allowed == true);

  handshake_fini ();
}
