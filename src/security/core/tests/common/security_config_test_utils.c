// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdlib.h>
#include <string.h>

#include "CUnit/Test.h"
#include "dds/dds.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/expand_vars.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/io.h"
#include "dds/security/openssl_support.h"
#include "common/config_env.h"
#include "common/test_utils.h"
#include "security_config_test_utils.h"

static const char *topic_rule =
    "        <topic_rule>"
    "          <topic_expression>${TOPIC_EXPRESSION}</topic_expression>"
    "          <enable_discovery_protection>${ENABLE_DISC_PROTECTION}</enable_discovery_protection>"
    "          <enable_liveliness_protection>${ENABLE_LIVELINESS_PROTECTION}</enable_liveliness_protection>"
    "          <enable_read_access_control>${ENABLE_READ_AC}</enable_read_access_control>"
    "          <enable_write_access_control>${ENABLE_WRITE_AC}</enable_write_access_control>"
    "          <metadata_protection_kind>${METADATA_PROTECTION_KIND}</metadata_protection_kind>"
    "          <data_protection_kind>${DATA_PROTECTION_KIND}</data_protection_kind>"
    "        </topic_rule>";

static const char *governance_xml =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<dds xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"https://www.omg.org/spec/DDS-SECURITY/20170901/omg_shared_ca_governance.xsd\">"
    "  <domain_access_rules>"
    "    <domain_rule>"
    "      <domains>"
    "        <id_range>"
    "          <min>0</min>"
    "          <max>230</max>"
    "        </id_range>"
    "      </domains>"
    "      <allow_unauthenticated_participants>${ALLOW_UNAUTH_PP:-false}</allow_unauthenticated_participants>"
    "      <enable_join_access_control>${ENABLE_JOIN_AC:-false}</enable_join_access_control>"
    "      <discovery_protection_kind>${DISCOVERY_PROTECTION_KIND:-NONE}</discovery_protection_kind>"
    "      <liveliness_protection_kind>${LIVELINESS_PROTECTION_KIND:-NONE}</liveliness_protection_kind>"
    "      <rtps_protection_kind>${RTPS_PROTECTION_KIND:-NONE}</rtps_protection_kind>"
    "      <topic_access_rules>"
    "        ${TOPIC_RULES}"
    "      </topic_access_rules>"
    "    </domain_rule>"
    "  </domain_access_rules>"
    "</dds>";

static const char *permissions_xml_pub =
    "        <publish>"
    "          <topics><topic>${TOPIC_NAME}</topic></topics>"
    "          <partitions>${PARTITIONS:-<partition>*</partition>}</partitions>"
    "        </publish>";

static const char *permissions_xml_sub =
    "        <subscribe>"
    "          <topics><topic>${TOPIC_NAME}</topic></topics>"
    "          <partitions>${PARTITIONS:-<partition>*</partition>}</partitions>"
    "        </subscribe>";

static const char *permissions_xml_allow_rule =
    "      <allow_rule>"
    "        <domains>${DOMAIN_ID:+<id>}${DOMAIN_ID:-<id_range><min>0</min><max>230</max></id_range>}${DOMAIN_ID:+</id>}</domains>"
    "        ${PUBLISH}"
    "        ${SUBSCRIBE}"
    "      </allow_rule>";

static const char *permissions_xml_deny_rule =
    "      <deny_rule>"
    "        <domains>${DOMAIN_ID:+<id>}${DOMAIN_ID:-<id_range><min>0</min><max>230</max></id_range>}${DOMAIN_ID:+</id>}</domains>"
    "        ${PUBLISH}"
    "        ${SUBSCRIBE}"
    "      </deny_rule>";

static const char *permissions_xml_grant =
    "    <grant name=\"${GRANT_NAME}\">"
    "      <subject_name>${SUBJECT_NAME}</subject_name>"
    "      <validity><not_before>${NOT_BEFORE:-2015-09-15T01:00:00}</not_before><not_after>${NOT_AFTER:-2115-09-15T01:00:00}</not_after></validity>"
    "      ${RULES}"
    "      <default>${DEFAULT_POLICY:-DENY}</default>"
    "    </grant>";

static const char *permissions_xml =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<dds xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"https://www.omg.org/spec/DDS-SECURITY/20170901/omg_shared_ca_permissions.xsd\">"
    "  <permissions>"
    "    ${GRANTS}"
    "  </permissions>"
    "</dds>";


const char * expand_lookup_vars(const char *name, void * data)
{
  struct kvp *vars = (struct kvp *)data;
  for (uint32_t i = 0; vars[i].key != NULL; i++)
  {
    if (!strcmp(vars[i].key, name))
    {
      vars[i].count--;
      return vars[i].value;
    }
  }
  return NULL;
}

