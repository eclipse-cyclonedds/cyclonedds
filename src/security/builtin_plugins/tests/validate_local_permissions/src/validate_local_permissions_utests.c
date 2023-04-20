// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>

#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/types.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/openssl_support.h"
#include "CUnit/CUnit.h"
#include "CUnit/Test.h"
#include "common/src/loader.h"
#include "config_env.h"

static const char *RELATIVE_PATH_TO_ETC_DIR = "/validate_local_permissions/etc/";

static const char *AUTH_IDENTITY_CERT =
    "data:,-----BEGIN CERTIFICATE-----\n"
    "MIIEQTCCAymgAwIBAgIINpuaAAnrQZIwDQYJKoZIhvcNAQELBQAwXzELMAkGA1UE\n"
    "BhMCTkwxEzARBgNVBAgTClNvbWUtU3RhdGUxITAfBgNVBAoTGEludGVybmV0IFdp\n"
    "ZGdpdHMgUHR5IEx0ZDEYMBYGA1UEAxMPQ0hBTTUwMCByb290IGNhMCAXDTE3MDIy\n"
    "MjIyMjIwMFoYDzIyMjIwMjIyMjIyMjAwWjBcMQswCQYDVQQGEwJOTDETMBEGA1UE\n"
    "CBMKU29tZS1TdGF0ZTEhMB8GA1UEChMYSW50ZXJuZXQgV2lkZ2l0cyBQdHkgTHRk\n"
    "MRUwEwYDVQQDEwxDSEFNNTAwIGNlcnQwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAw\n"
    "ggEKAoIBAQDCpVhivH/wBIyu74rvQncnSZqKyspN6CvD1pmV9wft5PHhVt9jV79v\n"
    "gSub5LADoRHAgFdv9duYgBr17Ob6uRrIY4B18CcrCjhQcC4gjx8y2jl9PeYm+qYD\n"
    "3o44FYBrBq0QCnrQgKsb/qX9Z+Mw/VUiw65x68W876LEHQQoEgT4kxSuagwBoVRk\n"
    "ePD6fYAKmT4XS3x+O0v+rHESTcsKF6yMadgp7h3eH1b8kJTzSx8JV9Zzq++mxjox\n"
    "qhbBVP5nDze2hhSIeCkCvSrx7efkgKS4AQXa5/Z44GiAu1TfXXUqdic9rxwD0edn\n"
    "ajNElnZe7sjok/0yuqvH+2hSqpNva/zpAgMBAAGjggEAMIH9MAwGA1UdDwQFAwMH\n"
    "/4AwgewGA1UdJQSB5DCB4QYIKwYBBQUHAwEGCCsGAQUFBwMCBggrBgEFBQcDAwYI\n"
    "KwYBBQUHAwQGCCsGAQUFBwMIBgorBgEEAYI3AgEVBgorBgEEAYI3AgEWBgorBgEE\n"
    "AYI3CgMBBgorBgEEAYI3CgMDBgorBgEEAYI3CgMEBglghkgBhvhCBAEGCysGAQQB\n"
    "gjcKAwQBBggrBgEFBQcDBQYIKwYBBQUHAwYGCCsGAQUFBwMHBggrBgEFBQgCAgYK\n"
    "KwYBBAGCNxQCAgYIKwYBBQUHAwkGCCsGAQUFBwMNBggrBgEFBQcDDgYHKwYBBQID\n"
    "BTANBgkqhkiG9w0BAQsFAAOCAQEAawdHy0Xw7nTK2ltp91Ion6fJ7hqYuj///zr7\n"
    "Adt6uonpDh/xl3esuwcFimIJrJrHujnGkL0nLddRCikmnzuBMNDWS6yq0/Ckl/YG\n"
    "yjNr44dlX24wo+MVAgkj3/8CyWDZ3a8kBg9QT3bs2SqbjmhTrXN1DRyf9S5vJysE\n"
    "I7V1gTN66BeKL64hOrAlRVrEu8Ds6TWL6Q/YH+61ViZkoLTeSaPjH4nknaFr4C35\n"
    "iji0JhkyfRHRRVPHFnaj25AkxOrSV64qVKoTMjDl5fji5iMGtjm6iJ7q05ml/qDl\n"
    "nLotHXemZNvYhbwUmRzbt4Dls9EMH4VRbP85I94nM5TAvtHVNA==\n"
    "-----END CERTIFICATE-----\n";

