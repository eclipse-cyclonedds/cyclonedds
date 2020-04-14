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
#include <string.h>
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include "CUnit/Test.h"
#include "dds/dds.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/expand_vars.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/io.h"
#include "common/config_env.h"
#include "security_config_test_utils.h"

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
    "        <topic_rule>"
    "          <topic_expression>*</topic_expression>"
    "          <enable_discovery_protection>${ENABLE_DISC_PROTECTION:-false}</enable_discovery_protection>"
    "          <enable_liveliness_protection>${ENABLE_LIVELINESS_PROTECTION:-false}</enable_liveliness_protection>"
    "          <enable_read_access_control>${ENABLE_READ_AC:-true}</enable_read_access_control>"
    "          <enable_write_access_control>${ENABLE_WRITE_AC:-true}</enable_write_access_control>"
    "          <metadata_protection_kind>${METADATA_PROTECTION_KIND:-NONE}</metadata_protection_kind>"
    "          <data_protection_kind>${DATA_PROTECTION_KIND:-NONE}</data_protection_kind>"
    "        </topic_rule>"
    "      </topic_access_rules>"
    "    </domain_rule>"
    "  </domain_access_rules>"
    "</dds>";

static const char *topic_xml =
    "<topic>${TOPIC_NAME}</topic>";

static const char *permissions_xml_grant =
    "    <grant name=\"${GRANT_NAME}\">"
    "      <subject_name>${SUBJECT_NAME}</subject_name>"
    "      <validity><not_before>${NOT_BEFORE:-2015-09-15T01:00:00}</not_before><not_after>${NOT_AFTER:-2115-09-15T01:00:00}</not_after></validity>"
    "      <allow_rule>"
    "        <domains>${DOMAIN_ID:+<id>}${DOMAIN_ID:-<id_range><min>0</min><max>230</max></id_range>}${DOMAIN_ID:+</id>}</domains>"
    "        <publish>"
    "          <topics>${PUB_TOPICS:-<topic>*</topic>}</topics>"
    "          <partitions><partition>*</partition></partitions>"
    "        </publish>"
    "        <subscribe>"
    "          <topics>${SUB_TOPICS:-<topic>*</topic>}</topics>"
    "          <partitions><partition>*</partition></partitions>"
    "        </subscribe>"
    "      </allow_rule>"
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

static char * smime_sign(char * ca_cert_path, char * ca_priv_key_path, const char * data)
{
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

char * get_governance_config(struct kvp *config_vars, bool add_prefix)
{
  char * config = ddsrt_expand_vars (governance_xml, &expand_lookup_vars, config_vars);
  char * config_signed = get_signed_data (config);
  ddsrt_free (config);
  return prefix_data (config_signed, add_prefix);
}


char * get_permissions_topic(const char * name)
{
  struct kvp vars[] = {
    { "TOPIC_NAME", name, 1 },
    { NULL, NULL, 0 }
  };
  return ddsrt_expand_vars (topic_xml, &expand_lookup_vars, vars);
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

  strftime(buf, len, "%FT%TZ", &tm);
  return buf;
}

char * get_permissions_grant(const char * name, const char * subject, const char * domain_id,
    dds_time_t not_before, dds_time_t not_after, const char * pub_topics, const char * sub_topics, const char * default_policy)
{
  char not_before_str[] = "0000-00-00T00:00:00Z";
  char not_after_str[] = "0000-00-00T00:00:00Z";
  get_xml_datetime(not_before, not_before_str, sizeof(not_before_str));
  get_xml_datetime(not_after, not_after_str, sizeof(not_after_str));

  struct kvp vars[] = {
    { "GRANT_NAME", name, 1 },
    { "SUBJECT_NAME", subject, 1 },
    { "NOT_BEFORE", not_before_str, 1 },
    { "NOT_AFTER", not_after_str, 1 },
    { "DOMAIN_ID", domain_id, 3 },
    { "PUB_TOPICS", pub_topics, 1 },
    { "SUB_TOPICS", sub_topics, 1 },
    { "DEFAULT_POLICY", default_policy, 1 },
    { NULL, NULL, 0 }
  };
  return ddsrt_expand_vars (permissions_xml_grant, &expand_lookup_vars, vars);
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