const char * expand_lookup_vars_env(const char *name, void * data)
{
  const char *env;
  if ((env = expand_lookup_vars (name, data)))
    return env;
  return ((ddsrt_getenv(name, &env)) == DDS_RETCODE_OK) ? env : NULL;
}

int32_t expand_lookup_unmatched (const struct kvp * lookup_table)
{
  int32_t unmatched = 0;
  for (uint32_t i = 0; lookup_table[i].key != NULL; i++)
  {
    int32_t c = lookup_table[i].count;
    if (c > 0 && unmatched >= INT32_MAX - c)
      return INT32_MAX;
    if (c < 0 && unmatched <= INT32_MIN - c)
      return INT32_MIN;
    unmatched += c;
  }
  return unmatched;
}

static char * get_xml_datetime(dds_time_t t, char * buf, size_t len)
{
  struct tm tm;
  time_t sec = (time_t)(t / DDS_NSECS_IN_SEC);
#if _WIN32
  (void)gmtime_s(&tm, &sec);
#else
  (void)gmtime_r(&sec, &tm);
#endif /* _WIN32 */
  strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buf;
}

static char * smime_sign(char * ca_cert_path, char * ca_priv_key_path, const char * data)
{
  dds_openssl_init ();

  // Read CA certificate
  BIO *ca_cert_bio = BIO_new (BIO_s_file ());
  if (BIO_read_filename (ca_cert_bio, ca_cert_path) <= 0)
  {
      printf ("Error reading CA certificate file %s\n", ca_cert_path);
      CU_ASSERT_FATAL (false);
  }

  // Read CA private key
  BIO *ca_priv_key_bio = BIO_new (BIO_s_file ());
  if (BIO_read_filename (ca_priv_key_bio, ca_priv_key_path) <= 0)
  {
      printf ("Error reading CA private key file %s\n", ca_priv_key_path);
      CU_ASSERT_FATAL (false);
  }

  // Create Openssl certificate and private key from the BIO's
  X509 *ca_cert = PEM_read_bio_X509_AUX (ca_cert_bio, NULL, NULL, NULL);
  EVP_PKEY* ca_priv_key = PEM_read_bio_PrivateKey (ca_priv_key_bio, NULL, 0, NULL);

  // Read the data
  BIO *data_bio = BIO_new (BIO_s_mem ());
  if (BIO_puts (data_bio, data) <= 0) {
      printf ("Error getting configuration data for signing\n");
      CU_ASSERT_FATAL (false);
  }

  // Create the data signing object
  PKCS7 *signed_data = PKCS7_sign (ca_cert, ca_priv_key, NULL, data_bio, PKCS7_DETACHED | PKCS7_STREAM | PKCS7_TEXT);
  if (!signed_data) {
      printf ("Error signing configuration data\n");
      CU_ASSERT_FATAL (false);
  }

  // Create BIO for writing output
  BIO *output_bio = BIO_new (BIO_s_mem ());
  if (!SMIME_write_PKCS7 (output_bio, signed_data, data_bio, PKCS7_DETACHED | PKCS7_STREAM | PKCS7_TEXT)) {
      printf ("Error writing signed XML configuration\n");
      CU_ASSERT_FATAL (false);
  }

  // Get string
  char *output_tmp = NULL;
  size_t output_sz = (size_t)BIO_get_mem_data (output_bio, &output_tmp);
  char * output = ddsrt_malloc(output_sz + 1);
  memcpy(output, output_tmp, output_sz);
  output[output_sz] = 0;

  BIO_free (output_bio);
  PKCS7_free (signed_data);
  BIO_free (data_bio);
  EVP_PKEY_free (ca_priv_key);
  X509_free (ca_cert);
  BIO_free (ca_priv_key_bio);
  BIO_free (ca_cert_bio);

  return output;
}

static char *get_signed_data(const char *data)
{
  return smime_sign (
    COMMON_ETC_PATH("default_permissions_ca.pem"),
    COMMON_ETC_PATH("default_permissions_ca_key.pem"),
    data);
}

static char * prefix_data (char * config_signed, bool add_prefix)
{
  if (add_prefix)
  {
    char * tmp = config_signed;
    ddsrt_asprintf (&config_signed, "data:,%s", tmp);
    ddsrt_free (tmp);
  }
  return config_signed;
}

static void print_config_vars(struct kvp *vars)
{
  for (uint32_t i = 0; vars[i].key != NULL; i++)
    printf("%s=%s; ", vars[i].key, vars[i].value);
}