static const char *AUTH_IDENTITY_CA =
    "data:,-----BEGIN CERTIFICATE-----\n"
    "MIIEmTCCA4GgAwIBAgIIZ5gEIUFhO5wwDQYJKoZIhvcNAQELBQAwXzELMAkGA1UE\n"
    "BhMCTkwxEzARBgNVBAgTClNvbWUtU3RhdGUxITAfBgNVBAoTGEludGVybmV0IFdp\n"
    "ZGdpdHMgUHR5IEx0ZDEYMBYGA1UEAxMPQ0hBTTUwMCByb290IGNhMCAXDTE4MDIx\n"
    "MjE1MDUwMFoYDzIyMjIwMjIyMjIyMjAwWjBfMQswCQYDVQQGEwJOTDETMBEGA1UE\n"
    "CBMKU29tZS1TdGF0ZTEhMB8GA1UEChMYSW50ZXJuZXQgV2lkZ2l0cyBQdHkgTHRk\n"
    "MRgwFgYDVQQDEw9DSEFNNTAwIHJvb3QgY2EwggEiMA0GCSqGSIb3DQEBAQUAA4IB\n"
    "DwAwggEKAoIBAQC6Fa3TheL+UrdZCp9GhU/2WbneP2t/avUa3muwDttPxeI2XU9k\n"
    "ZjBR95mAXme4SPXHk5+YDN319AqIje3oKhzky/ngvKH2GkoJKYxWnuDBfMEHdViz\n"
    "2Q9/xso2ZvH50ukwWa0pfx2/EVV1wRxeQcRd/UVfq3KTJizG0M88mOYvGEAw3LFf\n"
    "zef7k1aCuOofQmBvLukUudcYpMzfyHFp7lQqU4CcrrR5RtmfiUfrWfdGLea2iPDB\n"
    "pJgN8ESOMwEHtOTEBDclYnH9L4t7CHQz+fXXS5IWFsDK9fCMQjnxDsDVeNrNzTYL\n"
    "FaZrMg9S6IUQCEsQWsnq5weS8omOpVLUm9klAgMBAAGjggFVMIIBUTAMBgNVHRME\n"
    "BTADAQH/MB0GA1UdDgQWBBQg2FZB/j8uWDVnJhjwXkX278znSTAfBgNVHSMEGDAW\n"
    "gBQg2FZB/j8uWDVnJhjwXkX278znSTAPBgNVHQ8BAf8EBQMDB/+AMIHvBgNVHSUB\n"
    "Af8EgeQwgeEGCCsGAQUFBwMBBggrBgEFBQcDAgYIKwYBBQUHAwMGCCsGAQUFBwME\n"
    "BggrBgEFBQcDCAYKKwYBBAGCNwIBFQYKKwYBBAGCNwIBFgYKKwYBBAGCNwoDAQYK\n"
    "KwYBBAGCNwoDAwYKKwYBBAGCNwoDBAYJYIZIAYb4QgQBBgsrBgEEAYI3CgMEAQYI\n"
    "KwYBBQUHAwUGCCsGAQUFBwMGBggrBgEFBQcDBwYIKwYBBQUIAgIGCisGAQQBgjcU\n"
    "AgIGCCsGAQUFBwMJBggrBgEFBQcDDQYIKwYBBQUHAw4GBysGAQUCAwUwDQYJKoZI\n"
    "hvcNAQELBQADggEBAKHmwejWRwGE1wf1k2rG8SNRV/neGsZ6Qfqf6co3TpR/Wi1s\n"
    "iZDvSeT/rbqNBS7z34xnG88NIUwu00y78e8Mfon31ZZbK4Uo7fla9/D3ukdJqPQC\n"
    "LKdbKJjR2kH+KCukY/1rghjJ8/X+t2egBit0LCOdsFCl07Sfksb9kpGUIZSFcYYm\n"
    "geqhjhoNwxazzHiw+QWHC5HG9248JIizBmy1aymNWuMnPudhjHAnPcsIlqMVNq3t\n"
    "Rv9ap7S8JeCxHVRPJvJeCwXWvW3dW/v3xH52Yn/fqRblN1w9Fxz5NhopKx0gj/Jd\n"
    "sw2N4Fk4gaOWEolFpa0bwNw8nAx7moehZpowzfw=\n"
    "-----END CERTIFICATE-----\n";

static const char *AUTH_PRIVATE_KEY =
    "data:,-----BEGIN RSA PRIVATE KEY-----\n"
    "MIIEogIBAAKCAQEAwqVYYrx/8ASMru+K70J3J0maisrKTegrw9aZlfcH7eTx4Vbf\n"
    "Y1e/b4Erm+SwA6ERwIBXb/XbmIAa9ezm+rkayGOAdfAnKwo4UHAuII8fMto5fT3m\n"
    "JvqmA96OOBWAawatEAp60ICrG/6l/WfjMP1VIsOucevFvO+ixB0EKBIE+JMUrmoM\n"
    "AaFUZHjw+n2ACpk+F0t8fjtL/qxxEk3LChesjGnYKe4d3h9W/JCU80sfCVfWc6vv\n"
    "psY6MaoWwVT+Zw83toYUiHgpAr0q8e3n5ICkuAEF2uf2eOBogLtU3111KnYnPa8c\n"
    "A9HnZ2ozRJZ2Xu7I6JP9Mrqrx/toUqqTb2v86QIDAQABAoIBAC1q32DKkx+yMBFx\n"
    "m32QiLUGG6VfBC2BixS7MkMnzRXZYgcuehl4FBc0kLRjfB6cqsO8LqrVN1QyMBhK\n"
    "GutN3c38SbE7RChqzhEW2+yE+Mao3Nk4ZEecHLiyaYT0n25ZtHAVwep823BAzwJ+\n"
    "BykbM45VEpNKbG1VjSktjBa9faNyZiZAEJEjVyla+6R8N4kHV52LbZcLjvJv3IQ2\n"
    "iPYRrmMyI5C23qTni0vy7yJbAXBo3CqgSlwie9FARBWT7Puu7F4mF1O1c/SnTysw\n"
    "Tm3e5FzgfHipQbnRVn0w4rDprPMKmPxMnvf/Wkw0zVgNadp1Tc1I6Yj525DEQ07i\n"
    "2gIn/gECgYEA4jNnY1u2Eu7x3pAQF3dRO0x35boVtuq9iwQk7q+uaZaK4RJRr+0Y\n"
    "T68S3bPnfer6SHvcxtST89Bvs/j/Ky4SOaX037UYjFh6T7OIzPl+MzO1yb+VOBT6\n"
    "D6FVGEJGp8ZAITU1OfJPeTYViUeEC8tHFGoKUCk50FbB6jOf1oKtv/ECgYEA3EnB\n"
    "Y7kSbJJaUuj9ciFUL/pAno86Cim3VjegK1wKgEiyDb610bhoMErovPwfVJbtcttG\n"
    "eKJNuwizkRcVbj+vpjDvqqaP5eMxLl6/Nd4haPMJYzGo88Z8NJpwFRNF2KEWjOpQ\n"
    "2NEvoCeRtVulCJyka2Tpljzw8cOXkxhPOe2UhHkCgYBo3entj0QO7QXm56T+LAvV\n"
    "0PK45xdQEO3EuCwjGAFk5C0IgUSrqeCeeIzniZMltj1IQ1wsNbtNynEu3530t8wt\n"
    "O7oVyFBUKGSz9IjUdkpClJOPr6kPMfJoMqRPtdIpz+hFPPSrI6IikKdVWHloOlp+\n"
    "pVaYqTQrWT1XRY2xli3VEQKBgGySmZN6Cx+h/oywswIGdUT0VdcQhq2to+QFpJba\n"
    "VX6m1cM6hMip2Ag9U3qZ1SNPBBdBBfm9HQybHE3dj713/C2wHuAAGhpXIM1W+20k\n"
    "X1knuC/AsSH9aQhQOf/ZMOq1crTfZBuI9q0782/sjGmzMsKPySU4QhUWruVb7OiD\n"
    "NVkZAoGAEvihW7G+8/iOE40vGHyBqUeopAAWLciTAUIEwM/Oi3BYfNWNTWF/FWNc\n"
    "nMvCZPYigY8C1vO+1iT2Frtd3CIU+f01Q3fJNJoRLlEiKLNZUJRF48OKUqjKSmsi\n"
    "w6pucFO40z05YW7utApj4L82rZnOS0pd1tUI1yexqvj0i4ThJfk=\n"
    "-----END RSA PRIVATE KEY-----\n";