char * get_governance_topic_rule(const char * topic_expr, bool discovery_protection, bool liveliness_protection,
    bool read_ac, bool write_ac, DDS_Security_ProtectionKind metadata_protection_kind, DDS_Security_BasicProtectionKind data_protection_kind)
{
  struct kvp vars[] = {
    { "TOPIC_EXPRESSION", topic_expr != NULL ? topic_expr : "*", 1 },
    { "ENABLE_DISC_PROTECTION", discovery_protection ? "true" : "false", 1 },
    { "ENABLE_LIVELINESS_PROTECTION", liveliness_protection ? "true" : "false", 1 },
    { "ENABLE_READ_AC", read_ac ? "true" : "false", 1 },
    { "ENABLE_WRITE_AC", write_ac ? "true" : "false", 1 },
    { "METADATA_PROTECTION_KIND", pk_to_str (metadata_protection_kind), 1 },
    { "DATA_PROTECTION_KIND", bpk_to_str (data_protection_kind), 1 },
    { NULL, NULL, 0 }
  };
  return ddsrt_expand_vars (topic_rule, &expand_lookup_vars, vars);
}

char * get_governance_config (bool allow_unauth_pp, bool enable_join_ac, DDS_Security_ProtectionKind discovery_protection_kind, DDS_Security_ProtectionKind liveliness_protection_kind,
    DDS_Security_ProtectionKind rtps_protection_kind, const char * topic_rules, bool add_prefix)
{
  struct kvp vars[] = {
    { "ALLOW_UNAUTH_PP", allow_unauth_pp ? "true" : "false", 1 },
    { "ENABLE_JOIN_AC", enable_join_ac ? "true" : "false", 1 },
    { "DISCOVERY_PROTECTION_KIND", pk_to_str (discovery_protection_kind), 1 },
    { "LIVELINESS_PROTECTION_KIND", pk_to_str (liveliness_protection_kind), 1 },
    { "RTPS_PROTECTION_KIND", pk_to_str (rtps_protection_kind), 1 },
    { "TOPIC_RULES", topic_rules != NULL ? topic_rules : get_governance_topic_rule (NULL, false, false, false, false, PK_N, BPK_N), 1 },
    { NULL, NULL, 0 }
  };
  char * config = ddsrt_expand_vars (governance_xml, &expand_lookup_vars, vars);
  char * config_signed = get_signed_data (config);
  ddsrt_free (config);

  print_test_msg ("governance configuration: ");
  print_config_vars (vars);
  printf("\n");

  return prefix_data (config_signed, add_prefix);
}

static char * expand_permissions_pubsub (const char * template, const char * topic_name, const char ** parts)
{
  char *xml_parts = NULL;
  if (parts)
  {
    static const char open[] = "<partition>", close[] = "</partition>";
    size_t len = 0;
    for (int i = 0; parts[i]; i++)
      len += strlen (parts[i]) + sizeof (open) + sizeof (close) - 2;
    xml_parts = ddsrt_malloc (len + 1);
    int pos = 0;
    for (int i = 0; parts[i]; i++)
      pos += snprintf (xml_parts + pos, len + 1 - (size_t) pos, "%s%s%s", open, parts[i], close);
    assert ((size_t) pos == len);
  }
  struct kvp vars[3] = {
    { "TOPIC_NAME", topic_name, 1 },
    { xml_parts ? "PARTITIONS" : NULL, xml_parts, xml_parts != NULL },
    { NULL, NULL, 0 }
  };
  char * x = ddsrt_expand_vars (template, &expand_lookup_vars, vars);
  ddsrt_free (xml_parts);
  return x;
}

static char * expand_permissions_rule (const char * template, const char * domain_id, const char * pub_xml, const char * sub_xml)
{
  struct kvp vars[] = {
    { "DOMAIN_ID", domain_id, 3 },
    { "PUBLISH", pub_xml, 1 },
    { "SUBSCRIBE", sub_xml, 1 },
    { NULL, NULL, 0 }
  };
  return ddsrt_expand_vars (template, &expand_lookup_vars, vars);
}

char * get_permissions_rules_w_partitions (const char * domain_id, const char * allow_pub_topic, const char * allow_sub_topic, const char ** allow_parts, const char * deny_pub_topic, const char * deny_sub_topic, const char ** deny_parts)
{
  char * allow_pub_xml = NULL, * allow_sub_xml = NULL, * deny_pub_xml = NULL, * deny_sub_xml = NULL;
  char * allow_rule_xml = NULL, * deny_rule_xml = NULL, * rules_xml;

  if (allow_pub_topic != NULL) allow_pub_xml = expand_permissions_pubsub (permissions_xml_pub, allow_pub_topic, allow_parts);
  if (allow_sub_topic != NULL) allow_sub_xml = expand_permissions_pubsub (permissions_xml_sub, allow_sub_topic, allow_parts);
  if (deny_pub_topic != NULL) deny_pub_xml = expand_permissions_pubsub (permissions_xml_pub, deny_pub_topic, deny_parts);
  if (deny_sub_topic != NULL) deny_sub_xml = expand_permissions_pubsub (permissions_xml_sub, deny_sub_topic, deny_parts);

  if (allow_pub_xml != NULL || allow_sub_xml != NULL)
  {
    allow_rule_xml = expand_permissions_rule (permissions_xml_allow_rule, domain_id, allow_pub_xml, allow_sub_xml);
    if (allow_pub_xml != NULL) ddsrt_free (allow_pub_xml);
    if (allow_sub_xml != NULL) ddsrt_free (allow_sub_xml);
  }
  if (deny_pub_xml != NULL || deny_sub_xml != NULL)
  {
    deny_rule_xml = expand_permissions_rule (permissions_xml_deny_rule, domain_id, deny_pub_xml, deny_sub_xml);
    if (deny_pub_xml != NULL) ddsrt_free (deny_pub_xml);
    if (deny_sub_xml != NULL) ddsrt_free (deny_sub_xml);
  }
  ddsrt_asprintf (&rules_xml, "%s%s", allow_rule_xml != NULL ? allow_rule_xml : "", deny_rule_xml != NULL ? deny_rule_xml : "");
  if (allow_rule_xml != NULL) ddsrt_free (allow_rule_xml);
  if (deny_rule_xml != NULL) ddsrt_free (deny_rule_xml);
  return rules_xml;
}

char * get_permissions_rules (const char * domain_id, const char * allow_pub_topic, const char * allow_sub_topic, const char * deny_pub_topic, const char * deny_sub_topic)
{
  return get_permissions_rules_w_partitions (domain_id, allow_pub_topic, allow_sub_topic, NULL, deny_pub_topic, deny_sub_topic, NULL);
}

char * get_permissions_grant (const char * grant_name, const char * subject_name, dds_time_t not_before, dds_time_t not_after, const char * rules_xml, const char * default_policy)
{
  char not_before_str[] = "0000-00-00T00:00:00Z";
  char not_after_str[] = "0000-00-00T00:00:00Z";
  get_xml_datetime (not_before, not_before_str, sizeof(not_before_str));
  get_xml_datetime (not_after, not_after_str, sizeof(not_after_str));

  struct kvp vars[] = {
    { "GRANT_NAME", grant_name, 1 },
    { "SUBJECT_NAME", subject_name, 1 },
    { "NOT_BEFORE", not_before_str, 1 },
    { "NOT_AFTER", not_after_str, 1 },
    { "RULES", rules_xml, 1 },
    { "DEFAULT_POLICY", default_policy, 1 },
    { NULL, NULL, 0 }
  };
  char * res = ddsrt_expand_vars (permissions_xml_grant, &expand_lookup_vars, vars);
  CU_ASSERT_FATAL (expand_lookup_unmatched (vars) == 0);
  return res;
}

char * get_permissions_default_grant (const char * grant_name, const char * subject_name, const char * topic_name)
{
  dds_time_t now = dds_time ();
  char * rules_xml = get_permissions_rules (NULL, topic_name, topic_name, NULL, NULL);
  char * grant_xml = get_permissions_grant (grant_name, subject_name, now, now + DDS_SECS(3600), rules_xml, "DENY");
  ddsrt_free (rules_xml);
  return grant_xml;
}

char * get_permissions_config(char * grants[], size_t ngrants, bool add_prefix)
{
  char *grants_str = NULL;
  for (size_t n = 0; n < ngrants; n++)
  {
    char * tmp = grants_str;
    ddsrt_asprintf (&grants_str, "%s%s", grants_str ? grants_str : "", grants[n]);
    ddsrt_free (tmp);
  }
  struct kvp vars[] = {
    { "GRANTS", grants_str, 1 },
    { NULL, NULL, 0}
  };
  char *config = ddsrt_expand_vars (permissions_xml, &expand_lookup_vars, vars);
  char *config_signed = get_signed_data (config);
  ddsrt_free (grants_str);
  ddsrt_free (config);
  return prefix_data (config_signed, add_prefix);
}