static struct plugins_hdl *g_plugins = NULL;
static dds_security_authentication *g_auth = NULL;
static dds_security_access_control *g_access_control = NULL;
static char *g_path_to_etc_dir = NULL;

/* Prepare a property sequence. */
static void dds_security_property_init(DDS_Security_PropertySeq *seq, DDS_Security_unsigned_long size)
{
  seq->_length = size;
  seq->_maximum = size;
  seq->_buffer = ddsrt_malloc(size * sizeof(DDS_Security_Property_t));
  memset(seq->_buffer, 0, size * sizeof(DDS_Security_Property_t));
}

/* Cleanup a property sequence.*/
static void dds_security_property_deinit(DDS_Security_PropertySeq *seq)
{
  uint32_t i;

  for (i = 0; i < seq->_length; i++)
  {
    ddsrt_free(seq->_buffer[i].name);
    ddsrt_free(seq->_buffer[i].value);
  }
  ddsrt_free(seq->_buffer);
}

/* Find a property within a sequence.*/
static DDS_Security_Property_t *dds_security_property_find(DDS_Security_PropertySeq *seq, const char *name)
{
  DDS_Security_Property_t *prop = NULL;
  uint32_t i;
  for (i = 0; (i < seq->_length) && (prop == NULL); i++)
  {
    if (strcmp(seq->_buffer[i].name, name) == 0)
    {
      prop = &(seq->_buffer[i]);
    }
  }
  return prop;
}

/* Cleanup exception contents.*/
static void reset_exception(DDS_Security_SecurityException *ex)
{
  ex->code = 0;
  ex->minor_code = 0;
  ddsrt_free(ex->message);
  ex->message = NULL;
}

/* Glue two strings together */
static char *combine_strings(const char *prefix, const char *postfix)
{
  char *str;
  ddsrt_asprintf(&str, "%s%s", prefix, postfix);
  return str;
}

/* Use the given file to create a proper file uri (with directory).*/
static char *create_uri_file(const char *file)
{
  char *uri;
  char *dir;
  if (file)
  {
    dir = combine_strings("file:", g_path_to_etc_dir);
    uri = combine_strings(dir, file);
    ddsrt_free(dir);
  }
  else
  {
    uri = ddsrt_strdup("file:");
  }
  return uri;
}

/* Read the given file contents and transform it into a data uri.*/
static char *create_uri_data(const char *file)
{
  char *data = NULL;
  char *location;
  char *contents;

  if (file)
  {
    location = combine_strings(g_path_to_etc_dir, file);
    if (location)
    {
      contents = load_file_contents(location);
      if (contents)
      {
        data = combine_strings("data:,", contents);
        ddsrt_free(contents);
      }
      ddsrt_free(location);
    }
  }
  else
  {
    data = ddsrt_strdup("data:,");
  }

  return data;
}

/* Fill the security properties of a participant QoS with the
 * authorization and access_control values. */
static void fill_property_policy(DDS_Security_PropertyQosPolicy *property, const char *permission_ca, const char *permission_uri, const char *governance_uri)
{
  dds_security_property_init(&property->value, 6);
  /* Authentication properties. */
  property->value._buffer[0].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_IDENTITY_CERT);
  property->value._buffer[0].value = ddsrt_strdup(AUTH_IDENTITY_CERT);
  property->value._buffer[1].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_IDENTITY_CA);
  property->value._buffer[1].value = ddsrt_strdup(AUTH_IDENTITY_CA);
  property->value._buffer[2].name = ddsrt_strdup(DDS_SEC_PROP_AUTH_PRIV_KEY);
  property->value._buffer[2].value = ddsrt_strdup(AUTH_PRIVATE_KEY);
  /* AccessControl properties. */
  property->value._buffer[3].name = ddsrt_strdup(DDS_SEC_PROP_ACCESS_PERMISSIONS_CA);
  property->value._buffer[3].value = permission_ca ? ddsrt_strdup(permission_ca) : NULL;
  property->value._buffer[4].name = ddsrt_strdup(DDS_SEC_PROP_ACCESS_PERMISSIONS);
  property->value._buffer[4].value = permission_uri ? ddsrt_strdup(permission_uri) : NULL;
  property->value._buffer[5].name = ddsrt_strdup(DDS_SEC_PROP_ACCESS_GOVERNANCE);
  property->value._buffer[5].value = governance_uri ? ddsrt_strdup(governance_uri) : NULL;
}

/* Open a local identity by calling the authorization plugin with
 * properly created dummy values and the given participant QoS.*/
static DDS_Security_IdentityHandle create_local_identity(DDS_Security_Qos *participant_qos)
{
  DDS_Security_IdentityHandle local_id_hdl = DDS_SECURITY_HANDLE_NIL;
  DDS_Security_ValidationResult_t result;
  DDS_Security_DomainId domain_id = 0;
  DDS_Security_GUID_t local_participant_guid;
  DDS_Security_GUID_t candidate_participant_guid;
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_GuidPrefix_t prefix = {0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb};
  DDS_Security_EntityId_t entityId = {{0xb0, 0xb1, 0xb2}, 0x1};

  CU_ASSERT_FATAL(g_auth != NULL);
  assert(g_auth != NULL);
  CU_ASSERT_FATAL(g_auth->validate_local_identity != NULL);
  assert(g_auth->validate_local_identity != 0);

  memset(&local_participant_guid, 0, sizeof(local_participant_guid));
  memcpy(&candidate_participant_guid.prefix, &prefix, sizeof(prefix));
  memcpy(&candidate_participant_guid.entityId, &entityId, sizeof(entityId));

  /* Now call the function. */
  result = g_auth->validate_local_identity(
      g_auth,
      &local_id_hdl,
      &local_participant_guid,
      domain_id,
      participant_qos,
      &candidate_participant_guid,
      &exception);

  if (result != DDS_SECURITY_VALIDATION_OK)
  {
    printf("validate_local_identity_failed: %s\n", exception.message ? exception.message : "Error message missing");
  }

  reset_exception(&exception);

  return local_id_hdl;
}

/* Close the given local identity by returning its handle to the
 * authorization plugin.*/
static void clear_local_identity(DDS_Security_IdentityHandle local_id_hdl)
{
  DDS_Security_SecurityException exception = {NULL, 0, 0};
  DDS_Security_boolean success;

  if (local_id_hdl != DDS_SECURITY_HANDLE_NIL)
  {
    success = g_auth->return_identity_handle(g_auth, local_id_hdl, &exception);
    if (!success)
    {
      printf("return_identity_handle failed: %s\n", exception.message ? exception.message : "Error message missing");
    }
    reset_exception(&exception);
  }
}

/* Prepare the global link to the test's "etc" directory.*/
static void set_path_to_etc_dir(void)
{
  ddsrt_asprintf(&g_path_to_etc_dir, "%s%s", CONFIG_ENV_TESTS_DIR, RELATIVE_PATH_TO_ETC_DIR);
}

/* Initialize the participant QoS with security related properties.
 * It will transform the given files into proper uri's.
 * A NULL will result in a file uri without actual link.*/
static void qos_init_file(DDS_Security_Qos *participant_qos, const char *certificate_filename, const char *permission_filename, const char *governance_filename)
{
  char *permission_ca;
  char *permission_uri;
  char *governance_uri;

  permission_ca = create_uri_file(certificate_filename);
  permission_uri = create_uri_file(permission_filename);
  governance_uri = create_uri_file(governance_filename);

  memset(participant_qos, 0, sizeof(*participant_qos));
  fill_property_policy(&(participant_qos->property),
                       permission_ca,
                       permission_uri,
                       governance_uri);

  ddsrt_free(permission_ca);
  ddsrt_free(permission_uri);
  ddsrt_free(governance_uri);
}

/* Initialize the participant QoS with security related properties.
 * It will transform the given files into data uri's.
 * A NULL will result in a data uri without actual data.*/
static void qos_init_data(DDS_Security_Qos *participant_qos, const char *certificate_filename, const char *permission_filename, const char *governance_filename)
{
  char *permission_ca;
  char *permission_uri;
  char *governance_uri;

  permission_ca = create_uri_data(certificate_filename);
  permission_uri = create_uri_data(permission_filename);
  governance_uri = create_uri_data(governance_filename);
  CU_ASSERT_FATAL(permission_ca != NULL);
  CU_ASSERT_FATAL(permission_uri != NULL);
  CU_ASSERT_FATAL(governance_uri != NULL);

  memset(participant_qos, 0, sizeof(*participant_qos));
  fill_property_policy(&(participant_qos->property),
                       permission_ca,
                       permission_uri,
                       governance_uri);

  ddsrt_free(permission_ca);
  ddsrt_free(permission_uri);
  ddsrt_free(governance_uri);
}

/* Initialize the participant QoS with security related properties.
 * A NULL will result in an uri with an unknown type.*/
static void qos_init_type(DDS_Security_Qos *participant_qos, const char *certificate_filename, const char *permission_filename, const char *governance_filename)
{
  char *permission_ca;
  char *permission_uri;
  char *governance_uri;

  if (certificate_filename)
    permission_ca = create_uri_file(certificate_filename);
  else
    permission_ca = ddsrt_strdup("unknown_type:,just some data");
  if (permission_filename)
    permission_uri = create_uri_file(permission_filename);
  else
    permission_uri = ddsrt_strdup("unknown_type:,just some data");
  if (governance_filename)
    governance_uri = create_uri_file(governance_filename);
  else
    governance_uri = ddsrt_strdup("unknown_type:,just some data");

  memset(participant_qos, 0, sizeof(*participant_qos));
  fill_property_policy(&(participant_qos->property),
                       permission_ca,
                       permission_uri,
                       governance_uri);

  ddsrt_free(permission_ca);
  ddsrt_free(permission_uri);
  ddsrt_free(governance_uri);
}

/* Initialize the participant QoS with security related properties.
 * Allow NULL as property value.*/
static void qos_init_null(DDS_Security_Qos *participant_qos, const char *certificate_filename, const char *permission_filename, const char *governance_filename)
{
  char *permission_ca = NULL;
  char *permission_uri = NULL;
  char *governance_uri = NULL;

  if (certificate_filename)
    permission_ca = create_uri_file(certificate_filename);
  if (permission_filename)
    permission_uri = create_uri_file(permission_filename);
  if (governance_filename)
    governance_uri = create_uri_file(governance_filename);

  memset(participant_qos, 0, sizeof(*participant_qos));
  fill_property_policy(&(participant_qos->property),
                       permission_ca,
                       permission_uri,
                       governance_uri);

  ddsrt_free(permission_ca);
  ddsrt_free(permission_uri);
  ddsrt_free(governance_uri);
}

/* Cleanup the participant QoS.*/
static void qos_deinit(DDS_Security_Qos *participant_qos)
{
  dds_security_property_deinit(&(participant_qos->property.value));
}

/* Setup the testing environment by loading the plugins and
 * creating a local identity.*/
static DDS_Security_IdentityHandle test_setup(DDS_Security_Qos *participant_qos)
{
  DDS_Security_IdentityHandle local_id_hdl = DDS_SECURITY_HANDLE_NIL;

  g_plugins = load_plugins(&g_access_control /* Access Control */,
                           &g_auth /* Authentication */,
                           NULL /* Cryptograpy    */,
                           NULL);
  if (g_plugins)
  {
    CU_ASSERT_FATAL(g_auth != NULL);
    assert(g_auth != NULL);
    CU_ASSERT_FATAL(g_access_control != NULL);
    assert(g_access_control != NULL);
    CU_ASSERT_FATAL(g_access_control->validate_local_permissions != NULL);
    assert(g_access_control->validate_local_permissions != 0);
    CU_ASSERT_FATAL(g_access_control->return_permissions_handle != NULL);
    assert(g_access_control->return_permissions_handle != 0);

    local_id_hdl = create_local_identity(participant_qos);
  }

  return local_id_hdl;
}

/* Teardown the testing environment by clearing the local identity
 * and closing the plugins.*/
static int test_teardown(DDS_Security_IdentityHandle local_id_hdl)
{
  clear_local_identity(local_id_hdl);
  unload_plugins(g_plugins);
  g_plugins = NULL;
  g_access_control = NULL;
  g_auth = NULL;
  return 0;
}

/* The AccessControl related properties in the participant_qos will
 * have some kind of problem that should force a failure when
 * checking the local permissions.*/
static DDS_Security_long test_failure_scenario(DDS_Security_Qos *participant_qos)
{
  DDS_Security_long code = DDS_SECURITY_ERR_OK_CODE;
  DDS_Security_IdentityHandle local_id_hdl = DDS_SECURITY_HANDLE_NIL;
  DDS_Security_PermissionsHandle result;
  DDS_Security_SecurityException exception = {NULL, 0, 0};

  /* Prepare testing environment. */
  local_id_hdl = test_setup(participant_qos);
  CU_ASSERT_FATAL(local_id_hdl != DDS_SECURITY_HANDLE_NIL);

  /* Call the plugin with the invalid property. */
  result = g_access_control->validate_local_permissions(
      g_access_control,
      g_auth,
      local_id_hdl,
      0,
      participant_qos,
      &exception);

  /* Be sure the plugin returned a failure. */
  CU_ASSERT(result == 0);
  if (result == 0)
  {
    code = exception.code;
    CU_ASSERT(exception.message != NULL);
    printf("validate_local_permissions failed: (%d) %s\n", (int)exception.code, exception.message ? exception.message : "Error message missing");
  }
  else
  {
    reset_exception(&exception);
    g_access_control->return_permissions_handle(g_access_control, result, &exception);
  }
  reset_exception(&exception);

  /* Cleanup the testing environment. */
  test_teardown(local_id_hdl);

  return code;
}

/* Use with invalid file link for certificate, permission or
 * governance. The local permissions check should fail.*/
static DDS_Security_long test_invalid_file_uri(const char *certificate_filename, const char *permission_filename, const char *governance_filename)
{
  DDS_Security_long code = DDS_SECURITY_ERR_OK_CODE;
  DDS_Security_Qos participant_qos;

  qos_init_file(&participant_qos,
                certificate_filename,
                permission_filename,
                governance_filename);

  code = test_failure_scenario(&participant_qos);

  qos_deinit(&participant_qos);

  return code;
}

/* Use with invalid data for certificate, permission or governance.
 * The local permissions check should fail.*/
static DDS_Security_long test_invalid_data_uri(const char *certificate_filename, const char *permission_filename, const char *governance_filename)
{
  DDS_Security_long code = DDS_SECURITY_ERR_OK_CODE;
  DDS_Security_Qos participant_qos;

  qos_init_data(&participant_qos,
                certificate_filename,
                permission_filename,
                governance_filename);

  code = test_failure_scenario(&participant_qos);

  qos_deinit(&participant_qos);

  return code;
}

/* Generate uri's with invalid types for certificate, permission
 * or governance. The local permissions check should fail.*/
static DDS_Security_long test_invalid_type_uri(const char *certificate_filename, const char *permission_filename, const char *governance_filename)
{
  DDS_Security_long code = DDS_SECURITY_ERR_OK_CODE;
  DDS_Security_Qos participant_qos;

  qos_init_type(&participant_qos,
                certificate_filename,
                permission_filename,
                governance_filename);

  code = test_failure_scenario(&participant_qos);
  qos_deinit(&participant_qos);
  return code;
}

/* Create properties in the QoS without actual values (NULL).
 * The local permissions check should fail.*/
static DDS_Security_long test_null_uri(const char *certificate_filename, const char *permission_filename, const char *governance_filename)
{
  DDS_Security_long code = DDS_SECURITY_ERR_OK_CODE;
  DDS_Security_Qos participant_qos;

  qos_init_null(&participant_qos,
                certificate_filename,
                permission_filename,
                governance_filename);

  code = test_failure_scenario(&participant_qos);

  qos_deinit(&participant_qos);

  return code;
}

/* Get valid documents, but corrupt the signatures.
 * The local permissions check should fail.*/
static DDS_Security_long test_corrupted_signature(bool corrupt_permissions, bool corrupt_governance)
{
  DDS_Security_long code = DDS_SECURITY_ERR_OK_CODE;
  DDS_Security_Property_t *prop = NULL;
  DDS_Security_Qos participant_qos;
  size_t len;

  /* Get data with valid signatures. */
  qos_init_data(&participant_qos,
                "Test_Permissions_ca.pem",
                "Test_Permissions_full.p7s",
                "Test_Governance_full.p7s");

  /* Only allow one signature to be corrupted. */
  CU_ASSERT_FATAL(corrupt_permissions != corrupt_governance);

  /* Corrupt the signature. */
  if (corrupt_permissions)
    prop = dds_security_property_find(&(participant_qos.property.value), DDS_SEC_PROP_ACCESS_PERMISSIONS);
  if (corrupt_governance)
    prop = dds_security_property_find(&(participant_qos.property.value), DDS_SEC_PROP_ACCESS_GOVERNANCE);

  /* Just some (hardcoded) sanity checks. */
  CU_ASSERT_FATAL(prop != NULL);
  CU_ASSERT_FATAL(prop->value != NULL);
  assert(prop && prop->value); // for Clang's static analyzer
  len = strlen(prop->value);
  CU_ASSERT_FATAL(len > 2250);

  /* Corrupt a byte somewhere in the signature. */
  prop->value[len - 75]--;

  code = test_failure_scenario(&participant_qos);
  qos_deinit(&participant_qos);
  return code;
}

static void suite_validate_local_permissions_init(void)
{
  set_path_to_etc_dir();
}

static void suite_validate_local_permissions_fini(void)
{
  ddsrt_free(g_path_to_etc_dir);
}

/* Supplying proper files should pass the local permissions check */
CU_Test(ddssec_builtin_validate_local_permissions, valid_file, .init = suite_validate_local_permissions_init, .fini = suite_validate_local_permissions_fini)
{
  DDS_Security_IdentityHandle local_id_hdl = DDS_SECURITY_HANDLE_NIL;
  DDS_Security_PermissionsHandle result;
  DDS_Security_Qos participant_qos;
  DDS_Security_SecurityException exception = {NULL, 0, 0};

  qos_init_file(&participant_qos,
                "Test_Permissions_ca.pem",
                "Test_Permissions_full.p7s",
                "Test_Governance_full.p7s");
  local_id_hdl = test_setup(&participant_qos);
  CU_ASSERT_FATAL(local_id_hdl != DDS_SECURITY_HANDLE_NIL);

  result = g_access_control->validate_local_permissions(
      g_access_control,
      g_auth,
      local_id_hdl,
      0,
      &participant_qos,
      &exception);

  CU_ASSERT(result != 0);
  if (result == 0)
  {
    printf("validate_local_permissions_failed: %s\n", exception.message ? exception.message : "Error message missing");
  }
  else
  {
    g_access_control->return_permissions_handle(g_access_control, result, &exception);
  }
  reset_exception(&exception);

  test_teardown(local_id_hdl);
  qos_deinit(&participant_qos);
}

/* Supplying proper data should pass the local permissions check */
CU_Test(ddssec_builtin_validate_local_permissions, valid_data, .init = suite_validate_local_permissions_init, .fini = suite_validate_local_permissions_fini)
{
  DDS_Security_IdentityHandle local_id_hdl = DDS_SECURITY_HANDLE_NIL;
  DDS_Security_PermissionsHandle result;
  DDS_Security_Qos participant_qos;
  DDS_Security_SecurityException exception = {NULL, 0, 0};

  qos_init_data(&participant_qos,
                "Test_Permissions_ca.pem",
                "Test_Permissions_full.p7s",
                "Test_Governance_full.p7s");
  local_id_hdl = test_setup(&participant_qos);
  CU_ASSERT(local_id_hdl != DDS_SECURITY_HANDLE_NIL);

  result = g_access_control->validate_local_permissions(
      g_access_control,
      g_auth,
      local_id_hdl,
      0,
      &participant_qos,
      &exception);

  CU_ASSERT(result != 0);
  if (result == 0)
  {
    printf("validate_local_permissions_failed: %s\n", exception.message ? exception.message : "Error message missing");
  }
  else
  {
    g_access_control->return_permissions_handle(g_access_control, result, &exception);
  }
  reset_exception(&exception);

  test_teardown(local_id_hdl);
  qos_deinit(&participant_qos);
}

/* Supplying no files but directories should fail the local permissions check. */
CU_Test(ddssec_builtin_validate_local_permissions, uri_directories, .init = suite_validate_local_permissions_init, .fini = suite_validate_local_permissions_fini)
{
  DDS_Security_long code;

  /* Certificate points to a valid directory.*/
  code = test_invalid_file_uri("",
                               "Test_Permissions_full.p7s",
                               "Test_Governance_full.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_FILE_PATH_CODE);

  /* Permission points to a valid directory. */
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "",
                               "Test_Governance_full.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_FILE_PATH_CODE);

  /* Governance points to a valid directory.*/
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_Permissions_full.p7s",
                               "");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_FILE_PATH_CODE);
}

/* Supplying empty files should fail the local permissions check. */
CU_Test(ddssec_builtin_validate_local_permissions, uri_empty_files, .init = suite_validate_local_permissions_init, .fini = suite_validate_local_permissions_fini)
{
  DDS_Security_long code;

  /* Certificate points to an empty file. */
  code = test_invalid_file_uri("Test_File_empty.txt",
                               "Test_Permissions_full.p7s",
                               "Test_Governance_full.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_FILE_PATH_CODE);

  /* Permission points to an empty file. */
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_File_empty.txt",
                               "Test_Governance_full.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_FILE_PATH_CODE);

  /* Governance points to an empty file. */
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_Permissions_full.p7s",
                               "Test_File_empty.txt");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_FILE_PATH_CODE);
}

/* Supplying text files should fail the local permissions check. */
CU_Test(ddssec_builtin_validate_local_permissions, uri_text_files, .init = suite_validate_local_permissions_init, .fini = suite_validate_local_permissions_fini)
{
  DDS_Security_long code;

  /* Certificate points to a file with only text. */
  code = test_invalid_file_uri("Test_File_text.txt",
                               "Test_Permissions_full.p7s",
                               "Test_Governance_full.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_CERTIFICATE_CODE);

  /* Permission points to a file with only text. */
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_File_text.txt",
                               "Test_Governance_full.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_SMIME_DOCUMENT_CODE);

  /* Governance points to a file with only text. */
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_Permissions_full.p7s",
                               "Test_File_text.txt");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_SMIME_DOCUMENT_CODE);
}

/* Not supplying files should fail the local permissions check. */
CU_Test(ddssec_builtin_validate_local_permissions, uri_absent_files, .init = suite_validate_local_permissions_init, .fini = suite_validate_local_permissions_fini)
{
  DDS_Security_long code;

  /* Certificate points to a non-existing file.*/
  code = test_invalid_file_uri("Test_File_absent.txt",
                               "Test_Permissions_full.p7s",
                               "Test_Governance_full.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_FILE_PATH_CODE);

  /* Permission points to a non-existing file.*/
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_File_absent.txt",
                               "Test_Governance_full.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_FILE_PATH_CODE);

  /* Governance points to a non-existing file.*/
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_Permissions_full.p7s",
                               "Test_File_absent.txt");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_FILE_PATH_CODE);
}

/* Not supplying file uris should fail the local permissions check. */
CU_Test(ddssec_builtin_validate_local_permissions, uri_no_files, .init = suite_validate_local_permissions_init, .fini = suite_validate_local_permissions_fini)
{
  DDS_Security_long code;

  /* Certificate file uri doesn't point to anything.*/
  code = test_invalid_file_uri(NULL,
                               "Test_Permissions_full.p7s",
                               "Test_Governance_full.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_FILE_PATH_CODE);

  /* Permission file uri doesn't point to anything.*/
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               NULL,
                               "Test_Governance_full.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_FILE_PATH_CODE);

  /* Governance file uri doesn't point to anything.*/
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_Permissions_full.p7s",
                               NULL);
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_FILE_PATH_CODE);
}

/* Supplying empty data should fail the local permissions check. */
CU_Test(ddssec_builtin_validate_local_permissions, uri_empty_data, .init = suite_validate_local_permissions_init, .fini = suite_validate_local_permissions_fini)
{
  DDS_Security_long code;

  /* Certificate is empty data.*/
  code = test_invalid_data_uri(NULL,
                               "Test_Permissions_full.p7s",
                               "Test_Governance_full.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_CERTIFICATE_CODE);

  /* Permission is empty data.*/
  code = test_invalid_data_uri("Test_Permissions_ca.pem",
                               NULL,
                               "Test_Governance_full.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_PERMISSION_DOCUMENT_PROPERTY_CODE);

  /* Governance is empty data.*/
  code = test_invalid_data_uri("Test_Permissions_ca.pem",
                               "Test_Permissions_full.p7s",
                               NULL);
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_GOVERNANCE_DOCUMENT_PROPERTY_CODE);
}

/* Supplying uris with invalid types should fail the local permissions check. */
CU_Test(ddssec_builtin_validate_local_permissions, uri_invalid_types, .init = suite_validate_local_permissions_init, .fini = suite_validate_local_permissions_fini)
{
  DDS_Security_long code;

  /* Certificate doesn't point to anything: results in invalid type.*/
  code = test_invalid_type_uri(NULL,
                               "Test_Permissions_full.p7s",
                               "Test_Governance_full.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_CERTIFICATE_TYPE_NOT_SUPPORTED_CODE);

  /* Permission doesn't point to anything: results in invalid type.*/
  code = test_invalid_type_uri("Test_Permissions_ca.pem",
                               NULL,
                               "Test_Governance_full.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_URI_TYPE_NOT_SUPPORTED_CODE);

  /* Governance doesn't point to anything: results in invalid type*/
  code = test_invalid_type_uri("Test_Permissions_ca.pem",
                               "Test_Permissions_full.p7s",
                               NULL);
  CU_ASSERT(code == DDS_SECURITY_ERR_URI_TYPE_NOT_SUPPORTED_CODE);
}

/* Not supplying actual uris should fail the local permissions check. */
CU_Test(ddssec_builtin_validate_local_permissions, uri_null, .init = suite_validate_local_permissions_init, .fini = suite_validate_local_permissions_fini)
{
  DDS_Security_long code;

  /* Certificate doesn't point to anything.*/
  code = test_null_uri(NULL,
                       "Test_Permissions_full.p7s",
                       "Test_Governance_full.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_MISSING_PROPERTY_CODE);

  /* Permission doesn't point to anything.*/
  code = test_null_uri("Test_Permissions_ca.pem",
                       NULL,
                       "Test_Governance_full.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_MISSING_PROPERTY_CODE);

  /* Governance doesn't point to anything.*/
  code = test_null_uri("Test_Permissions_ca.pem",
                       "Test_Permissions_full.p7s",
                       NULL);
  CU_ASSERT(code == DDS_SECURITY_ERR_MISSING_PROPERTY_CODE);
}

/* Corrupted signatures should fail the local permissions check. */
CU_Test(ddssec_builtin_validate_local_permissions, corrupted_signatures, .init = suite_validate_local_permissions_init, .fini = suite_validate_local_permissions_fini)
{
  DDS_Security_long code;

  /* Corrupt permission signature.*/
  code = test_corrupted_signature(true /* Corrupt permissions? Yes. */,
                                  false /* Corrupt governance?  No.  */);
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_SMIME_DOCUMENT_CODE);

  /* Corrupt governance signature.*/
  code = test_corrupted_signature(false /* Corrupt permissions? No.  */,
                                  true /* Corrupt governance?  Yes. */);
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_SMIME_DOCUMENT_CODE);
}

/* Unknown signatures should fail the local permissions check. */
CU_Test(ddssec_builtin_validate_local_permissions, unknown_ca, .init = suite_validate_local_permissions_init, .fini = suite_validate_local_permissions_fini)
{
  DDS_Security_long code;

  /* Permission with unknown CA.*/
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_Permissions_unknown_ca.p7s",
                               "Test_Governance_full.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_SMIME_DOCUMENT_CODE);

  /* Governance with unknown CA.*/
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_Permissions_full.p7s",
                               "Test_Governance_unknown_ca.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_SMIME_DOCUMENT_CODE);
}

/* Un-available signatures should fail the local permissions check. */
CU_Test(ddssec_builtin_validate_local_permissions, not_signed, .init = suite_validate_local_permissions_init, .fini = suite_validate_local_permissions_fini)
{
  DDS_Security_long code;

  /* Permission not signed.*/
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_Permissions_not_signed.p7s",
                               "Test_Governance_full.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_SMIME_DOCUMENT_CODE);

  /* Governance not signed.*/
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_Permissions_full.p7s",
                               "Test_Governance_not_signed.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_SMIME_DOCUMENT_CODE);
}

/* Permissions outside the validity data should fail the local */
CU_Test(ddssec_builtin_validate_local_permissions, validity, .init = suite_validate_local_permissions_init, .fini = suite_validate_local_permissions_fini)
{
  DDS_Security_long code;

  /* Permission already expired.*/
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_Permissions_expired.p7s",
                               "Test_Governance_full.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_VALIDITY_PERIOD_EXPIRED_CODE);

  /* Permission not yet valid.*/
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_Permissions_notyet.p7s",
                               "Test_Governance_full.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_VALIDITY_PERIOD_NOT_STARTED_CODE);
}

/* Permissions document does not contain a proper subject_name,
 * which should fail the local permissions check. */
CU_Test(ddssec_builtin_validate_local_permissions, subject_name, .init = suite_validate_local_permissions_init, .fini = suite_validate_local_permissions_fini)
{
  DDS_Security_long code;

  /* Permission document with unknown subject. */
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_Permissions_unknown_subject.p7s",
                               "Test_Governance_check_create_participant.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_INVALID_SUBJECT_NAME_CODE);
}

/* Documents with invalid xml should fail the local permissions check. */
CU_Test(ddssec_builtin_validate_local_permissions, xml_invalid, .init = suite_validate_local_permissions_init, .fini = suite_validate_local_permissions_fini)
{
  DDS_Security_long code;

  /* Permission XML contains invalid domain id. */
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_Permissions_invalid_data.p7s",
                               "Test_Governance_ok.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_CAN_NOT_PARSE_PERMISSIONS_CODE);

  /* Permission XML contains invalid domain id. */
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_Permissions_invalid_element.p7s",
                               "Test_Governance_ok.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_CAN_NOT_PARSE_PERMISSIONS_CODE);

  /* Permission XML is missing the 'not before' validity tag.*/
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_Permissions_lack_of_not_before.p7s",
                               "Test_Governance_ok.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_CAN_NOT_PARSE_PERMISSIONS_CODE);

  /* Permission XML is missing the 'not after' validity tag.*/
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_Permissions_lack_of_not_after.p7s",
                               "Test_Governance_ok.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_CAN_NOT_PARSE_PERMISSIONS_CODE);

  /* Governance XML contains invalid encryption kind.*/
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_Permissions_ok.p7s",
                               "Test_Governance_invalid_data.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_CAN_NOT_PARSE_GOVERNANCE_CODE);

  /* Governance XML contains unknown element.*/
  code = test_invalid_file_uri("Test_Permissions_ca.pem",
                               "Test_Permissions_ok.p7s",
                               "Test_Governance_invalid_element.p7s");
  CU_ASSERT(code == DDS_SECURITY_ERR_CAN_NOT_PARSE_GOVERNANCE_CODE);
}
